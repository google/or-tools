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

#ifndef OR_TOOLS_SAT_INTEGER_H_
#define OR_TOOLS_SAT_INTEGER_H_

#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/hash.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/graph/iterators.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

// Value type of an integer variable. An integer variable is always bounded
// on both sides, and this type is also used to store the bounds [lb, ub] of the
// range of each integer variable.
//
// Note that both bounds are inclusive, which allows to write many propagation
// algorithms for just one of the bound and apply it to the negated variables to
// get the symmetric algorithm for the other bound.
DEFINE_INT_TYPE(IntegerValue, int64);

// The max range of an integer variable is [kMinIntegerValue, kMaxIntegerValue].
//
// It is symmetric so the set of possible ranges stays the same when we take the
// negation of a variable. Moreover, we need some IntegerValue that fall outside
// this range on both side so that we can usally take care of integer overflow
// by simply doing "saturated arithmetic" and if one of the bound overflow, the
// two bounds will "cross" each others and we will get an empty range.
constexpr IntegerValue kMaxIntegerValue(
    std::numeric_limits<IntegerValue::ValueType>::max() - 1);
constexpr IntegerValue kMinIntegerValue(-kMaxIntegerValue);

inline double ToDouble(IntegerValue value) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (value >= kMaxIntegerValue) return kInfinity;
  if (value <= kMinIntegerValue) return -kInfinity;
  return static_cast<double>(value.value());
}

template <class IntType>
inline IntType IntTypeAbs(IntType t) {
  return IntType(std::abs(t.value()));
}

inline IntegerValue Subtract(IntegerValue a, IntegerValue b) {
  const int64 result = CapSub(a.value(), b.value());
  if (result == kint64min || IntegerValue(result) <= kMinIntegerValue) {
    return kMinIntegerValue;
  }
  if (result == kint64max || IntegerValue(result) >= kMaxIntegerValue) {
    return kMaxIntegerValue;
  }
  return IntegerValue(result);
}

inline IntegerValue CeilRatio(IntegerValue dividend,
                              IntegerValue positive_divisor) {
  CHECK_GT(positive_divisor, 0);
  const IntegerValue result = dividend / positive_divisor;
  const IntegerValue adjust =
      static_cast<IntegerValue>(result * positive_divisor < dividend);
  return result + adjust;
}

inline IntegerValue FloorRatio(IntegerValue dividend,
                               IntegerValue positive_divisor) {
  CHECK_GT(positive_divisor, 0);
  const IntegerValue result = dividend / positive_divisor;
  const IntegerValue adjust =
      static_cast<IntegerValue>(result * positive_divisor > dividend);
  return result - adjust;
}

// Computes result += a * b, and return false iff there is an overflow.
inline bool AddProductTo(IntegerValue a, IntegerValue b, IntegerValue* result) {
  const int64 prod = CapProd(a.value(), b.value());
  if (prod == kint64min || prod == kint64max) return false;
  const int64 add = CapAdd(prod, result->value());
  if (add == kint64min || add == kint64max) return false;
  *result = IntegerValue(add);
  return true;
}

// Index of an IntegerVariable.
//
// Each time we create an IntegerVariable we also create its negation. This is
// done like that so internally we only stores and deal with lower bound. The
// upper bound beeing the lower bound of the negated variable.
DEFINE_INT_TYPE(IntegerVariable, int32);
const IntegerVariable kNoIntegerVariable(-1);
inline IntegerVariable NegationOf(IntegerVariable i) {
  return IntegerVariable(i.value() ^ 1);
}

inline bool VariableIsPositive(IntegerVariable i) {
  return (i.value() & 1) == 0;
}

inline IntegerVariable PositiveVariable(IntegerVariable i) {
  return IntegerVariable(i.value() & (~1));
}

// Returns the vector of the negated variables.
std::vector<IntegerVariable> NegationOf(
    const std::vector<IntegerVariable>& vars);

// The integer equivalent of a literal.
// It represents an IntegerVariable and an upper/lower bound on it.
//
// Overflow: all the bounds below kMinIntegerValue and kMaxIntegerValue are
// treated as kMinIntegerValue - 1 and kMaxIntegerValue + 1.
struct IntegerLiteral {
  // Because IntegerLiteral should never be created at a bound less constrained
  // than an existing IntegerVariable bound, we don't allow GreaterOrEqual() to
  // have a bound lower than kMinIntegerValue, and LowerOrEqual() to have a
  // bound greater than kMaxIntegerValue. The other side is not constrained
  // to allow for a computed bound to overflow. Note that both the full initial
  // domain and the empty domain can always be represented.
  static IntegerLiteral GreaterOrEqual(IntegerVariable i, IntegerValue bound);
  static IntegerLiteral LowerOrEqual(IntegerVariable i, IntegerValue bound);

  // Clients should prefer the static construction methods above.
  IntegerLiteral() : var(-1), bound(0) {}
  IntegerLiteral(IntegerVariable v, IntegerValue b) : var(v), bound(b) {
    DCHECK_GE(bound, kMinIntegerValue);
    DCHECK_LE(bound, kMaxIntegerValue + 1);
  }

  // The negation of x >= bound is x <= bound - 1.
  IntegerLiteral Negated() const;

  bool operator==(IntegerLiteral o) const {
    return var == o.var && bound == o.bound;
  }
  bool operator!=(IntegerLiteral o) const {
    return var != o.var || bound != o.bound;
  }

  std::string DebugString() const {
    return VariableIsPositive(var)
               ? absl::StrCat("I", var.value() / 2, ">=", bound.value())
               : absl::StrCat("I", var.value() / 2, "<=", -bound.value());
  }

  // Note that bound should be in [kMinIntegerValue, kMaxIntegerValue + 1].
  IntegerVariable var;
  IntegerValue bound;
};

inline std::ostream& operator<<(std::ostream& os, IntegerLiteral i_lit) {
  os << i_lit.DebugString();
  return os;
}

using InlinedIntegerLiteralVector = absl::InlinedVector<IntegerLiteral, 2>;

// A singleton that holds the INITIAL integer variable domains.
struct IntegerDomains : public gtl::ITIVector<IntegerVariable, Domain> {
  explicit IntegerDomains(Model* model) {}
};

// Some heuristics may be generated automatically, for instance by constraints.
// Those will be stored in a SearchHeuristicsVector object owned by the model.
//
// TODO(user): Move this and other similar classes in a "model_singleton" file?
class SearchHeuristicsVector
    : public std::vector<std::function<LiteralIndex()>> {};

// Each integer variable x will be associated with a set of literals encoding
// (x >= v) for some values of v. This class maintains the relationship between
// the integer variables and such literals which can be created by a call to
// CreateAssociatedLiteral().
//
// The advantage of creating such Boolean variables is that the SatSolver which
// is driving the search can then take this variable as a decision and maintain
// these variables activity and so on. These variables can also be propagated
// directly by the learned clauses.
//
// This class also support a non-lazy full domain encoding which will create one
// literal per possible value in the domain. See FullyEncodeVariable(). This is
// meant to be called by constraints that directly work on the variable values
// like a table constraint or an all-diff constraint.
//
// TODO(user): We could also lazily create precedences Booleans between two
// arbitrary IntegerVariable. This is better done in the PrecedencesPropagator
// though.
class IntegerEncoder {
 public:
  explicit IntegerEncoder(Model* model)
      : sat_solver_(model->GetOrCreate<SatSolver>()),
        domains_(model->GetOrCreate<IntegerDomains>()),
        num_created_variables_(0) {}

