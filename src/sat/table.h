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

#ifndef OR_TOOLS_SAT_TABLE_H_
#define OR_TOOLS_SAT_TABLE_H_

#include "sat/integer.h"
#include "sat/model.h"

namespace operations_research {
namespace sat {

// Enforces that the given tuple of variables is equal to one of the given
// tuples. All the tuples must have the same size as var.size(), this is
// Checked.
std::function<void(Model*)> TableConstraint(
    const std::vector<IntegerVariable>& vars, const std::vector<std::vector<int64>>& tuples);

// Given an automata defined by a set of 3-tuples:
//     (state, transition_with_value_as_label, next_state)
// this accepts the sequences of vars.size() variables that are recognized by
// this automata. That is:
//   - We start from the initial state.
//   - For each variable, we move along the transition labeled by this variable
//     value. Moreover, the variable must take a value that correspond to a
//     feasible transition.
//   - We only accept sequences that ends in one of the final states.
//
// We CHECK that there is only one possible transition for a state/value pair.
// See the test for some examples.
std::function<void(Model*)> TransitionConstraint(
    const std::vector<IntegerVariable>& vars, const std::vector<std::vector<int64>>& automata,
    int64 initial_state, const std::vector<int64>& final_states);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TABLE_H_
