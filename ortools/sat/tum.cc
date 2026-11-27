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
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/mip_model.pb.h"
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

// What we could detect about a component matrix. Since the detection is not
// complete, kUnknown does not mean that it is not TUM or 2-modular.
enum class ModularityStatus {
  kUnknown = 0,
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
      if (size == 2) return ModularityStatus::kUnknown;
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
  return ModularityStatus::kUnknown;
}

// The scaling found by DetectComponentModularity() for the matrix `A` of a
// component: `|A(r, c)| == row_scale[r] * col_scale[c]` for all its non-zero
// entries, so that the scaled matrix has entries in {-1, 0, +1}. The scales
// are only meaningful when `status` is not kUnknown.
struct ComponentScaling {
  ModularityStatus status = ModularityStatus::kUnknown;
  std::vector<double> col_scale;
  std::vector<double> row_scale;

  // 1 for a totally unimodular component and 2 for a 2-modular one.
  int64_t modular_factor() const {
    return status == ModularityStatus::kTwoModular ? 2 : 1;
  }
};

// Detects whether the matrix of the continuous variables of `compo` can be
// scaled to a totally unimodular or 2-modular matrix, see ComponentScaling.
ComponentScaling DetectComponentModularity(
    const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, double tolerance) {
  CHECK(!compo.var_to_local_col.empty());

  glop::CompactSparseMatrix row_matrix;
  row_matrix.Reset(RowIndex(compo.continuous_vars.size()));
  for (const int c : compo.constraints) {
    const EnforcedLinearConstraintProto& constraint = mp_model.constraints(c);
    for (int i = 0; i < constraint.vars().size(); ++i) {
      const double coeff = constraint.coeffs(i);
      if (coeff == 0.0) continue;
      const int local_col = compo.var_to_local_col[constraint.vars(i)];
      if (local_col == -1) continue;
      row_matrix.AddEntryToCurrentColumn(RowIndex(local_col), coeff);
    }
    row_matrix.CloseCurrentColumn();
  }
  glop::CompactSparseMatrix col_matrix;
  col_matrix.PopulateFromTranspose(row_matrix);

  ComponentScaling scaling;
  if (ComputeRowAndColScales(col_matrix, row_matrix, tolerance,
                             &scaling.col_scale, &scaling.row_scale)) {
    scaling.status = CheckComponentModularity(col_matrix, row_matrix);
  }
  return scaling;
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
void TightenComponentBounds(const ResidualContinuousComponent& compo,
                            const ComponentScaling& scaling,
                            const MPModelProtoWrapper& mp_model,
                            double bound_tol,
                            absl::Span<MipVariableState> states,
                            TumStats* stats) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> compo_rows = compo.constraints;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  absl::Span<const double> col_scale = scaling.col_scale;
  absl::Span<const double> row_scale = scaling.row_scale;
  const double base_factor = static_cast<double>(scaling.modular_factor());
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
    magnitude[item] = ExactSumUb(magnitude[item], value);
  };

  // 1. Rows.
  //
  // If any connected external variable has an infinite bound, the effective RHS
  // `b_r - A_{r, ext} z*` cannot be bounded across all feasible `z*`, so we
  // abort.
  TightSum min_activity;
  TightSum max_activity;
  TightSum ext_disc_lb;
  TightSum ext_disc_ub;
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const EnforcedLinearConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]);
    const double r_scale = row_scale[local_row];
    min_activity.Clear();
    max_activity.Clear();
    ext_disc_lb.Clear();
    ext_disc_ub.Clear();
    int row_nnz = 0;
    bool all_external_integer = true;
    double min_external_step = std::numeric_limits<double>::infinity();
    for (int i = 0; i < constraint.vars().size(); ++i) {
      const double coeff = constraint.coeffs(i);
      if (coeff == 0.0) continue;
      const int var = constraint.vars(i);
      if (var_to_local_col[var] != -1) {
        ++row_nnz;
        continue;
      }
      const MipVariableState& external = states[var];
      if (!std::isfinite(external.lb) || !std::isfinite(external.ub)) return;
      const double lb_sum = ExactSumLb(external.lb, external.shift);
      const double ub_sum = ExactSumUb(external.ub, external.shift);
      if (!std::isfinite(lb_sum) || !std::isfinite(ub_sum)) return;
      const double var_lb = QuotientBounds(lb_sum, external.scaling).first;
      const double var_ub = QuotientBounds(ub_sum, external.scaling).second;
      if (!std::isfinite(var_lb) || !std::isfinite(var_ub)) return;
      if (coeff > 0.0) {
        min_activity.AddProd(coeff, var_lb);
        max_activity.AddProd(coeff, var_ub);
      } else {
        min_activity.AddProd(coeff, var_ub);
        max_activity.AddProd(coeff, var_lb);
      }
      if (bound_tol > 0.0 &&
          (external.error_lb != 0.0 || external.error_ub != 0.0)) {
        const auto [c_lb, c_ub] = QuotientBounds(coeff, external.scaling);
        AddTermErrorBounds(c_lb, c_ub, external.error_lb, external.error_ub,
                           &ext_disc_lb, &ext_disc_ub);
      }
      if (!external.is_integer) all_external_integer = false;
      const double abs_ext_coeff =
          ExactQuotientLb(std::abs(coeff), external.scaling);
      min_external_step =
          std::min(min_external_step, ExactQuotientLb(abs_ext_coeff, r_scale));
    }
    const double min_ext_activity = min_activity.TightLowerBound();
    const double max_ext_activity = max_activity.TightUpperBound();
    if (!std::isfinite(min_ext_activity) || !std::isfinite(max_ext_activity)) {
      return;
    }

    double row_lb = constraint.lower_bound();
    double row_ub = constraint.upper_bound();
    if (bound_tol > 0.0) {
      if (std::isfinite(row_lb)) {
        row_lb = ext_disc_lb.Add(row_lb).Add(-bound_tol).TightLowerBound();
      }
      if (std::isfinite(row_ub)) {
        row_ub = ext_disc_ub.Add(row_ub).Add(bound_tol).TightUpperBound();
      }
    }

    const bool has_lb = std::isfinite(row_lb);
    const bool has_ub = std::isfinite(row_ub);
    if (!can_drop || has_lb == has_ub) {
      // Keep both sides: `M_row(r)` is the maximum finite magnitude of the
      // effective RHS.
      double max_of_finite_rhs_magnitude = 0.0;
      if (has_lb) {
        max_of_finite_rhs_magnitude = std::max(
            {max_of_finite_rhs_magnitude,
             std::abs(ExactScaledSumLb(row_lb, -max_ext_activity, r_scale)),
             std::abs(ExactScaledSumUb(row_lb, -min_ext_activity, r_scale))});
      }
      if (has_ub) {
        max_of_finite_rhs_magnitude = std::max(
            {max_of_finite_rhs_magnitude,
             std::abs(ExactScaledSumLb(row_ub, -max_ext_activity, r_scale)),
             std::abs(ExactScaledSumUb(row_ub, -min_ext_activity, r_scale))});
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
      tightest = ExactScaledSumUb(row_lb, -min_ext_activity, r_scale);
      loosest = ExactScaledSumLb(row_lb, -max_ext_activity, r_scale);
    } else {
      tightest = -ExactScaledSumLb(row_ub, -max_ext_activity, r_scale);
      loosest = -ExactScaledSumUb(row_ub, -min_ext_activity, r_scale);
    }
    if (!std::isfinite(tightest) || !std::isfinite(loosest)) return;
    const auto drop_limit_for = [row_nnz](double rhs) {
      return ExactQuotientLb(-rhs, static_cast<double>(row_nnz));
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
      const double delta = ExactSumUb(static_magnitude, -tightest_magnitude);
      const double second = all_external_integer
                                ? ExactSumUb(tightest, -min_external_step)
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
bool ComponentHasIntegralVertices(const ResidualContinuousComponent& compo,
                                  const ComponentScaling& scaling,
                                  const MPModelProtoWrapper& mp_model,
                                  absl::Span<const MipVariableState> states,
                                  double tolerance) {
  if (scaling.status != ModularityStatus::kTotallyUnimodular) return false;
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> compo_rows = compo.constraints;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  absl::Span<const double> col_scale = scaling.col_scale;
  absl::Span<const double> row_scale = scaling.row_scale;
  const auto is_almost_integer = [tolerance](double x) {
    return !std::isfinite(x) || IsIntegerWithinTolerance(x, tolerance);
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    if (std::abs(col_scale[local_col] - 1.0) > tolerance) return false;
    const MipVariableState& state = states[compo_vars[local_col]];
    if (!is_almost_integer(state.lb) || !is_almost_integer(state.ub)) {
      return false;
    }
  }
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const EnforcedLinearConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]);
    const double r_scale = row_scale[local_row];
    if (!is_almost_integer(constraint.lower_bound() / r_scale) ||
        !is_almost_integer(constraint.upper_bound() / r_scale)) {
      return false;
    }
    for (int i = 0; i < constraint.vars().size(); ++i) {
      const double coeff = constraint.coeffs(i);
      if (coeff == 0.0) continue;
      const int var = constraint.vars(i);
      if (var_to_local_col[var] != -1) continue;
      if (!states[var].is_integer) return false;
      if (!is_almost_integer((coeff / states[var].scaling) / r_scale)) {
        return false;
      }
    }
  }
  return true;
}

