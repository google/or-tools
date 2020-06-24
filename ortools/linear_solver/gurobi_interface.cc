// Copyright 2010-2018 Google LLC
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

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/gurobi_environment.h"
#include "ortools/linear_solver/gurobi_proto_solver.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver_callback.h"

DEFINE_int32(num_gurobi_threads, 4, "Number of threads available for Gurobi.");

namespace operations_research {

class GurobiInterface : public MPSolverInterface {
 public:
  // Constructor that takes a name for the underlying GRB solver.
  explicit GurobiInterface(MPSolver* const solver, bool mip);
  ~GurobiInterface() override;

  // Sets the optimization direction (min/max).
  void SetOptimizationDirection(bool maximize) override;

  // ----- Solve -----
  // Solves the problem using the parameter values specified.
  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;
  absl::optional<MPSolutionResponse> DirectlySolveProto(
      const MPModelRequest& request) override;

  // Writes the model.
  void Write(const std::string& filename) override;

  // ----- Model modifications and extraction -----
  // Resets extracted model
  void Reset() override;

  // Modifies bounds.
  void SetVariableBounds(int var_index, double lb, double ub) override;
  void SetVariableInteger(int var_index, bool integer) override;
  void SetConstraintBounds(int row_index, double lb, double ub) override;

  // Adds Constraint incrementally.
  void AddRowConstraint(MPConstraint* const ct) override;
  bool AddIndicatorConstraint(MPConstraint* const ct) override;
  // Adds variable incrementally.
  void AddVariable(MPVariable* const var) override;
  // Changes a coefficient in a constraint.
  void SetCoefficient(MPConstraint* const constraint,
                      const MPVariable* const variable, double new_value,
                      double old_value) override;
  // Clears a constraint from all its terms.
  void ClearConstraint(MPConstraint* const constraint) override;
  // Changes a coefficient in the linear objective
  void SetObjectiveCoefficient(const MPVariable* const variable,
                               double coefficient) override;
  // Changes the constant term in the linear objective.
  void SetObjectiveOffset(double value) override;
  // Clears the objective from all its terms.
  void ClearObjective() override;
  bool CheckBestObjectiveBoundExists() const override;
  void BranchingPriorityChangedForVariable(int var_index) override;

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex or interior-point iterations
  int64 iterations() const override;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  int64 nodes() const override;
  // Best objective bound. Only available for discrete problems.
  double best_objective_bound() const override;

  // Returns the basis status of a row.
  MPSolver::BasisStatus row_status(int constraint_index) const override;
  // Returns the basis status of a column.
  MPSolver::BasisStatus column_status(int variable_index) const override;

  // ----- Misc -----
  // Queries problem type.
  bool IsContinuous() const override { return IsLP(); }
  bool IsLP() const override { return !mip_; }
  bool IsMIP() const override { return mip_; }

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  std::string SolverVersion() const override {
    int major, minor, technical;
    GRBversion(&major, &minor, &technical);
    return absl::StrFormat("Gurobi library version %d.%d.%d\n", major, minor,
                           technical);
  }

  bool InterruptSolve() override {
    if (model_ != nullptr) GRBterminate(model_);
    return true;
  }

  void* underlying_solver() override { return reinterpret_cast<void*>(model_); }

  double ComputeExactConditionNumber() const override {
    if (!IsContinuous()) {
      LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                  << " GUROBI_MIXED_INTEGER_PROGRAMMING";
      return 0.0;
    }

    // TODO(user,user): Not yet working.
    LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                << " GUROBI_LINEAR_PROGRAMMING";
    return 0.0;

    // double cond = 0.0;
    // const int status = GRBgetdblattr(model_, GRB_DBL_ATTR_KAPPA, &cond);
    // if (0 == status) {
    //   return cond;
    // } else {
    //   LOG(DFATAL) << "Condition number only available for "
    //               << "continuous problems";
    //   return 0.0;
    // }
  }

  // Iterates through the solutions in Gurobi's solution pool.
  bool NextSolution() override;

  void SetCallback(MPCallback* mp_callback) override;
  bool SupportsCallbacks() const override { return true; }

 private:
  // Sets all parameters in the underlying solver.
  void SetParameters(const MPSolverParameters& param) override;
  // Sets each parameter in the underlying solver.
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

  bool ReadParameterFile(const std::string& filename) override;
  std::string ValidFileExtensionForParameterFile() const override;

  MPSolver::BasisStatus TransformGRBVarBasisStatus(
      int gurobi_basis_status) const;
  MPSolver::BasisStatus TransformGRBConstraintBasisStatus(
      int gurobi_basis_status, int constraint_index) const;

  void CheckedGurobiCall(int err) const;

  int SolutionCount() const;

  GRBmodel* model_;
  GRBenv* env_;
  bool mip_;
  int current_solution_index_;
  MPCallback* callback_ = nullptr;
  bool update_branching_priorities_ = false;
};

namespace {

void CheckedGurobiCall(int err, GRBenv* const env) {
  CHECK_EQ(0, err) << "Fatal error with code " << err << ", due to "
                   << GRBgeterrormsg(env);
}

// For interacting directly with the Gurobi C API for callbacks.
struct GurobiInternalCallbackContext {
  GRBmodel* model;
  void* gurobi_internal_callback_data;
  int where;
};

class GurobiMPCallbackContext : public MPCallbackContext {
 public:
  GurobiMPCallbackContext(GRBenv* env, bool might_add_cuts,
                          bool might_add_lazy_constraints);

