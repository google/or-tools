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

#ifndef OR_TOOLS_SAT_INTEGER_H_
#define OR_TOOLS_SAT_INTEGER_H_

#include <deque>
#include <functional>
#include <unordered_map>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/inlined_vector.h"
#include "ortools/base/join.h"
#include "ortools/base/span.h"
#include "ortools/graph/iterators.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
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
const IntegerValue kMaxIntegerValue(
    std::numeric_limits<IntegerValue::ValueType>::max() - 1);
const IntegerValue kMinIntegerValue(-kMaxIntegerValue);

// IntegerValue version of the function in saturated_arithmetic.h
//
// The functions are not "sticky" to the min/max possible values so it is up to
// us to properly use them so that we never get an overflow and then go back to
// a feasible value. Hence the DCHECK().
inline IntegerValue CapAdd(IntegerValue a, IntegerValue b) {
  DCHECK(a >= kMinIntegerValue || b <= 0) << "Adding wrong sign to overflow.";
  DCHECK(a <= kMaxIntegerValue || b >= 0) << "Adding wrong sign to overflow.";
  DCHECK(b >= kMinIntegerValue || a <= 0) << "Adding wrong sign to overflow.";
  DCHECK(b <= kMaxIntegerValue || a >= 0) << "Adding wrong sign to overflow.";
  return IntegerValue(operations_research::CapAdd(a.value(), b.value()));
}
inline IntegerValue CapSub(IntegerValue a, IntegerValue b) {
  DCHECK(a >= kMinIntegerValue || b >= 0) << "Adding wrong sign to overflow.";
  DCHECK(a <= kMaxIntegerValue || b <= 0) << "Adding wrong sign to overflow.";
  DCHECK(b >= kMinIntegerValue || a >= 0) << "Adding wrong sign to overflow.";
  DCHECK(b <= kMaxIntegerValue || a <= 0) << "Adding wrong sign to overflow.";
  return IntegerValue(operations_research::CapSub(a.value(), b.value()));
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
               ? StrCat("I", var.value() / 2, ">=", bound.value())
               : StrCat("I", var.value() / 2, "<=", -bound.value());
  }

  // Note that bound should be in [kMinIntegerValue, kMaxIntegerValue + 1].
  IntegerVariable var;
  IntegerValue bound;
};

inline std::ostream& operator<<(std::ostream& os, IntegerLiteral i_lit) {
  os << i_lit.DebugString();
  return os;
}

using InlinedIntegerLiteralVector = gtl::InlinedVector<IntegerLiteral, 2>;

