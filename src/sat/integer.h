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

#include "base/int_type.h"
#include "sat/model.h"
#include "sat/sat_base.h"
#include "sat/sat_solver.h"
#include "util/bitset.h"
#include "util/iterators.h"

namespace operations_research {
namespace sat {

// Index of an IntegerVariable that can be bounded on both sides.
DEFINE_INT_TYPE(IntegerVariable, int32);
const IntegerVariable kNoIntegerVariable(-1);

// Internally we encode the two bounds of an IntegerVariable with a LbVar.
// The upper bound is encoded in negated form so that a LbVar always move in
// the same direction as more decisions are taken.
DEFINE_INT_TYPE(LbVar, int32);
const LbVar kNoLbVar(-1);

// Helper functions to manipulate an IntegerVariable and its associated LbVars.
inline LbVar OtherLbVar(LbVar var) { return LbVar(var.value() ^ 1); }
inline LbVar LbVarOf(IntegerVariable i) { return LbVar(2 * i.value()); }
inline LbVar MinusUbVarOf(IntegerVariable i) {
  return LbVar(2 * i.value() + 1);
}
inline IntegerVariable IntegerVariableOf(LbVar var) {
  return IntegerVariable(var.value() / 2);
}

// The integer equivalent of a literal.
// It represents an IntegerVariable and an upper/lower bound on it.
struct IntegerLiteral {
  static IntegerLiteral GreaterOrEqual(IntegerVariable i, int bound);
  static IntegerLiteral LowerOrEqual(IntegerVariable i, int bound);
  static IntegerLiteral FromLbVar(LbVar var, int bound);

  // Our external API uses IntegerVariable and LbVar, but internally we
  // only use LbVar, so we simply use an int for simplicity.
  //
  // TODO(user): We can't use const because we want to be able to copy a
  // std::vector<IntegerLiteral>. So instead make them private and provide some
  // getters.
  /*const*/ int var;
  /*const*/ int bound;

 private:
  IntegerLiteral(LbVar v, int b) : var(v.value()), bound(b) {}
};

// This class maintains a set of integer variables with their current bounds.
// Bounds can be propagated from an external "source" and this class helps
// to maintain the reason for each propagation.
//
// TODO(user): Add support for a lazy encoding of the integer variable in SAT.
class IntegerTrail : public Propagator {
 public:
  IntegerTrail() : Propagator("IntegerTrail"), num_enqueues_(0) {}
  ~IntegerTrail() final {}

  static IntegerTrail* CreateInModel(Model* model) {
    IntegerTrail* integer_trail = new IntegerTrail();
    model->GetOrCreate<SatSolver>()->AddPropagator(
        std::unique_ptr<IntegerTrail>(integer_trail));
    return integer_trail;
  }

  // Propagator interface. These functions make sure the current bounds
  // information is in sync with the current solver literal trail. Any
  // class/propagator using this class must make sure it is synced to the
  // correct state before calling any of its functions.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int literal_trail_index) final;
  ClauseRef Reason(const Trail& trail, int trail_index) const final;

  // Adds a new integer variable. Adding integer variable can only be done when
  // the decision level is zero (checked). The given bounds are INCLUSIVE.
  IntegerVariable AddIntegerVariable(int lower_bound, int upper_bound);

  // Returns the current lower/upper bound of the given integer variable.
  int LowerBound(IntegerVariable i) const;
  int UpperBound(IntegerVariable i) const;

  // Returns the integer literal that represent the current lower/upper bound of
  // the given integer variable.
  IntegerLiteral LowerBoundAsLiteral(IntegerVariable i) const;
  IntegerLiteral UpperBoundAsLiteral(IntegerVariable i) const;

  // Advanced usage (for efficienty). Sometimes it is better to directly
  // manipulates the internal representation using LbVarOf() and MinusUbVarOf().
  int NumLbVars() const { return vars_.size(); }
  int Value(LbVar var) const;
  IntegerLiteral ValueAsLiteral(LbVar var) const;

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
  void Enqueue(IntegerLiteral bound, const std::vector<Literal>& literal_reason,
               const std::vector<IntegerLiteral>& integer_reason);

