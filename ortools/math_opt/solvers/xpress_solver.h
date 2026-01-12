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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
#define ORTOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_

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
  explicit XpressSolver(std::unique_ptr<Xpress> g_xpress, bool extract_names);

 public:
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

 private:
  static constexpr XpressVariableIndex kUnspecifiedIndex = -1;
  static constexpr XpressAnyConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kPlusInf = XPRS_PLUSINFINITY;
  static constexpr double kMinusInf = XPRS_MINUSINFINITY;

  static bool isFinite(double value) {
    return value < kPlusInf && value > kMinusInf;
  }

  // Data associated with each linear constraint
 public:
  struct LinearConstraintData {
    XpressLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    double lower_bound = kMinusInf;
    double upper_bound = kPlusInf;
  };

 private:
  absl::StatusOr<SolveResultProto> ExtractSolveResultProto(
      absl::Time start, const ModelSolveParametersProto& model_parameters,
      const SolveParametersProto& solve_parameters);
  absl::Status ExtendWithMultiobj(SolutionProto& solution);
  absl::Status AppendSolution(SolveResultProto& solve_result,
                              const ModelSolveParametersProto& model_parameters,
                              const SolveParametersProto& solve_parameters);
  absl::StatusOr<SolveStatsProto> GetSolveStats(absl::Time start) const;

  absl::StatusOr<double> GetBestPrimalBound() const;
  absl::StatusOr<double> GetBestDualBound() const;

  absl::StatusOr<TerminationProto> ConvertTerminationReason(
      double best_primal_bound, double best_dual_bound) const;

  bool isPrimalFeasible() const;
  bool isDualFeasible() const;

  void ExtractBounds(double lb, double ub, char& sense, double& rhs,
                     double& rng);
  void ExtractLinear(SparseDoubleVectorProto const& expr,
                     std::vector<int>& colind, std::vector<double>& coef);
  void ExtractQuadratic(QuadraticConstraintProto const& expr,
                        std::vector<int>& lin_colind,
                        std::vector<double>& lin_coef,
                        std::vector<int>& quad_col1,
                        std::vector<int>& quad_col2,
                        std::vector<double>& quad_coef);

  absl::StatusOr<std::optional<BasisProto>> GetBasisIfAvailable(
      const SolveParametersProto& parameters);

  absl::Status AddNewLinearConstraints(
      const LinearConstraintsProto& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddObjective(const ObjectiveProto& objective,
                            std::optional<AuxiliaryObjectiveId> objective_id,
                            bool multiobj);
  absl::Status AddSOS(
      const google::protobuf::Map<AnyConstraintId, SosConstraintProto>& sets,
      bool sos1);
  absl::Status AddIndicators(
      const google::protobuf::Map<IndicatorConstraintId,
                                  IndicatorConstraintProto>& indicators);
  absl::Status AddQuadraticConstraints(
      const google::protobuf::Map<QuadraticConstraintId,
                                  QuadraticConstraintProto>& constraints);
  absl::Status AddSecondOrderConeConstraints(
      const google::protobuf::Map<SecondOrderConeConstraintId,
                                  SecondOrderConeConstraintProto>& constraints);
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
  bool const extract_names_;

  // Internal correspondence from variable proto IDs to Xpress-numbered
  // variables.
  gtl::linked_hash_map<VarId, XpressVariableIndex> variables_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Xpress-numbered linear constraint and extra information.
  gtl::linked_hash_map<LinearConstraintId, LinearConstraintData>
      linear_constraints_map_;
  // Internal correspondence from objective proto IDs to Xpress-numbered
  // objectives.
  gtl::linked_hash_map<AuxiliaryObjectiveId, XpressMultiObjectiveIndex>
      objectives_map_;
  // Internal correspondence from SOS1 proto IDs to Xpress-numbered
  // SOS1 constraints.
  gtl::linked_hash_map<Sos1ConstraintId, XpressSosConstraintIndex> sos1_map_;
  // Internal correspondence from SOS2 proto IDs to Xpress-numbered
  // SOS2 constraints.
  gtl::linked_hash_map<Sos2ConstraintId, XpressSosConstraintIndex> sos2_map_;
  // Internal correspondence from indicator proto IDs to Xpress-numbered
  // indicators.
  gtl::linked_hash_map<IndicatorConstraintId, LinearConstraintData>
      indicator_map_;
  // Internal correspondence from quadratic proto IDs to Xpress-numbered
  // rows.
  gtl::linked_hash_map<QuadraticConstraintId, LinearConstraintData>
      quad_constraints_map_;
  // Internal correspondence from second order cone constraint proto IDs to
  // Xpress-numbered rows.
  gtl::linked_hash_map<QuadraticConstraintId, LinearConstraintData> soc_map_;

  int get_model_index(XpressVariableIndex index) const { return index; }
  int get_model_index(const LinearConstraintData& index) const {
    return index.constraint_index;
  }
  SolutionStatusProto getPrimalSolutionStatus() const;
  SolutionStatusProto getDualSolutionStatus() const;
  absl::StatusOr<InvertedBounds> ListInvertedBounds() const;

  /** Whether to force an XPRSpostsolve() after solving. */
  bool force_postsolve_ = false;
  /** Stop immediately after the initial LP in MIPs. */
  bool stop_after_lp_ = false;
  /** Whether the model has a non-binary indicator variable.
   * The behavior expected by ortools is that
   * - we can happily create a model with non-binary indicators
   * - this must fail at _solve_ time
   * Xpress implicitly converts indicator variables to binaries, though,
   * so we must keep track of this fact at build time and raise an error
   * only at solve time.
   */
  bool nonbinary_indicator_ = false;
  bool is_multiobj_ = false;
  bool is_mip_ = false;
  // Results of the last solve
  int primal_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  int dual_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  // Information queried right after a solve and stored for solution reporting
  int solvestatus_ = XPRS_SOLVESTATUS_UNSTARTED;
  int solstatus_ = XPRS_SOLSTATUS_NOTFOUND;
  int algorithm_ = XPRS_ALG_DEFAULT;
  int optimizetypeused_ = -1;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVERS_XPRESS_SOLVER_H_
