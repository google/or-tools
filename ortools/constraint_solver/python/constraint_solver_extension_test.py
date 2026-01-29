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
from ortools.constraint_solver.python import constraint_solver as cp


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
            hasattr(cp.AssignmentIntContainer, "contains"),
            dir(cp.AssignmentIntContainer),
        )

    def test_element(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntContainer, "element"),
            dir(cp.AssignmentIntContainer),
        )

    def test_size(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntContainer, "size"),
            dir(cp.AssignmentIntContainer),
        )

    def test_store(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntContainer, "store"),
            dir(cp.AssignmentIntContainer),
        )

    def test_restore(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntContainer, "restore"),
            dir(cp.AssignmentIntContainer),
        )


class TestIntervalVarContainerAPI(absltest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntervalContainer, "contains"),
            dir(cp.AssignmentIntervalContainer),
        )

    def test_element(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntervalContainer, "element"),
            dir(cp.AssignmentIntervalContainer),
        )

    def test_size(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntervalContainer, "size"),
            dir(cp.AssignmentIntervalContainer),
        )

    def test_store(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntervalContainer, "store"),
            dir(cp.AssignmentIntervalContainer),
        )

    def test_restore(self):
        self.assertTrue(
            hasattr(cp.AssignmentIntervalContainer, "restore"),
            dir(cp.AssignmentIntervalContainer),
        )


class TestSequenceVarContainerAPI(absltest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(cp.AssignmentSequenceContainer, "contains"),
            dir(cp.AssignmentSequenceContainer),
        )

    def test_element(self):
        self.assertTrue(
            hasattr(cp.AssignmentSequenceContainer, "element"),
            dir(cp.AssignmentSequenceContainer),
        )

    def test_size(self):
        self.assertTrue(
            hasattr(cp.AssignmentSequenceContainer, "size"),
            dir(cp.AssignmentSequenceContainer),
        )

    def test_store(self):
        self.assertTrue(
            hasattr(cp.AssignmentSequenceContainer, "store"),
            dir(cp.AssignmentSequenceContainer),
        )

    def test_restore(self):
        self.assertTrue(
            hasattr(cp.AssignmentSequenceContainer, "restore"),
            dir(cp.AssignmentSequenceContainer),
        )