// Result of trying to make a component integral with a given multiplier.
enum class ScalingOutcome {
  kSuccess,
  // Another multiplier of the same candidate will not work either.
  kFailure,
  // A smaller multiplier could avoid a variable domain or activity overflow.
  kOverflow,
  // A larger multiplier could reduce the coefficient or bound errors.
  kNeedMorePrecision,
};

// Implements DetectAndConvertTotallyUnimodularComponent() for a component with
// a TUM or 2-modular `scaling`: looks for a multiplier `s` such that scaling
// each variable by `modular_factor * round(s * col_scale)` and each constraint
// by `modular_factor * s / row_scale` makes the component integral, i.e. with
// +/-1 coefficients on its variables, multiples of `modular_factor` on the
// external ones, and variable and constraint bounds that are multiples of
// `modular_factor`.
//
// This only keeps references to its arguments, which must outlive it.
class ModularComponentConverter {
 public:
  ModularComponentConverter(const SatParameters& params,
                            const ResidualContinuousComponent& compo,
                            const ComponentScaling& scaling,
                            const MPModelProtoWrapper& mp_model,
                            absl::Span<MipVariableState> states,
                            ConstraintScaler* scaler, CpModelProto* cp_model,
                            TumStats* stats);

  // Tries the candidate multipliers described in tum.h by increasing value. On
  // the first success, updates `states` and `stats`, adds the scaled
  // constraints of the component to `cp_model` and returns true. Otherwise,
  // returns false and leaves everything untouched.
  bool Convert();

 private:
  // A positive value of the row and column scaled component that a multiplier
  // `s` must make integer: `s * val` must be within `s * tol` of an integer.
  struct ValueItem {
    double val;
    double tol;
    // True for the coefficient of an external variable: `tol` then bounds the
    // resulting activity error, so any denominator that satisfies it is
    // accepted.
    bool is_ext_coeff;
  };

  // A variable or constraint bound of the scaled component, expressed as a
  // lower bound (upper bounds are negated). With a multiplier `s`, the bound
  // `s * val` is either snapped to the nearest integer, which must be within
  // `s * snap_tol`, or rounded down, with an error that should be at most
  // `s * wanted_tol`.
  struct BoundItem {
    double val;
    double snap_tol;
    double wanted_tol;
    // The lower and upper bounds are equal, so both are rounded outward.
    bool is_equality;
  };

  // The multipliers `q_odd * 2^k` in `[start_s, max_s]`.
  struct Candidate {
    double q_odd = 0.0;
    // Whether the scaled bounds are snapped to the nearest integer (they must
    // all be integer within tolerance) or rounded outward.
    bool snap_within_tol = false;
    // Smallest multiplier making all `coeff_items_` integer, or zero if there
    // is none up to `max_s`.
    double min_s = 0.0;
    // Smallest multiplier from `min_s` for which the bounds are expected to be
    // precise enough. This is above `max_s` if there is none.
    double start_s = 0.0;
    // Largest multiplier keeping the variable domains within the domain cap.
    double max_s = 0.0;
  };

  // A multiplier to try.
  struct Attempt {
    double s;
    // See Candidate.
    bool snap_within_tol;
    // Whether the bound rounding errors may exceed `mip_wanted_precision`.
    bool allow_imprecise;
  };

  // Fills `coeff_items_`, `all_items_` and `bound_items_`.
  void CollectItems();

  // Returns an odd integer `q` such that for each item, `q * 2^k * val` is
  // integer within tolerance for some `k` that fits in the domain cap, or zero
  // if there is none. Each item contributes the odd part of the denominator of
  // a rational approximation of its value (see FindRationalFactor()), like
  // FindOddRationalScaling() in fp_utils.cc.
  //
  // With `exact_pass`, a tight tolerance recovers exact rationals and decimals
  // (e.g. `5^6 = 15625` from `1e-6`). Otherwise, this uses the tolerance of
  // each item and only accepts small denominators (below the Dirichlet
  // threshold under which a rational approximation is not a coincidence).
  double ComputeOddFactor(absl::Span<const ValueItem> items,
                          bool exact_pass) const;

  Candidate MakeCandidate(double q_odd, bool snap_within_tol) const;

  // Tries to make the component integral with the given multiplier. The
  // variable and constraint bounds are rounded outward (or snapped within
  // `mip_tight_precision` when `snap_within_tol` is true) to multiples of
  // `modular_factor`: any inward rounding is limited to a total strictly below
  // one over the whole component, so every `mip_tight_precision`-feasible
  // solution can be repaired into a feasible integer one.
  //
  // This succeeds if nothing overflows, the coefficient errors are within
  // `mip_wanted_precision`, and so are the bound rounding errors unless
  // `allow_imprecise` is true. On success, updates `states_` and `stats_` and
  // adds the scaled constraints to `cp_model_`.
  ScalingOutcome TryConvert(const Attempt& attempt);

