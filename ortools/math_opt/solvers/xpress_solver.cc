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

#include "ortools/math_opt/solvers/xpress_solver.h"

#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/xpress/environment.h"

namespace operations_research::math_opt {

constexpr SupportedProblemStructures kXpressSupportedStructures = {
    .integer_variables = SupportType::kNotImplemented,
    .multi_objectives = SupportType::kNotImplemented,
    .quadratic_objectives = SupportType::kNotImplemented,
    .quadratic_constraints = SupportType::kNotImplemented,
    .second_order_cone_constraints = SupportType::kNotImplemented,
    .sos1_constraints = SupportType::kNotImplemented,
    .sos2_constraints = SupportType::kNotImplemented,
    .indicator_constraints = SupportType::kNotImplemented};

absl::StatusOr<std::unique_ptr<XpressSolver>> XpressSolver::New(
    const ModelProto& input_model, const SolverInterface::InitArgs& init_args) {
  if (!XpressIsCorrectlyInstalled()) {
    return absl::InvalidArgumentError("Xpress is not correctly installed.");
  }
  RETURN_IF_ERROR(
      ModelIsSupported(input_model, kXpressSupportedStructures, "XPRESS"));

  // We can add here extra checks that are not made in ModelIsSupported
  // (for example, if XPRESS does not support multi-objective with quad terms)

  ASSIGN_OR_RETURN(std::unique_ptr<Xpress> xpr,
                   Xpress::New(input_model.name()));
  auto xpress_solver = absl::WrapUnique(new XpressSolver(std::move(xpr)));
  RETURN_IF_ERROR(xpress_solver->LoadModel(input_model));
  return xpress_solver;
}

absl::Status XpressSolver::LoadModel(const ModelProto& input_model) {
  CHECK(xpress_ != nullptr);
  // TODO: set prob name, use XPRSsetprobname (must be added to environment)
  // (!) must be truncated to MAXPROBNAMELENGTH
  // RETURN_IF_ERROR(xpress_->SetProbName(input_model.name());
  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));
  RETURN_IF_ERROR(AddNewLinearConstraints(input_model.linear_constraints()));
  // TODO: instead of changing coefficients, set them when adding constraints?
  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));
  RETURN_IF_ERROR(AddSingleObjective(input_model.objective()));
  return absl::OkStatus();
}
absl::Status XpressSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  const int num_new_variables = new_variables.lower_bounds().size();
  std::vector<char> variable_type(num_new_variables);
  int n_variables = xpress_->GetNumberOfColumns();
  for (int j = 0; j < num_new_variables; ++j) {
    const VarId id = new_variables.ids(j);
    InsertOrDie(&variables_map_, id, j + n_variables);
    variable_type[j] =
        new_variables.integers(j) ? XPRS_INTEGER : XPRS_CONTINUOUS;
    if (new_variables.integers(j)) {
      is_mip_ = true;
      return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
    }
  }
  RETURN_IF_ERROR(xpress_->AddVars({}, new_variables.lower_bounds(),
                                   new_variables.upper_bounds(),
                                   variable_type));

  // Not adding names for performance (have to call XPRSaddnames)
  // TODO : keep names in a cache and add them when needed?

  return absl::OkStatus();
}

XpressSolver::XpressSolver(std::unique_ptr<Xpress> g_xpress)
    : xpress_(std::move(g_xpress)) {}

