"""Tests for ortools.sat.python.cp_model."""

import unittest
from ortools.sat import cp_model_pb2
from ortools.sat.python import cp_model
from ortools.sat.python import cp_model_helper


class SolutionCounter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def on_solution_callback(self):
        self.__solution_count += 1

    def solution_count(self):
        return self.__solution_count


class LogToString(object):
  """Record log in a string."""

  def __init__(self):
    self.__log = ''

  def NewMessage(self, message: str):
    self.__log += message
    self.__log += '\n'

  def Log(self):
    return self.__log


class CpModelTest(unittest.TestCase):

    def testDomainFromValues(self):
        print('testDomainFromValues')
        model = cp_model.CpModel()
        d = cp_model.Domain.FromValues([0, 1, 2, -2, 5, 4])
        self.assertEqual(d.FlattenedIntervals(), [-2, -2, 0, 2, 4, 5])

    def testDomainFromIntervals(self):
        print('testDomainFromIntervals')
        model = cp_model.CpModel()
        d = cp_model.Domain.FromIntervals([(0, 3), (5, 5), (-1, 1)])
        self.assertEqual(d.FlattenedIntervals(), [-1, 3, 5, 5])

    def testCreateIntegerVariable(self):
        print('testCreateIntegerVariable')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        self.assertEqual('x', str(x))
        self.assertEqual('x(-10..10)', repr(x))

    def testNegation(self):
        print('testNegation')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        b = model.NewBoolVar('b')
        nb = b.Not()
        self.assertEqual(b.Not(), nb)
        self.assertEqual(b.Not().Not(), b)
        self.assertEqual(nb.Index(), -b.Index() - 1)

    def testLinear(self):
        print('testLinear')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        model.AddLinearConstraint(x + 2 * y, 0, 10)
        model.Minimize(y)
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model_pb2.OPTIMAL, solver.Solve(model))
        self.assertEqual(10, solver.Value(x))
        self.assertEqual(-5, solver.Value(y))

    def testCstLeVar(self):
        print('testCstLeVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        model.Add(3 <= x)
        print(model.Proto())

    def testLinearNonEqual(self):
        print('testLinearNonEqual')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(-x + y != 3).Proto()
        self.assertEqual(4, len(ct.linear.domain))
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])
        self.assertEqual(4, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def testEq(self):
        print('testEq')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        ct = model.Add(x == 2).Proto()
        self.assertEqual(1, len(ct.linear.vars))
        self.assertEqual(1, len(ct.linear.coeffs))
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGe(self):
        print('testGe')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        ct = model.Add(x >= 2).Proto()
        self.assertEqual(1, len(ct.linear.vars))
        self.assertEqual(1, len(ct.linear.coeffs))
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testGt(self):
        print('testGt')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        ct = model.Add(x > 2).Proto()
        self.assertEqual(1, len(ct.linear.vars))
        self.assertEqual(1, len(ct.linear.coeffs))
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(3, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testLe(self):
        print('testLe')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        ct = model.Add(x <= 2).Proto()
        self.assertEqual(1, len(ct.linear.vars))
        self.assertEqual(1, len(ct.linear.coeffs))
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testLt(self):
        print('testLt')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        ct = model.Add(x < 2).Proto()
        self.assertEqual(1, len(ct.linear.vars))
        self.assertEqual(1, len(ct.linear.coeffs))
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def testEqVar(self):
        print('testEqVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x == y + 2).Proto()
        self.assertEqual(2, len(ct.linear.vars))
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertEqual(2, len(ct.linear.coeffs))
        self.assertEqual(0, ct.linear.coeffs[0] + ct.linear.coeffs[1])
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(2, ct.linear.domain[1])

    def testGeVar(self):
        print('testGeVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x >= 1 - y).Proto()
        self.assertEqual(2, len(ct.linear.vars))
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertEqual(2, len(ct.linear.coeffs))
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(1, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testGtVar(self):
        print('testGeVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x > 1 - y).Proto()
        self.assertEqual(2, len(ct.linear.vars))
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertEqual(2, len(ct.linear.coeffs))
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(2, ct.linear.domain[0])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

    def testLeVar(self):
        print('testLeVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x <= 1 - y).Proto()
        self.assertEqual(2, len(ct.linear.vars))
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertEqual(2, len(ct.linear.coeffs))
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(1, ct.linear.domain[1])

    def testLtVar(self):
        print('testLtVar')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x < 1 - y).Proto()
        self.assertEqual(2, len(ct.linear.vars))
        self.assertEqual(1, ct.linear.vars[0] + ct.linear.vars[1])
        self.assertEqual(2, len(ct.linear.coeffs))
        self.assertEqual(1, ct.linear.coeffs[0])
        self.assertEqual(1, ct.linear.coeffs[1])
        self.assertEqual(2, len(ct.linear.domain))
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(0, ct.linear.domain[1])

    def testSimplification1(self):
        print('testSimplification1')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        prod = (x * 2) * 2
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification2(self):
        print('testSimplification2')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        prod = 2 * (x * 2)
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification3(self):
        print('testSimplification3')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        prod = (2 * x) * 2
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testSimplification4(self):
        print('testSimplification4')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        prod = 2 * (2 * x)
        self.assertEqual(x, prod.Expression())
        self.assertEqual(4, prod.Coefficient())

    def testLinearNonEqualWithConstant(self):
        print('testLinearNonEqualWithConstant')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        ct = model.Add(x + y + 5 != 3).Proto()
        self.assertEqual(4, len(ct.linear.domain))
        # Checks that saturated arithmetics worked.
        self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
        self.assertEqual(-3, ct.linear.domain[1])
        self.assertEqual(-1, ct.linear.domain[2])
        self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

    def testLinearWithEnforcement(self):
        print('testLinearWithEnforcement')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        b = model.NewBoolVar('b')
        model.AddLinearConstraint(x + 2 * y, 0, 10).OnlyEnforceIf(
            b.Not())
        model.Minimize(y)
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(-3,
                         model.Proto().constraints[0].enforcement_literal[0])

    def testNaturalApiMinimize(self):
        print('testNaturalApiMinimize')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        model.Add(x * 2 - 1 * y == 1)
        model.Minimize(x * 1 - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual('OPTIMAL', solver.StatusName(solver.Solve(model)))
        self.assertEqual(5, solver.Value(x))
        self.assertEqual(15, solver.Value(x * 3))
        self.assertEqual(6, solver.Value(1 + x))
        self.assertEqual(-10.0, solver.ObjectiveValue())

    def testNaturalApiMaximize(self):
        print('testNaturalApiMaximize')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
        model.Add(2 * x - y == 1)
        model.Maximize(x - 2 * y + 3)
        solver = cp_model.CpSolver()
        self.assertEqual('OPTIMAL', solver.StatusName(solver.Solve(model)))
        self.assertEqual(-4, solver.Value(x))
        self.assertEqual(-9, solver.Value(y))
        self.assertEqual(17, solver.ObjectiveValue())

    def testSum(self):
        print('testSum')
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 2, 'x%i' % i) for i in range(100)]
        model.Add(sum(x) <= 1)
        model.Maximize(x[99])
        solver = cp_model.CpSolver()
        self.assertEqual(cp_model_pb2.OPTIMAL, solver.Solve(model))
        self.assertEqual(1.0, solver.ObjectiveValue())

    def testAllDifferent(self):
        print('testAllDifferent')
        model = cp_model.CpModel()
        x = [model.NewIntVar(0, 4, 'x%i' % i) for i in range(5)]
        model.AddAllDifferent(x)
        self.assertEqual(5, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5, len(model.Proto().constraints[0].all_diff.vars))

    def testMaxEquality(self):
        print('testMaxEquality')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
        model.AddMaxEquality(x, y)
        self.assertEqual(6, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5, len(model.Proto().constraints[0].int_max.vars))
        self.assertEqual(0, model.Proto().constraints[0].int_max.target)

    def testMinEquality(self):
        print('testMinEquality')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
        model.AddMinEquality(x, y)
        self.assertEqual(6, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5, len(model.Proto().constraints[0].int_min.vars))
        self.assertEqual(0, model.Proto().constraints[0].int_min.target)

    def testMinEqualityWithConstant(self):
        print('testMinEqualityWithConstant')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = model.NewIntVar(0, 4, 'y')
        model.AddMinEquality(x, [y, 3])
        self.assertEqual(3, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(2, len(model.Proto().constraints[0].int_min.vars))
        self.assertEqual(1, model.Proto().constraints[0].int_min.vars[0])
        self.assertEqual(2, model.Proto().constraints[0].int_min.vars[1])
        self.assertEqual(3, model.Proto().variables[2].domain[0])
        self.assertEqual(3, model.Proto().variables[2].domain[1])

    def testAbs(self):
        print('testAbs')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = model.NewIntVar(-5, 5, 'y')
        model.AddMaxEquality(x, [-y, y])
        self.assertEqual(2, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(2, len(model.Proto().constraints[0].int_max.vars))
        self.assertEqual(-2, model.Proto().constraints[0].int_max.vars[0])
        self.assertEqual(1, model.Proto().constraints[0].int_max.vars[1])
        passed = False
        try:
            z = abs(x)
        except NotImplementedError as e:
            self.assertEqual(
                'calling abs() on a linear expression is not supported, '
                'please use CpModel.AddAbsEquality',
                str(e))
            passed = True
        self.assertTrue(passed, 'abs() did not raise an error')

    def testDivision(self):
        print('testDivision')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, 'x')
        y = model.NewIntVar(0, 50, 'y')
        model.AddDivisionEquality(x, y, 6)
        self.assertEqual(3, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(2, len(model.Proto().constraints[0].int_div.vars))
        self.assertEqual(1, model.Proto().constraints[0].int_div.vars[0])
        self.assertEqual(2, model.Proto().constraints[0].int_div.vars[1])

    def testModulo(self):
        print('testModulo')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 10, 'x')
        y = model.NewIntVar(0, 50, 'y')
        model.AddModuloEquality(x, y, 6)
        self.assertEqual(3, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(2, len(model.Proto().constraints[0].int_mod.vars))
        self.assertEqual(1, model.Proto().constraints[0].int_mod.vars[0])
        self.assertEqual(2, model.Proto().constraints[0].int_mod.vars[1])

    def testProdEquality(self):
        print('testProdEquality')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
        model.AddProdEquality(x, y)
        self.assertEqual(6, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5, len(model.Proto().constraints[0].int_prod.vars))
        self.assertEqual(0, model.Proto().constraints[0].int_prod.target)

    def testBoolOr(self):
        print('testBoolOr')
        model = cp_model.CpModel()
        x = [model.NewBoolVar('x%i' % i) for i in range(5)]
        model.AddBoolOr(x)
        self.assertEqual(5, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5, len(model.Proto().constraints[0].bool_or.literals))
        model.AddBoolOr([x[0], x[1], False])
        self.assertEqual(6, len(model.Proto().variables))

    def testBoolAnd(self):
        print('testBoolAnd')
        model = cp_model.CpModel()
        x = [model.NewBoolVar('x%i' % i) for i in range(5)]
        model.AddBoolAnd(x)
        self.assertEqual(5, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5,
                         len(model.Proto().constraints[0].bool_and.literals))
        model.AddBoolAnd([x[1], x[2].Not(), True])
        self.assertEqual(1, model.Proto().constraints[1].bool_and.literals[0])
        self.assertEqual(-3, model.Proto().constraints[1].bool_and.literals[1])
        self.assertEqual(5, model.Proto().constraints[1].bool_and.literals[2])

    def testBoolXOr(self):
        print('testBoolXOr')
        model = cp_model.CpModel()
        x = [model.NewBoolVar('x%i' % i) for i in range(5)]
        model.AddBoolXOr(x)
        self.assertEqual(5, len(model.Proto().variables))
        self.assertEqual(1, len(model.Proto().constraints))
        self.assertEqual(5,
                         len(model.Proto().constraints[0].bool_xor.literals))

    def testAssertIsInt64(self):
        print('testAssertIsInt64')
        cp_model_helper.AssertIsInt64(123)
        cp_model_helper.AssertIsInt64(2**63 - 1)
        cp_model_helper.AssertIsInt64(-2**63)

    def testCapInt64(self):
        print('testCapInt64')
        self.assertEqual(
            cp_model_helper.CapInt64(cp_model.INT_MAX), cp_model.INT_MAX)
        self.assertEqual(
            cp_model_helper.CapInt64(cp_model.INT_MAX + 1), cp_model.INT_MAX)
        self.assertEqual(
            cp_model_helper.CapInt64(cp_model.INT_MIN), cp_model.INT_MIN)
        self.assertEqual(
            cp_model_helper.CapInt64(cp_model.INT_MIN - 1), cp_model.INT_MIN)
        self.assertEqual(cp_model_helper.CapInt64(15), 15)

    def testCapSub(self):
        print('testCapSub')
        self.assertEqual(cp_model_helper.CapSub(10, 5), 5)
        self.assertEqual(
            cp_model_helper.CapSub(cp_model.INT_MIN, 5), cp_model.INT_MIN)
        self.assertEqual(
            cp_model_helper.CapSub(cp_model.INT_MIN, -5), cp_model.INT_MIN)
        self.assertEqual(
            cp_model_helper.CapSub(cp_model.INT_MAX, 5), cp_model.INT_MAX)
        self.assertEqual(
            cp_model_helper.CapSub(cp_model.INT_MAX, -5), cp_model.INT_MAX)
        self.assertEqual(
            cp_model_helper.CapSub(2, cp_model.INT_MIN), cp_model.INT_MAX)
        self.assertEqual(
            cp_model_helper.CapSub(2, cp_model.INT_MAX), cp_model.INT_MIN)

    def testGetOrMakeIndexFromConstant(self):
        print('testGetOrMakeIndexFromConstant')
        model = cp_model.CpModel()
        self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
        self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
        self.assertEqual(1, model.GetOrMakeIndexFromConstant(5))
        model_var = model.Proto().variables[0]
        self.assertEqual(2, len(model_var.domain))
        self.assertEqual(3, model_var.domain[0])
        self.assertEqual(3, model_var.domain[1])

    def testStr(self):
        print('testStr')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        self.assertEqual(str(x == 2), 'x == 2')
        self.assertEqual(str(x >= 2), 'x >= 2')
        self.assertEqual(str(x <= 2), 'x <= 2')
        self.assertEqual(str(x > 2), 'x >= 3')
        self.assertEqual(str(x < 2), 'x <= 1')
        self.assertEqual(str(x != 2), 'x != 2')
        self.assertEqual(str(x * 3), '(3 * x)')
        self.assertEqual(str(-x), '-x')
        self.assertEqual(str(x + 3), '(x + 3)')
        self.assertEqual(str(x <= cp_model.INT_MAX), 'True (unbounded expr x)')
        self.assertEqual(
            str(x != 9223372036854775807), 'x <= 9223372036854775806')
        self.assertEqual(
            str(x != -9223372036854775808), 'x >= -9223372036854775807')
        y = model.NewIntVar(0, 4, 'y')
        self.assertEqual(str(x != y), '(x + -y) != 0')
        self.assertEqual('0 <= x <= 10',
                         str(cp_model.BoundedLinearExpression(x, [0, 10])))
        print(str(model))

    def testRepr(self):
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = model.NewIntVar(0, 3, 'y')
        self.assertEqual(repr(x), 'x(0..4)')
        self.assertEqual(repr(x * 2), 'ProductCst(x(0..4), 2)')
        self.assertEqual(repr(x + y), 'SumArray(x(0..4), y(0..3), 0)')
        self.assertEqual(repr(x + 5), 'SumArray(x(0..4), 5)')

    def testDisplayBounds(self):
        print('testDisplayBounds')
        self.assertEqual('10..20', cp_model.DisplayBounds([10, 20]))
        self.assertEqual('10', cp_model.DisplayBounds([10, 10]))
        self.assertEqual('10..15, 20..30',
                         cp_model.DisplayBounds([10, 15, 20, 30]))

    def testIntegerExpressionErrors(self):
        print('testIntegerExpressionErrors')
        model = cp_model.CpModel()
        x = model.NewIntVar(0, 4, 'x')
        y = model.NewIntVar(0, 3, 'y')
        errors = 0
        try:
            not_x = x.Not()
        except:
            errors += 1

        try:
            prod = x * y
        except:
            errors += 1
        self.assertEqual(errors, 2)

    def testLinearizedBoolAndEqual(self):
        print('testLinearizedBoolAndEqual')
        model = cp_model.CpModel()
        t = model.NewBoolVar('t')
        a = model.NewBoolVar('a')
        b = model.NewBoolVar('b')
        model.AddBoolAnd([a, b]).OnlyEnforceIf(t)
        model.Add(t == 1).OnlyEnforceIf([a, b])

        solver = cp_model.CpSolver()
        solution_counter = SolutionCounter()
        solver.parameters.enumerate_all_solutions = True
        status = solver.Solve(model, solution_counter)
        self.assertEqual(status, cp_model.OPTIMAL)
        self.assertEqual(4, solution_counter.solution_count())


    def testSequentialSolve(self):
        print('testSequentialSolve')
        model = cp_model.CpModel()
        t = model.NewBoolVar('t')
        a = model.NewBoolVar('a')
        b = model.NewBoolVar('b')
        model.AddBoolAnd([a, b]).OnlyEnforceIf(t)
        model.Add(t == 1).OnlyEnforceIf([a, b])

        solver = cp_model.CpSolver()
        status1 = solver.Solve(model)
        status2 = solver.Solve(model)
        self.assertEqual(status1, status2)

    def testPresolveBoolean(self):
        print('testPresolveBoolean')
        model = cp_model.CpModel()

        b = model.NewBoolVar('b')
        x = model.NewIntVar(1, 10, 'x')
        y = model.NewIntVar(1, 10, 'y')

        # b implies x == y  via linear inequations
        model.Add(x - y >= 9 * (b - 1))
        model.Add(y - x >= 9 * (b - 1))

        obj = 100 * b + x - 2 * y
        model.Minimize(obj)

        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(solver.ObjectiveValue(), -19.0)

    def testWindowsProtobufOverflow(self):
        print('testWindowsProtobufOverflow')
        model = cp_model.CpModel()

        a = model.NewIntVar(0, 10, 'a')
        b = model.NewIntVar(0, 10, 'b')

        ct = model.Add(a+b >= 5)
        self.assertEqual(ct.Proto().linear.domain[1], cp_model.INT_MAX)

    def testExporToFile(self):
        print('testExporToFile')
        model = cp_model.CpModel()

        a = model.NewIntVar(0, 10, 'a')
        b = model.NewIntVar(0, 10, 'b')

        ct = model.Add(a+b >= 5)
        self.assertTrue(model.ExportToFile('test_model_python.pbtxt'))

    def testProductIsNull(self):
        print('testProductIsNull')
        model = cp_model.CpModel()

        a = model.NewIntVar(0, 10, 'a')
        b = model.NewIntVar(0, 8, 'b')
        p = model.NewIntVar(0, 80, 'p')

        model.AddMultiplicationEquality(p, [a, b])
        model.Add(p == 0)
        solver = cp_model.CpSolver()
        status = solver.Solve(model)
        self.assertEqual(status, cp_model.OPTIMAL)

    def testCustomLog(self):
        print('testCustomLog')
        model = cp_model.CpModel()
        x = model.NewIntVar(-10, 10, 'x')
        y = model.NewIntVar(-10, 10, 'y')
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

        print(log_callback.Log())
        self.assertRegex(log_callback.Log(), 'Parameters.*log_to_stdout.*')

    def testIndexToProto(self):
        print('testIndexToProto')

        model = cp_model.CpModel()

        # Creates the variables.
        v0 = model.NewBoolVar('buggyVarIndexToVarProto')
        v1 = model.NewBoolVar('v1')

        self.assertEqual(model.VarIndexToVarProto(0).name, v0.Name())

    def testWrongBoolEvaluation(self):
        print('testWrongBoolEvaluation')

        model = cp_model.CpModel()

        # Creates the variables.
        v0 = model.NewIntVar(0, 10, 'v0')
        v1 = model.NewIntVar(0, 10, 'v1')
        v2 = model.NewIntVar(0, 10, 'v2')

        with self.assertRaises(NotImplementedError):
            if v0 == 2:
                print('== passed')

        with self.assertRaises(NotImplementedError):
            if v0 >= 3:
                print('>= passed')

        with self.assertRaises(NotImplementedError):
            model.Add(v2 == min(v0, v1))
            print('min passed')

        with self.assertRaises(NotImplementedError):
            if v0:
                print('bool passed')


if __name__ == '__main__':
    unittest.main(verbosity=2)
