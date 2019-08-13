from ctypes import *

def encoder(ty):
    if ty == int:
        return c_long
    elif ty == float:
        return c_double
    elif ty == str:
        return c_char_p
    raise ValueError

class WeldType(object):
    def __str__(self):
        return "type"

    def __hash__(self):
        return hash(str(self))

    def __eq__(self, other):
        return hash(other) == hash(self)

    @property
    def ctype_class(self):
        raise NotImplementedError


class WeldChar(WeldType):
    def __str__(self):
        return "i8"

    @property
    def ctype_class(self):
        return c_wchar_p


class WeldBit(WeldType):
    def __str__(self):
        return "bool"

    @property
    def ctype_class(self):
        return c_bool


class WeldInt(WeldType):

    def __str__(self):
        return "i32"

    @property
    def ctype_class(self):
        return c_int


class WeldLong(WeldType):

    def __str__(self):
        return "i64"

    @property
    def ctype_class(self):
        return c_long


class WeldFloat(WeldType):

    def __str__(self):
        return "f32"

    @property
    def ctype_class(self):
        return c_float


class WeldDouble(WeldType):

    def __str__(self):
        return "f64"

    @property
    def ctype_class(self):
        return c_double


class WeldVec(WeldType):
    # Kind of a hack, but ctypes requires that the class instance returned is
    # the same object. Every time we create a new Vec instance (templatized by
    # type), we cache it here.
    _singletons = {}

    def __init__(self, elemType):
        self.elemType = elemType

    def __str__(self):
        return "vec[%s]" % str(self.elemType)

    @property
    def ctype_class(self):
        def vec_factory(elemType):
            class Vec(Structure):
                _fields_ = [
                    ("ptr", POINTER(elemType.ctype_class)),
                    ("size", c_long),
                ]
            return Vec

        if self.elemType not in WeldVec._singletons:
            WeldVec._singletons[self.elemType] = vec_factory(self.elemType)
        return WeldVec._singletons[self.elemType]


class WeldStruct(WeldType):
    _singletons = {}

    def __init__(self, field_types):
        assert False not in [isinstance(e, WeldType) for e in field_types]
        self.field_types = field_types

    def __str__(self):
        return "{" + ",".join([str(f) for f in self.field_types]) + "}"

    @property
    def ctype_class(self):
        def struct_factory(field_types):
            class Struct(Structure):
                _fields_ = [("_" + str(i), t.ctype_class) for i, t in enumerate(field_types)]
            return Struct

        if frozenset(self.field_types) not in WeldVec._singletons:
            WeldStruct._singletons[
                frozenset(self.field_types)] = struct_factory(self.field_types)
        return WeldStruct._singletons[frozenset(self.field_types)]
