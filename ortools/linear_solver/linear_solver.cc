// Copyright 2010-2014 Google
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

//

#include "ortools/linear_solver/linear_solver.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#endif


#include <cmath>
#include <cstddef>
#include <utility>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"

#ifndef ANDROID_JNI
#include "ortools/base/file.h"
#endif


#ifdef ANDROID_JNI
#include "ortools/base/numbers.h"
#endif  /// ANDROID_JNI

#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/util/fp_utils.h"
#ifndef ANDROID_JNI
#include "ortools/util/proto_tools.h"
#endif

// TODO(user): Clean up includes. E.g., parameters.pb.h seems not used.

DEFINE_bool(verify_solution, false,
            "Systematically verify the solution when calling Solve()"
            ", and change the return value of Solve() to ABNORMAL if"
            " an error was detected.");
DEFINE_bool(log_verification_errors, true,
            "If --verify_solution is set: LOG(ERROR) all errors detected"
            " during the verification of the solution.");
DEFINE_bool(linear_solver_enable_verbose_output, false,
            "If set, enables verbose output for the solver. Setting this flag"
            " is the same as calling MPSolver::EnableOutput().");

DEFINE_bool(mpsolver_bypass_model_validation, false,
            "If set, the user-provided Model won't be verified before Solve()."
            " Invalid models will typically trigger various error responses"
            " from the underlying solvers; sometimes crashes.");


// To compile the open-source code, the anonymous namespace should be
// inside the operations_research namespace (This is due to the
// open-sourced version of StringPrintf which is defined inside the
// operations_research namespace in open_source/base).
namespace operations_research {

#if defined(ANDROID_JNI) && (defined(__ANDROID__) || defined(__APPLE__))
// Enum -> std::string conversions are not present in MessageLite that is being used
// on Android.
std::string MPSolverResponseStatus_Name(int status) {
  return SimpleItoa(status);
}

std::string MPModelRequest_SolverType_Name(int type) {
  return SimpleItoa(type);
}
#endif  // defined(ANDROID_JNI) && (defined(__ANDROID__) || defined(__APPLE__))

double MPConstraint::GetCoefficient(const MPVariable* const var) const {
  DLOG_IF(DFATAL, !interface_->solver_->OwnsVariable(var)) << var;
  if (var == nullptr) return 0.0;
  return FindWithDefault(coefficients_, var, 0.0);
}

void MPConstraint::SetCoefficient(const MPVariable* const var, double coeff) {
  DLOG_IF(DFATAL, !interface_->solver_->OwnsVariable(var)) << var;
  if (var == nullptr) return;
  if (coeff == 0.0) {
    CoeffMap::iterator it = coefficients_.find(var);
    // If setting a coefficient to 0 when this coefficient did not
    // exist or was already 0, do nothing: skip
    // interface_->SetCoefficient() and do not store a coefficient in
    // the map.  Note that if the coefficient being set to 0 did exist
    // and was not 0, we do have to keep a 0 in the coefficients_ map,
    // because the extraction of the constraint might rely on it,
    // depending on the underlying solver.
    if (it != coefficients_.end() && it->second != 0.0) {
      const double old_value = it->second;
      it->second = 0.0;
      interface_->SetCoefficient(this, var, 0.0, old_value);
    }
    return;
  }
  std::pair<CoeffMap::iterator, bool> insertion_result =
      coefficients_.insert(std::make_pair(var, coeff));
  const double old_value =
      insertion_result.second ? 0.0 : insertion_result.first->second;
  insertion_result.first->second = coeff;
  interface_->SetCoefficient(this, var, coeff, old_value);
}

void MPConstraint::Clear() {
  interface_->ClearConstraint(this);
  coefficients_.clear();
}

void MPConstraint::SetBounds(double lb, double ub) {
  const bool change = lb != lb_ || ub != ub_;
  lb_ = lb;
  ub_ = ub;
  if (change && interface_->constraint_is_extracted(index_)) {
    interface_->SetConstraintBounds(index_, lb_, ub_);
  }
}

double MPConstraint::dual_value() const {
  if (!interface_->IsContinuous()) {
    LOG(DFATAL) << "Dual value only available for continuous problems";
    return 0.0;
  }
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) return 0.0;
  return dual_value_;
}

MPSolver::BasisStatus MPConstraint::basis_status() const {
  if (!interface_->IsContinuous()) {
    LOG(DFATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) {
    return MPSolver::FREE;
  }
  // This is done lazily as this method is expected to be rarely used.
  return interface_->row_status(index_);
}

bool MPConstraint::ContainsNewVariables() {
  const int last_variable_index = interface_->last_variable_index();
  for (CoeffEntry entry : coefficients_) {
    const int variable_index = entry.first->index();
    if (variable_index >= last_variable_index ||
        !interface_->variable_is_extracted(variable_index)) {
      return true;
    }
  }
  return false;
}

// ----- MPObjective -----

double MPObjective::GetCoefficient(const MPVariable* const var) const {
  DLOG_IF(DFATAL, !interface_->solver_->OwnsVariable(var)) << var;
  if (var == nullptr) return 0.0;
  return FindWithDefault(coefficients_, var, 0.0);
}

void MPObjective::SetCoefficient(const MPVariable* const var, double coeff) {
  DLOG_IF(DFATAL, !interface_->solver_->OwnsVariable(var)) << var;
  if (var == nullptr) return;
  if (coeff == 0.0) {
    CoeffMap::iterator it = coefficients_.find(var);
    // See the discussion on MPConstraint::SetCoefficient() for 0 coefficients,
    // the same reasoning applies here.
    if (it == coefficients_.end() || it->second == 0.0) return;
    it->second = 0.0;
  } else {
    coefficients_[var] = coeff;
  }
  interface_->SetObjectiveCoefficient(var, coeff);
}

void MPObjective::SetOffset(double value) {
  offset_ = value;
  interface_->SetObjectiveOffset(offset_);
}

namespace {
void CheckLinearExpr(const MPSolver& solver, const LinearExpr& linear_expr) {
  for (auto var_value_pair : linear_expr.terms()) {
    CHECK(solver.OwnsVariable(var_value_pair.first))
        << "Bad MPVariable* in LinearExpr, did you try adding an integer to an "
           "MPVariable* directly?";
  }
}
}  // namespace

void MPObjective::OptimizeLinearExpr(const LinearExpr& linear_expr,
                                     bool is_maximization) {
  CheckLinearExpr(*interface_->solver_, linear_expr);
  interface_->ClearObjective();
  coefficients_.clear();
  offset_ = linear_expr.offset();
  for (const auto& kv : linear_expr.terms()) {
    SetCoefficient(kv.first, kv.second);
  }
  SetOptimizationDirection(is_maximization);
}

void MPObjective::AddLinearExpr(const LinearExpr& linear_expr) {
  CheckLinearExpr(*interface_->solver_, linear_expr);
  offset_ += linear_expr.offset();
  for (const auto& kv : linear_expr.terms()) {
    SetCoefficient(kv.first, GetCoefficient(kv.first) + kv.second);
  }
}

void MPObjective::Clear() {
  interface_->ClearObjective();
  coefficients_.clear();
  offset_ = 0.0;
  SetMinimization();
}

void MPObjective::SetOptimizationDirection(bool maximize) {
  // Note(user): The maximize_ bool would more naturally belong to the
  // MPObjective, but it actually has to be a member of MPSolverInterface,
  // because some implementations (such as GLPK) need that bool for the
  // MPSolverInterface constructor, i.e at a time when the MPObjective is not
  // constructed yet (MPSolverInterface is always built before MPObjective
  // when a new MPSolver is constructed).
  interface_->maximize_ = maximize;
  interface_->SetOptimizationDirection(maximize);
}

bool MPObjective::maximization() const { return interface_->maximize_; }

bool MPObjective::minimization() const { return !interface_->maximize_; }

double MPObjective::Value() const {
  // Note(user): implementation-wise, the objective value belongs more
  // naturally to the MPSolverInterface, since all of its implementations write
  // to it directly.
  return interface_->objective_value();
}

double MPObjective::BestBound() const {
  // Note(user): the best objective bound belongs to the interface for the
  // same reasons as the objective value does.
  return interface_->best_objective_bound();
}

// ----- MPVariable -----

double MPVariable::solution_value() const {
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) return 0.0;
  return integer_ ? round(solution_value_) : solution_value_;
}

