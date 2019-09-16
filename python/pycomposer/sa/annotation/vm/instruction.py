
from abc import ABC, abstractmethod
import types

from .driver import STOP_ITERATION

class Instruction(ABC):
    """
    An instruction that updates an operation in a lazy DAG.
    """

    @abstractmethod
    def evaluate(self, thread, start, end, values, context):
        """
        Evaluates an instruction.

        Parameters
        ----------

        thread : the thread that is  currently executing
        start : the start index of the current split value.
        end : the end index of the current split value
        values : a global value map holding the inputs.
        context : map holding execution state (arg ID -> value).

        """
        pass

class Split(Instruction):
    """
    An instruction that splits the inputs to an operation.
    """

    def __init__(self, target, ty):
        """
        A Split instruction takes an argument and split type and applies
        the splitter on the argument.

        Parameters
        ----------

        target : the arg ID that will be split.
        ty : the split type.
        """
        self.target = target
        self.ty = ty
        self.splitter = None

    def __str__(self):
        return "v{} = split {}:{}".format(self.target, self.target, self.ty)

    def evaluate(self, thread, start, end, values, context):
        """ Returns values from the split. """

        if self.splitter is None:
            # First time - check if the splitter is actually a generator.
            result = self.ty.split(start, end, values[self.target])
            if isinstance(result, types.GeneratorType):
                self.splitter = result
                result = next(self.splitter)
            else:
                self.splitter = self.ty.split
        else:
            if isinstance(self.splitter, types.GeneratorType):
                result = next(self.splitter)
            else:
                result = self.splitter(start, end, values[self.target])

        if isinstance(result, str) and result == STOP_ITERATION:
            return STOP_ITERATION
        else:
            context[self.target].append(result)

class Call(Instruction):
    """ An instruction that calls an SA-enabled function. """
    def __init__(self,  target, func, args, kwargs, ty):
        self.target = target
        # Function to call.
        self.func = func
        # Arguments: list of targets.
        self.args = args
        # Keyword arguments: Maps { name -> target }
        self.kwargs = kwargs
        # Return split type.
        self.ty = ty

    def __str__(self):
        args = ", ".join(map(lambda a: "v" + str(a), self.args))
        kwargs = list(map(lambda v: "{}=v{}".format(v[0], v[1]), self.kwargs.items()))
        arguments = ", ".join([args] + kwargs)
        return "v{} = call {}({}):{}".format(self.target, self.func.__name__, arguments, str(self.ty))

    def get_args(self, context):
        return [ context[target][-1] for target in self.args ]

    def get_kwargs(self, context):
        return dict([ (name, context[target][-1]) for (name, target) in self.kwargs.items() ])

    def evaluate(self, _thread, _start, _end, _values, context):
        """
        Evaluates a function call by gathering arguments and calling the
        function.
        
        """
        args = self.get_args(context)
        kwargs = self.get_kwargs(context)
        context[self.target].append(self.func(*args, **kwargs))
