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

from absl.testing import absltest

from ortools.set_cover.python import set_cover


def create_initial_cover_model():
    model = set_cover.SetCoverModel()
    model.add_empty_subset(1.0)
    model.add_element_to_last_subset(0)
    model.add_empty_subset(1.0)
    model.add_element_to_last_subset(1)
    model.add_element_to_last_subset(2)
    model.add_empty_subset(1.0)
    model.add_element_to_last_subset(1)
    model.add_empty_subset(1.0)
    model.add_element_to_last_subset(2)
    return model


def create_knights_cover_model(num_rows: int, num_cols: int) -> set_cover.SetCoverModel:
    model = set_cover.SetCoverModel()
    knight_row_move = [2, 1, -1, -2, -2, -1, 1, 2]
    knight_col_move = [1, 2, 2, 1, -1, -2, -2, -1]

    for row in range(num_rows):
        for col in range(num_cols):
            model.add_empty_subset(1.0)
            model.add_element_to_last_subset(row * num_cols + col)

            for i in range(8):
                new_row = row + knight_row_move[i]
                new_col = col + knight_col_move[i]
                if 0 <= new_row < num_rows and 0 <= new_col < num_cols:
                    model.add_element_to_last_subset(new_row * num_cols + new_col)

    return model


# This test case is mostly a Python port of set_cover_test.cc.
class SetCoverTest(absltest.TestCase):

    def test_save_reload(self):
        model = create_knights_cover_model(10, 10)
        model.sort_elements_in_subsets()
        proto = model.export_model_as_proto()
        reloaded = set_cover.SetCoverModel()
        reloaded.import_model_from_proto(proto)

        self.assertEqual(model.num_subsets, reloaded.num_subsets)
        self.assertEqual(model.num_elements, reloaded.num_elements)
        self.assertEqual(model.subset_costs, reloaded.subset_costs)
        self.assertEqual(model.columns, reloaded.columns)
        if model.row_view_is_valid and reloaded.row_view_is_valid:
            self.assertEqual(model.rows, reloaded.rows)

    def test_save_reload_twice(self):
        model = create_knights_cover_model(3, 3)
        inv = set_cover.SetCoverInvariant(model)

        greedy = set_cover.GreedySolutionOptimizer(inv)
        self.assertTrue(greedy.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )
        greedy_proto = inv.export_solution_as_proto()

        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )
        steepest_proto = inv.export_solution_as_proto()

        inv.import_solution_from_proto(greedy_proto)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )
        reloaded_proto = inv.export_solution_as_proto()
        self.assertEqual(str(steepest_proto), str(reloaded_proto))

    def test_initial_values(self):
        model = create_initial_cover_model()
        self.assertTrue(model.compute_feasibility())

        inv = set_cover.SetCoverInvariant(model)
        trivial = set_cover.TrivialSolutionGenerator(inv)
        self.assertTrue(trivial.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.COST_AND_COVERAGE)
        )

        greedy = set_cover.GreedySolutionOptimizer(inv)
        self.assertTrue(greedy.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

        self.assertEqual(inv.num_uncovered_elements(), 0)
        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.COST_AND_COVERAGE)
        )

    def test_infeasible(self):
        model = set_cover.SetCoverModel()
        model.add_empty_subset(1.0)
        model.add_element_to_last_subset(0)
        model.add_empty_subset(1.0)
        model.add_element_to_last_subset(3)
        self.assertFalse(model.compute_feasibility())

    def test_knights_cover_creation(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())

    def test_knights_cover_greedy(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        greedy = set_cover.GreedySolutionOptimizer(inv)
        self.assertTrue(greedy.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

    def test_knights_cover_degree(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        degree = set_cover.ElementDegreeSolutionGenerator(inv)
        self.assertTrue(degree.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.COST_AND_COVERAGE)
        )

        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

    def test_knights_cover_gls(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        greedy = set_cover.GreedySolutionOptimizer(inv)
        self.assertTrue(greedy.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

        gls = set_cover.GuidedLocalSearch(inv)
        gls.set_max_iterations(500)
        self.assertTrue(gls.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

    def test_knights_cover_random(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        random = set_cover.RandomSolutionGenerator(inv)
        self.assertTrue(random.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.COST_AND_COVERAGE)
        )

        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

    def test_knights_cover_trivial(self):
        model = create_knights_cover_model(16, 16)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        trivial = set_cover.TrivialSolutionGenerator(inv)
        self.assertTrue(trivial.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.COST_AND_COVERAGE)
        )

        steepest = set_cover.SteepestSearch(inv)
        steepest.set_max_iterations(500)
        self.assertTrue(steepest.optimize())
        self.assertTrue(
            inv.check_consistency(set_cover.consistency_level.FREE_AND_UNCOVERED)
        )

    # TODO(user): KnightsCoverGreedyAndTabu, KnightsCoverGreedyRandomClear,
    # KnightsCoverElementDegreeRandomClear, KnightsCoverRandomClearMip,
    # KnightsCoverMip

    def test_lagrangian_lower_bound(self):
        model = create_knights_cover_model(8, 8)
        self.assertTrue(model.compute_feasibility())
        inv = set_cover.SetCoverInvariant(model)

        greedy = set_cover.GreedySolutionOptimizer(inv)
        self.assertTrue(greedy.optimize())
        upper_bound = inv.cost()

        # The constructor creates the thread pool, so compute_lower_bound()
        # is usable without a prior use_num_threads() call.
        lagrangian = set_cover.SetCoverLagrangian(inv, num_threads=4)
        lower_bound, reduced_costs, multipliers = lagrangian.compute_lower_bound(
            model.subset_costs, upper_bound
        )
        self.assertLen(reduced_costs, model.num_subsets)
        self.assertLen(multipliers, model.num_elements)
        self.assertLessEqual(lower_bound, upper_bound)
        # The bound is also reported on the invariant, as DualAscentOptimizer
        # does with its bound.
        self.assertEqual(inv.export_solution_as_proto().cost_lower_bound, lower_bound)

    def test_lagrangian_lower_bound_never_exceeds_optimum(self):
        # Six elements on a cycle, subset i covers {i, i + 1}: the optimum is 3.
        model = set_cover.SetCoverModel()
        for i in range(6):
            model.add_empty_subset(1.0)
            model.add_element_to_last_subset(i)
            model.add_element_to_last_subset((i + 1) % 6)
        inv = set_cover.SetCoverInvariant(model)
        self.assertTrue(set_cover.GreedySolutionOptimizer(inv).optimize())

        lagrangian = set_cover.SetCoverLagrangian(inv)
        # use_num_threads() returns the object itself, so it can be chained.
        self.assertIs(lagrangian.use_num_threads(2), lagrangian)
        lower_bound, _, _ = lagrangian.compute_lower_bound(
            model.subset_costs, inv.cost()
        )
        self.assertGreater(lower_bound, 0.0)
        self.assertLessEqual(lower_bound, 3.0)

    def test_lagrangian_rejects_invalid_arguments(self):
        model = create_knights_cover_model(4, 4)
        inv = set_cover.SetCoverInvariant(model)
        with self.assertRaises(ValueError):
            set_cover.SetCoverLagrangian(inv, num_threads=0)
        lagrangian = set_cover.SetCoverLagrangian(inv)
        with self.assertRaises(ValueError):
            lagrangian.use_num_threads(-1)
        with self.assertRaises(ValueError):
            lagrangian.compute_lower_bound([1.0, 2.0], 10.0)


if __name__ == "__main__":
    absltest.main()
