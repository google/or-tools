// Copyright 2010-2011 Google
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
//                                                   (Laurent Perron)

#include "linear_solver/linear_solver.h"

#include <stddef.h>
#include <utility>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/hash.h"
#include "linear_solver/linear_solver.pb.h"

DEFINE_string(solver_write_model, "", "path of the file to write the model to");

namespace operations_research {
namespace {

// Insert name in name_set and check for duplicates.
void CheckDuplicateName(const string& name,
                        hash_set<string>* name_set) {
  if (!name.empty()) {
    std::pair<hash_set<string>::iterator, bool> result = name_set->insert(name);
    if (!result.second) {
      LOG(FATAL) << "Duplicate name: " << name;
    }
  }
}

// Create a valid name (unique) for a variable i in case it does
// not already have one.
string CreateValidVariableName(
    const operations_research::MPVariable& variable, int i) {
  if (variable.name().empty()) {
    // The index cannot be used because the model may not have been
    // extracted yet, so use i instead.
    return StringPrintf("auto_variable_%d", i);
  } else {
    return variable.name();
  }
}

// Create a valid name (unique) for constraint i in case it does
// not already have one.
string CreateValidConstraintName(
    const operations_research::MPConstraint& constraint, int i) {
  if (constraint.name().empty()) {
    // The index cannot be used because the model may not have been
    // extracted yet, so use i instead.
    return StringPrintf("auto_constraint_%d", i);
  } else {
    return constraint.name();
  }
}

}  // namespace

// ----- MPConstraint -----

void MPConstraint::AddTerm(MPVariable* const var, double coeff) {
  CHECK_NOTNULL(var);
  if (coeff != 0.0) {
    double* coefficient = FindOrNull(coefficients_, var);
    if (coefficient != NULL) {
      const double old_value = (*coefficient);
      const double new_value = (*coefficient) + coeff;
      (*coefficient) = new_value;
      interface_->SetCoefficient(this, var, new_value, old_value);
    } else {
      coefficients_[var] = coeff;
      interface_->SetCoefficient(this, var, coeff, 0.0);
    }
  }
}

void MPConstraint::AddTerm(MPVariable* const var) {
  AddTerm(var, 1.0);
}

void MPConstraint::SetCoefficient(MPVariable* const var, double coeff) {
  CHECK_NOTNULL(var);
  double* coefficient = FindOrNull(coefficients_, var);
  if (coefficient != NULL) {
    const double old_value = (*coefficient);
    const double new_value = coeff;
    (*coefficient) = new_value;
    interface_->SetCoefficient(this, var, new_value, old_value);
  } else {
    coefficients_[var] = coeff;
    interface_->SetCoefficient(this, var, coeff, 0.0);
  }
}

void MPConstraint::Clear() {
  interface_->ClearConstraint(this);
  coefficients_.clear();
}

void MPConstraint::SetBounds(double lb, double ub) {
  const bool change = lb != lb_ || ub != ub_;
  lb_ = lb;
  ub_ = ub;
  if (index_ != MPSolverInterface::kNoIndex && change) {
    interface_->SetConstraintBounds(index_, lb_, ub_);
  }
}

double MPConstraint::dual_value() const {
  CHECK(interface_->IsContinuous()) <<
        "Dual value only available for continuous problems";
  interface_->CheckSolutionIsSynchronized();
  interface_->CheckSolutionExists();
  return dual_value_;
}

bool MPConstraint::ContainsNewVariables() {
  const int last_variable_index = interface_->last_variable_index();
  for (ConstIter<hash_map<MPVariable*, double> > it(coefficients_);
       !it.at_end(); ++it) {
    const int variable_index = it->first->index();
    if (variable_index >= last_variable_index ||
        variable_index == MPSolverInterface::kNoIndex) {
      return true;
    }
  }
  return false;
}

// ----- MPObjective -----

void MPObjective::AddTerm(MPVariable* const var, double coeff) {
  CHECK_NOTNULL(var);
  if (coeff != 0.0) {
    double* coefficient = FindOrNull(coefficients_, var);
    if (coefficient != NULL) {
      (*coefficient) += coeff;
      interface_->SetObjectiveCoefficient(var, *coefficient);
    } else {
      coefficients_[var] = coeff;
      interface_->SetObjectiveCoefficient(var, coeff);
    }
  }
}

void MPObjective::AddTerm(MPVariable* const var) {
  AddTerm(var, 1.0);
}

void MPObjective::SetCoefficient(MPVariable* const var, double coeff) {
  CHECK_NOTNULL(var);
  coefficients_[var] = coeff;
  interface_->SetObjectiveCoefficient(var, coeff);
}

void MPObjective::AddOffset(double value) {
  offset_ += value;
  interface_->SetObjectiveOffset(offset_);
}

void MPObjective::SetOffset(double value) {
  offset_ = value;
  interface_->SetObjectiveOffset(offset_);
}

void MPObjective::Clear() {
  interface_->ClearObjective();
  coefficients_.clear();
  offset_ = 0.0;
}

// ----- MPVariable -----

double MPVariable::solution_value() const {
  interface_->CheckSolutionIsSynchronized();
  interface_->CheckSolutionExists();
  return solution_value_;
}

double MPVariable::reduced_cost() const {
  CHECK(interface_->IsContinuous()) <<
        "Reduced cost only available for continuous problems";
  interface_->CheckSolutionIsSynchronized();
  interface_->CheckSolutionExists();
  return reduced_cost_;
}

void MPVariable::SetBounds(double lb, double ub) {
  const bool change = lb != lb_ || ub != ub_;
  lb_ = lb;
  ub_ = ub;
  if (index_ != MPSolverInterface::kNoIndex && change) {
    interface_->SetVariableBounds(index_, lb_, ub_);
  }
}

void MPVariable::SetInteger(bool integer) {
  if (integer_ != integer) {
    integer_ = integer;
    if (index_ != MPSolverInterface::kNoIndex) {
      interface_->SetVariableInteger(index_, integer);
    }
  }
}

// ----- Objective -----

double MPSolver::objective_value() const {
  return interface_->objective_value();
}

double MPSolver::best_objective_bound() const {
  return interface_->best_objective_bound();
}

void MPSolver::ClearObjective() {
  linear_objective_.Clear();
}

void MPSolver::AddObjectiveTerm(MPVariable* const var, double coeff) {
  linear_objective_.AddTerm(var, coeff);
}

void MPSolver::AddObjectiveTerm(MPVariable* const var) {
  linear_objective_.AddTerm(var);
}

void MPSolver::SetObjectiveCoefficient(MPVariable* const var, double coeff) {
  linear_objective_.SetCoefficient(var, coeff);
}

void MPSolver::AddObjectiveOffset(double value) {
  linear_objective_.AddOffset(value);
}

void MPSolver::SetObjectiveOffset(double value) {
  linear_objective_.SetOffset(value);
}

void MPSolver::SetOptimizationDirection(bool maximize) {
  interface_->maximize_ = maximize;
  interface_->SetOptimizationDirection(maximize);
}
  // Minimizing or maximizing?
bool MPSolver::Maximization() const {
  return interface_->maximize_;
}

bool MPSolver::Minimization() const {
  return !interface_->maximize_;
}

// ----- Version -----

string MPSolver::SolverVersion() const {
  return interface_->SolverVersion();
}

// ----- Solver -----

#if defined(USE_CLP) || defined(USE_CBC)
extern MPSolverInterface* BuildCLPInterface(MPSolver* const solver);
#endif
#if defined(USE_CBC)
extern MPSolverInterface* BuildCBCInterface(MPSolver* const solver);
#endif
#if defined(USE_GLPK)
extern MPSolverInterface* BuildGLPKInterface(MPSolver* const solver, bool mip);
#endif
#if defined(USE_SCIP)
extern MPSolverInterface* BuildSCIPInterface(MPSolver* const solver);
#endif

const int64 MPSolverInterface::kUnknownNumberOfIterations = -1;
const int64 MPSolverInterface::kUnknownNumberOfNodes = -1;
const int MPSolverInterface::kNoIndex = -1;

namespace {
MPSolverInterface* BuildSolverInterface(
    MPSolver* const solver, MPSolver::OptimizationProblemType problem_type) {
  switch (problem_type) {
#if defined(USE_GLPK)
    case MPSolver::GLPK_LINEAR_PROGRAMMING:
      return BuildGLPKInterface(solver, false);
    case MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING:
      return BuildGLPKInterface(solver, true);
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
    default:
      LOG(FATAL) << "Linear solver not recognized.";
  }
  return NULL;
}
}  // namespace

// Creates a LP/MIP instance with the specified name and minimization objective.
MPSolver::MPSolver(const string& name, OptimizationProblemType problem_type)
    : name_(name),
      interface_(BuildSolverInterface(this, problem_type)),
      linear_objective_(interface_.get()),
      time_limit_(0.0),
      write_model_filename_("") {
  timer_.Restart();
}

// Frees the LP memory allocations.
MPSolver::~MPSolver() {
  Clear();
}

// ----- Names management -----

bool MPSolver::CheckNameValidity(const string& name) {
  // Allow names that conform to the LP and MPS format.
  const int kMaxNameLength = 255;
  if (name.size() > kMaxNameLength) {
    LOG(WARNING) << "Invalid name " << name
                 << ": length > " << kMaxNameLength << "."
                 << " Will be unable to write model to file.";
    return false;
  }
  if (name.find_first_of(" +-*<>=:\\") != string::npos) {
    LOG(WARNING) << "Invalid name " << name
                 << ": contains forbidden character: +-*<>=:\\ space."
                 << " Will be unable to write model to file.";
    return false;
  }
  size_t first_occurrence = name.find_first_of(".0123456789");
  if (first_occurrence != string::npos && first_occurrence == 0) {
    LOG(WARNING) << "Invalid name " << name
                 << ": first character should not be . or a number."
                 << " Will be unable to write model to file.";
    return false;
  }
  return true;
}

bool MPSolver::CheckAllNamesValidity() {
  for (int i = 0; i < variables_.size(); ++i) {
    if (!CheckNameValidity(variables_[i]->name())) {
      return false;
    }
  }
  for (int i = 0; i < constraints_.size(); ++i) {
    if (!CheckNameValidity(constraints_[i]->name())) {
      return false;
    }
  }
  return true;
}

// ----- Methods using protocol buffers -----
// Loads model from protocol buffer.
MPSolver::LoadStatus MPSolver::LoadModel(const MPModelProto& model) {
  hash_map<string, MPVariable*> variables;
  for (int i = 0; i < model.variables_size(); ++i) {
    const MPVariableProto& var_proto = model.variables(i);
    const string& id = var_proto.id();
    if (!ContainsKey(variables, id)) {
      MPVariable* variable = MakeNumVar(var_proto.lb(), var_proto.ub(), id);
      variable->SetInteger(var_proto.integer());
      variables[id] = variable;
    } else {
      return MPSolver::DUPLICATE_VARIABLE_ID;
    }
  }
  for (int i = 0; i < model.constraints_size(); ++i) {
    const MPConstraintProto& ct_proto = model.constraints(i);
    const string& ct_id = ct_proto.has_id() ? ct_proto.id() : "";
    MPConstraint* const ct = MakeRowConstraint(ct_proto.lb(),
                                               ct_proto.ub(),
                                               ct_id);
    for (int j = 0; j < ct_proto.terms_size(); ++j) {
      const MPTermProto& term_proto = ct_proto.terms(j);
      const string& id = term_proto.variable_id();
      MPVariable* variable = FindPtrOrNull(variables, id);
      if (variable != NULL) {
        ct->AddTerm(variable, term_proto.coefficient());
      } else {
        return MPSolver::UNKNOWN_VARIABLE_ID;
      }
    }
  }
  for (int i = 0; i < model.objective_terms_size(); ++i) {
    const MPTermProto& term_proto = model.objective_terms(i);
    const string& id = term_proto.variable_id();
    MPVariable* variable = FindPtrOrNull(variables, id);
    if (variable != NULL) {
      AddObjectiveTerm(variable, term_proto.coefficient());
    } else {
      return MPSolver::UNKNOWN_VARIABLE_ID;
    }
  }
  SetOptimizationDirection(model.maximize());
  if (model.has_objective_offset()) {
    linear_objective_.SetOffset(model.objective_offset());
  }
  return MPSolver::NO_ERROR;
}

// Exports model to protocol buffer.
void MPSolver::ExportModel(MPModelProto* model) const {
  CHECK_NOTNULL(model);
  if (model->variables_size() > 0 ||
      model->has_maximize() ||
      model->objective_terms_size() > 0 ||
      model->constraints_size() > 0 ||
      model->has_name() ||
      model->has_objective_offset()) {
    LOG(WARNING) << "The model protocol buffer is not empty, "
                 << "it will be overwritten.";
    model->clear_variables();
    model->clear_maximize();
    model->clear_objective_terms();
    model->clear_constraints();
    model->clear_name();
  }

  // Variables need non-empty names for the model protocol buffer, so
  // we may need to create names.
  hash_map<MPVariable*, string> valid_variable_names;

  // Variables
  for (int j = 0; j < variables_.size(); ++j) {
    MPVariable* const var = variables_[j];
    MPVariableProto* const variable_proto = model->add_variables();
    valid_variable_names[var] = CreateValidVariableName(*var, j);
    variable_proto->set_id(valid_variable_names[var]);
    variable_proto->set_lb(var->lb());
    variable_proto->set_ub(var->ub());
    variable_proto->set_integer(var->integer());
  }

  // Constraints
  for (int i = 0; i < constraints_.size(); ++i) {
    MPConstraint* const constraint = constraints_[i];
    MPConstraintProto* const constraint_proto = model->add_constraints();
    // Constraint names need to be non-empty.
    string valid_constraint_name = CreateValidConstraintName(*constraint, i);
    constraint_proto->set_id(valid_constraint_name);
    constraint_proto->set_lb(constraint->lb());
    constraint_proto->set_ub(constraint->ub());
    for (ConstIter<hash_map<MPVariable*, double> >
             it(constraint->coefficients_);
         !it.at_end(); ++it) {
      MPVariable* const var = it->first;
      const double coef = it->second;
      MPTermProto* const term = constraint_proto->add_terms();
      term->set_variable_id(valid_variable_names[var]);
      term->set_coefficient(coef);
    }
  }

  // Objective
  for (hash_map<MPVariable*, double>::const_iterator it =
           linear_objective_.coefficients_.begin();
       it != linear_objective_.coefficients_.end();
       ++it) {
    MPVariable* const var = it->first;
    const double coef = it->second;
    MPTermProto* const term = model->add_objective_terms();
    term->set_variable_id(valid_variable_names[var]);
    term->set_coefficient(coef);
  }
  model->set_maximize(Maximization());
  model->set_objective_offset(linear_objective_.offset_);
}

// Encode current solution in a solution response protocol buffer.
void MPSolver::FillSolutionResponse(MPSolutionResponse* response) const {
  CHECK_NOTNULL(response);
  if (response->has_result_status() ||
      response->has_objective_value() ||
      response->solution_values_size() > 0) {
    LOG(WARNING) << "The solution response is not empty, "
                 << "it will be overwritten.";
    response->clear_result_status();
    response->clear_objective_value();
    response->clear_solution_values();
  }

  switch (interface_->result_status_) {
    case MPSolver::OPTIMAL : {
      response->set_result_status(MPSolutionResponse::OPTIMAL);
      break;
    }
    case MPSolver::FEASIBLE : {
      response->set_result_status(MPSolutionResponse::FEASIBLE);
      break;
    }
    case MPSolver::INFEASIBLE : {
      response->set_result_status(MPSolutionResponse::INFEASIBLE);
      break;
    }
    case MPSolver::UNBOUNDED : {
      response->set_result_status(MPSolutionResponse::UNBOUNDED);
      break;
    }
    case MPSolver::ABNORMAL : {
      response->set_result_status(MPSolutionResponse::ABNORMAL);
      break;
    }
    case MPSolver::NOT_SOLVED : {
      response->set_result_status(MPSolutionResponse::NOT_SOLVED);
      break;
    }
    default: {
      response->set_result_status(MPSolutionResponse::ABNORMAL);
    }
  }
  if (interface_->result_status_ == MPSolver::OPTIMAL ||
      interface_->result_status_ == MPSolver::FEASIBLE) {
    response->set_objective_value(objective_value());
    for (int i = 0; i < variables_.size(); ++i) {
      MPVariable* const var = variables_[i];
      MPSolutionValue* value = response->add_solution_values();
      value->set_variable_id(var->name());
      value->set_value(var->solution_value());
    }
  }
}

// Solves the model encoded by a MPModelRequest protocol buffer and
// fills the solution encoded as a MPSolutionResponse.
// The model is solved by the interface specified in the constructor
// of MPSolver, MPModelRequest.OptimizationProblemType is ignored.
void MPSolver::SolveWithProtocolBuffers(const MPModelRequest& model_request,
                                        MPSolutionResponse* response) {
  CHECK_NOTNULL(response);
  Clear();
  const MPSolver::LoadStatus loadStatus = LoadModel(model_request.model());
  if (loadStatus != MPSolver::NO_ERROR) {
    LOG(WARNING) << "Loading model from protocol buffer failed, "
                 << "load status = " << loadStatus;
    response->set_result_status(MPSolutionResponse::ABNORMAL);
  }

  if (model_request.has_time_limit_ms()) {
    set_time_limit(model_request.time_limit_ms());
  }
  Solve();
  FillSolutionResponse(response);
}

void MPSolver::Clear() {
  ClearObjective();
  STLDeleteElements(&variables_);
  STLDeleteElements(&constraints_);
  variables_.clear();
  constraints_.clear();
  interface_->Reset();
  SetMinimization();
}

void MPSolver::Reset() {
  interface_->Reset();
}

void MPSolver::EnableOutput() {
  interface_->set_quiet(false);
}

void MPSolver::SuppressOutput() {
  interface_->set_quiet(true);
}

MPVariable* MPSolver::MakeVar(
    double lb, double ub, bool integer, const string& name) {
  CheckNameValidity(name);
  CheckDuplicateName(name, &variables_names_);
  MPVariable* v = new MPVariable(lb, ub, integer, name, interface_.get());
  variables_.push_back(v);
  interface_->AddVariable(v);
  return v;
}

MPVariable* MPSolver::MakeNumVar(
    double lb, double ub, const string& name) {
  return MakeVar(lb, ub, false, name);
}

MPVariable* MPSolver::MakeIntVar(
    double lb, double ub, const string& name) {
  return MakeVar(lb, ub, true, name);
}

MPVariable* MPSolver::MakeBoolVar(const string& name) {
  return MakeVar(0.0, 1.0, true, name);
}

void MPSolver::MakeVarArray(int nb,
                            double lb,
                            double ub,
                            bool integer,
                            const string& name,
                            std::vector<MPVariable*>* vars) {
  CHECK_GE(nb, 0);
  for (int i = 0; i < nb; ++i) {
    if (name.empty()) {
      vars->push_back(MakeVar(lb, ub, integer, name));
    } else {
      string vname = StringPrintf("%s%d", name.c_str(), i);
      vars->push_back(MakeVar(lb, ub, integer, vname));
    }
  }
}

void MPSolver::MakeNumVarArray(int nb,
                               double lb,
                               double ub,
                               const string& name,
                               std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, lb, ub, false, name, vars);
}

