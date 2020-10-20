// Copyright 2010-2018 Google LLC
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
                                 IntegerVariable target,
                                 const std::vector<IntegerVariable> &vars,
                                 const std::vector<Literal> &selectors,
                                 Model *model)
      : enforcement_literal_(enforcement_literal), target_(target), vars_(vars),
        selectors_(selectors), trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        precedences_(model->GetOrCreate<PrecedencesPropagator>()),
        true_literal_(model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher *watcher);

 private:
  const Literal enforcement_literal_;
  const IntegerVariable target_;
  const std::vector<IntegerVariable> vars_;
  const std::vector<Literal> selectors_;
  Trail *trail_;
  IntegerTrail *integer_trail_;
  PrecedencesPropagator *precedences_;
  const Literal true_literal_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(SelectedMinPropagator);
};

bool SelectedMinPropagator::Propagate() {
  const VariablesAssignment &assignment = trail_->Assignment();

  // helpers.
  const auto add_var_non_selection_to_reason = [&](int i) {
    DCHECK(assignment.LiteralIsFalse(selectors_[i]));
    literal_reason_.push_back(selectors_[i]);
  }
  ;
  const auto add_var_selection_to_reason = [&](int i) {
    DCHECK(assignment.LiteralIsTrue(selectors_[i]));
    literal_reason_.push_back(selectors_[i].Negated());
  }
  ;

  // Push the given integer literal if lit is true. Note that if lit is still
  // not assigned, we may still be able to deduce something.
  // TODO(user,user): Move this to integer_trail, and remove from here and
  // from scheduling helper.
  const auto push_bound = [&](Literal enforcement_lit, IntegerLiteral i_lit) {
    if (assignment.LiteralIsFalse(enforcement_lit))
      return true;
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
  }
  ;

  // Propagation.
  const int num_vars = vars_.size();
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
    if (assignment.LiteralIsFalse(selectors_[i]))
      continue;

    const IntegerVariable var = vars_[i];
    const IntegerValue var_min = integer_trail_->LowerBound(var);
    const IntegerValue var_max = integer_trail_->UpperBound(var);

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
      } else {
        integer_reason_.push_back(
            IntegerLiteral::GreaterOrEqual(vars_[i], min_of_mins));
      }
    }
    if (!push_bound(enforcement_literal_,
                    IntegerLiteral::GreaterOrEqual(target_, min_of_mins))) {
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
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(
        vars_[min_of_selected_maxes_index], min_of_selected_maxes));
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(target_, min_of_selected_maxes),
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
        } else {
          integer_reason_.push_back(
              IntegerLiteral::LowerOrEqual(vars_[i], max_of_possible_maxes));
        }
      }
      if (!push_bound(
              enforcement_literal_,
              IntegerLiteral::LowerOrEqual(target_, max_of_possible_maxes))) {
        return false;
      }
    }
  }

  // All propagations and checks belows rely of the presence of the target.
  if (!assignment.LiteralIsTrue(enforcement_literal_))
    return true;

  DCHECK_GE(integer_trail_->LowerBound(target_), min_of_mins);

  // Note that the case num_possible == 1, num_selected_vars == 0 shouldn't
  // happen because we assume that the enforcement <=> at_least_one_present
  // clause has already been propagated.
  if (num_possible_vars > 0) {
    DCHECK_GT(num_possible_vars + num_selected_vars, 1);
    return true;
  }
  if (num_selected_vars != 1)
    return true;

  DCHECK_NE(first_selected, -1);
  DCHECK(assignment.LiteralIsTrue(selectors_[first_selected]));
  const IntegerVariable unique_selected_var = vars_[first_selected];

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
    integer_reason_.push_back(
        IntegerLiteral::GreaterOrEqual(target_, target_min));
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(unique_selected_var, target_min),
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
    integer_reason_.push_back(
        IntegerLiteral::LowerOrEqual(target_, target_max));
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(unique_selected_var, target_max),
            literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

int SelectedMinPropagator::RegisterWith(GenericLiteralWatcher *watcher) {
  const int id = watcher->Register(this);
  for (int t = 0; t < vars_.size(); ++t) {
    watcher->WatchLowerBound(vars_[t], id);
    watcher->WatchUpperBound(vars_[t], id);
    watcher->WatchLiteral(selectors_[t], id);
  }
  watcher->WatchLowerBound(target_, id);
  watcher->WatchUpperBound(target_, id);
  watcher->WatchLiteral(enforcement_literal_, id);
  return id;
}

