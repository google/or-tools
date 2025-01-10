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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_HIGHS_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_HIGHS_SOLVER_H_

#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "Highs.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {

class HighsSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const InitArgs& init_args);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      const SolveInterrupter* interrupter) override;
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;
  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                             MessageCallback message_cb,
                             const SolveInterrupter* interrupter) override;

 private:
  struct SolutionClaims {
    bool highs_returned_primal_feasible_solution = false;
    bool highs_returned_dual_feasible_solution = false;
    bool highs_returned_primal_ray = false;
    bool highs_returned_dual_ray = false;
  };
  struct SolutionsAndClaims {
    std::vector<SolutionProto> solutions;
    // TODO(b/271104776): add rays.
    SolutionClaims solution_claims;
  };

  // Tracks the upper and lower bounds for either a variable or linear
  // constraint in the HiGHS model.
  //
  // Note that HiGHS does not allow bounds to cross. If a bound would cross, it
  // is set to zero in the actual HiGHS model and its true values are tracked
  // here (they may uncross before solve time on a model update).
  struct IndexAndBound {
    // The position of the variable/linear constraint in the HiGHS model. Note
    // that this is distinct from the MathOpt id.
    int index;
    double lb;
    double ub;

    // TODO(b/271595607): we won't need to track this once a bug in HiGHS is
    // fixed. Always false for constraints.
    bool is_integer;

    bool bounds_cross() const { return lb > ub; }

    // If we don't round the bounds for integer variables, HiGHS can give
    // garbage results, see go/paste/4836036214521856 for details. See also
    // b/271595607.
    double rounded_lb() const { return is_integer ? std::ceil(lb) : lb; }
    double rounded_ub() const { return is_integer ? std::floor(ub) : ub; }
    bool rounded_bounds_cross() const { return rounded_lb() > rounded_ub(); }

    IndexAndBound(int index, double lb, double ub, bool is_integer)
        : index(index), lb(lb), ub(ub), is_integer(is_integer) {}
  };
  HighsSolver(std::unique_ptr<Highs> highs,
              absl::flat_hash_map<int64_t, IndexAndBound> variable_data,
              absl::flat_hash_map<int64_t, IndexAndBound> lin_con_data)
      : highs_(std::move(highs)),
        variable_data_(std::move(variable_data)),
        lin_con_data_(std::move(lin_con_data)) {}

  absl::StatusOr<bool> PrimalRayReturned() const;
  absl::StatusOr<bool> DualRayReturned() const;

  // Will append solutions and rays to result_out. No other modifications are
  // made to result_out. Requires that highs_->getInfo is validated.
  absl::StatusOr<SolutionsAndClaims> ExtractSolutionAndRays(
      const ModelSolveParametersProto& model_params);

  static absl::StatusOr<FeasibilityStatusProto> PrimalFeasibilityStatus(
      SolutionClaims solution_claims);
  static absl::StatusOr<FeasibilityStatusProto> DualFeasibilityStatus(
      const HighsInfo& highs_info, bool is_integer,
      SolutionClaims solution_claims);
  static absl::StatusOr<TerminationProto> MakeTermination(
      HighsModelStatus highs_model_status, const HighsInfo& highs_info,
      bool is_integer, bool had_node_limit, bool had_solution_limit,
      bool is_maximize, SolutionClaims solution_claims);

  // Returns the current basis if it is available and MathOpt can represent it
  // (all kNonBasic values can be made more precise, see b/272767311).
  absl::StatusOr<std::optional<BasisProto>> ExtractBasis();

  template <typename T>
  absl::Status EnsureOneEntryPerVariable(const std::vector<T>& vec) {
    if (vec.size() != variable_data_.size()) {
      return util::InvalidArgumentErrorBuilder()
             << "expected one entry per variable, but model had "
             << variable_data_.size() << " variables and found " << vec.size()
             << " elements";
    }
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status EnsureOneEntryPerLinearConstraint(const std::vector<T>& vec) {
    if (vec.size() != lin_con_data_.size()) {
      return util::InvalidArgumentErrorBuilder()
             << "expected one entry per linear constraint, but model had "
             << lin_con_data_.size() << " linear constraints and found "
             << vec.size() << " elements";
    }
    return absl::OkStatus();
  }

  // Returns a SolveResult for when HiGHS returns the model status
  // HighsModelStatus::kModelEmpty. This happens on models that have no
  // variables, but may still have (potentially infeasible) linear constraints
  // and an objective offset.
  //
  // Assumes that there are no inverted linear constraint bounds.
  static SolveResultProto ResultForHighsModelStatusModelEmpty(
      bool is_maximize, double objective_offset,
      const absl::flat_hash_map<int64_t, IndexAndBound>& lin_con_data);

  InvertedBounds ListInvertedBounds();

  std::unique_ptr<Highs> highs_;

  // Key is the mathopt id, value.index is the variable index in HiGHS.
  absl::flat_hash_map<int64_t, IndexAndBound> variable_data_;

  // Key is the mathopt id, value.index is the linear constraint index in HiGHS.
  absl::flat_hash_map<int64_t, IndexAndBound> lin_con_data_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_HIGHS_SOLVER_H_
