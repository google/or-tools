// Copyright 2010-2017 Google
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

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "google/protobuf/text_format.h"
#endif  // __PORTABLE_PLATFORM__
#include "ortools/base/join.h"
#include "ortools/base/join.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/connectivity.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/table.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

DEFINE_string(cp_model_dump_file, "",
              "DEBUG ONLY. When this is set to a non-empty file name, "
              "SolveCpModel() will dump its model to this file. Note that the "
              "file will be ovewritten with the last such model. "
              "TODO(fdid): dump all model to a recordio file instead?");
DEFINE_string(cp_model_params, "",
              "This is interpreted as a text SatParameters proto. The "
              "specified fields will override the normal ones for all solves.");

DEFINE_string(
    drat_output, "",
    "If non-empty, a proof in DRAT format will be written to this file. "
    "This will only be used for pure-SAT problems.");

DEFINE_bool(drat_check, false,
            "If true, a proof in DRAT format will be stored in memory and "
            "checked if the problem is UNSAT. This will only be used for "
            "pure-SAT problems.");

DEFINE_double(max_drat_time_in_seconds, std::numeric_limits<double>::infinity(),
              "Maximum time in seconds to check the DRAT proof. This will only "
              "be used is the drat_check flag is enabled.");

namespace operations_research {
namespace sat {

namespace {

// =============================================================================
// Helper classes.
// =============================================================================

// Holds the sat::model and the mapping between the proto indices and the
// sat::model ones.
class ModelWithMapping {
 public:
  ModelWithMapping(const CpModelProto& model_proto, Model* model);

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
    DCHECK(IsInteger(i));
    const IntegerVariable var = integers_[PositiveRef(i)];
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
    DCHECK(IsBoolean(i));
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
    Trail* trail = model_->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model_->GetOrCreate<IntegerTrail>();
    for (int i = 0; i < num_variables; ++i) {
      if (integers_[i] != kNoIntegerVariable) {
        if (integer_trail->IsCurrentlyIgnored(integers_[i])) {
          // This variable is "ignored" so it may not be fixed, simply use
          // the current lower bound. Any value in its domain should lead to
          // a feasible solution.
          result.push_back(model_->Get(LowerBound(integers_[i])));
        } else {
          if (model_->Get(LowerBound(integers_[i])) !=
              model_->Get(UpperBound(integers_[i]))) {
            // Notify that everything is not fixed.
            return {};
          }
          result.push_back(model_->Get(Value(integers_[i])));
        }
      } else if (booleans_[i] != kNoBooleanVariable) {
        if (trail->Assignment().VariableIsAssigned(booleans_[i])) {
          result.push_back(model_->Get(Value(booleans_[i])));
        } else {
          // Notify that everything is not fixed.
          return {};
        }
      } else {
        // This variable is not used anywhere, fix it to its lower_bound.
        //
        // TODO(user): maybe it is better to fix it to its lowest possible
        // magnitude? Also in the postsolve, this will fix non-decision
        // variables to their lower bound instead of simply leaving their domain
        // unchanged!
        result.push_back(lower_bounds_[i]);
      }
    }
    return result;
  }

  // Returns true if we should not load this constraint. This is mainly used to
  // skip constraints that correspond to a basic encoding detected by
  // ExtractEncoding().
  bool IgnoreConstraint(const ConstraintProto* ct) const {
    return gtl::ContainsKey(ct_to_ignore_, ct);
  }

  Model* model() const { return model_; }

  // Note that both these functions returns positive reference or -1.
  int GetProtoVariableFromBooleanVariable(BooleanVariable var) {
    if (var.value() >= reverse_boolean_map_.size()) return -1;
    return reverse_boolean_map_[var];
  }
  int GetProtoVariableFromIntegerVariable(IntegerVariable var) {
    if (var.value() >= reverse_integer_map_.size()) return -1;
    return reverse_integer_map_[var];
  }

  const std::vector<IntegerVariable>& GetVariableMapping() const {
    return integers_;
  }

  // For logging only, these are not super efficient.
  int NumIntegerVariables() const {
    int result = 0;
    for (const IntegerVariable var : integers_) {
      if (var != kNoIntegerVariable) result++;
    }
    return result;
  }
  int NumBooleanVariables() const {
    int result = 0;
    for (const BooleanVariable var : booleans_) {
      if (var != kNoBooleanVariable) result++;
    }
    return result;
  }

 private:
  void ExtractEncoding(const CpModelProto& model_proto);

  Model* model_;

  // Note that only the variables used by at least one constraint will be
  // created, the other will have a kNo[Integer,Interval,Boolean]VariableValue.
  std::vector<IntegerVariable> integers_;
  std::vector<IntervalVariable> intervals_;
  std::vector<BooleanVariable> booleans_;

