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

"""Tests for Local Search in Constraint Solver."""

from absl.testing import absltest

from ortools.constraint_solver.python import constraint_solver as cp


class BaseOperatorTest(absltest.TestCase):

    def setUp(self):
        super().setUp()
        self.first_solution_ = []
        self.expected_deltas_ = []
        self.expected_delta_deltas_ = []

    def make_vars(self, solver):
        size = len(self.first_solution_)
        max_val = max(self.first_solution_) if self.first_solution_ else 0
        vars_list = [solver.new_int_var(0, max_val, f"nodes_{i}") for i in range(size)]
        return vars_list

    def set_first_solution(self, first_solution):
        self.first_solution_ = first_solution

    def set_first_paths(self, num_nodes, paths):
        self.first_solution_ = list(range(num_nodes))
        for path in paths:
            for i in range(len(path) - 1):
                self.first_solution_[path[i]] = path[i + 1]

    def add_expected_delta(self, expected_delta, expected_delta_delta=None):
        self.expected_deltas_.append(expected_delta)
        if expected_delta_delta is None:
            self.expected_delta_deltas_.append([])
        else:
            self.expected_delta_deltas_.append(expected_delta_delta)

    def add_expected_paths(self, paths):
        expected_solution = list(range(len(self.first_solution_)))
        for path in paths:
            for i in range(len(path) - 1):
                expected_solution[path[i]] = path[i + 1]

        expected_delta = [-1] * len(self.first_solution_)
        for i in range(len(self.first_solution_)):
            if self.first_solution_[i] != expected_solution[i]:
                expected_delta[i] = expected_solution[i]
        self.add_expected_delta(expected_delta)

    def base_solve_and_test(self, match_all, operator_type):
        solver = cp.Solver("Test")
        vars_list = self.make_vars(solver)
        assignment = solver.assignment()
        assignment.add(vars_list)
        for i in range(len(self.first_solution_)):
            assignment.set_value(vars_list[i], self.first_solution_[i])

        # We use solver.operator(vars, type) for basic operators.
        # Note: neighbors and neighbor_option are currently not supported in
        # Python Solver.operator
        ls_operator = solver.operator(vars_list, operator_type)

        ls_operator.start(assignment)
        if match_all:
            self.ordered_match(vars_list, solver, ls_operator)
        else:
            self.unordered_match(vars_list, solver, ls_operator)

    def ordered_match(self, vars_list, solver, ls_operator):
        delta = solver.assignment()
        deltadelta = solver.assignment()
        for i in range(len(self.expected_deltas_)):
            expected_delta = self.expected_deltas_[i]
            expected_delta_delta = self.expected_delta_deltas_[i]

            if not expected_delta_delta:
                delta.clear()

            self.assertTrue(
                ls_operator.next_neighbor(delta, deltadelta),
                msg=f"Expected {len(self.expected_deltas_)} neighbors, found {i}",
            )

            if not expected_delta:
                self.assertTrue(delta.empty())
                self.assertTrue(deltadelta.empty())
            else:
                for j in range(len(expected_delta)):
                    if expected_delta[j] >= 0:
                        self.assertTrue(
                            delta.int_var_container().contains(vars_list[j]),
                            msg=f"Delta {i} var {j} missing",
                        )
                        self.assertEqual(
                            delta.value(vars_list[j]),
                            expected_delta[j],
                            msg=f"Delta {i} var {j} mismatch",
                        )
                    else:
                        self.assertFalse(
                            delta.int_var_container().contains(vars_list[j]),
                            msg=f"Delta {i} var {j} should be absent",
                        )

                for j in range(len(expected_delta_delta)):
                    if expected_delta_delta[j] >= 0:
                        self.assertTrue(
                            deltadelta.int_var_container().contains(vars_list[j]),
                            msg=f"DeltaDelta {i} var {j} missing",
                        )
                        self.assertEqual(
                            deltadelta.value(vars_list[j]),
                            expected_delta_delta[j],
                            msg=f"DeltaDelta {i} var {j} mismatch",
                        )
                    else:
                        self.assertFalse(
                            deltadelta.int_var_container().contains(vars_list[j]),
                            msg=f"DeltaDelta {i} var {j} should be absent",
                        )

            deltadelta.clear()

        self.assertFalse(ls_operator.next_neighbor(delta, deltadelta))

    def unordered_match(self, vars_list, solver, ls_operator):
        delta = solver.assignment()
        deltadelta = solver.assignment()
        found_count = 0
        while ls_operator.next_neighbor(delta, deltadelta):
            found = False
            for i in range(len(self.expected_deltas_)):
                expected_delta = self.expected_deltas_[i]
                match = True
                for j in range(len(expected_delta)):
                    if expected_delta[j] >= 0:
                        if (
                            not delta.int_var_container().contains(vars_list[j])
                            or delta.value(vars_list[j]) != expected_delta[j]
                        ):
                            match = False
                            break
                    else:
                        if delta.int_var_container().contains(vars_list[j]):
                            match = False
                            break
                if match:
                    found = True
                    break
            self.assertTrue(found, msg=f"Unexpected neighbor found: {delta}")
            found_count += 1
            delta.clear()
            deltadelta.clear()
        self.assertLen(self.expected_deltas_, found_count)


