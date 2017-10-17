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

#ifndef OR_TOOLS_SAT_PB_CONSTRAINT_H_
#define OR_TOOLS_SAT_PB_CONSTRAINT_H_

#include <algorithm>
#include <unordered_map>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/span.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/hash.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace sat {

// The type of the integer coefficients in a pseudo-Boolean constraint.
// This is also used for the current value of a constraint or its bounds.
DEFINE_INT_TYPE(Coefficient, int64);

// IMPORTANT: We can't use numeric_limits<Coefficient>::max() which will compile
// but just returns zero!!
const Coefficient kCoefficientMax(
    std::numeric_limits<Coefficient::ValueType>::max());

// Represents a term in a pseudo-Boolean formula.
struct LiteralWithCoeff {
  LiteralWithCoeff() {}
  LiteralWithCoeff(Literal l, Coefficient c) : literal(l), coefficient(c) {}
  LiteralWithCoeff(Literal l, int64 c) : literal(l), coefficient(c) {}
  Literal literal;
  Coefficient coefficient;
  bool operator==(const LiteralWithCoeff& other) const {
    return literal.Index() == other.literal.Index() &&
           coefficient == other.coefficient;
  }
};
inline std::ostream& operator<<(std::ostream& os, LiteralWithCoeff term) {
  os << term.coefficient << "[" << term.literal.DebugString() << "]";
  return os;
}

// Puts the given Boolean linear expression in canonical form:
// - Merge all the literal corresponding to the same variable.
// - Remove zero coefficients.
// - Make all the coefficients positive.
// - Sort the terms by increasing coefficient values.
//
// This function also computes:
//  - max_value: the maximum possible value of the formula.
//  - bound_shift: which allows to updates initial bounds. That is, if an
//    initial pseudo-Boolean constraint was
//      lhs < initial_pb_formula < rhs
//    then the new one is:
//      lhs + bound_shift < canonical_form < rhs + bound_shift
//
// Finally, this will return false, if some integer overflow or underflow
// occurred during the reduction to the canonical form.
bool ComputeBooleanLinearExpressionCanonicalForm(
    std::vector<LiteralWithCoeff>* cst, Coefficient* bound_shift,
    Coefficient* max_value);

// Maps all the literals of the given constraint using the given mapping. The
// mapping may map a literal index to kTrueLiteralIndex or kFalseLiteralIndex in
// which case the literal will be considered fixed to the appropriate value.
//
// Note that this function also canonicalizes the constraint and updates
// bound_shift and max_value like ComputeBooleanLinearExpressionCanonicalForm()
// does.
//
// Finally, this will return false if some integer overflow or underflow
// occurred during the constraint simplification.
bool ApplyLiteralMapping(const ITIVector<LiteralIndex, LiteralIndex>& mapping,
                         std::vector<LiteralWithCoeff>* cst,
                         Coefficient* bound_shift, Coefficient* max_value);

// From a constraint 'expr <= ub' and the result (bound_shift, max_value) of
// calling ComputeBooleanLinearExpressionCanonicalForm() on 'expr', this returns
// a new rhs such that 'canonical expression <= rhs' is an equivalent
// constraint. This function deals with all the possible overflow corner cases.
//
// The result will be in [-1, max_value] where -1 means unsatisfiable and
// max_value means trivialy satisfiable.
Coefficient ComputeCanonicalRhs(Coefficient upper_bound,
                                Coefficient bound_shift, Coefficient max_value);

// Same as ComputeCanonicalRhs(), but uses the initial constraint lower bound
// instead. From a constraint 'lb <= expression', this returns a rhs such that
// 'canonical expression with literals negated <= rhs'.
//
// Note that the range is also [-1, max_value] with the same meaning.
Coefficient ComputeNegatedCanonicalRhs(Coefficient lower_bound,
                                       Coefficient bound_shift,
                                       Coefficient max_value);

// Returns true iff the Boolean linear expression is in canonical form.
bool BooleanLinearExpressionIsCanonical(
    const std::vector<LiteralWithCoeff>& cst);

