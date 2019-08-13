
from .driver import STOP_ITERATION
from .instruction import Split

class Program:
    """
    A Composer Virtual Machine Program.

    A program stores a sequence of instructions to execute.

    """

    __slots__ = ["ssa_counter", "insts", "registered", "index"]

    def __init__(self):
        # Counter for registering instructions.
        self.ssa_counter = 0
        # Instruction list.
        self.insts = []
        # Registered values. Maps SSA value to real value.
        self.registered = {} 

    def get(self, value):
        """
        Get the SSA value for a value, or None if the value is not registered.

        value : The value to lookup

        """
        for num, val in self.registered.items():
            if value is val:
                return num

    def set_range_end(self, range_end):
        for inst in self.insts:
            if isinstance(inst, Split):
                inst.ty.range_end = range_end

    def step(self, thread, piece_start, piece_end, values, context):
        """
        Step the program and return whether are still items to process.
        """
        for task in self.insts:
            result = task.evaluate(thread, piece_start, piece_end, values, context)
            if isinstance(result, str) and result == STOP_ITERATION:
                return False
        return True

    def elements(self, values):
        """Returns the number of elements that this program will process.

        This quantity is retrieved by querying the Split instructions in the program.

        """
        elements = None
        for inst in self.insts:
            if isinstance(inst, Split):
                e = inst.ty.elements(values[inst.target])
                if e is None:
                    continue
                if elements is not None:
                    assert(elements == e, inst)
                else:
                    elements = e
        return elements

    def __str__(self):
        return "\n".join([str(i) for i in self.insts])