class OperatorTest(BaseOperatorTest):

    def solve_and_test(self, operator_type):
        self.base_solve_and_test(True, operator_type)

    def test_two_opt_test1(self):
        self.set_first_solution([1, 2, 3])
        self.add_expected_delta([2, 3, 1])
        self.solve_and_test(cp.TWOOPT)

    def test_two_opt_test2(self):
        self.set_first_solution([1, 2, 4, 5])
        self.add_expected_delta([2, 4, 1, -1])
        self.solve_and_test(cp.TWOOPT)

    def test_two_opt_test3(self):
        self.set_first_solution([4, 2, 3, 5])
        self.add_expected_delta([-1, 3, 5, 2])
        self.solve_and_test(cp.TWOOPT)

    def test_or_opt_test1(self):
        self.set_first_solution([1, 2, 3])
        self.add_expected_delta([2, 3, 1])
        self.add_expected_delta([2, 3, 1])
        self.solve_and_test(cp.OROPT)

    def test_relocate1(self):
        self.set_first_solution([1, 2, 3])
        self.add_expected_delta([2, 3, 1])
        self.add_expected_delta([2, 3, 1])
        self.solve_and_test(cp.RELOCATE)

    def test_relocate2(self):
        self.set_first_solution([1, 3, 4])
        self.add_expected_delta([3, 4, 1])
        self.solve_and_test(cp.RELOCATE)

    def test_relocate3(self):
        self.set_first_solution([3, 2, 4])
        self.add_expected_delta([2, 4, 3])
        self.solve_and_test(cp.RELOCATE)

    def test_exchange1(self):
        self.set_first_solution([1, 2, 3])
        self.add_expected_delta([2, 3, 1])
        self.add_expected_delta([2, 3, 1])
        self.solve_and_test(cp.EXCHANGE)

    def test_exchange2(self):
        self.set_first_solution([1, 4, 3, 5])
        self.add_expected_delta([3, 5, 1, 4])
        self.add_expected_delta([3, 5, 1, 4])
        self.solve_and_test(cp.EXCHANGE)

    def test_exchange4(self):
        self.set_first_solution([4, 1, 3, 5])
        self.solve_and_test(cp.EXCHANGE)

    def test_cross1(self):
        self.set_first_paths(3, [[0, 1, 3], [2, 4]])
        self.add_expected_paths([[0, 3], [2, 1, 4]])
        self.solve_and_test(cp.CROSS)

    def test_cross2_not_equivalent(self):
        self.set_first_paths(4, [[0, 1, 2, 4], [3, 5]])
        self.add_expected_paths([[0, 2, 4], [3, 1, 5]])
        self.add_expected_paths([[0, 4], [3, 1, 2, 5]])
        self.add_expected_paths([[0, 1, 4], [3, 2, 5]])
        self.solve_and_test(cp.CROSS)

    def test_make_active(self):
        self.set_first_solution([1, 3, 2])
        self.add_expected_delta([2, -1, 1])
        self.add_expected_delta([-1, 2, 3])
        self.solve_and_test(cp.MAKEACTIVE)

    def test_make_inactive(self):
        self.set_first_solution([1, 2])
        self.add_expected_delta([2, 1])
        self.solve_and_test(cp.MAKEINACTIVE)

    def test_swap_active(self):
        self.set_first_solution([1, 3, 2])
        self.add_expected_delta([2, 1, 3])
        self.solve_and_test(cp.SWAPACTIVE)