void MPSolver::MakeIntVarArray(int nb,
                               double lb,
                               double ub,
                               const string& name,
                               std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, lb, ub, true, name, vars);
}

void MPSolver::MakeBoolVarArray(int nb,
                                const string& name,
                                std::vector<MPVariable*>* vars) {
  MakeVarArray(nb, 0.0, 1.0, true, name, vars);
}

// Creates a new row constraint, adds it to the LP/MIP and returns it.
// MPSolver owns the Constraint. Do not free the memory yourself.
MPConstraint* MPSolver::MakeRowConstraint(double lb, double ub) {
  return MakeRowConstraint(lb, ub, "");
}

MPConstraint* MPSolver::MakeRowConstraint() {
  return MakeRowConstraint(-infinity(), infinity(), "");
}

// Creates a new row constraint, adds it to the LP/MIP and returns it.
// MPSolver owns the Constraint. Do not free the memory yourself.
MPConstraint* MPSolver::MakeRowConstraint(double lb, double ub,
                                          const string& name) {
  CheckNameValidity(name);
  CheckDuplicateName(name, &constraints_names_);
  MPConstraint* const constraint =
      new MPConstraint(lb, ub, name, interface_.get());
  constraints_.push_back(constraint);
  interface_->AddRowConstraint(constraint);
  return constraint;
}

