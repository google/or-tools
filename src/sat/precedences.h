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

#ifndef OR_TOOLS_SAT_PRECEDENCES_H_
#define OR_TOOLS_SAT_PRECEDENCES_H_

#include <algorithm>
#include <queue>

#include "sat/integer.h"
#include "sat/model.h"
#include "sat/sat_base.h"
#include "sat/sat_solver.h"
#include "util/bitset.h"

namespace operations_research {
namespace sat {

// This class implement a propagator on simple inequalities between integer
// variables of the form (i1 + offset <= i2). The offset can be constant or
// given by the value of a third integer variable. Offsets can also be negative.
//
// The algorithm work by mapping the problem onto a graph where the edges carry
// the offset and the nodes correspond to one of the two bounds of an integer
// variable (lower_bound or -upper_bound). It then find the fixed point using an
// incremental variant of the Bellman-Ford(-Tarjan) algorithm.
//
// This is also known as an "integer difference logic theory" in the SMT world.
// Another word is "separation logic".
class PrecedencesPropagator : public SatPropagator, PropagatorInterface {
 public:
  PrecedencesPropagator(Trail* trail, IntegerTrail* integer_trail,
                        GenericLiteralWatcher* watcher)
      : SatPropagator("PrecedencesPropagator"),
        trail_(trail),
        integer_trail_(integer_trail),
        watcher_(watcher),
        watcher_id_(watcher->Register(this)) {
    integer_trail_->RegisterWatcher(&modified_vars_);
    watcher->SetPropagatorPriority(watcher_id_, 0);
  }

  static PrecedencesPropagator* CreateInModel(Model* model) {
    PrecedencesPropagator* precedences = new PrecedencesPropagator(
        model->GetOrCreate<Trail>(), model->GetOrCreate<IntegerTrail>(),
        model->GetOrCreate<GenericLiteralWatcher>());

    // TODO(user): Find a way to have more control on the order in which
    // the propagators are added.
    model->GetOrCreate<SatSolver>()->AddPropagator(
        std::unique_ptr<PrecedencesPropagator>(precedences));
    return precedences;
  }

  bool Propagate() final;
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;

  // Add a precedence relation (i1 + offset <= i2) between integer variables.
  void AddPrecedence(IntegerVariable i1, IntegerVariable i2);
  void AddPrecedenceWithOffset(IntegerVariable i1, IntegerVariable i2,
                               IntegerValue offset);

  // Same as above, but the relation is only true when the given literal is.
  void AddConditionalPrecedence(IntegerVariable i1, IntegerVariable i2,
                                Literal l);
  void AddConditionalPrecedenceWithOffset(IntegerVariable i1,
                                          IntegerVariable i2,
                                          IntegerValue offset, Literal l);

  // Note that we currently do not support marking a variable appearing as
  // an offset_var as optional (with MarkIntegerVariableAsOptional()). We could
  // give it a meaning (like the arcs are not propagated if it is optional), but
  // the code currently do not implement this.
  //
  // TODO(user): support optional offset_var?
  //
  // TODO(user): the variable offset should probably be tested more because
  // when I wrote this, I just had a couple of problems to test this on.
  void AddPrecedenceWithVariableOffset(IntegerVariable i1, IntegerVariable i2,
                                       IntegerVariable offset_var);

  // Generic function that cover all of the above case and more.
  void AddPrecedenceWithAllOptions(IntegerVariable i1, IntegerVariable i2,
                                   IntegerValue offset,
                                   IntegerVariable offset_var, LiteralIndex l);

  // An optional integer variable has a special behavior:
  // - If the bounds on i cross each other, then is_present must be false.
  // - It will only propagate any outgoing arcs if is_present is true.
  //
  // TODO(user): Accept a BinaryImplicationGraph* here, so that and arc
  // (tail -> head) can still propagate if tail.is_present => head.is_present.
  // Note that such propagation is only useful if the status of tail presence
  // is still undecided. Note that we do propagate if tail and head have the
  // same presence literal (see ArcShouldPropagate()).
  //
  // TODO(user): use instead integer_trail_->VariableIsOptional()? Note that the
  // meaning is not exactly the same, because here we also do not propagate the
  // outgoing arcs. And we need to watch the is_present variable, so we still
  // need to call this function.
  void MarkIntegerVariableAsOptional(IntegerVariable i, Literal is_present);

