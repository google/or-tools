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

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// Appends the enforcement literal `var == value` to `ct`. Currently this should
// only be called with `value` equal to a current bound of `var`.
inline void AddEnforcement(int var, int64_t value,
                           EnforcedLinearConstraintProto* ct) {
  ct->add_enforcement_vars(var);
  ct->add_enforcement_values(value);
}

// Truncates `ct`'s linear terms to `new_num_terms`.
inline void ResizeConstraintTerms(int new_num_terms,
                                  EnforcedLinearConstraintProto* ct) {
  CHECK_GE(new_num_terms, 0);
  CHECK_LE(new_num_terms, ct->vars().size());
  CHECK_EQ(ct->vars().size(), ct->coeffs().size());
  ct->mutable_vars()->Truncate(new_num_terms);
  ct->mutable_coeffs()->Truncate(new_num_terms);
}

// Resets `ct` to the always-true constraint `-inf <= 0 <= +inf` with no
// enforcement, preserving metadata fields such as `name` and `tolerance`.
inline void ClearConstraintToAlwaysTrue(EnforcedLinearConstraintProto* ct) {
  ct->set_lower_bound(-std::numeric_limits<double>::infinity());
  ct->set_upper_bound(std::numeric_limits<double>::infinity());
  ct->clear_vars();
  ct->clear_coeffs();
  ct->clear_enforcement_vars();
  ct->clear_enforcement_values();
}

// A thin wrapper around a MipModelProto converted from an MPModelProto. The
// idea is to simplify presolve code and to eventually just use MipModelProto in
// all our presolve stack.
//
// The objective is always a minimization: the "original" objective is
// (inner_objective + objective_offset()) * objective_scaling(), see
// MipModelProto.
class MPModelProtoWrapper {
 public:
  // Converts `mp_model` to a MipModelProto: the maximization objective (if
  // any) is negated, the general constraints become enforced linear
  // constraints, and the always true constraints are dropped.
  //
  // Note that the given MPModelProto must pass CanWrap(). This might crash if
  // this is not the case.
  //
  // The construction should be
  //   MPModelProtoWrapper wrapper(std::move(mp_model), logger);
  // in the usual case where we don't need the original MPModelProto anymore,
  // so that the memory of its constraints can be reused.
  explicit MPModelProtoWrapper(MPModelProto mp_model,
                               SolverLogger* logger = nullptr);

  // Wraps `model` as is. It must be a valid MipModelProto. Note that this is
  // not a constructor, since a constructor taking a MipModelProto would make
  // the construction from the result of ParseTestProto() ambiguous.
  static MPModelProtoWrapper FromMipModelProto(MipModelProto model);

  // Returns false (and logs the reason) if `mp_model` contains general
  // constraints that cannot be wrapped.
  static bool CanWrap(const MPModelProto& mp_model, SolverLogger* logger);

  absl::string_view name() const { return model_.name(); }

  // Variables, with the objective embedded.
  int num_variables() const { return model_.variables().size(); }
  const MipVariableProto& variables(int v) const { return model_.variables(v); }
  MipVariableProto* mutable_variables(int v) {
    return model_.mutable_variables(v);
  }

  // Constraints.
  int num_constraints() const { return model_.constraints().size(); }
  const EnforcedLinearConstraintProto& constraints(int c) const {
    return model_.constraints(c);
  }
  EnforcedLinearConstraintProto* mutable_constraints(int c) {
    return model_.mutable_constraints(c);
  }

  // Relaxes every constraint's bounds in `model_` outward by `tight_precision`
  // (which must be finite and non-negative), rounding inward so the relaxed
  // region never exceeds `tight_precision` and finite bounds stay finite even
  // at +/-DBL_MAX:
  // - Each lower bound `lb` becomes `ExactSumUb(lb, -tight_precision)`.
  // - Each upper bound `ub` becomes `ExactSumLb(ub, tight_precision)`.
  // Infinite bounds are left unchanged. This affects constraint bounds only
  // (including rows converted from `And`/`Or` general constraints); variable
  // bounds are untouched.
  void RelaxAllConstraintBounds(double tight_precision);

  // Sets `tolerance` on every constraint in `model_` (which must be finite and
  // non-negative), clearing the field when `tolerance == 0.0`. This applies
  // uniformly to all constraints (including rows converted from `And`/`Or`
  // general constraints); variable fields (including
  // `maximum_distance_to_integer`) are untouched.
  void SetAllConstraintToleranceBudget(double tolerance);

  // Appends an empty constraint to the underlying model and returns a pointer
  // to its wrapper. Must only be paired with `pop_constraint()` if the caller
  // removes it.
  EnforcedLinearConstraintProto* add_constraints() {
    return model_.add_constraints();
  }

  // Removes the last constraint (intended to undo `add_constraint()`).
  // CHECK-fails if the underlying model has no constraints.
  void pop_constraints() {
    CHECK(!model_.constraints().empty());
    model_.mutable_constraints()->RemoveLast();
  }

  // Returns whether the underlying model has a non-constant objective. The
  // possibly non-zero objective_offset() is not taken into account.
  bool has_non_constant_objective() const {
    return absl::c_any_of(model_.variables(), [](const MipVariableProto& v) {
      return v.objective_coefficient() != 0.0;
    });
  }

  // Returns whether the original model was a maximization problem.
  bool is_original_model_maximize() const {
    return model_.objective_scaling() < 0.0;
  }

  // The "original" objective is always
  // (inner_objective + objective_offset()) * objective_scaling(). If it was a
  // maximization, the scaling is -1.
  double objective_offset() const { return model_.objective_offset(); }
  void set_objective_offset(double value) {
    model_.set_objective_offset(value);
  }
  double objective_scaling() const { return model_.objective_scaling(); }

  // Upper bound on the absolute objective error incurred so far by the
  // presolve, in inner objective units, see MipModelProto. It is always finite
  // and non-negative.
  double objective_max_rounding_error() const {
    return model_.objective_max_rounding_error();
  }
  void set_objective_max_rounding_error(double value) {
    DCHECK(std::isfinite(value));
    DCHECK_GE(value, 0.0);
    model_.set_objective_max_rounding_error(value);
  }

  // Solution hint, in the same space as the variables.
  bool has_solution_hint() const { return model_.has_solution_hint(); }
  const MipPartialVariableAssignment& solution_hint() const {
    return model_.solution_hint();
  }
  MipPartialVariableAssignment* mutable_solution_hint() {
    return model_.mutable_solution_hint();
  }

  // The wrapped model.
  const MipModelProto& proto() const { return model_; }

 private:
  MPModelProtoWrapper() = default;

  MipModelProto model_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_MP_MODEL_WRAPPER_H_
