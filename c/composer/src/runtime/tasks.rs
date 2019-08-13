//! Stores and executes function calls in a task graph.

use byteorder::ByteOrder;
use crossbeam_utils::thread;
use libc::{c_void, int64_t, intptr_t};
use fnv::{FnvHashSet, FnvHashMap};
use log::*;
use std::{fmt, mem, slice};
use std::ops::Range;
use std::sync::atomic;
use std::time::{Duration, Instant};

use super::*;

/// A wrapper C function called to invoke a function call from the library.
///
/// The wrapper function is requred to provide a consistent argument list and return type.
pub type WrapperFunctionCallback = unsafe extern "C" fn(*const c_void) -> Future;

/// A C function that initializes a splitter.
///
/// The function takes the number of pieces to split the input into, a pointer to the item to
/// split, a pointer to the arguments requested by the annotation, and a pointer to the number of
/// items the splitter will emit. The function returns a
/// pointer to an intialized splitter. The pointer *must* be allocated using libc's `malloc`,
/// because the runtime will attempt to deallocate it with `free`.
///
/// The last argument must be updated by the function to denote the number of items. If the
/// splitter will emit items forever (e.g., for a constant), this should be set to
/// `COMPOSER_INFINITE_ITEMS`.
pub type SplitterInitFn = unsafe extern "C" fn(*const c_void, *const c_void, *mut int64_t) -> *mut c_void;

/// A C function that calls next() on a splitter.
///
/// The first argument is the splitter object initialized with `SplitterInitFn`. The second
/// argument is the start index to retrieve, the third is the end index, and and the fourth is
/// object is a pointer where the output should be written.  The fourth argument is always sized
/// sufficiently to hold the split object.
pub type SplitterNextFn = unsafe extern "C" fn(*const c_void, int64_t, int64_t, *mut c_void) -> SplitterStatus;

/// A C function that merges split values.
///
/// Arguments: array of values, number of values, number of threads.
pub type SplitterMergeFn = unsafe extern "C" fn(*const c_void, int64_t, int64_t) -> *mut c_void;

/// Alias for a map that maps an argument identifier to a buffer.
type ArgumentMap = FnvHashMap<ArgumentId, Vec<u8>>;
/// Alias for a map that stores an argument and the information needed to split it.
type InputMap = FnvHashMap<ArgumentId, (Vec<ArgumentId>, SplitTypeParameter)>;
/// Alias for a map that stores an argument and its splitter.
type SplitterMap = FnvHashMap<ArgumentId, Option<SendSyncPointer>>;

/// An identifier for an argument.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
struct ArgumentId(u64);

/// A simple Id generator that produces ArgumentIds in sequence.
#[derive(Debug, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
struct IdGenerator(u64);

/// A thread-safe pointer.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
struct SendSyncPointer(*const c_void);

/// A piece passed from parallel threads to the main thread for aggregation.
#[derive(Debug, Clone, Eq, PartialEq)]
struct ParallelPiece {
    tid: i64,
    values: FnvHashMap<ArgumentId, SendSyncPointer>,
}

unsafe impl Send for SendSyncPointer {}
unsafe impl Sync for SendSyncPointer {}

const FUTURE_BASE_PTR: isize = 0xdead_beef;

impl ParallelPiece {
    fn new(tid: i64, values: FnvHashMap<ArgumentId, SendSyncPointer>) -> ParallelPiece {
        ParallelPiece { tid, values }
    }
}

impl IdGenerator {
    fn new_with_start(x: u64) -> IdGenerator {
        IdGenerator(x)
    }

    fn next(&mut self) -> ArgumentId {
        let value = ArgumentId(self.0);
        self.0 += 1;
        value
    }
}

/// A single function call and its arguments.
#[derive(Debug, Clone)]
struct Task {
    annotation: Annotation,
    function: WrapperFunctionCallback,
    arguments: Vec<ArgumentId>,
    future_id: Option<ArgumentId>,
}

