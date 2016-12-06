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

#ifndef OR_TOOLS_SAT_INTEGER_H_
#define OR_TOOLS_SAT_INTEGER_H_

#include <queue>
#include <set>

#include "base/port.h"
#include "base/join.h"
#include "base/int_type.h"
#include "base/map_util.h"
#include "sat/model.h"
#include "sat/sat_base.h"
#include "sat/sat_solver.h"
#include "util/bitset.h"
#include "util/iterators.h"
#include "util/rev.h"
#include "util/saturated_arithmetic.h"

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

class IntegerEncoder;
class IntegerTrail;

// The integer equivalent of a literal.
// It represents an IntegerVariable and an upper/lower bound on it.
//
// Overflow: all the bounds below kMinIntegerValue and kMaxIntegerValue are
// treated as kMinIntegerValue - 1 and kMaxIntegerValue + 1.
struct IntegerLiteral {
  // This default constructor is needed for std::vector<IntegerLiteral>.
  IntegerLiteral() : var(-1), bound(0) {}

  // Because IntegerLiteral should never be created at a bound less constrained
  // than an existing IntegerVariable bound, we don't allow GreaterOrEqual() to
  // have a bound lower than kMinIntegerValue, and LowerOrEqual() to have a
  // bound greater than kMaxIntegerValue. The other side is not constrained
  // to allow for a computed bound to overflow. Note that both the full initial
  // domain and the empty domain can always be represented.
  static IntegerLiteral GreaterOrEqual(IntegerVariable i, IntegerValue bound);
  static IntegerLiteral LowerOrEqual(IntegerVariable i, IntegerValue bound);

  // The negation of x >= bound is x <= bound - 1.
  IntegerLiteral Negated() const;

  bool operator==(IntegerLiteral o) const {
    return var == o.var && bound == o.bound;
  }
  bool operator!=(IntegerLiteral o) const {
    return var != o.var || bound == o.bound;
  }

  std::string DebugString() const { return StrCat("I", var, ">=", bound.value()); }

 private:
  friend class IntegerEncoder;
  friend class IntegerTrail;

  IntegerLiteral(IntegerVariable v, IntegerValue b) : var(v.value()), bound(b) {
    DCHECK_GE(bound, kMinIntegerValue);
    DCHECK_LE(bound, kMaxIntegerValue + 1);
  }

  // Our external API uses IntegerVariable but internally we only use an int for
  // simplicity. TODO(user): change this?
  //
  // TODO(user): We can't use const because we want to be able to copy a
  // std::vector<IntegerLiteral>. So instead make them private and provide some
  // getters.
  //
  // Note that bound is always in [kMinIntegerValue, kMaxIntegerValue + 1].
  /*const*/ int var;
  /*const*/ IntegerValue bound;
};

inline std::ostream& operator<<(std::ostream& os, IntegerLiteral i_lit) {
  os << i_lit.DebugString();
  return os;
}

