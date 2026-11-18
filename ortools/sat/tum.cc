// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/sat/tum.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/base/types.h"
#include "ortools/graph/connected_components.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::kInfinity;
using glop::RowIndex;

using operations_research::MPConstraintProto;
using operations_research::MPVariableProto;

std::vector<int> PartitionGraph(
    int n, absl::Span<const EdgeConstraintForPartition> edges) {
  struct ArConstraintForPartition {
    int target;
    bool same_partition;
  };
  CompactVectorVectorBuilder<int, ArConstraintForPartition> graph_builder;
  for (const EdgeConstraintForPartition& edge : edges) {
    DCHECK_GE(edge.a, 0);
    DCHECK_LT(edge.a, n);
    DCHECK_GE(edge.b, 0);
    DCHECK_LT(edge.b, n);
    graph_builder.Add(edge.a,
                      ArConstraintForPartition{edge.b, edge.same_partition});
    graph_builder.Add(edge.b,
                      ArConstraintForPartition{edge.a, edge.same_partition});
  }
  const CompactVectorVector<int, ArConstraintForPartition> adj(graph_builder,
                                                               n);

  // -2: unvisited, -1: failed component, 0: partition R1, 1: partition R2.
  std::vector<int> partition(n, -2);
  std::vector<int> bfs_queue;

  for (int start_node = 0; start_node < n; ++start_node) {
    if (partition[start_node] != -2) continue;
    if (adj[start_node].empty()) {
      partition[start_node] = 0;
      continue;
    }

    int head = 0;
    bool conflict = false;
    bfs_queue.clear();
    bfs_queue.push_back(start_node);
    partition[start_node] = 0;

    while (head < bfs_queue.size()) {
      const int u = bfs_queue[head++];
      const int color_u = partition[u];

      for (const ArConstraintForPartition& edge : adj[u]) {
        const int v = edge.target;
        const int expected_color_v =
            edge.same_partition ? color_u : (1 - color_u);
        if (partition[v] == -2) {
          partition[v] = expected_color_v;
          bfs_queue.push_back(v);
        } else if (partition[v] != expected_color_v) {
          conflict = true;
        }
      }
    }

    if (conflict) {
      for (const int u : bfs_queue) {
        partition[u] = -1;
      }
    }
  }

  return partition;
}

namespace {

// Iteratively removes rows and columns with at most 1 active entry.
// `row_matrix` must be the transpose of `col_matrix`.
void PeelSingletons(const glop::CompactSparseMatrix& col_matrix,
                    const glop::CompactSparseMatrix& row_matrix,
                    std::vector<bool>* col_deleted,
                    std::vector<bool>* row_deleted) {
  const int num_cols = col_matrix.num_cols().value();
  const int num_rows = col_matrix.num_rows().value();
  col_deleted->assign(num_cols, false);
  row_deleted->assign(num_rows, false);

  std::vector<int> col_degree(num_cols, 0);
  std::vector<int> row_degree(num_rows, 0);
  std::vector<int> cols_to_peel;
  std::vector<int> rows_to_peel;

  for (int col = 0; col < num_cols; ++col) {
    col_degree[col] = col_matrix.ColumnNumEntries(ColIndex(col)).value();
    if (col_degree[col] <= 1) {
      (*col_deleted)[col] = true;
      cols_to_peel.push_back(col);
    }
  }
  for (int row = 0; row < num_rows; ++row) {
    row_degree[row] = row_matrix.ColumnNumEntries(ColIndex(row)).value();
    if (row_degree[row] <= 1) {
      (*row_deleted)[row] = true;
      rows_to_peel.push_back(row);
    }
  }

  while (!cols_to_peel.empty() || !rows_to_peel.empty()) {
    while (!rows_to_peel.empty()) {
      const int row = rows_to_peel.back();
      rows_to_peel.pop_back();
      for (const auto e : row_matrix.column(ColIndex(row))) {
        const int col = e.row().value();
        if (!(*col_deleted)[col] && --col_degree[col] <= 1) {
          (*col_deleted)[col] = true;
          cols_to_peel.push_back(col);
        }
      }
    }
    while (!cols_to_peel.empty()) {
      const int col = cols_to_peel.back();
      cols_to_peel.pop_back();
      for (const auto e : col_matrix.column(ColIndex(col))) {
        const int row = e.row().value();
        if (!(*row_deleted)[row] && --row_degree[row] <= 1) {
          (*row_deleted)[row] = true;
          rows_to_peel.push_back(row);
        }
      }
    }
  }
}

// Attempts to find positive `col_scale` (with min(col_scale) == 1.0) and
// `row_scale` such that for every entry (r, c) with value `coeff`,
// |coeff| == row_scale[r] * col_scale[c].
// `row_matrix` must be the transpose of `col_matrix`, and the bipartite graph
// must be connected when `num_rows > 0`.
bool ComputeRowAndColScales(const glop::CompactSparseMatrix& col_matrix,
                            const glop::CompactSparseMatrix& row_matrix,
                            double tolerance, std::vector<double>* col_scale,
                            std::vector<double>* row_scale) {
  const int num_cols = col_matrix.num_cols().value();
  const int num_rows = col_matrix.num_rows().value();
  col_scale->assign(num_cols, 0.0);
  row_scale->assign(num_rows, 0.0);
  if (num_rows == 0) {
    col_scale->assign(num_cols, 1.0);
    return true;
  }

  std::vector<int> col_queue;
  std::vector<int> row_queue;
  (*col_scale)[0] = 1.0;
  col_queue.push_back(0);
  double min_col_scale = 1.0;

  while (!col_queue.empty() || !row_queue.empty()) {
    while (!col_queue.empty()) {
      const int col = col_queue.back();
      col_queue.pop_back();
      const double c_scale = (*col_scale)[col];
      for (const auto e : col_matrix.column(ColIndex(col))) {
        const int row = e.row().value();
        const double abs_coeff = std::abs(e.coefficient());
        if ((*row_scale)[row] == 0.0) {
          const double r_scale = abs_coeff / c_scale;
          if (r_scale <= 0.0 || !std::isfinite(r_scale)) return false;
          (*row_scale)[row] = r_scale;
          row_queue.push_back(row);
        } else if (std::abs((*row_scale)[row] * c_scale - abs_coeff) >
                   tolerance * abs_coeff) {
          return false;
        }
      }
    }
    while (!row_queue.empty()) {
      const int row = row_queue.back();
      row_queue.pop_back();
      const double r_scale = (*row_scale)[row];
      for (const auto e : row_matrix.column(ColIndex(row))) {
        const int col = e.row().value();
        const double abs_coeff = std::abs(e.coefficient());
        if ((*col_scale)[col] == 0.0) {
          const double c_scale = abs_coeff / r_scale;
          if (c_scale <= 0.0 || !std::isfinite(c_scale)) return false;
          (*col_scale)[col] = c_scale;
          min_col_scale = std::min(min_col_scale, c_scale);
          col_queue.push_back(col);
        } else if (std::abs(r_scale * (*col_scale)[col] - abs_coeff) >
                   tolerance * abs_coeff) {
          return false;
        }
      }
    }
  }

  if (min_col_scale != 1.0) {
    for (double& c_scale : *col_scale) {
      c_scale /= min_col_scale;
      if (c_scale <= 0.0 || !std::isfinite(c_scale)) return false;
    }
    for (double& r_scale : *row_scale) {
      r_scale *= min_col_scale;
      if (r_scale <= 0.0 || !std::isfinite(r_scale)) return false;
    }
  }

  // Fail if the bipartite graph was not connected.
  for (const double c_scale : *col_scale) {
    if (c_scale == 0.0) return false;
  }
  for (const double r_scale : *row_scale) {
    if (r_scale == 0.0) return false;
  }
  return true;
}

enum class ModularityStatus {
  kNotTwoModular = 0,
  kTotallyUnimodular = 1,
  kTwoModular = 2,
};

// Checks Heller & Tompkins's criterion on the active (unpeeled) entries of
// `matrix`, assuming all entries have been validated to scale to {-1, +1} and
// every active column has at least 2 active entries.
ModularityStatus CheckHellerTompkins(const glop::CompactSparseMatrix& matrix,
                                     const std::vector<bool>& col_deleted,
                                     const std::vector<bool>& row_deleted) {
  std::vector<EdgeConstraintForPartition> edges;
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    if (col_deleted[col.value()]) continue;
    int rows[2];
    double vals[2];
    int size = 0;
    for (const auto e : matrix.column(col)) {
      if (row_deleted[e.row().value()]) continue;
      if (size == 2) return ModularityStatus::kNotTwoModular;
      rows[size] = e.row().value();
      vals[size] = e.coefficient();
      ++size;
    }
    if (size == 2) {
      // Same sign: must be in different subsets.
      // Opposite signs: must be in the same subset.
      edges.push_back({rows[0], rows[1],
                       /*same_partition=*/(vals[0] > 0.0) != (vals[1] > 0.0)});
    }
  }
  if (edges.empty()) return ModularityStatus::kTotallyUnimodular;

