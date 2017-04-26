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

#ifndef OR_TOOLS_FLATZINC_SOLVER_H_
#define OR_TOOLS_FLATZINC_SOLVER_H_

#include <unordered_map>
#include <unordered_set>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/reporting.h"
#include "ortools/flatzinc/solver_data.h"
#include "ortools/flatzinc/solver_util.h"

namespace operations_research {
class SatPropagator;

namespace fz {
// Search parameter for the flatzinc solver.
struct FlatzincParameters {
  enum SearchType {
    DEFAULT,
    IBS,
    FIRST_UNBOUND,
    MIN_SIZE,
    RANDOM_MIN,
    RANDOM_MAX,
  };

  FlatzincParameters();

  bool all_solutions;
  bool free_search;
  bool last_conflict;
  bool ignore_annotations;
  bool ignore_unknown;
  bool logging;
  bool statistics;
  bool verbose_impact;
  double restart_log_size;
  bool run_all_heuristics;
  int heuristic_period;
  int log_period;
  int luby_restart;
  int num_solutions;
  int random_seed;
  int threads;
  int thread_id;
  // TODO(user): Store time limit in seconds (double).
  int64 time_limit_in_ms;
  SearchType search_type;
  bool store_all_solutions;
};

// The main class to search for a solution in a flatzinc model.  It is
// responsible for parsing the search annotations, setting up the
// search state and performing the actual search.
class Solver {
 public:
  explicit Solver(const Model& model)
      : model_(model),
        statistics_(model),
        data_(model.name()),
        objective_var_(nullptr),
        objective_monitor_(nullptr),
        default_phase_(nullptr) {
    solver_ = data_.solver();
  }

  // Searches for for solutions in the model passed at construction
  // time.  The exact search context (search for optimal solution, for
  // n solutions, for the first solution) is specified in the
  // parameters.
  // The parallel context (sequential, multi-threaded) is encapsulated
  // in the search reporting interface.
  void Solve(FlatzincParameters p, SearchReportingInterface* report);

  // Extraction support.
  bool Extract();

  // String output for the minizinc interface.
  std::string SolutionString(const SolutionOutputSpecs& output) const;

  // Query the value of the variable. This must be called during search, when
  // a solution is found.
  int64 SolutionValue(IntegerVariable* var) const;

  // Programmatic interface to read the solutions.

  // Returns the number of solutions stored. You need to set store_all_solution
  // to true in the parameters, otherwise this method will always return 0.
  int NumStoredSolutions() const { return stored_values_.size(); }
  // Returns the stored value for the given variable in the solution_index'th
  // stored solution.
  // A variable is stored only if it appears in the output part of the model.
  int64 StoredValue(int solution_index, IntegerVariable* var) {
    CHECK_GE(solution_index, 0);
    CHECK_LT(solution_index, stored_values_.size());
    CHECK(ContainsKey(stored_values_[solution_index], var));
    return stored_values_[solution_index][var];
  }

  static void ReportInconsistentModel(const Model& model, FlatzincParameters p,
                                      SearchReportingInterface* report);

 private:
  void StoreSolution();
  bool HasSearchAnnotations() const;
  void ParseSearchAnnotations(bool ignore_unknown,
                              std::vector<DecisionBuilder*>* defined,
                              std::vector<IntVar*>* defined_variables,
                              std::vector<IntVar*>* active_variables,
                              std::vector<int>* defined_occurrences,
                              std::vector<int>* active_occurrences);
  void AddCompletionDecisionBuilders(
      const std::vector<IntVar*>& defined_variables,
      const std::vector<IntVar*>& active_variables, SearchLimit* limit,
      std::vector<DecisionBuilder*>* builders);
  DecisionBuilder* CreateDecisionBuilders(const FlatzincParameters& p,
                                          SearchLimit* limit);
  void CollectOutputVariables(std::vector<IntVar*>* output_variables);
  void SyncWithModel();

  const Model& model_;
  ModelStatistics statistics_;
  SolverData data_;
  std::vector<IntVar*> active_variables_;
  std::unordered_map<IntVar*, int> extracted_occurrences_;
  std::unordered_set<IntegerVariable*> implied_variables_;
  std::string search_name_;
  IntVar* objective_var_;
  OptimizeVar* objective_monitor_;
  // Default Search Phase (to get stats).
  DecisionBuilder* default_phase_;
  // Stored solutions.
  std::vector<std::unordered_map<IntegerVariable*, int64>> stored_values_;
  operations_research::Solver* solver_;
};
}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_SOLVER_H_
