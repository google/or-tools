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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/math_opt/callback.pb.h"
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
  absl::Status CallXpressSolve(bool enableOutput);

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
  using VariableId = int64_t;
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
  // A collection of other constraints (e.g., norm, max, indicator) supported by
  // Xpress. All general constraints share the same index set. See for more
  // detail: https://www.xpress.com/documentation/9.5/refman/constraints.html.
  using XpressGeneralConstraintIndex = int;
  using XpressAnyConstraintIndex = int;

  static constexpr XpressVariableIndex kUnspecifiedIndex = -1;
  static constexpr XpressAnyConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kPlusInf = XPRS_PLUSINFINITY;
  static constexpr double kMinusInf = XPRS_MINUSINFINITY;

  // Data associated with each linear constraint. With it we know if the
  // underlying representation is either:
  //   linear_terms <= upper_bound (if lower bound <= -INFINITY)
  //   linear_terms >= lower_bound (if upper bound >= INFINTY)
  //   linear_terms == xxxxx_bound (if upper_bound == lower_bound)
  //   linear_term - slack == 0 (with slack bounds equal to xxxxx_bound)
  struct LinearConstraintData {
    XpressLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    // only valid for true ranged constraints.
    XpressVariableIndex slack_index = kUnspecifiedIndex;
    double lower_bound = kMinusInf;
    double upper_bound = kPlusInf;
  };

  struct SolutionClaims {
    bool primal_feasible_solution_exists;
    bool dual_feasible_solution_exists;
  };

  struct SolutionsAndClaims {
    std::vector<SolutionProto> solutions;
    SolutionClaims solution_claims;
  };

  template <typename SolutionType>
  struct SolutionAndClaim {
    std::optional<SolutionType> solution;
    bool feasible_solution_exists = false;
  };

  absl::StatusOr<SolveResultProto> ExtractSolveResultProto(
      absl::Time start, const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolutionsAndClaims> GetSolutions(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolveStatsProto> GetSolveStats(absl::Time start) const;

  absl::StatusOr<double> GetBestDualBound() const;
  absl::StatusOr<double> GetBestPrimalBound() const;

  absl::StatusOr<TerminationProto> ConvertTerminationReason(
      SolutionClaims solution_claims, double best_primal_bound,
      double best_dual_bound) const;

  // Returns solution information appropriate and available for an LP (linear
  // constraints + linear objective, only).
  absl::StatusOr<SolutionsAndClaims> GetLpSolution(
      const ModelSolveParametersProto& model_parameters);
  bool isFeasible() const;

  // return bool field should be true if a primal solution exists.
  absl::StatusOr<SolutionAndClaim<PrimalSolutionProto>>
  GetConvexPrimalSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters) const;
  absl::StatusOr<SolutionAndClaim<DualSolutionProto>>
  GetConvexDualSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters) const;
  absl::StatusOr<std::optional<BasisProto>> GetBasisIfAvailable();

  absl::Status AddNewLinearConstraints(
      const LinearConstraintsProto& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddSingleObjective(const ObjectiveProto& objective);
  absl::Status ChangeCoefficients(const SparseDoubleMatrixProto& matrix);

  absl::Status LoadModel(const ModelProto& input_model);

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

  // Note that we use linked_hash_map for the indices of the xpress_model_
  // variables and linear constraints to ensure that iteration over the map
  // maintains their insertion order (and, thus, the order in which they appear
  // in the model).

  // Internal correspondence from variable proto IDs to Xpress-numbered
  // variables.
  gtl::linked_hash_map<VariableId, XpressVariableIndex> variables_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Xpress-numbered linear constraint and extra information.
  gtl::linked_hash_map<LinearConstraintId, LinearConstraintData>
      linear_constraints_map_;

  int get_model_index(XpressVariableIndex index) const { return index; }
  int get_model_index(const LinearConstraintData& index) const {
    return index.constraint_index;
  }

  SolutionStatusProto getLpSolutionStatus() const;

  // Fields to track the number of Xpress variables and constraints. These
  // quantities are updated immediately after adding or removing to the model,
  // so it is correct even if XPRESS C API has not yet been called.

  // Number of Xpress variables.
  int num_xpress_variables_ = 0;
  // Number of Xpress linear constraints.
  int num_xpress_lin_cons_ = 0;
  // Number of Xpress quadratic constraints.
  int num_xpress_quad_cons_ = 0;
  // Number of Xpress SOS constraints.
  int num_xpress_sos_cons_ = 0;
  // Number of Xpress general constraints.
  int num_xpress_gen_cons_ = 0;

  bool is_mip_ = false;
  bool is_maximize_ = false;
  int xpress_status_ = 0;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
