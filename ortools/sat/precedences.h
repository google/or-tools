// Copyright 2010-2022 Google LLC
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
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/graph/graph.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

struct FullIntegerPrecedence {
  IntegerVariable var;
  std::vector<int> indices;
  std::vector<IntegerValue> offsets;
};

// Stores all the precedences relation of the form "tail_X + offset <= head_X"
// that we could extract from the linear constraint of the model. These are
// stored in a directed graph.
//
// TODO(user): Support conditional relation.
// TODO(user): Support non-DAG like graph.
// TODO(user): Support variable offset that can be updated as search progress.
class PrecedenceRelations {
 public:
  explicit PrecedenceRelations(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

  void Resize(int num_variables) {
    graph_.ReserveNodes(num_variables);
    graph_.AddNode(num_variables - 1);
  }

  // Add a relation tail + offset <= head.
  void Add(IntegerVariable tail, IntegerVariable head, IntegerValue offset);

  // Returns a set of relations var >= max_i(vars[index[i]] + offsets[i]).
  //
  // This currently only works if the precedence relation form a DAG.
  // If not we will just abort. TODO(user): generalize.
  //
  // TODO(user): Put some work limit in place, as this can be slow. Complexity
  // is in O(vars.size()) * num_arcs.
  //
  // TODO(user): Since we don't need ALL precedences, we could just work on a
  // sub-DAG of the full precedence graph instead of aborting. Or we can just
  // support the general non-DAG cases.
  //
  // TODO(user): Many relations can be redundant. Filter them.
  void ComputeFullPrecedences(const std::vector<IntegerVariable>& vars,
                              std::vector<FullIntegerPrecedence>* output);

  // If we don't have too many variable, we compute the full transtive closure
  // and can query in O(1) if there is a relation between two variables.
  // This can be used to optimize some scheduling propagation and reasons.
  //
  // Warning: If we there are too many, this will NOT contain all relations.
  //
  // Returns kMinIntegerValue if there are none.
  // Otherwise a + offset <= b.
  IntegerValue GetOffset(IntegerVariable a, IntegerVariable b) {
    const auto it = all_relations_.find({a, b});
    return it == all_relations_.end() ? kMinIntegerValue : it->second;
  }

  // Update the hash table of precedence relation.
  void UpdateOffset(IntegerVariable a, IntegerVariable b, IntegerValue offset) {
    InternalUpdate(a, b, offset);
    InternalUpdate(NegationOf(b), NegationOf(a), -offset);
  }

  // The current code requires the internal data to be processed once all
  // relations are loaded.
  //
  // TODO(user): Be more dynamic as we start to add relations during search.
  void Build();

 private:
  void InternalUpdate(IntegerVariable a, IntegerVariable b,
                      IntegerValue offset) {
    const auto [it, inserted] = all_relations_.insert({{a, b}, offset});
    if (!inserted) {
      it->second = std::max(it->second, offset);
    }
  }

  IntegerTrail* integer_trail_;

  util::StaticGraph<> graph_;
  std::vector<IntegerValue> arc_offset_;

  bool is_built_ = false;
  bool is_dag_ = false;
  std::vector<IntegerVariable> topological_order_;

  absl::flat_hash_map<std::pair<IntegerVariable, IntegerVariable>, IntegerValue>
      all_relations_;
};

// This class implement a propagator on simple inequalities between integer
// variables of the form (i1 + offset <= i2). The offset can be constant or
// given by the value of a third integer variable. Offsets can also be negative.
//
// The algorithm works by mapping the problem onto a graph where the edges carry
// the offset and the nodes correspond to one of the two bounds of an integer
// variable (lower_bound or -upper_bound). It then find the fixed point using an
// incremental variant of the Bellman-Ford(-Tarjan) algorithm.
//
// This is also known as an "integer difference logic theory" in the SMT world.
// Another word is "separation logic".
//
// TODO(user): We could easily generalize the code to support any relation of
// the form a*X + b*Y + c*Z >= rhs (or <=). Do that since this class should be
// a lot faster at propagating small linear inequality than the generic
// propagator and the overhead of supporting coefficient should not be too bad.
class PrecedencesPropagator : public SatPropagator, PropagatorInterface {
 public:
  explicit PrecedencesPropagator(Model* model)
      : SatPropagator("PrecedencesPropagator"),
        trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->Mutable<SharedStatistics>()),
        watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
        watcher_id_(watcher_->Register(this)) {
    model->GetOrCreate<SatSolver>()->AddPropagator(this);
    integer_trail_->RegisterWatcher(&modified_vars_);
    watcher_->SetPropagatorPriority(watcher_id_, 0);
  }