  // Recover from a IntervalVariable/BooleanVariable its associated CpModelProto
  // index. The value of -1 is used to indicate that there is no correspondence
  // (i.e. this variable is only used internally).
  ITIVector<BooleanVariable, int> reverse_boolean_map_;
  ITIVector<IntegerVariable, int> reverse_integer_map_;

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
        return i_lit.var < o.i_lit.var;
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
      if (!encoder->VariableIsFullyEncoded(integers_[i])) {
        encoder->FullyEncodeVariable(integers_[i]);
      }
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
                                   Model* model)
    : model_(model) {
  const int num_proto_variables = model_proto.variables_size();

  // Fills lower_bounds_, this is only used in ExtractFullAssignment().
  lower_bounds_.resize(num_proto_variables, 0);
  for (int i = 0; i < num_proto_variables; ++i) {
    lower_bounds_[i] = model_proto.variables(i).domain(0);
  }

  // All [0, 1] variables always have a corresponding Boolean, even if it is
  // fixed to 0 (domain == [0,0]) or fixed to 1 (domain == [1,1]).
  booleans_.resize(num_proto_variables, kNoBooleanVariable);
  for (int i = 0; i < num_proto_variables; ++i) {
    const auto domain = ReadDomain(model_proto.variables(i));
    if (domain.size() != 1) continue;
    if (domain[0].start >= 0 && domain[0].end <= 1) {
      booleans_[i] = Add(NewBooleanVariable());
      if (booleans_[i] >= reverse_boolean_map_.size()) {
        reverse_boolean_map_.resize(booleans_[i].value() + 1, -1);
      }
      reverse_boolean_map_[booleans_[i]] = i;

      if (domain[0].start == 0 && domain[0].end == 0) {
        // Fix to false.
        Add(ClauseConstraint({sat::Literal(booleans_[i], false)}));
      } else if (domain[0].start == 1 && domain[0].end == 1) {
        // Fix to true.
        Add(ClauseConstraint({sat::Literal(booleans_[i], true)}));
      }
    }
  }

  // Compute the list of positive variable reference for which we need to
  // create an IntegerVariable.
  std::vector<int> var_to_instantiate_as_integer;
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  const bool view_all_booleans_as_integers =
      (parameters.linearization_level() > 2) ||
      (parameters.search_branching() == SatParameters::FIXED_SEARCH &&
       model_proto.search_strategy().empty());
  if (view_all_booleans_as_integers) {
    var_to_instantiate_as_integer.resize(num_proto_variables);
    for (int i = 0; i < num_proto_variables; ++i) {
      var_to_instantiate_as_integer[i] = i;
    }
  } else {
    // Compute the integer variable references used by the model.
    IndexReferences references;
    for (int c = 0; c < model_proto.constraints_size(); ++c) {
      const ConstraintProto& ct = model_proto.constraints(c);
      AddReferencesUsedByConstraint(ct, &references);
    }

    // Add the objectives and search heuristics variables that needs to be
    // referenceable as integer even if they are only used as Booleans.
    if (model_proto.has_objective()) {
      for (const int obj_var : model_proto.objective().vars()) {
        references.variables.insert(obj_var);
      }
    }
    for (const DecisionStrategyProto& strategy :
         model_proto.search_strategy()) {
      for (const int var : strategy.variables()) {
        references.variables.insert(var);
      }
    }

    // Make sure any unused variable, that is not already a Boolean is
    // considered "used". Same for optional variable that needs to be integer.
    for (int i = 0; i < num_proto_variables; ++i) {
      if (booleans_[i] == kNoBooleanVariable ||
          !model_proto.variables(i).enforcement_literal().empty()) {
        references.variables.insert(i);
      }
    }

    // We want the variable in the problem order.
    // Warning: references.variables also contains negative reference.
    var_to_instantiate_as_integer.assign(references.variables.begin(),
                                         references.variables.end());
    for (int& ref : var_to_instantiate_as_integer) {
      if (!RefIsPositive(ref)) ref = PositiveRef(ref);
    }
    gtl::STLSortAndRemoveDuplicates(&var_to_instantiate_as_integer);
  }
  integers_.resize(num_proto_variables, kNoIntegerVariable);
  for (const int i : var_to_instantiate_as_integer) {
    const auto& var_proto = model_proto.variables(i);
    integers_[i] = Add(NewIntegerVariable(ReadDomain(var_proto)));
    if (integers_[i] >= reverse_integer_map_.size()) {
      reverse_integer_map_.resize(integers_[i].value() + 1, -1);
    }
    reverse_integer_map_[integers_[i]] = i;
  }

  for (int i = 0; i < num_proto_variables; ++i) {
    // Mark the optional IntegerVariable.
    const auto& var_proto = model_proto.variables(i);
    if (!var_proto.enforcement_literal().empty()) {
      const sat::Literal l = Literal(var_proto.enforcement_literal(0));
      model_->GetOrCreate<IntegerTrail>()->MarkIntegerVariableAsOptional(
          Integer(i), l);
    }

    // Link any variable that has both views.
    if (integers_[i] == kNoIntegerVariable) continue;
    if (booleans_[i] == kNoBooleanVariable) continue;

    // Associate with corresponding integer variable.
    model_->GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
        sat::Literal(booleans_[i], true), integers_[i], IntegerValue(1));

    // This is needed so that IsFullyEncoded() returns true.
    model_->GetOrCreate<IntegerEncoder>()->FullyEncodeVariable(integers_[i]);
  }

  // Create the interval variables.
  intervals_.resize(model_proto.constraints_size(), kNoIntervalVariable);
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kInterval) {
      continue;
    }
    if (HasEnforcementLiteral(ct)) {
      const sat::Literal enforcement_literal =
          Literal(ct.enforcement_literal(0));
      // TODO(user): Fix the constant variable situation. An optional interval
      // with constant start/end or size cannot share the same constant
      // variable if it is used in non-optional situation.
      intervals_[c] = Add(NewOptionalInterval(
          Integer(ct.interval().start()), Integer(ct.interval().end()),
          Integer(ct.interval().size()), enforcement_literal));
    } else {
      intervals_[c] = Add(NewInterval(Integer(ct.interval().start()),
                                      Integer(ct.interval().end()),
                                      Integer(ct.interval().size())));
    }
  }

  // Detect the encodings (IntegerVariable <-> Booleans) present in the model.
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
        if (gtl::ContainsKey(constraint_is_finished_, ct)) continue;
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
    if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
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
  if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
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
  if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
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
    if (vars.size() == 1) {
      FullyEncode(vars.Get(0));
    }
    return true;
  } else {
    // If all variables but one are fully encoded,
    // force the last one to be fully encoded.
    int variable_not_fully_encoded;
    int num_fully_encoded = 0;
    for (const int var : ct->linear().vars()) {
      if (IsFullyEncoded(var)) {
        num_fully_encoded++;
      } else {
        variable_not_fully_encoded = var;
      }
    }
    const int num_vars = ct->linear().vars_size();
    if (num_fully_encoded == num_vars - 1) {
      FullyEncode(variable_not_fully_encoded);
      return true;
    }
    if (num_fully_encoded == num_vars) return true;

    // Register on remaining variables if not already done.
    if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
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

      if (!gtl::ContainsKey(target_map, value)) {
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

    if (!gtl::ContainsKey(value_to_literals, value)) {
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
  transitions.reserve(num_transitions);
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

// From vector of n IntegerVariables, returns an n x n matrix of Literal
// such that matrix[i][j] is the Literal corresponding to vars[i] == j.
std::vector<std::vector<Literal>> GetSquareMatrixFromIntegerVariables(
    const std::vector<IntegerVariable>& vars, ModelWithMapping* m) {
  const int n = vars.size();
  const Literal kTrueLiteral =
      m->GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
  const Literal kFalseLiteral =
      m->GetOrCreate<IntegerEncoder>()->GetFalseLiteral();
  std::vector<std::vector<Literal>> matrix(
      n, std::vector<Literal>(n, kFalseLiteral));
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (m->Get(IsFixed(vars[i]))) {
        const int value = m->Get(Value(vars[i]));
        DCHECK_LE(0, value);
        DCHECK_LT(value, n);
        matrix[i][value] = kTrueLiteral;
      } else {
        const auto encoding = m->Add(FullyEncodeVariable(vars[i]));
        for (const auto& entry : encoding) {
          const int value = entry.value.value();
          DCHECK_LE(0, value);
          DCHECK_LT(value, n);
          matrix[i][value] = entry.literal;
        }
      }
    }
  }
  return matrix;
}

void LoadCircuitConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const auto& circuit = ct.circuit();
  if (circuit.tails().empty()) return;

  std::vector<int> tails(circuit.tails().begin(), circuit.tails().end());
  std::vector<int> heads(circuit.heads().begin(), circuit.heads().end());
  std::vector<Literal> literals = m->Literals(circuit.literals());
  const int num_nodes = ReindexArcs(&tails, &heads, &literals);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals));
}

void LoadRoutesConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const auto& routes = ct.routes();
  if (routes.tails().empty()) return;

  std::vector<int> tails(routes.tails().begin(), routes.tails().end());
  std::vector<int> heads(routes.heads().begin(), routes.heads().end());
  std::vector<Literal> literals = m->Literals(routes.literals());
  const int num_nodes = ReindexArcs(&tails, &heads, &literals);
  m->Add(SubcircuitConstraint(num_nodes, tails, heads, literals,
                              /*multiple_subcircuit_through_zero=*/true));
}

