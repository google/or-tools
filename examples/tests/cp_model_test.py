"""Tests for ortools.sat.python.cp_model."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat import cp_model_pb2
from ortools.sat.python import cp_model


class CpModelTest(object):

  def assertEqual(self, a, b):
    if isinstance(a, cp_model.CpIntegerVariable) or isinstance(a, cp_model.NotBooleanVariable):
      if a.Index() != b.Index():
        print('Error: ' + repr(a) + ' != ' + repr(b) + '|' + str(a.Index()) +
              " <-> " + str(b.Index()))
    elif a != b:
      print('Error: ' + str(a) + ' != ' + str(b))

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
    nx = x.Not()
    nb = b.Not()
    self.assertEqual(x.Not(), nx)
    self.assertEqual(x.Not().Not(), x)
    self.assertEqual(b.Not(), nb)
    self.assertEqual(b.Not().Not(), b)
    self.assertEqual(nx.Index(), -x.Index() - 1)
    self.assertEqual(nb.Index(), -b.Index() - 1)

  def testLinear(self):
    print('testLinear')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    y = model.NewIntVar(-10, 10, 'y')
    model.AddLinearConstraint([(x, 1), (y, 2)], 0, 10)
    model.Minimize(y)
    solver = cp_model.CpSolver()
    self.assertEqual(cp_model_pb2.OPTIMAL, solver.Solve(model))
    self.assertEqual(10, solver.Value(x))
    self.assertEqual(-5, solver.Value(y))

  def testLinearNonEqual(self):
    print('testLinearNonEqual')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    y = model.NewIntVar(-10, 10, 'y')
    ct = model.Add(-x + y != 3).ConstraintProto()
    self.assertEqual(4, len(ct.linear.domain))
    self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
    self.assertEqual(2, ct.linear.domain[1])
    self.assertEqual(4, ct.linear.domain[2])
    self.assertEqual(cp_model.INT_MAX, ct.linear.domain[3])

  def testEq(self):
    print('testEq')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    ct = model.Add(x == 2).ConstraintProto()
    self.assertEqual(1, len(ct.linear.vars))
    self.assertEqual(1, len(ct.linear.coeffs))
    self.assertEqual(2, len(ct.linear.domain))
    self.assertEqual(2, ct.linear.domain[0])
    self.assertEqual(2, ct.linear.domain[1])

  def testGe(self):
    print('testGe')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    ct = model.Add(x >= 2).ConstraintProto()
    self.assertEqual(1, len(ct.linear.vars))
    self.assertEqual(1, len(ct.linear.coeffs))
    self.assertEqual(2, len(ct.linear.domain))
    self.assertEqual(2, ct.linear.domain[0])
    self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

  def testGt(self):
    print('testGt')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    ct = model.Add(x > 2).ConstraintProto()
    self.assertEqual(1, len(ct.linear.vars))
    self.assertEqual(1, len(ct.linear.coeffs))
    self.assertEqual(2, len(ct.linear.domain))
    self.assertEqual(3, ct.linear.domain[0])
    self.assertEqual(cp_model.INT_MAX, ct.linear.domain[1])

  def testLe(self):
    print('testLe')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    ct = model.Add(x <= 2).ConstraintProto()
    self.assertEqual(1, len(ct.linear.vars))
    self.assertEqual(1, len(ct.linear.coeffs))
    self.assertEqual(2, len(ct.linear.domain))
    self.assertEqual(cp_model.INT_MIN, ct.linear.domain[0])
    self.assertEqual(2, ct.linear.domain[1])

  def testLt(self):
    print('testLt')
    model = cp_model.CpModel()
    x = model.NewIntVar(-10, 10, 'x')
    ct = model.Add(x < 2).ConstraintProto()
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
    ct = model.Add(x == y + 2).ConstraintProto()
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
    ct = model.Add(x >= 1 - y).ConstraintProto()
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
    ct = model.Add(x > 1 - y).ConstraintProto()
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
    ct = model.Add(x <= 1 - y).ConstraintProto()
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
    ct = model.Add(x < 1 - y).ConstraintProto()
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
    ct = model.Add(x + y + 5 != 3).ConstraintProto()
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
    model.AddLinearConstraint([(x, 1), (y, 2)], 0, 10).OnlyEnforceIf(b.Not())
    model.Minimize(y)
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(-3,
                     model.ModelProto().constraints[0].enforcement_literal[0])

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
    self.assertEqual(5, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5, len(model.ModelProto().constraints[0].all_diff.vars))

  def testMaxEquality(self):
    print('testMaxEquality')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
    model.AddMaxEquality(x, y)
    self.assertEqual(6, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5, len(model.ModelProto().constraints[0].int_max.vars))
    self.assertEqual(0, model.ModelProto().constraints[0].int_max.target)

  def testMinEquality(self):
    print('testMinEquality')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
    model.AddMinEquality(x, y)
    self.assertEqual(6, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5, len(model.ModelProto().constraints[0].int_min.vars))
    self.assertEqual(0, model.ModelProto().constraints[0].int_min.target)

  def testMinEqualityWithConstant(self):
    print('testMinEqualityWithConstant')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = model.NewIntVar(0, 4, 'y')
    model.AddMinEquality(x, [y, 3])
    self.assertEqual(3, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(2, len(model.ModelProto().constraints[0].int_min.vars))
    self.assertEqual(1, model.ModelProto().constraints[0].int_min.vars[0])
    self.assertEqual(2, model.ModelProto().constraints[0].int_min.vars[1])
    self.assertEqual(3, model.ModelProto().variables[2].domain[0])
    self.assertEqual(3, model.ModelProto().variables[2].domain[1])

  def testAbs(self):
    print('testAbs')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = model.NewIntVar(-5, 5, 'y')
    model.AddMaxEquality(x, [-y, y])
    self.assertEqual(2, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(2, len(model.ModelProto().constraints[0].int_max.vars))
    self.assertEqual(-2, model.ModelProto().constraints[0].int_max.vars[0])
    self.assertEqual(1, model.ModelProto().constraints[0].int_max.vars[1])

  def testDivision(self):
    print('testDivision')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 10, 'x')
    y = model.NewIntVar(0, 50, 'y')
    model.AddDivisionEquality(x, y, 6)
    self.assertEqual(3, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(2, len(model.ModelProto().constraints[0].int_div.vars))
    self.assertEqual(1, model.ModelProto().constraints[0].int_div.vars[0])
    self.assertEqual(2, model.ModelProto().constraints[0].int_div.vars[1])

  def testModulo(self):
    print('testModulo')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 10, 'x')
    y = model.NewIntVar(0, 50, 'y')
    model.AddModuloEquality(x, y, 6)
    self.assertEqual(3, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(2, len(model.ModelProto().constraints[0].int_mod.vars))
    self.assertEqual(1, model.ModelProto().constraints[0].int_mod.vars[0])
    self.assertEqual(2, model.ModelProto().constraints[0].int_mod.vars[1])

  def testProdEquality(self):
    print('testProdEquality')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = [model.NewIntVar(0, 4, 'y%i' % i) for i in range(5)]
    model.AddProdEquality(x, y)
    self.assertEqual(6, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5, len(model.ModelProto().constraints[0].int_prod.vars))
    self.assertEqual(0, model.ModelProto().constraints[0].int_prod.target)

  def testBoolOr(self):
    print('testBoolOr')
    model = cp_model.CpModel()
    x = [model.NewBoolVar('x%i' % i) for i in range(5)]
    model.AddBoolOr(x)
    self.assertEqual(5, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5, len(model.ModelProto().constraints[0].bool_or.literals))
    model.AddBoolOr([x[0], x[1], False])
    self.assertEqual(6, len(model.ModelProto().variables))

  def testBoolAnd(self):
    print('testBoolAnd')
    model = cp_model.CpModel()
    x = [model.NewBoolVar('x%i' % i) for i in range(5)]
    model.AddBoolAnd(x)
    self.assertEqual(5, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5,
                     len(model.ModelProto().constraints[0].bool_and.literals))
    model.AddBoolAnd([x[1], x[2].Not(), True])
    self.assertEqual(1, model.ModelProto().constraints[1].bool_and.literals[0])
    self.assertEqual(-3, model.ModelProto().constraints[1].bool_and.literals[1])
    self.assertEqual(5, model.ModelProto().constraints[1].bool_and.literals[2])

  def testBoolXOr(self):
    print('testBoolXOr')
    model = cp_model.CpModel()
    x = [model.NewBoolVar('x%i' % i) for i in range(5)]
    model.AddBoolXOr(x)
    self.assertEqual(5, len(model.ModelProto().variables))
    self.assertEqual(1, len(model.ModelProto().constraints))
    self.assertEqual(5,
                     len(model.ModelProto().constraints[0].bool_xor.literals))

  def testAssertIsInt64(self):
    print('testAssertIsInt64')
    cp_model.AssertIsInt64(123)
    cp_model.AssertIsInt64(2**63 - 1)
    cp_model.AssertIsInt64(-2**63)

  def testCapInt64(self):
    print('testCapInt64')
    self.assertEqual(cp_model.CapInt64(cp_model.INT_MAX), cp_model.INT_MAX)
    self.assertEqual(cp_model.CapInt64(cp_model.INT_MAX + 1), cp_model.INT_MAX)
    self.assertEqual(cp_model.CapInt64(cp_model.INT_MIN), cp_model.INT_MIN)
    self.assertEqual(cp_model.CapInt64(cp_model.INT_MIN - 1), cp_model.INT_MIN)
    self.assertEqual(cp_model.CapInt64(15), 15)

  def testCapSub(self):
    print('testCapSub')
    self.assertEqual(cp_model.CapSub(10, 5), 5)
    self.assertEqual(cp_model.CapSub(cp_model.INT_MIN, 5), cp_model.INT_MIN)
    self.assertEqual(cp_model.CapSub(cp_model.INT_MIN, -5), cp_model.INT_MIN)
    self.assertEqual(cp_model.CapSub(cp_model.INT_MAX, 5), cp_model.INT_MAX)
    self.assertEqual(cp_model.CapSub(cp_model.INT_MAX, -5), cp_model.INT_MAX)
    self.assertEqual(cp_model.CapSub(2, cp_model.INT_MIN), cp_model.INT_MAX)
    self.assertEqual(cp_model.CapSub(2, cp_model.INT_MAX), cp_model.INT_MIN)

  def testGetOrMakeIndexFromConstant(self):
    print('testGetOrMakeIndexFromConstant')
    model = cp_model.CpModel()
    self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
    self.assertEqual(0, model.GetOrMakeIndexFromConstant(3))
    self.assertEqual(1, model.GetOrMakeIndexFromConstant(5))
    model_var = model.ModelProto().variables[0]
    self.assertEqual(2, len(model_var.domain))
    self.assertEqual(3, model_var.domain[0])
    self.assertEqual(3, model_var.domain[1])

  def testStr(self):
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    self.assertEqual(str(x == 2), 'x == 2')
    self.assertEqual(str(x >= 2), 'x >= 2')
    self.assertEqual(str(x <= 2), 'x <= 2')
    self.assertEqual(str(x > 2), 'x >= 3')
    self.assertEqual(str(x < 2), 'x <= 1')
    self.assertEqual(
        str(x != 2), 'x in [-9223372036854775808..1, 3..9223372036854775807]')
    self.assertEqual(str(x * 3), '(3 * x)')
    self.assertEqual(str(-x), '-x')
    self.assertEqual(str(x + 3), '(x + 3)')
    self.assertEqual(str(x <= cp_model.INT_MAX), 'True (unbounded expr x)')
    self.assertEqual(str(x != 9223372036854775807), 'x <= 9223372036854775806')
    self.assertEqual(
        str(x != -9223372036854775808), 'x >= -9223372036854775807')
    y = model.NewIntVar(0, 4, 'y')
    self.assertEqual(
        str(x != y),
        '(x + -y) in [-9223372036854775808..-1, 1..9223372036854775807]')
    self.assertEqual('(x * y)', str(x * y))
    self.assertEqual('0 <= x <= 10',
                     str(cp_model.BoundIntegerExpression(x, [0, 10])))
    print(str(model))

  def testProduct(self):
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = model.NewIntVar(0, 3, 'y')
    self.assertEqual(x, (x * y).Left())
    self.assertEqual(y, (x * y).Right())

  def testRepr(self):
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = model.NewIntVar(0, 3, 'y')
    self.assertEqual(repr(x), 'x(0..4)')
    self.assertEqual(repr(x * 2), 'ProductCst(x(0..4), 2)')
    self.assertEqual(repr(x + y), 'SumArray(x(0..4), y(0..3), 0)')
    self.assertEqual(repr(x + 5), 'SumArray(x(0..4), 5)')
    self.assertEqual('Product(x(0..4), y(0..3))', repr(x * y))

  def testDisplayBounds(self):
    print('testDisplayBounds')
    self.assertEqual('10..20', cp_model.DisplayBounds([10, 20]))
    self.assertEqual('10', cp_model.DisplayBounds([10, 10]))
    self.assertEqual('10..15, 20..30', cp_model.DisplayBounds([10, 15, 20, 30]))

  def testIntegerExpressionErrors(self):
    print('testIntegerExpressionErrors')
    model = cp_model.CpModel()
    x = model.NewIntVar(0, 4, 'x')
    y = model.NewIntVar(0, 3, 'y')
    not_x = x.Not()
    prod = x * y



if __name__ == '__main__':
  cp_model_test = CpModelTest()
  cp_model_test.testCreateIntegerVariable()
  cp_model_test.testNegation()
  cp_model_test.testLinear()
  cp_model_test.testLinearNonEqual()
  cp_model_test.testEq()
  cp_model_test.testGe()
  cp_model_test.testGt()
  cp_model_test.testLe()
  cp_model_test.testLt()
  cp_model_test.testEqVar()
  cp_model_test.testGeVar()
  cp_model_test.testGtVar()
  cp_model_test.testLeVar()
  cp_model_test.testLtVar()
  cp_model_test.testSimplification1()
  cp_model_test.testSimplification2()
  cp_model_test.testSimplification3()
  cp_model_test.testSimplification4()
  cp_model_test.testLinearNonEqualWithConstant()
  cp_model_test.testLinearWithEnforcement()
  cp_model_test.testNaturalApiMinimize()
  cp_model_test.testNaturalApiMaximize()
  cp_model_test.testSum()
  cp_model_test.testAllDifferent()
  cp_model_test.testMaxEquality()
  cp_model_test.testMinEquality()
  cp_model_test.testMinEqualityWithConstant()
  cp_model_test.testAbs()
  cp_model_test.testDivision()
  cp_model_test.testModulo()
  cp_model_test.testProdEquality()
  cp_model_test.testBoolOr()
  cp_model_test.testBoolAnd()
  cp_model_test.testBoolXOr()
  cp_model_test.testAssertIsInt64()
  cp_model_test.testCapInt64()
  cp_model_test.testCapSub()
  cp_model_test.testGetOrMakeIndexFromConstant()
  cp_model_test.testStr()
  cp_model_test.testProduct()
  cp_model_test.testRepr()
  cp_model_test.testDisplayBounds()
  cp_model_test.testIntegerExpressionErrors()