// A singleton that holds the INITIAL integer variable domains.
struct IntegerDomains
    : public ITIVector<IntegerVariable,
                       gtl::InlinedVector<ClosedInterval, 1>> {
  explicit IntegerDomains(Model* model) {}
};

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

  // Computes the full encoding of a variable on which FullyEncodeVariable() has
  // been called. The returned elements are always sorted by increasing
  // IntegerValue and we filter values associated to false literals.
  struct ValueLiteralPair {
    ValueLiteralPair(IntegerValue v, Literal l) : value(v), literal(l) {}
    bool operator==(const ValueLiteralPair& o) const {
      return value == o.value && literal == o.literal;
    }
    IntegerValue value;
    Literal literal;
  };
  std::vector<ValueLiteralPair> FullDomainEncoding(IntegerVariable var) const;

  // Returns true if a variable is fully encoded.
  bool VariableIsFullyEncoded(IntegerVariable var) const {
    if (var >= is_fully_encoded_.size()) return false;
    return is_fully_encoded_[var];
  }

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

  // Returns a Boolean literal associated with a bound lower than or equal to
  // the one of the given IntegerLiteral. If the given IntegerLiteral is true,
  // then the returned literal should be true too. Returns kNoLiteralIndex if no
  // such literal was created.
  //
  // Ex: if 'i' is (x >= 4) and we already created a literal associated to
  // (x >= 2) but not to (x >= 3), we will return the literal associated with
  // (x >= 2).
  LiteralIndex SearchForLiteralAtOrBefore(IntegerLiteral i) const;

  // Get the literal always set to true, make it if it does not exist.
  Literal GetLiteralTrue() {
    DCHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
    if (literal_index_true_ == kNoLiteralIndex) {
      const Literal literal_true =
          Literal(sat_solver_->NewBooleanVariable(), true);
      literal_index_true_ = literal_true.Index();
      sat_solver_->AddUnitClause(literal_true);
    }
    return Literal(literal_index_true_);
  }

 private:
  // Only add the equivalence between i_lit and literal, if there is already an
  // associated literal with i_lit, this make literal and this associated
  // literal equivalent.
  void HalfAssociateGivenLiteral(IntegerLiteral i_lit, Literal literal);

  // Adds the new associated_lit to encoding_by_var_.
  // Adds the implications: Literal(before) <= associated_lit <= Literal(after).
  void AddImplications(IntegerLiteral i, Literal associated_lit);

  SatSolver* sat_solver_;
  IntegerDomains* domains_;

  bool add_implications_ = true;
  int64 num_created_variables_ = 0;

  // We keep all the literals associated to an Integer variable in a map ordered
  // by bound (so we can properly add implications between the literals
  // corresponding to the same variable).
  ITIVector<IntegerVariable, std::map<IntegerValue, Literal>> encoding_by_var_;

  // Store for a given LiteralIndex the list of its associated IntegerLiterals.
  const InlinedIntegerLiteralVector empty_integer_literal_vector_;
  ITIVector<LiteralIndex, InlinedIntegerLiteralVector> reverse_encoding_;

  // Mapping (variable == value) -> associated literal. Note that even if
  // there is more than one literal associated to the same fact, we just keep
  // the first one that was added.
  std::unordered_map<std::pair<IntegerVariable, IntegerValue>, Literal>
      equality_to_associated_literal_;

  // Variables that are fully encoded.
  ITIVector<IntegerVariable, bool> is_fully_encoded_;

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
  gtl::Span<Literal> Reason(const Trail& trail,
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
  // disjoint intervals. Note that the ClosedInterval struct use int64 instead
  // of integer values (but we will convert them internally).
  //
  // Precondition: we check that IntervalsAreSortedAndDisjoint(domain) is true.
  IntegerVariable AddIntegerVariable(const std::vector<ClosedInterval>& domain);

  // Returns the initial domain of the given variable. Note that for variables
  // whose domain is a single interval, this is updated with level zero
  // propagations, but not if the domain is more complex.
  std::vector<ClosedInterval> InitialVariableDomain(IntegerVariable var) const;

  // Takes the intersection with the current initial variable domain.
  //
  // TODO(user): There is some memory inefficiency if this is called many time
  // because of the underlying data structure we use. In practice, when used
  // with a presolve, this is not often used, so that is fine though.
  //
  // TODO(user): The Enqueue() done at level zero on a variable are not
  // reflected on its initial domain. That can causes issue if the variable
  // is fully encoded afterwards because literals will be created for the values
  // no longer relevant, and these will not be propagated right away.
  bool UpdateInitialDomain(IntegerVariable var,
                           std::vector<ClosedInterval> domain);

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
  void MarkIntegerVariableAsOptional(IntegerVariable i, Literal is_considered) {
    is_ignored_literals_[i] = is_considered.NegatedIndex();
    is_ignored_literals_[NegationOf(i)] = is_considered.NegatedIndex();
  }

  // Returns the current lower/upper bound of the given integer variable.
  IntegerValue LowerBound(IntegerVariable i) const;
  IntegerValue UpperBound(IntegerVariable i) const;

  // Returns the value of the lower bound before the last Enqueue() that changed
  // it. Note that PreviousLowerBound() == LowerBound() iff this is the level
  // zero bound.
  IntegerValue PreviousLowerBound(IntegerVariable i) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given integer variable.
  IntegerLiteral LowerBoundAsLiteral(IntegerVariable i) const;
  IntegerLiteral UpperBoundAsLiteral(IntegerVariable i) const;

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
  // TODO(user): provide an API to give the reason lazily.
  //
  // TODO(user): If the given bound is equal to the current bound, maybe the new
  // reason is better? how to decide and what to do in this case? to think about
  // it. Currently we simply don't do anything.
  MUST_USE_RESULT bool Enqueue(IntegerLiteral i_lit,
                               gtl::Span<Literal> literal_reason,
                               gtl::Span<IntegerLiteral> integer_reason);

  // Same as Enqueue(), but takes an extra argument which if smaller than
  // integer_trail_.size() is interpreted as the trail index of an old Enqueue()
  // that had the same reason as this one. Note that the given Span must still
  // be valid as they are used in case of conflict.
  MUST_USE_RESULT bool Enqueue(IntegerLiteral i_lit,
                               gtl::Span<Literal> literal_reason,
                               gtl::Span<IntegerLiteral> integer_reason,
                               int trail_index_with_same_reason);

  // Enqueues the given literal on the trail.
  // See the comment of Enqueue() for the reason format.
  void EnqueueLiteral(Literal literal, gtl::Span<Literal> literal_reason,
                      gtl::Span<IntegerLiteral> integer_reason);

  // Returns the reason (as set of Literal currently false) for a given integer
  // literal. Note that the bound must be less restrictive than the current
  // bound (checked).
  std::vector<Literal> ReasonFor(IntegerLiteral bound) const;

  // Appends the reason for the given integer literals to the output and call
  // STLSortAndRemoveDuplicates() on it.
  void MergeReasonInto(gtl::Span<IntegerLiteral> bounds,
                       std::vector<Literal>* output) const;

  // Returns the number of enqueues that changed a variable bounds. We don't
  // count enqueues called with a less restrictive bound than the current one.
  //
  // Note(user): this can be used to see if any of the bounds changed. Just
  // looking at the integer trail index is not enough because at level zero it
  // doesn't change since we directly update the "fixed" bounds.
  int64 num_enqueues() const { return num_enqueues_; }

  // All the registered bitsets will be set to one each time a LbVar is
  // modified. It is up to the client to clear it if it wants to be notified
  // with the newly modified variables.
  void RegisterWatcher(SparseBitset<IntegerVariable>* p) {
    p->ClearAndResize(NumIntegerVariables());
    watchers_.push_back(p);
  }

  // Helper functions to report a conflict. Always return false so a client can
  // simply do: return integer_trail_->ReportConflict(...);
  bool ReportConflict(gtl::Span<Literal> literal_reason,
                      gtl::Span<IntegerLiteral> integer_reason) {
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->assign(literal_reason.begin(), literal_reason.end());
    MergeReasonInto(integer_reason, conflict);
    return false;
  }
  bool ReportConflict(gtl::Span<IntegerLiteral> integer_reason) {
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

 private:
  // Tests that all the literals in the given reason are assigned to false.
  // This is used to DCHECK the given reasons to the Enqueue*() functions.
  bool AllLiteralsAreFalse(gtl::Span<Literal> literals) const;

  // Does the work of MergeReasonInto() when queue_ is already initialized.
  void MergeReasonIntoInternal(std::vector<Literal>* output) const;

  // Helper used by Enqueue() to propagate one of the literal associated to
  // an integer literal and maintained by encoder_.
  bool EnqueueAssociatedLiteral(Literal literal,
                                int trail_index_with_same_reason,
                                gtl::Span<Literal> literal_reason,
                                gtl::Span<IntegerLiteral> integer_reason,
                                BooleanVariable* variable_with_same_reason);

  // Returns a lower bound on the given var that will always be valid.
  IntegerValue LevelZeroBound(IntegerVariable var) const {
    // The level zero bounds are stored at the beginning of the trail and they
    // also serves as sentinels. Their index match the variables index.
    return integer_trail_[var.value()].bound;
  }

  // Returns the lowest trail index of a TrailEntry that can be used to explain
  // the given IntegerLiteral. The literal must be currently true (CHECKed).
  // Returns -1 if the explanation is trivial.
  int FindLowestTrailIndexThatExplainBound(IntegerLiteral bound) const;

  // Helper function to return the "dependencies" of a bound assignment.
  // All the TrailEntry at these indices are part of the reason for this
  // assignment.
  util::BeginEndWrapper<std::vector<IntegerLiteral>::const_iterator>
  Dependencies(int trail_index) const;

  // Helper function to append the Literal part of the reason for this bound
  // assignment.
  void AppendLiteralsReason(int trail_index,
                            std::vector<Literal>* output) const;

  // Returns some debuging info.
  std::string DebugString();

  // Information for each internal variable about its current bound.
  struct VarInfo {
    // The current bound on this variable.
    IntegerValue current_bound;

    // Trail index of the last TrailEntry in the trail refering to this var.
    int current_trail_index;
  };
  ITIVector<IntegerVariable, VarInfo> vars_;

  // This is used by FindLowestTrailIndexThatExplainBound() to speed up
  // the lookup. It keeps a trail index for each variable that may or may not
  // point to a TrailEntry regarding this variable. The validity of the index is
  // verified before beeing used.
  mutable ITIVector<IntegerVariable, int> var_trail_index_cache_;

  // Used by GetOrCreateConstantIntegerVariable() to return already created
  // constant variables that share the same value.
  std::unordered_map<IntegerValue, IntegerVariable> constant_map_;

  // The integer trail. It always start by num_vars sentinel values with the
  // level 0 bounds (in one to one correspondence with vars_).
  struct TrailEntry {
    IntegerValue bound;
    IntegerVariable var;
    int32 prev_trail_index;

    // Index in literals_reason_start_/bounds_reason_starts_
    int32 reason_index;
  };
  std::vector<TrailEntry> integer_trail_;

  // Start of each decision levels in integer_trail_.
  // TODO(user): use more general reversible mechanism?
  std::vector<int> integer_decision_levels_;

  // Buffer to store the reason of each trail entry.
  // Note that bounds_reason_buffer_ is an "union". It initially contains the
  // IntegerLiteral, and is lazily replaced by the result of
  // FindLowestTrailIndexThatExplainBound() applied to these literals. The
  // encoding is a bit hacky, see Dependencies().
  std::vector<int> reason_decision_levels_;
  std::vector<int> literals_reason_starts_;
  std::vector<int> bounds_reason_starts_;
  std::vector<Literal> literals_reason_buffer_;
  mutable std::vector<IntegerLiteral> bounds_reason_buffer_;

  // The "is_ignored" literal of the optional variables or kNoLiteralIndex.
  ITIVector<IntegerVariable, LiteralIndex> is_ignored_literals_;

  // This is only filled for variables with a domain more complex than a single
  // interval of values. All intervals are stored in a vector, and we keep
  // indices to the current interval of the lower bound, and to the end index
  // which is exclusive.
  //
  // TODO(user): Avoid using hash_map here and above, a simple vector should
  // be more efficient. Except if there is really little variables like this.
  //
  // TODO(user): We could share the std::vector<ClosedInterval> entry between a
  // variable and its negations instead of having duplicates.
  RevMap<std::unordered_map<IntegerVariable, int>> var_to_current_lb_interval_index_;
  std::unordered_map<IntegerVariable, int> var_to_end_interval_index_;  // const entries.
  std::vector<ClosedInterval> all_intervals_;                 // const entries.

  // Temporary data used by MergeReasonInto().
  mutable std::vector<int> tmp_queue_;
  mutable std::vector<IntegerVariable> tmp_to_clear_;
  mutable ITIVector<IntegerVariable, int> tmp_var_to_trail_index_in_queue_;

  // For EnqueueLiteral(), we store a special TrailEntry to recover the reason
  // lazily. This vector indicates the correspondence between a literal that
  // was pushed by this class at a given trail index, and the index of its
  // TrailEntry in integer_trail_.
  std::vector<int> boolean_trail_index_to_integer_one_;

  int64 num_enqueues_;

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
  //   that where already fixed when the propagator was registered. Only the
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

  // Watches the corresponding quantity. The propagator with given id will be
  // called if it changes. Note that WatchLiteral() only trigger when the
  // literal becomes true.
  //
  // If watch_index is specified, it is associated with the watched literal.
  // Doing this will cause IncrementalPropagate() to be called (see the
  // documentation of this interface for more detail).
  void WatchLiteral(Literal l, int id, int watch_index = -1);
  void WatchLowerBound(IntegerVariable i, int id, int watch_index = -1);
  void WatchUpperBound(IntegerVariable i, int id, int watch_index = -1);
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

 private:
  // Updates queue_ and in_queue_ with the propagator ids that need to be
  // called.
  void UpdateCallingNeeds(Trail* trail);

  IntegerTrail* integer_trail_;
  RevIntRepository* rev_int_repository_;

  struct WatchData {
    int id;
    int watch_index;
  };
  ITIVector<LiteralIndex, std::vector<WatchData>> literal_to_watcher_;
  ITIVector<IntegerVariable, std::vector<WatchData>> var_to_watcher_;
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

inline IntegerValue IntegerTrail::PreviousLowerBound(IntegerVariable i) const {
  const int index = vars_[i].current_trail_index;
  if (index < vars_.size()) return LowerBound(i);
  return integer_trail_[integer_trail_[index].prev_trail_index].bound;
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
    const std::vector<ClosedInterval>& domain) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable(domain);
  };
}