double MPVariable::unrounded_solution_value() const {
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) return 0.0;
  return solution_value_;
}

double MPVariable::reduced_cost() const {
  if (!interface_->IsContinuous()) {
    LOG(DFATAL) << "Reduced cost only available for continuous problems";
    return 0.0;
  }
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) return 0.0;
  return reduced_cost_;
}

MPSolver::BasisStatus MPVariable::basis_status() const {
  if (!interface_->IsContinuous()) {
    LOG(DFATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) {
    return MPSolver::FREE;
  }
  // This is done lazily as this method is expected to be rarely used.
  return interface_->column_status(index_);
}

void MPVariable::SetBounds(double lb, double ub) {
  const bool change = lb != lb_ || ub != ub_;
  lb_ = lb;
  ub_ = ub;
  if (change && interface_->variable_is_extracted(index_)) {
    interface_->SetVariableBounds(index_, lb_, ub_);
  }
}

void MPVariable::SetInteger(bool integer) {
  if (integer_ != integer) {
    integer_ = integer;
    if (interface_->variable_is_extracted(index_)) {
      interface_->SetVariableInteger(index_, integer);
    }
  }
}

// ----- Interface shortcuts -----

bool MPSolver::IsMIP() const { return interface_->IsMIP(); }

std::string MPSolver::SolverVersion() const { return interface_->SolverVersion(); }

void* MPSolver::underlying_solver() { return interface_->underlying_solver(); }

// ---- Solver-specific parameters ----

bool MPSolver::SetSolverSpecificParametersAsString(const std::string& parameters) {
  solver_specific_parameter_string_ = parameters;
  return interface_->SetSolverSpecificParametersAsString(parameters);
}

// ----- Solver -----

#if defined(USE_CLP) || defined(USE_CBC)
extern MPSolverInterface* BuildCLPInterface(MPSolver* const solver);
#endif
#if defined(USE_CBC)
extern MPSolverInterface* BuildCBCInterface(MPSolver* const solver);
#endif
#if defined(USE_GLPK)
extern MPSolverInterface* BuildGLPKInterface(bool mip, MPSolver* const solver);
#endif
#if defined(USE_BOP)
extern MPSolverInterface* BuildBopInterface(MPSolver* const solver);
#endif
#if defined(USE_GLOP)
extern MPSolverInterface* BuildGLOPInterface(MPSolver* const solver);
#endif
#if defined(USE_SCIP)
extern MPSolverInterface* BuildSCIPInterface(MPSolver* const solver);
#endif
#if defined(USE_GUROBI)
extern MPSolverInterface* BuildGurobiInterface(bool mip,
                                               MPSolver* const solver);
#endif
#if defined(USE_CPLEX)
extern MPSolverInterface* BuildCplexInterface(bool mip, MPSolver* const solver);
#endif

#ifdef ANDROID_JNI
extern MPSolverInterface* BuildGLOPInterface(MPSolver* const solver);
#endif

namespace {
MPSolverInterface* BuildSolverInterface(MPSolver* const solver) {
  DCHECK(solver != nullptr);
  switch (solver->ProblemType()) {
#if defined(USE_BOP)
    case MPSolver::BOP_INTEGER_PROGRAMMING:
      return BuildBopInterface(solver);
#endif
#if defined(USE_GLOP)
    case MPSolver::GLOP_LINEAR_PROGRAMMING:
      return BuildGLOPInterface(solver);
#endif
#if defined(USE_GLPK)
    case MPSolver::GLPK_LINEAR_PROGRAMMING:
      return BuildGLPKInterface(false, solver);
    case MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING:
      return BuildGLPKInterface(true, solver);
#endif
#if defined(USE_CLP) || defined(USE_CBC)
    case MPSolver::CLP_LINEAR_PROGRAMMING:
      return BuildCLPInterface(solver);
#endif
#if defined(USE_CBC)
    case MPSolver::CBC_MIXED_INTEGER_PROGRAMMING:
      return BuildCBCInterface(solver);
#endif
#if defined(USE_SCIP)
    case MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING:
      return BuildSCIPInterface(solver);
#endif
#if defined(USE_GUROBI)
    case MPSolver::GUROBI_LINEAR_PROGRAMMING:
      return BuildGurobiInterface(false, solver);
    case MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING:
      return BuildGurobiInterface(true, solver);
#endif
#if defined(USE_CPLEX)
    case MPSolver::CPLEX_LINEAR_PROGRAMMING:
      return BuildCplexInterface(false, solver);
    case MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING:
      return BuildCplexInterface(true, solver);
#endif
    default:
      // TODO(user): Revert to the best *available* interface.
      LOG(FATAL) << "Linear solver not recognized.";
  }
  return NULL;
}
}  // namespace

namespace {
int NumDigits(int n) {
// Number of digits needed to write a non-negative integer in base 10.
// Note(user): std::max(1, log(0) + 1) == std::max(1, -inf) == 1.
#if defined(_MSC_VER)
  return static_cast<int>(std::max(1.0L, log(1.0L * n) / log(10.0L) + 1.0));
#else
  return static_cast<int>(std::max(1.0, log10(static_cast<double>(n)) + 1.0));
#endif
}
}  // namespace

MPSolver::MPSolver(const std::string& name, OptimizationProblemType problem_type)
    : name_(name),
      problem_type_(problem_type),
#if !defined(_MSC_VER)
      variable_name_to_index_(1),
      constraint_name_to_index_(1),
#endif
      time_limit_(0.0) {
  timer_.Restart();
  interface_.reset(BuildSolverInterface(this));
  if (FLAGS_linear_solver_enable_verbose_output) {
    EnableOutput();
  }
  objective_.reset(new MPObjective(interface_.get()));
}

MPSolver::~MPSolver() { Clear(); }

// static
bool MPSolver::SupportsProblemType(OptimizationProblemType problem_type) {
#ifdef USE_CLP
    if (problem_type == CLP_LINEAR_PROGRAMMING) return true;
    #endif
    #ifdef USE_GLPK
    if (problem_type == GLPK_LINEAR_PROGRAMMING) return true;
    if (problem_type == GLPK_MIXED_INTEGER_PROGRAMMING) return true;
    #endif
    #ifdef USE_BOP
    if (problem_type == BOP_INTEGER_PROGRAMMING) return true;
    #endif
    #ifdef USE_GLOP
    if (problem_type == GLOP_LINEAR_PROGRAMMING) return true;
    #endif
    #ifdef USE_GUROBI
    if (problem_type == GUROBI_LINEAR_PROGRAMMING) return true;
    if (problem_type == GUROBI_MIXED_INTEGER_PROGRAMMING) return true;
    #endif
    #ifdef USE_SCIP
    if (problem_type == SCIP_MIXED_INTEGER_PROGRAMMING) return true;
    #endif
    #ifdef USE_CBC
    if (problem_type == CBC_MIXED_INTEGER_PROGRAMMING) return true;
    #endif
    return false;
}

MPVariable* MPSolver::LookupVariableOrNull(const std::string& var_name) const {
  std::unordered_map<std::string, int>::const_iterator it =
      variable_name_to_index_.find(var_name);
  if (it == variable_name_to_index_.end()) return nullptr;
  return variables_[it->second];
}

MPConstraint* MPSolver::LookupConstraintOrNull(const std::string& constraint_name)
    const {
  std::unordered_map<std::string, int>::const_iterator it =
      constraint_name_to_index_.find(constraint_name);
  if (it == constraint_name_to_index_.end()) return nullptr;
  return constraints_[it->second];
}

// ----- Methods using protocol buffers -----

MPSolverResponseStatus MPSolver::LoadModelFromProto(
    const MPModelProto& input_model, std::string* error_message) {
  // The variable and constraint names are dropped, because we allow
  // duplicate names in the proto (they're not considered as 'ids'),
  // unlike the MPSolver C++ API which crashes if there are duplicate names.
  // Clearing the names makes the MPSolver generate unique names.
  //
  // TODO(user): This limits the number of variables and constraints to 10^9:
  // we should fix that.
  return LoadModelFromProtoInternal(input_model, /*clear_names=*/true,
                                    error_message);
}

MPSolverResponseStatus MPSolver::LoadModelFromProtoWithUniqueNamesOrDie(
    const MPModelProto& input_model, std::string* error_message) {
  return LoadModelFromProtoInternal(input_model, /*clear_names=*/false,
                                    error_message);
}

MPSolverResponseStatus MPSolver::LoadModelFromProtoInternal(
    const MPModelProto& input_model, bool clear_names, std::string* error_message) {
  CHECK(error_message != nullptr);
  const std::string error = FindErrorInMPModelProto(input_model);
  if (!error.empty()) {
    *error_message = error;
    LOG_IF(INFO, OutputIsEnabled())
        << "Invalid model given to LoadModelFromProto(): " << error;
    if (FLAGS_mpsolver_bypass_model_validation) {
      LOG_IF(INFO, OutputIsEnabled())
          << "Ignoring the model error(s) because of"
          << " --mpsolver_bypass_model_validation.";
    } else {
      return error.find("Infeasible") == std::string::npos ? MPSOLVER_MODEL_INVALID
                                                      : MPSOLVER_INFEASIBLE;
    }
  }

  MPObjective* const objective = MutableObjective();
  // Passing empty names makes the MPSolver generate unique names.
  const std::string empty;
  for (int i = 0; i < input_model.variable_size(); ++i) {
    const MPVariableProto& var_proto = input_model.variable(i);
    MPVariable* variable =
        MakeNumVar(var_proto.lower_bound(), var_proto.upper_bound(),
                   clear_names ? empty : var_proto.name());
    variable->SetInteger(var_proto.is_integer());
    objective->SetCoefficient(variable, var_proto.objective_coefficient());
  }

  for (int i = 0; i < input_model.constraint_size(); ++i) {
    const MPConstraintProto& ct_proto = input_model.constraint(i);
    MPConstraint* const ct =
        MakeRowConstraint(ct_proto.lower_bound(), ct_proto.upper_bound(),
                          clear_names ? empty : ct_proto.name());
    ct->set_is_lazy(ct_proto.is_lazy());
    for (int j = 0; j < ct_proto.var_index_size(); ++j) {
      ct->SetCoefficient(variables_[ct_proto.var_index(j)],
                         ct_proto.coefficient(j));
    }
  }
  objective->SetOptimizationDirection(input_model.maximize());
  if (input_model.has_objective_offset()) {
    objective->SetOffset(input_model.objective_offset());
  }

  // Stores any hints about where to start the solve.
  solution_hint_.clear();
  for (int i = 0; i < input_model.solution_hint().var_index_size(); ++i) {
    solution_hint_.push_back(
        std::make_pair(variables_[input_model.solution_hint().var_index(i)],
                       input_model.solution_hint().var_value(i)));
  }
  return MPSOLVER_MODEL_IS_VALID;
}

namespace {
MPSolverResponseStatus ResultStatusToMPSolverResponseStatus(
    MPSolver::ResultStatus status) {
  switch (status) {
    case MPSolver::OPTIMAL:
      return MPSOLVER_OPTIMAL;
    case MPSolver::FEASIBLE:
      return MPSOLVER_FEASIBLE;
    case MPSolver::INFEASIBLE:
      return MPSOLVER_INFEASIBLE;
    case MPSolver::UNBOUNDED:
      return MPSOLVER_UNBOUNDED;
    case MPSolver::ABNORMAL:
      return MPSOLVER_ABNORMAL;
    case MPSolver::MODEL_INVALID:
      return MPSOLVER_MODEL_INVALID;
    case MPSolver::NOT_SOLVED:
      return MPSOLVER_NOT_SOLVED;
  }
  return MPSOLVER_UNKNOWN_STATUS;
}
}  // namespace

void MPSolver::FillSolutionResponseProto(MPSolutionResponse* response) const {
  CHECK_NOTNULL(response);
  response->Clear();
  response->set_status(
      ResultStatusToMPSolverResponseStatus(interface_->result_status_));
  if (interface_->result_status_ == MPSolver::OPTIMAL ||
      interface_->result_status_ == MPSolver::FEASIBLE) {
    response->set_objective_value(Objective().Value());
    for (int i = 0; i < variables_.size(); ++i) {
      response->add_variable_value(variables_[i]->solution_value());
    }

    if (interface_->IsMIP()) {
      response->set_best_objective_bound(interface_->best_objective_bound());
    } else {
      // Dual values have no meaning in MIP.
      for (int j = 0; j < constraints_.size(); ++j) {
        response->add_dual_value(constraints_[j]->dual_value());
      }
    }
  }
}

// static
void MPSolver::SolveWithProto(const MPModelRequest& model_request,
                              MPSolutionResponse* response) {
  CHECK_NOTNULL(response);
  const MPModelProto& model = model_request.model();
  MPSolver solver(model.name(), static_cast<MPSolver::OptimizationProblemType>(
                                    model_request.solver_type()));
  if (model_request.enable_internal_solver_output()) {
    solver.EnableOutput();
  }
  std::string error_message;
  response->set_status(solver.LoadModelFromProto(model, &error_message));
  if (response->status() != MPSOLVER_MODEL_IS_VALID) {
    // LOG_EVERY_N_SEC(WARNING, 1.0)
    //     << "Loading model from protocol buffer failed, load status = "
    //     << MPSolverResponseStatus_Name(response->status()) << " ("
    //     << response->status() << "); Error: " << error_message;
    return;
  }
  if (model_request.has_solver_time_limit_seconds()) {
    // static_cast<int64> avoids a warning with -Wreal-conversion. This
    // helps catching bugs with unwanted conversions from double to ints.
    solver.set_time_limit(
        static_cast<int64>(model_request.solver_time_limit_seconds() * 1000));
  }
  solver.SetSolverSpecificParametersAsString(
      model_request.solver_specific_parameters());
  solver.Solve();
  solver.FillSolutionResponseProto(response);
}

void MPSolver::ExportModelToProto(MPModelProto* output_model) const {
  DCHECK(output_model != nullptr);
  output_model->Clear();
  // Name
  output_model->set_name(Name());
  // Variables
  for (int j = 0; j < variables_.size(); ++j) {
    const MPVariable* const var = variables_[j];
    MPVariableProto* const variable_proto = output_model->add_variable();
    // TODO(user): Add option to avoid filling the var name to avoid overly
    // large protocol buffers.
    variable_proto->set_name(var->name());
    variable_proto->set_lower_bound(var->lb());
    variable_proto->set_upper_bound(var->ub());
    variable_proto->set_is_integer(var->integer());
    if (objective_->GetCoefficient(var) != 0.0) {
      variable_proto->set_objective_coefficient(
          objective_->GetCoefficient(var));
    }
  }

  // Map the variables to their indices. This is needed to output the
  // variables in the order they were created, which in turn is needed to have
  // repeatable results with ExportModelAsLpString and ExportModelAsMpsString.
  // This step is needed as long as the variable indices are given by the
  // underlying solver at the time of model extraction.
  // TODO(user): remove this step.
  std::unordered_map<const MPVariable*, int> var_to_index;
  for (int j = 0; j < variables_.size(); ++j) {
    var_to_index[variables_[j]] = j;
  }

  // Constraints
  for (int i = 0; i < constraints_.size(); ++i) {
    MPConstraint* const constraint = constraints_[i];
    MPConstraintProto* const constraint_proto = output_model->add_constraint();
    constraint_proto->set_name(constraint->name());
    constraint_proto->set_lower_bound(constraint->lb());
    constraint_proto->set_upper_bound(constraint->ub());
    constraint_proto->set_is_lazy(constraint->is_lazy());
    // Vector linear_term will contain pairs (variable index, coeff), that will
    // be sorted by variable index.
    std::vector<std::pair<int, double> > linear_term;
    for (CoeffEntry entry : constraint->coefficients_) {
      const MPVariable* const var = entry.first;
      const int var_index = FindWithDefault(var_to_index, var, -1);
      DCHECK_NE(-1, var_index);
      const double coeff = entry.second;
      linear_term.push_back(std::pair<int, double>(var_index, coeff));
    }
    // The cost of sort is expected to be low as constraints usually have very
    // few terms.
    std::sort(linear_term.begin(), linear_term.end());
    // Now use linear term.
    for (const std::pair<int, double> var_and_coeff : linear_term) {
      constraint_proto->add_var_index(var_and_coeff.first);
      constraint_proto->add_coefficient(var_and_coeff.second);
    }
  }
  output_model->set_maximize(Objective().maximization());
  output_model->set_objective_offset(Objective().offset());
}

bool MPSolver::LoadSolutionFromProto(const MPSolutionResponse& response) {
  interface_->result_status_ = static_cast<ResultStatus>(response.status());
  if (response.status() != MPSOLVER_OPTIMAL &&
      response.status() != MPSOLVER_FEASIBLE) {
    LOG(ERROR)
        << "Cannot load a solution unless its status is OPTIMAL or FEASIBLE.";
    return false;
  }
  // Before touching the variables, verify that the solution looks legit:
  // each variable of the MPSolver must have its value listed exactly once, and
  // each listed solution should correspond to a known variable.
  if (response.variable_value_size() != variables_.size()) {
    LOG(ERROR) << "Trying to load a solution whose number of variables does not"
               << " correspond to the Solver.";
    return false;
  }
  double largest_error = 0;
  interface_->ExtractModel();
  int num_vars_out_of_bounds = 0;
  const double tolerance = MPSolverParameters::kDefaultPrimalTolerance;
  for (int i = 0; i < response.variable_value_size(); ++i) {
    // Look further: verify the bounds. Since linear solvers yield (small)
    // numerical errors, though, we just log a warning if the variables look
    // like they are out of their bounds. The user should inspect the values.
    const double var_value = response.variable_value(i);
    MPVariable* var = variables_[i];
    // TODO(user): Use parameter when they become available in this class.
    const double lb_error = var->lb() - var_value;
    const double ub_error = var_value - var->ub();
    if (lb_error > tolerance || ub_error > tolerance) {
      ++num_vars_out_of_bounds;
      largest_error = std::max(largest_error, std::max(lb_error, ub_error));
    }
    var->set_solution_value(var_value);
  }
  if (num_vars_out_of_bounds > 0) {
    LOG(WARNING)
        << "Loaded a solution whose variables matched the solver's, but "
        << num_vars_out_of_bounds << " out of " << variables_.size()
        << " exceed one of their bounds by more than the primal tolerance: "
        << tolerance;
  }
  // Set the objective value, if is known.
  if (response.has_objective_value()) {
    interface_->objective_value_ = response.objective_value();
  }
  // Mark the status as SOLUTION_SYNCHRONIZED, so that users may inspect the
  // solution normally.
  interface_->sync_status_ = MPSolverInterface::SOLUTION_SYNCHRONIZED;
  return true;
}

void MPSolver::Clear() {
  MutableObjective()->Clear();
  STLDeleteElements(&variables_);
  STLDeleteElements(&constraints_);
  variables_.clear();
  variable_name_to_index_.clear();
  variable_is_extracted_.clear();
  constraints_.clear();
  constraint_name_to_index_.clear();
  constraint_is_extracted_.clear();
  interface_->Reset();
  solution_hint_.clear();
}

void MPSolver::Reset() { interface_->Reset(); }

bool MPSolver::InterruptSolve() { return interface_->InterruptSolve(); }

void MPSolver::SetStartingLpBasis(
    const std::vector<BasisStatus>& variable_statuses,
    const std::vector<BasisStatus>& constraint_statuses) {
  interface_->SetStartingLpBasis(variable_statuses, constraint_statuses);
}

MPVariable* MPSolver::MakeVar(double lb, double ub, bool integer,
                              const std::string& name) {
  const int var_index = NumVariables();
  const std::string fixed_name =
      name.empty() ? StringPrintf("auto_v_%09d", var_index) : name;
  InsertOrDie(&variable_name_to_index_, fixed_name, var_index);
  MPVariable* v =
      new MPVariable(var_index, lb, ub, integer, fixed_name, interface_.get());
  variables_.push_back(v);
  variable_is_extracted_.push_back(false);
  interface_->AddVariable(v);
  return v;
}

MPVariable* MPSolver::MakeNumVar(double lb, double ub, const std::string& name) {
  return MakeVar(lb, ub, false, name);
}

MPVariable* MPSolver::MakeIntVar(double lb, double ub, const std::string& name) {
  return MakeVar(lb, ub, true, name);
}

MPVariable* MPSolver::MakeBoolVar(const std::string& name) {
  return MakeVar(0.0, 1.0, true, name);
}

void MPSolver::MakeVarArray(int nb, double lb, double ub, bool integer,
                            const std::string& name,
                            std::vector<MPVariable*>* vars) {
  DCHECK_GE(nb, 0);
  if (nb <= 0) return;
  const int num_digits = NumDigits(nb);
  for (int i = 0; i < nb; ++i) {
    if (name.empty()) {
      vars->push_back(MakeVar(lb, ub, integer, name));
    } else {
      std::string vname = StringPrintf("%s%0*d", name.c_str(), num_digits, i);
      vars->push_back(MakeVar(lb, ub, integer, vname));
    }
  }
}

void MPSolver::MakeNumVarArray(int nb, double lb, double ub, const std::string& name,
                               std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, lb, ub, false, name, vars);
}