impl Task {
    /// Return a new `Task` with the given function annotation, callback function, and argument
    /// set. The argument must be a repr(C) structure containing the function arguments.
    fn new(annotation: Annotation,
           function: WrapperFunctionCallback,
           arguments: Vec<ArgumentId>,
           future_id: Option<ArgumentId>) -> Task {
        Task {
            annotation,
            function,
            arguments,
            future_id,
        }
    }

    #[inline]
    unsafe fn call(&self, pointer: *const c_void) -> Future {
        (self.function)(pointer)
    }
}

impl fmt::Display for Task {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Task({}, {:?})", self.annotation.function.func_name, self.function)
    }
}


/// Copy a raw pointer into an owned `Vec<u8>`. `size` should be the size of the value in
/// bytes.
unsafe fn into_vec(pointer: *const u8, size: usize) -> Vec<u8> {
    let mut storage: Vec<u8> = vec![0; size];
    let slice = slice::from_raw_parts(pointer, size);
    storage.as_mut_slice().copy_from_slice(slice);
    storage
}

#[derive(Debug, Clone)]
pub struct TaskManager {
    pub evaluate: bool,
    pub threads: i64,
    pub init_task_size: usize,
    tasks: Vec<Task>,
    outputs: FnvHashMap<ArgumentId, (intptr_t, usize)>,
    arguments: FnvHashMap<ArgumentId, Vec<u8>>,
    futures: FnvHashSet<ArgumentId>,
    id_gen: IdGenerator,
    future_offset: isize,
}

// Methods called by the client library.
impl TaskManager {
    pub fn new() -> TaskManager {
        TaskManager {
            evaluate: true,
            threads: 1,
            init_task_size: 512,
            tasks: Vec::new(),
            outputs: FnvHashMap::default(),
            arguments: FnvHashMap::default(),
            futures: FnvHashSet::default(),
            id_gen: IdGenerator::new_with_start(1),
            future_offset: 0,
        }
    }

    fn next_future_pointer(&mut self) -> Future {
        let value = self.future_offset + FUTURE_BASE_PTR;
        self.future_offset += 1;
        Future::new(value)
    }

    /// Register a task with the runtime. The annotation should specify an ordered
    /// set of arguments for the function being register. `function` is a function pointer
    /// that takes a single `*const void` value as an argument.
    ///
    /// `arguments` is a packed C struct with the function arguments.
    pub unsafe fn register_task(&mut self,
                         annotation: &Annotation,
                         function: WrapperFunctionCallback,
                         arguments: *const c_void,
                         returns_value: bool) -> Option<Future> {

        debug!("Registering task with {} arguments", annotation.params.len());

        let future_id = if returns_value {
            Some(self.id_gen.next())
        } else {
            None
        };

        // Split the function's arguments into multiple pieces.
        let bytes = arguments as *const u8;
        let mut offset = 0;
        let split_arguments: Vec<_> = annotation.params.iter().map(&mut |a: &SplitTypeParameter| {
            // TODO: The runtime information should be associated with the parameter, not the type.
            // When the task is registered, generic types will not have been resolved.
            let runtime = a.ty.runtime_info();
            let storage = into_vec(bytes.offset(offset), runtime.data_size);
            trace!("Argument buffer: {:?}", storage);
            offset += runtime.data_size as isize;
            storage
        }).collect();
        debug!("register_task consumed {} bytes from the argument", offset);

        // Register the future as an "argument" so we can tie it in with other appearances.
        //
        // future is the valeu that we'll return to the caller.
        let (future, future_storage) = if future_id.is_some() {
            let mut storage: Vec<u8> = vec![0; mem::size_of::<intptr_t>()];     
            let future = self.next_future_pointer();
            byteorder::LittleEndian::write_u64(&mut storage, future.0 as u64);
            (Some(future), Some(storage))
        } else {
            (None, None)
        };

        // Construct the set of ArgumentIds for the task. An argument is assigned a new Id if we
        // don't have that argument registered already. Arguments are considered equal if their
        // stored values are equal (i.e., their bytes are equal) -- note that the stored values may
        // contain pointers, and the pointed-to values are not compared.
        let mut ids = vec![];
        for input_arg in split_arguments.into_iter() {
            let mut set = false;
            for (id, argument) in self.arguments.iter() {
                if *argument == input_arg {
                    ids.push(*id);
                    set = true;
                    break;
                }
            }

            if !set {
                // We didn't find the argument in the arguments registry. Add it in.
                let id = self.id_gen.next();
                self.arguments.insert(id, input_arg);
                ids.push(id);
            }
        }

        if let Some(future_storage) = future_storage {
            // Insert it into the arguments to track where else its used.
            self.arguments.insert(future_id.clone().unwrap(), future_storage);
            self.futures.insert(future_id.unwrap());
        }

        debug!("Registering task with arguments {:?}, returns {:?}", ids, future_id);
        self.tasks.push(Task::new(annotation.clone(), function, ids, future_id));
        future
    }