  // Finds all the IntegerVariable that are "after" one of the IntegerVariable
  // in vars. Returns a vector of these precedences relation sorted by
  // IntegerPrecedences.var so that it is efficient to find all the
  // IntegerVariable "before" another one.
  //
  // Note that we only consider direct precedences here. Given our usage, it may
  // be better to compute the full reachability in the precedence graph, but in
  // pratice that may be too slow. On a good note, because we have all the
  // potential precedences between tasks in disjunctions, on a single machine,
  // both notion should be the same since we automatically work on the
  // transitive closure.
  //
  // Note that the IntegerVariable in the vector are also returned in
  // topological order for a more efficient propagation in
  // DisjunctiveConstraint::PrecedencesPass() where this is used.
  struct IntegerPrecedences {
    int index;            // in vars.
    IntegerVariable var;  // An IntegerVariable that is >= to vars[index].
    LiteralIndex reason;  // The reaon for it to be >= or kNoLiteralIndex.

    // Only needed for testing.
    bool operator==(const IntegerPrecedences& o) const {
      return index == o.index && var == o.var && reason == o.reason;
    }
  };
  void ComputePrecedences(const std::vector<IntegerVariable>& vars,
                          const std::vector<bool>& to_consider,
                          std::vector<IntegerPrecedences>* output);

 private:
  // Information about an individual arc.
  struct ArcInfo {
    IntegerVariable tail_var;
    IntegerVariable head_var;

    IntegerValue offset;
    IntegerVariable offset_var;  // kNoIntegerVariable if none.
    LiteralIndex presence_l;     // kNoLiteralIndex if none.

    // Used temporarily by our implementation of the Bellman-Ford algorithm. It
    // should be false at the beginning of BellmanFordTarjan().
    mutable bool is_marked;
  };

  // Internal functions to add new precedence relations.
  //
  // Note that internally, we only propagate lower bounds, so each time we add
  // an arc, we actually create two of them: one on the given variables, and one
  // on their negation.
  void AdjustSizeFor(IntegerVariable i);
  void AddArc(IntegerVariable tail, IntegerVariable head, IntegerValue offset,
              IntegerVariable offset_var, LiteralIndex l);

  // Helper function for a slightly more readable code.
  LiteralIndex OptionalLiteralOf(IntegerVariable var) const {
    return optional_literals_[var];
  }

  // Enqueue a new lower bound for the variable arc.head_lb that was deduced
  // from the current value of arc.tail_lb and the offset of this arc.
  bool EnqueueAndCheck(const ArcInfo& arc, IntegerValue new_head_lb,
                       Trail* trail);
  IntegerValue ArcOffset(const ArcInfo& arc) const;

  // Returns true iff this arc should propagate. For now, this is true when:
  // - tail node is non-optional
  // - tail node is optional and present.
  // - tail node is optional, its presence is unknown, and its presence literal
  //   is the same as the one of head.
  bool ArcShouldPropagate(const ArcInfo& arc, const Trail& trail) const;

  // Inspect all the optional arcs that needs inspection (to stay sparse) and
  // check if their presence literal can be propagated to false.
  void PropagateOptionalArcs(Trail* trail);

  // The core algorithm implementation is split in these functions. One must
  // first call InitializeBFQueueWithModifiedNodes() that will push all the
  // IntegerVariable whose lower bound has been modified since the last call.
  // Then, BellmanFordTarjan() will take care of all the propagation and returns
  // false in case of conflict. Internally, it uses DisassembleSubtree() which
  // is the Tarjan variant to detect a possible positive cycle. Before exiting,
  // it will call CleanUpMarkedArcsAndParents().
  //
  // The Tarjan version of the Bellam-Ford algorithm is really nice in our
  // context because it was really easy to make it incremental. Moreover, it
  // supports batch increment!
  //
  // This implementation is kind of unique because of our context and the fact
  // that it is incremental, but a good reference is "Negative-cycle detection
  // algorithms", Boris V. Cherkassky, Andrew V. Goldberg, 1996,
  // http://people.cs.nctu.edu.tw/~tjshen/doc/ne.pdf
  void InitializeBFQueueWithModifiedNodes();
  bool BellmanFordTarjan(Trail* trail);
  bool DisassembleSubtree(int source, int target,
                          std::vector<bool>* can_be_skipped);
  void ReportPositiveCycle(int first_arc, Trail* trail);
  void CleanUpMarkedArcsAndParents();

  // Loops over all the arcs and verify that there is no propagation left.
  // This is only meant to be used in a DCHECK() and is not optimized.
  bool NoPropagationLeft(const Trail& trail) const;

