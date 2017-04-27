// Copyright 2010-2014 Google
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

//
// Frequency Assignment Problem
// The Radio Link Frequency Assignment Problem consists in assigning frequencies
// to a set of radio links defined between pairs of sites in order to avoid
// interferences. Each radio link is represented by a variable whose domain is
// the set of all frequencies that are available for this link.
// The essential constraint involving two variables of the problem F1 and F2,
// which represent two frequencies in the spectrum, is
// |F1 - F2| > k12, where k12 is a predefined constant value.
// The Frequency Assignment Problem is an NP-complete problem as proved by means
// of reduction from k-Colorability problem for undirected graphs.
// The solution of the problem can be based on various criteria:
// - Simple satisfaction
// - Minimizing the number of distinct frequencies used
// - Minimizing the maximum frequency used, i.e minimizing the total width of
// the spectrum
// - Minimizing a weighted sum of violated constraints if the problem is
//   inconsistent
// More on the Frequency Assignment Problem and the data format of its instances
// can be found at: http://www.inra.fr/mia/T/schiex/Doc/CELAR.shtml#synt
//
// Implementation
// Two solvers are implemented: The HardFapSolver finds the solution to
// feasible instances of the problem with objective either the minimization of
// the largest frequency assigned or the minimization of the number of
// frequencies used to the solution.
// The SoftFapSolver is optimizes the unfeasible instances. Some of the
// constraints of these instances may actually be soft constraints which may be
// violated at some predefined constant cost. The SoftFapSolver aims to minimize
// the total cost of violated constraints, i.e. to minimize the sum of all the
// violation costs.
// If the latter solver is forced to solve a feasible instance, the main
// function redirects to the former, afterwards.
//

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "examples/cpp/fap_model_printer.h"
#include "examples/cpp/fap_parser.h"
#include "examples/cpp/fap_utilities.h"

DEFINE_string(directory, "", "Specifies the directory of the data.");
DEFINE_string(value_evaluator, "",
              "Specifies if a value evaluator will be used by the "
              "decision builder.");
DEFINE_string(variable_evaluator, "",
              "Specifies if a variable evaluator will be used by the "
              "decision builder.");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, <= 0 means no limit.");
DEFINE_int32(choose_next_variable_strategy, 1,
             "Selection strategy for variable: "
             "1 = CHOOSE_FIRST_UNBOUND, "
             "2 = CHOOSE_MIN_SIZE_LOWEST_MIN, "
             "3 = CHOOSE_MIN_SIZE_HIGHEST_MAX, "
             "4 = CHOOSE_RANDOM, ");
DEFINE_int32(restart, -1, "Parameter for constant restart monitor.");
DEFINE_bool(find_components, false,
            "If possible, split the problem into independent sub-problems.");
DEFINE_bool(luby, false,
            "Use luby restart monitor instead of constant restart monitor.");
DEFINE_bool(log_search, true, "Create a search log.");
DEFINE_bool(soft, false, "Use soft solver instead of hard solver.");
DEFINE_bool(display_time, true,
            "Print how much time the solving process took.");
DEFINE_bool(display_results, true, "Print the results of the solving process.");

namespace operations_research {

// Decision on the relative order that the two variables of a constraint
// will have. It takes as parameters the components of the constraint.
class OrderingDecision : public Decision {
 public:
  OrderingDecision(IntVar* const variable1, IntVar* const variable2, int value,
                   std::string operation)
      : variable1_(variable1),
        variable2_(variable2),
        value_(value),
        operator_(std::move(operation)) {}
  ~OrderingDecision() override {}

  // Apply will be called first when the decision is executed.
  void Apply(Solver* const s) override {
    // variable1 < variable2
    MakeDecision(s, variable1_, variable2_);
  }

  // Refute will be called after a backtrack.
  void Refute(Solver* const s) override {
    // variable1 > variable2
    MakeDecision(s, variable2_, variable1_);
  }

 private:
  void MakeDecision(Solver* s, IntVar* variable1, IntVar* variable2) {
    if (operator_ == ">") {
      IntExpr* difference = (s->MakeDifference(variable2, variable1));
      s->AddConstraint(s->MakeGreater(difference, value_));
    } else if (operator_ == "=") {
      IntExpr* difference = (s->MakeDifference(variable2, variable1));
      s->AddConstraint(s->MakeEquality(difference, value_));
    } else {
      LOG(FATAL) << "No right operator specified.";
    }
  }

