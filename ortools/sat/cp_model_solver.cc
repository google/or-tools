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

#include "ortools/sat/cp_model_solver.h"

#include <functional>

#include "ortools/base/timer.h"
#include "ortools/base/join.h"
#include "ortools/base/join.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/connectivity.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/table.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

namespace {

// =============================================================================
// Helper classes.
// =============================================================================

// List all the CpModelProto references used.
struct VariableUsage {
  const std::vector<int> integers;
  const std::vector<int> intervals;
  const std::vector<int> booleans;
};

VariableUsage ComputeVariableUsage(const CpModelProto& model_proto) {
  // Since an interval is a constraint by itself, this will just list all
  // the interval constraint in order.
  std::vector<int> used_intervals;

  // TODO(user): use std::vector<bool> instead of unordered_set<int> + sort if
  // efficiency become an issue. Note that we need these to be sorted.
  IndexReferences references;
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kInterval) {
      used_intervals.push_back(c);
    }
    if (HasEnforcementLiteral(ct)) {
      references.literals.insert(ct.enforcement_literal(0));
    }
    AddReferencesUsedByConstraint(ct, &references);
  }

  // Add the objectives and search heuristics variables that needs to be
  // referenceable as integer even if they are only used as Booleans.
  if (model_proto.has_objective()) {
    for (const int obj_var : model_proto.objective().vars()) {
      references.variables.insert(obj_var);
    }
  }
  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    for (const int var : strategy.variables()) {
      references.variables.insert(var);
    }
  }

  // Make sure a Booleans is created for each [0,1] Boolean variables.
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (model_proto.variables(i).domain_size() != 2) continue;
    if (model_proto.variables(i).domain(0) != 0) continue;
    if (model_proto.variables(i).domain(1) != 1) continue;
    references.literals.insert(i);
  }

  std::vector<int> used_integers;
  for (const int var : references.variables) {
    used_integers.push_back(PositiveRef(var));
  }
  STLSortAndRemoveDuplicates(&used_integers);

  std::vector<int> used_booleans;
  for (const int lit : references.literals) {
    used_booleans.push_back(PositiveRef(lit));
  }
  STLSortAndRemoveDuplicates(&used_booleans);

  return VariableUsage{used_integers, used_intervals, used_booleans};
}

// Holds the sat::model and the mapping between the proto indices and the
// sat::model ones.
class ModelWithMapping {
 public:
  ModelWithMapping(const CpModelProto& model_proto, const VariableUsage& usage,
                   Model* model);

  // Shortcuts for the underlying model_ functions.
  template <typename T>
  T Add(std::function<T(Model*)> f) {
    return f(model_);
  }
  template <typename T>
  T Get(std::function<T(const Model&)> f) const {
    return f(*model_);
  }
  template <typename T>
  T* GetOrCreate() {
    return model_->GetOrCreate<T>();
  }
  template <typename T>
  void TakeOwnership(T* t) {
    return model_->TakeOwnership<T>(t);
  }

  bool IsInteger(int i) const {
    CHECK_LT(PositiveRef(i), integers_.size());
    return integers_[PositiveRef(i)] != kNoIntegerVariable;
  }

  // TODO(user): This does not returns true for [0,1] Integer variable that
  // never appear as a literal elsewhere. This is not ideal because in
  // LoadLinearConstraint() we probably still want to create the associated
  // Boolean and maybe not even create the [0,1] integer variable if it is not
  // used.
  bool IsBoolean(int i) const {
    CHECK_LT(PositiveRef(i), booleans_.size());
    return booleans_[PositiveRef(i)] != kNoBooleanVariable;
  }

  IntegerVariable Integer(int i) const {
    CHECK_LT(PositiveRef(i), integers_.size());
    const IntegerVariable var = integers_[PositiveRef(i)];
    CHECK_NE(var, kNoIntegerVariable);
    return RefIsPositive(i) ? var : NegationOf(var);
  }

