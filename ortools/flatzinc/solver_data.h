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

#ifndef OR_TOOLS_FLATZINC_SOLVER_DATA_H_
#define OR_TOOLS_FLATZINC_SOLVER_DATA_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/sat_constraint.h"

namespace operations_research {
namespace fz {

// Data structure to hold the mapping between flatzinc model objects and CP
// objects.
class SolverData {
 public:
  explicit SolverData(const std::string& name) : solver_(name), sat_(nullptr) {}

  // ----- Methods that deals with expressions and variables -----
  IntExpr* GetOrCreateExpression(const Argument& argument);
  std::vector<IntVar*> GetOrCreateVariableArray(const Argument& argument);
  IntExpr* Extract(IntegerVariable* var);
  void SetExtracted(IntegerVariable* var, IntExpr* expr);
  const std::unordered_map<IntegerVariable*, IntExpr*>& extracted_map() const {
    return extracted_map_;
  }

  // ----- Methods that deals with AllDifferent information -----

  // Stores the fact that the array of variable diffs appears in
  // an AllDifferent constraints.
  void StoreAllDifferent(std::vector<IntegerVariable*> diffs);

  // Queries wether the array diffs appears in an AllDifferent constraint.
  // Currently, this performs exact matching, therefore a sub-array of an
  // array of all-different variables will not match.
  bool IsAllDifferent(std::vector<IntegerVariable*> diffs) const;

  // Returns the CP solver.
  operations_research::Solver* solver() { return &solver_; }

  // Creates the sat propagator constraint and adds it to the solver.
  void CreateSatPropagatorAndAddToSolver();
  // Returns the sat propagator constraint.
  SatPropagator* Sat() const {
    CHECK(sat_ != nullptr);
    return sat_;
  }

 private:
  operations_research::Solver solver_;
  SatPropagator* sat_;
  std::unordered_map<IntegerVariable*, IntExpr*> extracted_map_;

  // Stores a set of sorted std::vector<IntegerVariables*>.
  // TODO(user, fdid): If it become too slow, switch to an unordered_set, it
  // isn't too hard to define the hash of a vector.
  std::set<std::vector<IntegerVariable*>> alldiffs_;
};
}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_SOLVER_DATA_H_
