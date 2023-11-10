// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_
#define OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_

#include <cstdint>
#include <optional>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {

// Returns solve_result.termination.objective_bounds if present. Otherwise,
// it builds ObjectiveBoundsProto from
// solve_result.solve_stats.best_primal/dual_bound
// TODO(b/290091715): Remove once solve_stats.best_primal/dual_bound is removed
// and we know termination.objective_bounds will always be present.
ObjectiveBoundsProto GetObjectiveBounds(const SolveResultProto& solve_result);

// Returns solve_result.termination.problem_status if present. Otherwise,
// it returns solve_result.solve_stats.problem_status
// TODO(b/290091715): Remove once solve_stats.problem_status is removed and we
// know termination.problem_status will always be present.
ProblemStatusProto GetProblemStatus(const SolveResultProto& solve_result);

inline int NumVariables(const VariablesProto& variables) {
  return variables.ids_size();
}

inline int NumConstraints(const LinearConstraintsProto& linear_constraints) {
  return linear_constraints.ids_size();
}

inline int NumMatrixNonzeros(const SparseDoubleMatrixProto& matrix) {
  return matrix.row_ids_size();
}

// Returns the id of the first variable if there is one. If the input proto is
// valid, this will also be the smallest id.
inline std::optional<int64_t> FirstVariableId(const VariablesProto& variables) {
  return variables.ids().empty() ? std::nullopt
                                 : std::make_optional(variables.ids()[0]);
}

// Returns the id of the first linear constraint if there is one. If the input
// proto is valid, this will also be the smallest id.
inline std::optional<int64_t> FirstLinearConstraintId(
    const LinearConstraintsProto& linear_constraints) {
  return linear_constraints.ids().empty()
             ? std::nullopt
             : std::make_optional(linear_constraints.ids()[0]);
}

// Removes the items in the sparse double vector for all indices whose value is
// exactly 0.0.
//
// NaN values are kept in place.
//
// The function asserts that input is a valid sparse vector, i.e. that the
// number of values and ids match.
void RemoveSparseDoubleVectorZeros(SparseDoubleVectorProto& sparse_vector);

// A utility class that tests if a pair (id, value) should be filtered based on
// an input SparseVectorFilterProto.
//
// This predicate expects the input is sorted by ids. In non-optimized builds,
// it will check that this is the case.
class SparseVectorFilterPredicate {
 public:
  // Builds a predicate based on the input filter. A reference to this filter is
  // kept so the caller must make sure this filter outlives the predicate.
  //
  // The filter.filtered_ids is expected to be sorted and not contain
  // duplicates. In non-optimized builds, it will be CHECKed.
  explicit SparseVectorFilterPredicate(const SparseVectorFilterProto& filter);

  // Returns true if the input value should be kept, false if it should be
  // ignored since it is not selected by the filter.
  //
  // This function is expected to be called with strictly increasing ids. In
  // non-optimized builds it will CHECK that this is the case. It updates an
  // internal counter when filtering by ids.
  template <typename Value>
  bool AcceptsAndUpdate(int64_t id, const Value& value);

 private:
  const SparseVectorFilterProto& filter_;

  // Index of the next element to consider in filter_.filtered_ids().
  int next_filtered_id_index_ = 0;

#ifndef NDEBUG
  // Invariant: next input id must be >= next_input_id_lower_bound_.
  //
  // The initial value is 0 since all ids are expected to be non-negative.
  int64_t next_input_id_lower_bound_ = 0;
#endif  // NDEBUG
};

// Applies filter to each element of input and returns the elements that remain.
//
// TODO(b/261603235): this function is not very efficient, decide if this
// matters.
SparseDoubleVectorProto FilterSparseVector(
    const SparseDoubleVectorProto& input,
    const SparseVectorFilterProto& filter);

// Applies the primal, dual and reduced costs filters from model_solve_params
// to the primal solution variable values, dual solution dual values, and dual
// solution reduced costs, respectively, and overwriting these values with
// the results.
//
// Warning: solution is modified in place.
//
// TODO(b/261603235): this function is not very efficient, decide if this
// matters.
void ApplyAllFilters(const ModelSolveParametersProto& model_solve_params,
                     SolutionProto& solution);

// Returns the callback_registration.request_registration as a set of enums.
absl::flat_hash_set<CallbackEventProto> EventSet(
    const CallbackRegistrationProto& callback_registration);

// Sets the reason to TERMINATION_REASON_FEASIBLE if feasible = true and
// TERMINATION_REASON_NO_SOLUTION_FOUND otherwise.
ABSL_DEPRECATED("Use LimitTerminationProto() instead")
TerminationProto TerminateForLimit(LimitProto limit, bool feasible,
                                   absl::string_view detail = {});

ABSL_DEPRECATED("Use FeasibleTerminationProto() instead")
TerminationProto FeasibleTermination(LimitProto limit,
                                     absl::string_view detail = {});

