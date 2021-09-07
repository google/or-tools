// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/scheduling_constraints.h"

#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

class SelectedMinPropagator : public PropagatorInterface {
 public:
  explicit SelectedMinPropagator(Literal enforcement_literal,
                                 AffineExpression target,
                                 const std::vector<AffineExpression>& exprs,
                                 const std::vector<Literal>& selectors,
                                 Model* model)
      : enforcement_literal_(enforcement_literal),
        target_(target),
        exprs_(exprs),
        selectors_(selectors),
        trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        precedences_(model->GetOrCreate<PrecedencesPropagator>()),
        true_literal_(model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const Literal enforcement_literal_;
  const AffineExpression target_;
  const std::vector<AffineExpression> exprs_;
  const std::vector<Literal> selectors_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  PrecedencesPropagator* precedences_;
  const Literal true_literal_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(SelectedMinPropagator);
};

bool SelectedMinPropagator::Propagate() {
  const VariablesAssignment& assignment = trail_->Assignment();

  // helpers.
  const auto add_var_non_selection_to_reason = [&](int i) {
    DCHECK(assignment.LiteralIsFalse(selectors_[i]));
    literal_reason_.push_back(selectors_[i]);
  };
  const auto add_var_selection_to_reason = [&](int i) {
    DCHECK(assignment.LiteralIsTrue(selectors_[i]));
    literal_reason_.push_back(selectors_[i].Negated());
  };

  // Push the given integer literal if lit is true. Note that if lit is still
  // not assigned, we may still be able to deduce something.
  // TODO(user): Move this to integer_trail, and remove from here and
  // from scheduling helper.
  const auto push_bound = [&](Literal enforcement_lit, IntegerLiteral i_lit) {
    if (assignment.LiteralIsFalse(enforcement_lit)) return true;
    if (integer_trail_->OptionalLiteralIndex(i_lit.var) !=
        enforcement_lit.Index()) {
      if (assignment.LiteralIsTrue(enforcement_lit)) {
        // We can still push, but we do need the presence reason.
        literal_reason_.push_back(Literal(enforcement_lit).Negated());
      } else {
        // In this case we cannot push lit.var, but we may force the enforcement
        // literal to be false.
        if (i_lit.bound > integer_trail_->UpperBound(i_lit.var)) {
          integer_reason_.push_back(
              IntegerLiteral::LowerOrEqual(i_lit.var, i_lit.bound - 1));
          DCHECK(!assignment.LiteralIsFalse(enforcement_lit));
          integer_trail_->EnqueueLiteral(Literal(enforcement_lit).Negated(),
                                         literal_reason_, integer_reason_);
        }
        return true;
      }
    }

    if (!integer_trail_->Enqueue(i_lit, literal_reason_, integer_reason_)) {
      return false;
    }

    return true;
  };

  // Propagation.
  const int num_vars = exprs_.size();
  const IntegerValue target_min = integer_trail_->LowerBound(target_);
  const IntegerValue target_max = integer_trail_->UpperBound(target_);

  // Loop through the variables, and fills the quantities below.
  // In our naming scheme, a variable is either ignored, selected, or possible.
  IntegerValue min_of_mins(kMaxIntegerValue);
  IntegerValue min_of_selected_maxes(kMaxIntegerValue);
  IntegerValue max_of_possible_maxes(kMinIntegerValue);
  int num_possible_vars = 0;
  int num_selected_vars = 0;
  int min_of_selected_maxes_index = -1;
  int first_selected = -1;
  for (int i = 0; i < num_vars; ++i) {
    if (assignment.LiteralIsFalse(selectors_[i])) continue;

    const IntegerValue var_min = integer_trail_->LowerBound(exprs_[i]);
    const IntegerValue var_max = integer_trail_->UpperBound(exprs_[i]);

    min_of_mins = std::min(min_of_mins, var_min);

    if (assignment.LiteralIsTrue(selectors_[i])) {
      DCHECK(assignment.LiteralIsTrue(enforcement_literal_));
      num_selected_vars++;
      if (var_max < min_of_selected_maxes) {
        min_of_selected_maxes = var_max;
        min_of_selected_maxes_index = i;
      }
      if (first_selected == -1) {
        first_selected = i;
      }
    } else {
      DCHECK(!assignment.LiteralIsFalse(selectors_[i]));
      num_possible_vars++;
      max_of_possible_maxes = std::max(max_of_possible_maxes, var_max);
    }
  }

  if (min_of_mins > target_min) {
    literal_reason_.clear();
    integer_reason_.clear();
    for (int i = 0; i < num_vars; ++i) {
      if (assignment.LiteralIsFalse(selectors_[i])) {
        add_var_non_selection_to_reason(i);
      } else if (exprs_[i].var != kNoIntegerVariable) {
        integer_reason_.push_back(exprs_[i].GreaterOrEqual(min_of_mins));
      }
    }
    if (!push_bound(enforcement_literal_,
                    target_.GreaterOrEqual(min_of_mins))) {
      return false;
    }
  }

  if (num_selected_vars > 0 && min_of_selected_maxes < target_max) {
    DCHECK(assignment.LiteralIsTrue(enforcement_literal_));
    DCHECK_NE(min_of_selected_maxes_index, -1);
    DCHECK(assignment.LiteralIsTrue(selectors_[min_of_selected_maxes_index]));
    literal_reason_.clear();
    integer_reason_.clear();
    add_var_selection_to_reason(min_of_selected_maxes_index);
    if (exprs_[min_of_selected_maxes_index].var != kNoIntegerVariable) {
      integer_reason_.push_back(
          exprs_[min_of_selected_maxes_index].LowerOrEqual(
              min_of_selected_maxes));
    }
    if (!integer_trail_->Enqueue(target_.LowerOrEqual(min_of_selected_maxes),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  // Propagates in case every vars are still optional.
  if (num_possible_vars > 0 && num_selected_vars == 0) {
    if (target_max > max_of_possible_maxes) {
      literal_reason_.clear();
      integer_reason_.clear();

      for (int i = 0; i < num_vars; ++i) {
        if (assignment.LiteralIsFalse(selectors_[i])) {
          add_var_non_selection_to_reason(i);
        } else if (exprs_[i].var != kNoIntegerVariable) {
          integer_reason_.push_back(
              exprs_[i].LowerOrEqual(max_of_possible_maxes));
        }
      }
      if (!push_bound(enforcement_literal_,
                      target_.LowerOrEqual(max_of_possible_maxes))) {
        return false;
      }
    }
  }

  // All propagations and checks belows rely of the presence of the target.
  if (!assignment.LiteralIsTrue(enforcement_literal_)) return true;

  DCHECK_GE(integer_trail_->LowerBound(target_), min_of_mins);

  // Note that the case num_possible == 1, num_selected_vars == 0 shouldn't
  // happen because we assume that the enforcement <=> at_least_one_present
  // clause has already been propagated.
  if (num_possible_vars > 0) {
    DCHECK_GT(num_possible_vars + num_selected_vars, 1);
    return true;
  }
  if (num_selected_vars != 1) return true;

  DCHECK_NE(first_selected, -1);
  DCHECK(assignment.LiteralIsTrue(selectors_[first_selected]));
  const AffineExpression unique_selected_var = exprs_[first_selected];

  // Propagate bound from target to the unique selected var.
  if (target_min > integer_trail_->LowerBound(unique_selected_var)) {
    literal_reason_.clear();
    integer_reason_.clear();
    for (int i = 0; i < num_vars; ++i) {
      if (i != first_selected) {
        add_var_non_selection_to_reason(i);
      } else {
        add_var_selection_to_reason(i);
      }
    }
    if (target_.var != kNoIntegerVariable) {
      integer_reason_.push_back(target_.GreaterOrEqual(target_min));
    }
    if (!integer_trail_->Enqueue(unique_selected_var.GreaterOrEqual(target_min),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  if (target_max < integer_trail_->UpperBound(unique_selected_var)) {
    literal_reason_.clear();
    integer_reason_.clear();
    for (int i = 0; i < num_vars; ++i) {
      if (i != first_selected) {
        add_var_non_selection_to_reason(i);
      } else {
        add_var_selection_to_reason(i);
      }
    }
    if (target_.var != kNoIntegerVariable) {
      integer_reason_.push_back(target_.LowerOrEqual(target_max));
    }
    if (!integer_trail_->Enqueue(unique_selected_var.LowerOrEqual(target_max),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

int SelectedMinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (int t = 0; t < exprs_.size(); ++t) {
    watcher->WatchAffineExpression(exprs_[t], id);
    watcher->WatchLiteral(selectors_[t], id);
  }
  watcher->WatchAffineExpression(target_, id);
  watcher->WatchLiteral(enforcement_literal_, id);
  return id;
}

std::function<void(Model*)> EqualMinOfSelectedVariables(
    Literal enforcement_literal, AffineExpression target,
    const std::vector<AffineExpression>& exprs,
    const std::vector<Literal>& selectors) {
  CHECK_EQ(exprs.size(), selectors.size());
  return [=](Model* model) {
    // If both a variable is selected and the enforcement literal is true, then
    // the var is always greater than the target.
    for (int i = 0; i < exprs.size(); ++i) {
      LinearConstraintBuilder builder(model, kMinIntegerValue, IntegerValue(0));
      builder.AddTerm(target, IntegerValue(1));
      builder.AddTerm(exprs[i], IntegerValue(-1));
      LoadConditionalLinearConstraint({enforcement_literal, selectors[i]},
                                      builder.Build(), model);
    }

    // Add the dedicated propagator.
    SelectedMinPropagator* constraint = new SelectedMinPropagator(
        enforcement_literal, target, exprs, selectors, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

std::function<void(Model*)> EqualMaxOfSelectedVariables(
    Literal enforcement_literal, AffineExpression target,
    const std::vector<AffineExpression>& exprs,
    const std::vector<Literal>& selectors) {
  CHECK_EQ(exprs.size(), selectors.size());
  return [=](Model* model) {
    std::vector<AffineExpression> negations;
    for (const AffineExpression expr : exprs) {
      negations.push_back(expr.Negated());
    }
    model->Add(EqualMinOfSelectedVariables(
        enforcement_literal, target.Negated(), negations, selectors));
  };
}

std::function<void(Model*)> SpanOfIntervals(
    IntervalVariable span, const std::vector<IntervalVariable>& intervals) {
  return [=](Model* model) {
    auto* sat_solver = model->GetOrCreate<SatSolver>();
    auto* repository = model->GetOrCreate<IntervalsRepository>();

    // If the target is absent, then all tasks are absent.
    if (repository->IsAbsent(span)) {
      for (const IntervalVariable interval : intervals) {
        if (repository->IsOptional(interval)) {
          sat_solver->AddBinaryClause(
              repository->PresenceLiteral(span).Negated(),
              repository->PresenceLiteral(interval));
        } else if (repository->IsPresent(interval)) {
          sat_solver->NotifyThatModelIsUnsat();
          return;
        }
      }
      return;
    }

    // The target is present iif at least one interval is present. This is a
    // strict equivalence.
    std::vector<Literal> presence_literals;
    std::vector<AffineExpression> starts;
    std::vector<AffineExpression> ends;
    std::vector<Literal> clause;
    bool at_least_one_interval_is_present = false;
    const Literal true_literal =
        model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();

    for (const IntervalVariable interval : intervals) {
      if (repository->IsAbsent(interval)) continue;

      if (repository->IsOptional(interval)) {
        const Literal task_lit = repository->PresenceLiteral(interval);
        presence_literals.push_back(task_lit);
        clause.push_back(task_lit);

        if (repository->IsOptional(span)) {
          // task is present => target is present.
          sat_solver->AddBinaryClause(task_lit.Negated(),
                                      repository->PresenceLiteral(span));
        }

      } else {
        presence_literals.push_back(true_literal);
        at_least_one_interval_is_present = true;
      }
      starts.push_back(repository->Start(interval));
      ends.push_back(repository->End(interval));
    }

    if (!at_least_one_interval_is_present) {
      // enforcement_literal is true => one of the task is present.
      if (repository->IsOptional(span)) {
        clause.push_back(repository->PresenceLiteral(span).Negated());
      }
      sat_solver->AddProblemClause(clause);
    }

    // Link target start and end to the starts and ends of the tasks.
    const Literal enforcement_literal =
        repository->IsOptional(span)
            ? repository->PresenceLiteral(span)
            : model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
    model->Add(EqualMinOfSelectedVariables(enforcement_literal,
                                           repository->Start(span), starts,
                                           presence_literals));
    model->Add(EqualMaxOfSelectedVariables(
        enforcement_literal, repository->End(span), ends, presence_literals));
  };
}

}  // namespace sat
}  // namespace operations_research