absl::Status XpressSolver::AddNewLinearConstraints(
    const LinearConstraintsProto& constraints) {
  const int num_new_constraints = constraints.lower_bounds().size();

  std::vector<char> constraint_sense;
  std::vector<double> constraint_rhs;
  std::vector<double> constraint_rng;
  std::vector<LinearConstraintData*> new_slacks;
  constraint_rhs.reserve(num_new_constraints);
  constraint_sense.reserve(num_new_constraints);
  new_slacks.reserve(num_new_constraints);
  int n_constraints = xpress_->GetNumberOfRows();
  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = constraints.ids(i);
    LinearConstraintData& constraint_data =
        InsertKeyOrDie(&linear_constraints_map_, id);
    const double lb = constraints.lower_bounds(i);
    const double ub = constraints.upper_bounds(i);
    constraint_data.lower_bound = lb;
    constraint_data.upper_bound = ub;
    constraint_data.constraint_index = i + n_constraints;
    char sense = XPRS_EQUAL;
    double rhs = 0.0;
    double rng = 0.0;
    const bool lb_is_xprs_neg_inf = lb <= XPRS_MINUSINFINITY;
    const bool ub_is_xprs_pos_inf = ub >= XPRS_PLUSINFINITY;
    if (lb_is_xprs_neg_inf && !ub_is_xprs_pos_inf) {
      sense = XPRS_LESS_EQUAL;
      rhs = ub;
    } else if (!lb_is_xprs_neg_inf && ub_is_xprs_pos_inf) {
      sense = XPRS_GREATER_EQUAL;
      rhs = lb;
    } else if (lb == ub) {
      sense = XPRS_EQUAL;
      rhs = lb;
    } else {
      if (ub < lb) {
        return absl::InvalidArgumentError("Lower bound > Upper bound");
      }
      sense = XPRS_RANGE;
      rhs = ub;
      rng = ub - lb;
    }
    constraint_sense.emplace_back(sense);
    constraint_rhs.emplace_back(rhs);
    constraint_rng.emplace_back(rng);
  }
  // Add all constraints in one call.
  return xpress_->AddConstrs(constraint_sense, constraint_rhs, constraint_rng);
}

absl::Status XpressSolver::AddSingleObjective(const ObjectiveProto& objective) {
  // TODO: reactivate the following code after figuring out why the condition is
  // true for LP tests
  /*if (objective.has_quadratic_coefficients()) {
    return absl::UnimplementedError(
        "Quadratic objectives are not yet implemented in XPRESS solver "
        "interface.");
  }*/
  if (objective.linear_coefficients().ids_size() == 0) {
    return absl::OkStatus();
  }
  std::vector<int> index;
  index.reserve(objective.linear_coefficients().ids_size());
  for (const int64_t id : objective.linear_coefficients().ids()) {
    index.push_back(variables_map_.at(id));
  }
  RETURN_IF_ERROR(
      xpress_->SetObjective(objective.maximize(), objective.offset(), index,
                            objective.linear_coefficients().values()));
  is_maximize_ = objective.maximize();
  return absl::OkStatus();
}

absl::Status XpressSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  const int num_coefficients = matrix.row_ids().size();
  std::vector<int> row_index;
  row_index.reserve(num_coefficients);
  std::vector<int> col_index;
  col_index.reserve(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index.push_back(
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index);
    col_index.push_back(variables_map_.at(matrix.column_ids(k)));
  }
  return xpress_->ChgCoeffs(row_index, col_index, matrix.coefficients());
}

absl::StatusOr<SolveResultProto> XpressSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, Callback cb,
    const SolveInterrupter* interrupter) {
  const absl::Time start = absl::Now();

  // TODO: set solve parameters
  // TODO: set basis
  // TODO: set hints
  // TODO: set branching properties
  // TODO: set lazy constraints
  // TODO: add interrupter using xpress_->Terminate();

  RETURN_IF_ERROR(CallXpressSolve(parameters.enable_output()))
      << "Error during XPRESS solve";

  ASSIGN_OR_RETURN(SolveResultProto solve_result,
                   ExtractSolveResultProto(start, model_parameters));

  return solve_result;
}

absl::Status XpressSolver::CallXpressSolve(bool enableOutput) {
  // Enable screen output right before solve
  if (enableOutput) {
    RETURN_IF_ERROR(xpress_->SetIntAttr(XPRS_OUTPUTLOG, 1))
        << "Unable to enable XPRESS logs";
  }
  // Solve
  if (is_mip_) {
    ASSIGN_OR_RETURN(xpress_status_, xpress_->MipOptimizeAndGetStatus());
  } else {
    ASSIGN_OR_RETURN(xpress_status_, xpress_->LpOptimizeAndGetStatus());
  }
  // Post-solve
  if (!(is_mip_ ? (xpress_status_ == XPRS_MIP_OPTIMAL)
                : (xpress_status_ == XPRS_LP_OPTIMAL))) {
    RETURN_IF_ERROR(xpress_->PostSolve()) << "Post-solve failed in XPRESS";
  }
  // Disable screen output right after solve
  if (enableOutput) {
    RETURN_IF_ERROR(xpress_->SetIntAttr(XPRS_OUTPUTLOG, 0))
        << "Unable to disable XPRESS logs";
  }
  return absl::OkStatus();
}

