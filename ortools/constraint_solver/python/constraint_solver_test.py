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

"""Test for constraint_solver pybind11 layer."""

from absl.testing import absltest
from ortools.constraint_solver.python import constraint_solver


class ConstraintSolverTest(absltest.TestCase):

    def test_create_solver(self):
        print("test_create_solver")
        solver = constraint_solver.Solver("test_create_solver")
        print(solver)

    def test_create_int_var(self):
        print("test_create_int_var")
        solver = constraint_solver.Solver("test_create_int_var")
        x = solver.new_int_var(0, 10, "x")
        self.assertEqual(str(x), "x(0..10)")
        self.assertEqual(x.min(), 0)
        self.assertEqual(x.max(), 10)
        self.assertEqual(x.name, "x")

        y = solver.new_int_var([0, 2, 4])
        self.assertEqual(y.min(), 0)
        self.assertEqual(y.max(), 4)
        self.assertEmpty(y.name)
        y.name = "y"
        self.assertEqual(y.name, "y")

    def test_create_int_expr(self):
        print("test_create_int_expr")
        solver = constraint_solver.Solver("test_create_int_expr")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")

        x_plus_3 = x + 3
        self.assertEqual(str(x_plus_3), "(x(0..10) + 3)")
        print(x_plus_3)
        self.assertEqual(x_plus_3.min(), 3)
        self.assertEqual(x_plus_3.max(), 13)

        self.assertEqual(str(x * 5), "(x(0..10) * 5)")
        self.assertEqual(str(x + y), "(x(0..10) + y(0..10))")
        self.assertEqual(str(2 + x), "(x(0..10) + 2)")
        self.assertEqual(str(7 * x), "(x(0..10) * 7)")
        self.assertEqual(str(x * y), "(x(0..10) * y(0..10))")
        self.assertEqual(str(x + 2 * y + 5), "((x(0..10) + (y(0..10) * 2)) + 5)")

    def test_fail_outside_solve(self):
        print("test_fail_outside_solve")
        solver = constraint_solver.Solver("test_fail_outside_solve")
        x = solver.new_int_var(0, 10, "x")
        try:
            x.set_min(20)
        except ValueError:
            print("  fail caught")

    def test_rabbits_pheasants(self):
        print("test_rabbits_pheasants")
        solver = constraint_solver.Solver("test_rabbits_pheasants")
        rabbits = solver.new_int_var(0, 20, "rabbits")
        pheasants = solver.new_int_var(0, 20, "pheasants")
        solver.add(rabbits + pheasants == 20)
        solver.add(4 * rabbits + 2 * pheasants == 56)
        solver.accept(solver.print_model_visitor())

    def _solve_and_check(self, solver, variables, check_single_solution_callback):
        db = solver.phase(
            variables,
            constraint_solver.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            constraint_solver.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.new_search(db)
        solution_count = 0
        while solver.next_solution():
            solution_count += 1
            check_single_solution_callback()
        solver.end_search()
        return solution_count

    def test_add_abs_equality(self):
        solver = constraint_solver.Solver("test_add_abs_equality")
        x = solver.new_int_var(-5, 5, "x")
        abs_x = solver.new_int_var(0, 5, "abs_x")
        solver.add_abs_equality(x, abs_x)

        def check_single_solution():
            self.assertEqual(abs(x.value()), abs_x.value())

        count = self._solve_and_check(solver, [x, abs_x], check_single_solution)
        self.assertEqual(count, 11)

    def test_add_all_different(self):
        solver = constraint_solver.Solver("test_add_all_different")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        solver.add_all_different(variables)

        def check_single_solution():
            values = [var.value() for var in variables]
            self.assertLen(set(values), 3)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 6)

    def test_add_all_different_except(self):
        solver = constraint_solver.Solver("test_add_all_different_except")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        # 0 is the escape value, so duplicated 0s are allowed.
        solver.add_all_different_except(variables, 0)

        def check_single_solution():
            values = [var.value() for var in variables]
            non_zeros = [val for val in values if val != 0]
            self.assertLen(set(non_zeros), len(non_zeros))

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 13)

    def test_add_between_ct(self):
        solver = constraint_solver.Solver("test_add_between_ct")
        x = solver.new_int_var(0, 10, "x")
        solver.add_between_ct(x, 2, 4)

        def check_single_solution():
            self.assertBetween(x.value(), 2, 4)

        count = self._solve_and_check(solver, [x], check_single_solution)
        self.assertEqual(count, 3)  # 2, 3, 4

    def test_add_circuit(self):
        solver = constraint_solver.Solver("test_add_circuit")
        # 3 nodes 0, 1, 2. Circuit must visit all.
        # x[i] is the successor of i.
        x = [solver.new_int_var(0, 2, f"x{i}") for i in range(3)]
        solver.add_circuit(x)

        def check_single_solution():
            # Rudimentary check: all different and no sub-cycles (implicitly checked
            # by circuit)
            values = [var.value() for var in x]
            self.assertLen(set(values), 3)
            # Check cycle length
            curr = 0
            visited = 0
            for _ in range(3):
                curr = values[curr]
                visited += 1
            self.assertEqual(curr, 0)  # Should return to start
            self.assertEqual(visited, 3)

        count = self._solve_and_check(solver, x, check_single_solution)
        self.assertEqual(count, 2)  # 0->1->2->0, 0->2->1->0

    def test_add_count(self):
        solver = constraint_solver.Solver("test_add_count")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        # Count occurrences of value 1.
        count_var = solver.new_int_var(0, 3, "count")
        solver.add_count(variables, 1, count_var)

        def check_single_solution():
            values = [var.value() for var in variables]
            self.assertEqual(values.count(1), count_var.value())

        count = self._solve_and_check(
            solver, variables + [count_var], check_single_solution
        )
        self.assertEqual(count, 27)

    def test_add_cover(self):
        solver = constraint_solver.Solver("test_add_cover")
        s1 = solver.new_int_var(0, 5, "s1")
        s2 = solver.new_int_var(0, 5, "s2")
        t1 = solver.new_fixed_duration_interval_var(s1, 2, "t1")
        t2 = solver.new_fixed_duration_interval_var(s2, 3, "t2")
        # target is the convex hull of t1 and t2.
        # As t1 and t2 are not optional, target is also non optional.
        target = solver.new_interval_var(
            start_min=0,
            start_max=5,
            duration_min=0,
            duration_max=10,
            end_min=0,
            end_max=10,
            optional=False,
            name="target",
        )
        solver.add_cover([t1, t2], target)

        def check_single_solution():
            self.assertEqual(target.start_min(), min(s1.value(), s2.value()))
            self.assertEqual(target.end_max(), max(s1.value() + 2, s2.value() + 3))

        count = self._solve_and_check(solver, [s1, s2], check_single_solution)
        self.assertEqual(count, 36)  # 6 * 6

    def test_add_cumulative(self):
        solver = constraint_solver.Solver("test_add_cumulative")
        # Just verify it doesn't crash on invocation, hard to verify semantics
        # deeply in small unit test without intervals.
        # But we can try a simple case.
        s0 = solver.new_int_var(0, 2, "s0")
        s1 = solver.new_int_var(0, 2, "s1")
        durations = [2, 2]
        demands = [1, 1]
        capacity = 1
        intervals = [
            solver.new_fixed_duration_interval_var(s0, durations[0], "i0"),
            solver.new_fixed_duration_interval_var(s1, durations[1], "i1"),
        ]

        solver.add_cumulative(intervals, demands, capacity, "cumul")
        # Since capacity is 1 and demands are 1, they cannot overlap.

        def check_single_solution():
            # Intervals are [s0, s0+2) and [s1, s1+2)
            # They must not overlap.
            self.assertTrue(
                s0.value() + 2 <= s1.value() or s1.value() + 2 <= s0.value()
            )

        count = self._solve_and_check(solver, [s0, s1], check_single_solution)
        self.assertEqual(count, 2)

    def test_add_deviation(self):
        solver = constraint_solver.Solver("test_add_deviation")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        deviation = solver.new_int_var(0, 10, "dev")
        # deviation = sum |variables[i] - mean| where mean is implied.
        # Actually MakeDeviation(variables, deviation, total_sum)
        # deviation = sum |n * variables[i] - total_sum|
        total_sum = 3  # if mean is 1.
        solver.add_deviation(variables, deviation, total_sum)

        def check_single_solution():
            vals = [var.value() for var in variables]
            calc_dev = sum(abs(3 * val - total_sum) for val in vals)
            self.assertGreaterEqual(deviation.value(), calc_dev)

        count = self._solve_and_check(
            solver, variables + [deviation], check_single_solution
        )
        self.assertEqual(count, 41)

    def test_add_disjunctive_constraint(self):
        solver = constraint_solver.Solver("test_add_disjunctive")
        s0 = solver.new_int_var(0, 10, "s0")
        s1 = solver.new_int_var(0, 10, "s1")
        intervals = [
            solver.new_fixed_duration_interval_var(s0, 2, "i0"),
            solver.new_fixed_duration_interval_var(s1, 2, "i1"),
        ]

        solver.add_disjunctive_constraint(intervals, "disjunctive")

        def check_single_solution():
            self.assertTrue(
                s0.value() + 2 <= s1.value() or s1.value() + 2 <= s0.value()
            )

        count = self._solve_and_check(solver, [s0, s1], check_single_solution)
        self.assertEqual(count, 90)

    def test_add_distribute(self):
        solver = constraint_solver.Solver("test_add_distribute")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        # Count how many times 0, 1, 2 appear.
        cards = [solver.new_int_var(0, 3, f"c{i}") for i in range(3)]
        solver.add_distribute(variables, cards)

        def check_single_solution():
            vals = [var.value() for var in variables]
            for i in range(3):
                self.assertEqual(vals.count(i), cards[i].value())

        count = self._solve_and_check(solver, variables + cards, check_single_solution)
        self.assertEqual(count, 27)

    def test_add_element_equality(self):
        solver = constraint_solver.Solver("test_add_element")
        values = [10, 20, 30]
        index = solver.new_int_var(0, 2, "index")
        target = solver.new_int_var(0, 100, "target")
        solver.add_element_equality(values, index, target)

        def check_single_solution():
            self.assertEqual(values[index.value()], target.value())

        count = self._solve_and_check(solver, [index, target], check_single_solution)
        self.assertEqual(count, 3)

    def test_add_false_constraint(self):
        solver = constraint_solver.Solver("test_add_false")
        solver.add_false_constraint()
        count = self._solve_and_check(solver, [], lambda: None)
        self.assertEqual(count, 0)

    def test_add_index_of_constraint(self):
        solver = constraint_solver.Solver("test_add_index_of")
        variables = [solver.new_int_var(0, 10, f"v{i}") for i in range(5)]
        index = solver.new_int_var(0, 4, "index")
        target = 5
        # variables[index] == target
        solver.add_index_of_constraint(variables, index, target)

        def check_single_solution():
            self.assertEqual(variables[index.value()].value(), target)

        count = self._solve_and_check(
            solver, variables + [index], check_single_solution
        )
        self.assertEqual(count, 50000)

    def test_add_inverse_permutation(self):
        solver = constraint_solver.Solver("test_inverse")
        left = [solver.new_int_var(0, 2, f"l{i}") for i in range(3)]
        right = [solver.new_int_var(0, 2, f"r{i}") for i in range(3)]
        solver.add_inverse_permutation_constraint(left, right)

        def check_single_solution():
            l_vals = [var.value() for var in left]
            r_vals = [var.value() for var in right]
            for i in range(3):
                self.assertEqual(r_vals[l_vals[i]], i)

        count = self._solve_and_check(solver, left + right, check_single_solution)
        self.assertEqual(count, 6)  # 3!

    def test_add_is_between_ct(self):
        solver = constraint_solver.Solver("test_is_between")
        x = solver.new_int_var(0, 5, "x")
        b = solver.new_int_var(0, 1, "b")
        solver.add_is_between_ct(x, 2, 4, b)

        def check_single_solution():
            self.assertEqual(2 <= x.value() <= 4, bool(b.value()))

        count = self._solve_and_check(solver, [x, b], check_single_solution)
        self.assertEqual(count, 6)

    def test_add_is_equal_ct(self):
        solver = constraint_solver.Solver("test_is_equal")
        x = solver.new_int_var(0, 2, "x")
        y = solver.new_int_var(0, 2, "y")
        b = solver.new_int_var(0, 1, "b")
        solver.add_is_equal_ct(x, y, b)

        def check_single_solution():
            self.assertEqual(x.value() == y.value(), bool(b.value()))

        count = self._solve_and_check(solver, [x, y, b], check_single_solution)
        self.assertEqual(count, 9)

    def test_add_lexical_less(self):
        solver = constraint_solver.Solver("test_lex_less")
        x_vars = [solver.new_int_var(0, 1, f"x{i}") for i in range(2)]
        y_vars = [solver.new_int_var(0, 1, f"y{i}") for i in range(2)]
        solver.add_lexical_less(x_vars, y_vars)

        def check_single_solution():
            vx = [var.value() for var in x_vars]
            vy = [var.value() for var in y_vars]
            self.assertLess(vx, vy)

        count = self._solve_and_check(solver, x_vars + y_vars, check_single_solution)
        self.assertEqual(count, 6)

    def test_add_max_equality(self):
        solver = constraint_solver.Solver("test_max_eq")
        variables = [solver.new_int_var(0, 5, f"v{i}") for i in range(3)]
        m = solver.new_int_var(0, 5, "max")
        solver.add_max_equality(variables, m)

        def check_single_solution():
            self.assertEqual(max(var.value() for var in variables), m.value())

        count = self._solve_and_check(solver, variables + [m], check_single_solution)
        self.assertEqual(count, 216)

    def test_add_member_ct(self):
        solver = constraint_solver.Solver("test_member")
        x = solver.new_int_var(0, 10, "x")
        values = [1, 3, 5]
        solver.add_member_ct(x, values)

        def check_single_solution():
            self.assertIn(x.value(), values)

        count = self._solve_and_check(solver, [x], check_single_solution)
        self.assertEqual(count, 3)

    def test_add_min_equality(self):
        solver = constraint_solver.Solver("test_min_eq")
        variables = [solver.new_int_var(0, 5, f"v{i}") for i in range(3)]
        m = solver.new_int_var(0, 5, "min")
        solver.add_min_equality(variables, m)

        def check_single_solution():
            self.assertEqual(min(var.value() for var in variables), m.value())

        count = self._solve_and_check(solver, variables + [m], check_single_solution)
        self.assertEqual(count, 216)

    def test_add_null_intersect(self):
        solver = constraint_solver.Solver("test_null_intersect")
        x_vars = [solver.new_int_var(0, 2, f"x{i}") for i in range(2)]
        y_vars = [solver.new_int_var(0, 2, f"y{i}") for i in range(2)]
        solver.add_null_intersect(x_vars, y_vars)

        def check_single_solution():
            vx = set(var.value() for var in x_vars)
            vy = set(var.value() for var in y_vars)
            self.assertTrue(vx.isdisjoint(vy))

        count = self._solve_and_check(solver, x_vars + y_vars, check_single_solution)
        self.assertEqual(count, 18)

    def test_add_pack(self):
        solver = constraint_solver.Solver("test_pack")
        # 4 items, 2 bins.
        # variables[i] is the bin index for item i.
        variables = [solver.new_int_var(0, 1, f"v{i}") for i in range(4)]
        solver.add_pack(variables, 2)
        # This is mainly creating the Pack object constraint.
        # Just checking it solves.
        count = self._solve_and_check(solver, variables, lambda: None)
        self.assertEqual(count, 16)

    def test_add_scal_prod_equality(self):
        solver = constraint_solver.Solver("test_scal_prod")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        coeffs = [1, 2, 3]
        # 1*x + 2*y + 3*z = 5
        solver.add_scal_prod_equality(variables, coeffs, 5)

        def check_single_solution():
            self.assertEqual(sum(variables[i].value() * coeffs[i] for i in range(3)), 5)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 3)

    def test_add_sorting_constraint(self):
        solver = constraint_solver.Solver("test_sorting")
        variables = [solver.new_int_var(0, 3, f"v{i}") for i in range(3)]
        sorted_vars = [solver.new_int_var(0, 3, f"s{i}") for i in range(3)]
        solver.add_sorting_constraint(variables, sorted_vars)

        def check_single_solution():
            vv = [var.value() for var in variables]
            ss = [var.value() for var in sorted_vars]
            self.assertEqual(sorted(vv), ss)

        count = self._solve_and_check(
            solver, variables + sorted_vars, check_single_solution
        )
        self.assertEqual(count, 64)

    def test_add_sum_equality(self):
        solver = constraint_solver.Solver("test_sum_eq")
        variables = [solver.new_int_var(0, 5, f"v{i}") for i in range(3)]
        solver.add_sum_equality(variables, 10)

        def check_single_solution():
            self.assertEqual(sum(var.value() for var in variables), 10)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 21)

    def test_add_true_constraint(self):
        solver = constraint_solver.Solver("test_true")
        solver.add_true_constraint()
        count = self._solve_and_check(solver, [], lambda: None)
        self.assertEqual(count, 1)

    def test_add_is_different_ct(self):
        solver = constraint_solver.Solver("test_is_diff")
        x = solver.new_int_var(0, 1, "x")
        y = solver.new_int_var(0, 1, "y")
        b = solver.new_int_var(0, 1, "b")
        solver.add_is_different_ct(x, y, b)

        def check_single_solution():
            self.assertEqual(x.value() != y.value(), bool(b.value()))

        count = self._solve_and_check(solver, [x, y, b], check_single_solution)
        self.assertEqual(count, 4)

    def test_add_sum_greater_or_equal(self):
        solver = constraint_solver.Solver("test_sum_ge")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        solver.add_sum_greater_or_equal(variables, 5)

        def check_single_solution():
            self.assertGreaterEqual(sum(var.value() for var in variables), 5)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 4)

    def test_add_scal_prod_greater_or_equal(self):
        solver = constraint_solver.Solver("test_scal_ge")
        v0 = solver.new_int_var(0, 2, "v0")
        v1 = solver.new_int_var(0, 2, "v1")
        variables = [v0, v1]
        coeffs = [2, 3]
        solver.add_scal_prod_greater_or_equal(variables, coeffs, 8)

        def check_single_solution():
            val = v0.value() * 2 + v1.value() * 3
            self.assertGreaterEqual(val, 8)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 2)

    def test_add_temporal_disjunction(self):
        solver = constraint_solver.Solver("test_temporal")
        # t1 before t2 or t2 before t1.
        s1 = solver.new_int_var(0, 10, "s1")
        s2 = solver.new_int_var(0, 10, "s2")
        t1 = solver.new_fixed_duration_interval_var(s1, 2, "t1")
        t2 = solver.new_fixed_duration_interval_var(s2, 2, "t2")
        solver.add_temporal_disjunction(t1, t2)

        def check_single_solution():
            # s1+2 <= s2 or s2+2 <= s1
            self.assertTrue(
                s1.value() + 2 <= s2.value() or s2.value() + 2 <= s1.value()
            )

        count = self._solve_and_check(solver, [s1, s2], check_single_solution)
        self.assertEqual(count, 90)

    def test_add_non_overlapping_boxes_constraint(self):
        solver = constraint_solver.Solver("test_boxes")
        # 2 boxes.
        x_vars = [solver.new_int_var(0, 5, f"x{i}") for i in range(2)]
        y_vars = [solver.new_int_var(0, 5, f"y{i}") for i in range(2)]
        dx = [2, 2]
        dy = [2, 2]
        solver.add_non_overlapping_boxes_constraint(x_vars, y_vars, dx, dy)

        def check_single_solution():
            x0, x1 = x_vars[0].value(), x_vars[1].value()
            y0, y1 = y_vars[0].value(), y_vars[1].value()
            # Check non overlap
            # left of, right of, above, below
            overlap = not (x0 + 2 <= x1 or x1 + 2 <= x0 or y0 + 2 <= y1 or y1 + 2 <= y0)
            self.assertFalse(overlap)

        count = self._solve_and_check(solver, x_vars + y_vars, check_single_solution)
        self.assertEqual(count, 1040)

    def test_add_lexical_less_or_equal(self):
        solver = constraint_solver.Solver("test_lex_le")
        x_vars = [solver.new_int_var(0, 1, f"x{i}") for i in range(2)]
        y_vars = [solver.new_int_var(0, 1, f"y{i}") for i in range(2)]
        solver.add_lexical_less_or_equal(x_vars, y_vars)

        def check_single_solution():
            vx = [var.value() for var in x_vars]
            vy = [var.value() for var in y_vars]
            self.assertLessEqual(vx, vy)

        count = self._solve_and_check(solver, x_vars + y_vars, check_single_solution)
        self.assertEqual(count, 10)

    def test_add_null_intersect_except(self):
        solver = constraint_solver.Solver("test_null_exc")
        x = [solver.new_int_var(0, 2, f"x{i}") for i in range(2)]
        y = [solver.new_int_var(0, 2, f"y{i}") for i in range(2)]
        # Intersect is null except for value 0.
        solver.add_null_intersect_except(x, y, 0)

        def check_single_solution():
            vx = set(var.value() for var in x)
            vy = set(var.value() for var in y)
            intersection = vx.intersection(vy)
            intersection.discard(0)
            self.assertEmpty(intersection)

        count = self._solve_and_check(solver, x + y, check_single_solution)
        self.assertEqual(count, 35)

    def test_add_not_member_ct(self):
        solver = constraint_solver.Solver("test_not_member")
        x = solver.new_int_var(0, 5, "x")
        solver.add_not_member_ct(x, [1, 3])

        def check_single_solution():
            self.assertNotIn(x.value(), [1, 3])

        count = self._solve_and_check(solver, [x], check_single_solution)
        self.assertEqual(count, 4)

    def test_add_is_member_ct(self):
        solver = constraint_solver.Solver("test_is_member")
        x = solver.new_int_var(0, 5, "x")
        b = solver.new_int_var(0, 1, "b")
        solver.add_is_member_ct(x, [1, 3], b)

        def check_single_solution():
            self.assertEqual(x.value() in [1, 3], bool(b.value()))

        count = self._solve_and_check(solver, [x, b], check_single_solution)
        self.assertEqual(count, 6)

    def test_add_sub_circuit(self):
        solver = constraint_solver.Solver("test_sub_circuit")
        x = [solver.new_int_var(0, 2, f"x{i}") for i in range(3)]
        solver.add_sub_circuit(x)
        # SubCircuit allows partial circuits (if x[i] == i, it's not in the
        # circuit). But if in circuit, must form a single circuit.

        def check_single_solution():
            # Check that nodes participating (x[i] != i) form a single cycle.
            values = [var.value() for var in x]
            self.assertLen(set(values), 3)

            in_cycle = [i for i, val in enumerate(values) if val != i]
            if not in_cycle:
                return

            start_node = in_cycle[0]
            curr = start_node
            visited = []
            for _ in range(len(in_cycle)):
                curr = values[curr]
                visited.append(curr)
            self.assertEqual(curr, start_node)
            self.assertCountEqual(visited, in_cycle)

        count = self._solve_and_check(solver, x, check_single_solution)
        self.assertEqual(count, 6)

    def test_add_sum_less_or_equal(self):
        solver = constraint_solver.Solver("test_sum_le")
        variables = [solver.new_int_var(0, 2, f"v{i}") for i in range(3)]
        solver.add_sum_less_or_equal(variables, 2)

        def check_single_solution():
            self.assertLessEqual(sum(var.value() for var in variables), 2)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 10)

    def test_add_scal_prod_less_or_equal(self):
        solver = constraint_solver.Solver("test_scal_le")
        v0 = solver.new_int_var(0, 2, "v0")
        v1 = solver.new_int_var(0, 2, "v1")
        variables = [v0, v1]
        coeffs = [2, 3]
        solver.add_scal_prod_less_or_equal(variables, coeffs, 4)

        def check_single_solution():
            val = v0.value() * 2 + v1.value() * 3
            self.assertLessEqual(val, 4)

        count = self._solve_and_check(solver, variables, check_single_solution)
        self.assertEqual(count, 4)

    def test_add_path_cumul(self):
        solver = constraint_solver.Solver("test_add_path_cumul")
        # 3 nodes.
        nexts = [solver.new_int_var(0, 3, f"n{i}") for i in range(3)]
        active = [solver.new_int_var(1, 1, f"a{i}") for i in range(3)]
        cumuls = [solver.new_int_var(0, 10, f"c{i}") for i in range(4)]
        transits = [solver.new_int_var(1, 1, f"t{i}") for i in range(3)]

        solver.add_path_cumul(nexts, active, cumuls, transits)
        solver.add_circuit(nexts + [solver.new_int_var(0, 0, "back to depot")])
        solver.add(cumuls[0] == 0)

        def check_single_solution():
            n_vals = [var.value() for var in nexts]
            c_vals = [var.value() for var in cumuls]
            cumul = 0
            current = 0
            while True:
                current = n_vals[current]
                if current == 3:
                    break
                cumul += 1
                self.assertEqual(c_vals[current], cumul)

        count = self._solve_and_check(solver, nexts + cumuls, check_single_solution)
        self.assertEqual(count, 2)

    def test_add_interval_var_relation_unary(self):
        solver = constraint_solver.Solver("test_unary_interval_relation")
        s = solver.new_int_var(0, 10, "s")
        t = solver.new_fixed_duration_interval_var(s, 5, "t")
        # t ends after 7. End(t) = s + 5 >= 7 => s >= 2.
        solver.add_interval_var_relation(
            t, constraint_solver.UnaryIntervalRelation.ENDS_AFTER, 7
        )

        def check_single_solution():
            self.assertGreaterEqual(t.end_min(), 7)

        count = self._solve_and_check(solver, [s], check_single_solution)
        self.assertEqual(count, 9)

    def test_add_interval_var_relation_binary(self):
        solver = constraint_solver.Solver("test_binary_interval_relation")
        s1 = solver.new_int_var(0, 10, "s1")
        t1 = solver.new_fixed_duration_interval_var(s1, 5, "t1")
        s2 = solver.new_int_var(0, 10, "s2")
        t2 = solver.new_fixed_duration_interval_var(s2, 5, "t2")
        # t1 starts after t2 end. s1 >= s2 + 5.
        solver.add_interval_var_relation(
            t1, constraint_solver.BinaryIntervalRelation.STARTS_AFTER_END, t2
        )

        def check_single_solution():
            self.assertGreaterEqual(s1.value(), s2.value() + 5)

        count = self._solve_and_check(solver, [s1, s2], check_single_solution)
        self.assertEqual(count, 21)

    def test_add_interval_var_relation_binary_delay(self):
        solver = constraint_solver.Solver("test_binary_interval_relation_delay")
        s1 = solver.new_int_var(0, 10, "s1")
        t1 = solver.new_fixed_duration_interval_var(s1, 5, "t1")
        s2 = solver.new_int_var(0, 10, "s2")
        t2 = solver.new_fixed_duration_interval_var(s2, 5, "t2")
        # t1 starts after t2 end with delay 2. s1 >= s2 + 5 + 2.
        solver.add_interval_var_relation(
            t1, constraint_solver.BinaryIntervalRelation.STARTS_AFTER_END, t2, 2
        )

        def check_single_solution():
            self.assertGreaterEqual(s1.value(), s2.value() + 7)

        count = self._solve_and_check(solver, [s1, s2], check_single_solution)
        self.assertEqual(count, 10)

    def test_int_expr_comparisons(self):
        solver = constraint_solver.Solver("test_int_expr_comparisons")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")

        # x != y
        solver.add(x != y)
        # x >= 3
        solver.add(x >= 3)
        # y <= 8
        solver.add(y <= 8)
        # x > y
        solver.add(x > y)
        # y < 5
        solver.add(y < 5)

        def check_single_solution():
            vx = x.value()
            vy = y.value()
            self.assertNotEqual(vx, vy)
            self.assertGreaterEqual(vx, 3)
            self.assertLessEqual(vy, 8)
            self.assertGreater(vx, vy)
            self.assertLess(vy, 5)

        count = self._solve_and_check(solver, [x, y], check_single_solution)
        self.assertEqual(count, 37)

    def test_int_expr_arithmetic(self):
        solver = constraint_solver.Solver("test_int_expr_arithmetic")
        x = solver.new_int_var(1, 10, "x")
        y = solver.new_int_var(1, 10, "y")

        # x - 5 == 2 => x=7
        solver.add((x - 5) == 2)
        # 10 - y == 3 => y=7
        solver.add((10 - y) == 3)

        # (x + y) // 2 == 7
        # (7 + 7) // 2 = 7
        solver.add(((x + y) // 2) == 7)

        # x % 3 == 1 (7 % 3 = 1)
        solver.add((x % 3) == 1)

        # -x + 10 == 3 (-7 + 10 = 3)
        solver.add((-x + 10) == 3)

        # abs(x - 10) == 3 (abs(7-10) = 3)
        solver.add(abs(x - 10) == 3)

        # x.square() == 49
        solver.add(x.square() == 49)

        def check_single_solution():
            self.assertEqual(x.value(), 7)
            self.assertEqual(y.value(), 7)

        count = self._solve_and_check(solver, [x, y], check_single_solution)
        self.assertEqual(count, 1)

    def test_solution_collector(self):
        solver = constraint_solver.Solver("test_solution_collector")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")
        solver.add(x + y == 10)

        collector = solver.all_solution_collector()
        collector.add([x, y])

        db = solver.phase(
            [x, y],
            constraint_solver.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            constraint_solver.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.solve(db, [collector])

        self.assertEqual(collector.solution_count, 11)
        self.assertTrue(collector.has_solution())
        for i in range(collector.solution_count):
            self.assertEqual(collector.value(i, x) + collector.value(i, y), 10)
            sol = collector.solution(i)
            self.assertEqual(sol.value(x) + sol.value(y), 10)

    def test_best_value_solution_collector(self):
        solver = constraint_solver.Solver("test_best_value_solution_collector")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")
        solver.add(x + y == 10)

        # Maximize x
        collector = solver.best_value_solution_collector(True)
        collector.add([x, y])
        collector.add_objective(x)

        db = solver.phase(
            [x, y],
            constraint_solver.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            constraint_solver.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.solve(db, [collector])

        self.assertEqual(collector.solution_count, 1)
        self.assertEqual(collector.value(0, x), 10)
        self.assertEqual(collector.value(0, y), 0)
        self.assertEqual(collector.objective_value(0), 10)

    def test_first_last_solution_collector(self):
        solver = constraint_solver.Solver("test_first_last_solution_collector")
        x = solver.new_int_var(0, 10, "x")
        solver.add(x >= 5)

        first_collector = solver.first_solution_collector()
        first_collector.add(x)
        last_collector = solver.last_solution_collector()
        last_collector.add(x)

        db = solver.phase(
            [x],
            constraint_solver.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            constraint_solver.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
        solver.solve(db, [first_collector, last_collector])

        self.assertEqual(first_collector.value(0, x), 5)
        self.assertEqual(last_collector.value(0, x), 10)

    def test_optimize_var(self):
        solver = constraint_solver.Solver("test_optimize_var")
        x = solver.new_int_var(0, 10, "x")
        obj = solver.maximize(x, 1)

        db = solver.phase(
            [x],
            constraint_solver.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            constraint_solver.IntValueStrategy.ASSIGN_MIN_VALUE,
        )

        collector = solver.last_solution_collector()
        collector.add(x)

        solver.solve(db, [obj, collector])
        self.assertEqual(collector.value(0, x), 10)


if __name__ == "__main__":
    absltest.main()
