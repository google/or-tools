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

#ifndef OR_TOOLS_MATH_OPT_CPP_MODEL_H_
#define OR_TOOLS_MATH_OPT_CPP_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/linear_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/update_tracker.h"     // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"  // IWYU pragma: export
#include "ortools/math_opt/model.pb.h"         // IWYU pragma: export
#include "ortools/math_opt/model_update.pb.h"  // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// A C++ API for building optimization problems.
//
// Warning: Variable and LinearConstraint are value types, see "Memory Model"
// below.
//
// A simple example:
//
// Model the problem:
//   max 2.0 * x + y
//   s.t. x + y <= 1.5
//            x in {0.0, 1.0}
//            y in [0.0, 2.5]
//
// math_opt::Model model("my_model");
// const math_opt::Variable x = model.AddBinaryVariable("x");
// const math_opt::Variable y = model.AddContinuousVariable(0.0, 2.5, "y");
// // We can directly use linear combinations of variables ...
// model.AddLinearConstraint(x + y <= 1.5, "c");
// // ... or build them incrementally.
// math_opt::LinearExpression objective_expression;
// objective_expression += 2 * x;
// objective_expression += y;
// model.Maximize(objective_expression);
// ASSIGN_OR_RETURN(const math_opt::SolveResult result,
//                  Solve(model, math_opt::SolverType::kGscip));
// switch (result.termination.reason) {
//   case math_opt::TerminationReason::kOptimal:
//   case math_opt::TerminationReason::kFeasible:
//     std::cout << "objective value: " << result.objective_value() << std::endl
//               << "value for variable x: " << result.variable_values().at(x)
//               << std::endl;
//     return absl::OkStatus();
//   default:
//     return util::InternalErrorBuilder()
//            << "model failed to solve: " << result.termination;
// }
//
// Memory model:
//
// Variable and LinearConstraint are value types that represent references to
// the underlying Model object. They don't hold any of the actual model data,
// they can be copied, and they should be passed by value. They can be
// regenerated arbitrarily from Model. Model holds all the data.
//
// Performance:
//
// This class is a thin wrapper around ModelStorage (for incrementally building
// the model and reading it back, and producing the Model proto) and Solver (for
// consuming the Model proto to solve the optimization problem). Operations for
// building/reading/modifying the problem typically run in O(read/write size)
// and rely on hashing, see the indexed model documentation for details. At
// solve time (if you are solving locally) beware that there will be (at least)
// three copies of the model in memory, ModelStorage, the Model proto, and the
// underlying solver's copy(/ies). Note that the Model proto is reclaimed before
// the underlying solver begins solving.
class Model {
 public:
  // Returns a model from the input proto. Returns a failure status if the input
  // proto is invalid.
  //
  // On top of loading a model from a MathOpt ModelProto, this function can also
  // be used to load a model from other formats using the functions in
  // math_opt/io/ like ReadMpsFile().
  //
  // See ExportModel() to get the proto of a Model. See ApplyUpdateProto() to
  // apply an update to the model.
  //
  // Usage example reading an MPS file:
  //   ASSIGN_OR_RETURN(const ModelProto model_proto, ReadMpsFile(path));
  //   ASSIGN_OR_RETURN(const std::unique_ptr<Model> model,
  //                    Model::FromModelProto(model_proto));
  static absl::StatusOr<std::unique_ptr<Model>> FromModelProto(
      const ModelProto& model_proto);

  // Creates an empty minimization problem.
  explicit Model(absl::string_view name = "");

  // Creates a model from the existing model storage.
  //
  // This constructor is used when loading a model, for example from a
  // ModelProto or an MPS file. Note that in those cases the FromModelProto()
  // should be used.
  explicit Model(std::unique_ptr<ModelStorage> storage);

  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // Returns a clone of this model.
  //
  // The variables and constraints have the same integer ids. The clone will
  // also not reused any id of variable/constraint that was deleted in the
  // original.
  //
  // That said, the Variable and LinearConstraint reference objects are model
  // specific. Hence the ones linked to the original model must NOT be used with
  // the clone. The Variable and LinearConstraint reference objects for the
  // clone can be obtained using:
  //   * the variable() and linear_constraint() methods on the ids from the old
  //     Variable and LinearConstraint objects.
  //   * in increasing id order using SortedVariables() and
  //     SortedLinearConstraints()
  //   * in an arbitrary order using Variables() and LinearConstraints().
  //
  // Note that the returned model does not have any update tracker.
  std::unique_ptr<Model> Clone() const;

