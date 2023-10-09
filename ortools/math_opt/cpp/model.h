// Copyright 2010-2022 Google LLC
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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_MODEL_H_
#define OR_TOOLS_MATH_OPT_CPP_MODEL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/linear_constraint.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/objective.h"          // IWYU pragma: export
#include "ortools/math_opt/cpp/update_tracker.h"     // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"  // IWYU pragma: export
#include "ortools/math_opt/model.pb.h"               // IWYU pragma: export
#include "ortools/math_opt/model_update.pb.h"        // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage.h"  // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage_types.h"  // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// A C++ API for building optimization problems.
//
// Warning: Variable and LinearConstraint (along with all other constraint
// objects) are value types, see "Memory Model" below.
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
// Variable, LinearConstraint, QuadraticConstraint, etc. are value types that
// represent references to the underlying Model object. They don't hold any of
// the actual model data, they can be copied, and they should be passed by
// value. They can be regenerated arbitrarily from Model. Model holds all the
// data.
//
// As a consequence of Variable and LinearConstraint holding back pointers,
// Model is not copyable or movable. Users needing to copy a Model can call
// Model::Clone() (this will create a new Model with no update trackers), and
// users needing to move a Model should wrap it in a std::unique_ptr.
//
// Performance:
//
// This class is a thin wrapper around ModelStorage (for incrementally building
// the model and reading it back, and producing the Model proto). Operations for
// building/reading/modifying the problem typically run in O(read/write size)
// and rely on hashing, see the ModelStorage documentation for details. At
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

  // Returns a clone of this model, optionally changing the model's name.
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
  std::unique_ptr<Model> Clone(
      std::optional<absl::string_view> new_name = std::nullopt) const;

  inline const std::string& name() const;

  //////////////////////////////////////////////////////////////////////////////
  // Variable methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds a variable to the model and returns a reference to it.
  inline Variable AddVariable(double lower_bound, double upper_bound,
                              bool is_integer, absl::string_view name = "");

  // Adds a continuous unbounded variable to the model.
  inline Variable AddVariable(absl::string_view name = "");

  // Adds a variable to the model with domain {0, 1}.
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

  // Returns an error if `variable` is from another model or the id is not in
  // this model (typically, if it was deleted).
  inline absl::Status ValidateExistingVariableOfThisModel(
      Variable variable) const;

  std::vector<LinearConstraint> ColumnNonzeros(Variable variable) const;

  //////////////////////////////////////////////////////////////////////////////
  // LinearConstraint methods
  //////////////////////////////////////////////////////////////////////////////

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

  std::vector<Variable> RowNonzeros(LinearConstraint constraint) const;

  // Returns all the existing (created and not deleted) linear constraints in
  // the model in an arbitrary order.
  std::vector<LinearConstraint> LinearConstraints() const;

  // Returns all the existing (created and not deleted) linear constraints in
  // the model sorted by id.
  std::vector<LinearConstraint> SortedLinearConstraints() const;

  // Returns an error if `linear_constraint` is from another model or the id is
  // not in this model (typically, if it was deleted).
  inline absl::Status ValidateExistingLinearConstraintOfThisModel(
      LinearConstraint linear_constraint) const;

  //////////////////////////////////////////////////////////////////////////////
  // QuadraticConstraint methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds a quadratic constraint from the given bounded quadratic expression.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   model.AddQuadraticConstraint(2 * x * x + y + 1 <= 5, "q");
  //   model.AddQuadraticConstraint(2 * x * x + y * y == x + 5 * z + 3);
  //   model.AddQuadraticConstraint(x * y >= 5);
  QuadraticConstraint AddQuadraticConstraint(
      const BoundedQuadraticExpression& bounded_expr,
      absl::string_view name = "");

  // Removes a quadratic constraint from the model.
  //
  // It is an error to use any reference to this quadratic constraint after this
  // operation. Runs in O(#linear or quadratic terms appearing in constraint).
  inline void DeleteQuadraticConstraint(QuadraticConstraint constraint);

  // The number of quadratic constraints in the model.
  //
  // Equal to the number of quadratic constraints created minus the number of
  // quadratic constraints deleted.
  inline int64_t num_quadratic_constraints() const;

  // The returned id of the next call to AddQuadraticConstraint.
  inline int64_t next_quadratic_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_quadratic_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_quadratic_constraint(QuadraticConstraintId id) const;

  // Will CHECK if has_quadratic_constraint(id) is false.
  inline QuadraticConstraint quadratic_constraint(int64_t id) const;

  // Will CHECK if has_quadratic_constraint(id) is false.
  inline QuadraticConstraint quadratic_constraint(
      QuadraticConstraintId id) const;

  // Returns all the existing (created and not deleted) quadratic constraints in
  // the model in an arbitrary order.
  inline std::vector<QuadraticConstraint> QuadraticConstraints() const;

  // Returns all the existing (created and not deleted) quadratic constraints in
  // the model sorted by id.
  inline std::vector<QuadraticConstraint> SortedQuadraticConstraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // SecondOrderConeConstraint methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds a second-order cone constraint to the model of the form
  //    ||arguments_to_norm||₂ ≤ upper_bound.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   model.AddSecondOrderConeConstraint({x, y}, 1.0, "soc");
  //   model.AddSecondOrderConeConstraint({1.0, 3 * y - x}, 2 * x);
  SecondOrderConeConstraint AddSecondOrderConeConstraint(
      const std::vector<LinearExpression>& arguments_to_norm,
      const LinearExpression& upper_bound, absl::string_view name = "");

  // Removes a second-order cone constraint from the model.
  //
  // It is an error to use any reference to this second-order cone constraint
  // after this operation. Runs in O(#linear terms appearing in constraint).
  inline void DeleteSecondOrderConeConstraint(
      SecondOrderConeConstraint constraint);

  // The number of second-order cone constraints in the model.
  //
  // Equal to the number of second-order cone constraints created minus the
  // number of second-order cone constraints deleted.
  inline int64_t num_second_order_cone_constraints() const;

  // The returned id of the next call to AddSecondOrderConeConstraint.
  inline int64_t next_second_order_cone_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_second_order_cone_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_second_order_cone_constraint(
      SecondOrderConeConstraintId id) const;

  // Will CHECK if has_second_order_cone_constraint(id) is false.
  inline SecondOrderConeConstraint second_order_cone_constraint(
      int64_t id) const;

  // Will CHECK if has_second_order_cone_constraint(id) is false.
  inline SecondOrderConeConstraint second_order_cone_constraint(
      SecondOrderConeConstraintId id) const;

  // Returns all the existing (created and not deleted) second-order cone
  // constraints in the model in an arbitrary order.
  inline std::vector<SecondOrderConeConstraint> SecondOrderConeConstraints()
      const;

  // Returns all the existing (created and not deleted) second-order cone
  // constraints in the model sorted by id.
  inline std::vector<SecondOrderConeConstraint>
  SortedSecondOrderConeConstraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // Sos1Constraint methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds an SOS1 constraint to the model: at most one of the `expressions` may
  // take a nonzero value.
  //
  // The `weights` are an implementation detail in the solver used to order the
  // `expressions`; see the Gurobi documentation for more detail:
  // https://www.gurobi.com/documentation/9.5/refman/constraints.html#subsubsection:SOSConstraints
  //
  // These `weights` must either be empty or the same length as `expressions`.
  // If it is empty, default weights of 1, 2, ... will be used.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   model.AddSos1Constraint({x, y}, {}, "c");
  //   model.AddSos1Constraint({1 - 2 * x, y}, {3, 2});
  Sos1Constraint AddSos1Constraint(
      const std::vector<LinearExpression>& expressions,
      std::vector<double> weights = {}, absl::string_view name = "");

  // Removes an SOS1 constraint from the model.
  //
  // It is an error to use any reference to this SOS1 constraint after this
  // operation. Runs in O(#terms in all expressions).
  inline void DeleteSos1Constraint(Sos1Constraint constraint);

  // The number of SOS1 constraints in the model.
  //
  // Equal to the number of SOS1 constraints created minus the number of SOS1
  // constraints deleted.
  inline int64_t num_sos1_constraints() const;

  // The returned id of the next call to AddSos1Constraint.
  inline int64_t next_sos1_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_sos1_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_sos1_constraint(Sos1ConstraintId id) const;

  // Will CHECK if has_sos1_constraint(id) is false.
  inline Sos1Constraint sos1_constraint(int64_t id) const;

  // Will CHECK if has_sos1_constraint(id) is false.
  inline Sos1Constraint sos1_constraint(Sos1ConstraintId id) const;

  // Returns all the existing (created and not deleted) SOS1 constraints in the
  // model in an arbitrary order.
  inline std::vector<Sos1Constraint> Sos1Constraints() const;

  // Returns all the existing (created and not deleted) SOS1 constraints in the
  // model sorted by id.
  inline std::vector<Sos1Constraint> SortedSos1Constraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // Sos2Constraint methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds an SOS2 constraint to the model: at most two of the `expressions` may
  // take a nonzero value, and they must be adjacent in their ordering.
  //
  // The `weights` are an implementation detail in the solver used to order the
  // `expressions`; see the Gurobi documentation for more detail:
  // https://www.gurobi.com/documentation/9.5/refman/constraints.html#subsubsection:SOSConstraints
  //
  // These `weights` must either be empty or the same length as `expressions`.
  // If it is empty, default weights of 1, 2, ... will be used.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   model.AddSos2Constraint({x, y}, {}, "c");
  //   model.AddSos2Constraint({1 - 2 * x, y}, {3, 2});
  Sos2Constraint AddSos2Constraint(
      const std::vector<LinearExpression>& expressions,
      std::vector<double> weights = {}, absl::string_view name = "");

  // Removes an SOS2 constraint from the model.
  //
  // It is an error to use any reference to this SOS2 constraint after this
  // operation. Runs in O(#terms in all expressions).
  inline void DeleteSos2Constraint(Sos2Constraint constraint);

  // The number of SOS2 constraints in the model.
  //
  // Equal to the number of SOS2 constraints created minus the number of SOS2
  // constraints deleted.
  inline int64_t num_sos2_constraints() const;

  // The returned id of the next call to AddSos2Constraint.
  inline int64_t next_sos2_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_sos2_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_sos2_constraint(Sos2ConstraintId id) const;

  // Will CHECK if has_sos2_constraint(id) is false.
  inline Sos2Constraint sos2_constraint(int64_t id) const;

  // Will CHECK if has_sos2_constraint(id) is false.
  inline Sos2Constraint sos2_constraint(Sos2ConstraintId id) const;

  // Returns all the existing (created and not deleted) SOS2 constraints in the
  // model in an arbitrary order.
  inline std::vector<Sos2Constraint> Sos2Constraints() const;

  // Returns all the existing (created and not deleted) SOS2 constraints in the
  // model sorted by id.
  inline std::vector<Sos2Constraint> SortedSos2Constraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // IndicatorConstraint methods
  //////////////////////////////////////////////////////////////////////////////

  // Adds an indicator constraint to the model.
  //
  // Assume for the moment that `activate_on_zero == false` (the default value).
  //   * If `indicator_variable == 1`, then `implied_constraint` must  hold.
  //   * If `indicator_variable == 0`, then `implied_constraint` need not hold.
  // Alternatively, if `activate_on_zero = true`, flip the 1 and 0 above.
  //
  // The `indicator_variable` is expected to be a binary variable in the model.
  // If this is not the case, the solver may elect to either implicitly add the
  // binary constraint, or reject the model.
  //
  // Usage:
  //   Model model = ...;
  //   const Variable x = model.AddBinaryVariable("x");
  //   const Variable y = model.AddBinaryVariable("y");
  //   model.AddIndicatorConstraint(x, y <= 0);
  //   model.AddIndicatorConstraint(y, x >= 2, true, "c");
  IndicatorConstraint AddIndicatorConstraint(
      Variable indicator_variable,
      const BoundedLinearExpression& implied_constraint,
      bool activate_on_zero = false, absl::string_view name = {});

  // Removes an indicator constraint from the model.
  //
  // It is an error to use any reference to this indicator constraint after this
  // operation. Runs in O(#terms in implied constraint).
  inline void DeleteIndicatorConstraint(IndicatorConstraint constraint);

  // The number of indicator constraints in the model.
  //
  // Equal to the number of indicator constraints created minus the number of
  // indicator constraints deleted.
  inline int64_t num_indicator_constraints() const;

  // The returned id of the next call to AddIndicatorConstraint.
  inline int64_t next_indicator_constraint_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_indicator_constraint(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_indicator_constraint(IndicatorConstraintId id) const;

  // Will CHECK if has_indicator_constraint(id) is false.
  inline IndicatorConstraint indicator_constraint(int64_t id) const;

  // Will CHECK if has_indicator_constraint(id) is false.
  inline IndicatorConstraint indicator_constraint(
      IndicatorConstraintId id) const;

  // Returns all the existing (created and not deleted) indicator constraints in
  // the model in an arbitrary order.
  inline std::vector<IndicatorConstraint> IndicatorConstraints() const;

  // Returns all the existing (created and not deleted) indicator constraints in
  // the model sorted by id.
  inline std::vector<IndicatorConstraint> SortedIndicatorConstraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // Objective methods
  //////////////////////////////////////////////////////////////////////////////

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

  // Returns an object referring to the primary objective in the model. Can be
  // used with the multi-objective API in the same way that an auxiliary
  // objective can be.
  inline Objective primary_objective() const;

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

  // Sets the objective offset, linear terms, and quadratic terms of the
  // objective to zero. The name, direction, and priority are unchanged.
  // Equivalent to SetObjective(0.0, is_maximize()).
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

  //////////////////////////////////////////////////////////////////////////////
  // Auxiliary objective methods
  //
  // This is an API for creating and deleting auxiliary objectives. To modify
  // them, use the multi-objective API below.
  //////////////////////////////////////////////////////////////////////////////

  // Adds an empty (== 0) auxiliary minimization objective to the model.
  inline Objective AddAuxiliaryObjective(int64_t priority,
                                         absl::string_view name = {});
  // Adds `expression` as an auxiliary objective to the model.
  inline Objective AddAuxiliaryObjective(const LinearExpression& expression,
                                         bool is_maximize, int64_t priority,
                                         absl::string_view name = {});
  // Adds `expression` as an auxiliary maximization objective to the model.
  inline Objective AddMaximizationObjective(const LinearExpression& expression,
                                            int64_t priority,
                                            absl::string_view name = {});
  // Adds `expression` as an auxiliary minimization objective to the model.
  inline Objective AddMinimizationObjective(const LinearExpression& expression,
                                            int64_t priority,
                                            absl::string_view name = {});

  // Removes an auxiliary objective from the model.
  //
  // It is an error to use any reference to this auxiliary objective after this
  // operation. Runs in O(1) time.
  //
  // Will CHECK-fail if `objective` is from another model, has already been
  // deleted, or is a primary objective.
  inline void DeleteAuxiliaryObjective(Objective objective);

  // The number of auxiliary objectives in the model.
  //
  // Equal to the number of auxiliary objectives created minus the number of
  // auxiliary objectives deleted.
  inline int64_t num_auxiliary_objectives() const;

  // The returned id of the next call to AddAuxiliaryObjectve.
  inline int64_t next_auxiliary_objective_id() const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_auxiliary_objective(int64_t id) const;

  // Returns true if this id has been created and not yet deleted.
  inline bool has_auxiliary_objective(AuxiliaryObjectiveId id) const;

  // Will CHECK if has_auxiliary_objective(id) is false.
  inline Objective auxiliary_objective(int64_t id) const;

  // Will CHECK if has_auxiliary_objective(id) is false.
  inline Objective auxiliary_objective(AuxiliaryObjectiveId id) const;

  // Returns all the existing (created and not deleted) auxiliary objectives in
  // the model in an arbitrary order.
  std::vector<Objective> AuxiliaryObjectives() const;

  // Returns all the existing (created and not deleted) auxiliary objectives in
  // the model sorted by id.
  std::vector<Objective> SortedAuxiliaryObjectives() const;

  //////////////////////////////////////////////////////////////////////////////
  // Multi-objective methods
  //
  // This is an API for setting objective properties (for either primary or
  // auxiliary objectives). Only linear objectives are supported through this
  // API. To query objective properties, use the methods on `Objective`.
  //////////////////////////////////////////////////////////////////////////////

  // Sets `objective` to be maximizing `expression`.
  inline void Maximize(Objective objective, const LinearExpression& expression);
  // Sets `objective` to be minimizing `expression`.
  inline void Minimize(Objective objective, const LinearExpression& expression);
  // Sets the objective to optimize the provided expression.
  void SetObjective(Objective objective, const LinearExpression& expression,
                    bool is_maximize);

  // Adds the provided expression terms to the objective.
  void AddToObjective(Objective objective, const LinearExpression& expression);

  // Sets the priority for an objective (lower is more important). `priority`
  // must be nonnegative.
  inline void set_objective_priority(Objective objective, int64_t priority);

  // Setting a value to 0.0 will delete the variable from the underlying sparse
  // representation (and has no effect if the variable is not present).
  inline void set_objective_coefficient(Objective objective, Variable variable,
                                        double value);

  inline void set_objective_offset(Objective objective, double value);

  inline void set_maximize(Objective objective);
  inline void set_minimize(Objective objective);

  // Prefer set_maximize() and set_minimize() above for more readable code.
  inline void set_is_maximize(Objective objective, bool is_maximize);

  // Returns a proto representation of the optimization model.
  //
  // See FromModelProto() to build a Model from a proto.
  ModelProto ExportModel(bool remove_names = false) const;

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

  // Prints the objective, the constraints and the variables of the model over
  // several lines in a human-readable way. Includes a new line at the end of
  // the model.
  friend std::ostream& operator<<(std::ostream& ostr, const Model& model);

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

// ------------------------------- Variables -----------------------------------

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

// -------------------------- Linear constraints -------------------------------

absl::Status Model::ValidateExistingVariableOfThisModel(
    Variable variable) const {
  // TODO(b/239810718): use << for Variable once it does not CHECK.
  if (storage_.get() != variable.storage()) {
    return util::InvalidArgumentErrorBuilder()
           << "variable with id " << variable.id()
           << " is from a different model";
  }
  if (!has_variable(variable.typed_id())) {
    return util::InvalidArgumentErrorBuilder()
           << "variable with id " << variable.id()
           << " is not found in this model (it was probably deleted)";
  }
  return absl::OkStatus();
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

absl::Status Model::ValidateExistingLinearConstraintOfThisModel(
    LinearConstraint linear_constraint) const {
  // TODO(b/239810718): use << for LinearConstraint once it does not CHECK.
  if (storage_.get() != linear_constraint.storage()) {
    return util::InvalidArgumentErrorBuilder()
           << "linear constraint with id " << linear_constraint.id()
           << " is from a different model";
  }
  if (!has_linear_constraint(linear_constraint.typed_id())) {
    return util::InvalidArgumentErrorBuilder()
           << "linear constraint with id " << linear_constraint.id()
           << " is not found in this model (it was probably deleted)";
  }
  return absl::OkStatus();
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

// ------------------------- Quadratic constraints -----------------------------

void Model::DeleteQuadraticConstraint(const QuadraticConstraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteAtomicConstraint(constraint.typed_id());
}

int64_t Model::num_quadratic_constraints() const {
  return storage()->num_constraints<QuadraticConstraintId>();
}

int64_t Model::next_quadratic_constraint_id() const {
  return storage()->next_constraint_id<QuadraticConstraintId>().value();
}

bool Model::has_quadratic_constraint(const int64_t id) const {
  return has_quadratic_constraint(QuadraticConstraintId(id));
}

bool Model::has_quadratic_constraint(const QuadraticConstraintId id) const {
  return storage()->has_constraint(id);
}

QuadraticConstraint Model::quadratic_constraint(const int64_t id) const {
  return quadratic_constraint(QuadraticConstraintId(id));
}

QuadraticConstraint Model::quadratic_constraint(
    const QuadraticConstraintId id) const {
  CHECK(has_quadratic_constraint(id))
      << "No quadratic constraint with id: " << id.value();
  return QuadraticConstraint(storage(), id);
}

std::vector<QuadraticConstraint> Model::QuadraticConstraints() const {
  return AtomicConstraints<QuadraticConstraint>(*storage());
}

std::vector<QuadraticConstraint> Model::SortedQuadraticConstraints() const {
  return SortedAtomicConstraints<QuadraticConstraint>(*storage());
}

// --------------------- Second-order cone constraints -------------------------

void Model::DeleteSecondOrderConeConstraint(
    const SecondOrderConeConstraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteAtomicConstraint(constraint.typed_id());
}

int64_t Model::num_second_order_cone_constraints() const {
  return storage()->num_constraints<SecondOrderConeConstraintId>();
}

int64_t Model::next_second_order_cone_constraint_id() const {
  return storage()->next_constraint_id<SecondOrderConeConstraintId>().value();
}

bool Model::has_second_order_cone_constraint(const int64_t id) const {
  return has_second_order_cone_constraint(SecondOrderConeConstraintId(id));
}

bool Model::has_second_order_cone_constraint(
    const SecondOrderConeConstraintId id) const {
  return storage()->has_constraint(id);
}

SecondOrderConeConstraint Model::second_order_cone_constraint(
    const int64_t id) const {
  return second_order_cone_constraint(SecondOrderConeConstraintId(id));
}

SecondOrderConeConstraint Model::second_order_cone_constraint(
    const SecondOrderConeConstraintId id) const {
  CHECK(has_second_order_cone_constraint(id))
      << "No second-order cone constraint with id: " << id.value();
  return SecondOrderConeConstraint(storage(), id);
}

std::vector<SecondOrderConeConstraint> Model::SecondOrderConeConstraints()
    const {
  return AtomicConstraints<SecondOrderConeConstraint>(*storage());
}

std::vector<SecondOrderConeConstraint> Model::SortedSecondOrderConeConstraints()
    const {
  return SortedAtomicConstraints<SecondOrderConeConstraint>(*storage());
}

// --------------------------- SOS1 constraints --------------------------------

void Model::DeleteSos1Constraint(const Sos1Constraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteAtomicConstraint(constraint.typed_id());
}

int64_t Model::num_sos1_constraints() const {
  return storage()->num_constraints<Sos1ConstraintId>();
}

int64_t Model::next_sos1_constraint_id() const {
  return storage()->next_constraint_id<Sos1ConstraintId>().value();
}

bool Model::has_sos1_constraint(const int64_t id) const {
  return has_sos1_constraint(Sos1ConstraintId(id));
}

bool Model::has_sos1_constraint(const Sos1ConstraintId id) const {
  return storage()->has_constraint(id);
}

Sos1Constraint Model::sos1_constraint(const int64_t id) const {
  return sos1_constraint(Sos1ConstraintId(id));
}

Sos1Constraint Model::sos1_constraint(const Sos1ConstraintId id) const {
  CHECK(has_sos1_constraint(id))
      << "No SOS1 constraint with id: " << id.value();
  return Sos1Constraint(storage(), id);
}

std::vector<Sos1Constraint> Model::Sos1Constraints() const {
  return AtomicConstraints<Sos1Constraint>(*storage());
}

std::vector<Sos1Constraint> Model::SortedSos1Constraints() const {
  return SortedAtomicConstraints<Sos1Constraint>(*storage());
}

// --------------------------- SOS2 constraints --------------------------------

void Model::DeleteSos2Constraint(const Sos2Constraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteAtomicConstraint(constraint.typed_id());
}

int64_t Model::num_sos2_constraints() const {
  return storage()->num_constraints<Sos2ConstraintId>();
}

int64_t Model::next_sos2_constraint_id() const {
  return storage()->next_constraint_id<Sos2ConstraintId>().value();
}

bool Model::has_sos2_constraint(const int64_t id) const {
  return has_sos2_constraint(Sos2ConstraintId(id));
}

bool Model::has_sos2_constraint(const Sos2ConstraintId id) const {
  return storage()->has_constraint(id);
}

Sos2Constraint Model::sos2_constraint(const int64_t id) const {
  return sos2_constraint(Sos2ConstraintId(id));
}

Sos2Constraint Model::sos2_constraint(const Sos2ConstraintId id) const {
  CHECK(has_sos2_constraint(id))
      << "No SOS2 constraint with id: " << id.value();
  return Sos2Constraint(storage(), id);
}

std::vector<Sos2Constraint> Model::Sos2Constraints() const {
  return AtomicConstraints<Sos2Constraint>(*storage());
}

std::vector<Sos2Constraint> Model::SortedSos2Constraints() const {
  return SortedAtomicConstraints<Sos2Constraint>(*storage());
}

// --------------------------- Indicator constraints ---------------------------

void Model::DeleteIndicatorConstraint(const IndicatorConstraint constraint) {
  CheckModel(constraint.storage());
  storage()->DeleteAtomicConstraint(constraint.typed_id());
}

int64_t Model::num_indicator_constraints() const {
  return storage()->num_constraints<IndicatorConstraintId>();
}

int64_t Model::next_indicator_constraint_id() const {
  return storage()->next_constraint_id<IndicatorConstraintId>().value();
}

bool Model::has_indicator_constraint(const int64_t id) const {
  return has_indicator_constraint(IndicatorConstraintId(id));
}

bool Model::has_indicator_constraint(const IndicatorConstraintId id) const {
  return storage()->has_constraint(id);
}

IndicatorConstraint Model::indicator_constraint(const int64_t id) const {
  return indicator_constraint(IndicatorConstraintId(id));
}

IndicatorConstraint Model::indicator_constraint(
    const IndicatorConstraintId id) const {
  CHECK(has_indicator_constraint(id))
      << "No indicator constraint with id: " << id.value();
  return IndicatorConstraint(storage(), id);
}

std::vector<IndicatorConstraint> Model::IndicatorConstraints() const {
  return AtomicConstraints<IndicatorConstraint>(*storage());
}

std::vector<IndicatorConstraint> Model::SortedIndicatorConstraints() const {
  return SortedAtomicConstraints<IndicatorConstraint>(*storage());
}

// ------------------------------- Objective -----------------------------------

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

Objective Model::primary_objective() const {
  return Objective::Primary(storage());
}

double Model::objective_coefficient(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->linear_objective_coefficient(kPrimaryObjectiveId,
                                                 variable.typed_id());
}

double Model::objective_coefficient(const Variable first_variable,
                                    const Variable second_variable) const {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  return storage()->quadratic_objective_coefficient(kPrimaryObjectiveId,
                                                    first_variable.typed_id(),
                                                    second_variable.typed_id());
}

void Model::set_objective_coefficient(const Variable variable,
                                      const double value) {
  CheckModel(variable.storage());
  storage()->set_linear_objective_coefficient(kPrimaryObjectiveId,
                                              variable.typed_id(), value);
}

void Model::set_objective_coefficient(const Variable first_variable,
                                      const Variable second_variable,
                                      const double value) {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  storage()->set_quadratic_objective_coefficient(
      kPrimaryObjectiveId, first_variable.typed_id(),
      second_variable.typed_id(), value);
}

void Model::clear_objective() {
  storage()->clear_objective(kPrimaryObjectiveId);
}

bool Model::is_objective_coefficient_nonzero(const Variable variable) const {
  CheckModel(variable.storage());
  return storage()->is_linear_objective_coefficient_nonzero(
      kPrimaryObjectiveId, variable.typed_id());
}

bool Model::is_objective_coefficient_nonzero(
    const Variable first_variable, const Variable second_variable) const {
  CheckModel(first_variable.storage());
  CheckModel(second_variable.storage());
  return storage()->is_quadratic_objective_coefficient_nonzero(
      kPrimaryObjectiveId, first_variable.typed_id(),
      second_variable.typed_id());
}

double Model::objective_offset() const {
  return storage()->objective_offset(kPrimaryObjectiveId);
}

void Model::set_objective_offset(const double value) {
  storage()->set_objective_offset(kPrimaryObjectiveId, value);
}

bool Model::is_maximize() const {
  return storage()->is_maximize(kPrimaryObjectiveId);
}

void Model::set_maximize() { storage()->set_maximize(kPrimaryObjectiveId); }

void Model::set_minimize() { storage()->set_minimize(kPrimaryObjectiveId); }

void Model::set_is_maximize(const bool is_maximize) {
  storage()->set_is_maximize(kPrimaryObjectiveId, is_maximize);
}

// -------------------------- Auxiliary objectives -----------------------------

Objective Model::AddAuxiliaryObjective(const int64_t priority,
                                       const absl::string_view name) {
  return Objective::Auxiliary(storage(),
                              storage()->AddAuxiliaryObjective(priority, name));
}

Objective Model::AddAuxiliaryObjective(const LinearExpression& expression,
                                       const bool is_maximize,
                                       const int64_t priority,
                                       const absl::string_view name) {
  const Objective obj = AddAuxiliaryObjective(priority, name);
  SetObjective(obj, expression, is_maximize);
  return obj;
}

Objective Model::AddMaximizationObjective(const LinearExpression& expression,
                                          const int64_t priority,
                                          const absl::string_view name) {
  return AddAuxiliaryObjective(expression, /*is_maximize=*/true, priority,
                               name);
}

Objective Model::AddMinimizationObjective(const LinearExpression& expression,
                                          const int64_t priority,
                                          const absl::string_view name) {
  return AddAuxiliaryObjective(expression, /*is_maximize=*/false, priority,
                               name);
}

void Model::DeleteAuxiliaryObjective(const Objective objective) {
  CheckModel(objective.storage());
  CHECK(!objective.is_primary()) << "cannot delete primary objective";
  const AuxiliaryObjectiveId id = *objective.typed_id();
  CHECK(storage()->has_auxiliary_objective(id))
      << "cannot delete unrecognized auxiliary objective id: " << id;
  storage()->DeleteAuxiliaryObjective(id);
}

int64_t Model::num_auxiliary_objectives() const {
  return storage()->num_auxiliary_objectives();
}

int64_t Model::next_auxiliary_objective_id() const {
  return storage()->next_auxiliary_objective_id().value();
}

bool Model::has_auxiliary_objective(const int64_t id) const {
  return has_auxiliary_objective(AuxiliaryObjectiveId(id));
}

bool Model::has_auxiliary_objective(const AuxiliaryObjectiveId id) const {
  return storage()->has_auxiliary_objective(id);
}

Objective Model::auxiliary_objective(const int64_t id) const {
  return auxiliary_objective(AuxiliaryObjectiveId(id));
}

Objective Model::auxiliary_objective(const AuxiliaryObjectiveId id) const {
  CHECK(has_auxiliary_objective(id))
      << "unrecognized auxiliary objective id: " << id;
  return Objective::Auxiliary(storage(), id);
}

// ---------------------------- Multi-objective --------------------------------

void Model::Maximize(const Objective objective,
                     const LinearExpression& expression) {
  SetObjective(objective, expression, /*is_maximize=*/true);
}

void Model::Minimize(const Objective objective,
                     const LinearExpression& expression) {
  SetObjective(objective, expression, /*is_maximize=*/false);
}

void Model::set_objective_priority(const Objective objective,
                                   const int64_t priority) {
  CheckModel(objective.storage());
  storage()->set_objective_priority(objective.typed_id(), priority);
}

void Model::set_objective_coefficient(const Objective objective,
                                      const Variable variable,
                                      const double value) {
  CheckModel(objective.storage());
  CheckModel(variable.storage());
  storage()->set_linear_objective_coefficient(objective.typed_id(),
                                              variable.typed_id(), value);
}

void Model::set_objective_offset(const Objective objective,
                                 const double value) {
  CheckModel(objective.storage());
  storage()->set_objective_offset(objective.typed_id(), value);
}

void Model::set_maximize(const Objective objective) {
  set_is_maximize(objective, /*is_maximize=*/true);
}

void Model::set_minimize(const Objective objective) {
  set_is_maximize(objective, /*is_maximize=*/false);
}

void Model::set_is_maximize(const Objective objective, const bool is_maximize) {
  CheckModel(objective.storage());
  storage()->set_is_maximize(objective.typed_id(), is_maximize);
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