// Given a Boolean linear constraint in canonical form, simplify its
// coefficients using simple heuristics.
void SimplifyCanonicalBooleanLinearConstraint(
    std::vector<LiteralWithCoeff>* cst, Coefficient* rhs);

// Holds a set of boolean linear constraints in canonical form:
// - The constraint is a linear sum of LiteralWithCoeff <= rhs.
// - The linear sum satisfies the properties described in
//   ComputeBooleanLinearExpressionCanonicalForm().
//
// TODO(user): Simplify further the constraints.
//
// TODO(user): Remove the duplication between this and what the sat solver
// is doing in AddLinearConstraint() which is basically the same.
//
// TODO(user): Remove duplicate constraints? some problems have them, and
// this is not ideal for the symmetry computation since it leads to a lot of
// symmetries of the associated graph that are not useful.
class CanonicalBooleanLinearProblem {
 public:
  CanonicalBooleanLinearProblem() {}

  // Adds a new constraint to the problem. The bounds are inclusive.
  // Returns false in case of a possible overflow or if the constraint is
  // never satisfiable.
  //
  // TODO(user): Use a return status to distinguish errors if needed.
  bool AddLinearConstraint(bool use_lower_bound, Coefficient lower_bound,
                           bool use_upper_bound, Coefficient upper_bound,
                           std::vector<LiteralWithCoeff>* cst);

  // Getters. All the constraints are guaranteed to be in canonical form.
  int NumConstraints() const { return constraints_.size(); }
  const Coefficient Rhs(int i) const { return rhs_[i]; }
  const std::vector<LiteralWithCoeff>& Constraint(int i) const {
    return constraints_[i];
  }

 private:
  bool AddConstraint(const std::vector<LiteralWithCoeff>& cst,
                     Coefficient max_value, Coefficient rhs);

  std::vector<Coefficient> rhs_;
  std::vector<std::vector<LiteralWithCoeff>> constraints_;
  DISALLOW_COPY_AND_ASSIGN(CanonicalBooleanLinearProblem);
};

// Encode a constraint sum term <= rhs, where each term is a positive
// Coefficient times a literal. This class allows efficient modification of the
// constraint and is used during pseudo-Boolean resolution.
class MutableUpperBoundedLinearConstraint {
 public:
  // This must be called before any other functions is used with an higher
  // variable index.
  void ClearAndResize(int num_variables);

  // Reset the constraint to 0 <= 0.
  // Note that the contraint size stays the same.
  void ClearAll();

  // Returns the coefficient (>= 0) of the given variable.
  Coefficient GetCoefficient(BooleanVariable var) const {
    return AbsCoefficient(terms_[var]);
  }

  // Returns the literal under which the given variable appear in the
  // constraint. Note that if GetCoefficient(var) == 0 this just returns
  // Literal(var, true).
  Literal GetLiteral(BooleanVariable var) const {
    return Literal(var, terms_[var] > 0);
  }

  // If we have a lower bounded constraint sum terms >= rhs, then it is trivial
  // to see that the coefficient of any term can be reduced to rhs if it is
  // bigger. This does exactly this operation, but on the upper bounded
  // representation.
  //
  // If we take a constraint sum ci.xi <= rhs, take its negation and add max_sum
  // on both side, we have sum ci.(1 - xi) >= max_sum - rhs
  // So every ci > (max_sum - rhs) can be replacend by (max_sum - rhs).
  // Not that this operation also change the original rhs of the constraint.
  void ReduceCoefficients();

  // Same as ReduceCoefficients() but only consider the coefficient of the given
  // variable.
  void ReduceGivenCoefficient(BooleanVariable var) {
    const Coefficient bound = max_sum_ - rhs_;
    const Coefficient diff = GetCoefficient(var) - bound;
    if (diff > 0) {
      rhs_ -= diff;
      max_sum_ -= diff;
      terms_[var] = (terms_[var] > 0) ? bound : -bound;
    }
  }

  // Compute the constraint slack assuming that only the variables with index <
  // trail_index are assigned.
  Coefficient ComputeSlackForTrailPrefix(const Trail& trail,
                                         int trail_index) const;