  inline const std::string& name() const;

  // Adds a variable to the model and returns a reference to it.
  inline Variable AddVariable(double lower_bound, double upper_bound,
                              bool is_integer, absl::string_view name = "");

  // Adds a continuous unbounded variable to the model.
  inline Variable AddVariable(absl::string_view name = "");

  // Adds an variable to the model with domain {0, 1}.
  inline Variable AddBinaryVariable(absl::string_view name = "");

  // Adds a variable to the model with domain [lower_bound, upper_bound].
  inline Variable AddContinuousVariable(double lower_bound, double upper_bound,
                                        absl::string_view name = "");

  // Adds a variable to the model that can take integer values between
  // lower_bound and upper_bound (inclusive).
  inline Variable AddIntegerVariable(double lower_bound, double upper_bound,
                                     absl::string_view name = "");

  // Removes a variable from the model.
  //
  // It is an error to use any reference to this variable after this operation.
  // Runs in O(#constraints containing the variable).
  inline void DeleteVariable(Variable variable);

  // The number of variables in the model.
  //
  // Equal to the number of variables created minus the number of variables
  // deleted.
  inline int num_variables() const;

  // The returned id of the next call to AddVariable.
  //
  // Equal to the number of variables created.
  inline int64_t next_variable_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_variable(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_variable(VariableId id) const;

  // Will CHECK if has_variable(id) is false.
  inline Variable variable(int64_t id) const;

  // Will CHECK if has_variable(id) is false.
  inline Variable variable(VariableId id) const;

  // Returns the variable name.
  inline const std::string& name(Variable variable) const;

  // Sets a variable lower bound.
  inline void set_lower_bound(Variable variable, double lower_bound);

  // Returns a variable lower bound.
  inline double lower_bound(Variable variable) const;

  // Sets a variable upper bound.
  inline void set_upper_bound(Variable variable, double upper_bound);

  // Returns a variable upper bound.
  inline double upper_bound(Variable variable) const;

  // Sets the integrality of a variable.
  inline void set_is_integer(Variable variable, bool is_integer);

  // Makes the input variable integer.
  inline void set_integer(Variable variable);

  // Makes the input variable continuous.
  inline void set_continuous(Variable variable);

  // Returns the integrality of a variable.
  inline bool is_integer(Variable variable) const;

  // Returns all the existing (created and not deleted) variables in the model
  // in an arbitrary order.
  std::vector<Variable> Variables() const;

  // Returns all the existing (created and not deleted) variables in the model,
  // sorted by id.
  std::vector<Variable> SortedVariables() const;

  std::vector<LinearConstraint> ColumnNonzeros(Variable variable);

  // Adds a linear constraint to the model with bounds [-inf, +inf].
  inline LinearConstraint AddLinearConstraint(absl::string_view name = "");

  // Adds a linear constraint with bounds [lower_bound, upper_bound].
  inline LinearConstraint AddLinearConstraint(double lower_bound,
                                              double upper_bound,
                                              absl::string_view name = "");

  // Adds a linear constraint from the given bounded linear expression.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   model.AddLinearConstraint(3 <= 2 * x + y + 1 <= 5, "c");
  //   // The new constraint formula is:
  //   //   3 - 1 <= 2 * x + y <= 5 - 1
  //   // Which is:
  //   //   2 <= 2 * x + y <= 4
  //   // since the offset has been removed from bounds.
  //
  //   model.AddLinearConstraint(2 * x + y == x + 5 * z + 3);
  //   model.AddLinearConstraint(x >= 5);
  LinearConstraint AddLinearConstraint(
      const BoundedLinearExpression& bounded_expr, absl::string_view name = "");

  // Removes a linear constraint from the model.
  //
  // It is an error to use any reference to this linear constraint after this
  // operation. Runs in O(#variables in the linear constraint).
  inline void DeleteLinearConstraint(LinearConstraint constraint);

  // The number of linear constraints in the model.
  //
  // Equal to the number of linear constraints created minus the number of
  // linear constraints deleted.
  inline int num_linear_constraints() const;

  // The returned id of the next call to AddLinearConstraint.
  //
  // Equal to the number of linear constraints created.
  inline int64_t next_linear_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_linear_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_linear_constraint(LinearConstraintId id) const;

  // Will CHECK if has_linear_constraint(id) is false.
  inline LinearConstraint linear_constraint(int64_t id) const;

  // Will CHECK if has_linear_constraint(id) is false.
  inline LinearConstraint linear_constraint(LinearConstraintId id) const;

  // Returns the linear constraint name.
  inline const std::string& name(LinearConstraint constraint) const;

  // Sets a linear constraint lower bound.
  inline void set_lower_bound(LinearConstraint constraint, double lower_bound);

  // Returns a linear constraint lower bound.
  inline double lower_bound(LinearConstraint constraint) const;

  // Sets a linear constraint upper bound.
  inline void set_upper_bound(LinearConstraint constraint, double upper_bound);

  // Returns a linear constraint upper bound.
  inline double upper_bound(LinearConstraint constraint) const;

  // Setting a value to 0.0 will delete the {constraint, variable} pair from the
  // underlying sparse matrix representation (and has no effect if the pair is
  // not present).
  inline void set_coefficient(LinearConstraint constraint, Variable variable,
                              double value);

  // Returns 0.0 if the variable is not used in the constraint.
  inline double coefficient(LinearConstraint constraint,
                            Variable variable) const;

  inline bool is_coefficient_nonzero(LinearConstraint constraint,
                                     Variable variable) const;

  // This method modifies some internal structures of the model and thus is not
  // const.
  std::vector<Variable> RowNonzeros(LinearConstraint constraint);

  // This method modifies some internal structures of the model and thus is not
  // const.
  BoundedLinearExpression AsBoundedLinearExpression(
      LinearConstraint constraint);

  // Returns all the existing (created and not deleted) linear constraints in
  // the model in an arbitrary order.
  std::vector<LinearConstraint> LinearConstraints() const;

  // Returns all the existing (created and not deleted) linear constraints in
  // the model sorted by id.
  std::vector<LinearConstraint> SortedLinearConstraints() const;

  // Sets the objective to maximize the provided expression.
  inline void Maximize(double objective);
  // Sets the objective to maximize the provided expression.
  inline void Maximize(Variable objective);
  // Sets the objective to maximize the provided expression.
  inline void Maximize(LinearTerm objective);
  // Sets the objective to maximize the provided expression.
  inline void Maximize(const LinearExpression& objective);
  // Sets the objective to maximize the provided expression.
  inline void Maximize(const QuadraticExpression& objective);

  // Sets the objective to minimize the provided expression.
  inline void Minimize(double objective);
  // Sets the objective to minimize the provided expression.
  inline void Minimize(Variable objective);
  // Sets the objective to minimize the provided expression.
  inline void Minimize(LinearTerm objective);
  // Sets the objective to minimize the provided expression.
  inline void Minimize(const LinearExpression& objective);
  // Sets the objective to minimize the provided expression.
  inline void Minimize(const QuadraticExpression& objective);

  // Sets the objective to optimize the provided expression.
  inline void SetObjective(double objective, bool is_maximize);
  // Sets the objective to optimize the provided expression.
  inline void SetObjective(Variable objective, bool is_maximize);
  // Sets the objective to optimize the provided expression.
  inline void SetObjective(LinearTerm objective, bool is_maximize);
  // Sets the objective to optimize the provided expression.
  void SetObjective(const LinearExpression& objective, bool is_maximize);
  // Sets the objective to optimize the provided expression.
  void SetObjective(const QuadraticExpression& objective, bool is_maximize);

  // Adds the provided expression terms to the objective.
  inline void AddToObjective(double objective);
  // Adds the provided expression terms to the objective.
  inline void AddToObjective(Variable objective);
  // Adds the provided expression terms to the objective.
  inline void AddToObjective(LinearTerm objective);
  // Adds the provided expression terms to the objective.
  void AddToObjective(const LinearExpression& objective);
  // Adds the provided expression terms to the objective.
  void AddToObjective(const QuadraticExpression& objective);

  // NOTE: This will CHECK fail if the objective has quadratic terms.
  LinearExpression ObjectiveAsLinearExpression() const;
  QuadraticExpression ObjectiveAsQuadraticExpression() const;

  // Returns 0.0 if this variable has no linear objective coefficient.
  inline double objective_coefficient(Variable variable) const;

  // Returns 0.0 if this variable pair has no quadratic objective coefficient.
  // The order of the variables does not matter.
  inline double objective_coefficient(Variable first_variable,
                                      Variable second_variable) const;

  // Setting a value to 0.0 will delete the variable from the underlying sparse
  // representation (and has no effect if the variable is not present).
  inline void set_objective_coefficient(Variable variable, double value);

  // Set quadratic objective terms for the product of two variables. Setting a
  // value to 0.0 will delete the variable pair from the underlying sparse
  // representation (and has no effect if the pair is not present). The order of
  // the variables does not matter.
  inline void set_objective_coefficient(Variable first_variable,
                                        Variable second_variable, double value);

  // Equivalent to calling set_linear_coefficient(v, 0.0) for every variable
  // with nonzero objective coefficient.
  //
  // Runs in O(#linear and quadratic objective terms with nonzero coefficient).
  inline void clear_objective();

  inline bool is_objective_coefficient_nonzero(Variable variable) const;
  inline bool is_objective_coefficient_nonzero(Variable first_variable,
                                               Variable second_variable) const;

  inline double objective_offset() const;

  inline void set_objective_offset(double value);

  inline bool is_maximize() const;

  inline void set_maximize();
  inline void set_minimize();

  // Prefer set_maximize() and set_minimize() above for more readable code.
  inline void set_is_maximize(bool is_maximize);

  // Returns a proto representation of the optimization model.
  //
  // See FromModelProto() to build a Model from a proto.
  ModelProto ExportModel() const;

  // Returns a tracker that can be used to generate a ModelUpdateProto with the
  // updates that happened since the last checkpoint. The tracker initial
  // checkpoint corresponds to the current state of the model.
  //
  // The returned UpdateTracker keeps a reference to this model. See the
  // implications in the documentation of the UpdateTracker class.
  //
  // Thread-safety: this method must not be used while modifying the model
  // (variables, constraints, ...). The user is expected to use proper
  // synchronization primitive to serialize changes to the model and the use of
  // this method.
  std::unique_ptr<UpdateTracker> NewUpdateTracker();

  // Apply the provided update to this model. Returns a failure if the update is
  // not valid.
  //
  // As with FromModelProto(), duplicated names are ignored.
  //
  // Note that it takes O(num_variables + num_constraints) extra memory and
  // execution to apply the update (due to the need to build a ModelSummary). So
  // even a small update will have some cost.
  absl::Status ApplyUpdateProto(const ModelUpdateProto& update_proto);

  // TODO(user): expose a way to efficiently iterate through the nonzeros of
  // the linear constraint matrix.

  // Returns a pointer to the underlying model storage.
  //
  // This API is for internal use only and regular users should have no need for
  // it.
  const ModelStorage* storage() const { return storage_.get(); }

  // Returns a pointer to the underlying model storage.
  //
  // This API is for internal use only and regular users should have no need for
  // it.
  ModelStorage* storage() { return storage_.get(); }

 private:
  // Asserts (with CHECK) that the input pointer is either nullptr or that it
  // points to the same model as storage_.
  //
  // Use CheckModel() when nullptr is not a valid value.
  inline void CheckOptionalModel(const ModelStorage* other_storage) const;

  // Asserts (with CHECK) that the input pointer is the same as storage_.
  //
  // Use CheckOptionalModel() if nullptr is a valid value too.
  inline void CheckModel(const ModelStorage* other_storage) const;

  // Don't use storage_ directly; prefer to use storage() so that const member
  // functions don't have modifying access to the underlying storage.
  //
  // We use a shared_ptr here so that the UpdateTracker class can have a
  // weak_ptr on the ModelStorage. This let it have a destructor that don't
  // crash when called after the destruction of the associated Model.
  const std::shared_ptr<ModelStorage> storage_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

const std::string& Model::name() const { return storage()->name(); }

Variable Model::AddVariable(const absl::string_view name) {
  return Variable(storage(), storage()->AddVariable(name));
}
Variable Model::AddVariable(const double lower_bound, const double upper_bound,
                            const bool is_integer,
                            const absl::string_view name) {
  return Variable(storage(), storage()->AddVariable(lower_bound, upper_bound,
                                                    is_integer, name));
}

Variable Model::AddBinaryVariable(const absl::string_view name) {
  return AddVariable(0.0, 1.0, true, name);
}

Variable Model::AddContinuousVariable(const double lower_bound,
                                      const double upper_bound,
                                      const absl::string_view name) {
  return AddVariable(lower_bound, upper_bound, false, name);
}

Variable Model::AddIntegerVariable(const double lower_bound,
                                   const double upper_bound,
                                   const absl::string_view name) {
  return AddVariable(lower_bound, upper_bound, true, name);
}

void Model::DeleteVariable(const Variable variable) {
  CheckModel(variable.storage());
  storage()->DeleteVariable(variable.typed_id());
}

int Model::num_variables() const { return storage()->num_variables(); }

int64_t Model::next_variable_id() const {
  return storage()->next_variable_id().value();
}

bool Model::has_variable(const int64_t id) const {
  return has_variable(VariableId(id));
}

bool Model::has_variable(const VariableId id) const {
  return storage()->has_variable(id);
}

Variable Model::variable(const int64_t id) const {
  return variable(VariableId(id));
}

Variable Model::variable(const VariableId id) const {
  CHECK(has_variable(id)) << "No variable with id: " << id.value();
  return Variable(storage(), id);
}

const std::string& Model::name(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->variable_name(variable.typed_id());
}

void Model::set_lower_bound(const Variable variable, double lower_bound) {
  CheckModel(variable.storage());
  storage()->set_variable_lower_bound(variable.typed_id(), lower_bound);
}

double Model::lower_bound(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->variable_lower_bound(variable.typed_id());
}

void Model::set_upper_bound(const Variable variable, double upper_bound) {
  CheckModel(variable.storage());
  storage()->set_variable_upper_bound(variable.typed_id(), upper_bound);
}

double Model::upper_bound(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->variable_upper_bound(variable.typed_id());
}

void Model::set_is_integer(const Variable variable, bool is_integer) {
  CheckModel(variable.storage());
  storage()->set_variable_is_integer(variable.typed_id(), is_integer);
}

void Model::set_integer(const Variable variable) {
  set_is_integer(variable, true);
}

void Model::set_continuous(const Variable variable) {
  set_is_integer(variable, false);
}

bool Model::is_integer(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->is_variable_integer(variable.typed_id());
}

LinearConstraint Model::AddLinearConstraint(const absl::string_view name) {
  return LinearConstraint(storage(), storage()->AddLinearConstraint(name));
}
LinearConstraint Model::AddLinearConstraint(const double lower_bound,
                                            const double upper_bound,
                                            const absl::string_view name) {
  return LinearConstraint(storage(), storage()->AddLinearConstraint(
                                         lower_bound, upper_bound, name));
}

void Model::DeleteLinearConstraint(const LinearConstraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteLinearConstraint(constraint.typed_id());
}

int Model::num_linear_constraints() const {
  return storage()->num_linear_constraints();
}

int64_t Model::next_linear_constraint_id() const {
  return storage()->next_linear_constraint_id().value();
}

bool Model::has_linear_constraint(const int64_t id) const {
  return has_linear_constraint(LinearConstraintId(id));
}

bool Model::has_linear_constraint(const LinearConstraintId id) const {
  return storage()->has_linear_constraint(id);
}

LinearConstraint Model::linear_constraint(const int64_t id) const {
  return linear_constraint(LinearConstraintId(id));
}

LinearConstraint Model::linear_constraint(const LinearConstraintId id) const {
  CHECK(has_linear_constraint(id))
      << "No linear constraint with id: " << id.value();
  return LinearConstraint(storage(), id);
}

const std::string& Model::name(const LinearConstraint constraint) const {
  CheckModel(constraint.storage());
  return storage()->linear_constraint_name(constraint.typed_id());
}

void Model::set_lower_bound(const LinearConstraint constraint,
                            double lower_bound) {
  CheckModel(constraint.storage());
  storage()->set_linear_constraint_lower_bound(constraint.typed_id(),
                                               lower_bound);
}

double Model::lower_bound(const LinearConstraint constraint) const {
  CheckModel(constraint.storage());
  return storage()->linear_constraint_lower_bound(constraint.typed_id());
}

void Model::set_upper_bound(const LinearConstraint constraint,
                            const double upper_bound) {
  CheckModel(constraint.storage());
  storage()->set_linear_constraint_upper_bound(constraint.typed_id(),
                                               upper_bound);
}

double Model::upper_bound(const LinearConstraint constraint) const {
  CheckModel(constraint.storage());
  return storage()->linear_constraint_upper_bound(constraint.typed_id());
}

void Model::set_coefficient(const LinearConstraint constraint,
                            const Variable variable, const double value) {
  CheckModel(constraint.storage());
  CheckModel(variable.storage());
  storage()->set_linear_constraint_coefficient(constraint.typed_id(),
                                               variable.typed_id(), value);
}

double Model::coefficient(const LinearConstraint constraint,
                          const Variable variable) const {
  CheckModel(constraint.storage());
  CheckModel(variable.storage());
  return storage()->linear_constraint_coefficient(constraint.typed_id(),
                                                  variable.typed_id());
}

bool Model::is_coefficient_nonzero(const LinearConstraint constraint,
                                   const Variable variable) const {
  CheckModel(constraint.storage());
  CheckModel(variable.storage());
  return storage()->is_linear_constraint_coefficient_nonzero(
      constraint.typed_id(), variable.typed_id());
}

void Model::Maximize(const double objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/true);
}
void Model::Maximize(const Variable objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/true);
}
void Model::Maximize(const LinearTerm objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/true);
}
void Model::Maximize(const LinearExpression& objective) {
  SetObjective(objective, /*is_maximize=*/true);
}
void Model::Maximize(const QuadraticExpression& objective) {
  SetObjective(objective, /*is_maximize=*/true);
}

