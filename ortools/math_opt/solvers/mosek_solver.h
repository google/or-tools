// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_MOSEK_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_MOSEK_SOLVER_H_

#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mosek/mosekwrp.h"
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

class MosekSolver : public SolverInterface {
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
  Mosek msk;
  absl::flat_hash_map<int64_t, Mosek::VariableIndex> variable_map;
  absl::flat_hash_map<int64_t, Mosek::ConstraintIndex> linconstr_map;
  absl::flat_hash_map<int64_t, Mosek::ConeConstraintIndex> coneconstr_map;
  absl::flat_hash_map<int64_t, Mosek::DisjunctiveConstraintIndex> indconstr_map;

  absl::Status ReplaceObjective(const ObjectiveProto& obj);
  absl::Status AddVariables(const VariablesProto& vars);
  absl::Status AddConstraints(const LinearConstraintsProto& cons);
  absl::Status AddConstraints(const LinearConstraintsProto& cons,
                              const SparseDoubleMatrixProto& lincofupds);
  absl::Status AddIndicatorConstraints(
      const ::google::protobuf::Map<int64_t, IndicatorConstraintProto>& cons);
  absl::Status AddConicConstraints(
      const ::google::protobuf::Map<int64_t, SecondOrderConeConstraintProto>&
          cons);

  absl::Status UpdateVariables(const VariableUpdatesProto& varupds);
  absl::Status UpdateConstraints(const LinearConstraintUpdatesProto& conupds,
                                 const SparseDoubleMatrixProto& lincofupds);
  absl::Status UpdateObjective(const ObjectiveUpdatesProto& objupds);
  absl::Status UpdateConstraint(
      const SecondOrderConeConstraintUpdatesProto& conupds);
  absl::Status UpdateConstraint(const IndicatorConstraintUpdatesProto& conupds);

  absl::StatusOr<PrimalSolutionProto> PrimalSolution(
      MSKsoltypee whichsol, const std::vector<int64_t>& ordered_xx_ids,
      bool skip_zeros);
  absl::StatusOr<DualSolutionProto> DualSolution(
      MSKsoltypee whichsol, const std::vector<int64_t>& ordered_y_ids,
      bool skip_y_zeros, const std::vector<int64_t>& ordered_yx_ids,
      bool skip_yx_zeros);
  absl::StatusOr<SolutionProto> Solution(
      MSKsoltypee whichsol, const std::vector<int64_t>& ordered_xc_ids,
      const std::vector<int64_t>& ordered_xx_ids, bool skip_xx_zeros,
      const std::vector<int64_t>& ordered_y_ids, bool skip_y_zeros,
      const std::vector<int64_t>& ordered_yx_ids, bool skip_yx_zeros);
  absl::StatusOr<PrimalRayProto> PrimalRay(
      MSKsoltypee whichsol, const std::vector<int64_t>& ordered_xx_ids,
      bool skip_zeros);
  absl::StatusOr<DualRayProto> DualRay(
      MSKsoltypee whichsol, const std::vector<int64_t>& ordered_y_ids,
      bool skip_y_zeros, const std::vector<int64_t>& ordered_yx_ids,
      bool skip_yx_zeros);

  MosekSolver(Mosek&& msk);
  MosekSolver(MosekSolver&) = delete;

#if 0
  struct SolutionClaims {
    bool mosek_returned_primal_feasible_solution = false;
    bool mosek_returned_dual_feasible_solution = false;
    bool mosek_returned_primal_ray = false;
    bool mosek_returned_dual_ray = false;
  };
  struct SolutionsAndClaims {
    std::vector<SolutionProto> solutions;
    SolutionClaims solution_claims;
  };


  absl::StatusOr<bool> PrimalRayReturned() const;
  absl::StatusOr<bool> DualRayReturned() const;

  // Will append solutions and rays to result_out. No other modifications are
  // made to result_out. Requires that mosek_->getInfo is validated.
  absl::StatusOr<SolutionsAndClaims> ExtractSolutionAndRays(
      const ModelSolveParametersProto& model_params);

  static absl::StatusOr<FeasibilityStatusProto> PrimalFeasibilityStatus(
      SolutionClaims solution_claims);
  static absl::StatusOr<FeasibilityStatusProto> DualFeasibilityStatus(
      const MosekInfo& mosek_info, bool is_integer,
      SolutionClaims solution_claims);
  static absl::StatusOr<TerminationProto> MakeTermination(
      MosekModelStatus mosek_model_status, const MosekInfo& mosek_info,
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
  // MosekModelStatus::kModelEmpty. This happens on models that have no
  // variables, but may still have (potentially infeasible) linear constraints
  // and an objective offset.
  //
  // Assumes that there are no inverted linear constraint bounds.
  static SolveResultProto ResultForMosekModelStatusModelEmpty(
      bool is_maximize, double objective_offset,
      const absl::flat_hash_map<int64_t, IndexAndBound>& lin_con_data);

  InvertedBounds ListInvertedBounds();

  std::unique_ptr<Mosek> mosek_;

  // Key is the mathopt id, value.index is the variable index in HiGHS.
  absl::flat_hash_map<int64_t, IndexAndBound> variable_data_;

  // Key is the mathopt id, value.index is the linear constraint index in HiGHS.
  absl::flat_hash_map<int64_t, IndexAndBound> lin_con_data_;
#endif
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_MOSEK_SOLVER_H_