absl::StatusOr<SolveResultProto> XpressSolver::ExtractSolveResultProto(
    absl::Time start, const ModelSolveParametersProto& model_parameters) {
  SolveResultProto result;
  ASSIGN_OR_RETURN(SolutionsAndClaims solution_and_claims,
                   GetSolutions(model_parameters));
  for (SolutionProto& solution : solution_and_claims.solutions) {
    *result.add_solutions() = std::move(solution);
  }
  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  ASSIGN_OR_RETURN(const double best_primal_bound, GetBestPrimalBound());
  ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  auto solution_claims = solution_and_claims.solution_claims;
  ASSIGN_OR_RETURN(*result.mutable_termination(),
                   ConvertTerminationReason(solution_claims, best_primal_bound,
                                            best_dual_bound));
  return result;
}

absl::StatusOr<double> XpressSolver::GetBestPrimalBound() const {
  return xpress_->GetDoubleAttr(is_mip_ ? XPRS_MIPOBJVAL : XPRS_LPOBJVAL);
}

absl::StatusOr<double> XpressSolver::GetBestDualBound() const {
  // TODO: setting LP primal value as best dual bound. Can this be improved?
  return xpress_->GetDoubleAttr(XPRS_LPOBJVAL);
}

absl::StatusOr<XpressSolver::SolutionsAndClaims> XpressSolver::GetSolutions(
    const ModelSolveParametersProto& model_parameters) {
  if (is_mip_) {
    return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
  } else {
    return GetLpSolution(model_parameters);
  }
}

absl::StatusOr<XpressSolver::SolutionsAndClaims> XpressSolver::GetLpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto primal_solution_and_claim,
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto dual_solution_and_claim,
                   GetConvexDualSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable());
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists =
          primal_solution_and_claim.feasible_solution_exists,
      .dual_feasible_solution_exists =
          dual_solution_and_claim.feasible_solution_exists};

  if (!primal_solution_and_claim.solution.has_value() &&
      !dual_solution_and_claim.solution.has_value() && !basis.has_value()) {
    return SolutionsAndClaims{.solution_claims = solution_claims};
  }
  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  SolutionProto& solution =
      solution_and_claims.solutions.emplace_back(SolutionProto());
  if (primal_solution_and_claim.solution.has_value()) {
    *solution.mutable_primal_solution() =
        std::move(*primal_solution_and_claim.solution);
  }
  if (dual_solution_and_claim.solution.has_value()) {
    *solution.mutable_dual_solution() =
        std::move(*dual_solution_and_claim.solution);
  }
  if (basis.has_value()) {
    *solution.mutable_basis() = std::move(*basis);
  }
  return solution_and_claims;
}

bool XpressSolver::isFeasible() const {
  return xpress_status_ == (is_mip_ ? XPRS_MIP_OPTIMAL : XPRS_LP_OPTIMAL);
}

SolutionStatusProto XpressSolver::getLpSolutionStatus() const {
  // TODO : put all statuses here
  switch (xpress_status_) {
    case XPRS_LP_OPTIMAL:
      return SOLUTION_STATUS_FEASIBLE;
    case XPRS_LP_INFEAS:
      return SOLUTION_STATUS_INFEASIBLE;
    case XPRS_LP_UNBOUNDED:
      return SOLUTION_STATUS_UNDETERMINED;
    default:
      return SOLUTION_STATUS_UNSPECIFIED;
  }
}