void Model::Minimize(const double objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/false);
}
void Model::Minimize(const Variable objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/false);
}
void Model::Minimize(const LinearTerm objective) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/false);
}
void Model::Minimize(const LinearExpression& objective) {
  SetObjective(objective, /*is_maximize=*/false);
}
void Model::Minimize(const QuadraticExpression& objective) {
  SetObjective(objective, /*is_maximize=*/false);
}

void Model::SetObjective(const double objective, const bool is_maximize) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/is_maximize);
}
void Model::SetObjective(const Variable objective, const bool is_maximize) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/is_maximize);
}
void Model::SetObjective(const LinearTerm objective, const bool is_maximize) {
  SetObjective(LinearExpression(objective), /*is_maximize=*/is_maximize);
}

void Model::AddToObjective(const double objective) {
  AddToObjective(LinearExpression(objective));
}
void Model::AddToObjective(const Variable objective) {
  AddToObjective(LinearExpression(objective));
}
void Model::AddToObjective(const LinearTerm objective) {
  AddToObjective(LinearExpression(objective));
}

double Model::objective_coefficient(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->linear_objective_coefficient(variable.typed_id());
}

double Model::objective_coefficient(const Variable first_variable,
                                    const Variable second_variable) const {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  return storage()->quadratic_objective_coefficient(first_variable.typed_id(),
                                                    second_variable.typed_id());
}