  // Implementation of the interface.
  MPCallbackEvent Event() override;
  bool CanQueryVariableValues() override;
  double VariableValue(const MPVariable* variable) override;
  void AddCut(const LinearRange& cutting_plane) override;
  void AddLazyConstraint(const LinearRange& lazy_constraint) override;
  double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) override;
  int64 NumExploredNodes() override;

  // Call this method to update the internal state of the callback context
  // before passing it to MPCallback::RunCallback().
  void UpdateFromGurobiState(
      const GurobiInternalCallbackContext& gurobi_internal_context);

 private:
  // Wraps GRBcbget(), used to query the state of the solver.  See
  // http://www.gurobi.com/documentation/8.0/refman/callback_codes.html#sec:CallbackCodes
  // for callback_code values.
  template <typename T>
  T GurobiCallbackGet(
      const GurobiInternalCallbackContext& gurobi_internal_context,
      int callback_code);
  void CheckedGurobiCall(int gurobi_error_code) const;

  template <typename GRBConstraintFunction>
  void AddGeneratedConstraint(const LinearRange& linear_range,
                              GRBConstraintFunction grb_constraint_function);

  // Returns the number of variables in the Gurobi model.
  // WARNING(rander): This is not the same as solver_->variables_.size(), the
  // use of range constraints adds new variables to the Gurobi model.
  int NumGurobiVariables() const;

  GRBenv* const env_;

  const bool might_add_cuts_;
  const bool might_add_lazy_constraints_;