absl::StatusOr<XpressSolver::SolutionAndClaim<PrimalSolutionProto>>
XpressSolver::GetConvexPrimalSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) const {
  PrimalSolutionProto primal_solution;
  primal_solution.set_feasibility_status(getLpSolutionStatus());
  if (isFeasible()) {
    ASSIGN_OR_RETURN(const double sol_val,
                     xpress_->GetDoubleAttr(XPRS_LPOBJVAL));
    primal_solution.set_objective_value(sol_val);
    XpressVectorToSparseDoubleVector(xpress_->GetPrimalValues().value(),
                                     variables_map_,
                                     *primal_solution.mutable_variable_values(),
                                     model_parameters.variable_values_filter());
  } else {
    // TODO
  }
  const bool primal_feasible_solution_exists =
      (primal_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE);
  return SolutionAndClaim<PrimalSolutionProto>{
      .solution = std::move(primal_solution),
      .feasible_solution_exists = primal_feasible_solution_exists};
}

absl::StatusOr<XpressSolver::SolutionAndClaim<DualSolutionProto>>
XpressSolver::GetConvexDualSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) const {
  DualSolutionProto dual_solution;
  const std::vector<double> xprs_constraint_duals =
      xpress_->GetConstraintDuals();
  XpressVectorToSparseDoubleVector(xprs_constraint_duals,
                                   linear_constraints_map_,
                                   *dual_solution.mutable_dual_values(),
                                   model_parameters.dual_values_filter());

  const std::vector<double> xprs_reduced_cost_values =
      xpress_->GetReducedCostValues();
  XpressVectorToSparseDoubleVector(xprs_reduced_cost_values, variables_map_,
                                   *dual_solution.mutable_reduced_costs(),
                                   model_parameters.reduced_costs_filter());

  if (isFeasible()) {
    ASSIGN_OR_RETURN(const double sol_val,
                     xpress_->GetDoubleAttr(XPRS_LPOBJVAL));
    dual_solution.set_objective_value(sol_val);
  } else {
    // TODO
  }
  dual_solution.set_feasibility_status(getLpSolutionStatus());
  bool dual_feasible_solution_exists =
      (dual_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE);
  ASSIGN_OR_RETURN(const double best_dual_bound,
                   xpress_->GetDoubleAttr(XPRS_LPOBJVAL));
  // const double best_dual_bound = is_maximize_ ? kMinusInf : kPlusInf;
  if (dual_feasible_solution_exists || std::isfinite(best_dual_bound)) {
    dual_feasible_solution_exists = true;
  } else if (xpress_status_ == XPRS_LP_OPTIMAL) {
    return absl::InternalError(
        "Xpress status is XPRS_LP_OPTIMAL, but XPRS_BESTBOUND is "
        "unavailable or infinite, and no dual feasible solution is returned");
  }
  return SolutionAndClaim<DualSolutionProto>{
      .solution = dual_solution,
      .feasible_solution_exists = dual_feasible_solution_exists};
}

inline BasisStatusProto ConvertVariableStatus(const int status) {
  switch (status) {
    case XPRS_BASIC:
      return BASIS_STATUS_BASIC;
    case XPRS_AT_LOWER:
      return BASIS_STATUS_AT_LOWER_BOUND;
    case XPRS_AT_UPPER:
      return BASIS_STATUS_AT_UPPER_BOUND;
    case XPRS_FREE_SUPER:
      return BASIS_STATUS_FREE;
    default:
      return BASIS_STATUS_UNSPECIFIED;
  }
}

absl::StatusOr<std::optional<BasisProto>> XpressSolver::GetBasisIfAvailable() {
  // Variable basis
  BasisProto basis;
  auto xprs_variable_basis_status = xpress_->GetVariableBasis();
  for (auto [variable_id, xprs_variable_index] : variables_map_) {
    basis.mutable_variable_status()->add_ids(variable_id);
    const BasisStatusProto variable_status =
        ConvertVariableStatus(xprs_variable_basis_status[xprs_variable_index]);
    if (variable_status == BASIS_STATUS_UNSPECIFIED) {
      return absl::InternalError(
          absl::StrCat("Invalid Xpress variable basis status: ",
                       xprs_variable_basis_status[xprs_variable_index]));
    }
    basis.mutable_variable_status()->add_values(variable_status);
  }
  // Constraint basis
  // TODO : implement this, mocked for now (else Basis validation will fail)
  for (auto [constraint_id, xprs_ct_index] : linear_constraints_map_) {
    basis.mutable_constraint_status()->add_ids(constraint_id);
    basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
  }
  // Dual basis
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
  if (xpress_status_ == XPRS_LP_OPTIMAL) {
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
  } else if (xpress_status_ == XPRS_LP_UNBOUNDED) {
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
  }
  return basis;
}