  const std::vector<int> partition =
      PartitionGraph(matrix.num_rows().value(), edges);
  for (const EdgeConstraintForPartition& edge : edges) {
    if (partition[edge.a] == -1) return ModularityStatus::kTwoModular;
  }
  return ModularityStatus::kTotallyUnimodular;
}

ModularityStatus CheckComponentModularity(
    const glop::CompactSparseMatrix& col_matrix,
    const glop::CompactSparseMatrix& row_matrix) {
  std::vector<bool> col_deleted;
  std::vector<bool> row_deleted;
  PeelSingletons(col_matrix, row_matrix, &col_deleted, &row_deleted);
  const ModularityStatus col_status =
      CheckHellerTompkins(col_matrix, col_deleted, row_deleted);
  if (col_status == ModularityStatus::kTotallyUnimodular) {
    return ModularityStatus::kTotallyUnimodular;
  }
  const ModularityStatus row_status =
      CheckHellerTompkins(row_matrix, row_deleted, col_deleted);
  if (row_status == ModularityStatus::kTotallyUnimodular) {
    return ModularityStatus::kTotallyUnimodular;
  }
  if (col_status == ModularityStatus::kTwoModular ||
      row_status == ModularityStatus::kTwoModular) {
    return ModularityStatus::kTwoModular;
  }
  return ModularityStatus::kNotTwoModular;
}

// Tightens variable bounds in a TUM (`base_factor = 1.0`) or 2-modular
// (`base_factor = 2.0`) component using Cramer's rule.
//
// The bounds computed here are valid for the vertices of the component LP. If
// the component contains integer variables, the caller must have checked that
// these vertices are integral for every integer assignment of the external
// variables (Hoffman-Kruskal), otherwise integer feasible solutions might be
// outside the bounds computed here.
//
// In scaled coordinates `\tilde{x}_j = col_scale[j] * x_j` and row-scaled
// constraints `\tilde{A}_{r, j} = A_{r, j} / (row_scale[r] * col_scale[j])`,
// the component matrix `\tilde{A}` has entries in `{-1, 0, +1}`.
//
// For any fixed assignment `z*` of the external variables, consider a basic
// optimal solution (vertex) of the component LP:
// - A subset `C_B` of `k` basic variables is uniquely determined by `k` active
//   rows `R_B` (where `\tilde{A}_{R_B, C_B}` is `k x k` non-singular):
//     `\tilde{x}_{C_B} = \tilde{A}_{R_B, C_B}^{-1} (b'_{R_B} - \tilde{A}_{R_B,
//     C_N} \tilde{x}_{C_N})`
// - The remaining non-basic variables `C_N` sit at one of their finite bounds
//   (or `0` along free directions in the lineality space).
// - By Cramer's rule, because every nonsingular submatrix of `\tilde{A}` has
//   determinant `\pm 1` (or `\pm 1, \pm 2` when 2-modular), every entry of
//   `\tilde{A}_{R_B, C_B}^{-1}` and `\tilde{A}_{R_B, C_B}^{-1} \tilde{A}_{R_B,
//   C_N}` has magnitude at most `base_factor`.
// - By the triangle inequality, every coordinate of the vertex satisfies:
//     `|\tilde{x}_j| <= base_factor * (\sum_{r in R_B} M_row(r)
//                                      + \sum_{j' in C_N} M_col(j'))`
//   where `M_row(r)` is the maximum finite magnitude of the effective RHS
//   `b'_r = (b_r - A_{r, ext} z*) / row_scale[r]`, and `M_col(j')` is the
//   maximum finite bound magnitude of variable `j'`. Since `|R_B| + |C_N| = n`,
//   this is at most `base_factor` times the sum of the `n` largest magnitudes.
// Bounds of a variable expressed in the wrapper space (see MipVariableState).
// Used for the external variables of a component, whose coefficients are read
// from the (untransformed) model. Note that this is exact for integer
// variables, which are the only external variables we have in practice.
std::pair<double, double> WrapperSpaceBounds(const MipVariableState& state) {
  return {state.lb / state.scaling + state.shift,
          state.ub / state.scaling + state.shift};
}