std::function<void(Model *)>
EqualMinOfSelectedVariables(Literal enforcement_literal, IntegerVariable target,
                            const std::vector<IntegerVariable> &vars,
                            const std::vector<Literal> &selectors) {
  CHECK_EQ(vars.size(), selectors.size());
  return [=](Model* model) {
    // If both a variable is selected and the enforcement literal is true, then
    // the var is always greater than the target.
    for (int i = 0;
  i < vars.size(); ++i) {
      std::vector<Literal> conditions = { enforcement_literal };
      conditions.push_back(selectors[i]);
      model->Add(ConditionalLowerOrEqual(target, vars[i], conditions));
    }

    // Add the dedicated propagator.
    SelectedMinPropagator *constraint = new SelectedMinPropagator(
        enforcement_literal, target, vars, selectors, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  }
  ;
}

std::function<void(Model *)>
EqualMaxOfSelectedVariables(Literal enforcement_literal, IntegerVariable target,
                            const std::vector<IntegerVariable> &vars,
                            const std::vector<Literal> &selectors) {
  CHECK_EQ(vars.size(), selectors.size());
  return[ = ](Model * model) { std::vector<IntegerVariable> negations;
    for (const IntegerVariable var : vars) {
      negations.push_back(NegationOf(var));
    }
    model->Add(EqualMinOfSelectedVariables(
        enforcement_literal, NegationOf(target), negations, selectors));
  }
  ;
}

std::function<void(Model *)>
SpanOfIntervals(IntervalVariable span,
                const std::vector<IntervalVariable> &intervals) {
  return[ = ](Model *model) { SatSolver *sat_solver =
                                  model->GetOrCreate<SatSolver>();
    SchedulingConstraintHelper task_helper(intervals, model);
    SchedulingConstraintHelper target_helper({
      span
  },
                                             model);

    // If the target is absent, then all tasks are absent.
    if (target_helper.IsAbsent(0)) {
      for (int t = 0; t < task_helper.NumTasks(); ++t) {
        if (task_helper.IsOptional(t)) {
          sat_solver->AddBinaryClause(
              target_helper.PresenceLiteral(0).Negated(),
              task_helper.PresenceLiteral(t));
        } else if (task_helper.IsPresent(t)) {
          sat_solver->NotifyThatModelIsUnsat();
          return;
        }
      }
      return;
    }

    // The target is present iif at least one interval is present. This is a
    // strict equivalence.
    std::vector<Literal> presence_literals;
    std::vector<IntegerVariable> starts;
    std::vector<IntegerVariable> ends;
    std::vector<Literal> clause;
    bool at_least_one_interval_is_present = false;
    const Literal true_literal =
        model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();

    for (int t = 0; t < task_helper.NumTasks(); ++t) {
      if (task_helper.IsAbsent(t))
        continue;

      if (task_helper.IsOptional(t)) {
        const Literal task_lit = task_helper.PresenceLiteral(t);
        presence_literals.push_back(task_lit);
        clause.push_back(task_lit);

        if (target_helper.IsOptional(0)) {
          // task is present => target is present.
          sat_solver->AddBinaryClause(task_lit.Negated(),
                                      target_helper.PresenceLiteral(0));
        }

      } else {
        presence_literals.push_back(true_literal);
        at_least_one_interval_is_present = true;
      }
      starts.push_back(task_helper.StartVars()[t]);
      ends.push_back(task_helper.EndVars()[t]);
    }

    if (!at_least_one_interval_is_present) {
      // enforcement_literal is true => one of the task is present.
      if (target_helper.IsOptional(0)) {
        clause.push_back(target_helper.PresenceLiteral(0).Negated());
      }
      sat_solver->AddProblemClause(clause);
    }

    // Link target start and end to the starts and ends of the tasks.
    const Literal enforcement_literal =
        target_helper.IsOptional(0)
            ? target_helper.PresenceLiteral(0)
            : model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
    model->Add(EqualMinOfSelectedVariables(
        enforcement_literal, target_helper.StartVars().front(), starts,
        presence_literals));
    model->Add(EqualMaxOfSelectedVariables(
        enforcement_literal, target_helper.EndVars().front(), ends,
        presence_literals));
  }
  ;
}

} // namespace sat
} // namespace operations_research
