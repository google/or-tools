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

#include "ortools/math_opt/solvers/glpk_solver.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>  // IWYU pragma: keep
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/glpk/glpk_env_deleter.h"
#include "ortools/glpk/glpk_formatters.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/empty_bounds.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_submatrix.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/glpk.pb.h"
#include "ortools/math_opt/solvers/glpk/gap.h"
#include "ortools/math_opt/solvers/glpk/glpk_sparse_vector.h"
#include "ortools/math_opt/solvers/glpk/rays.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

constexpr SupportedProblemStructures kGlpkSupportedStructures = {
    .integer_variables = SupportType::kSupported};

// Bounds of rows or columns.
struct Bounds {
  double lower = -kInf;
  double upper = kInf;
};

// Sets either a row or a column bounds. The index k is the one-based index of
// the row or the column.
//
// The Dimension type should be either GlpkSolver::Variable or
// GlpkSolver::LinearConstraints.
//
// When Dimension::IsInteger() returns true, the bounds are rounded before being
// applied which is mandatory for integer variables (solvers fail if a model
// contains non-integer bounds for integer variables). Thus the integrality of
// variables must be set/updated before calling this function.
template <typename Dimension>
void SetBounds(glp_prob* const problem, const int k, const Bounds& bounds) {
  // GLPK wants integer bounds for integer variables.
  const bool is_integer = Dimension::IsInteger(problem, k);
  const double lb = is_integer ? std::ceil(bounds.lower) : bounds.lower;
  const double ub = is_integer ? std::floor(bounds.upper) : bounds.upper;
  int type = GLP_FR;
  if (std::isinf(lb) && std::isinf(ub)) {
    type = GLP_FR;
  } else if (std::isinf(lb)) {
    type = GLP_UP;
  } else if (std::isinf(ub)) {
    type = GLP_LO;
  } else if (lb == ub) {
    type = GLP_FX;
  } else {  // Bounds not inf and not equal.
    type = GLP_DB;
  }
  Dimension::kSetBounds(problem, k, type, lb, ub);
}

// Gets either a row or a column bounds. The index k is the one-based index of
// the row or the column.
//
// The Dimension type should be either GlpkSolver::Variable or
// GlpkSolver::LinearConstraints.
template <typename Dimension>
Bounds GetBounds(glp_prob* const problem, const int k) {
  const int type = Dimension::kGetType(problem, k);
  switch (type) {
    case GLP_FR:
      return {};
    case GLP_LO:
      return {.lower = Dimension::kGetLb(problem, k)};
    case GLP_UP:
      return {.upper = Dimension::kGetUb(problem, k)};
    case GLP_DB:
    case GLP_FX:
      return {.lower = Dimension::kGetLb(problem, k),
              .upper = Dimension::kGetUb(problem, k)};
    default:
      LOG(FATAL) << type;
  }
}

// Updates the bounds of either rows or columns.
//
// The Dimension type should be either GlpkSolver::Variable or
// GlpkSolver::LinearConstraints.
//
// When Dimension::IsInteger() returns true, the bounds are rounded before being
// applied which is mandatory for integer variables (solvers fail if a model
// contains non-integer bounds for integer variables). Thus the integrality of
// variables must be updated before calling this function.
template <typename Dimension>
void UpdateBounds(glp_prob* const problem, const Dimension& dimension,
                  const SparseDoubleVectorProto& lower_bounds_proto,
                  const SparseDoubleVectorProto& upper_bounds_proto) {
  const auto lower_bounds = MakeView(lower_bounds_proto);
  const auto upper_bounds = MakeView(upper_bounds_proto);

  auto current_lower_bound = lower_bounds.begin();
  auto current_upper_bound = upper_bounds.begin();
  for (;;) {
    // Get the smallest unvisited id from either sparse container.
    std::optional<int64_t> next_id;
    if (current_lower_bound != lower_bounds.end()) {
      if (!next_id.has_value() || current_lower_bound->first < *next_id) {
        next_id = current_lower_bound->first;
      }
    }
    if (current_upper_bound != upper_bounds.end()) {
      if (!next_id.has_value() || current_upper_bound->first < *next_id) {
        next_id = current_upper_bound->first;
      }
    }

    if (!next_id.has_value()) {
      // We exhausted all collections.
      break;
    }

    // Find the corresponding row or column.
    const int row_or_col_index = dimension.id_to_index.at(*next_id);
    CHECK_EQ(dimension.ids[row_or_col_index - 1], *next_id);

    // Get the updated values for bounds and move the iterator for consumed
    // updates.
    Bounds bounds = GetBounds<Dimension>(problem,
                                         /*k=*/row_or_col_index);
    if (current_lower_bound != lower_bounds.end() &&
        current_lower_bound->first == *next_id) {
      bounds.lower = current_lower_bound->second;
      ++current_lower_bound;
    }
    if (current_upper_bound != upper_bounds.end() &&
        current_upper_bound->first == *next_id) {
      bounds.upper = current_upper_bound->second;
      ++current_upper_bound;
    }
    SetBounds<Dimension>(problem, /*k=*/row_or_col_index,
                         /*bounds=*/bounds);
  }

  CHECK(current_lower_bound == lower_bounds.end());
  CHECK(current_upper_bound == upper_bounds.end());
}

// Deletes in-place the data corresponding to the indices of rows/cols.
//
// The vector of one-based indices sorted_deleted_rows_or_cols is expected to be
// sorted and its first element of index 0 is ignored (this is the GLPK
// convention).
template <typename V>
void DeleteRowOrColData(std::vector<V>& data,
                        absl::Span<const int> sorted_deleted_rows_or_cols) {
  if (sorted_deleted_rows_or_cols.empty()) {
    // Avoid looping when not necessary.
    return;
  }

  std::size_t next_insertion_point = 0;
  std::size_t current_row_or_col = 0;
  for (std::size_t i = 1; i < sorted_deleted_rows_or_cols.size(); ++i) {
    const int deleted_row_or_col = sorted_deleted_rows_or_cols[i];
    for (; current_row_or_col + 1 < deleted_row_or_col;
         ++current_row_or_col, ++next_insertion_point) {
      DCHECK_LT(current_row_or_col, data.size());
      data[next_insertion_point] = data[current_row_or_col];
    }
    // Skip the deleted row/col.
    ++current_row_or_col;
  }
  for (; current_row_or_col < data.size();
       ++current_row_or_col, ++next_insertion_point) {
    data[next_insertion_point] = data[current_row_or_col];
  }
  data.resize(next_insertion_point);
}

// Deletes the row or cols of the GLPK problem and returns their indices. As a
// side effect it updates dimension.ids and dimension.id_to_index.
//
// The Dimension type should be either GlpkSolver::Variable or
// GlpkSolver::LinearConstraints.
//
// The returned vector is sorted and the first element (index 0) must be ignored
// (this is the GLPK convention). It can be used with DeleteRowOrColData().
template <typename Dimension>
std::vector<int> DeleteRowsOrCols(
    glp_prob* const problem, Dimension& dimension,
    const google::protobuf::RepeatedField<int64_t>& deleted_ids) {
  if (deleted_ids.empty()) {
    // This is not only an optimization. Functions glp_del_rows() and
    // glp_del_cols() fails if the number of deletion is 0.
    return {};
  }

  // Delete GLPK rows or columns.
  std::vector<int> deleted_rows_or_cols;
  // Functions glp_del_rows() and glp_del_cols() only use values in ranges
  // [1,n]. The first element is not used.
  deleted_rows_or_cols.reserve(deleted_ids.size() + 1);
  deleted_rows_or_cols.push_back(-1);
  for (const int64_t deleted_id : deleted_ids) {
    deleted_rows_or_cols.push_back(dimension.id_to_index.at(deleted_id));
  }
  Dimension::kDelElts(problem, deleted_rows_or_cols.size() - 1,
                      deleted_rows_or_cols.data());

  // Since deleted_ids are in strictly increasing order and we allocate
  // rows/cols in orders of MathOpt ids; deleted_rows_or_cols should also be
  // sorted.
  CHECK(
      std::is_sorted(deleted_rows_or_cols.begin(), deleted_rows_or_cols.end()));

  // Update the ids vector.
  DeleteRowOrColData(dimension.ids, deleted_rows_or_cols);

  // Update the id_to_index map.
  for (const int64_t deleted_id : deleted_ids) {
    CHECK(dimension.id_to_index.erase(deleted_id));
  }
  for (int i = 0; i < dimension.ids.size(); ++i) {
    dimension.id_to_index.at(dimension.ids[i]) = i + 1;
  }

  return deleted_rows_or_cols;
}

