// Copyright 2010-2013 Google
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

#include <deque>
#include <limits>
#include "sat/sat_base.h"
#include "util/stats.h"

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
// occured during the reduction to the canonical form.
bool ComputeBooleanLinearExpressionCanonicalForm(std::vector<LiteralWithCoeff>* cst,
                                                 Coefficient* bound_shift,
                                                 Coefficient* max_value);

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
bool BooleanLinearExpressionIsCanonical(const std::vector<LiteralWithCoeff>& cst);

// Given a Boolean linear constraint in canonical form, simplify its
// coefficients using simple heuristics.
void SimplifyCanonicalBooleanLinearConstraint(std::vector<LiteralWithCoeff>* cst,
                                              Coefficient* rhs);

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
  bool AddConstraint(const std::vector<LiteralWithCoeff>& cst, Coefficient max_value,
                     Coefficient rhs);

  std::vector<Coefficient> rhs_;
  std::vector<std::vector<LiteralWithCoeff>> constraints_;
  DISALLOW_COPY_AND_ASSIGN(CanonicalBooleanLinearProblem);
};

// This class contains "half" the propagation logic for a constraint of the form
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
  UpperBoundedLinearConstraint(const std::vector<LiteralWithCoeff>& cst,
                               ResolutionNode* node);

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
                     Trail* trail, std::vector<Literal>* conflict);

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
                 std::vector<Literal>* conflict);

  // Updates the given threshold and the internal state. This is the opposite of
  // Propagate(). Each time a literal in unassigned, the threshold value must
  // have been increased by its coefficient. This update the threshold to its
  // new value.
  void Untrail(Coefficient* threshold);

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
                  VariableIndex propagated_variable, std::vector<Literal>* reason);

  // Returns the resolution node associated to this constraint. Note that it can
  // be nullptr if the solver is not configured to compute the reason for an
  // unsatisfiable problem or if this constraint is not relevant for the current
  // core computation.
  ResolutionNode* ResolutionNodePointer() const { return node_; }
  void ChangeResolutionNode(ResolutionNode* node) { node_ = node; }

 private:
  Coefficient GetSlackFromThreshold(Coefficient threshold) {
    return (index_ < 0) ? threshold : coeffs_[index_] + threshold;
  }
  void Update(Coefficient slack, Coefficient* threshold) {
    *threshold = (index_ < 0) ? slack : slack - coeffs_[index_];
    already_propagated_end_ = starts_[index_ + 1];
  }

  int index_;
  int already_propagated_end_;
  Coefficient rhs_;

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

  ResolutionNode* node_;
};

// Class responsible for managing a set of pseudo-Boolean constraints and their
// propagation.
class PbConstraints {
 public:
  explicit PbConstraints(Trail* trail)
      : trail_(trail),
        propagation_trail_index_(0),
        stats_("PbConstraints"),
        num_constraint_lookups_(0),
        num_threshold_updates_(0) {}
  ~PbConstraints() {
    IF_STATS_ENABLED({
      LOG(INFO) << stats_.StatString();
      LOG(INFO) << "num_constraint_lookups_: " << num_constraint_lookups_;
      LOG(INFO) << "num_threshold_updates_: " << num_threshold_updates_;
    });
  }

  // Changes the number of variables.
  void Resize(int num_variables) { to_update_.resize(num_variables << 1); }

  // Adds a constraint in canonical form to the set of managed constraints.
  //
  // There are some preconditions, and the function will return false if they
  // are not met. The constraint can be added when the trail is not empty,
  // however given the current propagated assignment:
  // - The constraint cannot be conflicting.
  // - The constraint cannot have propagated at an earlier decision level.
  //
  // Note(user): There is an optimization if the last constraint added is the
  // same as the one we are trying to add.
  bool AddConstraint(const std::vector<LiteralWithCoeff>& cst, Coefficient rhs,
                     ResolutionNode* node);
  int NumberOfConstraints() const { return constraints_.size(); }

  // If some literals enqueued on the trail haven't been processed by this class
  // then PropagationNeeded() will returns true. In this case, it is possible to
  // call PropagateNext() to process the first of these literals.
  bool PropagationNeeded() const {
    return !constraints_.empty() && propagation_trail_index_ < trail_->Index();
  }
  bool PropagateNext();

  // Reverts the state so that all the literals with trail index greater or
  // equals to the given on are not processed for propagation.
  //
  // Note that this should only be called at the decision level boundaries.
  void Untrail(int trail_index);

  // Computes the reason for the given variable assignement.
  // Note that this should only be called if the reason type is PB_PROPAGATION.
  void ReasonFor(VariableIndex var, std::vector<Literal>* reason) const {
    SCOPED_TIME_STAT(&stats_);
    const AssignmentInfo& info = trail_->Info(var);
    DCHECK_EQ(trail_->InitialAssignmentType(var),
              AssignmentInfo::PB_PROPAGATION);
    info.pb_constraint->FillReason(*trail_, info.source_trail_index, var,
                                   reason);
  }

 private:
  // Each constraint managed by this class is associated with an index.
  // The set of indices is always [0, num_constraints_).
  DEFINE_INT_TYPE(ConstraintIndex, int32);
  struct ConstraintIndexWithCoeff {
    ConstraintIndexWithCoeff(bool n, ConstraintIndex i, Coefficient c)
        : need_untrail_inspection(n), index(i), coefficient(c) {}
    bool need_untrail_inspection;
    ConstraintIndex index;
    Coefficient coefficient;
  };

  // The solver trail that contains the variables assignements and all the
  // assignment info.
  Trail* trail_;

  // Index of the first assigned variable from the trail that is not yet
  // processed by this class.
  int propagation_trail_index_;

  // Temporary vector to hold the last conflict of a pseudo-Boolean propagation.
  mutable std::vector<Literal> conflict_scratchpad_;

  // We use a dequeue to store the pseudo-Boolean constraint because we want
  // pointers to its elements to be still valid after more push_back().
  std::deque<UpperBoundedLinearConstraint> constraints_;

  // The current value of the threshold for each constraints.
  ITIVector<ConstraintIndex, Coefficient> thresholds_;

  // For each literal, the list of all the constraints that contains it together
  // with the literal coefficient in these constraints.
  ITIVector<LiteralIndex, std::vector<ConstraintIndexWithCoeff>> to_update_;

  // Bitset used to optimize the Untrail() function.
  SparseBitset<ConstraintIndex> to_untrail_;

  // Some statistics.
  mutable StatsGroup stats_;
  int64 num_constraint_lookups_;
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
    seen_.ClearAndResize(VariableIndex(num_variables));
  }

  // Clears the cache. Call this before each conflict analysis.
  void Clear() { seen_.ClearAll(); }

  // Returns the first variable with exactly the same reason as 'var' on which
  // this function was called since the last Clear(). Note that if no variable
  // had the same reason, then var is returned.
  VariableIndex FirstVariableWithSameReason(VariableIndex var) {
    if (seen_[var]) return first_variable_[var];
    if (trail_.Info(var).type != AssignmentInfo::SAME_REASON_AS) return var;
    const VariableIndex reference_var = trail_.Info(var).reference_var;
    if (seen_[reference_var]) return first_variable_[reference_var];
    seen_.Set(reference_var);
    first_variable_[reference_var] = var;
    return var;
  }

 private:
  const Trail& trail_;
  ITIVector<VariableIndex, VariableIndex> first_variable_;
  SparseBitset<VariableIndex> seen_;

  DISALLOW_COPY_AND_ASSIGN(VariableWithSameReasonIdentifier);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PB_CONSTRAINT_H_