  // Stateful, updated before each call to the callback.
  GurobiInternalCallbackContext current_gurobi_internal_callback_context_;
  bool variable_values_extracted_ = false;
  std::vector<double> variable_values_;
};

void GurobiMPCallbackContext::CheckedGurobiCall(int gurobi_error_code) const {
  ::operations_research::CheckedGurobiCall(gurobi_error_code, env_);
}

GurobiMPCallbackContext::GurobiMPCallbackContext(
    GRBenv* env, bool might_add_cuts, bool might_add_lazy_constraints)
    : env_(ABSL_DIE_IF_NULL(env)),
      might_add_cuts_(might_add_cuts),
      might_add_lazy_constraints_(might_add_lazy_constraints) {}

void GurobiMPCallbackContext::UpdateFromGurobiState(
    const GurobiInternalCallbackContext& gurobi_internal_context) {
  current_gurobi_internal_callback_context_ = gurobi_internal_context;
  variable_values_extracted_ = false;
}

int64 GurobiMPCallbackContext::NumExploredNodes() {
  switch (Event()) {
    case MPCallbackEvent::kMipNode:
      return static_cast<int64>(GurobiCallbackGet<double>(
          current_gurobi_internal_callback_context_, GRB_CB_MIPNODE_NODCNT));
    case MPCallbackEvent::kMipSolution:
      return static_cast<int64>(GurobiCallbackGet<double>(
          current_gurobi_internal_callback_context_, GRB_CB_MIPSOL_NODCNT));
    default:
      LOG(FATAL) << "Node count is supported only for callback events MIP_NODE "
                    "and MIP_SOL, but was requested at: "
                 << ToString(Event());
  }
}

template <typename T>
T GurobiMPCallbackContext::GurobiCallbackGet(
    const GurobiInternalCallbackContext& gurobi_internal_context,
    const int callback_code) {
  T result = 0;
  CheckedGurobiCall(
      GRBcbget(gurobi_internal_context.gurobi_internal_callback_data,
               gurobi_internal_context.where, callback_code,
               static_cast<void*>(&result)));
  return result;
}

MPCallbackEvent GurobiMPCallbackContext::Event() {
  switch (current_gurobi_internal_callback_context_.where) {
    case GRB_CB_POLLING:
      return MPCallbackEvent::kPolling;
    case GRB_CB_PRESOLVE:
      return MPCallbackEvent::kPresolve;
    case GRB_CB_SIMPLEX:
      return MPCallbackEvent::kSimplex;
    case GRB_CB_MIP:
      return MPCallbackEvent::kMip;
    case GRB_CB_MIPSOL:
      return MPCallbackEvent::kMipSolution;
    case GRB_CB_MIPNODE:
      return MPCallbackEvent::kMipNode;
    case GRB_CB_MESSAGE:
      return MPCallbackEvent::kMessage;
    case GRB_CB_BARRIER:
      return MPCallbackEvent::kBarrier;
      // TODO(b/112427356): in Gurobi 8.0, there is a new callback location.
      // case GRB_CB_MULTIOBJ:
      //   return MPCallbackEvent::kMultiObj;
    default:
      LOG_FIRST_N(ERROR, 1) << "Gurobi callback at unknown where="
                            << current_gurobi_internal_callback_context_.where;
      return MPCallbackEvent::kUnknown;
  }
}

bool GurobiMPCallbackContext::CanQueryVariableValues() {
  const MPCallbackEvent where = Event();
  if (where == MPCallbackEvent::kMipSolution) {
    return true;
  }
  if (where == MPCallbackEvent::kMipNode) {
    const int gurobi_node_status = GurobiCallbackGet<int>(
        current_gurobi_internal_callback_context_, GRB_CB_MIPNODE_STATUS);
    return gurobi_node_status == GRB_OPTIMAL;
  }
  return false;
}

double GurobiMPCallbackContext::VariableValue(const MPVariable* variable) {
  CHECK(variable != nullptr);
  if (!variable_values_extracted_) {
    const MPCallbackEvent where = Event();
    CHECK(where == MPCallbackEvent::kMipSolution ||
          where == MPCallbackEvent::kMipNode)
        << "You can only call VariableValue at "
        << ToString(MPCallbackEvent::kMipSolution) << " or "
        << ToString(MPCallbackEvent::kMipNode)
        << " but called from: " << ToString(where);
    const int gurobi_get_var_param = where == MPCallbackEvent::kMipNode
                                         ? GRB_CB_MIPNODE_REL
                                         : GRB_CB_MIPSOL_SOL;

    variable_values_.resize(NumGurobiVariables());
    CheckedGurobiCall(GRBcbget(
        current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
        current_gurobi_internal_callback_context_.where, gurobi_get_var_param,
        static_cast<void*>(variable_values_.data())));
    variable_values_extracted_ = true;
  }
  return variable_values_[variable->index()];
}

template <typename GRBConstraintFunction>
void GurobiMPCallbackContext::AddGeneratedConstraint(
    const LinearRange& linear_range,
    GRBConstraintFunction grb_constraint_function) {
  std::vector<int> variable_indices;
  std::vector<double> variable_coefficients;
  const int num_terms = linear_range.linear_expr().terms().size();
  variable_indices.reserve(num_terms);
  variable_coefficients.reserve(num_terms);
  for (const auto& var_coef_pair : linear_range.linear_expr().terms()) {
    variable_indices.push_back(var_coef_pair.first->index());
    variable_coefficients.push_back(var_coef_pair.second);
  }
  if (std::isfinite(linear_range.upper_bound())) {
    CheckedGurobiCall(grb_constraint_function(
        current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
        variable_indices.size(), variable_indices.data(),
        variable_coefficients.data(), GRB_LESS_EQUAL,
        linear_range.upper_bound()));
  }
  if (std::isfinite(linear_range.lower_bound())) {
    CheckedGurobiCall(grb_constraint_function(
        current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
        variable_indices.size(), variable_indices.data(),
        variable_coefficients.data(), GRB_GREATER_EQUAL,
        linear_range.lower_bound()));
  }
}

void GurobiMPCallbackContext::AddCut(const LinearRange& cutting_plane) {
  CHECK(might_add_cuts_);
  const MPCallbackEvent where = Event();
  CHECK(where == MPCallbackEvent::kMipNode)
      << "Cuts can only be added at MIP_NODE, tried to add cut at: "
      << ToString(where);
  AddGeneratedConstraint(cutting_plane, GRBcbcut);
}

void GurobiMPCallbackContext::AddLazyConstraint(
    const LinearRange& lazy_constraint) {
  CHECK(might_add_lazy_constraints_);
  const MPCallbackEvent where = Event();
  CHECK(where == MPCallbackEvent::kMipNode ||
        where == MPCallbackEvent::kMipSolution)
      << "Lazy constraints can only be added at MIP_NODE or MIP_SOL, tried to "
         "add lazy constraint at: "
      << ToString(where);
  AddGeneratedConstraint(lazy_constraint, GRBcblazy);
}

double GurobiMPCallbackContext::SuggestSolution(
    const absl::flat_hash_map<const MPVariable*, double>& solution) {
  const MPCallbackEvent where = Event();
  CHECK(where == MPCallbackEvent::kMipNode)
      << "Feasible solutions can only be added at MIP_NODE, tried to add "
         "solution at: "
      << ToString(where);

  std::vector<double> full_solution(NumGurobiVariables(), GRB_UNDEFINED);
  for (const auto& variable_value : solution) {
    const MPVariable* var = variable_value.first;
    full_solution[var->index()] = variable_value.second;
  }

  double objval;
  CheckedGurobiCall(GRBcbsolution(
      current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
      full_solution.data(), &objval));

  return objval;
}

int GurobiMPCallbackContext::NumGurobiVariables() const {
  int num_gurobi_variables = 0;
  CheckedGurobiCall(
      GRBgetintattr(current_gurobi_internal_callback_context_.model, "NumVars",
                    &num_gurobi_variables));
  return num_gurobi_variables;
}

struct MPCallbackWithGurobiContext {
  GurobiMPCallbackContext* context;
  MPCallback* callback;
};

// NOTE(user): This function must have this exact API, because we are passing
// it to Gurobi as a callback.
int STDCALL CallbackImpl(GRBmodel* model, void* gurobi_internal_callback_data,
                         int where, void* raw_model_and_callback) {
  MPCallbackWithGurobiContext* const callback_with_context =
      static_cast<MPCallbackWithGurobiContext*>(raw_model_and_callback);
  CHECK(callback_with_context != nullptr);
  CHECK(callback_with_context->context != nullptr);
  CHECK(callback_with_context->callback != nullptr);
  GurobiInternalCallbackContext gurobi_internal_context{
      model, gurobi_internal_callback_data, where};
  callback_with_context->context->UpdateFromGurobiState(
      gurobi_internal_context);
  callback_with_context->callback->RunCallback(callback_with_context->context);
  return 0;
}

}  // namespace

