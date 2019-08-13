//! Functions for runtime management.

use lazy_static::*; 
use log::*;

use env_logger;

use std::ffi::CStr;
use std::sync::Mutex;
use std::time;

use libc::{size_t, c_char, c_int, c_void, intptr_t, int32_t, int64_t}; 

use super::*;

mod memory;
mod tasks;

pub use tasks::{SplitterInitFn, SplitterNextFn};

/// Handle to an annotation.
pub type AnnotationRef = *mut Annotation;

/// A constant that states a splitte will emit items indefinitely.
const COMPOSER_INFINITE_ITEMS: i64 = -1;

/// An enum that states whether a splitter is finished producing values.
#[derive(Copy,Clone,Debug,PartialEq,Eq)]
#[repr(C)]
pub enum SplitterStatus {
    SplitterContinue = 0,
    SplitterFinished = 1,
}

/// A future representing a delayed computation.
#[derive(Copy,Clone,Debug,PartialEq,Eq)]
#[repr(transparent)]
pub struct Future(intptr_t);

impl Future {
    pub fn new<T: Into<intptr_t>>(x: T) -> Future {
        Future(x.into())
    }

    pub fn null() -> Future {
        Future(0)
    }
}

/// Singleton state for the Composer library. 
#[derive(Clone)]
struct ComposerState {
    memory_manager: memory::MemoryManager,
    task_manager: tasks::TaskManager,
}

impl ComposerState {
    fn new() -> ComposerState {
        ComposerState {
            memory_manager: memory::MemoryManager::new(),
            task_manager: tasks::TaskManager::new(),
        }
    }
}

lazy_static! {
    static ref STATE: Mutex<ComposerState> = Mutex::new(ComposerState::new());
}

/// End a clock and report running time.
fn log_time(start: time::Instant, message: &str) {
    let end = time::Instant::now();
    let duration = end.duration_since(start);
    info!("{}: {}", message,
          duration.as_secs() as f64 + duration.subsec_nanos() as f64 * 1e-9);
}

/// Handles the SIGBUS signal and unprotects protected memory.
#[cfg(target_os = "linux")]
unsafe extern "C" fn sigbus_handler(_sig: libc::c_int,
                             si: *const memory::siginfo_t,
                             _uap: *const libc::c_void) {
    let mut state = STATE.lock().unwrap();
    let address = (*si).si_addr as *const c_void;
    trace!("Handling SIGBUS signal at address {:x}", address as usize);
    state.memory_manager.unprotect_all();
    state.task_manager.execute();
}

#[cfg(target_os = "macos")]
unsafe extern "C" fn sigbus_handler(_sig: libc::c_int,
                             si: *const libc::siginfo_t,
                             _uap: *const libc::c_void) {
    let mut state = STATE.lock().unwrap();
    let address = (*si).si_addr as *const c_void;
    trace!("Handling SIGBUS signal at address {:x}", address as usize);
    state.memory_manager.unprotect_all();
    state.task_manager.execute();
}

fn init_logging() {
    let mut builder = env_logger::Builder::from_default_env();
    builder.default_format_timestamp(true)
           .init();
}

#[no_mangle]
/// Initialize the composer runtime. This should be called before any of the other composer
/// functions.
pub unsafe extern "C" fn composer_init(threads: int64_t, piece_size: int64_t) {
    // Init the logging module.
    init_logging();

    // Register the SIGBUS signal handler.
    let mut state = STATE.lock().unwrap();
    state.memory_manager.register_sigbus_handler(sigbus_handler)
        .expect("Could not register SIGBUS handler");

    // Enable lazy execution.
    state.task_manager.evaluate = false;
    state.task_manager.threads = threads;

    // TODO This should be discovered dynamically.
    // TODO No longer needed! This just sets the initial size.
    state.task_manager.init_task_size = piece_size as usize;
}

/// Drop-in `malloc` replacement that protects memory for lazy evaluation.
#[no_mangle]
pub unsafe extern "C" fn composer_malloc(size: size_t, lazy: c_int) -> *mut c_void {
    let mut state = STATE.lock().unwrap();
    state.memory_manager.allocate(size, lazy)
}

