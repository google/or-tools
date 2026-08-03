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

"""Global optimisation of a gradient-boosted tree ensemble with CP-SAT.

A gradient-boosted ensemble predicts a scalar outcome as the sum of leaf
values, one per tree.  When the input variables are discrete (integer or
categorical), this is a piecewise-constant function over a finite domain.
Misic (2020) showed that the exact global optimum can be found by encoding
the ensemble as a 0-1 integer program and solving with a MIP/CP solver.

Reference
---------
Misic, V.V. (2020).  Optimization of Tree Ensembles.
Operations Research, 68(5), 1605–1624.
https://doi.org/10.1287/opre.2019.1928

Encoding
--------
For a single tree the encoding uses two families of binary variables:

  z[f][k]   – one-hot selector: z[f][k] = 1  iff tunable feature f takes its
              k-th candidate value.  Exactly one variable per feature is 1.

  leaf[t][l] – leaf indicator: leaf[t][l] = 1 iff leaf l of tree t is active.
              Exactly one leaf per tree is active.

Linking constraint (Proposition 1 in Misic 2020):
  leaf[t][l] <= sum(z[f][k] for k in allowed[f][l])   for each tunable f

where allowed[f][l] is the set of candidate indices consistent with every
split on feature f on the root-to-leaf path of leaf l.  This is a necessary
and sufficient condition because one-hot constraints on z already enforce
that exactly one candidate is active for every feature.

The objective is:
  Minimise  sum_t  sum_l  leaf_value[t][l] * leaf[t][l]

This example demonstrates the encoding on a small synthetic surrogate model
with two trees and three tunable setpoints (36 feasible combinations total),
then verifies the CP-SAT result against exhaustive brute-force search.

Usage
-----
    python tree_ensemble_optimization_sat.py
"""

import itertools
from typing import Any

from absl import app

from ortools.sat.python import cp_model

# ---------------------------------------------------------------------------
# Synthetic surrogate model (two trees, three integer setpoints)
# ---------------------------------------------------------------------------
# In practice these come from model.get_booster().trees_to_dataframe() or
# an equivalent sklearn/LightGBM export.  Here they are hard-coded so the
# example has no external ML-library dependency.
#
# Each tree is a nested dict: {"feature", "threshold", "left", "right"}
# where "left" / "right" are either a subtree dict or a float leaf value.

_TREES = [
    {  # Tree 0 — splits on setpoint_a and setpoint_b
        "feature": "setpoint_a",
        "threshold": 25.0,
        "left": {
            "feature": "setpoint_b",
            "threshold": 2.5,
            "left": -4.5,
            "right": 2.0,
        },
        "right": {
            "feature": "setpoint_b",
            "threshold": 3.5,
            "left": -1.0,
            "right": -3.0,
        },
    },
    {  # Tree 1 — splits on setpoint_c and setpoint_a
        "feature": "setpoint_c",
        "threshold": 1.5,
        "left": {
            "feature": "setpoint_a",
            "threshold": 15.0,
            "left": -3.0,
            "right": 1.0,
        },
        "right": {
            "feature": "setpoint_a",
            "threshold": 25.0,
            "left": -2.0,
            "right": -1.5,
        },
    },
]

# Tunable setpoints and their candidate values
_CANDIDATES: dict[str, list[float]] = {
    "setpoint_a": [10.0, 20.0, 30.0],
    "setpoint_b": [1.0, 2.0, 3.0, 4.0],
    "setpoint_c": [0.0, 1.0, 2.0],
}

# Scaling factor to convert float leaf values to integers for CP-SAT
_SCALE = 10**6


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _leaf_paths(
    node: dict[str, Any] | float,
    path: list[tuple[str, float, bool]] | None = None,
) -> list[tuple[float, list[tuple[str, float, bool]]]]:
    """Enumerate (leaf_value, root-to-leaf conditions) for every leaf.

    Each condition is (feature, threshold, went_left) where went_left=True
    means the split sent us left, i.e. feature_value < threshold.

    Args:
      node: The tree, or subtree, to enumerate.
      path: The path from the root to this node.

    Returns:
      List of (leaf_value, root-to-leaf conditions) tuples.
    """
    if path is None:
        path = []
    if isinstance(node, (int, float)):
        return [(float(node), list(path))]
    results = []
    results.extend(
        _leaf_paths(node["left"], path + [(node["feature"], node["threshold"], True)])
    )
    results.extend(
        _leaf_paths(node["right"], path + [(node["feature"], node["threshold"], False)])
    )
    return results


def _predict(
    candidates: dict[str, float],
    trees: list[dict[str, Any] | float],
) -> float:
    """Evaluate the ensemble at a single point (brute-force reference)."""
    total = 0.0
    for tree in trees:
        node = tree
        while isinstance(node, dict):
            feat_val = candidates[node["feature"]]
            node = node["left"] if feat_val < node["threshold"] else node["right"]
        total += float(node)
    return total


