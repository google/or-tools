// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_INTEGER_H_
#define ORTOOLS_SAT_INTEGER_H_

#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

using InlinedIntegerLiteralVector = absl::InlinedVector<IntegerLiteral, 2>;
using InlinedIntegerValueVector =
    absl::InlinedVector<std::pair<IntegerVariable, IntegerValue>, 2>;

struct LiteralValueValue {
  Literal literal;
  IntegerValue left_value;
  IntegerValue right_value;

  // Used for testing.
  bool operator==(const LiteralValueValue& rhs) const {
    return literal.Index() == rhs.literal.Index() &&
           left_value == rhs.left_value && right_value == rhs.right_value;
  }

  std::string DebugString() const {
    return absl::StrCat("(lit(", literal.Index().value(), ") * ",
                        left_value.value(), " * ", right_value.value(), ")");
  }
};

// It is convenient for loading some constraints to always have a literal for
// some condition, even if it is trivial (true or false) in some cases. This
// class is used to create and keep those literals.
class TrivialLiterals {
 public:
  explicit TrivialLiterals(Model* model)
      : sat_solver_(model->GetOrCreate<SatSolver>()),
        trail_(model->GetOrCreate<Trail>()) {}

  sat::Literal TrueLiteral() const {
    CreateLiteral();
    return *true_literal_;
  }
  sat::Literal FalseLiteral() const {
    CreateLiteral();
    return true_literal_->Negated();
  }

 private:
  void InitializeTrueLiteral(Literal literal) {
    CHECK(!true_literal_.has_value());
    true_literal_ = literal;
  }

  void CreateLiteral() const {
    if (!true_literal_.has_value()) {
      // This should happen only in unit tests, the loader should have created
      // the true literal via InitializeTrueLiteral().
      CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
      const BooleanVariable var = sat_solver_->NewBooleanVariable();
      true_literal_ = Literal(var, true);
      trail_->EnqueueWithUnitReason(*true_literal_);
    }
  }

  SatSolver* sat_solver_;
  Trail* trail_;
  mutable std::optional<sat::Literal> true_literal_;

  friend void LoadVariables(const CpModelProto& model_proto,
                            bool view_all_booleans_as_integers, Model* m);
};

// Sometimes we propagate fact with no reason at a positive level, those
// will automatically be fixed on the next restart.
//
// Note that for integer literal, we already remove all "stale" entry, however
// this is still needed to properly update the InitialVariableDomain().
//
// TODO(user): we should update the initial domain right away, but this as
// some complication to clean up first.
struct DelayedRootLevelDeduction {
  std::vector<Literal> literal_to_fix;
  std::vector<IntegerLiteral> integer_literal_to_fix;
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
        trail_(model->GetOrCreate<Trail>()),
        lrat_proof_handler_(model->Mutable<LratProofHandler>()),
        delayed_to_fix_(model->GetOrCreate<DelayedRootLevelDeduction>()),
        domains_(*model->GetOrCreate<IntegerDomains>()),
        trivial_literals_(*model->GetOrCreate<TrivialLiterals>()),
        num_created_variables_(0) {}

  // This type is neither copyable nor movable.
  IntegerEncoder(const IntegerEncoder&) = delete;
  IntegerEncoder& operator=(const IntegerEncoder&) = delete;

  ~IntegerEncoder() {
    VLOG(1) << "#variables created = " << num_created_variables_;
  }

  // Memory optimization: you can call this before encoding variables.
  void ReserveSpaceForNumVariables(int num_vars);

  // Fully encode a variable using its current initial domain.
  // If the variable is already fully encoded, this does nothing.
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

  // Returns true if we know that PartialDomainEncoding(var) span the full
  // domain of var. This is always true if FullyEncodeVariable(var) has been
  // called.
  bool VariableIsFullyEncoded(IntegerVariable var) const;

  // Returns the list of literal <=> var == value currently associated to the
  // given variable. The result is sorted by value. We filter literal at false,
  // and if a literal is true, then you will get a singleton. To be sure to get
  // the full set of encoded value, then you should call this at level zero.
  //
  // The FullDomainEncoding() just check VariableIsFullyEncoded() and returns
  // the same result.
  //
  // WARNING: The reference returned is only valid until the next call to one
  // of these functions.
  const std::vector<ValueLiteralPair>& FullDomainEncoding(
      IntegerVariable var) const;
  const std::vector<ValueLiteralPair>& PartialDomainEncoding(
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

  // Returns true if the given variable has a "complex" domain.
  bool VariableDomainHasHoles(IntegerVariable var) {
    return domains_[GetPositiveOnlyIndex(var)].NumIntervals() > 1;
  }

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

  // Returns kNoLiteralIndex if there is no associated or the associated literal
  // otherwise.
  //
  // Tricky: for domain with hole, like [0,1][5,6], we assume some equivalence
  // classes, like >=2, >=3, >=4 are all the same as >= 5.
  //
  // Note that GetAssociatedLiteral() should not be called with trivially true
  // or trivially false literal. This is DCHECKed.
  bool IsFixedOrHasAssociatedLiteral(IntegerLiteral i_lit) const;
  LiteralIndex GetAssociatedLiteral(IntegerLiteral i_lit) const;
  LiteralIndex GetAssociatedEqualityLiteral(IntegerVariable var,
                                            IntegerValue value) const;

  // Advanced usage. It is more efficient to create the associated literals in
  // order, but it might be annoying to do so. Instead, you can first call
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
    return reverse_encoding_[lit];
  }

  // Returns the variable == value pairs that were associated with the given
  // Literal. Note that only positive IntegerVariable appears here.
  const InlinedIntegerValueVector& GetEqualityLiterals(Literal lit) const {
    if (lit.Index() >= reverse_equality_encoding_.size()) {
      return empty_integer_value_vector_;
    }
    return reverse_equality_encoding_[lit];
  }

  // Returns all the variables for which this literal is associated to either
  // var >= value or var == value.
  const std::vector<IntegerVariable>& GetAllAssociatedVariables(
      Literal lit) const {
    temp_associated_vars_.clear();
    for (const IntegerLiteral l : GetIntegerLiterals(lit)) {
      temp_associated_vars_.push_back(l.var);
    }
    for (const auto& [var, value] : GetEqualityLiterals(lit)) {
      temp_associated_vars_.push_back(var);
    }
    return temp_associated_vars_;
  }

  // If it exists, returns a [0, 1] integer variable which is equal to 1 iff the
  // given literal is true. Returns kNoIntegerVariable if such variable does not
  // exist. Note that one can create one by creating a new IntegerVariable and
  // calling AssociateToIntegerEqualValue().
  //
  // Note that this will only return "positive" IntegerVariable.
  IntegerVariable GetLiteralView(Literal lit) const {
    if (lit.Index() >= literal_view_.size()) return kNoIntegerVariable;
    const IntegerVariable result = literal_view_[lit];
    DCHECK(result == kNoIntegerVariable || VariableIsPositive(result));
    return result;
  }

  // If this is true, then a literal can be linearized with an affine expression
  // involving an integer variable.
  ABSL_MUST_USE_RESULT bool LiteralOrNegationHasView(
      Literal lit, IntegerVariable* view = nullptr,
      bool* view_is_direct = nullptr) const;

  // Returns a Boolean literal associated with a bound lower than or equal to
  // the one of the given IntegerLiteral. If the given IntegerLiteral is true,
  // then the returned literal should be true too. Returns kNoLiteralIndex if no
  // such literal was created.
  //
  // Ex: if 'i' is (x >= 4) and we already created a literal associated to
  // (x >= 2) but not to (x >= 3), we will return the literal associated with
  // (x >= 2).
  LiteralIndex SearchForLiteralAtOrBefore(IntegerLiteral i_lit,
                                          IntegerValue* bound) const;
  LiteralIndex SearchForLiteralAtOrAfter(IntegerLiteral i_lit,
                                         IntegerValue* bound) const;