  // This type is neither copyable nor movable.
  PrecedencesPropagator(const PrecedencesPropagator&) = delete;
  PrecedencesPropagator& operator=(const PrecedencesPropagator&) = delete;
  ~PrecedencesPropagator() override;

  bool Propagate() final;
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;

  // Propagates all the outgoing arcs of the given variable (and only those). It
  // is more efficient to do all these propagation in one go by calling
  // Propagate(), but for scheduling problem, we wants to propagate right away
  // the end of an interval when its start moved.
  bool PropagateOutgoingArcs(IntegerVariable var);

  // Add a precedence relation (i1 + offset <= i2) between integer variables.
  //
  // Important: The optionality of the variable should be marked BEFORE this
  // is called.
  void AddPrecedence(IntegerVariable i1, IntegerVariable i2);
  void AddPrecedenceWithOffset(IntegerVariable i1, IntegerVariable i2,
                               IntegerValue offset);
  void AddPrecedenceWithVariableOffset(IntegerVariable i1, IntegerVariable i2,
                                       IntegerVariable offset_var);

  // Same as above, but the relation is only true when the given literal is.
  void AddConditionalPrecedence(IntegerVariable i1, IntegerVariable i2,
                                Literal l);
  void AddConditionalPrecedenceWithOffset(IntegerVariable i1,
                                          IntegerVariable i2,
                                          IntegerValue offset, Literal l);

  // Generic function that cover all of the above case and more.
  void AddPrecedenceWithAllOptions(IntegerVariable i1, IntegerVariable i2,
                                   IntegerValue offset,
                                   IntegerVariable offset_var,
                                   absl::Span<const Literal> presence_literals);

  // This version check current precedence. It is however "slow".
  bool AddPrecedenceWithOffsetIfNew(IntegerVariable i1, IntegerVariable i2,
                                    IntegerValue offset);

  // Finds all the IntegerVariable that are "after" at least two of the
  // IntegerVariable in vars. Returns a vector of these precedences relation
  // sorted by IntegerPrecedences.var so that it is efficient to find all the
  // IntegerVariable "before" another one.
  //
  // Note that we only consider direct precedences here. Given our usage, it may
  // be better to compute the full reachability in the precedence graph, but in
  // pratice that may be too slow.
  //
  // Note that the IntegerVariable in the vector are also returned in
  // topological order for a more efficient propagation in
  // DisjunctivePrecedences::Propagate() where this is used.
  //
  // Important: For identical vars, the entry are sorted by index.
  struct IntegerPrecedences {
    int index;            // position in vars.
    IntegerVariable var;  // An IntegerVariable that is >= to vars[index].
    int arc_index;        // Used by AddPrecedenceReason().
    IntegerValue offset;  // we have: vars[index] + offset <= var
  };
  void ComputePrecedences(const std::vector<IntegerVariable>& vars,
                          std::vector<IntegerPrecedences>* output);
  void AddPrecedenceReason(int arc_index, IntegerValue min_offset,
                           std::vector<Literal>* literal_reason,
                           std::vector<IntegerLiteral>* integer_reason) const;

  // This just wrap ComputePrecedences() above and convert its output format to
  // the same format as PrecedenceRelations::ComputeFullPrecedences(). This is
  // less efficient but more convenient to use.
  //
  //
  // Returns a bunch of precedences relations:
  // An IntegerVariable >= to vars[indices[i]] + offset[i], for i in indices.
  void ComputePartialPrecedences(const std::vector<IntegerVariable>& vars,
                                 std::vector<FullIntegerPrecedence>* output);

  // Advanced usage. To be called once all the constraints have been added to
  // the model. This will loop over all "node" in this class, and if one of its
  // optional incoming arcs must be chosen, it will add a corresponding
  // GreaterThanAtLeastOneOfConstraint(). Returns the number of added
  // constraint.
  //
  // TODO(user): This can be quite slow, add some kind of deterministic limit
  // so that we can use it all the time.
  int AddGreaterThanAtLeastOneOfConstraints(Model* model);

  // If known, return an offset such that we have a + offset <= b.
  // Note that this only cover the case where this was conditionned by a single
  // literal.
  //
  // TODO(user): Support list of literals, it isn't that much harder.
  std::pair<Literal, IntegerValue> GetConditionalOffset(IntegerVariable a,
                                                        IntegerVariable b) {
    const auto it = conditional_relations_.find({a, b});
    if (it == conditional_relations_.end()) {
      return {Literal(), kMinIntegerValue};
    }
    return it->second;
  }