  IntVar* const variable1_;
  IntVar* const variable2_;
  const int value_;
  const std::string operator_;

  DISALLOW_COPY_AND_ASSIGN(OrderingDecision);
};

// Decision on whether a soft constraint will be added to a model
// or if it will be violated.
class ConstraintDecision : public Decision {
 public:
  explicit ConstraintDecision(IntVar* const constraint_violation)
      : constraint_violation_(constraint_violation) {}

  ~ConstraintDecision() override {}

  // Apply will be called first when the decision is executed.
  void Apply(Solver* const s) override {
    // The constraint with which the builder is dealing, will be satisfied.
    constraint_violation_->SetValue(0);
  }

  // Refute will be called after a backtrack.
  void Refute(Solver* const s) override {
    // The constraint with which the builder is dealing, will not be satisfied.
    constraint_violation_->SetValue(1);
  }

 private:
  IntVar* const constraint_violation_;

  DISALLOW_COPY_AND_ASSIGN(ConstraintDecision);
};

// The ordering builder resolves the relative order of the two variables
// included in each of the constraints of the problem. In that way the
// solving becomes much more efficient since we are branching on the
// disjunction implied by the absolute value expression.
class OrderingBuilder : public DecisionBuilder {
 public:
  enum Order { LESS = -1, EQUAL = 0, GREATER = 1 };

  OrderingBuilder(const std::map<int, FapVariable>& data_variables,
                  const std::vector<FapConstraint>& data_constraints,
                  const std::vector<IntVar*>& variables,
                  const std::vector<IntVar*>& violated_constraints,
                  const std::map<int, int>& index_from_key)
      : data_variables_(data_variables),
        data_constraints_(data_constraints),
        variables_(variables),
        violated_constraints_(violated_constraints),
        index_from_key_(index_from_key),
        size_(data_constraints.size()),
        iter_(0),
        checked_iter_(0) {
    for (const auto& it : data_variables_) {
      int first_element = (it.second.domain)[0];
      minimum_value_available_.push_back(first_element);
      variable_state_.push_back(EQUAL);
    }
    CHECK_EQ(minimum_value_available_.size(), variables_.size());
    CHECK_EQ(variable_state_.size(), variables_.size());
  }

  ~OrderingBuilder() override {}

  Decision* Next(Solver* const s) override {
    if (iter_ < size_) {
      FapConstraint constraint = data_constraints_[iter_];
      const int index1 = FindOrDie(index_from_key_, constraint.variable1);
      const int index2 = FindOrDie(index_from_key_, constraint.variable2);
      IntVar* variable1 = variables_[index1];
      IntVar* variable2 = variables_[index2];

      // checked_iter is equal to 0 means that whether the constraint is to be
      // added or dropped hasn't been checked.
      // If it is equal to 1, this has already been checked and the ordering
      // of the constraint is to be done.
      if (!checked_iter_ && !constraint.hard) {
        // New Soft Constraint: Check if it will be added or dropped.
        ConstraintDecision* constraint_decision =
            new ConstraintDecision(violated_constraints_[iter_]);

        s->SaveAndAdd(&checked_iter_, 1);
        return s->RevAlloc(constraint_decision);
      }

      // The constraint is either hard or soft and checked already.
      if (violated_constraints_[iter_]->Bound() &&
          violated_constraints_[iter_]->Value() == 0) {
        // If the constraint is added, do the ordering of its variables.
        OrderingDecision* ordering_decision;
        Order hint = Hint(constraint);
        if (hint == LESS || hint == EQUAL) {
          ordering_decision = new OrderingDecision(
              variable1, variable2, constraint.value, constraint.operation);
        } else {
          ordering_decision = new OrderingDecision(
              variable2, variable1, constraint.value, constraint.operation);
        }
        // Proceed to the next constraint.
        s->SaveAndAdd(&iter_, 1);
        // Assign checked_iter_ back to 0 to flag a new unchecked constraint.
        s->SaveAndSetValue(&checked_iter_, 0);
        return s->RevAlloc(ordering_decision);
      } else {
        // The constraint was dropped.
        return nullptr;
      }
    } else {
      // All the constraints were processed. No decision to take.
      return nullptr;
    }
  }

