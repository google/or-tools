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

#include "ortools/sat/debug_solution.h"

#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

void DebugSolution::SynchronizeWithShared(const CpModelProto& model_proto) {
  if (shared_response_->DebugSolution().empty()) return;

  if (!SolutionIsFeasible(model_proto, shared_response_->DebugSolution())) {
    // TODO(user): we should probably CHECK-fail.
    SOLVER_LOG(logger_, "Debug solution is not feasible.");
    return;
  }
  SOLVER_LOG(logger_, "Debug solution is feasible.");

  // Copy the proto values.
  proto_values_ = shared_response_->DebugSolution();

  // Fill the values by integer variable.
  const int num_integers = integer_trail_->NumIntegerVariables().value();
  ivar_has_value_.assign(num_integers, false);
  ivar_values_.assign(num_integers, 0);
  boolean_solution_.clear();

  for (int i = 0; i < proto_values_.size(); ++i) {
    if (mapping_->IsBoolean(i)) {
      Literal l = mapping_->Literal(i);
      if (proto_values_[i] == 0) {
        l = l.Negated();
      }
      boolean_solution_.push_back(l);
    }

    if (!mapping_->IsInteger(i)) continue;
    const IntegerVariable var = mapping_->Integer(i);
    ivar_has_value_[var] = true;
    ivar_has_value_[NegationOf(var)] = true;
    ivar_values_[var] = proto_values_[i];
    ivar_values_[NegationOf(var)] = -proto_values_[i];
  }

  // Also add the trivial literal that is sometimes created by the loader
  if (trivial_literals_->TrueLiteral().Variable().value() ==
      proto_values_.size()) {
    boolean_solution_.push_back(trivial_literals_->TrueLiteral());
  }

  // The objective variable is usually not part of the proto, but it is still
  // nice to have it, so we recompute it here.
  if (objective_def_ != nullptr &&
      objective_def_->objective_var != kNoIntegerVariable) {
    if (absl::c_all_of(objective_def_->vars, [this](IntegerVariable var) {
          return var < ivar_has_value_.end_index() && ivar_has_value_[var];
        })) {
      const IntegerVariable objective_var = objective_def_->objective_var;
      if (objective_var + 1 >= ivar_has_value_.size()) {
        ivar_has_value_.resize(objective_var + 2, false);
        ivar_values_.resize(objective_var + 2, 0);
      }
      IntegerValue objective_value = 0;
      for (int i = 0; i < objective_def_->vars.size(); ++i) {
        objective_value +=
            objective_def_->coeffs[i] * ivar_values_[objective_def_->vars[i]];
      }
      SOLVER_LOG(
          logger_,
          absl::StrCat("Debug solution objective value: ",
                       objective_def_->ScaleIntegerObjective(objective_value)));
      ivar_has_value_[objective_var] = true;
      ivar_has_value_[NegationOf(objective_var)] = true;
      ivar_values_[objective_var] = objective_value;
      ivar_values_[NegationOf(objective_var)] = -objective_value;
      inner_objective_value_ = objective_value;
    }
  }
}

bool DebugSolution::CheckClause(
    absl::Span<const Literal> clause,
    absl::Span<const IntegerLiteral> integers) const {
  if (IsLookingForSolutionBetterThanDebugSolution()) return true;
  if (proto_values_.empty()) return true;

  bool is_satisfied = false;
  std::vector<std::tuple<Literal, IntegerLiteral, IntegerValue>> to_print;
  for (const Literal l : clause) {
    // First case, this Boolean is mapped.
    {
      const int proto_var =
          mapping_->GetProtoVariableFromBooleanVariable(l.Variable());
      if (proto_var != -1) {
        CHECK_LT(proto_var, proto_values_.size());
        to_print.push_back({l, IntegerLiteral(), proto_values_[proto_var]});
        if (proto_values_[proto_var] == (l.IsPositive() ? 1 : 0)) {
          is_satisfied = true;
          break;
        }
        continue;
      }
    }

    // Second case, it is associated to IntVar >= value.
    // We can use any of them, so if one is false, we use this one.
    bool all_true = true;
    for (const IntegerLiteral associated : encoder_->GetIntegerLiterals(l)) {
      if (associated.var >= ivar_has_value_.end_index() ||
          !ivar_has_value_[associated.var]) {
        continue;
      }
      const IntegerValue value = ivar_values_[associated.var];
      to_print.push_back({l, associated, value});

      if (value < associated.bound) {
        all_true = false;
        break;
      }
    }
    if (all_true) {
      is_satisfied = true;
      break;
    }
  }
  for (const IntegerLiteral i_lit : integers) {
    DCHECK(!i_lit.IsAlwaysFalse());
    if (i_lit.IsAlwaysTrue()) continue;
    if (i_lit.var >= ivar_has_value_.end_index() ||
        !ivar_has_value_[i_lit.var]) {
      is_satisfied = true;
      break;
    }

    const IntegerValue value = ivar_values_[i_lit.var];
    to_print.push_back({Literal(kNoLiteralIndex), i_lit, value});

    // This is a bit confusing, but since the i_lit in the reason are
    // not "negated", we need at least one to be FALSE, for the reason to
    // be valid.
    if (value < i_lit.bound) {
      is_satisfied = true;
      break;
    }
  }
  if (!is_satisfied) {
    LOG(INFO) << "Reason clause is not satisfied by loaded solution:";
    LOG(INFO) << "Worker '" << name_
              << "', level=" << sat_solver_->CurrentDecisionLevel();
    LOG(INFO) << "literals (neg): " << clause;
    LOG(INFO) << "integer literals: " << integers;
    for (const auto [l, i_lit, solution_value] : to_print) {
      if (i_lit.IsAlwaysTrue()) {
        const int proto_var =
            mapping_->GetProtoVariableFromBooleanVariable(l.Variable());
        LOG(INFO) << l << " (bool in model) proto_var=" << proto_var
                  << " value_in_sol=" << solution_value;
      } else {
        const int proto_var = mapping_->GetProtoVariableFromIntegerVariable(
            PositiveVariable(i_lit.var));
        LOG(INFO) << l << " " << i_lit << " proto_var="
                  << (proto_var == -1 ? "none" : absl::StrCat(proto_var))
                  << " value_in_sol="
                  << (VariableIsPositive(i_lit.var) ? solution_value
                                                    : -solution_value);
      }
    }
  }
  return is_satisfied;
}

bool DebugSolution::CheckCut(const LinearConstraint& cut,
                             bool only_check_ub) const {
  if (IsLookingForSolutionBetterThanDebugSolution()) return true;
  if (proto_values_.empty()) return true;
  absl::int128 activity(0);
  for (int i = 0; i < cut.num_terms; ++i) {
    const IntegerVariable var = cut.vars[i];
    const IntegerValue coeff = cut.coeffs[i];
    if (var >= ivar_has_value_.size() || !ivar_has_value_[var]) {
      return true;
    }
    activity += absl::int128(coeff.value()) * ivar_values_[var].value();
  }
  if (only_check_ub) {
    if (activity > cut.ub.value()) {
      LOG(INFO) << cut.DebugString();
      LOG(INFO) << "activity " << activity << " > " << cut.ub;
      LOG(INFO) << "Cut is not satisfied by loaded solution.";
      return false;
    }
  } else {
    if (activity > cut.ub.value() || activity < cut.lb.value()) {
      LOG(INFO) << cut.DebugString();
      LOG(INFO) << "activity " << activity << " not in [" << cut.lb << ","
                << cut.ub << "]";
      LOG(INFO) << "Cut is not satisfied by loaded solution.";
      return false;
    }
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