void GurobiInterface::CheckedGurobiCall(int err) const {
  ::operations_research::CheckedGurobiCall(err, env_);
}

// Creates a LP/MIP instance with the specified name and minimization objective.
GurobiInterface::GurobiInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver),
      model_(nullptr),
      env_(nullptr),
      mip_(mip),
      current_solution_index_(0) {
  CHECK_OK(LoadGurobiEnvironment(&env_));
  CheckedGurobiCall(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                0,          // numvars
                                nullptr,    // obj
                                nullptr,    // lb
                                nullptr,    // ub
                                nullptr,    // vtype
                                nullptr));  // varnanes
  CheckedGurobiCall(
      GRBsetintattr(model_, GRB_INT_ATTR_MODELSENSE, maximize_ ? -1 : 1));

  CheckedGurobiCall(
      GRBsetintparam(env_, GRB_INT_PAR_THREADS, FLAGS_num_gurobi_threads));
}

GurobiInterface::~GurobiInterface() {
  CheckedGurobiCall(GRBfreemodel(model_));
  GRBfreeenv(env_);
}

// ------ Model modifications and extraction -----

void GurobiInterface::Reset() {
  CheckedGurobiCall(GRBfreemodel(model_));
  CheckedGurobiCall(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                0,          // numvars
                                nullptr,    // obj
                                nullptr,    // lb
                                nullptr,    // ub
                                nullptr,    // vtype
                                nullptr));  // varnames
  ResetExtractionInformation();
}

void GurobiInterface::SetOptimizationDirection(bool maximize) {
  sync_status_ = MUST_RELOAD;
  // TODO(user,user): Fix, not yet working.
  // InvalidateSolutionSynchronization();
  // CheckedGurobiCall(GRBsetintattr(model_,
  //                                 GRB_INT_ATTR_MODELSENSE,
  //                                 maximize_ ? -1 : 1));
}

void GurobiInterface::SetVariableBounds(int var_index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
}

// Modifies integrality of an extracted variable.
void GurobiInterface::SetVariableInteger(int index, bool integer) {
  char current_type;
  CheckedGurobiCall(
      GRBgetcharattrelement(model_, GRB_CHAR_ATTR_VTYPE, index, &current_type));

  if ((integer &&
       (current_type == GRB_INTEGER || current_type == GRB_BINARY)) ||
      (!integer && current_type == GRB_CONTINUOUS)) {
    return;
  }

  InvalidateSolutionSynchronization();
  if (sync_status_ == MODEL_SYNCHRONIZED) {
    char type_var;
    if (integer) {
      type_var = GRB_INTEGER;
    } else {
      type_var = GRB_CONTINUOUS;
    }
    CheckedGurobiCall(
        GRBsetcharattrelement(model_, GRB_CHAR_ATTR_VTYPE, index, type_var));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::SetConstraintBounds(int index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

bool GurobiInterface::AddIndicatorConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
  return !IsContinuous();
}

void GurobiInterface::AddVariable(MPVariable* const ct) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetCoefficient(MPConstraint* const constraint,
                                     const MPVariable* const variable,
                                     double new_value, double old_value) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::ClearConstraint(MPConstraint* const constraint) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                              double coefficient) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
  // TODO(user,user): make it work.
  // InvalidateSolutionSynchronization();
  // CheckedGurobiCall(GRBsetdblattr(model_,
  //                                 GRB_DBL_ATTR_OBJCON,
  //                                 solver_->Objective().offset()));
  // CheckedGurobiCall(GRBupdatemodel(model_));
}

void GurobiInterface::ClearObjective() { sync_status_ = MUST_RELOAD; }

void GurobiInterface::BranchingPriorityChangedForVariable(int var_index) {
  update_branching_priorities_ = true;
}

// ------ Query statistics on the solution and the solve ------

int64 GurobiInterface::iterations() const {
  double iter;
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  CheckedGurobiCall(GRBgetdblattr(model_, GRB_DBL_ATTR_ITERCOUNT, &iter));
  return static_cast<int64>(iter);
}

int64 GurobiInterface::nodes() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    double nodes = 0;
    CheckedGurobiCall(GRBgetdblattr(model_, GRB_DBL_ATTR_NODECOUNT, &nodes));
    return static_cast<int64>(nodes);
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems.";
    return kUnknownNumberOfNodes;
  }
}

bool GurobiInterface::CheckBestObjectiveBoundExists() const {
  double value;
  const int error = GRBgetdblattr(model_, GRB_DBL_ATTR_OBJBOUND, &value);
  return error == 0;
}