void LoadCircuitCoveringConstraint(const ConstraintProto& ct,
                                   ModelWithMapping* m) {
  const std::vector<IntegerVariable> nexts =
      m->Integers(ct.circuit_covering().nexts());
  const std::vector<std::vector<Literal>> graph =
      GetSquareMatrixFromIntegerVariables(nexts, m);
  const std::vector<int> distinguished(
      ct.circuit_covering().distinguished_nodes().begin(),
      ct.circuit_covering().distinguished_nodes().end());
  m->Add(ExactlyOnePerRowAndPerColumn(graph));
  m->Add(CircuitCovering(graph, distinguished));
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
  return absl::StrCat(input.substr(0, half), " ... ",
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
  absl::StrAppend(&result, "Model '", model_proto.name(), "':\n");

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    absl::StrAppend(
        &result, "Search strategy: on ", strategy.variables_size(),
        " variables, ",
        ProtoEnumToString<DecisionStrategyProto::VariableSelectionStrategy>(
            strategy.variable_selection_strategy()),
        ", ",
        ProtoEnumToString<DecisionStrategyProto::DomainReductionStrategy>(
            strategy.domain_reduction_strategy()),
        "\n");
  }

  absl::StrAppend(&result, "#Variables: ", model_proto.variables_size(), "\n");
  if (num_vars_per_domains.size() < 20) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp = absl::StrCat(" - ", entry.second, " in ",
                                       IntervalsAsString(entry.first), "\n");
      absl::StrAppend(&result, Summarize(temp));
    }
  } else {
    int64 max_complexity = 0;
    int64 min = kint64max;
    int64 max = kint64min;
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.front().start);
      max = std::max(max, entry.first.back().end);
      max_complexity =
          std::max(max_complexity, static_cast<int64>(entry.first.size()));
    }
    absl::StrAppend(&result, " - ", num_vars_per_domains.size(),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        absl::StrCat(" - ", num_constants, " constants in {",
                     absl::StrJoin(constant_values, ","), "} \n");
    absl::StrAppend(&result, Summarize(temp));
  }

  std::vector<std::string> constraints;
  constraints.reserve(num_constraints_by_type.size());
  for (const auto entry : num_constraints_by_type) {
    constraints.push_back(
        absl::StrCat("#", ConstraintCaseName(entry.first), ": ", entry.second,
                     " (", num_reif_constraints_by_type[entry.first],
                     " with enforcement literal)"));
  }
  std::sort(constraints.begin(), constraints.end());
  absl::StrAppend(&result, absl::StrJoin(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response) {
  std::string result;
  absl::StrAppend(&result, "CpSolverResponse:");
  absl::StrAppend(&result, "\nstatus: ",
                  ProtoEnumToString<CpSolverStatus>(response.status()));

  // We special case the pure-decision problem for clarity.
  //
  // TODO(user): This test is not ideal for the corner case where the status is
  // still UNKNOWN yet we already know that if there is a solution, then its
  // objective is zero...
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.objective_value() == 0 && response.best_objective_bound() == 0) {
    absl::StrAppend(&result, "\nobjective: NA");
    absl::StrAppend(&result, "\nbest_bound: NA");
  } else {
    absl::StrAppend(&result, "\nobjective: ",
                    absl::LegacyPrecision(response.objective_value()));
    absl::StrAppend(&result, "\nbest_bound: ",
                    absl::LegacyPrecision(response.best_objective_bound()));
  }

  absl::StrAppend(&result, "\nbooleans: ", response.num_booleans());
  absl::StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  absl::StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  absl::StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  absl::StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());
  absl::StrAppend(&result,
                  "\nwalltime: ", absl::LegacyPrecision(response.wall_time()));
  absl::StrAppend(&result,
                  "\nusertime: ", absl::LegacyPrecision(response.user_time()));
  absl::StrAppend(&result, "\ndeterministic_time: ",
                  absl::LegacyPrecision(response.deterministic_time()));
  absl::StrAppend(&result, "\n");
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
    case ConstraintProto::ConstraintProto::kRoutes:
      LoadRoutesConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCircuitCovering:
      LoadCircuitCoveringConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInverse:
      LoadInverseConstraint(ct, m);
      return true;
    default:
      return false;
  }
}

void FillSolutionInResponse(const CpModelProto& model_proto,
                            const ModelWithMapping& m,
                            CpSolverResponse* response) {
  const std::vector<int64> solution = m.ExtractFullAssignment();
  response->set_status(CpSolverStatus::MODEL_SAT);
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();
  if (!solution.empty()) {
    DCHECK(SolutionIsFeasible(model_proto, solution));
    for (const int64 value : solution) response->add_solution(value);
  } else {
    // Not all variables are fixed.
    // We fill instead the lb/ub of each variables.
    const auto& assignment = m.model()->Get<Trail>()->Assignment();
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (m.IsInteger(i)) {
        response->add_solution_lower_bounds(m.Get(LowerBound(m.Integer(i))));
        response->add_solution_upper_bounds(m.Get(UpperBound(m.Integer(i))));
      } else if (m.IsBoolean(i)) {
        if (assignment.VariableIsAssigned(m.Boolean(i))) {
          const int value = m.Get(Value(m.Boolean(i)));
          response->add_solution_lower_bounds(value);
          response->add_solution_upper_bounds(value);
        } else {
          response->add_solution_lower_bounds(0);
          response->add_solution_upper_bounds(1);
        }
      } else {
        // Without presolve, some variable may never be used.
        response->add_solution_lower_bounds(model_proto.variables(i).domain(0));
        response->add_solution_upper_bounds(model_proto.variables(i).domain(
            model_proto.variables(i).domain_size() - 1));
      }
    }
  }
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
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
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  // Create a new variable and link it with the linear terms.
  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(terms, model);
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

