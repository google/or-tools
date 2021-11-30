// Copyright 2010-2021 Google LLC
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

// Gurobi backend to MPSolver.
//
// Implementation Notes:
//
// Incrementalism (last updated June 29, 2020): For solving both LPs and MIPs,
// Gurobi attempts to reuse information from previous solves, potentially
// giving a faster solve time. MPSolver supports this for the following problem
// modification types:
//   * Adding a variable,
//   * Adding a linear constraint,
//   * Updating a variable bound,
//   * Updating an objective coefficient or the objective offset (note that in
//     Gurobi 7.5 LP solver, there is a bug if you update only the objective
//     offset and nothing else).
//   * Updating a coefficient in the constraint matrix.
//   * Updating the type of variable (integer, continuous)
//   * Changing the optimization direction.
// Updates of the following types will force a resolve from scratch:
//   * Updating the upper or lower bounds of a linear constraint. Note that in
//     MPSolver's model, this includes updating the sense (le, ge, eq, range) of
//     a linear constraint.
//   * Clearing a constraint
// Any model containing indicator constraints is considered "non-incremental"
// and will always solve from scratch.
//
// The above limitations are largely due MPSolver and this file, not Gurobi.
//
// Warning(rander): the interactions between callbacks and incrementalism are
// poorly tested, proceed with caution.
//

#include <cmath>
#include <cstddef>
#include <cstdint>
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
#include "ortools/gurobi/environment.h"
#include "ortools/linear_solver/gurobi_proto_solver.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver_callback.h"

ABSL_FLAG(int, num_gurobi_threads, 4,
          "Number of threads available for Gurobi.");

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
      const MPModelRequest& request, std::atomic<bool>* interrupt) override;

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
  void BranchingPriorityChangedForVariable(int var_index) override;

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex or interior-point iterations
  int64_t iterations() const override;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  int64_t nodes() const override;

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
    const absl::MutexLock lock(&hold_interruptions_mutex_);
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

    // TODO(user): Not yet working.
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
  // Sets solver-specific parameters (avoiding using files). The previous
  // implementations supported multi-line strings of the form:
  // parameter_i value_i\n
  // We extend support for strings of the form:
  // parameter1=value1,....,parametern=valuen
  // or for strings of the form:
  // parameter1 value1, ... ,parametern valuen
  // which are easier to set in the command line.
  // This implementations relies on SetSolverSpecificParameters, which has the
  // extra benefit of unifying the way we handle specific parameters for both
  // proto-based solves and for MPModel solves.
  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;
  // Sets each parameter in the underlying solver.
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

  MPSolver::BasisStatus TransformGRBVarBasisStatus(
      int gurobi_basis_status) const;
  MPSolver::BasisStatus TransformGRBConstraintBasisStatus(
      int gurobi_basis_status, int constraint_index) const;

  // See the implementation note at the top of file on incrementalism.
  bool ModelIsNonincremental() const;

  void SetIntAttr(const char* name, int value);
  int GetIntAttr(const char* name) const;
  void SetDoubleAttr(const char* name, double value);
  double GetDoubleAttr(const char* name) const;
  void SetIntAttrElement(const char* name, int index, int value);
  int GetIntAttrElement(const char* name, int index) const;
  void SetDoubleAttrElement(const char* name, int index, double value);
  double GetDoubleAttrElement(const char* name, int index) const;
  std::vector<double> GetDoubleAttrArray(const char* name, int elements);
  void SetCharAttrElement(const char* name, int index, char value);
  char GetCharAttrElement(const char* name, int index) const;

  void CheckedGurobiCall(int err) const;

  int SolutionCount() const;

  GRBmodel* model_;
  GRBenv* env_;
  bool mip_;
  int current_solution_index_;
  MPCallback* callback_ = nullptr;
  bool update_branching_priorities_ = false;
  // Has length equal to the number of MPVariables in
  // MPSolverInterface::solver_. Values are the index of the corresponding
  // Gurobi variable. Note that Gurobi may have additional auxiliary variables
  // not represented by MPVariables, such as those created by two-sided range
  // constraints.
  std::vector<int> mp_var_to_gurobi_var_;
  // Has length equal to the number of MPConstraints in
  // MPSolverInterface::solver_. Values are the index of the corresponding
  // linear (or range) constraint in Gurobi, or -1 if no such constraint exists
  // (e.g. for indicator constraints).
  std::vector<int> mp_cons_to_gurobi_linear_cons_;
  // Should match the Gurobi model after it is updated.
  int num_gurobi_vars_ = 0;
  // Should match the Gurobi model after it is updated.
  // NOTE(user): indicator constraints are not counted below.
  int num_gurobi_linear_cons_ = 0;
  // See the implementation note at the top of file on incrementalism.
  bool had_nonincremental_change_ = false;

  // Mutex is held to prevent InterruptSolve() to call GRBterminate() when
  // model_ is not completely built. It also prevents model_ to be changed
  // during the execution of GRBterminate().
  mutable absl::Mutex hold_interruptions_mutex_;
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
  GurobiMPCallbackContext(GRBenv* env,
                          const std::vector<int>* mp_var_to_gurobi_var,
                          int num_gurobi_vars, bool might_add_cuts,
                          bool might_add_lazy_constraints);

  // Implementation of the interface.
  MPCallbackEvent Event() override;
  bool CanQueryVariableValues() override;
  double VariableValue(const MPVariable* variable) override;
  void AddCut(const LinearRange& cutting_plane) override;
  void AddLazyConstraint(const LinearRange& lazy_constraint) override;
  double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) override;
  int64_t NumExploredNodes() override;

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

  GRBenv* const env_;
  const std::vector<int>* const mp_var_to_gurobi_var_;
  const int num_gurobi_vars_;

  const bool might_add_cuts_;
  const bool might_add_lazy_constraints_;

  // Stateful, updated before each call to the callback.
  GurobiInternalCallbackContext current_gurobi_internal_callback_context_;
  bool variable_values_extracted_ = false;
  std::vector<double> gurobi_variable_values_;
};