  // External class needed to get the IntegerVariable lower bounds and Enqueue
  // new ones.
  Trail* trail_;
  IntegerTrail* integer_trail_;
  GenericLiteralWatcher* watcher_;
  int watcher_id_;

  // The key to our incrementality. This will be cleared once the propagation
  // is done, and automatically updated by the integer_trail_ with all the
  // IntegerVariable that changed since the last clear.
  SparseBitset<IntegerVariable> modified_vars_;

  // An arc needs to be inspected for propagation (i.e. is impacted) if:
  // - Its tail_var changed.
  // - Its offset_var changed.
  //
  // All the int are arc indices in the arcs_ vector.
  //
  // The first vector (impacted_arcs_) correspond to the arc currently present
  // whereas the second vector (impacted_potential_arcs_) list all the potential
  // arcs (the one not allways present) and is just used for propagation of the
  // arc presence literals.
  ITIVector<IntegerVariable, std::vector<int>> impacted_arcs_;
  ITIVector<IntegerVariable, std::vector<int>> impacted_potential_arcs_;

  // Temporary vectors used by ComputePrecedences().
  ITIVector<IntegerVariable, int> var_to_degree_;
  ITIVector<IntegerVariable, int> var_to_last_index_;
  struct SortedVar {
    IntegerVariable var;
    IntegerValue lower_bound;
    bool operator<(const SortedVar& other) const {
      return lower_bound < other.lower_bound;
    }
  };
  std::vector<SortedVar> tmp_sorted_vars_;
  std::vector<IntegerPrecedences> tmp_precedences_;

  // The set of arcs that must be added to impacted_arcs_ when a literal become
  // true.
  ITIVector<LiteralIndex, std::vector<int>> potential_arcs_;

  // Used for MarkIntegerVariableAsOptional(). The nodes associated to an
  // IntegerVariable whose entry is not kNoLiteralIndex will only propagate
  // something to its neighbors if the coresponding literal is assigned to true.
  ITIVector<IntegerVariable, LiteralIndex> optional_literals_;
  ITIVector<LiteralIndex, std::vector<int>> potential_nodes_;

  // TODO(user): rearranging the index so that the arc of the same node are
  // consecutive like in StaticGraph should have a big performance impact.
  std::vector<ArcInfo> arcs_;

  // Temp vectors to hold the reason of an assignment.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  // Temp vectors for the Bellman-Ford algorithm. The graph in which this
  // algorithm works is in one to one correspondance with the IntegerVariable in
  // impacted_arcs_.
  std::deque<int> bf_queue_;
  std::vector<bool> bf_in_queue_;
  std::vector<bool> bf_can_be_skipped_;
  std::vector<int> bf_parent_arc_of_;

  // Temp vector used by the tree traversal in DisassembleSubtree().
  std::vector<int> tmp_vector_;

  DISALLOW_COPY_AND_ASSIGN(PrecedencesPropagator);
};

// =============================================================================
// Implementation of the small API functions below.
// =============================================================================

inline void PrecedencesPropagator::AddPrecedence(IntegerVariable i1,
                                                 IntegerVariable i2) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         /*l=*/kNoLiteralIndex);
}

inline void PrecedencesPropagator::AddPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable,
         /*l=*/kNoLiteralIndex);
}

inline void PrecedencesPropagator::AddConditionalPrecedence(IntegerVariable i1,
                                                            IntegerVariable i2,
                                                            Literal l) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         l.Index());
}

inline void PrecedencesPropagator::AddConditionalPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset, Literal l) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable, l.Index());
}

inline void PrecedencesPropagator::AddPrecedenceWithVariableOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerVariable offset_var) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), offset_var, /*l=*/kNoLiteralIndex);
}

inline void PrecedencesPropagator::AddPrecedenceWithAllOptions(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset,
    IntegerVariable offset_var, LiteralIndex r) {
  AddArc(i1, i2, offset, offset_var, r);
}

// =============================================================================
// Model based functions.
// =============================================================================

// a <= b.
inline std::function<void(Model*)> LowerOrEqual(IntegerVariable a,
                                                IntegerVariable b) {
  return [=](Model* model) {
    return model->GetOrCreate<PrecedencesPropagator>()->AddPrecedence(a, b);
  };
}

// a + offset <= b.
inline std::function<void(Model*)> LowerOrEqualWithOffset(IntegerVariable a,
                                                          IntegerVariable b,
                                                          int64 offset) {
  return [=](Model* model) {
    return model->GetOrCreate<PrecedencesPropagator>()->AddPrecedenceWithOffset(
        a, b, IntegerValue(offset));
  };
}

