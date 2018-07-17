// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void ReifiedSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  auto add_reified_bool_and = [&cp_model](const std::vector<int>& literals,
                                          const int literal) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(literal);
    for (const int lit : literals) {
      ct->mutable_bool_and()->add_literals(lit);
    }
  };

  const int x = new_boolean_variable();
  const int y = new_boolean_variable();
  const int b = new_boolean_variable();

  // First version using a half-reified bool and.
  add_reified_bool_and({x, NegatedRef(y)}, b);

  // Second version using bool or.
  add_bool_or({NegatedRef(b), x});
  add_bool_or({NegatedRef(b), NegatedRef(y)});
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ReifiedSample();

  return EXIT_SUCCESS;
}