  BooleanVariable Boolean(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, booleans_.size());
    CHECK_NE(booleans_[i], kNoBooleanVariable);
    return booleans_[i];
  }

  IntervalVariable Interval(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, intervals_.size());
    CHECK_NE(intervals_[i], kNoIntervalVariable);
    return intervals_[i];
  }

  sat::Literal Literal(int i) const {
    CHECK_LT(PositiveRef(i), booleans_.size());
    return sat::Literal(booleans_[PositiveRef(i)], RefIsPositive(i));
  }

  template <typename List>
  std::vector<IntegerVariable> Integers(const List& list) const {
    std::vector<IntegerVariable> result;
    for (const auto i : list) result.push_back(Integer(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<sat::Literal> Literals(const ProtoIndices& indices) const {
    std::vector<sat::Literal> result;
    for (const int i : indices) result.push_back(ModelWithMapping::Literal(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<IntervalVariable> Intervals(const ProtoIndices& indices) const {
    std::vector<IntervalVariable> result;
    for (const int i : indices) result.push_back(Interval(i));
    return result;
  }

  const IntervalsRepository& GetIntervalsRepository() const {
    const IntervalsRepository* repository = model_->Get<IntervalsRepository>();
    return *repository;
  }

  std::vector<int64> ExtractFullAssignment() const {
    std::vector<int64> result;
    const int num_variables = integers_.size();
    for (int i = 0; i < num_variables; ++i) {
      if (integers_[i] != kNoIntegerVariable) {
        if (model_->Get(LowerBound(integers_[i])) !=
            model_->Get(UpperBound(integers_[i]))) {
          // Notify that everything is not fixed.
          result.clear();
          return {};
        }
        if (model_->GetOrCreate<IntegerTrail>()->IsCurrentlyIgnored(
                integers_[i])) {
          // This variable is "ignored" so it may not be fixed, simply use
          // the current lower bound. Any value in its domain should lead to
          // a feasible solution.
          result.push_back(model_->Get(LowerBound(integers_[i])));
        } else {
          result.push_back(model_->Get(Value(integers_[i])));
        }
      } else if (booleans_[i] != kNoBooleanVariable) {
        result.push_back(model_->Get(Value(booleans_[i])));
      } else {
        // This variable is not used anywhere, fix it to its lower_bound.
        // TODO(user): maybe it is better to fix it to its lowest possible
        // magnitude.
        result.push_back(lower_bounds_[i]);
      }
    }
    return result;
  }

  // Returns true if we should not load this constraint. This is mainly used to
  // skip constraints that correspond to a basic encoding detected by
  // ExtractEncoding().
  bool IgnoreConstraint(const ConstraintProto* ct) const {
    return ContainsKey(ct_to_ignore_, ct);
  }

  Model* model() const { return model_; }

 private:
  void ExtractEncoding(const CpModelProto& model_proto);

  Model* model_;

  // Note that only the variables used by at leat one constraint will be
  // created, the other will have a kNo[Integer,Interval,Boolean]VariableValue.
  std::vector<IntegerVariable> integers_;
  std::vector<IntervalVariable> intervals_;
  std::vector<BooleanVariable> booleans_;

  // Used to return a feasible solution for the unused variables.
  std::vector<int64> lower_bounds_;

  // Set of constraints to ignore because they where already dealt with by
  // ExtractEncoding().
  std::unordered_set<const ConstraintProto*> ct_to_ignore_;
};

template <typename Values>
std::vector<int64> ValuesFromProto(const Values& values) {
  return std::vector<int64>(values.begin(), values.end());
}

// Returns the size of the given domain capped to int64max.
int64 DomainSize(const std::vector<ClosedInterval>& domain) {
  int64 size = 0;
  for (const ClosedInterval interval : domain) {
    size += operations_research::CapAdd(
        1, operations_research::CapSub(interval.end, interval.start));
  }
  return size;
}

// The logic assumes that the linear constraints have been presolved, so that
// equality with a domain bound have been converted to <= or >= and so that we
// never have any trivial inequalities.
void ModelWithMapping::ExtractEncoding(const CpModelProto& model_proto) {
  IntegerEncoder* encoder = GetOrCreate<IntegerEncoder>();

  // Detection of literal equivalent to (i_var == value). We collect all the
  // half-reified constraint lit => equality or lit => inequality for a given
  // variable, and we will later sort them to detect equivalence.
  struct EqualityDetectionHelper {
    const ConstraintProto* ct;
    sat::Literal literal;
    int64 value;
    bool is_equality;  // false if != instead.

    bool operator<(const EqualityDetectionHelper& o) const {
      if (literal.Variable() == o.literal.Variable()) {
        if (value == o.value) return is_equality && !o.is_equality;
        return value < o.value;
      }
      return literal.Variable() < o.literal.Variable();
    }
  };
  std::vector<std::vector<EqualityDetectionHelper>> var_to_equalities(
      model_proto.variables_size());

  // Detection of literal equivalent to (i_var >= bound). We also collect
  // all the half-refied part and we will sort the vector for detection of the
  // equivalence.
  struct InequalityDetectionHelper {
    const ConstraintProto* ct;
    sat::Literal literal;
    IntegerLiteral i_lit;

    bool operator<(const InequalityDetectionHelper& o) const {
      if (literal.Variable() == o.literal.Variable()) {
        return i_lit.Var() < o.i_lit.Var();
      }
      return literal.Variable() < o.literal.Variable();
    }
  };
  std::vector<InequalityDetectionHelper> inequalities;

  // Loop over all contraints and fill var_to_equalities and inequalities.
  for (const ConstraintProto& ct : model_proto.constraints()) {
    // For now, we only look at linear constraints with one term and an
    // enforcement literal.
    if (ct.enforcement_literal().empty()) continue;
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
      continue;
    }
    if (ct.linear().vars_size() != 1) continue;

    const sat::Literal enforcement_literal = Literal(ct.enforcement_literal(0));
    const int ref = ct.linear().vars(0);
    const int var = PositiveRef(ref);
    const auto rhs = InverseMultiplicationOfSortedDisjointIntervals(
        ReadDomain(ct.linear()),
        ct.linear().coeffs(0) * (RefIsPositive(ref) ? 1 : -1));

    // Detect enforcement_literal => (var >= value or var <= value).
    if (rhs.size() == 1) {
      // We relax by 1 because we may take the negation of the rhs above.
      if (rhs[0].end >= kint64max - 1) {
        inequalities.push_back({&ct, enforcement_literal,
                                IntegerLiteral::GreaterOrEqual(
                                    Integer(var), IntegerValue(rhs[0].start))});
      } else if (rhs[0].start <= kint64min + 1) {
        inequalities.push_back({&ct, enforcement_literal,
                                IntegerLiteral::LowerOrEqual(
                                    Integer(var), IntegerValue(rhs[0].end))});
      }
    }

    // Detect enforcement_literal => (var == value or var != value).
    //
    // Note that for domain with 2 values like [0, 1], we will detect both == 0
    // and != 1. Similarly, for a domain in [min, max], we should both detect
    // (== min) and (<= min), and both detect (== max) and (>= max).
    const auto domain = ReadDomain(model_proto.variables(var));
    {
      const auto inter = IntersectionOfSortedDisjointIntervals(domain, rhs);
      if (inter.size() == 1 && inter[0].start == inter[0].end) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter[0].start, true});
      }
    }
    {
      const auto inter = IntersectionOfSortedDisjointIntervals(
          domain, ComplementOfSortedDisjointIntervals(rhs));
      if (inter.size() == 1 && inter[0].start == inter[0].end) {
        var_to_equalities[var].push_back(
            {&ct, enforcement_literal, inter[0].start, false});
      }
    }
  }

  // Detect Literal <=> X >= value
  int num_inequalities = 0;
  std::sort(inequalities.begin(), inequalities.end());
  for (int i = 0; i + 1 < inequalities.size(); i++) {
    if (inequalities[i].literal != inequalities[i + 1].literal.Negated()) {
      continue;
    }
    const auto pair_a = encoder->Canonicalize(inequalities[i].i_lit);
    const auto pair_b = encoder->Canonicalize(inequalities[i + 1].i_lit);
    if (pair_a.first == pair_b.second) {
      ++num_inequalities;
      encoder->AssociateToIntegerLiteral(inequalities[i].literal,
                                         inequalities[i].i_lit);
      ct_to_ignore_.insert(inequalities[i].ct);
      ct_to_ignore_.insert(inequalities[i + 1].ct);
    }
  }
  if (!inequalities.empty()) {
    VLOG(1) << num_inequalities << " literals associated to VAR >= value (cts: "
            << inequalities.size() << ")";
  }

  // Detect Literal <=> X == value and fully encoded variables.
  int num_constraints = 0;
  int num_equalities = 0;
  int num_fully_encoded = 0;
  int num_partially_encoded = 0;
  for (int i = 0; i < var_to_equalities.size(); ++i) {
    std::vector<EqualityDetectionHelper>& encoding = var_to_equalities[i];
    std::sort(encoding.begin(), encoding.end());
    if (encoding.empty()) continue;
    num_constraints += encoding.size();

    std::unordered_set<int64> values;
    for (int j = 0; j + 1 < encoding.size(); j++) {
      if ((encoding[j].value != encoding[j + 1].value) ||
          (encoding[j].literal != encoding[j + 1].literal.Negated()) ||
          (encoding[j].is_equality != true) ||
          (encoding[j + 1].is_equality != false)) {
        continue;
      }

      ++num_equalities;
      encoder->AssociateToIntegerEqualValue(encoding[j].literal, integers_[i],
                                            IntegerValue(encoding[j].value));
      ct_to_ignore_.insert(encoding[j].ct);
      ct_to_ignore_.insert(encoding[j + 1].ct);
      values.insert(encoding[j].value);
    }

    // Detect fully encoded variables and mark them as such.
    //
    // TODO(user): Also fully encode variable that are almost fully encoded.
    const std::vector<ClosedInterval> domain =
        ReadDomain(model_proto.variables(i));
    if (DomainSize(domain) == values.size()) {
      ++num_fully_encoded;
      encoder->FullyEncodeVariable(integers_[i]);
    } else {
      ++num_partially_encoded;
    }
  }
  if (num_constraints > 0) {
    VLOG(1) << num_equalities
            << " literals associated to VAR == value (cts: " << num_constraints
            << ")";
  }
  if (num_fully_encoded > 0) {
    VLOG(1) << "num_fully_encoded_variables: " << num_fully_encoded;
  }
  if (num_partially_encoded > 0) {
    VLOG(1) << "num_partially_encoded_variables: " << num_partially_encoded;
  }
}

// Extracts all the used variables in the CpModelProto and creates a sat::Model
// representation for them.
ModelWithMapping::ModelWithMapping(const CpModelProto& model_proto,
                                   const VariableUsage& usage, Model* sat_model)
    : model_(sat_model) {
  integers_.resize(model_proto.variables_size(), kNoIntegerVariable);
  booleans_.resize(model_proto.variables_size(), kNoBooleanVariable);
  intervals_.resize(model_proto.constraints_size(), kNoIntervalVariable);
  lower_bounds_.resize(model_proto.variables_size(), 0);

  // Fills lower_bounds_, this is only used in ExtractFullAssignment().
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    lower_bounds_[i] = model_proto.variables(i).domain(0);
  }

  for (const int i : usage.integers) {
    const auto& var_proto = model_proto.variables(i);
    integers_[i] = Add(NewIntegerVariable(ReadDomain(var_proto)));
  }

  for (const int i : usage.intervals) {
    const ConstraintProto& ct = model_proto.constraints(i);
    CHECK(!HasEnforcementLiteral(ct)) << "Optional interval not yet supported.";
    intervals_[i] = Add(NewInterval(Integer(ct.interval().start()),
                                    Integer(ct.interval().end()),
                                    Integer(ct.interval().size())));
  }

  for (const int i : usage.booleans) {
    booleans_[i] = Add(NewBooleanVariable());
    const auto domain = ReadDomain(model_proto.variables(i));
    CHECK_EQ(domain.size(), 1);
    if (domain[0].start == 0 && domain[0].end == 0) {
      // Fix to false.
      Add(ClauseConstraint({sat::Literal(booleans_[i], false)}));
    } else if (domain[0].start == 1 && domain[0].end == 1) {
      // Fix to true.
      Add(ClauseConstraint({sat::Literal(booleans_[i], true)}));
    } else if (integers_[i] != kNoIntegerVariable) {
      // Associate with corresponding integer variable.
      const sat::Literal lit(booleans_[i], true);
      GetOrCreate<IntegerEncoder>()->FullyEncodeVariableUsingGivenLiterals(
          integers_[i], {lit.Negated(), lit},
          {IntegerValue(0), IntegerValue(1)});
    }
  }

  ModelWithMapping::ExtractEncoding(model_proto);
}

// =============================================================================
// A class that detects when variables should be fully encoded by computing a
// fixed point.
// =============================================================================

// This class is designed to be used over a ModelWithMapping, it will ask the
// underlying Model to fully encode IntegerVariables of the model using
// constraint processors PropagateConstraintXXX(), until no such processor wants
// to fully encode a variable. The workflow is to call PropagateFullEncoding()
// on a set of constraints, then ComputeFixedPoint() to launch the fixed point
// computation.
class FullEncodingFixedPointComputer {
 public:
  explicit FullEncodingFixedPointComputer(ModelWithMapping* model)
      : model_(model), integer_encoder_(model->GetOrCreate<IntegerEncoder>()) {}

  // We only add to the propagation queue variable that are fully encoded.
  // Note that if a variable was already added once, we never add it again.
  void ComputeFixedPoint() {
    // Make sure all fully encoded variables of interest are in the queue.
    for (int v = 0; v < variable_watchers_.size(); v++) {
      if (!variable_watchers_[v].empty() && IsFullyEncoded(v)) {
        AddVariableToPropagationQueue(v);
      }
    }
    // Propagate until no additional variable can be fully encoded.
    while (!variables_to_propagate_.empty()) {
      const int variable = variables_to_propagate_.back();
      variables_to_propagate_.pop_back();
      for (const ConstraintProto* ct : variable_watchers_[variable]) {
        if (ContainsKey(constraint_is_finished_, ct)) continue;
        const bool finished = PropagateFullEncoding(ct);
        if (finished) constraint_is_finished_.insert(ct);
      }
    }
  }

