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

#include "sat/table.h"

#include "base/map_util.h"
#include "base/stl_util.h"

namespace operations_research {
namespace sat {

namespace {

// Transpose the given "matrix" and transform the value to IntegerValue.
std::vector<std::vector<IntegerValue>> Transpose(const std::vector<std::vector<int64>> tuples) {
  CHECK(!tuples.empty());
  const int n = tuples.size();
  const int m = tuples[0].size();
  std::vector<std::vector<IntegerValue>> transpose(m, std::vector<IntegerValue>(n));
  for (int i = 0; i < n; ++i) {
    CHECK_EQ(m, tuples[i].size());
    for (int j = 0; j < m; ++j) {
      transpose[j][i] = tuples[i][j];
    }
  }
  return transpose;
}

// Converts the vector representation returned by FullDomainEncoding() to a map.
hash_map<IntegerValue, Literal> GetEncoding(IntegerVariable var, Model* model) {
  hash_map<IntegerValue, Literal> encoding;
  for (const auto& entry :
       model->GetOrCreate<IntegerEncoder>()->FullDomainEncoding(var)) {
    encoding[entry.value] = entry.literal;
  }
  return encoding;
}

// Add the implications and clauses to link one column of a table to the Literal
// controling if the lines are possible or not. The column has the given values,
// and the Literal of the column variable can be retreived using the encoding
// map.
void ProcessOneColumn(const std::vector<Literal>& line_literals,
                      const std::vector<IntegerValue>& values,
                      const hash_map<IntegerValue, Literal>& encoding,
                      Model* model) {
  CHECK_EQ(line_literals.size(), values.size());
  hash_map<IntegerValue, std::vector<Literal>> value_to_list_of_line_literals;

  // If a value is false (i.e not possible), then the tuple with this value
  // is false too (i.e not possible).
  for (int i = 0; i < values.size(); ++i) {
    const IntegerValue v = values[i];
    value_to_list_of_line_literals[v].push_back(line_literals[i]);
    model->Add(Implication(FindOrDie(encoding, v).Negated(),
                           line_literals[i].Negated()));
  }

  // If all the tuples containing a value are false, then this value must be
  // false too.
  for (const auto& entry : value_to_list_of_line_literals) {
    std::vector<Literal> clause = entry.second;
    clause.push_back(FindOrDie(encoding, entry.first).Negated());
    model->Add(ClauseConstraint(clause));
  }
}

}  // namespace

std::function<void(Model*)> TableConstraint(
    const std::vector<IntegerVariable>& vars, const std::vector<std::vector<int64>>& tuples) {
  return [=](Model* model) {
    // Create one Boolean variable per tuple to indicate if it can still be
    // selected or not. Note that we don't enforce exactly one tuple to be
    // selected because these variables are just used by this constraint, so
    // only the information "can't be selected" is important.
    //
    // TODO(user): If a value in one column is unique, we don't need to create a
    // new BooleanVariable corresponding to this line since we can use the one
    // corresponding to this value in that column.
    std::vector<Literal> tuple_literals;
    for (int i = 0; i < tuples.size(); ++i) {
      tuple_literals.push_back(Literal(model->Add(NewBooleanVariable()), true));
    }

    // Fully encode the variables using all the values appearing in the tuples.
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    hash_map<IntegerValue, Literal> encoding;
    const std::vector<std::vector<IntegerValue>>& tr_tuples = Transpose(tuples);
    for (int i = 0; i < vars.size(); ++i) {
      encoder->FullyEncodeVariable(vars[i], tr_tuples[i]);
      encoding = GetEncoding(vars[i], model);
      ProcessOneColumn(tuple_literals, tr_tuples[i], encoding, model);
    }
  };
}

std::function<void(Model*)> TransitionConstraint(
    const std::vector<IntegerVariable>& vars, const std::vector<std::vector<int64>>& automata,
    int64 initial_state, const std::vector<int64>& final_states) {
  return [=](Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    const int n = vars.size();
    CHECK_GT(n, 0) << "No variables in TransitionConstraint().";

    // Test precondition.
    {
      std::set<std::pair<int64, int64>> unique_transition_checker;
      for (const std::vector<int64>& transition : automata) {
        CHECK_EQ(transition.size(), 3);
        const std::pair<int64, int64> p{transition[0], transition[1]};
        CHECK(!ContainsKey(unique_transition_checker, p))
            << "Duplicate outgoing transitions with value " << transition[1]
            << " from state " << transition[0] << ".";
        unique_transition_checker.insert(p);
      }
    }

    // Compute the set of reachable state at each time point.
    std::vector<std::set<int64>> reachable_states(n + 1);
    reachable_states[0].insert(initial_state);
    reachable_states[n] = {final_states.begin(), final_states.end()};

    // Forward.
    for (int time = 0; time + 1 < n; ++time) {
      for (const std::vector<int64>& transition : automata) {
        if (!ContainsKey(reachable_states[time], transition[0])) continue;
        reachable_states[time + 1].insert(transition[2]);
      }
    }

    // Backward.
    for (int time = n - 1; time > 0; --time) {
      std::set<int64> new_set;
      for (const std::vector<int64>& transition : automata) {
        if (!ContainsKey(reachable_states[time], transition[0])) continue;
        if (!ContainsKey(reachable_states[time + 1], transition[2])) continue;
        new_set.insert(transition[0]);
      }
      reachable_states[time].swap(new_set);
    }

    // We will model at each time step the current automata state using Boolean
    // variables. We will have n+1 time step. At time zero, we start in the
    // initial state, and at time n we should be in one of the final states. We
    // don't need to create Booleans at at time when there is just one possible
    // state (like at time zero).
    hash_map<IntegerValue, Literal> encoding;
    hash_map<IntegerValue, Literal> in_encoding;
    hash_map<IntegerValue, Literal> out_encoding;
    for (int time = 0; time < n; ++time) {
      // All these vector have the same size. We will use them to enforce a
      // local table constraint representing one step of the automata at the
      // given time.
      std::vector<Literal> tuple_literals;
      std::vector<IntegerValue> in_states;
      std::vector<IntegerValue> transition_values;
      std::vector<IntegerValue> out_states;
      for (const std::vector<int64>& transition : automata) {
        if (!ContainsKey(reachable_states[time], transition[0])) continue;
        if (!ContainsKey(reachable_states[time + 1], transition[2])) continue;

        // TODO(user): if this transition correspond to just one in-state or
        // one-out state or one variable value, we could reuse the corresponding
        // Boolean variable instead of creating a new one!
        tuple_literals.push_back(
            Literal(model->Add(NewBooleanVariable()), true));
        in_states.push_back(IntegerValue(transition[0]));

        transition_values.push_back(IntegerValue(transition[1]));
        out_states.push_back(IntegerValue(transition[2]));
      }

      // Fully instantiate vars[time].
      {
        std::vector<IntegerValue> s = transition_values;
        STLSortAndRemoveDuplicates(&s);

        encoding.clear();
        if (s.size() > 1) {
          std::vector<IntegerValue> values(s.begin(), s.end());
          encoder->FullyEncodeVariable(vars[time], values);
          encoding = GetEncoding(vars[time], model);
        } else {
          // Fix vars[time] to its unique possible value.
          CHECK_EQ(s.size(), 1);
          const int64 unique_value = s.begin()->value();
          model->Add(LowerOrEqual(vars[time], unique_value));
          model->Add(GreaterOrEqual(vars[time], unique_value));
        }
      }

      // For each possible out states, create one Boolean variable.
      //
      // TODO(user): enforce an at most one constraint? it is not really needed
      // though, so I am not sure it will improve or hurt the performance. To
      // investigate on real problems.
      {
        std::vector<IntegerValue> s = out_states;
        STLSortAndRemoveDuplicates(&s);

        out_encoding.clear();
        if (s.size() == 2) {
          const BooleanVariable var = model->Add(NewBooleanVariable());
          out_encoding[s.front()] = Literal(var, true);
          out_encoding[s.back()] = Literal(var, false);
        } else if (s.size() > 1) {
          // Enforce at most one constraint?
          for (const IntegerValue state : s) {
            out_encoding[state] =
                Literal(model->Add(NewBooleanVariable()), true);
          }
        }
      }

      // Now we link everything together.
      if (in_encoding.size() > 1) {
        ProcessOneColumn(tuple_literals, in_states, in_encoding, model);
      }
      if (encoding.size() > 1) {
        ProcessOneColumn(tuple_literals, transition_values, encoding, model);
      }
      if (out_encoding.size() > 1) {
        ProcessOneColumn(tuple_literals, out_states, out_encoding, model);
      }
      in_encoding = out_encoding;
    }
  };
}

}  // namespace sat
}  // namespace operations_research