 private:
  DEFINE_STRONG_INDEX_TYPE(ArcIndex);
  DEFINE_STRONG_INDEX_TYPE(OptionalArcIndex);

  // Given an existing clause, sees if it can be used to add "greater than at
  // least one of" type of constraints. Returns the number of such constraint
  // added.
  int AddGreaterThanAtLeastOneOfConstraintsFromClause(
      absl::Span<const Literal> clause, Model* model);

  // Another approach for AddGreaterThanAtLeastOneOfConstraints(), this one
  // might be a bit slow as it relies on the propagation engine to detect
  // clauses between incoming arcs presence literals.
  // Returns the number of added constraints.
  int AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(
      Model* model);

  // Information about an individual arc.
  struct ArcInfo {
    IntegerVariable tail_var;
    IntegerVariable head_var;

    IntegerValue offset;
    IntegerVariable offset_var;  // kNoIntegerVariable if none.

    // This arc is "present" iff all these literals are true.
    absl::InlinedVector<Literal, 6> presence_literals;

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
              IntegerVariable offset_var,
              absl::Span<const Literal> presence_literals);

  // Enqueue a new lower bound for the variable arc.head_lb that was deduced
  // from the current value of arc.tail_lb and the offset of this arc.
  bool EnqueueAndCheck(const ArcInfo& arc, IntegerValue new_head_lb,
                       Trail* trail);
  IntegerValue ArcOffset(const ArcInfo& arc) const;

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
  void AnalyzePositiveCycle(ArcIndex first_arc, Trail* trail,
                            std::vector<Literal>* must_be_all_true,
                            std::vector<Literal>* literal_reason,
                            std::vector<IntegerLiteral>* integer_reason);
  void CleanUpMarkedArcsAndParents();

  // Loops over all the arcs and verify that there is no propagation left.
  // This is only meant to be used in a DCHECK() and is not optimized.
  bool NoPropagationLeft(const Trail& trail) const;

  // Update conditional_relations_.
  void AddToConditionalRelations(const ArcInfo& arc);
  void RemoveFromConditionalRelations(const ArcInfo& arc);

  // External class needed to get the IntegerVariable lower bounds and Enqueue
  // new ones.
  Trail* trail_;
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_ = nullptr;
  GenericLiteralWatcher* watcher_;
  int watcher_id_;

  // The key to our incrementality. This will be cleared once the propagation
  // is done, and automatically updated by the integer_trail_ with all the
  // IntegerVariable that changed since the last clear.
  SparseBitset<IntegerVariable> modified_vars_;

  // An arc needs to be inspected for propagation (i.e. is impacted) if its
  // tail_var changed. If an arc has 3 variables (tail, offset, head), it will
  // appear as 6 different entries in the arcs_ vector, one for each variable
  // and its negation, each time with a different tail.
  //
  // TODO(user): rearranging the index so that the arc of the same node are
  // consecutive like in StaticGraph should have a big performance impact.
  //
  // TODO(user): We do not need to store ArcInfo.tail_var here.
  absl::StrongVector<IntegerVariable, absl::InlinedVector<ArcIndex, 6>>
      impacted_arcs_;
  absl::StrongVector<ArcIndex, ArcInfo> arcs_;

  // This is similar to impacted_arcs_/arcs_ but it is only used to propagate
  // one of the presence literals when the arc cannot be present. An arc needs
  // to appear only once in potential_arcs_, but it will be referenced by
  // all its variable in impacted_potential_arcs_.
  absl::StrongVector<IntegerVariable, absl::InlinedVector<OptionalArcIndex, 6>>
      impacted_potential_arcs_;
  absl::StrongVector<OptionalArcIndex, ArcInfo> potential_arcs_;

  // Temporary vectors used by ComputePrecedences().
  absl::StrongVector<IntegerVariable, int> var_to_degree_;
  absl::StrongVector<IntegerVariable, int> var_to_last_index_;
  struct SortedVar {
    IntegerVariable var;
    IntegerValue lower_bound;
    bool operator<(const SortedVar& other) const {
      return lower_bound < other.lower_bound;
    }
  };
  std::vector<SortedVar> tmp_sorted_vars_;
  std::vector<IntegerPrecedences> tmp_precedences_;