    /// Registers a value as a temporary. The passed value should generally be a pointer, but could
    /// also be an immutable struct that has pointers inside it.
    pub unsafe fn register_output(&mut self, pointer: *const c_void, size: usize, merger: usize) {
        let v = into_vec(pointer as *const u8, size);
        assert_eq!(size, mem::size_of::<isize>());
        debug!("Registering pointer {:?} of size {} as output variable", pointer, size);
        trace!("Output buffer: {:?}", v);

        let mut id = None;
        for (arg_id, argument) in self.arguments.iter() {
            if *argument == v {
                id = Some(*arg_id);
                break;
            }
        }

        if id.is_none() {
            // We didn't find the argument in the arguments registry. Add it in.
            let new_id = self.id_gen.next();
            self.arguments.insert(new_id, v);
            id = Some(new_id);
        }
        let id = id.unwrap();
        self.outputs.insert(id, (pointer as intptr_t, merger));
    }
}

// Methods for executing tasks.
impl TaskManager {
    /// Joins the `ArgumentId` and `SplitTypeParameter` for all registered tasks.
    ///
    /// The argument IDs are the arguments passed to the *first task* where this argument
    /// appeared. These argument IDs are needed in case the splitter requires other arguments.
    fn inputs(&self) -> InputMap {
        let mut map = FnvHashMap::default();
        for task in self.tasks.iter() {
            for (i, argument) in task.arguments.iter().enumerate() {
                let _ = map.entry(*argument)
                    .or_insert((task.arguments.clone(), task.annotation.params[i].clone()));
            }
        }
        map
    }

    /// Calls a task by assembling its arguments calling the task's function.
    ///
    /// If no argument map is passed, uses the default map constructed during task registration.
    /// Otherwise, a thread-local map should be passed, where the map associates argument IDs with
    /// split values.
    #[inline]
    unsafe fn call_task(&self,
                        task: &Task,
                        arguments: Option<&ArgumentMap>,
                        argument_buf: &mut Vec<u8>) -> Future {

        let arguments = arguments.unwrap_or(&self.arguments);
        argument_buf.clear();
        for id in task.arguments.iter() {
            let buf = arguments.get(id).unwrap();
            argument_buf.extend(buf.iter().cloned());
        }

        let start = Instant::now();
        let result = task.call(argument_buf.as_ptr() as *const c_void);
        let end = Instant::now();
        let duration = end.duration_since(start);

        trace!("task {} execution time: {}",
               task.annotation.function.func_name,
               duration.as_secs() as f64 + duration.subsec_nanos() as f64 * 1e-9);
        result
    }