  // Same as ReduceCoefficients() followed by ComputeSlackForTrailPrefix(). It
  // allows to loop only once over all the terms of the constraint instead of
  // doing it twice. This helps since doing that can be the main bottleneck.
  //
  // Note that this function assumes that the returned slack will be negative.
  // This allow to DCHECK some assumptions on what coefficients can be reduced
  // or not.
  //
  // TODO(user): Ideally the slack should be maitainable incrementally.
  Coefficient ReduceCoefficientsAndComputeSlackForTrailPrefix(
      const Trail& trail, int trail_index);

  // Relaxes the constraint so that:
  // - ComputeSlackForTrailPrefix(trail, trail_index) == target;
  // - All the variable that where propagated given the assignment < trail_index
  //   are still propagated.
  //
  // As a precondition, ComputeSlackForTrailPrefix(trail, trail_index) >= target
  // Note that nothing happen if the slack is already equals to target.
  //
  // Algorithm: Let diff = slack - target (>= 0). We will split the constraint
  // linear expression in 3 parts:
  // - P1: the true variables (only the one assigned < trail_index).
  // - P2: the other variables with a coeff > diff.
  //       Note that all these variables where the propagated ones.
  // - P3: the other variables with a coeff <= diff.
  // We can then transform P1 + P2 + P3 <= rhs_ into P1 + P2' <= rhs_ - diff
  // Where P2' is the same sum as P2 with all the coefficient reduced by diff.
  //
  // Proof: Given the old constraint, we want to show that the relaxed one is
  // always true. If all the variable in P2' are false, then
  // P1 <= rhs_ - slack <= rhs_ - diff is always true. If at least one of the
  // P2' variable is true, then P2 >= P2' + diff and we have
  // P1 + P2' + diff <= P1 + P2 <= rhs_.
  void ReduceSlackTo(const Trail& trail, int trail_index,
                     Coefficient initial_slack, Coefficient target);

  // Copies this constraint into a std::vector<LiteralWithCoeff> representation.
  void CopyIntoVector(std::vector<LiteralWithCoeff>* output);

  // Adds a non-negative value to this constraint Rhs().
  void AddToRhs(Coefficient value) {
    CHECK_GE(value, 0);
    rhs_ += value;
  }
  Coefficient Rhs() const { return rhs_; }
  Coefficient MaxSum() const { return max_sum_; }

  // Adds a term to this constraint. This is in the .h for efficiency.
  // The encoding used internally is described below in the terms_ comment.
  void AddTerm(Literal literal, Coefficient coeff) {
    CHECK_GT(coeff, 0);
    const BooleanVariable var = literal.Variable();
    const Coefficient term_encoding = literal.IsPositive() ? coeff : -coeff;
    if (literal != GetLiteral(var)) {
      // The two terms are of opposite sign, a "cancelation" happens.
      // We need to change the encoding of the lower magnitude term.
      // - If term > 0, term . x       -> term . (x - 1) + term
      // - If term < 0, term . (x - 1) -> term . x       - term
      // In both cases, rhs -= abs(term).
      rhs_ -= std::min(coeff, AbsCoefficient(terms_[var]));
      max_sum_ += AbsCoefficient(term_encoding + terms_[var]) -
                  AbsCoefficient(terms_[var]);
    } else {
      // Both terms are of the same sign (or terms_[var] is zero).
      max_sum_ += coeff;
    }
    CHECK_GE(max_sum_, 0) << "Overflow";
    terms_[var] += term_encoding;
    non_zeros_.Set(var);
  }

  // Returns the "cancelation" amount of AddTerm(literal, coeff).
  Coefficient CancelationAmount(Literal literal, Coefficient coeff) const {
    DCHECK_GT(coeff, 0);
    const BooleanVariable var = literal.Variable();
    if (literal == GetLiteral(var)) return Coefficient(0);
    return std::min(coeff, AbsCoefficient(terms_[var]));
  }

  // Returns a set of positions that contains all the non-zeros terms of the
  // constraint. Note that this set can also contains some zero terms.
  const std::vector<BooleanVariable>& PossibleNonZeros() const {
    return non_zeros_.PositionsSetAtLeastOnce();
  }

  // Returns a std::string representation of the constraint.
  std::string DebugString();