void Model::set_objective_coefficient(const Variable variable,
                                      const double value) {
  CheckModel(variable.storage());
  storage()->set_linear_objective_coefficient(variable.typed_id(), value);
}

void Model::set_objective_coefficient(const Variable first_variable,
                                      const Variable second_variable,
                                      const double value) {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  storage()->set_quadratic_objective_coefficient(
      first_variable.typed_id(), second_variable.typed_id(), value);
}

void Model::clear_objective() { storage()->clear_objective(); }

bool Model::is_objective_coefficient_nonzero(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->is_linear_objective_coefficient_nonzero(
      variable.typed_id());
}

bool Model::is_objective_coefficient_nonzero(
    const Variable first_variable, const Variable second_variable) const {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  return storage()->is_quadratic_objective_coefficient_nonzero(
      first_variable.typed_id(), second_variable.typed_id());
}

double Model::objective_offset() const { return storage()->objective_offset(); }

void Model::set_objective_offset(const double value) {
  storage()->set_objective_offset(value);
}

bool Model::is_maximize() const { return storage()->is_maximize(); }

void Model::set_maximize() { storage()->set_maximize(); }

void Model::set_minimize() { storage()->set_minimize(); }

void Model::set_is_maximize(const bool is_maximize) {
  storage()->set_is_maximize(is_maximize);
}

void Model::CheckOptionalModel(const ModelStorage* const other_storage) const {
  if (other_storage != nullptr) {
    CHECK_EQ(other_storage, storage())
        << internal::kObjectsFromOtherModelStorage;
  }
}

void Model::CheckModel(const ModelStorage* const other_storage) const {
  CHECK_EQ(other_storage, storage()) << internal::kObjectsFromOtherModelStorage;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MODEL_H_
