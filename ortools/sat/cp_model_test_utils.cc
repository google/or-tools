// Copyright 2010-2025 Google LLC
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

#include "ortools/sat/cp_model_test_utils.h"

#include <stdint.h>

#include <limits>
#include <vector>

#include "absl/random/random.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

CpModelProto Random3SatProblem(int num_variables,
                               double proportion_of_constraints) {
  CpModelProto result;
  absl::BitGen random;
  result.set_name("Random 3-SAT");
  for (int i = 0; i < num_variables; ++i) {
    sat::IntegerVariableProto* var = result.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  const int num_constraints = proportion_of_constraints * num_variables;
  for (int i = 0; i < num_constraints; ++i) {
    auto* ct = result.add_constraints()->mutable_bool_or();
    std::vector<int> clause;
    while (ct->literals_size() != 3) {
      const int literal =
          absl::Uniform(random, NegatedRef(num_variables - 1), num_variables);
      bool is_already_present = false;
      for (const int lit : ct->literals()) {
        if (lit != literal) continue;
        is_already_present = true;
        break;
      }
      if (!is_already_present) ct->add_literals(literal);
    }
  }
  return result;
}

CpModelProto RandomLinearProblem(int num_variables, int num_constraints) {
  CpModelProto result;
  absl::BitGen random;
  result.set_name("Random 0-1 linear problem");
  for (int i = 0; i < num_variables; ++i) {
    sat::IntegerVariableProto* var = result.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int i = 0; i < num_constraints; ++i) {
    // Sum >= num_variables / 10.
    auto* ct = result.add_constraints()->mutable_linear();
    const int min_value = num_variables / 10;
    ct->add_domain(min_value);
    ct->add_domain(std::numeric_limits<int64_t>::max());
    for (int v = 0; v < num_variables; ++v) {
      if (absl::Bernoulli(random, 0.5) ||
          // To ensure that the constraint is feasible, we enforce that it has
          // at least the 'minimum' number of terms. This clause should only
          // rarely be used, when num_variables is high.
          num_variables - v <= min_value - ct->vars_size()) {
        ct->add_vars(v);
        ct->add_coeffs(1);
      }
    }
  }

  // Objective: minimize variables at one.
  {
    const int objective_var_index = result.variables_size();
    {
      sat::IntegerVariableProto* var = result.add_variables();
      var->add_domain(0);
      var->add_domain(num_variables);
    }
    result.mutable_objective()->add_vars(objective_var_index);
    result.mutable_objective()->add_coeffs(1);

    // Sum of all other variables == 0
    auto* ct = result.add_constraints()->mutable_linear();
    ct->add_domain(0);
    ct->add_domain(0);
    for (int v = 0; v < num_variables; ++v) {
      ct->add_vars(v);
      ct->add_coeffs(1);
    }
    ct->add_vars(objective_var_index);
    ct->add_coeffs(-1);
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