void TightenComponentBounds(
    absl::Span<const int> compo_vars, absl::Span<const int> compo_rows,
    absl::Span<const double> col_scale, absl::Span<const double> row_scale,
    absl::Span<const int> var_to_local_col, double base_factor,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    TumStats* stats) {
  if (compo_rows.empty()) return;

  // Some half-spaces (variable bounds or constraint sides) can be relaxed
  // (dropped) without changing the optimal value as long as they contain the
  // vertex box `[-L_low, L_low]^n` of the relaxed polyhedron `P_low`.
  //
  // Specifically, in a minimization problem with objective `c_j`, provided
  // every variable in the component is bounded in its improving direction
  // (`\tilde{l}_j > -inf` whenever `c_j > 0`, and `\tilde{u}_j < +inf` whenever
  // `c_j < 0`) and these bounds are kept, any recession direction `d` of
  // `P_low` satisfies `d_j >= 0` whenever `c_j > 0` and `d_j <= 0` whenever
  // `c_j < 0`, so `c^T d >= 0`. Thus `P_low` has no improving recession rays,
  // and its minimum is still achieved at a basic solution `v*` of `P_low` with
  // `|\tilde{v}^*_j| <= L_low`. As long as every dropped half-space contains
  // `[-L_low, +L_low]^n`, `v*` automatically satisfies it and is optimal for
  // the original polyhedron.
  //
  // Note that the wrapper always holds a minimization problem.
  bool can_drop = true;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const int var = compo_vars[local_col];
    const double obj = mp_model.variables(var).objective_coefficient();
    if ((obj > 0.0 && !std::isfinite(states[var].lb)) ||
        (obj < 0.0 && !std::isfinite(states[var].ub))) {
      can_drop = false;
      break;
    }
  }

  // A droppable half-space is described by the largest vertex limit
  // `drop_limit` such that `[-drop_limit, +drop_limit]^n` is contained in it,
  // and by the increase `delta` of the magnitude of its row or column `item`
  // if it must be kept.
  struct DroppableHalfSpace {
    double drop_limit;
    double delta;
    int item;
    bool operator<(const DroppableHalfSpace& other) const {
      return drop_limit < other.drop_limit;
    }
  };
  std::vector<DroppableHalfSpace> droppable;

  // Magnitudes `M_row(r)` of the rows followed by `M_col(j)` of the columns,
  // assuming all droppable half-spaces are dropped.
  const int num_cols = compo_vars.size();
  std::vector<double> magnitude(compo_rows.size() + num_cols, 0.0);
  const auto add_to_magnitude = [&magnitude](int item, double value) {
    magnitude[item] =
        TightSum().Add(magnitude[item]).Add(value).TightUpperBound();
  };

  // 1. Rows.
  //
  // If any connected external variable has an infinite bound, the effective RHS
  // `b_r - A_{r, ext} z*` cannot be bounded across all feasible `z*`, so we
  // abort.
  TightSum min_activity;
  TightSum max_activity;
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const MPConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]).proto();
    const double r_scale = row_scale[local_row];
    min_activity.Clear();
    max_activity.Clear();
    int row_nnz = 0;
    bool all_external_integer = true;
    double min_external_step = std::numeric_limits<double>::infinity();
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int var = constraint.var_index(i);
      if (var_to_local_col[var] != -1) {
        ++row_nnz;
        continue;
      }
      const MipVariableState& external = states[var];
      const auto [var_lb, var_ub] = WrapperSpaceBounds(external);
      if (!std::isfinite(var_lb) || !std::isfinite(var_ub)) return;
      if (coeff > 0.0) {
        min_activity.AddProd(coeff, var_lb);
        max_activity.AddProd(coeff, var_ub);
      } else {
        min_activity.AddProd(coeff, var_ub);
        max_activity.AddProd(coeff, var_lb);
      }
      if (!external.is_integer) all_external_integer = false;
      min_external_step = std::min(
          min_external_step, ExactScaledSumLb(std::abs(coeff), 0.0, r_scale));
    }
    const double min_ext_activity = min_activity.TightLowerBound();
    const double max_ext_activity = max_activity.TightUpperBound();
    if (!std::isfinite(min_ext_activity) || !std::isfinite(max_ext_activity)) {
      return;
    }

    const bool has_lb = std::isfinite(constraint.lower_bound());
    const bool has_ub = std::isfinite(constraint.upper_bound());
    if (!can_drop || has_lb == has_ub) {
      // Keep both sides: `M_row(r)` is the maximum finite magnitude of the
      // effective RHS.
      double max_of_finite_rhs_magnitude = 0.0;
      if (has_lb) {
        max_of_finite_rhs_magnitude =
            std::max({max_of_finite_rhs_magnitude,
                      std::abs(ExactScaledSumLb(constraint.lower_bound(),
                                                -max_ext_activity, r_scale)),
                      std::abs(ExactScaledSumUb(constraint.lower_bound(),
                                                -min_ext_activity, r_scale))});
      }
      if (has_ub) {
        max_of_finite_rhs_magnitude =
            std::max({max_of_finite_rhs_magnitude,
                      std::abs(ExactScaledSumLb(constraint.upper_bound(),
                                                -max_ext_activity, r_scale)),
                      std::abs(ExactScaledSumUb(constraint.upper_bound(),
                                                -min_ext_activity, r_scale))});
      }
      if (!std::isfinite(max_of_finite_rhs_magnitude)) return;
      magnitude[local_row] = max_of_finite_rhs_magnitude;
      continue;
    }

    // One-sided row, written as `\tilde{a}_r \tilde{x} >= rhs(z)` (negating
    // the row for an upper bound). `tightest` is an upper bound on the largest
    // value of `rhs(z)` over the external domain, and `loosest` a lower bound
    // on its smallest value. Since `\tilde{a}_r` has `row_nnz` entries in
    // `{-1, +1}`, the half-space `\tilde{a}_r \tilde{x} >= rhs` contains the
    // box `[-L, +L]^n` iff `L <= -rhs / row_nnz`.
    double tightest;
    double loosest;
    if (has_lb) {
      tightest = ExactScaledSumUb(constraint.lower_bound(), -min_ext_activity,
                                  r_scale);
      loosest = ExactScaledSumLb(constraint.lower_bound(), -max_ext_activity,
                                 r_scale);
    } else {
      tightest = -ExactScaledSumLb(constraint.upper_bound(), -max_ext_activity,
                                   r_scale);
      loosest = -ExactScaledSumUb(constraint.upper_bound(), -min_ext_activity,
                                  r_scale);
    }
    if (!std::isfinite(tightest) || !std::isfinite(loosest)) return;
    const auto drop_limit_for = [row_nnz](double rhs) {
      return ExactScaledSumLb(-rhs, 0.0, static_cast<double>(row_nnz));
    };

    // When the row is kept only for the assignments `z*` where `rhs(z*)` is
    // the tightest, its contribution to the vertex bound is `|tightest|`.
    const double tightest_magnitude = std::abs(tightest);
    if (tightest < 0.0) {
      droppable.push_back(
          {drop_limit_for(tightest), tightest_magnitude, local_row});
    } else {
      magnitude[local_row] = tightest_magnitude;
    }

    // Otherwise, its contribution is the maximum magnitude over all `z*`.
    //
    // This is where big-M constraints `x - y + M b >= rhs` with a binary `b`
    // shine: when all external variables are integer, any `rhs(z*)` that is not
    // the tightest is at most `second = tightest - min_external_step`, which is
    // very negative, so the half-space is only ever needed with `rhs(z*) ==
    // tightest`.
    const double static_magnitude =
        std::max(tightest_magnitude, std::abs(loosest));
    if (static_magnitude > tightest_magnitude) {
      const double delta = TightSum()
                               .Add(static_magnitude)
                               .Add(-tightest_magnitude)
                               .TightUpperBound();
      const double second = all_external_integer
                                ? ExactScaledSumUb(tightest, -min_external_step)
                                : tightest;
      if (second < 0.0) {
        droppable.push_back({drop_limit_for(second), delta, local_row});
      } else {
        add_to_magnitude(local_row, delta);
      }
    }
  }

  // 2. Variable bounds: `M_col(j)` is the kept bound magnitude, and large
  // bounds in the non-improving direction of each variable are droppable.
  for (int local_col = 0; local_col < num_cols; ++local_col) {
    const int var = compo_vars[local_col];
    const int item = compo_rows.size() + local_col;
    const double c_scale = col_scale[local_col];
    const double raw_lb = states[var].lb;
    const double raw_ub = states[var].ub;
    if (std::isfinite(raw_lb) && std::isfinite(raw_ub)) {
      const double obj = mp_model.variables(var).objective_coefficient();
      if (can_drop && obj >= 0.0 && raw_ub > std::abs(raw_lb)) {
        const double kept_unscaled = std::abs(raw_lb);
        magnitude[item] = ProdBounds(kept_unscaled, c_scale).second;
        const double delta = TightSum()
                                 .AddProd(raw_ub, c_scale)
                                 .AddProd(-kept_unscaled, c_scale)
                                 .TightUpperBound();
        droppable.push_back({ProdBounds(raw_ub, c_scale).first, delta, item});
      } else if (can_drop && obj <= 0.0 && -raw_lb > std::abs(raw_ub)) {
        const double kept_unscaled = std::abs(raw_ub);
        magnitude[item] = ProdBounds(kept_unscaled, c_scale).second;
        const double delta = TightSum()
                                 .AddProd(-raw_lb, c_scale)
                                 .AddProd(-kept_unscaled, c_scale)
                                 .TightUpperBound();
        droppable.push_back({ProdBounds(-raw_lb, c_scale).first, delta, item});
      } else {
        magnitude[item] =
            ProdBounds(std::max(std::abs(raw_lb), std::abs(raw_ub)), c_scale)
                .second;
      }
    } else if (std::isfinite(raw_lb)) {
      magnitude[item] = ProdBounds(std::abs(raw_lb), c_scale).second;
    } else if (std::isfinite(raw_ub)) {
      magnitude[item] = ProdBounds(std::abs(raw_ub), c_scale).second;
    }
  }

  // The vertex limit is `base_factor` times the sum of the `n` largest
  // magnitudes: Cramer's rule involves `k` active rows and `n - k` non-basic
  // variables, i.e. `n` distinct rows or columns in total.
  std::vector<double> sorted_magnitudes;
  const auto compute_vertex_limit = [&]() {
    sorted_magnitudes = magnitude;
    if (sorted_magnitudes.size() > num_cols) {
      std::nth_element(sorted_magnitudes.begin(),
                       sorted_magnitudes.begin() + num_cols,
                       sorted_magnitudes.end(), std::greater<double>());
      sorted_magnitudes.resize(num_cols);
    }
    TightSum sum;
    for (const double m : sorted_magnitudes) sum.Add(m);
    return base_factor * sum.TightUpperBound();
  };

  // 3. Sweep droppable half-spaces from smallest to largest `drop_limit`: any
  // half-space with `drop_limit` smaller than the current vertex limit
  // `scaled_bound_limit` could be hit by a vertex of `P_low` and must be kept,
  // which increases the limit. We iterate until all the remaining half-spaces
  // have a `drop_limit >= scaled_bound_limit` and can be dropped. Note that
  // keeping a half-space is always valid, so to bound the complexity we keep
  // all the remaining ones after a few rounds.
  std::sort(droppable.begin(), droppable.end());
  double scaled_bound_limit = compute_vertex_limit();
  constexpr int kMaxNumRounds = 20;
  for (int round = 0, next = 0; next < droppable.size(); ++round) {
    const int old_next = next;
    while (next < droppable.size() &&
           (round >= kMaxNumRounds ||
            droppable[next].drop_limit < scaled_bound_limit)) {
      if (!std::isfinite(droppable[next].delta)) return;
      add_to_magnitude(droppable[next].item, droppable[next].delta);
      ++next;
    }
    if (next == old_next) break;
    scaled_bound_limit = compute_vertex_limit();
  }
  if (!std::isfinite(scaled_bound_limit)) return;

  // 4. Apply the limit to all variables of the component.
  bool component_touched = false;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const double limit = std::ceil(
        ExactScaledSumUb(scaled_bound_limit, 0.0, col_scale[local_col]));
    if (!std::isfinite(limit)) continue;
    MipVariableState& state = states[compo_vars[local_col]];
    const double lb = state.lb;
    const double ub = state.ub;
    if (lb < -limit || ub > limit) {
      component_touched = true;
      stats->old_min = std::min(stats->old_min, lb);
      stats->old_max = std::max(stats->old_max, ub);
      if (lb < -limit) {
        state.lb = -limit;
        ++stats->num_tightened_bounds;
      }
      if (ub > limit) {
        state.ub = limit;
        ++stats->num_tightened_bounds;
      }
      stats->new_min = std::min(stats->new_min, state.lb);
      stats->new_max = std::max(stats->new_max, state.ub);
    }
  }
  if (component_touched) {
    ++stats->num_bound_tightened_components;
  }
}