class TestOperator(cp.LocalSearchOperator):

    def __init__(self, vars_list):
        super().__init__()
        self.vars_ = vars_list
        self.num_neighbors_ = 0
        self.num_start_calls_ = 0

    def next_neighbor(self, delta, unused_deltadelta):
        if self.num_neighbors_ < len(self.vars_):
            delta.add(self.vars_[self.num_neighbors_]).set_value(0)
            self.num_neighbors_ += 1
            return True
        return False

    def start(self, unused_assignment):
        self.num_start_calls_ += 1

    def num_neighbors(self):
        return self.num_neighbors_

    def num_start_calls(self):
        return self.num_start_calls_


class LocalSearchFindOneNeighborTest(absltest.TestCase):

    def test_consecutive_solves(self):
        solver = cp.Solver("LocalSearchFindOneNeighborTest")
        vars_list = [solver.new_int_var(0, 10, f"v{i}") for i in range(3)]
        assignment = solver.assignment()
        assignment.add(vars_list)
        assignment.set_value(vars_list[0], 1)
        assignment.set_value(vars_list[1], 2)
        assignment.set_value(vars_list[2], 7)

        op = TestOperator(vars_list)
        objective = solver.new_int_var(0, 100, "obj")
        solver.add(solver.sum(vars_list) == 10)

        for i in range(len(vars_list)):
            ls_params = solver.local_search_phase_parameters(
                objective,
                op,
                solver.phase(vars_list, cp.INT_VAR_DEFAULT, cp.INT_VALUE_DEFAULT),
            )

            db = solver.local_search_phase(assignment, ls_params)
            solver.solve(db, [solver.minimize(objective, 1)])
            self.assertEqual(i + 1, op.num_start_calls())


class LocalSearchStateTest(absltest.TestCase):

    def test_dummy_variable_interface(self):
        variable = cp.LocalSearchState.dummy_variable()
        self.assertFalse(variable.exists())
        self.assertTrue(variable.set_min(2**63 - 1))
        self.assertTrue(variable.set_max(-(2**63)))
        variable.relax()  # Does not crash.

    def test_initial_variable_range(self):
        state = cp.LocalSearchState()
        variables = []
        for _ in range(5):
            domain_id = state.add_variable_domain(-(2**63), 2**63 - 1)
            variables.append(state.make_variable(domain_id))
        state.compile_constraints()

        for variable in variables:
            self.assertTrue(variable.exists())
            self.assertEqual(variable.min(), -(2**63))
            self.assertEqual(variable.max(), 2**63 - 1)
        self.assertTrue(state.state_is_feasible())

    def test_tightening_modifies_variables(self):
        state = cp.LocalSearchState()
        variables = []
        for _ in range(5):
            domain_id = state.add_variable_domain(-(2**63), 2**63 - 1)
            variables.append(state.make_variable(domain_id))
        state.compile_constraints()

        for i in range(len(variables)):
            variables[i].relax()
            self.assertTrue(variables[i].set_min(-i))
            self.assertTrue(variables[i].set_max(i))
        self.assertTrue(state.state_is_feasible())

        for i in range(len(variables)):
            self.assertEqual(-i, variables[i].min())
            self.assertEqual(i, variables[i].max())
        self.assertTrue(state.state_is_feasible())


class SubDagComputerTest(absltest.TestCase):

    def test_empty_dag(self):
        for num_nodes in range(8):
            computer = cp.SubDagComputer()
            computer.build_graph(num_nodes)
            for n in range(num_nodes):
                self.assertEmpty(computer.compute_sorted_sub_dag_arcs(n))

    def test_one_arc_dag(self):
        for num_nodes in range(8):
            for n1 in range(num_nodes):
                for n2 in range(num_nodes):
                    if n1 == n2:
                        continue
                    computer = cp.SubDagComputer()
                    computer.add_arc(n1, n2)
                    computer.build_graph(num_nodes)
                    for n in range(num_nodes):
                        if n == n1:
                            arcs = computer.compute_sorted_sub_dag_arcs(n)
                            self.assertLen(arcs, 1)
                            self.assertEqual(arcs[0], 0)
                        else:
                            self.assertEmpty(computer.compute_sorted_sub_dag_arcs(n))


class TestIntVarLocalSearchFilter(cp.IntVarLocalSearchFilter):

    def accept(self):
        return True


