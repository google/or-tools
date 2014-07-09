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
// the set of all frequences that are available for this link.
// The essential constraint involving two variables of the problem F1 and F2 is
// |F1 - F2| > k12, where k12 is a predefined constant value.
// The Frequency Assignment Problem is an NP-complete problem as proved by means
// of reduction from k-Colorability problem for undirected graphs.
// The solution of the problem can be based on various criteria:
// - Simple satisfaction
// - Minimizing the number of frequencies used
// - Minimizing the maximum frequency used
// - Minimizing a weighted sum of violated constraints if the problem is
//   inconsistent
// More on the Frequency Assignment Problem and the data format of its instances
// can be found at: http://www.inra.fr/mia/T/schiex/Doc/CELAR.shtml#synt
//
// Implementation
// Two solvers are implemented: The FapSolverHard is dealing with finding the
// solution to feasible instances of the problem with objective either the
// minimization of the largest frequency assigned or the minimization of
// the number of frequencies used to the solution.
// The FapSolverSoft is dealing with the optimization of unfeasible instances
// and aims to minimize the total cost of violated constraints.
// If the latter solver is forced to solve a feasible instance, the main
// function redirects to the former.
//

#include <algorithm>
#include <map>
#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/concise_iterator.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "cpp/fap_model_printer.h"
#include "cpp/fap_parser.h"
#include "cpp/fap_utilities.h"

DEFINE_string(directory, "", "Specifies the directory of the data.");
DEFINE_string(evaluator, "",
              "Specifies if a value evaluator will be used by the "
              "decision builder.");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, <= 0 means no limit.");
DEFINE_int32(choose_next_variable_strategy, 1,
            "Selection strategy for variable: "
             "1 = CHOOSE_MIN_SIZE_LOWEST_MIN, "
             "2 = CHOOSE_MIN_SIZE_HIGHEST_MAX, "
             "3 = CHOOSE_FIRST_UNBOUND, "
             "4 = CHOOSE_RANDOM, ");
DEFINE_int32(restart, -1, "Parameter for constant restart monitor.");
DEFINE_bool(luby, false,
            "Use luby restart monitor instead of constant restart monitor.");
DEFINE_bool(log_search, true,
            "Create a search log.");
DEFINE_bool(soft, false,
            "Use soft solver instead of hard solver.");
DEFINE_bool(display_time, true,
            "Print how much time the solving process took.");
DEFINE_bool(display_results, true,
            "Print the results of the solving process.");