  ~IntegerEncoder() {
    VLOG(1) << "#variables created = " << num_created_variables_;
  }

  // Fully encode a variable using its current initial domain.
  // This can be called only once.
  //
  // This creates new Booleans variables as needed:
  // 1) num_values for the literals X == value. Except when there is just
  //    two value in which case only one variable is created.
  // 2) num_values - 3 for the literals X >= value or X <= value (using their
  //    negation). The -3 comes from the fact that we can reuse the equality
  //    literals for the two extreme points.
  //
  // The encoding for NegationOf(var) is automatically created too. It reuses
  // the same Boolean variable as the encoding of var.
  //
  // TODO(user): It is currently only possible to call that at the decision
  // level zero because we cannot add ternary clause in the middle of the
  // search (for now). This is Checked.
  void FullyEncodeVariable(IntegerVariable var);

  // Returns true iff FullyEncodeVariable(var) has been called. Note that
  // PartialDomainEncoding() may actually return a full domain encoding, but if
  // FullyEncodeVariable() was not called, this will still return false.
  //
  // TODO(user): Detect this case and mark such variable as fully encoded?
  bool VariableIsFullyEncoded(IntegerVariable var) const {
    if (var >= is_fully_encoded_.size()) return false;
    return is_fully_encoded_[var];
  }

  // Computes the full encoding of a variable on which FullyEncodeVariable() has
  // been called. The returned elements are always sorted by increasing
  // IntegerValue and we filter values associated to false literals.
  //
  // Performance note: This function is not particularly fast, however it should
  // only be required during domain creation, so it should be ok. This allow us
  // to not waste memory.
  struct ValueLiteralPair {
    ValueLiteralPair(IntegerValue v, Literal l) : value(v), literal(l) {}
    bool operator==(const ValueLiteralPair& o) const {
      return value == o.value && literal == o.literal;
    }
    IntegerValue value;
    Literal literal;
  };
  std::vector<ValueLiteralPair> FullDomainEncoding(IntegerVariable var) const;

  // Same as FullDomainEncoding() but only returns the list of value that are
  // currently associated to a literal. In particular this has no guarantee to
  // span the full domain of the given variable (but it might).
  std::vector<ValueLiteralPair> PartialDomainEncoding(
      IntegerVariable var) const;

  // Returns the "canonical" (i_lit, negation of i_lit) pair. This mainly
  // deal with domain with initial hole like [1,2][5,6] so that if one ask
  // for x <= 3, this get canonicalized in the pair (x <= 2, x >= 5).
  //
  // Note that it is an error to call this with a literal that is trivially true
  // or trivially false according to the initial variable domain. This is
  // CHECKed to make sure we don't create wasteful literal.
  //
  // TODO(user): This is linear in the domain "complexity", we can do better if
  // needed.
  std::pair<IntegerLiteral, IntegerLiteral> Canonicalize(
      IntegerLiteral i_lit) const;

  // Returns, after creating it if needed, a Boolean literal such that:
  // - if true, then the IntegerLiteral is true.
  // - if false, then the negated IntegerLiteral is true.
  //
  // Note that this "canonicalize" the given literal first.
  //
  // This add the proper implications with the two "neighbor" literals of this
  // one if they exist. This is the "list encoding" in: Thibaut Feydy, Peter J.
  // Stuckey, "Lazy Clause Generation Reengineered", CP 2009.
  Literal GetOrCreateAssociatedLiteral(IntegerLiteral i_lit);
  Literal GetOrCreateLiteralAssociatedToEquality(IntegerVariable var,
                                                 IntegerValue value);

  // Associates the Boolean literal to (X >= bound) or (X == value). If a
  // literal was already associated to this fact, this will add an equality
  // constraints between both literals. If the fact is trivially true or false,
  // this will fix the given literal.
  void AssociateToIntegerLiteral(Literal literal, IntegerLiteral i_lit);
  void AssociateToIntegerEqualValue(Literal literal, IntegerVariable var,
                                    IntegerValue value);

  // Returns true iff the given integer literal is associated. The second
  // version returns the associated literal or kNoLiteralIndex. Note that none
  // of these function call Canonicalize() first for speed, so it is possible
  // that this returns false even though GetOrCreateAssociatedLiteral() would
  // not create a new literal.
  bool LiteralIsAssociated(IntegerLiteral i_lit) const;
  LiteralIndex GetAssociatedLiteral(IntegerLiteral i_lit);

  // Advanced usage. It is more efficient to create the associated literals in
  // order, but it might be anoying to do so. Instead, you can first call
  // DisableImplicationBetweenLiteral() and when you are done creating all the
  // associated literals, you can call (only at level zero)
  // AddAllImplicationsBetweenAssociatedLiterals() which will also turn back on
  // the implications between literals for the one that will be added
  // afterwards.
  void DisableImplicationBetweenLiteral() { add_implications_ = false; }
  void AddAllImplicationsBetweenAssociatedLiterals();

  // Returns the IntegerLiterals that were associated with the given Literal.
  const InlinedIntegerLiteralVector& GetIntegerLiterals(Literal lit) const {
    if (lit.Index() >= reverse_encoding_.size()) {
      return empty_integer_literal_vector_;
    }
    return reverse_encoding_[lit.Index()];
  }

  // Same as GetIntegerLiterals(), but in addition, if the literal was
  // associated to an integer == value, then the returned list will contain both
  // (integer >= value) and (integer <= value).
  const InlinedIntegerLiteralVector& GetAllIntegerLiterals(Literal lit) const {
    if (lit.Index() >= full_reverse_encoding_.size()) {
      return empty_integer_literal_vector_;
    }
    return full_reverse_encoding_[lit.Index()];
  }

  // This is part of a "hack" to deal with new association involving a fixed
  // literal. Note that these are only allowed at the decision level zero.
  const std::vector<IntegerLiteral> NewlyFixedIntegerLiterals() const {
    return newly_fixed_integer_literals_;
  }
  void ClearNewlyFixedIntegerLiterals() {
    newly_fixed_integer_literals_.clear();
  }

  // If it exists, returns a [0,1] integer variable which is equal to 1 iff the
  // given literal is true. Returns kNoIntegerVariable if such variable does not
  // exist. Note that one can create one by creating a new IntegerVariable and
  // calling AssociateToIntegerEqualValue().
  const IntegerVariable GetLiteralView(Literal lit) const {
    if (lit.Index() >= literal_view_.size()) return kNoIntegerVariable;
    return literal_view_[lit.Index()];
  }

