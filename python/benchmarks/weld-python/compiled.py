"""
"""

from bindings import *
# from bindings_latest import *
import weldtypes

import ctypes
import time

import numpy as np

# Global num threads setting.
THREADS = [ "1" ]

class WeldEncoder(object):
    """
    An abstract class that must be overwridden by libraries. This class
    is used to marshall objects from Python types to Weld types.
    """
    def encode(obj):
        """
        """
        raise NotImplementedError

    def py_to_weld_type(self, obj):
        raise NotImplementedError


class WeldDecoder(object):
    """
    An abstract class that must be overwridden by libraries. This class
    is used to marshall objects from Weld types to Python types.
    """
    def decode(obj, restype):
        """
        Decodes obj, assuming object is of type `restype`. obj's Python
        type is ctypes.POINTER(restype.ctype_class).
        """
        raise NotImplementedError

# Returns a wrapped ctypes Structure
def args_factory(arg_names, arg_types):
    class Args(ctypes.Structure):
        _fields_ = list(zip(arg_names, arg_types))
    return Args

def compile(program, arg_types, restype, decoder, verbose=False):
    """Compiles a program and returns a function for calling it.

    Parameters
    ----------

    program : a string representing a Weld program.
    arg_types : a tuple of (type, encoder)
    decoder : a decoder for the returned value.
    """

    start = time.time()

    conf = WeldConf()
    err = WeldError()
    module = WeldModule(program, conf, err)
    if err.code() != 0:
        raise ValueError("Could not compile function: {}".format(err.message()))
    end = time.time()

    if verbose:
        print("Weld compile time:", end - start)

    def func(*args):
        # Field names.
        names = []
        # C type of each argument.
        arg_c_types = []
        # Encoded version of each argument.
        encoded = []

        for (i, (arg, arg_type)) in enumerate(zip(args, arg_types)):
            names.append("_{}".format(i))
            if isinstance(arg_type, WeldEncoder):
                arg_c_types.append(arg_type.py_to_weld_type(arg).ctype_class)
                encoded.append(arg_type.encode(arg))
            else:
                # Primitive type with a builtin encoder
                assert isinstance(arg, arg_type)
                ctype = weldtypes.encoder(arg_type)
                arg_c_types.append(ctype)
                encoded.append(ctype(arg))

        Args = args_factory(names, arg_c_types)
        raw_args = Args()

        for name, value in zip(names, encoded):
            setattr(raw_args, name, value)

        raw_args_pointer = ctypes.cast(ctypes.byref(raw_args), ctypes.c_void_p)
        weld_input = WeldValue(raw_args_pointer)
        conf = WeldConf()

        # 100GB Memory limit
        conf.set("weld.memory.limit", "100000000000")
        conf.set("weld.threads", THREADS[0])

        err = WeldError()

        result = module.run(conf, weld_input, err)
        if err.code() != 0:
            raise ValueError("Error while running function: {}".format(err.message()))

        pointer_type = POINTER(restype.ctype_class)
        data = ctypes.cast(result.data(), pointer_type)
        result = decoder.decode(data, restype)

        return result

    return func
