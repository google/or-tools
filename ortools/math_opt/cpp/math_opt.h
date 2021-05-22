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

// A C++ API for building optimization problems.
//
// Warning: Variable, LinearConstraint, and Objective are value types, see
// "Memory Model" below.
//
// A simple example:
//
// Model the problem:
//   max 2.0 * x + y
//   s.t. x + y <= 1.5
//            x in {0.0, 1.0}
//            y in [0.0, 2.5]
//
//   using ::operations_research::math_opt::LinearConstraint;
//   using ::operations_research::math_opt::Objective;
//   using ::operations_research::math_opt::MathOpt;
//   using ::operations_research::math_opt::Result;
//   using ::operations_research::math_opt::SolveParameters;
//   using ::operations_research::math_opt::SolveResultProto;
//   using ::operations_research::math_opt::Variable;
//
// Version 1:
//
//   MathOpt optimizer(operations_research::math_opt::SOLVER_TYPE_GSCIP,
//                     "my_model");
//   const Variable x = optimizer.AddBinaryVariable("x");
//   const Variable y = optimizer.AddContinuousVariable(0.0, 2.5, "y");
//   const LinearConstraint c = optimizer.AddLinearConstraint(
//       -std::numeric_limits<double>::infinity(), 1.5, "c");
//   c.set_coefficient(x, 1.0);
//   c.set_coefficient(y, 1.0);
//   const Objective obj = optimizer.objective();
//   obj.set_linear_coefficient(x, 2.0);
//   obj.set_linear_coefficient(y, 1.0);
//   obj.set_maximize();
//   const Result result = optimizer.Solve(SolveParametersProto()).value();
//   for (const auto& warning : result.warnings) {
//     std::cerr << "Solver warning: " << warning << std::endl;
//   }
//   CHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
//       << result.termination_detail;
//   // The following code will print:
//   //  objective value: 2.5
//   //  value for variable x: 1
//   std::cout << "objective value: " << result.objective_value()
//             << "\nvalue for variable x: " << result.variable_values().at(x)
//             << std::endl;
//
// Version 2 (with linear expressions):
//
//   MathOpt optimizer(operations_research::math_opt::SOLVER_TYPE_GSCIP,
//                     "my_model");
//   const Variable x = optimizer.AddBinaryVariable("x");
//   const Variable y = optimizer.AddContinuousVariable(0.0, 2.5, "y");
//   // We can directly use linear combinations of variables ...
//   optimizer.AddLinearConstraint(x + y <= 1.5, "c");
//   // ... or build them incrementally.
//   LinearExpression objective_expression;
//   objective_expression += 2*x;
//   objective_expression += y;
//   optimizer.objective().Maximize(objective_expression);
//   const Result result = optimizer.Solve(SolveParametersProto()).value();
//   for (const auto& warning : result.warnings) {
//     std::cerr << "Solver warning: " << warning << std::endl;
//   }
//   CHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
//       << result.termination_detail;
//   // The following code will print:
//   //  objective value: 2.5
//   //  value for variable x: 1
//   std::cout << "objective value: " << result.objective_value()
//             << "\nvalue for variable x: " << result.variable_values().at(x)
//             << std::endl;
//
// Memory model:
//
// Variable, LinearConstraint, and Objective are all value types that
// represent references to the underlying MathOpt object. They don't hold any of
// the actual model data, they can be copied, and they should be passed by
// value. They can be regenerated arbitrarily from MathOpt. MathOpt holds all
// the data.
//
// Performance:
//
// This class is a thin wrapper around IndexedModel (for incrementally building
// the model and reading it back, and producing the Model proto) and Solver (for
// consuming the Model proto to solve the optimization problem). Operations for
// building/reading/modifying the problem typically run in O(read/write size)
// and rely on hashing, see the indexed model documentation for details. At
// solve time (if you are solving locally) beware that there will be (at least)
// three copies of the model in memory, IndexedModel, the Model proto, and the
// underlying solver's copy(/ies). Note that the Model proto is reclaimed before
// the underlying solver begins solving.

#ifndef OR_TOOLS_MATH_OPT_CPP_MATH_OPT_H_
#define OR_TOOLS_MATH_OPT_CPP_MATH_OPT_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/callback.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/linear_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/model_solve_parameters.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/objective.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/result.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"  // IWYU pragma: export
#include "ortools/math_opt/model.pb.h"  // IWYU pragma: export
#include "ortools/math_opt/parameters.pb.h"  // IWYU pragma: export
#include "ortools/math_opt/result.pb.h"  // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// Models and solves mathematical optimization problems.
class MathOpt {
 public:
  using Callback = std::function<CallbackResult(CallbackData)>;

  MathOpt(const MathOpt&) = delete;
  MathOpt& operator=(const MathOpt&) = delete;

  // Creates an empty minimization problem.
  inline explicit MathOpt(
      SolverType solver_type, absl::string_view name = "",
      SolverInitializerProto solver_initializer = SolverInitializerProto());

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
  inline int next_variable_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_variable(int id) const;