void GurobiMPCallbackContext::CheckedGurobiCall(int gurobi_error_code) const {
  ::operations_research::CheckedGurobiCall(gurobi_error_code, env_);
}

GurobiMPCallbackContext::GurobiMPCallbackContext(
    GRBenv* env, const std::vector<int>* mp_var_to_gurobi_var,
    int num_gurobi_vars, bool might_add_cuts, bool might_add_lazy_constraints)
    : env_(ABSL_DIE_IF_NULL(env)),
      mp_var_to_gurobi_var_(ABSL_DIE_IF_NULL(mp_var_to_gurobi_var)),
      num_gurobi_vars_(num_gurobi_vars),
      might_add_cuts_(might_add_cuts),
      might_add_lazy_constraints_(might_add_lazy_constraints) {}

void GurobiMPCallbackContext::UpdateFromGurobiState(
    const GurobiInternalCallbackContext& gurobi_internal_context) {
  current_gurobi_internal_callback_context_ = gurobi_internal_context;
  variable_values_extracted_ = false;
}

int64_t GurobiMPCallbackContext::NumExploredNodes() {
  switch (Event()) {
    case MPCallbackEvent::kMipNode:
      return static_cast<int64_t>(GurobiCallbackGet<double>(
          current_gurobi_internal_callback_context_, GRB_CB_MIPNODE_NODCNT));
    case MPCallbackEvent::kMipSolution:
      return static_cast<int64_t>(GurobiCallbackGet<double>(
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

    gurobi_variable_values_.resize(num_gurobi_vars_);
    CheckedGurobiCall(GRBcbget(
        current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
        current_gurobi_internal_callback_context_.where, gurobi_get_var_param,
        static_cast<void*>(gurobi_variable_values_.data())));
    variable_values_extracted_ = true;
  }
  return gurobi_variable_values_[mp_var_to_gurobi_var_->at(variable->index())];
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
    variable_indices.push_back(
        mp_var_to_gurobi_var_->at(var_coef_pair.first->index()));
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

  std::vector<double> full_solution(num_gurobi_vars_, GRB_UNDEFINED);
  for (const auto& variable_value : solution) {
    const MPVariable* var = variable_value.first;
    full_solution[mp_var_to_gurobi_var_->at(var->index())] =
        variable_value.second;
  }

  double objval;
  CheckedGurobiCall(GRBcbsolution(
      current_gurobi_internal_callback_context_.gurobi_internal_callback_data,
      full_solution.data(), &objval));

  return objval;
}

struct MPCallbackWithGurobiContext {
  GurobiMPCallbackContext* context;
  MPCallback* callback;
};

// NOTE(user): This function must have this exact API, because we are passing
// it to Gurobi as a callback.
int GUROBI_STDCALL CallbackImpl(GRBmodel* model,
                                void* gurobi_internal_callback_data, int where,
                                void* raw_model_and_callback) {
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

void GurobiInterface::SetIntAttr(const char* name, int value) {
  CheckedGurobiCall(GRBsetintattr(model_, name, value));
}

int GurobiInterface::GetIntAttr(const char* name) const {
  int value;
  CheckedGurobiCall(GRBgetintattr(model_, name, &value));
  return value;
}

void GurobiInterface::SetDoubleAttr(const char* name, double value) {
  CheckedGurobiCall(GRBsetdblattr(model_, name, value));
}

double GurobiInterface::GetDoubleAttr(const char* name) const {
  double value;
  CheckedGurobiCall(GRBgetdblattr(model_, name, &value));
  return value;
}

void GurobiInterface::SetIntAttrElement(const char* name, int index,
                                        int value) {
  CheckedGurobiCall(GRBsetintattrelement(model_, name, index, value));
}

int GurobiInterface::GetIntAttrElement(const char* name, int index) const {
  int value;
  CheckedGurobiCall(GRBgetintattrelement(model_, name, index, &value));
  return value;
}

void GurobiInterface::SetDoubleAttrElement(const char* name, int index,
                                           double value) {
  CheckedGurobiCall(GRBsetdblattrelement(model_, name, index, value));
}
double GurobiInterface::GetDoubleAttrElement(const char* name,
                                             int index) const {
  double value;
  CheckedGurobiCall(GRBgetdblattrelement(model_, name, index, &value));
  return value;
}

std::vector<double> GurobiInterface::GetDoubleAttrArray(const char* name,
                                                        int elements) {
  std::vector<double> results(elements);
  CheckedGurobiCall(
      GRBgetdblattrarray(model_, name, 0, elements, results.data()));
  return results;
}

void GurobiInterface::SetCharAttrElement(const char* name, int index,
                                         char value) {
  CheckedGurobiCall(GRBsetcharattrelement(model_, name, index, value));
}
char GurobiInterface::GetCharAttrElement(const char* name, int index) const {
  char value;
  CheckedGurobiCall(GRBgetcharattrelement(model_, name, index, &value));
  return value;
}

// Creates a LP/MIP instance with the specified name and minimization objective.
GurobiInterface::GurobiInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver),
      model_(nullptr),
      env_(nullptr),
      mip_(mip),
      current_solution_index_(0) {
  env_ = GetGurobiEnv().value();
  CheckedGurobiCall(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                0,          // numvars
                                nullptr,    // obj
                                nullptr,    // lb
                                nullptr,    // ub
                                nullptr,    // vtype
                                nullptr));  // varnanes
  SetIntAttr(GRB_INT_ATTR_MODELSENSE, maximize_ ? GRB_MAXIMIZE : GRB_MINIMIZE);
  CheckedGurobiCall(GRBsetintparam(env_, GRB_INT_PAR_THREADS,
                                   absl::GetFlag(FLAGS_num_gurobi_threads)));
}

GurobiInterface::~GurobiInterface() {
  CheckedGurobiCall(GRBfreemodel(model_));
  GRBfreeenv(env_);
}

// ------ Model modifications and extraction -----

void GurobiInterface::Reset() {
  // We hold calls to GRBterminate() until the new model_ is ready.
  const absl::MutexLock lock(&hold_interruptions_mutex_);

  GRBmodel* old_model = model_;
  CheckedGurobiCall(GRBnewmodel(env_, &model_, solver_->name_.c_str(),
                                0,          // numvars
                                nullptr,    // obj
                                nullptr,    // lb
                                nullptr,    // ub
                                nullptr,    // vtype
                                nullptr));  // varnames

  // Copy all existing parameters from the previous model to the new one. This
  // ensures that if a user calls multiple times
  // SetSolverSpecificParametersAsString() and then Reset() is called, we still
  // take into account all parameters.
  //
  // The current code only reapplies the parameters stored in
  // solver_specific_parameter_string_ at the start of the solve; other
  // parameters set by previous calls are only kept in the Gurobi model.
  CheckedGurobiCall(GRBcopyparams(GRBgetenv(model_), GRBgetenv(old_model)));

  CheckedGurobiCall(GRBfreemodel(old_model));
  old_model = nullptr;

  ResetExtractionInformation();
  mp_var_to_gurobi_var_.clear();
  mp_cons_to_gurobi_linear_cons_.clear();
  num_gurobi_vars_ = 0;
  num_gurobi_linear_cons_ = 0;
  had_nonincremental_change_ = false;
}

void GurobiInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  SetIntAttr(GRB_INT_ATTR_MODELSENSE, maximize_ ? GRB_MAXIMIZE : GRB_MINIMIZE);
}

void GurobiInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_ && variable_is_extracted(var_index)) {
    SetDoubleAttrElement(GRB_DBL_ATTR_LB, mp_var_to_gurobi_var_.at(var_index),
                         lb);
    SetDoubleAttrElement(GRB_DBL_ATTR_UB, mp_var_to_gurobi_var_.at(var_index),
                         ub);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::SetVariableInteger(int index, bool integer) {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_ && variable_is_extracted(index)) {
    char type_var;
    if (integer) {
      type_var = GRB_INTEGER;
    } else {
      type_var = GRB_CONTINUOUS;
    }
    SetCharAttrElement(GRB_CHAR_ATTR_VTYPE, mp_var_to_gurobi_var_.at(index),
                       type_var);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::SetConstraintBounds(int index, double lb, double ub) {
  sync_status_ = MUST_RELOAD;
  if (constraint_is_extracted(index)) {
    had_nonincremental_change_ = true;
  }
  // TODO(user): this is nontrivial to make incremental:
  //   1. Make sure it is a linear constraint (not an indicator or indicator
  //      range constraint).
  //   2. Check if the sense of the constraint changes. If it was previously a
  //      range constraint, we can do nothing, and if it becomes a range
  //      constraint, we can do nothing. We could support range constraints if
  //      we tracked the auxiliary variable that is added with range
  //      constraints.
}

void GurobiInterface::AddRowConstraint(MPConstraint* const ct) {
  sync_status_ = MUST_RELOAD;
}

bool GurobiInterface::AddIndicatorConstraint(MPConstraint* const ct) {
  had_nonincremental_change_ = true;
  sync_status_ = MUST_RELOAD;
  return !IsContinuous();
}

void GurobiInterface::AddVariable(MPVariable* const var) {
  sync_status_ = MUST_RELOAD;
}

void GurobiInterface::SetCoefficient(MPConstraint* const constraint,
                                     const MPVariable* const variable,
                                     double new_value, double old_value) {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_ && variable_is_extracted(variable->index()) &&
      constraint_is_extracted(constraint->index())) {
    // Cannot be const, GRBchgcoeffs needs non-const pointer.
    int grb_var = mp_var_to_gurobi_var_.at(variable->index());
    int grb_cons = mp_cons_to_gurobi_linear_cons_.at(constraint->index());
    if (grb_cons < 0) {
      had_nonincremental_change_ = true;
      sync_status_ = MUST_RELOAD;
    } else {
      // TODO(user): investigate if this has bad performance.
      CheckedGurobiCall(
          GRBchgcoeffs(model_, 1, &grb_cons, &grb_var, &new_value));
    }
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::ClearConstraint(MPConstraint* const constraint) {
  had_nonincremental_change_ = true;
  sync_status_ = MUST_RELOAD;
  // TODO(user): this is difficult to make incremental, like
  //  SetConstraintBounds(), because of the auxiliary Gurobi variables that
  //  range constraints introduce.
}

void GurobiInterface::SetObjectiveCoefficient(const MPVariable* const variable,
                                              double coefficient) {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_ && variable_is_extracted(variable->index())) {
    SetDoubleAttrElement(GRB_DBL_ATTR_OBJ,
                         mp_var_to_gurobi_var_.at(variable->index()),
                         coefficient);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::SetObjectiveOffset(double value) {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_) {
    SetDoubleAttr(GRB_DBL_ATTR_OBJCON, value);
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::ClearObjective() {
  InvalidateSolutionSynchronization();
  if (!had_nonincremental_change_) {
    SetObjectiveOffset(0.0);
    for (const auto& entry : solver_->objective_->coefficients_) {
      SetObjectiveCoefficient(entry.first, 0.0);
    }
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void GurobiInterface::BranchingPriorityChangedForVariable(int var_index) {
  update_branching_priorities_ = true;
}

// ------ Query statistics on the solution and the solve ------

int64_t GurobiInterface::iterations() const {
  double iter;
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  CheckedGurobiCall(GRBgetdblattr(model_, GRB_DBL_ATTR_ITERCOUNT, &iter));
  return static_cast<int64_t>(iter);
}

int64_t GurobiInterface::nodes() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    return static_cast<int64_t>(GetDoubleAttr(GRB_DBL_ATTR_NODECOUNT));
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems.";
    return kUnknownNumberOfNodes;
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
  const int grb_index = mp_cons_to_gurobi_linear_cons_.at(constraint_index);
  if (grb_index < 0) {
    LOG(DFATAL) << "Basis status not available for nonlinear constraints.";
    return MPSolver::FREE;
  }
  switch (gurobi_basis_status) {
    case GRB_BASIC:
      return MPSolver::BASIC;
    default: {
      // Non basic.
      double tolerance = 0.0;
      CheckedGurobiCall(GRBgetdblparam(GRBgetenv(model_),
                                       GRB_DBL_PAR_FEASIBILITYTOL, &tolerance));
      const double slack = GetDoubleAttrElement(GRB_DBL_ATTR_SLACK, grb_index);
      const char sense = GetCharAttrElement(GRB_CHAR_ATTR_SENSE, grb_index);
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
  const int optim_status = GetIntAttr(GRB_INT_ATTR_STATUS);
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  const int grb_index = mp_cons_to_gurobi_linear_cons_.at(constraint_index);
  if (grb_index < 0) {
    LOG(DFATAL) << "Basis status not available for nonlinear constraints.";
    return MPSolver::FREE;
  }
  const int gurobi_basis_status =
      GetIntAttrElement(GRB_INT_ATTR_CBASIS, grb_index);
  return TransformGRBConstraintBasisStatus(gurobi_basis_status,
                                           constraint_index);
}

// Returns the basis status of a column.
MPSolver::BasisStatus GurobiInterface::column_status(int variable_index) const {
  const int optim_status = GetIntAttr(GRB_INT_ATTR_STATUS);
  if (optim_status != GRB_OPTIMAL && optim_status != GRB_SUBOPTIMAL) {
    LOG(DFATAL) << "Basis status only available after a solution has "
                << "been found.";
    return MPSolver::FREE;
  }
  if (mip_) {
    LOG(DFATAL) << "Basis status only available for continuous problems.";
    return MPSolver::FREE;
  }
  const int grb_index = mp_var_to_gurobi_var_.at(variable_index);
  const int gurobi_basis_status =
      GetIntAttrElement(GRB_INT_ATTR_VBASIS, grb_index);
  return TransformGRBVarBasisStatus(gurobi_basis_status);
}

// Extracts new variables.
void GurobiInterface::ExtractNewVariables() {
  const int total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    // Define new variables.
    for (int j = last_variable_index_; j < total_num_vars; ++j) {
      const MPVariable* const var = solver_->variables_.at(j);
      set_variable_as_extracted(var->index(), true);
      CheckedGurobiCall(GRBaddvar(
          model_, 0,  // numnz
          nullptr,    // vind
          nullptr,    // vval
          solver_->objective_->GetCoefficient(var), var->lb(), var->ub(),
          var->integer() && mip_ ? GRB_INTEGER : GRB_CONTINUOUS,
          var->name().empty() ? nullptr : var->name().c_str()));
      mp_var_to_gurobi_var_.push_back(num_gurobi_vars_++);
    }
    CheckedGurobiCall(GRBupdatemodel(model_));
    // Add new variables to existing constraints.
    std::vector<int> grb_cons_ind;
    std::vector<int> grb_var_ind;
    std::vector<double> coef;
    for (int i = 0; i < last_constraint_index_; ++i) {
      // If there was a nonincremental change/the model is not incremental (e.g.
      // there is an indicator constraint), we should never enter this loop, as
      // last_variable_index_ will be reset to zero before ExtractNewVariables()
      // is called.
      MPConstraint* const ct = solver_->constraints_[i];
      const int grb_ct_idx = mp_cons_to_gurobi_linear_cons_.at(ct->index());
      DCHECK_GE(grb_ct_idx, 0);
      DCHECK(ct->indicator_variable() == nullptr);
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));

        if (var_index >= last_variable_index_) {
          grb_cons_ind.push_back(grb_ct_idx);
          grb_var_ind.push_back(mp_var_to_gurobi_var_.at(var_index));
          coef.push_back(entry.second);
        }
      }
    }
    if (!grb_cons_ind.empty()) {
      CheckedGurobiCall(GRBchgcoeffs(model_, grb_cons_ind.size(),
                                     grb_cons_ind.data(), grb_var_ind.data(),
                                     coef.data()));
    }
  }
  CheckedGurobiCall(GRBupdatemodel(model_));
  DCHECK_EQ(GetIntAttr(GRB_INT_ATTR_NUMVARS), num_gurobi_vars_);
}

void GurobiInterface::ExtractNewConstraints() {
  int total_num_rows = solver_->constraints_.size();
  if (last_constraint_index_ < total_num_rows) {
    // Add each new constraint.
    for (int row = last_constraint_index_; row < total_num_rows; ++row) {
      MPConstraint* const ct = solver_->constraints_[row];
      set_constraint_as_extracted(row, true);
      const int size = ct->coefficients_.size();
      std::vector<int> grb_vars;
      std::vector<double> coefs;
      grb_vars.reserve(size);
      coefs.reserve(size);
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        CHECK(variable_is_extracted(var_index));
        grb_vars.push_back(mp_var_to_gurobi_var_.at(var_index));
        coefs.push_back(entry.second);
      }
      char* const name =
          ct->name().empty() ? nullptr : const_cast<char*>(ct->name().c_str());
      if (ct->indicator_variable() != nullptr) {
        const int grb_ind_var =
            mp_var_to_gurobi_var_.at(ct->indicator_variable()->index());
        if (ct->lb() > -std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddgenconstrIndicator(
              model_, name, grb_ind_var, ct->indicator_value(), size,
              grb_vars.data(), coefs.data(),
              ct->ub() == ct->lb() ? GRB_EQUAL : GRB_GREATER_EQUAL, ct->lb()));
        }
        if (ct->ub() < std::numeric_limits<double>::infinity() &&
            ct->lb() != ct->ub()) {
          CheckedGurobiCall(GRBaddgenconstrIndicator(
              model_, name, grb_ind_var, ct->indicator_value(), size,
              grb_vars.data(), coefs.data(), GRB_LESS_EQUAL, ct->ub()));
        }
        mp_cons_to_gurobi_linear_cons_.push_back(-1);
      } else {
        // Using GRBaddrangeconstr for constraints that don't require it adds
        // a slack which is not always removed by presolve.
        if (ct->lb() == ct->ub()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, grb_vars.data(),
                                         coefs.data(), GRB_EQUAL, ct->lb(),
                                         name));
        } else if (ct->lb() == -std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, grb_vars.data(),
                                         coefs.data(), GRB_LESS_EQUAL, ct->ub(),
                                         name));
        } else if (ct->ub() == std::numeric_limits<double>::infinity()) {
          CheckedGurobiCall(GRBaddconstr(model_, size, grb_vars.data(),
                                         coefs.data(), GRB_GREATER_EQUAL,
                                         ct->lb(), name));
        } else {
          CheckedGurobiCall(GRBaddrangeconstr(model_, size, grb_vars.data(),
                                              coefs.data(), ct->lb(), ct->ub(),
                                              name));
          // NOTE(user): range constraints implicitly add an extra variable
          // to the model.
          num_gurobi_vars_++;
        }
        mp_cons_to_gurobi_linear_cons_.push_back(num_gurobi_linear_cons_++);
      }
    }
  }
  CheckedGurobiCall(GRBupdatemodel(model_));
  DCHECK_EQ(GetIntAttr(GRB_INT_ATTR_NUMCONSTRS), num_gurobi_linear_cons_);
}