  // Fills `new_states_` with the scaled, rounded and shifted state of each
  // variable of the component, and `lb_tightening_` / `ub_tightening_` with
  // how much each bound was snapped inward.
  ScalingOutcome ScaleVariables(const Attempt& attempt);

  // Fills `rows_[local_row]` with the given constraint scaled according to
  // `new_states_`, if this is possible without overflow and with the wanted
  // precision.
  ScalingOutcome ScaleRow(const Attempt& attempt, int local_row);

  const SatParameters& params_;
  const ResidualContinuousComponent& compo_;
  const ComponentScaling& scaling_;
  const MPModelProtoWrapper& mp_model_;
  absl::Span<MipVariableState> states_;
  ConstraintScaler* scaler_;
  CpModelProto* cp_model_;
  TumStats* stats_;

  const int64_t modular_factor_;
  const double m_;  // `modular_factor_` as a double.
  const double wanted_tol_;
  const double tight_tol_;
  const double rational_tol_;
  const double max_domain_bound_;

  // Whether some column scales are not one.
  bool has_col_scaling_ = false;
  // A multiplier `s` is within the domain cap if
  // `s * base_col_bound_ <= max_domain_bound_`, i.e. if `s <= max_s_`.
  double base_col_bound_ = 0.0;
  double max_s_ = 0.0;

  // Column scales and external coefficients.
  std::vector<ValueItem> coeff_items_;
  // Same as `coeff_items_`, plus the variable and active constraint bounds.
  std::vector<ValueItem> all_items_;
  std::vector<BoundItem> bound_items_;

  // Filled by TryConvert().
  std::vector<MipVariableState> new_states_;
  std::vector<double> lb_tightening_;
  std::vector<double> ub_tightening_;
  double max_bound_error_ = 0.0;
  std::vector<ScaledLinearConstraint> rows_;
  TightSum total_tightening_;
  bool any_row_relaxed_ = false;

  // Temporary data for the terms of a constraint with a bounded variable.
  BoundedLinearExpression bounded_expr_;
  std::vector<double> coeffs_lb_;
  std::vector<double> coeffs_ub_;
  std::vector<int64_t> int_lbs_;
  std::vector<int64_t> int_ubs_;
  TightScalingErrorHelper tight_error_helper_;
};

// Use a higher target bound than mip_max_bound (1e7) so components with large
// or clamped bounds can still be scaled together, while keeping scaled TUM
// domains within 1e11 so double-precision LP simplex tolerances (1e-4 in CP
// space) remain accurate on unit-coefficient rows.
constexpr double kTumPrecisionMaxBound = 1e11;

ModularComponentConverter::ModularComponentConverter(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const ComponentScaling& scaling, const MPModelProtoWrapper& mp_model,
    absl::Span<MipVariableState> states, ConstraintScaler* scaler,
    CpModelProto* cp_model, TumStats* stats)
    : params_(params),
      compo_(compo),
      scaling_(scaling),
      mp_model_(mp_model),
      states_(states),
      scaler_(scaler),
      cp_model_(cp_model),
      stats_(stats),
      modular_factor_(scaling.modular_factor()),
      m_(static_cast<double>(modular_factor_)),
      wanted_tol_(params.mip_wanted_precision()),
      tight_tol_(params.mip_tight_precision()),
      rational_tol_(std::max(1e-12, tight_tol_)),
      max_domain_bound_(
          std::min(std::max(params.mip_max_bound(), kTumPrecisionMaxBound),
                   FastStepDown(static_cast<double>(kint64max / 2)))) {}

bool ModularComponentConverter::Convert() {
  absl::Span<const int> compo_vars = compo_.continuous_vars;
  double max_col_scaled_bound = 0.0;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const MipVariableState& state = states_[compo_vars[local_col]];
    DCHECK(std::isfinite(state.lb));
    DCHECK(std::isfinite(state.ub));
    const double c_scale = scaling_.col_scale[local_col];
    if (std::abs(c_scale - 1.0) > wanted_tol_ * std::min(1.0, c_scale)) {
      has_col_scaling_ = true;
    }
    max_col_scaled_bound =
        std::max({max_col_scaled_bound, c_scale, std::abs(state.lb * c_scale),
                  std::abs(state.ub * c_scale)});
  }
  base_col_bound_ = m_ * std::max(1.0, max_col_scaled_bound);
  max_s_ = max_domain_bound_ / base_col_bound_;
  if (modular_factor_ > 1 && max_s_ < 1.0) return false;

  CollectItems();

  // The candidates are tried by increasing first multiplier.
  std::vector<Candidate> candidates;
  const auto add_candidate = [&](double q_odd, bool snap_within_tol) {
    if (q_odd <= 0.0) return;
    for (const Candidate& prev : candidates) {
      if (prev.q_odd == q_odd && prev.snap_within_tol == snap_within_tol) {
        return;
      }
    }
    const Candidate c = MakeCandidate(q_odd, snap_within_tol);
    if (c.min_s > 0.0 && c.min_s <= c.max_s) candidates.push_back(c);
  };
  add_candidate(ComputeOddFactor(all_items_, /*exact_pass=*/true),
                /*snap_within_tol=*/true);
  add_candidate(ComputeOddFactor(all_items_, /*exact_pass=*/false),
                /*snap_within_tol=*/true);
  if (!coeff_items_.empty()) {
    const double exact_q = ComputeOddFactor(coeff_items_, /*exact_pass=*/true);
    const double approx_q =
        ComputeOddFactor(coeff_items_, /*exact_pass=*/false);
    add_candidate(exact_q > 0.0 ? exact_q : approx_q,
                  /*snap_within_tol=*/false);
  }
  if (!has_col_scaling_) add_candidate(1.0, /*snap_within_tol=*/false);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.start_s != b.start_s) return a.start_s < b.start_s;
              return a.snap_within_tol > b.snap_within_tol;
            });

  // Range of the pure power of two multipliers with bounds rounded outward,
  // and whether they were limited by a cap: this is used for the best effort
  // fallback below.
  double pow2_min_s = 0.0;
  double pow2_max_s = 0.0;
  bool pow2_hit_cap = false;
  for (const Candidate& c : candidates) {
    const bool is_pow2 =
        !has_col_scaling_ && c.q_odd == 1.0 && !c.snap_within_tol;
    if (is_pow2) {
      pow2_min_s = c.min_s;
      pow2_max_s = c.max_s;
      pow2_hit_cap = c.start_s > c.max_s;
    }
    for (double s = c.start_s; s <= c.max_s; s *= 2.0) {
      const ScalingOutcome outcome =
          TryConvert({.s = s,
                      .snap_within_tol = c.snap_within_tol,
                      .allow_imprecise = false});
      if (outcome == ScalingOutcome::kSuccess) return true;
      if (outcome == ScalingOutcome::kOverflow) {
        if (is_pow2) {
          pow2_max_s = s * 0.5;
          pow2_hit_cap = true;
        }
        break;
      }
      if (outcome != ScalingOutcome::kNeedMorePrecision) break;
      if (is_pow2 && s * 2.0 > c.max_s) pow2_hit_cap = true;
    }
  }

  // Best effort: use the largest power of two that does not overflow, even if
  // the bound rounding errors exceed `mip_wanted_precision`.
  if (pow2_hit_cap && params_.mip_proper_relaxation()) {
    const double min_best_effort_s = std::max(2.0, pow2_min_s);
    for (double s = pow2_max_s; s >= min_best_effort_s; s *= 0.5) {
      const ScalingOutcome outcome = TryConvert(
          {.s = s, .snap_within_tol = false, .allow_imprecise = true});
      if (outcome == ScalingOutcome::kSuccess) return true;
      if (outcome != ScalingOutcome::kOverflow) break;
    }
  }
  return false;
}

