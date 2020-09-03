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

#ifndef OR_TOOLS_BOP_BOP_SOLUTION_H_
#define OR_TOOLS_BOP_BOP_SOLUTION_H_

#include "ortools/bop/bop_types.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"

namespace operations_research {
namespace bop {

// A Bop solution is a Boolean assignment for each variable of the problem. The
// cost value associated to the solution is the instantiation of the objective
// cost of the problem.
//
// Note that a solution might not be a feasible solution, i.e. might violate
// some constraints of the problem. The IsFeasible() method can be used to test
// the feasibility.
class BopSolution {
 public:
  BopSolution(const LinearBooleanProblem& problem, const std::string& name);

  void SetValue(VariableIndex var, bool value) {
    recompute_cost_ = true;
    recompute_is_feasible_ = true;
    values_[var] = value;
  }

  size_t Size() const { return values_.size(); }
  bool Value(VariableIndex var) const { return values_[var]; }
  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  // Returns the objective cost of the solution.
  // Note that this code is lazy but not incremental and might run in the
  // problem size. Use with care during search.
  int64 GetCost() const {
    if (recompute_cost_) {
      cost_ = ComputeCost();
    }
    return cost_;
  }

  // Returns the objective cost of the solution taking into account the problem
  // cost scaling and offset. This is mainly useful for displaying the current
  // problem cost, while internally, the algorithm works directly with the
  // integer version of the cost returned by GetCost().
  double GetScaledCost() const {
    return sat::AddOffsetAndScaleObjectiveValue(*problem_,
                                                sat::Coefficient(GetCost()));
  }

  // Returns true iff the solution is feasible.
  // Note that this code is lazy but not incremental and might run in the
  // problem size. Use with care during search.
  bool IsFeasible() const {
    if (recompute_is_feasible_) {
      is_feasible_ = ComputeIsFeasible();
    }
    return is_feasible_;
  }

  // For range based iteration, i.e. for (const bool value : solution) {...}.
  gtl::ITIVector<VariableIndex, bool>::const_iterator begin() const {
    return values_.begin();
  }
  gtl::ITIVector<VariableIndex, bool>::const_iterator end() const {
    return values_.end();
  }

  // Returns true when the cost of the argument solution is strictly greater
  // than the cost of the object.
  // This is used to sort solutions.
  bool operator<(const BopSolution& solution) const {
    return IsFeasible() == solution.IsFeasible()
               ? GetCost() < solution.GetCost()
               : IsFeasible() > solution.IsFeasible();
  }

 private:
  bool ComputeIsFeasible() const;
  int64 ComputeCost() const;

  const LinearBooleanProblem* problem_;
  std::string name_;
  gtl::ITIVector<VariableIndex, bool> values_;

  // Those are mutable because they behave as const values for a given solution
  // but for performance reasons we want to be lazy on their computation,
  // e.g. not compute the cost each time set_value() is called.
  mutable bool recompute_cost_;
  mutable bool recompute_is_feasible_;
  mutable int64 cost_;
  mutable bool is_feasible_;

  // Note that assign/copy are defined to allow usage of
  // STL collections / algorithms.
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_SOLUTION_H_