absl::StatusOr<SolveStatsProto> XpressSolver::GetSolveStats(
    absl::Time start) const {
  SolveStatsProto solve_stats;
  CHECK_OK(util_time::EncodeGoogleApiProto(absl::Now() - start,
                                           solve_stats.mutable_solve_time()));
  // TODO : complete these stats
  return solve_stats;
}

template <typename T>
void XpressSolver::XpressVectorToSparseDoubleVector(
    absl::Span<const double> xpress_values, const T& map,
    SparseDoubleVectorProto& result,
    const SparseVectorFilterProto& filter) const {
  SparseVectorFilterPredicate predicate(filter);
  for (auto [id, xpress_data] : map) {
    const double value = xpress_values[get_model_index(xpress_data)];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
}

absl::StatusOr<TerminationProto> XpressSolver::ConvertTerminationReason(
    SolutionClaims solution_claims, double best_primal_bound,
    double best_dual_bound) const {
  // TODO : improve this
  if (!is_mip_) {
    switch (xpress_status_) {
      case XPRS_LP_UNSTARTED:
        return TerminateForReason(
            is_maximize_, TERMINATION_REASON_OTHER_ERROR,
            "Problem solve has not started (XPRS_LP_UNSTARTED)");
      case XPRS_LP_OPTIMAL:
        return OptimalTerminationProto(best_primal_bound, best_dual_bound);
      case XPRS_LP_INFEAS:
        return InfeasibleTerminationProto(
            is_maximize_, solution_claims.dual_feasible_solution_exists
                              ? FEASIBILITY_STATUS_FEASIBLE
                              : FEASIBILITY_STATUS_UNDETERMINED);
      case XPRS_LP_CUTOFF:
        return CutoffTerminationProto(
            is_maximize_, "Objective worse than cutoff (XPRS_LP_CUTOFF)");
      case XPRS_LP_UNFINISHED:
        return LimitTerminationProto(
            is_maximize_, LIMIT_UNSPECIFIED, best_primal_bound, best_dual_bound,
            "Solve did not finish (XPRS_LP_UNFINISHED)");
      case XPRS_LP_UNBOUNDED:
        if (solution_claims.primal_feasible_solution_exists) {
          return UnboundedTerminationProto(is_maximize_);
        }
        return InfeasibleOrUnboundedTerminationProto(
            is_maximize_, FEASIBILITY_STATUS_INFEASIBLE,
            "Xpress status XPRS_LP_UNBOUNDED");
      case XPRS_LP_CUTOFF_IN_DUAL:
        return CutoffTerminationProto(
            is_maximize_, "Cutoff in dual (XPRS_LP_CUTOFF_IN_DUAL)");
      case XPRS_LP_UNSOLVED:
        return TerminateForReason(
            is_maximize_, TERMINATION_REASON_NUMERICAL_ERROR,
            "Problem could not be solved due to numerical issues "
            "(XPRS_LP_UNSOLVED)");
      case XPRS_LP_NONCONVEX:
        return TerminateForReason(is_maximize_, TERMINATION_REASON_OTHER_ERROR,
                                  "Problem contains quadratic data, which is "
                                  "not convex (XPRS_LP_NONCONVEX)");
      default:
        return absl::InternalError(absl::StrCat(
            "Missing Xpress LP status code case: ", xpress_status_));
    }
  } else {
    return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
  }
}

absl::StatusOr<bool> XpressSolver::Update(
    const ModelUpdateProto& model_update) {
  // TODO: implement this
  return absl::UnimplementedError(
      "XpressSolver::Update is not implemented yet");
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
XpressSolver::ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                                         MessageCallback message_cb,
                                         const SolveInterrupter* interrupter) {
  // TODO: implement this
  return absl::UnimplementedError(
      "XpressSolver::ComputeInfeasibleSubsystem is not implemented yet");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_XPRESS, XpressSolver::New)

}  // namespace operations_research::math_opt
