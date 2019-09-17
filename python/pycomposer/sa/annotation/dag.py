
from collections import defaultdict, deque
import copy

from .annotation import Annotation
from .config import config
from .split_types import *
from .unevaluated import UNEVALUATED

from .vm.instruction import *
from .vm.vm import VM
from .vm import Program, Driver, STOP_ITERATION
from .vm.driver import DEFAULT_BATCH_SIZE

import functools

class Operation:
    """ A lazily evaluated computation in the DAG.

    Accessing any field of an operation will cause the DAG to execute. Operations
    keep a reference to their DAG to allow this.

    """

    def __init__(self, func, args, kwargs, annotation, owner_ref): 
        """Initialize an operation.

        Parameters
        __________

        func : the function to evaluate
        args : non-keyword arguments
        kwargs : keyword arguments
        annotation : a _mutable_ annotation object for this function
        owner_ref : reference to the DAG.

        """
        self.func = func
        self.args = args
        self.kwargs = kwargs
        self.annotation = annotation

        # Reference to the computed output.
        self._output = UNEVALUATED

        # The pipeline this operator is a part of.
        self.pipeline = None
        # Disable sending results. Hack.
        self.dontsend = False

        # Reference to the DAG object.
        self._owner_ref = owner_ref
        # Signals whether this is a root expression with no parent.
        self.root = True
        # Children of this operation that must be evaluated first.
        self.children = []

    def all_args(self):
        """ Returns a list of all the args in this operation. """
        return tuple(self.args) + tuple(self.kwargs.values())

    def mutable_args(self):
        """ Returns a list of all the mutable args in this operation. """
        mutables = []
        for (i, arg) in enumerate(self.args):
            if self.is_mutable(i):
                mutables.append(arg)

        for (key, value) in self.kwargs.items():
            if self.is_mutable(key):
                mutables.append(value)

        return mutables

    def split_type_of(self, index):
        """ Returns the split type of the argument with the given index.

        index can be a number to access regular arguments or a name to access
        keyword arguments.

        """
        if isinstance(index, int):
            return self.annotation.arg_types[index]
        elif isinstance(index, str):
            return self.annotation.kwarg_types[index]
        else:
            raise ValueError("invalid index {}".format(index))

    def is_mutable(self, index):
        """ Returns whether the argument at the given index is mutable. """
        return index in self.annotation.mutables
        
    def dependency_of(self, other):
        """ Returns whether self is a dependency of other. """
        if self in other.args:
            return True
        elif self in other.kwargs.values():
            return True
        else:
            # Check if any of our mutable arguments appear in other.
            # NOTE: This will currenty create n^2 edges where n is the number
            # of nodes that mutate an argument...
            mutable_args = self.mutable_args()
            for arg in other.all_args():
                for arg2 in mutable_args:
                    if arg is arg2:
                        return True
        return False

    @property
    def value(self):
        """ Returns the value of the operation.

        Causes execution of the DAG this operation is owned by, if a value has
        not been computed yet.

        """
        if self._output is UNEVALUATED:
            evaluate_dag(self._owner_ref)
        return self._output

    def _str(self, depth):
        s = "{}@sa({}){}(...) (pipeline {})".format(
                "  " * depth,
                self.annotation,
                self.func.__name__,
                self.pipeline)
        deps = []
        for dep in self.children:
            deps.append(dep._str(depth+1))
        for dep in deps:
            s += "\n{}".format(dep)
        return s

    def pretty_print(self):
        return "\n" + self._str(0)

    ## Magic Methods
    # NOTE: Some of these are broken (e.g., == won't evaluate right now since
    # its needed by Operation)

    def __eq__(self, other):
        """ Override equality to always check by reference. """
        if self._output is UNEVALUATED:
            return id(self) == id(other)
        else:
            return self._output == other

    def __hash__(self):
        """ Override equality to always check by reference. """
        if self._output is UNEVALUATED:
            return id(self)
        else:
            return hash(self._output)

    def __getitem__(self, key):
        return self.value.__getitem__(key)

    def __setitem__(self, key, value):
        return self.value.__getitem__(key, value)

    def __len__(self):
        return len(self.value)

    def __iter__(self):
        return self.value.__iter__()

    def __reversed__(self):
        return self.value.__reversed__()

    def __contains__(self, item):
        return self.value.__contains__(item)

    def __missing__(self, key):
        return self.value.__missing__(key)