    /// Returns initialized splitters for each argument and the number of items they produce.
    ///
    /// If the argument is constructed inside the pipeline or has a split type of `broadcast`, it
    /// won't have a splitter.
    ///
    /// Currently, this method also checks that each splitter returns the same number of items,
    /// though this requirement is not strictly necessary.
    unsafe fn init_splitters(&mut self, inputs: &InputMap) -> (SplitterMap, i64) {
        let mut num_items = vec![0; inputs.len()];
        let splitters = inputs
            .iter()
            .enumerate()
            .map(|(i, (arg_id, (task_args, split_type_param)))| {
                if split_type_param.ty.is_broadcast() {
                    trace!("No splitter for arg {:?} (broadcast)", arg_id);
                    num_items[i] = COMPOSER_INFINITE_ITEMS;
                    return (*arg_id, None);
                } else if self.futures.contains(arg_id) {
                    // don't split values that were returned by a function in the pipeline -
                    // they're already split.
                    trace!("No splitter for arg {:?} (returned from pipeline)", arg_id);
                    num_items[i] = COMPOSER_INFINITE_ITEMS;
                    return (*arg_id, None);
                }

                let runtime = split_type_param.ty.runtime_info();
                let splitter_init_fn = mem::transmute::<usize, SplitterInitFn>(runtime.initializer);
                let argval = self.arguments.get(arg_id).unwrap();

                // Construct the argument struct for the splitter initializer.
                // Note that this struct needs to be packed.
                let mut splitter_arg_struct = vec![];
                for arg_index in split_type_param.arguments.iter() {
                    let ref splitter_arg_id = task_args[*arg_index];
                    let ref data = self.arguments[splitter_arg_id];
                    splitter_arg_struct.extend(data.iter().cloned());
                }

                trace!("About to call splitter init for arg {:?} at pointer {:x}",
                       arg_id,
                       runtime.initializer);

                let arg_val_ptr = argval.as_ptr() as *const c_void;
                let init_args_ptr = splitter_arg_struct.as_ptr() as *const c_void;

                // Initialize the splitter.
                let splitter = (splitter_init_fn)(arg_val_ptr,
                                                  init_args_ptr,
                                                  num_items.as_mut_ptr().offset(i as isize));
                (*arg_id, Some(SendSyncPointer(splitter)))
            })
        .collect();

        // TODO this check should eventually be removed.
        let num_items = {
            let mut final_items = None;
            for item in num_items {
                if item != COMPOSER_INFINITE_ITEMS {
                    if let Some(final_items) = final_items {
                        assert_eq!(final_items, item);
                    } else {
                        final_items = Some(item);
                    }
                }
            }
            final_items.unwrap()
        };
        (splitters, num_items)
    }

    /// Returns a range that this thread should process.
    ///
    /// If this method returns `None`, the thread has no elements to process and should terminate.
    fn thread_range(&self, num_items: i64, tid: i64) -> Option<Range<i64>> {
        let mut thread_elements = num_items / self.threads;
        if thread_elements == 0 && tid != 0 {
            return None;
        }

        if thread_elements == 0 {
            // we are on thread 0.
            thread_elements = num_items;
        } else {
            // Adjust so we round up.
            thread_elements = (num_items / self.threads + ((num_items % self.threads != 0) as i64)) as i64;
        }

        let thread_start = thread_elements * tid;
        let thread_end = std::cmp::min(num_items, thread_elements * (tid + 1));
        Some(thread_start..thread_end)
    }

    /// Returns thread-local buffers for storing intermediate results for each argument.
    fn buffers(&self, inputs: &InputMap) -> ArgumentMap {
        let mut buffers: ArgumentMap = inputs
            .iter()
            .map(|(arg_id, (_, split_type_param))| {
                let runtime = split_type_param.ty.runtime_info();
                let output = (*arg_id, vec![0 as u8; runtime.data_size]);

                trace!(
                    "Assigning {:?} buffer of size {} (ptr={:?})",
                    arg_id,
                    runtime.data_size,
                    output.1.as_ptr()
                );

                // Values that are returned and passed around must be pointer-sized.
                if self.futures.contains(arg_id) {
                    assert_eq!(runtime.data_size, mem::size_of::<Future>());
                }
                output
            }).collect();

        for future_id in self.futures.iter() {
            buffers.entry(*future_id)
                .or_insert(vec![0 as u8; self.arguments[future_id].len()]);
        }
        buffers
    }

