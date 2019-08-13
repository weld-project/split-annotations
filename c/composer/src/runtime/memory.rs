//! Memory management functions.
//!
//! Note that the behavior of these functions varies wildly based on the operating system being
//! used. For example, on MacOS, laziness via memory protection is effectively not possible because
//! the kernel will swap out pages with PROT_NONE permissions. The Linux kernel does not seem to do
//! this.
//!
//! One saving grace is the `pkeys(7)` set of system calls on Linux, which allows associated a tag
//! with a region of memory, and then protecting and unprotecting the entire region all at once via
//! a write to a register (instead of O(# of pages) calls to `mprotect`, which is slow).  Right
//! now, we either keep everything protected or unprotected, so a single `pkey_mprotect` will
//! toggle between lazy mode and non-lazy mode. This feature is known to work on AWS C5 instances
//! with Ubuntu 17.04.

use log::*;

use std::ptr;

use super::{composer_err, Result};

use lazy_static::*;

use libc::{size_t, c_int, c_void};
use libc::{mmap, munmap, mprotect};
use libc::{PROT_NONE, PROT_READ, PROT_WRITE};
use libc::{MAP_PRIVATE, MAP_FAILED, MAP_ANONYMOUS};
use libc::SA_SIGINFO;
use libc::{sigemptyset, sigaction, sighandler_t, sigset_t};
use libc::{SIG_DFL, SIG_IGN};

use std::time;
use std::time::Instant;
use super::log_time;

#[cfg(target_os = "macos")]
pub use libc::siginfo_t;

#[cfg(target_os = "linux")]
use libc::{c_long, c_short, c_uint};
#[cfg(target_os = "linux")]
use std::mem;

lazy_static! {
    static ref PAGESIZE: usize = unsafe { libc::sysconf(libc::_SC_PAGESIZE) as usize };
}

#[cfg(target_os = "linux")]
#[repr(C)]
pub struct siginfo_t {
    pub si_signo: c_int,
    pub si_errno: c_int,
    pub si_code: c_int,
    pub si_trapno: c_int,
    pub si_pid: libc::pid_t,
    pub si_uid: libc::uid_t,
    pub si_status: c_int,
    pub si_utime: libc::clock_t,
    pub si_stime: libc::clock_t,
    pub si_value: libc::sigval,
    pub si_int: libc::c_int,
    pub si_ptr: *const c_void,
    pub si_overrun: libc::c_int,
    pub si_timerid: libc::c_int,
    pub si_addr: *const c_void,
    pub si_band: c_long,
    pub si_fd: c_int,
    pub si_addr_lsb: c_short,
    pub si_lower: *const c_void,
    pub si_upper: *const c_void,
    pub si_pkey: c_int,
    pub si_call_addr: *const c_void,
    pub si_syscall: c_int,
    pub si_arch: c_uint,
}

/// A single handler.
pub type SignalHandler = unsafe extern "C" fn(libc::c_int, *const siginfo_t, *const libc::c_void);

#[derive(Debug, Clone)]
pub struct MemoryManager {
    allocations: Vec<MemorySegment>,
    all_protected: bool,
}

impl MemoryManager {
    /// Return a new MemoryManager.
    pub fn new() -> MemoryManager {
        MemoryManager {
            allocations: Vec::new(),
            all_protected: false,
        }
    }

    #[cfg(target_os = "linux")]
    unsafe fn sigaction(handler: SignalHandler) -> sigaction {
        sigaction {
            sa_sigaction: handler as sighandler_t,
            sa_flags: SA_SIGINFO,
            sa_mask: mem::zeroed::<sigset_t>(),
            sa_restorer: None,
        }
    }

    #[cfg(target_os = "macos")]
    unsafe fn sigaction(handler: SignalHandler) -> sigaction {
        sigaction {
            sa_sigaction: handler as sighandler_t,
            sa_flags: SA_SIGINFO,
            sa_mask: sigset_t::default(),
        }
    }

    /// Register a signal handler for this `MemoryManager`.
    pub unsafe fn register_sigbus_handler(&self, handler: SignalHandler) -> Result<()> {
        let mut sa = MemoryManager::sigaction(handler);
        sigemptyset(&mut sa.sa_mask as *mut _);

        let signal = if cfg!(target_os = "macos") {
            libc::SIGBUS
        } else if cfg!(target_os = "linux") {
            libc::SIGSEGV
        } else {
            panic!("Unsupported platform")
        };

        if sigaction(signal, &sa as *const _, ptr::null_mut()) == -1 {
            return composer_err!("Composer initialization failed: could not intercept SIGBUS");
        } else if sa.sa_sigaction == SIG_DFL {
            return composer_err!("Composer initialization failed: SIGBUS handled with default behavior");
        } else if sa.sa_sigaction == SIG_IGN {
            return composer_err!("Composer initialization failed: SIGBUS is ignored");
        }
        Ok(())
    }