// Translates the input MathOpt indices in row/column GLPK indices to use with
// glp_load_matrix(). The returned vector first element is always 0 and unused
// as it is required by GLPK (which uses one-based indices for arrays as well).
//
// The id_to_index is supposed to contain GLPK's one-based indices for rows and
// columns.
std::vector<int> MatrixIds(
    const google::protobuf::RepeatedField<int64_t>& proto_ids,
    const absl::flat_hash_map<int64_t, int>& id_to_index) {
  std::vector<int> ids;
  ids.reserve(proto_ids.size() + 1);
  // First item (index 0) is not used by GLPK.
  ids.push_back(0);
  for (const int64_t proto_id : proto_ids) {
    ids.push_back(id_to_index.at(proto_id));
  }
  return ids;
}

// Returns a vector of coefficients starting at index 1 (as used by GLPK) to use
// with glp_load_matrix(). The returned vector first element is always 0 and it
// is ignored by GLPK.
std::vector<double> MatrixCoefficients(
    const google::protobuf::RepeatedField<double>& proto_coeffs) {
  std::vector<double> coeffs(proto_coeffs.size() + 1);
  // First item (index 0) is not used by GLPK.
  coeffs[0] = 0.0;
  std::copy(proto_coeffs.begin(), proto_coeffs.end(), coeffs.begin() + 1);
  return coeffs;
}

// Returns true if the input GLPK problem contains integer variables.
bool IsMip(glp_prob* const problem) {
  const int num_vars = glp_get_num_cols(problem);
  for (int v = 1; v <= num_vars; ++v) {
    if (glp_get_col_kind(problem, v) != GLP_CV) {
      return true;
    }
  }
  return false;
}

// Returns true if the input GLPK problem has no rows and no cols.
bool IsEmpty(glp_prob* const problem) {
  return glp_get_num_cols(problem) == 0 && glp_get_num_rows(problem) == 0;
}

// Returns a sparse vector with the values returned by the getter for the input
// ids and taking into account the provided filter.
SparseDoubleVectorProto FilteredVector(glp_prob* const problem,
                                       const SparseVectorFilterProto& filter,
                                       absl::Span<const int64_t> ids,
                                       double (*const getter)(glp_prob*, int)) {
  SparseDoubleVectorProto vec;
  vec.mutable_ids()->Reserve(ids.size());
  vec.mutable_values()->Reserve(ids.size());

  SparseVectorFilterPredicate predicate(filter);
  for (int i = 0; i < ids.size(); ++i) {
    const double value = getter(problem, i + 1);
    if (predicate.AcceptsAndUpdate(ids[i], value)) {
      vec.add_ids(ids[i]);
      vec.add_values(value);
    }
  }
  return vec;
}

// Returns the ray data the corresponds to element id having the given value and
// all other elements of ids having 0.
SparseDoubleVectorProto FilteredRay(const SparseVectorFilterProto& filter,
                                    absl::Span<const int64_t> ids,
                                    absl::Span<const double> values) {
  CHECK_EQ(ids.size(), values.size());
  SparseDoubleVectorProto vec;
  SparseVectorFilterPredicate predicate(filter);
  for (int i = 0; i < ids.size(); ++i) {
    if (predicate.AcceptsAndUpdate(ids[i], values[i])) {
      vec.add_ids(ids[i]);
      vec.add_values(values[i]);
    }
  }
  return vec;
}

// Sets the parameters shared between MIP and LP and returns warnings for bad
// parameters.
//
// The input Parameters type should be glp_smcp (for LP), glp_iptcp (for LP with
// interior point) or glp_iocp (for MIP).
template <typename Parameters>
absl::Status SetSharedParameters(const SolveParametersProto& parameters,
                                 const bool has_message_callback,
                                 Parameters& glpk_parameters) {
  std::vector<std::string> warnings;
  if (parameters.has_threads() && parameters.threads() > 1) {
    warnings.push_back(
        absl::StrCat("GLPK only supports parameters.threads = 1; value ",
                     parameters.threads(), " is not supported"));
  }
  if (parameters.enable_output() || has_message_callback) {
    glpk_parameters.msg_lev = GLP_MSG_ALL;
  } else {
    glpk_parameters.msg_lev = GLP_MSG_OFF;
  }
  if (parameters.has_node_limit()) {
    warnings.push_back("parameter node_limit not supported by GLPK");
  }
  if (parameters.has_objective_limit()) {
    warnings.push_back("parameter objective_limit not supported by GLPK");
  }
  if (parameters.has_best_bound_limit()) {
    warnings.push_back("parameter best_bound_limit not supported by GLPK");
  }
  if (parameters.has_cutoff_limit()) {
    warnings.push_back("parameter cutoff_limit not supported by GLPK");
  }
  if (parameters.has_solution_limit()) {
    warnings.push_back("parameter solution_limit not supported by GLPK");
  }
  if (parameters.has_random_seed()) {
    warnings.push_back("parameter random_seed not supported by GLPK");
  }
  if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter cuts not supported by GLPK");
  }
  if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter heuristics not supported by GLPK");
  }
  if (parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter scaling not supported by GLPK");
  }
  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return absl::OkStatus();
}

// Sets the time limit parameter which is only supported by some LP algorithm
// and MIP, but not by interior point.
//
// The input Parameters type should be glp_smcp (for LP), or glp_iocp (for MIP).
template <typename Parameters>
void SetTimeLimitParameter(const SolveParametersProto& parameters,
                           Parameters& glpk_parameters) {
  if (parameters.has_time_limit()) {
    const int64_t time_limit_ms = absl::ToInt64Milliseconds(
        util_time::DecodeGoogleApiProto(parameters.time_limit()).value());
    glpk_parameters.tm_lim = static_cast<int>(std::min(
        static_cast<int64_t>(std::numeric_limits<int>::max()), time_limit_ms));
  }
}

// Sets the LP specific parameters and returns an InvalidArgumentError for
// invalid parameters or parameter values.
absl::Status SetLPParameters(const SolveParametersProto& parameters,
                             glp_smcp& glpk_parameters) {
  std::vector<std::string> warnings;
  if (parameters.has_iteration_limit()) {
    int limit = static_cast<int>(std::min<int64_t>(
        std::numeric_limits<int>::max(), parameters.iteration_limit()));
    glpk_parameters.it_lim = limit;
  }
  switch (parameters.presolve()) {
    case EMPHASIS_UNSPECIFIED:
      // Keep the default.
      //
      // TODO(b/187027049): default is off, which may be surprising for users.
      break;
    case EMPHASIS_OFF:
      glpk_parameters.presolve = GLP_OFF;
      break;
    default:
      glpk_parameters.presolve = GLP_ON;
      break;
  }
  switch (parameters.lp_algorithm()) {
    case LP_ALGORITHM_UNSPECIFIED:
      break;
    case LP_ALGORITHM_PRIMAL_SIMPLEX:
      glpk_parameters.meth = GLP_PRIMAL;
      break;
    case LP_ALGORITHM_DUAL_SIMPLEX:
      // Use GLP_DUALP to switch back to primal simplex if the dual simplex
      // fails.
      //
      // TODO(b/187027049): GLPK also supports GLP_DUAL to only try dual
      // simplex. We should have an option to support it.
      glpk_parameters.meth = GLP_DUALP;
      break;
    default:
      warnings.push_back(absl::StrCat(
          "GLPK does not support ",
          operations_research::ProtoEnumToString(parameters.lp_algorithm()),
          " for parameters.lp_algorithm"));
      break;
  }
  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return absl::OkStatus();
}

class MipCallbackData {
 public:
  explicit MipCallbackData(const SolveInterrupter* const interrupter)
      : interrupter_(interrupter) {}

  void Callback(glp_tree* const tree) {
    // We only update the best bound on some specific events since it makes a
    // traversal of all active nodes.
    switch (glp_ios_reason(tree)) {
      case GLP_ISELECT:
        // The ISELECT call is the first one that happens after a node has been
        // split on two sub-nodes (IBRANCH) with updated `bound`s based on the
        // integer value of the branched variable.
      case GLP_IBINGO:
        // We found a new integer solution, the `bound` has been updated.
      case GLP_IROWGEN:
        // The IROWGEN call is the first one that happens on a current node
        // after the relaxed problem has been solved and the `bound` field
        // updated.
        //
        // Note that the model/cut pool changes done in IROWGEN and ICUTGEN have
        // no influence on the `bound` and IROWGEN is the first call to happen.
        if (const int best_node = glp_ios_best_node(tree); best_node != 0) {
          best_bound_ = glp_ios_node_bound(tree, best_node);
        }
        break;
      default:
        // We can ignore:
        // - IPREPRO: since the `bound` of the current node has not been
        //     computed yet.
        // - IHEUR: since we have IBINGO if the integer solution is better.
        // - ICUTGEN: since the `bound` is not updated with the rows added at
        //     IROWGEN so we would get the same best bound.
        // - IBRANCH: since the sub-nodes will be created after that and their
        //     `bound`s taken into account at ISELECT.
        break;
    }
    if (interrupter_ != nullptr && interrupter_->IsInterrupted()) {
      glp_ios_terminate(tree);
      interrupted_by_interrupter_ = true;
      return;
    }
  }