  // Return true if the constraint is finished encoding what its wants.
  bool PropagateFullEncoding(const ConstraintProto* ct) {
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintProto::kElement:
        return PropagateElement(ct);
      case ConstraintProto::ConstraintProto::kTable:
        return PropagateTable(ct);
      case ConstraintProto::ConstraintProto::kAutomata:
        return PropagateAutomata(ct);
      case ConstraintProto::ConstraintProto::kCircuit:
        return PropagateCircuit(ct);
      case ConstraintProto::ConstraintProto::kInverse:
        return PropagateInverse(ct);
      case ConstraintProto::ConstraintProto::kLinear:
        return PropagateLinear(ct);
      default:
        return true;
    }
  }

 private:
  // Constraint ct is interested by (full-encoding) state of variable.
  void Register(const ConstraintProto* ct, int variable) {
    variable = PositiveRef(variable);
    if (!ContainsKey(constraint_is_registered_, ct)) {
      constraint_is_registered_.insert(ct);
    }
    if (variable_watchers_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    variable_watchers_[variable].push_back(ct);
  }

  void AddVariableToPropagationQueue(int variable) {
    variable = PositiveRef(variable);
    if (variable_was_added_in_to_propagate_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    if (!variable_was_added_in_to_propagate_[variable]) {
      variable_was_added_in_to_propagate_[variable] = true;
      variables_to_propagate_.push_back(variable);
    }
  }

  // Note that we always consider a fixed variable to be fully encoded here.
  const bool IsFullyEncoded(int v) {
    const IntegerVariable variable = model_->Integer(v);
    return model_->Get(IsFixed(variable)) ||
           integer_encoder_->VariableIsFullyEncoded(variable);
  }

  void FullyEncode(int v) {
    v = PositiveRef(v);
    const IntegerVariable variable = model_->Integer(v);
    if (!model_->Get(IsFixed(variable))) {
      model_->Add(FullyEncodeVariable(variable));
    }
    AddVariableToPropagationQueue(v);
  }

  bool PropagateElement(const ConstraintProto* ct);
  bool PropagateTable(const ConstraintProto* ct);
  bool PropagateAutomata(const ConstraintProto* ct);
  bool PropagateCircuit(const ConstraintProto* ct);
  bool PropagateInverse(const ConstraintProto* ct);
  bool PropagateLinear(const ConstraintProto* ct);

  ModelWithMapping* model_;
  IntegerEncoder* integer_encoder_;

  std::vector<bool> variable_was_added_in_to_propagate_;
  std::vector<int> variables_to_propagate_;
  std::vector<std::vector<const ConstraintProto*>> variable_watchers_;

  std::unordered_set<const ConstraintProto*> constraint_is_finished_;
  std::unordered_set<const ConstraintProto*> constraint_is_registered_;
};

bool FullEncodingFixedPointComputer::PropagateElement(
    const ConstraintProto* ct) {
  // Index must always be full encoded.
  FullyEncode(ct->element().index());

  // If target is a constant or fully encoded, variables must be fully encoded.
  const int target = ct->element().target();
  if (IsFullyEncoded(target)) {
    for (const int v : ct->element().vars()) FullyEncode(v);
  }

  // If all non-target variables are fully encoded, target must be too.
  bool all_variables_are_fully_encoded = true;
  for (const int v : ct->element().vars()) {
    if (v == target) continue;
    if (!IsFullyEncoded(v)) {
      all_variables_are_fully_encoded = false;
      break;
    }
  }
  if (all_variables_are_fully_encoded) {
    if (!IsFullyEncoded(target)) FullyEncode(target);
    return true;
  }

  // If some variables are not fully encoded, register on those.
  if (!ContainsKey(constraint_is_registered_, ct)) {
    for (const int v : ct->element().vars()) Register(ct, v);
    Register(ct, target);
  }
  return false;
}

// If a constraint uses its variables in a symbolic (vs. numeric) manner,
// always encode its variables.
bool FullEncodingFixedPointComputer::PropagateTable(const ConstraintProto* ct) {
  if (ct->table().negated()) return true;
  for (const int variable : ct->table().vars()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateAutomata(
    const ConstraintProto* ct) {
  for (const int variable : ct->automata().vars()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateCircuit(
    const ConstraintProto* ct) {
  for (const int variable : ct->circuit().nexts()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateInverse(
    const ConstraintProto* ct) {
  for (const int variable : ct->inverse().f_direct()) {
    FullyEncode(variable);
  }
  for (const int variable : ct->inverse().f_inverse()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateLinear(
    const ConstraintProto* ct) {
  // Only act when the constraint is an equality.
  if (ct->linear().domain(0) != ct->linear().domain(1)) return true;

  // If some domain is too large, abort;
  if (!ContainsKey(constraint_is_registered_, ct)) {
    for (const int v : ct->linear().vars()) {
      const IntegerVariable var = model_->Integer(v);
      IntegerTrail* integer_trail = model_->GetOrCreate<IntegerTrail>();
      const IntegerValue lb = integer_trail->LowerBound(var);
      const IntegerValue ub = integer_trail->UpperBound(var);
      if (ub - lb > 1024) return true;  // Arbitrary limit value.
    }
  }

  if (HasEnforcementLiteral(*ct)) {
    // Fully encode x in half-reified equality b => x == constant.
    const auto& vars = ct->linear().vars();
    if (vars.size() == 1) FullyEncode(vars.Get(0));
    return true;
  } else {
    // If all variables but one are fully encoded,
    // force the last one to be fully encoded.
    int variable_not_fully_encoded;
    int num_fully_encoded = 0;
    for (const int var : ct->linear().vars()) {
      if (IsFullyEncoded(var))
        num_fully_encoded++;
      else
        variable_not_fully_encoded = var;
    }
    const int num_vars = ct->linear().vars_size();
    if (num_fully_encoded == num_vars - 1) {
      FullyEncode(variable_not_fully_encoded);
      return true;
    }
    if (num_fully_encoded == num_vars) return true;

    // Register on remaining variables if not already done.
    if (!ContainsKey(constraint_is_registered_, ct)) {
      for (const int var : ct->linear().vars()) {
        if (!IsFullyEncoded(var)) Register(ct, var);
      }
    }
    return false;
  }
}

// =============================================================================
// Constraint loading functions.
// =============================================================================

void LoadBoolOrConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  std::vector<Literal> literals = m->Literals(ct.bool_or().literals());
  if (HasEnforcementLiteral(ct)) {
    literals.push_back(m->Literal(ct.enforcement_literal(0)).Negated());
  }
  m->Add(ClauseConstraint(literals));
}

void LoadBoolAndConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<Literal> literals = m->Literals(ct.bool_and().literals());
  if (HasEnforcementLiteral(ct)) {
    const Literal is_true = m->Literal(ct.enforcement_literal(0));
    for (const Literal lit : literals) m->Add(Implication(is_true, lit));
  } else {
    for (const Literal lit : literals) m->Add(ClauseConstraint({lit}));
  }
}

void LoadBoolXorConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  CHECK(!HasEnforcementLiteral(ct)) << "Not supported.";
  m->Add(LiteralXorIs(m->Literals(ct.bool_xor().literals()), true));
}

void LoadLinearConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.linear().vars());
  const std::vector<int64> coeffs = ValuesFromProto(ct.linear().coeffs());
  if (ct.linear().domain_size() == 2) {
    const int64 lb = ct.linear().domain(0);
    const int64 ub = ct.linear().domain(1);
    if (!HasEnforcementLiteral(ct)) {
      // Detect if there is only Booleans in order to use a more efficient
      // propagator. TODO(user): we should probably also implement an
      // half-reified version of this constraint.
      bool all_booleans = true;
      std::vector<LiteralWithCoeff> cst;
      for (int i = 0; i < vars.size(); ++i) {
        const int ref = ct.linear().vars(i);
        if (!m->IsBoolean(ref)) {
          all_booleans = false;
          continue;
        }
        cst.push_back({m->Literal(ref), coeffs[i]});
      }
      if (all_booleans) {
        m->Add(BooleanLinearConstraint(lb, ub, &cst));
      } else {
        if (lb != kint64min) {
          m->Add(WeightedSumGreaterOrEqual(vars, coeffs, lb));
        }
        if (ub != kint64max) {
          m->Add(WeightedSumLowerOrEqual(vars, coeffs, ub));
        }
      }
    } else {
      const Literal is_true = m->Literal(ct.enforcement_literal(0));
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(is_true, vars, coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual(is_true, vars, coeffs, ub));
      }
    }
  } else {
    std::vector<Literal> clause;
    for (int i = 0; i < ct.linear().domain_size(); i += 2) {
      const int64 lb = ct.linear().domain(i);
      const int64 ub = ct.linear().domain(i + 1);
      const Literal literal(m->Add(NewBooleanVariable()), true);
      clause.push_back(literal);
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(literal, vars, coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual(literal, vars, coeffs, ub));
      }
    }
    if (HasEnforcementLiteral(ct)) {
      clause.push_back(m->Literal(ct.enforcement_literal(0)).Negated());
    }

    // TODO(user): In the cases where this clause only contains two literals,
    // then we could have only used one literal and its negation above.
    m->Add(ClauseConstraint(clause));
  }
}

void LoadAllDiffConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.all_diff().vars());
  // If all variables are fully encoded and domains are not too large, use
  // arc-consistent reasoning. Otherwise, use bounds-consistent reasoning.
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  int num_fully_encoded = 0;
  int64 max_domain_size = 0;
  for (const IntegerVariable variable : vars) {
    if (encoder->VariableIsFullyEncoded(variable)) num_fully_encoded++;

    IntegerValue lb = integer_trail->LowerBound(variable);
    IntegerValue ub = integer_trail->UpperBound(variable);
    int64 domain_size = ub.value() - lb.value();
    max_domain_size = std::max(max_domain_size, domain_size);
  }

  if (num_fully_encoded == vars.size() && max_domain_size < 1024) {
    m->Add(AllDifferentBinary(vars));
    m->Add(AllDifferentAC(vars));
  } else {
    m->Add(AllDifferentOnBounds(vars));
  }
}

void LoadIntProdConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable prod = m->Integer(ct.int_prod().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_prod().vars());
  CHECK_EQ(vars.size(), 2) << "General int_prod not supported yet.";
  m->Add(ProductConstraint(vars[0], vars[1], prod));
}

void LoadIntDivConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable div = m->Integer(ct.int_div().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_div().vars());
  m->Add(DivisionConstraint(vars[0], vars[1], div));
}

void LoadIntMinConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable min = m->Integer(ct.int_min().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_min().vars());
  m->Add(IsEqualToMinOf(min, vars));
}

void LoadIntMaxConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable max = m->Integer(ct.int_max().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_max().vars());
  m->Add(IsEqualToMaxOf(max, vars));
}

void LoadNoOverlapConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  m->Add(Disjunctive(m->Intervals(ct.no_overlap().intervals())));
}

void LoadNoOverlap2dConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntervalVariable> x_intervals =
      m->Intervals(ct.no_overlap_2d().x_intervals());
  const std::vector<IntervalVariable> y_intervals =
      m->Intervals(ct.no_overlap_2d().y_intervals());

  const IntervalsRepository& repository = m->GetIntervalsRepository();
  std::vector<IntegerVariable> x;
  std::vector<IntegerVariable> y;
  std::vector<IntegerVariable> dx;
  std::vector<IntegerVariable> dy;
  for (int i = 0; i < x_intervals.size(); ++i) {
    x.push_back(repository.StartVar(x_intervals[i]));
    y.push_back(repository.StartVar(y_intervals[i]));
    dx.push_back(repository.SizeVar(x_intervals[i]));
    dy.push_back(repository.SizeVar(y_intervals[i]));
  }
  m->Add(StrictNonOverlappingRectangles(x, y, dx, dy));
}

void LoadCumulativeConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntervalVariable> intervals =
      m->Intervals(ct.cumulative().intervals());
  const IntegerVariable capacity = m->Integer(ct.cumulative().capacity());
  const std::vector<IntegerVariable> demands =
      m->Integers(ct.cumulative().demands());
  m->Add(Cumulative(intervals, demands, capacity));
}

// If a variable is constant and its value appear in no other variable domains,
// then the literal encoding the index and the one encoding the target at this
// value are equivalent.
void DetectEquivalencesInElementConstraint(const ConstraintProto& ct,
                                           ModelWithMapping* m) {
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();

  const IntegerVariable index = m->Integer(ct.element().index());
  const IntegerVariable target = m->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.element().vars());

  if (m->Get(IsFixed(index))) return;

  std::vector<ClosedInterval> union_of_non_constant_domains;
  std::map<IntegerValue, int> constant_to_num;
  for (const auto literal_value : m->Add(FullyEncodeVariable(index))) {
    const int i = literal_value.value.value();
    if (m->Get(IsFixed(vars[i]))) {
      const IntegerValue value(m->Get(Value(vars[i])));
      constant_to_num[value]++;
    } else {
      union_of_non_constant_domains = UnionOfSortedDisjointIntervals(
          union_of_non_constant_domains,
          integer_trail->InitialVariableDomain(vars[i]));
    }
  }

  // Bump the number if the constant appear in union_of_non_constant_domains.
  for (const auto entry : constant_to_num) {
    if (SortedDisjointIntervalsContain(union_of_non_constant_domains,
                                       entry.first.value())) {
      constant_to_num[entry.first]++;
    }
  }

  // Use the literal from the index encoding to encode the target at the
  // "unique" values.
  for (const auto literal_value : m->Add(FullyEncodeVariable(index))) {
    const int i = literal_value.value.value();
    if (!m->Get(IsFixed(vars[i]))) continue;

    const IntegerValue value(m->Get(Value(vars[i])));
    if (constant_to_num[value] == 1) {
      const Literal r = literal_value.literal;
      encoder->AssociateToIntegerEqualValue(r, target, value);
    }
  }
}

// TODO(user): Be more efficient when the element().vars() are constants.
// Ideally we should avoid creating them as integer variable since we don't
// use them.
void LoadElementConstraintBounds(const ConstraintProto& ct,
                                 ModelWithMapping* m) {
  const IntegerVariable index = m->Integer(ct.element().index());
  const IntegerVariable target = m->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.element().vars());

  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  if (m->Get(IsFixed(index))) {
    const int64 value = integer_trail->LowerBound(index).value();
    m->Add(Equality(target, vars[value]));
    return;
  }

  // We always fully encode the index on an element constraint.
  const auto encoding = m->Add(FullyEncodeVariable((index)));
  std::vector<Literal> selectors;
  std::vector<IntegerVariable> possible_vars;
  for (const auto literal_value : encoding) {
    const int i = literal_value.value.value();
    CHECK_GE(i, 0) << "Should be presolved.";
    CHECK_LT(i, vars.size()) << "Should be presolved.";
    possible_vars.push_back(vars[i]);
    selectors.push_back(literal_value.literal);
    const Literal r = literal_value.literal;

    if (vars[i] == target) continue;
    if (m->Get(IsFixed(target))) {
      const int64 value = m->Get(Value(target));
      m->Add(ImpliesInInterval(r, vars[i], value, value));
    } else if (m->Get(IsFixed(vars[i]))) {
      const int64 value = m->Get(Value(vars[i]));
      m->Add(ImpliesInInterval(r, target, value, value));
    } else {
      m->Add(ConditionalLowerOrEqualWithOffset(vars[i], target, 0, r));
      m->Add(ConditionalLowerOrEqualWithOffset(target, vars[i], 0, r));
    }
  }
  m->Add(PartialIsOneOfVar(target, possible_vars, selectors));
}

// Arc-Consistent encoding of the element constraint as SAT clauses.
// The constraint enforces vars[index] == target.
//
// The AC propagation can be decomposed in three rules:
// Rule 1: dom(index) == i => dom(vars[i]) == dom(target).
// Rule 2: dom(target) \subseteq \Union_{i \in dom(index)} dom(vars[i]).
// Rule 3: dom(index) \subseteq { i | |dom(vars[i]) \inter dom(target)| > 0 }.
//
// We encode this in a way similar to the table constraint, except that the
// set of admissible tuples is not explicit.
// First, we add Booleans selected[i][value] <=> (index == i /\ vars[i] ==
// value). Rules 1 and 2 are enforced by target == value <=> \Or_{i}
// selected[i][value]. Rule 3 is enforced by index == i <=> \Or_{value}
// selected[i][value].
void LoadElementConstraintAC(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable index = m->Integer(ct.element().index());
  const IntegerVariable target = m->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.element().vars());

  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  if (m->Get(IsFixed(index))) {
    const int64 value = integer_trail->LowerBound(index).value();
    m->Add(Equality(target, vars[value]));
    return;
  }

  // Make map target_value -> literal.
  if (m->Get(IsFixed(target))) {
    return LoadElementConstraintBounds(ct, m);
  }
  std::unordered_map<IntegerValue, Literal> target_map;
  const auto target_encoding = m->Add(FullyEncodeVariable(target));
  for (const auto literal_value : target_encoding) {
    target_map[literal_value.value] = literal_value.literal;
  }

  // For i \in index and value in vars[i], make (index == i /\ vars[i] == value)
  // literals and store them by value in vectors.
  std::unordered_map<IntegerValue, std::vector<Literal>> value_to_literals;
  const auto index_encoding = m->Add(FullyEncodeVariable(index));
  for (const auto literal_value : index_encoding) {
    const int i = literal_value.value.value();
    const Literal i_lit = literal_value.literal;

    // Special case where vars[i] == value /\ i_lit is actually i_lit.
    if (m->Get(IsFixed(vars[i]))) {
      value_to_literals[integer_trail->LowerBound(vars[i])].push_back(i_lit);
      continue;
    }

    const auto var_encoding = m->Add(FullyEncodeVariable(vars[i]));
    std::vector<Literal> var_selected_literals;
    for (const auto var_literal_value : var_encoding) {
      const IntegerValue value = var_literal_value.value;
      const Literal var_is_value = var_literal_value.literal;

      if (!ContainsKey(target_map, value)) {
        // No need to add to value_to_literals, selected[i][value] is always
        // false.
        m->Add(Implication(i_lit, var_is_value.Negated()));
        continue;
      }

      const Literal var_is_value_and_selected =
          Literal(m->Add(NewBooleanVariable()), true);
      m->Add(ReifiedBoolAnd({i_lit, var_is_value}, var_is_value_and_selected));
      value_to_literals[value].push_back(var_is_value_and_selected);
      var_selected_literals.push_back(var_is_value_and_selected);
    }
    // index == i <=> \Or_{value} selected[i][value].
    m->Add(ReifiedBoolOr(var_selected_literals, i_lit));
  }

  // target == value <=> \Or_{i \in index} (vars[i] == value /\ index == i).
  for (const auto& entry : target_map) {
    const IntegerValue value = entry.first;
    const Literal target_is_value = entry.second;

    if (!ContainsKey(value_to_literals, value)) {
      m->Add(ClauseConstraint({target_is_value.Negated()}));
    } else {
      m->Add(ReifiedBoolOr(value_to_literals[value], target_is_value));
    }
  }
}

void LoadElementConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  IntegerEncoder* encoder = m->GetOrCreate<IntegerEncoder>();

  const int target = ct.element().target();
  const IntegerVariable target_var = m->Integer(target);
  const bool target_is_AC = m->Get(IsFixed(target_var)) ||
                            encoder->VariableIsFullyEncoded(target_var);

  int num_AC_variables = 0;
  const int num_vars = ct.element().vars().size();
  for (const int v : ct.element().vars()) {
    IntegerVariable variable = m->Integer(v);
    const bool is_full =
        m->Get(IsFixed(variable)) || encoder->VariableIsFullyEncoded(variable);
    if (is_full) num_AC_variables++;
  }

  DetectEquivalencesInElementConstraint(ct, m);
  if (target_is_AC || num_AC_variables >= num_vars - 1) {
    LoadElementConstraintAC(ct, m);
  } else {
    LoadElementConstraintBounds(ct, m);
  }
}

void LoadTableConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.table().vars());
  const std::vector<int64> values = ValuesFromProto(ct.table().values());
  const int num_vars = vars.size();
  const int num_tuples = values.size() / num_vars;
  std::vector<std::vector<int64>> tuples(num_tuples);
  int count = 0;
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < num_vars; ++j) {
      tuples[i].push_back(values[count++]);
    }
  }
  if (ct.table().negated()) {
    m->Add(NegatedTableConstraintWithoutFullEncoding(vars, tuples));
  } else {
    m->Add(TableConstraint(vars, tuples));
  }
}

void LoadAutomataConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.automata().vars());

  const int num_transitions = ct.automata().transition_tail_size();
  std::vector<std::vector<int64>> transitions;
  for (int i = 0; i < num_transitions; ++i) {
    transitions.push_back({ct.automata().transition_tail(i),
                           ct.automata().transition_label(i),
                           ct.automata().transition_head(i)});
  }

  const int64 starting_state = ct.automata().starting_state();
  const std::vector<int64> final_states =
      ValuesFromProto(ct.automata().final_states());
  m->Add(TransitionConstraint(vars, transitions, starting_state, final_states));
}

void LoadCircuitConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const int num_nodes = ct.circuit().nexts_size();
  const std::vector<IntegerVariable> nexts = m->Integers(ct.circuit().nexts());
  std::vector<std::vector<LiteralIndex>> graph(
      num_nodes, std::vector<LiteralIndex>(num_nodes, kFalseLiteralIndex));
  for (int i = 0; i < num_nodes; ++i) {
    if (m->Get(IsFixed(nexts[i]))) {
      // This is just an optimization. Note that if nexts[i] is not used in
      // other places, we didn't even need to create this constant variable in
      // the IntegerTrail...
      graph[i][m->Get(Value(nexts[i]))] = kTrueLiteralIndex;
      continue;
    } else {
      const auto encoding = m->Add(FullyEncodeVariable(nexts[i]));
      for (const auto& entry : encoding) {
        graph[i][entry.value.value()] = entry.literal.Index();
      }
    }
  }
  m->Add(SubcircuitConstraint(graph));
}

void LoadInverseConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  // Fully encode both arrays of variables, encode the constraint using Boolean
  // equalities: f_direct[i] == j <=> f_inverse[j] == i.
  const int num_variables = ct.inverse().f_direct_size();
  CHECK_EQ(num_variables, ct.inverse().f_inverse_size());
  const std::vector<IntegerVariable> direct =
      m->Integers(ct.inverse().f_direct());
  const std::vector<IntegerVariable> inverse =
      m->Integers(ct.inverse().f_inverse());

  // Fill LiteralIndex matrices.
  std::vector<std::vector<LiteralIndex>> matrix_direct(
      num_variables,
      std::vector<LiteralIndex>(num_variables, kFalseLiteralIndex));

  std::vector<std::vector<LiteralIndex>> matrix_inverse(
      num_variables,
      std::vector<LiteralIndex>(num_variables, kFalseLiteralIndex));

  auto fill_matrix = [&m](std::vector<std::vector<LiteralIndex>>& matrix,
                          const std::vector<IntegerVariable>& variables) {
    const int num_variables = variables.size();
    for (int i = 0; i < num_variables; i++) {
      if (m->Get(IsFixed(variables[i]))) {
        matrix[i][m->Get(Value(variables[i]))] = kTrueLiteralIndex;
      } else {
        const auto encoding = m->Add(FullyEncodeVariable(variables[i]));
        for (const auto literal_value : encoding) {
          matrix[i][literal_value.value.value()] =
              literal_value.literal.Index();
        }
      }
    }
  };

  fill_matrix(matrix_direct, direct);
  fill_matrix(matrix_inverse, inverse);

  // matrix_direct should be the transpose of matrix_inverse.
  for (int i = 0; i < num_variables; i++) {
    for (int j = 0; j < num_variables; j++) {
      LiteralIndex l_ij = matrix_direct[i][j];
      LiteralIndex l_ji = matrix_inverse[j][i];
      if (l_ij >= 0 && l_ji >= 0) {
        // l_ij <=> l_ji.
        m->Add(ClauseConstraint({Literal(l_ij), Literal(l_ji).Negated()}));
        m->Add(ClauseConstraint({Literal(l_ij).Negated(), Literal(l_ji)}));
      } else if (l_ij < 0 && l_ji < 0) {
        // Problem infeasible if l_ij != l_ji, otherwise nothing to add.
        if (l_ij != l_ji) {
          m->Add(ClauseConstraint({}));
          return;
        }
      } else {
        // One of the LiteralIndex is fixed, let it be l_ij.
        if (l_ij > l_ji) std::swap(l_ij, l_ji);
        const Literal lit = Literal(l_ji);
        m->Add(ClauseConstraint(
            {l_ij == kFalseLiteralIndex ? lit.Negated() : lit}));
      }
    }
  }
}