 private:
  Coefficient AbsCoefficient(Coefficient a) const { return a > 0 ? a : -a; }

  // Only used for DCHECK_EQ(max_sum_, ComputeMaxSum());
  Coefficient ComputeMaxSum() const;

  // The encoding is special:
  // - If terms_[x] > 0, then the associated term is 'terms_[x] . x'
  // - If terms_[x] < 0, then the associated term is 'terms_[x] . (x - 1)'
  ITIVector<BooleanVariable, Coefficient> terms_;

  // The right hand side of the constraint (sum terms <= rhs_).
  Coefficient rhs_;

  // The constraint maximum sum (i.e. sum of the absolute term coefficients).
  // Note that checking the integer overflow on this sum is enough.
  Coefficient max_sum_;

  // Contains the possibly non-zeros terms_ value.
  SparseBitset<BooleanVariable> non_zeros_;
};

// A simple "helper" class to enqueue a propagated literal on the trail and
// keep the information needed to explain it when requested.
class UpperBoundedLinearConstraint;

struct PbConstraintsEnqueueHelper {
  void Enqueue(Literal l, int source_trail_index,
               UpperBoundedLinearConstraint* ct, Trail* trail) {
    reasons[trail->Index()] = {source_trail_index, ct};
    trail->Enqueue(l, propagator_id);
  }

  // The propagator id of PbConstraints.
  int propagator_id;

  // A temporary vector to store the last conflict.
  std::vector<Literal> conflict;

  // Information needed to recover the reason of an Enqueue().
  // Indexed by trail_index.
  struct ReasonInfo {
    int source_trail_index;
    UpperBoundedLinearConstraint* pb_constraint;
  };
  std::vector<ReasonInfo> reasons;
};

// This class contains half the propagation logic for a constraint of the form
//
//   sum ci * li <= rhs, ci positive coefficients, li literals.
//
// The other half is implemented by the PbConstraints class below which takes
// care of updating the 'threshold' value of this constraint:
//  - 'slack' is rhs minus all the ci of the variables xi assigned to
//    true. Note that it is not updated as soon as xi is assigned, but only
//    later when this assignment is "processed" by the PbConstraints class.
//  - 'threshold' is the distance from 'slack' to the largest coefficient ci
//    smaller or equal to slack. By definition, all the literals with
//    even larger coefficients that are yet 'processed' must be false for the
//    constraint to be satisfiable.
class UpperBoundedLinearConstraint {
 public:
  // Takes a pseudo-Boolean formula in canonical form.
  explicit UpperBoundedLinearConstraint(
      const std::vector<LiteralWithCoeff>& cst);

  // Returns true if the given terms are the same as the one in this constraint.
  bool HasIdenticalTerms(const std::vector<LiteralWithCoeff>& cst);
  Coefficient Rhs() const { return rhs_; }

  // Sets the rhs of this constraint. Compute the initial threshold value using
  // only the literal with a trail index smaller than the given one. Enqueues on
  // the trail any propagated literals.
  //
  // Returns false if the preconditions described in
  // PbConstraints::AddConstraint() are not meet.
  bool InitializeRhs(Coefficient rhs, int trail_index, Coefficient* threshold,
                     Trail* trail, PbConstraintsEnqueueHelper* helper);

  // Tests for propagation and enqueues propagated literals on the trail.
  // Returns false if a conflict was detected, in which case conflict is filled.
  //
  // Preconditions:
  // - For each "processed" literal, the given threshold value must have been
  //   decreased by its associated coefficient in the constraint. It must now
  //   be stricly negative.
  // - The given trail_index is the index of a true literal in the trail which
  //   just caused threshold to become stricly negative. All literals with
  //   smaller index must have been "processed". All assigned literals with
  //   greater trail index are not yet "processed".
  //
  // The threshold is updated to its new value.
  bool Propagate(int trail_index, Coefficient* threshold, Trail* trail,
                 PbConstraintsEnqueueHelper* helper);

  // Updates the given threshold and the internal state. This is the opposite of
  // Propagate(). Each time a literal in unassigned, the threshold value must
  // have been increased by its coefficient. This update the threshold to its
  // new value.
  void Untrail(Coefficient* threshold, int trail_index);