// Returns true if, for any integer assignment of the external variables, all
// the vertices of the component LP are integral (Hoffman-Kruskal). This
// requires the scaled matrix to be TUM, the column scaling to be unit so that
// scaled vertices are the actual variable values, and all variable bounds,
// scaled RHS and scaled external coefficients to be integer (so that the
// effective RHS `(b_r - A_{r, ext} z*) / row_scale[r]` is integer).
bool ComponentHasIntegralVertices(const MPModelProtoWrapper& mp_model,
                                  absl::Span<const MipVariableState> states,
                                  absl::Span<const int> compo_vars,
                                  absl::Span<const int> compo_rows,
                                  absl::Span<const double> col_scale,
                                  absl::Span<const double> row_scale,
                                  absl::Span<const int> var_to_local_col,
                                  ModularityStatus status, double tolerance) {
  if (status != ModularityStatus::kTotallyUnimodular) return false;
  const auto is_almost_integer = [tolerance](double x) {
    return !std::isfinite(x) || std::abs(x - std::round(x)) <= tolerance;
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    if (std::abs(col_scale[local_col] - 1.0) > tolerance) return false;
    const MipVariableState& state = states[compo_vars[local_col]];
    if (!is_almost_integer(state.lb) || !is_almost_integer(state.ub)) {
      return false;
    }
  }
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const MPConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]).proto();
    const double r_scale = row_scale[local_row];
    if (!is_almost_integer(constraint.lower_bound() / r_scale) ||
        !is_almost_integer(constraint.upper_bound() / r_scale)) {
      return false;
    }
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int var = constraint.var_index(i);
      if (var_to_local_col[var] != -1) continue;
      if (!states[var].is_integer) return false;
      if (!is_almost_integer(coeff / r_scale)) return false;
    }
  }
  return true;
}

