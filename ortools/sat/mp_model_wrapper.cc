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

#include "ortools/sat/mp_model_wrapper.h"

#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool ConstraintIsAlwaysTrue(const MPConstraintProto& mp_constraint) {
  return mp_constraint.lower_bound() == -kInfinity &&
         mp_constraint.upper_bound() == kInfinity;
}

}  // namespace

MPModelProtoWrapper::MPModelProtoWrapper(MPModelProto mp_model,
                                         SolverLogger* logger)
    : mp_model_(std::move(mp_model)) {
  has_non_constant_objective_ = false;
  if (mp_model_.maximize()) {
    objective_scaling_ = -1.0;
    mp_model_.set_maximize(false);
    if (mp_model_.objective_offset() != 0.0) {
      mp_model_.set_objective_offset(-mp_model_.objective_offset());
    }
    const int num_vars = num_variables();
    for (int v = 0; v < num_vars; ++v) {
      const double coeff = variables(v).objective_coefficient();
      if (coeff != 0.0) {
        has_non_constant_objective_ = true;
        mutable_variables(v)->set_objective_coefficient(-coeff);
      }
    }
  } else {
    objective_scaling_ = 1.0;
    has_non_constant_objective_ =
        absl::c_any_of(mp_model_.variable(), [](const MPVariableProto& v) {
          return v.objective_coefficient() != 0.0;
        });
  }

  const int num_initial_constraints = mp_model_.constraint().size();
  for (int c = 0; c < num_initial_constraints; ++c) {
    if (ConstraintIsAlwaysTrue(mp_model_.constraint(c))) continue;
    constraints_.push_back(EnforcedLinearConstraint(c, &mp_model_));
  }

  int num_indicator_constraints = 0;
  int num_boolean_constraints = 0;
  for (const MPGeneralConstraintProto& general_constraint :
       mp_model_.general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint: {
        const auto& indicator_constraint =
            general_constraint.indicator_constraint();
        const MPConstraintProto& mp_constraint =
            indicator_constraint.constraint();
        if (ConstraintIsAlwaysTrue(mp_constraint)) continue;

        ++num_indicator_constraints;
        const int c = mp_model_.constraint().size();
        MPConstraintProto* new_ct = mp_model_.add_constraint();
        *new_ct = mp_constraint;
        if (new_ct->name().empty() && !general_constraint.name().empty()) {
          new_ct->set_name(general_constraint.name());
        }
        constraints_.push_back(EnforcedLinearConstraint(c, &mp_model_));
        const int var = indicator_constraint.var_index();
        const int value = indicator_constraint.var_value();
        constraints_.back().AddEnforcement(var, value);
        break;
      }
      case MPGeneralConstraintProto::kAndConstraint: {
        ++num_boolean_constraints;
        const auto& and_constraint = general_constraint.and_constraint();
        absl::string_view name = general_constraint.name();

        const int c_pos = mp_model_.constraint().size();
        MPConstraintProto* ct_pos = mp_model_.add_constraint();
        ct_pos->set_name(name.empty() ? "" : absl::StrCat(name, "_pos"));
        for (const int var_index : and_constraint.var_index()) {
          ct_pos->add_var_index(var_index);
          ct_pos->add_coefficient(1.0);
        }
        ct_pos->set_lower_bound(and_constraint.var_index().size());
        ct_pos->set_upper_bound(kInfinity);
        constraints_.push_back(EnforcedLinearConstraint(c_pos, &mp_model_));
        constraints_.back().AddEnforcement(and_constraint.resultant_var_index(),
                                           1);

        const int c_neg = mp_model_.constraint().size();
        MPConstraintProto* ct_neg = mp_model_.add_constraint();
        ct_neg->set_name(name.empty() ? "" : absl::StrCat(name, "_neg"));
        for (const int var_index : and_constraint.var_index()) {
          ct_neg->add_var_index(var_index);
          ct_neg->add_coefficient(1.0);
        }
        ct_neg->set_lower_bound(-kInfinity);
        ct_neg->set_upper_bound(and_constraint.var_index().size() - 1);
        constraints_.push_back(EnforcedLinearConstraint(c_neg, &mp_model_));
        constraints_.back().AddEnforcement(and_constraint.resultant_var_index(),
                                           0);
        break;
      }
      case MPGeneralConstraintProto::kOrConstraint: {
        ++num_boolean_constraints;
        const auto& or_constraint = general_constraint.or_constraint();
        absl::string_view name = general_constraint.name();

        const int c_pos = mp_model_.constraint().size();
        MPConstraintProto* ct_pos = mp_model_.add_constraint();
        ct_pos->set_name(name.empty() ? "" : absl::StrCat(name, "_pos"));
        for (const int var_index : or_constraint.var_index()) {
          ct_pos->add_var_index(var_index);
          ct_pos->add_coefficient(1.0);
        }
        ct_pos->set_lower_bound(1.0);
        ct_pos->set_upper_bound(kInfinity);
        constraints_.push_back(EnforcedLinearConstraint(c_pos, &mp_model_));
        constraints_.back().AddEnforcement(or_constraint.resultant_var_index(),
                                           1);

        const int c_neg = mp_model_.constraint().size();
        MPConstraintProto* ct_neg = mp_model_.add_constraint();
        ct_neg->set_name(name.empty() ? "" : absl::StrCat(name, "_neg"));
        for (const int var_index : or_constraint.var_index()) {
          ct_neg->add_var_index(var_index);
          ct_neg->add_coefficient(1.0);
        }
        ct_neg->set_lower_bound(-kInfinity);
        ct_neg->set_upper_bound(0.0);
        constraints_.push_back(EnforcedLinearConstraint(c_neg, &mp_model_));
        constraints_.back().AddEnforcement(or_constraint.resultant_var_index(),
                                           0);
        break;
      }
      default:
        // This should have failed validation before, so we don't need to
        // handle the error at this point.
        LOG(FATAL) << "Can't convert general constraints of type "
                   << general_constraint.general_constraint_case()
                   << " to EnforcedLinearConstraint.";
        break;
    }
  }
  mp_model_.clear_general_constraint();

  if (logger != nullptr) {
    if (num_indicator_constraints > 0) {
      SOLVER_LOG(logger, "MIP model had ",
                 FormatCounter(num_indicator_constraints),
                 " indicator constraints.");
    }
    if (num_boolean_constraints > 0) {
      SOLVER_LOG(logger, "MIP model had ",
                 FormatCounter(num_boolean_constraints),
                 " Boolean constraints.");
    }
  }
}

}  // namespace sat
}  // namespace operations_research
