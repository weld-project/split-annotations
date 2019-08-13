
from .program import Program

class VM:
    """
    A Composer virtual machine, which holds a program and its associated data.
    """
    def __init__(self):
        # Counter for argument IDs
        self.ssa_counter = 0
        # Program
        self.program = Program()
        # Values, mapping argID -> values
        self.values = dict()

    def get(self, value):
        """
        Get the SSA value for a value, or None if the value is not registered.

        value : The value to lookup

        """
        for num, val in self.values.items():
            if value is val:
                return num

    def register_value(self, value):
        """
        Register a counter to a value.
        """
        arg_id = self.ssa_counter
        self.ssa_counter += 1
        self.values[arg_id] = value
        return arg_id

