// Copyright 2010-2022 Google LLC
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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"
#ifndef OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_RESULT_H_
#define OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_RESULT_H_

#include <ostream>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// Represents a subset of the constraints (including variable bounds and
// integrality) of a `Model`.
//
// The fields contain `Variable` and Constraint objects which retain pointers
//  back to their associated `Model`. Therefore, a `ModelSubset` should not
// outlive the `Model` it is in reference to.
struct ModelSubset {
  struct Bounds {
    static Bounds FromProto(const ModelSubsetProto::Bounds& bounds_proto);
    ModelSubsetProto::Bounds Proto() const;

    bool empty() const { return !lower && !upper; }

    friend bool operator==(const Bounds& lhs, const Bounds& rhs) {
      return lhs.lower == rhs.lower && lhs.upper == rhs.upper;
    }
    friend bool operator!=(const Bounds& lhs, const Bounds& rhs) {
      return !(lhs == rhs);
    }

    bool lower = false;
    bool upper = false;
  };

  // Returns the `ModelSubset` equivalent to `proto`.
  //
  // Returns an error when `model` does not contain a variable or constraint
  // associated with an index present in `proto`.
  static absl::StatusOr<ModelSubset> FromProto(const ModelStorage* model,
                                               const ModelSubsetProto& proto);

  // Returns the proto equivalent of this object.
  //
  // The caller should use CheckModelStorage() as this function does not check
  // internal consistency of the referenced variables and constraints.
  ModelSubsetProto Proto() const;

  // Returns a failure if the `Variable` and Constraints contained in the fields
  // do not belong to the input expected_storage (which must not be nullptr).
  absl::Status CheckModelStorage(const ModelStorage* expected_storage) const;

  // True if this object corresponds to the empty subset.
  bool empty() const;

  // Returns a detailed string description of the contents of the model subset.
  // (not the component names, use `<<` for that instead).
  std::string ToString() const;

  absl::flat_hash_map<Variable, Bounds> variable_bounds;
  absl::flat_hash_set<Variable> variable_integrality;
  absl::flat_hash_map<LinearConstraint, Bounds> linear_constraints;
  absl::flat_hash_map<QuadraticConstraint, Bounds> quadratic_constraints;
  absl::flat_hash_set<SecondOrderConeConstraint> second_order_cone_constraints;
  absl::flat_hash_set<Sos1Constraint> sos1_constraints;
  absl::flat_hash_set<Sos2Constraint> sos2_constraints;
  absl::flat_hash_set<IndicatorConstraint> indicator_constraints;
};

std::ostream& operator<<(std::ostream& out, const ModelSubset::Bounds& bounds);
std::ostream& operator<<(std::ostream& out, const ModelSubset& model_subset);

struct ComputeInfeasibleSubsystemResult {
  // Returns the `ComputeInfeasibleSubsystemResult` equivalent to `proto`.
  //
  // Returns an error when:
  //   * `model` does not contain a variable or constraint associated with an
  //     index present in `proto.infeasible_subsystem`.
  //   * ValidateComputeInfeasibleSubsystemResultNoModel(result_proto) fails.
  static absl::StatusOr<ComputeInfeasibleSubsystemResult> FromProto(
      const ModelStorage* model,
      const ComputeInfeasibleSubsystemResultProto& result_proto);

  // Returns the proto equivalent of this object.
  //
  // The caller should use CheckModelStorage() before calling this function as
  // it does not check internal consistency of the referenced variables and
  // constraints.
  ComputeInfeasibleSubsystemResultProto Proto() const;

  // Returns a failure if this object contains references to a model other than
  // `expected_storage` (which must not be nullptr).
  absl::Status CheckModelStorage(const ModelStorage* expected_storage) const;

  // The primal feasibility status of the model, as determined by the solver.
  FeasibilityStatus feasibility = FeasibilityStatus::kUndetermined;

  // An infeasible subsystem of the input model. Set if `feasible` is
  // kInfeasible, and empty otherwise. The IDs correspond to those constraints
  // included in the infeasible subsystem. Submessages with `Bounds` values
  // indicate which side of a potentially ranged constraint are included in the
  // subsystem: lower bound, upper bound, or both.
  ModelSubset infeasible_subsystem;

  // True if the solver has certified that the returned subsystem is minimal
  // (the instance is feasible if any additional constraint is removed). Note
  // that, due to problem transformations MathOpt applies or idiosyncrasies of
  // the solvers contract, the returned infeasible subsystem may not actually be
  // minimal.
  bool is_minimal = false;
};

std::ostream& operator<<(std::ostream& out,
                         const ComputeInfeasibleSubsystemResult& result);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_RESULT_H_