void ModularComponentConverter::CollectItems() {
  const auto add_bound = [this](double lb, double snap_tol, double wanted_tol,
                                bool is_equality) {
    all_items_.push_back({std::abs(lb), snap_tol, /*is_ext_coeff=*/false});
    bound_items_.push_back({lb, snap_tol, wanted_tol, is_equality});
  };

  absl::Span<const int> compo_vars = compo_.continuous_vars;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const MipVariableState& state = states_[compo_vars[local_col]];
    const double c_scale = scaling_.col_scale[local_col];
    const double col_tol = rational_tol_ * std::min(1.0, c_scale);
    if (c_scale != 1.0) {
      coeff_items_.push_back({c_scale, col_tol, /*is_ext_coeff=*/false});
      all_items_.push_back({c_scale, col_tol, /*is_ext_coeff=*/false});
    }
    const bool eq_var = state.lb == state.ub;
    add_bound(state.lb * c_scale, col_tol, wanted_tol_ * c_scale, eq_var);
    if (!eq_var) {
      add_bound(-state.ub * c_scale, col_tol, wanted_tol_ * c_scale, false);
    }
  }

  absl::Span<const int> compo_rows = compo_.constraints;
  std::vector<double> coeffs;
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const EnforcedLinearConstraintProto& ct =
        mp_model_.constraints(compo_rows[local_row]);
    const double r_scale = scaling_.row_scale[local_row];
    double raw_lb;
    double raw_ub;
    TransformConstraint(ct, states_, &coeffs, &raw_lb, &raw_ub);
    double min_activity = 0.0;
    double max_activity = 0.0;
    for (int i = 0; i < ct.vars().size(); ++i) {
      const double coeff = coeffs[i];
      if (coeff == 0.0) continue;
      const int var = ct.vars(i);
      const MipVariableState& state = states_[var];
      DCHECK(std::isfinite(state.lb));
      DCHECK(std::isfinite(state.ub));
      const int local_col = compo_.var_to_local_col[var];
      const double tol = local_col != -1 ? wanted_tol_ : 0.0;
      const double lb = state.lb - tol;
      const double ub = state.ub + tol;
      if (lb == 0.0 && ub == 0.0) continue;
      if (lb <= ub) {
        min_activity += coeff > 0.0 ? coeff * lb : coeff * ub;
        max_activity += coeff > 0.0 ? coeff * ub : coeff * lb;
      }
      if (local_col != -1) continue;

      // The tolerance on the coefficient of an external variable is the one
      // that keeps the activity error within `mip_wanted_precision`.
      const double val = std::abs(coeff) / r_scale;
      const double max_dom = std::max(std::abs(state.lb), std::abs(state.ub));
      const double ext_tol =
          std::min(wanted_tol_, wanted_tol_ / std::max(1.0, r_scale * max_dom));
      coeff_items_.push_back({val, ext_tol, /*is_ext_coeff=*/true});
      all_items_.push_back(
          {val, std::max(rational_tol_, ext_tol), /*is_ext_coeff=*/true});
    }
    // Only the constraint sides that are not redundant matter.
    const bool active_lb =
        std::isfinite(raw_lb) && min_activity < raw_lb - tight_tol_;
    const bool active_ub =
        std::isfinite(raw_ub) && max_activity > raw_ub + tight_tol_;
    const bool eq_row = active_lb && active_ub && raw_lb == raw_ub;
    if (active_lb) {
      add_bound(raw_lb / r_scale, rational_tol_, wanted_tol_ / r_scale, eq_row);
    }
    if (active_ub && !eq_row) {
      add_bound(-raw_ub / r_scale, rational_tol_, wanted_tol_ / r_scale, false);
    }
  }
}

double ModularComponentConverter::ComputeOddFactor(
    absl::Span<const ValueItem> items, bool exact_pass) const {
  if (max_s_ < 1.0) return 0.0;
  const double exact_base_tol = std::min(1e-12, rational_tol_);
  double mult = 1.0;
  for (const ValueItem& item : items) {
    if (!std::isfinite(item.val) || item.val == 0.0) continue;
    if (mult * base_col_bound_ > max_domain_bound_) return 0.0;
    const int max_p2 = MaxPowerOfTwoExponentWithinBound(mult * base_col_bound_,
                                                        max_domain_bound_);
    const int exp = FastILogB(item.val) + 1;
    double tol = item.tol;
    int min_target_exp = 0;
    if (exact_pass) {
      tol = item.is_ext_coeff ? std::max(exact_base_tol, item.tol)
                              : exact_base_tol;
      const int tol_exp = FastILogB(tol) + 1;
      min_target_exp = std::clamp(53 + tol_exp - 7, 0, 52);
    } else {
      if (IsIntegerWithinTolerance(mult * item.val, mult * item.tol)) {
        continue;
      }
      tol = std::min(tol, rational_tol_ * std::min(1.0, item.val));
    }

    // Multiply the value by a power of two (as far as the domain cap allows)
    // and only keep its fractional part.
    const int shift =
        std::min(max_p2, min_target_exp > exp ? min_target_exp - exp : 0);
    if (!exact_pass && shift > 0) tol = FastLdExp(tol, shift);
    tol = std::max(tol, FastTwoExp((exp + shift) - 51));
    const double scaled = FastLdExp(item.val, shift);
    const double frac = scaled - std::floor(scaled);
    const double sv = mult * frac;
    const double sv_tol = mult * tol;
    if (IsIntegerWithinTolerance(sv, sv_tol)) continue;

    // Multiply `mult` by the odd part of the smallest factor making `sv`
    // integer.
    const double rem_max = max_s_ / mult;
    if (rem_max < 2.0) return 0.0;
    const int64_t int_limit =
        rem_max >= static_cast<double>(kint64max)
            ? kint64max
            : CapAdd(static_cast<int64_t>(std::floor(rem_max)), 1);
    int64_t f = FindRationalFactor(sv, int_limit, sv_tol);
    if (f == 0) return 0.0;
    const int tz = std::countr_zero(static_cast<uint64_t>(f));
    f >>= tz;
    if (!item.is_ext_coeff &&
        static_cast<double>(f) > 0.1 / std::sqrt(sv_tol)) {
      return 0.0;
    }
    if (mult * static_cast<double>(f) * base_col_bound_ > max_domain_bound_) {
      return 0.0;
    }
    mult *= static_cast<double>(f);

    // If the power of two that this item needs no longer fits in the domain
    // cap, check that the largest one that fits is precise enough.
    const int max_p2_after = MaxPowerOfTwoExponentWithinBound(
        mult * base_col_bound_, max_domain_bound_);
    if (shift + tz > max_p2_after) {
      const int r = std::min(max_p2_after, std::max(0, 53 - exp));
      double frac_r = FastLdExp(item.val, r);
      frac_r -= std::floor(frac_r);
      const double prod = mult * frac_r;
      if (!IsIntegerWithinTolerance(prod, FastLdExp(mult * item.tol, r))) {
        return 0.0;
      }
    }
  }
  return mult;
}

