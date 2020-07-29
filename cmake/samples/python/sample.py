from ortools.linear_solver import pywraplp
from ortools.linear_solver import linear_solver_pb2
from ortools.constraint_solver import pywrapcp
from ortools.sat import pywrapsat
from ortools.graph import pywrapgraph
from ortools.algorithms import pywrapknapsack_solver
from ortools.data import pywraprcpsp

def lpsolver():
    print('Test lpsolver...')
    lpsolver = pywraplp.Solver(
            'LinearTest',
            pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
    lpsolver.Solve()
    print('Test lpsolver...DONE')

def cpsolver():
    print('Test cpsolver...')
    cpsolver = pywrapcp.Solver('ConstraintTest')
    num_vals = 3
    x = cpsolver.IntVar(0, num_vals - 1, "x")
    y = cpsolver.IntVar(0, num_vals - 1, "y")
    z = cpsolver.IntVar(0, num_vals - 1, "z")
    cpsolver.Add(x != y)
    db = cpsolver.Phase([x, y, z], cpsolver.CHOOSE_FIRST_UNBOUND, cpsolver.ASSIGN_MIN_VALUE)
    cpsolver.Solve(db)
    print('Test cpsolver...DONE')

def main():
    lpsolver()
    cpsolver()

if __name__ == "__main__":
    main()
