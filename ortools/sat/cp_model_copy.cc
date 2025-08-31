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

#include "ortools/sat/cp_model_copy.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

ModelCopy::ModelCopy(PresolveContext* context) : context_(context) {}

void ModelCopy::ImportVariablesAndMaybeIgnoreNames(
    const CpModelProto& in_model) {
  if (context_->params().ignore_names()) {
    context_->working_model->clear_variables();
    context_->working_model->mutable_variables()->Reserve(
        in_model.variables_size());
    for (const IntegerVariableProto& var_proto : in_model.variables()) {
      *context_->working_model->add_variables()->mutable_domain() =
          var_proto.domain();
    }
  } else {
    *context_->working_model->mutable_variables() = in_model.variables();
  }
}

void ModelCopy::CreateVariablesFromDomains(absl::Span<const Domain> domains) {
  for (const Domain& domain : domains) {
    FillDomainInProto(domain, context_->working_model->add_variables());
  }
}

// TODO(user): Merge with the phase 1 of the presolve code.
//
// TODO(user): It seems easy to forget to update this if any new constraint
// contains an interval or if we add a field to an existing constraint. Find a
// way to remind contributor to not forget this.
bool ModelCopy::ImportAndSimplifyConstraints(
    const CpModelProto& in_model, bool first_copy,
    std::function<bool(int)> active_constraints) {
  context_->InitializeNewDomains();
  if (context_->ModelIsUnsat()) return false;
  const bool ignore_names = context_->params().ignore_names();

  // If first_copy is true, we reorder the scheduling constraint to be sure they
  // refer to interval before them.
  std::vector<int> constraints_using_intervals;

  interval_mapping_.assign(in_model.constraints().size(), -1);

  starting_constraint_index_ = context_->working_model->constraints_size();
  for (int c = 0; c < in_model.constraints_size(); ++c) {
    if (active_constraints != nullptr && !active_constraints(c)) {
      continue;
    }
    const ConstraintProto& ct = in_model.constraints(c);
    if (first_copy) {
      if (!PrepareEnforcementCopyWithDup(ct)) continue;
    } else {
      if (!PrepareEnforcementCopy(ct)) continue;
    }

    // TODO(user): if ignore_names is false, we should make sure the
    // name are properly copied by all these functions. Or we should never copy
    // name and have a separate if (!ignore_name) copy the name...
    switch (ct.constraint_case()) {
      case ConstraintProto::CONSTRAINT_NOT_SET:
        break;
      case ConstraintProto::kBoolOr:
        if (first_copy) {
          if (!CopyBoolOrWithDupSupport(ct)) return CreateUnsatModel(c, ct);
        } else {
          if (!CopyBoolOr(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kBoolAnd:
        if (temp_enforcement_literals_.empty()) {
          for (const int lit : ct.bool_and().literals()) {
            context_->UpdateRuleStats("bool_and: non-reified.");
            if (!context_->SetLiteralToTrue(lit)) {
              return CreateUnsatModel(c, ct);
            }
          }
        } else if (first_copy) {
          if (!CopyBoolAndWithDupSupport(ct)) return CreateUnsatModel(c, ct);
        } else {
          if (!CopyBoolAnd(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kLinear:
        if (!CopyLinear(ct, /*canonicalize=*/first_copy)) {
          return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kIntProd:
        if (!CopyIntProd(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntDiv:
        if (!CopyIntDiv(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntMod:
        if (!CopyIntMod(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kElement:
        if (!CopyElement(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kTable:
        if (!CopyTable(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAutomaton:
        if (!CopyAutomaton(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAllDiff:
        if (!CopyAllDiff(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kLinMax:
        if (!CopyLinMax(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAtMostOne:
        if (!CopyAtMostOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kExactlyOne:
        if (!CopyExactlyOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kInterval:
        if (!CopyInterval(ct, c, ignore_names)) return CreateUnsatModel(c, ct);
        if (first_copy) {
          if (!AddLinearConstraintForInterval(ct))
            return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kNoOverlap:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap(ct);
        }
        break;
      case ConstraintProto::kNoOverlap2D:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap2D(ct);
        }
        break;
      case ConstraintProto::kCumulative:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      default: {
        ConstraintProto* new_ct = context_->working_model->add_constraints();
        *new_ct = ct;
        new_ct->mutable_enforcement_literal()->Clear();
        FinishEnforcementCopy(new_ct);
        if (ignore_names) {
          // TODO(user): find a better way than copy then clear_name()?
          new_ct->clear_name();
        }
      }
    }
  }

  // This should be empty if first_copy is false.
  DCHECK(first_copy || constraints_using_intervals.empty());
  for (const int c : constraints_using_intervals) {
    const ConstraintProto& ct = in_model.constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kNoOverlap:
        CopyAndMapNoOverlap(ct);
        break;
      case ConstraintProto::kNoOverlap2D:
        CopyAndMapNoOverlap2D(ct);
        break;
      case ConstraintProto::kCumulative:
        if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        break;
      default:
        LOG(DFATAL) << "Shouldn't be here.";
    }
  }

  if (first_copy) {
    ExpandNonAffineExpressions();
  }
  return true;
}

bool ModelCopy::PrepareEnforcementCopy(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) continue;
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("enforcement: always false");
      return false;
    }
    temp_enforcement_literals_.push_back(lit);
  }
  return true;  // Continue processing.
}

bool ModelCopy::PrepareEnforcementCopyWithDup(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  temp_enforcement_literals_set_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) continue;
    if (temp_enforcement_literals_set_.contains(lit)) {
      context_->UpdateRuleStats("enforcement: removed duplicate literal");
      continue;
    }

    // Cannot be satisfied.
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("enforcement: always false");
      return false;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("enforcement: contains x and not(x)");
      return false;
    }

    temp_enforcement_literals_.push_back(lit);
    temp_enforcement_literals_set_.insert(lit);
  }
  return true;  // Continue processing.
}

void ModelCopy::FinishEnforcementCopy(ConstraintProto* ct) {
  ct->mutable_enforcement_literal()->Add(temp_enforcement_literals_.begin(),
                                         temp_enforcement_literals_.end());
}

bool ModelCopy::FinishBoolOrCopy() {
  if (temp_literals_.empty()) return false;

  if (temp_literals_.size() == 1) {
    context_->UpdateRuleStats("bool_or: only one literal");
    return context_->SetLiteralToTrue(temp_literals_[0]);
  }

  context_->working_model->add_constraints()
      ->mutable_bool_or()
      ->mutable_literals()
      ->Add(temp_literals_.begin(), temp_literals_.end());
  return true;
}

bool ModelCopy::CopyBoolOr(const ConstraintProto& ct) {
  temp_literals_.clear();
  for (const int lit : temp_enforcement_literals_) {
    temp_literals_.push_back(NegatedRef(lit));
  }
  for (const int lit : ct.bool_or().literals()) {
    if (context_->LiteralIsTrue(lit)) {
      return true;
    }
    if (!context_->LiteralIsFalse(lit)) {
      temp_literals_.push_back(lit);
    }
  }
  return FinishBoolOrCopy();
}

bool ModelCopy::CopyBoolOrWithDupSupport(const ConstraintProto& ct) {
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int enforcement_lit : temp_enforcement_literals_) {
    // Having an enforcement literal is the same as having its negation on
    // the clause.
    const int lit = NegatedRef(enforcement_lit);

    // Note that we already dealt with duplicate since we should have called
    // PrepareEnforcementCopyWithDup() in this case.
    temp_literals_set_.insert(lit);
    temp_literals_.push_back(lit);
  }
  for (const int lit : ct.bool_or().literals()) {
    if (context_->LiteralIsTrue(lit)) {
      context_->UpdateRuleStats("bool_or: always true");
      return true;
    }
    if (context_->LiteralIsFalse(lit)) continue;
    if (temp_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool_or: always true");
      return true;
    }
    const auto [it, inserted] = temp_literals_set_.insert(lit);
    if (inserted) temp_literals_.push_back(lit);
  }
  return FinishBoolOrCopy();
}

bool ModelCopy::CopyBoolAnd(const ConstraintProto& ct) {
  bool at_least_one_false = false;
  int num_non_fixed_literals = 0;
  for (const int lit : ct.bool_and().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      at_least_one_false = true;
      break;
    }
    if (!context_->LiteralIsTrue(lit)) {
      num_non_fixed_literals++;
    }
  }

  if (at_least_one_false) {
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        context_->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  } else if (num_non_fixed_literals > 0) {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    FinishEnforcementCopy(new_ct);
    BoolArgumentProto* bool_and = new_ct->mutable_bool_and();
    bool_and->mutable_literals()->Reserve(num_non_fixed_literals);
    for (const int lit : ct.bool_and().literals()) {
      if (context_->LiteralIsTrue(lit)) continue;
      bool_and->add_literals(lit);
    }
  }
  return true;
}

bool ModelCopy::CopyBoolAndWithDupSupport(const ConstraintProto& ct) {
  DCHECK(!temp_enforcement_literals_.empty());

  bool at_least_one_false = false;
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int lit : ct.bool_and().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("bool and: always false");
      at_least_one_false = true;
      break;
    }
    if (temp_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool and: => x and not(x) ");
      at_least_one_false = true;
      break;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool and: not(x) => x");
      at_least_one_false = true;
      break;
    }

    if (context_->LiteralIsTrue(lit)) continue;
    if (temp_enforcement_literals_set_.contains(lit)) {
      context_->UpdateRuleStats("bool and: x => x");
      continue;
    }
    const auto [it, inserted] = temp_literals_set_.insert(lit);
    if (inserted) temp_literals_.push_back(lit);
  }

  if (at_least_one_false) {
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        context_->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  }

  if (temp_literals_.empty()) {
    context_->UpdateRuleStats("bool and: empty");
    return true;
  }

  // Copy.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_bool_and()->mutable_literals()->Add(temp_literals_.begin(),
                                                      temp_literals_.end());
  return true;
}

bool ModelCopy::CopyLinearExpression(
    const LinearExpressionProto& expr, LinearExpressionProto* dst,
    absl::Span<const int> enforcement_literals) {
  non_fixed_variables_.clear();
  non_fixed_coefficients_.clear();
  int64_t offset = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int ref = expr.vars(i);
    const int64_t coeff = expr.coeffs(i);
    if (coeff == 0) continue;
    if (context_->IsFixed(ref)) {
      offset += coeff * context_->MinOf(ref);
      continue;
    }

    // Make sure we never have negative ref in a linear constraint.
    if (RefIsPositive(ref)) {
      non_fixed_variables_.push_back(ref);
      non_fixed_coefficients_.push_back(coeff);
    } else {
      non_fixed_variables_.push_back(NegatedRef(ref));
      non_fixed_coefficients_.push_back(-coeff);
    }
  }

  dst->set_offset(offset);
  dst->mutable_vars()->Add(non_fixed_variables_.begin(),
                           non_fixed_variables_.end());
  dst->mutable_coeffs()->Add(non_fixed_coefficients_.begin(),
                             non_fixed_coefficients_.end());
  // TODO(user): We could save work by only doing this if this is the first
  // copy.
  context_->CanonicalizeLinearExpression(enforcement_literals, dst);
  return true;
}

bool ModelCopy::CopyLinear(const ConstraintProto& ct, bool canonicalize) {
  non_fixed_variables_.clear();
  non_fixed_coefficients_.clear();
  int64_t offset = 0;
  int64_t min_activity = 0;
  int64_t max_activity = 0;
  for (int i = 0; i < ct.linear().vars_size(); ++i) {
    const int ref = ct.linear().vars(i);
    const int64_t coeff = ct.linear().coeffs(i);
    if (coeff == 0) continue;
    if (context_->IsFixed(ref)) {
      offset += coeff * context_->MinOf(ref);
      continue;
    }

    if (coeff > 0) {
      min_activity += coeff * context_->MinOf(ref);
      max_activity += coeff * context_->MaxOf(ref);
    } else {
      min_activity += coeff * context_->MaxOf(ref);
      max_activity += coeff * context_->MinOf(ref);
    }

    // Make sure we never have negative ref in a linear constraint.
    if (RefIsPositive(ref)) {
      non_fixed_variables_.push_back(ref);
      non_fixed_coefficients_.push_back(coeff);
    } else {
      non_fixed_variables_.push_back(NegatedRef(ref));
      non_fixed_coefficients_.push_back(-coeff);
    }
  }

  const Domain implied(min_activity, max_activity);
  const Domain new_rhs =
      ReadDomainFromProto(ct.linear()).AdditionWith(Domain(-offset));

  // Trivial constraint?
  if (implied.IsIncludedIn(new_rhs)) {
    context_->UpdateRuleStats("linear: always true");
    return true;
  }

  // Constraint is false?
  const Domain tight_domain = implied.IntersectionWith(new_rhs);
  if (tight_domain.IsEmpty()) {
    if (ct.enforcement_literal().empty()) return false;
    temp_literals_.clear();
    for (const int literal : ct.enforcement_literal()) {
      if (!context_->LiteralIsTrue(literal)) {
        temp_literals_.push_back(NegatedRef(literal));
      }
    }
    context_->working_model->add_constraints()
        ->mutable_bool_or()
        ->mutable_literals()
        ->Add(temp_literals_.begin(), temp_literals_.end());
    return !temp_literals_.empty();
  }

  DCHECK(!non_fixed_variables_.empty());

  if (non_fixed_variables_.size() == 1 && ct.enforcement_literal().empty()) {
    context_->UpdateRuleStats("linear1: x in domain");
    return context_->IntersectDomainWith(
        non_fixed_variables_[0],
        new_rhs.InverseMultiplicationBy(non_fixed_coefficients_[0]));
  }

  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  LinearConstraintProto* linear = new_ct->mutable_linear();
  linear->mutable_vars()->Add(non_fixed_variables_.begin(),
                              non_fixed_variables_.end());
  linear->mutable_coeffs()->Add(non_fixed_coefficients_.begin(),
                                non_fixed_coefficients_.end());
  FillDomainInProto(tight_domain, linear);
  if (canonicalize) {
    context_->CanonicalizeLinearConstraint(new_ct);
    // We checked if the constraint was trivial above, but canonicalization can
    // make it trivial again by simplifying expressions like (x - x).
    if (new_ct->linear().vars().empty() &&
        ReadDomainFromProto(new_ct->linear()).Contains(0)) {
      context_->UpdateRuleStats("linear: trivial 0=0");
      context_->working_model->mutable_constraints()->RemoveLast();
      return true;
    }
  }
  return true;
}

bool ModelCopy::CopyElement(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (ct.element().vars().empty() && !ct.element().exprs().empty()) {
    // New format, just copy.
    *new_ct = ct;
    new_ct->mutable_enforcement_literal()->Clear();
    FinishEnforcementCopy(new_ct);
    return true;
  }

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  FinishEnforcementCopy(new_ct);
  fill_expr(ct.element().index(),
            new_ct->mutable_element()->mutable_linear_index());
  fill_expr(ct.element().target(),
            new_ct->mutable_element()->mutable_linear_target());
  for (const int var : ct.element().vars()) {
    fill_expr(var, new_ct->mutable_element()->add_exprs());
  }
  return true;
}

bool ModelCopy::CopyAutomaton(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  new_ct->mutable_automaton()->set_starting_state(
      ct.automaton().starting_state());
  *new_ct->mutable_automaton()->mutable_final_states() =
      ct.automaton().final_states();
  *new_ct->mutable_automaton()->mutable_transition_tail() =
      ct.automaton().transition_tail();
  *new_ct->mutable_automaton()->mutable_transition_head() =
      ct.automaton().transition_head();
  *new_ct->mutable_automaton()->mutable_transition_label() =
      ct.automaton().transition_label();
  for (const LinearExpressionProto& expr : ct.automaton().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_automaton()->add_exprs());
  }
  FinishEnforcementCopy(new_ct);

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  for (const int var : ct.automaton().vars()) {
    fill_expr(var, new_ct->mutable_automaton()->add_exprs());
  }

  return true;
}

bool ModelCopy::CopyTable(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (ct.table().vars().empty() && !ct.table().exprs().empty()) {
    // New format, just copy.
    *new_ct = ct;
    new_ct->mutable_enforcement_literal()->Clear();
    FinishEnforcementCopy(new_ct);
    return true;
  }

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  FinishEnforcementCopy(new_ct);
  for (const int var : ct.table().vars()) {
    fill_expr(var, new_ct->mutable_table()->add_exprs());
  }
  *new_ct->mutable_table()->mutable_values() = ct.table().values();
  new_ct->mutable_table()->set_negated(ct.table().negated());
  *new_ct->mutable_enforcement_literal() = ct.enforcement_literal();

  return true;
}

bool ModelCopy::CopyAllDiff(const ConstraintProto& ct) {
  if (ct.all_diff().exprs().size() <= 1) return true;
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_all_diff()->add_exprs());
  }
  FinishEnforcementCopy(new_ct);
  return true;
}

bool ModelCopy::CopyLinMax(const ConstraintProto& ct) {
  // We will create it lazily if we end up copying something.
  ConstraintProto* new_ct = nullptr;

  // Regroup all constant terms and copy the other.
  int64_t max_of_fixed_terms = std::numeric_limits<int64_t>::min();
  for (const auto& expr : ct.lin_max().exprs()) {
    const std::optional<int64_t> fixed = context_->FixedValueOrNullopt(expr);
    if (fixed != std::nullopt) {
      max_of_fixed_terms = std::max(max_of_fixed_terms, fixed.value());
    } else {
      // copy.
      if (new_ct == nullptr) {
        new_ct = context_->working_model->add_constraints();
      }
      CopyLinearExpression(expr, new_ct->mutable_lin_max()->add_exprs());
    }
  }

  // If we have no non-fixed expression, we can just fix the target when it
  // involve at most one variable.
  if (new_ct == nullptr && ct.enforcement_literal().empty() &&
      ct.lin_max().target().vars().size() <= 1) {
    context_->UpdateRuleStats("lin_max: all exprs fixed during copy");
    return context_->IntersectDomainWith(ct.lin_max().target(),
                                         Domain(max_of_fixed_terms));
  }

  // Otherwise, add a constant term if needed.
  if (max_of_fixed_terms > std::numeric_limits<int64_t>::min()) {
    if (new_ct == nullptr) {
      new_ct = context_->working_model->add_constraints();
    }
    new_ct->mutable_lin_max()->add_exprs()->set_offset(max_of_fixed_terms);
  }

  // Finish by copying the target.
  if (new_ct == nullptr) return false;  // No expr == unsat.
  CopyLinearExpression(ct.lin_max().target(),
                       new_ct->mutable_lin_max()->mutable_target());
  FinishEnforcementCopy(new_ct);
  return true;
}

bool ModelCopy::CopyAtMostOne(const ConstraintProto& ct) {
  if (!ct.enforcement_literal().empty()) {
    ConstraintProto new_ct;
    FinishEnforcementCopy(&new_ct);
    LiteralsToLinear(ct.at_most_one().literals(), /*lb=*/0, /*ub=*/1,
                     new_ct.mutable_linear());
    return CopyLinear(new_ct, true);
  }
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.at_most_one().literals()) {
    if (context_->LiteralIsFalse(lit)) continue;
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.size() <= 1) return true;
  if (num_true > 1) return false;

  // TODO(user): presolve if num_true == 1.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  new_ct->mutable_at_most_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyExactlyOne(const ConstraintProto& ct) {
  if (!ct.enforcement_literal().empty()) {
    ConstraintProto new_ct;
    FinishEnforcementCopy(&new_ct);
    LiteralsToLinear(ct.exactly_one().literals(), /*lb=*/1, /*ub=*/1,
                     new_ct.mutable_linear());
    return CopyLinear(new_ct, true);
  }
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.exactly_one().literals()) {
    if (context_->LiteralIsFalse(lit)) continue;
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.empty() || num_true > 1) return false;
  if (temp_literals_.size() == 1 && num_true == 1) return true;

  // TODO(user): presolve if num_true == 1 and not everything is false.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  new_ct->mutable_exactly_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyInterval(const ConstraintProto& ct, int c,
                             bool ignore_names) {
  CHECK_EQ(starting_constraint_index_, 0)
      << "Adding new interval constraints to partially filled model is not "
         "supported.";
  interval_mapping_[c] = context_->working_model->constraints_size();
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  *new_ct->mutable_enforcement_literal() = ct.enforcement_literal();
  CopyLinearExpression(ct.interval().start(),
                       new_ct->mutable_interval()->mutable_start(),
                       ct.enforcement_literal());
  CopyLinearExpression(ct.interval().size(),
                       new_ct->mutable_interval()->mutable_size(),
                       ct.enforcement_literal());
  CopyLinearExpression(ct.interval().end(),
                       new_ct->mutable_interval()->mutable_end(),
                       ct.enforcement_literal());
  return true;
}

bool ModelCopy::CopyIntProd(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_prod()->add_exprs());
  }
  CopyLinearExpression(ct.int_prod().target(),
                       new_ct->mutable_int_prod()->mutable_target());
  return true;
}

bool ModelCopy::CopyIntDiv(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_div().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_div()->add_exprs());
  }
  CopyLinearExpression(ct.int_div().target(),
                       new_ct->mutable_int_div()->mutable_target());
  return true;
}

bool ModelCopy::CopyIntMod(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  FinishEnforcementCopy(new_ct);
  for (const LinearExpressionProto& expr : ct.int_mod().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_mod()->add_exprs());
  }
  CopyLinearExpression(ct.int_mod().target(),
                       new_ct->mutable_int_mod()->mutable_target());
  return true;
}

bool ModelCopy::AddLinearConstraintForInterval(const ConstraintProto& ct) {
  // Add the linear constraint enforcement => (start + size == end).
  //
  // We rely on the presolve for simplification, but deal with the trivial
  // case of (start, offset, start + offset) here.
  const IntervalConstraintProto& itv = ct.interval();
  if (itv.size().vars().empty() &&
      itv.start().offset() + itv.size().offset() == itv.end().offset() &&
      absl::Span<const int>(itv.start().vars()) ==
          absl::Span<const int>(itv.end().vars()) &&
      absl::Span<const int64_t>(itv.start().coeffs()) ==
          absl::Span<const int64_t>(itv.end().coeffs())) {
    // Trivial constraint, nothing to do.
  } else {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    LinearConstraintProto* mutable_linear = tmp_constraint_.mutable_linear();

    mutable_linear->add_domain(0);
    mutable_linear->add_domain(0);
    AddLinearExpressionToLinearConstraint(itv.start(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.size(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.end(), -1, mutable_linear);
    if (!CopyLinear(tmp_constraint_, true)) return false;
  }

  // An enforced interval must have its size non-negative.
  const LinearExpressionProto& size_expr = itv.size();
  if (context_->MinOf(size_expr) < 0) {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    *tmp_constraint_.mutable_linear()->mutable_vars() = size_expr.vars();
    *tmp_constraint_.mutable_linear()->mutable_coeffs() = size_expr.coeffs();
    tmp_constraint_.mutable_linear()->add_domain(-size_expr.offset());
    tmp_constraint_.mutable_linear()->add_domain(
        std::numeric_limits<int64_t>::max());
    if (!CopyLinear(tmp_constraint_, true)) return false;
  }

  return true;
}

void ModelCopy::CopyAndMapNoOverlap(const ConstraintProto& ct) {
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_no_overlap();
  new_ct->mutable_intervals()->Reserve(ct.no_overlap().intervals().size());
  for (const int index : ct.no_overlap().intervals()) {
    const int new_index = interval_mapping_[index];
    if (new_index != -1) {
      new_ct->add_intervals(new_index);
    }
  }
}

void ModelCopy::CopyAndMapNoOverlap2D(const ConstraintProto& ct) {
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_no_overlap_2d();

  const int num_intervals = ct.no_overlap_2d().x_intervals().size();
  new_ct->mutable_x_intervals()->Reserve(num_intervals);
  new_ct->mutable_y_intervals()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const int new_x = interval_mapping_[ct.no_overlap_2d().x_intervals(i)];
    if (new_x == -1) continue;
    const int new_y = interval_mapping_[ct.no_overlap_2d().y_intervals(i)];
    if (new_y == -1) continue;
    new_ct->add_x_intervals(new_x);
    new_ct->add_y_intervals(new_y);
  }
}

bool ModelCopy::CopyAndMapCumulative(const ConstraintProto& ct) {
  if (ct.cumulative().intervals().empty() &&
      context_->IsFixed(ct.cumulative().capacity())) {
    // Trivial constraint, either obviously SAT or UNSAT.
    return context_->FixedValue(ct.cumulative().capacity()) >= 0;
  }
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_cumulative();
  CopyLinearExpression(ct.cumulative().capacity(), new_ct->mutable_capacity());

  const int num_intervals = ct.cumulative().intervals().size();
  new_ct->mutable_intervals()->Reserve(num_intervals);
  new_ct->mutable_demands()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const int new_index = interval_mapping_[ct.cumulative().intervals(i)];
    if (new_index != -1) {
      new_ct->add_intervals(new_index);
      CopyLinearExpression(ct.cumulative().demands(i), new_ct->add_demands());
    }
  }

  return true;
}

bool ModelCopy::CreateUnsatModel(int c, const ConstraintProto& ct) {
  context_->working_model->mutable_constraints()->Clear();
  context_->working_model->add_constraints()->mutable_bool_or();

  // If the model was already marked as unsat, we keep the old message and just
  // return. TODO(user): Append messages instead?
  if (context_->ModelIsUnsat()) return false;

  std::string proto_string;
#if !defined(__PORTABLE_PLATFORM__)
  google::protobuf::TextFormat::Printer printer;
  SetupTextFormatPrinter(&printer);
  printer.PrintToString(ct, &proto_string);
#endif  // !defined(__PORTABLE_PLATFORM__)
  std::string message = absl::StrCat(
      "proven during initial copy of constraint #", c, ":\n", proto_string);
  std::vector<int> vars = UsedVariables(ct);
  if (vars.size() < 10) {
    absl::StrAppend(&message, "With current variable domains:\n");
    for (const int var : vars) {
      absl::StrAppend(&message, "var:", var,
                      " domain:", context_->DomainOf(var).ToString(), "\n");
    }
  }
  return context_->NotifyThatModelIsUnsat(message);
}

void ModelCopy::ExpandNonAffineExpressions() {
  // Make sure all domains are initialized (they are used in
  // MaybeExpandNonAffineExpression()).
  context_->InitializeNewDomains();

  non_affine_expression_to_new_var_.clear();
  for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
    ConstraintProto* const ct = context_->working_model->mutable_constraints(c);
    switch (ct->constraint_case()) {
      case ConstraintProto::kIntDiv:
        MaybeExpandNonAffineExpressions(ct->mutable_int_div());
        break;
      case ConstraintProto::kIntMod:
        MaybeExpandNonAffineExpressions(ct->mutable_int_mod());
        break;
      case ConstraintProto::kIntProd:
        MaybeExpandNonAffineExpressions(ct->mutable_int_prod());
        break;
      case ConstraintProto::kAllDiff:
        for (LinearExpressionProto& expr :
             *ct->mutable_all_diff()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kElement:
        if (!ct->element().exprs().empty()) {
          MaybeExpandNonAffineExpression(
              ct->mutable_element()->mutable_linear_index());
          MaybeExpandNonAffineExpression(
              ct->mutable_element()->mutable_linear_target());
          for (LinearExpressionProto& expr :
               *ct->mutable_element()->mutable_exprs()) {
            MaybeExpandNonAffineExpression(&expr);
          }
        }
        break;
      case ConstraintProto::kInterval:
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_start());
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_end());
        MaybeExpandNonAffineExpression(ct->mutable_interval()->mutable_size());
        break;
      case ConstraintProto::kReservoir:
        for (LinearExpressionProto& expr :
             *ct->mutable_reservoir()->mutable_time_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kRoutes:
        for (RoutesConstraintProto::NodeExpressions& node_exprs :
             *ct->mutable_routes()->mutable_dimensions()) {
          for (LinearExpressionProto& expr : *node_exprs.mutable_exprs()) {
            MaybeExpandNonAffineExpression(&expr);
          }
        }
        break;
      case ConstraintProto::kTable:
        for (LinearExpressionProto& expr :
             *ct->mutable_table()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      case ConstraintProto::kAutomaton:
        for (LinearExpressionProto& expr :
             *ct->mutable_automaton()->mutable_exprs()) {
          MaybeExpandNonAffineExpression(&expr);
        }
        break;
      default:
        break;
    }
  }
}

// Replaces the expression sum a_i * x_i + c with gcd * y + c, where y is a new
// variable defined with an additional constraint y = sum a_i / gcd * x_i.
void ModelCopy::MaybeExpandNonAffineExpression(LinearExpressionProto* expr) {
  if (expr->vars_size() < 2) return;

  int64_t gcd = std::abs(expr->coeffs(0));
  for (int i = 1; i < expr->coeffs().size(); ++i) {
    gcd = std::gcd(gcd, std::abs(expr->coeffs(i)));
  }
  Domain domain(0);
  std::vector<std::pair<int, int64_t>> definition;
  for (int i = 0; i < expr->vars().size(); ++i) {
    const int var = expr->vars(i);
    const int64_t coeff = expr->coeffs(i) / gcd;
    domain =
        domain.AdditionWith(context_->DomainOf(var).MultiplicationBy(coeff));
    definition.push_back({var, coeff});
  }
  int new_var;
  auto it = non_affine_expression_to_new_var_.find(definition);
  if (it != non_affine_expression_to_new_var_.end()) {
    new_var = it->second;
  } else {
    new_var = context_->NewIntVar(domain);
    auto* new_linear =
        context_->working_model->add_constraints()->mutable_linear();
    new_linear->add_vars(new_var);
    new_linear->add_coeffs(-1);
    for (const auto [var, coeff] : definition) {
      new_linear->add_vars(var);
      new_linear->add_coeffs(coeff);
    }
    new_linear->add_domain(0);
    new_linear->add_domain(0);
    context_->solution_crush().SetVarToLinearExpression(new_var, definition);
  }
  expr->clear_vars();
  expr->clear_coeffs();
  expr->add_vars(new_var);
  expr->add_coeffs(gcd);
}

void ModelCopy::MaybeExpandNonAffineExpressions(
    LinearArgumentProto* linear_argument) {
  MaybeExpandNonAffineExpression(linear_argument->mutable_target());
  for (LinearExpressionProto& expr : *linear_argument->mutable_exprs()) {
    MaybeExpandNonAffineExpression(&expr);
  }
}

bool ImportModelWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                             PresolveContext* context) {
  ModelCopy copier(context);
  copier.ImportVariablesAndMaybeIgnoreNames(in_model);
  if (copier.ImportAndSimplifyConstraints(in_model, /*first_copy=*/true)) {
    CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(in_model,
                                                                 context);
    return true;
  }
  return !context->ModelIsUnsat();
}

