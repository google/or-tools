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

#include "ortools/constraint_solver/sat_constraint.h"

namespace operations_research {

void SatTableConstraint::Post() {
  BooleanVariableManager* manager = sat_constraint_.VariableManager();
  sat::SatSolver* sat_solver = sat_constraint_.SatSolver();

  // First register the variable.
  DCHECK_EQ(vars_.size(), tuples_.Arity());
  std::vector<int> reg_indices;
  for (IntVar* int_var : vars_) {
    reg_indices.push_back(manager->RegisterIntVar(int_var));
  }

  // Then create an extra BooleanVariable per tuple.
  const sat::BooleanVariable first_tuple_var(sat_solver->NumVariables());
  sat_solver->SetNumVariables(sat_solver->NumVariables() + tuples_.NumTuples());

  std::vector<sat::Literal> clause;
  std::vector<std::pair<int64, int>> column_values;
  for (int i = 0; i < tuples_.Arity(); ++i) {
    column_values.clear();
    IntVarLiteralGetter literal_getter =
        manager->AssociatedBooleanVariables(reg_indices[i]);
    for (int tuple_index = 0; tuple_index < tuples_.NumTuples();
         ++tuple_index) {
      // Add the implications not(int_var == value) => not(tuple_var).
      clause.clear();
      clause.push_back(sat::Literal(first_tuple_var + tuple_index, false));
      clause.push_back(literal_getter.IsEqualTo(tuples_.Value(tuple_index, i)));
      column_values.push_back(
          std::make_pair(tuples_.Value(tuple_index, i), tuple_index));
      sat_solver->AddProblemClause(clause);
    }

    // We need to process all the tuple with the same value for the current
    // variable, so we sort them.
    clause.clear();
    std::sort(column_values.begin(), column_values.end());

    // Loop over all the current variable value.
    const IntVar* int_var = vars_[i];
    int column_index = 0;
    for (int value = int_var->Min(); value <= int_var->Max(); ++value) {
      // It is possible that the tuples contains out of range value, so we
      // remove them.
      while (column_index < column_values.size() &&
             column_values[column_index].first < value) {
        ++column_index;
      }

      // If a value doesn't appear, then we can fix a Boolean variable to false.
      if (column_index >= column_values.size() ||
          value != column_values[column_index].first) {
        sat_solver->AddUnitClause(literal_getter.IsNotEqualTo(value));
        continue;
      }

      // Otherwise, we create a clause to indicates that we can't have this
      // value if all the tuple containing it are false.
      clause.clear();
      clause.push_back(literal_getter.IsNotEqualTo(value));
      for (; column_index < column_values.size(); ++column_index) {
        const std::pair<int64, int>& entry = column_values[column_index];
        if (entry.first != value) break;
        clause.push_back(sat::Literal(first_tuple_var + entry.second, true));
      }
      sat_solver->AddProblemClause(clause);
    }
  }

  sat_constraint_.Post();
}

int BooleanVariableManager::RegisterIntVar(IntVar* int_var) {
  const int reg_index = LookupOrInsert(&registration_index_map_, int_var,
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
