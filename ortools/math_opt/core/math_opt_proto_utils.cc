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

#include "ortools/math_opt/core/math_opt_proto_utils.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

void RemoveSparseDoubleVectorZeros(SparseDoubleVectorProto& sparse_vector) {
  CHECK_EQ(sparse_vector.ids_size(), sparse_vector.values_size());
  // Keep track of the next index that has not yet been used for a non zero
  // value.
  int next = 0;
  for (const auto [id, value] : MakeView(sparse_vector)) {
    // Se use `!(== 0.0)` here so that we keep NaN values for which both `v ==
    // 0` and `v != 0` returns false.
    if (!(value == 0.0)) {
      sparse_vector.set_ids(next, id);
      sparse_vector.set_values(next, value);
      ++next;
    }
  }
  // At the end of the iteration, `next` contains the index of the first unused
  // index. This means it contains the number of used elements.
  sparse_vector.mutable_ids()->Truncate(next);
  sparse_vector.mutable_values()->Truncate(next);
}

SparseVectorFilterPredicate::SparseVectorFilterPredicate(
    const SparseVectorFilterProto& filter)
    : filter_(filter) {
  // We only do this test in non-optimized builds.
  if (DEBUG_MODE && filter_.filter_by_ids()) {
    // Checks that input filtered_ids are strictly increasing.
    // That is: for all i, ids(i) < ids(i+1).
    // Hence here we fail if there exists i such that ids(i) >= ids(i+1).
    const auto& ids = filter_.filtered_ids();
    CHECK(std::adjacent_find(ids.begin(), ids.end(),
                             std::greater_equal<int64_t>()) == ids.end())
        << "The input filter.filtered_ids must be strictly increasing.";
  }
}

SparseDoubleVectorProto FilterSparseVector(
    const SparseDoubleVectorProto& input,
    const SparseVectorFilterProto& filter) {
  SparseDoubleVectorProto result;
  SparseVectorFilterPredicate predicate(filter);
  for (const auto [id, val] : MakeView(input)) {
    if (predicate.AcceptsAndUpdate(id, val)) {
      result.add_ids(id);
      result.add_values(val);
    }
  }
  return result;
}

void ApplyAllFilters(const ModelSolveParametersProto& model_solve_params,
                     SolutionProto& solution) {
  if (model_solve_params.has_variable_values_filter() &&
      solution.has_primal_solution()) {
    *solution.mutable_primal_solution()->mutable_variable_values() =
        FilterSparseVector(solution.primal_solution().variable_values(),
                           model_solve_params.variable_values_filter());
  }
  if (model_solve_params.has_dual_values_filter() &&
      solution.has_dual_solution()) {
    *solution.mutable_dual_solution()->mutable_dual_values() =
        FilterSparseVector(solution.dual_solution().dual_values(),
                           model_solve_params.dual_values_filter());
  }
  if (model_solve_params.has_reduced_costs_filter() &&
      solution.has_dual_solution()) {
    *solution.mutable_dual_solution()->mutable_reduced_costs() =
        FilterSparseVector(solution.dual_solution().reduced_costs(),
                           model_solve_params.reduced_costs_filter());
  }
}

absl::flat_hash_set<CallbackEventProto> EventSet(
    const CallbackRegistrationProto& callback_registration) {
  // Here we don't use for-range loop since for repeated enum fields, the type
  // used in C++ is RepeatedField<int>. Using the generated getter instead
  // guarantees type safety.
  absl::flat_hash_set<CallbackEventProto> events;
  for (int i = 0; i < callback_registration.request_registration_size(); ++i) {
    events.emplace(callback_registration.request_registration(i));
  }
  return events;
}

TerminationProto TerminateForLimit(const LimitProto limit, const bool feasible,
                                   const absl::string_view detail) {
  TerminationProto result;
  if (feasible) {
    result.set_reason(TERMINATION_REASON_FEASIBLE);
  } else {
    result.set_reason(TERMINATION_REASON_NO_SOLUTION_FOUND);
  }
  result.set_limit(limit);
  if (!detail.empty()) {
    result.set_detail(std::string(detail));
  }
  return result;
}

TerminationProto FeasibleTermination(const LimitProto limit,
                                     const absl::string_view detail) {
  return TerminateForLimit(limit, /*feasible=*/true, detail);
}

TerminationProto NoSolutionFoundTermination(const LimitProto limit,
                                            const absl::string_view detail) {
  return TerminateForLimit(limit, /*feasible=*/false, detail);
}

TerminationProto TerminateForReason(const TerminationReasonProto reason,
                                    const absl::string_view detail) {
  TerminationProto result;
  result.set_reason(reason);
  if (!detail.empty()) {
    result.set_detail(std::string(detail));
  }
  return result;
}