void MPSolver::MakeIntVarArray(int nb, double lb, double ub, const std::string& name,
                               std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, lb, ub, true, name, vars);
}

void MPSolver::MakeBoolVarArray(int nb, const std::string& name,
                                std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, 0.0, 1.0, true, name, vars);
}

MPConstraint* MPSolver::MakeRowConstraint(double lb, double ub) {
  return MakeRowConstraint(lb, ub, "");
}

MPConstraint* MPSolver::MakeRowConstraint() {
  return MakeRowConstraint(-infinity(), infinity(), "");
}

MPConstraint* MPSolver::MakeRowConstraint(double lb, double ub,
                                          const std::string& name) {
  const int constraint_index = NumConstraints();
  const std::string fixed_name =
      name.empty() ? StringPrintf("auto_c_%09d", constraint_index) : name;
  InsertOrDie(&constraint_name_to_index_, fixed_name, constraint_index);
  MPConstraint* const constraint =
      new MPConstraint(constraint_index, lb, ub, fixed_name, interface_.get());
  constraints_.push_back(constraint);
  constraint_is_extracted_.push_back(false);
  interface_->AddRowConstraint(constraint);
  return constraint;
}

MPConstraint* MPSolver::MakeRowConstraint(const std::string& name) {
  return MakeRowConstraint(-infinity(), infinity(), name);
}

