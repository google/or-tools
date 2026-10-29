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

#ifndef ORTOOLS_SAT_MP_MODEL_WRAPPER_H_
#define ORTOOLS_SAT_MP_MODEL_WRAPPER_H_

#include <stdint.h>

#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

class MPModelProtoWrapper;

// This wrapper expose a single type of constraint:
//      enforcement => linear_constraint
// Where enforcement is a conjunction of integer literals, each of the form
//    integer_variable == value.
//
// Having just a single type should simplify the presolve code. And extracting
// enforcement helps for both presolve algorithm and the final scaling to
// integer.
//
// Note that currently we really only handle either value == var_lb or value ==
// var_ub, or value outside the bounds (enforcement trivially false). We could
// handle more general enforcement, but then we cannot easily encode them via
// big-M and would need to create extra Booleans when converting to a
// CpModelProto.
class EnforcedLinearConstraint {
 public:
  // Accessors.
  int64_t num_terms() const { return constraint_->var_index().size(); }
  int vars(int i) const { return constraint_->var_index(i); }
  absl::Span<const int> vars() const {
    return absl::MakeConstSpan(constraint_->var_index());
  }

  // Since this is only constructed out of a validated MPModelProto. All
  // coefficients are guaranteed be finite and with magnitude <= to the
  // mip_max_valid_magnitude() parameter.
  double coeffs(int i) const { return constraint_->coefficient(i); }
  absl::Span<const double> coeffs() const {
    return absl::MakeConstSpan(constraint_->coefficient());
  }

  // Note that the bounds could be +/- infinity() to mean unconstrained in one
  // direction. We usually relax large enough bound to this.
  double lower_bound() const { return constraint_->lower_bound(); }
  double upper_bound() const { return constraint_->upper_bound(); }

  absl::string_view name() const { return constraint_->name(); }
  const MPConstraintProto& proto() const { return *constraint_; }

  // Mutable API is kept minimal and proto like.
  void set_vars(int i, int value) { constraint_->set_var_index(i, value); }
  void set_coeffs(int i, double value) {
    constraint_->set_coefficient(i, value);
  }
  void set_lower_bound(double value) { constraint_->set_lower_bound(value); }
  void set_upper_bound(double value) { constraint_->set_upper_bound(value); }
  void resize(int new_num_terms) {
    CHECK_LE(new_num_terms, num_terms());
    constraint_->mutable_var_index()->Truncate(new_num_terms);
    constraint_->mutable_coefficient()->Truncate(new_num_terms);
  }

  // And for the enforcement.
  bool IsEnforced() const { return !enforcement_vars_.empty(); }
  absl::Span<const int> enforcement_vars() const { return enforcement_vars_; }
  absl::Span<const int64_t> enforcement_values() const {
    return enforcement_values_;
  }
  void set_enforcement_values(int i, int64_t value) {
    enforcement_values_[i] = value;
  }

  // Note that currently this should only be called with value == current_var_lb
  // or value == current_var_ub.
  void AddEnforcement(int var, int64_t value) {
    enforcement_vars_.push_back(var);
    enforcement_values_.push_back(value);
  }

 private:
  friend MPModelProtoWrapper;
  EnforcedLinearConstraint(int c, MPModelProto* mp_model)
      : constraint_(mp_model->mutable_constraint(c)) {}

  // Enforcement.
  std::vector<int> enforcement_vars_;
  std::vector<int64_t> enforcement_values_;

  // The linear part is stored in the underlying MPModelProto constraint.
  MPConstraintProto* constraint_;
};

// A simple wrapper that see a MPModelProto as an uniform list of
// EnforcedLinearConstraint. The idea is to simplify presolve code and to
// eventually just use a new proto corresponding to this in all our presolve
// stack.
class MPModelProtoWrapper {
 public:
  // Note that the given MPModelProto must pass
  // MPModelProtoValidationBeforeConversion(). This might crash if this is not
  // the case.
  //
  // The wrapper will take ownership of the MPModelProto taken by value. So the
  // construction should be
  //   MPModelProtoWrapper wrapper(std::move(mp_model), logger);
  // in the usual case where we don't need the original MPModelProto anymore.
  // Note that the (slightly modified) input model is still accessible via
  // `mutable_underlying_model()`.
  explicit MPModelProtoWrapper(MPModelProto mp_model,
                               SolverLogger* logger = nullptr);

  absl::string_view name() const { return mp_model_.name(); }

  // Variable. Same API for now, with the objective embedded.
  int num_variables() const { return mp_model_.variable().size(); }
  const MPVariableProto& variables(int v) const {
    return mp_model_.variable(v);
  }
  MPVariableProto* mutable_variables(int v) {
    return mp_model_.mutable_variable(v);
  }

  // Constraints.
  int num_constraints() const { return constraints_.size(); }
  const EnforcedLinearConstraint& constraints(int c) const {
    return constraints_[c];
  }
  EnforcedLinearConstraint* mutable_constraints(int c) {
    return &constraints_[c];
  }

  // Returns whether the underlying model has a non-constant objective. The
  // possibly non-zero objective_offset() is not taken into account.
  bool has_non_constant_objective() const {
    return has_non_constant_objective_;
  }

  // Returns whether the original model was a maximization problem.
  bool is_original_model_maximize() const { return objective_scaling_ < 0.0; }

  // We change the objective to minimization. The "original" objective will
  // always be (inner_objective + objective_offset()) * objective_scaling().
  // If it was a maximization, we will have scaling set to -1.
  double objective_offset() const { return mp_model_.objective_offset(); }
  void set_objective_offset(double value) {
    mp_model_.set_objective_offset(value);
  }
  double objective_scaling() const { return objective_scaling_; }

  // WARNING: This do not see "enforcement" and the model should be manipulated
  // with care. It is forbidden to change the number of variable or constraint.
  MPModelProto* mutable_underlying_model() { return &mp_model_; }

 private:
  double objective_scaling_;
  bool has_non_constant_objective_;
  MPModelProto mp_model_;
  std::vector<EnforcedLinearConstraint> constraints_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_MP_MODEL_WRAPPER_H_
