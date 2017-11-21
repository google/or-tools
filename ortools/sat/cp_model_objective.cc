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

#include "ortools/sat/cp_model_objective.h"

#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

void EncodeObjectiveAsSingleVariable(CpModelProto* cp_model) {
  if (!cp_model->has_objective()) return;
  if (cp_model->objective().vars_size() == 1) {
    // Canonicalize the objective to make it easier on us by always making the
    // coefficient equal to 1.0.
    const int old_ref = cp_model->objective().vars(0);
    const int64 old_coeff = cp_model->objective().coeffs(0);
    const double muliplier = static_cast<double>(std::abs(old_coeff));
    if (old_coeff < 0) {
      cp_model->mutable_objective()->set_vars(0, NegatedRef(old_ref));
    }
    if (muliplier != 1.0) {
      double old_factor = cp_model->objective().scaling_factor();
      if (old_factor == 0.0) old_factor = 1.0;
      const double old_offset = cp_model->objective().offset();
      cp_model->mutable_objective()->set_scaling_factor(old_factor * muliplier);
      cp_model->mutable_objective()->set_offset(old_offset / muliplier);
    }
    cp_model->mutable_objective()->set_coeffs(0, 1.0);
    return;
  }

  // Create the new objective var.
  const int obj_ref = cp_model->variables_size();
  {
    IntegerVariableProto* obj = cp_model->add_variables();
    if (cp_model->objective().domain().empty()) {
      obj->add_domain(kint64min);
      obj->add_domain(kint64max);
    } else {
      *(obj->mutable_domain()) = cp_model->objective().domain();
      cp_model->mutable_objective()->clear_domain();
    }
  }

  // Add the linear constraint.
  LinearConstraintProto* ct = cp_model->add_constraints()->mutable_linear();
  ct->add_domain(0);
  ct->add_domain(0);
  *(ct->mutable_vars()) = cp_model->objective().vars();
  *(ct->mutable_coeffs()) = cp_model->objective().coeffs();
  ct->add_vars(obj_ref);
  ct->add_coeffs(-1);

  // Update the objective.
  cp_model->mutable_objective()->clear_vars();
  cp_model->mutable_objective()->clear_coeffs();
  cp_model->mutable_objective()->add_vars(obj_ref);
  cp_model->mutable_objective()->add_coeffs(1);
}

}  // namespace sat
}  // namespace operations_research