ABSL_DEPRECATED("Use NoSolutionFound() instead")
TerminationProto NoSolutionFoundTermination(LimitProto limit,
                                            absl::string_view detail = {});

ABSL_DEPRECATED(
    "Use TerminateForReason(bool, TerminationReasonProto, absl::string_view) "
    "instead")
TerminationProto TerminateForReason(TerminationReasonProto reason,
                                    absl::string_view detail = {});

// Returns trivial bounds.
//
// Trivial bounds are:
// * for a maximization:
//   - primal_bound: -inf
//   - dual_bound  : +inf
// * for a minimization:
//   - primal_bound: +inf
//   - dual_bound  : -inf
ObjectiveBoundsProto MakeTrivialBounds(bool is_maximize);

// Returns a TerminationProto with the provided reason and details along with
// trivial bounds and FEASIBILITY_STATUS_UNDETERMINED statuses.
TerminationProto TerminateForReason(bool is_maximize,
                                    TerminationReasonProto reason,
                                    absl::string_view detail = {});

// Returns a TERMINATION_REASON_OPTIMAL termination with the provided objective
// bounds and FEASIBILITY_STATUS_FEASIBLE primal and dual statuses.
//
// finite_primal_objective must be finite for a valid TerminationProto to be
// returned.
//
// TODO(b/290359402): additionally require dual_objective to be finite.
TerminationProto OptimalTerminationProto(double finite_primal_objective,
                                         double dual_objective,
                                         absl::string_view detail = {});

// Returns a TERMINATION_REASON_INFEASIBLE termination with
// FEASIBILITY_STATUS_INFEASIBLE primal status and the provided dual status.
//
// It sets a trivial primal bound and a trivial dual bound based on the provided
// dual status.
//
// The convention for infeasible MIPs is that dual_feasibility_status is
// feasible (There always exist a dual feasible convex relaxation of an
// infeasible MIP).
//
// dual_feasibility_status must not be FEASIBILITY_STATUS_UNSPECIFIED for a
// valid TerminationProto to be returned.
TerminationProto InfeasibleTerminationProto(
    bool is_maximize, FeasibilityStatusProto dual_feasibility_status,
    absl::string_view detail = {});

// Returns a TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED termination with
// FEASIBILITY_STATUS_UNDETERMINED primal status and the provided dual status
// along with trivial bounds.
//
// primal_or_dual_infeasible is set if dual_feasibility_status is
// FEASIBILITY_STATUS_UNDETERMINED.
//
// dual_feasibility_status must be infeasible or undetermined for a valid
// TerminationProto to be returned.
TerminationProto InfeasibleOrUnboundedTerminationProto(
    bool is_maximize, FeasibilityStatusProto dual_feasibility_status,
    absl::string_view detail = {});

// Returns a TERMINATION_REASON_UNBOUNDED termination with a
// FEASIBILITY_STATUS_FEASIBLE primal status and FEASIBILITY_STATUS_INFEASIBLE
// dual one along with corresponding trivial bounds.
TerminationProto UnboundedTerminationProto(bool is_maximize,
                                           absl::string_view detail = {});

// Returns a TERMINATION_REASON_NO_SOLUTION_FOUND termination with
// FEASIBILITY_STATUS_UNDETERMINED primal status.
//
// Assumes dual solution exists iff optional_dual_objective is set even if
// infinite (some solvers return feasible dual solutions without an objective
// value). optional_dual_objective should not be set when limit is LIMIT_CUTOFF
// for a valid TerminationProto to be returned (use
// LimitCutoffTerminationProto() below instead).
//
// It sets a trivial primal bound. The dual bound is either set to the
// optional_dual_objective if set, else to a trivial value.
//
// TODO(b/290359402): Consider improving to require a finite dual bound when
// dual feasible solutions are returned.
TerminationProto NoSolutionFoundTerminationProto(
    bool is_maximize, LimitProto limit,
    std::optional<double> optional_dual_objective = std::nullopt,
    absl::string_view detail = {});

// Returns a TERMINATION_REASON_FEASIBLE termination with a
// FEASIBILITY_STATUS_FEASIBLE primal status. The dual status depends on
// optional_dual_objective.
//
// finite_primal_objective should be finite and limit should not be LIMIT_CUTOFF
// for a valid TerminationProto to be returned (use
// LimitCutoffTerminationProto() below instead).
//
// Assumes dual solution exists iff optional_dual_objective is set even if
// infinite (some solvers return feasible dual solutions without an objective
// value). If set the dual status is set to FEASIBILITY_STATUS_FEASIBLE, else it
// is FEASIBILITY_STATUS_UNDETERMINED.
//
// It sets the primal bound based on the primal objective. The dual bound is
// either set to the optional_dual_objective if set, else to a trivial value.
//
// TODO(b/290359402): Consider improving to require a finite dual bound when
// dual feasible solutions are returned.
TerminationProto FeasibleTerminationProto(
    bool is_maximize, LimitProto limit, double finite_primal_objective,
    std::optional<double> optional_dual_objective = std::nullopt,
    absl::string_view detail = {});