// Constraints might not accept Literals as input, forcing the user to pass
// 0-1 integer views of a literal.
// This class contains all such literal views of a given model, so that asking
// for a view of a literal will always return the same IntegerVariable.
class LiteralViews {
 public:
  explicit LiteralViews(Model* model) : model_(model) {}

  IntegerVariable GetIntegerView(const Literal lit) {
    const LiteralIndex index = lit.Index();

    if (!ContainsKey(literal_index_to_integer_, index)) {
      const IntegerVariable int_var = model_->Add(NewIntegerVariable(0, 1));
      model_->GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
          lit, int_var, IntegerValue(1));
      literal_index_to_integer_[index] = int_var;
    }

    return literal_index_to_integer_[index];
  }

 private:
  std::unordered_map<LiteralIndex, IntegerVariable> literal_index_to_integer_;
  Model* model_;
};

// Creates a 0-1 integer variable "view" of the given literal. It will have a
// value of 1 when the literal is true, and 0 when the literal is false.
inline std::function<IntegerVariable(Model*)> NewIntegerVariableFromLiteral(
    Literal lit) {
  return [=](Model* model) {
    const auto& assignment = model->GetOrCreate<SatSolver>()->Assignment();
    if (assignment.LiteralIsTrue(lit)) {
      return model->Add(ConstantIntegerVariable(1));
    } else if (assignment.LiteralIsFalse(lit)) {
      return model->Add(ConstantIntegerVariable(0));
    } else {
      return model->GetOrCreate<LiteralViews>()->GetIntegerView(lit);
    }
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
inline std::function<void(Model*)> Implication(Literal l, IntegerLiteral i) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    if (i.bound <= integer_trail->LowerBound(i.var)) {
      // Always true! nothing to do.
    } else if (i.bound > integer_trail->UpperBound(i.var)) {
      // Always false.
      model->Add(ClauseConstraint({l.Negated()}));
    } else {
      // TODO(user): Double check what happen when we associate a trivially
      // true or false literal.
      IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
      const Literal current = encoder->GetOrCreateAssociatedLiteral(i);
      model->Add(Implication(l, current));
    }
  };
}

