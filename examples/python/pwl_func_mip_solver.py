from __future__ import print_function
from ortools.linear_solver import pywraplp
solver = pywraplp.PwlSolver('Test', pywraplp.PwlSolver.GUROBI)

x = [ [ 1.0, 2.0, 4.0 ] ]
y = [ 1.0, 1.5, 2.0 ]
A = [ [ 2.0 ], [ 1.0 ] ]
B = [ [ -1.0, 2.8 ], [ 2.8, 1.0 ] ]
b = [ 1.0, 2.0 ]
c = [ 0.5 ]
d = [ 10.8, 13.8 ]

solver.SetXValues(x)
solver.SetYValues(y)
solver.SetParameter(b, pywraplp.PwlSolver.bVector)
solver.SetParameter(c, pywraplp.PwlSolver.cVector)
solver.SetParameter(d, pywraplp.PwlSolver.dVector)
solver.SetParameter(A, pywraplp.PwlSolver.AMatrix)
solver.SetParameter(B, pywraplp.PwlSolver.BMatrix)

print("Number of discrete points = {0}".format(solver.NumOfXPoints()))

print("Total number of variables (integer and continuous) = {0}".format(solver.NumVariables()))

print("Total number of continuous variables = {0}".format(solver.NumRealVariables()))

result_status = solver.Solve()

assert result_status == pywraplp.Solver.OPTIMAL

assert solver.VerifySolution(1e-7, True)

objective = solver.Objective()

print("The objective value is equal to {0}".format(objective.Value()))

var1 = solver.GetVariable(0)
print("The solution for lambda_1 = {0}".format(var1.solution_value()))

var2 = solver.GetVariable(1)
print("The solution for lambda_2 = {0}".format(var2.solution_value()))

var3 = solver.GetVariable(2)
print("The solution for lambda_3 = {0}".format(var3.solution_value()))

var4 = solver.GetVariable(3)
print("The solution for z_1 = {0}".format(var4.solution_value()))

var5 = solver.GetVariable(4)
print("The solution for z_2 = {0}".format(var5.solution_value()))