using InlinedIntegerLiteralVector = std::vector<IntegerLiteral>;

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
  explicit IntegerEncoder(SatSolver* sat_solver)
      : sat_solver_(sat_solver), num_created_variables_(0) {}

  ~IntegerEncoder() {
    VLOG(1) << "#variables created = " << num_created_variables_;
  }

  static IntegerEncoder* CreateInModel(Model* model) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntegerEncoder* encoder = new IntegerEncoder(sat_solver);
    model->TakeOwnership(encoder);
    return encoder;
  }

  // This has 3 effects:
  // 1/ It restricts the given variable to only take values amongst the given
  //    ones.
  // 2/ It creates one Boolean variable per value that convey the fact that the
  //    var is equal to this value iff the Boolean is true. If there is only
  //    2 values, then just one Boolean variable is created. For more than two
  //    values, a constraint is also added to enforce that exactly one Boolean
  //    variable is true.
  // 3/ The encoding for NegationOf(var) is automatically created too. It reuses
  //    the same Boolean variable as the encoding of var.
  //
  // Calling this more than once will take the intersection of all the given
  // values arguments. However, this is not optimal because the first calls may
  // creates new Boolean variables that will later be fixed, so we log a warning
  // when this happen. Ideally, the intersection should be done in a presolve
  // step to be as efficient as possible here.
  //
  // Note(user): There is currently no relation here between
  // FullyEncodeVariable() and CreateAssociatedLiteral(). However the
  // IntegerTrail class will automatically link the two representations and do
  // the right thing.
  //
  // Note(user): Calling this with just one value will cause a CHECK fail. One
  // need to fix the IntegerVariable inside the IntegerTrail instead of calling
  // this.
  //
  // TODO(user): It is currently only possible to call that at the decision
  // level zero. This is Checked.
  void FullyEncodeVariable(IntegerVariable var,
                           std::vector<IntegerValue> values);
  void FullyEncodeVariable(IntegerVariable var, IntegerValue lb,
                           IntegerValue ub);

  // Similar to FullyEncodeVariable() but use the given literal for each values.
  // This can only be called on variable that are not fully encoded yet, This is
  // checked.
  //
  // Note that duplicate values are supported, but exactly one literal must be
  // true at the same time. The "exactly one" constraint will implicitely be
  // enforced by the code in IntegerTrail.
  void FullyEncodeVariableUsingGivenLiterals(
      IntegerVariable var, const std::vector<Literal>& literals,
      const std::vector<IntegerValue>& values);

  // Gets the full encoding of a variable on which FullyEncodeVariable() has
  // been called. The returned elements are always sorted by increasing
  // IntegerValue. Once created, the encoding never changes, but some Boolean
  // variables may become fixed.
  //
  // IMPORTANT: the returned vector will be sorted by value, but may contain
  // duplicates values! IntegerTrail is doing the correct thing in this case.
  // This allows us to direcly support the "int element" constraint and its
  // variant at the core level.
  struct ValueLiteralPair {
    ValueLiteralPair(IntegerValue v, Literal l) : value(v), literal(l) {}
    bool operator==(const ValueLiteralPair& o) const {
      return value == o.value && literal == o.literal;
    }
    bool operator<(const ValueLiteralPair& o) const { return value < o.value; }
    IntegerValue value;
    Literal literal;
  };
  const std::vector<ValueLiteralPair>& FullDomainEncoding(
      IntegerVariable var) const {
    return full_encoding_[FindOrDie(full_encoding_index_, var)];
  }

  // Returns true if a variable is fully encoded.
  bool VariableIsFullyEncoded(IntegerVariable var) const {
    return ContainsKey(full_encoding_index_, var);
  }

  // Returns the set of variable encoded as the keys in a map. The map values
  // only have an internal meaning. The set of encoded variables is returned
  // with this "weird" api for efficiency.
  const hash_map<IntegerVariable, int>& GetFullyEncodedVariables() const {
    return full_encoding_index_;
  }

  // Creates a new Boolean variable 'var' such that
  // - if true, then the IntegerLiteral is true.
  // - if false, then the negated IntegerLiteral is true.
  //
  // Returns Literal(var, true).
  //
  // This add the proper implications with the two "neighbor" literals of this
  // one if they exist. This is the "list encoding" in: Thibaut Feydy, Peter J.
  // Stuckey, "Lazy Clause Generation Reengineered", CP 2009.
  //
  // It is an error to call this with an already created literal. This is
  // Checked.
  //
  // The second version use the given literal instead of creating a new
  // variable.
  Literal CreateAssociatedLiteral(IntegerLiteral i_lit);
  void AssociateGivenLiteral(IntegerLiteral i_lit, Literal wanted);

  // Return true iff the given integer literal is associated.
  bool LiteralIsAssociated(IntegerLiteral i_lit) const;

  // Same as CreateAssociatedLiteral() but safe to call if already created.
  Literal GetOrCreateAssociatedLiteral(IntegerLiteral i_lit);

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

 private:
  // Adds the new associated_lit to encoding_by_var_.
  // Adds the implications: Literal(before) <= associated_lit <= Literal(after).
  void AddImplications(IntegerLiteral i, Literal associated_lit);

  SatSolver* sat_solver_;
  int64 num_created_variables_;

  // We keep all the literals associated to an Integer variable in a map ordered
  // by bound (so we can properly add implications between the literals
  // corresponding to the same variable).
  ITIVector<IntegerVariable, std::map<IntegerValue, Literal>> encoding_by_var_;

  // Store for a given LiteralIndex the list of its associated IntegerLiterals.
  const InlinedIntegerLiteralVector empty_integer_literal_vector_;
  ITIVector<LiteralIndex, InlinedIntegerLiteralVector> reverse_encoding_;

  // Full domain encoding. The map contains the index in full_encoding_ of
  // the fully encoded variable. Each entry in full_encoding_ is sorted by
  // IntegerValue and contains the encoding of one IntegerVariable.
  hash_map<IntegerVariable, int> full_encoding_index_;
  std::vector<std::vector<ValueLiteralPair>> full_encoding_;

  DISALLOW_COPY_AND_ASSIGN(IntegerEncoder);
};