// Returns the best objective bound. Only available for discrete problems.
double GurobiInterface::best_objective_bound() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists()) {
      return trivial_worst_objective_bound();
    }
    if (solver_->variables_.empty() && solver_->constraints_.empty()) {
      // Special case for empty model.
      return solver_->Objective().offset();
    }
    double value;
    const int error = GRBgetdblattr(model_, GRB_DBL_ATTR_OBJBOUND, &value);
    if (result_status_ == MPSolver::OPTIMAL &&
        error == GRB_ERROR_DATA_NOT_AVAILABLE) {
      // Special case for when presolve removes all the variables so the model
      // becomes empty after the presolve phase.
      return objective_value_;
    }
    CheckedGurobiCall(error);
    return value;
  } else {
    LOG(DFATAL) << "Best objective bound only available for discrete problems.";
    return trivial_worst_objective_bound();
  }
}

MPSolver::BasisStatus GurobiInterface::TransformGRBVarBasisStatus(
    int gurobi_basis_status) const {
  switch (gurobi_basis_status) {
    case GRB_BASIC:
      return MPSolver::BASIC;
    case GRB_NONBASIC_LOWER:
      return MPSolver::AT_LOWER_BOUND;
    case GRB_NONBASIC_UPPER:
      return MPSolver::AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return MPSolver::FREE;
    default:
      LOG(DFATAL) << "Unknown GRB basis status.";
      return MPSolver::FREE;
  }
}

MPSolver::BasisStatus GurobiInterface::TransformGRBConstraintBasisStatus(
    int gurobi_basis_status, int constraint_index) const {
  switch (gurobi_basis_status) {
    case GRB_BASIC:
      return MPSolver::BASIC;
    default: {
      // Non basic.
      double slack = 0.0;
      double tolerance = 0.0;
      CheckedGurobiCall(GRBgetdblparam(GRBgetenv(model_),
                                       GRB_DBL_PAR_FEASIBILITYTOL, &tolerance));
      CheckedGurobiCall(GRBgetdblattrelement(model_, GRB_DBL_ATTR_SLACK,
                                             constraint_index, &slack));
      char sense;
      CheckedGurobiCall(GRBgetcharattrelement(model_, GRB_CHAR_ATTR_SENSE,
                                              constraint_index, &sense));
      VLOG(4) << "constraint " << constraint_index << " , slack = " << slack
              << " , sense = " << sense;
      if (fabs(slack) <= tolerance) {
        switch (sense) {
          case GRB_EQUAL:
          case GRB_LESS_EQUAL:
            return MPSolver::AT_UPPER_BOUND;
          case GRB_GREATER_EQUAL:
            return MPSolver::AT_LOWER_BOUND;
          default:
            return MPSolver::FREE;
        }
      } else {
        return MPSolver::FREE;
      }
    }
  }
}

// Returns the basis status of a row.
MPSolver::BasisStatus GurobiInterface::row_status(int constraint_index) const {
  int optim_status = 0;
  CheckedGurobiCall(GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status));
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  CheckedGurobiCall(GRBgetintattrelement(
      model_, GRB_INT_ATTR_CBASIS, constraint_index, &gurobi_basis_status));
  return TransformGRBConstraintBasisStatus(gurobi_basis_status,
                                           constraint_index);
}

// Returns the basis status of a column.
MPSolver::BasisStatus GurobiInterface::column_status(int variable_index) const {
  int optim_status = 0;
  CheckedGurobiCall(GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optim_status));
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  int gurobi_basis_status = 0;
  CheckedGurobiCall(GRBgetintattrelement(model_, GRB_INT_ATTR_VBASIS,
                                         variable_index, &gurobi_basis_status));
  return TransformGRBVarBasisStatus(gurobi_basis_status);
}

// Extracts new variables.
void GurobiInterface::ExtractNewVariables() {
  CHECK(last_variable_index_ == 0 ||
        last_variable_index_ == solver_->variables_.size());
  CHECK(last_constraint_index_ == 0 ||
        last_constraint_index_ == solver_->constraints_.size());
  const int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    int num_new_variables = total_num_vars - last_variable_index_;
    std::unique_ptr<double[]> obj_coeffs(new double[num_new_variables]);
    std::unique_ptr<double[]> lb(new double[num_new_variables]);
    std::unique_ptr<double[]> ub(new double[num_new_variables]);
    std::unique_ptr<char[]> ctype(new char[num_new_variables]);
    std::unique_ptr<const char*[]> colname(new const char*[num_new_variables]);

    for (int j = 0; j < num_new_variables; ++j) {
      MPVariable* const var = solver_->variables_[last_variable_index_ + j];
      set_variable_as_extracted(var->index(), true);
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype.get()[j] = var->integer() && mip_ ? GRB_INTEGER : GRB_CONTINUOUS;
      if (!var->name().empty()) {
        colname[j] = var->name().c_str();
      }
      obj_coeffs[j] = solver_->objective_->GetCoefficient(var);
    }

    CheckedGurobiCall(GRBaddvars(model_, num_new_variables, 0, nullptr, nullptr,
                                 nullptr, obj_coeffs.get(), lb.get(), ub.get(),
                                 ctype.get(),
                                 const_cast<char**>(colname.get())));
  }
  CheckedGurobiCall(GRBupdatemodel(model_));
}