// Makes the std::string fit in one line by cutting it in the middle if necessary.
std::string Summarize(const std::string& input) {
  if (input.size() < 105) return input;
  const int half = 50;
  return StrCat(input.substr(0, half), " ... ",
                input.substr(input.size() - half, half));
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

std::string CpModelStats(const CpModelProto& model_proto) {
  std::map<ConstraintProto::ConstraintCase, int> num_constraints_by_type;
  std::map<ConstraintProto::ConstraintCase, int> num_reif_constraints_by_type;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    num_constraints_by_type[ct.constraint_case()]++;
    if (!ct.enforcement_literal().empty()) {
      num_reif_constraints_by_type[ct.constraint_case()]++;
    }
  }
  const VariableUsage usage = ComputeVariableUsage(model_proto);

  int num_constants = 0;
  std::set<int64> constant_values;
  std::map<std::vector<ClosedInterval>, int> num_vars_per_domains;
  for (const IntegerVariableProto& var : model_proto.variables()) {
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      ++num_constants;
      constant_values.insert(var.domain(0));
    } else {
      num_vars_per_domains[ReadDomain(var)]++;
    }
  }

  std::string result;
  StrAppend(&result, "Model '", model_proto.name(), "':\n");

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    StrAppend(&result, "Search strategy: on ", strategy.variables_size(),
                    " variables, ",
                    DecisionStrategyProto::VariableSelectionStrategy_Name(
                        strategy.variable_selection_strategy()),
                    ", ",
                    DecisionStrategyProto::DomainReductionStrategy_Name(
                        strategy.domain_reduction_strategy()),
                    "\n");
  }

  StrAppend(&result, "#Variables: ", model_proto.variables_size(), "\n");
  if (num_vars_per_domains.size() < 20) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp = StrCat(" - ", entry.second, " in ",
                                       IntervalsAsString(entry.first), "\n");
      StrAppend(&result, Summarize(temp));
    }
  } else {
    size_t max_complexity = 0;
    int64 min = kint64max;
    int64 max = kint64min;
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.front().start);
      max = std::max(max, entry.first.back().end);
      max_complexity = std::max(max_complexity, entry.first.size());
    }
    StrAppend(&result, " - ", num_vars_per_domains.size(),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        StrCat(" - ", num_constants, " constants in {",
                     strings::Join(constant_values, ","), "} \n");
    StrAppend(&result, Summarize(temp));
  }

  StrAppend(&result, "#Booleans: ", usage.booleans.size(), "\n");
  StrAppend(&result, "#Integers: ", usage.integers.size(), "\n");

  std::vector<std::string> constraints;
  for (const auto entry : num_constraints_by_type) {
    constraints.push_back(
        StrCat("#", ConstraintCaseName(entry.first), ": ", entry.second,
                     " (", num_reif_constraints_by_type[entry.first],
                     " with enforcement literal)"));
  }
  std::sort(constraints.begin(), constraints.end());
  StrAppend(&result, strings::Join(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response) {
  std::string result;
  StrAppend(&result, "CpSolverResponse:");
  StrAppend(&result,
                  "\nstatus: ", CpSolverStatus_Name(response.status()));

  // We special case the pure-decision problem for clarity.
  //
  // TODO(user): This test is not ideal for the corner case where the status is
  // still UNKNOWN yet we already know that if there is a solution, then its
  // objective is zero...
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.objective_value() == 0 && response.best_objective_bound() == 0) {
    StrAppend(&result, "\nobjective: NA");
    StrAppend(&result, "\nbest_bound: NA");
  } else {
    StrAppend(&result, "\nobjective: ", response.objective_value());
    StrAppend(&result, "\nbest_bound: ", response.best_objective_bound());
  }

  StrAppend(&result, "\nbooleans: ", response.num_booleans());
  StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());
  StrAppend(&result, "\nwalltime: ", response.wall_time());
  StrAppend(&result, "\nusertime: ", response.user_time());
  StrAppend(&result,
                  "\ndeterministic_time: ", response.deterministic_time());
  StrAppend(&result, "\n");
  return result;
}

namespace {

double ScaleObjectiveValue(const CpObjectiveProto& proto, int64 value) {
  double result = value + proto.offset();
  if (proto.scaling_factor() == 0) return result;
  return proto.scaling_factor() * result;
}

bool LoadConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      return true;
    case ConstraintProto::ConstraintCase::kBoolOr:
      LoadBoolOrConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      LoadBoolAndConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolXor:
      LoadBoolXorConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kLinear:
      LoadLinearConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAllDiff:
      LoadAllDiffConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntProd:
      LoadIntProdConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntDiv:
      LoadIntDivConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMin:
      LoadIntMinConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMax:
      LoadIntMaxConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInterval:
      // Already dealt with.
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap:
      LoadNoOverlapConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap2D:
      LoadNoOverlap2dConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCumulative:
      LoadCumulativeConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kElement:
      LoadElementConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kTable:
      LoadTableConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAutomata:
      LoadAutomataConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCircuit:
      LoadCircuitConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInverse:
      LoadInverseConstraint(ct, m);
      return true;
    default:
      return false;
  }
}

// TODO(user): In full generality, we could encode all the constraint as an LP.
void LoadConstraintInGlobalLp(const ConstraintProto& ct, ModelWithMapping* m,
                              LinearProgrammingConstraint* lp) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (HasEnforcementLiteral(ct)) return;
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
    // TODO(user): Support this when the LinearProgrammingConstraint support
    // SetCoefficient() with literals.
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
    const int target = ct.int_max().target();
    for (const int var : ct.int_max().vars()) {
      // This deal with the corner case X = std::max(X, Y, Z, ..) !
      // Note that this can be presolved into X >= Y, X >= Z, ...
      if (target == var) continue;
      const auto lp_constraint = lp->CreateNewConstraint(-kInfinity, 0.0);
      lp->SetCoefficient(lp_constraint, m->Integer(var), 1.0);
      lp->SetCoefficient(lp_constraint, m->Integer(target), -1.0);
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMin) {
    const int target = ct.int_min().target();
    for (const int var : ct.int_min().vars()) {
      if (target == var) continue;
      const auto lp_constraint = lp->CreateNewConstraint(-kInfinity, 0.0);
      lp->SetCoefficient(lp_constraint, m->Integer(target), 1.0);
      lp->SetCoefficient(lp_constraint, m->Integer(var), -1.0);
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    // Note that we ignore the holes in the domain...
    const int64 min = ct.linear().domain(0);
    const int64 max = ct.linear().domain(ct.linear().domain_size() - 1);
    if (min == kint64min && max == kint64max) return;

    // This is needed in case of duplicate variables in the linear constraint.
    std::unordered_map<IntegerVariable, double> terms;
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      terms[m->Integer(ct.linear().vars(i))] += ct.linear().coeffs(i);
    }

    const double lb = (min == kint64min) ? -kInfinity : min;
    const double ub = (max == kint64max) ? kInfinity : max;
    const auto lp_constraint = lp->CreateNewConstraint(lb, ub);
    for (const auto entry : terms) {
      lp->SetCoefficient(lp_constraint, entry.first, entry.second);
    }
  }
}

void FillSolutionInResponse(const CpModelProto& model_proto,
                            const ModelWithMapping& m,
                            CpSolverResponse* response) {
  const std::vector<int64> solution = m.ExtractFullAssignment();
  if (!solution.empty()) {
    CHECK(SolutionIsFeasible(model_proto, solution));
    response->clear_solution();
    for (const int64 value : solution) response->add_solution(value);
  } else {
    // Not all variables are fixed.
    // We fill instead the lb/ub of each variables.
    response->clear_solution_lower_bounds();
    response->clear_solution_upper_bounds();
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (m.IsInteger(i)) {
        response->add_solution_lower_bounds(m.Get(LowerBound(m.Integer(i))));
        response->add_solution_upper_bounds(m.Get(UpperBound(m.Integer(i))));
      } else {
        const int value = m.Get(Value(m.Boolean(i)));
        response->add_solution_lower_bounds(value);
        response->add_solution_upper_bounds(value);
      }
    }
  }
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    Model* model, const std::vector<std::pair<IntegerVariable, int64>>& terms) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  int64 sum_min = 0;
  int64 sum_max = 0;
  for (const std::pair<IntegerVariable, int64> var_coeff : terms) {
    const int64 min_domain = model->Get(LowerBound(var_coeff.first));
    const int64 max_domain = model->Get(UpperBound(var_coeff.first));
    const int64 coeff = var_coeff.second;
    const int64 prod1 = min_domain * coeff;
    const int64 prod2 = max_domain * coeff;
    sum_min += std::min(prod1, prod2);
    sum_max += std::max(prod1, prod2);
  }
  return model->Add(NewIntegerVariable(sum_min, sum_max));
}

