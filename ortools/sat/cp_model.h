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

/**
 * \file
 * This file implements a wrapper around the CP-SAT model proto.
 *
 * Here is a minimal example that shows how to create a model, solve it, and
 * print out the solution.
 * \code
 CpModelBuilder cp_model;
 Domain all_animals(0, 20);
 IntVar rabbits = cp_model.NewIntVar(all_animals).WithName("rabbits");
 IntVar pheasants = cp_model.NewIntVar(all_animals).WithName("pheasants");

 cp_model.AddEquality(LinearExpr::Sum({rabbits, pheasants}), 20);
 cp_model.AddEquality(LinearExpr::ScalProd({rabbits, pheasants}, {4, 2}), 56);

 const CpSolverResponse response = Solve(cp_model.Build());
 if (response.status() == CpSolverStatus::FEASIBLE) {
   LOG(INFO) << SolutionIntegerValue(response, rabbits)
             << " rabbits, and " << SolutionIntegerValue(response, pheasants)
             << " pheasants.";
 }
 \endcode
 */

#ifndef OR_TOOLS_SAT_CP_MODEL_H_
#define OR_TOOLS_SAT_CP_MODEL_H_

#include <cstdint>
#include <limits>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

class CpModelBuilder;
class LinearExpr;
class IntVar;

/**
 *  A Boolean variable.
 *
 * This class wraps an IntegerVariableProto with domain [0, 1].
 * It supports the logical negation (Not).
 *
 * This can only be constructed via \c CpModelBuilder.NewBoolVar().
 */
class BoolVar {
 public:
  BoolVar();

  /// Sets the name of the variable.
  BoolVar WithName(const std::string& name);

  /// Returns the name of the variable.
  const std::string& Name() const { return Proto().name(); }

  /// Returns the logical negation of the current Boolean variable.
  BoolVar Not() const { return BoolVar(NegatedRef(index_), cp_model_); }

