#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

"""Test Constraint Solver API."""

import sys

from absl.testing import absltest
from ortools.constraint_solver import search_limit_pb2
from ortools.constraint_solver import solver_parameters_pb2
from ortools.constraint_solver import pywrapcp


def inc_callback(i):
  return i + 1


class ClassIncCallback:

  def __init__(self, increment):
    self.__increment = increment

  def inc_method(self, i):
    return i + self.__increment


class TestIntVarContainerAPI(absltest.TestCase):

  def test_contains(self):
    self.assertTrue(
        hasattr(pywrapcp.IntVarContainer, "Contains"),
        dir(pywrapcp.IntVarContainer),
    )

  def test_element(self):
    self.assertTrue(
        hasattr(pywrapcp.IntVarContainer, "Element"),
        dir(pywrapcp.IntVarContainer),
    )

  def test_size(self):
    self.assertTrue(
        hasattr(pywrapcp.IntVarContainer, "Size"), dir(pywrapcp.IntVarContainer)
    )

  def test_store(self):
    self.assertTrue(
        hasattr(pywrapcp.IntVarContainer, "Store"),
        dir(pywrapcp.IntVarContainer),
    )

  def test_restore(self):
    self.assertTrue(
        hasattr(pywrapcp.IntVarContainer, "Restore"),
        dir(pywrapcp.IntVarContainer),
    )


class TestIntervalVarContainerAPI(absltest.TestCase):

  def test_contains(self):
    self.assertTrue(
        hasattr(pywrapcp.IntervalVarContainer, "Contains"),
        dir(pywrapcp.IntervalVarContainer),
    )

  def test_element(self):
    self.assertTrue(
        hasattr(pywrapcp.IntervalVarContainer, "Element"),
        dir(pywrapcp.IntervalVarContainer),
    )

  def test_size(self):
    self.assertTrue(
        hasattr(pywrapcp.IntervalVarContainer, "Size"),
        dir(pywrapcp.IntervalVarContainer),
    )

  def test_store(self):
    self.assertTrue(
        hasattr(pywrapcp.IntervalVarContainer, "Store"),
        dir(pywrapcp.IntervalVarContainer),
    )

  def test_restore(self):
    self.assertTrue(
        hasattr(pywrapcp.IntervalVarContainer, "Restore"),
        dir(pywrapcp.IntervalVarContainer),
    )


class TestSequenceVarContainerAPI(absltest.TestCase):

  def test_contains(self):
    self.assertTrue(
        hasattr(pywrapcp.SequenceVarContainer, "Contains"),
        dir(pywrapcp.SequenceVarContainer),
    )

  def test_element(self):
    self.assertTrue(
        hasattr(pywrapcp.SequenceVarContainer, "Element"),
        dir(pywrapcp.SequenceVarContainer),
    )

  def test_size(self):
    self.assertTrue(
        hasattr(pywrapcp.SequenceVarContainer, "Size"),
        dir(pywrapcp.SequenceVarContainer),
    )

  def test_store(self):
    self.assertTrue(
        hasattr(pywrapcp.SequenceVarContainer, "Store"),
        dir(pywrapcp.SequenceVarContainer),
    )

  def test_restore(self):
    self.assertTrue(
        hasattr(pywrapcp.SequenceVarContainer, "Restore"),
        dir(pywrapcp.SequenceVarContainer),
    )