  // Returns all the existing (created and not deleted) variables in the model
  // in an arbitrary order.
  std::vector<Variable> Variables();

  // Returns all the existing (created and not deleted) variables in the model,
  // sorted by id.
  std::vector<Variable> SortedVariables();

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
  //   MathOpt model = ...;
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
  inline int next_linear_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_linear_constraint(int id) const;

  // Returns all the existing (created and not deleted) linear constraints in
  // the model in an arbitrary order.
  std::vector<LinearConstraint> LinearConstraints();

  // Returns all the existing (created and not deleted) linear constraints in
  // the model sorted by id.
  std::vector<LinearConstraint> SortedLinearConstraints();

  inline Objective objective();

  // Solves the current optimization problem.
  //
  // A Status error will be returned if there is an unexpected failure in an
  // underlying solver or for some internal MathOpt errors. Otherwise, check
  // Result::termination_reason to see if an optimal solution was found.
  //
  // Memory model: the returned Result owns its own memory (for solutions, solve
  // stats, etc.), EXPECT for a pointer back to this->model_. As a result:
  //  * Keep this alive to access Result
  //  * Avoid unnecessarily copying Result,
  //  * The result is generally accessible after mutating this, but some care
  //    is needed if Variables or LinearConstraints are added or deleted.
  //
  // Asserts (using CHECK) that the inputs model_parameters and
  // callback_registration only contain variables and constraints from this
  // model.
  //
  // See callback.h for documentation on callback and callback_registration.
  absl::StatusOr<Result> Solve(
      const SolveParametersProto& solver_parameters,
      const ModelSolveParameters& model_parameters = {},
      const CallbackRegistration& callback_registration = {},
      Callback callback = nullptr);

  ModelProto ExportModel() const;

  // TODO(user): expose a way to efficiently iterate through the nonzeros of
  // the linear constraint matrix.
 private:
  // Asserts (with CHECK) that the input pointer is either nullptr or that it
  // points to the same model as model_.
  void CheckModel(IndexedModel* model);
  const SolverType solver_type_;
  const SolverInitializerProto solver_initializer_;
  const std::unique_ptr<IndexedModel> model_;
  std::unique_ptr<Solver> solver_;
  std::unique_ptr<IndexedModel::UpdateTracker> update_tracker_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

MathOpt::MathOpt(const SolverType solver_type, const absl::string_view name,
                 SolverInitializerProto solver_initializer)
    : solver_type_(solver_type),
      solver_initializer_(std::move(solver_initializer)),
      model_(absl::make_unique<IndexedModel>(name)) {}

const std::string& MathOpt::name() const { return model_->name(); }

Variable MathOpt::AddVariable(const absl::string_view name) {
  return Variable(model_.get(), model_->AddVariable(name));
}
Variable MathOpt::AddVariable(const double lower_bound,
                              const double upper_bound, const bool is_integer,
                              const absl::string_view name) {
  return Variable(model_.get(), model_->AddVariable(lower_bound, upper_bound,
                                                    is_integer, name));
}

Variable MathOpt::AddBinaryVariable(const absl::string_view name) {
  return AddVariable(0.0, 1.0, true, name);
}

Variable MathOpt::AddContinuousVariable(const double lower_bound,
                                        const double upper_bound,
                                        const absl::string_view name) {
  return AddVariable(lower_bound, upper_bound, false, name);
}

Variable MathOpt::AddIntegerVariable(const double lower_bound,
                                     const double upper_bound,
                                     const absl::string_view name) {
  return AddVariable(lower_bound, upper_bound, true, name);
}

void MathOpt::DeleteVariable(const Variable variable) {
  CHECK_EQ(model_.get(), variable.model());
  model_->DeleteVariable(variable.typed_id());
}

int MathOpt::num_variables() const { return model_->num_variables(); }

int MathOpt::next_variable_id() const {
  return model_->next_variable_id().value();
}

bool MathOpt::has_variable(const int id) const {
  return model_->has_variable(VariableId(id));
}

LinearConstraint MathOpt::AddLinearConstraint(const absl::string_view name) {
  return LinearConstraint(model_.get(), model_->AddLinearConstraint(name));
}
LinearConstraint MathOpt::AddLinearConstraint(const double lower_bound,
                                              const double upper_bound,
                                              const absl::string_view name) {
  return LinearConstraint(model_.get(), model_->AddLinearConstraint(
                                            lower_bound, upper_bound, name));
}

void MathOpt::DeleteLinearConstraint(const LinearConstraint constraint) {
  CHECK_EQ(model_.get(), constraint.model());
  model_->DeleteLinearConstraint(constraint.typed_id());
}

int MathOpt::num_linear_constraints() const {
  return model_->num_linear_constraints();
}

int MathOpt::next_linear_constraint_id() const {
  return model_->next_linear_constraint_id().value();
}

bool MathOpt::has_linear_constraint(const int id) const {
  return model_->has_linear_constraint(LinearConstraintId(id));
}

Objective MathOpt::objective() { return Objective(model_.get()); }

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MATH_OPT_H_