  // Each time a literal becomes true, this list the set of arcs for which we
  // need to decrement their count. When an arc count reach zero, it must be
  // added to the set of impacted_arcs_. Note that counts never becomes
  // negative.
  //
  // TODO(user): Try a one-watcher approach instead. Note that in most cases
  // arc should be controlled by 1 or 2 literals, so not sure it is worth it.
  absl::StrongVector<LiteralIndex, absl::InlinedVector<ArcIndex, 6>>
      literal_to_new_impacted_arcs_;
  absl::StrongVector<ArcIndex, int> arc_counts_;

  // Temp vectors to hold the reason of an assignment.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  // Temp vectors for the Bellman-Ford algorithm. The graph in which this
  // algorithm works is in one to one correspondence with the IntegerVariable in
  // impacted_arcs_.
  std::deque<int> bf_queue_;
  std::vector<bool> bf_in_queue_;
  std::vector<bool> bf_can_be_skipped_;
  std::vector<ArcIndex> bf_parent_arc_of_;

  // Temp vector used by the tree traversal in DisassembleSubtree().
  std::vector<int> tmp_vector_;

  // When a literal => X + offset <= Y become true, we add it here if X and Y
  // do not already have a conditial relation. We also remove it on untrail.
  // This is especially useful when we create all the literal between pair of
  // interval for a disjunctive constraint.
  absl::flat_hash_map<std::pair<IntegerVariable, IntegerVariable>,
                      std::pair<Literal, IntegerValue>>
      conditional_relations_;

  // Stats.
  int64_t num_cycles_ = 0;
  int64_t num_pushes_ = 0;
  int64_t num_enforcement_pushes_ = 0;
};

// =============================================================================
// Implementation of the small API functions below.
// =============================================================================

inline void PrecedencesPropagator::AddPrecedence(IntegerVariable i1,
                                                 IntegerVariable i2) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         {});
}

inline void PrecedencesPropagator::AddPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable, {});
}

inline void PrecedencesPropagator::AddConditionalPrecedence(IntegerVariable i1,
                                                            IntegerVariable i2,
                                                            Literal l) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         {l});
}

inline void PrecedencesPropagator::AddConditionalPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset, Literal l) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable, {l});
}

inline void PrecedencesPropagator::AddPrecedenceWithVariableOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerVariable offset_var) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), offset_var, {});
}

inline void PrecedencesPropagator::AddPrecedenceWithAllOptions(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset,
    IntegerVariable offset_var, absl::Span<const Literal> presence_literals) {
  AddArc(i1, i2, offset, offset_var, presence_literals);
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
                                                          int64_t offset) {
  return [=](Model* model) {
    model->GetOrCreate<PrecedenceRelations>()->Add(a, b, IntegerValue(offset));
    model->GetOrCreate<PrecedencesPropagator>()->AddPrecedenceWithOffset(
        a, b, IntegerValue(offset));
  };
}

// a + b <= ub.
inline std::function<void(Model*)> Sum2LowerOrEqual(IntegerVariable a,
                                                    IntegerVariable b,
                                                    int64_t ub) {
  return LowerOrEqualWithOffset(a, NegationOf(b), -ub);
}

// l => (a + b <= ub).
inline std::function<void(Model*)> ConditionalSum2LowerOrEqual(
    IntegerVariable a, IntegerVariable b, int64_t ub,
    const std::vector<Literal>& enforcement_literals) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(b), IntegerValue(-ub),
                                   kNoIntegerVariable, enforcement_literals);
  };
}

// a + b + c <= ub.
inline std::function<void(Model*)> Sum3LowerOrEqual(IntegerVariable a,
                                                    IntegerVariable b,
                                                    IntegerVariable c,
                                                    int64_t ub) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(c), IntegerValue(-ub), b, {});
  };
}

// l => (a + b + c <= ub).
inline std::function<void(Model*)> ConditionalSum3LowerOrEqual(
    IntegerVariable a, IntegerVariable b, IntegerVariable c, int64_t ub,
    const std::vector<Literal>& enforcement_literals) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddPrecedenceWithAllOptions(a, NegationOf(c), IntegerValue(-ub), b,
                                   enforcement_literals);
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
                                                      int64_t offset) {
  return [=](Model* model) {
    model->Add(LowerOrEqualWithOffset(a, b, offset));
    model->Add(LowerOrEqualWithOffset(b, a, -offset));
  };
}

// is_le => (a + offset <= b).
inline std::function<void(Model*)> ConditionalLowerOrEqualWithOffset(
    IntegerVariable a, IntegerVariable b, int64_t offset, Literal is_le) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddConditionalPrecedenceWithOffset(a, b, IntegerValue(offset), is_le);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRECEDENCES_H_