  bool HasBeenInterruptedByInterrupter() const {
    return interrupted_by_interrupter_.load();
  }

  std::optional<double> best_bound() const { return best_bound_; }

 private:
  // Optional interrupter.
  const SolveInterrupter* const interrupter_;

  // Set to true if glp_ios_terminate() has been called due to the interrupter.
  std::atomic<bool> interrupted_by_interrupter_ = false;

  // Set on each callback that may update the best bound.
  std::optional<double> best_bound_;
};

void MipCallback(glp_tree* const tree, void* const info) {
  static_cast<MipCallbackData*>(info)->Callback(tree);
}

// Returns the MathOpt ids of the rows/columns with lower_bound > upper_bound.
//
// For variables we use the unrounded bounds as we don't want to return a
// failing status when rounded bounds of integer variables cross due to the
// rounding. See EmptyIntegerBoundsResult() for dealing with this case.
InvertedBounds ListInvertedBounds(
    glp_prob* const problem, absl::Span<const int64_t> variable_ids,
    absl::Span<const double> unrounded_variable_lower_bounds,
    absl::Span<const double> unrounded_variable_upper_bounds,
    absl::Span<const int64_t> linear_constraint_ids) {
  InvertedBounds inverted_bounds;

  const int num_cols = glp_get_num_cols(problem);
  for (int c = 1; c <= num_cols; ++c) {
    if (unrounded_variable_lower_bounds[c - 1] >
        unrounded_variable_upper_bounds[c - 1]) {
      inverted_bounds.variables.push_back(variable_ids[c - 1]);
    }
  }

  const int num_rows = glp_get_num_rows(problem);
  for (int r = 1; r <= num_rows; ++r) {
    if (glp_get_row_lb(problem, r) > glp_get_row_ub(problem, r)) {
      inverted_bounds.linear_constraints.push_back(
          linear_constraint_ids[r - 1]);
    }
  }

  return inverted_bounds;
}