void GurobiInterface::ExtractObjective() {
  SetIntAttr(GRB_INT_ATTR_MODELSENSE, maximize_ ? GRB_MAXIMIZE : GRB_MINIMIZE);
  SetDoubleAttr(GRB_DBL_ATTR_OBJCON, solver_->Objective().offset());
}

// ------ Parameters  -----

void GurobiInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  if (mip_) {
    SetMIPParameters(param);
  }
}

bool GurobiInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  return SetSolverSpecificParameters(parameters, GRBgetenv(model_)).ok();
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
  return GetIntAttr(GRB_INT_ATTR_SOLCOUNT);
}

bool GurobiInterface::ModelIsNonincremental() const {
  for (const MPConstraint* c : solver_->constraints()) {
    if (c->indicator_variable() != nullptr) {
      return true;
    }
  }
  return false;
}

MPSolver::ResultStatus GurobiInterface::Solve(const MPSolverParameters& param) {
  WallTimer timer;
  timer.Start();

  if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
          MPSolverParameters::INCREMENTALITY_OFF ||
      ModelIsNonincremental() || had_nonincremental_change_) {
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
    SetDoubleAttrElement(GRB_DBL_ATTR_START,
                         mp_var_to_gurobi_var_.at(p.first->index()), p.second);
  }

  // Pass branching priority annotations if at least one has been updated.
  if (update_branching_priorities_) {
    for (const MPVariable* var : solver_->variables_) {
      SetIntAttrElement(GRB_INT_ATTR_BRANCHPRIORITY,
                        mp_var_to_gurobi_var_.at(var->index()),
                        var->branching_priority());
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
        env_, &mp_var_to_gurobi_var_, num_gurobi_vars_,
        callback_->might_add_cuts(), callback_->might_add_lazy_constraints());
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
  const int optimization_status = GetIntAttr(GRB_INT_ATTR_STATUS);
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
      // TODO(user): We could introduce our own "infeasible or
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

  if (IsMIP() && (result_status_ != MPSolver::UNBOUNDED &&
                  result_status_ != MPSolver::INFEASIBLE)) {
    const int error =
        GRBgetdblattr(model_, GRB_DBL_ATTR_OBJBOUND, &best_objective_bound_);
    LOG_IF(WARNING, error != 0)
        << "Best objective bound is not available, error=" << error
        << ", message=" << GRBgeterrormsg(env_);
    VLOG(1) << "best bound = " << best_objective_bound_;
  }

  if (solution_count > 0 && (result_status_ == MPSolver::FEASIBLE ||
                             result_status_ == MPSolver::OPTIMAL)) {
    current_solution_index_ = 0;
    // Get the results.
    objective_value_ = GetDoubleAttr(GRB_DBL_ATTR_OBJVAL);
    VLOG(1) << "objective = " << objective_value_;

    {
      const std::vector<double> grb_variable_values =
          GetDoubleAttrArray(GRB_DBL_ATTR_X, num_gurobi_vars_);
      for (int i = 0; i < solver_->variables_.size(); ++i) {
        MPVariable* const var = solver_->variables_[i];
        const double val = grb_variable_values.at(mp_var_to_gurobi_var_.at(i));
        var->set_solution_value(val);
        VLOG(3) << var->name() << ", value = " << val;
      }
    }
    if (!mip_) {
      {
        const std::vector<double> grb_reduced_costs =
            GetDoubleAttrArray(GRB_DBL_ATTR_RC, num_gurobi_vars_);
        for (int i = 0; i < solver_->variables_.size(); ++i) {
          MPVariable* const var = solver_->variables_[i];
          const double rc = grb_reduced_costs.at(mp_var_to_gurobi_var_.at(i));
          var->set_reduced_cost(rc);
          VLOG(4) << var->name() << ", reduced cost = " << rc;
        }
      }

      {
        std::vector<double> grb_dual_values =
            GetDoubleAttrArray(GRB_DBL_ATTR_PI, num_gurobi_linear_cons_);
        for (int i = 0; i < solver_->constraints_.size(); ++i) {
          MPConstraint* const ct = solver_->constraints_[i];
          const double dual_value =
              grb_dual_values.at(mp_cons_to_gurobi_linear_cons_.at(i));
          ct->set_dual_value(dual_value);
          VLOG(4) << "row " << ct->index() << ", dual value = " << dual_value;
        }
      }
    }
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  GRBresetparams(GRBgetenv(model_));
  return result_status_;
}

absl::optional<MPSolutionResponse> GurobiInterface::DirectlySolveProto(
    const MPModelRequest& request, std::atomic<bool>* interrupt) {
  // Interruption via atomic<bool> is not directly supported by Gurobi.
  if (interrupt != nullptr) return absl::nullopt;

  // Here we reuse the Gurobi environment to support single-use license that
  // forbids creating a second environment if one already exists.
  const auto status_or = GurobiSolveProto(request, env_);
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
  return response;
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

  CheckedGurobiCall(GRBsetintparam(
      GRBgetenv(model_), GRB_INT_PAR_SOLUTIONNUMBER, current_solution_index_));

  objective_value_ = GetDoubleAttr(GRB_DBL_ATTR_POOLOBJVAL);
  const std::vector<double> grb_variable_values =
      GetDoubleAttrArray(GRB_DBL_ATTR_XN, num_gurobi_vars_);

  for (int i = 0; i < solver_->variables_.size(); ++i) {
    MPVariable* const var = solver_->variables_[i];
    var->set_solution_value(
        grb_variable_values.at(mp_var_to_gurobi_var_.at(i)));
  }
  // TODO(user): This reset may not be necessary, investigate.
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

MPSolverInterface* BuildGurobiInterface(bool mip, MPSolver* const solver) {
  return new GurobiInterface(solver, mip);
}

void GurobiInterface::SetCallback(MPCallback* mp_callback) {
  callback_ = mp_callback;
}

}  // namespace operations_research