    /// Allocate memory of size `size`.
    pub unsafe fn allocate(&mut self, size: size_t, lazy: c_int) -> *mut c_void {
        let lazy = lazy == 1;
        let segment = MemorySegment::allocate(size as usize, lazy);
        let pointer = segment.address;
        self.allocations.push(segment);
        self.all_protected &= lazy;
        pointer as *mut c_void
    }

    /// Frees the pointer.
    ///
    /// The pointer must be allocated with `allocate`.
    pub unsafe fn free(&mut self, pointer: *const c_void) {
        if let Some(index) = self.segment_index(pointer as usize) {
            {
                let segment = self.allocations.get_mut(index).unwrap();
                segment.free();
            }
            self.allocations.swap_remove(index);
        }
    }

    /// Protect the pointer, removing read and write priveleges from the VM page containing it.
    pub unsafe fn protect(&mut self, pointer: *const c_void) {
        if let Some(index) = self.segment_index(pointer as usize) {
            let segment = self.allocations.get_mut(index).unwrap();
            segment.protect();
        }
    }

    /// Protects all registered segments.
    pub unsafe fn protect_all(&mut self) {
        if self.all_protected {
            return;
        }

        let start = Instant::now();
        for segment in self.allocations.iter_mut() {
            segment.protect();
        }

        let end = Instant::now();
        let duration = end.duration_since(start);
        warn!("memory protection time: {}",
               duration.as_secs() as f64 + duration.subsec_nanos() as f64 * 1e-9);

        self.all_protected = true;
    }

    /// Unprotects all registered segments.
    pub unsafe fn unprotect_all(&mut self) {
        let start = time::Instant::now();
        for segment in self.allocations.iter_mut() {
            segment.unprotect();
        }
        self.all_protected = false;
        log_time(start, "Unprotect memory");
    }

    /// Get the index into the allocations vector of the segment containing this pointer.
    fn segment_index(&self, address: usize) -> Option<usize> {
        for (i, segment) in self.allocations.iter().enumerate() {
            if segment.contains(address) {
                return Some(i);
            }
        }
        None
    }
}

/// A segment of memory allocated using the composer allocation functions.
#[derive(Debug, Clone, PartialEq)]
pub struct MemorySegment {
    pub address: usize,
    pub size: usize,
    pub protected: bool,
}

impl MemorySegment {
    /// Allocates a protected memory segment of at least `size` bytes.
    pub unsafe fn allocate(size: usize, protected: bool) -> MemorySegment {
        let size = MemorySegment::page_aligned_size(size);
        let pointer = mmap(ptr::null_mut(), size as size_t,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if pointer == MAP_FAILED {
            error!("mmap failed");
        } else {
            info!("Allocated {} bytes with mmap", size);
        }

        let mut segment = MemorySegment {
            address: pointer as usize,
            size: size,
            protected: false,
        };

        if protected {
            segment.protect();
        }
        segment
    }

    /// Frees memory held by this segment.
    pub unsafe fn free(&mut self) {
        munmap(self.address as *mut c_void, self.size as size_t);
    }

    /// Remove permissions from the segment.
    pub unsafe fn protect(&mut self) {
        if self.protected {
            return;
        }
        trace!("protecting {:?}", self);
        if mprotect(self.address as *mut c_void, self.size as size_t, PROT_NONE) == -1 {
            panic!("mprotect failed");
        }
        self.protected = true;
    }

    /// Give read and write permissions to the segment.
    pub unsafe fn unprotect(&mut self) {
        if !self.protected {
            return;
        }
        trace!("unprotecting {:?}", self);
        if mprotect(self.address as *mut c_void, self.size as size_t, PROT_READ | PROT_WRITE) == -1 {
            panic!("mprotect failed");
        }
        self.protected = false;
    }

    /// Returns `true` if this segment contains the given pointer in its range.
    pub fn contains(&self, pointer: usize) -> bool {
        pointer >= self.address && pointer < self.address + self.size
    }

    /// Rounds up `size` to the next `_SC_PAGESIZE` multiple. On most systems, this is the next
    /// multiple of 4096.
    fn page_aligned_size(size: usize) -> usize {
        let page_size =  *PAGESIZE;
        if size % page_size != 0 {
            let num_pages = size / page_size + 1;
            num_pages * page_size
        } else {
            size
        }
    }
}