class LocalSearchFilterTest(absltest.TestCase):

    def test_accept_filter(self):
        solver = cp.Solver("AcceptFilterTest")
        filter_ls = solver.make_accept_filter()
        delta = solver.assignment()
        filter_ls.synchronize(delta, delta)
        self.assertEqual(0, filter_ls.get_synchronized_objective_value())

        vars_list = [solver.new_int_var(0, 10, f"x{i}") for i in range(3)]
        for i in range(len(vars_list)):
            deltadelta = solver.assignment()
            delta.add(vars_list[i])
            delta.set_value(vars_list[i], i)
            deltadelta.add(vars_list[i])
            deltadelta.set_value(vars_list[i], i)
            self.assertTrue(filter_ls.accept(delta, deltadelta, 0, 10))
            self.assertEqual(0, filter_ls.get_accepted_objective_value())

    def test_reject_filter(self):
        solver = cp.Solver("RejectFilterTest")
        filter_ls = solver.make_reject_filter()
        delta = solver.assignment()
        filter_ls.synchronize(delta, delta)
        self.assertEqual(0, filter_ls.get_synchronized_objective_value())

        vars_list = [solver.new_int_var(0, 10, f"x{i}") for i in range(3)]
        for i in range(len(vars_list)):
            deltadelta = solver.assignment()
            delta.add(vars_list[i])
            delta.set_value(vars_list[i], i)
            deltadelta.add(vars_list[i])
            deltadelta.set_value(vars_list[i], i)
            self.assertFalse(filter_ls.accept(delta, deltadelta, 0, 10))

    def test_domain_filter(self):
        solver = cp.Solver("DomainFilterTest")
        size = 3
        vars_list = [solver.new_int_var(0, 2, f"vars{i}") for i in range(size)]
        vars_list[1].set_value(0)
        vars_list[2].remove_value(1)
        deltadelta = solver.assignment()
        value = 1
        filter_ls = solver.make_variable_domain_filter()
        for i in range(size):
            delta = solver.assignment()
            delta.add(vars_list[i])
            delta.set_value(vars_list[i], value)
            self.assertEqual(
                vars_list[i].contains(value),
                filter_ls.accept(delta, deltadelta, -1000000, 1000000),
            )

    def test_int_var_local_search_filter(self):
        num_vars = 3
        solver = cp.Solver("IntVarLocalSearchFilterTest")
        vars_list = [solver.new_int_var(0, 5, f"vars{i}") for i in range(num_vars)]
        filter_ls = TestIntVarLocalSearchFilter(vars_list)
        self.assertEqual(num_vars, filter_ls.size())
        for i in range(num_vars):
            self.assertEqual(vars_list[i].name, filter_ls.var(i).name)
            found, index = filter_ls.find_index(vars_list[i])
            self.assertTrue(found)
            self.assertEqual(i, index)

        assignment = solver.assignment()
        filter_ls.synchronize(assignment, None)
        for i in range(num_vars):
            self.assertFalse(filter_ls.is_var_synced(i))

        for i in range(num_vars):
            assignment.add(vars_list[i])
            assignment.set_value(vars_list[i], i)

        filter_ls.synchronize(assignment, None)
        for i in range(num_vars):
            self.assertTrue(filter_ls.is_var_synced(i))
            self.assertEqual(i, filter_ls.value(i))

        extra_vars = [solver.new_bool_var("extra")]
        filter_ls.add_vars(extra_vars)
        self.assertEqual(num_vars + 1, filter_ls.size())
        filter_ls.synchronize(assignment, None)
        self.assertFalse(filter_ls.is_var_synced(num_vars))
        assignment.add(extra_vars[-1])
        assignment.set_value(extra_vars[-1], 1)
        filter_ls.synchronize(assignment, None)
        self.assertTrue(filter_ls.is_var_synced(num_vars))
        self.assertEqual(1, filter_ls.value(num_vars))


class NamedLocalSearchOperator(cp.LocalSearchOperator):

    def __init__(self, name):
        super().__init__()
        self.name_ = name
        self.called_ = False

    def next_neighbor(self, unused_delta, unused_deltadelta):
        if self.called_:
            return False
        self.called_ = True
        return True

    def start(self, assignment):
        pass

    def __str__(self):
        return self.name_


