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

"""A simple set-covering problem."""

# [START program]
# [START import]
from ortools.set_cover.python import set_cover

# [END import]


def main():
    # [START data]
    model = set_cover.SetCoverModel()
    model.add_empty_subset(2.0)
    model.add_element_to_last_subset(0)
    model.add_empty_subset(2.0)
    model.add_element_to_last_subset(1)
    model.add_empty_subset(1.0)
    model.add_element_to_last_subset(0)
    model.add_element_to_last_subset(1)
    # [END data]

    # [START solve]
    inv = set_cover.SetCoverInvariant(model)
    greedy = set_cover.GreedySolutionGenerator(inv)
    has_found = greedy.next_solution()
    if not has_found:
        print("No solution found by the greedy heuristic.")
        return
    solution = inv.export_solution_as_proto()
    # [END solve]

    # [START print_solution]
    print(f"Total cost: {solution.cost}")  # == inv.cost()
    print(f"Total number of selected subsets: {solution.num_subsets}")
    print("Chosen subsets:")
    for subset in solution.subset:
        print(f"  {subset}")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