ModularComponentConverter::Candidate ModularComponentConverter::MakeCandidate(
    double q_odd, bool snap_within_tol) const {
  Candidate c{.q_odd = q_odd, .snap_within_tol = snap_within_tol};
  if (q_odd <= 0.0 || q_odd * base_col_bound_ > max_domain_bound_) return c;
  c.max_s = FastLdExp(q_odd, MaxPowerOfTwoExponentWithinBound(
                                 q_odd * base_col_bound_, max_domain_bound_));
  double min_s = q_odd;
  for (const ValueItem& item : coeff_items_) {
    while (min_s <= c.max_s) {
      const double sv = min_s * item.val;
      if (IsIntegerWithinTolerance(sv, min_s * item.tol)) break;
      min_s *= 2.0;
    }
    if (min_s > c.max_s) return c;
  }
  c.min_s = min_s;

  double start_s = min_s;
  for (const BoundItem& item : bound_items_) {
    while (start_s <= c.max_s) {
      const double sv = start_s * item.val;
      if (snap_within_tol) {
        if (IsIntegerWithinTolerance(sv, start_s * item.snap_tol)) break;
      } else {
        const double out_err = item.is_equality ? std::ceil(sv) - std::floor(sv)
                                                : sv - std::floor(sv);
        if (out_err <= start_s * item.wanted_tol) break;
      }
      start_s *= 2.0;
    }
    if (start_s > c.max_s) break;
  }
  c.start_s = start_s;
  return c;
}

ScalingOutcome ModularComponentConverter::TryConvert(const Attempt& attempt) {
  ScalingOutcome outcome = ScaleVariables(attempt);
  if (outcome != ScalingOutcome::kSuccess) return outcome;

  absl::Span<const int> compo_vars = compo_.continuous_vars;
  const int num_rows = compo_.constraints.size();
  rows_.resize(num_rows);
  total_tightening_.Clear();
  any_row_relaxed_ = false;
  for (int local_row = 0; local_row < num_rows; ++local_row) {
    outcome = ScaleRow(attempt, local_row);
    if (outcome != ScalingOutcome::kSuccess) return outcome;
  }

  // Add the scaled constraints. This can only fail when adding back an integer
  // enforcement overflows.
  const int old_num_constraints = cp_model_->constraints_size();
  for (int local_row = 0; local_row < num_rows; ++local_row) {
    const EnforcedLinearConstraintProto& ct =
        mp_model_.constraints(compo_.constraints[local_row]);
    if (!scaler_
             ->AddAlreadyScaledConstraint(ct, states_, rows_[local_row],
                                          cp_model_)
             .ok()) {
      cp_model_->mutable_constraints()->DeleteSubrange(
          old_num_constraints,
          cp_model_->constraints_size() - old_num_constraints);
      return ScalingOutcome::kFailure;
    }
  }

  // Success: `states_`, `cp_model_`, and `stats_` are only updated now.
  const double row_tightening = total_tightening_.TightUpperBound();
  TightSum obj_error;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const int var = compo_vars[local_col];
    MipVariableState& state = states_[var];
    const MipVariableState& new_state = new_states_[local_col];
    if (state.shift == 0.0 && new_state.shift != 0.0) {
      ++stats_->shift_stats.num_shifted;
      stats_->shift_stats.max_shift =
          std::max(stats_->shift_stats.max_shift, std::abs(new_state.shift));
    }
    state = new_state;
    if (var < cp_model_->variables_size()) {
      IntegerVariableProto* cp_var = cp_model_->mutable_variables(var);
      cp_var->clear_domain();
      cp_var->add_domain(static_cast<int64_t>(state.lb));
      cp_var->add_domain(static_cast<int64_t>(state.ub));
    }
    const double obj = mp_model_.variables(var).objective_coefficient();
    if (obj != 0.0) {
      const double scaled_abs_obj =
          QuotientBounds(std::abs(obj), new_state.scaling).second;
      const double clamp_tightening =
          obj > 0.0 ? lb_tightening_[local_col] : ub_tightening_[local_col];
      obj_error.AddProd(scaled_abs_obj, clamp_tightening)
          .AddProd(scaled_abs_obj, row_tightening);
    }
  }
  stats_->num_modular_constraints += num_rows;
  stats_->max_var_bound_error =
      std::max(stats_->max_var_bound_error, max_bound_error_);
  stats_->objective_error_bound =
      ExactSumUb(stats_->objective_error_bound, obj_error.TightUpperBound());
  if (attempt.snap_within_tol && !any_row_relaxed_) {
    if (scaling_.status == ModularityStatus::kTotallyUnimodular) {
      stats_->num_new_tum_integers += compo_vars.size();
    } else {
      stats_->num_new_2_modular_integers += compo_vars.size();
    }
  } else {
    ++stats_->num_relaxed_modular_components;
    stats_->num_relaxed_modular_integers += compo_vars.size();
    for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
      const double var_s =
          m_ * std::round(attempt.s * scaling_.col_scale[local_col]);
      stats_->min_relaxed_modular_scaling =
          std::min(stats_->min_relaxed_modular_scaling, var_s);
      stats_->max_relaxed_modular_scaling =
          std::max(stats_->max_relaxed_modular_scaling, var_s);
    }
  }
  return ScalingOutcome::kSuccess;
}