// This class maintains a set of integer variables with their current bounds.
// Bounds can be propagated from an external "source" and this class helps
// to maintain the reason for each propagation.
class IntegerTrail : public SatPropagator {
 public:
  IntegerTrail(IntegerEncoder* encoder, Trail* trail)
      : SatPropagator("IntegerTrail"),
        num_enqueues_(0),
        encoder_(encoder),
        trail_(trail) {}
  ~IntegerTrail() final {}

  static IntegerTrail* CreateInModel(Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = new IntegerTrail(encoder, trail);
    model->GetOrCreate<SatSolver>()->AddPropagator(
        std::unique_ptr<IntegerTrail>(integer_trail));
    return integer_trail;
  }

  // SatPropagator interface. These functions make sure the current bounds
  // information is in sync with the current solver literal trail. Any
  // class/propagator using this class must make sure it is synced to the
  // correct state before calling any of its functions.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;
  ClauseRef Reason(const Trail& trail, int trail_index) const final;

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

  // Same as AddIntegerVariable() but uses the maximum possible range. Note
  // that since we take negation of bounds in various places, we make sure that
  // we don't have overflow when we take the negation of the lower bound or of
  // the upper bound.
  IntegerVariable AddIntegerVariable() {
    return AddIntegerVariable(kMinIntegerValue, kMaxIntegerValue);
  }

  // The domain [lb, ub] of an "optional" variable is allowed to be empty (i.e.
  // ub < lb) iff the given is_empty literal is true.
  bool IsOptional(IntegerVariable i) const {
    return is_empty_literals_[i] != kNoLiteralIndex;
  }
  LiteralIndex GetIsEmptyLiteral(IntegerVariable i) const {
    return is_empty_literals_[i];
  }
  void MarkIntegerVariableAsOptional(IntegerVariable i, Literal is_non_empty) {
    is_empty_literals_[i] = is_non_empty.NegatedIndex();
    is_empty_literals_[NegationOf(i)] = is_non_empty.NegatedIndex();
  }

  // Returns the current lower/upper bound of the given integer variable.
  IntegerValue LowerBound(IntegerVariable i) const;
  IntegerValue UpperBound(IntegerVariable i) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given integer variable.
  IntegerLiteral LowerBoundAsLiteral(IntegerVariable i) const;
  IntegerLiteral UpperBoundAsLiteral(IntegerVariable i) const;

  // Enqueue new information about a variable bound. Note that this can be used
  // at the decision level zero to change the initial variable bounds, but only
  // to make them more restricted. Calling this with a less restrictive bound
  // than the current one will have no effect.
  //
  // The reason for this "assignment" can be a combination of:
  // - A set of Literal currently beeing all false.
  // - A set of IntegerLiteral currently beeing all satisfied.
  //
  // TODO(user): provide an API to give the reason lazily.
  //
  // TODO(user): change the Literal signs to all true? it is often confusing to
  // have all false as a reason. But this is kind of historical because of a
  // clause beeing a reason for an assignment when all but one of its literals
  // are false.
  //
  // TODO(user): If the given bound is equal to the current bound, maybe the new
  // reason is better? how to decide and what to do in this case? to think about
  // it. Currently we simply don't do anything.
  MUST_USE_RESULT bool Enqueue(
      IntegerLiteral bound, const std::vector<Literal>& literal_reason,
      const std::vector<IntegerLiteral>& integer_reason);

  // Enqueues the given literal on the trail.
  void EnqueueLiteral(Literal literal,
                      const std::vector<Literal>& literal_reason,
                      const std::vector<IntegerLiteral>& integer_reason);

  // Returns the reason (as set of Literal currently false) for a given integer
  // literal. Note that the bound must be less restrictive than the current
  // bound (checked).
  std::vector<Literal> ReasonFor(IntegerLiteral bound) const;