/// Convert a value to be lazily evaluated.
#[no_mangle]
pub unsafe extern "C" fn composer_tolazy(pointer: *mut c_void) {
    let mut state = STATE.lock().unwrap();
    state.memory_manager.protect(pointer)
}

/// Drop-in `free` replacement for freeing memory allocated using `composer_malloc`.
#[no_mangle]
pub unsafe extern "C" fn composer_free(pointer: *mut c_void) {
    let mut state = STATE.lock().unwrap();
    state.memory_manager.free(pointer)
}

/// Returns whether a function should be evaluated directly or registered for lazy execution.
#[no_mangle]
pub unsafe extern "C" fn composer_evaluate() -> c_int {
    // TODO deadlocks if called during evaluation...
    let state = STATE.lock().unwrap();
    if state.task_manager.evaluate {
        1
    } else {
        0
    }
}

/// Registers a value as a temporary.
///
/// The passed value should generally be a pointer, but could also be an immutable struct that has
/// pointers inside it. If a struct is passed and value of the struct is changed when used in a
/// function, the struct won't be detected as a temporary value.
#[no_mangle]
pub unsafe extern "C" fn composer_emit(pointer: *const c_void, size: size_t, merger: intptr_t) {
    debug!("Registering value {:?} as temporary value", pointer);
    let mut state = STATE.lock().unwrap();
    state.task_manager.register_output(pointer, size as usize, merger as usize);
}

/// Registers a function and its arguments for lazy execution.
///
/// Returns a handle to a `Future` (i.e., an `intptr_t` in C) that represents the return value is
/// returned. If the registered function doesn't return a value, 0 is returned.
#[no_mangle]
pub unsafe extern "C" fn composer_register_function(annotation: AnnotationRef,
                                                    callback: tasks::WrapperFunctionCallback,
                                                    arguments: *const c_void,
                                                    returns_value: int32_t) -> Future {
    let mut state = STATE.lock().unwrap();
    let annotation = &*annotation;
    let returns_value = if returns_value != 0 { true } else { false };
    state.task_manager.register_task(&*annotation,
                                     callback,
                                     arguments,
                                     returns_value).unwrap_or(Future::null())
}

/// Forces execution.
#[no_mangle]
pub unsafe extern "C" fn composer_execute() {
    let mut state = STATE.lock().unwrap();
    state.memory_manager.unprotect_all();
    state.task_manager.execute();
}

/// Protects all memory.
#[no_mangle]
pub unsafe extern "C" fn composer_protect_all() {
    let mut state = STATE.lock().unwrap();
    state.memory_manager.protect_all();
}

/// Prints the splitter status.
///
/// This function is not used: it is only hear to force `SplitterStatus` to be exported.
#[no_mangle]
pub unsafe extern "C" fn PrintSplitStatus(s: SplitterStatus) {
    println!("{:?}", s);
}

/// Returns the constant designating an infinite splitter.
#[no_mangle]
pub unsafe extern "C" fn ComposerInfiniteItems() -> int64_t {
    COMPOSER_INFINITE_ITEMS
}


/// Initializes a function's annotation definition from JSON.
#[no_mangle]
pub unsafe extern "C" fn InitFromJson(s: *const c_char) -> AnnotationRef {
    let s = CStr::from_ptr(s);
    let s = s.to_owned();
    let s = s.into_string().unwrap();
    let annotation = Annotation::from_json(&s).unwrap();
    Box::into_raw(Box::new(annotation))
}

/// Sets runtime information for a split type.
///
/// This information can only be gathered when a C header file is compiled (e.g., it includes
/// information such as function pointers and data type sizes).
#[no_mangle]
pub unsafe extern "C" fn SetSplitTypeInfo(annotation: AnnotationRef,
                                          index: usize,
                                          initializer: intptr_t,
                                          next: intptr_t,
                                          data_size: size_t) {
    let info = SplitTypeRuntimeInfo {
        initializer: initializer as FunctionPointer,
        next: next as FunctionPointer,
        data_size: data_size as usize,
    };
    let annotation = &mut *annotation;

    debug!("Setting runtime info {:?} for {:?}", &info, &annotation);

    annotation.set_type_runtime_info(index, info).unwrap();
}
