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

"""Create a balanced music playlist.

Create a music playlist by selecting tunes from a list of tunes.

Each tune has a duration in seconds and a music genre (e.g. Rock, Disco, Techno,
etc).

The total playlist duration must be as close as possible to a given total
duration. Each tune can appear at most once in the playlist. All existing
genres must appear at least once in the playlist. Two consecutive tunes must be
of different genres. There is a positive cost to go from a genre to another, and
the playlist must minimize this cost overall.
"""

from collections.abc import Sequence

from absl import app
from ortools.sat.python import cp_model


def Solve():
    """Solves the music playlist problem."""

    # --------------------
    # 1. Data
    # --------------------
    tunes = [
        ("Song 01", 202, "Pop"),
        ("Song 02", 233, "Techno"),
        ("Song 03", 108, "Disco"),
        ("Song 04", 281, "Disco"),
        ("Song 05", 129, "Techno"),
        ("Song 06", 122, "Techno"),
        ("Song 07", 244, "Pop"),
        ("Song 08", 178, "Techno"),
        ("Song 09", 213, "Techno"),
        ("Song 10", 124, "Rock"),
        ("Song 11", 120, "Disco"),
        ("Song 12", 196, "Rock"),
        ("Song 13", 249, "Disco"),
        ("Song 14", 294, "Disco"),
        ("Song 15", 103, "Techno"),
        ("Song 16", 179, "Disco"),
        ("Song 17", 146, "Disco"),
        ("Song 18", 126, "Techno"),
        ("Song 19", 100, "Pop"),
        ("Song 20", 122, "Disco"),
        ("Song 21", 190, "Disco"),
        ("Song 22", 181, "Techno"),
        ("Song 23", 273, "Pop"),
        ("Song 24", 121, "Disco"),
        ("Song 25", 159, "Pop"),
        ("Song 26", 234, "Rock"),
        ("Song 27", 169, "Rock"),
        ("Song 28", 151, "Rock"),
        ("Song 29", 142, "Techno"),
        ("Song 30", 245, "Pop"),
        ("Song 31", 281, "Techno"),
        ("Song 32", 154, "Rock"),
        ("Song 33", 148, "Disco"),
        ("Song 34", 120, "Pop"),
        ("Song 35", 163, "Disco"),
        ("Song 36", 158, "Pop"),
        ("Song 37", 235, "Rock"),
        ("Song 38", 106, "Techno"),
        ("Song 39", 117, "Disco"),
        ("Song 40", 110, "Pop"),
        ("Song 41", 144, "Rock"),
        ("Song 42", 156, "Disco"),
        ("Song 43", 204, "Rock"),
        ("Song 44", 108, "Pop"),
        ("Song 45", 255, "Pop"),
        ("Song 46", 165, "Rock"),
        ("Song 47", 290, "Disco"),
        ("Song 48", 242, "Pop"),
        ("Song 49", 272, "Rock"),
        ("Song 50", 212, "Pop"),
    ]

    # Genre transition costs. A higher cost means a less desirable transition.
    genre_transition_costs = {
        "Rock": {"Pop": 3, "Disco": 5, "Techno": 7},
        "Pop": {"Rock": 3, "Disco": 6, "Techno": 8},
        "Disco": {"Rock": 5, "Pop": 6, "Techno": 9},
        "Techno": {"Rock": 7, "Pop": 8, "Disco": 9},
    }

    num_tunes = len(tunes)
    all_tunes = range(num_tunes)

    # Playlist target duration in seconds.
    target_duration = 60 * 60  # 1 hour

    # We use a circuit constraint to model the playlist. In the circuit constraint
    # graph, each node is a tune, and each arc represents a pair of consecutive
    # tunes in the playlist. We introduce a dummy node to represent the start and
    # the end of the playlist.
    #
    # The constraint that two consecutive tunes must be of different genres is
    # encoded by not creating an arc between two tunes that are of the same genre.
    # This is crucial in the modelisation of this problem: it reduces the number
    # of variables in the model, and it avoids additional constraints to ensure
    # two consecutive tunes are of different genres.

    # Dummy node representing the start and end of the playlist.
    dummy_node = num_tunes

    # `possible_successors[i]` contains the list of nodes that can be reached
    # after node `i`.
    possible_successors = {}
    possible_successors[dummy_node] = [dummy_node]
    for i in all_tunes:
        # Any node can be the first tune in the playlist.
        possible_successors[dummy_node].append(i)
        # Any node can be the last tune in the playlist.
        possible_successors[i] = [dummy_node]
        genre_i = tunes[i][2]
        for j in all_tunes:
            genre_j = tunes[j][2]
            # If `i` and `j` are of different genres, we can go from `i` to `j`.
            if genre_i != genre_j:
                possible_successors[i].append(j)

    # --------------------
    # 2. Model
    # --------------------
    model = cp_model.CpModel()

    # --------------------
    # 3. Decision Variables
    # --------------------
    # `literals[i][j]` is true if tune `j` follows tune `i` in the playlist.
    literals = {}

    # --------------------
    # 4. Constraints
    # --------------------

    # 4.1 Two consecutive tunes must be of different genres.
    # This is encoded in possible_successors, which doesn't contain any arcs
    # between two tunes that are of the same genre. Now we just have to add a
    # circuit constraint.

    # `arcs` contains the list of possible arcs in the circuit graph, each arc
    # is a tuple (i, j, literals[i][j]).
    arcs = []

    def AddArc(i, j):
        literals[(i, j)] = model.new_bool_var(f"lit_{i}_{j}")
        arcs.append((i, j, literals[(i, j)]))

    # Add all possible arcs between different nodes.
    for i, successors in possible_successors.items():
        for j in successors:
            AddArc(i, j)

    # Add self-arcs to let tunes not be in the playlist.
    for i in all_tunes:
        AddArc(i, i)

    # Add a circuit constraint with the arcs.
    model.add_circuit(arcs)

    # 4.2  All genres must appear at least once.
    # This is encoded by adding a constraint that the sum of all literals for a
    # given genre is at least 1.

    # `is_active[i]` is true iff tune `i` is in the playlist, i.e. if its self-arc
    # is not active in the circuit.
    is_active = {}
    for i in all_tunes:
        is_active[i] = literals[(i, i)].Not()

    # `genre_tunes[genre]` contains the list of tunes that are of genre `genre`.
    genre_tunes = {}
    for genre in genre_transition_costs:
        genre_tunes[genre] = []
    for i in all_tunes:
        genre_tunes[tunes[i][2]].append(i)

    # For each genre, at least one tune must be active: the sum of all literals
    # for this genre is at least 1.
    for t in genre_tunes.values():
        model.add(sum(is_active[i] for i in t) >= 1)

    # --------------------
    # 5. Objective
    # --------------------

    # 5.1. Minimize genre transition costs.

    # Add a total_transition_cost variable representing the sum of all transition
    # costs in the playlist.
    max_transition_cost = 0
    for genre_costs in genre_transition_costs.values():
        for cost in genre_costs.values():
            max_transition_cost = max(cost, max_transition_cost)
    total_transition_cost_upper_bound = (num_tunes - 1) * max_transition_cost
    total_transition_cost = model.new_int_var(
        0, total_transition_cost_upper_bound, "total_transition_cost"
    )

    transition_cost_terms = []
    for i, successors in possible_successors.items():
        if i == dummy_node:
            continue
        genre_i = tunes[i][2]
        for j in successors:
            if j == dummy_node:
                continue
            genre_j = tunes[j][2]
            cost = genre_transition_costs[genre_i][genre_j]
            transition_cost_terms.append(cost * literals[(i, j)])
    model.add(total_transition_cost == sum(transition_cost_terms))

    # 5.2. Minimize the deviation between the target duration and the actual total
    # duration.

    # Add a total_duration variable representing the duration of all active tunes.
    total_duration_upper_bound = sum([t[1] for t in tunes])
    total_duration = model.new_int_var(0, total_duration_upper_bound, "total_duration")
    model.add(total_duration == sum(tunes[i][1] * is_active[i] for i in all_tunes))

    # Minimize the absolute difference from the target duration.
    deviation = model.new_int_var(0, target_duration, "deviation")
    model.add_abs_equality(deviation, total_duration - target_duration)

    # 5.3 Combine the objectives.
    #
    # You can add a weight to prioritize one over the other.
    # For example, `model.minimize(10 * total_transition_cost + deviation)`
    model.minimize(total_transition_cost + deviation)

    # --------------------
    # 6. Solve
    # --------------------
    solver = cp_model.CpSolver()
    # Set a time limit for the solver
    solver.parameters.max_time_in_seconds = 30.0
    status = solver.solve(model)

    # -----------------------
    # 7. Print the solution
    # -----------------------
    if status == cp_model.OPTIMAL:
        print("Found Optimal Playlist:")
    elif status == cp_model.FEASIBLE:
        print("Found Feasible Playlist:")
    else:
        print("No solution found.")
        return

    print(f"  Total Transition Cost: {solver.value(total_transition_cost)}")
    print(
        f"  Playlist Duration: {solver.value(total_duration)} seconds "
        f"({solver.value(total_duration) / 60:.2f} minutes)"
    )
    print(
        f"  Deviation from target duration ({target_duration}):"
        f" {solver.value(deviation)} seconds"
    )
    print("-" * 30)

    # Reconstruct the playlist sequence by starting from the dummy node.
    playlist = []
    current_node = dummy_node
    while True:
        # Find the successor of the current node.
        next_node = dummy_node
        for next_node in possible_successors[current_node]:
            if solver.value(literals[(current_node, next_node)]):
                break

        if next_node == dummy_node:
            break  # We've completed the loop back to the start.

        playlist.append(next_node)
        current_node = next_node

    if not playlist:
        print("Empty playlist.")
    else:
        for i in playlist:
            (name, duration, genre) = tunes[i]
            print(f"{i+1}. {name} ({genre}) - {duration}s")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    Solve()


if __name__ == "__main__":
    app.run(main)