  // Appends the reason for the given integer literals to the output and call
  // STLSortAndRemoveDuplicates() on it.
  void MergeReasonInto(const std::vector<IntegerLiteral>& bounds,
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
  bool ReportConflict(const std::vector<Literal>& literal_reason,
                      const std::vector<IntegerLiteral>& integer_reason) {
    std::vector<Literal>* conflict = trail_->MutableConflict();
    *conflict = literal_reason;
    MergeReasonInto(integer_reason, conflict);
    return false;
  }
  bool ReportConflict(const std::vector<IntegerLiteral>& integer_reason) {
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->clear();
    MergeReasonInto(integer_reason, conflict);
    return false;
  }

  // Returns true if the variable lower bound is still the one from level zero.
  bool VariableLowerBoundIsFixed(IntegerVariable var) const {
    return vars_[var.value()].current_trail_index < vars_.size();
  }

 private:
  // Does the work of MergeReasonInto() when queue_ is already initialized.
  void MergeReasonIntoInternal(std::vector<Literal>* output) const;

  // Helper used by Enqueue() to propagate one of the literal associated to
  // the given i_lit and maintained by encoder_.
  bool EnqueueAssociatedLiteral(
      Literal literal, IntegerLiteral i_lit,
      const std::vector<Literal>& literals_reason,
      const std::vector<IntegerLiteral>& bounds_reason,
      BooleanVariable* variable_with_same_reason);

  // Returns a lower bound on the given var that will always be valid.
  IntegerValue LevelZeroBound(int var) const {
    // The level zero bounds are stored at the begining of the trail and they
    // also serves as sentinels. Their index match the variables index.
    return integer_trail_[var].bound;
  }

  // Returns the lowest trail index of a TrailEntry that can be used to explain
  // the given IntegerLiteral. The literal must be currently true (CHECKed).
  // Returns -1 if the explanation is trivial.
  int FindLowestTrailIndexThatExplainBound(IntegerLiteral bound) const;

  // Helper function to return the "dependencies" of a bound assignment.
  // All the TrailEntry at these indices are part of the reason for this
  // assignment.
  BeginEndWrapper<std::vector<IntegerLiteral>::const_iterator> Dependencies(
      int trail_index) const;

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
  std::vector<VarInfo> vars_;

  // The integer trail. It always start by num_vars sentinel values with the
  // level 0 bounds (in one to one correspondance with vars_).
  struct TrailEntry {
    IntegerValue bound;
    int32 var;
    int32 prev_trail_index;

    // Start index in the respective *_buffer_ vectors below.
    int32 literals_reason_start_index;
    int32 dependencies_start_index;
  };
  std::vector<TrailEntry> integer_trail_;

  // Start of each decision levels in integer_trail_.
  std::vector<int> integer_decision_levels_;

  // Buffer to store the reason of each trail entry.
  // Note that bounds_reason_buffer_ is an "union". It initially contains the
  // IntegerLiteral, and is lazily replaced by the result of
  // FindLowestTrailIndexThatExplainBound() applied to these literals. The
  // encoding is a bit hacky, see Dependencies().
  std::vector<Literal> literals_reason_buffer_;
  mutable std::vector<IntegerLiteral> bounds_reason_buffer_;

  // The "is_empty" literal of the optional variables or kNoLiteralIndex.
  ITIVector<IntegerVariable, LiteralIndex> is_empty_literals_;

  // Data used to support the propagation of fully encoded variable. We keep
  // for each variable the index in encoder_.GetDomainEncoding() of the first
  // literal that is not assigned to false, and call this the "min".
  int64 num_encoded_variables_ = 0;
  std::vector<Literal> tmp_literals_reason_;
  RevGrowingMultiMap<LiteralIndex, IntegerVariable> watched_min_;
  RevMap<hash_map<IntegerVariable, int>> current_min_;

  // Temporary data used by MergeReasonInto().
  mutable std::vector<int> tmp_queue_;
  mutable std::vector<int> tmp_trail_indices_;
  mutable std::vector<int> tmp_var_to_highest_explained_trail_index_;

  // For EnqueueLiteral(), we store a special TrailEntry to recover the reason
  // lazily. This vector indicates the correspondance between a literal that
  // was pushed by this class at a given trail index, and the index of its
  // TrailEntry in integer_trail_.
  std::vector<int> boolean_trail_index_to_integer_one_;

  int64 num_enqueues_;

  std::vector<SparseBitset<IntegerVariable>*> watchers_;

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
  }
};

// This class allows registering Propagator that will be called if a
// watched Literal or LbVar changes.
class GenericLiteralWatcher : public SatPropagator {
 public:
  explicit GenericLiteralWatcher(IntegerTrail* trail,
                                 RevRepository<int>* rev_int_repository);
  ~GenericLiteralWatcher() final {}

  static GenericLiteralWatcher* CreateInModel(Model* model) {
    // TODO(user): Have a general mecanism to register "global" reversible
    // classes and keep them synchronized with the search.
    std::unique_ptr<RevRepository<int>> rev_int_repository(
        new RevRepository<int>());
    GenericLiteralWatcher* watcher = new GenericLiteralWatcher(
        model->GetOrCreate<IntegerTrail>(), rev_int_repository.get());
    model->SetSingleton(std::move(rev_int_repository));

    // TODO(user): This propagator currently needs to be last because it is the
    // only one enforcing that a fix-point is reached on the integer variables.
    // Figure out a better interaction between the sat propagation loop and
    // this one.
    model->GetOrCreate<SatSolver>()->AddLastPropagator(
        std::unique_ptr<GenericLiteralWatcher>(watcher));
    return watcher;
  }