ScalingOutcome ModularComponentConverter::ScaleVariables(
    const Attempt& attempt) {
  const double max_var_bound = MaxCpSatVariableBound();
  const auto is_too_large = [max_var_bound](double b) {
    return !std::isfinite(b) || std::abs(b) > max_var_bound;
  };
  const double round_tol = attempt.snap_within_tol ? tight_tol_ : 0.0;

  absl::Span<const int> compo_vars = compo_.continuous_vars;
  new_states_.resize(compo_vars.size());
  lb_tightening_.assign(compo_vars.size(), 0.0);
  ub_tightening_.assign(compo_vars.size(), 0.0);
  max_bound_error_ = 0.0;
  bool bound_imprecise = false;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const MipVariableState& state = states_[compo_vars[local_col]];
    const double scaled_col = attempt.s * scaling_.col_scale[local_col];
    const double col_s = std::round(scaled_col);
    if (!(col_s >= 1.0) ||
        std::abs(scaled_col - col_s) > scaled_col * wanted_tol_) {
      return ScalingOutcome::kFailure;
    }

    // Returns `modular_factor` times the lower bound scaled by `col_s` and
    // rounded down, or to the nearest integer if it is within `round_tol` (then
    // setting `tightening` to how much the bound was snapped inward).
    const auto round_lb = [&](double lb, double* tightening) {
      const double w =
          RoundScaledLowerBound(ProdBounds(col_s, lb).first, col_s * round_tol);
      const double error =
          MaxAbsScaledTightSum(TightSum().Add(w).AddProd(-col_s, lb), col_s);
      if (error > round_tol) {
        if (error > wanted_tol_) bound_imprecise = true;
        max_bound_error_ = std::max(max_bound_error_, error);
      }
      const double scaled_lb = m_ * w;
      if (attempt.snap_within_tol) {
        *tightening = std::max(0.0, TightSum()
                                        .Add(scaled_lb)
                                        .AddProd(-m_ * col_s, lb)
                                        .TightUpperBound());
      }
      return scaled_lb;
    };
    MipVariableState& new_state = new_states_[local_col];
    new_state = state;
    new_state.lb = round_lb(state.lb, &lb_tightening_[local_col]);
    new_state.ub = -round_lb(-state.ub, &ub_tightening_[local_col]);
    if (!attempt.allow_imprecise && bound_imprecise) {
      return ScalingOutcome::kNeedMorePrecision;
    }
    new_state.is_integer = true;
    if (new_state.shift != 0.0) new_state.shift *= m_ * col_s;
    new_state.scaling *= m_ * col_s;
    new_state.error_lb = 0.0;
    new_state.error_ub = 0.0;
    if (is_too_large(new_state.lb) || is_too_large(new_state.ub) ||
        !(new_state.scaling <= max_var_bound)) {
      return ScalingOutcome::kOverflow;
    }
    if (new_state.lb > new_state.ub) {
      return ScalingOutcome::kFailure;
    }

    // The constraints are scaled for the final domain of the variable: move the
    // domain as close to zero as possible via FinalCpSatShiftValue().
    if (new_state.shift == 0.0) {
      ShiftVariable(FinalCpSatShiftValue(new_state.lb, new_state.ub,
                                         /*is_integer=*/true, wanted_tol_),
                    &new_state);
    }
  }
  return ScalingOutcome::kSuccess;
}