  // Returns a Boolean literal associated with a bound lower than or equal to
  // the one of the given IntegerLiteral. If the given IntegerLiteral is true,
  // then the returned literal should be true too. Returns kNoLiteralIndex if no
  // such literal was created.
  //
  // Ex: if 'i' is (x >= 4) and we already created a literal associated to
  // (x >= 2) but not to (x >= 3), we will return the literal associated with
  // (x >= 2).
  LiteralIndex SearchForLiteralAtOrBefore(IntegerLiteral i,
                                          IntegerValue* bound) const;

  // Gets the literal always set to true, make it if it does not exist.
  Literal GetTrueLiteral() {
    DCHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
    if (literal_index_true_ == kNoLiteralIndex) {
      const Literal literal_true =
          Literal(sat_solver_->NewBooleanVariable(), true);
      literal_index_true_ = literal_true.Index();
      sat_solver_->AddUnitClause(literal_true);
    }
    return Literal(literal_index_true_);
  }
  Literal GetFalseLiteral() { return GetTrueLiteral().Negated(); }

  // Returns the set of Literal associated to IntegerLiteral of the form var >=
  // value. We make a copy, because this can be easily invalidated when calling
  // any function of this class. So it is less efficient but safer.
  std::map<IntegerValue, Literal> PartialGreaterThanEncoding(
      IntegerVariable var) const {
    if (var >= encoding_by_var_.size()) {
      return std::map<IntegerValue, Literal>();
    }
    return encoding_by_var_[var];
  }

 private:
  // Only add the equivalence between i_lit and literal, if there is already an
  // associated literal with i_lit, this make literal and this associated
  // literal equivalent.
  void HalfAssociateGivenLiteral(IntegerLiteral i_lit, Literal literal);

  // Adds the implications:
  //    Literal(before) <= associated_lit <= Literal(after).
  // Arguments:
  //  - map is just encoding_by_var_[associated_lit.var] and is passed as a
  //    slight optimization.
  //  - 'it' is the current position of associated_lit in map, i.e we must have
  //    it->second == associated_lit.
  void AddImplications(const std::map<IntegerValue, Literal>& map,
                       std::map<IntegerValue, Literal>::const_iterator it,
                       Literal associated_lit);

  SatSolver* sat_solver_;
  IntegerDomains* domains_;

  bool add_implications_ = true;
  int64 num_created_variables_ = 0;

  // We keep all the literals associated to an Integer variable in a map ordered
  // by bound (so we can properly add implications between the literals
  // corresponding to the same variable).
  //
  // TODO(user): Remove the entry no longer needed because of level zero
  // propagations.
  gtl::ITIVector<IntegerVariable, std::map<IntegerValue, Literal>>
      encoding_by_var_;

  // Store for a given LiteralIndex the list of its associated IntegerLiterals.
  const InlinedIntegerLiteralVector empty_integer_literal_vector_;
  gtl::ITIVector<LiteralIndex, InlinedIntegerLiteralVector> reverse_encoding_;
  gtl::ITIVector<LiteralIndex, InlinedIntegerLiteralVector>
      full_reverse_encoding_;
  std::vector<IntegerLiteral> newly_fixed_integer_literals_;

  // Store for a given LiteralIndex its IntegerVariable view or kNoLiteralIndex
  // if there is none.
  gtl::ITIVector<LiteralIndex, IntegerVariable> literal_view_;

  // Mapping (variable == value) -> associated literal. Note that even if
  // there is more than one literal associated to the same fact, we just keep
  // the first one that was added.
  absl::flat_hash_map<std::pair<IntegerVariable, IntegerValue>, Literal>
      equality_to_associated_literal_;

  // Variables that are fully encoded.
  gtl::ITIVector<IntegerVariable, bool> is_fully_encoded_;

  // A literal that is always true, convenient to encode trivial domains.
  // This will be lazily created when needed.
  LiteralIndex literal_index_true_ = kNoLiteralIndex;

  DISALLOW_COPY_AND_ASSIGN(IntegerEncoder);
};