  // Enqueues the given literal on the trail. The two returned vector pointers
  // will point to empty vectors that can be filled to store the reason of this
  // assignment. They are only valid just after this is called. The full literal
  // reason will be computed lazily when it becomes needed.
  void EnqueueLiteral(Literal literal, std::vector<Literal>** literal_reason,
                      std::vector<IntegerLiteral>** integer_reason, Trail* trail);
  void EnqueueLiteral(Literal literal, const std::vector<Literal>& literal_reason,
                      const std::vector<IntegerLiteral>& integer_reason,
                      Trail* trail);

  // Tests if the domain of the given variable is empty or not. It empty,
  // returns true and fills trail->MutableConflict() with an explanation of why
  // it it the case.
  bool DomainIsEmpty(IntegerVariable i, Trail* trail) const;

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
  void RegisterWatcher(SparseBitset<LbVar>* p) {
    p->ClearAndResize(LbVar(NumLbVars()));
    watchers_.push_back(p);
  }

 private:
  // Returns a lower bound on the given var that will always be valid.
  int LevelZeroBound(int var) const {
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
  BeginEndWrapper<std::vector<int>::const_iterator> Dependencies(
      int trail_index) const;

  // Helper function to append the Literal part of the reason for this bound
  // assignment.
  void AppendLiteralsReason(int trail_index, std::vector<Literal>* output) const;

  // Information for each internal variable about its current bound.
  struct VarInfo {
    // The current bound on this variable.
    int current_bound;

    // Trail index of the last TrailEntry in the trail refering to this var.
    int current_trail_index;
  };
  std::vector<VarInfo> vars_;

  // The integer trail. It always start by num_vars sentinel values with the
  // level 0 bounds (in one to one correspondance with vars_).
  struct TrailEntry {
    int bound;
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
  std::vector<Literal> literals_reason_buffer_;
  std::vector<int> dependencies_buffer_;

  // Temporary data used by MergeReasonInto().
  mutable std::vector<int> tmp_queue_;
  mutable std::vector<int> tmp_trail_indices_;
  mutable ITIVector<LbVar, int> tmp_var_to_highest_explained_trail_index_;

  // Lazy reason repository.
  std::vector<std::vector<Literal>> literal_reasons_;
  std::vector<std::vector<IntegerLiteral>> integer_reasons_;

  int64 num_enqueues_;

  std::vector<SparseBitset<LbVar>*> watchers_;

  DISALLOW_COPY_AND_ASSIGN(IntegerTrail);
};

inline std::function<BooleanVariable(Model*)> NewBooleanVariable() {
  return [=](Model* model) {
    return model->GetOrCreate<SatSolver>()->NewBooleanVariable();
  };
}

inline std::function<IntegerVariable(Model*)> NewIntegerVariable(int lb,
                                                                 int ub) {
  return [=](Model* model) {
    return model->GetOrCreate<IntegerTrail>()->AddIntegerVariable(lb, ub);
  };
}

inline std::function<void(Model*)> GreaterOrEqual(IntegerVariable v, int lb) {
  return [=](Model* model) {
    model->GetOrCreate<IntegerTrail>()->Enqueue(
        IntegerLiteral::GreaterOrEqual(v, lb), std::vector<Literal>(),
        std::vector<IntegerLiteral>());
  };
}
inline std::function<void(Model*)> LowerOrEqual(IntegerVariable v, int ub) {
  return [=](Model* model) {
    model->GetOrCreate<IntegerTrail>()->Enqueue(
        IntegerLiteral::LowerOrEqual(v, ub), std::vector<Literal>(),
        std::vector<IntegerLiteral>());
  };
}

inline std::function<int(const Model&)> LowerBound(IntegerVariable v) {
  return [=](const Model& model) {
    return model.Get<IntegerTrail>()->LowerBound(v);
  };
}

inline std::function<int(const Model&)> UpperBound(IntegerVariable v) {
  return [=](const Model& model) {
    return model.Get<IntegerTrail>()->UpperBound(v);
  };
}

// This class allows registering Propagator that will be called if a
// watched Literal or LbVar changes.
class GenericLiteralWatcher : public Propagator {
 public:
  explicit GenericLiteralWatcher(IntegerTrail* trail);
  ~GenericLiteralWatcher() final {}

  static GenericLiteralWatcher* CreateInModel(Model* model) {
    GenericLiteralWatcher* watcher =
        new GenericLiteralWatcher(model->GetOrCreate<IntegerTrail>());

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

  // Watches a literal (or LbVar). The propagator with given id will be called
  // if it changes.
  void WatchIntegerVariable(IntegerVariable i, int id);
  void WatchLiteral(Literal l, int id);
  void WatchLbVar(LbVar var, int id);

 private:
  // Updates queue_ and in_queue_ with the propagator ids that need to be
  // called.
  void UpdateCallingNeeds();

  IntegerTrail* integer_trail_;

  ITIVector<LiteralIndex, std::vector<int>> literal_to_watcher_ids_;
  ITIVector<LbVar, std::vector<int>> lb_var_to_watcher_ids_;
  std::vector<PropagatorInterface*> watchers_;
  SparseBitset<LbVar> modified_vars_;

  // Propagator ids that needs to be called.
  std::deque<int> queue_;
  std::vector<bool> in_queue_;

  DISALLOW_COPY_AND_ASSIGN(GenericLiteralWatcher);
};

// ============================================================================
// Implementation.
// ============================================================================

inline IntegerLiteral IntegerLiteral::GreaterOrEqual(IntegerVariable i,
                                                     int bound) {
  return IntegerLiteral(LbVarOf(i), bound);
}

inline IntegerLiteral IntegerLiteral::LowerOrEqual(IntegerVariable i,
                                                   int bound) {
  return IntegerLiteral(MinusUbVarOf(i), -bound);
}

inline IntegerLiteral IntegerLiteral::FromLbVar(LbVar var, int bound) {
  return IntegerLiteral(var, bound);
}

inline int IntegerTrail::LowerBound(IntegerVariable i) const {
  return vars_[LbVarOf(i).value()].current_bound;
}

inline int IntegerTrail::UpperBound(IntegerVariable i) const {
  return -vars_[MinusUbVarOf(i).value()].current_bound;
}

inline IntegerLiteral IntegerTrail::LowerBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::GreaterOrEqual(i, LowerBound(i));
}

inline IntegerLiteral IntegerTrail::UpperBoundAsLiteral(
    IntegerVariable i) const {
  return IntegerLiteral::LowerOrEqual(i, UpperBound(i));
}

inline int IntegerTrail::Value(LbVar var) const {
  return vars_[var.value()].current_bound;
}

inline IntegerLiteral IntegerTrail::ValueAsLiteral(LbVar var) const {
  return IntegerLiteral::FromLbVar(var, Value(var));
}

inline void GenericLiteralWatcher::WatchIntegerVariable(IntegerVariable i,
                                                        int id) {
  WatchLbVar(LbVarOf(i), id);
  WatchLbVar(MinusUbVarOf(i), id);
}

inline void GenericLiteralWatcher::WatchLiteral(Literal l, int id) {
  if (l.Index() >= literal_to_watcher_ids_.size()) {
    literal_to_watcher_ids_.resize(l.Index().value() + 1);
  }
  literal_to_watcher_ids_[l.Index()].push_back(id);
}

inline void GenericLiteralWatcher::WatchLbVar(LbVar var, int id) {
  if (var.value() >= lb_var_to_watcher_ids_.size()) {
    lb_var_to_watcher_ids_.resize(var.value() + 1);
  }
  lb_var_to_watcher_ids_[var].push_back(id);
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_H_
