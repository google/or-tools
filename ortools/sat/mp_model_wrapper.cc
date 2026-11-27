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

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool ConstraintIsAlwaysTrue(const MPConstraintProto& mp_constraint) {
  return mp_constraint.lower_bound() == -kInfinity &&
         mp_constraint.upper_bound() == kInfinity;
}

// Moves the linear part of `mp_constraint` into a new constraint of `model`,
// and returns it.
EnforcedLinearConstraintProto* MoveLinearConstraint(
    MPConstraintProto* mp_constraint, MipModelProto* model) {
  EnforcedLinearConstraintProto* ct = model->add_constraints();
  *ct->mutable_vars() = std::move(*mp_constraint->mutable_var_index());
  *ct->mutable_coeffs() = std::move(*mp_constraint->mutable_coefficient());
  ct->set_lower_bound(mp_constraint->lower_bound());
  ct->set_upper_bound(mp_constraint->upper_bound());
  if (!mp_constraint->name().empty()) ct->set_name(mp_constraint->name());
  return ct;
}

// Adds to `model` the constraint `lb <= sum(vars) <= ub` enforced by
// `enforcement_var == enforcement_value`.
void AddEnforcedSum(absl::Span<const int> vars, double lb, double ub,
                    int enforcement_var, int64_t enforcement_value,
                    absl::string_view name, MipModelProto* model) {
  EnforcedLinearConstraintProto* ct = model->add_constraints();
  if (!name.empty()) ct->set_name(name);
  for (const int var : vars) {
    ct->add_vars(var);
    ct->add_coeffs(1.0);
  }
  ct->set_lower_bound(lb);
  ct->set_upper_bound(ub);
  ct->add_enforcement_vars(enforcement_var);
  ct->add_enforcement_values(enforcement_value);
}

}  // namespace

bool MPModelProtoWrapper::CanWrap(const MPModelProto& mp_model,
                                  SolverLogger* logger) {
  for (const MPGeneralConstraintProto& general_constraint :
       mp_model.general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint:
      case MPGeneralConstraintProto::kAndConstraint:
      case MPGeneralConstraintProto::kOrConstraint:
        break;
      default:
        SOLVER_LOG(logger, "General constraints of type ",
                   general_constraint.general_constraint_case(),
                   " are not supported.");
        return false;
    }
  }
  return true;
}