void GurobiInterface::ExtractNewConstraints() {
  CHECK(last_variable_index_ == 0 ||
        last_variable_index_ == solver_->variables_.size());
  CHECK(last_constraint_index_ == 0 ||
        last_constraint_index_ == solver_->constraints_.size());
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    // Find the length of the longest row.
    int max_row_length = 0;
    for (int row = last_constraint_index_; row < total_num_rows; ++row) {
      MPConstraint* const ct = solver_->constraints_[row];
      CHECK(!constraint_is_extracted(row));
      set_constraint_as_extracted(row, true);
      if (ct->coefficients_.size() > max_row_length) {
        max_row_length = ct->coefficients_.size();
      }
    }

    max_row_length = std::max(1, max_row_length);
    std::unique_ptr<int[]> col_indices(new int[max_row_length]);
    std::unique_ptr<double[]> coeffs(new double[max_row_length]);

    // Add each new constraint.
    for (int row = last_constraint_index_; row < total_num_rows; ++row) {
      MPConstraint* const ct = solver_->constraints_[row];
      CHECK(constraint_is_extracted(row));
      const int size = ct->coefficients_.size();
      int col = 0;
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        CHECK(variable_is_extracted(var_index));
        col_indices[col] = var_index;
        coeffs[col] = entry.second;
        col++;
      }
      char* const name =
          ct->name().empty() ? nullptr : const_cast<char*>(ct->name().c_str());
      if (ct->indicator_variable() != nullptr) {
        if (ct->lb() > -std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddgenconstrIndicator(
              model_, name, ct->indicator_variable()->index(),
              ct->indicator_value(), size, col_indices.get(), coeffs.get(),
              ct->ub() == ct->lb() ? GRB_EQUAL : GRB_GREATER_EQUAL, ct->lb()));
        }
        if (ct->ub() < std::numeric_limits<double>::infinity() &&
            ct->lb() != ct->ub()) {
          CheckedGurobiCall(GRBaddgenconstrIndicator(
              model_, name, ct->indicator_variable()->index(),
              ct->indicator_value(), size, col_indices.get(), coeffs.get(),
              GRB_LESS_EQUAL, ct->ub()));
        }
      } else {
        // Using GRBaddrangeconstr for constraints that don't require it adds
        // a slack which is not always removed by presolve.
        if (ct->lb() == ct->ub()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, col_indices.get(),
                                         coeffs.get(), GRB_EQUAL, ct->lb(),
                                         name));
        } else if (ct->lb() == -std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, col_indices.get(),
                                         coeffs.get(), GRB_LESS_EQUAL, ct->ub(),
                                         name));
        } else if (ct->ub() == std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, col_indices.get(),
                                         coeffs.get(), GRB_GREATER_EQUAL,
                                         ct->lb(), name));
        } else {
          CheckedGurobiCall(GRBaddrangeconstr(model_, size, col_indices.get(),
                                              coeffs.get(), ct->lb(), ct->ub(),
                                              name));
        }
      }
    }
  }
  CheckedGurobiCall(GRBupdatemodel(model_));
}

void GurobiInterface::ExtractObjective() {
  CheckedGurobiCall(
      GRBsetintattr(model_, GRB_INT_ATTR_MODELSENSE, maximize_ ? -1 : 1));
  CheckedGurobiCall(GRBsetdblattr(model_, GRB_DBL_ATTR_OBJCON,
                                  solver_->Objective().offset()));
}

// ------ Parameters  -----

void GurobiInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

void GurobiInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    CheckedGurobiCall(
        GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_MIPGAP, value));
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

// Gurobi has two different types of primal tolerance (feasibility tolerance):
// constraint and integrality. We need to set them both.
// See:
// http://www.gurobi.com/documentation/6.0/refman/feasibilitytol.html
// and
// http://www.gurobi.com/documentation/6.0/refman/intfeastol.html
void GurobiInterface::SetPrimalTolerance(double value) {
  CheckedGurobiCall(
      GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_FEASIBILITYTOL, value));
  CheckedGurobiCall(
      GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_INTFEASTOL, value));
}

// As opposed to primal (feasibility) tolerance, the dual (optimality) tolerance
// applies only to the reduced costs in the improving direction.
// See:
// http://www.gurobi.com/documentation/6.0/refman/optimalitytol.html
void GurobiInterface::SetDualTolerance(double value) {
  CheckedGurobiCall(
      GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_OPTIMALITYTOL, value));
}

void GurobiInterface::SetPresolveMode(int value) {
  switch (value) {
    case MPSolverParameters::PRESOLVE_OFF: {
      CheckedGurobiCall(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_PRESOLVE, false));
      break;
    }
    case MPSolverParameters::PRESOLVE_ON: {
      CheckedGurobiCall(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_PRESOLVE, true));
      break;
    }
    default: {
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
    }
  }
}

// Sets the scaling mode.
void GurobiInterface::SetScalingMode(int value) {
  switch (value) {
    case MPSolverParameters::SCALING_OFF:
      CheckedGurobiCall(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_SCALEFLAG, false));
      break;
    case MPSolverParameters::SCALING_ON:
      CheckedGurobiCall(
          GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_SCALEFLAG, true));
      CheckedGurobiCall(
          GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_OBJSCALE, 0.0));
      break;
    default:
      // Leave the parameters untouched.
      break;
  }
}