// Add a linear relaxation of the CP constraint to the set of linear
// constraints. The highest linearization_level is, the more types of constraint
// we encode. At level zero, we only encode non-reified linear constraints.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
void TryToLinearizeConstraint(
    const CpModelProto& model_proto, const ConstraintProto& ct,
    ModelWithMapping* m, int linearization_level,
    std::vector<LinearConstraint>* linear_constraints) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
    if (linearization_level < 2) return;
    if (HasEnforcementLiteral(ct)) return;
    LinearConstraintBuilder lc(m->model(), 1.0, kInfinity);
    for (const int ref : ct.bool_or().literals()) {
      CHECK(lc.AddLiteralTerm(m->Literal(ref), 1.0));
    }
    linear_constraints->push_back(lc.Build());
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kBoolAnd) {
    if (linearization_level < 2) return;
    if (!HasEnforcementLiteral(ct)) return;
    const Literal e = m->Literal(ct.enforcement_literal(0));
    for (const int ref : ct.bool_and().literals()) {
      // We linearize (e implies literal) as (e - literals <= 0).
      LinearConstraintBuilder lc(m->model(), -kInfinity, 0.0);
      CHECK(lc.AddLiteralTerm(e, 1.0));
      CHECK(lc.AddLiteralTerm(m->Literal(ref), -1.0));
      linear_constraints->push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_max().target();
    for (const int var : ct.int_max().vars()) {
      // This deal with the corner case X = max(X, Y, Z, ..) !
      // Note that this can be presolved into X >= Y, X >= Z, ...
      if (target == var) continue;
      LinearConstraintBuilder lc(m->model(), -kInfinity, 0.0);
      lc.AddTerm(m->Integer(var), 1.0);
      lc.AddTerm(m->Integer(target), -1.0);
      linear_constraints->push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMin) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_min().target();
    for (const int var : ct.int_min().vars()) {
      if (target == var) continue;
      LinearConstraintBuilder lc(m->model(), -kInfinity, 0.0);
      lc.AddTerm(m->Integer(target), 1.0);
      lc.AddTerm(m->Integer(var), -1.0);
      linear_constraints->push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    // Note that we ignore the holes in the domain.
    //
    // TODO(user): In LoadLinearConstraint() we already created intermediate
    // Booleans for each disjoint interval, we should reuse them here if
    // possible.
    const int64 min = ct.linear().domain(0);
    const int64 max = ct.linear().domain(ct.linear().domain_size() - 1);
    if (min == kint64min && max == kint64max) return;

    if (!HasEnforcementLiteral(ct)) {
      LinearConstraintBuilder lc(m->model(),
                                 (min == kint64min) ? -kInfinity : min,
                                 (max == kint64max) ? kInfinity : max);
      for (int i = 0; i < ct.linear().vars_size(); i++) {
        const int ref = ct.linear().vars(i);
        const int64 coeff = ct.linear().coeffs(i);
        lc.AddTerm(m->Integer(ref), coeff);
      }
      linear_constraints->push_back(lc.Build());
      return;
    }

    // Reified version.
    if (linearization_level < 3) return;

    // Compute the implied bounds on the linear expression.
    double implied_lb = 0.0;
    double implied_ub = 0.0;
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      int ref = ct.linear().vars(i);
      double coeff = static_cast<double>(ct.linear().coeffs(i));
      if (!RefIsPositive(ref)) {
        ref = PositiveRef(ref);
        coeff -= coeff;
      }
      const IntegerVariableProto& p = model_proto.variables(ref);
      if (coeff > 0.0) {
        implied_lb += coeff * static_cast<double>(p.domain(0));
        implied_ub +=
            coeff * static_cast<double>(p.domain(p.domain_size() - 1));
      } else {
        implied_lb +=
            coeff * static_cast<double>(p.domain(p.domain_size() - 1));
        implied_ub += coeff * static_cast<double>(p.domain(0));
      }
    }
    const int e = ct.enforcement_literal(0);
    if (min != kint64min) {
      // (e => terms >= min) <=> terms >= implied_lb + e * (min - implied_lb);
      LinearConstraintBuilder lc(m->model(), implied_lb, kInfinity);
      for (int i = 0; i < ct.linear().vars_size(); i++) {
        const int ref = ct.linear().vars(i);
        const int64 coeff = ct.linear().coeffs(i);
        lc.AddTerm(m->Integer(ref), coeff);
      }
      CHECK(lc.AddLiteralTerm(m->Literal(e), implied_lb - min));
      linear_constraints->push_back(lc.Build());
    }
    if (max != kint64max) {
      // (e => terms <= max) <=> terms <= implied_ub + e * (max - implied_ub)
      LinearConstraintBuilder lc(m->model(), -kInfinity, implied_ub);
      for (int i = 0; i < ct.linear().vars_size(); i++) {
        const int ref = ct.linear().vars(i);
        const int64 coeff = ct.linear().coeffs(i);
        lc.AddTerm(m->Integer(ref), coeff);
      }
      CHECK(lc.AddLiteralTerm(m->Literal(e), implied_ub - max));
      linear_constraints->push_back(lc.Build());
    }
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kCircuit) {
    if (HasEnforcementLiteral(ct)) return;
    const int num_arcs = ct.circuit().literals_size();
    CHECK_EQ(num_arcs, ct.circuit().tails_size());
    CHECK_EQ(num_arcs, ct.circuit().heads_size());

    // Each node must have exactly one incoming and one outgoing arc (note that
    // it can be the unique self-arc of this node too).
    std::map<int, std::unique_ptr<LinearConstraintBuilder>>
        incoming_arc_constraints;
    std::map<int, std::unique_ptr<LinearConstraintBuilder>>
        outgoing_arc_constraints;
    auto get_constraint =
        [m](std::map<int, std::unique_ptr<LinearConstraintBuilder>>* node_map,
            int node) {
          if (!gtl::ContainsKey(*node_map, node)) {
            (*node_map)[node].reset(
                new LinearConstraintBuilder(m->model(), 1, 1));
          }
          return (*node_map)[node].get();
        };
    for (int i = 0; i < num_arcs; i++) {
      const Literal arc = m->Literal(ct.circuit().literals(i));
      const int tail = ct.circuit().tails(i);
      const int head = ct.circuit().heads(i);

      // Make sure this literal has a view.
      m->Add(NewIntegerVariableFromLiteral(arc));

      CHECK(get_constraint(&outgoing_arc_constraints, tail)
                ->AddLiteralTerm(arc, 1.0));
      CHECK(get_constraint(&incoming_arc_constraints, head)
                ->AddLiteralTerm(arc, 1.0));
    }
    for (const auto* node_map :
         {&outgoing_arc_constraints, &incoming_arc_constraints}) {
      for (const auto& entry : *node_map) {
        if (entry.second->size() > 1) {
          linear_constraints->push_back(entry.second->Build());
        }
      }
    }
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kElement) {
    const IntegerVariable index = m->Integer(ct.element().index());
    const IntegerVariable target = m->Integer(ct.element().target());
    const std::vector<IntegerVariable> vars = m->Integers(ct.element().vars());

    // We only relax the case where all the vars are constant.
    // target = sum (index == i) * fixed_vars[i].
    LinearConstraintBuilder constraint(m->model(), 0.0, 0.0);
    constraint.AddTerm(target, -1.0);
    for (const auto literal_value : m->Add(FullyEncodeVariable((index)))) {
      const IntegerVariable var = vars[literal_value.value.value()];
      if (!m->Get(IsFixed(var))) return;

      // Make sure this literal has a view.
      m->Add(NewIntegerVariableFromLiteral(literal_value.literal));
      CHECK(
          constraint.AddLiteralTerm(literal_value.literal, m->Get(Value(var))));
    }

    linear_constraints->push_back(constraint.Build());
  }
}

