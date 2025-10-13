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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/xpress/g_xpress.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/third_party_solvers/xpress_environment.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {

// Interface to FICO XPRESS solver
// Largely inspired by the Gurobi interface
class XpressSolver : public SolverInterface {
 public:
  // Creates the XPRESS solver and loads the model into it
  static absl::StatusOr<std::unique_ptr<XpressSolver>> New(
      const ModelProto& input_model,
      const SolverInterface::InitArgs& init_args);

  // Solves the optimization problem
  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      const SolveInterrupter* interrupter) override;

  // Updates the problem (not implemented yet)
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;

  // Computes the infeasible subsystem (not implemented yet)
  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                             MessageCallback message_cb,
                             const SolveInterrupter* interrupter) override;

 private:
  explicit XpressSolver(std::unique_ptr<Xpress> g_xpress);

  // For easing reading the code, we declare these types:
  using VarId = int64_t;
  using AuxiliaryObjectiveId = int64_t;
  using LinearConstraintId = int64_t;
  using QuadraticConstraintId = int64_t;
  using SecondOrderConeConstraintId = int64_t;
  using Sos1ConstraintId = int64_t;
  using Sos2ConstraintId = int64_t;
  using IndicatorConstraintId = int64_t;
  using AnyConstraintId = int64_t;
  using XpressVariableIndex = int;
  using XpressMultiObjectiveIndex = int;
  using XpressLinearConstraintIndex = int;
  using XpressQuadraticConstraintIndex = int;
  using XpressSosConstraintIndex = int;
  using XpressGeneralConstraintIndex = int;
  using XpressAnyConstraintIndex = int;

  static constexpr XpressVariableIndex kUnspecifiedIndex = -1;
  static constexpr XpressAnyConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kPlusInf = XPRS_PLUSINFINITY;
  static constexpr double kMinusInf = XPRS_MINUSINFINITY;

  static bool isFinite(double value) {
    return value < kPlusInf && value > kMinusInf;
  }

  // Data associated with each linear constraint
  struct LinearConstraintData {
    XpressLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    double lower_bound = kMinusInf;
    double upper_bound = kPlusInf;
  };

  absl::StatusOr<SolveResultProto> ExtractSolveResultProto(
      absl::Time start, const ModelSolveParametersProto& model_parameters,
      const SolveParametersProto& solve_parameters);
  absl::StatusOr<SolutionProto> GetSolution(
      const ModelSolveParametersProto& model_parameters,
      const SolveParametersProto& solve_parameters);
  absl::StatusOr<SolveStatsProto> GetSolveStats(absl::Time start) const;

  absl::StatusOr<double> GetBestPrimalBound() const;
  absl::StatusOr<double> GetBestDualBound() const;

  absl::StatusOr<TerminationProto> ConvertTerminationReason(
      double best_primal_bound, double best_dual_bound) const;

  absl::StatusOr<SolutionProto> GetLpSolution(
      const ModelSolveParametersProto& model_parameters,
      const SolveParametersProto& solve_parameters);
  bool isPrimalFeasible() const;
  bool isDualFeasible() const;

  absl::StatusOr<std::optional<BasisProto>> GetBasisIfAvailable(
      const SolveParametersProto& parameters);

  absl::Status AddNewLinearConstraints(
      const LinearConstraintsProto& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddSingleObjective(const ObjectiveProto& objective);
  absl::Status ChangeCoefficients(const SparseDoubleMatrixProto& matrix);

  absl::Status LoadModel(const ModelProto& input_model);

  std::string GetLpOptimizationFlags(const SolveParametersProto& parameters);
  absl::Status CallXpressSolve(const SolveParametersProto& parameters);

  // Fills in result with the values in xpress_values aided by the index
  // conversion from map which should be either variables_map_ or
  // linear_constraints_map_ as appropriate. Only key/value pairs that passes
  // the filter predicate are added.
  template <typename T>
  void XpressVectorToSparseDoubleVector(
      absl::Span<const double> xpress_values, const T& map,
      SparseDoubleVectorProto& result,
      const SparseVectorFilterProto& filter) const;

  const std::unique_ptr<Xpress> xpress_;

  // Internal correspondence from variable proto IDs to Xpress-numbered
  // variables.
  gtl::linked_hash_map<VarId, XpressVariableIndex> variables_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Xpress-numbered linear constraint and extra information.
  gtl::linked_hash_map<LinearConstraintId, LinearConstraintData>
      linear_constraints_map_;

  int get_model_index(XpressVariableIndex index) const { return index; }
  int get_model_index(const LinearConstraintData& index) const {
    return index.constraint_index;
  }
  SolutionStatusProto getLpSolutionStatus() const;
  SolutionStatusProto getDualSolutionStatus() const;
  absl::StatusOr<InvertedBounds> ListInvertedBounds() const;
  absl::Status SetXpressStartingBasis(const BasisProto& basis);
  absl::Status SetLpIterLimits(const SolveParametersProto& parameters);

  bool is_mip_ = false;
  bool is_maximize_ = false;

  struct LpStatus {
    int primal_status = 0;
    int dual_status = 0;
  };
  LpStatus xpress_lp_status_;
  LPAlgorithmProto lp_algorithm_ = LP_ALGORITHM_UNSPECIFIED;

  int xpress_mip_status_ = 0;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