// Either calls FeasibleTerminationProto() or NoSolutionFoundTerminationProto()
// based on optional_finite_primal_objective having a value.
//
// That is it assumes a primal feasible solution exists iff
// optional_finite_primal_objective has a value.
TerminationProto LimitTerminationProto(
    bool is_maximize, LimitProto limit,
    std::optional<double> optional_finite_primal_objective,
    std::optional<double> optional_dual_objective = std::nullopt,
    absl::string_view detail = {});

// Returns either a TERMINATION_REASON_FEASIBLE or
// TERMINATION_REASON_NO_SOLUTION_FOUND termination depending on
// primal_objective being finite or not. That is it assumes there is a primal
// feasible solution iff primal_objective is finite.
//
// If claim_dual_feasible_solution_exists is true, the dual_status is set as
// FEASIBILITY_STATUS_FEASIBLE, else FEASIBILITY_STATUS_UNDETERMINED.
//
// This function assumes dual solution exists if
// claim_dual_feasible_solution_exists is true even if dual_objective is
// infinite (some solvers return feasible dual solutions without an objective
// value). If dual_objective is finite then claim_dual_feasible_solution_exists
// must be true for a valid termination to be returned.
//
// TODO(b/290359402): Consider improving to require a finite dual bound when
// dual feasible solutions are returned.
TerminationProto LimitTerminationProto(LimitProto limit,
                                       double primal_objective,
                                       double dual_objective,
                                       bool claim_dual_feasible_solution_exists,
                                       absl::string_view detail = {});

// Calls NoSolutionFoundTerminationProto() with LIMIT_CUTOFF LIMIT.
TerminationProto CutoffTerminationProto(bool is_maximize,
                                        absl::string_view detail = {});

enum class SupportType {
  kNotSupported = 1,
  kSupported = 2,
  kNotImplemented = 3,
};

struct SupportedProblemStructures {
  SupportType integer_variables = SupportType::kNotSupported;
  SupportType multi_objectives = SupportType::kNotSupported;
  SupportType quadratic_objectives = SupportType::kNotSupported;
  SupportType quadratic_constraints = SupportType::kNotSupported;
  SupportType second_order_cone_constraints = SupportType::kNotSupported;
  SupportType sos1_constraints = SupportType::kNotSupported;
  SupportType sos2_constraints = SupportType::kNotSupported;
  SupportType indicator_constraints = SupportType::kNotSupported;
};

// Returns an InvalidArgumentError (respectively, UnimplementedError) if a
// problem structure is present in `model` and not supported (resp., not yet
// implemented) according to `support_menu`.
absl::Status ModelIsSupported(const ModelProto& model,
                              const SupportedProblemStructures& support_menu,
                              absl::string_view solver_name);

// Returns false if a problem structure is present in `update` and not
// not implemented or supported according to `support_menu`.
bool UpdateIsSupported(const ModelUpdateProto& update,
                       const SupportedProblemStructures& support_menu);

void UpgradeSolveResultProtoForStatsMigration(
    SolveResultProto& solve_result_proto);

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename Value>
bool SparseVectorFilterPredicate::AcceptsAndUpdate(const int64_t id,
                                                   const Value& value) {
#ifndef NDEBUG
  CHECK_GE(id, next_input_id_lower_bound_)
      << "This function must be called with strictly increasing ids.";

  // Update the range of the next expected id. We expect input to be strictly
  // increasing.
  next_input_id_lower_bound_ = id + 1;
#endif  // NDEBUG

  // For this predicate we use `0` as the zero to test with since as of today we
  // only have SparseDoubleVectorProto and SparseBoolVectorProto. The `bool`
  // type is an integral type so the comparison with 0 will indeed be equivalent
  // to keeping only `true` values.
  if (filter_.skip_zero_values() && value == 0) {
    return false;
  }

  if (!filter_.filter_by_ids()) {
    return true;
  }

  // Skip all filtered_ids that are smaller than the input id.
  while (next_filtered_id_index_ < filter_.filtered_ids_size() &&
         filter_.filtered_ids(next_filtered_id_index_) < id) {
    ++next_filtered_id_index_;
  }

  if (next_filtered_id_index_ == filter_.filtered_ids_size()) {
    // We filter by ids and there are no more ids that should pass.
    return false;
  }

  // The previous loop ensured that the element at next_filtered_id_index_ is
  // the first element greater or equal to id.
  return id == filter_.filtered_ids(next_filtered_id_index_);
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_
