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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/gurobi/environment.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/invalid_indicators.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"
#include "ortools/math_opt/solvers/gurobi_callback.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {

class GurobiSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<GurobiSolver>> New(
      const ModelProto& input_model,
      const SolverInterface::InitArgs& init_args);

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
  struct GurobiCallbackData {
    explicit GurobiCallbackData(GurobiCallbackInput callback_input,
                                SolveInterrupter* const local_interrupter)
        : callback_input(std::move(callback_input)),
          local_interrupter(local_interrupter) {}
    const GurobiCallbackInput callback_input;

    // Interrupter triggered when either the user interrupter passed to Solve()
    // is triggered or after one user callback returned a true `terminate`.
    //
    // This is not the user interrupter though so it safe for callbacks to
    // trigger it.
    //
    // It is optional; it is not null when either we have a LP/MIP callback or a
    // user interrupter. But it can be null if we only have a message callback.
    SolveInterrupter* const local_interrupter;

    MessageCallbackData message_callback_data;

    absl::Status status = absl::OkStatus();
  };

  explicit GurobiSolver(std::unique_ptr<Gurobi> g_gurobi);

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
  using GurobiVariableIndex = int;
  using GurobiMultiObjectiveIndex = int;
  using GurobiLinearConstraintIndex = int;
  using GurobiQuadraticConstraintIndex = int;
  using GurobiSosConstraintIndex = int;
  // A collection of other constraints (e.g., norm, max, indicator) supported by
  // Gurobi. All general constraints share the same index set. See for more
  // detail: https://www.gurobi.com/documentation/9.5/refman/constraints.html.
  using GurobiGeneralConstraintIndex = int;
  using GurobiAnyConstraintIndex = int;

  static constexpr GurobiVariableIndex kUnspecifiedIndex = -1;
  static constexpr GurobiAnyConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kInf = std::numeric_limits<double>::infinity();

  struct GurobiModelElements {
    std::vector<GurobiVariableIndex> variables;
    std::vector<GurobiLinearConstraintIndex> linear_constraints;
    std::vector<GurobiQuadraticConstraintIndex> quadratic_constraints;
    std::vector<GurobiSosConstraintIndex> sos_constraints;
    std::vector<GurobiGeneralConstraintIndex> general_constraints;
  };

  // Data associated with each linear constraint. With it we know if the
  // underlying representation is either:
  //   linear_terms <= upper_bound (if lower bound <= -GRB_INFINITY)
  //   linear_terms >= lower_bound (if upper bound >= GRB_INFINTY)
  //   linear_terms == xxxxx_bound (if upper_bound == lower_bound)
  //   linear_term - slack == 0 (with slack bounds equal to xxxxx_bound)
  struct LinearConstraintData {
    // Returns all Gurobi elements related to this constraint (including the
    // linear constraint itself). Will CHECK-fail if any element is unspecified.
    GurobiModelElements DependentElements() const;

    GurobiLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    // only valid for true ranged constraints.
    GurobiVariableIndex slack_index = kUnspecifiedIndex;
    double lower_bound = -kInf;
    double upper_bound = kInf;
  };

  struct SecondOrderConeConstraintData {
    // Returns all Gurobi elements related to this constraint (including the
    // linear constraint itself). Will CHECK-fail if any element is unspecified.
    GurobiModelElements DependentElements() const;

    GurobiQuadraticConstraintIndex constraint_index = kUnspecifiedConstraint;
    std::vector<GurobiVariableIndex> slack_variables;
    std::vector<GurobiLinearConstraintIndex> slack_constraints;
  };

  struct SosConstraintData {
    // Returns all Gurobi elements related to this constraint (including the
    // linear constraint itself). Will CHECK-fail if any element is unspecified.
    GurobiModelElements DependentElements() const;

    GurobiSosConstraintIndex constraint_index = kUnspecifiedConstraint;
    std::vector<GurobiVariableIndex> slack_variables;
    std::vector<GurobiLinearConstraintIndex> slack_constraints;
  };

  struct IndicatorConstraintData {
    // The Gurobi-numbered general constraint ID (Gurobi ids are shared among
    // all general constraint types).
    GurobiGeneralConstraintIndex constraint_index;
    // The MathOpt-numbered indicator variable ID. Used for reporting invalid
    // indicator variables.
    int64_t indicator_variable_id;
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

  using IdHashMap = gtl::linked_hash_map<int64_t, int>;

  absl::StatusOr<SolveResultProto> ExtractSolveResultProto(
      absl::Time start, const ModelSolveParametersProto& model_parameters);
  absl::Status FillRays(const ModelSolveParametersProto& model_parameters,
                        SolutionClaims solution_claims,
                        SolveResultProto& result);
  absl::StatusOr<GurobiSolver::SolutionsAndClaims> GetSolutions(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolveStatsProto> GetSolveStats(absl::Time start) const;

  absl::StatusOr<double> GetGurobiBestDualBound() const;
  absl::StatusOr<double> GetBestDualBound(
      const std::vector<SolutionProto>& solutions) const;
  absl::StatusOr<double> GetBestPrimalBound(
      absl::Span<const SolutionProto> solutions) const;

  bool PrimalSolutionQualityAvailable() const;
  absl::StatusOr<double> GetPrimalSolutionQuality() const;

  // Returns true if any entry in `grb_elements` is contained in the IIS.
  absl::StatusOr<bool> AnyElementInIIS(const GurobiModelElements& grb_elements);
  // Returns which variable bounds are in the IIS, or unset if neither are.
  absl::StatusOr<std::optional<ModelSubsetProto::Bounds>> VariableBoundsInIIS(
      GurobiVariableIndex grb_index);
  // Returns true if any variable bound is contained in the IIS.
  absl::StatusOr<bool> VariableInIIS(GurobiVariableIndex grb_index);
  absl::StatusOr<std::optional<ModelSubsetProto::Bounds>> LinearConstraintInIIS(
      const LinearConstraintData& grb_data);
  absl::StatusOr<std::optional<ModelSubsetProto::Bounds>>
  QuadraticConstraintInIIS(GurobiQuadraticConstraintIndex grb_index);
  // NOTE: Gurobi may claim that an IIS is minimal when the returned message is
  // not. This occurs because Gurobi does not return variable integrality IIS
  // attributes, and because internally we apply model transformations to handle
  // ranged constraints and linear expressions in nonlinear constraints.
  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ExtractComputeInfeasibleSubsystemResultProto(bool proven_infeasible);

  // Warning: is read from gurobi, take care with gurobi update.
  absl::StatusOr<bool> IsMaximize() const;

  absl::StatusOr<TerminationProto> ConvertTerminationReason(
      int gurobi_status, SolutionClaims solution_claims,
      double best_primal_bound, double best_dual_bound);

  // Returns solution information appropriate and available for an LP (linear
  // constraints + linear objective, only).
  absl::StatusOr<SolutionsAndClaims> GetLpSolution(
      const ModelSolveParametersProto& model_parameters);
  // Returns solution information appropriate and available for a QP (linear
  // constraints + quadratic objective, only).
  absl::StatusOr<SolutionsAndClaims> GetQpSolution(
      const ModelSolveParametersProto& model_parameters);
  // Returns solution information appropriate and available for a QCP
  // (linear/quadratic constraints + linear/quadratic objective, only).
  absl::StatusOr<SolutionsAndClaims> GetQcpSolution(
      const ModelSolveParametersProto& model_parameters);
  // Returns solution information appropriate and available for a MIP
  // (integrality on some/all decision variables). Following Gurobi's API, this
  // is also used for any multi-objective model.
  absl::StatusOr<SolutionsAndClaims> GetMipSolutions(
      const ModelSolveParametersProto& model_parameters);

  // return bool field should be true if a primal solution exists.
  absl::StatusOr<SolutionAndClaim<PrimalSolutionProto>>
  GetConvexPrimalSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolutionAndClaim<DualSolutionProto>>
  GetLpDualSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<std::optional<BasisProto>> GetBasisIfAvailable();

  absl::Status SetParameters(const SolveParametersProto& parameters);
  absl::Status AddNewLinearConstraints(
      const LinearConstraintsProto& constraints);
  absl::Status AddNewQuadraticConstraints(
      const google::protobuf::Map<QuadraticConstraintId,
                                  QuadraticConstraintProto>& constraints);
  absl::Status AddNewSecondOrderConeConstraints(
      const google::protobuf::Map<SecondOrderConeConstraintId,
                                  SecondOrderConeConstraintProto>& constraints);
  absl::Status AddNewSosConstraints(
      const google::protobuf::Map<AnyConstraintId, SosConstraintProto>&
          constraints,
      int sos_type,
      absl::flat_hash_map<int64_t, SosConstraintData>& constraints_map);
  absl::Status AddNewIndicatorConstraints(
      const google::protobuf::Map<IndicatorConstraintId,
                                  IndicatorConstraintProto>& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddSingleObjective(const ObjectiveProto& objective);
  absl::Status AddMultiObjectives(
      const ObjectiveProto& primary_objective,
      const google::protobuf::Map<int64_t, ObjectiveProto>&
          auxiliary_objectives);
  // * `objective_id` is the corresponding key into `multi_objectives_map_`; see
  //   that field for further comment.
  // * `is_maximize` is true if the entire Gurobi model is the maximization
  //   sense (only one sense is permitted per model to be shared by all
  //   objectives). Note that this need not agree with `objective.maximize`.
  absl::Status AddNewMultiObjective(
      const ObjectiveProto& objective,
      std::optional<AuxiliaryObjectiveId> objective_id, bool is_maximize);
  absl::Status AddNewSlacks(
      const std::vector<LinearConstraintData*>& new_slacks);
  absl::Status ChangeCoefficients(const SparseDoubleMatrixProto& matrix);
  // NOTE: Clears any existing quadratic objective terms.
  absl::Status ResetQuadraticObjectiveTerms(
      const SparseDoubleMatrixProto& terms);
  // Updates objective so that it is the sum of everything in terms, plus all
  // other terms prexisting in the objective that are not overwritten by terms.
  absl::Status UpdateQuadraticObjectiveTerms(
      const SparseDoubleMatrixProto& terms);
  absl::Status LoadModel(const ModelProto& input_model);

  absl::Status UpdateDoubleListAttribute(const SparseDoubleVectorProto& update,
                                         const char* attribute_name,
                                         const IdHashMap& id_hash_map);
  absl::Status UpdateInt32ListAttribute(const SparseInt32VectorProto& update,
                                        const char* attribute_name,
                                        const IdHashMap& id_hash_map);

  struct DeletedIndices {
    std::vector<GurobiVariableIndex> variables;
    std::vector<GurobiLinearConstraintIndex> linear_constraints;
    std::vector<GurobiQuadraticConstraintIndex> quadratic_constraints;
    std::vector<GurobiSosConstraintIndex> sos_constraints;
    std::vector<GurobiGeneralConstraintIndex> general_constraints;
  };

  void UpdateGurobiIndices(const DeletedIndices& deleted_indices);
  absl::Status UpdateLinearConstraints(
      const LinearConstraintUpdatesProto& update,
      std::vector<GurobiVariableIndex>& deleted_variables_index);

  int get_model_index(GurobiVariableIndex index) const { return index; }
  int get_model_index(const LinearConstraintData& index) const {
    return index.constraint_index;
  }

  // Fills in result with the values in gurobi_values aided by the index
  // conversion from map which should be either variables_map_ or
  // linear_constraints_map_ as appropriate. Only key/value pairs that passes
  // the filter predicate are added.
  template <typename T>
  void GurobiVectorToSparseDoubleVector(
      absl::Span<const double> gurobi_values, const T& map,
      SparseDoubleVectorProto& result,
      const SparseVectorFilterProto& filter) const;
  absl::StatusOr<BasisProto> GetGurobiBasis();
  absl::Status SetGurobiBasis(const BasisProto& basis);
  absl::StatusOr<DualRayProto> GetGurobiDualRay(
      const SparseVectorFilterProto& linear_constraints_filter,
      const SparseVectorFilterProto& variables_filter, bool is_maximize);
  // Returns true if the problem has any integrality constraints.
  absl::StatusOr<bool> IsMIP() const;
  // Returns true if the problem has a quadratic objective.
  absl::StatusOr<bool> IsQP() const;
  // Returns true if the problem has any quadratic constraints.
  absl::StatusOr<bool> IsQCP() const;

  absl::StatusOr<std::unique_ptr<GurobiCallbackData>> RegisterCallback(
      const CallbackRegistrationProto& registration, Callback cb,
      MessageCallback message_cb, absl::Time start,
      SolveInterrupter* local_interrupter);

  // Returns the ids of variables and linear constraints with inverted bounds.
  absl::StatusOr<InvertedBounds> ListInvertedBounds() const;

  // True if the model is in "multi-objective" mode: That is, at some point it
  // has been modified via the multi-objective API.
  bool is_multi_objective_mode() const;

  // Returns the ids of indicator constraint/variables that are invalid because
  // the indicator is not a binary variable.
  absl::StatusOr<InvalidIndicators> ListInvalidIndicators() const;

  struct VariableEqualToExpression {
    GurobiVariableIndex variable_index;
    GurobiLinearConstraintIndex constraint_index;
  };

  // If `expression` is equivalent to a single variable in the model, returns
  // the corresponding MathOpt variable ID. Otherwise, returns nullopt.
  std::optional<VariableId> TryExtractVariable(
      const LinearExpressionProto& expression);
  // Returns a Gurobi variable that is constrained to be equal to `expression`
  // in `.variable_index`. The variable is newly added to the model and
  // `.constraint_index` is set and refers to the Gurobi linear constraint index
  // for a slack constraint just added to the model such that:
  // `expression` == `.variable_index`.
  // TODO(b/267310257): Use this for linear constraint slacks, and maybe move it
  // up the stack to a bridge.
  absl::StatusOr<VariableEqualToExpression>
  CreateSlackVariableEqualToExpression(const LinearExpressionProto& expression);

  absl::Status SetMultiObjectiveTolerances(
      const ModelSolveParametersProto& model_parameters);

  absl::Status ResetModelParameters(
      const ModelSolveParametersProto& model_parameters);

  const std::unique_ptr<Gurobi> gurobi_;

  // Note that we use linked_hash_map for the indices of the gurobi_model_
  // variables and linear constraints to ensure that iteration over the map
  // maintains their insertion order (and, thus, the order in which they appear
  // in the model). As of 2022-06-28 this property is necessary to ensure that
  // duals and bases are deterministically ordered.

  // Internal correspondence from variable proto IDs to Gurobi-numbered
  // variables.
  gtl::linked_hash_map<VariableId, GurobiVariableIndex> variables_map_;
  // An unset key corresponds to the `ModelProto.objective` field; all other
  // entries come from `ModelProto.auxiliary_objectives`.
  absl::flat_hash_map<std::optional<AuxiliaryObjectiveId>,
                      GurobiMultiObjectiveIndex>
      multi_objectives_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Gurobi-numbered linear constraint and extra information.
  gtl::linked_hash_map<LinearConstraintId, LinearConstraintData>
      linear_constraints_map_;
  // Internal correspondence from quadratic constraint proto IDs to
  // Gurobi-numbered quadratic constraint.
  absl::flat_hash_map<QuadraticConstraintId, GurobiQuadraticConstraintIndex>
      quadratic_constraints_map_;
  // Internal correspondence from second-order cone constraint proto IDs to
  // corresponding Gurobi information.
  absl::flat_hash_map<SecondOrderConeConstraintId,
                      SecondOrderConeConstraintData>
      soc_constraints_map_;
  // Internal correspondence from SOS1 constraint proto IDs to Gurobi-numbered
  // SOS constraint (Gurobi ids are shared between SOS1 and SOS2).
  absl::flat_hash_map<Sos1ConstraintId, SosConstraintData>
      sos1_constraints_map_;
  // Internal correspondence from SOS2 constraint proto IDs to Gurobi-numbered
  // SOS constraint (Gurobi ids are shared between SOS1 and SOS2).
  absl::flat_hash_map<Sos2ConstraintId, SosConstraintData>
      sos2_constraints_map_;
  // Internal correspondence from indicator constraint proto IDs to indicator
  // constraint data. If unset, the values indicate that the indicator variable
  // is unset; since Gurobi does not support this, we simply do not add the
  // constraint to the model.
  absl::flat_hash_map<IndicatorConstraintId,
                      std::optional<IndicatorConstraintData>>
      indicator_constraints_map_;

  // Fields to track the number of Gurobi variables and constraints. These
  // quantities are updated immediately after adding or removing to the model,
  // so it is correct even if GRBUpdate has not yet been called.

  // Number of Gurobi variables.
  int num_gurobi_variables_ = 0;
  // Number of Gurobi linear constraints.
  int num_gurobi_lin_cons_ = 0;
  // Number of Gurobi quadratic constraints.
  int num_gurobi_quad_cons_ = 0;
  // Number of Gurobi SOS constraints.
  int num_gurobi_sos_cons_ = 0;
  // Number of Gurobi general constraints.
  int num_gurobi_gen_cons_ = 0;

  // Gurobi does not expose a way to query quadratic objective terms from the
  // model, so we track them. Notes:
  //   * Keys are in upper triangular order (.first <= .second)
  //   * Terms not in the map have zero coefficients
  // Note also that the map may also have entries with zero coefficient value.
  absl::flat_hash_map<std::pair<VariableId, VariableId>, double>
      quadratic_objective_coefficients_;

  // Some MathOpt variables cannot be deleted without rendering the rest of the
  // model invalid. We flag these variables to check in CanUpdate(). As of
  // 2022-07-01 elements are not erased from this set, and so it may be overly
  // conservative in rejecting updates.
  absl::flat_hash_set<VariableId> undeletable_variables_;

  static constexpr int kGrbBasicConstraint = 0;
  static constexpr int kGrbNonBasicConstraint = -1;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_