bool ImportModelAndDomainsWithBasicPresolveIntoContext(
    const CpModelProto& in_model, absl::Span<const Domain> domains,
    std::function<bool(int)> active_constraints, PresolveContext* context,
    std::vector<int>* interval_mapping) {
  CHECK_EQ(domains.size(), in_model.variables_size());
  ModelCopy copier(context);
  copier.CreateVariablesFromDomains(domains);
  if (copier.ImportAndSimplifyConstraints(in_model, /*first_copy=*/false,
                                          active_constraints)) {
    CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(in_model,
                                                                 context);
    interval_mapping->assign(copier.InternalIntervalMapping().begin(),
                             copier.InternalIntervalMapping().end());
    return true;
  }
  return !context->ModelIsUnsat();
}

void CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
    const CpModelProto& in_model, PresolveContext* context) {
  if (!in_model.name().empty()) {
    context->working_model->set_name(in_model.name());
  }
  if (in_model.has_objective()) {
    *context->working_model->mutable_objective() = in_model.objective();
  }
  if (in_model.has_floating_point_objective()) {
    *context->working_model->mutable_floating_point_objective() =
        in_model.floating_point_objective();
  }
  if (!in_model.search_strategy().empty()) {
    // We make sure we do not use the old variables field.
    *context->working_model->mutable_search_strategy() =
        in_model.search_strategy();
    for (DecisionStrategyProto& strategy :
         *context->working_model->mutable_search_strategy()) {
      google::protobuf::util::RemoveIf(strategy.mutable_exprs(),
                                       [](const LinearExpressionProto* expr) {
                                         return expr->vars().empty();
                                       });
      if (!strategy.variables().empty()) {
        CHECK(strategy.exprs().empty());
        for (const int ref : strategy.variables()) {
          LinearExpressionProto* expr = strategy.add_exprs();
          expr->add_vars(PositiveRef(ref));
          expr->add_coeffs(RefIsPositive(ref) ? 1 : -1);
        }
        strategy.clear_variables();
      }
    }
  }
  if (!in_model.assumptions().empty()) {
    *context->working_model->mutable_assumptions() = in_model.assumptions();
  }
  if (in_model.has_symmetry()) {
    *context->working_model->mutable_symmetry() = in_model.symmetry();
  }
  if (in_model.has_solution_hint()) {
    *context->working_model->mutable_solution_hint() = in_model.solution_hint();

    // We make sure the hint is within the variables domain.
    //
    // This allows to avoid overflow because we know evaluating constraints on
    // the variables domains should be safe thanks to the initial validation.
    const int num_terms = in_model.solution_hint().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = in_model.solution_hint().vars(i);
      const int64_t value = in_model.solution_hint().values(i);
      const Domain& domain = ReadDomainFromProto(in_model.variables(var));
      if (domain.IsEmpty()) continue;  // UNSAT.
      const int64_t closest_domain_value = domain.ClosestValue(value);
      if (closest_domain_value != value) {
        context->UpdateRuleStats("hint: moved var hint within its domain.");
        context->working_model->mutable_solution_hint()->set_values(
            i, closest_domain_value);
      }
    }
  }
}

}  // namespace sat
}  // namespace operations_research