  /// Equality test with another boolvar.
  bool operator==(const BoolVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  /// Dis-Equality test.
  bool operator!=(const BoolVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  /// Debug string.
  std::string DebugString() const;

  /// Returns the underlying protobuf object (useful for testing).
  const IntegerVariableProto& Proto() const {
    return cp_model_->variables(index_);
  }

  /// Returns the mutable underlying protobuf object (useful for model edition).
  IntegerVariableProto* MutableProto() const {
    return cp_model_->mutable_variables(index_);
  }

  /**
   * Returns the index of the variable in the model.
   *
   * If the variable is the negation of another variable v, its index is
   * -v.index() - 1.
   */
  int index() const { return index_; }

 private:
  friend class CircuitConstraint;
  friend class Constraint;
  friend class CpModelBuilder;
  friend class IntVar;
  friend class IntervalVar;
  friend class MultipleCircuitConstraint;
  friend class LinearExpr;
  friend class ReservoirConstraint;
  friend bool SolutionBooleanValue(const CpSolverResponse& r, BoolVar x);

  BoolVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_ = nullptr;
  int index_ = std::numeric_limits<int32_t>::min();
};

std::ostream& operator<<(std::ostream& os, const BoolVar& var);

/**
 *  A convenient wrapper so we can write Not(x) instead of x.Not() which is
 * sometimes clearer.
 */
BoolVar Not(BoolVar x);

/**
 * An integer variable.
 *
 * This class wraps an IntegerVariableProto.
 * This can only be constructed via \c CpModelBuilder.NewIntVar().
 *
 * Note that a BoolVar can be used in any place that accept an
 * IntVar via an implicit cast. It will simply take the value
 * 0 (when false) or 1 (when true).
 */
class IntVar {
 public:
  IntVar();

  /// Implicit cast BoolVar -> IntVar.
  IntVar(const BoolVar& var);  // NOLINT(runtime/explicit)

  /// Cast  IntVar -> BoolVar.
  /// Checks that the domain of the var is within {0,1}.
  BoolVar ToBoolVar() const;

  /// Sets the name of the variable.
  IntVar WithName(const std::string& name);

  /// Returns the name of the variable (or the empty string if not set).
  const std::string& Name() const { return Proto().name(); }

  /// Adds a constant value to an integer variable and returns a linear
  /// expression.
  LinearExpr AddConstant(int64_t value) const;

  /// Equality test with another IntVar.
  bool operator==(const IntVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  /// Difference test with anpther IntVar.
  bool operator!=(const IntVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  /// Returns a debug string.
  std::string DebugString() const;

  /// Returns the underlying protobuf object (useful for testing).
  const IntegerVariableProto& Proto() const {
    return cp_model_->variables(index_);
  }

  /// Returns the mutable underlying protobuf object (useful for model edition).
  IntegerVariableProto* MutableProto() const {
    return cp_model_->mutable_variables(index_);
  }

  /// Returns the index of the variable in the model.
  int index() const { return index_; }

 private:
  friend class BoolVar;
  friend class CpModelBuilder;
  friend class CumulativeConstraint;
  friend class LinearExpr;
  friend class IntervalVar;
  friend class ReservoirConstraint;
  friend int64_t SolutionIntegerValue(const CpSolverResponse& r,
                                      const LinearExpr& expr);
  friend int64_t SolutionIntegerMin(const CpSolverResponse& r, IntVar x);
  friend int64_t SolutionIntegerMax(const CpSolverResponse& r, IntVar x);

  IntVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_ = nullptr;
  int index_ = std::numeric_limits<int32_t>::min();
};

std::ostream& operator<<(std::ostream& os, const IntVar& var);

/**
 * A dedicated container for linear expressions.
 *
 * This class helps building and manipulating linear expressions.
 * With the use of implicit constructors, it can accept integer values, Boolean
 * and Integer variables. Note that Not(x) will be silently transformed into
 * 1 - x when added to the linear expression.
 *
 * Furthermore, static methods allows sums and scalar products, with or without
 * an additional constant.
 *
 * Usage:
 * \code
  CpModelBuilder cp_model;
  IntVar x = model.NewIntVar(0, 10, "x");
  IntVar y = model.NewIntVar(0, 10, "y");
  BoolVar b = model.NewBoolVar().WithName("b");
  BoolVar c = model.NewBoolVar().WithName("c");
  LinearExpr e1(x);  // e1 = x.
  LinearExpr e2 = LinearExpr::Sum({x, y}).AddConstant(5);  // e2 = x + y + 5;
  LinearExpr e3 = LinearExpr::ScalProd({x, y}, {2, -1});  // e3 = 2 * x - y.
  LinearExpr e4(b);  // e4 = b.
  LinearExpr e5(b.Not());  // e5 = 1 - b.
  // If passing a std::vector<BoolVar>, a specialized method must be called.
  std::vector<BoolVar> bools = {b, Not(c)};
  LinearExpr e6 = LinearExpr::BooleanSum(bools);  // e6 = b + 1 - c;
  // e7 = -3 * b + 1 - c;
  LinearExpr e7 = LinearExpr::BooleanScalProd(bools, {-3, 1});
  \endcode
 *  This can be used implicitly in some of the CpModelBuilder methods.
 * \code
  cp_model.AddGreaterThan(x, 5);  // x > 5
  cp_model.AddEquality(x, LinearExpr(y).AddConstant(5));  // x == y + 5
  \endcode
  */
class LinearExpr {
 public:
  LinearExpr();

  /**
   * Constructs a linear expression from a Boolean variable.
   *
   *  It deals with logical negation correctly.
   */
  LinearExpr(BoolVar var);  // NOLINT(runtime/explicit)

  /// Constructs a linear expression from an integer variable.
  LinearExpr(IntVar var);  // NOLINT(runtime/explicit)

  /// Constructs a constant linear expression.
  LinearExpr(int64_t constant);  // NOLINT(runtime/explicit)

  /// Adds a constant value to the linear expression.
  LinearExpr& AddConstant(int64_t value);

  /// Adds a single integer variable to the linear expression.
  LinearExpr& AddVar(IntVar var);

  /// Adds a term (var * coeff) to the linear expression.
  LinearExpr& AddTerm(IntVar var, int64_t coeff);

  /// Adds another linear expression to the linear expression.
  LinearExpr& AddExpression(const LinearExpr& expr);

  /// Constructs the sum of a list of variables.
  static LinearExpr Sum(absl::Span<const IntVar> vars);

  /// Constructs the scalar product of variables and coefficients.
  static LinearExpr ScalProd(absl::Span<const IntVar> vars,
                             absl::Span<const int64_t> coeffs);

  /// Constructs the sum of a list of Booleans.
  static LinearExpr BooleanSum(absl::Span<const BoolVar> vars);

  /// Constructs the scalar product of Booleans and coefficients.
  static LinearExpr BooleanScalProd(absl::Span<const BoolVar> vars,
                                    absl::Span<const int64_t> coeffs);
  /// Constructs var * coefficient.
  static LinearExpr Term(IntVar var, int64_t coefficient);

  /// Returns the vector of variables.
  const std::vector<IntVar>& variables() const { return variables_; }

  /// Returns the vector of coefficients.
  const std::vector<int64_t>& coefficients() const { return coefficients_; }

  /// Returns the constant term.
  int64_t constant() const { return constant_; }

  /// Checks that the expression is 1 * var + 0, and returns var.
  IntVar Var() const;

  /// Checks that the expression is constant and returns its value.
  int64_t Value() const;

  /// Debug string.
  std::string DebugString() const;

 private:
  std::vector<IntVar> variables_;
  std::vector<int64_t> coefficients_;
  int64_t constant_ = 0;
};

std::ostream& operator<<(std::ostream& os, const LinearExpr& e);

/**
 * Represents a Interval variable.
 *
 * An interval variable is both a constraint and a variable. It is defined by
 * three objects: start, size, and end. All three can be an integer variable, a
 * constant, or an affine expression.
 *
 * It is a constraint because, internally, it enforces that start + size == end.
 *
 * It is also a variable as it can appear in specific scheduling constraints:
 * NoOverlap, NoOverlap2D, Cumulative.
 *
 * Optionally, a presence literal can be added to this constraint. This presence
 * literal is understood by the same constraints. These constraints ignore
 * interval variables with precence literals assigned to false. Conversely,
 * these constraints will also set these presence literals to false if they
 * cannot fit these intervals into the schedule.
 *
 * It can only be constructed via \c CpModelBuilder.NewIntervalVar().
 */
class IntervalVar {
 public:
  /// Default ctor.
  IntervalVar();

  /// Sets the name of the variable.
  IntervalVar WithName(const std::string& name);

  /// Returns the name of the interval (or the empty string if not set).
  std::string Name() const;

  /// Returns the start linear expression. Note that this rebuilds the
  /// expression each time this method is called.
  LinearExpr StartExpr() const;

  /// Returns the size linear expression. Note that this rebuilds the
  /// expression each time this method is called.
  LinearExpr SizeExpr() const;

  /// Returns the end linear expression. Note that this rebuilds the
  /// expression each time this method is called.
  LinearExpr EndExpr() const;

  /**
   * Returns a BoolVar indicating the presence of this interval.
   *
   * It returns \c CpModelBuilder.TrueVar() if the interval is not optional.
   */
  BoolVar PresenceBoolVar() const;

  /// Equality test with another interval variable.
  bool operator==(const IntervalVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  /// Difference test with another interval variable.
  bool operator!=(const IntervalVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  /// Returns a debug string.
  std::string DebugString() const;

  /// Returns the underlying protobuf object (useful for testing).
  const IntervalConstraintProto& Proto() const {
    return cp_model_->constraints(index_).interval();
  }

  /// Returns the mutable underlying protobuf object (useful for model edition).
  IntervalConstraintProto* MutableProto() const {
    return cp_model_->mutable_constraints(index_)->mutable_interval();
  }

  /// Returns the index of the interval constraint in the model.
  int index() const { return index_; }

 private:
  friend class CpModelBuilder;
  friend class CumulativeConstraint;
  friend class NoOverlap2DConstraint;
  friend std::ostream& operator<<(std::ostream& os, const IntervalVar& var);

  IntervalVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_ = nullptr;
  int index_ = std::numeric_limits<int32_t>::min();
};

std::ostream& operator<<(std::ostream& os, const IntervalVar& var);

/**
 * A constraint.
 *
 * This class enables you to modify the constraint that was previously added to
 * the model.
 *
 * The constraint must be built using the different \c CpModelBuilder::AddXXX
 * methods.
 */
class Constraint {
 public:
  /**
   * The constraint will be enforced iff all literals listed here are true.
   *
   * If this is empty, then the constraint will always be enforced. An enforced
   * constraint must be satisfied, and an un-enforced one will simply be
   * ignored.
   *
   * This is also called half-reification. To have an equivalence between a
   * literal and a constraint (full reification), one must add both a constraint
   * (controlled by a literal l) and its negation (controlled by the negation of
   * l).
   *
   * Important: as of September 2018, only a few constraint support enforcement:
   * - bool_or, bool_and, linear: fully supported.
   * - interval: only support a single enforcement literal.
   * - other: no support (but can be added on a per-demand basis).
   */
  Constraint OnlyEnforceIf(absl::Span<const BoolVar> literals);

  /// See OnlyEnforceIf(absl::Span<const BoolVar> literals).
  Constraint OnlyEnforceIf(BoolVar literal);

  /// Sets the name of the constraint.
  Constraint WithName(const std::string& name);

  /// Returns the name of the constraint (or the empty string if not set).
  const std::string& Name() const;

  /// Returns the underlying protobuf object (useful for testing).
  const ConstraintProto& Proto() const { return *proto_; }

  /// Returns the mutable underlying protobuf object (useful for model edition).
  ConstraintProto* MutableProto() const { return proto_; }

 protected:
  friend class CpModelBuilder;

  explicit Constraint(ConstraintProto* proto);

  ConstraintProto* proto_ = nullptr;
};

/**
 * Specialized circuit constraint.
 *
 * This constraint allows adding arcs to the circuit constraint incrementally.
 */
class CircuitConstraint : public Constraint {
 public:
  /**
   * Add an arc to the circuit.
   *
   * @param tail the index of the tail node.
   * @param head the index of the head node.
   * @param literal it will be set to true if the arc is selected.
   */
  void AddArc(int tail, int head, BoolVar literal);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

/**
 * Specialized circuit constraint.
 *
 * This constraint allows adding arcs to the multiple circuit constraint
 * incrementally.
 */
class MultipleCircuitConstraint : public Constraint {
 public:
  /**
   * Add an arc to the circuit.
   *
   * @param tail the index of the tail node.
   * @param head the index of the head node.
   * @param literal it will be set to true if the arc is selected.
   */
  void AddArc(int tail, int head, BoolVar literal);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

/**
 * Specialized assignment constraint.
 *
 * This constraint allows adding tuples to the allowed/forbidden assignment
 * constraint incrementally.
 */
class TableConstraint : public Constraint {
 public:
  /// Adds a tuple of possible values to the constraint.
  void AddTuple(absl::Span<const int64_t> tuple);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

/**
 * Specialized reservoir constraint.
 *
 * This constraint allows adding emptying/refilling events to the reservoir
 * constraint incrementally.
 */
class ReservoirConstraint : public Constraint {
 public:
  /**
   * Adds a mandatory event
   *
   * It will increase the used capacity by `c demand at time `c time.
   */
  void AddEvent(IntVar time, int64_t demand);

  /**
   * Adds a optional event
   *
   * If `c is_active is true, It will increase the used capacity by `c demand at
   * time `c time.
   */
  void AddOptionalEvent(IntVar time, int64_t demand, BoolVar is_active);

 private:
  friend class CpModelBuilder;

  ReservoirConstraint(ConstraintProto* proto, CpModelBuilder* builder);

  CpModelBuilder* builder_;
};

/**
 * Specialized automaton constraint.
 *
 * This constraint allows adding transitions to the automaton constraint
 * incrementally.
 */
class AutomatonConstraint : public Constraint {
 public:
  /// Adds a transitions to the automaton.
  void AddTransition(int tail, int head, int64_t transition_label);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

/**
 * Specialized no_overlap2D constraint.
 *
 * This constraint allows adding rectangles to the no_overlap2D
 * constraint incrementally.
 */
class NoOverlap2DConstraint : public Constraint {
 public:
  /// Adds a rectangle (parallel to the axis) to the constraint.
  void AddRectangle(IntervalVar x_coordinate, IntervalVar y_coordinate);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

/**
 * Specialized cumulative constraint.
 *
 * This constraint allows adding fixed or variables demands to the cumulative
 * constraint incrementally.
 */
class CumulativeConstraint : public Constraint {
 public:
  /// Adds a pair (interval, demand) to the constraint.
  void AddDemand(IntervalVar interval, IntVar demand);

 private:
  friend class CpModelBuilder;

  CumulativeConstraint(ConstraintProto* proto, CpModelBuilder* builder);

  CpModelBuilder* builder_;
};

/**
 * Wrapper class around the cp_model proto.
 *
 * This class provides two types of methods:
 *  - NewXXX to create integer, boolean, or interval variables.
 *  - AddXXX to create new constraints and add them to the model.
 */
class CpModelBuilder {
 public:
  /// Creates an integer variable with the given domain.
  IntVar NewIntVar(const Domain& domain);

  /// Creates a Boolean variable.
  BoolVar NewBoolVar();

  /// Creates a constant variable.
  IntVar NewConstant(int64_t value);

  /// Creates an always true Boolean variable.
  /// If this is called multiple time, always the same variable will be
  /// returned.
  BoolVar TrueVar();

  /// Creates an always false Boolean variable.
  /// If this is called multiple time, always the same variable will be
  /// returned.
  BoolVar FalseVar();

  /// Creates an interval variable from 3 affine expressions.
  IntervalVar NewIntervalVar(const LinearExpr& start, const LinearExpr& size,
                             const LinearExpr& end);

  /// Creates an interval variable with a fixed size.
  IntervalVar NewFixedSizeIntervalVar(const LinearExpr& start, int64_t size);

  /// Creates an optional interval variable from 3 affine expressions and a
  /// Boolean variable.
  IntervalVar NewOptionalIntervalVar(const LinearExpr& start,
                                     const LinearExpr& size,
                                     const LinearExpr& end, BoolVar presence);

  /// Creates an optional interval variable with a fixed size.
  IntervalVar NewOptionalFixedSizeIntervalVar(const LinearExpr& start,
                                              int64_t size, BoolVar presence);

  /// Adds the constraint that at least one of the literals must be true.
  Constraint AddBoolOr(absl::Span<const BoolVar> literals);

  /// Adds the constraint that all literals must be true.
  Constraint AddBoolAnd(absl::Span<const BoolVar> literals);

  /// Adds the constraint that a odd number of literal is true.
  Constraint AddBoolXor(absl::Span<const BoolVar> literals);

  /// Adds a => b.
  Constraint AddImplication(BoolVar a, BoolVar b) {
    return AddBoolOr({a.Not(), b});
  }

  /// Adds left == right.
  Constraint AddEquality(const LinearExpr& left, const LinearExpr& right);

  /// Adds left >= right.
  Constraint AddGreaterOrEqual(const LinearExpr& left, const LinearExpr& right);

  /// Adds left > right.
  Constraint AddGreaterThan(const LinearExpr& left, const LinearExpr& right);

  /// Adds left <= right.
  Constraint AddLessOrEqual(const LinearExpr& left, const LinearExpr& right);

  /// Adds left < right.
  Constraint AddLessThan(const LinearExpr& left, const LinearExpr& right);

  /// Adds expr in domain.
  Constraint AddLinearConstraint(const LinearExpr& expr, const Domain& domain);

  /// Adds left != right.
  Constraint AddNotEqual(const LinearExpr& left, const LinearExpr& right);

  /// this constraint forces all variables to have different values.
  Constraint AddAllDifferent(absl::Span<const IntVar> vars);

  /// Adds the element constraint: variables[index] == target
  Constraint AddVariableElement(IntVar index,
                                absl::Span<const IntVar> variables,
                                IntVar target);

  /// Adds the element constraint: values[index] == target
  Constraint AddElement(IntVar index, absl::Span<const int64_t> values,
                        IntVar target);

  /**
   * Adds a circuit constraint.
   *
   * The circuit constraint is defined on a graph where the arc presence is
   * controlled by literals. That is the arc is part of the circuit of its
   * corresponding literal is assigned to true.
   *
   * For now, we ignore node indices with no incident arc. All the other nodes
   * must have exactly one incoming and one outgoing selected arc (i.e. literal
   * at true). All the selected arcs that are not self-loops must form a single
   * circuit.
   *
   * It returns a circuit constraint that allows adding arcs incrementally after
   * construction.
   *
   */
  CircuitConstraint AddCircuitConstraint();

  /**
   * Adds a multiple circuit constraint, aka the "VRP" (Vehicle Routing Problem)
   * constraint.
   *
   * The direct graph where arc #i (from tails[i] to head[i]) is present iff
   * literals[i] is true must satisfy this set of properties:
   * - #incoming arcs == 1 except for node 0.
   * - #outgoing arcs == 1 except for node 0.
   * - for node zero, #incoming arcs == #outgoing arcs.
   * - There are no duplicate arcs.
   * - Self-arcs are allowed except for node 0.
   * - There is no cycle in this graph, except through node 0.
   */
  MultipleCircuitConstraint AddMultipleCircuitConstraint();

  /**
   * Adds an allowed assignments constraint.
   *
   * An AllowedAssignments constraint is a constraint on an array of variables
   * that forces, when all variables are fixed to a single value, that the
   * corresponding list of values is equal to one of the tuple added to the
   * constraint.
   *
   * It returns a table constraint that allows adding tuples incrementally after
   * construction,
   */
  TableConstraint AddAllowedAssignments(absl::Span<const IntVar> vars);

  /**
   * Adds an forbidden assignments constraint.
   *
   * A ForbiddenAssignments constraint is a constraint on an array of variables
   * where the list of impossible combinations is provided in the tuples added
   * to the constraint.
   *
   * It returns a table constraint that allows adding tuples incrementally after
   * construction,
   */
  TableConstraint AddForbiddenAssignments(absl::Span<const IntVar> vars);

  /** An inverse constraint
   *
   * It enforces that if 'variables[i]' is assigned a value
   * 'j', then inverse_variables[j] is assigned a value 'i'. And vice versa.
   */
  Constraint AddInverseConstraint(absl::Span<const IntVar> variables,
                                  absl::Span<const IntVar> inverse_variables);

  /**
   * Adds a reservoir constraint with optional refill/emptying events.
   *
   * Maintain a reservoir level within bounds. The water level starts at 0, and
   * at any time >= 0, it must be within min_level, and max_level. Furthermore,
   * this constraints expect all times variables to be >= 0. Given an event
   * (time, demand, active), if active is true, and if time is assigned a value
   * t, then the level of the reservoir changes by demand (which is constant) at
   * time t.
   *
   * Note that level_min can be > 0, or level_max can be < 0. It just forces
   * some demands to be executed at time 0 to make sure that we are within those
   * bounds with the executed demands. Therefore, at any time t >= 0:
   *     sum(demands[i] * actives[i] if times[i] <= t) in [min_level, max_level]
   *
   * It returns a ReservoirConstraint that allows adding optional and non
   * optional events incrementally after construction.
   */
  ReservoirConstraint AddReservoirConstraint(int64_t min_level,
                                             int64_t max_level);

  /**
   *
   * An automaton constraint/
   *
   * An automaton constraint takes a list of variables (of size n), an initial
   * state, a set of final states, and a set of transitions. A transition is a
   * triplet ('tail', 'head', 'label'), where 'tail' and 'head' are states,
   * and 'label' is the label of an arc from 'head' to 'tail',
   * corresponding to the value of one variable in the list of variables.
   *
   * This automaton will be unrolled into a flow with n + 1 phases. Each phase
   * contains the possible states of the automaton. The first state contains the
   * initial state. The last phase contains the final states.
   *
   * Between two consecutive phases i and i + 1, the automaton creates a set of
   * arcs. For each transition (tail, head, label), it will add an arc from
   * the state 'tail' of phase i and the state 'head' of phase i + 1. This arc
   * labeled by the value 'label' of the variables 'variables[i]'. That is,
   * this arc can only be selected if 'variables[i]' is assigned the value
   * 'label'. A feasible solution of this constraint is an assignment of
   * variables such that, starting from the initial state in phase 0, there is a
   * path labeled by the values of the variables that ends in one of the final
   * states in the final phase.
   *
   * It returns an AutomatonConstraint that allows adding transition
   * incrementally after construction.
   */
  AutomatonConstraint AddAutomaton(
      absl::Span<const IntVar> transition_variables, int starting_state,
      absl::Span<const int> final_states);

  /// Adds target == min(vars).
  Constraint AddMinEquality(IntVar target, absl::Span<const IntVar> vars);

  /// Adds target == min(exprs).
  Constraint AddLinMinEquality(const LinearExpr& target,
                               absl::Span<const LinearExpr> exprs);

  /// Adds target == max(vars).
  Constraint AddMaxEquality(IntVar target, absl::Span<const IntVar> vars);

  /// Adds target == max(exprs).
  Constraint AddLinMaxEquality(const LinearExpr& target,
                               absl::Span<const LinearExpr> exprs);

  /// Adds target = num / denom (integer division rounded towards 0).
  Constraint AddDivisionEquality(IntVar target, IntVar numerator,
                                 IntVar denominator);

  /// Adds target == abs(var).
  Constraint AddAbsEquality(IntVar target, IntVar var);

  /// Adds target = var % mod.
  Constraint AddModuloEquality(IntVar target, IntVar var, IntVar mod);

  /// Adds target == prod(vars).
  Constraint AddProductEquality(IntVar target, absl::Span<const IntVar> vars);

  /**
   *  Adds a no-overlap constraint that ensures that all present intervals do
   * not overlap in time.
   */
  Constraint AddNoOverlap(absl::Span<const IntervalVar> vars);

  /**
   * The no_overlap_2d constraint prevents a set of boxes from overlapping.
   */
  NoOverlap2DConstraint AddNoOverlap2D();

  /** The cumulative constraint
   *
   * It ensures that for any integer point, the sum of the demands of the
   * intervals containing that point does not exceed the capacity.
   */
  CumulativeConstraint AddCumulative(IntVar capacity);

  /// Adds a linear minimization objective.
  void Minimize(const LinearExpr& expr);

  /// Adds a linear maximization objective.
  void Maximize(const LinearExpr& expr);

  /**
   * Sets scaling of the objective.
   *
   * It must be called after \c Minimize() or \c Maximize()).
   *
   * \c scaling must be > 0.0.
   */
  void ScaleObjectiveBy(double scaling);

  /// Adds a decision strategy on a list of integer variables.
  void AddDecisionStrategy(
      absl::Span<const IntVar> variables,
      DecisionStrategyProto::VariableSelectionStrategy var_strategy,
      DecisionStrategyProto::DomainReductionStrategy domain_strategy);

  /// Adds a decision strategy on a list of boolean variables.
  void AddDecisionStrategy(
      absl::Span<const BoolVar> variables,
      DecisionStrategyProto::VariableSelectionStrategy var_strategy,
      DecisionStrategyProto::DomainReductionStrategy domain_strategy);

  /// Adds hinting to a variable.
  void AddHint(IntVar var, int64_t value);

  /// Remove all hints.
  void ClearHints();

  /// Adds a literal to the model as assumptions.
  void AddAssumption(BoolVar lit);

  /// Adds multiple literals to the model as assumptions.
  void AddAssumptions(absl::Span<const BoolVar> literals);

  /// Remove all assumptions from the model.
  void ClearAssumptions();

  // TODO(user) : add MapDomain?

  const CpModelProto& Build() const { return Proto(); }

  const CpModelProto& Proto() const { return cp_model_; }
  CpModelProto* MutableProto() { return &cp_model_; }

  /// Replace the current model with the one from the given proto.
  void CopyFrom(const CpModelProto& model_proto);

  /// Returns the Boolean variable from its index in the proto.
  BoolVar GetBoolVarFromProtoIndex(int index);

  /// Returns the integer variable from its index in the proto.
  IntVar GetIntVarFromProtoIndex(int index);

  /// Returns the interval variable from its index in the proto.
  IntervalVar GetIntervalVarFromProtoIndex(int index);

 private:
  friend class CumulativeConstraint;
  friend class ReservoirConstraint;
  friend class IntervalVar;

  // Fills the 'expr_proto' with the linear expression represented by 'expr'.
  void LinearExprToProto(const LinearExpr& expr,
                         LinearExpressionProto* expr_proto);

  // Rebuilds a LinearExpr from a LinearExpressionProto.
  // This method is a member of CpModelBuilder because it needs to be friend
  // with IntVar.
  static LinearExpr LinearExprFromProto(const LinearExpressionProto& expr_proto,
                                        CpModelProto* model_proto_);

  // Returns a (cached) integer variable index with a constant value.
  int IndexFromConstant(int64_t value);

  // Returns a valid integer index from a BoolVar index.
  // If the input index is a positive, it returns this index.
  // If the input index is negative, it creates a cached IntVar equal to
  // 1 - BoolVar(PositiveRef(index)), and returns the index of this new
  // variable.
  int GetOrCreateIntegerIndex(int index);

  void FillLinearTerms(const LinearExpr& left, const LinearExpr& right,
                       LinearConstraintProto* proto);

  CpModelProto cp_model_;
  absl::flat_hash_map<int64_t, int> constant_to_index_map_;
  absl::flat_hash_map<int, int> bool_to_integer_index_map_;
};

/// Evaluates the value of an linear expression in a solver response.
int64_t SolutionIntegerValue(const CpSolverResponse& r, const LinearExpr& expr);

/// Returns the min of an integer variable in a solution.
int64_t SolutionIntegerMin(const CpSolverResponse& r, IntVar x);

/// Returns the max of an integer variable in a solution.
int64_t SolutionIntegerMax(const CpSolverResponse& r, IntVar x);

/// Evaluates the value of a Boolean literal in a solver response.
bool SolutionBooleanValue(const CpSolverResponse& r, BoolVar x);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_H_
