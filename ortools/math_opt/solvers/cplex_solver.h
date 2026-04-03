// Copyright 2010-2026 Google LLC
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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_CPLEX_SOLVER_H_
#define ORTOOLS_MATH_OPT_SOLVERS_CPLEX_SOLVER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "absl/container/linked_hash_map.h"
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
#include "ortools/math_opt/solvers/cplex.pb.h"
#include "ortools/math_opt/solvers/cplex/g_cplex.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/third_party_solvers/cplex_environment.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {

struct CplexVersion {
  int major = 0;
  int minor = 0;
  int revision = 0;
  int subrevision = 0;

  bool operator>=(const CplexVersion& other) const {
    return std::tie(major, minor, revision, subrevision) >=
           std::tie(other.major, other.minor, other.revision,
                    other.subrevision);
  }
};

absl::StatusOr<CplexVersion> ParseCplexVersion(absl::string_view version_str);

// Returns true if the loaded CPLEX library supports objective limit.
// Returns false if the library cannot be loaded or the version is too old.
bool CplexSupportsObjectiveLimit();  // Could be generalized with a
                                     // supported-features struct.

class CplexSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<CplexSolver>> New(
      const ModelProto& input_model,
      const SolverInterface::InitArgs& init_args);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      const SolveInterrupter* absl_nullable interrupter) override;
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;
  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(
      const SolveParametersProto& parameters, MessageCallback message_cb,
      const SolveInterrupter* absl_nullable interrupter) override;

  absl::StatusOr<std::string> Version() const;

 private:
  explicit CplexSolver(std::unique_ptr<Cplex> g_cplex);

  // For easing reading the code, we declare these types:
  using VariableId = int64_t;
  using AuxiliaryObjectiveId = int64_t;
  using LinearConstraintId = int64_t;
  using AnyConstraintId = int64_t;
  using CplexVariableIndex = int;
  using CplexLinearConstraintIndex = int;
  using CplexGeneralConstraintIndex = int;
  using CplexAnyConstraintIndex = int;

  static constexpr CplexVariableIndex kUnspecifiedIndex = -1;
  static constexpr CplexAnyConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kInf = std::numeric_limits<double>::infinity();

  struct CplexModelElements {
    std::vector<CplexVariableIndex> variables;
    std::vector<CplexLinearConstraintIndex> linear_constraints;
  };

  // Data associated with each linear constraint. With it we know if the
  // underlying representation is either:
  //   linear_terms <= upper_bound (if lower bound <= -CPX_INFBOUND)
  //   linear_terms >= lower_bound (if upper bound >= CPX_INFBOUND)
  //   linear_terms == xxxxx_bound (if upper_bound == lower_bound)
  //   lower_bound <= linear_term <= upper_bound
  struct LinearConstraintData {
    // Returns all Cplex elements related to this constraint (including the
    // linear constraint itself). Will CHECK-fail if any element is unspecified.
    CplexModelElements DependentElements() const;

    CplexLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    double lower_bound = -kInf;
    double upper_bound = kInf;
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

  using IdHashMap = absl::linked_hash_map<int64_t, int>;

  absl::StatusOr<SolveResultProto> ExtractSolveResultProto(
      absl::Time start, const ModelSolveParametersProto& model_parameters,
      bool had_cutoff, bool had_iteration_limit, bool had_objective_limit);
  absl::StatusOr<CplexSolver::SolutionsAndClaims> GetSolutions(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolveStatsProto> GetSolveStats(absl::Time start) const;

  absl::StatusOr<double> GetCplexBestDualBound() const;
  absl::StatusOr<double> GetBestDualBound(
      absl::Span<const SolutionProto> solutions) const;
  absl::StatusOr<double> GetBestPrimalBound(
      absl::Span<const SolutionProto> solutions) const;

  absl::StatusOr<bool> IsMaximize() const;

  absl::StatusOr<TerminationProto> ConvertTerminationReason(
      int cplex_status, bool had_cutoff, bool had_iteration_limit,
      bool had_objective_limit, SolutionClaims solution_claims,
      double best_primal_bound, double best_dual_bound);

  // Returns solution information appropriate and available for an LP (linear
  // constraints + linear objective, only).
  absl::StatusOr<SolutionsAndClaims> GetLpSolution(
      const ModelSolveParametersProto& model_parameters);
  // Returns solution information appropriate and available for a MIP
  // (integrality on some/all decision variables).
  absl::StatusOr<SolutionsAndClaims> GetMipSolutions(
      const ModelSolveParametersProto& model_parameters);

  // return bool field should be true if a primal solution exists.
  absl::StatusOr<SolutionAndClaim<PrimalSolutionProto>>
  GetConvexPrimalSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters);
  absl::StatusOr<SolutionAndClaim<DualSolutionProto>>
  GetConvexDualSolutionIfAvailable(
      const ModelSolveParametersProto& model_parameters);

  absl::Status SetParameters(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters = {});
  absl::Status AddNewLinearConstraints(
      const LinearConstraintsProto& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddSingleObjective(const ObjectiveProto& objective);
  absl::Status ChangeCoefficients(const SparseDoubleMatrixProto& matrix);
  absl::Status LoadModel(const ModelProto& input_model);

  struct DeletedIndices {
    std::vector<CplexVariableIndex> variables;
    std::vector<CplexLinearConstraintIndex> linear_constraints;
  };

  void UpdateCplexIndices(const DeletedIndices& deleted_indices);
  absl::Status UpdateLinearConstraints(
      const LinearConstraintUpdatesProto& update,
      std::vector<CplexVariableIndex>& deleted_variables_index);

  // Fills in result with the values in cplex_values aided by the index
  // conversion from map which should be either variables_map_ or
  // linear_constraints_map_ as appropriate. Only key/value pairs that passes
  // the filter predicate are added.
  template <typename T>
  void CplexVectorToSparseDoubleVector(
      absl::Span<const double> cplex_values, const T& map,
      SparseDoubleVectorProto& result,
      const SparseVectorFilterProto& filter) const;

  int get_model_index(CplexVariableIndex index) const { return index; }
  int get_model_index(const LinearConstraintData& index) const {
    return index.constraint_index;
  }

  // Returns true if the problem has any integrality constraints.
  absl::StatusOr<bool> IsMIP() const;

  // Returns the ids of variables and linear constraints with inverted bounds.
  absl::StatusOr<InvertedBounds> ListInvertedBounds() const;

  absl::Status ResetModelParameters(
      const ModelSolveParametersProto& model_parameters);

  const std::unique_ptr<Cplex> cplex_;

  // Note that we use linked_hash_map for the indices of the CPLEX model
  // variables and linear constraints to ensure that iteration over the map
  // maintains their insertion order (and, thus, the order in which they appear
  // in the model). As of 2022-06-28 this property is necessary to ensure that
  // duals and bases are deterministically ordered.

  // Internal correspondence from variable proto IDs to Cplex-numbered
  // variables.
  absl::linked_hash_map<VariableId, CplexVariableIndex> variables_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Cplex-numbered linear constraint and extra information.
  absl::linked_hash_map<LinearConstraintId, LinearConstraintData>
      linear_constraints_map_;

  // Fields to track the number of Cplex variables and constraints. These
  // quantities are updated immediately after adding or removing to the model.

  // Number of Cplex variables.
  int num_cplex_variables_ = 0;
  // Number of Cplex linear constraints.
  int num_cplex_lin_cons_ = 0;

  // Some MathOpt variables cannot be deleted without rendering the rest of the
  // model invalid. We flag these variables to check in CanUpdate(). As of
  // 2022-07-01 elements are not erased from this set, and so it may be overly
  // conservative in rejecting updates.
  absl::flat_hash_set<VariableId> undeletable_variables_;

  static constexpr int kCpxBasicConstraint = 0;
  static constexpr int kCpxNonBasicConstraint = -1;

  // Respects solvers interpretation of finite values (CPX_INFBOUND)
  static bool IsFinite(double value);

  absl::StatusOr<std::tuple<std::vector<int>, std::vector<double>>>
  PrepareLinearObjectiveNonzeros(const absl::Span<const int64_t> indices,
                                 const absl::Span<const double> values);
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVERS_CPLEX_SOLVER_H_