MPConstraint* MPSolver::MakeRowConstraint(const LinearRange& range) {
  return MakeRowConstraint(range, "");
}

MPConstraint* MPSolver::MakeRowConstraint(const LinearRange& range,
                                          const std::string& name) {
  CheckLinearExpr(*this, range.linear_expr());
  MPConstraint* constraint =
      MakeRowConstraint(range.lower_bound(), range.upper_bound(), name);
  for (const auto& kv : range.linear_expr().terms()) {
    constraint->SetCoefficient(kv.first, kv.second);
  }
  return constraint;
}

int MPSolver::ComputeMaxConstraintSize(int min_constraint_index,
                                       int max_constraint_index) const {
  int max_constraint_size = 0;
  DCHECK_GE(min_constraint_index, 0);
  DCHECK_LE(max_constraint_index, constraints_.size());
  for (int i = min_constraint_index; i < max_constraint_index; ++i) {
    MPConstraint* const ct = constraints_[i];
    if (ct->coefficients_.size() > max_constraint_size) {
      max_constraint_size = ct->coefficients_.size();
    }
  }
  return max_constraint_size;
}

bool MPSolver::HasInfeasibleConstraints() const {
  bool hasInfeasibleConstraints = false;
  for (int i = 0; i < constraints_.size(); ++i) {
    if (constraints_[i]->lb() > constraints_[i]->ub()) {
      LOG(WARNING) << "Constraint " << constraints_[i]->name() << " (" << i
                   << ") has contradictory bounds:"
                   << " lower bound = " << constraints_[i]->lb()
                   << " upper bound = " << constraints_[i]->ub();
      hasInfeasibleConstraints = true;
    }
  }
  return hasInfeasibleConstraints;
}