IntegerVariable GetOrCreateVariableGreaterOrEqualToSumOf(
    Model* model, const std::vector<std::pair<IntegerVariable, int64>>& terms) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  // Create a new variable and link it with the linear terms.
  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(model, terms);
  std::vector<IntegerVariable> vars;
  std::vector<int64> coeffs;
  for (const auto& term : terms) {
    vars.push_back(term.first);
    coeffs.push_back(term.second);
  }
  vars.push_back(new_var);
  coeffs.push_back(-1);
  model->Add(WeightedSumLowerOrEqual(vars, coeffs, 0));
  return new_var;
}
}  // namespace

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(const CpModelProto& model_proto,
                                 ModelWithMapping* m) {
  const int num_constraints = model_proto.constraints().size();
  const int num_variables = model_proto.variables().size();

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_constraints),
  // variable nodes by [num_constraints..num_constraints+num_variables).
  // TODO(user): look into biconnected components.
  ConnectedComponents<int, int> components;
  components.Init(num_constraints + num_variables);
  std::vector<bool> constraint_has_lp_representation(num_constraints);
  auto get_var_index = [num_constraints](int proto_var_index) {
    return num_constraints + PositiveRef(proto_var_index);
  };

  for (int i = 0; i < num_constraints; i++) {
    const auto& ct = model_proto.constraints(i);
    // Skip reified constraints.
    if (HasEnforcementLiteral(ct)) continue;

    constraint_has_lp_representation[i] = true;
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
      components.AddArc(i, get_var_index(ct.int_max().target()));
      for (const int var : ct.int_max().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kIntMin) {
      components.AddArc(i, get_var_index(ct.int_min().target()));
      for (const int var : ct.int_min().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kLinear) {
      for (const int var : ct.linear().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else {
      constraint_has_lp_representation[i] = false;
    }
  }

  std::unordered_map<int, int> components_to_size;
  for (int i = 0; i < num_constraints; i++) {
    if (constraint_has_lp_representation[i]) {
      const int id = components.GetClassRepresentative(i);
      components_to_size[id] += 1;
    }
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::unordered_map<int, LinearProgrammingConstraint*> representative_to_lp_constraint;
  std::unordered_map<int, std::vector<std::pair<IntegerVariable, int64>>>
      representative_to_cp_terms;
  std::vector<std::pair<IntegerVariable, int64>> top_level_cp_terms;
  std::vector<LinearProgrammingConstraint*> lp_constraints;
  for (int i = 0; i < num_constraints; i++) {
    if (constraint_has_lp_representation[i]) {
      const auto& ct = model_proto.constraints(i);
      const int id = components.GetClassRepresentative(i);
      if (components_to_size[id] <= 1) continue;
      if (!ContainsKey(representative_to_lp_constraint, id)) {
        auto* lp = m->model()->Create<LinearProgrammingConstraint>();
        representative_to_lp_constraint[id] = lp;
        lp_constraints.push_back(lp);
      }
      LoadConstraintInGlobalLp(ct, m, representative_to_lp_constraint[id]);
    }
  }

  // Add the objective.
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const int var = model_proto.objective().vars(i);
      const IntegerVariable cp_var = m->Integer(var);
      const int64 coeff = model_proto.objective().coeffs(i);
      const int id = components.GetClassRepresentative(get_var_index(var));
      if (ContainsKey(representative_to_lp_constraint, id)) {
        representative_to_lp_constraint[id]->SetObjectiveCoefficient(cp_var,
                                                                     coeff);
        representative_to_cp_terms[id].push_back(std::make_pair(cp_var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(cp_var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (const auto& it : representative_to_cp_terms) {
      const int id = it.first;
      LinearProgrammingConstraint* lp =
          FindOrDie(representative_to_lp_constraint, id);
      const std::vector<std::pair<IntegerVariable, int64>>& terms = it.second;
      const IntegerVariable sub_obj_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(m->model(), terms);
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  IntegerVariable main_objective_var;
  if (m->GetOrCreate<SatSolver>()->parameters().optimize_with_core()) {
    main_objective_var =
        GetOrCreateVariableWithTightBound(m->model(), top_level_cp_terms);
  } else {
    main_objective_var = GetOrCreateVariableGreaterOrEqualToSumOf(
        m->model(), top_level_cp_terms);
  }

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (auto* lp_constraint : lp_constraints) {
    lp_constraint->RegisterWith(m->GetOrCreate<GenericLiteralWatcher>());
  }

  VLOG(1) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  VLOG_IF(1, !lp_constraints.empty())
      << "Added " << lp_constraints.size() << " LP constraints.";
  return main_objective_var;
}

// The function responsible for implementing the chosen search strategy.
//
// TODO(user): expose and unit-test, it seems easy to get the order wrong, and
// that would not change the correctness.
struct Strategy {
  std::vector<IntegerVariable> variables;
  DecisionStrategyProto::VariableSelectionStrategy var_strategy;
  DecisionStrategyProto::DomainReductionStrategy domain_strategy;
};
const std::function<LiteralIndex()> ConstructSearchStrategy(
    const std::unordered_map<int, std::pair<int64, int64>>&
        var_to_coeff_offset_pair,
    const std::vector<Strategy>& strategies, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();

  // Note that we copy strategies to keep the return function validity
  // independently of the life of the passed vector.
  return [integer_encoder, integer_trail, strategies,
          var_to_coeff_offset_pair]() {
    for (const Strategy& strategy : strategies) {
      IntegerVariable candidate = kNoIntegerVariable;
      IntegerValue candidate_value = kMaxIntegerValue;
      IntegerValue candidate_lb;
      IntegerValue candidate_ub;

      // TODO(user): Improve the complexity if this becomes an issue which
      // may be the case if we do a fixed_search.
      for (const IntegerVariable var : strategy.variables) {
        if (integer_trail->IsCurrentlyIgnored(var)) continue;
        const IntegerValue lb = integer_trail->LowerBound(var);
        const IntegerValue ub = integer_trail->UpperBound(var);
        if (lb == ub) continue;
        IntegerValue value(0);
        IntegerValue coeff(1);
        IntegerValue offset(0);
        if (ContainsKey(var_to_coeff_offset_pair, var.value())) {
          const auto coeff_offset =
              FindOrDie(var_to_coeff_offset_pair, var.value());
          coeff = coeff_offset.first;
          offset = coeff_offset.second;
        }
        DCHECK_GT(coeff, 0);
        switch (strategy.var_strategy) {
          case DecisionStrategyProto::CHOOSE_FIRST:
            break;
          case DecisionStrategyProto::CHOOSE_LOWEST_MIN:
            value = coeff * lb + offset;
            break;
          case DecisionStrategyProto::CHOOSE_HIGHEST_MAX:
            value = -(coeff * ub + offset);
            break;
          case DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE:
            value = coeff * (ub - lb);
            break;
          case DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE:
            value = -coeff * (ub - lb);
            break;
          default:
            LOG(FATAL) << "Unknown VariableSelectionStrategy "
                       << strategy.var_strategy;
        }
        if (value < candidate_value) {
          candidate = var;
          candidate_lb = lb;
          candidate_ub = ub;
          candidate_value = value;
        }
        if (strategy.var_strategy == DecisionStrategyProto::CHOOSE_FIRST) break;
      }
      if (candidate == kNoIntegerVariable) continue;

      IntegerLiteral literal;
      switch (strategy.domain_strategy) {
        case DecisionStrategyProto::SELECT_MIN_VALUE:
          literal = IntegerLiteral::LowerOrEqual(candidate, candidate_lb);
          break;
        case DecisionStrategyProto::SELECT_MAX_VALUE:
          literal = IntegerLiteral::GreaterOrEqual(candidate, candidate_ub);
          break;
        case DecisionStrategyProto::SELECT_LOWER_HALF:
          literal = IntegerLiteral::LowerOrEqual(
              candidate, candidate_lb + (candidate_ub - candidate_lb) / 2);
          break;
        case DecisionStrategyProto::SELECT_UPPER_HALF:
          literal = IntegerLiteral::GreaterOrEqual(
              candidate, candidate_ub - (candidate_ub - candidate_lb) / 2);
          break;
        default:
          LOG(FATAL) << "Unknown DomainReductionStrategy "
                     << strategy.domain_strategy;
      }
      return integer_encoder->GetOrCreateAssociatedLiteral(literal).Index();
    }
    return kNoLiteralIndex;
  };
}

void ExtractLinearObjective(const CpModelProto& model_proto,
                            ModelWithMapping* m,
                            std::vector<IntegerVariable>* linear_vars,
                            std::vector<IntegerValue>* linear_coeffs) {
  CHECK(model_proto.has_objective());
  const CpObjectiveProto& obj = model_proto.objective();
  linear_vars->reserve(obj.vars_size());
  linear_coeffs->reserve(obj.vars_size());
  for (int i = 0; i < obj.vars_size(); ++i) {
    linear_vars->push_back(m->Integer(obj.vars(i)));
    linear_coeffs->push_back(IntegerValue(obj.coeffs(i)));
  }
}

}  // namespace

// Used by NewFeasibleSolutionObserver to register observers.
struct SolutionObservers {
  explicit SolutionObservers(Model* model) {}
  std::vector<std::function<void(const std::vector<int64>& values)>> observers;
};

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const std::vector<int64>& values)>& observer) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->observers.push_back(observer);
  };
}

namespace {

CpSolverResponse SolveCpModelInternal(const CpModelProto& model_proto,
                                      bool display_fixing_constraints,
                                      Model* model) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // Initialize a default invalid response.
  CpSolverResponse response;
  response.set_status(CpSolverStatus::MODEL_INVALID);

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  // Instantiate all the needed variables.
  const VariableUsage usage = ComputeVariableUsage(model_proto);
  ModelWithMapping m(model_proto, usage, model);

  const SatParameters& parameters =
      model->GetOrCreate<SatSolver>()->parameters();

  // Force some variables to be fully encoded.
  FullEncodingFixedPointComputer fixpoint(&m);
  for (const ConstraintProto& ct : model_proto.constraints()) {
    fixpoint.PropagateFullEncoding(&ct);
  }
  fixpoint.ComputeFixedPoint();

  // Load the constraints.
  std::set<std::string> unsupported_types;
  Trail* trail = model->GetOrCreate<Trail>();
  int num_ignored_constraints = 0;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (m.IgnoreConstraint(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    const int old_num_fixed = trail->Index();
    if (!LoadConstraint(ct, &m)) {
      unsupported_types.insert(ConstraintCaseName(ct.constraint_case()));
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call Propagate() manually here.
    // TODO(user): Do that automatically?
    model->GetOrCreate<SatSolver>()->Propagate();
    if (display_fixing_constraints && trail->Index() > old_num_fixed) {
      VLOG(1) << "Constraint fixed " << trail->Index() - old_num_fixed
              << " Boolean variable(s): " << ct.DebugString();
    }
    if (model->GetOrCreate<SatSolver>()->IsModelUnsat()) {
      VLOG(1) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ct.DebugString();
      break;
    }
  }
  if (num_ignored_constraints > 0) {
    VLOG(1) << num_ignored_constraints << " constraints where skipped.";
  }
  if (!unsupported_types.empty()) {
    VLOG(1) << "There is unsuported constraints types in this model: ";
    for (const std::string& type : unsupported_types) {
      VLOG(1) << " - " << type;
    }
    return response;
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (parameters.use_global_lp_constraint()) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var = AddLPConstraints(model_proto, &m);
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(std::make_pair(m.Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core()) {
      objective_var = GetOrCreateVariableWithTightBound(m.model(), terms);
    } else {
      objective_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(m.model(), terms);
    }
  }

  // Note that we do one last propagation at level zero once all the constraints
  // where added.
  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  model->GetOrCreate<SatSolver>()->Propagate();

  // Initialize the search strategy function.
  std::function<LiteralIndex()> next_decision;
  if (model_proto.search_strategy().empty()) {
    std::vector<IntegerVariable> decisions;
    for (const int i : usage.integers) {
      if (model_proto.has_objective()) {
        // Make sure we try to fix the objective to its lowest value first.
        if (m.Integer(i) == NegationOf(objective_var)) {
          decisions.push_back(objective_var);
          continue;
        }
      }
      decisions.push_back(m.Integer(i));
    }
    next_decision = FirstUnassignedVarAtItsMinHeuristic(decisions, model);
  } else {
    std::vector<Strategy> strategies;
    std::unordered_map<int, std::pair<int64, int64>> var_to_coeff_offset_pair;
    for (const DecisionStrategyProto& proto : model_proto.search_strategy()) {
      strategies.push_back(Strategy());
      Strategy& strategy = strategies.back();
      for (const int ref : proto.variables()) {
        strategy.variables.push_back(m.Integer(ref));
      }
      strategy.var_strategy = proto.variable_selection_strategy();
      strategy.domain_strategy = proto.domain_reduction_strategy();
      for (const auto& tranform : proto.transformations()) {
        const IntegerVariable var = m.Integer(tranform.var());
        if (!ContainsKey(var_to_coeff_offset_pair, var.value())) {
          var_to_coeff_offset_pair[var.value()] = {tranform.positive_coeff(),
                                                   tranform.offset()};
        }
      }
    }
    next_decision =
        ConstructSearchStrategy(var_to_coeff_offset_pair, strategies, model);
  }

  // Solve.
  int num_solutions = 0;
  SatSolver::Status status;
  if (!model_proto.has_objective()) {
    int num_solutions = 0;
    while (true) {
      status = SolveIntegerProblemWithLazyEncoding(
          /*assumptions=*/{}, next_decision, model);
      if (status != SatSolver::Status::MODEL_SAT) break;

      // TODO(user): add all solutions to the response? or their count?
      if (num_solutions == 0) {
        FillSolutionInResponse(model_proto, m, &response);
      }

      ++num_solutions;
      for (const auto& observer :
           m.GetOrCreate<SolutionObservers>()->observers) {
        observer(m.ExtractFullAssignment());
      }

      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionAndBacktrack());
    }
    if (num_solutions > 0) {
      status = SatSolver::Status::MODEL_SAT;
    }
  } else {
    // Optimization problem.
    const CpObjectiveProto& obj = model_proto.objective();
    VLOG(1) << obj.vars_size() << " terms in the proto objective.";
    const auto solution_observer =
        [&model_proto, &response, &num_solutions, &obj, &m,
         objective_var](const Model& sat_model) {
          num_solutions++;
          for (const auto observer :
               m.GetOrCreate<SolutionObservers>()->observers) {
            observer(m.ExtractFullAssignment());
          }
          FillSolutionInResponse(model_proto, m, &response);
          int64 objective_value = 0;
          for (int i = 0; i < model_proto.objective().vars_size(); ++i) {
            objective_value += model_proto.objective().coeffs(i) *
                               sat_model.Get(Value(
                                   m.Integer(model_proto.objective().vars(i))));
          }
          response.set_objective_value(
              ScaleObjectiveValue(obj, objective_value));
          VLOG(1) << "Solution #" << num_solutions
                  << " obj:" << response.objective_value()
                  << " num_bool:" << sat_model.Get<SatSolver>()->NumVariables();
        };

    if (parameters.optimize_with_core()) {
      std::vector<IntegerVariable> linear_vars;
      std::vector<IntegerValue> linear_coeffs;
      ExtractLinearObjective(model_proto, &m, &linear_vars, &linear_coeffs);
#if defined(USE_CBC) || defined(USE_SCIPe)
      if (parameters.optimize_with_max_hs()) {
        status = MinimizeWithHittingSetAndLazyEncoding(
            VLOG_IS_ON(1), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
      } else {
#endif  //  defined(USE_CBC) || defined(USE_SCIPe)
        status = MinimizeWithCoreAndLazyEncoding(
            VLOG_IS_ON(1), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
#if defined(USE_CBC) || defined(USE_SCIPe)
      }
#endif  //  defined(USE_CBC) || defined(USE_SCIPe)
    } else {
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          /*log_info=*/false, objective_var, next_decision, solution_observer,
          model);
    }

    if (status == SatSolver::LIMIT_REACHED) {
      model->GetOrCreate<SatSolver>()->Backtrack(0);
      if (num_solutions == 0) {
        response.set_objective_value(
            ScaleObjectiveValue(obj, model->Get(UpperBound(objective_var))));
      }
      response.set_best_objective_bound(
          ScaleObjectiveValue(obj, model->Get(LowerBound(objective_var))));
    } else if (status == SatSolver::MODEL_SAT) {
      // Optimal!
      response.set_best_objective_bound(response.objective_value());
    }
  }

  // Fill response.
  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(num_solutions != 0 ? CpSolverStatus::MODEL_SAT
                                             : CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::MODEL_SAT: {
      response.set_status(model_proto.has_objective()
                              ? CpSolverStatus::OPTIMAL
                              : CpSolverStatus::MODEL_SAT);
      break;
    }
    case SatSolver::MODEL_UNSAT: {
      response.set_status(CpSolverStatus::MODEL_UNSAT);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  response.set_num_booleans(model->Get<SatSolver>()->NumVariables());
  response.set_num_branches(model->Get<SatSolver>()->num_branches());
  response.set_num_conflicts(model->Get<SatSolver>()->num_failures());
  response.set_num_binary_propagations(
      model->Get<SatSolver>()->num_propagations());
  response.set_num_integer_propagations(
      model->Get<IntegerTrail>() == nullptr
          ? 0
          : model->Get<IntegerTrail>()->num_enqueues());
  response.set_wall_time(wall_timer.Get());
  response.set_user_time(user_timer.Get());
  response.set_deterministic_time(
      model->Get<SatSolver>()->deterministic_time());
  return response;
}

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  {
    const std::string error = ValidateCpModel(model_proto);
    if (!error.empty()) {
      VLOG(1) << error;
      CpSolverResponse response;
      response.set_status(CpSolverStatus::MODEL_INVALID);
      return response;
    }
  }

  const SatParameters& parameters =
      model->GetOrCreate<SatSolver>()->parameters();
  if (!parameters.cp_model_presolve()) {
    return SolveCpModelInternal(model_proto, true, model);
  }

  CpModelProto presolved_proto;
  CpModelProto mapping_proto;
  std::vector<int> postsolve_mapping;
  PresolveCpModel(model_proto, &presolved_proto, &mapping_proto,
                  &postsolve_mapping);

  VLOG(1) << CpModelStats(presolved_proto);

  CpSolverResponse response =
      SolveCpModelInternal(presolved_proto, true, model);
  if (response.status() != CpSolverStatus::MODEL_SAT &&
      response.status() != CpSolverStatus::OPTIMAL) {
    return response;
  }

  // Postsolve.
  for (int i = 0; i < response.solution_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain(response.solution(i));
    var_proto->add_domain(response.solution(i));
  }
  for (int i = 0; i < response.solution_lower_bounds_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    FillDomain(
        IntersectionOfSortedDisjointIntervals(
            ReadDomain(*var_proto), {{response.solution_lower_bounds(i),
                                      response.solution_upper_bounds(i)}}),
        var_proto);
  }
  Model postsolve_model;

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  {
    SatParameters params;
    params.set_use_global_lp_constraint(false);
    postsolve_model.Add(operations_research::sat::NewSatParameters(params));
  }
  const CpSolverResponse postsolve_response =
      SolveCpModelInternal(mapping_proto, false, &postsolve_model);
  CHECK_EQ(postsolve_response.status(), CpSolverStatus::MODEL_SAT);
  response.clear_solution();
  response.clear_solution_lower_bounds();
  response.clear_solution_upper_bounds();
  if (!postsolve_response.solution().empty()) {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response.add_solution(postsolve_response.solution(i));
    }
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response.solution().begin(),
                                                response.solution().end())));
  } else {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response.add_solution_lower_bounds(
          postsolve_response.solution_lower_bounds(i));
      response.add_solution_upper_bounds(
          postsolve_response.solution_upper_bounds(i));
    }
  }
  return response;
}

}  // namespace sat
}  // namespace operations_research
