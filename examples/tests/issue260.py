#!/usr/bin/env python3
#from ortools.constraint_solver import pywrapcp
from ortools.sat.python import cp_model

model = cp_model.CpModel()
y = model.NewIntVar(0, 3, 'y')
y == None

#solver = pywrapcp.Solver('my solver')
#x = solver.IntVar(0, 3, 'var_name')
#x == None
