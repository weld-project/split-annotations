""" Convinience methods to create generic split types.

A generic split type can take on the value of any other split type during
execution. Generics with the same name (e.g., ``A``) are always assigned the
same concrete split type.  Generic names are local to a single SA (i.e., two
SAs which use the generic ``A`` could have the ``A`` assigned different
concrete split types). Generic split types are assigned concrete split types via *type inference*.

Note that, if a generic split type cannot be assigned a concrete split type at
execution time, an error will be thrown.

"""

from .split_types import GenericType

A = lambda: GenericType("A")
B = lambda: GenericType("B")
C = lambda: GenericType("C")
D = lambda: GenericType("D")
E = lambda: GenericType("E")
F = lambda: GenericType("F")
G = lambda: GenericType("G")
H = lambda: GenericType("H")
I = lambda: GenericType("I")
J = lambda: GenericType("J")
K = lambda: GenericType("K")
L = lambda: GenericType("L")
M = lambda: GenericType("M")
N = lambda: GenericType("N")
O = lambda: GenericType("O")
P = lambda: GenericType("P")
Q = lambda: GenericType("Q")
R = lambda: GenericType("R")
S = lambda: GenericType("S")
T = lambda: GenericType("T")
U = lambda: GenericType("U")
V = lambda: GenericType("V")
W = lambda: GenericType("V")
X = lambda: GenericType("X")
Y = lambda: GenericType("Y")
Z = lambda: GenericType("Z")