void TryToAddCutGenerators(const CpModelProto& model_proto,
                           const ConstraintProto& ct, ModelWithMapping* m,
                           std::vector<CutGenerator>* cut_generators) {
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit) {
    std::vector<int> tails(ct.circuit().tails().begin(),
                           ct.circuit().tails().end());
    std::vector<int> heads(ct.circuit().heads().begin(),
                           ct.circuit().heads().end());
    std::vector<Literal> literals = m->Literals(ct.circuit().literals());
    const int num_nodes = ReindexArcs(&tails, &heads, &literals);

    std::vector<IntegerVariable> vars;
    vars.reserve(literals.size());
    for (const Literal& literal : literals) {
      vars.push_back(m->Add(NewIntegerVariableFromLiteral(literal)));
    }

    cut_generators->push_back(CreateStronglyConnectedGraphCutGenerator(
        num_nodes, tails, heads, vars));
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kRoutes) {
    std::vector<int> tails;
    std::vector<int> heads;
    std::vector<IntegerVariable> vars;
    int num_nodes = 0;
    auto* encoder = m->GetOrCreate<IntegerEncoder>();
    for (int i = 0; i < ct.routes().tails_size(); ++i) {
      const IntegerVariable var =
          encoder->GetLiteralView(m->Literal(ct.routes().literals(i)));
      if (var == kNoIntegerVariable) return;

      vars.push_back(var);
      tails.push_back(ct.routes().tails(i));
      heads.push_back(ct.routes().heads(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
    }
    if (ct.routes().demands().empty() || ct.routes().capacity() == 0) {
      cut_generators->push_back(CreateStronglyConnectedGraphCutGenerator(
          num_nodes, tails, heads, vars));
    } else {
      const std::vector<int64> demands(ct.routes().demands().begin(),
                                       ct.routes().demands().end());
      cut_generators->push_back(CreateCVRPCutGenerator(
          num_nodes, tails, heads, vars, demands, ct.routes().capacity()));
    }
  }
}

}  // namespace

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(const CpModelProto& model_proto,
                                 int linearization_level, ModelWithMapping* m) {
  // Linearize the constraints.
  IndexReferences refs;
  std::vector<LinearConstraint> linear_constraints;
  std::vector<CutGenerator> cut_generators;
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  for (const auto& ct : model_proto.constraints()) {
    // We linearize fully/partially encoded variable differently, so we just
    // skip all these constraint that corresponds to these encoding.
    if (m->IgnoreConstraint(&ct)) continue;

    // Make sure the literal from a circuit constraint always have a view.
    if (linearization_level > 1) {
      if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit) {
        for (const int ref : ct.circuit().literals()) {
          m->Add(NewIntegerVariableFromLiteral(m->Literal(ref)));
        }
      }
    }

    // For now, we skip any constraint with literals that do not have an integer
    // view. Ideally it should be up to the constraint to decide if creating a
    // view is worth it.
    //
    // TODO(user): It should be possible to speed this up if needed.
    refs.variables.clear();
    refs.literals.clear();
    refs.intervals.clear();
    AddReferencesUsedByConstraint(ct, &refs);
    bool ok = true;
    for (const int literal_ref : refs.literals) {
      const Literal literal = m->Literal(literal_ref);
      if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
          encoder->GetLiteralView(literal.Negated()) == kNoIntegerVariable) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    TryToLinearizeConstraint(model_proto, ct, m, linearization_level,
                             &linear_constraints);

    // For now these are only useful on problem with circuit. They can help
    // a lot on complex problems, but they also slow down simple ones.
    if (linearization_level > 1) {
      TryToAddCutGenerators(model_proto, ct, m, &cut_generators);
    }
  }

  // Linearize the encoding of variable that are fully encoded in the proto.
  int num_full_encoding_relaxations = 0;
  const int old_size = linear_constraints.size();
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (m->IsBoolean(i)) continue;

    const IntegerVariable var = m->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // TODO(user): This different encoding for the partial variable might be
    // better (less LP constraints), but we do need more investigation to
    // decide.
    if (/* DISABLES CODE */ false) {
      AppendPartialEncodingRelaxation(var, *(m->model()), &linear_constraints);
      continue;
    }

    if (encoder->VariableIsFullyEncoded(var)) {
      if (AppendFullEncodingRelaxation(var, *(m->model()),
                                       &linear_constraints)) {
        ++num_full_encoding_relaxations;
      }
    } else {
      AppendPartialGreaterThanEncodingRelaxation(var, *(m->model()),
                                                 &linear_constraints);
    }
  }

  // Remove size one LP constraints, they are not useful.
  const int num_extra_constraints = linear_constraints.size() - old_size;
  {
    int new_size = 0;
    for (int i = 0; i < linear_constraints.size(); ++i) {
      if (linear_constraints[i].vars.size() == 1) continue;
      std::swap(linear_constraints[new_size++], linear_constraints[i]);
    }
    linear_constraints.resize(new_size);
  }

  VLOG(1) << "num_full_encoding_relaxations: " << num_full_encoding_relaxations;
  VLOG(1) << "num_integer_encoding_constraints: " << num_extra_constraints;
  VLOG(1) << linear_constraints.size() << " constraints in the LP relaxation.";
  VLOG(1) << cut_generators.size() << " cuts generators.";

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_lp_constraints),
  // variable nodes by [num_lp_constraints..num_lp_constraints+num_variables).
  //
  // TODO(user): look into biconnected components.
  const int num_lp_constraints = linear_constraints.size();
  const int num_lp_cut_generators = cut_generators.size();
  const int num_integer_variables =
      m->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();
  ConnectedComponents<int, int> components;
  components.Init(num_lp_constraints + num_lp_cut_generators +
                  num_integer_variables);
  auto get_constraint_index = [](int ct_index) { return ct_index; };
  auto get_cut_generator_index = [num_lp_constraints](int cut_index) {
    return num_lp_constraints + cut_index;
  };
  auto get_var_index = [num_lp_constraints,
                        num_lp_cut_generators](IntegerVariable var) {
    return num_lp_constraints + num_lp_cut_generators + var.value();
  };
  for (int i = 0; i < num_lp_constraints; i++) {
    for (const IntegerVariable var : linear_constraints[i].vars) {
      components.AddArc(get_constraint_index(i), get_var_index(var));
    }
  }
  for (int i = 0; i < num_lp_cut_generators; ++i) {
    for (const IntegerVariable var : cut_generators[i].vars) {
      components.AddArc(get_cut_generator_index(i), get_var_index(var));
    }
  }

  std::map<int, int> components_to_size;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    components_to_size[id] += 1;
  }
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    components_to_size[id] += 1;
  }

  // Make sure any constraint that touch the objective is not discarded even
  // if it is the only one in its component. This is important to propagate
  // as much as possible the objective bound by using any bounds the LP give
  // us on one of its components. This is critical on the zephyrus problems for
  // instance.
  for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
    const IntegerVariable var = m->Integer(model_proto.objective().vars(i));
    const int id = components.GetClassRepresentative(get_var_index(var));
    components_to_size[id] += 1;
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::map<int, LinearProgrammingConstraint*> representative_to_lp_constraint;
  std::vector<LinearProgrammingConstraint*> lp_constraints;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    if (components_to_size[id] <= 1) continue;
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->model()->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }

    // Load the constraint.
    LinearProgrammingConstraint* lp = representative_to_lp_constraint[id];
    const auto lp_constraint = lp->CreateNewConstraint(
        linear_constraints[i].lb, linear_constraints[i].ub);
    for (int j = 0; j < linear_constraints[i].vars.size(); ++j) {
      lp->SetCoefficient(lp_constraint, linear_constraints[i].vars[j],
                         linear_constraints[i].coeffs[j]);
    }
  }

  // Dispatch every cut generator to its LinearProgrammingConstraint.
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->model()->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }
    LinearProgrammingConstraint* lp = representative_to_lp_constraint[id];
    lp->AddCutGenerator(std::move(cut_generators[i]));
  }

  // Add the objective.
  std::map<int, std::vector<std::pair<IntegerVariable, int64>>>
      representative_to_cp_terms;
  std::vector<std::pair<IntegerVariable, int64>> top_level_cp_terms;
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var = m->Integer(model_proto.objective().vars(i));
      const int64 coeff = model_proto.objective().coeffs(i);
      const int id = components.GetClassRepresentative(get_var_index(var));
      if (gtl::ContainsKey(representative_to_lp_constraint, id)) {
        representative_to_lp_constraint[id]->SetObjectiveCoefficient(var,
                                                                     coeff);
        representative_to_cp_terms[id].push_back(std::make_pair(var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (const auto& it : representative_to_cp_terms) {
      const int id = it.first;
      LinearProgrammingConstraint* lp =
          gtl::FindOrDie(representative_to_lp_constraint, id);
      const std::vector<std::pair<IntegerVariable, int64>>& terms = it.second;
      const IntegerVariable sub_obj_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(terms, m->model());
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  const IntegerVariable main_objective_var =
      GetOrCreateVariableGreaterOrEqualToSumOf(top_level_cp_terms, m->model());

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (auto* lp_constraint : lp_constraints) {
    VLOG(1) << "LP constraint: " << lp_constraint->DimensionString() << ".";
    lp_constraint->RegisterWith(m->model());
  }

  VLOG(1) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  return main_objective_var;
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
  std::vector<std::function<void(const CpSolverResponse& response)>> observers;
};

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->observers.push_back(observer);
  };
}

#if !defined(__PORTABLE_PLATFORM__)
// TODO(user): Support it on android.
std::function<SatParameters(Model*)> NewSatParameters(const std::string& params) {
  sat::SatParameters parameters;
  if (!params.empty()) {
    CHECK(google::protobuf::TextFormat::ParseFromString(params, &parameters)) << params;
  }
  return NewSatParameters(parameters);
}
#endif  // __PORTABLE_PLATFORM__

std::function<SatParameters(Model*)> NewSatParameters(
    const sat::SatParameters& parameters) {
  return [=](Model* model) {
    model->GetOrCreate<SatSolver>()->SetParameters(parameters);
    return parameters;
  };
}

namespace {

// Because we also use this function for postsolve, we call it with
// is_real_solve set to true and avoid doing non-useful work in this case.
CpSolverResponse SolveCpModelInternal(
    const CpModelProto& model_proto, bool is_real_solve,
    const std::function<void(const CpSolverResponse&)>&
        external_solution_observer,
    Model* model) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // Initialize a default invalid response.
  CpSolverResponse response;
  response.set_status(CpSolverStatus::MODEL_INVALID);

  auto fill_response_statistics = [&]() {
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
        model->Get<TimeLimit>()->GetElapsedDeterministicTime());
  };

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  ModelWithMapping m(model_proto, model);

  // Force some variables to be fully encoded.
  FullEncodingFixedPointComputer fixpoint(&m);
  for (const ConstraintProto& ct : model_proto.constraints()) {
    fixpoint.PropagateFullEncoding(&ct);
  }
  fixpoint.ComputeFixedPoint();

  // Load the constraints.
  std::set<std::string> unsupported_types;
  int num_ignored_constraints = 0;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (m.IgnoreConstraint(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    if (!LoadConstraint(ct, &m)) {
      unsupported_types.insert(ConstraintCaseName(ct.constraint_case()));
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call Propagate() manually here. Note that we do not do
    // that in the postsolve as there is some corner case where propagating
    // after each new constraint can have a quadratic behavior.
    //
    // Note that we only do that in debug mode as this can be really slow on
    // certain types of problems with millions of constraints.
    if (DEBUG_MODE) {
      model->GetOrCreate<SatSolver>()->Propagate();
      Trail* trail = model->GetOrCreate<Trail>();
      const int old_num_fixed = trail->Index();
      if (trail->Index() > old_num_fixed) {
        VLOG(1) << "Constraint fixed " << trail->Index() - old_num_fixed
                << " Boolean variable(s): " << ProtobufDebugString(ct);
      }
    }
    if (model->GetOrCreate<SatSolver>()->IsModelUnsat()) {
      VLOG(1) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ProtobufDebugString(ct);
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

  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  model->GetOrCreate<SatSolver>()->Propagate();

  // Auto detect "at least one of" constraints in the PrecedencesPropagator.
  // Note that we do that before we finish loading the problem (objective and LP
  // relaxation), because propagation will be faster at this point and it should
  // be enough for the purpose of this auto-detection.
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  if (model->Mutable<PrecedencesPropagator>() != nullptr &&
      parameters.auto_detect_greater_than_at_least_one_of()) {
    model->Mutable<PrecedencesPropagator>()
        ->AddGreaterThanAtLeastOneOfConstraints(model);
    model->GetOrCreate<SatSolver>()->Propagate();
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (is_real_solve && parameters.linearization_level() > 0) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var =
        AddLPConstraints(model_proto, parameters.linearization_level(), &m);
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(std::make_pair(m.Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core()) {
      objective_var = GetOrCreateVariableWithTightBound(terms, model);
    } else {
      objective_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(terms, m.model());
    }
  }

  // Intersect the objective domain with the given one if any.
  if (!model_proto.objective().domain().empty()) {
    const auto user_domain = ReadDomain(model_proto.objective());
    const auto automatic_domain =
        model->GetOrCreate<IntegerTrail>()->InitialVariableDomain(
            objective_var);
    VLOG(1) << "Objective offset:" << model_proto.objective().offset()
            << " scaling_factor:" << model_proto.objective().scaling_factor();
    VLOG(1) << "Automatic internal objective domain: " << automatic_domain;
    VLOG(1) << "User specified internal objective domain: " << user_domain;
    CHECK_NE(objective_var, kNoIntegerVariable);
    const bool ok = model->GetOrCreate<IntegerTrail>()->UpdateInitialDomain(
        objective_var, user_domain);
    if (!ok) {
      VLOG(1) << "UNSAT due to the objective domain.";
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    }

    // Make sure the sum take a value inside the objective domain by adding
    // the other side: objective <= sum terms.
    //
    // TODO(user): Use a better condition to detect when this is not useful.
    if (user_domain != automatic_domain) {
      std::vector<IntegerVariable> vars;
      std::vector<int64> coeffs;
      const CpObjectiveProto& obj = model_proto.objective();
      for (int i = 0; i < obj.vars_size(); ++i) {
        vars.push_back(m.Integer(obj.vars(i)));
        coeffs.push_back(obj.coeffs(i));
      }
      vars.push_back(objective_var);
      coeffs.push_back(-1);
      model->Add(WeightedSumGreaterOrEqual(vars, coeffs, 0));
    }
  }

  // Note that we do one last propagation at level zero once all the constraints
  // where added.
  model->GetOrCreate<SatSolver>()->Propagate();

  // Probing Boolean variables. Because we don't have a good deterministic time
  // yet in the non-Boolean part of the problem, we disable it by default.
  //
  // TODO(user): move this inside the presolve somehow, and exploit the variable
  // detected to be equivalent to each other!
  if (/* DISABLES CODE */ false && is_real_solve) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    SatPostsolver postsolver(sat_solver->NumVariables());
    ITIVector<LiteralIndex, LiteralIndex> equiv_map;
    ProbeAndFindEquivalentLiteral(sat_solver, &postsolver, nullptr, &equiv_map);
  }

  // Initialize the search strategy function.
  std::function<LiteralIndex()> next_decision = ConstructSearchStrategy(
      model_proto, m.GetVariableMapping(), objective_var, model);
  if (VLOG_IS_ON(2)) {
    next_decision = InstrumentSearchStrategy(
        model_proto, m.GetVariableMapping(), next_decision, model);
  }

  // Solve.
  int num_solutions = 0;
  SatSolver::Status status;

  // Load solution hint.
  // We follow it and allow for a tiny number of conflicts before giving up.
  //
  // TODO(user): Double check that when this get a feasible solution, it is
  // properly used in the various optimization algorithm. some of them will
  // reset the solver to its initial state, but then with phase saving it
  // should still follow the same path again.
  if (model_proto.has_solution_hint()) {
    LOG(INFO) << "Loading solution hint ... ";
    const int64 old_conflict_limit = parameters.max_number_of_conflicts();
    model->GetOrCreate<SatParameters>()->set_max_number_of_conflicts(10);
    std::vector<BooleanOrIntegerVariable> vars;
    std::vector<IntegerValue> values;
    for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
      const int ref = model_proto.solution_hint().vars(i);
      CHECK(RefIsPositive(ref));
      BooleanOrIntegerVariable var;
      if (m.IsBoolean(ref)) {
        var.bool_var = m.Boolean(ref);
      } else {
        var.int_var = m.Integer(ref);
      }
      vars.push_back(var);
      values.push_back(IntegerValue(model_proto.solution_hint().values(i)));
    }
    std::vector<std::function<LiteralIndex()>> decision_policies = {
        SequentialSearch({FollowHint(vars, values, model),
                          SatSolverHeuristic(model), next_decision})};
    auto no_restart = []() { return false; };
    status =
        SolveProblemWithPortfolioSearch(decision_policies, {no_restart}, model);
    if (status == SatSolver::Status::MODEL_SAT) {
      LOG(INFO) << "Solution hint: success, feasible solution found.";
    } else {
      LOG(INFO) << "Solution: failure, no feasible solution found.";
    }
    model->GetOrCreate<SatParameters>()->set_max_number_of_conflicts(
        old_conflict_limit);
  }

  if (!model_proto.has_objective()) {
    while (true) {
      status = SolveIntegerProblemWithLazyEncoding(
          /*assumptions=*/{}, next_decision, model);
      if (status != SatSolver::Status::MODEL_SAT) break;

      // TODO(user): add all solutions to the response? or their count?
      ++num_solutions;
      FillSolutionInResponse(model_proto, m, &response);
      fill_response_statistics();
      external_solution_observer(response);

      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack());
    }
    if (num_solutions > 0) {
      if (status == SatSolver::Status::MODEL_UNSAT) {
        response.set_all_solutions_were_found(true);
      }
      status = SatSolver::Status::MODEL_SAT;
    }
  } else {
    // Optimization problem.
    const CpObjectiveProto& obj = model_proto.objective();
    VLOG(1) << obj.vars_size() << " terms in the proto objective.";
    VLOG(1) << "Initial num_bool: " << model->Get<SatSolver>()->NumVariables();
    const auto solution_observer =
        [&model_proto, &response, &num_solutions, &obj, &m,
         &external_solution_observer, objective_var,
         &fill_response_statistics](const Model& sat_model) {
          num_solutions++;
          FillSolutionInResponse(model_proto, m, &response);
          fill_response_statistics();
          int64 objective_value = 0;
          for (int i = 0; i < model_proto.objective().vars_size(); ++i) {
            objective_value += model_proto.objective().coeffs(i) *
                               sat_model.Get(LowerBound(
                                   m.Integer(model_proto.objective().vars(i))));
          }
          response.set_objective_value(
              ScaleObjectiveValue(obj, objective_value));
          external_solution_observer(response);
          VLOG(1) << "Solution #" << num_solutions
                  << " obj:" << response.objective_value()
                  << " num_bool:" << sat_model.Get<SatSolver>()->NumVariables();
        };

    if (parameters.optimize_with_core()) {
      std::vector<IntegerVariable> linear_vars;
      std::vector<IntegerValue> linear_coeffs;
      ExtractLinearObjective(model_proto, &m, &linear_vars, &linear_coeffs);
      if (parameters.optimize_with_max_hs()) {
        status = MinimizeWithHittingSetAndLazyEncoding(
            VLOG_IS_ON(1), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
      } else {
        status = MinimizeWithCoreAndLazyEncoding(
            VLOG_IS_ON(1), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
      }
    } else {
      if (parameters.binary_search_num_conflicts() >= 0) {
        RestrictObjectiveDomainWithBinarySearch(objective_var, next_decision,
                                                solution_observer, model);
      }
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          /*log_info=*/false, objective_var, next_decision, solution_observer,
          model);
      if (num_solutions > 0 && status == SatSolver::MODEL_UNSAT) {
        status = SatSolver::MODEL_SAT;
      }
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
  fill_response_statistics();
  return response;
}

// TODO(user): If this ever shows up in the profile, we could avoid copying
// the mapping_proto if we are careful about how we modify the variable domain
// before postsolving it.
void PostsolveResponse(const CpModelProto& model_proto,
                       CpModelProto mapping_proto,
                       const std::vector<int>& postsolve_mapping,
                       CpSolverResponse* response) {
  if (response->status() != CpSolverStatus::MODEL_SAT &&
      response->status() != CpSolverStatus::OPTIMAL) {
    return;
  }
  // Postsolve.
  for (int i = 0; i < response->solution_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain(response->solution(i));
    var_proto->add_domain(response->solution(i));
  }
  for (int i = 0; i < response->solution_lower_bounds_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    FillDomain(
        IntersectionOfSortedDisjointIntervals(
            ReadDomain(*var_proto), {{response->solution_lower_bounds(i),
                                      response->solution_upper_bounds(i)}}),
        var_proto);
  }

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  Model postsolve_model;
  {
    SatParameters params;
    params.set_linearization_level(0);
    postsolve_model.Add(operations_research::sat::NewSatParameters(params));
  }
  const CpSolverResponse postsolve_response = SolveCpModelInternal(
      mapping_proto, false, [](const CpSolverResponse&) {}, &postsolve_model);
  CHECK_EQ(postsolve_response.status(), CpSolverStatus::MODEL_SAT);

  // We only copy the solution from the postsolve_response to the response.
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();
  if (!postsolve_response.solution().empty()) {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution(postsolve_response.solution(i));
    }
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response->solution().begin(),
                                                response->solution().end())));
  } else {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution_lower_bounds(
          postsolve_response.solution_lower_bounds(i));
      response->add_solution_upper_bounds(
          postsolve_response.solution_upper_bounds(i));
    }
  }
}