// a + b <= ub.
inline std::function<void(Model*)> Sum2LowerOrEqual(IntegerVariable a,
                                                    IntegerVariable b,
                                                    int64 ub) {
  return LowerOrEqualWithOffset(a, NegationOf(b), -ub);
}

// l => (a + b <= ub).
inline std::function<void(Model*)> ConditionalSum2LowerOrEqual(
    IntegerVariable a, IntegerVariable b, int64 ub, Literal l) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(b), IntegerValue(-ub),
                                   kNoIntegerVariable, l.Index());
  };
}

// a + b + c <= ub.
inline std::function<void(Model*)> Sum3LowerOrEqual(IntegerVariable a,
                                                    IntegerVariable b,
                                                    IntegerVariable c,
                                                    int64 ub) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(c), IntegerValue(-ub), b,
                                   kNoLiteralIndex);
  };
}

// l => (a + b + c <= ub).
inline std::function<void(Model*)> ConditionalSum3LowerOrEqual(
    IntegerVariable a, IntegerVariable b, IntegerVariable c, int64 ub,
    Literal l) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(c), IntegerValue(-ub), b,
                                   l.Index());
  };
}

// a >= b.
inline std::function<void(Model*)> GreaterOrEqual(IntegerVariable a,
                                                  IntegerVariable b) {
  return [=](Model* model) {
    return model->GetOrCreate<PrecedencesPropagator>()->AddPrecedence(b, a);
  };
}

// a == b.
inline std::function<void(Model*)> Equality(IntegerVariable a,
                                            IntegerVariable b) {
  return [=](Model* model) {
    model->Add(LowerOrEqual(a, b));
    model->Add(LowerOrEqual(b, a));
  };
}

// a + offset == b.
inline std::function<void(Model*)> EqualityWithOffset(IntegerVariable a,
                                                      IntegerVariable b,
                                                      int64 offset) {
  return [=](Model* model) {
    model->Add(LowerOrEqualWithOffset(a, b, offset));
    model->Add(LowerOrEqualWithOffset(b, a, -offset));
  };
}

// is_le => (a + offset <= b).
inline std::function<void(Model*)> ConditionalLowerOrEqualWithOffset(
    IntegerVariable a, IntegerVariable b, int64 offset, Literal is_le) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddConditionalPrecedenceWithOffset(a, b, IntegerValue(offset), is_le);
  };
}

// is_le <=> (a + offset <= b).
inline std::function<void(Model*)> ReifiedLowerOrEqualWithOffset(
    IntegerVariable a, IntegerVariable b, int64 offset, Literal is_le) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddConditionalPrecedenceWithOffset(a, b, IntegerValue(offset), is_le);

    // The negation of (a + offset <= b) is (a + offset > b) which can be
    // rewritten as (b + 1 - offset <= a).
    p->AddConditionalPrecedenceWithOffset(b, a, IntegerValue(1 - offset),
                                          is_le.Negated());
  };
}

// is_eq <=> (a == b).
inline std::function<void(Model*)> ReifiedEquality(IntegerVariable a,
                                                   IntegerVariable b,
                                                   Literal is_eq) {
  return [=](Model* model) {
    // We creates two extra Boolean variables in this case.
    //
    // TODO(user): Avoid creating them if we already have some literal that
    // have the same meaning. For instance if a client also wanted to know if
    // a <= b, he would have called ReifiedLowerOrEqualWithOffset() directly.
    const Literal is_le = Literal(model->Add(NewBooleanVariable()), true);
    const Literal is_ge = Literal(model->Add(NewBooleanVariable()), true);
    model->Add(ReifiedBoolAnd({is_le, is_ge}, is_eq));
    model->Add(ReifiedLowerOrEqualWithOffset(a, b, 0, is_le));
    model->Add(ReifiedLowerOrEqualWithOffset(b, a, 0, is_ge));
  };
}

// a != b.
inline std::function<void(Model*)> NotEqual(IntegerVariable a,
                                            IntegerVariable b) {
  return [=](Model* model) {
    // We have two options (is_gt or is_lt) and one must be true.
    const Literal is_lt = Literal(model->Add(NewBooleanVariable()), true);
    const Literal is_gt = is_lt.Negated();
    model->Add(ConditionalLowerOrEqualWithOffset(a, b, 1, is_lt));
    model->Add(ConditionalLowerOrEqualWithOffset(b, a, 1, is_gt));
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRECEDENCES_H_