MPConstraint* MPSolver::MakeRowConstraint(const string& name) {
  return MakeRowConstraint(-infinity(), infinity(), name);
}

// Compute the size of the constraint with the largest number of
// coefficients with index in [min_constraint_index,
// max_constraint_index)
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
      LOG(WARNING) << "Constraint " << constraints_[i]->name()
                   << " (" << i << ") has contradictory bounds:"
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

MPSolver::ResultStatus MPSolver::Solve(const MPSolverParameters &param) {
  // Special case for infeasible constraints so that all solvers have
  // the same behavior.
  if (HasInfeasibleConstraints()) {
    interface_->result_status_ = MPSolver::INFEASIBLE;
    return interface_->result_status_;
  }

  return interface_->Solve(param);
}

int64 MPSolver::iterations() const {
  return interface_->iterations();
}

int64 MPSolver::nodes() const {
  return interface_->nodes();
}

// ---------- MPSolverInterface ----------

const int MPSolverInterface::kDummyVariableIndex = 0;

MPSolverInterface::MPSolverInterface(MPSolver* const solver)
    : solver_(solver), sync_status_(MODEL_SYNCHRONIZED),
      result_status_(MPSolver::NOT_SOLVED), maximize_(false),
      last_constraint_index_(0), last_variable_index_(0),
      objective_value_(0.0), quiet_(true) {}

