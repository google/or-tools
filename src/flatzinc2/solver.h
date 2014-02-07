// Copyright 2010-2013 Google
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
#ifndef OR_TOOLS_FLATZINC_FZ_SOLVER_H_
#define OR_TOOLS_FLATZINC_FZ_SOLVER_H_

#include "constraint_solver/constraint_solver.h"
#include "flatzinc2/model.h"
#include "flatzinc2/search.h"

namespace operations_research {
// The main class to search for a solution in a flatzinc model.  It is
// responsible for parsing the search annotations, setting up the
// search state and perform the actual search.
class FzSolver {
 public:
  FzSolver(const FzModel& model)
  : model_(model), statistics_(model), solver_(model.name()) {}

  // Search for for solutions in the model passed at construction
  // time.  The exact search context (search for optimal solution, for
  // n solutions, for the first solution) are specified in the
  // parameters.
  // The parallel context (sequential, multi-threaded) is encapsulated
  // in the parallel support interface.
  void Solve(FzSolverParameters p,
             FzParallelSupportInterface* const parallel_support);

  // Extraction support.
  bool Extract();
  IntExpr* GetExpression(const FzArgument& argument);
  std::vector<IntVar*> GetVariableArray(const FzArgument& argument);
  IntExpr* Extract(FzIntegerVariable* const var);

  // Output support.
  std::string SolutionString(const FzOnSolutionOutput& output);

  // Returns the cp solver.
  Solver* const solver() { return &solver_; }

 private:
  void ExtractConstraint(FzConstraint* const ct);
  bool HasSearchAnnotations() const;
  void ParseSearchAnnotations(bool ignore_unknown,
                              std::vector<DecisionBuilder*>* const defined,
                              std::vector<IntVar*>* const defined_variables,
                              std::vector<IntVar*>* const active_variables,
                              std::vector<int>* const defined_occurrences,
                              std::vector<int>* const active_occurrences);
  void AddCompletionDecisionBuilders(const std::vector<IntVar*>& defined_variables,
                                     const std::vector<IntVar*>& active_variables,
                                     std::vector<DecisionBuilder*>* const builders);
  DecisionBuilder* CreateDecisionBuilders(const FzSolverParameters& p);
  const std::vector<IntVar*>& PrimaryVariables() const;
  const std::vector<IntVar*>& SecondaryVariables() const;
  void CollectOutputVariables(std::vector<IntVar*>* const output_variables);
  void SyncWithModel();

  const FzModel& model_;
  FzModelStatistics statistics_;
  Solver solver_;
  hash_map<FzIntegerVariable*, IntExpr*> extrated_map_;
  std::vector<IntVar*> active_variables_;
  std::vector<IntVar*> introduced_variables_;
  hash_map<IntVar*, int> extracted_occurrences_;
  std::string search_name_;
  IntVar* objective_var_;
  OptimizeVar* objective_monitor_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_FZ_SOLVER_H_