// Tries to make a TUM (`modular_factor = 1`) or 2-modular (`modular_factor =
// 2`) component integral with the rational factor `chosen_s`: each variable is
// scaled by `modular_factor * round(chosen_s * col_scale)` and its bounds are
// rounded to multiples of `modular_factor`, see RoundScaledLowerBound(). Each
// constraint is then scaled by `modular_factor * chosen_s / row_scale`, which
// gives +/-1 coefficients on the component variables and multiples of
// `modular_factor` on the external ones.
//
// Returns false, without changing `states`, if this does not hold, or if the
// scaled constraints could overflow or are not precise enough. Otherwise
// updates `states` and fills the scaling of each constraint.
bool TryScaleModularComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<const double> col_scale,
    absl::Span<const double> row_scale, int64_t modular_factor, double chosen_s,
    absl::Span<MipVariableState> states, TumStats* stats,
    std::vector<ModularConstraintScaling>* row_scalings) {
  const double kMaxVarBound =
      std::nextafter(static_cast<double>(kint64max / 2), 0.0);
  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const bool proper_relaxation = params.mip_proper_relaxation();
  const double wanted_tol = params.mip_wanted_precision();
  // Same tolerances as in ConstraintScaler::ScaleAndAddConstraint().
  const double bound_tol =
      params.mip_use_tight_bounds() ? params.mip_tight_precision() : wanted_tol;
  const double round_tol = proper_relaxation ? bound_tol : wanted_tol;
  const double m = static_cast<double>(modular_factor);

  // Scale the variables, the states are only updated on success.
  absl::Span<const int> compo_vars = compo.continuous_vars;
  std::vector<MipVariableState> new_states(compo_vars.size());
  double max_bound_error = 0.0;
  const auto is_too_large = [kMaxVarBound](double b) {
    return std::isfinite(b) && std::abs(b) > kMaxVarBound;
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const MipVariableState& state = states[compo_vars[local_col]];
    const double s = std::round(chosen_s * col_scale[local_col]);
    if (!(s >= 1.0)) return false;
    const auto round_lb = [&](double lb) {
      if (!std::isfinite(lb)) return lb;
      const double w = RoundScaledLowerBound(ProdBounds(s, lb).first,
                                             s * round_tol, proper_relaxation);
      const double error =
          MaxAbsScaledTightSum(TightSum().Add(w).AddProd(-s, lb), s);
      if (error > bound_tol) max_bound_error = std::max(max_bound_error, error);
      return m * w;
    };
    MipVariableState& new_state = new_states[local_col];
    new_state = state;
    new_state.lb = round_lb(state.lb);
    new_state.ub = -round_lb(-state.ub);
    new_state.is_integer = true;
    new_state.scaling *= m * s;
    new_state.error_lb = 0.0;
    new_state.error_ub = 0.0;
    if (new_state.lb > new_state.ub || new_state.lb == kInfinity ||
        new_state.ub == -kInfinity || is_too_large(new_state.lb) ||
        is_too_large(new_state.ub) || !(new_state.scaling <= kMaxVarBound)) {
      return false;
    }
  }

  // Check the scaled constraints.
  std::vector<ModularConstraintScaling> scalings;
  std::vector<double> coeffs;
  std::vector<double> lbs;
  std::vector<double> ubs;
  for (int local_row = 0; local_row < compo.constraints.size(); ++local_row) {
    const EnforcedLinearConstraint& ct =
        mp_model.constraints(compo.constraints[local_row]);
    const double factor = m * chosen_s / row_scale[local_row];
    if (!(factor > 0.0 && std::isfinite(factor))) return false;
    coeffs.clear();
    lbs.clear();
    ubs.clear();
    LinearOverflowChecker checker;
    for (int i = 0; i < ct.num_terms(); ++i) {
      if (ct.coeffs(i) == 0.0) continue;
      const int local_col = compo.var_to_local_col[ct.vars(i)];
      const MipVariableState& state =
          local_col == -1 ? states[ct.vars(i)] : new_states[local_col];

      // The CP-SAT domain of the variable after the final shift, see
      // FinalCpSatShiftValue() and ConvertVariableToCpVariable(). Note that the
      // external variables are already shifted.
      const double shift =
          state.lb > 0.0 ? state.lb : (state.ub < 0.0 ? state.ub : 0.0);
      const double lb =
          std::ceil(state.lb - shift - std::max(wanted_tol, -state.error_lb));
      const double ub =
          std::floor(state.ub - shift + std::max(wanted_tol, state.error_ub));
      if (lb == 0.0 && ub == 0.0) continue;

      const double coeff = ct.coeffs(i) / state.scaling;
      const double scaled_coeff = coeff * factor;
      if (!(std::abs(scaled_coeff) < static_cast<double>(kint64max))) {
        return false;
      }
      const int64_t int_coeff = std::llround(scaled_coeff);
      if (local_col == -1 ? int_coeff % modular_factor != 0
                          : std::abs(int_coeff) != 1) {
        return false;
      }
      if (!std::isfinite(lb) || !std::isfinite(ub)) {
        // The coefficient of an unbounded variable must be exact.
        if (std::abs(scaled_coeff - static_cast<double>(int_coeff)) >
            1e-12 * std::max(1.0, std::abs(scaled_coeff))) {
          return false;
        }
        continue;
      }
      if (!checker.AddTerm(int_coeff, static_cast<int64_t>(lb),
                           static_cast<int64_t>(ub)) ||
          -checker.sum_min > scaling_target ||
          checker.sum_max > scaling_target) {
        return false;
      }
      coeffs.push_back(coeff);
      lbs.push_back(lb);
      ubs.push_back(ub);
    }
    double relative_coeff_error;
    double scaled_sum_error;
    ComputeScalingErrors(coeffs, lbs, ubs, factor, &relative_coeff_error,
                         &scaled_sum_error);
    if (scaled_sum_error > params.mip_check_precision() * factor) return false;
    scalings.push_back(
        {.scaling_factor = factor, .modular_factor = modular_factor});
  }

  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    states[compo_vars[local_col]] = new_states[local_col];
  }
  stats->num_modular_constraints += scalings.size();
  stats->max_var_bound_error =
      std::max(stats->max_var_bound_error, max_bound_error);
  *row_scalings = std::move(scalings);
  return true;
}