// This class maintains a set of integer variables with their current bounds.
// Bounds can be propagated from an external "source" and this class helps
// to maintain the reason for each propagation.
class IntegerTrail : public SatPropagator {
 public:
  explicit IntegerTrail(Model* model)
      : SatPropagator("IntegerTrail"),
        num_enqueues_(0),
        domains_(model->GetOrCreate<IntegerDomains>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        trail_(model->GetOrCreate<Trail>()) {
    model->GetOrCreate<SatSolver>()->AddPropagator(this);
  }
  ~IntegerTrail() final {}

  // SatPropagator interface. These functions make sure the current bounds
  // information is in sync with the current solver literal trail. Any
  // class/propagator using this class must make sure it is synced to the
  // correct state before calling any of its functions.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;
  absl::Span<const Literal> Reason(const Trail& trail,
                                   int trail_index) const final;

  // Returns the number of created integer variables.
  //
  // Note that this is twice the number of call to AddIntegerVariable() since
  // we automatically create the NegationOf() variable too.
  IntegerVariable NumIntegerVariables() const {
    return IntegerVariable(vars_.size());
  }

  // Adds a new integer variable. Adding integer variable can only be done when
  // the decision level is zero (checked). The given bounds are INCLUSIVE.
  IntegerVariable AddIntegerVariable(IntegerValue lower_bound,
                                     IntegerValue upper_bound);

  // Same as above but for a more complex domain specified as a sorted list of
  // disjoint intervals. See the Domain class.
  IntegerVariable AddIntegerVariable(const Domain& domain);

  // Returns the initial domain of the given variable. Note that the min/max
  // are updated with level zero propagation, but not holes.
  const Domain& InitialVariableDomain(IntegerVariable var) const;

  // Takes the intersection with the current initial variable domain.
  //
  // TODO(user): There is some memory inefficiency if this is called many time
  // because of the underlying data structure we use. In practice, when used
  // with a presolve, this is not often used, so that is fine though.
  bool UpdateInitialDomain(IntegerVariable var, Domain domain);

  // Same as AddIntegerVariable(value, value), but this is a bit more efficient
  // because it reuses another constant with the same value if its exist.
  //
  // Note(user): Creating constant integer variable is a bit wasteful, but not
  // that much, and it allows to simplify a lot of constraints that do not need
  // to handle this case any differently than the general one. Maybe there is a
  // better solution, but this is not really high priority as of December 2016.
  IntegerVariable GetOrCreateConstantIntegerVariable(IntegerValue value);
  int NumConstantVariables() const;

  // Same as AddIntegerVariable() but uses the maximum possible range. Note
  // that since we take negation of bounds in various places, we make sure that
  // we don't have overflow when we take the negation of the lower bound or of
  // the upper bound.
  IntegerVariable AddIntegerVariable() {
    return AddIntegerVariable(kMinIntegerValue, kMaxIntegerValue);
  }

  // For an optional variable, both its lb and ub must be valid bound assuming
  // the fact that the variable is "present". However, the domain [lb, ub] is
  // allowed to be empty (i.e. ub < lb) if the given is_ignored literal is true.
  // Moreover, if is_ignored is true, then the bound of such variable should NOT
  // impact any non-ignored variable in any way (but the reverse is not true).
  bool IsOptional(IntegerVariable i) const {
    return is_ignored_literals_[i] != kNoLiteralIndex;
  }
  bool IsCurrentlyIgnored(IntegerVariable i) const {
    const LiteralIndex is_ignored_literal = is_ignored_literals_[i];
    return is_ignored_literal != kNoLiteralIndex &&
           trail_->Assignment().LiteralIsTrue(Literal(is_ignored_literal));
  }
  Literal IsIgnoredLiteral(IntegerVariable i) const {
    DCHECK(IsOptional(i));
    return Literal(is_ignored_literals_[i]);
  }
  LiteralIndex OptionalLiteralIndex(IntegerVariable i) const {
    return is_ignored_literals_[i] == kNoLiteralIndex
               ? kNoLiteralIndex
               : Literal(is_ignored_literals_[i]).NegatedIndex();
  }
  void MarkIntegerVariableAsOptional(IntegerVariable i, Literal is_considered) {
    DCHECK(is_ignored_literals_[i] == kNoLiteralIndex ||
           is_ignored_literals_[i] == is_considered.NegatedIndex());
    is_ignored_literals_[i] = is_considered.NegatedIndex();
    is_ignored_literals_[NegationOf(i)] = is_considered.NegatedIndex();
  }

  // Returns the current lower/upper bound of the given integer variable.
  IntegerValue LowerBound(IntegerVariable i) const;
  IntegerValue UpperBound(IntegerVariable i) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given integer variable.
  IntegerLiteral LowerBoundAsLiteral(IntegerVariable i) const;
  IntegerLiteral UpperBoundAsLiteral(IntegerVariable i) const;

  // Returns the current value (if known) of an IntegerLiteral.
  bool IntegerLiteralIsTrue(IntegerLiteral l) const;
  bool IntegerLiteralIsFalse(IntegerLiteral l) const;

  // Returns globally valid lower/upper bound on the given integer variable.
  IntegerValue LevelZeroLowerBound(IntegerVariable var) const;
  IntegerValue LevelZeroUpperBound(IntegerVariable var) const;

  // Advanced usage. Given the reason for
  // (Sum_i coeffs[i] * reason[i].var >= current_lb) initially in reason,
  // this function relaxes the reason given that we only need the explanation of
  // (Sum_i coeffs[i] * reason[i].var >= current_lb - slack).
  //
  // Preconditions:
  // - coeffs must be of same size as reason, and all entry must be positive.
  // - *reason must initially contains the trivial initial reason, that is
  //   the current lower-bound of each variables.
  //
  // TODO(user): Requiring all initial literal to be at their current bound is
  // not really clean. Maybe we can change the API to only take IntegerVariable
  // and produce the reason directly.
  //
  // TODO(user): change API so that this work is performed during the conflict
  // analysis where we can be smarter in how we relax the reason. Note however
  // that this function is mainly used when we have a conflict, so this is not
  // really high priority.
  //
  // TODO(user): Test that the code work in the presence of integer overflow.
  void RelaxLinearReason(IntegerValue slack,
                         absl::Span<const IntegerValue> coeffs,
                         std::vector<IntegerLiteral>* reason) const;

  // Same as above but take in IntegerVariables instead of IntegerLiterals.
  void AppendRelaxedLinearReason(IntegerValue slack,
                                 absl::Span<const IntegerValue> coeffs,
                                 absl::Span<const IntegerVariable> vars,
                                 std::vector<IntegerLiteral>* reason) const;

  // Same as above but relax the given trail indices.
  void RelaxLinearReason(IntegerValue slack,
                         absl::Span<const IntegerValue> coeffs,
                         std::vector<int>* trail_indices) const;

  // Removes from the reasons the literal that are always true.
  // This is mainly useful for experiments/testing.
  void RemoveLevelZeroBounds(std::vector<IntegerLiteral>* reason) const;

  // Enqueue new information about a variable bound. Calling this with a less
  // restrictive bound than the current one will have no effect.
  //
  // The reason for this "assignment" must be provided as:
  // - A set of Literal currently beeing all false.
  // - A set of IntegerLiteral currently beeing all true.
  //
  // IMPORTANT: Notice the inversed sign in the literal reason. This is a bit
  // confusing but internally SAT use this direction for efficiency.
  //
  // Note(user): Duplicates Literal/IntegerLiteral are supported because we call
  // STLSortAndRemoveDuplicates() in MergeReasonInto(), but maybe they shouldn't
  // for efficiency reason.
  //
  // TODO(user): If the given bound is equal to the current bound, maybe the new
  // reason is better? how to decide and what to do in this case? to think about
  // it. Currently we simply don't do anything.
  ABSL_MUST_USE_RESULT bool Enqueue(
      IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  // Same as Enqueue(), but takes an extra argument which if smaller than
  // integer_trail_.size() is interpreted as the trail index of an old Enqueue()
  // that had the same reason as this one. Note that the given Span must still
  // be valid as they are used in case of conflict.
  //
  // TODO(user): This currently cannot refer to a trail_index with a lazy
  // reason. Fix or at least check that this is the case.
  ABSL_MUST_USE_RESULT bool Enqueue(
      IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason,
      int trail_index_with_same_reason);

  // Lazy reason API.
  //
  // The function is provided with the IntegerLiteral to explain and its index
  // in the integer trail. It must fill the two vectors so that literals
  // contains any Literal part of the reason and dependencies contains the trail
  // index of any IntegerLiteral that is also part of the reason.
  //
  // Remark: sometimes this is called to fill the conflict while the literal
  // to explain is propagated. In this case, trail_index_of_literal will be
  // the current trail index, and we cannot assume that there is anything filled
  // yet in integer_literal[trail_index_of_literal].
  using LazyReasonFunction = std::function<void(
      IntegerLiteral literal_to_explain, int trail_index_of_literal,
      std::vector<Literal>* literals, std::vector<int>* dependencies)>;
  ABSL_MUST_USE_RESULT bool Enqueue(IntegerLiteral i_lit,
                                    LazyReasonFunction lazy_reason);

  // Enqueues the given literal on the trail.
  // See the comment of Enqueue() for the reason format.
  void EnqueueLiteral(Literal literal, absl::Span<const Literal> literal_reason,
                      absl::Span<const IntegerLiteral> integer_reason);

  // Returns the reason (as set of Literal currently false) for a given integer
  // literal. Note that the bound must be less restrictive than the current
  // bound (checked).
  std::vector<Literal> ReasonFor(IntegerLiteral literal) const;

  // Appends the reason for the given integer literals to the output and call
  // STLSortAndRemoveDuplicates() on it.
  void MergeReasonInto(absl::Span<const IntegerLiteral> literals,
                       std::vector<Literal>* output) const;

  // Returns the number of enqueues that changed a variable bounds. We don't
  // count enqueues called with a less restrictive bound than the current one.
  //
  // Note(user): this can be used to see if any of the bounds changed. Just
  // looking at the integer trail index is not enough because at level zero it
  // doesn't change since we directly update the "fixed" bounds.
  int64 num_enqueues() const { return num_enqueues_; }

  // Same as num_enqueues but only count the level zero changes.
  int64 num_level_zero_enqueues() const { return num_level_zero_enqueues_; }

  // All the registered bitsets will be set to one each time a LbVar is
  // modified. It is up to the client to clear it if it wants to be notified
  // with the newly modified variables.
  void RegisterWatcher(SparseBitset<IntegerVariable>* p) {
    p->ClearAndResize(NumIntegerVariables());
    watchers_.push_back(p);
  }

  // Helper functions to report a conflict. Always return false so a client can
  // simply do: return integer_trail_->ReportConflict(...);
  bool ReportConflict(absl::Span<const Literal> literal_reason,
                      absl::Span<const IntegerLiteral> integer_reason) {
    DCHECK(ReasonIsValid(literal_reason, integer_reason));
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->assign(literal_reason.begin(), literal_reason.end());
    MergeReasonInto(integer_reason, conflict);
    return false;
  }
  bool ReportConflict(absl::Span<const IntegerLiteral> integer_reason) {
    DCHECK(ReasonIsValid({}, integer_reason));
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->clear();
    MergeReasonInto(integer_reason, conflict);
    return false;
  }

  // Returns true if the variable lower bound is still the one from level zero.
  bool VariableLowerBoundIsFromLevelZero(IntegerVariable var) const {
    return vars_[var].current_trail_index < vars_.size();
  }

  // Registers a reversible class. This class will always be synced with the
  // correct decision level.
  void RegisterReversibleClass(ReversibleInterface* rev) {
    reversible_classes_.push_back(rev);
  }

  int Index() const { return integer_trail_.size(); }

  // Inspects the trail and output all the non-level zero bounds (one per
  // variables) to the output. The algo is sparse if there is only a few
  // propagations on the trail.
  void AppendNewBounds(std::vector<IntegerLiteral>* output) const;

  // Returns the trail index < threshold of a TrailEntry about var. Returns -1
  // if there is no such entry (at a positive decision level). This is basically
  // the trail index of the lower bound of var at the time.
  //
  // Important: We do some optimization internally, so this should only be
  // used from within a LazyReasonFunction().
  int FindTrailIndexOfVarBefore(IntegerVariable var, int threshold) const;

 private:
  // Used for DHECKs to validate the reason given to the public functions above.
  // Tests that all Literal are false. Tests that all IntegerLiteral are true.
  bool ReasonIsValid(absl::Span<const Literal> literal_reason,
                     absl::Span<const IntegerLiteral> integer_reason);

  // Called by the Enqueue() functions that detected a conflict. This does some
  // common conflict initialization that must terminate by a call to
  // MergeReasonIntoInternal(conflict) where conflict is the returned vector.
  std::vector<Literal>* InitializeConflict(
      IntegerLiteral integer_literal, const LazyReasonFunction& lazy_reason,
      absl::Span<const Literal> literals_reason,
      absl::Span<const IntegerLiteral> bounds_reason);

  // Internal implementation of the different public Enqueue() functions.
  ABSL_MUST_USE_RESULT bool EnqueueInternal(
      IntegerLiteral i_lit, LazyReasonFunction lazy_reason,
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason,
      int trail_index_with_same_reason);

  // Internal implementation of the EnqueueLiteral() functions.
  void EnqueueLiteralInternal(Literal literal, LazyReasonFunction lazy_reason,
                              absl::Span<const Literal> literal_reason,
                              absl::Span<const IntegerLiteral> integer_reason);

  // Same as EnqueueInternal() but for the case where we push an IntegerLiteral
  // because an associated Literal is true (and we know it). In this case, we
  // have less work to do, so this has the same effect but is faster.
  ABSL_MUST_USE_RESULT bool EnqueueAssociatedIntegerLiteral(
      IntegerLiteral i_lit, Literal literal_reason);

  // Does the work of MergeReasonInto() when queue_ is already initialized.
  void MergeReasonIntoInternal(std::vector<Literal>* output) const;

  // Returns the lowest trail index of a TrailEntry that can be used to explain
  // the given IntegerLiteral. The literal must be currently true (CHECKed).
  // Returns -1 if the explanation is trivial.
  int FindLowestTrailIndexThatExplainBound(IntegerLiteral i_lit) const;

  // This must be called before Dependencies() or AppendLiteralsReason().
  //
  // TODO(user): Not really robust, try to find a better way.
  void ComputeLazyReasonIfNeeded(int trail_index) const;

  // Helper function to return the "dependencies" of a bound assignment.
  // All the TrailEntry at these indices are part of the reason for this
  // assignment.
  //
  // Important: The returned Span is only valid up to the next call.
  absl::Span<const int> Dependencies(int trail_index) const;

  // Helper function to append the Literal part of the reason for this bound
  // assignment. We use added_variables_ to not add the same literal twice.
  // Note that looking at literal.Variable() is enough since all the literals
  // of a reason must be false.
  void AppendLiteralsReason(int trail_index,
                            std::vector<Literal>* output) const;

  // Returns some debugging info.
  std::string DebugString();

  // Information for each internal variable about its current bound.
  struct VarInfo {
    // The current bound on this variable.
    IntegerValue current_bound;

    // Trail index of the last TrailEntry in the trail refering to this var.
    int current_trail_index;
  };
  gtl::ITIVector<IntegerVariable, VarInfo> vars_;

  // This is used by FindLowestTrailIndexThatExplainBound() and
  // FindTrailIndexOfVarBefore() to speed up the lookup. It keeps a trail index
  // for each variable that may or may not point to a TrailEntry regarding this
  // variable. The validity of the index is verified before beeing used.
  //
  // The cache will only be updated with trail_index >= threshold.
  mutable int var_trail_index_cache_threshold_ = 0;
  mutable gtl::ITIVector<IntegerVariable, int> var_trail_index_cache_;

  // Used by GetOrCreateConstantIntegerVariable() to return already created
  // constant variables that share the same value.
  absl::flat_hash_map<IntegerValue, IntegerVariable> constant_map_;

  // The integer trail. It always start by num_vars sentinel values with the
  // level 0 bounds (in one to one correspondence with vars_).
  struct TrailEntry {
    IntegerValue bound;
    IntegerVariable var;
    int32 prev_trail_index;

    // Index in literals_reason_start_/bounds_reason_starts_ If this is -1, then
    // this was a propagation with a lazy reason, and the reason can be
    // re-created by calling the function lazy_reasons_[trail_index].
    int32 reason_index;
  };
  std::vector<TrailEntry> integer_trail_;
  std::vector<LazyReasonFunction> lazy_reasons_;

  // Start of each decision levels in integer_trail_.
  // TODO(user): use more general reversible mechanism?
  std::vector<int> integer_search_levels_;

  // Buffer to store the reason of each trail entry.
  // Note that bounds_reason_buffer_ is an "union". It initially contains the
  // IntegerLiteral, and is lazily replaced by the result of
  // FindLowestTrailIndexThatExplainBound() applied to these literals. The
  // encoding is a bit hacky, see Dependencies().
  std::vector<int> reason_decision_levels_;
  std::vector<int> literals_reason_starts_;
  std::vector<int> bounds_reason_starts_;
  std::vector<Literal> literals_reason_buffer_;

  // These two vectors are in one to one correspondence. Dependencies() will
  // "cache" the result of the conversion from IntegerLiteral to trail indices
  // in trail_index_reason_buffer_.
  std::vector<IntegerLiteral> bounds_reason_buffer_;
  mutable std::vector<int> trail_index_reason_buffer_;

  // Temporary vector filled by calls to LazyReasonFunction().
  mutable std::vector<Literal> lazy_reason_literals_;
  mutable std::vector<int> lazy_reason_trail_indices_;

  // The "is_ignored" literal of the optional variables or kNoLiteralIndex.
  gtl::ITIVector<IntegerVariable, LiteralIndex> is_ignored_literals_;

  // This is only filled for variables with a domain more complex than a single
  // interval of values. var_to_current_lb_interval_index_[var] stores the
  // intervals in (*domains_)[var] where the current lower-bound lies.
  //
  // TODO(user): Avoid using hash_map here, a simple vector should be more
  // efficient, but we need the "rev" aspect.
  RevMap<absl::flat_hash_map<IntegerVariable, int>>
      var_to_current_lb_interval_index_;

  // Temporary data used by MergeReasonInto().
  mutable bool has_dependency_ = false;
  mutable std::vector<int> tmp_queue_;
  mutable std::vector<IntegerVariable> tmp_to_clear_;
  mutable gtl::ITIVector<IntegerVariable, int> tmp_var_to_trail_index_in_queue_;
  mutable SparseBitset<BooleanVariable> added_variables_;

  // Temporary heap used by RelaxLinearReason();
  struct RelaxHeapEntry {
    int index;
    IntegerValue coeff;
    int64 diff;
    bool operator<(const RelaxHeapEntry& o) const { return index < o.index; }
  };
  mutable std::vector<RelaxHeapEntry> relax_heap_;
  mutable std::vector<int> tmp_indices_;

  // Temporary data used by AppendNewBounds().
  mutable SparseBitset<IntegerVariable> tmp_marked_;

  // For EnqueueLiteral(), we store a special TrailEntry to recover the reason
  // lazily. This vector indicates the correspondence between a literal that
  // was pushed by this class at a given trail index, and the index of its
  // TrailEntry in integer_trail_.
  std::vector<int> boolean_trail_index_to_integer_one_;

  int64 num_enqueues_ = 0;
  int64 num_level_zero_enqueues_ = 0;

  std::vector<SparseBitset<IntegerVariable>*> watchers_;
  std::vector<ReversibleInterface*> reversible_classes_;

  IntegerDomains* domains_;
  IntegerEncoder* encoder_;
  Trail* trail_;

  DISALLOW_COPY_AND_ASSIGN(IntegerTrail);
};

// Base class for CP like propagators.
//
// TODO(user): Think about an incremental Propagate() interface.
//
// TODO(user): Add shortcuts for the most used functions? like
// Min(IntegerVariable) and Max(IntegerVariable)?
class PropagatorInterface {
 public:
  PropagatorInterface() {}
  virtual ~PropagatorInterface() {}

  // This will be called after one or more literals that are watched by this
  // propagator changed. It will also always be called on the first propagation
  // cycle after registration.
  virtual bool Propagate() = 0;

  // This will only be called on a non-empty vector, otherwise Propagate() will
  // be called. The passed vector will contain the "watch index" of all the
  // literals that were given one at registration and that changed since the
  // last call to Propagate(). This is only true when going down in the search
  // tree, on backjump this list will be cleared.
  //
  // Notes:
  // - The indices may contain duplicates if the same integer variable as been
  //   updated many times or if different watched literals have the same
  //   watch_index.
  // - At level zero, it will not contain any indices associated with literals
  //   that were already fixed when the propagator was registered. Only the
  //   indices of the literals modified after the registration will be present.
  virtual bool IncrementalPropagate(const std::vector<int>& watch_indices) {
    LOG(FATAL) << "Not implemented.";
    return false;  // Remove warning in Windows
  }
};

// Singleton for basic reversible types. We need the wrapper so that they can be
// accessed with model->GetOrCreate<>() and properly registered at creation.
class RevIntRepository : public RevRepository<int> {
 public:
  explicit RevIntRepository(Model* model) {
    model->GetOrCreate<IntegerTrail>()->RegisterReversibleClass(this);
  }
};
class RevIntegerValueRepository : public RevRepository<IntegerValue> {
 public:
  explicit RevIntegerValueRepository(Model* model) {
    model->GetOrCreate<IntegerTrail>()->RegisterReversibleClass(this);
  }
};

// This class allows registering Propagator that will be called if a
// watched Literal or LbVar changes.
//
// TODO(user): Move this to its own file. Add unit tests!
class GenericLiteralWatcher : public SatPropagator {
 public:
  explicit GenericLiteralWatcher(Model* model);
  ~GenericLiteralWatcher() final {}

  // On propagate, the registered propagators will be called if they need to
  // until a fixed point is reached. Propagators with low ids will tend to be
  // called first, but it ultimately depends on their "waking" order.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;

  // Registers a propagator and returns its unique ids.
  int Register(PropagatorInterface* propagator);

  // Changes the priority of the propagator with given id. The priority is a
  // non-negative integer. Propagators with a lower priority will always be
  // run before the ones with a higher one. The default priority is one.
  void SetPropagatorPriority(int id, int priority);

  // The default behavior is to assume that a propagator does not need to be
  // called twice in a row. However, propagators on which this is called will be
  // called again if they change one of their own watched variables.
  void NotifyThatPropagatorMayNotReachFixedPointInOnePass(int id);

  // Whether we call a propagator even if its watched variables didn't change.
  // This is only used when we are back to level zero. This was introduced for
  // the LP propagator where we might need to continue an interrupted solve or
  // add extra cuts at level zero.
  void AlwaysCallAtLevelZero(int id);

  // Watches the corresponding quantity. The propagator with given id will be
  // called if it changes. Note that WatchLiteral() only trigger when the
  // literal becomes true.
  //
  // If watch_index is specified, it is associated with the watched literal.
  // Doing this will cause IncrementalPropagate() to be called (see the
  // documentation of this interface for more detail).
  void WatchLiteral(Literal l, int id, int watch_index = -1);
  void WatchLowerBound(IntegerVariable var, int id, int watch_index = -1);
  void WatchUpperBound(IntegerVariable var, int id, int watch_index = -1);
  void WatchIntegerVariable(IntegerVariable i, int id, int watch_index = -1);

  // No-op overload for "constant" IntegerVariable that are sometimes templated
  // as an IntegerValue.
  void WatchLowerBound(IntegerValue i, int id) {}
  void WatchUpperBound(IntegerValue i, int id) {}
  void WatchIntegerVariable(IntegerValue v, int id) {}

  // Registers a reversible class with a given propagator. This class will be
  // changed to the correct state just before the propagator is called.
  //
  // Doing it just before should minimize cache-misses and bundle as much as
  // possible the "backtracking" together. Many propagators only watches a
  // few variables and will not be called at each decision levels.
  void RegisterReversibleClass(int id, ReversibleInterface* rev);

  // Registers a reversible int with a given propagator. The int will be changed
  // to its correct value just before Propagate() is called.
  //
  // Note that this will work in O(num_rev_int_of_propagator_id) per call to
  // Propagate() and happens at most once per decision level. As such this is
  // meant for classes that have just a few reversible ints or that will have a
  // similar complexity anyway.
  //
  // Alternatively, one can directly get the underlying RevRepository<int> with
  // a call to model.Get<>(), and use SaveWithStamp() before each modification
  // to have just a slight overhead per int updates. This later option is what
  // is usually done in a CP solver at the cost of a sligthly more complex API.
  void RegisterReversibleInt(int id, int* rev);

  // Returns the number of registered propagators.
  int NumPropagators() const { return in_queue_.size(); }

  // Set a callback for new variable bounds at level 0.
  //
  // This will be called (only at level zero) with the list
  // of IntegerVariable with changed lower bounds. Note that it
  // might be called more than once during the same propagation
  // cycle if we fix variables in "stages".
  void RegisterLevelZeroModifiedVariablesCallback(
      const std::function<void(const std::vector<IntegerVariable>&)> cb) {
    level_zero_modified_variable_callback_.push_back(cb);
  }

  // Returns the id of the propagator we are currently calling. This is meant
  // to be used from inside Propagate() in case a propagator was registered
  // more than once at different priority for instance.
  int GetCurrentId() const { return current_id_; }

 private:
  // Updates queue_ and in_queue_ with the propagator ids that need to be
  // called.
  void UpdateCallingNeeds(Trail* trail);

  TimeLimit* time_limit_;
  IntegerTrail* integer_trail_;
  RevIntRepository* rev_int_repository_;

  struct WatchData {
    int id;
    int watch_index;
  };
  gtl::ITIVector<LiteralIndex, std::vector<WatchData>> literal_to_watcher_;
  gtl::ITIVector<IntegerVariable, std::vector<WatchData>> var_to_watcher_;
  std::vector<PropagatorInterface*> watchers_;
  SparseBitset<IntegerVariable> modified_vars_;

  // Propagator ids that needs to be called. There is one queue per priority but
  // just one Boolean to indicate if a propagator is in one of them.
  std::vector<std::deque<int>> queue_by_priority_;
  std::vector<bool> in_queue_;

  // Data for each propagator.
  std::vector<int> id_to_level_at_last_call_;
  std::vector<int> id_to_greatest_common_level_since_last_call_;
  std::vector<std::vector<ReversibleInterface*>> id_to_reversible_classes_;
  std::vector<std::vector<int*>> id_to_reversible_ints_;
  std::vector<std::vector<int>> id_to_watch_indices_;
  std::vector<int> id_to_priority_;
  std::vector<int> id_to_idempotence_;

  // Special propagators that needs to always be called at level zero.
  std::vector<int> propagator_ids_to_call_at_level_zero_;

  // The id of the propagator we just called.
  int current_id_;

  std::vector<std::function<void(const std::vector<IntegerVariable>&)>>
      level_zero_modified_variable_callback_;

  DISALLOW_COPY_AND_ASSIGN(GenericLiteralWatcher);
};

// ============================================================================
// Implementation.
// ============================================================================

inline IntegerLiteral IntegerLiteral::GreaterOrEqual(IntegerVariable i,
                                                     IntegerValue bound) {
  return IntegerLiteral(
      i, bound > kMaxIntegerValue ? kMaxIntegerValue + 1 : bound);
}

inline IntegerLiteral IntegerLiteral::LowerOrEqual(IntegerVariable i,
                                                   IntegerValue bound) {
  return IntegerLiteral(
      NegationOf(i), bound < kMinIntegerValue ? kMaxIntegerValue + 1 : -bound);
}

inline IntegerLiteral IntegerLiteral::Negated() const {
  // Note that bound >= kMinIntegerValue, so -bound + 1 will have the correct
  // capped value.
  return IntegerLiteral(
      NegationOf(IntegerVariable(var)),
      bound > kMaxIntegerValue ? kMinIntegerValue : -bound + 1);
}

inline IntegerValue IntegerTrail::LowerBound(IntegerVariable i) const {
  return vars_[i].current_bound;
}

inline IntegerValue IntegerTrail::UpperBound(IntegerVariable i) const {
  return -vars_[NegationOf(i)].current_bound;
}

inline IntegerLiteral IntegerTrail::LowerBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::GreaterOrEqual(i, LowerBound(i));
}

inline IntegerLiteral IntegerTrail::UpperBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::LowerOrEqual(i, UpperBound(i));
}

