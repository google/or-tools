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
#ifndef OR_TOOLS_FLATZINC_SOLVER_H_
#define OR_TOOLS_FLATZINC_SOLVER_H_

#include "constraint_solver/constraint_solver.h"
#include "flatzinc2/model.h"
#include "flatzinc2/search.h"

namespace operations_research {
class SatPropagator;

// The main class to search for a solution in a flatzinc model.  It is
// responsible for parsing the search annotations, setting up the
// search state and perform the actual search.
class FzSolver {
 public:
  explicit FzSolver(const FzModel& model)
      : model_(model),
        statistics_(model),
        solver_(model.name()),
        sat_(nullptr) {}

  // Search for for solutions in the model passed at construction
  // time.  The exact search context (search for optimal solution, for
  // n solutions, for the first solution) are specified in the
  // parameters.
  // The parallel context (sequential, multi-threaded) is encapsulated
  // in the parallel support interface.
  void Solve(FzSolverParameters p,
             FzParallelSupportInterface* parallel_support);

  // Extraction support.
  bool Extract();
  IntExpr* GetExpression(const FzArgument& argument);
  std::vector<IntVar*> GetVariableArray(const FzArgument& argument);
  IntExpr* Extract(FzIntegerVariable* var);
  void SetExtracted(FzIntegerVariable* var, IntExpr* expr);
  bool IsAllDifferent(const std::vector<FzIntegerVariable*>& diffs) const;

  // Output support.
  std::string SolutionString(const FzOnSolutionOutput& output);

  // Returns the cp solver.
  Solver* solver() { return &solver_; }

  // Returns the sat constraint.
  SatPropagator* Sat() const { return sat_; }

 private:
  void ExtractConstraint(FzConstraint* ct);
  bool HasSearchAnnotations() const;
  void ParseSearchAnnotations(bool ignore_unknown,
                              std::vector<DecisionBuilder*>* defined,
                              std::vector<IntVar*>* defined_variables,
                              std::vector<IntVar*>* active_variables,
                              std::vector<int>* defined_occurrences,
                              std::vector<int>* active_occurrences);
  void AddCompletionDecisionBuilders(const std::vector<IntVar*>& defined_variables,
                                     const std::vector<IntVar*>& active_variables,
                                     std::vector<DecisionBuilder*>* builders);
  DecisionBuilder* CreateDecisionBuilders(const FzSolverParameters& p);
  const std::vector<IntVar*>& PrimaryVariables() const;
  const std::vector<IntVar*>& SecondaryVariables() const;
  void CollectOutputVariables(std::vector<IntVar*>* output_variables);
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
  // Alldiff info before extraction
  void StoreAllDifferent(const std::vector<FzIntegerVariable*>& diffs);
  hash_map<const FzIntegerVariable*, std::vector<std::vector<FzIntegerVariable*> > >
      alldiffs_;
  // Sat constraint.
  SatPropagator* sat_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_SOLVER_H_