// Scales a continuous TUM component (with unit column scaling and finite
// non-zero max bound) by a common power-of-two factor when
// mip_scale_for_precision is enabled.
void ApplyTumCommonPrecisionScaling(const SatParameters& params,
                                    const ResidualContinuousComponent& compo,
                                    double compo_max_bound,
                                    const MPModelProtoWrapper& mp_model,
                                    absl::Span<MipVariableState> states,
                                    TumStats* stats) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  // Use a higher target bound than mip_max_bound (1e7) so components with
  // large or clamped bounds can still be scaled together, while keeping scaled
  // continuous TUM domains within 1e11 so double-precision LP simplex
  // tolerances (1e-4 in CP space) remain accurate on unit-coefficient rows.
  constexpr double kTumPrecisionMaxBound = 1e11;
  const double max_domain_bound =
      std::min(std::max(params.mip_max_bound(), kTumPrecisionMaxBound),
               std::nextafter(static_cast<double>(kint64max / 2), 0.0));
  const int max_exp =
      MaxPowerOfTwoExponentWithinBound(compo_max_bound, max_domain_bound);
  if (max_exp <= 0) return;
  const double compo_max_scaling = std::ldexp(1.0, max_exp);

  const ComponentPrecisionScalingBounds bounds =
      ComputeComponentPrecisionScalingBounds(params, mp_model, compo, states,
                                             max_domain_bound);

  double compo_min_headroom = kInfinity;
  double compo_max_needed = 0.0;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    compo_min_headroom =
        std::min(compo_min_headroom, bounds.min_headroom[local_col]);
    compo_max_needed =
        std::max(compo_max_needed, bounds.max_scaling_needed[local_col]);
  }
  if (compo_min_headroom == kInfinity || compo_max_needed <= 1.0) return;

  const double needed_scaling =
      std::ldexp(1.0, LowestPowerOfTwoAtOrAbove(compo_max_needed));
  const double scaling =
      std::min({compo_max_scaling, compo_min_headroom, needed_scaling});
  if (scaling <= 1.0) return;

  for (const int var : compo_vars) {
    MipVariableState& state = states[var];
    CHECK_EQ(state.scaling, 1.0);
    ++stats->num_common_scaled;
    ScaleVariableState(scaling, &state);
  }
  stats->min_common_scaling = std::min(stats->min_common_scaling, scaling);
  stats->max_common_scaling = std::max(stats->max_common_scaling, scaling);
}

struct TumComponentScratch {
  struct NonIntegerValue {
    double value;
    double tol;
  };
  glop::CompactSparseMatrix row_matrix;
  glop::CompactSparseMatrix col_matrix;
  std::vector<double> col_scale;
  std::vector<double> row_scale;
  std::vector<NonIntegerValue> non_integer_values;
};