inline bool IntegerTrail::IntegerLiteralIsTrue(IntegerLiteral l) const {
  return l.bound <= LowerBound(l.var);
}

inline bool IntegerTrail::IntegerLiteralIsFalse(IntegerLiteral l) const {
  return l.bound > UpperBound(l.var);
}

// The level zero bounds are stored at the beginning of the trail and they also
// serves as sentinels. Their index match the variables index.
inline IntegerValue IntegerTrail::LevelZeroLowerBound(
    IntegerVariable var) const {
  return integer_trail_[var.value()].bound;
}

inline IntegerValue IntegerTrail::LevelZeroUpperBound(
    IntegerVariable var) const {
  return -integer_trail_[NegationOf(var).value()].bound;
}

inline void GenericLiteralWatcher::WatchLiteral(Literal l, int id,
                                                int watch_index) {
  if (l.Index() >= literal_to_watcher_.size()) {
    literal_to_watcher_.resize(l.Index().value() + 1);
  }
  literal_to_watcher_[l.Index()].push_back({id, watch_index});
}

inline void GenericLiteralWatcher::WatchLowerBound(IntegerVariable var, int id,
                                                   int watch_index) {
  if (var.value() >= var_to_watcher_.size()) {
    var_to_watcher_.resize(var.value() + 1);
  }
  var_to_watcher_[var].push_back({id, watch_index});
}

