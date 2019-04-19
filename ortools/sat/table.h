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

#ifndef OR_TOOLS_SAT_TABLE_H_
#define OR_TOOLS_SAT_TABLE_H_

#include <functional>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Enforces that the given tuple of variables is equal to one of the given
// tuples. All the tuples must have the same size as var.size(), this is
// Checked.
void AddTableConstraint(absl::Span<const IntegerVariable> vars,
                        std::vector<std::vector<int64>> tuples, Model* model);

// Enforces that none of the given tuple appear.
//
// TODO(user): we could propagate more than what we currently do which is simply
// adding one clause per tuples.
void AddNegatedTableConstraint(absl::Span<const IntegerVariable> vars,
                               std::vector<std::vector<int64>> tuples,
                               Model* model);

// Enforces that exactly one literal in line_literals is true, and that
// all literals in the corresponding line of the literal_tuples matrix are true.
// This constraint assumes that exactly one literal per column of the
// literal_tuples matrix is true.
std::function<void(Model*)> LiteralTableConstraint(
    const std::vector<std::vector<Literal>>& literal_tuples,
    const std::vector<Literal>& line_literals);

// This method tries to compress a list of tuples by merging complementary
// tuples, that is a set of tuples that only differ on one variable, and that
// cover the domain of the variable. In that case, it will keep only one tuple,
// and replace the value for variable by any_value, the equivalent of '*' in
// regexps.
//
// This method is exposed for testing purposes.
void CompressTuples(absl::Span<const int64> domain_sizes, int64 any_value,
                    std::vector<std::vector<int64>>* tuples);

// Given an automaton defined by a set of 3-tuples:
//     (state, transition_with_value_as_label, next_state)
// this accepts the sequences of vars.size() variables that are recognized by
// this automaton. That is:
//   - We start from the initial state.
//   - For each variable, we move along the transition labeled by this variable
//     value. Moreover, the variable must take a value that correspond to a
//     feasible transition.
//   - We only accept sequences that ends in one of the final states.
//
// We CHECK that there is only one possible transition for a state/value pair.
// See the test for some examples.
std::function<void(Model*)> TransitionConstraint(
    const std::vector<IntegerVariable>& vars,
    const std::vector<std::vector<int64>>& automaton, int64 initial_state,
    const std::vector<int64>& final_states);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TABLE_H_