MPSolver::ResultStatus MPSolver::Solve() {
  MPSolverParameters default_param;
  return Solve(default_param);
}

MPSolver::ResultStatus MPSolver::Solve(const MPSolverParameters& param) {
  // Special case for infeasible constraints so that all solvers have
  // the same behavior.
  // TODO(user): replace this by model extraction to proto + proto validation
  // (the proto has very low overhead compared to the wrapper, both in
  // performance and memory, so it's ok).
  if (HasInfeasibleConstraints()) {
    interface_->result_status_ = MPSolver::INFEASIBLE;
    return interface_->result_status_;
  }


  MPSolver::ResultStatus status = interface_->Solve(param);
  if (FLAGS_verify_solution) {
    if (status != MPSolver::OPTIMAL && status != MPSolver::FEASIBLE) {
      VLOG(1) << "--verify_solution enabled, but the solver did not find a"
              << " solution: skipping the verification.";
    } else if (!VerifySolution(
                    param.GetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE),
                    FLAGS_log_verification_errors)) {
      status = MPSolver::ABNORMAL;
      interface_->result_status_ = status;
    }
  }
  DCHECK_EQ(interface_->result_status_, status);
  return status;
}

double MPSolver::SolutionValue(const LinearExpr& linear_expr) const {
  double ans = linear_expr.offset();
  for (const auto& kv : linear_expr.terms()) {
    ans += (kv.second * kv.first->solution_value());
  }
  return ans;
}

void MPSolver::Write(const std::string& file_name) { interface_->Write(file_name); }

namespace {
std::string PrettyPrintVar(const MPVariable& var) {
  const std::string prefix = "Variable '" + var.name() + "': domain = ";
  if (var.lb() >= MPSolver::infinity() || var.ub() <= -MPSolver::infinity() ||
      var.lb() > var.ub()) {
    return prefix + "∅";  // Empty set.
  }
  // Special case: integer variable with at most two possible values
  // (and potentially none).
  if (var.integer() && var.ub() - var.lb() <= 1) {
    const int64 lb = static_cast<int64>(ceil(var.lb()));
    const int64 ub = static_cast<int64>(floor(var.ub()));
    if (lb > ub) {
      return prefix + "∅";
    } else if (lb == ub) {
      return StringPrintf("%s{ %lld }", prefix.c_str(), lb);
    } else {
      return StringPrintf("%s{ %lld, %lld }", prefix.c_str(), lb, ub);
    }
  }
  // Special case: single (non-infinite) real value.
  if (var.lb() == var.ub()) {
    return StringPrintf("%s{ %f }", prefix.c_str(), var.lb());
  }
  return prefix + (var.integer() ? "Integer" : "Real") + " in " +
         (var.lb() <= -MPSolver::infinity() ? std::string("]-∞")
                                            : StringPrintf("[%f", var.lb())) +
         ", " +
         (var.ub() >= MPSolver::infinity() ? std::string("+∞[")
                                           : StringPrintf("%f]", var.ub()));
}

std::string PrettyPrintConstraint(const MPConstraint& constraint) {
  std::string prefix = "Constraint '" + constraint.name() + "': ";
  if (constraint.lb() >= MPSolver::infinity() ||
      constraint.ub() <= -MPSolver::infinity() ||
      constraint.lb() > constraint.ub()) {
    return prefix + "ALWAYS FALSE";
  }
  if (constraint.lb() <= -MPSolver::infinity() &&
      constraint.ub() >= MPSolver::infinity()) {
    return prefix + "ALWAYS TRUE";
  }
  prefix += "<linear expr>";
  // Equality.
  if (constraint.lb() == constraint.ub()) {
    return StringPrintf("%s = %f", prefix.c_str(), constraint.lb());
  }
  // Inequalities.
  if (constraint.lb() <= -MPSolver::infinity()) {
    return StringPrintf("%s ≤ %f", prefix.c_str(), constraint.ub());
  }
  if (constraint.ub() >= MPSolver::infinity()) {
    return StringPrintf("%s ≥ %f", prefix.c_str(), constraint.lb());
  }
  return StringPrintf("%s ∈ [%f, %f]", prefix.c_str(), constraint.lb(),
                      constraint.ub());
}
}  // namespace

