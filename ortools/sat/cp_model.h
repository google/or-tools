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

// This file implements a wrapper around the CP-SAT model proto.
//
// Here is a minimal example that shows how to create a model, solve it, and
// print out the solution.
//
// CpModelBuilder cp_model;
// Domain all_animals(0, 20);
// IntVar rabbits = cp_model.NewIntVar(all_animals).WithName("rabbits");
// IntVar pheasants = cp_model.NewIntVar(all_animals).WithName("pheasants");
//
// cp_model.AddEquality(LinearExpr::Sum({rabbits, pheasants}), 20);
// cp_model.AddEquality(LinearExpr::ScalProd({rabbits, pheasants}, {4, 2}), 56);
//
// const CpSolverResponse response = Solve(cp_model);
// if (response.status() == CpSolverStatus::FEASIBLE) {
//   LOG(INFO) << SolutionIntegerValue(response, rabbits)
//             << " rabbits, and " << SolutionIntegerValue(response, pheasants)
//             << " pheasants.";
// }

#ifndef OR_TOOLS_SAT_CP_MODEL_H_
#define OR_TOOLS_SAT_CP_MODEL_H_

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

// A Boolean variable.
// This class wraps an IntegerVariableProto with domain [0, 1].
// It supports logical negation (Not).
//
// This can only be constructed via CpModelBuilder.NewBoolVar().
class BoolVar {
 public:
  BoolVar();

  // Sets the name of the variable.
  BoolVar WithName(const std::string& name);

  const std::string& Name() const { return Proto().name(); }

  // Returns the logical negation of the current Boolean variable.
  BoolVar Not() const { return BoolVar(NegatedRef(index_), cp_model_); }