// Returns the termination reason based on the current MIP data of the problem
// assuming that the last call to glp_intopt() returned 0 and that the model has
// not been modified since.
absl::StatusOr<TerminationProto> MipTerminationOnSuccess(
    glp_prob* const problem, MipCallbackData* const mip_cb_data) {
  if (mip_cb_data == nullptr) {
    return absl::InternalError(
        "MipTerminationOnSuccess() called with nullptr mip_cb_data");
  }
  const int status = glp_mip_status(problem);
  const bool is_maximize = glp_get_obj_dir(problem) == GLP_MAX;
  switch (status) {
    case GLP_OPT:
    case GLP_FEAS: {
      const double objective_value = glp_mip_obj_val(problem);
      if (status == GLP_OPT) {
        // Note that here we don't use MipCallbackData->best_bound(), even if
        // set, as if the Gap was used to interrupt the solve GLPK is supposed
        // to return GLP_EMIPGAP and not 0. And thus we should not go through
        // this code path if the Gap limit is used.
        return OptimalTerminationProto(objective_value, objective_value);
      }
      return FeasibleTerminationProto(
          is_maximize, LIMIT_UNDETERMINED, objective_value,
          mip_cb_data->best_bound(), "glp_mip_status() returned GLP_FEAS");
    }
    case GLP_NOFEAS:
      // According to InfeasibleTerminationProto()'s documentation: "the
      // convention for infeasible MIPs is that dual_feasibility_status is
      // feasible".
      return InfeasibleTerminationProto(
          is_maximize, /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
    default:
      return absl::InternalError(
          absl::StrCat("glp_intopt() returned 0 but glp_mip_status()"
                       "returned the unexpected value ",
                       SolutionStatusString(status)));
  }
}

// Returns the termination reason based on the current interior point data of
// the problem assuming that the last call to glp_interior() returned 0 and that
// the model has not been modified since.
absl::StatusOr<TerminationProto> InteriorTerminationOnSuccess(
    glp_prob* const problem, MipCallbackData*) {
  const int status = glp_ipt_status(problem);
  const bool is_maximize = glp_get_obj_dir(problem) == GLP_MAX;
  switch (status) {
    case GLP_OPT: {
      const double objective_value = glp_ipt_obj_val(problem);
      // TODO(b/290359402): here we assume that the objective value of the dual
      // is exactly the same as the one of the primal. This may not be true as
      // some tolerance may apply.
      return OptimalTerminationProto(objective_value, objective_value);
    }
    case GLP_INFEAS:
      return NoSolutionFoundTerminationProto(
          is_maximize, LIMIT_UNDETERMINED,
          /*optional_dual_objective=*/std::nullopt,
          "glp_ipt_status() returned GLP_INFEAS");
    case GLP_NOFEAS:
      // Documentation in glpapi08.c for glp_ipt_status says this status means
      // "no feasible solution exists", but the Reference Manual for GLPK
      // Version 5.0 clarifies that it means "no feasible primal-dual solution
      // exists." (See also the comment in glpipm.c when ipm_solve returns 1).
      // Hence, GLP_NOFEAS corresponds to the solver claiming that either the
      // primal problem, the dual problem (or both) are infeasible. Under this
      // condition if the primal is feasible, then the dual must be infeasible
      // and hence the primal is unbounded.
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
    default:
      return absl::InternalError(
          absl::StrCat("glp_interior() returned 0 but glp_ipt_status()"
                       "returned the unexpected value ",
                       SolutionStatusString(status)));
  }
}

// Returns the termination reason based on the current interior point data of
// the problem assuming that the last call to glp_simplex() returned 0 and that
// the model has not been modified since.
absl::StatusOr<TerminationProto> SimplexTerminationOnSuccess(
    glp_prob* const problem, MipCallbackData*) {
  // Here we don't use glp_get_status() since it is biased towards the primal
  // simplex algorithm. For example if the dual simplex returns GLP_NOFEAS for
  // the dual and GLP_INFEAS for the primal then glp_get_status() returns
  // GLP_INFEAS. This is misleading since the dual successfully determined that
  // the problem was dual infeasible. So here we use the two statuses of the
  // primal and the dual to get a better result (the glp_get_status() only
  // combines them anyway, it does not have any other benefit).
  const int prim_status = glp_get_prim_stat(problem);
  const int dual_status = glp_get_dual_stat(problem);
  const bool is_maximize = glp_get_obj_dir(problem) == GLP_MAX;

  // Returns a status error indicating that glp_get_dual_stat() returned an
  // unexpected value.
  const auto unexpected_dual_stat = [&]() -> absl::Status {
    return util::InternalErrorBuilder()
           << "glp_simplex() returned 0 but glp_get_dual_stat() returned the "
              "unexpected value "
           << SolutionStatusString(dual_status)
           << " while glp_get_prim_stat() returned "
           << SolutionStatusString(prim_status);
  };

  switch (prim_status) {
    case GLP_FEAS:
      switch (dual_status) {
        case GLP_FEAS: {
          // Dual feasibility here means that the solution is dual feasible
          // (correct signs of the residual costs) and that the complementary
          // slackness condition are respected. Hence the solution is optimal.
          const double objective_value = glp_get_obj_val(problem);
          return OptimalTerminationProto(objective_value, objective_value);
        }
        case GLP_NOFEAS:
          return UnboundedTerminationProto(is_maximize);
        case GLP_INFEAS:
        default:
          return unexpected_dual_stat();
      }
    case GLP_INFEAS:
      switch (dual_status) {
        case GLP_NOFEAS:
          return InfeasibleOrUnboundedTerminationProto(
              is_maximize,
              /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
        case GLP_FEAS:
        case GLP_INFEAS:
        default:
          return unexpected_dual_stat();
      }
    case GLP_NOFEAS:
      switch (dual_status) {
        case GLP_FEAS:
          // Dual being feasible (GLP_FEAS) here would lead to dual unbounded;
          // but this does not exist as a reason.
          return InfeasibleTerminationProto(
              is_maximize,
              /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
        case GLP_INFEAS:
          return InfeasibleTerminationProto(
              is_maximize,
              /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
          return unexpected_dual_stat();
        case GLP_NOFEAS:
          // If both the primal and dual are proven infeasible (GLP_NOFEAS), the
          // primal wins. Maybe GLPK does never return that though since it
          // implements either primal or dual simplex algorithm but does not
          // combine both of them.
          return InfeasibleTerminationProto(
              is_maximize,
              /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
        default:
          return unexpected_dual_stat();
      }
    default:
      return absl::InternalError(
          absl::StrCat("glp_simplex() returned 0 but glp_get_prim_stat() "
                       "returned the unexpected value ",
                       SolutionStatusString(prim_status)));
  }
}

// Function called by BuildTermination() when the return code of the solve
// function is 0.
//
// Parameter mip_cb_data is not nullptr iff glp_intopt() was used.
using TerminationOnSuccessFn = std::function<absl::StatusOr<TerminationProto>(
    glp_prob* problem, MipCallbackData* const mip_cb_data)>;

// Returns the termination reason based on the return code rc of calling fn_name
// function which is glp_simplex(), glp_interior() or glp_intopt().
//
// For return code 0 which means successful solve, the function
// termination_on_success is called to build the termination. Other return
// values (errors) are dealt with specifically.
//
// For glp_intopt(), the optional mip_cb_data is used to test for interruption
// and the LIMIT_INTERRUPTED is set if the interrupter has been triggered (even
// if the return code is 0). It is also used to get the dual bound. The
// gap_limit must also be set to the gap limit parameter.
//
// When a primal feasible solution exists, feasible_solution_objective_value
// must be set with its objective. When this variable is unset this function
// assumes no such solution exists.
absl::StatusOr<TerminationProto> BuildTermination(
    glp_prob* const problem, const absl::string_view fn_name, const int rc,
    const TerminationOnSuccessFn termination_on_success,
    MipCallbackData* const mip_cb_data,
    const std::optional<double> feasible_solution_objective_value,
    const double gap_limit) {
  const bool is_maximize = glp_get_obj_dir(problem) == GLP_MAX;
  if (mip_cb_data != nullptr &&
      mip_cb_data->HasBeenInterruptedByInterrupter()) {
    return LimitTerminationProto(is_maximize, LIMIT_INTERRUPTED,
                                 feasible_solution_objective_value);
  }

  // TODO(b/187027049): see if GLP_EOBJLL and GLP_EOBJUL should be handled with
  // dual simplex.
  switch (rc) {
    case 0:
      return termination_on_success(problem, mip_cb_data);
    case GLP_EBOUND: {
      // GLP_EBOUND is returned when a variable or a constraint has the GLP_DB
      // bounds type and lower_bound >= upper_bound. The code in this file makes
      // sure we don't use GLP_DB but GLP_FX when lower_bound == upper_bound
      // thus we expect GLP_EBOUND only when lower_bound > upper_bound. This
      // should never happen as we call ListInvertedBounds() and
      // EmptyIntegerBoundsResult() before we call GLPK. Thus we don't expect
      // GLP_EBOUND to happen.
      return util::InternalErrorBuilder()
             << fn_name << "() returned `" << ReturnCodeString(rc)
             << "` but the model does not contain variables with inverted "
                "bounds";
    }
    case GLP_EITLIM:
      return LimitTerminationProto(is_maximize, LIMIT_ITERATION,
                                   feasible_solution_objective_value);
    case GLP_ETMLIM:
      return LimitTerminationProto(is_maximize, LIMIT_TIME,
                                   feasible_solution_objective_value);
    case GLP_EMIPGAP: {
      if (!feasible_solution_objective_value.has_value()) {
        return util::InternalErrorBuilder()
               << fn_name << "() returned `" << ReturnCodeString(rc)
               << "` but glp_mip_status() returned "
               << SolutionStatusString(glp_mip_status(problem));
      }
      if (!mip_cb_data) {
        return util::InternalErrorBuilder()
               << fn_name << "() returned `" << ReturnCodeString(rc)
               << "` but there is no MipCallbackData";
      }
      const double objective_value = feasible_solution_objective_value.value();
      // Here we expect mip_cb_data->best_bound() to always be set. If this is
      // not the case we use a worst estimation of the dual bound.
      return OptimalTerminationProto(
          objective_value,
          mip_cb_data->best_bound().value_or(
              WorstGLPKDualBound(is_maximize, objective_value, gap_limit)));
    }
    case GLP_ESTOP:
      return LimitTerminationProto(is_maximize, LIMIT_INTERRUPTED,
                                   feasible_solution_objective_value);
    case GLP_ENOPFS:
      // With presolve on, this error is returned if the LP has no feasible
      // solution.
      return InfeasibleTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
    case GLP_ENODFS:
      // With presolve on, this error is returned if the LP has no dual
      // feasible solution.
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
    case GLP_ENOCVG:
      // Very slow convergence/divergence (for glp_interior).
      return LimitTerminationProto(is_maximize, LIMIT_SLOW_PROGRESS,
                                   feasible_solution_objective_value);
    case GLP_EINSTAB:
      // Numeric stability solving Newtonian system (for glp_interior).
      return TerminateForReason(
          is_maximize, TERMINATION_REASON_NUMERICAL_ERROR,
          absl::StrCat(fn_name, "() returned ", ReturnCodeString(rc),
                       " which means that there is a numeric stability issue "
                       "solving Newtonian system"));
    default:
      return util::InternalErrorBuilder()
             << fn_name
             << "() returned unexpected value: " << ReturnCodeString(rc);
  }
}

// Callback for glp_term_hook().
//
// It expects `info` to be a pointer on a TermHookData.
int TermHook(void* const info, const char* const message) {
  static_cast<BufferedMessageCallback*>(info)->OnMessage(message);

  // Returns non-zero to remove any terminal output.
  return 1;
}

// Returns the objective offset. This is used as a placeholder for function
// returning the objective value for solve method not supporting solving empty
// models (glp_exact() and glp_interior()).
double OffsetOnlyObjVal(glp_prob* const problem) {
  return glp_get_obj_coef(problem, 0);
}

// Returns GLP_OPT. This is used as a placeholder for function returning the
// status for solve method not supporting solving empty models (glp_exact() and
// glp_interior()).
int OptStatus(glp_prob*) { return GLP_OPT; }

}  // namespace

absl::StatusOr<std::unique_ptr<SolverInterface>> GlpkSolver::New(
    const ModelProto& model, const InitArgs& /*init_args*/) {
  RETURN_IF_ERROR(ModelIsSupported(model, kGlpkSupportedStructures, "GLPK"));
  return absl::WrapUnique(new GlpkSolver(model));
}

GlpkSolver::GlpkSolver(const ModelProto& model)
    : thread_id_(std::this_thread::get_id()), problem_(glp_create_prob()) {
  // Make sure glp_free_env() is called at the exit of the current thread.
  SetupGlpkEnvAutomaticDeletion();

  glp_set_prob_name(problem_, TruncateAndQuoteGLPKName(model.name()).c_str());

  AddVariables(model.variables());

  AddLinearConstraints(model.linear_constraints());

  glp_set_obj_dir(problem_, model.objective().maximize() ? GLP_MAX : GLP_MIN);
  // Glpk uses index 0 for the "shift" of the objective.
  glp_set_obj_coef(problem_, 0, model.objective().offset());
  for (const auto [v, coeff] :
       MakeView(model.objective().linear_coefficients())) {
    const int col_index = variables_.id_to_index.at(v);
    CHECK_EQ(variables_.ids[col_index - 1], v);
    glp_set_obj_coef(problem_, col_index, coeff);
  }

  const SparseDoubleMatrixProto& proto_matrix =
      model.linear_constraint_matrix();
  glp_load_matrix(
      problem_, proto_matrix.row_ids_size(),
      MatrixIds(proto_matrix.row_ids(), linear_constraints_.id_to_index).data(),
      MatrixIds(proto_matrix.column_ids(), variables_.id_to_index).data(),
      MatrixCoefficients(proto_matrix.coefficients()).data());
}

GlpkSolver::~GlpkSolver() {
  // Here we simply log an error but glp_delete_prob() should crash with an
  // error like: `glp_free: memory allocation error`.
  if (const absl::Status status = CheckCurrentThread(); !status.ok()) {
    LOG(ERROR) << status;
  }
  glp_delete_prob(problem_);
}

namespace {

ProblemStatusProto GetMipProblemStatusProto(const int rc, const int mip_status,
                                            const bool has_finite_dual_bound) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  switch (rc) {
    case GLP_ENOPFS:
      problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      return problem_status;
    case GLP_ENODFS:
      problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);
      return problem_status;
  }

  switch (mip_status) {
    case GLP_OPT:
      problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
      problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
      return problem_status;
    case GLP_FEAS:
      problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
      break;
    case GLP_NOFEAS:
      problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      break;
  }

  if (has_finite_dual_bound) {
    problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
  }
  return problem_status;
}

absl::StatusOr<FeasibilityStatusProto> TranslateProblemStatus(
    const int glpk_status, const absl::string_view fn_name) {
  switch (glpk_status) {
    case GLP_FEAS:
      return FEASIBILITY_STATUS_FEASIBLE;
    case GLP_NOFEAS:
      return FEASIBILITY_STATUS_INFEASIBLE;
    case GLP_INFEAS:
    case GLP_UNDEF:
      return FEASIBILITY_STATUS_UNDETERMINED;
    default:
      return absl::InternalError(
          absl::StrCat(fn_name, " returned the unexpected value ",
                       SolutionStatusString(glpk_status)));
  }
}

// Builds problem status from:
//   * glp_simplex_rc: code returned by glp_simplex.
//   * glpk_primal_status: primal status returned by glp_get_prim_stat.
//   * glpk_dual_status: dual status returned by glp_get_dual_stat.
absl::StatusOr<ProblemStatusProto> GetSimplexProblemStatusProto(
    const int glp_simplex_rc, const int glpk_primal_status,
    const int glpk_dual_status) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  switch (glp_simplex_rc) {
    case GLP_ENOPFS:
      // LP presolver concluded primal infeasibility.
      problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      return problem_status;
    case GLP_ENODFS:
      // LP presolver concluded dual infeasibility.
      problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);
      return problem_status;
    default: {
      // Get primal status from basic solution.
      ASSIGN_OR_RETURN(
          const FeasibilityStatusProto primal_status,
          TranslateProblemStatus(glpk_primal_status, "glp_get_prim_stat"));
      problem_status.set_primal_status(primal_status);

      // Get dual status from basic solution.
      ASSIGN_OR_RETURN(
          const FeasibilityStatusProto dual_status,
          TranslateProblemStatus(glpk_dual_status, "glp_get_dual_stat"));
      problem_status.set_dual_status(dual_status);
      return problem_status;
    }
  }
}

absl::StatusOr<ProblemStatusProto> GetBarrierProblemStatusProto(
    const int glp_interior_rc, const int ipt_status) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  switch (glp_interior_rc) {
    case 0:
      // We only use the glp_ipt_status() result when glp_interior() returned 0.
      switch (ipt_status) {
        case GLP_OPT:
          problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
          problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
          return problem_status;
        case GLP_INFEAS:
          return problem_status;
        case GLP_NOFEAS:
          problem_status.set_primal_or_dual_infeasible(true);
          return problem_status;
        case GLP_UNDEF:
          return problem_status;
        default:
          return absl::InternalError(
              absl::StrCat("glp_ipt_status returned the unexpected value ",
                           SolutionStatusString(ipt_status)));
      }
    default:
      return problem_status;
  }
}

}  // namespace