std::vector<double> MPSolver::ComputeConstraintActivities() const {
  // TODO(user): test this failure case.
  if (!interface_->CheckSolutionIsSynchronizedAndExists()) return {};
  std::vector<double> activities(constraints_.size(), 0.0);
  for (int i = 0; i < constraints_.size(); ++i) {
    const MPConstraint& constraint = *constraints_[i];
    AccurateSum<double> sum;
    for (CoeffEntry entry : constraint.coefficients_) {
      sum.Add(entry.first->solution_value() * entry.second);
    }
    activities[i] = sum.Value();
  }
  return activities;
}

// TODO(user): split.
bool MPSolver::VerifySolution(double tolerance, bool log_errors) const {
  double max_observed_error = 0;
  if (tolerance < 0) tolerance = infinity();
  int num_errors = 0;

  // Verify variables.
  for (int i = 0; i < variables_.size(); ++i) {
    const MPVariable& var = *variables_[i];
    const double value = var.solution_value();
    // Check for NaN.
    if (std::isnan(value)) {
      ++num_errors;
      max_observed_error = infinity();
      LOG_IF(ERROR, log_errors) << "NaN value for " << PrettyPrintVar(var);
      continue;
    }
    // Check lower bound.
    if (var.lb() != -infinity()) {
      if (value < var.lb() - tolerance) {
        ++num_errors;
        max_observed_error = std::max(max_observed_error, var.lb() - value);
        LOG_IF(ERROR, log_errors) << "Value " << value << " too low for "
                                  << PrettyPrintVar(var);
      }
    }
    // Check upper bound.
    if (var.ub() != infinity()) {
      if (value > var.ub() + tolerance) {
        ++num_errors;
        max_observed_error = std::max(max_observed_error, value - var.ub());
        LOG_IF(ERROR, log_errors) << "Value " << value << " too high for "
                                  << PrettyPrintVar(var);
      }
    }
    // Check integrality.
    if (var.integer()) {
      if (fabs(value - round(value)) > tolerance) {
        ++num_errors;
        max_observed_error =
            std::max(max_observed_error, fabs(value - round(value)));
        LOG_IF(ERROR, log_errors) << "Non-integer value " << value << " for "
                                  << PrettyPrintVar(var);
      }
    }
  }

  // Verify constraints.
  const std::vector<double> activities = ComputeConstraintActivities();
  for (int i = 0; i < constraints_.size(); ++i) {
    const MPConstraint& constraint = *constraints_[i];
    const double activity = activities[i];
    // Re-compute the activity with a inaccurate summing algorithm.
    double inaccurate_activity = 0.0;
    for (CoeffEntry entry : constraint.coefficients_) {
      inaccurate_activity += entry.first->solution_value() * entry.second;
    }
    // Catch NaNs.
    if (std::isnan(activity) || std::isnan(inaccurate_activity)) {
      ++num_errors;
      max_observed_error = infinity();
      LOG_IF(ERROR, log_errors) << "NaN value for "
                                << PrettyPrintConstraint(constraint);
      continue;
    }
    // Check bounds.
    if (constraint.lb() != -infinity()) {
      if (activity < constraint.lb() - tolerance) {
        ++num_errors;
        max_observed_error =
            std::max(max_observed_error, constraint.lb() - activity);
        LOG_IF(ERROR, log_errors) << "Activity " << activity << " too low for "
                                  << PrettyPrintConstraint(constraint);
      } else if (inaccurate_activity < constraint.lb() - tolerance) {
        LOG_IF(WARNING, log_errors)
            << "Activity " << activity << ", computed with the (inaccurate)"
            << " standard sum of its terms, is too low for "
            << PrettyPrintConstraint(constraint);
      }
    }
    if (constraint.ub() != infinity()) {
      if (activity > constraint.ub() + tolerance) {
        ++num_errors;
        max_observed_error =
            std::max(max_observed_error, activity - constraint.ub());
        LOG_IF(ERROR, log_errors) << "Activity " << activity << " too high for "
                                  << PrettyPrintConstraint(constraint);
      } else if (inaccurate_activity > constraint.ub() + tolerance) {
        LOG_IF(WARNING, log_errors)
            << "Activity " << activity << ", computed with the (inaccurate)"
            << " standard sum of its terms, is too high for "
            << PrettyPrintConstraint(constraint);
      }
    }
  }

  // Verify that the objective value wasn't reported incorrectly.
  const MPObjective& objective = Objective();
  AccurateSum<double> objective_sum;
  objective_sum.Add(objective.offset());
  double inaccurate_objective_value = objective.offset();
  for (CoeffEntry entry : objective.coefficients_) {
    const double term = entry.first->solution_value() * entry.second;
    objective_sum.Add(term);
    inaccurate_objective_value += term;
  }
  const double actual_objective_value = objective_sum.Value();
  if (!AreWithinAbsoluteOrRelativeTolerances(
           objective.Value(), actual_objective_value, tolerance, tolerance)) {
    ++num_errors;
    max_observed_error = std::max(
        max_observed_error, fabs(actual_objective_value - objective.Value()));
    LOG_IF(ERROR, log_errors) << "Objective value " << objective.Value()
                              << " isn't accurate"
                              << ", it should be " << actual_objective_value
                              << " (delta=" << actual_objective_value -
                                                   objective.Value() << ").";
  } else if (!AreWithinAbsoluteOrRelativeTolerances(objective.Value(),
                                                    inaccurate_objective_value,
                                                    tolerance, tolerance)) {
    LOG_IF(WARNING, log_errors)
        << "Objective value " << objective.Value() << " doesn't correspond"
        << " to the value computed with the standard (and therefore inaccurate)"
        << " sum of its terms.";
  }
  if (num_errors > 0) {
    LOG_IF(ERROR, log_errors) << "There were " << num_errors
                              << " errors above the tolerance (" << tolerance
                              << "), the largest was " << max_observed_error;
    return false;
  }
  return true;
}

bool MPSolver::OutputIsEnabled() const { return !interface_->quiet(); }

void MPSolver::EnableOutput() { interface_->set_quiet(false); }

void MPSolver::SuppressOutput() { interface_->set_quiet(true); }

int64 MPSolver::iterations() const { return interface_->iterations(); }

int64 MPSolver::nodes() const { return interface_->nodes(); }

double MPSolver::ComputeExactConditionNumber() const {
  return interface_->ComputeExactConditionNumber();
}

bool MPSolver::OwnsVariable(const MPVariable* var) const {
  if (var == nullptr) return false;
  // First, verify that a variable with the same name exists, and look up
  // its index (names are unique, so there can be only one).
  const int var_index =
      FindWithDefault(variable_name_to_index_, var->name(), -1);
  if (var_index == -1) return false;
  // Then, verify that the variable with this index has the same address.
  return variables_[var_index] == var;
}

#ifndef ANDROID_JNI
bool MPSolver::ExportModelAsLpFormat(bool obfuscate, std::string* output) {
  MPModelProto proto;
  ExportModelToProto(&proto);
  MPModelProtoExporter exporter(proto);
  return exporter.ExportModelAsLpFormat(obfuscate, output);
}

bool MPSolver::ExportModelAsMpsFormat(bool fixed_format, bool obfuscate,
                                      std::string* output) {
  MPModelProto proto;
  ExportModelToProto(&proto);
  MPModelProtoExporter exporter(proto);
  return exporter.ExportModelAsMpsFormat(fixed_format, obfuscate, output);
}
#endif

// ---------- MPSolverInterface ----------

const int MPSolverInterface::kDummyVariableIndex = 0;