 private:
  Order Variable1LessVariable2(const int variable1, const int variable2,
                               const int value) {
    minimum_value_available_[variable2] =
        std::max(minimum_value_available_[variable2],
                 minimum_value_available_[variable1] + value);
    return LESS;
  }

  Order Variable1GreaterVariable2(const int variable1, const int variable2,
                                  const int value) {
    minimum_value_available_[variable1] =
        std::max(minimum_value_available_[variable1],
                 minimum_value_available_[variable2] + value);
    return GREATER;
  }
  // The Hint() function takes as parameter a constraint of the model and
  // returns the most probable relative order that the two variables
  // involved in the constraint should have.
  // The function reaches such a decision, by taking into consideration if
  // variable1 or variable2 or both have been denoted as less (state = -1)
  // or greater (state = 1) than another variable in a previous constraint
  // and tries to maintain the same state in the current constraint too.
  // If both variables have the same state, the variable whose minimum value is
  // the smallest is set to be lower than the other one.
  // If none of the above are applicable variable1 is set to be lower than
  // variable2. This ordering is more efficient if used with the
  // Solver::ASSIGN_MIN_VALUE value selection strategy.
  // It returns 1 if variable1 > variable2 or -1 if variable1 < variable2.
  Order Hint(const FapConstraint& constraint) {
    const int id1 = constraint.variable1;
    const int id2 = constraint.variable2;
    const int variable1 = FindOrDie(index_from_key_, id1);
    const int variable2 = FindOrDie(index_from_key_, id2);
    const int value = constraint.value;
    CHECK_LT(variable1, variable_state_.size());
    CHECK_LT(variable2, variable_state_.size());
    CHECK_LT(variable1, minimum_value_available_.size());
    CHECK_LT(variable2, minimum_value_available_.size());

    if (variable_state_[variable1] > variable_state_[variable2]) {
      variable_state_[variable1] = GREATER;
      variable_state_[variable2] = LESS;
      return Variable1GreaterVariable2(variable1, variable2, value);
    } else if (variable_state_[variable1] < variable_state_[variable2]) {
      variable_state_[variable1] = LESS;
      variable_state_[variable2] = GREATER;
      return Variable1LessVariable2(variable1, variable2, value);
    } else {
      if (variable_state_[variable1] == 0 && variable_state_[variable2] == 0) {
        variable_state_[variable1] = LESS;
        variable_state_[variable2] = GREATER;
        return Variable1LessVariable2(variable1, variable2, value);
      } else {
        if (minimum_value_available_[variable1] >
            minimum_value_available_[variable2]) {
          return Variable1GreaterVariable2(variable1, variable2, value);
        } else {
          return Variable1LessVariable2(variable1, variable2, value);
        }
      }
    }
  }

  // Passed as arguments from the function that creates the Decision Builder.
  const std::map<int, FapVariable> data_variables_;
  const std::vector<FapConstraint> data_constraints_;
  const std::vector<IntVar*> variables_;
  const std::vector<IntVar*> violated_constraints_;
  const std::map<int, int> index_from_key_;
  // Used by Next() for monitoring decisions.
  const int size_;
  int iter_;
  int checked_iter_;
  // Used by Hint() for indicating the most probable ordering.
  std::vector<Order> variable_state_;
  std::vector<int> minimum_value_available_;