# ---------------------------------------------------------------------------
# CP-SAT encoding
# ---------------------------------------------------------------------------


def solve_with_cpsat(
    trees: list[dict[str, Any] | float],
    candidates: dict[str, list[float]],
) -> tuple[dict[str, float], float, str]:
    """Encode the tree ensemble as a CP-SAT model and solve to global optimality.

    Args:
      trees: List of tree dicts (or scalar leaf values for depth-0 stumps).
      candidates: Mapping from feature name to the ordered list of candidate
        values.

    Returns:
      A tuple of (best_point, best_obj, status) containing:
        best_point: Optimal assignment {feature: value}.
        best_obj: Predicted objective at the optimal point (true float, not
          scaled).
        status: CP-SAT solver status string.
    """
    model = cp_model.CpModel()
    features = list(candidates)

    # One-hot variables: z[f][k] = 1  iff feature f takes its k-th candidate
    z: dict[str, list[cp_model.IntVar]] = {
        f: [model.new_bool_var(f"z_{f}_{k}") for k in range(len(candidates[f]))]
        for f in features
    }
    for f in features:
        model.add_exactly_one(z[f])

    obj_terms: list[Any] = []

    for tree_idx, tree in enumerate(trees):
        leaf_paths = _leaf_paths(tree)
        leaf_vars: list[cp_model.IntVar] = []

        for leaf_idx, (leaf_val, path) in enumerate(leaf_paths):
            # Compute, per tunable feature, which candidate indices are
            # compatible with every split on this root-to-leaf path.
            allowed: dict[str, set[int]] = {
                f: set(range(len(candidates[f]))) for f in features
            }
            feasible = True
            for feat, thr, went_left in path:
                if feat not in allowed:
                    continue  # fixed (non-tunable) feature — skip
                allowed[feat] &= {
                    k for k, v in enumerate(candidates[feat]) if (v < thr) == went_left
                }
                if not allowed[feat]:
                    feasible = False
                    break

            if not feasible:
                # No candidate combination can activate this leaf.
                continue

            lv = model.new_bool_var(f"leaf_{tree_idx}_{leaf_idx}")
            leaf_vars.append(lv)

            # Linking: leaf active => each feature selects an allowed candidate
            for f in features:
                if allowed[f] != set(range(len(candidates[f]))):
                    # At least one candidate is excluded for this feature.
                    model.add(lv <= sum(z[f][k] for k in allowed[f]))

            obj_terms.append(int(round(leaf_val * _SCALE)) * lv)

        # Exactly one leaf per tree must be active.
        model.add_exactly_one(leaf_vars)

    model.minimize(sum(obj_terms))

    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = False
    status_code = solver.solve(model)
    status = solver.status_name(status_code)

    if status_code not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        return {}, float("inf"), status

    best_point = {
        f: candidates[f][
            next(k for k in range(len(candidates[f])) if solver.boolean_value(z[f][k]))
        ]
        for f in features
    }
    best_obj = _predict(best_point, trees)
    return best_point, best_obj, status


# ---------------------------------------------------------------------------
# Brute-force reference
# ---------------------------------------------------------------------------


def solve_brute_force(
    trees: list[dict[str, Any] | float],
    candidates: dict[str, list[float]],
) -> tuple[dict[str, float], float]:
    """Enumerate all candidate combinations and return the global optimum."""
    features = list(candidates)
    best_obj = float("inf")
    best_point: dict[str, float] = {}
    for values in itertools.product(*candidates.values()):
        point = dict(zip(features, values))
        obj = _predict(point, trees)
        if obj < best_obj:
            best_obj = obj
            best_point = point
    return best_point, best_obj


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main(_):
    n_combos = 1
    for vals in _CANDIDATES.values():
        n_combos *= len(vals)

    print(
        "Tree ensemble optimisation via CP-SAT\n"
        f"  Trees: {len(_TREES)}, features: {list(_CANDIDATES)}\n"
        f"  Search space: {n_combos} combinations\n"
    )

    # CP-SAT exact solve
    cp_point, cp_obj, status = solve_with_cpsat(_TREES, _CANDIDATES)
    print(f"CP-SAT  [{status}]")
    for f, v in cp_point.items():
        print(f"  {f} = {v}")
    print(f"  objective = {cp_obj:.6f}")

    # Brute-force verification
    bf_point, bf_obj = solve_brute_force(_TREES, _CANDIDATES)
    print("\nBrute-force")
    for f, v in bf_point.items():
        print(f"  {f} = {v}")
    print(f"  objective = {bf_obj:.6f}")

    # Verify
    tol = 1e-4
    assert (
        abs(cp_obj - bf_obj) < tol
    ), f"CP-SAT objective {cp_obj:.6f} differs from brute-force {bf_obj:.6f}"
    print(f"\nVerification passed: CP-SAT matches brute-force (tol={tol})")


if __name__ == "__main__":
    app.run(main)