class LocalSearchOperatorTest(absltest.TestCase):

    def test_debug_string(self):
        op1 = NamedLocalSearchOperator("op1")
        self.assertEqual("op1", str(op1))
        op2 = NamedLocalSearchOperator("op2")
        solver = cp.Solver("Test")
        op = solver.concatenate_operators([op1, op2])
        assignment = solver.assignment()
        delta = solver.assignment()
        op.start(assignment)
        op.next_neighbor(delta, None)
        self.assertEqual("op1", str(op))
        op.next_neighbor(delta, None)
        self.assertEqual("op2", str(op))

    def test_self(self):
        op1 = NamedLocalSearchOperator("op1")
        self.assertEqual(op1, op1.self())
        op2 = NamedLocalSearchOperator("op2")
        solver = cp.Solver("Test")
        op = solver.concatenate_operators([op1, op2])
        assignment = solver.assignment()
        delta = solver.assignment()
        op.start(assignment)
        op.next_neighbor(delta, None)
        self.assertEqual(op1, op.self())
        op.next_neighbor(delta, None)
        self.assertEqual(op2, op.self())


class SetAllToValue(cp.DecisionBuilder):

    def __init__(self, vars_list, value):
        super().__init__()
        self.vars_list_ = vars_list
        self.value_ = value

    def next(self, solver):
        for var in self.vars_list_:
            var.set_value(self.value_)
        return None

    def __str__(self):
        return "SetAllToValue"


class LocalSearchTest(absltest.TestCase):

    def test_improve_assignment_with_decrement_values(self):
        num_vars = 10
        solver = cp.Solver("TestDecrement")
        vars_list = [solver.new_int_var(0, 10, f"v{i}") for i in range(num_vars)]
        objective = solver.new_int_var(0, 100, "obj")
        solver.add(objective == solver.sum(vars_list).var())

        assignment = solver.assignment()
        assignment.add(vars_list)
        assignment.add_objective(objective)

        initial_value = 1
        first_solution = SetAllToValue(vars_list, initial_value)
        store = solver.make_store_assignment(assignment)
        first_solution_and_store = solver.compose(first_solution, store)

        op = solver.operator(vars_list, cp.DECREMENT)
        ls_params = solver.local_search_phase_parameters(
            objective, op, None, None, None
        )
        local_search = solver.local_search_phase(assignment, ls_params)

        self.assertTrue(solver.solve(first_solution_and_store))
        self.assertEqual(num_vars * initial_value, assignment.objective_value())

        collector = solver.all_solution_collector(assignment)
        limit = solver.limit(2**63 - 1, 2**63 - 1, 2**63 - 1, 2**63 - 1)
        optimize = solver.minimize(objective, 1)

        self.assertTrue(solver.solve(local_search, [collector, optimize, limit]))
        self.assertGreater(collector.solution_count, 0)
        self.assertEqual(0, collector.objective_value(collector.solution_count - 1))


class RandomLnsTest(absltest.TestCase):

    def test_deterministic_random_lns(self):
        num_iterations = 4
        random_variables = 4
        num_vars = 100
        solver = cp.Solver("TestRandomLns")
        vars_list = [solver.new_bool_var(f"v{i}") for i in range(num_vars)]
        random_lns = solver.random_lns_operator(vars_list, random_variables, 1)

        assignment = solver.assignment()
        assignment.add(vars_list)
        for i in range(num_vars):
            assignment.set_value(vars_list[i], 0)

        random_lns.start(assignment)
        delta = solver.assignment()
        for _ in range(num_iterations):
            self.assertTrue(random_lns.next_neighbor(delta, None))
            self.assertEqual(random_variables, delta.size())
            delta.clear()


class TestIntVarLocalSearchOperator(cp.IntVarLocalSearchOperator):

    def __init__(self, vars_list):
        super().__init__(vars_list, False)
        self.index_ = 0

    def one_neighbor(self):
        if self.index_ >= self.size():
            return False
        cp.IntVarLocalSearchOperator.set_value(
            self, self.index_, self.value(self.index_) + 1
        )
        self.index_ += 1
        return True

    def on_start(self):
        self.index_ = 0


class IntVarLocalSearchOperatorTest(absltest.TestCase):

    def test_make_one_neighbor(self):
        solver = cp.Solver("Test")
        vars_list = [solver.new_int_var(0, 10, f"v{i}") for i in range(5)]
        op = TestIntVarLocalSearchOperator(vars_list)
        assignment = solver.assignment()
        assignment.add(vars_list)
        for i in range(5):
            assignment.set_value(vars_list[i], i)
        op.start(assignment)
        delta = solver.assignment()
        for i in range(5):
            res = op.next_neighbor(delta, None)
            self.assertTrue(res)
            self.assertEqual(1, delta.size())
            self.assertEqual(i + 1, delta.value(vars_list[i]))
        self.assertFalse(op.next_neighbor(delta, None))


if __name__ == "__main__":
    absltest.main()
