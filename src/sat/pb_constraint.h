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
#include "base/hash.h"
#include "sat/sat_base.h"
#include "util/stats.h"

namespace operations_research {
namespace sat {

// The type of the integer coefficients in a pseudo-Boolean constraint.
// This is also used for the current value of a constraint or its bounds.
typedef int64 Coefficient;

// Represents a term in a pseudo-Boolean formula.
struct LiteralWithCoeff {
  LiteralWithCoeff() {}
  LiteralWithCoeff(Literal l, Coefficient c) : literal(l), coefficient(c) {}
  Literal literal;
  Coefficient coefficient;
  bool operator==(const LiteralWithCoeff& other) const {
    return literal.Index() == other.literal.Index()
        && coefficient == other.coefficient;
  }
};
inline std::ostream &operator<<(std::ostream &os, LiteralWithCoeff term) {
  os << term.coefficient << "[" << term.literal.DebugString() << "]";
  return os;
}

// Puts the given pseudo-Boolean formula in cannonical form:
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
//      lhs + bound_shift < cannonical_form < rhs + bound_shift
//
// Finaly, this will return false, if some integer overflow or underflow occured
// during the reduction to the cannonical form.
bool PbCannonicalForm(std::vector<LiteralWithCoeff>* cst,
                      Coefficient* bound_shift,
                      Coefficient* max_value);

// Returns true iff the linear constraint is in cannonical form.
bool LinearConstraintIsCannonical(const std::vector<LiteralWithCoeff>& cst);

// This class contains "half" the propagation logic for a constraint of the form
//   sum ci * li <= rhs, ci positive coefficients, li literals.
//
// The other half is implemented by the PbConstraints class below which takes
// care of updating the 'slack' value of this constraint:
//  - 'current_rhs' is rhs minus all the ci of the variables xi assigned to
//    true. Note that it is not updated as soon as xi is assigned, but only
//    later when this assignment is "processed" by the PbConstraints class.
//  - 'slack' is the distance from 'current_rhs' to the largest coefficient ci
//    smaller or equal to current_rhs. By definition, all the literals with
//    even larger coefficients that are yet 'processed' must be false for the
//    constraint to be satisfiable.
class UpperBoundedLinearConstraint {
 public:
  // Takes a pseudo-Boolean formula in canonical form.
  explicit UpperBoundedLinearConstraint(const std::vector<LiteralWithCoeff>& cst);

  // Returns true if the given terms are the same as the one in this constraint.
  bool HasIdenticalTerms(const std::vector<LiteralWithCoeff>& cst);
  Coefficient Rhs() const { return rhs_; }

  // Sets the rhs of this constraint. Compute the initial slack value using only
  // the literal with a trail index smaller than the given one. Enqueues on the
  // trail any propagated literals.
  bool InitializeRhs(Coefficient rhs, int trail_index, Coefficient* slack,
                     Trail* trail, std::vector<Literal>* conflict);

  // Tests for propagation and enqueues propagated literals on the trail.
  // Returns false if a conflict was detected, in which case conflict is filled.
  //
  // Preconditions:
  // - For each "processed" literal, the given slack value must have been
  //   decreased by its associated coefficient in the constraint. It must now
  //   be stricly negative.
  // - The given trail_index is the index of a true literal in the trail which
  //   just caused slack to become stricly negative. All literals with smaller
  //   index must have been "processed". All assigned literals with greater
  //   trail index are not yet "processed".
  //
  // The slack is updated to its new value.
  bool Propagate(int trail_index, Coefficient* slack,
                 Trail* trail, std::vector<Literal>* conflict);

  // Updates the given slack and the internal state.
  // This is the opposite of Propagate(). Each time a literal in unassigned,
  // the slack value must have been increased by its coefficient. This update
  // the slack to its new value.
  void Untrail(Coefficient* slack);

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
  // - It is possible that the same source_trail_index literal propagated more
  //   than one literal. It is important that the reason of each of these
  //   literal is exactly the same so we can use PbReasonCache.
  //
  // TODO(user): Maybe it is possible to derive a better reason by using more
  // information. For instance one could use the mask of literals that are
  // better to use during conflict minimization (namely the one already in the
  // 1-UIP conflict).
  void FillReason(const Trail& trail,
                  int source_trail_index,
                  std::vector<Literal>* reason);