// in_interval => v in [lb, ub].
inline std::function<void(Model*)> ImpliesInInterval(Literal in_interval,
                                                     IntegerVariable v,
                                                     int64 lb, int64 ub) {
  return [=](Model* model) {
    model->Add(Implication(
        in_interval, IntegerLiteral::GreaterOrEqual(v, IntegerValue(lb))));
    model->Add(Implication(in_interval,
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

// A wrapper around SatSolver::Solve that handles integer variable with lazy
// encoding. Repeatedly calls SatSolver::Solve() on the model until the given
// next_decision() function return kNoLiteralIndex or the model is proved to
// be UNSAT.
//
// Returns the status of the last call to SatSolver::Solve().
//
// Note that the next_decision() function must always return an unassigned
// literal or kNoLiteralIndex to end the search.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    const std::vector<Literal>& assumptions,
    const std::function<LiteralIndex()>& next_decision, Model* model);

// Shortcut for SolveIntegerProblemWithLazyEncoding() when there is no
// assumption and we consider all variables in their index order for the next
// search decision.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Returns a
// function that will return the literal corresponding to the fact that the
// first currently non-fixed variable value is <= its min. The function will
// return kNoLiteralIndex if all the given variables are fixed.
//
// Note that this function will create the associated literal if needed.
std::function<LiteralIndex()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model);

// Decision heuristic for SolveIntegerProblemWithLazyEncoding(). Like
// FirstUnassignedVarAtItsMinHeuristic() but the function will return the
// literal corresponding to the fact that the currently non-assigned variable
// with the lowest min has a value <= this min.
std::function<LiteralIndex()> UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model);

// Combines search heuristics in order: if the i-th one returns kNoLiteralIndex,
// ask the (i+1)-th. If every heuristic returned kNoLiteralIndex,
// returns kNoLiteralIndex.
std::function<LiteralIndex()> SequentialSearch(
    std::vector<std::function<LiteralIndex()>> heuristics);

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
