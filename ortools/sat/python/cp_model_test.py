#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for ortools.sat.python.cp_model."""
from absl.testing import absltest
import pandas as pd

from ortools.sat.python import cp_model


class SolutionCounter(cp_model.CpSolverSolutionCallback):
    """Count solutions."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def OnSolutionCallback(self):
        self.__solution_count += 1

    def SolutionCount(self):
        return self.__solution_count


class SolutionSum(cp_model.CpSolverSolutionCallback):
    """Record the sum of variables in the solution."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__sum = 0
        self.__vars = variables

    def OnSolutionCallback(self):
        self.__sum = sum(self.Value(x) for x in self.__vars)

    def Sum(self):
        return self.__sum


class SolutionObjective(cp_model.CpSolverSolutionCallback):
    """Record the objective value of the solution."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__obj = 0

    def OnSolutionCallback(self):
        self.__obj = self.ObjectiveValue()

    def Obj(self):
        return self.__obj


class LogToString:
    """Record log in a string."""

    def __init__(self):
        self.__log = ""

    def NewMessage(self, message: str):
        self.__log += message
        self.__log += "\n"

    def Log(self):
        return self.__log


class CpModelTest(absltest.TestCase):
    def testCreateIntegerVariable(self):
        print("testCreateIntegerVariable")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        self.assertEqual("x", str(x))
        self.assertEqual("x(-10..10)", repr(x))
        y = model.NewIntVarFromDomain(cp_model.Domain.FromIntervals([[2, 4], [7]]), "y")
        self.assertEqual("y", str(y))
        self.assertEqual("y(2..4, 7)", repr(y))
        z = model.NewIntVarFromDomain(cp_model.Domain.FromValues([2, 3, 4, 7]), "z")
        self.assertEqual("z", str(z))
        self.assertEqual("z(2..4, 7)", repr(z))
        t = model.NewIntVarFromDomain(
            cp_model.Domain.FromFlatIntervals([2, 4, 7, 7]), "t"
        )
        self.assertEqual("t", str(t))
        self.assertEqual("t(2..4, 7)", repr(t))
        cst = model.NewConstant(5)
        self.assertEqual("5", str(cst))

    def testNegation(self):
        print("testNegation")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        b = model.NewBoolVar("b")
        nb = b.Not()
        self.assertEqual(b.Not(), nb)
        self.assertEqual(b.Not().Not(), b)
        self.assertEqual(nb.Index(), -b.Index() - 1)
        self.assertRaises(TypeError, x.Not)

    def testEqualityOverload(self):
        print("testEqualityOverload")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(0, 5, "y")
        self.assertEqual(x, x)
        self.assertNotEqual(x, y)

    def testLinear(self):
        print("testLinear")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        model.Minimize(y)
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        self.assertEqual(10, solver.Value(x))
        self.assertEqual(-5, solver.Value(y))

    def testLinearNonEqual(self):
        print("testLinearNonEqual")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(-x + y != 3).Proto()
        self.assertLen(ct.linear.domain, 4)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])
        self.assertEqual(4, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def testEq(self):
        print("testEq")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        ct = model.Add(x == 2).Proto()
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGe(self):
        print("testGe")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        ct = model.Add(x >= 2).Proto()
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testGt(self):
        print("testGt")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        ct = model.Add(x > 2).Proto()
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(3, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testLe(self):
        print("testLe")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        ct = model.Add(x <= 2).Proto()
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testLt(self):
        print("testLt")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        ct = model.Add(x < 2).Proto()
        self.assertLen(ct.linear.vars, 1)
        self.assertLen(ct.linear.coeffs, 1)
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def testEqVar(self):
        print("testEqVar")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x == y + 2).Proto()
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(0, ct.linear.coeffs[0] + ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGeVar(self):
        print("testGeVar")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x >= 1 - y).Proto()
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(1, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testGtVar(self):
        print("testGeVar")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x > 1 - y).Proto()
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testLeVar(self):
        print("testLeVar")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x <= 1 - y).Proto()
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def testLtVar(self):
        print("testLtVar")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x < 1 - y).Proto()
        self.assertLen(ct.linear.vars, 2)
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertLen(ct.linear.coeffs, 2)
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertLen(ct.linear.domain, 2)
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(0, ct.linear.domain[1])

    def testSimplification1(self):
        print("testSimplification1")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        prod = (x * 2) * 2
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification2(self):
        print("testSimplification2")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        prod = 2 * (x * 2)
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification3(self):
        print("testSimplification3")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        prod = (2 * x) * 2
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification4(self):
        print("testSimplification4")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        prod = 2 * (2 * x)
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testLinearNonEqualWithConstant(self):
        print("testLinearNonEqualWithConstant")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.Add(x + y + 5 != 3).Proto()
        self.assertLen(ct.linear.domain, 4)
        # Checks that saturated arithmetics worked.
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(-3, ct.linear.domain[1])
        self.assertEqual(-1, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def testLinearWithEnforcement(self):
        print("testLinearWithEnforcement")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        b = model.NewBoolVar("b")
        model.AddLinearConstraint(x + 2 * y, 0, 10).OnlyEnforceIf(b.Not())
        model.Minimize(y)
        self.assertLen(model.Proto().constraints, 1)
        self.assertEqual(-3, model.Proto().constraints[0].enforcement_literal[0])
        c = model.NewBoolVar("c")
        model.AddLinearConstraint(x + 4 * y, 0, 10).OnlyEnforceIf([b, c])
        self.assertLen(model.Proto().constraints, 2)
        self.assertEqual(2, model.Proto().constraints[1].enforcement_literal[0])
        self.assertEqual(3, model.Proto().constraints[1].enforcement_literal[1])
        model.AddLinearConstraint(x + 5 * y, 0, 10).OnlyEnforceIf(c.Not(), b)
        self.assertLen(model.Proto().constraints, 3)
        self.assertEqual(-4, model.Proto().constraints[2].enforcement_literal[0])
        self.assertEqual(2, model.Proto().constraints[2].enforcement_literal[1])

    def testConstraintWithName(self):
        print("testConstraintWithName")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        ct = model.AddLinearConstraint(x + 2 * y, 0, 10).WithName("test_constraint")
        self.assertEqual("test_constraint", ct.Name())

    def testNaturalApiMinimize(self):
        print("testNaturalApiMinimize")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.Add(x * 2 - 1 * y == 1)
        model.Minimize(x * 1 - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(5, solver.Value(x))
        self.assertEqual(15, solver.Value(x * 3))
        self.assertEqual(6, solver.Value(1 + x))
        self.assertEqual(-10.0, solver.ObjectiveValue())

    def testNaturalApiMaximizeFloat(self):
        print("testNaturalApiMaximizeFloat")
        model = cp_model.CpModel()
        x = model.NewBoolVar("x")
        y = model.NewIntVar(0, 10, "y")
        model.Maximize(x.Not() * 3.5 + x.Not() - y + 2 * y + 1.6)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertFalse(solver.BooleanValue(x))
        self.assertTrue(solver.BooleanValue(x.Not()))
        self.assertEqual(-10, solver.Value(-y))
        self.assertEqual(16.1, solver.ObjectiveValue())

    def testNaturalApiMaximizeComplex(self):
        print("testNaturalApiMaximizeFloat")
        model = cp_model.CpModel()
        x1 = model.NewBoolVar("x1")
        x2 = model.NewBoolVar("x1")
        x3 = model.NewBoolVar("x1")
        x4 = model.NewBoolVar("x1")
        model.Maximize(
            cp_model.LinearExpr.Sum([x1, x2])
            + cp_model.LinearExpr.WeightedSum([x3, x4.Not()], [2, 4])
        )
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(5, solver.Value(3 + 2 * x1))
        self.assertEqual(3, solver.Value(x1 + x2 + x3))
        self.assertEqual(1, solver.Value(cp_model.LinearExpr.Sum([x1, x2, x3, 0, -2])))
        self.assertEqual(
            7,
            solver.Value(
                cp_model.LinearExpr.WeightedSum([x1, x2, x4, 3], [2, 2, 2, 1])
            ),
        )
        self.assertEqual(5, solver.Value(5 * x4.Not()))
        self.assertEqual(8, solver.ObjectiveValue())

    def testNaturalApiMaximize(self):
        print("testNaturalApiMaximize")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.Add(2 * x - y == 1)
        model.Maximize(x - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(-4, solver.Value(x))
        self.assertEqual(-9, solver.Value(y))
        self.assertEqual(17, solver.ObjectiveValue())

    def testMinimizeConstant(self):
        print("testMinimizeConstant")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        model.Add(x >= -1)
        model.Minimize(10)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(10, solver.ObjectiveValue())

    def testMaximizeConstant(self):
        print("testMinimizeConstant")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        model.Add(x >= -1)
        model.Maximize(5)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(5, solver.ObjectiveValue())

    def testAddTrue(self):
        print("testAddTrue")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        model.Add(3 >= -1)
        model.Minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("OPTIMAL", solver.StatusName(solver.Solve(model)))
        self.assertEqual(-10, solver.Value(x))

    def testAddFalse(self):
        print("testAddFalse")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        model.Add(3 <= -1)
        model.Minimize(x)
        solver = cp_model.CpSolver()
        self.assertEqual("INFEASIBLE", solver.StatusName(solver.Solve(model)))

    def testSum(self):
        print("testSum")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 2, "x%i" % i) for i in range(100)]
        model.Add(sum(x) <= 1)
        model.Maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        self.assertEqual(1.0, solver.ObjectiveValue())
        for i in range(100):
            self.assertEqual(solver.Value(x[i]), 1 if i == 99 else 0)

    def testSumWithApi(self):
        print("testSumWithApi")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 2, "x%i" % i) for i in range(100)]
        model.Add(cp_model.LinearExpr.Sum(x) <= 1)
        model.Maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        self.assertEqual(1.0, solver.ObjectiveValue())
        for i in range(100):
            self.assertEqual(solver.Value(x[i]), 1 if i == 99 else 0)

    def testWeightedSum(self):
        print("testWeightedSum")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 2, "x%i" % i) for i in range(100)]
        c = [2] * 100
        model.Add(cp_model.LinearExpr.WeightedSum(x, c) <= 3)
        model.Maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        self.assertEqual(1.0, solver.ObjectiveValue())
        for i in range(100):
            self.assertEqual(solver.Value(x[i]), 1 if i == 99 else 0)

    def testAllDifferent(self):
        print("testAllDifferent")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddAllDifferent(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].all_diff.exprs, 5)

    def testAllDifferentGen(self):
        print("testAllDifferentGen")
        model = cp_model.CpModel()
        model.AddAllDifferent(model.NewIntVar(0, 4, "x%i" % i) for i in range(5))
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].all_diff.exprs, 5)

    def testAllDifferentList(self):
        print("testAllDifferentList")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddAllDifferent(x[0], x[1], x[2], x[3], x[4])
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].all_diff.exprs, 5)

    def testElement(self):
        print("testElement")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddElement(x[0], [x[1], 2, 4, x[2]], x[4])
        self.assertLen(model.Proto().variables, 7)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].element.vars, 4)
        self.assertEqual(0, model.Proto().constraints[0].element.index)
        self.assertEqual(4, model.Proto().constraints[0].element.target)
        self.assertRaises(ValueError, model.AddElement, x[0], [], x[4])

    def testCircuit(self):
        print("testCircuit")
        model = cp_model.CpModel()
        x = [model.NewBoolVar(f"x{i}") for i in range(5)]
        model.AddCircuit((i, i + 1, x[i]) for i in range(5))
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].circuit.heads, 5)
        self.assertLen(model.Proto().constraints[0].circuit.tails, 5)
        self.assertLen(model.Proto().constraints[0].circuit.literals, 5)
        self.assertRaises(ValueError, model.AddCircuit, [])

    def testMultipleCircuit(self):
        print("testMultipleCircuit")
        model = cp_model.CpModel()
        x = [model.NewBoolVar(f"x{i}") for i in range(5)]
        model.AddMultipleCircuit((i, i + 1, x[i]) for i in range(5))
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].routes.heads, 5)
        self.assertLen(model.Proto().constraints[0].routes.tails, 5)
        self.assertLen(model.Proto().constraints[0].routes.literals, 5)
        self.assertRaises(ValueError, model.AddMultipleCircuit, [])

    def testAllowedAssignments(self):
        print("testAllowedAssignments")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddAllowedAssignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].table.vars, 5)
        self.assertLen(model.Proto().constraints[0].table.values, 15)
        self.assertRaises(
            TypeError,
            model.AddAllowedAssignments,
            x,
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )
        self.assertRaises(
            ValueError,
            model.AddAllowedAssignments,
            [],
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )

    def testForbiddenAssignments(self):
        print("testForbiddenAssignments")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddForbiddenAssignments(
            x, [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0, 0)]
        )
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].table.vars, 5)
        self.assertLen(model.Proto().constraints[0].table.values, 15)
        self.assertTrue(model.Proto().constraints[0].table.negated)
        self.assertRaises(
            TypeError,
            model.AddForbiddenAssignments,
            x,
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )
        self.assertRaises(
            ValueError,
            model.AddForbiddenAssignments,
            [],
            [(0, 1, 2, 3, 4), (4, 3, 2, 1, 1), (0, 0, 0, 0)],
        )

    def testAutomaton(self):
        print("testAutomaton")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        model.AddAutomaton(x, 0, [2, 3], [(0, 0, 0), (0, 1, 1), (1, 2, 2), (2, 3, 3)])
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].automaton.vars, 5)
        self.assertLen(model.Proto().constraints[0].automaton.transition_tail, 4)
        self.assertLen(model.Proto().constraints[0].automaton.transition_head, 4)
        self.assertLen(model.Proto().constraints[0].automaton.transition_label, 4)
        self.assertLen(model.Proto().constraints[0].automaton.final_states, 2)
        self.assertEqual(0, model.Proto().constraints[0].automaton.starting_state)
        self.assertRaises(
            TypeError,
            model.AddAutomaton,
            x,
            0,
            [2, 3],
            [(0, 0, 0), (0, 1, 1), (2, 2), (2, 3, 3)],
        )
        self.assertRaises(
            ValueError,
            model.AddAutomaton,
            [],
            0,
            [2, 3],
            [(0, 0, 0), (0, 1, 1), (2, 3, 3)],
        )
        self.assertRaises(
            ValueError, model.AddAutomaton, x, 0, [], [(0, 0, 0), (0, 1, 1), (2, 3, 3)]
        )
        self.assertRaises(ValueError, model.AddAutomaton, x, 0, [2, 3], [])

    def testInverse(self):
        print("testInverse")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, "x%i" % i) for i in range(5)]
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddInverse(x, y)
        self.assertLen(model.Proto().variables, 10)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].inverse.f_direct, 5)
        self.assertLen(model.Proto().constraints[0].inverse.f_inverse, 5)

    def testMaxEquality(self):
        print("testMaxEquality")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMaxEquality(x, y)
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.Proto().constraints[0].lin_max.target.vars[0])
        self.assertEqual(1, model.Proto().constraints[0].lin_max.target.coeffs[0])

    def testMinEquality(self):
        print("testMinEquality")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMinEquality(x, y)
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.Proto().constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.Proto().constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityList(self):
        print("testMinEqualityList")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMinEquality(x, [y[0], y[2], y[1], y[3]])
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.Proto().constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.Proto().constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityTuple(self):
        print("testMinEqualityTuple")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMinEquality(x, (y[0], y[2], y[1], y[3]))
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 4)
        self.assertEqual(0, model.Proto().constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.Proto().constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityGenerator(self):
        print("testMinEqualityGenerator")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMinEquality(x, (z for z in y))
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 5)
        self.assertEqual(0, model.Proto().constraints[0].lin_max.target.vars[0])
        self.assertEqual(-1, model.Proto().constraints[0].lin_max.target.coeffs[0])

    def testMinEqualityWithConstant(self):
        print("testMinEqualityWithConstant")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 4, "y")
        model.AddMinEquality(x, [y, 3])
        self.assertLen(model.Proto().variables, 2)
        self.assertLen(model.Proto().constraints, 1)
        lin_max = model.Proto().constraints[0].lin_max
        self.assertLen(lin_max.exprs, 2)
        self.assertLen(lin_max.exprs[0].vars, 1)
        self.assertEqual(1, lin_max.exprs[0].vars[0])
        self.assertEqual(-1, lin_max.exprs[0].coeffs[0])
        self.assertEqual(0, lin_max.exprs[0].offset)
        self.assertEmpty(lin_max.exprs[1].vars)
        self.assertEqual(-3, lin_max.exprs[1].offset)

    def testAbs(self):
        print("testAbs")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(-5, 5, "y")
        model.AddAbsEquality(x, y)
        self.assertLen(model.Proto().variables, 2)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].lin_max.exprs, 2)
        self.assertEqual(1, model.Proto().constraints[0].lin_max.exprs[0].vars[0])
        self.assertEqual(1, model.Proto().constraints[0].lin_max.exprs[0].coeffs[0])
        self.assertEqual(1, model.Proto().constraints[0].lin_max.exprs[1].vars[0])
        self.assertEqual(-1, model.Proto().constraints[0].lin_max.exprs[1].coeffs[0])
        passed = False
        error_msg = None
        try:
            abs(x)
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling abs() on a linear expression is not supported, "
            "please use CpModel.AddAbsEquality",
            error_msg,
        )
        self.assertTrue(passed)

    def testDivision(self):
        print("testDivision")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, "x")
        y = model.NewIntVar(0, 50, "y")
        model.AddDivisionEquality(x, y, 6)
        self.assertLen(model.Proto().variables, 2)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].int_div.exprs, 2)
        self.assertEqual(model.Proto().constraints[0].int_div.exprs[0].vars[0], 1)
        self.assertEqual(model.Proto().constraints[0].int_div.exprs[0].coeffs[0], 1)
        self.assertEmpty(model.Proto().constraints[0].int_div.exprs[1].vars)
        self.assertEqual(model.Proto().constraints[0].int_div.exprs[1].offset, 6)
        passed = False
        error_msg = None
        try:
            x / 3
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling // on a linear expression is not supported, "
            "please use CpModel.AddDivisionEquality",
            error_msg,
        )
        self.assertTrue(passed)

    def testModulo(self):
        print("testModulo")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, "x")
        y = model.NewIntVar(0, 50, "y")
        model.AddModuloEquality(x, y, 6)
        self.assertLen(model.Proto().variables, 2)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].int_mod.exprs, 2)
        self.assertEqual(model.Proto().constraints[0].int_mod.exprs[0].vars[0], 1)
        self.assertEqual(model.Proto().constraints[0].int_mod.exprs[0].coeffs[0], 1)
        self.assertEmpty(model.Proto().constraints[0].int_mod.exprs[1].vars)
        self.assertEqual(model.Proto().constraints[0].int_mod.exprs[1].offset, 6)
        passed = False
        error_msg = None
        try:
            x % 3
        except NotImplementedError as e:
            error_msg = str(e)
            passed = True
        self.assertEqual(
            "calling %% on a linear expression is not supported, "
            "please use CpModel.AddModuloEquality",
            error_msg,
        )
        self.assertTrue(passed)

    def testMultiplicationEquality(self):
        print("testMultiplicationEquality")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = [model.NewIntVar(0, 4, "y%i" % i) for i in range(5)]
        model.AddMultiplicationEquality(x, y)
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].int_prod.exprs, 5)
        self.assertEqual(0, model.Proto().constraints[0].int_prod.target.vars[0])

    def testImplication(self):
        print("testImplication")
        model = cp_model.CpModel()
        x = model.NewBoolVar("x")
        y = model.NewBoolVar("y")
        model.AddImplication(x, y)
        self.assertLen(model.Proto().variables, 2)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].bool_or.literals, 1)
        self.assertLen(model.Proto().constraints[0].enforcement_literal, 1)
        self.assertEqual(x.Index(), model.Proto().constraints[0].enforcement_literal[0])
        self.assertEqual(y.Index(), model.Proto().constraints[0].bool_or.literals[0])

    def testBoolOr(self):
        print("testBoolOr")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddBoolOr(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].bool_or.literals, 5)
        model.AddBoolOr([x[0], x[1], False])
        self.assertLen(model.Proto().variables, 6)
        self.assertRaises(TypeError, model.AddBoolOr, [x[2], 2])
        y = model.NewIntVar(0, 4, "y")
        self.assertRaises(TypeError, model.AddBoolOr, [y, False])

    def testBoolOrListOrGet(self):
        print("testBoolOrListOrGet")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddBoolOr(x)
        model.AddBoolOr(True, x[0], x[2])
        model.AddBoolOr(False, x[0])
        model.AddBoolOr(x[i] for i in [0, 2, 3, 4])
        self.assertLen(model.Proto().variables, 7)
        self.assertLen(model.Proto().constraints, 4)
        self.assertLen(model.Proto().constraints[0].bool_or.literals, 5)
        self.assertLen(model.Proto().constraints[1].bool_or.literals, 3)
        self.assertLen(model.Proto().constraints[2].bool_or.literals, 2)
        self.assertLen(model.Proto().constraints[3].bool_or.literals, 4)

    def testAtLeastOne(self):
        print("testAtLeastOne")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddAtLeastOne(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].bool_or.literals, 5)
        model.AddAtLeastOne([x[0], x[1], False])
        self.assertLen(model.Proto().variables, 6)
        self.assertRaises(TypeError, model.AddAtLeastOne, [x[2], 2])
        y = model.NewIntVar(0, 4, "y")
        self.assertRaises(TypeError, model.AddAtLeastOne, [y, False])

    def testAtMostOne(self):
        print("testAtMostOne")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddAtMostOne(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].at_most_one.literals, 5)
        model.AddAtMostOne([x[0], x[1], False])
        self.assertLen(model.Proto().variables, 6)
        self.assertRaises(TypeError, model.AddAtMostOne, [x[2], 2])
        y = model.NewIntVar(0, 4, "y")
        self.assertRaises(TypeError, model.AddAtMostOne, [y, False])

    def testExactlyOne(self):
        print("testExactlyOne")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddExactlyOne(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].exactly_one.literals, 5)
        model.AddExactlyOne([x[0], x[1], False])
        self.assertLen(model.Proto().variables, 6)
        self.assertRaises(TypeError, model.AddExactlyOne, [x[2], 2])
        y = model.NewIntVar(0, 4, "y")
        self.assertRaises(TypeError, model.AddExactlyOne, [y, False])

    def testBoolAnd(self):
        print("testBoolAnd")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddBoolAnd(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].bool_and.literals, 5)
        model.AddBoolAnd([x[1], x[2].Not(), True])
        self.assertEqual(1, model.Proto().constraints[1].bool_and.literals[0])
        self.assertEqual(-3, model.Proto().constraints[1].bool_and.literals[1])
        self.assertEqual(5, model.Proto().constraints[1].bool_and.literals[2])

    def testBoolXOr(self):
        print("testBoolXOr")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        model.AddBoolXOr(x)
        self.assertLen(model.Proto().variables, 5)
        self.assertLen(model.Proto().constraints, 1)
        self.assertLen(model.Proto().constraints[0].bool_xor.literals, 5)

    def testMapDomain(self):
        print("testMapDomain")
        model = cp_model.CpModel()
        x = [model.NewBoolVar("x%i" % i) for i in range(5)]
        y = model.NewIntVar(0, 10, "y")
        model.AddMapDomain(y, x, 2)
        self.assertLen(model.Proto().variables, 6)
        self.assertLen(model.Proto().constraints, 10)

    def testInterval(self):
        print("testInterval")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        i = model.NewIntervalVar(x, 3, y, "i")
        self.assertEqual(1, i.Index())

        j = model.NewFixedSizeIntervalVar(x, 2, "j")
        self.assertEqual(2, j.Index())
        start_expr = j.StartExpr()
        size_expr = j.SizeExpr()
        end_expr = j.EndExpr()
        self.assertEqual(x.Index(), start_expr.Index())
        self.assertEqual(size_expr, 2)
        self.assertEqual(str(end_expr), "(x + 2)")

    def testOptionalInterval(self):
        print("testOptionalInterval")
        model = cp_model.CpModel()
        b = model.NewBoolVar("b")
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        i = model.NewOptionalIntervalVar(x, 3, y, b, "i")
        j = model.NewOptionalIntervalVar(x, y, 10, b, "j")
        k = model.NewOptionalIntervalVar(x, -y, 10, b, "k")
        l = model.NewOptionalIntervalVar(x, 10, -y, b, "l")
        self.assertEqual(1, i.Index())
        self.assertEqual(3, j.Index())
        self.assertEqual(5, k.Index())
        self.assertEqual(7, l.Index())
        self.assertRaises(TypeError, model.NewOptionalIntervalVar, 1, 2, 3, x, "x")
        self.assertRaises(TypeError, model.NewOptionalIntervalVar, b + x, 2, 3, b, "x")
        self.assertRaises(
            AttributeError, model.NewOptionalIntervalVar, 1, 2, 3, b + 1, "x"
        )

    def testNoOverlap(self):
        print("testNoOverlap")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        z = model.NewIntVar(0, 3, "y")
        i = model.NewIntervalVar(x, 3, y, "i")
        j = model.NewIntervalVar(x, 5, z, "j")
        ct = model.AddNoOverlap([i, j])
        self.assertEqual(4, ct.Index())
        self.assertLen(ct.Proto().no_overlap.intervals, 2)
        self.assertEqual(1, ct.Proto().no_overlap.intervals[0])
        self.assertEqual(3, ct.Proto().no_overlap.intervals[1])

    def testNoOverlap2D(self):
        print("testNoOverlap2D")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        z = model.NewIntVar(0, 3, "y")
        i = model.NewIntervalVar(x, 3, y, "i")
        j = model.NewIntervalVar(x, 5, z, "j")
        ct = model.AddNoOverlap2D([i, j], [j, i])
        self.assertEqual(4, ct.Index())
        self.assertLen(ct.Proto().no_overlap_2d.x_intervals, 2)
        self.assertEqual(1, ct.Proto().no_overlap_2d.x_intervals[0])
        self.assertEqual(3, ct.Proto().no_overlap_2d.x_intervals[1])
        self.assertLen(ct.Proto().no_overlap_2d.y_intervals, 2)
        self.assertEqual(3, ct.Proto().no_overlap_2d.y_intervals[0])
        self.assertEqual(1, ct.Proto().no_overlap_2d.y_intervals[1])

    def testCumulative(self):
        print("testCumulative")
        model = cp_model.CpModel()
        intervals = [
            model.NewIntervalVar(
                model.NewIntVar(0, 10, f"s_{i}"),
                5,
                model.NewIntVar(5, 15, f"e_{i}"),
                f"interval[{i}]",
            )
            for i in range(10)
        ]
        demands = [1, 3, 5, 2, 4, 5, 3, 4, 2, 3]
        capacity = 4
        ct = model.AddCumulative(intervals, demands, capacity)
        self.assertEqual(20, ct.Index())
        self.assertLen(ct.Proto().cumulative.intervals, 10)
        self.assertRaises(TypeError, model.AddCumulative, [intervals[0], 3], [2, 3], 3)

    def testGetOrMakeIndexFromConstant(self):
        print("testGetOrMakeIndexFromConstant")
        model = cp_model.CpModel()
        self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
        self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
        self.assertEqual(1, model.GetOrMakeIndexFromConstant(5))
        model_var = model.Proto().variables[0]
        self.assertLen(model_var.domain, 2)
        self.assertEqual(3, model_var.domain[0])
        self.assertEqual(3, model_var.domain[1])

    def testStr(self):
        print("testStr")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        self.assertEqual(str(x == 2), "x == 2")
        self.assertEqual(str(x >= 2), "x >= 2")
        self.assertEqual(str(x <= 2), "x <= 2")
        self.assertEqual(str(x > 2), "x >= 3")
        self.assertEqual(str(x < 2), "x <= 1")
        self.assertEqual(str(x != 2), "x != 2")
        self.assertEqual(str(x * 3), "(3 * x)")
        self.assertEqual(str(-x), "-x")
        self.assertEqual(str(x + 3), "(x + 3)")
        self.assertEqual(str(x <= cp_model.INT_MAX), "True (unbounded expr x)")
        self.assertEqual(str(x != 9223372036854775807), "x <= 9223372036854775806")
        self.assertEqual(str(x != -9223372036854775808), "x >= -9223372036854775807")
        y = model.NewIntVar(0, 4, "y")
        self.assertEqual(
            str(cp_model.LinearExpr.WeightedSum([x, y + 1, 2], [1, -2, 3])),
            "x - 2 * (y + 1) + 6",
        )
        self.assertEqual(str(cp_model.LinearExpr.Term(x, 3)), "(3 * x)")
        self.assertEqual(str(x != y), "(x + -y) != 0")
        self.assertEqual(
            "0 <= x <= 10", str(cp_model.BoundedLinearExpression(x, [0, 10]))
        )
        print(str(model))
        b = model.NewBoolVar("b")
        self.assertEqual(str(cp_model.LinearExpr.Term(b.Not(), 3)), "(3 * not(b))")

        i = model.NewIntervalVar(x, 2, y, "i")
        self.assertEqual(str(i), "i")

    def testRepr(self):
        print("testRepr")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        z = model.NewIntVar(0, 3, "z")
        self.assertEqual(repr(x), "x(0..4)")
        self.assertEqual(repr(x * 2), "ProductCst(x(0..4), 2)")
        self.assertEqual(repr(x + y), "Sum(x(0..4), y(0..3))")
        self.assertEqual(
            repr(cp_model.LinearExpr.Sum([x, y, z])),
            "SumArray(x(0..4), y(0..3), z(0..3), 0)",
        )
        self.assertEqual(
            repr(cp_model.LinearExpr.WeightedSum([x, y, 2], [1, 2, 3])),
            "WeightedSum([x(0..4), y(0..3)], [1, 2], 6)",
        )
        i = model.NewIntervalVar(x, 2, y, "i")
        self.assertEqual(repr(i), "i(start = x, size = 2, end = y)")
        b = model.NewBoolVar("b")
        x1 = model.NewIntVar(0, 4, "x1")
        y1 = model.NewIntVar(0, 3, "y1")
        j = model.NewOptionalIntervalVar(x1, 2, y1, b, "j")
        self.assertEqual(repr(j), "j(start = x1, size = 2, end = y1, is_present = b)")
        x2 = model.NewIntVar(0, 4, "x2")
        y2 = model.NewIntVar(0, 3, "y2")
        k = model.NewOptionalIntervalVar(x2, 2, y2, b.Not(), "k")
        self.assertEqual(
            repr(k), "k(start = x2, size = 2, end = y2, is_present = Not(b))"
        )

    def testDisplayBounds(self):
        print("testDisplayBounds")
        self.assertEqual("10..20", cp_model.DisplayBounds([10, 20]))
        self.assertEqual("10", cp_model.DisplayBounds([10, 10]))
        self.assertEqual("10..15, 20..30", cp_model.DisplayBounds([10, 15, 20, 30]))

    def testShortName(self):
        print("testShortName")
        model = cp_model.CpModel()
        model.Proto().variables.add(domain=[5, 10])
        self.assertEqual("[5..10]", cp_model.ShortName(model.Proto(), 0))

    def testIntegerExpressionErrors(self):
        print("testIntegerExpressionErrors")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 1, "x")
        y = model.NewIntVar(0, 3, "y")
        self.assertRaises(TypeError, x.__mul__, y)
        self.assertRaises(NotImplementedError, x.__div__, y)
        self.assertRaises(NotImplementedError, x.__truediv__, y)
        self.assertRaises(NotImplementedError, x.__mod__, y)
        self.assertRaises(NotImplementedError, x.__pow__, y)
        self.assertRaises(NotImplementedError, x.__lshift__, y)
        self.assertRaises(NotImplementedError, x.__rshift__, y)
        self.assertRaises(NotImplementedError, x.__and__, y)
        self.assertRaises(NotImplementedError, x.__or__, y)
        self.assertRaises(NotImplementedError, x.__xor__, y)
        self.assertRaises(ArithmeticError, x.__lt__, cp_model.INT_MIN)
        self.assertRaises(ArithmeticError, x.__gt__, cp_model.INT_MAX)
        self.assertRaises(TypeError, x.__add__, "dummy")
        self.assertRaises(TypeError, x.__mul__, "dummy")

    def testModelErrors(self):
        print("testModelErrors")
        model = cp_model.CpModel()
        self.assertRaises(TypeError, model.Add, "dummy")
        self.assertRaises(TypeError, model.GetOrMakeIndex, "dummy")
        self.assertRaises(TypeError, model.Minimize, "dummy")

    def testSolverErrors(self):
        print("testSolverErrors")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 1, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        model.Minimize(y)
        solver = cp_model.CpSolver()
        self.assertRaises(RuntimeError, solver.Value, x)
        solver.Solve(model)
        self.assertRaises(TypeError, solver.Value, "not_a_variable")
        self.assertRaises(TypeError, model.AddBoolOr, [x, y])

    def testHasObjectiveMinimize(self):
        print("testHasObjectiveMinimizs")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 1, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        self.assertFalse(model.HasObjective())
        model.Minimize(y)
        self.assertTrue(model.HasObjective())

    def testHasObjectiveMaximize(self):
        print("testHasObjectiveMaximizs")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 1, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        self.assertFalse(model.HasObjective())
        model.Maximize(y)
        self.assertTrue(model.HasObjective())

    def testSearchForAllSolutions(self):
        print("testSearchForAllSolutions")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)

        solver = cp_model.CpSolver()
        solution_counter = SolutionCounter()
        status = solver.SearchForAllSolutions(model, solution_counter)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(5, solution_counter.SolutionCount())
        model.Minimize(x)
        self.assertRaises(
            TypeError, solver.SearchForAllSolutions, model, solution_counter
        )

    def testSolveWithSolutionCallback(self):
        print("testSolveWithSolutionCallback")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)

        solver = cp_model.CpSolver()
        solution_sum = SolutionSum([x, y])
        self.assertRaises(RuntimeError, solution_sum.Value, x)
        status = solver.SolveWithSolutionCallback(model, solution_sum)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(6, solution_sum.Sum())

    def testValue(self):
        print("testValue")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, "x")
        y = model.NewIntVar(0, 10, "y")
        model.Add(x + 2 * y == 29)
        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.Value(x), 9)
        self.assertEqual(solver.Value(y), 10)
        self.assertEqual(solver.Value(2), 2)

    def testBooleanValue(self):
        print("testBooleanValue")
        model = cp_model.CpModel()
        x = model.NewBoolVar("x")
        y = model.NewBoolVar("y")
        z = model.NewBoolVar("z")
        model.AddBoolOr([x, z.Not()])
        model.AddBoolOr([x, z])
        model.AddBoolOr([x.Not(), y.Not()])
        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.BooleanValue(x), True)
        self.assertEqual(solver.Value(x), 1 - solver.Value(x.Not()))
        self.assertEqual(solver.Value(y), 1 - solver.Value(y.Not()))
        self.assertEqual(solver.Value(z), 1 - solver.Value(z.Not()))
        self.assertEqual(solver.BooleanValue(y), False)
        self.assertEqual(solver.BooleanValue(True), True)
        self.assertEqual(solver.BooleanValue(False), False)
        self.assertEqual(solver.BooleanValue(2), True)
        self.assertEqual(solver.BooleanValue(0), False)

    def testUnsupportedOperators(self):
        print("testUnsupportedOperators")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, "x")
        y = model.NewIntVar(0, 10, "y")
        z = model.NewIntVar(0, 10, "z")

        with self.assertRaises(NotImplementedError):
            model.Add(x == min(y, z))
        with self.assertRaises(NotImplementedError):
            if x > y:
                print("passed1")
        with self.assertRaises(NotImplementedError):
            if x == 2:
                print("passed2")

    def testIsLiteralTrueFalse(self):
        print("testIsLiteralTrueFalse")
        model = cp_model.CpModel()
        x = model.NewConstant(0)
        self.assertFalse(cp_model.ObjectIsATrueLiteral(x))
        self.assertTrue(cp_model.ObjectIsAFalseLiteral(x))
        self.assertTrue(cp_model.ObjectIsATrueLiteral(x.Not()))
        self.assertFalse(cp_model.ObjectIsAFalseLiteral(x.Not()))
        self.assertTrue(cp_model.ObjectIsATrueLiteral(True))
        self.assertTrue(cp_model.ObjectIsAFalseLiteral(False))
        self.assertFalse(cp_model.ObjectIsATrueLiteral(False))
        self.assertFalse(cp_model.ObjectIsAFalseLiteral(True))

    def testSolveMinimizeWithSolutionCallback(self):
        print("testSolveMinimizeWithSolutionCallback")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)
        model.Maximize(x + 2 * y)

        solver = cp_model.CpSolver()
        solution_obj = SolutionObjective()
        status = solver.SolveWithSolutionCallback(model, solution_obj)
        self.assertEqual(cp_model.OPTIMAL, status)
        print("obj = ", solution_obj.Obj())
        self.assertEqual(11, solution_obj.Obj())

    def testSolutionHinting(self):
        print("testSolutionHinting")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)
        model.AddHint(x, 2)
        model.AddHint(y, 4)
        solver = cp_model.CpSolver()
        solver.parameters.cp_model_presolve = False
        status = solver.Solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(2, solver.Value(x))
        self.assertEqual(4, solver.Value(y))

    def testStats(self):
        print("testStats")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 4, 6)
        model.AddLinearConstraint(2 * x + y, 0, 10)
        model.Maximize(x + 2 * y)

        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(cp_model.OPTIMAL, status)
        self.assertEqual(solver.NumBooleans(), 0)
        self.assertEqual(solver.NumConflicts(), 0)
        self.assertEqual(solver.NumBranches(), 0)
        self.assertGreater(solver.WallTime(), 0.0)

    def testSearchStrategy(self):
        print("testSearchStrategy")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddDecisionStrategy(
            [y, x], cp_model.CHOOSE_MIN_DOMAIN_SIZE, cp_model.SELECT_MAX_VALUE
        )
        self.assertLen(model.Proto().search_strategy, 1)
        strategy = model.Proto().search_strategy[0]
        self.assertLen(strategy.variables, 2)
        self.assertEqual(y.Index(), strategy.variables[0])
        self.assertEqual(x.Index(), strategy.variables[1])
        self.assertEqual(
            cp_model.CHOOSE_MIN_DOMAIN_SIZE, strategy.variable_selection_strategy
        )
        self.assertEqual(cp_model.SELECT_MAX_VALUE, strategy.domain_reduction_strategy)

    def testModelAndResponseStats(self):
        print("testStats")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)
        model.Maximize(x + 2 * y)
        self.assertTrue(model.ModelStats())

        solver = cp_model.CpSolver()
        solver.Solve(model)
        self.assertTrue(solver.ResponseStats())

    def testValidateModel(self):
        print("testValidateModel")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 5, "x")
        y = model.NewIntVar(0, 5, "y")
        model.AddLinearConstraint(x + y, 6, 6)
        model.Maximize(x + 2 * y)
        self.assertFalse(model.Validate())

    def testValidateModelWithOverflow(self):
        print("testValidateModel")
        model = cp_model.CpModel()
        x = model.NewIntVar(0, cp_model.INT_MAX, "x")
        y = model.NewIntVar(0, 10, "y")
        model.AddLinearConstraint(x + y, 6, cp_model.INT_MAX)
        model.Maximize(x + 2 * y)
        self.assertTrue(model.Validate())

    def testCopyModel(self):
        print("testCopyModel")
        model = cp_model.CpModel()
        b = model.NewBoolVar("b")
        x = model.NewIntVar(0, 4, "x")
        y = model.NewIntVar(0, 3, "y")
        i = model.NewOptionalIntervalVar(x, 12, y, b, "i")
        lin = model.Add(x + y <= 10)

        new_model = cp_model.CpModel()
        new_model.CopyFrom(model)
        copy_b = new_model.GetBoolVarFromProtoIndex(b.Index())
        copy_x = new_model.GetIntVarFromProtoIndex(x.Index())
        copy_y = new_model.GetIntVarFromProtoIndex(y.Index())
        copy_i = new_model.GetIntervalVarFromProtoIndex(i.Index())

        self.assertEqual(b.Index(), copy_b.Index())
        self.assertEqual(x.Index(), copy_x.Index())
        self.assertEqual(y.Index(), copy_y.Index())
        self.assertEqual(i.Index(), copy_i.Index())

        with self.assertRaises(ValueError):
            new_model.GetBoolVarFromProtoIndex(-1)

        with self.assertRaises(ValueError):
            new_model.GetIntVarFromProtoIndex(-1)

        with self.assertRaises(ValueError):
            new_model.GetIntervalVarFromProtoIndex(-1)

        with self.assertRaises(ValueError):
            new_model.GetBoolVarFromProtoIndex(x.Index())

        with self.assertRaises(ValueError):
            new_model.GetIntervalVarFromProtoIndex(lin.Index())

        interval_ct = new_model.Proto().constraints[copy_i.Index()].interval
        self.assertEqual(12, interval_ct.size.offset)

    def testCustomLog(self):
        print("testCustomLog")
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, "x")
        y = model.NewIntVar(-10, 10, "y")
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        model.Minimize(y)
        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        solver.parameters.log_to_stdout = False
        log_callback = LogToString()
        solver.log_callback = log_callback.NewMessage

        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        self.assertEqual(10, solver.Value(x))
        self.assertEqual(-5, solver.Value(y))

        self.assertRegex(log_callback.Log(), ".*log_to_stdout.*")

    def testIssue2762(self):
        print("testIssue2762")
        model = cp_model.CpModel()

        x = [model.NewBoolVar("a"), model.NewBoolVar("b")]
        with self.assertRaises(NotImplementedError):
            model.Add((x[0] != 0) or (x[1] != 0))

    def testModelError(self):
        print("TestModelError")
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, -2, "x%i" % i) for i in range(100)]
        model.Add(sum(x) <= 1)
        solver = cp_model.CpSolver()
        solver.parameters.log_search_progress = True
        self.assertEqual(cp_model.MODEL_INVALID, solver.Solve(model))
        self.assertEqual(solver.SolutionInfo(), 'var #0 has no domain(): name: "x0"')

    def testIntVarSeries(self):
        print("testIntVarSeries")
        df = pd.DataFrame([1, -1, 1], columns=["coeffs"])
        model = cp_model.CpModel()
        x = model.NewIntVarSeries(
            name="x", index=df.index, lower_bounds=0, upper_bounds=5
        )
        model.Minimize(df.coeffs.dot(x))
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        solution = solver.Values(x)
        self.assertTrue((solution.values == [0, 5, 0]).all())

    def testBoolVarSeries(self):
        print("testBoolVarSeries")
        df = pd.DataFrame([1, -1, 1], columns=["coeffs"])
        model = cp_model.CpModel()
        x = model.NewBoolVarSeries(name="x", index=df.index)
        model.Minimize(df.coeffs.dot(x))
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model.OPTIMAL, solver.Solve(model))
        solution = solver.BooleanValues(x)
        self.assertTrue((solution.values == [False, True, False]).all())


if __name__ == "__main__":
    absltest.main()