// Sets the LP algorithm : primal, dual or barrier. Note that GRB
// offers automatic selection
void GurobiInterface::SetLpAlgorithm(int value) {
  switch (value) {
    case MPSolverParameters::DUAL:
      CheckedGurobiCall(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                       GRB_METHOD_DUAL));
      break;
    case MPSolverParameters::PRIMAL:
      CheckedGurobiCall(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                       GRB_METHOD_PRIMAL));
      break;
    case MPSolverParameters::BARRIER:
      CheckedGurobiCall(GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_METHOD,
                                       GRB_METHOD_BARRIER));
      break;
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM,
                                        value);
  }
}

int GurobiInterface::SolutionCount() const {
  int solution_count = 0;
  CheckedGurobiCall(
      GRBgetintattr(model_, GRB_INT_ATTR_SOLCOUNT, &solution_count));
  return solution_count;
}

MPSolver::ResultStatus GurobiInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
      MPSolverParameters::INCREMENTALITY_OFF) {
    Reset();
  }

  // TODO(user,user): Support incrementality.
  if (sync_status_ == MUST_RELOAD) {
    Reset();
  }

  // Set log level.
  CheckedGurobiCall(
      GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_OUTPUTFLAG, !quiet_));

  ExtractModel();
  // Sync solver.
  CheckedGurobiCall(GRBupdatemodel(model_));
  VLOG(1) << absl::StrFormat("Model built in %s.",
                             absl::FormatDuration(timer.GetDuration()));

  // Set solution hints if any.
  for (const std::pair<const MPVariable*, double>& p :
       solver_->solution_hint_) {
    CheckedGurobiCall(
        GRBsetdblattrelement(model_, "Start", p.first->index(), p.second));
  }

  // Pass branching priority annotations if at least one has been updated.
  if (update_branching_priorities_) {
    for (const MPVariable* var : solver_->variables_) {
      CheckedGurobiCall(
          GRBsetintattrelement(model_, GRB_INT_ATTR_BRANCHPRIORITY,
                               var->index(), var->branching_priority()));
    }
    update_branching_priorities_ = false;
  }

  // Time limit.
  if (solver_->time_limit() != 0) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    CheckedGurobiCall(GRBsetdblparam(GRBgetenv(model_), GRB_DBL_PAR_TIMELIMIT,
                                     solver_->time_limit_in_secs()));
  }

  // We first set our internal MPSolverParameters from 'param' and then set
  // any user-specified internal solver parameters via
  // solver_specific_parameter_string_.
  // Default MPSolverParameters can override custom parameters (for example for
  // presolving) and therefore we apply MPSolverParameters first.
  SetParameters(param);
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);

  std::unique_ptr<GurobiMPCallbackContext> gurobi_context;
  MPCallbackWithGurobiContext mp_callback_with_context;
  int gurobi_precrush = 0;
  int gurobi_lazy_constraint = 0;
  if (callback_ == nullptr) {
    CheckedGurobiCall(GRBsetcallbackfunc(model_, nullptr, nullptr));
  } else {
    gurobi_context = absl::make_unique<GurobiMPCallbackContext>(
        env_, callback_->might_add_cuts(),
        callback_->might_add_lazy_constraints());
    mp_callback_with_context.context = gurobi_context.get();
    mp_callback_with_context.callback = callback_;
    CheckedGurobiCall(GRBsetcallbackfunc(
        model_, CallbackImpl, static_cast<void*>(&mp_callback_with_context)));
    gurobi_precrush = callback_->might_add_cuts();
    gurobi_lazy_constraint = callback_->might_add_lazy_constraints();
  }
  CheckedGurobiCall(
      GRBsetintparam(GRBgetenv(model_), GRB_INT_PAR_PRECRUSH, gurobi_precrush));
  CheckedGurobiCall(GRBsetintparam(
      GRBgetenv(model_), GRB_INT_PAR_LAZYCONSTRAINTS, gurobi_lazy_constraint));

  // Solve
  timer.Restart();
  const int status = GRBoptimize(model_);

  if (status) {
    VLOG(1) << "Failed to optimize MIP." << GRBgeterrormsg(env_);
  } else {
    VLOG(1) << absl::StrFormat("Solved in %s.",
                               absl::FormatDuration(timer.GetDuration()));
  }

  // Get the status.
  int optimization_status = 0;
  CheckedGurobiCall(
      GRBgetintattr(model_, GRB_INT_ATTR_STATUS, &optimization_status));
  VLOG(1) << absl::StrFormat("Solution status %d.\n", optimization_status);
  const int solution_count = SolutionCount();

  switch (optimization_status) {
    case GRB_OPTIMAL:
      result_status_ = MPSolver::OPTIMAL;
      break;
    case GRB_INFEASIBLE:
      result_status_ = MPSolver::INFEASIBLE;
      break;
    case GRB_UNBOUNDED:
      result_status_ = MPSolver::UNBOUNDED;
      break;
    case GRB_INF_OR_UNBD:
      // TODO(user,user): We could introduce our own "infeasible or
      // unbounded" status.
      result_status_ = MPSolver::INFEASIBLE;
      break;
    default: {
      if (solution_count > 0) {
        result_status_ = MPSolver::FEASIBLE;
      } else {
        result_status_ = MPSolver::NOT_SOLVED;
      }
      break;
    }
  }

  if (solution_count > 0 && (result_status_ == MPSolver::FEASIBLE ||
                             result_status_ == MPSolver::OPTIMAL)) {
    current_solution_index_ = 0;
    // Get the results.
    const int total_num_rows = solver_->constraints_.size();
    const int total_num_cols = solver_->variables_.size();

    {
      std::vector<double> variable_values(total_num_cols);
      CheckedGurobiCall(
          GRBgetdblattr(model_, GRB_DBL_ATTR_OBJVAL, &objective_value_));
      CheckedGurobiCall(GRBgetdblattrarray(
          model_, GRB_DBL_ATTR_X, 0, total_num_cols, variable_values.data()));

      VLOG(1) << "objective = " << objective_value_;
      for (int i = 0; i < solver_->variables_.size(); ++i) {
        MPVariable* const var = solver_->variables_[i];
        var->set_solution_value(variable_values[i]);
        VLOG(3) << var->name() << ", value = " << variable_values[i];
      }
    }
    if (!mip_) {
      {
        std::vector<double> reduced_costs(total_num_cols);
        CheckedGurobiCall(GRBgetdblattrarray(
            model_, GRB_DBL_ATTR_RC, 0, total_num_cols, reduced_costs.data()));
        for (int i = 0; i < solver_->variables_.size(); ++i) {
          MPVariable* const var = solver_->variables_[i];
          var->set_reduced_cost(reduced_costs[i]);
          VLOG(4) << var->name() << ", reduced cost = " << reduced_costs[i];
        }
      }

      {
        std::vector<double> dual_values(total_num_rows);
        CheckedGurobiCall(GRBgetdblattrarray(
            model_, GRB_DBL_ATTR_PI, 0, total_num_rows, dual_values.data()));
        for (int i = 0; i < solver_->constraints_.size(); ++i) {
          MPConstraint* const ct = solver_->constraints_[i];
          ct->set_dual_value(dual_values[i]);
          VLOG(4) << "row " << ct->index()
                  << ", dual value = " << dual_values[i];
        }
      }
    }
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  GRBresetparams(GRBgetenv(model_));
  return result_status_;
}