class Future:
    """
    A wrapper class that causes lazy values to evaluate when any attribute of this
    class is accessed.
    """

    __slots__ = [ "operation", "value" ]
    def __init__(self, operation):
        self.operation = operation
        self.value = None


class LogicalPlan:
    """ A logical plan representing dataflow.

    The plan is evaluated from leaves to the root.
    
    """

    def __init__(self):
        """Initialize a logical DAG.
        
        The DAG is meant to be used as a singleton for registering tasks.

        """
        # Roots in the DAG.
        self.roots = []

    def clear(self):
        """ Clear the operators in this DAG by removing its nodes. """
        self.roots = []
    
    def register(self, func, args, kwargs, annotation):
        """ Register a function invocation along with its annotation.

        This method will clone the annotation since its types will eventually be modified to reflect
        concrete split types, in the case where some of the annotations are generics.

        Parameters
        __________

        func : the function to call.
        args : the non-keyword args of the function.
        kwargs : the keyworkd args of the function.
        annotation : the split type annotation of the function.

        Returns
        _______

        A lazy object representing a computation. Accessing the lazy object
        will cause the full computation DAG to be evaluated.

        """

        annotation = copy.deepcopy(annotation)
        operation = Operation(func, args, kwargs, annotation, self)

        def wire(op, newop):
            """ Wire the new operation into the DAG.

            Existing operations are children of the new operation if:

            1. The new operation uses an existing one as an argument.
            2. The new operation uses a value that is mutated by the
            existing operation.

            """
            if op is newop:
                return
            if op.dependency_of(newop):
                newop.children.append(op)
                newop.root = False
                # Don't need to recurse -- we only want the "highest" dependency.
                return True

        self.walk(wire, operation)

        # Update the roots.
        self.roots = [root for root in self.roots if not root.dependency_of(operation)]
        self.roots.append(operation)
        return operation

    def _walk_bottomup(self, op, f, context, visited):
        """ Recursive bottom up DAG walk implementation. """
        if op in visited:
            return
        visited.add(op)
        for dep in op.children:
            self._walk_bottomup(dep, f, context, visited)
        f(op, context)

    def walk(self, f, context, mode="topdown"):
        """ Walk the DAG in the specified order.

        Each node in the DAG is visited exactly once.

        Parameters
        __________

        f : A function to apply to each record. The function takes an operation
        and an optional context (i.e., any object) as arguments.

        context : An initial context.

        mode : The order in which to process the DAG. "topdown" (the default)
        traverses each node as its visited in breadth-first order. "bottomup"
        traverses the graph depth-first, so the roots are visited after the
        leaves (i.e., nodes are represented in "execution order" where
        dependencies are processed first).

        """

        if mode == "bottomup":
            for root in self.roots:
                self._walk_bottomup(root, f, context, set())
            return

        assert mode == "topdown"

        visited = set()
        queue = deque(self.roots[:])
        while len(queue) != 0:
            cur = queue.popleft()
            if cur not in visited:
                should_break = f(cur, context)
                visited.add(cur)
                if should_break is None:
                    for child in cur.children:
                        queue.append(child)

    def infer_types(self):
        """ Infer concrete types for each argument in the DAG. """
        
        def uniquify_generics(op, ident):
            """ Give each generic type an annotation-local identifier. """
            for ty in op.annotation.types():
                if isinstance(ty, GenericType):
                    ty._id = ident[0]
            ident[0] += 1

        def infer_locally(op, changed):
            """ Sync the annotation type in the current op with the annotations
            in its children.
            """
            try:
                for (i, argument) in enumerate(op.args):
                    if isinstance(argument, Operation) and argument in op.children:
                        split_type = op.split_type_of(i)
                        changed[0] |= split_type._sync(argument.annotation.return_type)

                for (name, argument) in op.kwargs.items():
                    if isinstance(argument, Operation) and argument in op.children:
                        split_type = op.split_type_of(name)
                        changed[0] |= split_type._sync(argument.annotation.return_type)

                # Sync types within a single annotation so all generics have the same types.
                # This will also update the return type.
                for ty in op.annotation.types():
                    for other in op.annotation.types():
                        if ty is other: continue
                        if isinstance(ty, GenericType) and isinstance(other, GenericType) and\
                                ty.name == other.name and ty._id == other._id:
                            changed[0] |= ty._sync(other)
                op.pipeline = changed[1]
            except SplitTypeError as e:
                print("Pipeline break: {}".format(e))
                changed[1] += 1
                op.pipeline = changed[1]

        def finalize(op, _):
            """ Replace generics with concrete types. """
            op.annotation.arg_types = list(map(lambda ty: ty._finalized(),
                    op.annotation.arg_types))
            if op.annotation.return_type is not None:
                op.annotation.return_type = op.annotation.return_type._finalized()
            for key in op.annotation.kwarg_types:
                op.annotation.kwarg_types[key] = op.annotation.kwarg_types[key]._finalized()

        self.walk(uniquify_generics, [0])

        # Run type-inference to fix point.
        while True:
            changed = [False, 0]
            self.walk(infer_locally, changed, mode="bottomup")
            if not changed[0]:
                break

        self.walk(finalize, None)

    def to_vm(self):
        """
        Convert the graph to a sequence of VM instructions that can be executed
        on worker nodes. One VM program is constructed per pipeline.

        Returns a list of VMs, sorted by pipeline.
        """
        def construct(op, vms):
            vm = vms[1][op.pipeline]
            added = vms[0]

            # Already processed this op.
            if op in added:
                return

            args = []
            kwargs = {}
            for (i, arg) in enumerate(op.args):
                valnum = vm.get(arg)
                if valnum is None:
                    valnum = vm.register_value(arg)
                    ty = op.split_type_of(i)
                    setattr(ty, "mutable", op.is_mutable(i))
                    vm.program.insts.append(Split(valnum, ty))
                args.append(valnum)

            for (key, value) in op.kwargs.items():
                valnum = vm.get(value)
                if valnum is None:
                    valnum = vm.register_value(value)
                    ty = op.split_type_of(key)
                    setattr(ty, "mutable", op.is_mutable(key))
                    vm.program.insts.append(Split(valnum, ty))
                kwargs[key] = valnum

            result = vm.register_value(op)
            # In this context, mutability just means we need to merge objects.
            if op.annotation.return_type is not None:
                setattr(op.annotation.return_type, "mutable", not op.dontsend)
            vm.program.insts.append(Call(result, op.func, args, kwargs, op.annotation.return_type))
            added.add(op)

        # programs: Maps Pipeline IDs to VM Programs.
        # arg_id_to_ops: Maps Arguments to ops. Store separately so we don't serialize ops.
        vms = (set(), defaultdict(lambda: VM()))
        self.walk(construct, vms, mode="bottomup")
        return sorted(list(vms[1].items()))


    @staticmethod
    def commit(values, results):
        """
        Commit outputs into the DAG nodes so programs can access data.
        """
        for (arg_id, value) in values.items():
            if isinstance(value, Operation):
                value._output = results[arg_id]

    def __str__(self):
        roots = []
        for root in self.roots:
            roots.append(root.pretty_print())
        return "\n".join(roots)


def evaluate_dag(dag, workers=config["workers"], batch_size=config["batch_size"], profile=False):
    try:
        dag.infer_types()
    except (SplitTypeError) as e:
        print(e)

    vms = dag.to_vm()
    for _, vm in vms:

        # print(vm.program)

        driver = Driver(workers=workers, batch_size=batch_size, optimize_single=True, profile=profile)
        results = driver.run(vm.program, vm.values)

        dag.commit(vm.values, results)
        # TODO We need to update vm.values in the remaining programs to use the
        # materialized data in _DAG.operation.
        #
        # If in the future we see something like "object dag.Operation does not
        # have method <annotated method>" in a multi-stage program, that's why!
        pass

    dag.clear()