absl::Status ModelIsSupported(const ModelProto& model,
                              const SupportedProblemStructures& support_menu,
                              const absl::string_view solver_name) {
  const auto error_status = [solver_name](
                                const absl::string_view structure,
                                const SupportType support) -> absl::Status {
    switch (support) {
      case SupportType::kNotSupported:
        return util::InvalidArgumentErrorBuilder()
               << solver_name << " does not support " << structure;
      case SupportType::kNotImplemented:
        return util::UnimplementedErrorBuilder()
               << "MathOpt does not currently support " << solver_name
               << " models with " << structure;
      case SupportType::kSupported:
        LOG(FATAL) << "Unexpected call with `kSupported`";
    }
  };
  if (const SupportType support = support_menu.integer_variables;
      support != SupportType::kSupported) {
    for (const bool is_integer : model.variables().integers()) {
      if (is_integer) {
        return error_status("integer variables", support);
      }
    }
  }
  if (const SupportType support = support_menu.multi_objectives;
      support != SupportType::kSupported) {
    if (!model.auxiliary_objectives().empty()) {
      return error_status("multiple objectives", support);
    }
  }
  if (const SupportType support = support_menu.quadratic_objectives;
      support != SupportType::kSupported) {
    if (!model.objective().quadratic_coefficients().row_ids().empty()) {
      return error_status("quadratic objectives", support);
    }
    for (const auto& [_, objective] : model.auxiliary_objectives()) {
      if (!objective.quadratic_coefficients().row_ids().empty()) {
        return error_status("quadratic objectives", support);
      }
    }
  }
  if (const SupportType support = support_menu.quadratic_constraints;
      support != SupportType::kSupported) {
    if (!model.quadratic_constraints().empty()) {
      return error_status("quadratic constraints", support);
    }
  }
  if (const SupportType support = support_menu.second_order_cone_constraints;
      support != SupportType::kSupported) {
    if (!model.second_order_cone_constraints().empty()) {
      return error_status("second-order cone constraints", support);
    }
  }
  if (const SupportType support = support_menu.sos1_constraints;
      support != SupportType::kSupported) {
    if (!model.sos1_constraints().empty()) {
      return error_status("sos1 constraints", support);
    }
  }
  if (const SupportType support = support_menu.sos2_constraints;
      support != SupportType::kSupported) {
    if (!model.sos2_constraints().empty()) {
      return error_status("sos2 constraints", support);
    }
  }
  if (const SupportType support = support_menu.indicator_constraints;
      support != SupportType::kSupported) {
    if (!model.indicator_constraints().empty()) {
      return error_status("indicator constraints", support);
    }
  }
  return absl::OkStatus();
}

bool UpdateIsSupported(const ModelUpdateProto& update,
                       const SupportedProblemStructures& support_menu) {
  if (support_menu.integer_variables != SupportType::kSupported) {
    for (const bool is_integer :
         update.variable_updates().integers().values()) {
      if (is_integer) {
        return false;
      }
    }
    for (const bool is_integer : update.new_variables().integers()) {
      if (is_integer) {
        return false;
      }
    }
  }
  if (support_menu.multi_objectives != SupportType::kSupported) {
    if (!update.auxiliary_objectives_updates()
             .deleted_objective_ids()
             .empty() ||
        !update.auxiliary_objectives_updates().new_objectives().empty() ||
        !update.auxiliary_objectives_updates().objective_updates().empty()) {
      return false;
    }
  }
  if (support_menu.quadratic_objectives != SupportType::kSupported) {
    if (!update.objective_updates()
             .quadratic_coefficients()
             .row_ids()
             .empty()) {
      return false;
    }
    for (const auto& [_, new_objective] :
         update.auxiliary_objectives_updates().new_objectives()) {
      if (!new_objective.quadratic_coefficients().row_ids().empty()) {
        return false;
      }
    }
    for (const auto& [_, objective_update] :
         update.auxiliary_objectives_updates().objective_updates()) {
      if (!objective_update.quadratic_coefficients().row_ids().empty()) {
        return false;
      }
    }
  }
  // Duck-types that the proto parameter contains fields named `new_constraints`
  // and `deleted_constraint_ids`. This is standard for "mapped" constraints.
  const auto contains_new_or_deleted_constraints =
      [](const auto& constraint_update) {
        return !constraint_update.new_constraints().empty() ||
               !constraint_update.deleted_constraint_ids().empty();
      };
  if (support_menu.quadratic_constraints != SupportType::kSupported) {
    if (contains_new_or_deleted_constraints(
            update.quadratic_constraint_updates())) {
      return false;
    }
  }
  if (support_menu.second_order_cone_constraints != SupportType::kSupported) {
    if (contains_new_or_deleted_constraints(
            update.second_order_cone_constraint_updates())) {
      return false;
    }
  }
  if (support_menu.sos1_constraints != SupportType::kSupported) {
    if (contains_new_or_deleted_constraints(update.sos1_constraint_updates())) {
      return false;
    }
  }
  if (support_menu.sos2_constraints != SupportType::kSupported) {
    if (contains_new_or_deleted_constraints(update.sos2_constraint_updates())) {
      return false;
    }
  }
  if (support_menu.indicator_constraints != SupportType::kSupported) {
    if (contains_new_or_deleted_constraints(
            update.indicator_constraint_updates())) {
      return false;
    }
  }
  return true;
}

}  // namespace math_opt
}  // namespace operations_research