absl::StatusOr<SolveResultProto> GlpkSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration,
    const Callback /*cb*/, const SolveInterrupter* const interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kGlpkSupportedStructures, "GLPK"));
  RETURN_IF_ERROR(CheckCurrentThread());

  const absl::Time start = absl::Now();

  const auto set_solve_time =
      [&start](SolveResultProto& result) -> absl::Status {
    RETURN_IF_ERROR(util_time::EncodeGoogleApiProto(
        absl::Now() - start,
        result.mutable_solve_stats()->mutable_solve_time()))
        << "failed to set SolveResultProto.solve_stats.solve_time";
    return absl::OkStatus();
  };

  RETURN_IF_ERROR(
      ListInvertedBounds(
          problem_,
          /*variable_ids=*/variables_.ids,
          /*unrounded_variable_lower_bounds=*/variables_.unrounded_lower_bounds,
          /*unrounded_variable_upper_bounds=*/variables_.unrounded_upper_bounds,
          /*linear_constraint_ids=*/linear_constraints_.ids)
          .ToStatus());

  // Deal with empty integer bounds that result in inverted bounds due to bounds
  // rounding.
  {  // Limit scope of `result`.
    std::optional<SolveResultProto> result = EmptyIntegerBoundsResult();
    if (result.has_value()) {
      RETURN_IF_ERROR(set_solve_time(result.value()));
      return std::move(result).value();
    }
  }

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

  BufferedMessageCallback term_hook_data(std::move(message_cb));
  if (term_hook_data.has_user_message_callback()) {
    // Note that glp_term_hook() uses get_env_ptr() that relies on thread local
    // storage to have a different environment per thread. Thus using
    // glp_term_hook() is thread-safe.
    //
    glp_term_hook(TermHook, &term_hook_data);
  }

  // We must reset the term hook when before exiting or before flushing the last
  // unfinished line.
  auto message_cb_cleanup = absl::MakeCleanup([&]() {
    if (term_hook_data.has_user_message_callback()) {
      glp_term_hook(/*func=*/nullptr, /*info=*/nullptr);
      term_hook_data.Flush();
    }
  });

  SolveResultProto result;

  const bool is_mip = IsMip(problem_);

  // We need to use different functions depending on the solve function we used
  // (or placeholders if no solve function was called in case of empty models).
  int (*get_prim_stat)(glp_prob*) = nullptr;
  double (*obj_val)(glp_prob*) = nullptr;
  double (*col_val)(glp_prob*, int) = nullptr;

  int (*get_dual_stat)(glp_prob*) = nullptr;
  double (*row_dual)(glp_prob*, int) = nullptr;
  double (*col_dual)(glp_prob*, int) = nullptr;

  const bool maximize = glp_get_obj_dir(problem_) == GLP_MAX;
  double best_dual_bound = maximize ? kInf : -kInf;

  // Here we use different solve algorithms depending on the type of problem:
  //   * For MIPs: glp_intopt()
  //   * For LPs:
  //     * glp_interior() when using BARRIER LP algorithm
  //     * glp_simplex() for other LP algorithms.
  //
  // These solve algorithms have dedicated data segments in glp_prob which use
  // different access functions to get the solution; hence each branch will set
  // the corresponding function pointers accordingly. They also use a custom
  // struct for parameters that will be initialized and passed to the algorithm.
  if (is_mip) {
    get_prim_stat = glp_mip_status;
    obj_val = glp_mip_obj_val;
    col_val = glp_mip_col_val;

    glp_iocp glpk_parameters;
    glp_init_iocp(&glpk_parameters);
    RETURN_IF_ERROR(SetSharedParameters(
        parameters, term_hook_data.has_user_message_callback(),
        glpk_parameters));
    SetTimeLimitParameter(parameters, glpk_parameters);
    // TODO(b/187027049): glp_intopt with presolve off requires an optional
    // solution of the relaxed problem. Here we simply always enable pre-solve
    // but we should support disabling the presolve and call glp_simplex() in
    // that case.
    glpk_parameters.presolve = GLP_ON;
    if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
      return util::InvalidArgumentErrorBuilder()
             << "parameter presolve not supported by GLPK for MIP";
    }
    if (parameters.has_relative_gap_tolerance()) {
      glpk_parameters.mip_gap = parameters.relative_gap_tolerance();
    }
    if (parameters.has_absolute_gap_tolerance()) {
      return util::InvalidArgumentErrorBuilder()
             << "parameter absolute_gap_tolerance not supported by GLPK "
                "(relative_gap_tolerance is supported)";
    }
    if (parameters.has_iteration_limit()) {
      return util::InvalidArgumentErrorBuilder()
             << "parameter iteration_limit not supported by GLPK for MIP";
    }
    if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
      return util::InvalidArgumentErrorBuilder()
             << "parameter lp_algorithm not supported by GLPK for MIP";
    }
    MipCallbackData mip_cb_data(interrupter);
    glpk_parameters.cb_func = MipCallback;
    glpk_parameters.cb_info = &mip_cb_data;
    const int rc = glp_intopt(problem_, &glpk_parameters);
    const int mip_status = glp_mip_status(problem_);
    const bool has_feasible_solution =
        mip_status == GLP_OPT || mip_status == GLP_FEAS;
    const std::optional<double> feasible_solution_objective_value =
        has_feasible_solution ? std::make_optional(glp_mip_obj_val(problem_))
                              : std::nullopt;
    ASSIGN_OR_RETURN(
        *result.mutable_termination(),
        BuildTermination(problem_, "glp_intopt", rc, MipTerminationOnSuccess,
                         &mip_cb_data, feasible_solution_objective_value,
                         glpk_parameters.mip_gap));
    if (mip_cb_data.best_bound().has_value()) {
      best_dual_bound = *mip_cb_data.best_bound();
    }
    *result.mutable_solve_stats()->mutable_problem_status() =
        GetMipProblemStatusProto(rc, mip_status,
                                 std::isfinite(best_dual_bound));
  } else {
    if (parameters.lp_algorithm() == LP_ALGORITHM_BARRIER) {
      get_prim_stat = glp_ipt_status;
      obj_val = glp_ipt_obj_val;
      col_val = glp_ipt_col_prim;

      get_dual_stat = glp_ipt_status;
      row_dual = glp_ipt_row_dual;
      col_dual = glp_ipt_col_dual;

      glp_iptcp glpk_parameters;
      glp_init_iptcp(&glpk_parameters);
      if (parameters.has_time_limit()) {
        return absl::InvalidArgumentError(
            "parameter time_limit not supported by GLPK for interior point "
            "algorithm");
      }
      RETURN_IF_ERROR(SetSharedParameters(
          parameters, term_hook_data.has_user_message_callback(),
          glpk_parameters));

      // glp_interior() does not support being called with an empty model and
      // returns GLP_EFAIL. Thus we use placeholders in that case.
      //
      // TODO(b/259557110): the emptiness is tested by glp_interior() *after*
      // some pre-processing (including removing fixed variables). The current
      // IsEmpty() is thus not good enough to deal with all cases.
      if (IsEmpty(problem_)) {
        get_prim_stat = OptStatus;
        get_dual_stat = OptStatus;
        obj_val = OffsetOnlyObjVal;
        const double objective_value = OffsetOnlyObjVal(problem_);
        *result.mutable_termination() = OptimalTerminationProto(
            /*finite_primal_objective=*/objective_value,
            /*dual_objective=*/objective_value,
            "glp_interior() not called since the model is empty");
        result.mutable_solve_stats()
            ->mutable_problem_status()
            ->set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
        result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
            FEASIBILITY_STATUS_FEASIBLE);
      } else {
        // TODO(b/187027049): add solver specific parameters for
        // glp_iptcp.ord_alg.
        const int glp_interior_rc = glp_interior(problem_, &glpk_parameters);
        const int ipt_status = glp_ipt_status(problem_);
        const bool has_feasible_solution = ipt_status == GLP_OPT;
        const std::optional<double> feasible_solution_objective_value =
            has_feasible_solution
                ? std::make_optional(glp_ipt_obj_val(problem_))
                : std::nullopt;
        ASSIGN_OR_RETURN(
            *result.mutable_termination(),
            BuildTermination(problem_, "glp_interior", glp_interior_rc,
                             InteriorTerminationOnSuccess,
                             /*mip_cb_data=*/nullptr,
                             feasible_solution_objective_value,
                             /*gap_limit=*/kNaN));
        ASSIGN_OR_RETURN(
            *result.mutable_solve_stats()->mutable_problem_status(),
            GetBarrierProblemStatusProto(/*glp_interior_rc=*/glp_interior_rc,
                                         /*ipt_status=*/ipt_status));
      }
    } else {
      get_prim_stat = glp_get_prim_stat;
      obj_val = glp_get_obj_val;
      col_val = glp_get_col_prim;

      get_dual_stat = glp_get_dual_stat;
      row_dual = glp_get_row_dual;
      col_dual = glp_get_col_dual;

      glp_smcp glpk_parameters;
      glp_init_smcp(&glpk_parameters);
      RETURN_IF_ERROR(SetSharedParameters(
          parameters, term_hook_data.has_user_message_callback(),
          glpk_parameters));
      SetTimeLimitParameter(parameters, glpk_parameters);
      RETURN_IF_ERROR(SetLPParameters(parameters, glpk_parameters));

      // TODO(b/187027049): add option to use glp_exact().
      const int glp_simplex_rc = glp_simplex(problem_, &glpk_parameters);
      const int prim_stat = glp_get_prim_stat(problem_);
      const bool has_feasible_solution = prim_stat == GLP_FEAS;
      const std::optional<double> feasible_solution_objective_value =
          has_feasible_solution ? std::make_optional(glp_get_obj_val(problem_))
                                : std::nullopt;
      ASSIGN_OR_RETURN(*result.mutable_termination(),
                       BuildTermination(problem_, "glp_simplex", glp_simplex_rc,
                                        SimplexTerminationOnSuccess,
                                        /*mip_cb_data=*/nullptr,
                                        feasible_solution_objective_value,
                                        /*gap_limit=*/kNaN));

      // If the primal is proven infeasible and the dual is feasible, the dual
      // is unbounded. Thus we can compute a better dual bound rather than the
      // default value.
      if (glp_get_prim_stat(problem_) == GLP_NOFEAS &&
          glp_get_dual_stat(problem_) == GLP_FEAS) {
        best_dual_bound = maximize ? -kInf : +kInf;
      }

      ASSIGN_OR_RETURN(*result.mutable_solve_stats()->mutable_problem_status(),
                       GetSimplexProblemStatusProto(
                           /*glp_simplex_rc=*/glp_simplex_rc,
                           /*glpk_primal_status=*/prim_stat,
                           /*glpk_dual_status=*/glp_get_dual_stat(problem_)));
      VLOG(1) << "glp_get_status: "
              << SolutionStatusString(glp_get_status(problem_))
              << " glp_get_prim_stat: " << SolutionStatusString(prim_stat)
              << " glp_get_dual_stat: "
              << SolutionStatusString(glp_get_dual_stat(problem_));
    }
  }

  // Unregister the callback and flush the potential last unfinished line.
  std::move(message_cb_cleanup).Invoke();

  switch (result.termination().reason()) {
    case TERMINATION_REASON_OPTIMAL:
    case TERMINATION_REASON_FEASIBLE:
      result.mutable_solve_stats()->set_best_primal_bound(obj_val(problem_));
      break;
    case TERMINATION_REASON_UNBOUNDED:
      // Here we can't use obj_val(problem_) as it would be a finite value of
      // the feasible solution found.
      result.mutable_solve_stats()->set_best_primal_bound(maximize ? +kInf
                                                                   : -kInf);
      break;
    default:
      result.mutable_solve_stats()->set_best_primal_bound(maximize ? -kInf
                                                                   : kInf);
      break;
  }
  // TODO(b/187027049): compute the dual value when the dual is feasible (or
  // problem optimal for interior point) based on the bounds and the dual values
  // for LPs.
  result.mutable_solve_stats()->set_best_dual_bound(best_dual_bound);
  SolutionProto solution;
  AddPrimalSolution(get_prim_stat, obj_val, col_val, model_parameters,
                    solution);
  if (!is_mip) {
    AddDualSolution(get_dual_stat, obj_val, row_dual, col_dual,
                    model_parameters, solution);
  }
  if (solution.has_primal_solution() || solution.has_dual_solution() ||
      solution.has_basis()) {
    *result.add_solutions() = std::move(solution);
  }
  if (parameters.glpk().compute_unbound_rays_if_possible()) {
    RETURN_IF_ERROR(AddPrimalOrDualRay(model_parameters, result));
  }

  RETURN_IF_ERROR(set_solve_time(result));
  return result;
}