MPModelProtoWrapper::MPModelProtoWrapper(MPModelProto mp_model,
                                         SolverLogger* logger) {
  if (!mp_model.name().empty()) model_.set_name(mp_model.name());

  // Variables, with a minimization objective.
  const bool maximize = mp_model.maximize();
  model_.set_objective_scaling(maximize ? -1.0 : 1.0);
  if (mp_model.objective_offset() != 0.0) {
    model_.set_objective_offset(maximize ? -mp_model.objective_offset()
                                         : mp_model.objective_offset());
  }
  model_.mutable_variables()->Reserve(mp_model.variable().size());
  for (MPVariableProto& mp_var : *mp_model.mutable_variable()) {
    MipVariableProto* var = model_.add_variables();
    var->set_lower_bound(mp_var.lower_bound());
    var->set_upper_bound(mp_var.upper_bound());
    const double coeff = mp_var.objective_coefficient();
    if (coeff != 0.0) {
      var->set_objective_coefficient(maximize ? -coeff : coeff);
    }
    if (mp_var.is_integer()) var->set_is_integer(true);
    if (!mp_var.name().empty())
      *var->mutable_name() = std::move(*mp_var.mutable_name());
  }

  // Constraints. The always true ones are dropped.
  for (MPConstraintProto& mp_constraint : *mp_model.mutable_constraint()) {
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;
    MoveLinearConstraint(&mp_constraint, &model_);
  }

  // General constraints.
  int num_indicator_constraints = 0;
  int num_boolean_constraints = 0;
  for (MPGeneralConstraintProto& general_constraint :
       *mp_model.mutable_general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint: {
        MPIndicatorConstraint* indicator_constraint =
            general_constraint.mutable_indicator_constraint();
        MPConstraintProto* mp_constraint =
            indicator_constraint->mutable_constraint();
        if (ConstraintIsAlwaysTrue(*mp_constraint)) continue;

        ++num_indicator_constraints;
        EnforcedLinearConstraintProto* ct =
            MoveLinearConstraint(mp_constraint, &model_);
        if (ct->name().empty() && !general_constraint.name().empty()) {
          ct->set_name(general_constraint.name());
        }
        ct->add_enforcement_vars(indicator_constraint->var_index());
        ct->add_enforcement_values(indicator_constraint->var_value());
        break;
      }
      case MPGeneralConstraintProto::kAndConstraint: {
        ++num_boolean_constraints;
        const auto& and_constraint = general_constraint.and_constraint();
        absl::string_view name = general_constraint.name();
        const int size = and_constraint.var_index().size();
        AddEnforcedSum(and_constraint.var_index(), size, kInfinity,
                       and_constraint.resultant_var_index(), 1,
                       name.empty() ? "" : absl::StrCat(name, "_pos"), &model_);
        AddEnforcedSum(and_constraint.var_index(), -kInfinity, size - 1,
                       and_constraint.resultant_var_index(), 0,
                       name.empty() ? "" : absl::StrCat(name, "_neg"), &model_);
        break;
      }
      case MPGeneralConstraintProto::kOrConstraint: {
        ++num_boolean_constraints;
        const auto& or_constraint = general_constraint.or_constraint();
        absl::string_view name = general_constraint.name();
        AddEnforcedSum(or_constraint.var_index(), 1.0, kInfinity,
                       or_constraint.resultant_var_index(), 1,
                       name.empty() ? "" : absl::StrCat(name, "_pos"), &model_);
        AddEnforcedSum(or_constraint.var_index(), -kInfinity, 0.0,
                       or_constraint.resultant_var_index(), 0,
                       name.empty() ? "" : absl::StrCat(name, "_neg"), &model_);
        break;
      }
      default:
        // This should have failed CanWrap() before, so we don't need to
        // handle the error at this point.
        LOG(FATAL) << "Can't convert general constraints of type "
                   << general_constraint.general_constraint_case()
                   << " to EnforcedLinearConstraintProto.";
        break;
    }
  }

  // Solution hint.
  if (mp_model.has_solution_hint()) {
    MipPartialVariableAssignment* hint = model_.mutable_solution_hint();
    *hint->mutable_vars() =
        std::move(*mp_model.mutable_solution_hint()->mutable_var_index());
    *hint->mutable_values() =
        std::move(*mp_model.mutable_solution_hint()->mutable_var_value());
  }

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

MPModelProtoWrapper MPModelProtoWrapper::FromMipModelProto(
    MipModelProto model) {
  MPModelProtoWrapper wrapper;
  wrapper.model_ = std::move(model);
  return wrapper;
}

void MPModelProtoWrapper::RelaxAllConstraintBounds(
    const double tight_precision) {
  CHECK(std::isfinite(tight_precision) && tight_precision >= 0.0)
      << tight_precision;
  if (tight_precision == 0.0) return;
  for (EnforcedLinearConstraintProto& ct : *model_.mutable_constraints()) {
    ct.set_lower_bound(ExactSumUb(ct.lower_bound(), -tight_precision));
    ct.set_upper_bound(ExactSumLb(ct.upper_bound(), tight_precision));
  }
}

void MPModelProtoWrapper::SetAllConstraintToleranceBudget(
    const double tolerance) {
  CHECK(std::isfinite(tolerance) && tolerance >= 0.0) << tolerance;
  for (EnforcedLinearConstraintProto& ct : *model_.mutable_constraints()) {
    if (tolerance == 0.0) {
      ct.clear_tolerance();
    } else {
      ct.set_tolerance(tolerance);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