class ConstraintSolverPropagationTest(absltest.TestCase):

    def test_rabbit_pheasant(self):
        # Create the solver.
        solver = cp.Solver("test_rabbit_pheasant")

        # Create the variables.
        pheasant = solver.new_int_var(0, 100, "pheasant")
        rabbit = solver.new_int_var(0, 100, "rabbit")

        # Create the constraints.
        solver.add(pheasant + rabbit == 20)
        solver.add(pheasant * 2 + rabbit * 4 == 56)

        # Create the search phase.
        db = solver.phase(
            [rabbit, pheasant],
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )

        # And solve.
        solver.new_search(db)
        solver.next_solution()

        # Check values directly
        self.assertEqual(12, pheasant.value())
        self.assertEqual(8, rabbit.value())

        solver.end_search()

    def test_solver_parameters(self):
        # Create the parameters.
        params = cp.Solver.default_solver_parameters()
        self.assertIsInstance(params, solver_parameters_pb2.ConstraintSolverParameters)
        self.assertFalse(params.trace_propagation)
        params.trace_propagation = True
        self.assertTrue(params.trace_propagation)

        # Create the solver.
        solver = cp.Solver("test_solver_parameters", params)
        inside_params = solver.parameters
        self.assertTrue(inside_params.trace_propagation)

    def test_solver_parameters_fields(self):
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

    def test_int_var_api(self):
        # Create the solver.
        solver = cp.Solver("test_int_var_api")

        c = solver.new_int_var(3, 3, "c")
        self.assertEqual(3, c.min())
        self.assertEqual(3, c.max())
        self.assertEqual(3, c.value())
        self.assertTrue(c.bound())

        b = solver.new_int_var(0, 1, "b")
        self.assertEqual(0, b.min())
        self.assertEqual(1, b.max())

        v1 = solver.new_int_var(3, 10, "v1")
        self.assertEqual(3, v1.min())
        self.assertEqual(10, v1.max())

        v2 = solver.new_int_var([1, 5, 3], "v2")
        self.assertEqual(1, v2.min())
        self.assertEqual(5, v2.max())
        self.assertEqual(3, v2.size())

    def test_domain_iterator(self):
        solver = cp.Solver("test_domain_iterator")
        x = solver.new_int_var([1, 2, 4, 6], "x")
        self.assertListEqual([1, 2, 4, 6], list(x.domain_iterator()))

    def test_remove_values(self):
        print("test_remove_values")
        solver = cp.Solver("test_remove_values")
        x = solver.new_int_var(0, 10, "x")
        x.remove_values([1, 3, 5])
        self.assertFalse(x.contains(1))
        self.assertFalse(x.contains(3))
        self.assertFalse(x.contains(5))
        self.assertTrue(x.contains(0))
        self.assertTrue(x.contains(2))
        self.assertEqual(x.size(), 8)

    # pylint: disable=too-many-statements
    def test_integer_arithmetic(self):
        solver = cp.Solver("test_integer_arithmetic")

        v1 = solver.new_int_var(0, 10, "v1")
        v2 = solver.new_int_var(0, 10, "v2")
        v3 = solver.new_int_var(0, 10, "v3")

        e1 = v1 + v2
        e2 = v1 + 2
        e3 = solver.sum([v1, v2, (v3 * 3).var()])

        e4 = v1 - 3
        e5 = v1 - v2
        e6 = -v1

        e7 = abs(e6)
        e8 = v3.square()

        e9 = v1 * 3
        e10 = v1 * v2

        e11 = v2.index_of([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])
        e11b = v2.index_of([0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])
        e12 = solver.min(e1, e2)
        e13 = solver.min(e3, 3)
        e14 = solver.min([(e1 + 1).var(), (e2 + 2).var(), (e3 + 3).var()])

        e15 = solver.max(e1, e2)
        e16 = solver.max(e3, 3)
        e17 = solver.max([(e1 + 1).var(), (e2 + 2).var(), (e3 + 3).var()])

        solver.add(v1 == 1)
        solver.add(v2 == 2)
        solver.add(v3 == 3)

        db = solver.phase(
            [v1, v2, v3],
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )

        solver.new_search(db)
        self.assertTrue(solver.next_solution())

        self.assertEqual(1, v1.value())
        self.assertEqual(2, v2.value())
        self.assertEqual(3, v3.value())

        self.assertEqual(3, e1.min())
        self.assertEqual(3, e1.max())
        self.assertEqual(3, e2.min())
        self.assertEqual(3, e2.max())
        self.assertEqual(12, e3.min())
        self.assertEqual(12, e3.max())
        self.assertEqual(-2, e4.min())
        self.assertEqual(-2, e4.max())
        self.assertEqual(-1, e5.min())
        self.assertEqual(-1, e5.max())
        self.assertEqual(-1, e6.min())
        self.assertEqual(-1, e6.max())
        self.assertEqual(1, e7.min())
        self.assertEqual(1, e7.max())
        self.assertEqual(9, e8.min())
        self.assertEqual(9, e8.max())
        self.assertEqual(3, e9.min())
        self.assertEqual(3, e9.max())
        self.assertEqual(2, e10.min())
        self.assertEqual(2, e10.max())
        self.assertEqual(4, e11.min())
        self.assertEqual(4, e11.max())
        self.assertEqual(4, e11b.min())
        self.assertEqual(4, e11b.max())
        self.assertEqual(3, e12.min())
        self.assertEqual(3, e12.max())
        self.assertEqual(3, e13.min())
        self.assertEqual(3, e13.max())
        self.assertEqual(4, e14.min())
        self.assertEqual(4, e14.max())
        self.assertEqual(3, e15.min())
        self.assertEqual(3, e15.max())
        self.assertEqual(12, e16.min())
        self.assertEqual(12, e16.max())
        self.assertEqual(15, e17.min())
        self.assertEqual(15, e17.max())
        solver.end_search()

    def test_status_var(self):
        solver = cp.Solver("test_status_var")
        v1 = solver.new_int_var(0, 10, "v1")
        v2 = solver.new_int_var(0, 10, "v2")
        c1 = v1 == 3
        c2 = v1 != 2
        self.assertEqual("(v1(0..10) == 3)", str(c1))
        self.assertEqual("Watch<v1 == 3>(0 .. 1)", str(c1.var()))
        self.assertEqual("(v1(0..10) != 2)", str(c2))
        self.assertEqual("Not(Watch<v1 == 2>(0 .. 1))", str(c2.var()))
        e3 = v1 + c1
        self.assertEqual("(v1(0..10) + Watch<v1 == 3>(0 .. 1))", str(e3))
        e4 = c1 + c2 == 1
        self.assertEqual(
            "((Watch<v1 == 3>(0 .. 1) + Not(Watch<v1 == 2>(0 .. 1))) == 1)", str(e4)
        )
        e5 = solver.min(c1.var(), c2.var())
        self.assertEqual(
            "MinIntExpr(Watch<v1 == 3>(0 .. 1), Not(Watch<v1 == 2>(0 .. 1)))",
            str(e5),
        )
        e6 = solver.max([c1.var(), c2.var(), e3.var()])
        self.assertEqual("DomainIntVar(0..11)", str(e6))
        e7 = 1 + c2
        self.assertEqual("(2 - Watch<v1 == 2>(0 .. 1))", str(e7))
        e8 = solver.max(
            [(v1 > 3).var(), (v1 <= 2).var(), v2, (v2 <= 0).var(), (v2 > 5).var()]
        )
        self.assertEqual("DomainIntVar(0..10)", str(e8))
        e9 = solver.min(
            [
                (v1 == v2).var(),
                (v1 != v2).var(),
                (v1 < v2).var(),
                (v1 > v2).var(),
                (v1 <= v2).var(),
                (v1 >= v2).var(),
            ]
        )
        self.assertEqual("BooleanVar(0 .. 1)", str(e9))

    def test_allowed_assignment(self):
        solver = cp.Solver("test_allowed_assignment")

        v1 = solver.new_int_var(0, 10, "v1")
        v2 = solver.new_int_var(0, 10, "v2")
        v3 = solver.new_int_var(0, 10, "v3")

        tuples = [(0, 0, 0), (1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4)]
        dvars = [v1, v2, v3]
        solver.add_allowed_assignments(dvars, tuples)
        db = solver.phase(
            dvars,
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )

        solver.new_search(db)
        counter = 0
        while solver.next_solution():
            self.assertEqual(counter, v1.value())
            self.assertEqual(counter, v2.value())
            self.assertEqual(counter, v3.value())
            counter += 1
        solver.end_search()
        self.assertEqual(5, counter)

    def test_allowed_assignment2(self):
        solver = cp.Solver("test_allowed_assignment2")

        v1 = solver.new_int_var(0, 10, "v1")
        v2 = solver.new_int_var(0, 10, "v2")
        v3 = solver.new_int_var(0, 10, "v3")

        dvars = [v1, v2, v3]
        solver.add_allowed_assignments(dvars, [(x, x, x) for x in range(5)])
        db = solver.phase(
            dvars,
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )

        solver.new_search(db)
        counter = 0
        while solver.next_solution():
            self.assertEqual(counter, v1.value())
            self.assertEqual(counter, v2.value())
            self.assertEqual(counter, v3.value())
            counter += 1
        solver.end_search()
        self.assertEqual(5, counter)

    def test_int_expr_to_int_var_cast(self):
        solver = cp.Solver("test_int_expr_to_int_var_cast")

        var1 = solver.new_int_var(0, 10, "var1")
        var2 = solver.new_int_var(0, 10, "var2")
        values = [1, 3, 5, 7, 9, 2, 4, 6, 8, 0]
        # This test fails if the cast is not correctly done.
        expr = (var1 + var2).index_of(values)
        self.assertTrue(expr)

    def test_int_expr_to_int_var_cast_in_solution(self):
        solver = cp.Solver("test_int_expr_to_int_var_cast_in_solution")

        var1 = solver.new_int_var(0, 10, "var1")
        var2 = solver.new_int_var(0, 10, "var2")
        solution = solver.assignment()
        expr = var1 + var2
        solution.add(expr.var())
        solution.store()
        # The next line fails if the cast is not correctly done.
        self.assertEqual(20, solution.max(expr.var()))

    def test_index_of(self):
        solver = cp.Solver("test_index_of")
        index = solver.new_int_var(0, 2, "index")
        element = index.index_of([1, 2, 3])
        self.assertEqual(1, element.min())
        self.assertEqual(3, element.max())

    def test_element_function(self):
        solver = cp.Solver("test_element_function")
        index = solver.new_int_var(0, 2, "index")
        element = solver.element_function(inc_callback, index)
        self.assertEqual(1, element.min())
        self.assertEqual(3, element.max())

    def test_element_method(self):
        solver = cp.Solver("test_element_method")
        index = solver.new_int_var(0, 2, "index")
        class_callback = ClassIncCallback(2)
        class_method = class_callback.inc_method
        self.assertEqual(5, class_method(3))
        element = solver.element_function(class_method, index)
        self.assertEqual(2, element.min())
        self.assertEqual(4, element.max())

    # TODO(user): better test all other ForwardSequence methods.
    def test_forward_sequence(self):
        solver = cp.Solver("test_forward_sequence")
        intervals = [
            solver.new_fixed_duration_interval_var(0, 10, 5, False, "Youpi")
            for _ in range(10)
        ]
        disjunction = solver.add_disjunctive_constraint(intervals, "Blup")
        sequence = disjunction.make_sequence_var()
        assignment = solver.assignment()
        assignment.add(sequence)
        assignment.set_forward_sequence(sequence, [1, 3, 5])
        self.assertListEqual(assignment.forward_sequence(sequence), [1, 3, 5])

    def test_modulo(self):
        solver = cp.Solver("test_modulo")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(2, 4, "y")
        self.assertEqual("CastVar<0>(-9..10)", str(x % 3))
        self.assertEqual("CastVar<1>(-20..10)", str(x % y))

    def test_modulo2(self):
        solver = cp.Solver("test_modulo2")
        x = solver.new_int_var([-7, 7], "x")
        y = solver.new_int_var([-4, 4], "y")
        z = (x % y).var()
        t = (x // y).var()
        db = solver.phase(
            [x, y],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        solutions = []
        while solver.next_solution():
            solutions.append(
                f"x = {x.value()}, y = {y.value()}, x % y = {z.value()}, x div y ="
                f" {t.value()}"
            )
        self.assertListEqual(
            [
                "x = -7, y = -4, x % y = -3, x div y = 1",
                "x = -7, y = 4, x % y = -3, x div y = -1",
                "x = 7, y = -4, x % y = 3, x div y = -1",
                "x = 7, y = 4, x % y = 3, x div y = 1",
            ],
            solutions,
        )
        solver.end_search()

    def test_size_1_var(self):
        solver = cp.Solver("test_size_1_var")
        x = solver.new_int_var([0], "x")
        self.assertTrue(x.contains(0))
        self.assertFalse(x.contains(1))

    def test_cumulative_api(self):
        solver = cp.Solver("test_cumulative_api")

        # Vars
        intervals = [
            solver.new_fixed_duration_interval_var(0, 10, 5, False, f"S_{a}")
            for a in range(10)
        ]
        demands = [a % 3 + 2 for a in range(10)]
        capacity = solver.new_int_var(2, 5)
        solver.add_cumulative(intervals, demands, capacity, "cumul")

    def test_search_alldiff(self):
        solver = cp.Solver("test_search_alldiff")
        in_pos = [solver.new_int_var(0, 7, f"{i}") for i in range(8)]
        solver.add_all_different(in_pos)
        aux_phase = solver.phase(
            in_pos,
            cp.IntVarStrategy.CHOOSE_LOWEST_MIN,
            cp.IntValueStrategy.ASSIGN_MAX_VALUE,
        )
        collector = solver.first_solution_collector()
        for i in range(8):
            collector.add(in_pos[i])
        solver.solve(aux_phase, [collector])
        expected = [7, 6, 5, 4, 3, 2, 1, 0]
        for i in range(8):
            self.assertEqual(expected[i], collector.value(0, in_pos[i]))

    def test_true_constraint(self):
        solver = cp.Solver("test_true_constraint")
        x1 = solver.new_int_var(4, 8, "x1")
        x2 = solver.new_int_var(3, 7, "x2")
        x3 = solver.new_int_var(1, 5, "x3")
        solver.add((x1 >= 3) + (x2 >= 6) + (x3 <= 3) == 3)
        db = solver.phase(
            [x1, x2, x3],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        solver.next_solution()
        solver.end_search()

    def test_false_constraint(self):
        solver = cp.Solver("test_false_constraint")
        x1 = solver.new_int_var(4, 8, "x1")
        x2 = solver.new_int_var(3, 7, "x2")
        x3 = solver.new_int_var(1, 5, "x3")
        solver.add((x1 <= 3) + (x2 >= 6) + (x3 <= 3) == 3)
        db = solver.phase(
            [x1, x2, x3],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        solver.next_solution()
        solver.end_search()


class CustomSearchMonitor(cp.SearchMonitor):

    def __init__(self, solver, nexts, output_list):
        super().__init__(solver)
        self._nexts = nexts
        self._output_list = output_list

    def begin_initial_propagation(self):
        self._output_list.append(str(self._nexts))

    def end_initial_propagation(self):
        self._output_list.append(str(self._nexts))


class SearchMonitorTest(absltest.TestCase):

    def test_limit(self):
        solver = cp.Solver("test_limit")
        # TODO(user): expose the proto-based MakeLimit() API in or-tools and test it
        # here.
        time = 10000  # ms
        branches = 10
        failures = sys.maxsize
        solutions = sys.maxsize
        smart_time_check = True
        cumulative = False
        limit = solver.limit(
            time, branches, failures, solutions, smart_time_check, cumulative
        )
        self.assertEqual(
            "RegularLimit(crossed = 0, duration_limit = 10s, branches = 10, "
            "failures = 9223372036854775807, solutions = 9223372036854775807 "
            "cumulative = false",
            str(limit),
        )

    def test_limit_proto(self):
        solver = cp.Solver("test_limit_proto")
        limit_proto = search_limit_pb2.RegularLimitParameters(time=10000, branches=10)
        self.assertEqual("time: 10000\nbranches: 10\n", str(limit_proto))
        limit = solver.limit(limit_proto)
        self.assertEqual(
            "RegularLimit(crossed = 0, duration_limit = 10s, branches = 10, "
            "failures = 0, solutions = 0 cumulative = false",
            str(limit),
        )

    def test_search_monitor(self):
        solver = cp.Solver("test_search_monitor")
        x = solver.new_int_var(1, 10, "x")
        ct = x == 3
        solver.add(ct)
        db = solver.phase(
            [x],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        output = []
        monitor = CustomSearchMonitor(solver, x, output)
        solver.solve(db, monitor)
        self.assertListEqual(["x(1..10)", "x(3)"], output)


class CustomDecisionBuilder(cp.DecisionBuilder):

    def __init__(self):
        super().__init__()
        self._counter = 0

    def next(self, _):
        self._counter += 1
        return None

    def debug_string(self):
        return "CustomDecisionBuilder"


class CustomDecision(cp.Decision):

    def __init__(self):
        super().__init__()
        self._val = 1

    def apply(self, solver):
        solver.fail()

    def refute(self, _):
        pass

    def debug_string(self):
        return "CustomDecision"


class CustomDecisionBuilderCustomDecision(cp.DecisionBuilder):

    def __init__(self):
        super().__init__()
        self.__done = False
        self._counter = 0

    def next(self, _):
        self._counter += 1
        if not self.__done:
            self.__done = True
            self.__decision = CustomDecision()
            return self.__decision
        return None

    def debug_string(self):
        return "CustomDecisionBuilderCustomDecision"


class DecisionTest(absltest.TestCase):

    def test_custom_decision_builder(self):
        solver = cp.Solver("test_custom_decision_builder")
        db = CustomDecisionBuilder()
        self.assertEqual("CustomDecisionBuilder", str(db))
        solver.solve(db)
        self.assertEqual(db._counter, 1)

    def test_custom_decision(self):
        solver = cp.Solver("test_custom_decision")
        db = CustomDecisionBuilderCustomDecision()
        self.assertEqual("CustomDecisionBuilderCustomDecision", str(db))
        solver.solve(db)
        self.assertEqual(db._counter, 2)


class MyDecisionBuilder(cp.DecisionBuilder):

    def __init__(self, var, value):
        super().__init__()
        self.__var = var
        self.__value = value

    def next(self, solver):
        if not self.__var.bound():
            decision = solver.assign_variable_value(self.__var, self.__value)
            return decision


class MyDecisionBuilderWithRev(cp.DecisionBuilder):

    def __init__(self, var, value, rev):
        super().__init__()
        self.__var = var
        self.__value = value
        self.__rev = rev

    def next(self, solver):
        if not self.__var.bound():
            if self.__var.contains(self.__value):
                decision = solver.assign_variable_value(self.__var, self.__value)
                self.__rev.set_value(solver, self.__value)
                return decision
            else:
                return solver.fail_decision()


class MyDecisionBuilderThatFailsWithRev(cp.DecisionBuilder):

    def next(self, solver):
        solver.fail()
        return None


class PyWrapCPSearchTest(absltest.TestCase):
    NUMBER_OF_VARIABLES = 10
    VARIABLE_MIN = 0
    VARIABLE_MAX = 10
    LNS_NEIGHBORS = 100
    LNS_VARIABLES = 4
    DECISION_BUILDER_VALUE = 5
    OTHER_DECISION_BUILDER_VALUE = 2

    def test_new_class_as_decision_builder(self):
        solver = cp.Solver("test_new_class_as_decision_builder")
        x = solver.new_int_var(self.VARIABLE_MIN, self.VARIABLE_MAX, "x")
        phase = MyDecisionBuilder(x, self.DECISION_BUILDER_VALUE)
        solver.new_search(phase)
        solver.next_solution()
        self.assertTrue(x.bound())
        self.assertEqual(self.DECISION_BUILDER_VALUE, x.min())
        solver.end_search()

    def test_compose_two_decisions(self):
        solver = cp.Solver("test_compose_two_decisions")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")
        phase_x = MyDecisionBuilder(x, self.DECISION_BUILDER_VALUE)
        phase_y = MyDecisionBuilder(y, self.OTHER_DECISION_BUILDER_VALUE)
        phase = solver.compose([phase_x, phase_y])
        solver.new_search(phase)
        solver.next_solution()
        self.assertTrue(x.bound())
        self.assertEqual(self.DECISION_BUILDER_VALUE, x.min())
        self.assertTrue(y.bound())
        self.assertEqual(self.OTHER_DECISION_BUILDER_VALUE, y.min())
        solver.end_search()

    def test_rev_integer_outside_search(self):
        solver = cp.Solver("test_rev_integer_outside_search")
        revx = cp.RevInteger(12)
        self.assertEqual(12, revx.value())
        revx.set_value(solver, 25)
        self.assertEqual(25, revx.value())

    def test_rev_integer_in_search(self):
        solver = cp.Solver("test_rev_integer_in_search")
        x = solver.new_int_var(0, 10, "x")
        rev = cp.RevInteger(12)
        phase = MyDecisionBuilderWithRev(x, 5, rev)
        solver.new_search(phase)
        solver.next_solution()
        self.assertTrue(x.bound())
        self.assertEqual(5, rev.value())
        solver.next_solution()
        self.assertFalse(x.bound())
        self.assertEqual(12, rev.value())
        solver.end_search()

    def test_decision_builder_that_fails(self):
        solver = cp.Solver("test_decision_builder_that_fails")
        phase = MyDecisionBuilderThatFailsWithRev()
        self.assertFalse(solver.solve(phase))

    # ----------------helper for binpacking posting----------------

    def bin_packing_helper(self, solver, binvars, weights, loadvars):
        nbins = len(loadvars)
        nitems = len(binvars)
        for j in range(nbins):
            b = [solver.new_bool_var(str(i)) for i in range(nitems)]
            for i in range(nitems):
                solver.add_is_equal_cst_ct(binvars[i], j, b[i])
                solver.add(
                    solver.sum([b[i] * weights[i] for i in range(nitems)])
                    == loadvars[j]
                )
        solver.add(solver.sum(loadvars) == sum(weights))

    def test_no_new_search(self):
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

        solver = cp.Solver("test_no_new_search")
        x = [
            solver.new_int_var(list(range(nbslab)), "x" + str(i)) for i in range(nbslab)
        ]
        l = [
            solver.new_int_var(list(range(maxcapa)), "l" + str(i))
            for i in range(nbslab)
        ]
        obj = solver.new_int_var(list(range(nbslab * maxcapa)), "obj")

        # -------------------post of the constraints--------------

        self.bin_packing_helper(solver, x, weights[:nbslab], l)
        solver.add(solver.sum([l[s].index_of(loss) for s in range(nbslab)]) == obj)

        unused_sol = [2, 0, 0, 0, 0, 1, 2, 2, 1, 1, 2]

        # ------------start the search and optimization-----------

        unused_objective = solver.minimize(obj, 1)
        unused_db = solver.phase(
            x,
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.INT_VALUE_DEFAULT,
        )
        # solver.NewSearch(db,[objective]) #segfault if NewSearch is not called.

        while solver.next_solution():
            pass
        solver.end_search()


class SplitDomainDecisionBuilder(cp.DecisionBuilder):

    def __init__(self, var, value, lower):
        super().__init__()
        self.__var = var
        self.__value = value
        self.__lower = lower
        self.__done = cp.RevBool(False)

    def next(self, solver):
        if self.__done.value():
            return None
        self.__done.set_value(solver, True)
        return solver.split_variable_domain(self.__var, self.__value, self.__lower)


class PyWrapCPDecisionTest(absltest.TestCase):

    def test_split_domain_lower(self):
        solver = cp.Solver("test_split_domain_lower")
        x = solver.new_int_var(0, 10, "x")
        phase = SplitDomainDecisionBuilder(x, 3, True)
        solver.new_search(phase)
        self.assertTrue(solver.next_solution())
        self.assertEqual(0, x.min())
        self.assertEqual(3, x.max())
        self.assertTrue(solver.next_solution())
        self.assertEqual(4, x.min())
        self.assertEqual(10, x.max())
        self.assertFalse(solver.next_solution())
        solver.end_search()

    def test_split_domain_upper(self):
        solver = cp.Solver("test_split_domain_upper")
        x = solver.new_int_var(0, 10, "x")
        phase = SplitDomainDecisionBuilder(x, 6, False)
        solver.new_search(phase)
        self.assertTrue(solver.next_solution())
        self.assertEqual(7, x.min())
        self.assertEqual(10, x.max())
        self.assertTrue(solver.next_solution())
        self.assertEqual(0, x.min())
        self.assertEqual(6, x.max())
        self.assertFalse(solver.next_solution())
        solver.end_search()

    def test_true_constraint(self):
        solver = cp.Solver("test_true_constraint")
        x1 = solver.new_int_var(4, 8, "x1")
        x2 = solver.new_int_var(3, 7, "x2")
        x3 = solver.new_int_var(1, 5, "x3")
        solver.add((x1 >= 3) + (x2 >= 6) + (x3 <= 3) == 3)
        db = solver.phase(
            [x1, x2, x3],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        solver.next_solution()
        solver.end_search()

    def test_false_constraint(self):
        solver = cp.Solver("test_false_constraint")
        x1 = solver.new_int_var(4, 8, "x1")
        x2 = solver.new_int_var(3, 7, "x2")
        x3 = solver.new_int_var(1, 5, "x3")
        solver.add((x1 <= 3) + (x2 >= 6) + (x3 <= 3) == 3)
        db = solver.phase(
            [x1, x2, x3],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        self.assertFalse(solver.next_solution())
        solver.end_search()


if __name__ == "__main__":
    absltest.main()