 private:
  Coefficient GetCurrentRhsFromSlack(Coefficient slack) {
    return (index_ < 0) ? slack : coeffs_[index_] + slack;
  }
  void Update(Coefficient current_rhs, Coefficient* slack) {
    *slack = (index_ < 0) ? current_rhs : current_rhs - coeffs_[index_];
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
};

// Class responsible for managing a set of pseudo-Boolean constraints and their
// propagation.
class PbConstraints {
 public:
  explicit PbConstraints(Trail* trail) : trail_(trail),
                                         propagation_trail_index_(0),
                                         stats_("PbConstraints"),
                                         num_constraint_lookups_(0),
                                         num_slack_updates_(0) {}
  ~PbConstraints() {
    IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
    LOG(INFO) << "num_constraint_lookups_: " << num_constraint_lookups_;
    LOG(INFO) << "num_slack_updates_: " << num_slack_updates_;
  }

  // Changes the number of variables.
  void Resize(int num_variables) { to_update_.resize(num_variables << 1); }

  // Adds a constraint to the set of managed constraints.
  // Returns false if the constraint can never be satisfied.
  //
  // Note(user): There is an optimization if the last constraint added is the
  // same as the one we are trying to add.
  bool AddConstraint(const std::vector<LiteralWithCoeff>& cst, Coefficient rhs);
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
  void Untrail(int trail_index);

  // Returns the reason for the given variable assignement.
  // Note that this should only be called if the reason type is PB_PROPAGATION.
  const std::vector<Literal>& ReasonFor(VariableIndex var) const {
    SCOPED_TIME_STAT(&stats_);
    const AssignmentInfo& info = trail_->Info(var);
    DCHECK_EQ(info.type, AssignmentInfo::PB_PROPAGATION);
    info.pb_constraint->FillReason(
        *trail_, info.source_trail_index, &reason_scratchpad_);
    return reason_scratchpad_;
  }

 private:
  // Each constraint managed by this class is associated with an index.
  // The set of indices is always [0, num_constraints_).
  DEFINE_INT_TYPE(ConstraintIndex, int32);
  struct ConstraintIndexWithCoeff {
    ConstraintIndexWithCoeff(ConstraintIndex i, Coefficient c)
      : need_untrail_inspection(false), index(i), coefficient(c) {}
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

  // Temporary vector to hold the reason of a pseudo-Boolean propagation.
  mutable std::vector<Literal> reason_scratchpad_;

  // We use a dequeue to store the pseudo-Boolean constraint because we want
  // pointers to its elements to be still valid after more push_back().
  std::deque<UpperBoundedLinearConstraint> constraints_;

  // The current value of the slack for each constraints.
  ITIVector<ConstraintIndex, Coefficient> slacks_;

  // For each literal, the list of all the constraints that contains it together
  // with the literal coefficient in these constraints.
  ITIVector<LiteralIndex, std::vector<ConstraintIndexWithCoeff>> to_update_;

  // Bitset used to optimize the Untrail() function.
  SparseBitset<ConstraintIndex> to_untrail_;

  // Some statistics.
  mutable StatsGroup stats_;
  int64 num_constraint_lookups_;
  int64 num_slack_updates_;
  DISALLOW_COPY_AND_ASSIGN(PbConstraints);
};

// Boolean linear constraints can propagate a lot of literals at the same time.
// As a result, all these literals will have exactly the same reason. We can
// take advantage of that during the conflict computation/minimization using
// this simple "cache". On some problem, this can have a huge impact.
class PbReasonCache {
 public:
  explicit PbReasonCache(const Trail& trail) : trail_(trail) {}

  // Clears the cache. Call this before each conflict analysis.
  void Clear() { map_.clear(); }

  // If this function was already called since the last Clear(), returns the
  // variable that first called it. Otherwise, return the given variable.
  VariableIndex FirstVariableWithSameReason(VariableIndex var) {
    if (trail_.Info(var).type != AssignmentInfo::PB_PROPAGATION) return var;
    return map_.insert(std::make_pair(Key(var), var)).first->second;
  }

 private:
  const Trail& trail_;

  // This uniquely identify the reason for a linear constraint if there is
  // no backtracking. Note that we clear the cache before each conflict, so
  // this is enough.
  std::pair<UpperBoundedLinearConstraint*, int> Key(VariableIndex var) const {
    const AssignmentInfo& info = trail_.Info(var);
    return std::make_pair(info.pb_constraint, info.source_trail_index);
  }

  std::map<std::pair<UpperBoundedLinearConstraint*, int>, VariableIndex> map_;
  DISALLOW_COPY_AND_ASSIGN(PbReasonCache);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PB_CONSTRAINT_H_