void GlpkSolver::AddVariables(const VariablesProto& new_variables) {
  if (new_variables.ids().empty()) {
    return;
  }

  // Indices in GLPK are one-based.
  const int first_new_var_index = variables_.ids.size() + 1;

  variables_.ids.insert(variables_.ids.end(), new_variables.ids().begin(),
                        new_variables.ids().end());
  for (int v = 0; v < new_variables.ids_size(); ++v) {
    CHECK(variables_.id_to_index
              .try_emplace(new_variables.ids(v), first_new_var_index + v)
              .second);
  }
  glp_add_cols(problem_, new_variables.ids_size());
  if (!new_variables.names().empty()) {
    for (int v = 0; v < new_variables.names_size(); ++v) {
      glp_set_col_name(
          problem_, v + first_new_var_index,
          TruncateAndQuoteGLPKName(new_variables.names(v)).c_str());
    }
  }
  CHECK_EQ(new_variables.lower_bounds_size(),
           new_variables.upper_bounds_size());
  CHECK_EQ(new_variables.lower_bounds_size(), new_variables.ids_size());
  variables_.unrounded_lower_bounds.insert(
      variables_.unrounded_lower_bounds.end(),
      new_variables.lower_bounds().begin(), new_variables.lower_bounds().end());
  variables_.unrounded_upper_bounds.insert(
      variables_.unrounded_upper_bounds.end(),
      new_variables.upper_bounds().begin(), new_variables.upper_bounds().end());
  for (int i = 0; i < new_variables.lower_bounds_size(); ++i) {
    // Here we don't use the boolean "kind" GLP_BV since it does not exist. It
    // is an artifact of glp_(get|set)_col_kind() functions. When
    // glp_set_col_kind() is called with GLP_BV, in addition to setting the kind
    // to GLP_IV (integer) it also sets the bounds to [0,1]. Symmetrically
    // glp_get_col_kind() returns GLP_BV when the kind is GLP_IV and the bounds
    // are [0,1].
    glp_set_col_kind(problem_, i + first_new_var_index,
                     new_variables.integers(i) ? GLP_IV : GLP_CV);
    SetBounds<Variables>(problem_, /*k=*/i + first_new_var_index,
                         {.lower = new_variables.lower_bounds(i),
                          .upper = new_variables.upper_bounds(i)});
  }
}

