"""
programming examples that show how to use the PWL solver.
Notes on the PwlSolver class

this class provides a solver optimizing an objective function containing
a piecewise linear function as well as continuous variables.

Problem definition:
We have a piecewise linear (discrete) function given in tabular form with the relation

(1)     y_i = f(x_i), i = 1..k

Here x_i are m-dimensional real vectors i.e. x_i b.t. R(m), y_i are real scalar values i.e. y_i b.t. R

We would like to find the maximum of the following expression

(2)     f(x) + c^{T} * x + b^{T} * z

Here the subscript T denotes a matrix transpose operation. All quantities with small letters
denote column vectors and all quantities with capital letters will denote matrices
unless explicitly stated otherwise. The operator '*' denotes matrix multiplication.

The objective function (2) is subject to the following constraints:

(3)     A * x + B * z <= d
(4)     z >= 0

Here z b.t. R(l) is a l-dimensional vector of continuous variables while x_i is a vector of given values.
The number of individual constraints in (3) is p so the matrix A b.t. R(p,k), the matrix B b.t. R(p,l),
and the vector d b.t. R(p).

After change of variables the problem given with (2)-(4) is transformed into

(5)    sum_{i=1}^{k} lambda_i * y_i + c^{T}*( sum_{i=1}^k lambda_i * x_i ) + b^{T} * z

subject to the system of constraints:

(6)    A * (sum_{i=1}^k lambda_i * x_i) + B * z <= d
(7)    sum_{i=1}^{k} lambda_i = 1
(8)    z >= 0

Here lambda_i, i = 1..k are the new set of binary variables where at most one of them can be 1. The goal is
to find such set of binary variables given with the vector lambda b.t. R(k) and continuous variables
z b.t. R(l) such that the maximum of (5) is attained subject to constraints (6)-(8).

The dimensionality of the various parameters and variables in the PWL Solver is summarized below:

x b.t. R(m), y b.t. R, c b.t. R(l), z b.t. R(l)
A b.t. R(p,m), B b.t. R(p,l), d b.t. R(p)

It is useful to rewrite (5)-(8) in a more concise form where (5) becomes

(9)    maximize a * lambda + b * z

Here we have made the substiution

(10)   a = y + c * x, a b.t. R

In (6) we make the substitution

(11)   C = A * X, C b.t. R(p,k)

where X is a matrix with m rows and k columns composed of all x_i, i=1..k each represented as a column in X.

Then (6) becomes

(12)   C * lambda + B * z <= d

(13)   sum_{i=1}^{k} = 1

(14)   z >= 0

"""

from ortools.linear_solver import pywraplp

def RunPWLExampleWithScalarXDomainAndTwoContinuousVars(optimization_suite):
    """
    Example of a PWL three-valued function defined in scalar X domain.
    Two continuous variables are defined.
    """
    problemName = 'scalar x domain and two continuous variables'
    solver = pywraplp.PwlSolver(problemName, optimization_suite)

    x = [ [ 1.0, 2.0, 4.0 ] ]
    y = [ 1.0, 1.5, 2.0 ]
    A = [ [ 2.0 ], [ 1.0 ] ]
    B = [ [ -1.0, 2.8 ], [ 2.8, 1.0 ] ]
    b = [ 1.0, 2.0 ]
    c = [ 0.5 ]
    d = [ 10.8, 13.8 ]

    SetProblemParameters(solver,x,y,A,B,b,c,d)
    PrintProblemHeader(solver,problemName,optimization_suite)
    SolveAndPrintSolution(solver)

def RunPWLExampleWithScalarXDomainAndThreeContinuousVars(optimization_suite):
    """
    Example of a PWL three-valued function defined in scalar X domain.
    Three continuous variables are defined.
    """
    problemName = 'scalar x domain and three continuous variables'
    solver = pywraplp.PwlSolver(problemName, optimization_suite)

    x = [ [ 1.0, 2.0, 4.0 ] ]
    y = [ 1.0, 1.5, 2.0 ]
    A = [ [ 1.0 ], [ 1.0 ], [ 1.0 ] ];
    B = [ [ 1.0, 1.0, 0.0 ], [ 1.0, 0.0, 1.0 ], [ 0.0, 1.0, 1.0 ] ]
    b = [ 1.0, 0.25, 0.25 ]
    c = [ 0.5 ]
    d = [ 10.0, 10.0, 10.0 ]

    SetProblemParameters(solver,x,y,A,B,b,c,d)
    PrintProblemHeader(solver,problemName,optimization_suite)
    SolveAndPrintSolution(solver)