  // Provided that the literal with given source_trail_index was the one that
  // propagated the conflict or the literal we wants to explain, then this will
  // compute the reason.
  //
  // Some properties of the reason:
  // - Literals of level 0 are removed.
  // - It will always contain the literal with given source_trail_index (except
  //   if it is of level 0).
  // - We make the reason more compact by greedily removing terms with small
  //   coefficients that would not have changed the propagation.
  //
  // TODO(user): Maybe it is possible to derive a better reason by using more
  // information. For instance one could use the mask of literals that are
  // better to use during conflict minimization (namely the one already in the
  // 1-UIP conflict).
  void FillReason(const Trail& trail, int source_trail_index,
                  BooleanVariable propagated_variable,
                  std::vector<Literal>* reason);

  // Same operation as SatSolver::ResolvePBConflict(), the only difference is
  // that here the reason for var is *this.
  void ResolvePBConflict(const Trail& trail, BooleanVariable var,
                         MutableUpperBoundedLinearConstraint* conflict,
                         Coefficient* conflict_slack);

  // Adds this pb constraint into the given mutable one.
  //
  // TODO(user): Provides instead an easy to use iterator over an
  // UpperBoundedLinearConstraint and move this function to
  // MutableUpperBoundedLinearConstraint.
  void AddToConflict(MutableUpperBoundedLinearConstraint* conflict);

  // Compute the sum of the "cancelation" in AddTerm() if *this is added to
  // the given conflict. The sum doesn't take into account literal assigned with
  // a trail index smaller than the given one.
  //
  // Note(user): Currently, this is only used in DCHECKs.
  Coefficient ComputeCancelation(
      const Trail& trail, int trail_index,
      const MutableUpperBoundedLinearConstraint& conflict);

  // API to mark a constraint for deletion before actually deleting it.
  void MarkForDeletion() { is_marked_for_deletion_ = true; }
  bool is_marked_for_deletion() const { return is_marked_for_deletion_; }

  // Only learned constraints are considered for deletion during the constraint
  // cleanup phase. We also can't delete variables used as a reason.
  void set_is_learned(bool is_learned) { is_learned_ = is_learned; }
  bool is_learned() const { return is_learned_; }
  bool is_used_as_a_reason() const { return first_reason_trail_index_ != -1; }

  // Activity of the constraint. Only low activity constraint will be deleted
  // during the constraint cleanup phase.
  void set_activity(double activity) { activity_ = activity; }
  double activity() const { return activity_; }

  // Returns a fingerprint of the constraint linear expression (without rhs).
  // This is used for duplicate detection.
  int64 hash() const { return hash_; }

  // This is used to get statistics of the number of literals inspected by a
  // Propagate() call.
  int already_propagated_end() const { return already_propagated_end_; }

 private:
  Coefficient GetSlackFromThreshold(Coefficient threshold) {
    return (index_ < 0) ? threshold : coeffs_[index_] + threshold;
  }
  void Update(Coefficient slack, Coefficient* threshold) {
    *threshold = (index_ < 0) ? slack : slack - coeffs_[index_];
    already_propagated_end_ = starts_[index_ + 1];
  }

  // Constraint management fields.
  // TODO(user): Rearrange and specify bit size to minimize memory usage.
  bool is_marked_for_deletion_;
  bool is_learned_;
  int first_reason_trail_index_;
  double activity_;

  // Constraint propagation fields.
  int index_;
  int already_propagated_end_;

  // In the internal representation, we merge the terms with the same
  // coefficient.
  // - literals_ contains all the literal of the constraint sorted by increasing
  //   coefficients.
  // - coeffs_ contains unique increasing coefficients.
  // - starts_[i] is the index in literals_ of the first literal with
  //   coefficient coeffs_[i].
  std::vector<Coefficient> coeffs_;
  std::vector<int> starts_;
  std::vector<Literal> literals_;
  Coefficient rhs_;

  int64 hash_;
};