MPSolverInterface::~MPSolverInterface() {}

void MPSolverInterface::WriteModelToPredefinedFiles() {
  if (!FLAGS_solver_write_model.empty()) {
    if (!solver_->CheckAllNamesValidity()) {
      LOG(FATAL) << "Invalid name. Unable to write model to file";
    }
    WriteModel(FLAGS_solver_write_model);
  }
  const string filename = solver_->write_model_filename();
  if (!filename.empty()) {
    if (!solver_->CheckAllNamesValidity()) {
      LOG(FATAL) << "Invalid name. Unable to write model to file";
    }
    WriteModel(filename);
  }
}

// Extracts model stored in MPSolver
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
      CHECK_EQ(last_constraint_index_, solver_->constraints_.size());
      CHECK_EQ(last_variable_index_, solver_->variables_.size());
      break;
    }
    case SOLUTION_SYNCHRONIZED: {
      // Nothing has changed since last solve.
      CHECK_EQ(last_constraint_index_, solver_->constraints_.size());
      CHECK_EQ(last_variable_index_, solver_->variables_.size());
      break;
    }
  }
}

void MPSolverInterface::ResetExtractionInformation() {
  sync_status_ = MUST_RELOAD;
  last_constraint_index_ = 0;
  last_variable_index_ = 0;
  for (int j = 0; j < solver_->variables_.size(); ++j) {
    MPVariable* const var = solver_->variables_[j];
    var->set_index(kNoIndex);
  }
  for (int i = 0; i < solver_->constraints_.size(); ++i) {
    MPConstraint* const ct = solver_->constraints_[i];
    ct->set_index(kNoIndex);
  }
}

