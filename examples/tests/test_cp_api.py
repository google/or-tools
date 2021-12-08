#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
""" Various calls to CP api from python to verify they work."""

import unittest
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import search_limit_pb2


class TestIntVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Contains'),
            dir(pywrapcp.IntVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Element'),
            dir(pywrapcp.IntVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Size'), dir(pywrapcp.IntVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Store'), dir(pywrapcp.IntVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Restore'),
            dir(pywrapcp.IntVarContainer))


class TestIntervalVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Contains'),
            dir(pywrapcp.IntervalVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Element'),
            dir(pywrapcp.IntervalVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Size'),
            dir(pywrapcp.IntervalVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Store'),
            dir(pywrapcp.IntervalVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Restore'),
            dir(pywrapcp.IntervalVarContainer))


class TestSequenceVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Contains'),
            dir(pywrapcp.SequenceVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Element'),
            dir(pywrapcp.SequenceVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Size'),
            dir(pywrapcp.SequenceVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Store'),
            dir(pywrapcp.SequenceVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Restore'),
            dir(pywrapcp.SequenceVarContainer))

class CPSolverTest(unittest.TestCase):

    def test_member(self):
        solver = pywrapcp.Solver('test member')
        x = solver.IntVar(1, 10, 'x')
        ct = x.Member([1, 2, 3, 5])
        print('Constraint: {}'.format(ct))

    def test_sparse_var(self):
        solver = pywrapcp.Solver('test sparse')
        x = solver.IntVar([1, 3, 5], 'x')
        self.assertTrue(x.Contains(1))
        self.assertFalse(x.Contains(2))
        #print(x)

    def test_modulo(self):
        solver = pywrapcp.Solver('test modulo')
        x = solver.IntVar(0, 10, 'x')
        y = solver.IntVar(2, 4, 'y')
        print(x % 3)
        print(x % y)

    def test_modulo2(self):
        solver = pywrapcp.Solver('test modulo')
        x = solver.IntVar([-7, 7], 'x')
        y = solver.IntVar([-4, 4], 'y')
        z = (x % y).Var()
        t = (x // y).Var()
        db = solver.Phase([x, y], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.NewSearch(db)
        while solver.NextSolution():
            print('x = %d, y = %d, x %% y = %d, x div y = %d' % (x.Value(),
                                                                 y.Value(),
                                                                 z.Value(),
                                                                 t.Value()))
        solver.EndSearch()

    def test_limit(self):
        solver = pywrapcp.Solver('test limit')
        #limit_proto = solver.DefaultSearchLimitParameters()
        limit_proto = search_limit_pb2.RegularLimitParameters()
        limit_proto.time = 10000
        limit_proto.branches = 10
        print('limit proto: {}'.format(limit_proto))
        limit = solver.Limit(limit_proto)
        print('limit: {}'.format(limit))

    def test_export(self):
        solver = pywrapcp.Solver('test export')
        x = solver.IntVar(1, 10, 'x')
        ct = x.Member([1, 2, 3, 5])
        solver.Add(ct)
        #proto = model_pb2.CpModel()
        #proto.model = 'wrong name'
        #solver.ExportModel(proto)
        #print(repr(proto))
        #print(str(proto))

    def test_size_1_var(self):
        solver = pywrapcp.Solver('test_size_1_var')
        x = solver.IntVar([0], 'x')


    def test_cumulative_api(self):
        solver = pywrapcp.Solver('Problem')

        #Vars
        S = [
            solver.FixedDurationIntervalVar(0, 10, 5, False, "S_%s" % a)
            for a in range(10)
        ]
        C = solver.IntVar(2, 5)
        D = [a % 3 + 2 for a in range(10)]
        solver.Add(solver.Cumulative(S, D, C, "cumul"))

    def test_search_alldiff(self):
        solver = pywrapcp.Solver('test_search_alldiff')
        in_pos = [solver.IntVar(0, 7, "%i" % i) for i in range(8)]
        solver.Add(solver.AllDifferent(in_pos))
        aux_phase = solver.Phase(in_pos, solver.CHOOSE_LOWEST_MIN,
                                 solver.ASSIGN_MAX_VALUE)
        collector = solver.FirstSolutionCollector()
        for i in range(8):
            collector.Add(in_pos[i])
        solver.Solve(aux_phase, [collector])
        for i in range(8):
            print(collector.Value(0, in_pos[i]))


class CustomSearchMonitor(pywrapcp.SearchMonitor):
    def __init__(self, solver, nexts):
        print('Build')
        pywrapcp.SearchMonitor.__init__(self, solver)
        self._nexts = nexts

    def BeginInitialPropagation(self):
        print('In BeginInitialPropagation')
        print(self._nexts)

    def EndInitialPropagation(self):
        print('In EndInitialPropagation')
        print(self._nexts)


class SearchMonitorTest(unittest.TestCase):
    def test_search_monitor(self):
        print('test_search_monitor')
        solver = pywrapcp.Solver('test search monitor')
        x = solver.IntVar(1, 10, 'x')
        ct = (x == 3)
        solver.Add(ct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        monitor = CustomSearchMonitor(solver, x)
        solver.Solve(db, monitor)


class CustomDemon(pywrapcp.PyDemon):
    def __init__(self, x):
        pywrapcp.Demon.__init__(self)
        self._x = x
        print('Demon built')

    def Run(self, solver):
        print('in Run(), saw ' + str(self._x))


class DemonTest(unittest.TestCase):
    def test_demon(self):
        print('test_demon')
        solver = pywrapcp.Solver('test export')
        x = solver.IntVar(1, 10, 'x')
        demon = CustomDemon(x)
        demon.Run(solver)


class CustomConstraint(pywrapcp.PyConstraint):
    def __init__(self, solver, x):
        pywrapcp.Constraint.__init__(self, solver)
        self._x = x
        print('Constraint built')

    def Post(self):
        print('in Post()')
        self._demon = CustomDemon(self._x)
        self._x.WhenBound(self._demon)
        print('out of Post()')

    def InitialPropagate(self):
        print('in InitialPropagate()')
        self._x.SetMin(5)
        print(self._x)
        print('out of InitialPropagate()')

    def DebugString(self):
        return ('CustomConstraint')


class InitialPropagateDemon(pywrapcp.PyDemon):
    def __init__(self, ct):
        pywrapcp.Demon.__init__(self)
        self._ct = ct

    def Run(self, solver):
        self._ct.InitialPropagate()


class DumbGreaterOrEqualToFive(pywrapcp.PyConstraint):
    def __init__(self, solver, x):
        pywrapcp.Constraint.__init__(self, solver)
        self._x = x

    def Post(self):
        self._demon = InitialPropagateDemon(self)
        self._x.WhenBound(self._demon)

    def InitialPropagate(self):
        if self._x.Bound():
            if self._x.Value() < 5:
                print('Reject %d' % self._x.Value())
                self.solver().Fail()
            else:
                print('Accept %d' % self._x.Value())


class WatchDomain(pywrapcp.PyDemon):
    def __init__(self, x):
        pywrapcp.Demon.__init__(self)
        self._x = x

    def Run(self, solver):
        for i in self._x.HoleIterator():
            print('Removed %d' % i)


class HoleConstraint(pywrapcp.PyConstraint):
    def __init__(self, solver, x):
        pywrapcp.Constraint.__init__(self, solver)
        self._x = x

    def Post(self):
        self._demon = WatchDomain(self._x)
        self._x.WhenDomain(self._demon)

    def InitialPropagate(self):
        self._x.RemoveValue(5)


class BinarySum(pywrapcp.PyConstraint):
    def __init__(self, solver, x, y, z):
        pywrapcp.Constraint.__init__(self, solver)
        self._x = x
        self._y = y
        self._z = z

    def Post(self):
        self._demon = InitialPropagateDemon(self)
        self._x.WhenRange(self._demon)
        self._y.WhenRange(self._demon)
        self._z.WhenRange(self._demon)

    def InitialPropagate(self):
        self._z.SetRange(self._x.Min() + self._y.Min(),
                         self._x.Max() + self._y.Max())
        self._x.SetRange(self._z.Min() - self._y.Max(),
                         self._z.Max() - self._y.Min())
        self._y.SetRange(self._z.Min() - self._x.Max(),
                         self._z.Max() - self._x.Min())

class ConstraintTest(unittest.TestCase):
    def test_constraint(self):
        solver = pywrapcp.Solver('test export')
        x = solver.IntVar(1, 10, 'x')
        myct = CustomConstraint(solver, x)
        solver.Add(myct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.Solve(db)

    def test_failing_constraint(self):
        solver = pywrapcp.Solver('test export')
        x = solver.IntVar(1, 10, 'x')
        myct = DumbGreaterOrEqualToFive(solver, x)
        solver.Add(myct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.Solve(db)


    def test_domain_iterator(self):
        print('test_domain_iterator')
        solver = pywrapcp.Solver('test_domain_iterator')
        x = solver.IntVar([1, 2, 4, 6], 'x')
        for i in x.DomainIterator():
            print(i)

    def test_hole_iterator(self):
        print('test_hole_iterator')
        solver = pywrapcp.Solver('test export')
        x = solver.IntVar(1, 10, 'x')
        myct = HoleConstraint(solver, x)
        solver.Add(myct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.Solve(db)

    def test_sum_constraint(self):
        print('test_sum_constraint')
        solver = pywrapcp.Solver('test_sum_constraint')
        x = solver.IntVar(1, 5, 'x')
        y = solver.IntVar(1, 5, 'y')
        z = solver.IntVar(1, 5, 'z')
        binary_sum = BinarySum(solver, x, y, z)
        solver.Add(binary_sum)
        db = solver.Phase([x, y, z], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.NewSearch(db)
        while solver.NextSolution():
            print('%d + %d == %d' % (x.Value(), y.Value(), z.Value()))
        solver.EndSearch()


class CustomDecisionBuilder(pywrapcp.PyDecisionBuilder):
    def __init__(self):
        pywrapcp.PyDecisionBuilder.__init__(self)

    def Next(self, solver):
        print("In Next")
        return None

    def DebugString(self):
        return 'CustomDecisionBuilder'



class CustomDecision(pywrapcp.PyDecision):
    def __init__(self):
        pywrapcp.PyDecision.__init__(self)
        self._val = 1
        print("Set value to", self._val)

    def Apply(self, solver):
        print('In Apply')
        print("Expect value", self._val)
        solver.Fail()

    def Refute(self, solver):
        print('In Refute')

    def DebugString(self):
        return ('CustomDecision')


class CustomDecisionBuilderCustomDecision(pywrapcp.PyDecisionBuilder):
    def __init__(self):
        pywrapcp.PyDecisionBuilder.__init__(self)
        self.__done = False

    def Next(self, solver):
        if not self.__done:
            self.__done = True
            self.__decision = CustomDecision()
            return self.__decision
        return None

    def DebugString(self):
        return 'CustomDecisionBuilderCustomDecision'


class DecisionTest(unittest.TestCase):
    def test_custom_decision_builder(self):
        solver = pywrapcp.Solver('test_custom_decision_builder')
        db = CustomDecisionBuilder()
        print(str(db))
        solver.Solve(db)

    def test_custom_decision(self):
        solver = pywrapcp.Solver('test_custom_decision')
        db = CustomDecisionBuilderCustomDecision()
        print(str(db))
        solver.Solve(db)


if __name__ == '__main__':
    unittest.main(verbosity=2)