  // On propagate, the registered propagators will be called if they need to
  // until a fixed point is reached. Propagators with low ids will tend to be
  // called first, but it ultimately depends on their "waking" order.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;

  // Registers a propagator and returns its unique ids.
  int Register(PropagatorInterface* propagator);

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
  RevRepository<int>* rev_int_repository_;

  struct WatchData {
    int id;
    int watch_index;
  };
  ITIVector<LiteralIndex, std::vector<WatchData>> literal_to_watcher_;
  ITIVector<IntegerVariable, std::vector<WatchData>> var_to_watcher_;
  std::vector<PropagatorInterface*> watchers_;
  SparseBitset<IntegerVariable> modified_vars_;

  // Propagator ids that needs to be called.
  std::deque<int> queue_;
  std::vector<bool> in_queue_;

  // Decision levels and reversible classes for each propagator.
  std::vector<int> id_to_level_at_last_call_;
  std::vector<int> id_to_greatest_common_level_since_last_call_;
  std::vector<std::vector<ReversibleInterface*>> id_to_reversible_classes_;
  std::vector<std::vector<int*>> id_to_reversible_ints_;
  std::vector<std::vector<int>> id_to_watch_indices_;

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
  return vars_[i.value()].current_bound;
}

inline IntegerValue IntegerTrail::UpperBound(IntegerVariable i) const {
  return -vars_[NegationOf(i).value()].current_bound;
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

inline std::function<IntegerVariable(Model*)> NewIntegerVariable(int64 lb,
                                                                 int64 ub) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable(
        IntegerValue(lb), IntegerValue(ub));
  };
}

// Creates a 0-1 integer variable "view" of the given literal. It will have a
// value of 1 when the literal is true, and 0 when the literal is false.
inline std::function<IntegerVariable(Model*)> NewIntegerVariableFromLiteral(
    Literal lit) {
  return [=](Model* model) {
    const IntegerVariable int_var = model->Add(NewIntegerVariable(0, 1));
    model->GetOrCreate<IntegerEncoder>()->FullyEncodeVariableUsingGivenLiterals(
        int_var, {lit.Negated(), lit}, {IntegerValue(0), IntegerValue(1)});
    return int_var;
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
    CHECK_EQ(trail->LowerBound(v), trail->UpperBound(v));
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

// Associate the given literal to the given integer inequality.
inline std::function<void(Model*)> Equality(IntegerLiteral i, Literal l) {
  return [=](Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    if (encoder->LiteralIsAssociated(i)) {
      const Literal current = encoder->GetOrCreateAssociatedLiteral(i);
      model->Add(Equality(current, l));
    } else {
      encoder->AssociateGivenLiteral(i, l);
    }
  };
}

// in_interval <=> v in [lb, ub].
inline std::function<void(Model*)> ReifiedInInterval(IntegerVariable v,
                                                     int64 lb, int64 ub,
                                                     Literal in_interval) {
  return [=](Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    const auto lb_lit = IntegerLiteral::GreaterOrEqual(v, IntegerValue(lb));
    const auto ub_lit = IntegerLiteral::LowerOrEqual(v, IntegerValue(ub));
    if (lb <= model->Get(LowerBound(v))) {
      if (ub >= model->Get(UpperBound(v))) {
        model->GetOrCreate<SatSolver>()->AddUnitClause(in_interval);
      } else {
        model->Add(Equality(ub_lit, in_interval));
      }
    } else if (ub >= model->Get(UpperBound(v))) {
      model->Add(Equality(lb_lit, in_interval));
    } else {
      const Literal is_ge_lb = encoder->GetOrCreateAssociatedLiteral(lb_lit);
      const Literal is_le_ub = encoder->GetOrCreateAssociatedLiteral(ub_lit);
      model->Add(ReifiedBoolAnd({is_ge_lb, is_le_ub}, in_interval));
    }
  };
}

// A wrapper around SatSolver::Solve that handles integer variable with lazy
// encoding. Repeatedly calls SatSolver::Solve on the model, and instantiates
// literals for non-fixed variables in vars_with_lazy_encoding, until either all
// variables are fixed to a single value, or the model is proved UNSAT. Returns
// the status of the last call to SatSolver::Solve().
SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    Model* model, const std::vector<IntegerVariable>& vars_with_lazy_encoding);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_H_