CpSolverResponse SolvePureSatModel(const CpModelProto& model_proto,
                                   Model* model) {
  std::unique_ptr<SatSolver> solver(new SatSolver());
  SatParameters parameters = *model->GetOrCreate<SatParameters>();
  parameters.set_log_search_progress(true);
  solver->SetParameters(parameters);
  model->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(parameters);

  // Create a DratProofHandler?
  std::unique_ptr<DratProofHandler> drat_proof_handler;
#if !defined(__PORTABLE_PLATFORM__)
  if (!FLAGS_drat_output.empty() || FLAGS_drat_check) {
    if (!FLAGS_drat_output.empty()) {
      File* output;
      CHECK_OK(file::Open(FLAGS_drat_output, "w", &output, file::Defaults()));
      drat_proof_handler.reset(new DratProofHandler(
          /*in_binary_format=*/false, output, FLAGS_drat_check));
    } else {
      drat_proof_handler.reset(new DratProofHandler());
    }
    solver->SetDratProofHandler(drat_proof_handler.get());
  }
#endif  // __PORTABLE_PLATFORM__

  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  auto get_literal = [](int ref) {
    if (ref >= 0) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };

  std::vector<Literal> temp;
  const int num_variables = model_proto.variables_size();
  solver->SetNumVariables(num_variables);
  if (drat_proof_handler != nullptr) {
    drat_proof_handler->SetNumVariables(num_variables);
  }

  for (const ConstraintProto& ct : model_proto.constraints()) {
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolAnd: {
        // a => b
        const Literal not_a = get_literal(ct.enforcement_literal(0)).Negated();
        for (const int ref : ct.bool_and().literals()) {
          const Literal b = get_literal(ref);
          solver->AddProblemClause({not_a, b});
          if (drat_proof_handler != nullptr) {
            drat_proof_handler->AddProblemClause({not_a, b});
          }
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kBoolOr:
        temp.clear();
        for (const int ref : ct.bool_or().literals()) {
          temp.push_back(get_literal(ref));
        }
        solver->AddProblemClause(temp);
        if (drat_proof_handler != nullptr) {
          drat_proof_handler->AddProblemClause(temp);
        }
        break;
      default:
        LOG(FATAL) << "Not supported";
    }
  }

  // Deal with fixed variables.
  for (int ref = 0; ref < num_variables; ++ref) {
    const auto domain = ReadDomain(model_proto.variables(ref));
    CHECK_EQ(domain.size(), 1);
    if (domain[0].start == domain[0].end) {
      const Literal ref_literal =
          domain[0].start == 0 ? get_literal(ref).Negated() : get_literal(ref);
      solver->AddUnitClause(ref_literal);
      if (drat_proof_handler != nullptr) {
        drat_proof_handler->AddProblemClause({ref_literal});
      }
    }
  }

  SatSolver::Status status;
  CpSolverResponse response;
  if (parameters.cp_model_presolve()) {
    std::vector<bool> solution;
    status = SolveWithPresolve(&solver, model->GetOrCreate<TimeLimit>(),
                               &solution, drat_proof_handler.get());
    if (status == SatSolver::MODEL_SAT) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(solution[ref]);
      }
    }
  } else {
    status = solver->SolveWithTimeLimit(model->GetOrCreate<TimeLimit>());
    if (status == SatSolver::MODEL_SAT) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(
            solver->Assignment().LiteralIsTrue(get_literal(ref)) ? 1 : 0);
      }
    }
  }

  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::MODEL_SAT: {
      CHECK(SolutionIsFeasible(model_proto,
                               std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
      response.set_status(CpSolverStatus::MODEL_SAT);
      break;
    }
    case SatSolver::MODEL_UNSAT: {
      response.set_status(CpSolverStatus::MODEL_UNSAT);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  response.set_num_booleans(solver->NumVariables());
  response.set_num_branches(solver->num_branches());
  response.set_num_conflicts(solver->num_failures());
  response.set_num_binary_propagations(solver->num_propagations());
  response.set_num_integer_propagations(0);
  response.set_wall_time(wall_timer.Get());
  response.set_user_time(user_timer.Get());
  response.set_deterministic_time(
      model->Get<TimeLimit>()->GetElapsedDeterministicTime());

  if (status == SatSolver::MODEL_UNSAT && drat_proof_handler != nullptr) {
    wall_timer.Restart();
    user_timer.Restart();
    DratChecker::Status drat_status =
        drat_proof_handler->Check(FLAGS_max_drat_time_in_seconds);
    switch (drat_status) {
      case DratChecker::UNKNOWN:
        LOG(INFO) << "DRAT status: UNKNOWN";
        break;
      case DratChecker::VALID:
        LOG(INFO) << "DRAT status: VALID";
        break;
      case DratChecker::INVALID:
        LOG(ERROR) << "DRAT status: INVALID";
        break;
      default:
        // Should not happen.
        break;
    }
    LOG(INFO) << "DRAT wall time: " << wall_timer.Get();
    LOG(INFO) << "DRAT user time: " << user_timer.Get();
  } else if (drat_proof_handler != nullptr) {
    // Always log a DRAT status to make it easier to extract it from a multirun
    // result with awk.
    LOG(INFO) << "DRAT status: NA";
    LOG(INFO) << "DRAT wall time: NA";
    LOG(INFO) << "DRAT user time: NA";
  }
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

#if !defined(__PORTABLE_PLATFORM__)
  // Dump?
  if (!FLAGS_cp_model_dump_file.empty()) {
    LOG(INFO) << "Dumping cp model proto to '" << FLAGS_cp_model_dump_file
              << "'.";
    CHECK_OK(file::SetBinaryProto(FLAGS_cp_model_dump_file, model_proto,
                                  file::Defaults()));
  }

  // Override parameters?
  if (!FLAGS_cp_model_params.empty()) {
    SatParameters params = *model->GetOrCreate<SatParameters>();
    SatParameters flag_params;
    CHECK(google::protobuf::TextFormat::ParseFromString(FLAGS_cp_model_params,
                                              &flag_params));
    params.MergeFrom(flag_params);
    model->Add(NewSatParameters(params));
    LOG(INFO) << "Parameters: " << params.ShortDebugString();
  }
#endif  // __PORTABLE_PLATFORM__

  // Special case for pure-sat problem.
  // TODO(user): improve the normal presolver to do the same thing.
  // TODO(user): Support solution hint, but then the first TODO will make it
  // automatic.
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (!model_proto.has_objective() && !model_proto.has_solution_hint() &&
      !params.enumerate_all_solutions()) {
    bool is_pure_sat = true;
    for (const IntegerVariableProto& var : model_proto.variables()) {
      if (var.domain_size() != 2 || var.domain(0) < 0 || var.domain(1) > 1) {
        is_pure_sat = false;
        break;
      }
    }
    if (is_pure_sat) {
      for (const ConstraintProto& ct : model_proto.constraints()) {
        if (ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolOr &&
            ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolAnd) {
          is_pure_sat = false;
          break;
        }
      }
    }
    if (is_pure_sat) return SolvePureSatModel(model_proto, model);
  }

  // Starts by expanding some constraints if needed.
  CpModelProto presolved_proto = ExpandCpModel(model_proto);

  // Solve without presolving ?
  const auto& observers = model->GetOrCreate<SolutionObservers>()->observers;
  const int num_original_variables = model_proto.variables_size();
  if (!params.cp_model_presolve() || params.enumerate_all_solutions()) {
    CpSolverResponse response = SolveCpModelInternal(
        presolved_proto, true,
        [&](const CpSolverResponse& intermediate_response) {
          if (observers.empty()) return;
          // Truncate the solution in case model expansion added more variables.
          CpSolverResponse truncated_response = intermediate_response;
          truncated_response.mutable_solution()->Truncate(
              num_original_variables);
          DCHECK(SolutionIsFeasible(
              model_proto,
              std::vector<int64>(truncated_response.solution().begin(),
                                 truncated_response.solution().end())));
          for (const auto& observer : observers) {
            observer(truncated_response);
          }
        },
        model);
    if (response.status() == CpSolverStatus::MODEL_SAT ||
        response.status() == CpSolverStatus::OPTIMAL) {
      // Truncate the solution in case model expansion added more variables.
      response.mutable_solution()->Truncate(num_original_variables);
      CHECK(SolutionIsFeasible(model_proto,
                               std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
    }
    return response;
  }

  // Do the actual presolve.
  CpModelProto mapping_proto;
  std::vector<int> postsolve_mapping;
  PresolveCpModel(&presolved_proto, &mapping_proto, &postsolve_mapping);
  VLOG(1) << CpModelStats(presolved_proto);

  // Note that it is okay to use the initial model_proto in the postsolve even
  // though we called PresolveCpModel() on the expanded proto. This is because
  // PostsolveResponse() only use the proto to known the number of variables to
  // fill in the response and to check the solution feasibility of these
  // variables.
  CpSolverResponse response = SolveCpModelInternal(
      presolved_proto, true,
      [&](const CpSolverResponse& response) {
        if (observers.empty()) return;
        CpSolverResponse copy = response;
        PostsolveResponse(model_proto, mapping_proto, postsolve_mapping, &copy);
        for (const auto& observer : observers) {
          observer(copy);
        }
      },
      model);
  PostsolveResponse(model_proto, mapping_proto, postsolve_mapping, &response);
  return response;
}

}  // namespace sat
}  // namespace operations_research