    /// Writes the next split values into buffers.
    ///
    /// Returns whether the driving loop should continue processing data depending on whether
    /// splitters have more data to produce.
    unsafe fn split_values(&self,
                    range: &Range<i64>,
                    inputs: &InputMap,
                    buffers: &mut ArgumentMap,
                    splitters: &SplitterMap) -> bool {

        for (id, splitter) in splitters.iter() {
            let (_, split_type_param) = inputs.get(id).unwrap();
            let runtime = split_type_param.ty.runtime_info();
            let buffer = buffers.get_mut(id).unwrap();

            let finished = if let Some(splitter) = splitter {
                let next_fn = mem::transmute::<usize, SplitterNextFn>(runtime.next);
                // Fill buffer with the split piece.
                trace!("Calling next(start={}, end={}) on argument {:?}", range.start, range.end, id);
                (next_fn)(
                    splitter.0,
                    range.start,
                    range.end,
                    buffer.as_mut_ptr() as *mut c_void
                )
            } else {
                // Broadcast the value.
                let argval = self.arguments.get(id).unwrap();
                buffer.copy_from_slice(argval);
                SplitterStatus::SplitterContinue
            };

            if finished == SplitterStatus::SplitterFinished {
                return false;
            }
        }

        true
    }

    /// Executes the tasks registered with the `TaskManager` so far.
    ///
    /// The task list is cleared afterwards.
    pub unsafe fn execute(&mut self) {
        if self.tasks.is_empty() {
            warn!("No tasks in execute: returning.");
            return;
        }

        self.evaluate = true;
        let inputs = self.inputs();

        let planner_start = Instant::now();

        debug!("{} inputs in execute", inputs.len());
        debug!("Inputs: {:?}", inputs);

        let (splitters, num_items) = self.init_splitters(&inputs);
        let tid = atomic::AtomicUsize::new(0);

        let thread_func = || {
            let tid = tid.fetch_add(1, atomic::Ordering::Relaxed) as i64;
            info!("Hello from thread {}", tid);

            let thread_range = if let Some(range) = self.thread_range(num_items, tid) {
                range
            } else {
                info!("no items to process: thread {} quitting!", tid);
                return ParallelPiece::new(tid, FnvHashMap::default());
            };

            info!("Thread {} assigned range ({}, {})", tid, thread_range.start, thread_range.end);

            let batch_size = std::cmp::min(
                thread_range.end - thread_range.start,
                self.init_task_size as i64
            );
            let mut current_range = thread_range.start..thread_range.start + batch_size;

            let mut buffers = self.buffers(&inputs);
            // A buffer where arguments for a task are written as a packed C struct - this buffer
            // is passed to the callback function.
            let mut argument_buf = Vec::with_capacity(64);
            let mut output_lists: FnvHashMap<ArgumentId, Vec<u8>> = FnvHashMap::default();
            let mut num_output_pieces = 0;

            let driver_start = Instant::now();

            let mut splitter_total_duration = Duration::new(0, 0);
            let mut task_total_duration = Duration::new(0, 0);

            'driver: loop {
                trace!("thread {} processing ({}, {})", tid, current_range.start, current_range.end);

                let split_start = Instant::now();
                if !self.split_values(&current_range, &inputs, &mut buffers, &splitters) {
                    info!("Thread {} finished driver loop", tid);
                    break 'driver;
                }

                let split_end = Instant::now();
                let task_start = split_end;
                splitter_total_duration = splitter_total_duration + split_end.duration_since(split_start);

                // Call each task on the split pieces.
                for (i, task) in self.tasks.iter().enumerate() {
                    trace!("Executing task {}: {}", i, task);
                    let task_result = self.call_task(task, Some(&buffers), &mut argument_buf);
                    if let Some(ref future_id) = task.future_id {
                        byteorder::LittleEndian::write_u64(
                            &mut buffers.get_mut(future_id).unwrap(),
                            task_result.0 as u64
                        );
                    }
                }

                // Add to the merger lists.
                for (output_id, _) in self.outputs.iter() {
                    let output_buf = output_lists.entry(*output_id).or_insert_with(|| vec![]);
                    output_buf.extend(buffers.get(output_id).unwrap().iter());
                }
                num_output_pieces += 1;

                let task_end = Instant::now();
                task_total_duration = task_total_duration + task_end.duration_since(task_start);

                current_range.start += batch_size;
                current_range.end = std::cmp::min(thread_range.end, current_range.start + batch_size);
                if current_range.start >= thread_range.end {
                    break 'driver;
                }
            }

            let driver_end = Instant::now();
            let driver_duration = driver_end.duration_since(driver_start);

            info!("thread {} total split time: {}",
                  tid, splitter_total_duration.as_secs() as f64 + splitter_total_duration.subsec_nanos() as f64 * 1e-9);
            info!("thread {} total task time: {}",
                  tid, task_total_duration.as_secs() as f64 + splitter_total_duration.subsec_nanos() as f64 * 1e-9);

            info!(
                "thread {} driver time: {} (produced {} pieces to merge)",
                tid,
                driver_duration.as_secs() as f64 + driver_duration.subsec_nanos() as f64 * 1e-9,
                num_output_pieces
            );

            let mut final_outputs = FnvHashMap::default();

            let merge_start = Instant::now();
            for (output_id, (_, merger)) in self.outputs.iter() {
                let merge_fn = mem::transmute::<usize, SplitterMergeFn>(*merger);
                let pieces = output_lists.get(output_id).unwrap().as_ptr() as *mut c_void;
                let output = (merge_fn)(pieces, num_output_pieces, self.threads);
                final_outputs.insert(*output_id, SendSyncPointer(output));
            }
            let merge_end = Instant::now();
            let merge_duration = merge_end.duration_since(merge_start);

            info!(
                "thread {} merge time: {}",
                tid,
                merge_duration.as_secs() as f64 + merge_duration.subsec_nanos() as f64 * 1e-9
            );
            ParallelPiece::new(tid, final_outputs)
        };