void MPSolverInterface::CheckSolutionIsSynchronized() const {
  CHECK_EQ(SOLUTION_SYNCHRONIZED, sync_status_) <<
      "The model has been changed since the solution was last computed.";
}

// Default version that can be overwritten by a solver-specific
// version to accomodate for the quirks of each solver.
void MPSolverInterface::CheckSolutionExists() const {
  CHECK(result_status_ == MPSolver::OPTIMAL ||
        result_status_ == MPSolver::FEASIBLE) <<
      "No solution exists.";
}

// Default version that can be overwritten by a solver-specific
// version to accomodate for the quirks of each solver.
void MPSolverInterface::CheckBestObjectiveBoundExists() const {
  CHECK(result_status_ == MPSolver::OPTIMAL ||
        result_status_ == MPSolver::FEASIBLE)
      << "No information is available for the best objective bound.";
}

double MPSolverInterface::objective_value() const {
  CheckSolutionIsSynchronized();
  CheckSolutionExists();
  return objective_value_;
}

void MPSolverInterface::InvalidateSolutionSynchronization() {
  if (sync_status_ == SOLUTION_SYNCHRONIZED) {
    sync_status_ = MODEL_SYNCHRONIZED;
  }
}

void MPSolverInterface::SetCommonParameters(const MPSolverParameters& param) {
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
  SetRelativeMipGap(param.GetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP));
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
    MPSolverParameters::DoubleParam param, int value) const {
  LOG(WARNING) << "Trying to set a supported parameter: " << param
               << " to an unsupported value: " << value;
}
void MPSolverInterface::SetIntegerParamToUnsupportedValue(
    MPSolverParameters::IntegerParam param, double value) const {
  LOG(WARNING) << "Trying to set a supported parameter: " << param
               << " to an unsupported value: " << value;
}