def RunPWLExampleWith2DimXDomainAndTwoContinuousVars(optimization_suite):
    """
    Example of a PWL three-valued function defined in 2-dimensional X domain.
    Two continuous variables are defined.
    """
    problemName = 'two-dimensional x domain and two continuous variables'
    solver = pywraplp.PwlSolver(problemName, optimization_suite)

    x = [ [ 1.0, 2.0, 4.0 ], [ 1.0, 2.0, 4.0 ] ]
    y = [ 1.0, 1.5, 2.0 ]
    A = [ [ 2.0, 1.0 ], [ 1.0, 0.5 ] ]
    B = [ [ -1.0, 2.8 ], [ 2.8, 1.0 ] ]
    b = [ 1.0, 2.0 ]
    c = [ 0.5, 0.25 ]
    d = [ 10.8, 13.8 ]

    SetProblemParameters(solver,x,y,A,B,b,c,d)
    PrintProblemHeader(solver,problemName,optimization_suite)
    SolveAndPrintSolution(solver)

def RunPWLExampleWith2DimXDomainAndThreeContinuousVars(optimization_suite):
    """
    Example of a PWL three-valued function defined in scalar X domain.
    Three continuous variables are defined.
    """
    problemName = 'two-dimensional x domain and three continuous variables'
    solver = pywraplp.PwlSolver(problemName, optimization_suite)

    x = [ [ 1.0, 2.0, 4.0 ], [ 1.0, 2.0, 4.0 ] ]
    y = [ 1.0, 1.5, 2.0 ]
    A = [ [ 1.0, 0.5 ], [ 1.0, 0.5 ], [ 1.0, 0.5 ] ];
    B = [ [ 1.0, 1.0, 0.0 ], [ 1.0, 0.0, 1.0 ], [ 0.0, 1.0, 1.0 ] ]
    b = [ 1.0, 0.25, 0.25 ]
    c = [ 0.5, 0.25 ]
    d = [ 10.0, 10.0, 10.0 ]

    SetProblemParameters(solver,x,y,A,B,b,c,d)
    PrintProblemHeader(solver,problemName,optimization_suite)
    SolveAndPrintSolution(solver)

def SolveAndPrintSolution(solver):
    """Solve the PWL problem and print the solution."""
    print(('Total number of integer and continuous variables = %d' % solver.NumVariables()))
    print(('Number of continuous variables = %d' % solver.NumRealVariables()))

    result_status = solver.Solve()

    # The problem has an optimal solution.
    assert result_status == pywraplp.Solver.OPTIMAL

    # The solution looks legit (when using solvers others than
    # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
    assert solver.VerifySolution(1e-7, True)

    print(('Problem solved in %f milliseconds' % solver.wall_time()))

    # The objective value of the solution.
    print(('Optimal objective value = %f' % solver.Objective().Value()))

    numIntVars = solver.NumVariables() - solver.NumRealVariables()
    numContVars = solver.NumRealVariables()
    print('Solution values for the integer variables:')
    for i in range(numIntVars):
        variable = solver.GetVariable(i)
        print(('lambda_%d = %d' % ((i+1), variable.solution_value())))

    print('Solution values for the continuous variables:')
    for j in range(numContVars):
        variable = solver.GetVariable(numIntVars+j)
        print(('z_%d = %f' % ((j+1), variable.solution_value())))

    print('Advanced usage:')
    print(('Problem solved in %d branch-and-bound nodes' % solver.nodes()))

def PrintProblemHeader(solver, name, optimization_suite):
    print()
    title = ('Mixed integer programming example with PWL function in %s' % name)
    titleLen = len(title)
    print('-' * titleLen)
    print(title)
    print('-' * titleLen)
    print(('Solver type: %s' % optimization_suite))
    print(('Total number of variables = %d' % solver.NumVariables()))
    print(('Number of continuous variables = %d' % solver.NumRealVariables()))
    print(('Number of integer variables = %d' % (solver.NumVariables() - solver.NumRealVariables())))
    print(('Number of discrete points = %d' % solver.NumOfXPoints()))
    print(('Dimension of X domain = %d' % solver.DimOfXPoint()))
    print(('Number of constraints = %d' % solver.NumConstraints()))

def SetProblemParameters(solver, x, y, A, B, b, c, d):
    solver.SetXValues(x)
    solver.SetYValues(y)
    solver.SetParameter(b, pywraplp.PwlSolver.bVector)
    solver.SetParameter(c, pywraplp.PwlSolver.cVector)
    solver.SetParameter(d, pywraplp.PwlSolver.dVector)
    solver.SetParameter(A, pywraplp.PwlSolver.AMatrix)
    solver.SetParameter(B, pywraplp.PwlSolver.BMatrix)

def main():
    RunPWLExampleWithScalarXDomainAndTwoContinuousVars(pywraplp.PwlSolver.GUROBI)
    RunPWLExampleWithScalarXDomainAndThreeContinuousVars(pywraplp.PwlSolver.GUROBI)
    RunPWLExampleWith2DimXDomainAndTwoContinuousVars(pywraplp.PwlSolver.GUROBI)
    RunPWLExampleWith2DimXDomainAndThreeContinuousVars(pywraplp.PwlSolver.GUROBI)

if __name__ == '__main__':
    main()