inline void GenericLiteralWatcher::WatchUpperBound(IntegerVariable var, int id,
                                                   int watch_index) {
  WatchLowerBound(NegationOf(var), id, watch_index);
}

inline void GenericLiteralWatcher::WatchIntegerVariable(IntegerVariable i,
                                                        int id,
                                                        int watch_index) {
  WatchLowerBound(i, id, watch_index);
  WatchUpperBound(i, id, watch_index);
}

// ============================================================================
// Model based functions.
//
// Note that in the model API, we simply use int64 for the integer values, so
// that it is nicer for the client. Internally these are converted to
// IntegerValue which is typechecked.
// ============================================================================

inline std::function<BooleanVariable(Model*)> NewBooleanVariable() {
  return [=](Model* model) {
    return model->GetOrCreate<SatSolver>()->NewBooleanVariable();
  };
}

inline std::function<IntegerVariable(Model*)> NewIntegerVariable() {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable();
  };
}

inline std::function<IntegerVariable(Model*)> ConstantIntegerVariable(
    int64 value) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()
        ->GetOrCreateConstantIntegerVariable(IntegerValue(value));
  };
}

inline std::function<IntegerVariable(Model*)> NewIntegerVariable(int64 lb,
                                                                 int64 ub) {
  return [=](Model* model) {
    CHECK_LE(lb, ub);
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable(
        IntegerValue(lb), IntegerValue(ub));
  };
}

