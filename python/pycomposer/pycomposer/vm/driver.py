
from collections import defaultdict
import pickle

import multiprocessing
#import multiprocessing.dummy as multiprocessing

import threading
import time

STOP_ITERATION = "stop"

# Global reference to values. These should be in read-only shared memory with
# all child processes.
_VALUES = None
# Global reference to program currently being executed.
_PROGRAM = None
# Batch size to use.
_BATCH_SIZE = None

# Size of the L2 Cache (TODO read this from somewhere)
CACHE_SIZE = 252144
# Default batch size if we don't know anything
DEFAULT_BATCH_SIZE = 4096 * 4 * 4

def _worker(worker_id, index_range):
    """
    A multiprocessing worker.

    The index indicates the "thread ID" of the worker, and the queue is a
    channel with which the driver program sends this worker programs to
    execute.

    Parameters
    ----------

    worker_id : the thread ID of this worker.
    index_range : A range 
    and the master.

    """
    result = _run_program(worker_id, index_range)
    return result

def _run_program(worker_id, index_range):
    """Runs the global program to completion and return partial values.
    
    Parameters
    ----------

    worker_id : the ID of this worker.
    program : the program to execute.
    """
    global _VALUES
    global _PROGRAM
    global _BATCH_SIZE

    print("Thread", worker_id, "range:", index_range, "batch size:", _BATCH_SIZE)
    start = time.time()

    context = defaultdict(list)
    just_parallel = False
    if just_parallel:
        _BATCH_SIZE = index_range[1] - index_range[0]
        piece_start = index_range[0]
        piece_end = index_range[1] 
    else:
        piece_start = index_range[0]
        piece_end = piece_start + _BATCH_SIZE

    _PROGRAM.set_range_end(index_range[1])

    while _PROGRAM.step(worker_id, piece_start, piece_end, _VALUES, context):
        piece_start += _BATCH_SIZE
        piece_end += _BATCH_SIZE
        # Clamp to the range assigned to this thread.

        if piece_start >= index_range[1]:
            break
        elif piece_end > index_range[1]:
            piece_end = index_range[1]

    process_end = time.time()

    # Free non-shared memory on this worker.
    _merge(_PROGRAM, context)

    merge_end = time.time()

    print("Thread {}\t processing: {:.3f}\t merge: {:.3f}\t total:{:.3f}\t".format(
            worker_id,
            process_end - start,
            merge_end - process_end,
            merge_end - start))

    return context

def _merge(program, context):
    """
    Merge a context that was generated with the given program.
    """
    merged = set()
    for inst in reversed(program.insts):
        # Reverse list to obtain the last split type assigned to each target.
        # For now, given the way pipelines are set up, this shouldn't matter
        # since the split type of each target won't change.
        if inst.target in merged:
            continue
        else:
            merged.add(inst.target)
            if inst.ty is not None:
                if inst.ty.mutable:
                    context[inst.target] = inst.ty.combine(context[inst.target])
                else:
                    # No need to merge values and send the result back: it's immutable,
                    # and should not have changed on the master process.
                    context[inst.target] = None

class Driver:
    """
    Parallel driver and scheduler for the virtual machine.
    """

    __slots__ = [ "workers", "batch_size", "optimize_single", "profile" ]

    def __init__(self, workers=1, batch_size=DEFAULT_BATCH_SIZE, optimize_single=True, profile=False):
        self.workers = workers
        self.batch_size = batch_size
        self.optimize_single = optimize_single
        self.profile = profile
        if self.profile:
            assert self.workers == 1, "Profiling only supported on single thread"
            assert self.optimize_single, "Profiling only supported with optimize_single=True"

    def get_partitions(self, total_elements):
        """ Returns a list of index ranges to process for each worker. """
        ranges = []
        for tid in range(self.workers):
            elements = total_elements // self.workers
            if elements == 0 and tid != 0:
                ranges.append(None)
                continue

            if elements == 0:
                # Thread 0
                elements = total_elements
            else:
                # Round up
                elements = total_elements //\
                        self.workers + int(total_elements % self.workers != 0)

            thread_start = elements * tid
            thread_end = min(total_elements, elements * (tid + 1))
            ranges.append((thread_start, thread_end))

        return ranges
        
    def run(self, program, values):
        """ Executes the program with the provided values. """
        elements = program.elements(values)
        ranges = self.get_partitions(elements)

        # Make the values accessible to child processes.
        global _VALUES
        global _PROGRAM
        global _BATCH_SIZE

        _VALUES = values
        _PROGRAM = program
        _BATCH_SIZE = self.batch_size

        if self.workers == 1 and self.optimize_single:
            if self.profile:
                import cProfile
                import sys
                cProfile.runctx("_run_program(0, ranges[0])", globals(), locals())
                print("Finished profiling! exiting...")
                sys.exit(1)
            result = _run_program(0, ranges[0])
        elif self.workers > 1 and ranges[1] is None:
            # We should really dynamically adjust the number of processes
            # (i.e., self.workers should be the maximum allowed workers), but
            # for now its 1 or all to make evaluation easier.
            result = _run_program(0, ranges[0])
        else:
            # This needs to go after the assignment to _VALUES, so
            # the process snapshot sees the updated variable. The advantage of
            # this approach is copy-on-write semantics on POSIX systems for
            # (potentially large) inputs. I'm not sure what the Python
            # interpreter does, but assuming it's sane, the underlying objects
            # should never be written to, hence preventing the copy. The big
            # disadvantage of this approach is that we need to incur a
            # process-start overhead every time...
            pool = multiprocessing.Pool(self.workers)

            # TODO Just use Pool.imap instead?
            partial_results = []
            for (i, index_range) in enumerate(ranges):
                partial_results.append(pool.apply_async(_worker, args=(i, index_range)))
            for i in range(len(partial_results)):
                partial_results[i] = partial_results[i].get()

            result = defaultdict(list)
            start = time.time()
            if self.workers > 1:
                for partial_result in partial_results:
                    if partial_result is not None:
                        for (key, value) in partial_result.items():
                            # Don't add unnecessary None values.
                            if value is not None:
                                result[key].append(value)
                
                _merge(program, result)

                # Reinstate non-mutable values, broadcast values, etc.
                for value_key in _VALUES:
                    if value_key not in result:
                        result[value_key] = _VALUES[value_key]
            else:
                result = partial_results[0]

            end = time.time()
            print("Final merge time:", end - start)
            pool.terminate()

        _VALUES = None
        _PROGRAM = None
        _BATCH_SIZE = None

        return result

