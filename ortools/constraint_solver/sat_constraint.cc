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

#include "ortools/constraint_solver/sat_constraint.h"

namespace operations_research {

int BooleanVariableManager::RegisterIntVar(IntVar* int_var) {
  const int reg_index = gtl::LookupOrInsert(&registration_index_map_, int_var,
                                            registered_int_vars_.size());
  if (reg_index < registered_int_vars_.size()) return reg_index;
  registered_int_vars_.push_back(int_var);

  const int num_variables(solver_->NumVariables());
  IntVarLiteralGetter literal_getter(sat::BooleanVariable(num_variables),
                                     int_var->Min(), int_var->Max());
  associated_variables_.push_back(literal_getter);
  solver_->SetNumVariables(num_variables + literal_getter.NumVariableUsed());

  // Note that we want to be robust to the case where new variables where
  // created in the solver using other means than this class.
  variable_meaning_.resize(num_variables, std::make_pair(nullptr, 0));

  // Fill variable_meaning_ and add the "at most one value constraint".
  std::vector<sat::LiteralWithCoeff> cst;
  int64 value = int_var->Min();
  for (int i = 0; i < literal_getter.NumVariableUsed(); ++i) {
    variable_meaning_.push_back(std::make_pair(int_var, value));
    cst.push_back(sat::LiteralWithCoeff(literal_getter.IsEqualTo(value),
                                        sat::Coefficient(1)));
    ++value;
  }
  CHECK(solver_->AddLinearConstraint(false, sat::Coefficient(0), true,
                                     sat::Coefficient(1), &cst));
  return reg_index;
}

}  // namespace operations_research