void GlpkSolver::AddLinearConstraints(
    const LinearConstraintsProto& new_linear_constraints) {
  if (new_linear_constraints.ids().empty()) {
    return;
  }

  // Indices in GLPK are one-based.
  const int first_new_cstr_index = linear_constraints_.ids.size() + 1;

  linear_constraints_.ids.insert(linear_constraints_.ids.end(),
                                 new_linear_constraints.ids().begin(),
                                 new_linear_constraints.ids().end());
  for (int c = 0; c < new_linear_constraints.ids_size(); ++c) {
    CHECK(linear_constraints_.id_to_index
              .try_emplace(new_linear_constraints.ids(c),
                           first_new_cstr_index + c)
              .second);
  }
  glp_add_rows(problem_, new_linear_constraints.ids_size());
  if (!new_linear_constraints.names().empty()) {
    for (int c = 0; c < new_linear_constraints.names_size(); ++c) {
      glp_set_row_name(
          problem_, c + first_new_cstr_index,
          TruncateAndQuoteGLPKName(new_linear_constraints.names(c)).c_str());
    }
  }
  CHECK_EQ(new_linear_constraints.lower_bounds_size(),
           new_linear_constraints.upper_bounds_size());
  for (int i = 0; i < new_linear_constraints.lower_bounds_size(); ++i) {
    SetBounds<LinearConstraints>(
        problem_, /*k=*/i + first_new_cstr_index,
        {.lower = new_linear_constraints.lower_bounds(i),
         .upper = new_linear_constraints.upper_bounds(i)});
  }
}

void GlpkSolver::UpdateObjectiveCoefficients(
    const SparseDoubleVectorProto& coefficients_proto) {
  for (const auto [id, coeff] : MakeView(coefficients_proto)) {
    const int col_index = variables_.id_to_index.at(id);
    CHECK_EQ(variables_.ids[col_index - 1], id);
    glp_set_obj_coef(problem_, col_index, coeff);
  }
}

void GlpkSolver::UpdateLinearConstraintMatrix(
    const SparseDoubleMatrixProto& matrix_updates,
    const std::optional<int64_t> first_new_var_id,
    const std::optional<int64_t> first_new_cstr_id) {
  // GLPK's does not have an API to set matrix elements one by one. Instead it
  // can either update an entire row or update an entire column or load the
  // entire matrix. On top of that there is no API to get the entire matrix at
  // once.
  //
  // Hence to update existing coefficients we have to read rows (or columns)
  // coefficients, update existing non-zero that have been changed and add new
  // values and write back the result. For new rows and columns we can be more
  // efficient since we don't have to read the existing values back.
  //
  // The strategy used below is to split the matrix in three regions:
  //
  //                existing    new
  //                columns   columns
  //              /         |         \
  //    existing  |    1    |    2    |
  //    rows      |         |         |
  //              |---------+---------|
  //    new       |                   |
  //    rows      |         3         |
  //              \                   /
  //
  // We start by updating the region 1 of existing rows and columns to limit the
  // number of reads of existing coefficients. Then we update region 2 with all
  // new columns but we only existing rows. Finally we update region 3 with all
  // new rows and include new columns. Doing updates this way remove the need to
  // read existing coefficients for the updates 2 & 3 since by construction
  // those values are 0.

  // Updating existing rows (constraints), ignoring the new columns.
  {
    // We reuse the same vectors for all calls to GLPK's API to limit
    // reallocations of these temporary buffers.
    GlpkSparseVector data(static_cast<int>(variables_.ids.size()));
    for (const auto& [row_id, row_coefficients] :
         SparseSubmatrixByRows(matrix_updates,
                               /*start_row_id=*/0,
                               /*end_row_id=*/first_new_cstr_id,
                               /*start_col_id=*/0,
                               /*end_col_id=*/first_new_var_id)) {
      // Find the index of the row in GLPK corresponding to the MathOpt's row
      // id.
      const int row_index = linear_constraints_.id_to_index.at(row_id);
      CHECK_EQ(linear_constraints_.ids[row_index - 1], row_id);

      // Read the current row coefficients.
      data.Load([&](int* const indices, double* const values) {
        return glp_get_mat_row(problem_, row_index, indices, values);
      });

      // Update the row data.
      for (const auto [col_id, coefficient] : row_coefficients) {
        const int col_index = variables_.id_to_index.at(col_id);
        CHECK_EQ(variables_.ids[col_index - 1], col_id);
        data.Set(col_index, coefficient);
      }

      // Change the row values.
      glp_set_mat_row(problem_, row_index, data.size(), data.indices(),
                      data.values());
    }
  }

  // Add new columns's coefficients of existing rows. The coefficients of new
  // columns in new rows will be added when adding new rows below.
  if (first_new_var_id.has_value()) {
    GlpkSparseVector data(static_cast<int>(linear_constraints_.ids.size()));
    for (const auto& [col_id, col_coefficients] : TransposeSparseSubmatrix(
             SparseSubmatrixByRows(matrix_updates,
                                   /*start_row_id=*/0,
                                   /*end_row_id=*/first_new_cstr_id,
                                   /*start_col_id=*/*first_new_var_id,
                                   /*end_col_id=*/std::nullopt))) {
      // Find the index of the column in GLPK corresponding to the MathOpt's
      // column id.
      const int col_index = variables_.id_to_index.at(col_id);
      CHECK_EQ(variables_.ids[col_index - 1], col_id);

      // Prepare the column data replacing MathOpt ids by GLPK one-based row
      // indices.
      data.Clear();
      for (const auto [row_id, coefficient] : MakeView(col_coefficients)) {
        const int row_index = linear_constraints_.id_to_index.at(row_id);
        CHECK_EQ(linear_constraints_.ids[row_index - 1], row_id);
        data.Set(row_index, coefficient);
      }

      // Change the column values.
      glp_set_mat_col(problem_, col_index, data.size(), data.indices(),
                      data.values());
    }
  }

  // Add new rows, including the new columns' coefficients.
  if (first_new_cstr_id.has_value()) {
    GlpkSparseVector data(static_cast<int>(variables_.ids.size()));
    for (const auto& [row_id, row_coefficients] :
         SparseSubmatrixByRows(matrix_updates,
                               /*start_row_id=*/*first_new_cstr_id,
                               /*end_row_id=*/std::nullopt,
                               /*start_col_id=*/0,
                               /*end_col_id=*/std::nullopt)) {
      // Find the index of the row in GLPK corresponding to the MathOpt's row
      // id.
      const int row_index = linear_constraints_.id_to_index.at(row_id);
      CHECK_EQ(linear_constraints_.ids[row_index - 1], row_id);

      // Prepare the row data replacing MathOpt ids by GLPK one-based column
      // indices.
      data.Clear();
      for (const auto [col_id, coefficient] : row_coefficients) {
        const int col_index = variables_.id_to_index.at(col_id);
        CHECK_EQ(variables_.ids[col_index - 1], col_id);
        data.Set(col_index, coefficient);
      }

      // Change the row values.
      glp_set_mat_row(problem_, row_index, data.size(), data.indices(),
                      data.values());
    }
  }
}

void GlpkSolver::AddPrimalSolution(
    int (*get_prim_stat)(glp_prob*), double (*obj_val)(glp_prob*),
    double (*col_val)(glp_prob*, int),
    const ModelSolveParametersProto& model_parameters,
    SolutionProto& solution_proto) {
  const int status = get_prim_stat(problem_);
  if (status == GLP_OPT || status == GLP_FEAS) {
    PrimalSolutionProto& primal_solution =
        *solution_proto.mutable_primal_solution();
    primal_solution.set_objective_value(obj_val(problem_));
    primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    *primal_solution.mutable_variable_values() =
        FilteredVector(problem_, model_parameters.variable_values_filter(),
                       variables_.ids, col_val);
  }
}

void GlpkSolver::AddDualSolution(
    int (*get_dual_stat)(glp_prob*), double (*obj_val)(glp_prob*),
    double (*row_dual)(glp_prob*, int), double (*col_dual)(glp_prob*, int),
    const ModelSolveParametersProto& model_parameters,
    SolutionProto& solution_proto) {
  const int status = get_dual_stat(problem_);
  if (status == GLP_OPT || status == GLP_FEAS) {
    DualSolutionProto& dual_solution = *solution_proto.mutable_dual_solution();
    dual_solution.set_objective_value(obj_val(problem_));
    *dual_solution.mutable_dual_values() =
        FilteredVector(problem_, model_parameters.dual_values_filter(),
                       linear_constraints_.ids, row_dual);
    *dual_solution.mutable_reduced_costs() =
        FilteredVector(problem_, model_parameters.reduced_costs_filter(),
                       variables_.ids, col_dual);
    // TODO(b/197867442): Check that `status == GLP_FEAS` implies dual feasible
    // solution on early termination with barrier (where both `get_dual_stat`
    // and `get_prim_stat` are equal to `glp_ipt_status`).
    dual_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  }
}

