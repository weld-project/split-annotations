"""
Parser for log data for system overhead breakdown.
"""

class ThreadTime(object):
    def __init__(self, thread):
        self.thread = thread
        self.split_time = None
        self.driver_time = None
        self.merge_time = None

    @property
    def task_time(self):
        return self.driver_time - self.split_time

    def this_thread(self, line):
        return line.find("thread {}".format(self.thread)) != -1

    def parse(self, line):
        if self.this_thread(line):
            self.parse_split_time(line)
            self.parse_driver_time(line)
            self.parse_merge_time(line)

    def parse_split_time(self, line):
        if self.split_time is not None:
            return
        if line.find("total split time: ") != -1:
            self.split_time = float(line.split("total split time: ")[1].strip())

    def parse_driver_time(self, line):
        if self.driver_time is not None:
            return
        if line.find("driver time: ") != -1:
            self.driver_time = float(line.split("driver time: ")[1].split(' ')[0].strip())

    def parse_merge_time(self, line):
        if self.merge_time is not None:
            return
        if line.find("merge time: ") != -1:
            self.merge_time = float(line.split("merge time: ")[1].strip())

    def __str__(self):
        return "{},{},{}".format(self.split_time, self.task_time, self.merge_time)

class Run(object):
    def __init__(self, threads, run_delimiter):
        self.threads = threads
        self.run_delimiter = run_delimiter
        self.unprotect = None
        self.planner = None
        self.final_merge = None
        self.thread_times = [ThreadTime(i) for i in xrange(threads)]

    def parse(self, line):
        self.parse_unprotect(line)
        self.parse_planner(line)
        for thread in self.thread_times:
            thread.parse(line)
        self.parse_final_merge(line)

    def parse_unprotect(self, line):
        if self.unprotect is not None:
            return
        if line.find("Unprotect memory: ") != -1:
            self.unprotect = float(line.split("Unprotect memory: ")[1].strip())

    def parse_planner(self, line):
        if self.planner is not None:
            return
        if line.find("Planner time: ") != -1:
            self.planner = float(line.split("Planner time: ")[1].strip())

    def parse_final_merge(self, line):
        if self.final_merge is not None:
            return
        if line.find("final merge time: ") != -1:
            self.final_merge = float(line.split("final merge time: ")[1].strip())

    def finished(self, line):
        if line.find(self.run_delimiter) != -1:
            return True
        else:
            return False

    def __str__(self):
        average_split_time = max([t.split_time for t in self.thread_times])
        average_task_time = max([t.task_time for t in self.thread_times])
        average_merge_time = max([t.merge_time for t in self.thread_times]) + self.final_merge
        return "{:.5f},{:.5f},{:.5f},{:.5f},{:.5f}".format(self.unprotect, self.planner, average_split_time, average_task_time, average_merge_time)

def parse(filename, threads, run_delimiter):
    current_run = Run(threads, run_delimiter)
    print "unprotect,planner,split,task,merge"
    with open(filename) as f:
        for line in f:
            if current_run.finished(line):
                print current_run
                current_run = Run(threads, run_delimiter)
            else:
                current_run.parse(line)

# black scholes
# parse("blackscholes/breakdown-results/mklcomposer.stderr", 16, "First put value")

parse("nashville/breakdown-results/composer.stderr", 16, "image size:")