// Class responsible for managing a set of pseudo-Boolean constraints and their
// propagation.
class PbConstraints : public SatPropagator {
 public:
  PbConstraints()
      : SatPropagator("PbConstraints"),
        conflicting_constraint_index_(-1),
        num_learned_constraint_before_cleanup_(0),
        constraint_activity_increment_(1.0),
        stats_("PbConstraints"),
        num_constraint_lookups_(0),
        num_inspected_constraint_literals_(0),
        num_threshold_updates_(0) {}
  ~PbConstraints() override {
    IF_STATS_ENABLED({
      LOG(INFO) << stats_.StatString();
      LOG(INFO) << "num_constraint_lookups_: " << num_constraint_lookups_;
      LOG(INFO) << "num_threshold_updates_: " << num_threshold_updates_;
    });
  }

  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;
  gtl::Span<Literal> Reason(const Trail& trail,
                                   int trail_index) const final;

  // Changes the number of variables.
  void Resize(int num_variables) {
    // Note that we avoid using up memory in the common case where there is no
    // pb constraints at all. If there is 10 million variables, this vector
    // alone will take 480 MB!
    if (!constraints_.empty()) {
      to_update_.resize(num_variables << 1);
      enqueue_helper_.reasons.resize(num_variables);
    }
  }

  // Parameter management.
  void SetParameters(const SatParameters& parameters) {
    parameters_ = parameters;
  }

  // Adds a constraint in canonical form to the set of managed constraints. Note
  // that this detects constraints with exactly the same terms. In this case,
  // the constraint rhs is updated if the new one is lower or nothing is done
  // otherwise.
  //
  // There are some preconditions, and the function will return false if they
  // are not met. The constraint can be added when the trail is not empty,
  // however given the current propagated assignment:
  // - The constraint cannot be conflicting.
  // - The constraint cannot have propagated at an earlier decision level.
  bool AddConstraint(const std::vector<LiteralWithCoeff>& cst, Coefficient rhs,
                     Trail* trail);

  // Same as AddConstraint(), but also marks the added constraint as learned
  // so that it can be deleted during the constraint cleanup phase.
  bool AddLearnedConstraint(const std::vector<LiteralWithCoeff>& cst,
                            Coefficient rhs, Trail* trail);

  // Returns the number of constraints managed by this class.
  int NumberOfConstraints() const { return constraints_.size(); }

  // ConflictingConstraint() returns the last PB constraint that caused a
  // conflict. Calling ClearConflictingConstraint() reset this to nullptr.
  //
  // TODO(user): This is a hack to get the PB conflict, because the rest of
  // the solver API assume only clause conflict. Find a cleaner way?
  void ClearConflictingConstraint() { conflicting_constraint_index_ = -1; }
  UpperBoundedLinearConstraint* ConflictingConstraint() {
    if (conflicting_constraint_index_ == -1) return nullptr;
    return constraints_[conflicting_constraint_index_.value()].get();
  }

  // Returns the underlying UpperBoundedLinearConstraint responsible for
  // assigning the literal at given trail index.
  UpperBoundedLinearConstraint* ReasonPbConstraint(int trail_index) const;

  // Activity update functions.
  // TODO(user): Remove duplication with other activity update functions.
  void BumpActivity(UpperBoundedLinearConstraint* constraint);
  void RescaleActivities(double scaling_factor);
  void UpdateActivityIncrement();

  // Only used for testing.
  void DeleteConstraint(int index) {
    constraints_[index]->MarkForDeletion();
    DeleteConstraintMarkedForDeletion();
  }

  // Some statistics.
  int64 num_constraint_lookups() const { return num_constraint_lookups_; }
  int64 num_inspected_constraint_literals() const {
    return num_inspected_constraint_literals_;
  }
  int64 num_threshold_updates() const { return num_threshold_updates_; }

 private:
  bool PropagateNext(Trail* trail);

  // Same function as the clause related one is SatSolver().
  // TODO(user): Remove duplication.
  void ComputeNewLearnedConstraintLimit();
  void DeleteSomeLearnedConstraintIfNeeded();

  // Deletes all the UpperBoundedLinearConstraint for which
  // is_marked_for_deletion() is true. This is relatively slow in O(number of
  // terms in all constraints).
  void DeleteConstraintMarkedForDeletion();

