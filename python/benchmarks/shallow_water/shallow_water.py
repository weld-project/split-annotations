
import argparse
import numpy
import sys
import time

# Does this cause some weird overriding behavior in NumPy?
import bohrium

sys.path.append("../../lib")
sys.path.append("../../pycomposer")

# Total time, roll time
time_in_sd = [ 0.0, 0.0 ]

batch_size = 2

def spatial_derivative(A, axis, out, threads):
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

    if mode == "composer":
        np.evaluate(workers=threads, batch_size=batch_size)

    start = time.time()
    # XXX Avoid writing roll_1 and roll_2 across threads since they don't
    # reside in shared memory...
    roll_1 = np.roll(A, -1, axis=axis)
    roll_2 = np.roll(A, 1, axis=axis)

    end = time.time()
    time_in_sd[1] += (end - start)

    np.subtract(roll_1, roll_2, out=out)
    np.divide(out, grid_spacing * 2.0, out=out)

    end = time.time()
    time_in_sd[0] += (end - start)

def d_dx(A, out, threads):
    spatial_derivative(A, 1, out, threads)

def d_dy(A, out, threads):
    spatial_derivative(A, 0, out, threads)

def d_dt(eta, u, v, temporaries, threads):
    """
    http://en.wikipedia.org/wiki/Shallow_water_equations#Non-conservative_form
    """

    tmp, du_dt, dv_dt, tmp1 = temporaries

    # tmp1 = d_dx(eta)
    d_dx(eta, tmp1, threads)
    d_dy(eta, tmp, threads)

    # du_dt = -g*d_dx(eta) - b*u
    np.multiply(-g, tmp1, out=du_dt)
    np.multiply(b, u, out=tmp1)
    np.subtract(du_dt, tmp1, out=du_dt)

    # dv_dt = -g*d_dy(eta) - b*v
    np.multiply(-g, tmp, out=dv_dt)
    np.multiply(b, v, out=tmp)
    np.subtract(dv_dt, tmp, out=dv_dt)

    H = 0

    # deta_dt = -d_dx(u * (H+eta)) - d_dy(v * (H+eta))
    np.add(H, eta, out=tmp)
    np.multiply(u, tmp, out=tmp1)
    np.multiply(v, tmp, out=tmp)

    ddx = d_dx(tmp1, tmp1, threads)
    ddy = d_dy(tmp, tmp, threads)

    np.multiply(-1, tmp1, out=tmp1)
    np.subtract(tmp1, tmp, out=tmp)

    return tmp, du_dt, dv_dt

def evolveEuler(eta, u, v, temporaries, threads):
    """
    Evolve state (eta, u, v, g) forward in time using simple Euler method
    x_{n+1} = x_{n} +   dx/dt * d_t

    Returns an generator / infinite list of all states in the evolution
    """

    elapsed = 0
    yield eta, u, v, elapsed # return initial conditions as first state in sequence

    while(True):
        deta_dt, du_dt, dv_dt = d_dt(eta, u, v, temporaries, threads)

        _, _, _, tmp = temporaries

        # eta = eta + deta_dt * dt
        np.multiply(deta_dt, dt, out=tmp)
        np.add(eta, tmp, out=eta)

        # u = u + du_dt * dt
        np.multiply(du_dt, dt, out=tmp)
        np.add(u, tmp, out=u)

        # v = v + dv_dt * dt
        np.multiply(dv_dt, dt, out=tmp)
        np.add(v, tmp, out=v)

        if composer:
            np.evaluate(workers=threads, batch_size=batch_size)

        elapsed += dt
        yield eta, u, v, elapsed

def simulate(eta, u, v, temporaries, iterations, threads):

    trajectory = evolveEuler(eta, u, v, temporaries, threads)

    start = time.time()

    # Emit initial conditions.
    eta, u, v, elapsed = next(trajectory)
    if mode == "bohrium":
        np.flush()

    for i in range(iterations):
        eta, u, v, elapsed = next(trajectory)
        if mode == "bohrium":
            np.flush()
        print(eta[0][0])

    end = time.time()
    print("Total time:", end - start, "Time in Derivative:", time_in_sd[0], "Time in roll:", time_in_sd[1])

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
parser.add_argument('-m', "--mode", type=str, default="naive", help="Mode (composer|naive)")
args = parser.parse_args()

size = (1 << args.size)
iterations = args.iterations 
piece_size = args.piece_size
threads = args.threads
loglevel = args.verbosity
mode = args.mode.strip().lower()

assert threads >= 1

print("Size:", size)
print("Piece Size:", piece_size)
print("Threads:", threads)
print("Log Level", loglevel)
print("Mode:", mode)

if mode == "composer":
    composer = True
elif mode == "bohrium":
    composer = False
elif mode == "naive":
    composer = False
else:
    raise ValueError("invalid mode", mode)

if composer:
    import composer_numpy as np
elif mode == "bohrium":
    import bohrium as np
else:
    import numpy as np

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

# tmp, du_dt, dv_dt
temporaries = (
        np.ones((n, n)),
        np.ones((n, n)),
        np.ones((n, n)),
        np.ones((n, n))
        )

# Set eta.
for i in range(n):
    eta[i] *= 0.1 * i

# Constants
G     = numpy.float64(6.67384e-11)     # m/(kg*s^2)
dt    = numpy.float64(60*60*24*365.25) # Years in seconds
r_ly  = numpy.float64(9.4607e15)       # Lightyear in m
m_sol = numpy.float64(1.9891e30)       # Solar mass in kg
b     = numpy.float64(0.0)

box_size = 1.
grid_spacing =  1.0 * box_size / n
g = 1.
dt = grid_spacing / 100.
print("done.")

simulate(eta, u, v, temporaries, iterations, threads)