MPSolverInterface::MPSolverInterface(MPSolver* const solver)
    : solver_(solver),
      sync_status_(MODEL_SYNCHRONIZED),
      result_status_(MPSolver::NOT_SOLVED),
      maximize_(false),
      last_constraint_index_(0),
      last_variable_index_(0),
      objective_value_(0.0),
      quiet_(true) {}

MPSolverInterface::~MPSolverInterface() {}

void MPSolverInterface::Write(const std::string& filename) {
  LOG(WARNING) << "Writing model not implemented in this solver interface.";
}

void MPSolverInterface::ExtractModel() {
  switch (sync_status_) {
    case MUST_RELOAD: {
      ExtractNewVariables();
      ExtractNewConstraints();
      ExtractObjective();

      last_constraint_index_ = solver_->constraints_.size();
      last_variable_index_ = solver_->variables_.size();
      sync_status_ = MODEL_SYNCHRONIZED;
      break;
    }
    case MODEL_SYNCHRONIZED: {
      // Everything has already been extracted.
      DCHECK_EQ(last_constraint_index_, solver_->constraints_.size());
      DCHECK_EQ(last_variable_index_, solver_->variables_.size());
      break;
    }
    case SOLUTION_SYNCHRONIZED: {
      // Nothing has changed since last solve.
      DCHECK_EQ(last_constraint_index_, solver_->constraints_.size());
      DCHECK_EQ(last_variable_index_, solver_->variables_.size());
      break;
    }
  }
}

// TODO(user): remove this method.
void MPSolverInterface::ResetExtractionInformation() {
  sync_status_ = MUST_RELOAD;
  last_constraint_index_ = 0;
  last_variable_index_ = 0;
  solver_->variable_is_extracted_.assign(solver_->variables_.size(), false);
  solver_->constraint_is_extracted_.assign(solver_->constraints_.size(), false);
}

bool MPSolverInterface::CheckSolutionIsSynchronized() const {
  if (sync_status_ != SOLUTION_SYNCHRONIZED) {
    LOG(DFATAL)
        << "The model has been changed since the solution was last computed."
        << " MPSolverInterface::status_ = " << sync_status_;
    return false;
  }
  return true;
}

// Default version that can be overwritten by a solver-specific
// version to accomodate for the quirks of each solver.
bool MPSolverInterface::CheckSolutionExists() const {
  if (result_status_ != MPSolver::OPTIMAL &&
      result_status_ != MPSolver::FEASIBLE) {
    LOG(DFATAL) << "No solution exists. MPSolverInterface::result_status_ = "
                << result_status_;
    return false;
  }
  return true;
}

// Default version that can be overwritten by a solver-specific
// version to accomodate for the quirks of each solver.
bool MPSolverInterface::CheckBestObjectiveBoundExists() const {
  if (result_status_ != MPSolver::OPTIMAL &&
      result_status_ != MPSolver::FEASIBLE) {
    LOG(DFATAL) << "No information is available for the best objective bound."
                << " MPSolverInterface::result_status_ = " << result_status_;
    return false;
  }
  return true;
}

double MPSolverInterface::trivial_worst_objective_bound() const {
  return maximize_ ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
}

double MPSolverInterface::objective_value() const {
  if (!CheckSolutionIsSynchronizedAndExists()) return 0;
  return objective_value_;
}

void MPSolverInterface::InvalidateSolutionSynchronization() {
  if (sync_status_ == SOLUTION_SYNCHRONIZED) {
    sync_status_ = MODEL_SYNCHRONIZED;
  }
}

double MPSolverInterface::ComputeExactConditionNumber() const {
  // Override this method in interfaces that actually support it.
  LOG(DFATAL) << "ComputeExactConditionNumber not implemented for "
              << MPModelRequest_SolverType_Name(
                     static_cast<MPModelRequest::SolverType>(
                         solver_->ProblemType()));
  return 0.0;
}

void MPSolverInterface::SetCommonParameters(const MPSolverParameters& param) {
// TODO(user): Overhaul the code that sets parameters to enable changing
// GLOP parameters without issuing warnings.
// By default, we let GLOP keep its own default tolerance, much more accurate
// than for the rest of the solvers.
//
#if defined(USE_GLOP)
  if (solver_->ProblemType() != MPSolver::GLOP_LINEAR_PROGRAMMING) {
#endif
    SetPrimalTolerance(
        param.GetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE));
    SetDualTolerance(param.GetDoubleParam(MPSolverParameters::DUAL_TOLERANCE));
#if defined(USE_GLOP)
  }
#endif
  SetPresolveMode(param.GetIntegerParam(MPSolverParameters::PRESOLVE));
  // TODO(user): In the future, we could distinguish between the
  // algorithm to solve the root LP and the algorithm to solve node
  // LPs. Not sure if underlying solvers support it.
  int value = param.GetIntegerParam(MPSolverParameters::LP_ALGORITHM);
  if (value != MPSolverParameters::kDefaultIntegerParamValue) {
    SetLpAlgorithm(value);
  }
}

void MPSolverInterface::SetMIPParameters(const MPSolverParameters& param) {
#if defined(USE_GLOP)
  if (solver_->ProblemType() != MPSolver::GLOP_LINEAR_PROGRAMMING) {
#endif
    SetRelativeMipGap(param.GetDoubleParam(
        MPSolverParameters::RELATIVE_MIP_GAP));
#if defined(USE_GLOP)
  }
#endif
}

void MPSolverInterface::SetUnsupportedDoubleParam(
    MPSolverParameters::DoubleParam param) const {
  LOG(WARNING) << "Trying to set an unsupported parameter: " << param << ".";
}
void MPSolverInterface::SetUnsupportedIntegerParam(
    MPSolverParameters::IntegerParam param) const {
  LOG(WARNING) << "Trying to set an unsupported parameter: " << param << ".";
}
void MPSolverInterface::SetDoubleParamToUnsupportedValue(
    MPSolverParameters::DoubleParam param, double value) const {
  LOG(WARNING) << "Trying to set a supported parameter: " << param
               << " to an unsupported value: " << value;
}
void MPSolverInterface::SetIntegerParamToUnsupportedValue(
    MPSolverParameters::IntegerParam param, int value) const {
  LOG(WARNING) << "Trying to set a supported parameter: " << param
               << " to an unsupported value: " << value;
}

bool MPSolverInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
#ifdef ANDROID_JNI
  // This is not implemented on Android because there is no default /tmp and a
  // pointer to the Java environment is require to query for the application
  // folder or the location of external storage (if any).
  return false;
#else
  if (parameters.empty()) return true;

  // Note(user): this method needs to return a success/failure boolean
  // immediately, so we also perform the actual parameter parsing right away.
  // Some implementations will keep them forever and won't need to re-parse
  // them; some (eg. SCIP, Gurobi) need to re-parse the parameters every time
  // they do Solve(). We just store the parameters std::string anyway.
  std::string extension = ValidFileExtensionForParameterFile();
  #if defined(__linux)
    int32 tid = static_cast<int32>(pthread_self());
  #else  // defined(__linux__)
    int32 tid = 123;
  #endif  // defined(__linux__)
  #if !defined(_MSC_VER)
    int32 pid = static_cast<int32>(getpid());
  #else  // _MSC_VER
    int32 pid = 456;
  #endif  // _MSC_VER
    int64 now = base::GetCurrentTimeNanos();
    std::string filename = StringPrintf("/tmp/parameters-tempfile-%x-%d-%llx%s",
        tid, pid, now, extension.c_str());
    bool no_error_so_far = true;
  if (no_error_so_far) {
    no_error_so_far =
        file::SetContents(filename, parameters, file::Defaults()).ok();
  }
  if (no_error_so_far) {
    no_error_so_far = ReadParameterFile(filename);
    // We need to clean up the file even if ReadParameterFile() returned
    // false. In production we can continue even if the deletion failed.
    if (!file::Delete(filename, file::Defaults()).ok()) {
      LOG(DFATAL) << "Couldn't delete temporary parameters file: " << filename;
    }
  }
  if (!no_error_so_far) {
    LOG(WARNING) << "Error in SetSolverSpecificParametersAsString() "
                 << "for solver type: "
                 << MPModelRequest::SolverType_Name(
                        static_cast<MPModelRequest::SolverType>(
                            solver_->ProblemType()));
  }
  return no_error_so_far;