        log_time(planner_start, "Planner time");

        let mut pieces: Vec<ParallelPiece> = vec![];
        if self.threads == 1 {
            pieces.push(thread_func());
        } else {
            thread::scope(|s| {
                let mut threads = vec![];
                for _ in 0..self.threads {
                    threads.push(s.spawn(|_| thread_func()));
                }

                // These will be sorted due to the order of the join() calls.
                for thread in threads {
                    pieces.push(thread.join().unwrap());
                }
            }).unwrap();
        }

        let merge_start = Instant::now();
        pieces.sort_by_key(|p| p.tid);
        let mut merged = FnvHashMap::default();
        for piece in pieces {
            for (key, value) in piece.values {
                let entry = merged.entry(key).or_insert_with(Vec::new);
                entry.push(value);
            }
        }
        for (output_id, (loc, merger)) in self.outputs.clone() {
            let merge_fn = mem::transmute::<usize, SplitterMergeFn>(merger);
            let pieces = merged.get(&output_id).unwrap();

            let output = (merge_fn)(
                pieces.as_ptr() as *mut c_void,
                pieces.len() as i64,
                self.threads
            ) as intptr_t;

            // Write back the result.
            let loc = loc as *mut intptr_t;
            *loc = output;
        }
        let merge_end = Instant::now();
        let merge_duration = merge_end.duration_since(merge_start);

        info!(
            "final merge time: {}",
            merge_duration.as_secs() as f64 + merge_duration.subsec_nanos() as f64 * 1e-9
        );

        self.tasks.clear();
        self.futures.clear();
        self.outputs.clear();
        self.evaluate = false;
    }

}