// ---------- MPSolverParameters ----------

const double MPSolverParameters::kDefaultRelativeMipGap = 1e-4;
const MPSolverParameters::PresolveValues MPSolverParameters::kDefaultPresolve =
  MPSolverParameters::PRESOLVE_ON;

const double MPSolverParameters::kDefaultDoubleParamValue = -1.0;
const int MPSolverParameters::kDefaultIntegerParamValue = -1;
const double MPSolverParameters::kUnknownDoubleParamValue = -2.0;
const int MPSolverParameters::kUnknownIntegerParamValue = -2;

// The constructor sets all parameters to their default value.
MPSolverParameters::MPSolverParameters()
    : relative_mip_gap_value_(kDefaultRelativeMipGap),
      presolve_value_(kDefaultPresolve),
      lp_algorithm_value_(kDefaultIntegerParamValue),
      lp_algorithm_is_default_(true) {}

void MPSolverParameters::SetDoubleParam(MPSolverParameters::DoubleParam param,
                                        double value) {
  switch (param) {
    case RELATIVE_MIP_GAP: {
      relative_mip_gap_value_ = value;
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
    case LP_ALGORITHM: {
      if (value != DUAL && value != PRIMAL && value != BARRIER) {
        LOG(ERROR) << "Trying to set a supported parameter: " << param
                   << " to an unknown value: " << value;
      }
      lp_algorithm_value_ = value;
      lp_algorithm_is_default_ = false;
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
    case LP_ALGORITHM: {
      lp_algorithm_is_default_ = true;
      break;
    }
    default: {
      LOG(ERROR) << "Trying to reset an unknown parameter: " << param << ".";
    }
  }
}

void MPSolverParameters::Reset() {
  ResetDoubleParam(RELATIVE_MIP_GAP);
  ResetIntegerParam(PRESOLVE);
  ResetIntegerParam(LP_ALGORITHM);
}

double MPSolverParameters::GetDoubleParam(
    MPSolverParameters::DoubleParam param) const {
  switch (param) {
    case RELATIVE_MIP_GAP: {
      return relative_mip_gap_value_;
    }
    default: {
      LOG(ERROR) << "Trying to get an unknown parameter: " << param << ".";
      return kUnknownDoubleParamValue;
    }
  }
}

int MPSolverParameters::GetIntegerParam(
    MPSolverParameters::IntegerParam param) const {
  switch (param) {
    case PRESOLVE: {
      return presolve_value_;
    }
    case LP_ALGORITHM: {
      if (lp_algorithm_is_default_) return kDefaultIntegerParamValue;
      return lp_algorithm_value_;
    }
    default: {
      LOG(ERROR) << "Trying to get an unknown parameter: " << param << ".";
      return kUnknownIntegerParamValue;
    }
  }
}

}  // namespace operations_research