absl::optional<MPSolutionResponse> GurobiInterface::DirectlySolveProto(
    const MPModelRequest& request) {
  const auto status_or = GurobiSolveProto(request);
  if (status_or.ok()) return status_or.value();
  // Special case: if something is not implemented yet, fall back to solving
  // through MPSolver.
  if (absl::IsUnimplemented(status_or.status())) return absl::nullopt;

  if (request.enable_internal_solver_output()) {
    LOG(INFO) << "Invalid Gurobi status: " << status_or.status();
  }
  MPSolutionResponse response;
  response.set_status(MPSOLVER_NOT_SOLVED);
  response.set_status_str(status_or.status().ToString());
  return std::move(response);
}

bool GurobiInterface::NextSolution() {
  // Next solution only supported for MIP
  if (!mip_) return false;

  // Make sure we have successfully solved the problem and not modified it.
  if (!CheckSolutionIsSynchronizedAndExists()) {
    return false;
  }
  // Check if we are out of solutions.
  if (current_solution_index_ + 1 >= SolutionCount()) {
    return false;
  }
  current_solution_index_++;

  const int total_num_cols = solver_->variables_.size();
  std::vector<double> variable_values(total_num_cols);

  CheckedGurobiCall(GRBsetintparam(
      GRBgetenv(model_), GRB_INT_PAR_SOLUTIONNUMBER, current_solution_index_));

  CheckedGurobiCall(
      GRBgetdblattr(model_, GRB_DBL_ATTR_POOLOBJVAL, &objective_value_));
  CheckedGurobiCall(GRBgetdblattrarray(model_, GRB_DBL_ATTR_XN, 0,
                                       total_num_cols, variable_values.data()));
  for (int i = 0; i < solver_->variables_.size(); ++i) {
    MPVariable* const var = solver_->variables_[i];
    var->set_solution_value(variable_values[i]);
  }
  // TODO(user,user): This reset may not be necessary, investigate.
  GRBresetparams(GRBgetenv(model_));
  return true;
}

void GurobiInterface::Write(const std::string& filename) {
  if (sync_status_ == MUST_RELOAD) {
    Reset();
  }
  ExtractModel();
  // Sync solver.
  CheckedGurobiCall(GRBupdatemodel(model_));
  VLOG(1) << "Writing Gurobi model file \"" << filename << "\".";
  const int status = GRBwrite(model_, filename.c_str());
  if (status) {
    LOG(WARNING) << "Failed to write MIP." << GRBgeterrormsg(env_);
  }
}

bool GurobiInterface::ReadParameterFile(const std::string& filename) {
  // A non-zero return value indicates that a problem occurred.
  return GRBreadparams(GRBgetenv(model_), filename.c_str()) == 0;
}

std::string GurobiInterface::ValidFileExtensionForParameterFile() const {
  return ".prm";
}

MPSolverInterface* BuildGurobiInterface(bool mip, MPSolver* const solver) {
  MPSolver::LoadGurobiSharedLibrary();
  return new GurobiInterface(solver, mip);
}

void GurobiInterface::SetCallback(MPCallback* mp_callback) {
  callback_ = mp_callback;
}

}  // namespace operations_research

