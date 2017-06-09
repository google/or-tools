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

#ifndef OR_TOOLS_SAT_ALL_DIFFERENT_H_
#define OR_TOOLS_SAT_ALL_DIFFERENT_H_

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// This constraint forces all variables to take different values.
// It uses the matching algorithm described in Regin at AAAI1994:
// "A filtering algorithm for constraints of difference in CSPs".
// This propagator is meant to be used as a complement to an alldifferent
// decomposition: DO NOT USE WITHOUT A BINARY ALLDIFFERENT.
// Doing the filtering that the decomposition can do with an appropriate
// algorithm should be cheaper and yield more accurate explanations.
// This will fully encode variables.
std::function<void(Model*)> AllDifferentAC(
    const std::vector<IntegerVariable>& variables);

class AllDifferentConstraint : PropagatorInterface {
 public:
  AllDifferentConstraint(std::vector<IntegerVariable> variables,
                         IntegerEncoder* encoder, Trail* trail,
                         IntegerTrail* integer_trail);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // MakeAugmentingPath() is a step in Ford-Fulkerson's augmenting path
  // algorithm. It changes its current internal state (see vectors below)
  // to assign a value to the start vertex using an augmenting path.
  // If it is not possible, it keeps variable_to_value_[start] to -1 and returns
  // false, otherwise it modifies the current assignment and returns true.
  // It uses value/variable_visited to mark the nodes it visits during its
  // search: one can use this information to generate an explanation of failure,
  // or manipulate it to create what-if scenarios without modifying successor_.
  bool MakeAugmentingPath(int start);

  // Accessors to the cache of literals.
  inline LiteralIndex VariableLiteralIndexOf(int x, int64 value);
  inline bool VariableHasPossibleValue(int x, int64 value);

  // This caches all literals of the fully encoded variables.
  // Values of a given variable are 0-indexed using offsets variable_min_value_,
  // the set of all values is globally offset using offset min_all_values_.
  // TODO(user): compare this encoding to a sparser hash_map encoding.
  const int num_variables_;
  const std::vector<IntegerVariable> variables_;
  int64 min_all_values_;
  int64 num_all_values_;
  std::vector<int64> variable_min_value_;
  std::vector<int64> variable_max_value_;
  std::vector<std::vector<LiteralIndex>> variable_literal_index_;

  // Internal state of MakeAugmentingPath().
  // value_to_variable_ and variable_to_value_ represent the current assignment;
  // -1 means not assigned. Otherwise,
  // variable_to_value_[var] = value <=> value_to_variable_[value] = var.
  std::vector<std::vector<int>> successor_;
  std::vector<bool> value_visited_;
  std::vector<bool> variable_visited_;
  std::vector<int> value_to_variable_;
  std::vector<int> variable_to_value_;
  std::vector<int> prev_matching_;
  std::vector<int> visiting_;
  std::vector<int> variable_visited_from_;

  // Internal state of ComputeSCCs().
  // Variable nodes are indexed by [0, num_variables_),
  // value nodes by [num_variables_, num_variables_ + num_all_values_),
  // and a dummy node with index num_variables_ + num_all_values_ is added.
  // The graph passed to ComputeSCCs() is the residual of the possible graph
  // by the current matching, i.e. its arcs are:
  // _ (var, val) if val \in dom(var) and var not matched to val,
  // _ (val, var) if var matched to val,
  // _ (val, dummy) if val not matched to any variable,
  // _ (dummy, var) for all variables.
  // In the original paper, forbidden arcs are identified by detecting that they
  // are not in any alternating cycle or alternating path starting at a
  // free vertex. Adding the dummy node allows to factor the alternating path
  // part in the alternating cycle, and filter with only the SCC decomposition.
  // When num_variables_ == num_all_values_, the dummy node is useless,
  // we add it anyway to simplify the code.
  std::vector<std::vector<int>> residual_graph_successors_;
  std::vector<int> component_number_;

  Trail* trail_;
  IntegerTrail* integer_trail_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ALL_DIFFERENT_H_