  DISALLOW_COPY_AND_ASSIGN(OrderingBuilder);
};

// A comparator for sorting the constraints depending on their impact.
bool ConstraintImpactComparator(FapConstraint constraint1,
                                FapConstraint constraint2) {
  if (constraint1.impact == constraint2.impact) {
    return (constraint1.value > constraint2.value);
  }
  return (constraint1.impact > constraint2.impact);
}

int64 ValueEvaluator(
    std::unordered_map<int64, std::pair<int64, int64>>* value_evaluator_map,
    int64 variable_index, int64 value) {
  CHECK_NOTNULL(value_evaluator_map);
  // Evaluate the choice. Smaller ranking denotes a better choice.
  int64 ranking = -1;
  for (const auto& it : *value_evaluator_map) {
    if ((it.first != variable_index) && (it.second.first == value)) {
      ranking = -2;
      break;
    }
  }

  // Update the history of assigned values and their rankings of each variable.
  std::unordered_map<int64, std::pair<int64, int64>>::iterator it;
  int64 new_value = value;
  int64 new_ranking = ranking;
  if ((it = value_evaluator_map->find(variable_index)) !=
      value_evaluator_map->end()) {
    std::pair<int64, int64> existing_value_ranking = it->second;
    // Replace only if the current choice for this variable has smaller
    // ranking or same ranking but smaller value of the existing choice.
    if (!(existing_value_ranking.second > ranking ||
          (existing_value_ranking.second == ranking &&
           existing_value_ranking.first > value))) {
      new_value = existing_value_ranking.first;
      new_ranking = existing_value_ranking.second;
    }
  }
  std::pair<int64, int64> new_value_ranking =
      std::make_pair(new_value, new_ranking);
  InsertOrUpdate(value_evaluator_map, variable_index, new_value_ranking);

  return new_ranking;
}

// The variables which participate in more constraints and have the
// smaller domain should be in higher priority for assignment.
int64 VariableEvaluator(const std::vector<int>& key_from_index,
                        const std::map<int, FapVariable>& data_variables,
                        int64 variable_index) {
  FapVariable variable =
      FindOrDie(data_variables, key_from_index[variable_index]);
  int64 result = -(variable.degree * 100 / variable.domain_size);
  return result;
}

// Creates the variables of the solver from the parsed data.
void CreateModelVariables(const std::map<int, FapVariable>& data_variables,
                          Solver* solver, std::vector<IntVar*>* model_variables,
                          std::map<int, int>* index_from_key,
                          std::vector<int>* key_from_index) {
  CHECK_NOTNULL(solver);
  CHECK_NOTNULL(model_variables);
  CHECK_NOTNULL(index_from_key);
  CHECK_NOTNULL(key_from_index);

  const int number_of_variables = static_cast<int>(data_variables.size());
  model_variables->resize(number_of_variables);
  key_from_index->resize(number_of_variables);

  int index = 0;
  for (const auto& it : data_variables) {
    CHECK_LT(index, model_variables->size());
    (*model_variables)[index] = solver->MakeIntVar(it.second.domain);
    InsertOrUpdate(index_from_key, it.first, index);
    (*key_from_index)[index] = it.first;

    if ((it.second.initial_position != -1) && (it.second.hard)) {
      CHECK_LT(it.second.mobility_cost, 0);
      solver->AddConstraint(solver->MakeEquality((*model_variables)[index],
                                                 it.second.initial_position));
    }
    index++;
  }
}

// Creates the constraints of the instance from the parsed data.
void CreateModelConstraints(const std::vector<FapConstraint>& data_constraints,
                            const std::vector<IntVar*>& variables,
                            const std::map<int, int>& index_from_key,
                            Solver* solver) {
  CHECK_NOTNULL(solver);

  for (const FapConstraint& ct : data_constraints) {
    const int index1 = FindOrDie(index_from_key, ct.variable1);
    const int index2 = FindOrDie(index_from_key, ct.variable2);
    CHECK_LT(index1, variables.size());
    CHECK_LT(index2, variables.size());
    IntVar* var1 = variables[index1];
    IntVar* var2 = variables[index2];
    IntVar* absolute_difference =
        solver->MakeAbs(solver->MakeDifference(var1, var2))->Var();
    if (ct.operation == ">") {
      solver->AddConstraint(solver->MakeGreater(absolute_difference, ct.value));
    } else if (ct.operation == "=") {
      solver->AddConstraint(
          solver->MakeEquality(absolute_difference, ct.value));
    } else {
      LOG(FATAL) << "Invalid operator detected.";
      return;
    }
  }
}

// According to the value of a command line flag, chooses the strategy which
// determines the selection of the variable to be assigned next.
void ChooseVariableStrategy(Solver::IntVarStrategy* variable_strategy) {
  CHECK_NOTNULL(variable_strategy);

  switch (FLAGS_choose_next_variable_strategy) {
    case 1: {
      *variable_strategy = Solver::CHOOSE_FIRST_UNBOUND;
      LOG(INFO) << "Using Solver::CHOOSE_FIRST_UNBOUND "
                   "for variable selection strategy.";
      break;
    }
    case 2: {
      *variable_strategy = Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
      LOG(INFO) << "Using Solver::CHOOSE_MIN_SIZE_LOWEST_MIN "
                   "for variable selection strategy.";
      break;
    }
    case 3: {
      *variable_strategy = Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX;
      LOG(INFO) << "Using Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX "
                   "for variable selection strategy.";
      break;
    }
    case 4: {
      *variable_strategy = Solver::CHOOSE_RANDOM;
      LOG(INFO) << "Using Solver::CHOOSE_RANDOM "
                   "for variable selection strategy.";
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here";
      return;
    }
  }
}

// According to the values of some command line flags, adds some monitors
// for the search of the Solver.
void CreateAdditionalMonitors(OptimizeVar* const objective, Solver* solver,
                              std::vector<SearchMonitor*>* monitors) {
  CHECK_NOTNULL(solver);
  CHECK_NOTNULL(monitors);

  // Search Log
  if (FLAGS_log_search) {
    SearchMonitor* const log = solver->MakeSearchLog(100000, objective);
    monitors->push_back(log);
  }

  // Time Limit
  if (FLAGS_time_limit_in_ms != 0) {
    LOG(INFO) << "Adding time limit of " << FLAGS_time_limit_in_ms << " ms.";
    SearchLimit* const limit = solver->MakeLimit(
        FLAGS_time_limit_in_ms, kint64max, kint64max, kint64max);
    monitors->push_back(limit);
  }

  // Search Restart
  SearchMonitor* const restart =
      FLAGS_restart != -1
          ? (FLAGS_luby ? solver->MakeLubyRestart(FLAGS_restart)
                        : solver->MakeConstantRestart(FLAGS_restart))
          : nullptr;
  if (restart) {
    monitors->push_back(restart);
  }
}

// The Hard Solver is dealing with finding the solution to feasible
// instances of the problem with objective either the minimization of
// the largest frequency assigned or the minimization of the number
// of frequencies used to the solution.
void HardFapSolver(const std::map<int, FapVariable>& data_variables,
                   const std::vector<FapConstraint>& data_constraints,
                   const std::string& data_objective,
                   const std::vector<int>& values) {
  Solver solver("HardFapSolver");
  std::vector<SearchMonitor*> monitors;

  // Create Model Variables.
  std::vector<IntVar*> variables;
  std::map<int, int> index_from_key;
  std::vector<int> key_from_index;
  CreateModelVariables(data_variables, &solver, &variables, &index_from_key,
                       &key_from_index);

  // Create Model Constraints.
  CreateModelConstraints(data_constraints, variables, index_from_key, &solver);

  // Order the constraints according to their impact in the instance.
  std::vector<FapConstraint> ordered_constraints(data_constraints);
  std::sort(ordered_constraints.begin(), ordered_constraints.end(),
            ConstraintImpactComparator);

  std::vector<IntVar*> violated_constraints;
  solver.MakeIntVarArray(ordered_constraints.size(), 0, 0,
                         &violated_constraints);

  // Objective:
  // Either minimize the largest assigned frequency or
  // minimize the number of different frequencies assigned.
  IntVar* objective_var;
  OptimizeVar* objective;
  if (data_objective == "Minimize the largest assigned value.") {
    LOG(INFO) << "Minimize the largest assigned value.";
    // The objective_var is set to hold the maximum value assigned
    // in the variables vector.
    objective_var = solver.MakeMax(variables)->Var();
    objective = solver.MakeMinimize(objective_var, 1);
  } else if (data_objective == "Minimize the number of assigned values.") {
    LOG(INFO) << "Minimize the number of assigned values.";

    std::vector<IntVar*> cardinality;
    solver.MakeIntVarArray(static_cast<int>(values.size()), 0,
                           static_cast<int>(variables.size()), &cardinality);
    solver.AddConstraint(solver.MakeDistribute(variables, values, cardinality));
    std::vector<IntVar*> value_not_assigned;
    for (int val = 0; val < values.size(); ++val) {
      value_not_assigned.push_back(
          solver.MakeIsEqualCstVar(cardinality[val], 0));
    }
    CHECK(!value_not_assigned.empty());
    // The objective_var is set to maximize the number of values
    // that have not been assigned to a variable.
    objective_var = solver.MakeSum(value_not_assigned)->Var();
    objective = solver.MakeMaximize(objective_var, 1);
  } else {
    LOG(FATAL) << "No right objective specified.";
    return;
  }
  monitors.push_back(objective);

  // Ordering Builder
  OrderingBuilder* ob = solver.RevAlloc(
      new OrderingBuilder(data_variables, ordered_constraints, variables,
                          violated_constraints, index_from_key));

  // Decision Builder Configuration
  // Choose the next variable selection strategy.
  Solver::IntVarStrategy variable_strategy;
  ChooseVariableStrategy(&variable_strategy);
  // Choose the value selection strategy.
  DecisionBuilder* db;
  std::unordered_map<int64, std::pair<int64, int64>> history;
  if (FLAGS_value_evaluator == "value_evaluator") {
    LOG(INFO) << "Using ValueEvaluator for value selection strategy.";
    Solver::IndexEvaluator2 index_evaluator2 = [&history](int64 var,
                                                          int64 value) {
      return ValueEvaluator(&history, var, value);
    };
    LOG(INFO) << "Using ValueEvaluator for value selection strategy.";
    db = solver.MakePhase(variables, variable_strategy, index_evaluator2);
  } else {
    LOG(INFO) << "Using Solver::ASSIGN_MIN_VALUE for value selection strategy.";
    db = solver.MakePhase(variables, variable_strategy,
                          Solver::ASSIGN_MIN_VALUE);
  }

  DecisionBuilder* final_db = solver.Compose(ob, db);

  // Create Additional Monitors.
  CreateAdditionalMonitors(objective, &solver, &monitors);

  // Collector
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(variables);
  collector->Add(objective_var);
  monitors.push_back(collector);

  // Solve.
  LOG(INFO) << "Solving...";
  const int64 time1 = solver.wall_time();
  solver.Solve(final_db, monitors);
  const int64 time2 = solver.wall_time();

  // Display Time.
  if (FLAGS_display_time) {
    PrintElapsedTime(time1, time2);
  }
  // Display Results.
  if (FLAGS_display_results) {
    PrintResultsHard(collector, variables, objective_var, data_variables,
                     data_constraints, index_from_key, key_from_index);
  }
}

// Splits variables of the instance to hard and soft.
void SplitVariablesHardSoft(const std::map<int, FapVariable>& data_variables,
                            std::map<int, FapVariable>* hard_variables,
                            std::map<int, FapVariable>* soft_variables) {
  for (const auto& it : data_variables) {
    if (it.second.initial_position != -1) {
      if (it.second.hard) {
        CHECK_LT(it.second.mobility_cost, 0);
        InsertOrUpdate(hard_variables, it.first, it.second);
      } else {
        CHECK_GE(it.second.mobility_cost, 0);
        InsertOrUpdate(soft_variables, it.first, it.second);
      }
    }
  }
}

// Splits constraints of the instance to hard and soft.
void SplitConstraintHardSoft(const std::vector<FapConstraint>& data_constraints,
                             std::vector<FapConstraint>* hard_constraints,
                             std::vector<FapConstraint>* soft_constraints) {
  for (const FapConstraint& ct : data_constraints) {
    if (ct.hard) {
      CHECK_LT(ct.weight_cost, 0);
      hard_constraints->push_back(ct);
    } else {
      CHECK_GE(ct.weight_cost, 0);
      soft_constraints->push_back(ct);
    }
  }
}

// Penalize the modification of the initial position of soft variable of
// the instance.
void PenalizeVariablesViolation(
    const std::map<int, FapVariable>& soft_variables,
    const std::map<int, int>& index_from_key,
    const std::vector<IntVar*>& variables, std::vector<IntVar*>* cost,
    Solver* solver) {
  for (const auto& it : soft_variables) {
    const int index = FindOrDie(index_from_key, it.first);
    CHECK_LT(index, variables.size());
    IntVar* const displaced = solver->MakeIsDifferentCstVar(
        variables[index], it.second.initial_position);
    IntVar* const weight =
        solver->MakeProd(displaced, it.second.mobility_cost)->Var();
    cost->push_back(weight);
  }
}

// Penalize the violation of soft constraints of the instance.
void PenalizeConstraintsViolation(
    const std::vector<FapConstraint>& constraints,
    const std::vector<FapConstraint>& soft_constraints,
    const std::map<int, int>& index_from_key,
    const std::vector<IntVar*>& variables, std::vector<IntVar*>* cost,
    std::vector<IntVar*>* violated_constraints, Solver* solver) {
  int violated_constraints_index = 0;
  for (const FapConstraint& ct : constraints) {
    CHECK_LT(violated_constraints_index, violated_constraints->size());
    if (!ct.hard) {
      // The violated_constraints_index will stop at the first soft constraint.
      break;
    }
    IntVar* const hard_violation = solver->MakeIntVar(0, 0);
    (*violated_constraints)[violated_constraints_index] = hard_violation;
    violated_constraints_index++;
  }

  for (const FapConstraint& ct : soft_constraints) {
    const int index1 = FindOrDie(index_from_key, ct.variable1);
    const int index2 = FindOrDie(index_from_key, ct.variable2);
    CHECK_LT(index1, variables.size());
    CHECK_LT(index2, variables.size());
    IntVar* const absolute_difference =
        solver
            ->MakeAbs(
                solver->MakeDifference(variables[index1], variables[index2]))
            ->Var();
    IntVar* violation = nullptr;
    if (ct.operation == ">") {
      violation = solver->MakeIsLessCstVar(absolute_difference, ct.value);
    } else if (ct.operation == "=") {
      violation = solver->MakeIsDifferentCstVar(absolute_difference, ct.value);
    } else {
      LOG(FATAL) << "Invalid operator detected.";
    }
    IntVar* const weight = solver->MakeProd(violation, ct.weight_cost)->Var();
    cost->push_back(weight);
    CHECK_LT(violated_constraints_index, violated_constraints->size());
    (*violated_constraints)[violated_constraints_index] = violation;
    violated_constraints_index++;
  }
  CHECK_EQ(violated_constraints->size(), constraints.size());
}

// The Soft Solver is dealing with the optimization of unfeasible instances
// and aims to minimize the total cost of violated constraints. Returning value
// equal to 0 denotes that the instance is feasible.
int SoftFapSolver(const std::map<int, FapVariable>& data_variables,
                  const std::vector<FapConstraint>& data_constraints,
                  const std::string& data_objective,
                  const std::vector<int>& values) {
  Solver solver("SoftFapSolver");
  std::vector<SearchMonitor*> monitors;

  // Split variables to hard and soft.
  std::map<int, FapVariable> hard_variables;
  std::map<int, FapVariable> soft_variables;
  SplitVariablesHardSoft(data_variables, &hard_variables, &soft_variables);

  // Order instance's constraints by their impact and then split them to
  // hard and soft.
  std::vector<FapConstraint> ordered_constraints(data_constraints);
  std::sort(ordered_constraints.begin(), ordered_constraints.end(),
            ConstraintImpactComparator);
  std::vector<FapConstraint> hard_constraints;
  std::vector<FapConstraint> soft_constraints;
  SplitConstraintHardSoft(ordered_constraints, &hard_constraints,
                          &soft_constraints);

  // Create Model Variables.
  std::vector<IntVar*> variables;
  std::map<int, int> index_from_key;
  std::vector<int> key_from_index;
  CreateModelVariables(data_variables, &solver, &variables, &index_from_key,
                       &key_from_index);

  // Create Model Constraints.
  CreateModelConstraints(hard_constraints, variables, index_from_key, &solver);

  // Penalize variable and constraint violations.
  std::vector<IntVar*> cost;
  std::vector<IntVar*> violated_constraints(ordered_constraints.size(),
                                            nullptr);
  PenalizeVariablesViolation(soft_variables, index_from_key, variables, &cost,
                             &solver);
  PenalizeConstraintsViolation(ordered_constraints, soft_constraints,
                               index_from_key, variables, &cost,
                               &violated_constraints, &solver);

  // Objective
  // Minimize the sum of violation penalties.
  IntVar* objective_var = solver.MakeSum(cost)->Var();
  OptimizeVar* objective = solver.MakeMinimize(objective_var, 1);
  monitors.push_back(objective);

  // Ordering Builder
  OrderingBuilder* ob = solver.RevAlloc(
      new OrderingBuilder(data_variables, ordered_constraints, variables,
                          violated_constraints, index_from_key));

  // Decision Builder Configuration
  // Choose the next variable selection strategy.
  DecisionBuilder* db;
  if (FLAGS_variable_evaluator == "variable_evaluator") {
    LOG(INFO) << "Using VariableEvaluator for variable selection strategy and "
                 "Solver::ASSIGN_MIN_VALUE for value selection strategy.";
    Solver::IndexEvaluator1 var_evaluator = [&key_from_index,
                                             &data_variables](int64 index) {
      return VariableEvaluator(key_from_index, data_variables, index);
    };
    db = solver.MakePhase(variables, var_evaluator, Solver::ASSIGN_MIN_VALUE);
  } else {
    LOG(INFO) << "Using Solver::CHOOSE_FIRST_UNBOUND for variable selection "
                 "strategy and Solver::ASSIGN_MIN_VALUE for value selection "
                 "strategy.";
    db = solver.MakePhase(variables, Solver::CHOOSE_FIRST_UNBOUND,
                          Solver::ASSIGN_MIN_VALUE);
  }
  DecisionBuilder* final_db = solver.Compose(ob, db);

  // Create Additional Monitors.
  CreateAdditionalMonitors(objective, &solver, &monitors);

  // Collector
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(variables);
  collector->Add(objective_var);
  monitors.push_back(collector);

  // Solve.
  LOG(INFO) << "Solving...";
  const int64 time1 = solver.wall_time();
  solver.Solve(final_db, monitors);
  const int64 time2 = solver.wall_time();

  int violation_sum =
      collector->Value(collector->solution_count() - 1, objective_var);
  // Display Time.
  if (FLAGS_display_time) {
    PrintElapsedTime(time1, time2);
  }
  // Display Results.
  if (FLAGS_display_results) {
    PrintResultsSoft(collector, variables, objective_var, hard_variables,
                     hard_constraints, soft_variables, soft_constraints,
                     index_from_key, key_from_index);
  }

  return violation_sum;
}

void SolveProblem(const std::map<int, FapVariable>& variables,
                  const std::vector<FapConstraint>& constraints,
                  const std::string& objective, const std::vector<int>& values,
                  bool soft) {
  // Print Instance!
  FapModelPrinter model_printer(variables, constraints, objective, values);
  model_printer.PrintFapObjective();
  model_printer.PrintFapVariables();
  model_printer.PrintFapConstraints();
  model_printer.PrintFapValues();
  // Create Model & Solve!
  if (!soft) {
    LOG(INFO) << "Running HardFapSolver";
    HardFapSolver(variables, constraints, objective, values);
  } else {
    LOG(INFO) << "Running SoftFapSolver";
    int violation = SoftFapSolver(variables, constraints, objective, values);
    if (violation == 0) {
      LOG(INFO) << "The instance is feasible. "
                   "Now the HardFapSolver will be executed.";
      LOG(INFO) << "Running HardFapSolver";
      HardFapSolver(variables, constraints, objective, values);
    }
  }
}

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);

  CHECK(!FLAGS_directory.empty()) << "Requires --directory=<directory name>";

  LOG(INFO) << "Solving instance in directory  " << FLAGS_directory;
  // Parse!
  std::map<int, operations_research::FapVariable> variables;
  std::vector<operations_research::FapConstraint> constraints;
  std::string objective;
  std::vector<int> values;
  std::unordered_map<int, operations_research::FapComponent> components;
  operations_research::ParseInstance(FLAGS_directory, FLAGS_find_components,
                                     &variables, &constraints, &objective,
                                     &values, &components);
  if (!FLAGS_find_components) {
    operations_research::SolveProblem(variables, constraints, objective, values,
                                      FLAGS_soft);
  } else {
    int component_id = 1;
    LOG(INFO) << "Number of components in the RLFAP graph "
              << components.size();
    for (const auto& component : components) {
      LOG(INFO) << "Solving Component " << component_id;
      operations_research::SolveProblem(component.second.variables,
                                        component.second.constraints, objective,
                                        values, FLAGS_soft);
      component_id++;
    }
  }
  return 0;
}