// Evaluates a single residual continuous component for total unimodularity /
// 2-modularity, tightens bounds, and (when `!only_tighten_bounds`) either
// makes its variables integral (filling `row_scalings`, see
// TryScaleModularComponent()) or returns true with
// `*common_precision_max_bound` set for `ApplyTumCommonPrecisionScaling()`.
//
// The matrix coefficients are read from `mp_model`, the variables are read
// from and written to `states`. Note that when `only_tighten_bounds` is true,
// `compo.continuous_vars` may also contain integer variables.
bool TryProcessTumComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    bool only_tighten_bounds, bool include_large_integers,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    TumStats* stats, TumComponentScratch* scratch,
    double* common_precision_max_bound,
    std::vector<ModularConstraintScaling>* row_scalings) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> compo_rows = compo.constraints;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  CHECK(!var_to_local_col.empty());
  ++stats->num_components;
  const double tolerance = params.mip_wanted_precision();

  // Compute this component's row_matrix and col_matrix.
  scratch->row_matrix.Reset(RowIndex(compo_vars.size()));
  for (const int c : compo_rows) {
    const MPConstraintProto& constraint = mp_model.constraints(c).proto();
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int local_col = var_to_local_col[constraint.var_index(i)];
      if (local_col == -1) continue;
      scratch->row_matrix.AddEntryToCurrentColumn(RowIndex(local_col), coeff);
    }
    scratch->row_matrix.CloseCurrentColumn();
  }
  scratch->col_matrix.PopulateFromTranspose(scratch->row_matrix);

  // Can we scale it to only +1/-1?
  if (!ComputeRowAndColScales(scratch->col_matrix, scratch->row_matrix,
                              tolerance, &scratch->col_scale,
                              &scratch->row_scale)) {
    return false;
  }

  // Is the scaled matrix TUM or 2-modular? Note that positive row/column
  // scaling preserves both the sparsity pattern and entry signs, which is all
  // CheckComponentModularity inspects.
  const ModularityStatus status =
      CheckComponentModularity(scratch->col_matrix, scratch->row_matrix);
  if (status == ModularityStatus::kNotTwoModular) {
    return false;
  }

  // If the component contains integer variables, we can only tighten bounds
  // if all the vertices of the component LP are integral whatever the
  // (integer) value of the external variables. By Hoffman-Kruskal, this is
  // the case if the matrix is TUM with unit column scaling and if all the
  // variable bounds, scaled RHS and scaled external coefficients are integer.
  if (include_large_integers) {
    bool has_integer_vars = false;
    for (const int var : compo_vars) {
      if (states[var].is_integer) {
        has_integer_vars = true;
        break;
      }
    }
    if (has_integer_vars &&
        !ComponentHasIntegralVertices(mp_model, states, compo_vars, compo_rows,
                                      scratch->col_scale, scratch->row_scale,
                                      var_to_local_col, status, tolerance)) {
      return false;
    }
  }

  if (status == ModularityStatus::kTotallyUnimodular) {
    ++stats->num_tum_components;
  } else {
    ++stats->num_2_modular_components;
  }

  const double base_factor =
      status == ModularityStatus::kTwoModular ? 2.0 : 1.0;
  TightenComponentBounds(compo_vars, compo_rows, scratch->col_scale,
                         scratch->row_scale, var_to_local_col, base_factor,
                         mp_model, states, stats);
  if (only_tighten_bounds) return false;

  // Collect bounds and discrete coefficients in this component, pre-scaled
  // by `col_scale` and `1 / row_scale`.
  bool has_col_scaling = false;
  double max_bound = 0.0;
  scratch->non_integer_values.clear();
  auto record_value = [&](double raw_v, double v_tol) {
    if (!std::isfinite(raw_v)) return;
    const double v = std::abs(raw_v);
    if (std::abs(v - std::round(v)) > v_tol) {
      scratch->non_integer_values.push_back({v, v_tol});
    }
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const int var = compo_vars[local_col];
    const double c_scale = scratch->col_scale[local_col];
    const double col_tol = tolerance * std::min(1.0, c_scale);
    if (std::abs(c_scale - 1.0) > col_tol) has_col_scaling = true;
    max_bound = std::max(max_bound, c_scale);
    record_value(c_scale, col_tol);

    const double lb = states[var].lb * c_scale;
    const double ub = states[var].ub * c_scale;
    if (std::isfinite(lb)) max_bound = std::max(max_bound, std::abs(lb));
    if (std::isfinite(ub)) max_bound = std::max(max_bound, std::abs(ub));
    record_value(lb, col_tol);
    record_value(ub, col_tol);
  }
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const int c = compo_rows[local_row];
    const MPConstraintProto& constraint = mp_model.constraints(c).proto();
    const double r_scale = scratch->row_scale[local_row];
    AccurateSum<double> ext_offset;
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int v = constraint.var_index(i);
      if (var_to_local_col[v] != -1) continue;
      const MipVariableState& ext_state = states[v];
      if (ext_state.shift != 0.0) ext_offset.Add(coeff * ext_state.shift);
      if (ext_state.lb == 0.0 && ext_state.ub == 0.0) continue;
      const double val = std::abs(coeff / ext_state.scaling) / r_scale;
      const double max_dom =
          std::max(std::abs(ext_state.lb), std::abs(ext_state.ub));
      const double ext_tol =
          std::isfinite(max_dom)
              ? std::min(tolerance, 0.5 * params.mip_check_precision() /
                                        std::max(1.0, r_scale * max_dom))
              : tolerance * std::min(1.0, val);
      record_value(val, ext_tol);
    }
    record_value((constraint.lower_bound() - ext_offset.Value()) / r_scale,
                 tolerance);
    record_value((constraint.upper_bound() - ext_offset.Value()) / r_scale,
                 tolerance);
  }

  // Determine whether we can mark variables as integer (possibly with a
  // rational scaling factor) or apply a common component power-of-two factor.
  const int64_t modular_factor =
      status == ModularityStatus::kTwoModular ? 2 : 1;
  const double max_s =
      params.mip_max_bound() / (base_factor * std::max(1.0, max_bound));
  bool mark_as_integer = false;

  double chosen_s = 1.0;
  if (scratch->non_integer_values.empty() && modular_factor == 1 &&
      !has_col_scaling) {
    mark_as_integer = true;
  } else if (params.mip_automatically_scale_variables() && max_s >= 1.0) {
    const int64_t int_limit =
        max_s >= static_cast<double>(kint64max)
            ? kint64max
            : CapAdd(static_cast<int64_t>(std::floor(max_s)), 1);
    double s = 1.0;
    for (const auto& [v, v_tol] : scratch->non_integer_values) {
      const int64_t f = FindRationalFactor(s * v, int_limit, s * v_tol);
      if (f == 0 || s * f > max_s) {
        s = 0.0;
        break;
      }
      s *= f;
    }
    if (s > 0.0) {
      chosen_s = s;
      mark_as_integer = true;
    }
  }

  if (mark_as_integer &&
      TryScaleModularComponent(params, compo, mp_model, scratch->col_scale,
                               scratch->row_scale, modular_factor, chosen_s,
                               states, stats, row_scalings)) {
    if (status == ModularityStatus::kTotallyUnimodular) {
      stats->num_new_tum_integers += compo_vars.size();
    } else {
      stats->num_new_2_modular_integers += compo_vars.size();
    }
    return false;
  }

  if (!has_col_scaling && params.mip_scale_for_precision()) {
    // Skip components with an infinite or zero bound.
    double compo_max_bound = 0.0;
    for (const int var : compo_vars) {
      compo_max_bound = std::max({compo_max_bound, std::abs(states[var].lb),
                                  std::abs(states[var].ub)});
    }
    if (compo_max_bound > 0.0 && std::isfinite(compo_max_bound)) {
      *common_precision_max_bound = compo_max_bound;
      return true;
    }
  }
  return false;
}

void LogTumBoundTighteningStats(const TumStats& stats, SolverLogger* logger) {
  if (stats.num_tightened_bounds > 0) {
    SOLVER_LOG(logger, "[TUM] Tightened ",
               FormatCounter(stats.num_tightened_bounds),
               " variable bounds across ", stats.num_bound_tightened_components,
               " components from [", stats.old_min, ", ", stats.old_max,
               "] to [", stats.new_min, ", ", stats.new_max, "].");
  }
}

}  // namespace