namespace operations_research {

int64 ValueEvaluator(hash_map<int64, std::pair<int64, int64> >* value_evaluator,
                    int64 variable_index,
                    int64 value) {
  CHECK_NOTNULL(value_evaluator);
  // Evaluate the choice. Smaller ranking denotes a better choice.
  int64 ranking = -1;
  for (ConstIter<hash_map<int64, std::pair<int64, int64> > >
	 it(*value_evaluator); !it.at_end(); ++it) {
    if ((it->first != variable_index) && (it->second.first == value)) {
      ranking = -2;
      break;
    }
  }

  // Update the history of assigned values and their rankings of each variable.
  hash_map<int64, std::pair<int64, int64> >::iterator it;
  int64 new_value = value;
  int64 new_ranking = ranking;
  if ((it = value_evaluator->find(variable_index)) != value_evaluator->end()) {
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
  InsertOrUpdate(value_evaluator, variable_index, new_value_ranking);

  return new_ranking;
}

// Creates the variables of the solver from the parsed data.
void CreateModelVariables(const std::map<int, FapVariable>& data_variables,
                          Solver* solver,
                          std::vector<IntVar*>* model_variables,
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
  for (ConstIter<std::map<int, FapVariable> > it(data_variables);
       !it.at_end(); ++it) {
    CHECK_LT(index, model_variables->size());
    (*model_variables)[index] = solver->MakeIntVar(it->second.domain_);
    InsertOrUpdate(index_from_key, it->first, index);
    (*key_from_index)[index] = it->first;

    if ((it->second.initial_position_ != -1) && (it->second.hard_)) {
      CHECK_LT(it->second.mobility_cost_, 0);
      solver->AddConstraint(solver->MakeEquality((*model_variables)[index],
                                               it->second.initial_position_));
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

  for (ConstIter<std::vector<FapConstraint> > it(data_constraints);
       !it.at_end(); ++it) {
    const int index1 = FindOrDie(index_from_key, it->variable1_);
    const int index2 = FindOrDie(index_from_key, it->variable2_);
    CHECK_LT(index1, variables.size());
    CHECK_LT(index2, variables.size());
    IntVar* var1 = variables[index1];
    IntVar* var2 = variables[index2];
    IntVar* absolute_difference = solver->MakeAbs(solver->MakeDifference(var1,
                                                                         var2))
                                        ->Var();
    if (it->operator_ == ">") {
      solver->AddConstraint(solver->MakeGreater(absolute_difference,
                                                it->value_));
    } else if (it->operator_ == "=") {
      solver->AddConstraint(solver->MakeEquality(absolute_difference,
                                                 it->value_));
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
      *variable_strategy = Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
      LOG(INFO) << "Using Solver::CHOOSE_MIN_SIZE_LOWEST_MIN "
                   "for variable selection strategy.";
    break;
    }
    case 2: {
      *variable_strategy = Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX;
      LOG(INFO) << "Using Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX "
                   "for variable selection strategy.";
      break;
    }
    case 3: {
      *variable_strategy = Solver::CHOOSE_FIRST_UNBOUND;
      LOG(INFO) << "Using Solver::CHOOSE_FIRST_UNBOUND "
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
void CreateAdditionalMonitors(OptimizeVar* const objective,
                              Solver* solver,
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
    SearchLimit* const limit = solver->MakeLimit(FLAGS_time_limit_in_ms,
                                                 kint64max,
                                                 kint64max,
                                                 kint64max);
    monitors->push_back(limit);
  }

  // Search Restart
  SearchMonitor* const restart = FLAGS_restart != -1?
      (FLAGS_luby?
       solver->MakeLubyRestart(FLAGS_restart):
       solver->MakeConstantRestart(FLAGS_restart)):
      NULL;
  if (restart) {
    monitors->push_back(restart);
  }
}

// The Hard Solver is dealing with finding the solution to feasible
// instances of the problem with objective either the minimization of
// the largest frequency assigned or the minimization of the number
// of frequencies used to the solution.
void FapSolverHard(const std::map<int, FapVariable>& data_variables,
                   const std::vector<FapConstraint>& data_constraints,
                   const string& data_objective,
                   const std::vector<int>& values) {
  Solver solver("FapSolverHard");
  std::vector<SearchMonitor*> monitors;

  // Create Model Variables
  std::vector<IntVar*> variables;
  std::map<int, int> index_from_key;
  std::vector<int> key_from_index;
  CreateModelVariables(data_variables, &solver, &variables,
                       &index_from_key, &key_from_index);

  // Create Model Constraints
  CreateModelConstraints(data_constraints, variables, index_from_key, &solver);

  // Objective:
  // Either minimize the largest assigned frequency or
  // minimize the number of different frequencies assigned
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
    solver.MakeIntVarArray(static_cast<int>(values.size()),
                           0,
                           static_cast<int>(variables.size()),
                           &cardinality);
    solver.AddConstraint(solver.MakeDistribute(variables, values, cardinality));
    std::vector<IntVar*> value_not_assigned;
    for (int val = 0; val < values.size(); ++val) {
      value_not_assigned.push_back(solver.MakeIsEqualCstVar(cardinality[val],
                                                            0));
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
  LOG(INFO) << "Finished with objective specifier.";
  monitors.push_back(objective);

  // Collector
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(variables);
  collector->Add(objective_var);
  LOG(INFO) << "Made collector.";
  monitors.push_back(collector);

  // Decision Builder Configuration
  // Choose the next variable selection strategy
  Solver::IntVarStrategy variable_strategy;
  ChooseVariableStrategy(&variable_strategy);
  // Choose the value selection strategy
  DecisionBuilder* db;
  hash_map<int64, std::pair<int64, int64> > history;
  if (FLAGS_evaluator == "evaluator") {
    LOG(INFO) << "Using ValueEvaluator for value selection strategy.";
    db = solver.MakePhase(variables,
                          variable_strategy,
                          NewPermanentCallback(&ValueEvaluator, &history));
  } else {
    LOG(INFO) << "Using Solver::ASSIGN_MIN_VALUE for value selection strategy.";
    db = solver.MakePhase(variables,
                          variable_strategy,
                          Solver::ASSIGN_MIN_VALUE);
  }

  // Create Additional Monitors
  CreateAdditionalMonitors(objective, &solver, &monitors);

  // Solve
  LOG(INFO) << "Solving...";
  const int64 time1 = solver.wall_time();
  solver.Solve(db, monitors);
  const int64 time2 = solver.wall_time();

  // Display
  if (FLAGS_display_time) {
    PrintElapsedTime(time1, time2);
  }

  if (FLAGS_display_results) {
    PrintResultsHard(collector, variables, objective_var,
                     data_variables, data_constraints,
                     index_from_key, key_from_index);
  }
}

// The Soft Solver is dealing with the optimization of unfeasible instances
// and aims to minimize the total cost of violated constraints. Returning value
// equals to 0 denotes that the instance is feasible.
int FapSolverSoft(const std::map<int, FapVariable>& data_variables,
                  const std::vector<FapConstraint>& data_constraints,
                  const string& data_objective, const std::vector<int>& values) {
  Solver solver("FapSolverSoft");
  std::vector<SearchMonitor*> monitors;

  // Split variables to hard and soft
  std::map<int, FapVariable> hard_variables;
  std::map<int, FapVariable> soft_variables;
  for (ConstIter<std::map<int, FapVariable> > it(data_variables);
       !it.at_end(); ++it) {
    if (it->second.initial_position_ != -1) {
      if (it->second.hard_) {
        CHECK_LT(it->second.mobility_cost_, 0);
        InsertOrUpdate(&hard_variables, it->first, it->second);
      } else {
        CHECK_GE(it->second.mobility_cost_, 0);
        InsertOrUpdate(&soft_variables, it->first, it->second);
      }
    }
  }
  // Split constraints to hard and soft
  std::vector<FapConstraint> hard_constraints;
  std::vector<FapConstraint> soft_constraints;
  for (ConstIter<std::vector<FapConstraint> > it(data_constraints);
       !it.at_end(); ++it) {
    if (it->hard_) {
      CHECK_LT(it->weight_cost_ , 0);
      hard_constraints.push_back(*it);
    } else {
      CHECK_GE(it->weight_cost_ , 0);
      soft_constraints.push_back(*it);
    }
  }

  // Create Model Variables
  std::vector<IntVar*> variables;
  std::map<int, int> index_from_key;
  std::vector<int> key_from_index;
  CreateModelVariables(data_variables, &solver, &variables,
                       &index_from_key, &key_from_index);

  // Create Model Constraints
  CreateModelConstraints(hard_constraints, variables, index_from_key, &solver);

  // Objective:
  // Minimize the weighted sum of violated constraints
  IntVar* objective_var;
  OptimizeVar* objective;
  std::vector<IntVar*> cost;
  // Penalize the modification of the initial position of a soft variable
  for (ConstIter<std::map<int, FapVariable> > it(soft_variables);
       !it.at_end(); ++it) {
    const int index = index_from_key[it->first];
    CHECK_LT(index, variables.size());
    IntExpr* displaced =
        solver.MakeIsDifferentCstVar(variables[index],
                                     it->second.initial_position_);
    IntExpr* weight = solver.MakeProd(displaced, it->second.mobility_cost_);
    cost.push_back(weight->Var());
  }
  // Penalize the violation of a soft constraint
  for (ConstIter<std::vector<FapConstraint> > it(soft_constraints);
       !it.at_end(); ++it) {
    const int index1 = index_from_key[it->variable1_];
    const int index2 = index_from_key[it->variable2_];
    CHECK_LT(index1, variables.size());
    CHECK_LT(index2, variables.size());
    IntVar* absolute_difference =
        solver.MakeAbs(solver.MakeDifference(variables[index1],
                                             variables[index2]))
                      ->Var();
    IntExpr* violation;
    if (it->operator_ == ">") {
      violation = solver.MakeIsLessCstVar(absolute_difference,
                                          it->value_);
    } else if (it->operator_ == "=") {
      violation = solver.MakeIsDifferentCstVar(absolute_difference,
                                               it->value_);
    } else {
      LOG(FATAL) << "Invalid operator detected.";
      return -1;
    }
    IntExpr* weight = solver.MakeProd(violation, it->weight_cost_);
    cost.push_back(weight->Var());
  }
  objective_var = solver.MakeSum(cost)->Var();
  objective = solver.MakeMinimize(objective_var, 1);
  LOG(INFO) << "Finished with penalties.";
  monitors.push_back(objective);

  // Collector
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(variables);
  collector->Add(objective_var);
  LOG(INFO) << "Made collector.";
  monitors.push_back(collector);

  // Decision Builder Configuration
  // Choose the next variable selection strategy
  Solver::IntVarStrategy variable_strategy;
  ChooseVariableStrategy(&variable_strategy);
  // Choose the value selection strategy
  LOG(INFO) << "Using Solver::ASSIGN_RANDOM_VALUE for value selection "
               "strategy.";
  DecisionBuilder* const db = solver.MakePhase(variables,
                                               variable_strategy,
                                               Solver::ASSIGN_RANDOM_VALUE);

  // Create Additional Monitors
  CreateAdditionalMonitors(objective, &solver, &monitors);

  // Solve
  LOG(INFO) << "Solving...";
  const int64 time1 = solver.wall_time();
  solver.Solve(db, monitors);
  const int64 time2 = solver.wall_time();

  int result = collector->Value(collector->solution_count() - 1, objective_var);
  // Display Time //
  if (FLAGS_display_time) {
    PrintElapsedTime(time1, time2);
  }
  if (result != 0) {
    // Display Results //
    if (FLAGS_display_results) {
      PrintResultsSoft(collector, variables, objective_var,
                       hard_variables, hard_constraints,
                       soft_variables, soft_constraints,
                       index_from_key, key_from_index);
    }
  }

  return result;
}

}  // namespace operations_research


int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  CHECK(!FLAGS_directory.empty()) << "Requires --directory=<directory name>";

  // Parse!
  std::map<int, operations_research::FapVariable> variables;
  std::vector<operations_research::FapConstraint> constraints;
  string objective;
  std::vector<int> values;
  operations_research::ParseInstance(FLAGS_directory, &variables,
                                     &constraints, &objective, &values);
  // Print Instance!
  operations_research::FapModelPrinter model_printer(variables, constraints,
                                                     objective, values);
  model_printer.PrintFapObjective();
  model_printer.PrintFapVariables();
  model_printer.PrintFapConstraints();
  model_printer.PrintFapValues();

  // Create Model & Solve!
  if (!FLAGS_soft) {
    LOG(INFO) << "Running FapSolverHard on directory: " << FLAGS_directory;
    operations_research::FapSolverHard(variables, constraints,
                                       objective, values);
  } else {
    LOG(INFO) << "Running FapSolverSoft on directory: " << FLAGS_directory;
    int result = operations_research::FapSolverSoft(variables, constraints,
                                                    objective, values);
    if (result == 0) {
      LOG(INFO) << "The instance is feasible. "
                   "Now the FapSolverHard will be executed.";
      LOG(INFO) << "Running FapSolverHard on directory: " << FLAGS_directory;
      operations_research::FapSolverHard(variables, constraints,
                                         objective, values);
    }
  }

  return 0;
}