  // Returns the set of Literal associated to IntegerLiteral of the form var >=
  // value. We make a copy, because this can be easily invalidated when calling
  // any function of this class. So it is less efficient but safer.
  std::vector<ValueLiteralPair> PartialGreaterThanEncoding(
      IntegerVariable var) const;

  // Makes sure all element in the >= encoding are non-trivial and canonical.
  // The input variable must be positive.
  bool UpdateEncodingOnInitialDomainChange(IntegerVariable var, Domain domain);

  // All IntegerVariable passed to the functions above must be in
  // [0, NumVariables).
  int NumVariables() const { return 2 * encoding_by_var_.size(); }

 private:
  // Adds the implications:
  //    Literal(before) <= associated_lit <= Literal(after).
  // Arguments:
  //  - map is just encoding_by_var_[associated_lit.var] and is passed as a
  //    slight optimization.
  //  - 'it' is the current position of associated_lit in map, i.e. we must have
  //    it->second == associated_lit.
  void AddImplications(
      const absl::btree_map<IntegerValue, Literal>& map,
      absl::btree_map<IntegerValue, Literal>::const_iterator it,
      Literal associated_lit);

  SatSolver* sat_solver_;
  Trail* trail_;
  LratProofHandler* lrat_proof_handler_;
  DelayedRootLevelDeduction* delayed_to_fix_;
  const IntegerDomains& domains_;
  const TrivialLiterals& trivial_literals_;

  bool add_implications_ = true;
  int64_t num_created_variables_ = 0;

  // We keep all the literals associated to an Integer variable in a map ordered
  // by bound (so we can properly add implications between the literals
  // corresponding to the same variable).
  //
  // Note that we only keep this for positive variable.
  // The one for the negation can be inferred by it.
  //
  // Like                x >= 1     x >= 4     x >= 5
  // Correspond to       x <= 0     x <= 3     x <= 4
  // That is            -x >= 0    -x >= -2   -x >= -4
  //
  // With potentially stronger <= bound if we fall into domain holes.
  //
  // TODO(user): Remove the entry no longer needed because of level zero
  // propagations.
  util_intops::StrongVector<PositiveOnlyIndex,
                            absl::btree_map<IntegerValue, Literal>>
      encoding_by_var_;

  // Store for a given LiteralIndex the list of its associated IntegerLiterals.
  const InlinedIntegerLiteralVector empty_integer_literal_vector_;
  util_intops::StrongVector<LiteralIndex, InlinedIntegerLiteralVector>
      reverse_encoding_;
  const InlinedIntegerValueVector empty_integer_value_vector_;
  util_intops::StrongVector<LiteralIndex, InlinedIntegerValueVector>
      reverse_equality_encoding_;

  // Used by GetAllAssociatedVariables().
  mutable std::vector<IntegerVariable> temp_associated_vars_;

  // Store for a given LiteralIndex its IntegerVariable view or kNoVariableIndex
  // if there is none. Note that only positive IntegerVariable will appear here.
  util_intops::StrongVector<LiteralIndex, IntegerVariable> literal_view_;

  // Mapping (variable == value) -> associated literal. Note that even if
  // there is more than one literal associated to the same fact, we just keep
  // the first one that was added.
  //
  // Note that we only keep positive IntegerVariable here to reduce memory
  // usage.
  absl::flat_hash_map<std::pair<PositiveOnlyIndex, IntegerValue>, Literal>
      equality_to_associated_literal_;

  // Mutable because this is lazily cleaned-up by PartialDomainEncoding().
  mutable util_intops::StrongVector<PositiveOnlyIndex,
                                    absl::InlinedVector<ValueLiteralPair, 2>>
      equality_by_var_;

  // Variables that are fully encoded.
  mutable util_intops::StrongVector<PositiveOnlyIndex, bool> is_fully_encoded_;

  // A literal that is always true, convenient to encode trivial domains.
  // This will be lazily created when needed.
  LiteralIndex literal_index_true_ = kNoLiteralIndex;

  // Temporary memory used by FullyEncodeVariable().
  std::vector<IntegerValue> tmp_values_;
  std::vector<ValueLiteralPair> tmp_encoding_;

  // Temporary memory for the result of PartialDomainEncoding().
  mutable std::vector<ValueLiteralPair> partial_encoding_;
};

// The reason is the union of all of these fact.
//
// WARNING: the Span<> will not be valid forever, so this IntegerReason
// class should just be used temporarily during conflict resolution, and not
// keep in persistent storage.
struct IntegerReason {
  void clear() {
    index_at_propagation = 0;
    slack = 0;

    boolean_literals_at_true = {};
    boolean_literals_at_false = {};
    integer_literals = {};
    vars = {};
    coeffs = {};
  }

  bool empty() const {
    return boolean_literals_at_true.empty() &&
           boolean_literals_at_false.empty() && integer_literals.empty() &&
           vars.empty();
  }

  // Note the integer_literals are always "true". But for Booleans, we support
  // both specification. Just listing true literals make more sense, but
  // historically in the SAT world, a span into (n - 1) literals of a clause is
  // used as a reason, where all the literals there are false.
  absl::Span<const Literal> boolean_literals_at_true;
  absl::Span<const Literal> boolean_literals_at_false;
  absl::Span<const IntegerLiteral> integer_literals;

  // Expresses the fact that the propagated_i_lit is true assuming the lower
  // bound for all variables in the linear expression at the time of
  // @index_at_propagation and the reasons above.
  //
  // WARNING: to avoid copying stuff around, vars/coeffs will contain an entry
  // for propagated_i_lit.var that should be ignored.
  //
  // The slack and individual coefficients indicate by how much the bounds at
  // the time might be relaxed and still explain propagated_i_lit correctly.
  int index_at_propagation;
  IntegerValue slack;
  IntegerLiteral propagated_i_lit;
  absl::Span<const IntegerVariable> vars;
  absl::Span<const IntegerValue> coeffs;
};

class LazyReasonInterface {
 public:
  LazyReasonInterface() = default;
  virtual ~LazyReasonInterface() = default;

  virtual std::string LazyReasonName() const = 0;

  // When called, this must fill the given reason (which is already cleared).
  // Note that some fields of IntegerReason like index_at_propagation and
  // propagated_i_lit are already filled.
  //
  // Remark: integer_literal[reason.index_at_propagation] might not exist or has
  // nothing to do with what was propagated.
  virtual void Explain(int id, IntegerLiteral to_explain,
                       IntegerReason* reason) = 0;
};

// This is used by the IntegerConflictResolution class.
//
// An ordered sequence of index, the "reason" for each index should be
// a sequence of lower indices.
//
// TODO(user): We could use a single int32_t if we use a generic trail. Or
// alternatively, push "place_holder" to the boolean trail. But for
// non-chronological backtracking, having the assignment level here seems nice.
struct GlobalTrailIndex {
  constexpr static int kNoIntegerIndex = std::numeric_limits<int>::max();

  int level;
  int bool_index;
  int integer_index = kNoIntegerIndex;

  bool IsBoolean() const { return integer_index == kNoIntegerIndex; }
  bool IsInteger() const { return integer_index != kNoIntegerIndex; }

  // By convention, for all indices sharing the same bool_index, we need all
  // IsInteger() indices to be before the unique IsBoolean() one.
  bool operator<(const GlobalTrailIndex& o) const {
    if (level != o.level) return level < o.level;
    if (bool_index != o.bool_index) return bool_index < o.bool_index;
    return integer_index < o.integer_index;
  }

  bool operator==(const GlobalTrailIndex& o) const {
    return level == o.level && bool_index == o.bool_index &&
           integer_index == o.integer_index;
  }

  bool operator<=(const GlobalTrailIndex& o) const {
    return *this == o || *this < o;
  }