#endif
}

bool MPSolverInterface::ReadParameterFile(const std::string& filename) {
  LOG(WARNING) << "ReadParameterFile() not supported by this solver.";
  return false;
}

std::string MPSolverInterface::ValidFileExtensionForParameterFile() const {
  return ".tmp";
}

// ---------- MPSolverParameters ----------

const double MPSolverParameters::kDefaultRelativeMipGap = 1e-4;
// For the primal and dual tolerances, choose the same default as CLP and GLPK.
const double MPSolverParameters::kDefaultPrimalTolerance = 1e-7;
const double MPSolverParameters::kDefaultDualTolerance = 1e-7;
const MPSolverParameters::PresolveValues MPSolverParameters::kDefaultPresolve =
    MPSolverParameters::PRESOLVE_ON;
const MPSolverParameters::IncrementalityValues
    MPSolverParameters::kDefaultIncrementality =
        MPSolverParameters::INCREMENTALITY_ON;

const double MPSolverParameters::kDefaultDoubleParamValue = -1.0;
const int MPSolverParameters::kDefaultIntegerParamValue = -1;
const double MPSolverParameters::kUnknownDoubleParamValue = -2.0;
const int MPSolverParameters::kUnknownIntegerParamValue = -2;

// The constructor sets all parameters to their default value.
MPSolverParameters::MPSolverParameters()
    : relative_mip_gap_value_(kDefaultRelativeMipGap),
      primal_tolerance_value_(kDefaultPrimalTolerance),
      dual_tolerance_value_(kDefaultDualTolerance),
      presolve_value_(kDefaultPresolve),
      scaling_value_(kDefaultIntegerParamValue),
      lp_algorithm_value_(kDefaultIntegerParamValue),
      incrementality_value_(kDefaultIncrementality),
      lp_algorithm_is_default_(true) {}

void MPSolverParameters::SetDoubleParam(MPSolverParameters::DoubleParam param,
                                        double value) {
  switch (param) {
    case RELATIVE_MIP_GAP: {
      relative_mip_gap_value_ = value;
      break;
    }
    case PRIMAL_TOLERANCE: {
      primal_tolerance_value_ = value;
      break;
    }
    case DUAL_TOLERANCE: {
      dual_tolerance_value_ = value;
      break;
    }
    default: {
      LOG(ERROR) << "Trying to set an unknown parameter: " << param << ".";
    }
  }
}

void MPSolverParameters::SetIntegerParam(MPSolverParameters::IntegerParam param,
                                         int value) {
  switch (param) {
    case PRESOLVE: {
      if (value != PRESOLVE_OFF && value != PRESOLVE_ON) {
        LOG(ERROR) << "Trying to set a supported parameter: " << param
                   << " to an unknown value: " << value;
      }
      presolve_value_ = value;
      break;
    }
    case SCALING: {
      if (value != SCALING_OFF && value != SCALING_ON) {
        LOG(ERROR) << "Trying to set a supported parameter: " << param
                   << " to an unknown value: " << value;
      }
      scaling_value_ = value;
      break;
    }
    case LP_ALGORITHM: {
      if (value != DUAL && value != PRIMAL && value != BARRIER) {
        LOG(ERROR) << "Trying to set a supported parameter: " << param
                   << " to an unknown value: " << value;
      }
      lp_algorithm_value_ = value;
      lp_algorithm_is_default_ = false;
      break;
    }
    case INCREMENTALITY: {
      if (value != INCREMENTALITY_OFF && value != INCREMENTALITY_ON) {
        LOG(ERROR) << "Trying to set a supported parameter: " << param
                   << " to an unknown value: " << value;
      }
      incrementality_value_ = value;
      break;
    }
    default: {
      LOG(ERROR) << "Trying to set an unknown parameter: " << param << ".";
    }
  }
}

void MPSolverParameters::ResetDoubleParam(
    MPSolverParameters::DoubleParam param) {
  switch (param) {
    case RELATIVE_MIP_GAP: {
      relative_mip_gap_value_ = kDefaultRelativeMipGap;
      break;
    }
    case PRIMAL_TOLERANCE: {
      primal_tolerance_value_ = kDefaultPrimalTolerance;
      break;
    }
    case DUAL_TOLERANCE: {
      dual_tolerance_value_ = kDefaultDualTolerance;
      break;
    }
    default: {
      LOG(ERROR) << "Trying to reset an unknown parameter: " << param << ".";
    }
  }
}

void MPSolverParameters::ResetIntegerParam(
    MPSolverParameters::IntegerParam param) {
  switch (param) {
    case PRESOLVE: {
      presolve_value_ = kDefaultPresolve;
      break;
    }
    case SCALING: {
      scaling_value_ = kDefaultIntegerParamValue;
      break;
    }
    case LP_ALGORITHM: {
      lp_algorithm_is_default_ = true;
      break;
    }
    case INCREMENTALITY: {
      incrementality_value_ = kDefaultIncrementality;
      break;
    }
    default: {
      LOG(ERROR) << "Trying to reset an unknown parameter: " << param << ".";
    }
  }
}

void MPSolverParameters::Reset() {
  ResetDoubleParam(RELATIVE_MIP_GAP);
  ResetDoubleParam(PRIMAL_TOLERANCE);
  ResetDoubleParam(DUAL_TOLERANCE);
  ResetIntegerParam(PRESOLVE);
  ResetIntegerParam(SCALING);
  ResetIntegerParam(LP_ALGORITHM);
  ResetIntegerParam(INCREMENTALITY);
}

double MPSolverParameters::GetDoubleParam(MPSolverParameters::DoubleParam param)
    const {
  switch (param) {
    case RELATIVE_MIP_GAP: { return relative_mip_gap_value_; }
    case PRIMAL_TOLERANCE: { return primal_tolerance_value_; }
    case DUAL_TOLERANCE: { return dual_tolerance_value_; }
    default: {
      LOG(ERROR) << "Trying to get an unknown parameter: " << param << ".";
      return kUnknownDoubleParamValue;
    }
  }
}

int MPSolverParameters::GetIntegerParam(MPSolverParameters::IntegerParam param)
    const {
  switch (param) {
    case PRESOLVE: { return presolve_value_; }
    case LP_ALGORITHM: {
      if (lp_algorithm_is_default_) return kDefaultIntegerParamValue;
      return lp_algorithm_value_;
    }
    case INCREMENTALITY: { return incrementality_value_; }
    case SCALING: { return scaling_value_; }
    default: {
      LOG(ERROR) << "Trying to get an unknown parameter: " << param << ".";
      return kUnknownIntegerParamValue;
    }
  }
}


}  // namespace operations_research