  bool operator==(const BoolVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  bool operator!=(const BoolVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  std::string DebugString() const;

  // Useful for testing.
  const IntegerVariableProto& Proto() const {
    return cp_model_->variables(index_);
  }

  // Useful for model edition.
  IntegerVariableProto* MutableProto() const {
    return cp_model_->mutable_variables(index_);
  }

  // Returns the index of the variable in the model. If the variable is the
  // negation of another variable v, its index is -v.index() - 1.
  int index() const { return index_; }

 private:
  friend class CircuitConstraint;
  friend class Constraint;
  friend class CpModelBuilder;
  friend class IntVar;
  friend class IntervalVar;
  friend class LinearExpr;
  friend class ReservoirConstraint;
  friend bool SolutionBooleanValue(const CpSolverResponse& r, BoolVar x);

  BoolVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_;
  int index_;
};

std::ostream& operator<<(std::ostream& os, const BoolVar& var);

// A convenient wrapper so we can write Not(x) instead of x.Not() which is
// sometimes clearer.
BoolVar Not(BoolVar x);

// An integer variable.
// This class wraps an IntegerVariableProto.
// This can only be constructed via CpModelBuilder.NewIntVar().
//
// Note that a BoolVar can be used in any place that accept an
// IntVar via an implicit cast. It will simply take the value
// 0 (when false) or 1 (when true).
class IntVar {
 public:
  IntVar();

  // Implicit cast BoolVar -> IntVar.
  IntVar(const BoolVar& var);  // NOLINT(runtime/explicit)

  // Sets the name of the variable.
  IntVar WithName(const std::string& name);

  const std::string& Name() const { return Proto().name(); }

  bool operator==(const IntVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  bool operator!=(const IntVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  std::string DebugString() const;

  // Useful for testing.
  const IntegerVariableProto& Proto() const {
    return cp_model_->variables(index_);
  }

  // Useful for model edition.
  IntegerVariableProto* MutableProto() const {
    return cp_model_->mutable_variables(index_);
  }

  // Returns the index of the variable in the model.
  int index() const { return index_; }

 private:
  friend class CpModelBuilder;
  friend class CumulativeConstraint;
  friend class LinearExpr;
  friend class IntervalVar;
  friend class ReservoirConstraint;
  friend int64 SolutionIntegerValue(const CpSolverResponse& r,
                                    const LinearExpr& expr);
  friend int64 SolutionIntegerMin(const CpSolverResponse& r, IntVar x);
  friend int64 SolutionIntegerMax(const CpSolverResponse& r, IntVar x);

  IntVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_;
  int index_;
};

std::ostream& operator<<(std::ostream& os, const IntVar& var);

// A dedicated container for linear expressions.
//
// This class helps building and manipulating linear expressions.
// With the use of implicit constructors, it can accept integer values, Boolean
// and Integer variables. Note that Not(x) will be silently transformed into
// 1 - x when added to the linear expression.
//
// Furthermore, static methods allows sums and scalar products, with or without
// an additional constant.
//
// Usage:
//   CpModelBuilder cp_model;
//   IntVar x = model.NewIntVar(0, 10, "x");
//   IntVar y = model.NewIntVar(0, 10, "y");
//   BoolVar b = model.NewBoolVar().WithName("b");
//   BoolVar c = model.NewBoolVar().WithName("c");
//   LinearExpr e1(x);  // e1 = x.
//   LinearExpr e2 = LinearExpr::Sum({x, y}).AddConstant(5);  // e2 = x + y + 5;
//   LinearExpr e3 = LinearExpr::ScalProd({x, y}, {2, -1});  // e3 = 2 * x - y.
//   LinearExpr e4(b);  // e4 = b.
//   LinearExpr e5(b.Not());  // e5 = 1 - b.
//   // If passing a std::vector<BoolVar>, a specialized method must be called.
//   std::vector<BoolVar> bools = {b, Not(c)};
//   LinearExpr e6 = LinearExpr::BooleanSum(bools);  // e6 = b + 1 - c;
//   // e7 = -3 * b + 1 - c;
//   LinearExpr e7 = LinearExpr::BooleanScalProd(bools, {-3, 1});
//
// This can be used implicitly in some of the CpModelBuilder methods.
//   cp_model.AddGreaterThan(x, 5);  // x > 5
//   cp_model.AddEquality(x, LinearExpr(y).AddConstant(5));  // x == y + 5
class LinearExpr {
 public:
  LinearExpr();

  // Constructs a linear expression from a Boolean variable.
  // It deals with logical negation correctly.
  LinearExpr(BoolVar var);  // NOLINT(runtime/explicit)

  // Constructs a linear expression from an integer variable.
  LinearExpr(IntVar var);  // NOLINT(runtime/explicit)

  // Constructs a constant linear expression.
  LinearExpr(int64 constant);  // NOLINT(runtime/explicit)

  // Adds a constant value to the linear expression.
  LinearExpr& AddConstant(int64 value);

  // Adds a single integer variable to the linear expression.
  void AddVar(IntVar var);

  // Adds a term (var * coeff) to the linear expression.
  void AddTerm(IntVar var, int64 coeff);

  // Constructs the sum of a list of variables.
  static LinearExpr Sum(absl::Span<const IntVar> vars);

  // Constructs the scalar product of variables and coefficients.
  static LinearExpr ScalProd(absl::Span<const IntVar> vars,
                             absl::Span<const int64> coeffs);

  // Constructs the sum of a list of Booleans.
  static LinearExpr BooleanSum(absl::Span<const BoolVar> vars);

  // Constructs the scalar product of Booleans and coefficients.
  static LinearExpr BooleanScalProd(absl::Span<const BoolVar> vars,
                                    absl::Span<const int64> coeffs);

  // Useful for testing.
  const std::vector<IntVar>& variables() const { return variables_; }
  const std::vector<int64>& coefficients() const { return coefficients_; }
  int64 constant() const { return constant_; }

  // TODO(user): LinearExpr.DebugString() and operator<<.

 private:
  std::vector<IntVar> variables_;
  std::vector<int64> coefficients_;
  int64 constant_ = 0;
};

// Represents a Interval variable.
//
// An interval variable is both a constraint and a variable. It is defined by
// three integer variables: start, size, and end.
//
// It is a constraint because, internally, it enforces that start + size == end.
//
// It is also a variable as it can appear in specific scheduling constraints:
// NoOverlap, NoOverlap2D, Cumulative.
//
// Optionally, a presence literal can be added to this constraint. This presence
// literal is understood by the same constraints. These constraints ignore
// interval variables with precence literals assigned to false. Conversely,
// these constraints will also set these presence literals to false if they
// cannot fit these intervals into the schedule.
//
// It can only be constructed via CpModelBuilder.NewIntervalVar().
class IntervalVar {
 public:
  // Default ctor.
  IntervalVar();

  // Sets the name of the variable.
  IntervalVar WithName(const std::string& name);

  std::string Name() const;

  // Returns the start variable.
  IntVar StartVar() const;

  // Returns the size variable.
  IntVar SizeVar() const;

  // Returns the end variable.
  IntVar EndVar() const;

  // Returns a BoolVar indicating the presence of this interval.
  // It returns CpModelBuilder.TrueVar() if the interval is not optional.
  BoolVar PresenceBoolVar() const;

  bool operator==(const IntervalVar& other) const {
    return other.cp_model_ == cp_model_ && other.index_ == index_;
  }

  bool operator!=(const IntervalVar& other) const {
    return other.cp_model_ != cp_model_ || other.index_ != index_;
  }

  std::string DebugString() const;

  // Useful for testing.
  const IntervalConstraintProto& Proto() const {
    return cp_model_->constraints(index_).interval();
  }

  // Useful for model edition.
  IntervalConstraintProto* MutableProto() const {
    return cp_model_->mutable_constraints(index_)->mutable_interval();
  }

  // Returns the index of the interval constraint in the model.
  int index() const { return index_; }

 private:
  friend class CpModelBuilder;
  friend class CumulativeConstraint;
  friend class NoOverlap2DConstraint;
  friend std::ostream& operator<<(std::ostream& os, const IntervalVar& var);

  IntervalVar(int index, CpModelProto* cp_model);

  CpModelProto* cp_model_;
  int index_;
};

std::ostream& operator<<(std::ostream& os, const IntervalVar& var);

// A constraint.
//
// This class enable modifying the constraint that was added to the model.
//
// It can anly be built by the different CpModelBuilder::AddXXX methods.
class Constraint {
 public:
  // The constraint will be enforced iff all literals listed here are true. If
  // this is empty, then the constraint will always be enforced. An enforced
  // constraint must be satisfied, and an un-enforced one will simply be
  // ignored.
  //
  // This is also called half-reification. To have an equivalence between a
  // literal and a constraint (full reification), one must add both a constraint
  // (controlled by a literal l) and its negation (controlled by the negation of
  // l).
  //
  // Important: as of September 2018, only a few constraint support enforcement:
  // - bool_or, bool_and, linear: fully supported.
  // - interval: only support a single enforcement literal.
  // - other: no support (but can be added on a per-demand basis).
  Constraint OnlyEnforceIf(absl::Span<const BoolVar> literals);

  // See OnlyEnforceIf(absl::Span<const BoolVar> literals).
  Constraint OnlyEnforceIf(BoolVar literal);

  // Sets the name of the constraint.
  Constraint WithName(const std::string& name);

  const std::string& Name() const;

  // Useful for testing.
  const ConstraintProto& Proto() const { return *proto_; }

  // Useful for model edition.
  ConstraintProto* MutableProto() const { return proto_; }

 protected:
  friend class CpModelBuilder;

  explicit Constraint(ConstraintProto* proto);

  ConstraintProto* proto_;
};

// Specialized circuit constraint.
//
// This constraint allows adding arcs to the circuit constraint incrementally.
class CircuitConstraint : public Constraint {
 public:
  void AddArc(int tail, int head, BoolVar literal);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

// Specialized assignment constraint.
//
// This constraint allows adding tuples to the allowed/forbidden assignment
// constraint incrementally.
class TableConstraint : public Constraint {
 public:
  void AddTuple(absl::Span<const int64> tuple);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

// Specialized reservoir constraint.
//
// This constraint allows adding emptying/refilling events to the reservoir
// constraint incrementally.
class ReservoirConstraint : public Constraint {
 public:
  void AddEvent(IntVar time, int64 demand);
  void AddOptionalEvent(IntVar time, int64 demand, BoolVar is_active);

 private:
  friend class CpModelBuilder;

  ReservoirConstraint(ConstraintProto* proto, CpModelBuilder* builder);

  CpModelBuilder* builder_;
};

// Specialized automaton constraint.
//
// This constraint allows adding transitions to the automaton constraint
// incrementally.
class AutomatonConstraint : public Constraint {
 public:
  void AddTransition(int tail, int head, int64 transition_label);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

// Specialized no_overlap2D constraint.
//
// This constraint allows adding rectangles to the no_overlap2D
// constraint incrementally.
class NoOverlap2DConstraint : public Constraint {
 public:
  void AddRectangle(IntervalVar x_coordinate, IntervalVar y_coordinate);

 private:
  friend class CpModelBuilder;

  using Constraint::Constraint;
};

// Specialized cumulative constraint.
//
// This constraint allows adding fixed or variables demands to the cumulative
// constraint incrementally.
class CumulativeConstraint : public Constraint {
 public:
  void AddDemand(IntervalVar interval, IntVar demand);

 private:
  friend class CpModelBuilder;

  CumulativeConstraint(ConstraintProto* proto, CpModelBuilder* builder);

  CpModelBuilder* builder_;
};

// Wrapper class around the cp_model proto.
//
// This class provides two types of methods:
//  - NewXXX to create integer, boolean, or interval variables.
//  - AddXXX to create new constraints and add them to the model.
class CpModelBuilder {
 public:
  // Creates an integer variable with the given domain.
  IntVar NewIntVar(const Domain& domain);

  // Creates a Boolean variable.
  BoolVar NewBoolVar();

  // Creates a constant variable.
  IntVar NewConstant(int64 value);

  // Creates an always true Boolean variable.
  BoolVar TrueVar();

  // Creates an always false Boolean variable.
  BoolVar FalseVar();

  // Creates an interval variable.
  IntervalVar NewIntervalVar(IntVar start, IntVar size, IntVar end);

  // Creates an optional interval variable.
  IntervalVar NewOptionalIntervalVar(IntVar start, IntVar size, IntVar end,
                                     BoolVar presence);

  // Adds the constraint that at least one of the literals must be true.
  Constraint AddBoolOr(absl::Span<const BoolVar> literals);

  // Adds the constraint that all literals must be true.
  Constraint AddBoolAnd(absl::Span<const BoolVar> literals);

  /// Adds the constraint that a odd number of literal is true.
  Constraint AddBoolXor(absl::Span<const BoolVar> literals);

  // Adds a => b.
  Constraint AddImplication(BoolVar a, BoolVar b) {
    return AddBoolOr({a.Not(), b});
  }

  // Adds left == right.
  Constraint AddEquality(const LinearExpr& left, const LinearExpr& right);

  // Adds left >= right.
  Constraint AddGreaterOrEqual(const LinearExpr& left, const LinearExpr& right);

  // Adds left > right.
  Constraint AddGreaterThan(const LinearExpr& left, const LinearExpr& right);

  // Adds left <= right.
  Constraint AddLessOrEqual(const LinearExpr& left, const LinearExpr& right);

  // Adds left < right.
  Constraint AddLessThan(const LinearExpr& left, const LinearExpr& right);

  // Adds expr in domain.
  Constraint AddLinearConstraint(const LinearExpr& expr, const Domain& domain);

  // Adds left != right.
  Constraint AddNotEqual(const LinearExpr& left, const LinearExpr& right);

  // this constraint forces all variables to have different values.
  Constraint AddAllDifferent(absl::Span<const IntVar> vars);

  // Adds the element constraint: variables[index] == target
  Constraint AddVariableElement(IntVar index,
                                absl::Span<const IntVar> variables,
                                IntVar target);

  // Adds the element constraint: values[index] == target
  Constraint AddElement(IntVar index, absl::Span<const int64> values,
                        IntVar target);

  // Adds a circuit constraint.
  //
  // The circuit constraint is defined on a graph where the arc presence are
  // controlled by literals. That is the arc is part of the circuit of its
  // corresponding literal is assigned to true.
  //
  // For now, we ignore node indices with no incident arc. All the other nodes
  // must have exactly one incoming and one outgoing selected arc (i.e. literal
  // at true). All the selected arcs that are not self-loops must form a single
  // circuit.
  //
  // It returns a circuit constraint that allows adding arcs incrementally after
  // construction.
  CircuitConstraint AddCircuitConstraint();

  // Adds an allowed assignments constraint.
  //
  // An AllowedAssignments constraint is a constraint on an array of variables
  // that forces, when all variables are fixed to a single value, that the
  // corresponding list of values is equal to one of the tuple added to the
  // constraint.
  //
  // It returns a table constraint that allows adding tuples incrementally after
  // construction,
  TableConstraint AddAllowedAssignments(absl::Span<const IntVar> vars);

  // Adds an forbidden assignments constraint.
  //
  // A ForbiddenAssignments constraint is a constraint on an array of variables
  // where the list of impossible combinations is provided in the tuples added
  // to the constraint.
  //
  // It returns a table constraint that allows adding tuples incrementally after
  // construction,
  TableConstraint AddForbiddenAssignments(absl::Span<const IntVar> vars);

  // An inverse constraint enforces that if 'variables[i]' is assigned a value
  // 'j', then inverse_variables[j] is assigned a value 'i'. And vice versa.
  Constraint AddInverseConstraint(absl::Span<const IntVar> variables,
                                  absl::Span<const IntVar> inverse_variables);

  // Adds a reservoir constraint with optional refill/emptying events.
  //
  // Maintain a reservoir level within bounds. The water level starts at 0, and
  // at any time >= 0, it must be within min_level, and max_level. Furthermore,
  // this constraints expect all times variables to be >= 0. Given an event
  // (time, demand, active), if active is true, and if time is assigned a value
  // t, then the level of the reservoir changes by demand (which is constant) at
  // time t.
  //
  // Note that level_min can be > 0, or level_max can be < 0. It just forces
  // some demands to be executed at time 0 to make sure that we are within those
  // bounds with the executed demands. Therefore, at any time t >= 0:
  //     sum(demands[i] * actives[i] if times[i] <= t) in [min_level, max_level]
  //
  // It returns a ReservoirConstraint that allows adding optional and non
  // optional events incrementally after construction.
  ReservoirConstraint AddReservoirConstraint(int64 min_level, int64 max_level);

  // An automaton constraint takes a list of variables (of size n), an initial
  // state, a set of final states, and a set of transitions. A transition is a
  // triplet ('tail', 'head', 'label'), where 'tail' and 'head' are states,
  // and 'label' is the label of an arc from 'head' to 'tail',
  // corresponding to the value of one variable in the list of variables.
  //
  // This automaton will be unrolled into a flow with n + 1 phases. Each phase
  // contains the possible states of the automaton. The first state contains the
  // initial state. The last phase contains the final states.
  //
  // Between two consecutive phases i and i + 1, the automaton creates a set of
  // arcs. For each transition (tail, head, label), it will add an arc from
  // the state 'tail' of phase i and the state 'head' of phase i + 1. This arc
  // labeled by the value 'label' of the variables 'variables[i]'. That is,
  // this arc can only be selected if 'variables[i]' is assigned the value
  // 'label'. A feasible solution of this constraint is an assignment of
  // variables such that, starting from the initial state in phase 0, there is a
  // path labeled by the values of the variables that ends in one of the final
  // states in the final phase.
  //
  // It returns an AutomatonConstraint that allows adding transition
  // incrementally after construction.
  AutomatonConstraint AddAutomaton(
      absl::Span<const IntVar> transition_variables, int starting_state,
      absl::Span<const int> final_states);

  // Adds target == min(vars).
  Constraint AddMinEquality(IntVar target, absl::Span<const IntVar> vars);

  // Adds target == max(vars).
  Constraint AddMaxEquality(IntVar target, absl::Span<const IntVar> vars);

  // Adds target = num / denom (integer division rounded towards 0).
  Constraint AddDivisionEquality(IntVar target, IntVar numerator,
                                 IntVar denominator);

  // Adds target == abs(var).
  Constraint AddAbsEquality(IntVar target, IntVar var);

  // Adds target = var % mod.
  Constraint AddModuloEquality(IntVar target, IntVar var, IntVar mod);

  // Adds target == prod(vars).
  Constraint AddProductEquality(IntVar target, absl::Span<const IntVar> vars);

  // Adds a constraint than ensures that all present intervals do not overlap
  // in time.
  Constraint AddNoOverlap(absl::Span<const IntervalVar> vars);

  // The no_overlap_2d constraint prevents a set of boxes from overlapping.
  NoOverlap2DConstraint AddNoOverlap2D();

  // The cumulative constraint ensures that for any integer point, the sum of
  // the demands of the intervals containing that point does not exceed the
  // capacity.
  CumulativeConstraint AddCumulative(IntVar capacity);

  // Adds a linear minimization objective.
  void Minimize(const LinearExpr& expr);

  // Adds a linear maximization objective.
  void Maximize(const LinearExpr& expr);

  // Sets scaling of the objective. (must be called after Minimize() of
  // Maximize()). 'scaling' must be > 0.0.
  void ScaleObjectiveBy(double scaling);

  // Adds a decision strategy on a list of integer variables.
  void AddDecisionStrategy(
      absl::Span<const IntVar> variables,
      DecisionStrategyProto::VariableSelectionStrategy var_strategy,
      DecisionStrategyProto::DomainReductionStrategy domain_strategy);

  // Adds a decision strategy on a list of boolean variables.
  void AddDecisionStrategy(
      absl::Span<const BoolVar> variables,
      DecisionStrategyProto::VariableSelectionStrategy var_strategy,
      DecisionStrategyProto::DomainReductionStrategy domain_strategy);

  // TODO(user) : add MapDomain?

  const CpModelProto& Build() const { return Proto(); }

  const CpModelProto& Proto() const { return cp_model_; }
  CpModelProto* MutableProto() { return &cp_model_; }

 private:
  friend class CumulativeConstraint;
  friend class ReservoirConstraint;

  // Returns a (cached) integer variable index with a constant value.
  int IndexFromConstant(int64 value);

  // Returns a valid integer index from a BoolVar index.
  // If the input index is a positive, it returns this index.
  // If the input index is negative, it creates a cached IntVar equal to
  // 1 - BoolVar(PositiveRef(index)), and returns the index of this new
  // variable.
  int GetOrCreateIntegerIndex(int index);

  void FillLinearTerms(const LinearExpr& left, const LinearExpr& right,
                       LinearConstraintProto* proto);

  CpModelProto cp_model_;
  absl::flat_hash_map<int64, int> constant_to_index_map_;
  absl::flat_hash_map<int, int> bool_to_integer_index_map_;
};

// Solves the current cp_model and returns an instance of CpSolverResponse.
CpSolverResponse Solve(const CpModelProto& model_proto);

// Solves the current cp_model within the given model, and returns an
// instance of CpSolverResponse.
CpSolverResponse SolveWithModel(const CpModelProto& model_proto, Model* model);

// Solves the current cp_model with the give sat parameters, and returns an
// instance of CpSolverResponse.
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const SatParameters& params);

// Solves the current cp_model with the given sat parameters as std::string in
// JSon format, and returns an instance of CpSolverResponse.
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const std::string& params);

// Evaluates the value of an linear expression in a solver response.
int64 SolutionIntegerValue(const CpSolverResponse& r, const LinearExpr& expr);

// Returns the min of an integer variable in a solution.
int64 SolutionIntegerMin(const CpSolverResponse& r, IntVar x);

// Returns the max of an integer variable in a solution.
int64 SolutionIntegerMax(const CpSolverResponse& r, IntVar x);

// Returns the value of a Boolean literal (a Boolean variable or its negation)
// in a solver response.
bool SolutionBooleanValue(const CpSolverResponse& r, BoolVar x);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_H_