  // Each constraint managed by this class is associated with an index.
  // The set of indices is always [0, num_constraints_).
  //
  // Note(user): this complicate things during deletion, but the propagation is
  // about two times faster with this implementation than one with direct
  // pointer to an UpperBoundedLinearConstraint. The main reason for this is
  // probably that the thresholds_ vector is a lot more efficient cache-wise.
  DEFINE_INT_TYPE(ConstraintIndex, int32);
  struct ConstraintIndexWithCoeff {
    ConstraintIndexWithCoeff() {}  // Needed for vector.resize()
    ConstraintIndexWithCoeff(bool n, ConstraintIndex i, Coefficient c)
        : need_untrail_inspection(n), index(i), coefficient(c) {}
    bool need_untrail_inspection;
    ConstraintIndex index;
    Coefficient coefficient;
  };

  // The set of all pseudo-boolean constraint managed by this class.
  std::vector<std::unique_ptr<UpperBoundedLinearConstraint>> constraints_;

  // The current value of the threshold for each constraints.
  ITIVector<ConstraintIndex, Coefficient> thresholds_;

  // For each literal, the list of all the constraints that contains it together
  // with the literal coefficient in these constraints.
  ITIVector<LiteralIndex, std::vector<ConstraintIndexWithCoeff>> to_update_;

  // Bitset used to optimize the Untrail() function.
  SparseBitset<ConstraintIndex> to_untrail_;

  // Pointers to the constraints grouped by their hash.
  // This is used to find duplicate constraints by AddConstraint().
  std::unordered_map<int64, std::vector<UpperBoundedLinearConstraint*>>
      possible_duplicates_;

  // Helper to enqueue propagated literals on the trail and store their reasons.
  PbConstraintsEnqueueHelper enqueue_helper_;

  // Last conflicting PB constraint index. This is reset to -1 when
  // ClearConflictingConstraint() is called.
  ConstraintIndex conflicting_constraint_index_;

  // Used for the constraint cleaning policy.
  int target_number_of_learned_constraint_;
  int num_learned_constraint_before_cleanup_;
  double constraint_activity_increment_;

  // Algorithm parameters.
  SatParameters parameters_;

  // Some statistics.
  mutable StatsGroup stats_;
  int64 num_constraint_lookups_;
  int64 num_inspected_constraint_literals_;
  int64 num_threshold_updates_;
  DISALLOW_COPY_AND_ASSIGN(PbConstraints);
};

// Boolean linear constraints can propagate a lot of literals at the same time.
// As a result, all these literals will have exactly the same reason. It is
// important to take advantage of that during the conflict
// computation/minimization. On some problem, this can have a huge impact.
//
// TODO(user): With the new SAME_REASON_AS mechanism, this is more general so
// move out of pb_constraint.
class VariableWithSameReasonIdentifier {
 public:
  explicit VariableWithSameReasonIdentifier(const Trail& trail)
      : trail_(trail) {}

  void Resize(int num_variables) {
    first_variable_.resize(num_variables);
    seen_.ClearAndResize(BooleanVariable(num_variables));
  }

  // Clears the cache. Call this before each conflict analysis.
  void Clear() { seen_.ClearAll(); }

  // Returns the first variable with exactly the same reason as 'var' on which
  // this function was called since the last Clear(). Note that if no variable
  // had the same reason, then var is returned.
  BooleanVariable FirstVariableWithSameReason(BooleanVariable var) {
    if (seen_[var]) return first_variable_[var];
    const BooleanVariable reference_var =
        trail_.ReferenceVarWithSameReason(var);
    if (reference_var == var) return var;
    if (seen_[reference_var]) return first_variable_[reference_var];
    seen_.Set(reference_var);
    first_variable_[reference_var] = var;
    return var;
  }

 private:
  const Trail& trail_;
  ITIVector<BooleanVariable, BooleanVariable> first_variable_;
  SparseBitset<BooleanVariable> seen_;

  DISALLOW_COPY_AND_ASSIGN(VariableWithSameReasonIdentifier);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PB_CONSTRAINT_H_
