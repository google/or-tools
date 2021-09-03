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
"""Unit tests for python/constraint_solver.swig. Not exhaustive."""


import sys
import unittest
from ortools.constraint_solver import pywrapcp


class SearchMonitorTest(pywrapcp.SearchMonitor):

    def __init__(self, solver, nexts):
        pywrapcp.SearchMonitor.__init__(self, solver)
        self._nexts = nexts

    def BeginInitialPropagation(self):
        print(self._nexts)

    def EndInitialPropagation(self):
        print(self._nexts)


class DemonTest(pywrapcp.PyDemon):

    def __init__(self, x):
        pywrapcp.PyDemon.__init__(self)
        self._x = x
        print('Demon built')

    def Run(self, solver):
        print('in Run(), saw ' + str(self._x))


class ConstraintTest(pywrapcp.PyConstraint):

    def __init__(self, solver, x):
        pywrapcp.Constraint.__init__(self, solver)
        self._x = x
        print('Constraint built')

    def Post(self):
        print('in Post()')
        self._demon = DemonTest(self._x)
        self._x.WhenBound(self._demon)
        print('out of Post()')

    def InitialPropagate(self):
        print('in InitialPropagate()')
        self._x.SetMin(5)
        print(self._x)
        print('out of InitialPropagate()')


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


class HoleConstraintTest(pywrapcp.PyConstraint):

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


class PyWrapCp(unittest.TestCase):

    def test_member(self):
        print('test_member')
        solver = pywrapcp.Solver('test member')
        x = solver.IntVar(1, 10, 'x')
        ct = x.Member([1, 2, 3, 5])
        print(ct)

    def test_sparse_var(self):
        print('test_sparse_var')
        solver = pywrapcp.Solver('test_sparse_var')
        x = solver.IntVar([1, 3, 5], 'x')
        print(x)

    def test_modulo(self):
        print('test_modulo')
        solver = pywrapcp.Solver('test_modulo')
        x = solver.IntVar(0, 10, 'x')
        y = solver.IntVar(2, 4, 'y')
        print(x % 3)
        print(x % y)

    def test_limit(self):
        solver = pywrapcp.Solver('test_limit')
        # TODO(user): expose the proto-based MakeLimit() API in or-tools and test it
        # here.
        time = 10000  # ms
        branches = 10
        failures = sys.maxsize
        solutions = sys.maxsize
        smart_time_check = True
        cumulative = False
        limit = solver.Limit(time, branches, failures, solutions,
                             smart_time_check, cumulative)
        print(limit)

    def test_search_monitor(self):
        print('test_search_monitor')
        solver = pywrapcp.Solver('test search_monitor')
        x = solver.IntVar(1, 10, 'x')
        ct = (x == 3)
        solver.Add(ct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        monitor = SearchMonitorTest(solver, x)
        solver.Solve(db, monitor)

    def test_demon(self):
        print('test_demon')
        solver = pywrapcp.Solver('test_demon')
        x = solver.IntVar(1, 10, 'x')
        demon = DemonTest(x)
        demon.Run(solver)

    def test_constraint(self):
        print('test_constraint')
        solver = pywrapcp.Solver('test_constraint')
        x = solver.IntVar(1, 10, 'x')
        myct = ConstraintTest(solver, x)
        solver.Add(myct)
        db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND,
                          solver.ASSIGN_MIN_VALUE)
        solver.Solve(db)

    def test_failing_constraint(self):
        print('test_failing_constraint')
        solver = pywrapcp.Solver('test failing constraint')
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
        solver = pywrapcp.Solver('test_hole_iterator')
        x = solver.IntVar(1, 10, 'x')
        myct = HoleConstraintTest(solver, x)
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


if __name__ == '__main__':
    unittest.main()