class PyWrapCPTest(absltest.TestCase):

  def testRabbitPheasant(self):
    # Create the solver.
    solver = pywrapcp.Solver("testRabbitPheasant")

    # Create the variables.
    pheasant = solver.IntVar(0, 100, "pheasant")
    rabbit = solver.IntVar(0, 100, "rabbit")

    # Create the constraints.
    solver.Add(pheasant + rabbit == 20)
    solver.Add(pheasant * 2 + rabbit * 4 == 56)

    # Create the search phase.
    db = solver.Phase(
        [rabbit, pheasant], solver.INT_VAR_DEFAULT, solver.ASSIGN_MIN_VALUE
    )

    # Create assignment
    solution = solver.Assignment()
    solution.Add(rabbit)
    solution.Add(pheasant)

    collector = solver.FirstSolutionCollector(solution)

    # And solve.
    solver.Solve(db, collector)

    self.assertEqual(1, collector.SolutionCount())
    current = collector.Solution(0)

    self.assertEqual(12, current.Value(pheasant))
    self.assertEqual(8, current.Value(rabbit))

  def testSolverParameters(self):
    # Create the parameters.
    params = pywrapcp.Solver.DefaultSolverParameters()
    self.assertIsInstance(
        params, solver_parameters_pb2.ConstraintSolverParameters
    )
    self.assertFalse(params.trace_propagation)
    params.trace_propagation = True
    self.assertTrue(params.trace_propagation)

    # Create the solver.
    solver = pywrapcp.Solver("testRabbitPheasantWithParameters", params)
    inside_params = solver.Parameters()
    self.assertTrue(inside_params.trace_propagation)

  def testSolverParametersFields(self):
    params = solver_parameters_pb2.ConstraintSolverParameters()
    bool_params = [
        "store_names",
        "name_cast_variables",
        "name_all_variables",
        "profile_propagation",
        "trace_propagation",
        "trace_search",
        "print_model",
        "print_model_stats",
        "print_added_constraints",
        "disable_solve",
    ]
    for p in bool_params:
      for v in [True, False]:
        setattr(params, p, v)
        self.assertEqual(getattr(params, p), v)

    int_params = ["trail_block_size", "array_split_size"]
    for p in int_params:
      for v in [10, 100]:
        setattr(params, p, v)
        self.assertEqual(getattr(params, p), v)

    string_params = ["profile_file"]
    for p in string_params:
      for v in ["", "tmp_file"]:
        setattr(params, p, v)
        self.assertEqual(getattr(params, p), v)

  def testIntVarAPI(self):
    # Create the solver.
    solver = pywrapcp.Solver("testIntVarAPI")

    c = solver.IntConst(3, "c")
    self.assertEqual(3, c.Min())
    self.assertEqual(3, c.Max())
    self.assertEqual(3, c.Value())
    self.assertTrue(c.Bound())

    b = solver.BoolVar("b")
    self.assertEqual(0, b.Min())
    self.assertEqual(1, b.Max())

    v1 = solver.IntVar(3, 10, "v1")
    self.assertEqual(3, v1.Min())
    self.assertEqual(10, v1.Max())

    v2 = solver.IntVar([1, 5, 3], "v2")
    self.assertEqual(1, v2.Min())
    self.assertEqual(5, v2.Max())
    self.assertEqual(3, v2.Size())

  # pylint: disable=too-many-statements
  def testIntegerArithmetic(self):
    solver = pywrapcp.Solver("testIntegerArithmetic")

    v1 = solver.IntVar(0, 10, "v1")
    v2 = solver.IntVar(0, 10, "v2")
    v3 = solver.IntVar(0, 10, "v3")

    e1 = v1 + v2
    e2 = v1 + 2
    e3 = solver.Sum([v1, v2, v3 * 3])

    e4 = v1 - 3
    e5 = v1 - v2
    e6 = -v1

    e7 = abs(e6)
    e8 = v3.Square()

    e9 = v1 * 3
    e10 = v1 * v2

    e11 = v2.IndexOf([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])
    e11b = v2.IndexOf([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])
    e12 = solver.Min(e1, e2)
    e13 = solver.Min(e3, 3)
    e14 = solver.Min([e1 + 1, e2 + 2, e3 + 3])

    e15 = solver.Max(e1, e2)
    e16 = solver.Max(e3, 3)
    e17 = solver.Max([e1 + 1, e2 + 2, e3 + 3])

    solver.Add(v1 == 1)
    solver.Add(v2 == 2)
    solver.Add(v3 == 3)

    db = solver.Phase(
        [v1, v2, v3], solver.INT_VAR_DEFAULT, solver.ASSIGN_MIN_VALUE
    )

    solver.NewSearch(db)
    solver.NextSolution()

    self.assertEqual(1, v1.Value())
    self.assertEqual(2, v2.Value())
    self.assertEqual(3, v3.Value())

    self.assertEqual(3, e1.Min())
    self.assertEqual(3, e1.Max())
    self.assertEqual(3, e2.Min())
    self.assertEqual(3, e2.Max())
    self.assertEqual(12, e3.Min())
    self.assertEqual(12, e3.Max())
    self.assertEqual(-2, e4.Min())
    self.assertEqual(-2, e4.Max())
    self.assertEqual(-1, e5.Min())
    self.assertEqual(-1, e5.Max())
    self.assertEqual(-1, e6.Min())
    self.assertEqual(-1, e6.Max())
    self.assertEqual(1, e7.Min())
    self.assertEqual(1, e7.Max())
    self.assertEqual(9, e8.Min())
    self.assertEqual(9, e8.Max())
    self.assertEqual(3, e9.Min())
    self.assertEqual(3, e9.Max())
    self.assertEqual(2, e10.Min())
    self.assertEqual(2, e10.Max())
    self.assertEqual(4, e11.Min())
    self.assertEqual(4, e11.Max())
    self.assertEqual(4, e11b.Min())
    self.assertEqual(4, e11b.Max())
    self.assertEqual(3, e12.Min())
    self.assertEqual(3, e12.Max())
    self.assertEqual(3, e13.Min())
    self.assertEqual(3, e13.Max())
    self.assertEqual(4, e14.Min())
    self.assertEqual(4, e14.Max())
    self.assertEqual(3, e15.Min())
    self.assertEqual(3, e15.Max())
    self.assertEqual(12, e16.Min())
    self.assertEqual(12, e16.Max())
    self.assertEqual(15, e17.Min())
    self.assertEqual(15, e17.Max())
    solver.EndSearch()

  def testStatusVar(self):
    solver = pywrapcp.Solver("testStatusVar")
    v1 = solver.IntVar(0, 10, "v1")
    v2 = solver.IntVar(0, 10, "v2")
    c1 = v1 == 3
    c2 = v1 != 2
    print(c1)
    print(c1.Var())
    print(c2)
    print(c2.Var())
    e3 = v1 + c1
    print(e3)
    e4 = c1 + c2 == 1
    print(e4)
    e5 = solver.Min(c1, c2)
    print(e5)
    e6 = solver.Max([c1, c2, e3])
    print(e6)
    e7 = 1 + c2
    print(e7)
    e8 = solver.Max([v1 > 3, v1 <= 2, v2, v2 <= 0, v2 > 5])
    print(e8)
    e9 = solver.Min([v1 == v2, v1 != v2, v1 < v2, v1 > v2, v1 <= v2, v1 >= v2])
    print(e9)

  def testAllowedAssignment(self):
    solver = pywrapcp.Solver("testAllowedAssignment")

    v1 = solver.IntVar(0, 10, "v1")
    v2 = solver.IntVar(0, 10, "v2")
    v3 = solver.IntVar(0, 10, "v3")

    tuples = [(0, 0, 0), (1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4)]
    dvars = [v1, v2, v3]
    solver.Add(solver.AllowedAssignments(dvars, tuples))
    db = solver.Phase(
        dvars, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )

    solver.NewSearch(db)
    counter = 0
    while solver.NextSolution():
      self.assertEqual(counter, v1.Value())
      self.assertEqual(counter, v2.Value())
      self.assertEqual(counter, v3.Value())
      counter += 1
    solver.EndSearch()
    self.assertEqual(5, counter)

  def testAllowedAssignment2(self):
    solver = pywrapcp.Solver("testAllowedAssignment")

    v1 = solver.IntVar(0, 10, "v1")
    v2 = solver.IntVar(0, 10, "v2")
    v3 = solver.IntVar(0, 10, "v3")

    dvars = [v1, v2, v3]
    solver.Add(solver.AllowedAssignments(dvars, [(x, x, x) for x in range(5)]))
    db = solver.Phase(
        dvars, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )

    solver.NewSearch(db)
    counter = 0
    while solver.NextSolution():
      self.assertEqual(counter, v1.Value())
      self.assertEqual(counter, v2.Value())
      self.assertEqual(counter, v3.Value())
      counter += 1
    solver.EndSearch()
    self.assertEqual(5, counter)

  def testIntExprToIntVarCast(self):
    solver = pywrapcp.Solver("testIntExprToIntVarCast")

    var1 = solver.IntVar(0, 10, "var1")
    var2 = solver.IntVar(0, 10, "var2")
    values = [1, 3, 5, 7, 9, 2, 4, 6, 8, 0]
    # This test fails if the cast is not correctly done.
    expr = (var1 + var2).IndexOf(values)
    self.assertTrue(expr)

  def testIntExprToIntVarCastInSolution(self):
    solver = pywrapcp.Solver("testIntExprToIntVarCastInSolution")

    var1 = solver.IntVar(0, 10, "var1")
    var2 = solver.IntVar(0, 10, "var2")
    solution = solver.Assignment()
    expr = var1 + var2
    solution.Add(expr)
    solution.Store()
    # The next line fails if the cast is not correctly done.
    self.assertEqual(20, solution.Max(expr))

  def testIndexOf(self):
    solver = pywrapcp.Solver("element")
    index = solver.IntVar(0, 2, "index")
    element = index.IndexOf([1, 2, 3])
    self.assertEqual(1, element.Min())
    self.assertEqual(3, element.Max())

  def testElementFunction(self):
    solver = pywrapcp.Solver("element")
    index = solver.IntVar(0, 2, "index")
    element = solver.ElementFunction(inc_callback, index)
    self.assertEqual(1, element.Min())
    self.assertEqual(3, element.Max())

  def testElementMethod(self):
    solver = pywrapcp.Solver("element")
    index = solver.IntVar(0, 2, "index")
    class_callback = ClassIncCallback(2)
    class_method = class_callback.inc_method
    self.assertEqual(5, class_method(3))
    element = solver.ElementFunction(class_method, index)
    self.assertEqual(2, element.Min())
    self.assertEqual(4, element.Max())

  # TODO(user): better test all other ForwardSequence methods.
  def testForwardSequence(self):
    solver = pywrapcp.Solver("element")
    intervals = [
        solver.FixedDurationIntervalVar(0, 10, 5, False, "Youpi")
        for _ in range(10)
    ]
    disjunction = solver.DisjunctiveConstraint(intervals, "Blup")
    sequence = disjunction.SequenceVar()
    assignment = solver.Assignment()
    assignment.Add(sequence)
    assignment.SetForwardSequence(sequence, [1, 3, 5])
    self.assertListEqual(assignment.ForwardSequence(sequence), [1, 3, 5])

  def test_member(self):
    solver = pywrapcp.Solver("test member")
    x = solver.IntVar(1, 10, "x")
    ct = x.Member([1, 2, 3, 5])
    print("Constraint: {}".format(ct))

  def test_sparse_var(self):
    solver = pywrapcp.Solver("test sparse")
    x = solver.IntVar([1, 3, 5], "x")
    self.assertTrue(x.Contains(1))
    self.assertFalse(x.Contains(2))
    # print(x)

  def test_modulo(self):
    solver = pywrapcp.Solver("test modulo")
    x = solver.IntVar(0, 10, "x")
    y = solver.IntVar(2, 4, "y")
    print(x % 3)
    print(x % y)

  def test_modulo2(self):
    solver = pywrapcp.Solver("test modulo")
    x = solver.IntVar([-7, 7], "x")
    y = solver.IntVar([-4, 4], "y")
    z = (x % y).Var()
    t = (x // y).Var()
    db = solver.Phase(
        [x, y], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )
    solver.NewSearch(db)
    while solver.NextSolution():
      print(
          "x = %d, y = %d, x %% y = %d, x div y = %d"
          % (x.Value(), y.Value(), z.Value(), t.Value())
      )
    solver.EndSearch()

  def test_limit(self):
    solver = pywrapcp.Solver("test limit")
    # limit_proto = solver.DefaultSearchLimitParameters()
    limit_proto = search_limit_pb2.RegularLimitParameters(
        time=10000, branches=10
    )
    print("limit proto: {}".format(limit_proto))
    limit = solver.Limit(limit_proto)
    print("limit: {}".format(limit))

  def test_export(self):
    solver = pywrapcp.Solver("test export")
    x = solver.IntVar(1, 10, "x")
    ct = x.Member([1, 2, 3, 5])
    solver.Add(ct)
    # proto = model_pb2.CpModel()
    # proto.model = 'wrong name'
    # solver.ExportModel(proto)
    # print(repr(proto))
    # print(str(proto))

  def test_size_1_var(self):
    solver = pywrapcp.Solver("test_size_1_var")
    x = solver.IntVar([0], "x")
    self.assertTrue(x.Contains(0))
    self.assertFalse(x.Contains(1))

  def test_cumulative_api(self):
    solver = pywrapcp.Solver("Problem")

    # Vars
    intervals = [
        solver.FixedDurationIntervalVar(0, 10, 5, False, "S_%s" % a)
        for a in range(10)
    ]
    demands = [a % 3 + 2 for a in range(10)]
    capacity = solver.IntVar(2, 5)
    solver.Add(solver.Cumulative(intervals, demands, capacity, "cumul"))

  def test_search_alldiff(self):
    solver = pywrapcp.Solver("test_search_alldiff")
    in_pos = [solver.IntVar(0, 7, "%i" % i) for i in range(8)]
    solver.Add(solver.AllDifferent(in_pos))
    aux_phase = solver.Phase(
        in_pos, solver.CHOOSE_LOWEST_MIN, solver.ASSIGN_MAX_VALUE
    )
    collector = solver.FirstSolutionCollector()
    for i in range(8):
      collector.Add(in_pos[i])
    solver.Solve(aux_phase, [collector])
    for i in range(8):
      print(collector.Value(0, in_pos[i]))


class CustomSearchMonitor(pywrapcp.SearchMonitor):

  def __init__(self, solver, nexts):
    pywrapcp.SearchMonitor.__init__(self, solver)
    self._nexts = nexts

  def BeginInitialPropagation(self):
    print(self._nexts)

  def EndInitialPropagation(self):
    print(self._nexts)


class SearchMonitorTest(absltest.TestCase):

  def test_search_monitor(self):
    print("test_search_monitor")
    solver = pywrapcp.Solver("test search monitor")
    x = solver.IntVar(1, 10, "x")
    ct = x == 3
    solver.Add(ct)
    db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
    monitor = CustomSearchMonitor(solver, x)
    solver.Solve(db, monitor)


class CustomDemon(pywrapcp.PyDemon):

  def __init__(self, x):
    super().__init__()
    self._x = x
    print("Demon built")

  def Run(self, solver):
    print("in Run(), saw " + str(self._x))


class DemonTest(absltest.TestCase):

  def test_demon(self):
    print("test_demon")
    solver = pywrapcp.Solver("test export")
    x = solver.IntVar(1, 10, "x")
    demon = CustomDemon(x)
    demon.Run(solver)


class CustomConstraint(pywrapcp.PyConstraint):

  def __init__(self, solver, x):
    super().__init__(solver)
    self._x = x
    print("Constraint built")

  def Post(self):
    print("in Post()", file=sys.stderr)
    self._demon = CustomDemon(self._x)
    self._x.WhenBound(self._demon)
    print("out of Post()", file=sys.stderr)

  def InitialPropagate(self):
    print("in InitialPropagate()")
    self._x.SetMin(5)
    print(self._x)
    print("out of InitialPropagate()")

  def DebugString(self):
    return "CustomConstraint"


class InitialPropagateDemon(pywrapcp.PyDemon):

  def __init__(self, constraint):
    super().__init__()
    self._ct = constraint

  def Run(self, solver):
    self._ct.InitialPropagate()


class DumbGreaterOrEqualToFive(pywrapcp.PyConstraint):

  def __init__(self, solver, x):
    super().__init__(solver)
    self._x = x

  def Post(self):
    self._demon = InitialPropagateDemon(self)
    self._x.WhenBound(self._demon)

  def InitialPropagate(self):
    if self._x.Bound():
      if self._x.Value() < 5:
        print("Reject %d" % self._x.Value(), file=sys.stderr)
        self.solver().Fail()
      else:
        print("Accept %d" % self._x.Value(), file=sys.stderr)


class WatchDomain(pywrapcp.PyDemon):

  def __init__(self, x):
    super().__init__()
    self._x = x

  def Run(self, solver):
    for i in self._x.HoleIterator():
      print("Removed %d" % i)


class HoleConstraint(pywrapcp.PyConstraint):

  def __init__(self, solver, x):
    super().__init__(solver)
    self._x = x

  def Post(self):
    self._demon = WatchDomain(self._x)
    self._x.WhenDomain(self._demon)

  def InitialPropagate(self):
    self._x.RemoveValue(5)


class BinarySum(pywrapcp.PyConstraint):

  def __init__(self, solver, x, y, z):
    super().__init__(solver)
    self._x = x
    self._y = y
    self._z = z

  def Post(self):
    self._demon = InitialPropagateDemon(self)
    self._x.WhenRange(self._demon)
    self._y.WhenRange(self._demon)
    self._z.WhenRange(self._demon)

  def InitialPropagate(self):
    self._z.SetRange(
        self._x.Min() + self._y.Min(), self._x.Max() + self._y.Max()
    )
    self._x.SetRange(
        self._z.Min() - self._y.Max(), self._z.Max() - self._y.Min()
    )
    self._y.SetRange(
        self._z.Min() - self._x.Max(), self._z.Max() - self._x.Min()
    )


class ConstraintTest(absltest.TestCase):

  def test_member(self):
    print("test_member")
    solver = pywrapcp.Solver("test member")
    x = solver.IntVar(1, 10, "x")
    constraint = x.Member([1, 2, 3, 5])
    print(constraint)

  def test_sparse_var(self):
    print("test_sparse_var")
    solver = pywrapcp.Solver("test_sparse_var")
    x = solver.IntVar([1, 3, 5], "x")
    print(x)

  def test_modulo(self):
    print("test_modulo")
    solver = pywrapcp.Solver("test_modulo")
    x = solver.IntVar(0, 10, "x")
    y = solver.IntVar(2, 4, "y")
    print(x % 3)
    print(x % y)

  def test_limit(self):
    solver = pywrapcp.Solver("test_limit")
    # TODO(user): expose the proto-based MakeLimit() API in or-tools and test it
    # here.
    time = 10000  # ms
    branches = 10
    failures = sys.maxsize
    solutions = sys.maxsize
    smart_time_check = True
    cumulative = False
    limit = solver.Limit(
        time, branches, failures, solutions, smart_time_check, cumulative
    )
    print(limit)

  def test_search_monitor(self):
    print("test_search_monitor")
    solver = pywrapcp.Solver("test search_monitor")
    x = solver.IntVar(1, 10, "x")
    ct = x == 3
    solver.Add(ct)
    db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
    monitor = CustomSearchMonitor(solver, x)
    solver.Solve(db, monitor)

  def test_constraint(self):
    print("test_constraint")
    solver = pywrapcp.Solver("test_constraint")
    x = solver.IntVar(1, 10, "x")
    myct = CustomConstraint(solver, x)
    solver.Add(myct)
    db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
    solver.Solve(db)

  def test_failing_constraint(self):
    print("test_failing_constraint")
    solver = pywrapcp.Solver("test failing constraint")
    x = solver.IntVar(1, 10, "x")
    myct = DumbGreaterOrEqualToFive(solver, x)
    solver.Add(myct)
    db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
    solver.Solve(db)

  def test_domain_iterator(self):
    print("test_domain_iterator")
    solver = pywrapcp.Solver("test_domain_iterator")
    x = solver.IntVar([1, 2, 4, 6], "x")
    for i in x.DomainIterator():
      print(i)

  def test_hole_iterator(self):
    print("test_hole_iterator")
    solver = pywrapcp.Solver("test_hole_iterator")
    x = solver.IntVar(1, 10, "x")
    myct = HoleConstraint(solver, x)
    solver.Add(myct)
    db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
    solver.Solve(db)

  def test_sum_constraint(self):
    print("test_sum_constraint")
    solver = pywrapcp.Solver("test_sum_constraint")
    x = solver.IntVar(1, 5, "x")
    y = solver.IntVar(1, 5, "y")
    z = solver.IntVar(1, 5, "z")
    binary_sum = BinarySum(solver, x, y, z)
    solver.Add(binary_sum)
    db = solver.Phase(
        [x, y, z], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )
    solver.NewSearch(db)
    while solver.NextSolution():
      print("%d + %d == %d" % (x.Value(), y.Value(), z.Value()))
    solver.EndSearch()


class CustomDecisionBuilder(pywrapcp.PyDecisionBuilder):

  def __init__(self):
    super().__init__()
    self._counter = 0

  def Next(self, solver):
    print("In Next", file=sys.stderr)
    self._counter += 1
    return None

  def DebugString(self):
    return "CustomDecisionBuilder"


class CustomDecision(pywrapcp.PyDecision):

  def __init__(self):
    print("In CustomDecision ctor", file=sys.stderr)
    super().__init__()
    self._val = 1
    print("Set value to", self._val, file=sys.stderr)

  def Apply(self, solver):
    print("In CustomDecision.Apply()", file=sys.stderr)
    print("Expect value", self._val, file=sys.stderr)
    solver.Fail()

  def Refute(self, solver):
    print("In CustomDecision.Refute()", file=sys.stderr)

  def DebugString(self):
    return "CustomDecision"


class CustomDecisionBuilderCustomDecision(pywrapcp.PyDecisionBuilder):

  def __init__(self):
    super().__init__()
    self.__done = False
    self._counter = 0

  def Next(self, solver):
    print("In CustomDecisionBuilderCustomDecision.Next()", file=sys.stderr)
    self._counter += 1
    if not self.__done:
      self.__done = True
      self.__decision = CustomDecision()
      return self.__decision
    return None

  def DebugString(self):
    return "CustomDecisionBuilderCustomDecision"


class DecisionTest(absltest.TestCase):

  def test_custom_decision_builder(self):
    solver = pywrapcp.Solver("test_custom_decision_builder")
    db = CustomDecisionBuilder()
    print(str(db))
    solver.Solve(db)
    self.assertEqual(db._counter, 1)

  def test_custom_decision(self):
    solver = pywrapcp.Solver("test_custom_decision")
    db = CustomDecisionBuilderCustomDecision()
    print(str(db))
    solver.Solve(db)
    self.assertEqual(db._counter, 2)


class LocalSearchTest(absltest.TestCase):

  class OneVarLNS(pywrapcp.BaseLns):
    """One Var LNS."""

    def __init__(self, int_vars):
      super().__init__(int_vars)
      self.__index = 0

    def InitFragments(self):
      print("OneVarLNS.InitFragments()...", file=sys.stderr)
      self.__index = 0

    def NextFragment(self):
      print("OneVarLNS.NextFragment()...", file=sys.stderr)
      if self.__index < self.Size():
        self.AppendToFragment(self.__index)
        self.__index += 1
        return True
      else:
        return False

  class MoveOneVar(pywrapcp.IntVarLocalSearchOperator):
    """Move one var up or down."""

    def __init__(self, int_vars):
      super().__init__(int_vars)
      self.__index = 0
      self.__up = False

    def OneNeighbor(self):
      print("MoveOneVar.OneNeighbor()...", file=sys.stderr)
      current_value = self.OldValue(self.__index)
      if self.__up:
        self.SetValue(self.__index, current_value + 1)
        self.__index = (self.__index + 1) % self.Size()
      else:
        self.SetValue(self.__index, current_value - 1)
      self.__up = not self.__up
      return True

    def OnStart(self):
      print("MoveOneVar.OnStart()...", file=sys.stderr)
      pass

    def IsIncremental(self):
      return False

  class SumFilter(pywrapcp.IntVarLocalSearchFilter):
    """Filter to speed up LS computation."""

    def __init__(self, int_vars):
      super().__init__(int_vars)
      self.__sum = 0

    def OnSynchronize(self, delta):
      self.__sum = sum(self.Value(index) for index in range(self.Size()))

    def Accept(
        self,
        delta,
        unused_delta_delta,
        unused_objective_min,
        unused_objective_max,
    ):
      solution_delta = delta.IntVarContainer()
      solution_delta_size = solution_delta.Size()
      for i in range(solution_delta_size):
        if not solution_delta.Element(i).Activated():
          return True
      new_sum = self.__sum
      for i in range(solution_delta_size):
        element = solution_delta.Element(i)
        int_var = element.Var()
        touched_var_index = self.IndexFromVar(int_var)
        old_value = self.Value(touched_var_index)
        new_value = element.Value()
        new_sum += new_value - old_value

      return new_sum < self.__sum

    def IsIncremental(self):
      return False

  def solve(self, local_search_type):
    solver = pywrapcp.Solver("Solve")
    int_vars = [solver.IntVar(0, 4) for _ in range(4)]
    sum_var = solver.Sum(int_vars).Var()
    objective = solver.Minimize(sum_var, 1)
    inner_db = solver.Phase(
        int_vars, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE
    )
    if local_search_type == 0:  # LNS
      print("Large Neighborhood Search", file=sys.stderr)
      one_var_lns = self.OneVarLNS(int_vars)
      ls_params = solver.LocalSearchPhaseParameters(
          sum_var, one_var_lns, inner_db
      )
      ls = solver.LocalSearchPhase(int_vars, inner_db, ls_params)
    elif local_search_type == 1:  # LS
      print("Local Search", file=sys.stderr)
      move_one_var = self.MoveOneVar(int_vars)
      ls_params = solver.LocalSearchPhaseParameters(
          sum_var, move_one_var, inner_db
      )
      ls = solver.LocalSearchPhase(int_vars, inner_db, ls_params)
    else:
      print("Local Search with Filter", file=sys.stderr)
      move_one_var = self.MoveOneVar(int_vars)
      sum_filter = self.SumFilter(int_vars)
      filter_manager = pywrapcp.LocalSearchFilterManager([sum_filter])
      ls_params = solver.LocalSearchPhaseParameters(
          sum_var, move_one_var, inner_db, None, filter_manager
      )
      ls = solver.LocalSearchPhase(int_vars, inner_db, ls_params)

    collector = solver.LastSolutionCollector()
    collector.Add(int_vars)
    collector.AddObjective(sum_var)
    log = solver.SearchLog(1000, objective)
    solver.Solve(ls, [collector, objective, log])
    print("Objective value = %d" % collector.ObjectiveValue(0), file=sys.stderr)

  def test_large_neighborhood_search(self):
    self.solve(0)

  def test_local_search(self):
    self.solve(1)

  def test_local_search_with_filter(self):
    self.solve(2)


class MyDecisionBuilder(pywrapcp.PyDecisionBuilder):

  def __init__(self, var, value):
    super().__init__()
    self.__var = var
    self.__value = value

  def Next(self, solver):
    if not self.__var.Bound():
      decision = solver.AssignVariableValue(self.__var, self.__value)
      return decision


class MyLns(pywrapcp.BaseLns):

  def __init__(self, int_vars):
    super().__init__(int_vars)
    self.__current = 0

  def InitFragments(self):
    self.__current = 0

  def NextFragment(self, fragment, values):
    while self.__current < len(values):
      if values[self.__current] == 1:
        fragment.append(self.__current)
        self.__current += 1
        return True
      else:
        self.__current += 1


class MyLnsNoValues(pywrapcp.BaseLns):

  def __init__(self, int_vars):
    super().__init__(int_vars)
    self.__current = 0
    self.__size = len(int_vars)

  def InitFragments(self):
    self.__current = 0

  def NextFragment(self, fragment):
    while self.__current < self.__size:
      fragment.append(self.__current)
      self.__current += 1
      return True


class MyDecisionBuilderWithRev(pywrapcp.PyDecisionBuilder):

  def __init__(self, var, value, rev):
    super().__init__()
    self.__var = var
    self.__value = value
    self.__rev = rev

  def Next(self, solver):
    if not self.__var.Bound():
      if self.__var.Contains(self.__value):
        decision = solver.AssignVariableValue(self.__var, self.__value)
        self.__rev.SetValue(solver, self.__value)
        return decision
      else:
        return solver.FailDecision()


class MyDecisionBuilderThatFailsWithRev(pywrapcp.PyDecisionBuilder):

  def Next(self, solver):
    solver.Fail()
    return None


class PyWrapCPSearchTest(absltest.TestCase):
  NUMBER_OF_VARIABLES = 10
  VARIABLE_MIN = 0
  VARIABLE_MAX = 10
  LNS_NEIGHBORS = 100
  LNS_VARIABLES = 4
  DECISION_BUILDER_VALUE = 5
  OTHER_DECISION_BUILDER_VALUE = 2

  def testNewClassAsDecisionBuilder(self):
    solver = pywrapcp.Solver("testNewClassAsDecisionBuilder")
    x = solver.IntVar(self.VARIABLE_MIN, self.VARIABLE_MAX, "x")
    phase = MyDecisionBuilder(x, self.DECISION_BUILDER_VALUE)
    solver.NewSearch(phase)
    solver.NextSolution()
    self.assertTrue(x.Bound())
    self.assertEqual(self.DECISION_BUILDER_VALUE, x.Min())
    solver.EndSearch()

  def testComposeTwoDecisions(self):
    solver = pywrapcp.Solver("testNewClassAsDecisionBuilder")
    x = solver.IntVar(0, 10, "x")
    y = solver.IntVar(0, 10, "y")
    phase_x = MyDecisionBuilder(x, self.DECISION_BUILDER_VALUE)
    phase_y = MyDecisionBuilder(y, self.OTHER_DECISION_BUILDER_VALUE)
    phase = solver.Compose([phase_x, phase_y])
    solver.NewSearch(phase)
    solver.NextSolution()
    self.assertTrue(x.Bound())
    self.assertEqual(self.DECISION_BUILDER_VALUE, x.Min())
    self.assertTrue(y.Bound())
    self.assertEqual(self.OTHER_DECISION_BUILDER_VALUE, y.Min())
    solver.EndSearch()

  def testRandomLns(self):
    solver = pywrapcp.Solver("testRandomLnsOperator")
    x = [solver.BoolVar("x_%d" % i) for i in range(self.NUMBER_OF_VARIABLES)]
    lns = solver.RandomLnsOperator(x, self.LNS_VARIABLES)
    delta = solver.Assignment()
    for _ in range(self.LNS_NEIGHBORS):
      delta.Clear()
      self.assertTrue(lns.NextNeighbor(delta, delta))
      self.assertLess(0, delta.Size())
      self.assertGreater(self.LNS_VARIABLES + 1, delta.Size())

  def testCallbackLns(self):
    solver = pywrapcp.Solver("testCallbackLNS")
    x = [solver.BoolVar("x_%d" % i) for i in range(self.NUMBER_OF_VARIABLES)]
    lns = MyLns(x)
    solution = solver.Assignment()
    solution.Add(x)
    for v in x:
      solution.SetValue(v, 1)
    obj_var = solver.Sum(x)
    objective = solver.Minimize(obj_var, 1)
    collector = solver.LastSolutionCollector(solution)
    inner_db = solver.Phase(
        x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )

    ls_params = solver.LocalSearchPhaseParameters(obj_var.Var(), lns, inner_db)
    ls = solver.LocalSearchPhase(x, inner_db, ls_params)
    log = solver.SearchLog(1000, objective)
    solver.Solve(ls, [collector, objective, log])
    for v in x:
      self.assertEqual(0, collector.Solution(0).Value(v))

  def testCallbackLnsNoValues(self):
    solver = pywrapcp.Solver("testCallbackLnsNoValues")
    x = [solver.BoolVar("x_%d" % i) for i in range(self.NUMBER_OF_VARIABLES)]
    lns = MyLnsNoValues(x)
    solution = solver.Assignment()
    solution.Add(x)
    for v in x:
      solution.SetValue(v, 1)
    obj_var = solver.Sum(x)
    objective = solver.Minimize(obj_var, 1)
    collector = solver.LastSolutionCollector(solution)
    inner_db = solver.Phase(
        x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )

    ls_params = solver.LocalSearchPhaseParameters(obj_var.Var(), lns, inner_db)
    db = solver.LocalSearchPhase(x, inner_db, ls_params)
    log = solver.SearchLog(1000, objective)
    solver.Solve(db, [collector, objective, log])
    for v in x:
      self.assertEqual(0, collector.Solution(0).Value(v))

  def testConcatenateOperators(self):
    solver = pywrapcp.Solver("testConcatenateOperators")
    x = [solver.BoolVar("x_%d" % i) for i in range(self.NUMBER_OF_VARIABLES)]
    op1 = solver.Operator(x, solver.INCREMENT)
    op2 = solver.Operator(x, solver.DECREMENT)
    concatenate = solver.ConcatenateOperators([op1, op2])
    solution = solver.Assignment()
    solution.Add(x)
    for v in x:
      solution.SetValue(v, 1)
    obj_var = solver.Sum(x)
    objective = solver.Minimize(obj_var, 1)
    collector = solver.LastSolutionCollector(solution)
    ls_params = solver.LocalSearchPhaseParameters(
        obj_var.Var(), concatenate, None
    )
    db = solver.LocalSearchPhase(solution, ls_params)
    solver.Solve(db, [objective, collector])
    for v in x:
      self.assertEqual(0, collector.Solution(0).Value(v))

  def testRevIntegerOutsideSearch(self):
    solver = pywrapcp.Solver("testRevValue")
    revx = pywrapcp.RevInteger(12)
    self.assertEqual(12, revx.Value())
    revx.SetValue(solver, 25)
    self.assertEqual(25, revx.Value())

  def testMemberApi(self):
    solver = pywrapcp.Solver("testMemberApi")
    x = solver.IntVar(0, 10, "x")
    c1 = solver.MemberCt(x, [2, 5])
    c2 = x.Member([2, 5])
    self.assertEqual(str(c1), str(c2))
    c3 = solver.NotMemberCt(x, [2, 7], [4, 9])
    c4 = x.NotMember([2, 7], [4, 9])
    self.assertEqual(str(c3), str(c4))

  def testRevIntegerInSearch(self):
    solver = pywrapcp.Solver("testRevIntegerInSearch")
    x = solver.IntVar(0, 10, "x")
    rev = pywrapcp.RevInteger(12)
    phase = MyDecisionBuilderWithRev(x, 5, rev)
    solver.NewSearch(phase)
    solver.NextSolution()
    self.assertTrue(x.Bound())
    self.assertEqual(5, rev.Value())
    solver.NextSolution()
    self.assertFalse(x.Bound())
    self.assertEqual(12, rev.Value())
    solver.EndSearch()

  def testDecisionBuilderThatFails(self):
    solver = pywrapcp.Solver("testRevIntegerInSearch")
    phase = MyDecisionBuilderThatFailsWithRev()
    self.assertFalse(solver.Solve(phase))

  # ----------------helper for binpacking posting----------------

  def bin_packing_helper(self, cp, binvars, weights, loadvars):
    nbins = len(loadvars)
    nitems = len(binvars)
    for j in range(nbins):
      b = [cp.BoolVar(str(i)) for i in range(nitems)]
      for i in range(nitems):
        cp.Add(cp.IsEqualCstCt(binvars[i], j, b[i]))
        cp.Add(
            cp.Sum([b[i] * weights[i] for i in range(nitems)]) == loadvars[j]
        )
    cp.Add(cp.Sum(loadvars) == sum(weights))

  def testNoNewSearch(self):
    maxcapa = 44
    weights = [4, 22, 9, 5, 8, 3, 3, 4, 7, 7, 3]
    loss = [
        0,
        11,
        10,
        9,
        8,
        7,
        6,
        5,
        4,
        3,
        2,
        1,
        0,
        1,
        0,
        2,
        1,
        0,
        0,
        0,
        0,
        2,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        2,
        1,
        0,
        3,
        2,
        1,
        0,
        2,
        1,
        0,
        0,
        0,
    ]
    nbslab = 11

    # ------------------solver and variable declaration-------------

    solver = pywrapcp.Solver("Steel Mill Slab")
    x = [
        solver.IntVar(list(range(nbslab)), "x" + str(i)) for i in range(nbslab)
    ]
    l = [
        solver.IntVar(list(range(maxcapa)), "l" + str(i)) for i in range(nbslab)
    ]
    obj = solver.IntVar(list(range(nbslab * maxcapa)), "obj")

    # -------------------post of the constraints--------------

    self.bin_packing_helper(solver, x, weights[:nbslab], l)
    solver.Add(solver.Sum([l[s].IndexOf(loss) for s in range(nbslab)]) == obj)

    unused_sol = [2, 0, 0, 0, 0, 1, 2, 2, 1, 1, 2]

    # ------------start the search and optimization-----------

    unused_objective = solver.Minimize(obj, 1)
    unused_db = solver.Phase(
        x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT
    )
    # solver.NewSearch(db,[objective]) #segfault if NewSearch is not called.

    while solver.NextSolution():
      print(obj, "check:", sum([loss[l[s].Min()] for s in range(nbslab)]))
      print(l)
    solver.EndSearch()


class SplitDomainDecisionBuilder(pywrapcp.PyDecisionBuilder):

  def __init__(self, var, value, lower):
    super().__init__()
    self.__var = var
    self.__value = value
    self.__lower = lower
    self.__done = pywrapcp.RevBool(False)

  def Next(self, solver):
    if self.__done.Value():
      return None
    self.__done.SetValue(solver, True)
    return solver.SplitVariableDomain(self.__var, self.__value, self.__lower)


class PyWrapCPDecisionTest(absltest.TestCase):

  def testSplitDomainLower(self):
    solver = pywrapcp.Solver("testSplitDomainLower")
    x = solver.IntVar(0, 10, "x")
    phase = SplitDomainDecisionBuilder(x, 3, True)
    solver.NewSearch(phase)
    self.assertTrue(solver.NextSolution())
    self.assertEqual(0, x.Min())
    self.assertEqual(3, x.Max())
    self.assertTrue(solver.NextSolution())
    self.assertEqual(4, x.Min())
    self.assertEqual(10, x.Max())
    self.assertFalse(solver.NextSolution())
    solver.EndSearch()

  def testSplitDomainUpper(self):
    solver = pywrapcp.Solver("testSplitDomainUpper")
    x = solver.IntVar(0, 10, "x")
    phase = SplitDomainDecisionBuilder(x, 6, False)
    solver.NewSearch(phase)
    self.assertTrue(solver.NextSolution())
    self.assertEqual(7, x.Min())
    self.assertEqual(10, x.Max())
    self.assertTrue(solver.NextSolution())
    self.assertEqual(0, x.Min())
    self.assertEqual(6, x.Max())
    self.assertFalse(solver.NextSolution())
    solver.EndSearch()

  def testTrueConstraint(self):
    solver = pywrapcp.Solver("test")
    x1 = solver.IntVar(4, 8, "x1")
    x2 = solver.IntVar(3, 7, "x2")
    x3 = solver.IntVar(1, 5, "x3")
    solver.Add((x1 >= 3) + (x2 >= 6) + (x3 <= 3) == 3)
    db = solver.Phase(
        [x1, x2, x3], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )
    solver.NewSearch(db)
    solver.NextSolution()
    solver.EndSearch()

  def testFalseConstraint(self):
    solver = pywrapcp.Solver("test")
    x1 = solver.IntVar(4, 8, "x1")
    x2 = solver.IntVar(3, 7, "x2")
    x3 = solver.IntVar(1, 5, "x3")
    solver.Add((x1 <= 3) + (x2 >= 6) + (x3 <= 3) == 3)
    db = solver.Phase(
        [x1, x2, x3], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE
    )
    solver.NewSearch(db)
    solver.NextSolution()
    solver.EndSearch()


class IntVarLocalSearchOperatorTest(absltest.TestCase):

  def test_ctor(self):
    solver = pywrapcp.Solver("Solve")
    int_vars = [solver.IntVar(0, 4) for _ in range(4)]
    ivlso = pywrapcp.IntVarLocalSearchOperator(int_vars)
    self.assertIsNotNone(ivlso)

  def test_api(self):
    # print(f"{dir(pywrapcp.IntVarLocalSearchOperator)}")
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Size"))

    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Var"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "AddVars"))
    self.assertTrue(
        hasattr(pywrapcp.IntVarLocalSearchOperator, "IsIncremental")
    )

    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Activate"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Deactivate"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Activated"))

    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "OldValue"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "PrevValue"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Value"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "SetValue"))

    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "Start"))
    self.assertTrue(hasattr(pywrapcp.IntVarLocalSearchOperator, "OnStart"))


if __name__ == "__main__":
  absltest.main()