ScalingOutcome ModularComponentConverter::ScaleRow(const Attempt& attempt,
                                                   int local_row) {
  const EnforcedLinearConstraintProto& ct =
      mp_model_.constraints(compo_.constraints[local_row]);
  const double factor = m_ * attempt.s / scaling_.row_scale[local_row];
  if (!std::isfinite(factor)) return ScalingOutcome::kOverflow;
  if (!(factor > 0.0)) return ScalingOutcome::kNeedMorePrecision;
  ScaledLinearConstraint& row = rows_[local_row];
  row = ScaledLinearConstraint{.scaling_factor = factor};

  const int64_t scaling_target = int64_t{1}
                                 << params_.mip_max_activity_exponent();
  bounded_expr_.Clear();
  coeffs_lb_.clear();
  coeffs_ub_.clear();
  int_lbs_.clear();
  int_ubs_.clear();
  LinearOverflowChecker checker;
  ExactActivityBounds exact_activity;
  TightSum neg_offset_lb;
  TightSum neg_offset_ub;
  TightSum disc_lb;
  TightSum disc_ub;
  TightSum clamp_lb;
  TightSum clamp_ub;
  double min_activity = 0.0;
  double max_activity = 0.0;
  bool is_constant = true;
  for (int i = 0; i < ct.vars().size(); ++i) {
    if (ct.coeffs(i) == 0.0) continue;
    const int var = ct.vars(i);
    const int local_col = compo_.var_to_local_col[var];
    const bool is_external = local_col == -1;
    const MipVariableState& state =
        is_external ? states_[var] : new_states_[local_col];

    const double coeff = ct.coeffs(i) / state.scaling;
    const auto [c_lb, c_ub] = QuotientBounds(ct.coeffs(i), state.scaling);

    if (state.shift != 0.0) {
      AddShiftedTermOffset(c_lb, c_ub, state.shift, &neg_offset_lb,
                           &neg_offset_ub);
    }

    AddTermErrorBounds(c_lb, c_ub, state.error_lb, state.error_ub, &disc_lb,
                       &disc_ub);
    if (!is_external && (lb_tightening_[local_col] != 0.0 ||
                         ub_tightening_[local_col] != 0.0)) {
      if (!std::isfinite(c_lb) || !std::isfinite(c_ub)) {
        return ScalingOutcome::kOverflow;
      }
      const double sc_lb = ProdBounds(c_lb, factor).first;
      const double sc_ub = ProdBounds(c_ub, factor).second;
      if (!std::isfinite(sc_lb) || !std::isfinite(sc_ub)) {
        return ScalingOutcome::kOverflow;
      }
      AddTermErrorBounds(sc_lb, sc_ub, -ub_tightening_[local_col],
                         lb_tightening_[local_col], &clamp_lb, &clamp_ub);
    }

    // Component variables already have exact integer bounds in `new_states_`;
    // external variables use their converted CP-SAT domain when populated (or
    // fall back to snapping `state` in standalone tests).
    double lb = state.lb;
    double ub = state.ub;
    if (is_external) {
      if (var < cp_model_->variables_size() &&
          cp_model_->variables(var).domain_size() == 2) {
        lb = static_cast<double>(cp_model_->variables(var).domain(0));
        ub = static_cast<double>(cp_model_->variables(var).domain(1));
      } else {
        lb = std::ceil(state.lb - std::max(wanted_tol_, -state.error_lb));
        ub = std::floor(state.ub + std::max(wanted_tol_, state.error_ub));
      }
    }
    if (!std::isfinite(lb) || !std::isfinite(ub)) {
      return ScalingOutcome::kOverflow;
    }
    if (lb > ub) return ScalingOutcome::kFailure;
    if (lb == 0.0 && ub == 0.0) continue;
    is_constant = false;

    const double scaled_coeff = coeff * factor;
    if (!(std::abs(scaled_coeff) < static_cast<double>(kint64max))) {
      return ScalingOutcome::kOverflow;
    }
    const int64_t int_coeff = std::llround(scaled_coeff);
    if (is_external) {
      if (int_coeff % modular_factor_ != 0) {
        return ScalingOutcome::kNeedMorePrecision;
      }
    } else if (std::abs(int_coeff) != 1) {
      return ScalingOutcome::kFailure;
    }
    if (!(lb >= static_cast<double>(kint64min) &&
          ub < static_cast<double>(kint64max))) {
      return ScalingOutcome::kOverflow;
    }
    const int64_t int_lb = static_cast<int64_t>(lb);
    const int64_t int_ub = static_cast<int64_t>(ub);
    if (!checker.AddTerm(int_coeff, int_lb, int_ub) ||
        -checker.sum_min > scaling_target || checker.sum_max > scaling_target) {
      return ScalingOutcome::kOverflow;
    }
    bounded_expr_.AddTerm(coeff, lb, ub);
    coeffs_lb_.push_back(c_lb);
    coeffs_ub_.push_back(c_ub);
    int_lbs_.push_back(int_lb);
    int_ubs_.push_back(int_ub);
    exact_activity.AddTerm(c_lb, c_ub, int_lb, int_ub);
    min_activity += coeff > 0.0 ? coeff * lb : coeff * ub;
    max_activity += coeff > 0.0 ? coeff * ub : coeff * lb;
    if (int_coeff == 0) continue;
    row.vars.push_back(var);
    row.coeffs.push_back(int_coeff);
    row.var_lbs.push_back(int_lb);
    row.var_ubs.push_back(int_ub);
  }

  if (!std::isfinite(clamp_lb.TightLowerBound()) ||
      !std::isfinite(clamp_ub.TightUpperBound())) {
    return ScalingOutcome::kOverflow;
  }

  // Shift and relax the constraint bounds by the discretization error of the
  // variables and by `mip_tight_precision`, like in
  // ConstraintScaler::ScaleAndAddConstraint().
  double ct_lb = ct.lower_bound();
  double ct_ub = ct.upper_bound();
  const double orig_lb_upper =
      std::isfinite(ct_lb)
          ? (neg_offset_ub.IsFinite()
                 ? TightSum(neg_offset_ub).Add(ct_lb).TightUpperBound()
                 : kInfinity)
          : ct_lb;
  const double orig_ub_lower =
      std::isfinite(ct_ub)
          ? (neg_offset_lb.IsFinite()
                 ? TightSum(neg_offset_lb).Add(ct_ub).TightLowerBound()
                 : -kInfinity)
          : ct_ub;
  const double max_disc_snap =
      std::max(wanted_tol_, params_.mip_check_precision());
  double pre_tol_lb = ct_lb;
  if (std::isfinite(ct_lb)) {
    const double raw_lb = neg_offset_lb.Add(ct_lb).TightLowerBound();
    const double tol_lb = raw_lb - tight_tol_;
    row.discretization_error =
        std::max(row.discretization_error,
                 std::min(-disc_lb.TightLowerBound(), tol_lb - min_activity));
    neg_offset_lb.Add(disc_lb);
    pre_tol_lb = -disc_lb.TightLowerBound() <= max_disc_snap
                     ? raw_lb
                     : neg_offset_lb.TightLowerBound();
    ct_lb = neg_offset_lb.Add(-tight_tol_).TightLowerBound();
    if (min_activity >= ct_lb) ct_lb = -kInfinity;
  }
  double pre_tol_ub = ct_ub;
  if (std::isfinite(ct_ub)) {
    const double raw_ub = neg_offset_ub.Add(ct_ub).TightUpperBound();
    const double tol_ub = raw_ub + tight_tol_;
    row.discretization_error =
        std::max(row.discretization_error,
                 std::min(disc_ub.TightUpperBound(), max_activity - tol_ub));
    neg_offset_ub.Add(disc_ub);
    pre_tol_ub = disc_ub.TightUpperBound() <= max_disc_snap
                     ? raw_ub
                     : neg_offset_ub.TightUpperBound();
    ct_ub = neg_offset_ub.Add(tight_tol_).TightUpperBound();
    if (max_activity <= ct_ub) ct_ub = kInfinity;
  }
  if (is_constant) {
    // All the variables are fixed to zero: the constraint is either always
    // satisfied or infeasible.
    if (ct_lb != -kInfinity || ct_ub != kInfinity) {
      row.lb = 1;
      row.ub = 0;
    } else {
      row.total_unscaling_error =
          std::max(exact_activity.MaxLowerViolation(orig_lb_upper),
                   exact_activity.MaxUpperViolation(orig_ub_lower));
    }
    return ScalingOutcome::kSuccess;
  }
  if (ct_lb == kInfinity || ct_ub == -kInfinity) {
    return ScalingOutcome::kFailure;
  }
  const bool has_lb = std::isfinite(ct_lb);
  const bool has_ub = std::isfinite(ct_ub);
  if (!has_lb && !has_ub) {
    row.total_unscaling_error =
        std::max(exact_activity.MaxLowerViolation(orig_lb_upper),
                 exact_activity.MaxUpperViolation(orig_ub_lower));
    return ScalingOutcome::kSuccess;
  }
  if ((has_lb && !std::isfinite(pre_tol_lb)) ||
      (has_ub && !std::isfinite(pre_tol_ub))) {
    return ScalingOutcome::kOverflow;
  }

  tight_error_helper_.LoadUnscaledConstraint(bounded_expr_.coeffs, coeffs_lb_,
                                             coeffs_ub_, int_lbs_, int_ubs_);
  tight_error_helper_.LoadScalingFactor(factor);

  TightSum row_scaled_lb;
  TightSum row_pre_tol_lb;
  if (has_lb) {
    row_scaled_lb.AddProd(ct_lb, factor).Add(clamp_lb);
    row_pre_tol_lb.AddProd(pre_tol_lb, factor).Add(clamp_lb);
    const TightSum& lb_err =
        tight_error_helper_.GetRelaxationLowerBoundError(row_scaled_lb);
    row.rounding_error =
        std::max(row.rounding_error, -lb_err.TightLowerBound() / factor);
    row_scaled_lb.Add(lb_err);
    row_pre_tol_lb.Add(lb_err);
    if (!std::isfinite(row_scaled_lb.TightLowerBound()) ||
        !std::isfinite(row_pre_tol_lb.TightLowerBound())) {
      return ScalingOutcome::kOverflow;
    }
  }

  TightSum row_scaled_ub;
  TightSum row_pre_tol_ub;
  if (has_ub) {
    row_scaled_ub.AddProd(ct_ub, factor).Add(clamp_ub);
    row_pre_tol_ub.AddProd(pre_tol_ub, factor).Add(clamp_ub);
    const TightSum& ub_err =
        tight_error_helper_.GetRelaxationUpperBoundError(row_scaled_ub);
    row.rounding_error =
        std::max(row.rounding_error, ub_err.TightUpperBound() / factor);
    row_scaled_ub.Add(ub_err);
    row_pre_tol_ub.Add(ub_err);
    if (!std::isfinite(row_scaled_ub.TightUpperBound()) ||
        !std::isfinite(row_pre_tol_ub.TightUpperBound())) {
      return ScalingOutcome::kOverflow;
    }
  }

  // Rounds the bounds to multiples of `modular_factor` and returns by how much
  // this can tighten the constraint. With `include_tol`, the bounds are first
  // relaxed by `mip_tight_precision` and always rounded outward.
  double scaled_lb = -kInfinity;
  double scaled_ub = kInfinity;
  const auto round_bounds = [&](bool include_tol) {
    const double tol = (!include_tol && attempt.snap_within_tol)
                           ? (factor / m_) * tight_tol_
                           : 0.0;
    double d_lb = 0.0;
    double d_ub = 0.0;
    if (has_lb) {
      const double base_lb =
          (include_tol ? row_scaled_lb : row_pre_tol_lb).TightLowerBound();
      scaled_lb =
          m_ * RoundScaledLowerBound(QuotientBounds(base_lb, m_).first, tol);
      d_lb = std::max(0.0, TightSum()
                               .Add(scaled_lb)
                               .Add(-row_scaled_lb.TightLowerBound())
                               .TightUpperBound());
    }
    if (has_ub) {
      const double base_ub =
          (include_tol ? row_scaled_ub : row_pre_tol_ub).TightUpperBound();
      scaled_ub =
          m_ * RoundScaledUpperBound(QuotientBounds(base_ub, m_).second, tol);
      d_ub = std::max(0.0, TightSum()
                               .Add(row_scaled_ub.TightUpperBound())
                               .Add(-scaled_ub)
                               .TightUpperBound());
    }
    return std::max(d_lb, d_ub);
  };

  // The total tightening over the component must stay strictly below one.
  double tightening = round_bounds(/*include_tol=*/false);
  if (scaled_lb > scaled_ub ||
      !(TightSum(total_tightening_).Add(tightening).TightUpperBound() < 1.0)) {
    any_row_relaxed_ = true;
    tightening = round_bounds(/*include_tol=*/true);
  }
  total_tightening_.Add(tightening);

  const auto fits_int64 = [](double x) {
    return !std::isfinite(x) || (x >= static_cast<double>(kint64min) &&
                                 x < static_cast<double>(kint64max));
  };
  if (!fits_int64(scaled_lb) || !fits_int64(scaled_ub)) {
    return ScalingOutcome::kOverflow;
  }
  if (scaled_lb > scaled_ub || !(total_tightening_.TightUpperBound() < 1.0)) {
    return ScalingOutcome::kFailure;
  }

  double max_coeff_unround = 0.0;
  if (has_lb) {
    const double lb = std::max(scaled_lb, min_activity * factor);
    const double coeff_unround =
        tight_error_helper_.GetLowerBoundUnroundingError(lb) / factor;
    max_coeff_unround = std::max(max_coeff_unround, coeff_unround);
    row.unrounding_error = std::max(
        row.unrounding_error,
        coeff_unround +
            std::max(0.0, (row_scaled_lb.TightUpperBound() - lb) / factor));
  }
  if (has_ub) {
    const double ub = std::min(scaled_ub, max_activity * factor);
    const double coeff_unround =
        tight_error_helper_.GetUpperBoundUnroundingError(ub) / factor;
    max_coeff_unround = std::max(max_coeff_unround, coeff_unround);
    row.unrounding_error = std::max(
        row.unrounding_error,
        coeff_unround +
            std::max(0.0, (ub - row_scaled_ub.TightLowerBound()) / factor));
  }
  const double row_error = attempt.allow_imprecise
                               ? row.rounding_error + max_coeff_unround
                               : row.rounding_error + row.unrounding_error;
  if (row_error > wanted_tol_) return ScalingOutcome::kNeedMorePrecision;
  double scaled_sum_error;
  ComputeScalingErrors(bounded_expr_, factor, &row.relative_coeff_error,
                       &scaled_sum_error);
  row.absolute_rhs_error = scaled_sum_error / factor;
  row.lb = SafeDoubleToInt64(scaled_lb);
  row.ub = SafeDoubleToInt64(scaled_ub);
  row.total_unscaling_error = ComputeBoundedRowTotalUnscalingError(
      factor, /*gcd=*/1, row.lb, row.ub, orig_lb_upper, orig_ub_lower,
      exact_activity, &tight_error_helper_);
  return ScalingOutcome::kSuccess;
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
               stats.max_var_bound_error,
               ", objective error bound: ", stats.objective_error_bound, ").");
  }
  if (stats.num_relaxed_modular_integers > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_relaxed_modular_integers,
               " variables of ", stats.num_relaxed_modular_components,
               " components made integer with exact rows and bounds rounded "
               "outward, scaling in [",
               stats.min_relaxed_modular_scaling, ", ",
               stats.max_relaxed_modular_scaling, "].");
  }
  LogTumBoundTighteningStats(stats, logger);
}

bool DetectAndConvertTotallyUnimodularComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    ConstraintScaler* scaler, CpModelProto* cp_model, TumStats* stats) {
  CHECK_EQ(compo.var_to_local_col.size(), mp_model.num_variables());
  CHECK_EQ(states.size(), mp_model.num_variables());
  if (compo.continuous_vars.empty() ||
      params.keep_all_feasible_solutions_in_presolve() ||
      !params.mip_automatically_scale_variables()) {
    return false;
  }

  ++stats->num_components;
  const ComponentScaling scaling =
      DetectComponentModularity(compo, mp_model, params.mip_wanted_precision());
  if (scaling.status == ModularityStatus::kUnknown) return false;
  if (scaling.status == ModularityStatus::kTotallyUnimodular) {
    ++stats->num_tum_components;
  } else {
    ++stats->num_2_modular_components;
  }

  TightenComponentBounds(compo, scaling, mp_model, params.mip_tight_precision(),
                         states, stats);
  return ModularComponentConverter(params, compo, scaling, mp_model, states,
                                   scaler, cp_model, stats)
      .Convert();
}

void TightenResidualTotallyUnimodularVariableBounds(
    const SatParameters& params, MPModelProtoWrapper* mp_model,
    SolverLogger* logger) {
  if (params.keep_all_feasible_solutions_in_presolve()) return;

  const int num_vars = mp_model->num_variables();
  std::vector<MipVariableState> states = InitialVariableStates(*mp_model);

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
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> residual_var_indices;
  std::vector<int> var_to_local_col_buffer(num_vars, -1);
  const double tolerance = params.mip_wanted_precision();
  const double bound_tol =
      params.mip_proper_relaxation() ? params.mip_tight_precision() : 0.0;
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
    // components, then tighten each valid TUM component independently.
    std::vector<ResidualContinuousComponent> compos =
        PartitionResidualComponents(*mp_model, residual_var_indices,
                                    to_dense_index);
    for (ResidualContinuousComponent& compo : compos) {
      const ScopedVarToLocalCol scoped(&compo, &var_to_local_col_buffer);
      const ComponentScaling scaling =
          DetectComponentModularity(compo, *mp_model, tolerance);
      if (scaling.status == ModularityStatus::kUnknown) continue;
      if (include_large_integers) {
        const bool has_integer_vars = std::any_of(
            compo.continuous_vars.begin(), compo.continuous_vars.end(),
            [&states](int var) { return states[var].is_integer; });
        if (has_integer_vars &&
            !ComponentHasIntegralVertices(compo, scaling, *mp_model, states,
                                          tolerance)) {
          continue;
        }
      }
      TightenComponentBounds(compo, scaling, *mp_model, bound_tol,
                             absl::MakeSpan(states), &stats);
    }
  }
  LogTumBoundTighteningStats(stats, logger);

  // Only the bounds can change.
  for (int var = 0; var < num_vars; ++var) {
    MipVariableProto* mp_var = mp_model->mutable_variables(var);
    mp_var->set_lower_bound(states[var].lb);
    mp_var->set_upper_bound(states[var].ub);
  }
}

}  // namespace sat
}  // namespace operations_research
