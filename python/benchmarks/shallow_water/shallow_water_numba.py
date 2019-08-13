
import numpy as np
from numba import njit

import argparse
import sys
import time

@njit(parallel=True)
def spatial_derivative(A, axis=0):
    """
    Compute derivative of array A using balanced finite differences
    Axis specifies direction of spatial derivative (d/dx or d/dy)

    dA[i] =  A[i+1] - A[i-1]   / 2
    ... or with grid spacing included ...
    dA[i]/dx =  A[i+1] - A[i-1]   / 2dx

    Used By:
        d_dx
        d_dy
    """
    return (np.roll(A, -1) - np.roll(A, 1)) / (grid_spacing*2.)

@njit(parallel=True)
def d_dx(A):
    return spatial_derivative(A, 1)

@njit(parallel=True)
def d_dy(A):
    return spatial_derivative(A, 0)

@njit(parallel=True)
def d_dt(eta, u, v, g, b=0):
    """
    http://en.wikipedia.org/wiki/Shallow_water_equations#Non-conservative_form
    """

    du_dt = -g*d_dx(eta) - b*u
    dv_dt = -g*d_dy(eta) - b*v

    H = 0#eta.mean() - our definition of eta includes this term
    deta_dt = -d_dx(u * (H+eta)) - d_dy(v * (H+eta))

    eta = eta + deta_dt * dt
    u = u + du_dt * dt
    v = v + dv_dt * dt
    return eta, u, v


def evolveEuler(eta, u, v, g, dt):
    """
    Evolve state (eta, u, v, g) forward in time using simple Euler method
    x_{n+1} = x_{n} +   dx/dt * d_t

    Returns an generator / infinite list of all states in the evolution
    """
    elapsedTime = 0
    yield eta, u, v, elapsedTime # return initial conditions as first state in sequence

    while(True):
        eta, u, v = d_dt(eta, u, v, g)
        elapsedTime += dt
        yield eta, u, v, elapsedTime

def simulate(eta, u, v, g, dt, iterations):

    trajectory = evolveEuler(eta, u, v, g, dt)

    # Figure with initial conditions

    start = time.time()

    eta, u, v, elapsedTime = next(trajectory)
    for i in range(iterations):
        eta, u, v, elapsedTime = next(trajectory)
        print(eta[0][0])

    end = time.time()
    print("total time:", end - start)

    print("Final State:")
    print(eta[0][0])


####################################################################
#                            ENTRY POINT
####################################################################

parser = argparse.ArgumentParser(
    description="Shallow Water benchmark."
)
parser.add_argument('-s', "--size", type=int, default=10, help="Size of each array")
parser.add_argument('-i', "--iterations", type=int, default=1, help="Iterations of simulation")
parser.add_argument('-p', "--piece_size", type=int, default=16384, help="Size of each piece.")
parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
parser.add_argument('-v', "--verbosity", type=str, default="none",\
        help="Log level (debug|info|warning|error|critical|none)")
args = parser.parse_args()

size = (1 << args.size)
iterations = args.iterations 
piece_size = args.piece_size
threads = args.threads
loglevel = args.verbosity

assert threads >= 1

print("Size:", size)
print("Piece Size:", piece_size)
print("Threads:", threads)
print("Log Level", loglevel)
print("Mode:", "Numba")

sys.stdout.write("Generating data...")
sys.stdout.flush()

# Initial Conditions
n = size

# velocity in x direction
u = np.zeros((n, n))
# velocity in y direction
v = np.zeros((n, n))
# pressure deviation (like height)
eta = np.ones((n, n))

# Set eta.
for i in range(n):
    eta[i] *= 0.1 * i

# Constants
G     = np.float64(6.67384e-11)     # m/(kg*s^2)
dt    = np.float64(60*60*24*365.25) # Years in seconds
r_ly  = np.float64(9.4607e15)       # Lightyear in m
m_sol = np.float64(1.9891e30)       # Solar mass in kg
b     = np.float64(0.0)

box_size = 1.
grid_spacing =  1.0 * box_size / n
g = 1.
dt = grid_spacing / 100.
print("done.")

simulate(eta, u, v, g, dt, iterations)