  bool operator>=(const GlobalTrailIndex& o) const { return !(*this < o); }

  bool operator>(const GlobalTrailIndex& o) const { return !(*this <= o); }
};

// Index used to store reasons.
DEFINE_STRONG_INDEX_TYPE(ReasonIndex);

// This class maintains a set of integer variables with their current bounds.
// Bounds can be propagated from an external "source" and this class helps
// to maintain the reason for each propagation.
class IntegerTrail final : public SatPropagator {
 public:
  explicit IntegerTrail(Model* model)
      : SatPropagator("IntegerTrail"),
        delayed_to_fix_(model->GetOrCreate<DelayedRootLevelDeduction>()),
        domains_(model->GetOrCreate<IntegerDomains>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        trail_(model->GetOrCreate<Trail>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        parameters_(*model->GetOrCreate<SatParameters>()) {
    model->GetOrCreate<SatSolver>()->AddPropagator(this);
  }

  // This type is neither copyable nor movable.
  IntegerTrail(const IntegerTrail&) = delete;
  IntegerTrail& operator=(const IntegerTrail&) = delete;
  ~IntegerTrail() final;

  // SatPropagator interface. These functions make sure the current bounds
  // information is in sync with the current solver literal trail. Any
  // class/propagator using this class must make sure it is synced to the
  // correct state before calling any of its functions.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;
  absl::Span<const Literal> Reason(const Trail& trail, int trail_index,
                                   int64_t conflict_id) const final;

  // Warning the conflict is only set just after ReportConflict() was called
  // here. It will be destroyed as soon as ReasonAsGlobalIndex() is used.
  const IntegerReason& IntegerConflict() {
    if (trail_->conflict_timestamp() != global_id_conflict_timestamp_) {
      tmp_reason_.clear();
      tmp_reason_.boolean_literals_at_false = trail_->FailingClause();
    }
    return tmp_reason_;
  }

  // Returns the reason for the propagation with given "global" index.
  // This will correctly handle boolean/integer reasons.
  //
  // If index.IsInteger(), one can provide the bound that need to be explained,
  // which might be lower than the one that was propagated.
  const IntegerReason& GetIntegerReason(
      GlobalTrailIndex index, std::optional<IntegerValue> needed_bound);

  // Returns the number of created integer variables.
  //
  // Note that this is twice the number of call to AddIntegerVariable() since
  // we automatically create the NegationOf() variable too.
  IntegerVariable NumIntegerVariables() const {
    return IntegerVariable(var_lbs_.size());
  }

  // Optimization: you can call this before calling AddIntegerVariable()
  // num_vars time.
  void ReserveSpaceForNumVariables(int num_vars);

  // Adds a new integer variable. Adding integer variable can only be done when
  // the decision level is zero (checked). The given bounds are INCLUSIVE and
  // must not cross.
  //
  // Note on integer overflow: 'upper_bound - lower_bound' must fit on an
  // int64_t, this is DCHECKed. More generally, depending on the constraints
  // that are added, the bounds magnitude must be small enough to satisfy each
  // constraint overflow precondition.
  IntegerVariable AddIntegerVariable(IntegerValue lower_bound,
                                     IntegerValue upper_bound);

  // Same as above but for a more complex domain specified as a sorted list of
  // disjoint intervals. See the Domain class.
  IntegerVariable AddIntegerVariable(const Domain& domain);

  // Returns the initial domain of the given variable. Note that the min/max
  // are updated with level zero propagation, but not holes.
  const Domain& InitialVariableDomain(IntegerVariable var) const;

  // Useful for debugging the solver.
  std::string VarDebugString(IntegerVariable var) const;

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

  // Returns the current lower/upper bound of the given integer variable.
  IntegerValue LowerBound(IntegerVariable i) const;
  IntegerValue UpperBound(IntegerVariable i) const;

  // If one needs to do a lot of LowerBound()/UpperBound() it will be faster
  // to cache the current pointer to the underlying vector.
  const IntegerValue* LowerBoundsData() const { return var_lbs_.data(); }

  // Checks if the variable is fixed.
  bool IsFixed(IntegerVariable i) const;

  // Checks that the variable is fixed and returns its value.
  IntegerValue FixedValue(IntegerVariable i) const;

  // Same as above for an affine expression.
  IntegerValue LowerBound(AffineExpression expr) const;
  IntegerValue UpperBound(AffineExpression expr) const;
  IntegerValue UpperBound(LinearExpression2 expr) const;
  bool IsFixed(AffineExpression expr) const;
  IntegerValue FixedValue(AffineExpression expr) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given integer variable.
  IntegerLiteral LowerBoundAsLiteral(IntegerVariable i) const;
  IntegerLiteral UpperBoundAsLiteral(IntegerVariable i) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given affine expression. In case the expression is constant, it returns
  // IntegerLiteral::TrueLiteral().
  IntegerLiteral LowerBoundAsLiteral(AffineExpression expr) const;
  IntegerLiteral UpperBoundAsLiteral(AffineExpression expr) const;

  // Returns the current value (if known) of an IntegerLiteral.
  bool IntegerLiteralIsTrue(IntegerLiteral l) const;
  bool IntegerLiteralIsFalse(IntegerLiteral l) const;
  bool IsTrueAtLevelZero(IntegerLiteral l) const;

  // Returns globally valid lower/upper bound on the given integer variable.
  IntegerValue LevelZeroLowerBound(IntegerVariable var) const;
  IntegerValue LevelZeroUpperBound(IntegerVariable var) const;

  // Returns globally valid lower/upper bound on the given affine expression.
  IntegerValue LevelZeroLowerBound(AffineExpression exp) const;
  IntegerValue LevelZeroUpperBound(AffineExpression exp) const;

  // Returns globally valid lower/upper bound on the given linear expression.
  IntegerValue LevelZeroLowerBound(LinearExpression2 expr) const;
  IntegerValue LevelZeroUpperBound(LinearExpression2 expr) const;

  // Returns true if the variable is fixed at level 0.
  bool IsFixedAtLevelZero(IntegerVariable var) const;

  // Returns true if the affine expression is fixed at level 0.
  bool IsFixedAtLevelZero(AffineExpression expr) const;

  // Advanced usage.
  // Returns the current lower bound assuming the literal is true.
  IntegerValue ConditionalLowerBound(Literal l, IntegerVariable i) const;
  IntegerValue ConditionalLowerBound(Literal l, AffineExpression expr) const;

  // Returns the current upper bound assuming the literal is true.
  IntegerValue ConditionalUpperBound(Literal l, IntegerVariable i) const;
  IntegerValue ConditionalUpperBound(Literal l, AffineExpression expr) const;

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
  // - A set of Literal currently being all false.
  // - A set of IntegerLiteral currently being all true.
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
  ABSL_MUST_USE_RESULT bool Enqueue(IntegerLiteral i_lit) {
    return EnqueueInternal(i_lit, false, {}, {});
  }
  ABSL_MUST_USE_RESULT bool Enqueue(
      IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason) {
    return EnqueueInternal(i_lit, false, literal_reason, integer_reason);
  }

  // Enqueue new information about a variable bound. It has the same behavior
  // as the Enqueue() method, except that it accepts true and false integer
  // literals, both for i_lit, and for the integer reason.
  //
  // This method will do nothing if i_lit is a true literal. It will report a
  // conflict if i_lit is a false literal, and enqueue i_lit normally otherwise.
  // Furthemore, it will check that the integer reason does not contain any
  // false literals, and will remove true literals before calling
  // ReportConflict() or Enqueue().
  ABSL_MUST_USE_RESULT bool SafeEnqueue(
      IntegerLiteral i_lit, absl::Span<const IntegerLiteral> integer_reason);
  ABSL_MUST_USE_RESULT bool SafeEnqueue(
      IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  // Pushes the given integer literal assuming that the Boolean literal is true.
  // This can do a few things:
  // - If lit it true, add it to the reason and push the integer bound.
  // - If the bound is infeasible, push lit to false.
  // - If the underlying variable is optional and also controlled by lit, push
  //   the bound even if lit is not assigned.
  ABSL_MUST_USE_RESULT bool ConditionalEnqueue(
      Literal lit, IntegerLiteral i_lit, std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason);

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
      int trail_index_with_same_reason) {
    return EnqueueInternal(i_lit, false, literal_reason, integer_reason,
                           trail_index_with_same_reason);
  }

  // Lazy reason API.
  ABSL_MUST_USE_RESULT bool EnqueueWithLazyReason(
      IntegerLiteral i_lit, int id, IntegerValue propagation_slack,
      LazyReasonInterface* explainer) {
    const int trail_index = integer_trail_.size();
    lazy_reasons_.push_back(
        LazyReasonEntry{explainer, propagation_slack, i_lit, id, trail_index});
    return EnqueueInternal(i_lit, true, {}, {});
  }

  // These temporary vector can be used by the lazy reason explainer to fill
  // the IntegerReason spans. Note that we clear them on each call.
  std::vector<Literal>& ClearedMutableTmpLiterals() {
    tmp_lazy_reason_boolean_literals_.clear();
    return tmp_lazy_reason_boolean_literals_;
  }
  std::vector<IntegerLiteral>& ClearedMutableTmpIntegerLiterals() {
    tmp_lazy_reason_integer_literals_.clear();
    return tmp_lazy_reason_integer_literals_;
  }

  // Sometimes we infer some root level bounds but we are not at the root level.
  // In this case, we will update the level-zero bounds right away, but will
  // delay the current push until the next restart.
  //
  // Note that if you want to also push the literal at the current level, then
  // just calling Enqueue() is enough. Since there is no reason, the literal
  // will still be recorded properly.
  ABSL_MUST_USE_RESULT bool RootLevelEnqueue(IntegerLiteral i_lit);

  // Enqueues the given literal on the trail.
  // See the comment of Enqueue() for the reason format.
  ABSL_MUST_USE_RESULT bool EnqueueLiteral(
      Literal literal, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  ABSL_MUST_USE_RESULT bool SafeEnqueueLiteral(
      Literal literal, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  // Returns the reason (as set of Literal currently false) for a given integer
  // literal. Note that the bound must be less restrictive than the current
  // bound (checked).
  std::vector<Literal> ReasonFor(IntegerLiteral literal) const;

  // Appends the reason for the given integer literals to the output and call
  // STLSortAndRemoveDuplicates() on it. This function accept "constant"
  // literal.
  void MergeReasonInto(absl::Span<const IntegerLiteral> literals,
                       std::vector<Literal>* output) const;

  // Returns the number of enqueues that changed a variable bounds. We don't
  // count enqueues called with a less restrictive bound than the current one.
  //
  // Note(user): this can be used to see if any of the bounds changed. Just
  // looking at the integer trail index is not enough because at level zero it
  // doesn't change since we directly update the "fixed" bounds.
  int64_t num_enqueues() const { return num_enqueues_; }
  int64_t timestamp() const { return num_enqueues_ + num_untrails_; }

  // Same as num_enqueues but only count the level zero changes.
  int64_t num_level_zero_enqueues() const { return num_level_zero_enqueues_; }

  // All the registered bitsets will be set to one each time a LbVar is
  // modified. It is up to the client to clear it if it wants to be notified
  // with the newly modified variables.
  void RegisterWatcher(SparseBitset<IntegerVariable>* p) {
    p->ClearAndResize(NumIntegerVariables());
    watchers_.push_back(p);
  }

  IntegerVariable VarAtIndex(int integer_trail_index) const {
    return integer_trail_[integer_trail_index].var;
  }
  IntegerLiteral IntegerLiteralAtIndex(int integer_trail_index) const {
    return IntegerLiteral::GreaterOrEqual(
        integer_trail_[integer_trail_index].var,
        integer_trail_[integer_trail_index].bound);
  }
  int GetFirstIndexBefore(IntegerVariable var, GlobalTrailIndex source_index,
                          int starting_integer_index) {
    CHECK_GE(var, 0);
    CHECK_LT(var, var_trail_index_.size());
    int index = std::min(starting_integer_index, var_trail_index_[var]);
    while (true) {
      // Did we reach root level?
      if (index < static_cast<int>(var_lbs_.size())) return -1;
      CHECK_LT(index, integer_trail_.size());
      CHECK_EQ(integer_trail_[index].var, var);
      if (GlobalIndexAt(index) < source_index) return index;
      index = integer_trail_[index].prev_trail_index;
    }
  }
  int PreviousTrailIndex(int integer_trail_index) const {
    if (integer_trail_index < var_lbs_.size()) {
      CHECK_EQ(integer_trail_[integer_trail_index].prev_trail_index, -1);
    }
    return integer_trail_[integer_trail_index].prev_trail_index;
  }
  GlobalTrailIndex GlobalIndexAt(int integer_index) const {
    CHECK_GE(integer_index, var_lbs_.size());
    CHECK_NE(integer_trail_[integer_index].var, kNoIntegerVariable);

    const auto& entry = extra_trail_info_[integer_index];
    CHECK_NE(entry.assignment_level, 0);
    return GlobalTrailIndex{entry.assignment_level, entry.bool_trail_index,
                            integer_index};
  }

  // Helper functions to report a conflict. Always return false so a client can
  // simply do: return integer_trail_->ReportConflict(...);
  bool ReportConflict(absl::Span<const Literal> literal_reason,
                      absl::Span<const IntegerLiteral> integer_reason) {
    DCHECK(ReasonIsValid(IntegerLiteral::FalseLiteral(), literal_reason,
                         integer_reason));
    std::vector<Literal>* conflict = trail_->MutableConflict();
    if (new_conflict_resolution_) {
      global_id_conflict_timestamp_ = trail_->conflict_timestamp();

      // Copy.
      tmp_reason_.clear();
      tmp_literal_reason_.assign(literal_reason.begin(), literal_reason.end());
      tmp_integer_reason_.assign(integer_reason.begin(), integer_reason.end());
      tmp_reason_.boolean_literals_at_false = tmp_literal_reason_;
      tmp_reason_.integer_literals = tmp_integer_reason_;
      return false;
    }

    conflict->assign(literal_reason.begin(), literal_reason.end());
    MergeReasonInto(integer_reason, conflict);
    return false;
  }
  bool ReportConflict(absl::Span<const IntegerLiteral> integer_reason) {
    return ReportConflict({}, integer_reason);
  }

  // Returns true if the variable lower bound is still the one from level zero.
  bool VariableLowerBoundIsFromLevelZero(IntegerVariable var) const {
    return var_trail_index_[var] < var_lbs_.size();
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

  // Inspects the trail and output all the non-level zero bounds from the base
  // index (one per variables) to the output. The algo is sparse if there is
  // only a few propagations on the trail.
  void AppendNewBoundsFrom(int base_index,
                           std::vector<IntegerLiteral>* output) const;

  // Returns the trail index < threshold of a TrailEntry about var. Returns -1
  // if there is no such entry (at a positive decision level). This is basically
  // the trail index of the lower bound of var at the time.
  //
  // Important: We do some optimization internally, so this should only be
  // used from within a LazyReasonFunction().
  int FindTrailIndexOfVarBefore(IntegerVariable var, int threshold) const;

  // Basic heuristic to detect when we are in a propagation loop, and suggest
  // a good variable to branch on (taking the middle value) to get out of it.
  bool InPropagationLoop() const;
  void NotifyThatPropagationWasAborted();
  IntegerVariable NextVariableToBranchOnInPropagationLoop() const;

  // If we had an incomplete propagation, it is important to fix all the
  // variables and not really on the propagation to do so. This is related to
  // the InPropagationLoop() code above.
  bool CurrentBranchHadAnIncompletePropagation();
  IntegerVariable FirstUnassignedVariable() const;

  // Return true if we can fix new fact at level zero.
  bool HasPendingRootLevelDeduction() const {
    return !delayed_to_fix_->literal_to_fix.empty() ||
           !delayed_to_fix_->integer_literal_to_fix.empty();
  }

  // If this is set, and in debug mode, we will call this on all conflict to
  // be checked for potential issue. Usually against a known optimal solution.
  void RegisterDebugChecker(
      std::function<bool(absl::Span<const Literal> clause,
                         absl::Span<const IntegerLiteral> integers)>
          checker) {
    debug_checker_ = std::move(checker);
  }

  // Tricky: we cannot rely on the parameters
  // use_new_integer_conflict_resolution, because they are many codepath when
  // the IntegerConflictResolution class is not yet registered, like in
  // presolve.
  //
  // TODO(user): Ideally if the parameter is on, we should just use it
  // everywhere, find a way.
  void UseNewConflictResolution() { new_conflict_resolution_ = true; }

  // Saves the given reason and returns a new reason index.
  // The content can be retrieved by the ReasonAsSpan() functions as long as
  // the reason index is valid.
  //
  // Warning: a reason_index is only valid until we backtrack over it, at which
  // time the memory used by the reason will be reclaimed.
  //
  // This is mostly an internal API, but it is exposed so that we can store
  // payload in a lazy reasons.
  ReasonIndex AppendReasonToInternalBuffers(
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);
  absl::Span<const IntegerLiteral> IntegerReasonAsSpan(ReasonIndex index) const;
  absl::Span<const Literal> LiteralReasonAsSpan(ReasonIndex index) const;

 private:
  // All integer_trail_.push_back() should go through this.
  void PushOnTrail(IntegerLiteral i_lit, int prev_trail_index,
                   int bool_trail_index, ReasonIndex reason_index,
                   int assignment_level);

  // Used for DHECKs to validate the reason given to the public functions above.
  // Tests that all Literal are false. Tests that all IntegerLiteral are true.
  bool ReasonIsValid(absl::Span<const Literal> literal_reason,
                     absl::Span<const IntegerLiteral> integer_reason);

  // Same as above, but with the literal for which this is the reason for.
  bool ReasonIsValid(Literal lit, absl::Span<const Literal> literal_reason,
                     absl::Span<const IntegerLiteral> integer_reason);
  bool ReasonIsValid(IntegerLiteral i_lit,
                     absl::Span<const Literal> literal_reason,
                     absl::Span<const IntegerLiteral> integer_reason);

  // If the variable has holes in its domain, make sure the literal is
  // canonicalized.
  void CanonicalizeLiteralIfNeeded(IntegerLiteral* i_lit);

  // Called by the Enqueue() functions that detected a conflict. This does some
  // common conflict initialization that must terminate by a call to
  // MergeReasonIntoInternal(conflict) where conflict is the returned vector.
  std::vector<Literal>* InitializeConflict(
      IntegerLiteral integer_literal, bool use_lazy_reason,
      absl::Span<const Literal> literals_reason,
      absl::Span<const IntegerLiteral> bounds_reason);

  // Internal implementation of the different public Enqueue() functions.
  ABSL_MUST_USE_RESULT bool EnqueueInternal(
      IntegerLiteral i_lit, bool use_lazy_reason,
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason,
      int trail_index_with_same_reason = std::numeric_limits<int>::max());

  // Internal implementation of the EnqueueLiteral() functions.
  ABSL_MUST_USE_RESULT bool EnqueueLiteralInternal(
      Literal literal, bool use_lazy_reason,
      absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  // Same as EnqueueInternal() but for the case where we push an IntegerLiteral
  // because an associated Literal is true (and we know it). In this case, we
  // have less work to do, so this has the same effect but is faster.
  ABSL_MUST_USE_RESULT bool EnqueueAssociatedIntegerLiteral(
      IntegerLiteral i_lit, Literal literal_reason);

  // Does the work of MergeReasonInto() when queue_ is already initialized.
  void MergeReasonIntoInternal(std::vector<Literal>* output,
                               int64_t conflict_id) const;

  // Returns the lowest trail index of a TrailEntry that can be used to explain
  // the given IntegerLiteral. The literal must be currently true but not true
  // at level zero (DCHECKed).
  int FindLowestTrailIndexThatExplainBound(IntegerLiteral i_lit) const;

  // This must be called before Dependencies() or AppendLiteralsReason().
  //
  // TODO(user): Not really robust, try to find a better way.
  void ComputeLazyReasonIfNeeded(ReasonIndex index) const;

  // Helper function to return the "dependencies" of a bound assignment.
  // All the TrailEntry at these indices are part of the reason for this
  // assignment.
  //
  // Important: The returned Span is only valid up to the next call.
  absl::Span<const int> Dependencies(ReasonIndex index) const;

  // Helper function to append the Literal part of the reason for this bound
  // assignment. We use added_variables_ to not add the same literal twice.
  // Note that looking at literal.Variable() is enough since all the literals
  // of a reason must be false.
  void AppendLiteralsReason(ReasonIndex index,
                            std::vector<Literal>* output) const;

  // Returns some debugging info.
  std::string DebugString();

  // Information for each integer variable about its current lower bound and
  // position of the last TrailEntry in the trail referring to this var.
  util_intops::StrongVector<IntegerVariable, IntegerValue> var_lbs_;
  util_intops::StrongVector<IntegerVariable, int> var_trail_index_;

  // This is used by FindLowestTrailIndexThatExplainBound() and
  // FindTrailIndexOfVarBefore() to speed up the lookup. It keeps a trail index
  // for each variable that may or may not point to a TrailEntry regarding this
  // variable. The validity of the index is verified before being used.
  //
  // The cache will only be updated with trail_index >= threshold.
  mutable int var_trail_index_cache_threshold_ = 0;
  mutable util_intops::StrongVector<IntegerVariable, int>
      var_trail_index_cache_;

  // Used by GetOrCreateConstantIntegerVariable() to return already created
  // constant variables that share the same value.
  absl::flat_hash_map<IntegerValue, IntegerVariable> constant_map_;

  // The integer trail. It always start by num_vars sentinel values with the
  // level 0 bounds (in one to one correspondence with var_lbs_).
  struct TrailEntry {
    IntegerValue bound;
    IntegerVariable var;
    int32_t prev_trail_index = -1;

    // Index in literals_reason_start_/bounds_reason_starts_ If this is negative
    // then it is a lazy reason.
    ReasonIndex reason_index = ReasonIndex(0);
  };
  std::vector<TrailEntry> integer_trail_;

  // trail->CurrentDecisionLevel() and trail_->Index() at the time this entry
  // was added. This is only used when use_new_integer_conflict_resolution is
  // true.
  struct ExtraTrailEntry {
    int32_t assignment_level;
    int32_t bool_trail_index;
  };
  std::vector<ExtraTrailEntry> extra_trail_info_;

  struct LazyReasonEntry {
    LazyReasonInterface* explainer;
    IntegerValue propagation_slack;
    IntegerLiteral propagated_i_lit;
    int id;
    int trail_index_at_propagation_time;

    // TODO(user): I am not sure we need to store
    // trail_index_at_propagation_time as we will know it when we call this
    // Explain function. Make sure this is the case and just clean this up.
    // We can assume the info refer the time when this entry was added.
    void Explain(IntegerValue bound_to_explain, IntegerReason* reason) const {
      DCHECK_LE(bound_to_explain, propagated_i_lit.bound);
      reason->clear();
      reason->slack = propagation_slack;
      reason->propagated_i_lit = propagated_i_lit;
      reason->index_at_propagation = trail_index_at_propagation_time;
      explainer->Explain(id,
                         IntegerLiteral::GreaterOrEqual(propagated_i_lit.var,
                                                        bound_to_explain),
                         reason);
    }

    void Explain(IntegerReason* reason) const {
      Explain(propagated_i_lit.bound, reason);
    }
  };
  std::vector<int> lazy_reason_decision_levels_;
  util_intops::StrongVector<ReasonIndex, LazyReasonEntry> lazy_reasons_;

  // Start of each decision levels in integer_trail_.
  // TODO(user): use more general reversible mechanism?
  std::vector<int> integer_search_levels_;

  // Buffer to store the reason of each trail entry.
  std::vector<ReasonIndex> reason_decision_levels_;
  util_intops::StrongVector<ReasonIndex, int> literals_reason_starts_;
  std::vector<Literal> literals_reason_buffer_;

  // The last two vectors are in one to one correspondence. Dependencies() will
  // "cache" the result of the conversion from IntegerLiteral to trail indices
  // in trail_index_reason_buffer_.
  util_intops::StrongVector<ReasonIndex, int> bounds_reason_starts_;
  mutable util_intops::StrongVector<ReasonIndex, int> cached_sizes_;
  std::vector<IntegerLiteral> bounds_reason_buffer_;
  mutable std::vector<int> trail_index_reason_buffer_;

  // Temporary data for IntegerReason.
  IntegerReason tmp_reason_;
  std::vector<Literal> tmp_lazy_reason_boolean_literals_;
  std::vector<IntegerLiteral> tmp_lazy_reason_integer_literals_;

  // Temporary for use with tmp_reason_.
  // Tricky: these cannot be the same as the one used by lazy reason.
  std::vector<Literal> tmp_boolean_literals_;
  std::vector<IntegerLiteral> tmp_integer_literals_;

  // Temporary vector filled by calls to LazyReasonFunction().
  mutable IntegerReason tmp_lazy_reason_;
  mutable std::vector<Literal> lazy_reason_literals_;
  mutable std::vector<int> lazy_reason_trail_indices_;
  mutable std::vector<int> tmp_trail_indices_;
  mutable std::vector<IntegerValue> tmp_coeffs_;

  // Temporary data used by MergeReasonInto().
  mutable bool has_dependency_ = false;
  mutable std::vector<int> tmp_queue_;
  mutable std::vector<IntegerVariable> tmp_to_clear_;
  mutable util_intops::StrongVector<IntegerVariable, int>
      tmp_var_to_trail_index_in_queue_;
  mutable SparseBitset<BooleanVariable> added_variables_;

  // Temporary heap used by RelaxLinearReason();
  struct RelaxHeapEntry {
    int index;
    IntegerValue coeff;
    int64_t diff;
    bool operator<(const RelaxHeapEntry& o) const { return index < o.index; }
  };
  mutable std::vector<RelaxHeapEntry> relax_heap_;
  mutable std::vector<int> tmp_indices_;

  // Temporary data used by AppendNewBounds().
  mutable SparseBitset<IntegerVariable> tmp_marked_;

  // Temporary data used by SafeEnqueue();
  std::vector<IntegerLiteral> tmp_cleaned_reason_;

  // For EnqueueLiteral(), we store the reason index at its Boolean trail index.
  std::vector<ReasonIndex> boolean_trail_index_to_reason_index_;

  // We need to know if we skipped some propagation in the current branch.
  // This is reverted as we backtrack over it.
  int first_level_without_full_propagation_ = -1;

  // This is used to detect when MergeReasonIntoInternal() is called multiple
  // time while processing the same conflict. It allows to optimize the reason
  // and the time taken to compute it.
  mutable int64_t last_conflict_id_ = -1;
  mutable bool info_is_valid_on_subsequent_last_level_expansion_ = false;
  mutable util_intops::StrongVector<IntegerVariable, int>
      var_to_trail_index_at_lower_level_;
  mutable std::vector<int> tmp_seen_;
  mutable std::vector<IntegerVariable> to_clear_for_lower_level_;

  int64_t global_id_conflict_timestamp_ = 0;
  int64_t num_enqueues_ = 0;
  int64_t num_untrails_ = 0;
  int64_t num_level_zero_enqueues_ = 0;
  mutable int64_t num_decisions_to_break_loop_ = 0;

  std::vector<SparseBitset<IntegerVariable>*> watchers_;
  std::vector<ReversibleInterface*> reversible_classes_;

  mutable int64_t work_done_in_find_indices_ = 0;

  mutable Domain temp_domain_;
  DelayedRootLevelDeduction* delayed_to_fix_;
  IntegerDomains* domains_;
  IntegerEncoder* encoder_;
  Trail* trail_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  const SatParameters& parameters_;

  bool new_conflict_resolution_ = false;

  std::vector<Literal> tmp_literal_reason_;
  std::vector<IntegerLiteral> tmp_integer_reason_;

  // Temporary "hash" to keep track of all the conditional enqueue that were
  // done. Note that we currently do not keep any reason for them, and as such,
  // we can only use this in heuristics. See ConditionalLowerBound().
  absl::flat_hash_map<std::pair<LiteralIndex, IntegerVariable>, IntegerValue>
      conditional_lbs_;

  std::function<bool(absl::Span<const Literal> clause,
                     absl::Span<const IntegerLiteral> integers)>
      debug_checker_ = nullptr;
};

// Base class for CP like propagators.
class PropagatorInterface {
 public:
  PropagatorInterface() = default;
  virtual ~PropagatorInterface() = default;

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
  virtual bool IncrementalPropagate(const std::vector<int>& /*watch_indices*/) {
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
class GenericLiteralWatcher final : public SatPropagator {
 public:
  explicit GenericLiteralWatcher(Model* model);

  // This type is neither copyable nor movable.
  GenericLiteralWatcher(const GenericLiteralWatcher&) = delete;
  GenericLiteralWatcher& operator=(const GenericLiteralWatcher&) = delete;

  ~GenericLiteralWatcher() final = default;

  // Memory optimization: you can call this before registering watchers.
  void ReserveSpaceForNumVariables(int num_vars);

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

  // Because the coeff is always positive, watching an affine expression is
  // the same as watching its var.
  void WatchLowerBound(AffineExpression e, int id) {
    WatchLowerBound(e.var, id);
  }
  void WatchUpperBound(AffineExpression e, int id) {
    WatchUpperBound(e.var, id);
  }
  void WatchAffineExpression(AffineExpression e, int id) {
    WatchIntegerVariable(e.var, id);
  }

  // No-op overload for "constant" IntegerVariable that are sometimes templated
  // as an IntegerValue.
  void WatchLowerBound(IntegerValue /*i*/, int /*id*/) {}
  void WatchUpperBound(IntegerValue /*i*/, int /*id*/) {}
  void WatchIntegerVariable(IntegerValue /*v*/, int /*id*/) {}

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
  // is usually done in a CP solver at the cost of a slightly more complex API.
  void RegisterReversibleInt(int id, int* rev);

  // A simple form of incremental update is to maintain state as we dive into
  // the search tree but forget everything on every backtrack. A propagator
  // can be called many times by decision, so this can make a large proportion
  // of the calls incremental.
  //
  // This allows to achieve this with a really low overhead.
  //
  // The propagator can define a bool rev_is_in_dive_ = false; and at the
  // beginning of each propagate do:
  // const bool no_backtrack_since_last_call = rev_is_in_dive_;
  // watcher_->SetUntilNextBacktrack(&rev_is_in_dive_);
  void SetUntilNextBacktrack(bool* is_in_dive) {
    if (!*is_in_dive) {
      *is_in_dive = true;
      bool_to_reset_on_backtrack_.push_back(is_in_dive);
    }
  }

  // Returns the number of registered propagators.
  int NumPropagators() const { return in_queue_.size(); }

  // Set a callback for new variable bounds at level 0.
  //
  // This will be called (only at level zero) with the list of IntegerVariable
  // with changed lower bounds. Note that it might be called more than once
  // during the same propagation cycle if we fix variables in "stages".
  //
  // Also note that this will be called if some BooleanVariable where fixed even
  // if no IntegerVariable are changed, so the passed vector to the function
  // might be empty.
  void RegisterLevelZeroModifiedVariablesCallback(
      const std::function<void(const std::vector<IntegerVariable>&)> cb) {
    level_zero_modified_variable_callback_.push_back(cb);
  }

  // This will be called not too often during propagation (when we finish
  // propagating one priority). If it returns true, we will stop propagation
  // there. It is used by LbTreeSearch as we can stop as soon as the objective
  // lower bound crossed a threshold and do not need to call expensive
  // propagator when this is the case.
  void SetStopPropagationCallback(std::function<bool()> callback) {
    stop_propagation_callback_ = callback;
  }

  // Returns the id of the propagator we are currently calling. This is meant
  // to be used from inside Propagate() in case a propagator was registered
  // more than once at different priority for instance.
  int GetCurrentId() const { return current_id_; }

  // Add the given propagator to its queue.
  //
  // Warning: This will have no effect if called from within the propagation of
  // a propagator since the propagator is still marked as "in the queue" until
  // its propagation is done. Use CallAgainDuringThisPropagation() if that is
  // what you need instead.
  void CallOnNextPropagate(int id);

  void CallAgainDuringThisPropagation() { call_again_ = true; };

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
    bool operator==(const WatchData& o) const {
      return id == o.id && watch_index == o.watch_index;
    }
  };
  util_intops::StrongVector<LiteralIndex, std::vector<WatchData>>
      literal_to_watcher_;
  util_intops::StrongVector<IntegerVariable, std::vector<WatchData>>
      var_to_watcher_;
  std::vector<PropagatorInterface*> watchers_;
  SparseBitset<IntegerVariable> modified_vars_;

  // For RegisterLevelZeroModifiedVariablesCallback().
  SparseBitset<IntegerVariable> modified_vars_for_callback_;

  // Propagator ids that needs to be called. There is one queue per priority but
  // just one Boolean to indicate if a propagator is in one of them.
  std::vector<std::deque<int>> queue_by_priority_;
  std::vector<bool> in_queue_;

  // Data for each propagator.
  DEFINE_STRONG_INDEX_TYPE(IdType);
  std::vector<bool> id_need_reversible_support_;
  std::vector<int> id_to_level_at_last_call_;
  RevVector<IdType, int> id_to_greatest_common_level_since_last_call_;
  std::vector<std::vector<ReversibleInterface*>> id_to_reversible_classes_;
  std::vector<std::vector<int*>> id_to_reversible_ints_;

  std::vector<std::vector<int>> id_to_watch_indices_;
  std::vector<int> id_to_priority_;
  std::vector<int> id_to_idempotence_;

  // Special propagators that needs to always be called at level zero.
  std::vector<int> propagator_ids_to_call_at_level_zero_;

  // The id of the propagator we just called.
  int current_id_;
  bool call_again_ = false;

  std::vector<std::function<void(const std::vector<IntegerVariable>&)>>
      level_zero_modified_variable_callback_;

  std::function<bool()> stop_propagation_callback_;

  std::vector<bool*> bool_to_reset_on_backtrack_;
};

// ============================================================================
// Implementation.
// ============================================================================

inline IntegerValue IntegerTrail::LowerBound(IntegerVariable i) const {
  return var_lbs_[i];
}

inline IntegerValue IntegerTrail::UpperBound(IntegerVariable i) const {
  return -var_lbs_[NegationOf(i)];
}

inline bool IntegerTrail::IsFixed(IntegerVariable i) const {
  return var_lbs_[i] == -var_lbs_[NegationOf(i)];
}

inline IntegerValue IntegerTrail::FixedValue(IntegerVariable i) const {
  DCHECK(IsFixed(i));
  return var_lbs_[i];
}

inline IntegerValue IntegerTrail::ConditionalLowerBound(
    Literal l, IntegerVariable i) const {
  const auto it = conditional_lbs_.find({l.Index(), i});
  if (it != conditional_lbs_.end()) {
    return std::max(var_lbs_[i], it->second);
  }
  return var_lbs_[i];
}

inline IntegerValue IntegerTrail::ConditionalLowerBound(
    Literal l, AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return ConditionalLowerBound(l, expr.var) * expr.coeff + expr.constant;
}

inline IntegerValue IntegerTrail::ConditionalUpperBound(
    Literal l, IntegerVariable i) const {
  return -ConditionalLowerBound(l, NegationOf(i));
}

inline IntegerValue IntegerTrail::ConditionalUpperBound(
    Literal l, AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return ConditionalUpperBound(l, expr.var) * expr.coeff + expr.constant;
}

inline IntegerLiteral IntegerTrail::LowerBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::GreaterOrEqual(i, LowerBound(i));
}

inline IntegerLiteral IntegerTrail::UpperBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::LowerOrEqual(i, UpperBound(i));
}

inline IntegerValue IntegerTrail::LowerBound(AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return LowerBound(expr.var) * expr.coeff + expr.constant;
}

inline IntegerValue IntegerTrail::UpperBound(AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return UpperBound(expr.var) * expr.coeff + expr.constant;
}

inline IntegerValue IntegerTrail::UpperBound(LinearExpression2 expr) const {
  IntegerValue result = 0;
  for (int i = 0; i < 2; ++i) {
    if (expr.coeffs[i] == 0) {
      continue;
    } else if (expr.coeffs[i] > 0) {
      result += expr.coeffs[i] * UpperBound(expr.vars[i]);
    } else {
      result += expr.coeffs[i] * LowerBound(expr.vars[i]);
    }
  }
  return result;
}

inline bool IntegerTrail::IsFixed(AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return true;
  return IsFixed(expr.var);
}

inline IntegerValue IntegerTrail::FixedValue(AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return FixedValue(expr.var) * expr.coeff + expr.constant;
}

inline IntegerLiteral IntegerTrail::LowerBoundAsLiteral(
    AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return IntegerLiteral::TrueLiteral();
  return IntegerLiteral::GreaterOrEqual(expr.var, LowerBound(expr.var));
}

inline IntegerLiteral IntegerTrail::UpperBoundAsLiteral(
    AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return IntegerLiteral::TrueLiteral();
  return IntegerLiteral::LowerOrEqual(expr.var, UpperBound(expr.var));
}

inline bool IntegerTrail::IntegerLiteralIsTrue(IntegerLiteral l) const {
  return l.bound <= LowerBound(l.var);
}

inline bool IntegerTrail::IntegerLiteralIsFalse(IntegerLiteral l) const {
  return l.bound > UpperBound(l.var);
}

inline bool IntegerTrail::IsTrueAtLevelZero(IntegerLiteral l) const {
  return l.bound <= LevelZeroLowerBound(l.var);
}

// The level zero bounds are stored at the beginning of the trail and they also
// serves as sentinels. Their index match the variables index.
inline IntegerValue IntegerTrail::LevelZeroLowerBound(
    IntegerVariable var) const {
  DCHECK_GE(var, 0);
  DCHECK_LT(var, integer_trail_.size());
  return integer_trail_[var.value()].bound;
}

inline IntegerValue IntegerTrail::LevelZeroUpperBound(
    IntegerVariable var) const {
  return -integer_trail_[NegationOf(var).value()].bound;
}

inline bool IntegerTrail::IsFixedAtLevelZero(IntegerVariable var) const {
  return integer_trail_[var.value()].bound ==
         -integer_trail_[NegationOf(var).value()].bound;
}

inline IntegerValue IntegerTrail::LevelZeroLowerBound(
    AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return expr.ValueAt(LevelZeroLowerBound(expr.var));
}

inline IntegerValue IntegerTrail::LevelZeroUpperBound(
    AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return expr.constant;
  return expr.ValueAt(LevelZeroUpperBound(expr.var));
}

inline IntegerValue IntegerTrail::LevelZeroLowerBound(
    LinearExpression2 expr) const {
  expr.SimpleCanonicalization();
  IntegerValue result = 0;
  for (int i = 0; i < 2; ++i) {
    if (expr.coeffs[i] != 0) {
      result += expr.coeffs[i] * LevelZeroLowerBound(expr.vars[i]);
    }
  }
  return result;
}

inline IntegerValue IntegerTrail::LevelZeroUpperBound(
    LinearExpression2 expr) const {
  expr.SimpleCanonicalization();
  IntegerValue result = 0;
  for (int i = 0; i < 2; ++i) {
    if (expr.coeffs[i] != 0) {
      result += expr.coeffs[i] * LevelZeroUpperBound(expr.vars[i]);
    }
  }
  return result;
}

inline bool IntegerTrail::IsFixedAtLevelZero(AffineExpression expr) const {
  if (expr.var == kNoIntegerVariable) return true;
  return IsFixedAtLevelZero(expr.var);
}

inline void GenericLiteralWatcher::WatchLiteral(Literal l, int id,
                                                int watch_index) {
  if (l.Index() >= literal_to_watcher_.size()) {
    literal_to_watcher_.resize(l.Index().value() + 1);
  }
  literal_to_watcher_[l].push_back({id, watch_index});
}

inline void GenericLiteralWatcher::WatchLowerBound(IntegerVariable var, int id,
                                                   int watch_index) {
  if (var == kNoIntegerVariable) return;
  if (var.value() >= var_to_watcher_.size()) {
    var_to_watcher_.resize(var.value() + 1);
  }

  // Minor optim, so that we don't watch the same variable twice. Propagator
  // code is easier this way since for example when one wants to watch both
  // an interval start and interval end, both might have the same underlying
  // variable.
  const WatchData data = {id, watch_index};
  if (!var_to_watcher_[var].empty() && var_to_watcher_[var].back() == data) {
    return;
  }
  var_to_watcher_[var].push_back(data);
}

inline void GenericLiteralWatcher::WatchUpperBound(IntegerVariable var, int id,
                                                   int watch_index) {
  if (var == kNoIntegerVariable) return;
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
// Note that in the model API, we simply use int64_t for the integer values, so
// that it is nicer for the client. Internally these are converted to
// IntegerValue which is typechecked.
// ============================================================================

inline std::function<BooleanVariable(Model*)> NewBooleanVariable() {
  return [=](Model* model) {
    return model->GetOrCreate<SatSolver>()->NewBooleanVariable();
  };
}

inline std::function<IntegerVariable(Model*)> ConstantIntegerVariable(
    int64_t value) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()
        ->GetOrCreateConstantIntegerVariable(IntegerValue(value));
  };
}

inline std::function<IntegerVariable(Model*)> NewIntegerVariable(int64_t lb,
                                                                 int64_t ub) {
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
inline IntegerVariable CreateNewIntegerVariableFromLiteral(Literal lit,
                                                           Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  const IntegerVariable candidate = encoder->GetLiteralView(lit);
  if (candidate != kNoIntegerVariable) return candidate;

  IntegerVariable var;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const auto& assignment = model->GetOrCreate<SatSolver>()->Assignment();
  if (assignment.LiteralIsTrue(lit)) {
    var = integer_trail->GetOrCreateConstantIntegerVariable(IntegerValue(1));
  } else if (assignment.LiteralIsFalse(lit)) {
    var = integer_trail->GetOrCreateConstantIntegerVariable(IntegerValue(0));
  } else {
    var = integer_trail->AddIntegerVariable(IntegerValue(0), IntegerValue(1));
  }

  encoder->AssociateToIntegerEqualValue(lit, var, IntegerValue(1));
  DCHECK_NE(encoder->GetLiteralView(lit), kNoIntegerVariable);
  return var;
}

// Deprecated.
inline std::function<IntegerVariable(Model*)> NewIntegerVariableFromLiteral(
    Literal lit) {
  return [=](Model* model) {
    return CreateNewIntegerVariableFromLiteral(lit, model);
  };
}

inline std::function<int64_t(const Model&)> LowerBound(IntegerVariable v) {
  return [=](const Model& model) {
    return model.Get<IntegerTrail>()->LowerBound(v).value();
  };
}

inline std::function<int64_t(const Model&)> UpperBound(IntegerVariable v) {
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
inline std::function<int64_t(const Model&)> Value(IntegerVariable v) {
  return [=](const Model& model) {
    const IntegerTrail* trail = model.Get<IntegerTrail>();
    CHECK_EQ(trail->LowerBound(v), trail->UpperBound(v)) << v;
    return trail->LowerBound(v).value();
  };
}

inline std::function<void(Model*)> GreaterOrEqual(IntegerVariable v,
                                                  int64_t lb) {
  return [=](Model* model) {
    if (!model->GetOrCreate<IntegerTrail>()->Enqueue(
            IntegerLiteral::GreaterOrEqual(v, IntegerValue(lb)),
            std::vector<Literal>(), std::vector<IntegerLiteral>())) {
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
      VLOG(1) << "Model trivially infeasible, variable " << v
              << " has upper bound " << model->Get(UpperBound(v))
              << " and GreaterOrEqual() was called with a lower bound of "
              << lb;
    }
  };
}

inline std::function<void(Model*)> LowerOrEqual(IntegerVariable v, int64_t ub) {
  return [=](Model* model) {
    if (!model->GetOrCreate<IntegerTrail>()->Enqueue(
            IntegerLiteral::LowerOrEqual(v, IntegerValue(ub)),
            std::vector<Literal>(), std::vector<IntegerLiteral>())) {
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
      VLOG(1) << "Model trivially infeasible, variable " << v
              << " has lower bound " << model->Get(LowerBound(v))
              << " and LowerOrEqual() was called with an upper bound of " << ub;
    }
  };
}

// Fix v to a given value.
inline std::function<void(Model*)> Equality(IntegerVariable v, int64_t value) {
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
    absl::Span<const Literal> enforcement_literals, IntegerLiteral i) {
  return [=](Model* model) {
    auto* sat_solver = model->GetOrCreate<SatSolver>();
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    if (i.bound <= integer_trail->LowerBound(i.var)) {
      // Always true! nothing to do.
    } else if (i.bound > integer_trail->UpperBound(i.var)) {
      // Always false.
      std::vector<Literal> clause;
      for (const Literal literal : enforcement_literals) {
        clause.push_back(literal.Negated());
      }
      sat_solver->AddClauseDuringSearch(clause);
    } else {
      // TODO(user): Double check what happen when we associate a trivially
      // true or false literal.
      IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
      std::vector<Literal> clause{encoder->GetOrCreateAssociatedLiteral(i)};
      for (const Literal literal : enforcement_literals) {
        clause.push_back(literal.Negated());
      }
      sat_solver->AddClauseDuringSearch(clause);
    }
  };
}

// in_interval => v in [lb, ub].
inline std::function<void(Model*)> ImpliesInInterval(Literal in_interval,
                                                     IntegerVariable v,
                                                     int64_t lb, int64_t ub) {
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
inline std::function<std::vector<ValueLiteralPair>(Model*)> FullyEncodeVariable(
    IntegerVariable var) {
  return [=](Model* model) {
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    if (!encoder->VariableIsFullyEncoded(var)) {
      encoder->FullyEncodeVariable(var);
    }
    return encoder->FullDomainEncoding(var);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_INTEGER_H_
