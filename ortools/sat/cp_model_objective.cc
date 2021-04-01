// Copyright 2010-2021 Google LLC
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

#include <cstdint>

#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

void EncodeObjectiveAsSingleVariable(CpModelProto* cp_model) {
  if (!cp_model->has_objective()) return;

  if (cp_model->objective().vars_size() == 1) {
    // Canonicalize the objective to make it easier on us by always making the
    // coefficient equal to 1.0.
    const int old_ref = cp_model->objective().vars(0);
    const int64_t old_coeff = cp_model->objective().coeffs(0);
    const double muliplier = static_cast<double>(std::abs(old_coeff));
    if (old_coeff < 0) {
      cp_model->mutable_objective()->set_vars(0, NegatedRef(old_ref));
    }
    if (muliplier != 1.0) {
      // TODO(user): deal with this case.
      CHECK(cp_model->objective().domain().empty());

      double old_factor = cp_model->objective().scaling_factor();
      if (old_factor == 0.0) old_factor = 1.0;
      const double old_offset = cp_model->objective().offset();
      cp_model->mutable_objective()->set_scaling_factor(old_factor * muliplier);
      cp_model->mutable_objective()->set_offset(old_offset / muliplier);
    }
    cp_model->mutable_objective()->set_coeffs(0, 1.0);
    return;
  }

  // Compute trivial bounds on the objective, this is needed otherwise the
  // overflow checker might not be happy with the new constraint we are about
  // to create. Note that the model validator should make sure that there is no
  // overflow in the computation below.
  int64_t min_obj = 0;
  int64_t max_obj = 0;
  for (int i = 0; i < cp_model->objective().vars_size(); ++i) {
    const int ref = cp_model->objective().vars(i);
    const int var = PositiveRef(ref);
    const int64_t coeff =
        cp_model->objective().coeffs(i) * (RefIsPositive(ref) ? 1 : -1);
    const int64_t value1 = cp_model->variables(var).domain(0) * coeff;
    const int64_t value2 = cp_model->variables(var).domain(
                               cp_model->variables(var).domain_size() - 1) *
                           coeff;
    min_obj += std::min(value1, value2);
    max_obj += std::max(value1, value2);
  }

  // Create the new objective var.
  const int obj_ref = cp_model->variables_size();
  {
    IntegerVariableProto* obj = cp_model->add_variables();
    Domain obj_domain(min_obj, max_obj);
    if (!cp_model->objective().domain().empty()) {
      obj_domain = obj_domain.IntersectionWith(
          ReadDomainFromProto(cp_model->objective()));
    }
    FillDomainInProto(obj_domain, obj);
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
  cp_model->mutable_objective()->clear_domain();
}

}  // namespace sat
}  // namespace operations_research
