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

#include "ortools/lp_data/proto_utils.h"

namespace operations_research {
namespace glop {

// Converts a LinearProgram to a MPModelProto.
void LinearProgramToMPModelProto(const LinearProgram& input,
                                 MPModelProto* output) {
  output->Clear();
  output->set_name(input.name());
  output->set_maximize(input.IsMaximizationProblem());
  output->set_objective_offset(input.objective_offset());
  for (ColIndex col(0); col < input.num_variables(); ++col) {
    MPVariableProto* variable = output->add_variable();
    variable->set_lower_bound(input.variable_lower_bounds()[col]);
    variable->set_upper_bound(input.variable_upper_bounds()[col]);
    variable->set_name(input.GetVariableName(col));
    variable->set_is_integer(input.IsVariableInteger(col));
    variable->set_objective_coefficient(input.objective_coefficients()[col]);
  }
  // We need the matrix transpose because a LinearProgram stores the data
  // column-wise but the MPModelProto uses a row-wise format.
  SparseMatrix transpose;
  transpose.PopulateFromTranspose(input.GetSparseMatrix());
  for (RowIndex row(0); row < input.num_constraints(); ++row) {
    MPConstraintProto* constraint = output->add_constraint();
    constraint->set_lower_bound(input.constraint_lower_bounds()[row]);
    constraint->set_upper_bound(input.constraint_upper_bounds()[row]);
    constraint->set_name(input.GetConstraintName(row));
    for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
      constraint->add_var_index(e.row().value());
      constraint->add_coefficient(e.coefficient());
    }
  }
}

// Converts a MPModelProto to a LinearProgram.
void MPModelProtoToLinearProgram(const MPModelProto& input,
                                 LinearProgram* output) {
  output->Clear();
  output->SetName(input.name());
  output->SetMaximizationProblem(input.maximize());
  output->SetObjectiveOffset(input.objective_offset());
  // TODO(user,user): clean up loops to use natural range iteration.
  for (int i = 0; i < input.variable_size(); ++i) {
    const MPVariableProto& var = input.variable(i);
    const ColIndex col = output->CreateNewVariable();
    output->SetVariableName(col, var.name());
    output->SetVariableBounds(col, var.lower_bound(), var.upper_bound());
    output->SetObjectiveCoefficient(col, var.objective_coefficient());
    if (var.is_integer()) {
      output->SetVariableType(col, LinearProgram::VariableType::INTEGER);
    }
  }
  for (int j = 0; j < input.constraint_size(); ++j) {
    const MPConstraintProto& cst = input.constraint(j);
    const RowIndex row = output->CreateNewConstraint();
    output->SetConstraintName(row, cst.name());
    output->SetConstraintBounds(row, cst.lower_bound(), cst.upper_bound());
    // TODO(user,user,user): implement strong proto validation in the
    // linear solver server and re-use it here.
    CHECK_EQ(cst.var_index_size(), cst.coefficient_size());
    for (int k = 0; k < cst.var_index_size(); ++k) {
      output->SetCoefficient(row, ColIndex(cst.var_index(k)),
                             cst.coefficient(k));
    }
  }
}

}  // namespace glop
}  // namespace operations_research