inline std::function<IntegerVariable(Model*)> NewIntegerVariable(
    const Domain& domain) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable(domain);
  };
}

// Creates a 0-1 integer variable "view" of the given literal. It will have a
// value of 1 when the literal is true, and 0 when the literal is false.
inline std::function<IntegerVariable(Model*)> NewIntegerVariableFromLiteral(
    Literal lit) {
  return [=](Model* model) {
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    const IntegerVariable candidate = encoder->GetLiteralView(lit);
    if (candidate != kNoIntegerVariable) return candidate;

    IntegerVariable var;
    const auto& assignment = model->GetOrCreate<SatSolver>()->Assignment();
    if (assignment.LiteralIsTrue(lit)) {
      var = model->Add(ConstantIntegerVariable(1));
    } else if (assignment.LiteralIsFalse(lit)) {
      var = model->Add(ConstantIntegerVariable(0));
    } else {
      var = model->Add(NewIntegerVariable(0, 1));
    }

    encoder->AssociateToIntegerEqualValue(lit, var, IntegerValue(1));
    DCHECK_NE(encoder->GetLiteralView(lit), kNoIntegerVariable);
    return var;
  };
}

inline std::function<int64(const Model&)> LowerBound(IntegerVariable v) {
  return [=](const Model& model) {
    return model.Get<IntegerTrail>()->LowerBound(v).value();
  };
}