void LogTumStats(const TumStats& stats, SolverLogger* logger) {
  if (stats.num_tum_components + stats.num_2_modular_components == 0) {
    if (stats.num_components > 1) {
      SOLVER_LOG(logger, "[TUM] 0/", stats.num_components,
                 " continuous residual matrix with simple structure.");
    }
    LogTumBoundTighteningStats(stats, logger);
    return;
  }

  SOLVER_LOG(logger, "[TUM] ", stats.num_tum_components, "/",
             stats.num_components,
             " continuous residual matrix are Totally Unimodular; +",
             stats.num_new_tum_integers, " variables marked as integer.");
  if (stats.num_new_2_modular_integers > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_2_modular_components, "/",
               stats.num_components - stats.num_tum_components,
               " components left are 2-modular; +",
               stats.num_new_2_modular_integers,
               " variable scaled and marked as integer.");
  }
  if (stats.num_modular_constraints > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_modular_constraints,
               " modular constraints scaled directly (max bound error: ",
               stats.max_var_bound_error, ").");
  }
  if (stats.num_common_scaled > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_common_scaled,
               " variables scaled with a common component factor in [",
               stats.min_common_scaling, ", ", stats.max_common_scaling, "].");
  }
  LogTumBoundTighteningStats(stats, logger);
}

std::vector<ModularConstraintScaling> MarkComponentTotallyUnimodularAsInteger(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    TumStats* stats) {
  DCHECK(mp_model.underlying_model().general_constraint().empty());
  CHECK_EQ(compo.var_to_local_col.size(), mp_model.num_variables());
  CHECK_EQ(states.size(), mp_model.num_variables());
  std::vector<ModularConstraintScaling> row_scalings;
  if (compo.continuous_vars.empty()) return row_scalings;
  if (params.keep_all_feasible_solutions_in_presolve()) return row_scalings;

  TumComponentScratch scratch;
  double common_precision_max_bound = 0.0;
  if (TryProcessTumComponent(params, compo, /*only_tighten_bounds=*/false,
                             /*include_large_integers=*/false, mp_model, states,
                             stats, &scratch, &common_precision_max_bound,
                             &row_scalings)) {
    ApplyTumCommonPrecisionScaling(params, compo, common_precision_max_bound,
                                   mp_model, states, stats);
  }
  return row_scalings;
}

void TightenResidualTotallyUnimodularVariableBounds(
    const SatParameters& params, MPModelProtoWrapper* mp_model,
    SolverLogger* logger) {
  if (!mp_model->underlying_model().general_constraint().empty()) return;
  if (params.keep_all_feasible_solutions_in_presolve()) return;

  const int num_vars = mp_model->num_variables();
  const int num_constraints = mp_model->num_constraints();
  std::vector<MipVariableState> states(num_vars);
  for (int var = 0; var < num_vars; ++var) {
    const MPVariableProto& mp_var = mp_model->variables(var);
    states[var].lb = mp_var.lower_bound();
    states[var].ub = mp_var.upper_bound();
    states[var].is_integer = mp_var.is_integer();
  }

  // Integer variables with a large domain (typically unbounded ones) are
  // first included in the residual matrix: this allows to tighten them when
  // they are part of an integral TUM component (e.g. big-M scheduling
  // formulations), and this also provides finite bounds for the external
  // variables of the second pass. The second pass only considers continuous
  // variables, since it can lead to different (smaller) components.
  //
  // In both passes, we only tighten the bounds of components whose LP vertices
  // are guaranteed to be integral for any integer assignment of the external
  // variables (Hoffman-Kruskal).
  TumStats stats;
  TumComponentScratch scratch;
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> residual_var_indices;
  std::vector<int> row_first_col(num_constraints, -1);
  std::vector<int> var_to_local_col_buffer(num_vars, -1);
  const double tighten_split = 0.1 * params.mip_max_bound();
  for (const bool include_large_integers : {true, false}) {
    // 1. Identify and re-index residual variables.
    //
    // Note: Rather than considering only integer variable as external, we also
    // mark as external any variable that have a "small" domain. This should
    // allow to split the problem into more component, increasing the chance
    // that some of them are TUM and allow to restrict their large bounds via
    // TightenComponentBounds().
    to_dense_index.assign(num_vars, -1);
    residual_var_indices.clear();
    for (int var = 0; var < num_vars; ++var) {
      const double lb = states[var].lb;
      const double ub = states[var].ub;
      if (std::abs(lb) <= tighten_split && std::abs(ub) <= tighten_split) {
        continue;
      }
      if (!include_large_integers && states[var].is_integer) continue;
      to_dense_index[var] = residual_var_indices.size();
      residual_var_indices.push_back(var);
    }
    if (residual_var_indices.empty()) continue;

    // 2. Partition residual variables and constraints into connected
    // components.
    const int num_cols = residual_var_indices.size();
    DenseConnectedComponentsFinder components;
    components.SetNumberOfNodes(num_cols);
    for (int c = 0; c < num_constraints; ++c) {
      const MPConstraintProto& constraint = mp_model->constraints(c).proto();
      int first_col = -1;
      for (int i = 0; i < constraint.var_index_size(); ++i) {
        if (constraint.coefficient(i) == 0.0) continue;
        const int col = to_dense_index[constraint.var_index(i)];
        if (col == -1) continue;  // Skip external variables.
        if (first_col == -1) {
          first_col = col;
        } else {
          components.AddEdge(first_col, col);
        }
      }
      row_first_col[c] = first_col;
    }

    const int num_components = components.GetNumberOfComponents();
    const std::vector<int> col_to_components = components.GetComponentIds();
    std::vector<ResidualContinuousComponent> compos(num_components);
    for (int col = 0; col < num_cols; ++col) {
      compos[col_to_components[col]].continuous_vars.push_back(
          residual_var_indices[col]);
    }
    for (int c = 0; c < num_constraints; ++c) {
      if (row_first_col[c] != -1) {
        compos[col_to_components[row_first_col[c]]].constraints.push_back(c);
      }
    }

    // 3. Process each connected component independently.
    for (ResidualContinuousComponent& compo : compos) {
      const ScopedVarToLocalCol scoped(&compo, &var_to_local_col_buffer);
      double unused_common_precision_max_bound = 0.0;
      TryProcessTumComponent(params, compo, /*only_tighten_bounds=*/true,
                             include_large_integers, *mp_model,
                             absl::MakeSpan(states), &stats, &scratch,
                             &unused_common_precision_max_bound,
                             /*row_scalings=*/nullptr);
    }
  }
  LogTumBoundTighteningStats(stats, logger);

  // Only the bounds can change.
  for (int var = 0; var < num_vars; ++var) {
    MPVariableProto* mp_var = mp_model->mutable_variables(var);
    mp_var->set_lower_bound(states[var].lb);
    mp_var->set_upper_bound(states[var].ub);
  }
}

}  // namespace sat
}  // namespace operations_research