absl::Status GlpkSolver::AddPrimalOrDualRay(
    const ModelSolveParametersProto& model_parameters,
    SolveResultProto& result) {
  ASSIGN_OR_RETURN(const std::optional<GlpkRay> opt_unbound_ray,
                   GlpkComputeUnboundRay(problem_));
  if (!opt_unbound_ray.has_value()) {
    return absl::OkStatus();
  }

  const int num_cstrs = linear_constraints_.ids.size();
  switch (opt_unbound_ray->type) {
    case GlpkRayType::kPrimal: {
      const int num_cstrs = static_cast<int>(linear_constraints_.ids.size());
      // Note that GlpkComputeUnboundRay() returned ray considers the variables
      // of the computational form. Thus it contains both structural and
      // auxiliary variables. In the MathOpt's primal ray we only consider
      // structural variables though.
      std::vector<double> ray_values(variables_.ids.size());

      for (const auto [k, value] : opt_unbound_ray->non_zero_components) {
        if (k <= num_cstrs) {
          // Ignore auxiliary variables.
          continue;
        }
        const int var_index = k - num_cstrs;
        CHECK_GE(var_index, 1);
        ray_values[var_index - 1] = value;
      }

      *result.add_primal_rays()->mutable_variable_values() =
          FilteredRay(model_parameters.variable_values_filter(), variables_.ids,
                      ray_values);

      return absl::OkStatus();
    }
    case GlpkRayType::kDual: {
      // Note that GlpkComputeUnboundRay() returned ray considers the variables
      // of the computational form. Thus it contains reduced costs of both
      // structural and auxiliary variables. In the MathOpt's dual ray we split
      // the reduced costs. The ones of auxiliary variables (variables of
      // constraints) are called "dual values" and the ones of structural
      // variables are called "reduced costs".
      std::vector<double> ray_reduced_costs(variables_.ids.size());
      std::vector<double> ray_dual_values(num_cstrs);

      for (const auto [k, value] : opt_unbound_ray->non_zero_components) {
        if (k <= num_cstrs) {
          ray_dual_values[k - 1] = value;
        } else {
          const int var_index = k - num_cstrs;
          CHECK_GE(var_index, 1);
          ray_reduced_costs[var_index - 1] = value;
        }
      }

      DualRayProto& dual_ray = *result.add_dual_rays();
      *dual_ray.mutable_dual_values() =
          FilteredRay(model_parameters.dual_values_filter(),
                      linear_constraints_.ids, ray_dual_values);
      *dual_ray.mutable_reduced_costs() =
          FilteredRay(model_parameters.reduced_costs_filter(), variables_.ids,
                      ray_reduced_costs);

      return absl::OkStatus();
    }
  }
}

absl::StatusOr<bool> GlpkSolver::Update(const ModelUpdateProto& model_update) {
  RETURN_IF_ERROR(CheckCurrentThread());

  // We must do that *after* testing current thread since the Solver class won't
  // destroy this instance from another thread when the update is not supported
  // (the Solver class destroy the SolverInterface only when an Update() returns
  // false).
  if (!UpdateIsSupported(model_update, kGlpkSupportedStructures)) {
    return false;
  }

  {
    const std::vector<int> sorted_deleted_cols = DeleteRowsOrCols(
        problem_, variables_, model_update.deleted_variable_ids());
    DeleteRowOrColData(variables_.unrounded_lower_bounds, sorted_deleted_cols);
    DeleteRowOrColData(variables_.unrounded_upper_bounds, sorted_deleted_cols);
    CHECK_EQ(variables_.unrounded_lower_bounds.size(),
             variables_.unrounded_upper_bounds.size());
    CHECK_EQ(variables_.unrounded_lower_bounds.size(), variables_.ids.size());
  }
  DeleteRowsOrCols(problem_, linear_constraints_,
                   model_update.deleted_linear_constraint_ids());

  for (const auto [var_id, is_integer] :
       MakeView(model_update.variable_updates().integers())) {
    // See comment in AddVariables() to see why we don't use GLP_BV here.
    const int var_index = variables_.id_to_index.at(var_id);
    glp_set_col_kind(problem_, var_index, is_integer ? GLP_IV : GLP_CV);

    // Either restore the fractional bounds if the variable was integer and is
    // now integer, or rounds the existing bounds if the variable was fractional
    // and is now integer. Here we use the old bounds; they will get updated
    // below by the call to UpdateBounds() if they are also changed by this
    // update.
    SetBounds<Variables>(
        problem_, var_index,
        {.lower = variables_.unrounded_lower_bounds[var_index - 1],
         .upper = variables_.unrounded_upper_bounds[var_index - 1]});
  }
  for (const auto [var_id, lower_bound] :
       MakeView(model_update.variable_updates().lower_bounds())) {
    variables_.unrounded_lower_bounds[variables_.id_to_index.at(var_id) - 1] =
        lower_bound;
  }
  for (const auto [var_id, upper_bound] :
       MakeView(model_update.variable_updates().upper_bounds())) {
    variables_.unrounded_upper_bounds[variables_.id_to_index.at(var_id) - 1] =
        upper_bound;
  }
  UpdateBounds(
      problem_, variables_,
      /*lower_bounds_proto=*/model_update.variable_updates().lower_bounds(),
      /*upper_bounds_proto=*/model_update.variable_updates().upper_bounds());
  UpdateBounds(problem_, linear_constraints_,
               /*lower_bounds_proto=*/
               model_update.linear_constraint_updates().lower_bounds(),
               /*upper_bounds_proto=*/
               model_update.linear_constraint_updates().upper_bounds());

  AddVariables(model_update.new_variables());
  AddLinearConstraints(model_update.new_linear_constraints());

  if (model_update.objective_updates().has_direction_update()) {
    glp_set_obj_dir(problem_,
                    model_update.objective_updates().direction_update()
                        ? GLP_MAX
                        : GLP_MIN);
  }
  if (model_update.objective_updates().has_offset_update()) {
    // Glpk uses index 0 for the "shift" of the objective.
    glp_set_obj_coef(problem_, 0,
                     model_update.objective_updates().offset_update());
  }
  UpdateObjectiveCoefficients(
      model_update.objective_updates().linear_coefficients());

  UpdateLinearConstraintMatrix(
      model_update.linear_constraint_matrix_updates(),
      /*first_new_var_id=*/FirstVariableId(model_update.new_variables()),
      /*first_new_cstr_id=*/
      FirstLinearConstraintId(model_update.new_linear_constraints()));

  return true;
}

absl::Status GlpkSolver::CheckCurrentThread() {
  if (std::this_thread::get_id() != thread_id_) {
    return absl::InvalidArgumentError(
        "GLPK is not thread-safe and thus the solver should only be used on "
        "the same thread as it was created");
  }
  return absl::OkStatus();
}

std::optional<SolveResultProto> GlpkSolver::EmptyIntegerBoundsResult() {
  const int num_cols = glp_get_num_cols(problem_);
  for (int c = 1; c <= num_cols; ++c) {
    if (!variables_.IsInteger(problem_, c)) {
      continue;
    }
    const double lb = variables_.unrounded_lower_bounds[c - 1];
    const double ub = variables_.unrounded_upper_bounds[c - 1];
    if (lb > ub) {
      // Unrounded bounds are inverted; this case is covered by
      // ListInvertedBounds(). We don't want to depend on the order of calls of
      // the two functions here so we exclude this case.
      continue;
    }
    if (std::ceil(lb) <= std::floor(ub)) {
      continue;
    }

    // We found a variable with empty integer bounds (that is lb <= ub but
    // ceil(lb) > floor(ub)).
    return ResultForIntegerInfeasible(
        /*is_maximize=*/glp_get_obj_dir(problem_) == GLP_MAX,
        /*bad_variable_id=*/variables_.ids[c - 1],
        /*lb=*/lb, /*ub=*/ub);
  }

  return std::nullopt;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
GlpkSolver::ComputeInfeasibleSubsystem(
    const SolveParametersProto& parameters, MessageCallback message_cb,
    const SolveInterrupter* const interrupter) {
  return absl::UnimplementedError(
      "GLPK does not provide a method to compute an infeasible subsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GLPK, GlpkSolver::New)

}  // namespace math_opt
}  // namespace operations_research