inline std::function<int64(const Model&)> UpperBound(IntegerVariable v) {
  return [=](const Model& model) {
    return model.Get<IntegerTrail>()->UpperBound(v).value();
  };
}

inline std::function<bool(const Model&)> IsFixed(IntegerVariable v) {
  return [=](const Model& model) {
    const IntegerTrail* trail = model.Get<IntegerTrail>();
    return trail->LowerBound(v) == trail->UpperBound(v);
  };
}

// This checks that the variable is fixed.
inline std::function<int64(const Model&)> Value(IntegerVariable v) {
  return [=](const Model& model) {
    const IntegerTrail* trail = model.Get<IntegerTrail>();
    CHECK_EQ(trail->LowerBound(v), trail->UpperBound(v)) << v;
    return trail->LowerBound(v).value();
  };
}

inline std::function<void(Model*)> GreaterOrEqual(IntegerVariable v, int64 lb) {
  return [=](Model* model) {
    if (!model->GetOrCreate<IntegerTrail>()->Enqueue(
            IntegerLiteral::GreaterOrEqual(v, IntegerValue(lb)),
            std::vector<Literal>(), std::vector<IntegerLiteral>())) {
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
      LOG(WARNING) << "Model trivially infeasible, variable " << v
                   << " has upper bound " << model->Get(UpperBound(v))
                   << " and GreaterOrEqual() was called with a lower bound of "
                   << lb;
    }
  };
}

inline std::function<void(Model*)> LowerOrEqual(IntegerVariable v, int64 ub) {
  return [=](Model* model) {
    if (!model->GetOrCreate<IntegerTrail>()->Enqueue(
            IntegerLiteral::LowerOrEqual(v, IntegerValue(ub)),
            std::vector<Literal>(), std::vector<IntegerLiteral>())) {
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
      LOG(WARNING) << "Model trivially infeasible, variable " << v
                   << " has lower bound " << model->Get(LowerBound(v))
                   << " and LowerOrEqual() was called with an upper bound of "
                   << ub;
    }
  };
}

// Fix v to a given value.
inline std::function<void(Model*)> Equality(IntegerVariable v, int64 value) {
  return [=](Model* model) {
    model->Add(LowerOrEqual(v, value));
    model->Add(GreaterOrEqual(v, value));
  };
}

// TODO(user): This is one of the rare case where it is better to use Equality()
// rather than two Implications(). Maybe we should modify our internal
// implementation to use half-reified encoding? that is do not propagate the
// direction integer-bound => literal, but just literal => integer-bound? This
// is the same as using different underlying variable for an integer literal and
// its negation.
inline std::function<void(Model*)> Implication(
    const std::vector<Literal>& enforcement_literals, IntegerLiteral i) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    if (i.bound <= integer_trail->LowerBound(i.var)) {
      // Always true! nothing to do.
    } else if (i.bound > integer_trail->UpperBound(i.var)) {
      // Always false.
      std::vector<Literal> clause;
      for (const Literal literal : enforcement_literals) {
        clause.push_back(literal.Negated());
      }
      model->Add(ClauseConstraint(clause));
    } else {
      // TODO(user): Double check what happen when we associate a trivially
      // true or false literal.
      IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
      std::vector<Literal> clause{encoder->GetOrCreateAssociatedLiteral(i)};
      for (const Literal literal : enforcement_literals) {
        clause.push_back(literal.Negated());
      }
      model->Add(ClauseConstraint(clause));
    }
  };
}

// in_interval => v in [lb, ub].
inline std::function<void(Model*)> ImpliesInInterval(Literal in_interval,
                                                     IntegerVariable v,
                                                     int64 lb, int64 ub) {
  return [=](Model* model) {
    if (lb == ub) {
      IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
      model->Add(Implication({in_interval},
                             encoder->GetOrCreateLiteralAssociatedToEquality(
                                 v, IntegerValue(lb))));
      return;
    }
    model->Add(Implication(
        {in_interval}, IntegerLiteral::GreaterOrEqual(v, IntegerValue(lb))));
    model->Add(Implication({in_interval},
                           IntegerLiteral::LowerOrEqual(v, IntegerValue(ub))));
  };
}

// Calling model.Add(FullyEncodeVariable(var)) will create one literal per value
// in the domain of var (if not already done), and wire everything correctly.
// This also returns the full encoding, see the FullDomainEncoding() method of
// the IntegerEncoder class.
inline std::function<std::vector<IntegerEncoder::ValueLiteralPair>(Model*)>
FullyEncodeVariable(IntegerVariable var) {
  return [=](Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    if (!encoder->VariableIsFullyEncoded(var)) {
      encoder->FullyEncodeVariable(var);
    }
    return encoder->FullDomainEncoding(var);
  };
}

// Same as ExcludeCurrentSolutionAndBacktrack() but this version works for an
// integer problem with optional variables. The issue is that an optional
// variable that is ignored can basically take any value, and we don't really
// want to enumerate them. This function should exclude all solutions where
// only the ignored variable values change.
std::function<void(Model*)>
ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack();

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_H_
