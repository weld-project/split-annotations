

from abc import ABC, abstractmethod
import copy

class SplitTypeError(TypeError):
    """ Custom type error for when annotation types cannot be propagated.

    Used for debugging so an exception may be raised during a pipeline break.

    """
    pass


class SplitType(ABC):
    """The base split type class.

    Other types should subclass this to define custom split types for a
    library.

    """

    def __init__(self):
        """Initialize a new split type."""
        pass

    def __hash__(self):
        return hash(str(self))

    def elements(self, value):
        """ Returns the number of elements that this value will emit.
        
        This function should return `None` if the splitter will emit elements
        indefinitely. The default implementation calls `len` on value. If this
        is not suitable, the split type should override this method.

        """
        return len(value)

    @abstractmethod
    def combine(self, values):
        """Combine a list of values into a single merged value.

        This method should be associative.

        Parameters
        ----------

        values : list
            A list of values that should be combined into a single value.

        Returns
        -------
        any
            Returns the combined value.

        """
        pass

    @abstractmethod
    def split(self, obj):
        """Returns disjoint split objects based on obj.
        
        split can return any iterable object, but will preferably return a
        generator that lazily yields split values from the source object.

        Parameters
        ----------

        obj : any
            An object that should be split into multiple objects of the same
            type.

        Returns
        -------
        iterable
            An iterable that collectively represents the input object.

        """
        pass

    def __eq__(self, other):
        """ Check whether two types are equal.

        Note that two split types are equal if (a) they have the same class
        name and (b) their attributes dictionary is equal.
        """
        if other is None:
            return False
        return self.__dict__ == other.__dict__() and\
                type(self).__name__ == type(other).__name__
    
    def __ne__(self, other):
        """ Check whether two types are not equal. """
        if other is None:
            return True
        return self.__dict__ != other.__dict__ or\
                type(self).__name__ != type(other).__name__


    def _sync_check_equal(self, other):
        """ Checks whether two types are equal and raises a SplitTypeError if
        they are not.

        Returns False otherwise (this function is only used in _sync).
        """
        if self != other:
            raise SplitTypeError("could not sync types {} and {}".format(self, other))
        else:
            return False

    def _sync(self, other):
        """ Enforce that two types are the same and returns whether anything
        changed.

        If the other type is a generic, this type is propagated to the generic
        type. Implementators of split types should not need to override this
        function.
        """
        if not isinstance(other, GenericType):
            return self._sync_check_equal(other)

        if other._concrete is None:
            other._concrete = copy.deepcopy(self)
            return True

        return self._sync_check_equal(other._concrete)

    def _finalized(self):
        """ Returns the finalized type of this type. """
        return self


class GenericType(SplitType):
    """A generic type that can be substituted with any other type.

    Generic types are the only ones that do not have an associated split and
    combine implementation.  Instead, these types are placeholders that are
    replaced with a concrete type before execution.
    
    """

    def __init__(self, name):
        """Creates a new generic named type with a given name.

        The given name is local to single annotation. Names across annotations
        do not have any meaning, but if two generic types in the same
        annotation have the same name, then they will be assigned the same
        concrete split type.

        """

        # Name of the generic (e.g., "A").
        self.name = name

        # Used for type inference to distinguish generics with the same name
        # but different concrete types.
        self._id = None;
        # Set of concrete types assigned to this generic type. After type inference,
        # the generic type is replaced with this.
        self._concrete = None

    def _sync(self, other):
        """ Several cases to handle here:

        1. Generic(ConcreteLeft), Other
            assert ConcreteLeft == Other
        2. Generic(None), Other
            self = Other
        3. Generic(ConcreteLeft), Generic(ConcreteRight)
            assert ConcreteLeft == ConcreteRight
        4. Generic(ConcreteLeft), Generic(None)
            other == ConcreteLeft
        5. Generic(None), Generic(ConcreteRight)
            self = ConcreteRight
        6. Generic(None), Generic(None)
            < no op >

        """
        if not isinstance(other, GenericType):
            if self._concrete is not None:
                # Case 1
                return self._concrete._sync_check_equal(other)
            else:
                # Case 2
                self._concrete = copy.deepcopy(other)
                return True

        if self._concrete is not None:
            if other._concrete is not None:
                # Case 3
                return self._concrete._sync_check_equal(other._concrete)
            else:
                # Case 4
                other._concrete = copy.deepcopy(self._concrete)
                return True
        else:
            if other._concrete is not None:
                # Case 5
                self._concrete = copy.deepcopy(other._concrete)
                return True
            else:
                # Case 6 (nothing happens)
                return False

    def _finalized(self):
        """ Converts non-finalized types into finalized types. """
        assert self._concrete is not None
        return self._concrete

    def __str__(self):
        suffix = ""
        if self._id is not None:
            suffix += "<{}>".format(self._id)
        if self._concrete is not None:
            suffix += "({})".format(self._concrete)
        return str(self.name) + suffix

    def combine(self, _):
        raise ValueError("Combiner called on generic split type")

    def split(self, _):
        raise ValueError("Split called on generic split type")


class Broadcast(SplitType):
    """ A split type that broadcasts values. """
    def __init__(self):
        pass

    def combine(self, values):
        """ A combiner that returns itself.

        Parameters
        ----------
            values : list
                A list that contains a single value.

        Returns
        -------
            any
                Returns ``values[0]``

        """
        if len(values) > 0:
            return values[0]

    def split(self, start, end, value):
        """ A splitter that returns the value unmodified.

        Parameters
        ----------
            start : int
                Unused.
            end : int
                Unused.
            value : any
                The value to broadcast
            
        Returns
        -------
            any
                Returns ``value``.

        """
        return value

    def elements(self, _):
        """ Returns ``None`` to indicate infinite elements."""
        return None

    def __str__(self): return "broadcast"

