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

#include "ortools/sat/precedences.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

bool PrecedenceRelations::Add(IntegerVariable tail, IntegerVariable head,
                              IntegerValue offset) {
  // Ignore trivial relation: tail + offset <= head.
  if (integer_trail_->LevelZeroUpperBound(tail) + offset <=
      integer_trail_->LevelZeroLowerBound(head)) {
    return false;
  }

  // TODO(user): Return infeasible if tail == head and offset > 0.
  // TODO(user): if tail = Negation(head) also update Domain.
  if (tail == head) return false;

  // Add to root_relations_.
  //
  // TODO(user): AddInternal() only returns true if this is the first relation
  // between head and tail. But we can still avoid an extra lookup.
  if (offset <= GetOffset(tail, head)) return false;
  AddInternal(tail, head, offset);

  // If we are not built, make sure there is enough room in the graph.
  // TODO(user): Alternatively, force caller to do a Resize().
  const int max_node =
      std::max(PositiveVariable(tail), PositiveVariable(head)).value() + 1;
  if (!is_built_ && max_node >= graph_.num_nodes()) {
    graph_.AddNode(max_node);
  }
  return true;
}

void PrecedenceRelations::PushConditionalRelation(
    absl::Span<const Literal> enforcements, IntegerVariable a,
    IntegerVariable b, IntegerValue rhs) {
  // This must be currently true.
  if (DEBUG_MODE) {
    for (const Literal l : enforcements) {
      CHECK(trail_->Assignment().LiteralIsTrue(l));
    }
  }

  if (enforcements.empty() || trail_->CurrentDecisionLevel() == 0) {
    Add(a, NegationOf(b), -rhs);
    return;
  }

  // Ignore if no better than best_relations, otherwise increase it.
  const auto key = GetKey(a, b);
  {
    const auto [it, inserted] = best_relations_.insert({key, rhs});
    if (!inserted) {
      if (rhs >= it->second) return;  // Ignore.
      it->second = rhs;
    }
  }

  const int new_index = conditional_stack_.size();
  const auto [it, inserted] = conditional_relations_.insert({key, new_index});
  if (inserted) {
    CreateLevelEntryIfNeeded();
    conditional_stack_.emplace_back(/*prev_entry=*/-1, rhs, key, enforcements);

    const int new_size = std::max(a.value(), b.value()) + 1;
    if (new_size > conditional_after_.size()) {
      conditional_after_.resize(new_size);
    }
    conditional_after_[a].push_back(NegationOf(b));
    conditional_after_[b].push_back(NegationOf(a));
  } else {
    // We should only decrease because we ignored entry worse than the one in
    // best_relations_.
    const int prev_entry = it->second;
    DCHECK_LT(rhs, conditional_stack_[prev_entry].rhs);

    // Update.
    it->second = new_index;
    CreateLevelEntryIfNeeded();
    conditional_stack_.emplace_back(prev_entry, rhs, key, enforcements);
  }
}

void PrecedenceRelations::CreateLevelEntryIfNeeded() {
  const int current = trail_->CurrentDecisionLevel();
  if (!level_to_stack_size_.empty() &&
      level_to_stack_size_.back().first == current)
    return;
  level_to_stack_size_.push_back({current, conditional_stack_.size()});
}

// We only pop what is needed.
void PrecedenceRelations::SetLevel(int level) {
  while (!level_to_stack_size_.empty() &&
         level_to_stack_size_.back().first > level) {
    const int target = level_to_stack_size_.back().second;
    DCHECK_GE(conditional_stack_.size(), target);
    while (conditional_stack_.size() > target) {
      const ConditionalEntry& back = conditional_stack_.back();
      if (back.prev_entry != -1) {
        conditional_relations_[back.key] = back.prev_entry;
        UpdateBestRelation(back.key, conditional_stack_[back.prev_entry].rhs);
      } else {
        UpdateBestRelation(back.key, kMaxIntegerValue);
        conditional_relations_.erase(back.key);

        DCHECK_EQ(conditional_after_[back.key.first].back(),
                  NegationOf(back.key.second));
        DCHECK_EQ(conditional_after_[back.key.second].back(),
                  NegationOf(back.key.first));
        conditional_after_[back.key.first].pop_back();
        conditional_after_[back.key.second].pop_back();
      }
      conditional_stack_.pop_back();
    }
    level_to_stack_size_.pop_back();
  }
}

IntegerValue PrecedenceRelations::GetOffset(IntegerVariable a,
                                            IntegerVariable b) const {
  const auto it = root_relations_.find(GetKey(a, NegationOf(b)));
  if (it != root_relations_.end()) {
    return -it->second;
  }
  return kMinIntegerValue;
}

absl::Span<const Literal> PrecedenceRelations::GetConditionalEnforcements(
    IntegerVariable a, IntegerVariable b) const {
  const auto it = conditional_relations_.find(GetKey(a, NegationOf(b)));
  if (it == conditional_relations_.end()) return {};

  const ConditionalEntry& entry = conditional_stack_[it->second];
  if (DEBUG_MODE) {
    for (const Literal l : entry.enforcements) {
      CHECK(trail_->Assignment().LiteralIsTrue(l));
    }
  }
  const IntegerValue root_level_offset = GetOffset(a, b);
  const IntegerValue conditional_offset = -entry.rhs;
  if (conditional_offset <= root_level_offset) return {};

  DCHECK_EQ(entry.rhs, -GetConditionalOffset(a, b));
  return entry.enforcements;
}

IntegerValue PrecedenceRelations::GetConditionalOffset(
    IntegerVariable a, IntegerVariable b) const {
  const auto it = best_relations_.find(GetKey(a, NegationOf(b)));
  if (it != best_relations_.end()) {
    return -it->second;
  }
  DCHECK(!root_relations_.contains(GetKey(a, NegationOf(b))));
  DCHECK(!conditional_relations_.contains(GetKey(a, NegationOf(b))));
  return kMinIntegerValue;
}

void PrecedenceRelations::Build() {
  if (is_built_) return;
  is_built_ = true;

  const int num_nodes = graph_.num_nodes();
  util_intops::StrongVector<IntegerVariable, std::vector<IntegerVariable>>
      before(num_nodes);

  // We will construct a graph with the current relation from all_relations_.
  // And use this to compute the "closure".
  CHECK(arc_offsets_.empty());
  graph_.ReserveArcs(2 * root_relations_.size());
  std::vector<
      std::pair<std::pair<IntegerVariable, IntegerVariable>, IntegerValue>>
      root_relations_sorted(root_relations_.begin(), root_relations_.end());
  std::sort(root_relations_sorted.begin(), root_relations_sorted.end());
  for (const auto [var_pair, negated_offset] : root_relations_sorted) {
    // TODO(user): Support negative offset?
    //
    // Note that if we only have >= 0 ones, if we do have a cycle, we could
    // make sure all variales are the same, and otherwise, we have a DAG or a
    // conflict.
    const IntegerValue offset = -negated_offset;
    if (offset < 0) continue;

    // We have two arcs.
    {
      const IntegerVariable tail = var_pair.first;
      const IntegerVariable head = NegationOf(var_pair.second);
      graph_.AddArc(tail.value(), head.value());
      arc_offsets_.push_back(offset);
      CHECK_LT(var_pair.second, before.size());
      before[head].push_back(tail);
    }
    {
      const IntegerVariable tail = var_pair.second;
      const IntegerVariable head = NegationOf(var_pair.first);
      graph_.AddArc(tail.value(), head.value());
      arc_offsets_.push_back(offset);
      CHECK_LT(var_pair.second, before.size());
      before[head].push_back(tail);
    }
  }

  std::vector<int> permutation;
  graph_.Build(&permutation);
  util::Permute(permutation, &arc_offsets_);

  // Is it a DAG?
  // Get a topological order of the DAG formed by all the arcs that are present.
  //
  // TODO(user): This can fail if we don't have a DAG. We could just skip Bad
  // edges instead, and have a sub-DAG as an heuristic. Or analyze the arc
  // weight and make sure cycle are not an issue. We can also start with arcs
  // with strictly positive weight.
  //
  // TODO(user): Only explore the sub-graph reachable from "vars".
  DenseIntStableTopologicalSorter sorter(num_nodes);
  for (int arc = 0; arc < graph_.num_arcs(); ++arc) {
    sorter.AddEdge(graph_.Tail(arc), graph_.Head(arc));
  }
  int next;
  bool graph_has_cycle = false;
  topological_order_.clear();
  while (sorter.GetNext(&next, &graph_has_cycle, nullptr)) {
    topological_order_.push_back(IntegerVariable(next));
    if (graph_has_cycle) {
      is_dag_ = false;
      return;
    }
  }
  is_dag_ = !graph_has_cycle;

  // Lets build full precedences if we don't have too many of them.
  // TODO(user): Also do that if we don't have a DAG?
  if (!is_dag_) return;

  int work = 0;
  const int kWorkLimit = 1e6;
  for (const IntegerVariable tail_var : topological_order_) {
    if (++work > kWorkLimit) break;
    for (const int arc : graph_.OutgoingArcs(tail_var.value())) {
      DCHECK_EQ(tail_var.value(), graph_.Tail(arc));
      const IntegerVariable head_var = IntegerVariable(graph_.Head(arc));
      const IntegerValue arc_offset = arc_offsets_[arc];

      if (++work > kWorkLimit) break;
      if (AddInternal(tail_var, head_var, arc_offset)) {
        before[head_var].push_back(tail_var);
      }

      for (const IntegerVariable before_var : before[tail_var]) {
        if (++work > kWorkLimit) break;
        const IntegerValue offset =
            -root_relations_.at(GetKey(before_var, NegationOf(tail_var))) +
            arc_offset;
        if (AddInternal(before_var, head_var, offset)) {
          before[head_var].push_back(before_var);
        }
      }
    }
  }

  VLOG(2) << "Full precedences. Work=" << work
          << " Relations=" << root_relations_.size();
}

void PrecedenceRelations::ComputeFullPrecedences(
    absl::Span<const IntegerVariable> vars,
    std::vector<FullIntegerPrecedence>* output) {
  output->clear();
  if (!is_built_) Build();
  if (!is_dag_) return;

  VLOG(2) << "num_nodes: " << graph_.num_nodes()
          << " num_arcs: " << graph_.num_arcs() << " is_dag: " << is_dag_;

  // Compute all precedences.
  // We loop over the node in topological order, and we maintain for all
  // variable we encounter, the list of "to_consider" variables that are before.
  //
  // TODO(user): use vector of fixed size.
  absl::flat_hash_set<IntegerVariable> is_interesting;
  absl::flat_hash_set<IntegerVariable> to_consider(vars.begin(), vars.end());
  absl::flat_hash_map<IntegerVariable,
                      absl::flat_hash_map<IntegerVariable, IntegerValue>>
      vars_before_with_offset;
  absl::flat_hash_map<IntegerVariable, IntegerValue> tail_map;
  for (const IntegerVariable tail_var : topological_order_) {
    if (!to_consider.contains(tail_var) &&
        !vars_before_with_offset.contains(tail_var)) {
      continue;
    }

    // We copy the data for tail_var here, because the pointer is not stable.
    // TODO(user): optimize when needed.
    tail_map.clear();
    {
      const auto it = vars_before_with_offset.find(tail_var);
      if (it != vars_before_with_offset.end()) {
        tail_map = it->second;
      }
    }

    for (const int arc : graph_.OutgoingArcs(tail_var.value())) {
      CHECK_EQ(tail_var.value(), graph_.Tail(arc));
      const IntegerVariable head_var = IntegerVariable(graph_.Head(arc));
      const IntegerValue arc_offset = arc_offsets_[arc];

      // No need to create an empty entry in this case.
      if (tail_map.empty() && !to_consider.contains(tail_var)) continue;

      auto& to_update = vars_before_with_offset[head_var];
      for (const auto& [var_before, offset] : tail_map) {
        if (!to_update.contains(var_before)) {
          to_update[var_before] = arc_offset + offset;
        } else {
          to_update[var_before] =
              std::max(arc_offset + offset, to_update[var_before]);
        }
      }
      if (to_consider.contains(tail_var)) {
        if (!to_update.contains(tail_var)) {
          to_update[tail_var] = arc_offset;
        } else {
          to_update[tail_var] = std::max(arc_offset, to_update[tail_var]);
        }
      }

      // Small filtering heuristic: if we have (before) < tail, and tail < head,
      // we really do not need to list (before, tail) < head. We only need that
      // if the list of variable before head contains some variable that are not
      // already before tail.
      if (to_update.size() > tail_map.size() + 1) {
        is_interesting.insert(head_var);
      } else {
        is_interesting.erase(head_var);
      }
    }

    // Extract the output for tail_var. Because of the topological ordering, the
    // data for tail_var is already final now.
    //
    // TODO(user): Release the memory right away.
    if (!is_interesting.contains(tail_var)) continue;
    if (tail_map.size() == 1) continue;

    FullIntegerPrecedence data;
    data.var = tail_var;
    IntegerValue min_offset = kMaxIntegerValue;
    for (int i = 0; i < vars.size(); ++i) {
      const auto offset_it = tail_map.find(vars[i]);
      if (offset_it == tail_map.end()) continue;
      data.indices.push_back(i);
      data.offsets.push_back(offset_it->second);
      min_offset = std::min(data.offsets.back(), min_offset);
    }
    output->push_back(std::move(data));
  }
}

void PrecedenceRelations::CollectPrecedences(
    absl::Span<const IntegerVariable> vars,
    std::vector<PrecedenceData>* output) {
  // +1 for the negation.
  const int needed_size =
      std::max(after_.size(), conditional_after_.size()) + 1;
  var_to_degree_.resize(needed_size);
  var_to_last_index_.resize(needed_size);
  var_with_positive_degree_.resize(needed_size);
  tmp_precedences_.clear();

  // We first compute the degree/size in order to perform the transposition.
  // Note that we also remove duplicates.
  int num_relevants = 0;
  IntegerVariable* var_with_positive_degree = var_with_positive_degree_.data();
  int* var_to_degree = var_to_degree_.data();
  int* var_to_last_index = var_to_last_index_.data();
  const auto process = [&](int index, absl::Span<const IntegerVariable> v) {
    for (const IntegerVariable after : v) {
      DCHECK_LT(after, needed_size);
      if (var_to_degree[after.value()] == 0) {
        var_with_positive_degree[num_relevants++] = after;
      } else {
        // We do not want duplicates.
        if (var_to_last_index[after.value()] == index) continue;
      }

      tmp_precedences_.push_back({after, index});
      var_to_degree[after.value()]++;
      var_to_last_index[after.value()] = index;
    }
  };

  for (int index = 0; index < vars.size(); ++index) {
    const IntegerVariable var = vars[index];
    if (var < after_.size()) {
      process(index, after_[var]);
    }
    if (var < conditional_after_.size()) {
      process(index, conditional_after_[var]);
    }
  }

  // Permute tmp_precedences_ into the output to put it in the correct order.
  // For that we transform var_to_degree to point to the first position of
  // each lbvar in the output vector.
  int start = 0;
  for (int i = 0; i < num_relevants; ++i) {
    const IntegerVariable var = var_with_positive_degree[i];
    const int degree = var_to_degree[var.value()];
    if (degree > 1) {
      var_to_degree[var.value()] = start;
      start += degree;
    } else {
      // Optimization: we remove degree one relations.
      var_to_degree[var.value()] = -1;
    }
  }

  output->resize(start);
  for (const auto& precedence : tmp_precedences_) {
    // Note that it is okay to increase the -1 pos since they appear only once.
    const int pos = var_to_degree[precedence.var.value()]++;
    if (pos < 0) continue;
    (*output)[pos] = precedence;
  }

  // Cleanup var_to_degree, note that we don't need to clean
  // var_to_last_index_.
  for (int i = 0; i < num_relevants; ++i) {
    const IntegerVariable var = var_with_positive_degree[i];
    var_to_degree[var.value()] = 0;
  }
}

namespace {

void AppendLowerBoundReasonIfValid(IntegerVariable var,
                                   const IntegerTrail& i_trail,
                                   std::vector<IntegerLiteral>* reason) {
  if (var != kNoIntegerVariable) {
    reason->push_back(i_trail.LowerBoundAsLiteral(var));
  }
}

}  // namespace

PrecedencesPropagator::~PrecedencesPropagator() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"precedences/num_cycles", num_cycles_});
  stats.push_back({"precedences/num_pushes", num_pushes_});
  stats.push_back(
      {"precedences/num_enforcement_pushes", num_enforcement_pushes_});
  shared_stats_->AddStats(stats);
}

bool PrecedencesPropagator::Propagate(Trail* trail) { return Propagate(); }

bool PrecedencesPropagator::Propagate() {
  while (propagation_trail_index_ < trail_->Index()) {
    const Literal literal = (*trail_)[propagation_trail_index_++];
    if (literal.Index() >= literal_to_new_impacted_arcs_.size()) continue;

    // IMPORTANT: Because of the way Untrail() work, we need to add all the
    // potential arcs before we can abort. It is why we iterate twice here.
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (--arc_counts_[arc_index] == 0) {
        const ArcInfo& arc = arcs_[arc_index];
        PushConditionalRelations(arc);
        impacted_arcs_[arc.tail_var].push_back(arc_index);
      }
    }

    // Iterate again to check for a propagation and indirectly update
    // modified_vars_.
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (arc_counts_[arc_index] > 0) continue;
      const ArcInfo& arc = arcs_[arc_index];
      const IntegerValue new_head_lb =
          integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc);
      if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
        if (!EnqueueAndCheck(arc, new_head_lb, trail_)) return false;
      }
    }
  }

  // Do the actual propagation of the IntegerVariable bounds.
  InitializeBFQueueWithModifiedNodes();
  if (!BellmanFordTarjan(trail_)) return false;

  // We can only test that no propagation is left if we didn't enqueue new
  // literal in the presence of optional variables.
  //
  // TODO(user): Because of our code to deal with InPropagationLoop(), this is
  // not always true. Find a cleaner way to DCHECK() while not failing in this
  // corner case.
  if (/*DISABLES CODE*/ (false) &&
      propagation_trail_index_ == trail_->Index()) {
    DCHECK(NoPropagationLeft(*trail_));
  }

  // Propagate the presence literals of the arcs that can't be added.
  PropagateOptionalArcs(trail_);

  // Clean-up modified_vars_ to do as little as possible on the next call.
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  return true;
}

bool PrecedencesPropagator::PropagateOutgoingArcs(IntegerVariable var) {
  CHECK_NE(var, kNoIntegerVariable);
  if (var >= impacted_arcs_.size()) return true;
  for (const ArcIndex arc_index : impacted_arcs_[var]) {
    const ArcInfo& arc = arcs_[arc_index];
    const IntegerValue new_head_lb =
        integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc);
    if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
      if (!EnqueueAndCheck(arc, new_head_lb, trail_)) return false;
    }
  }
  return true;
}

// TODO(user): Remove literal fixed at level zero from there.
void PrecedencesPropagator::PushConditionalRelations(const ArcInfo& arc) {
  // We currently do not handle variable size in the reasons.
  // TODO(user): we could easily take a level zero ArcOffset() instead, or
  // add this to the reason though.
  if (arc.offset_var != kNoIntegerVariable) return;
  const IntegerValue offset = ArcOffset(arc);
  relations_->PushConditionalRelation(arc.presence_literals, arc.tail_var,
                                      NegationOf(arc.head_var), -offset);
}

void PrecedencesPropagator::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ > trail_index) {
    // This means that we already propagated all there is to propagate
    // at the level trail_index, so we can safely clear modified_vars_ in case
    // it wasn't already done.
    modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  }
  while (propagation_trail_index_ > trail_index) {
    const Literal literal = trail[--propagation_trail_index_];
    if (literal.Index() >= literal_to_new_impacted_arcs_.size()) continue;
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (arc_counts_[arc_index]++ == 0) {
        const ArcInfo& arc = arcs_[arc_index];
        impacted_arcs_[arc.tail_var].pop_back();
      }
    }
  }
}

void PrecedencesPropagator::AdjustSizeFor(IntegerVariable i) {
  const int index = std::max(i.value(), NegationOf(i).value());
  if (index >= impacted_arcs_.size()) {
    // TODO(user): only watch lower bound of the relevant variable instead
    // of watching everything in [0, max_index_of_variable_used_in_this_class).
    for (IntegerVariable var(impacted_arcs_.size()); var <= index; ++var) {
      watcher_->WatchLowerBound(var, watcher_id_);
    }
    impacted_arcs_.resize(index + 1);
    impacted_potential_arcs_.resize(index + 1);
  }
}

void PrecedencesPropagator::AddArc(
    IntegerVariable tail, IntegerVariable head, IntegerValue offset,
    IntegerVariable offset_var, absl::Span<const Literal> presence_literals) {
  AdjustSizeFor(tail);
  AdjustSizeFor(head);
  if (offset_var != kNoIntegerVariable) AdjustSizeFor(offset_var);

  // This arc is present iff all the literals here are true.
  absl::InlinedVector<Literal, 6> enforcement_literals;
  {
    for (const Literal l : presence_literals) {
      enforcement_literals.push_back(l);
    }
    gtl::STLSortAndRemoveDuplicates(&enforcement_literals);

    if (trail_->CurrentDecisionLevel() == 0) {
      int new_size = 0;
      for (const Literal l : enforcement_literals) {
        if (trail_->Assignment().LiteralIsTrue(Literal(l))) {
          continue;  // At true, ignore this literal.
        } else if (trail_->Assignment().LiteralIsFalse(Literal(l))) {
          return;  // At false, ignore completely this arc.
        }
        enforcement_literals[new_size++] = l;
      }
      enforcement_literals.resize(new_size);
    }
  }

  if (head == tail) {
    // A self-arc is either plain SAT or plain UNSAT or it forces something on
    // the given offset_var or presence_literal_index. In any case it could be
    // presolved in something more efficient.
    VLOG(1) << "Self arc! This could be presolved. "
            << "var:" << tail << " offset:" << offset
            << " offset_var:" << offset_var
            << " conditioned_by:" << presence_literals;
  }

  // Remove the offset_var if it is fixed.
  // TODO(user): We should also handle the case where tail or head is fixed.
  if (offset_var != kNoIntegerVariable) {
    const IntegerValue lb = integer_trail_->LevelZeroLowerBound(offset_var);
    if (lb == integer_trail_->LevelZeroUpperBound(offset_var)) {
      offset += lb;
      offset_var = kNoIntegerVariable;
    }
  }

  // Deal first with impacted_potential_arcs_/potential_arcs_.
  if (!enforcement_literals.empty()) {
    const OptionalArcIndex arc_index(potential_arcs_.size());
    potential_arcs_.push_back(
        {tail, head, offset, offset_var, enforcement_literals});
    impacted_potential_arcs_[tail].push_back(arc_index);
    impacted_potential_arcs_[NegationOf(head)].push_back(arc_index);
    if (offset_var != kNoIntegerVariable) {
      impacted_potential_arcs_[offset_var].push_back(arc_index);
    }
  }

  // Now deal with impacted_arcs_/arcs_.
  struct InternalArc {
    IntegerVariable tail_var;
    IntegerVariable head_var;
    IntegerVariable offset_var;
  };
  std::vector<InternalArc> to_add;
  if (offset_var == kNoIntegerVariable) {
    // a + offset <= b and -b + offset <= -a
    to_add.push_back({tail, head, kNoIntegerVariable});
    to_add.push_back({NegationOf(head), NegationOf(tail), kNoIntegerVariable});
  } else {
    // tail (a) and offset_var (b) are symmetric, so we add:
    // - a + b + offset <= c
    to_add.push_back({tail, head, offset_var});
    to_add.push_back({offset_var, head, tail});
    // - a - c + offset <= -b
    to_add.push_back({tail, NegationOf(offset_var), NegationOf(head)});
    to_add.push_back({NegationOf(head), NegationOf(offset_var), tail});
    // - b - c + offset <= -a
    to_add.push_back({offset_var, NegationOf(tail), NegationOf(head)});
    to_add.push_back({NegationOf(head), NegationOf(tail), offset_var});
  }
  for (const InternalArc a : to_add) {
    // Since we add a new arc, we will need to consider its tail during the next
    // propagation. Note that the size of modified_vars_ will be automatically
    // updated when new integer variables are created since we register it with
    // IntegerTrail in this class constructor.
    //
    // TODO(user): Adding arcs and then calling Untrail() before Propagate()
    // will cause this mecanism to break. Find a more robust implementation.
    //
    // TODO(user): In some rare corner case, rescanning the whole list of arc
    // leaving tail_var can make AddVar() have a quadratic complexity where it
    // shouldn't. A better solution would be to see if this new arc currently
    // propagate something, and if it does, just update the lower bound of
    // a.head_var and let the normal "is modified" mecanism handle any eventual
    // follow up propagations.
    modified_vars_.Set(a.tail_var);

    // If a.head_var is optional, we can potentially remove some literal from
    // enforcement_literals.
    const ArcIndex arc_index(arcs_.size());
    arcs_.push_back(
        {a.tail_var, a.head_var, offset, a.offset_var, enforcement_literals});
    auto& presence_literals = arcs_.back().presence_literals;

    if (presence_literals.empty()) {
      impacted_arcs_[a.tail_var].push_back(arc_index);
    } else {
      for (const Literal l : presence_literals) {
        if (l.Index() >= literal_to_new_impacted_arcs_.size()) {
          literal_to_new_impacted_arcs_.resize(l.Index().value() + 1);
        }
        literal_to_new_impacted_arcs_[l.Index()].push_back(arc_index);
      }
    }

    if (trail_->CurrentDecisionLevel() == 0) {
      arc_counts_.push_back(presence_literals.size());
    } else {
      arc_counts_.push_back(0);
      for (const Literal l : presence_literals) {
        if (!trail_->Assignment().LiteralIsTrue(l)) {
          ++arc_counts_.back();
        }
      }
      CHECK(presence_literals.empty() || arc_counts_.back() > 0);
    }
  }
}

bool PrecedencesPropagator::AddPrecedenceWithOffsetIfNew(IntegerVariable i1,
                                                         IntegerVariable i2,
                                                         IntegerValue offset) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (i1 < impacted_arcs_.size() && i2 < impacted_arcs_.size()) {
    for (const ArcIndex index : impacted_arcs_[i1]) {
      const ArcInfo& arc = arcs_[index];
      if (arc.head_var == i2) {
        const IntegerValue current = ArcOffset(arc);
        if (offset <= current) {
          return false;
        } else {
          // TODO(user): Modify arc in place!
        }
        break;
      }
    }
  }

  AddPrecedenceWithOffset(i1, i2, offset);
  return true;
}

// TODO(user): On jobshop problems with a lot of tasks per machine (500), this
// takes up a big chunk of the running time even before we find a solution.
// This is because, for each lower bound changed, we inspect 500 arcs even
// though they will never be propagated because the other bound is still at the
// horizon. Find an even sparser algorithm?
void PrecedencesPropagator::PropagateOptionalArcs(Trail* trail) {
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    // The variables are not in increasing order, so we need to continue.
    if (var >= impacted_potential_arcs_.size()) continue;

    // Note that we can currently check the same ArcInfo up to 3 times, one for
    // each of the arc variables: tail, NegationOf(head) and offset_var.
    for (const OptionalArcIndex arc_index : impacted_potential_arcs_[var]) {
      const ArcInfo& arc = potential_arcs_[arc_index];
      int num_not_true = 0;
      Literal to_propagate;
      for (const Literal l : arc.presence_literals) {
        if (!trail->Assignment().LiteralIsTrue(l)) {
          ++num_not_true;
          to_propagate = l;
        }
      }
      if (num_not_true != 1) continue;
      if (trail->Assignment().LiteralIsFalse(to_propagate)) continue;

      // Test if this arc can be present or not.
      // Important arc.tail_var can be different from var here.
      const IntegerValue tail_lb = integer_trail_->LowerBound(arc.tail_var);
      const IntegerValue head_ub = integer_trail_->UpperBound(arc.head_var);
      if (tail_lb + ArcOffset(arc) > head_ub) {
        integer_reason_.clear();
        integer_reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(arc.tail_var));
        integer_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(arc.head_var));
        AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                      &integer_reason_);
        literal_reason_.clear();
        for (const Literal l : arc.presence_literals) {
          if (l != to_propagate) literal_reason_.push_back(l.Negated());
        }
        ++num_enforcement_pushes_;
        integer_trail_->EnqueueLiteral(to_propagate.Negated(), literal_reason_,
                                       integer_reason_);
      }
    }
  }
}

IntegerValue PrecedencesPropagator::ArcOffset(const ArcInfo& arc) const {
  return arc.offset + (arc.offset_var == kNoIntegerVariable
                           ? IntegerValue(0)
                           : integer_trail_->LowerBound(arc.offset_var));
}

bool PrecedencesPropagator::EnqueueAndCheck(const ArcInfo& arc,
                                            IntegerValue new_head_lb,
                                            Trail* trail) {
  ++num_pushes_;
  DCHECK_GT(new_head_lb, integer_trail_->LowerBound(arc.head_var));

  // Compute the reason for new_head_lb.
  //
  // TODO(user): do like for clause and keep the negation of
  // arc.presence_literals? I think we could change the integer.h API to accept
  // true literal like for IntegerVariable, it is really confusing currently.
  literal_reason_.clear();
  for (const Literal l : arc.presence_literals) {
    literal_reason_.push_back(l.Negated());
  }

  integer_reason_.clear();
  integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(arc.tail_var));
  AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                &integer_reason_);

  // The code works without this block since Enqueue() below can already take
  // care of conflicts. However, it is better to deal with the conflict
  // ourselves because we can be smarter about the reason this way.
  //
  // The reason for a "precedence" conflict is always a linear reason
  // involving the tail lower_bound, the head upper bound and eventually the
  // size lower bound. Because of that, we can use the RelaxLinearReason()
  // code.
  if (new_head_lb > integer_trail_->UpperBound(arc.head_var)) {
    const IntegerValue slack =
        new_head_lb - integer_trail_->UpperBound(arc.head_var) - 1;
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(arc.head_var));
    std::vector<IntegerValue> coeffs(integer_reason_.size(), IntegerValue(1));
    integer_trail_->RelaxLinearReason(slack, coeffs, &integer_reason_);
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(arc.head_var, new_head_lb),
      literal_reason_, integer_reason_);
}

bool PrecedencesPropagator::NoPropagationLeft(const Trail& trail) const {
  const int num_nodes = impacted_arcs_.size();
  for (IntegerVariable var(0); var < num_nodes; ++var) {
    for (const ArcIndex arc_index : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc) >
          integer_trail_->LowerBound(arc.head_var)) {
        return false;
      }
    }
  }
  return true;
}

void PrecedencesPropagator::InitializeBFQueueWithModifiedNodes() {
  // Sparse clear of the queue. TODO(user): only use the sparse version if
  // queue.size() is small or use SparseBitset.
  const int num_nodes = impacted_arcs_.size();
  bf_in_queue_.resize(num_nodes, false);
  for (const int node : bf_queue_) bf_in_queue_[node] = false;
  bf_queue_.clear();
  DCHECK(std::none_of(bf_in_queue_.begin(), bf_in_queue_.end(),
                      [](bool v) { return v; }));
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= num_nodes) continue;
    bf_queue_.push_back(var.value());
    bf_in_queue_[var.value()] = true;
  }
}

void PrecedencesPropagator::CleanUpMarkedArcsAndParents() {
  // To be sparse, we use the fact that each node with a parent must be in
  // modified_vars_.
  const int num_nodes = impacted_arcs_.size();
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= num_nodes) continue;
    const ArcIndex parent_arc_index = bf_parent_arc_of_[var.value()];
    if (parent_arc_index != -1) {
      arcs_[parent_arc_index].is_marked = false;
      bf_parent_arc_of_[var.value()] = -1;
      bf_can_be_skipped_[var.value()] = false;
    }
  }
  DCHECK(std::none_of(bf_parent_arc_of_.begin(), bf_parent_arc_of_.end(),
                      [](ArcIndex v) { return v != -1; }));
  DCHECK(std::none_of(bf_can_be_skipped_.begin(), bf_can_be_skipped_.end(),
                      [](bool v) { return v; }));
}

bool PrecedencesPropagator::DisassembleSubtree(
    int source, int target, std::vector<bool>* can_be_skipped) {
  // Note that we explore a tree, so we can do it in any order, and the one
  // below seems to be the fastest.
  tmp_vector_.clear();
  tmp_vector_.push_back(source);
  while (!tmp_vector_.empty()) {
    const int tail = tmp_vector_.back();
    tmp_vector_.pop_back();
    for (const ArcIndex arc_index : impacted_arcs_[IntegerVariable(tail)]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (arc.is_marked) {
        arc.is_marked = false;  // mutable.
        if (arc.head_var.value() == target) return true;
        DCHECK(!(*can_be_skipped)[arc.head_var.value()]);
        (*can_be_skipped)[arc.head_var.value()] = true;
        tmp_vector_.push_back(arc.head_var.value());
      }
    }
  }
  return false;
}

void PrecedencesPropagator::AnalyzePositiveCycle(
    ArcIndex first_arc, Trail* trail, std::vector<Literal>* must_be_all_true,
    std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) {
  must_be_all_true->clear();
  literal_reason->clear();
  integer_reason->clear();

  // Follow bf_parent_arc_of_[] to find the cycle containing first_arc.
  const IntegerVariable first_arc_head = arcs_[first_arc].head_var;
  ArcIndex arc_index = first_arc;
  std::vector<ArcIndex> arc_on_cycle;

  // Just to be safe and avoid an infinite loop we use the fact that the maximum
  // cycle size on a graph with n nodes is of size n. If we have more in the
  // code below, it means first_arc is not part of a cycle according to
  // bf_parent_arc_of_[], which should never happen.
  const int num_nodes = impacted_arcs_.size();
  while (arc_on_cycle.size() <= num_nodes) {
    arc_on_cycle.push_back(arc_index);
    const ArcInfo& arc = arcs_[arc_index];
    if (arc.tail_var == first_arc_head) break;
    arc_index = bf_parent_arc_of_[arc.tail_var.value()];
    CHECK_NE(arc_index, ArcIndex(-1));
  }
  CHECK_NE(arc_on_cycle.size(), num_nodes + 1) << "Infinite loop.";

  // Compute the reason for this cycle.
  IntegerValue sum(0);
  for (const ArcIndex arc_index : arc_on_cycle) {
    const ArcInfo& arc = arcs_[arc_index];
    sum += ArcOffset(arc);
    AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                  integer_reason);
    for (const Literal l : arc.presence_literals) {
      literal_reason->push_back(l.Negated());
    }
  }

  // TODO(user): what if the sum overflow? this is just a check so I guess
  // we don't really care, but fix the issue.
  CHECK_GT(sum, 0);
}

// Note that in our settings it is important to use an algorithm that tries to
// minimize the number of integer_trail_->Enqueue() as much as possible.
//
// TODO(user): The current algorithm is quite efficient, but there is probably
// still room for improvements.
bool PrecedencesPropagator::BellmanFordTarjan(Trail* trail) {
  const int num_nodes = impacted_arcs_.size();

  // These vector are reset by CleanUpMarkedArcsAndParents() so resize is ok.
  bf_can_be_skipped_.resize(num_nodes, false);
  bf_parent_arc_of_.resize(num_nodes, ArcIndex(-1));
  const auto cleanup =
      ::absl::MakeCleanup([this]() { CleanUpMarkedArcsAndParents(); });

  // The queue initialization is done by InitializeBFQueueWithModifiedNodes().
  while (!bf_queue_.empty()) {
    const int node = bf_queue_.front();
    bf_queue_.pop_front();
    bf_in_queue_[node] = false;

    // TODO(user): we don't need bf_can_be_skipped_ since we can detect this
    // if this node has a parent arc which is not marked. Investigate if it is
    // faster without the vector<bool>.
    //
    // TODO(user): An alternative algorithm is to remove all these nodes from
    // the queue instead of simply marking them. This should also lead to a
    // better "relaxation" order of the arcs. It is however a bit more work to
    // remove them since we need to track their position.
    if (bf_can_be_skipped_[node]) {
      DCHECK_NE(bf_parent_arc_of_[node], -1);
      DCHECK(!arcs_[bf_parent_arc_of_[node]].is_marked);
      continue;
    }

    const IntegerValue tail_lb =
        integer_trail_->LowerBound(IntegerVariable(node));
    for (const ArcIndex arc_index : impacted_arcs_[IntegerVariable(node)]) {
      const ArcInfo& arc = arcs_[arc_index];
      DCHECK_EQ(arc.tail_var, node);
      const IntegerValue candidate = tail_lb + ArcOffset(arc);
      if (candidate > integer_trail_->LowerBound(arc.head_var)) {
        if (!EnqueueAndCheck(arc, candidate, trail)) return false;

        // This is the Tarjan contribution to Bellman-Ford. This code detect
        // positive cycle, and because it disassemble the subtree while doing
        // so, the cost is amortized during the algorithm execution. Another
        // advantages is that it will mark the node explored here as skippable
        // which will avoid to propagate them too early (knowing that they will
        // need to be propagated again later).
        if (DisassembleSubtree(arc.head_var.value(), arc.tail_var.value(),
                               &bf_can_be_skipped_)) {
          std::vector<Literal> must_be_all_true;
          AnalyzePositiveCycle(arc_index, trail, &must_be_all_true,
                               &literal_reason_, &integer_reason_);
          if (must_be_all_true.empty()) {
            ++num_cycles_;
            return integer_trail_->ReportConflict(literal_reason_,
                                                  integer_reason_);
          } else {
            gtl::STLSortAndRemoveDuplicates(&must_be_all_true);
            for (const Literal l : must_be_all_true) {
              if (trail_->Assignment().LiteralIsFalse(l)) {
                literal_reason_.push_back(l);
                return integer_trail_->ReportConflict(literal_reason_,
                                                      integer_reason_);
              }
            }
            for (const Literal l : must_be_all_true) {
              if (trail_->Assignment().LiteralIsTrue(l)) continue;
              integer_trail_->EnqueueLiteral(l, literal_reason_,
                                             integer_reason_);
            }

            // We just marked some optional variable as ignored, no need
            // to update bf_parent_arc_of_[].
            continue;
          }
        }

        // We need to enforce the invariant that only the arc_index in
        // bf_parent_arc_of_[] are marked (but not necessarily all of them
        // since we unmark some in DisassembleSubtree()).
        if (bf_parent_arc_of_[arc.head_var.value()] != -1) {
          arcs_[bf_parent_arc_of_[arc.head_var.value()]].is_marked = false;
        }

        // Tricky: We just enqueued the fact that the lower-bound of head is
        // candidate. However, because the domain of head may be discrete, it is
        // possible that the lower-bound of head is now higher than candidate!
        // If this is the case, we don't update bf_parent_arc_of_[] so that we
        // don't wrongly detect a positive weight cycle because of this "extra
        // push".
        const IntegerValue new_bound = integer_trail_->LowerBound(arc.head_var);
        if (new_bound == candidate) {
          bf_parent_arc_of_[arc.head_var.value()] = arc_index;
          arcs_[arc_index].is_marked = true;
        } else {
          // We still unmark any previous dependency, since we have pushed the
          // value of arc.head_var further.
          bf_parent_arc_of_[arc.head_var.value()] = -1;
        }

        // We do not re-enqueue if we are in a propagation loop and new_bound
        // was not pushed to candidate or higher.
        bf_can_be_skipped_[arc.head_var.value()] = false;
        if (!bf_in_queue_[arc.head_var.value()] && new_bound >= candidate) {
          bf_queue_.push_back(arc.head_var.value());
          bf_in_queue_[arc.head_var.value()] = true;
        }
      }
    }
  }
  return true;
}

void BinaryRelationRepository::Add(Literal lit, LinearTerm a, LinearTerm b,
                                   IntegerValue lhs, IntegerValue rhs) {
  Relation r;
  r.enforcement = lit;
  r.a = a;
  r.b = b;
  r.lhs = lhs;
  r.rhs = rhs;

  // We shall only consider positive variable here.
  if (r.a.var != kNoIntegerVariable && !VariableIsPositive(r.a.var)) {
    r.a.var = NegationOf(r.a.var);
    r.a.coeff = -r.a.coeff;
  }
  if (r.b.var != kNoIntegerVariable && !VariableIsPositive(r.b.var)) {
    r.b.var = NegationOf(r.b.var);
    r.b.coeff = -r.b.coeff;
  }

  relations_.push_back(std::move(r));
}

void BinaryRelationRepository::Build() {
  DCHECK(!is_built_);
  is_built_ = true;
  std::vector<LiteralIndex> keys;
  const int num_relations = relations_.size();
  keys.reserve(num_relations);
  for (int i = 0; i < num_relations; ++i) {
    keys.push_back(relations_[i].enforcement.Index());
  }
  lit_to_relations_.ResetFromFlatMapping(keys, IdentityMap<int>());
}

bool GreaterThanAtLeastOneOfDetector::AddRelationFromIndices(
    IntegerVariable var, absl::Span<const Literal> clause,
    absl::Span<const int> indices, Model* model) {
  std::vector<AffineExpression> exprs;
  std::vector<Literal> selectors;
  absl::flat_hash_set<LiteralIndex> used;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  const IntegerValue var_lb = integer_trail->LevelZeroLowerBound(var);
  for (const int index : indices) {
    Relation r = repository_.relation(index);
    if (r.a.var != PositiveVariable(var)) std::swap(r.a, r.b);
    CHECK_EQ(r.a.var, PositiveVariable(var));

    if ((r.a.coeff == 1) == VariableIsPositive(var)) {
      //  a + b >= lhs
      if (r.lhs <= kMinIntegerValue) continue;
      exprs.push_back(AffineExpression(r.b.var, -r.b.coeff, r.lhs));
    } else {
      // -a + b <= rhs.
      if (r.rhs >= kMaxIntegerValue) continue;
      exprs.push_back(AffineExpression(r.b.var, r.b.coeff, -r.rhs));
    }

    // Ignore this entry if it is always true.
    if (var_lb >= integer_trail->LevelZeroUpperBound(exprs.back())) {
      exprs.pop_back();
      continue;
    }

    // Note that duplicate selector are supported.
    selectors.push_back(r.enforcement);
    used.insert(r.enforcement);
  }

  // The enforcement of the new constraint are simply the literal not used
  // above.
  std::vector<Literal> enforcements;
  for (const Literal l : clause) {
    if (!used.contains(l.Index())) {
      enforcements.push_back(l.Negated());
    }
  }

  // No point adding a constraint if there is not at least two different
  // literals in selectors.
  if (used.size() <= 1) return false;

  // Add the constraint.
  GreaterThanAtLeastOneOfPropagator* constraint =
      new GreaterThanAtLeastOneOfPropagator(var, exprs, selectors, enforcements,
                                            model);
  constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  model->TakeOwnership(constraint);
  return true;
}

int GreaterThanAtLeastOneOfDetector::
    AddGreaterThanAtLeastOneOfConstraintsFromClause(
        const absl::Span<const Literal> clause, Model* model) {
  CHECK_EQ(model->GetOrCreate<Trail>()->CurrentDecisionLevel(), 0);
  if (clause.size() < 2) return 0;

  // Collect all relations impacted by this clause.
  std::vector<std::pair<IntegerVariable, int>> infos;
  for (const Literal l : clause) {
    for (const int index : repository_.relation_indices(l.Index())) {
      const Relation& r = repository_.relation(index);
      if (r.a.var != kNoIntegerVariable && IntTypeAbs(r.a.coeff) == 1) {
        infos.push_back({r.a.var, index});
      }
      if (r.b.var != kNoIntegerVariable && IntTypeAbs(r.b.coeff) == 1) {
        infos.push_back({r.b.var, index});
      }
    }
  }
  if (infos.size() <= 1) return 0;

  // Stable sort to regroup by var.
  std::stable_sort(infos.begin(), infos.end());

  // We process the info with same variable together.
  int num_added_constraints = 0;
  std::vector<int> indices;
  for (int i = 0; i < infos.size();) {
    const int start = i;
    const IntegerVariable var = infos[start].first;

    indices.clear();
    for (; i < infos.size() && infos[i].first == var; ++i) {
      indices.push_back(infos[i].second);
    }

    // Skip single relations, we are not interested in these.
    if (indices.size() < 2) continue;

    // Heuristic. Look for full or almost full clauses.
    //
    // TODO(user): We could add GreaterThanAtLeastOneOf() with more enforcement
    // literals. Experiment.
    if (indices.size() + 1 < clause.size()) continue;

    if (AddRelationFromIndices(var, clause, indices, model)) {
      ++num_added_constraints;
    }
    if (AddRelationFromIndices(NegationOf(var), clause, indices, model)) {
      ++num_added_constraints;
    }
  }
  return num_added_constraints;
}

int GreaterThanAtLeastOneOfDetector::
    AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(Model* model) {
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();

  // Fill the set of interesting relations for each variables.
  util_intops::StrongVector<IntegerVariable, std::vector<int>> var_to_relations;
  for (int index = 0; index < repository_.size(); ++index) {
    const Relation& r = repository_.relation(index);
    if (r.a.var != kNoIntegerVariable && IntTypeAbs(r.a.coeff) == 1) {
      if (r.a.var >= var_to_relations.size()) {
        var_to_relations.resize(r.a.var + 1);
      }
      var_to_relations[r.a.var].push_back(index);
    }
    if (r.b.var != kNoIntegerVariable && IntTypeAbs(r.b.coeff) == 1) {
      if (r.b.var >= var_to_relations.size()) {
        var_to_relations.resize(r.b.var + 1);
      }
      var_to_relations[r.b.var].push_back(index);
    }
  }

  int num_added_constraints = 0;
  for (IntegerVariable target(0); target < var_to_relations.size(); ++target) {
    if (var_to_relations[target].size() <= 1) continue;
    if (time_limit->LimitReached()) return num_added_constraints;

    // Detect set of incoming arcs for which at least one must be present.
    // TODO(user): Find more than one disjoint set of incoming arcs.
    // TODO(user): call MinimizeCoreWithPropagation() on the clause.
    solver->Backtrack(0);
    if (solver->ModelIsUnsat()) return num_added_constraints;
    std::vector<Literal> clause;
    for (const int index : var_to_relations[target]) {
      const Literal literal = repository_.relation(index).enforcement;
      if (solver->Assignment().LiteralIsFalse(literal)) continue;
      const SatSolver::Status status =
          solver->EnqueueDecisionAndBacktrackOnConflict(literal.Negated());
      if (status == SatSolver::INFEASIBLE) return num_added_constraints;
      if (status == SatSolver::ASSUMPTIONS_UNSAT) {
        // We need to invert it, since a clause is not all false.
        clause = solver->GetLastIncompatibleDecisions();
        for (Literal& ref : clause) ref = ref.Negated();
        break;
      }
    }
    solver->Backtrack(0);
    if (clause.size() <= 1) continue;

    // Recover the indices corresponding to this clause.
    const absl::btree_set<Literal> clause_set(clause.begin(), clause.end());

    std::vector<int> indices;
    for (const int index : var_to_relations[target]) {
      const Literal literal = repository_.relation(index).enforcement;
      if (clause_set.contains(literal)) {
        indices.push_back(index);
      }
    }

    // Try both direction.
    if (AddRelationFromIndices(target, clause, indices, model)) {
      ++num_added_constraints;
    }
    if (AddRelationFromIndices(NegationOf(target), clause, indices, model)) {
      ++num_added_constraints;
    }
  }

  solver->Backtrack(0);
  return num_added_constraints;
}

int GreaterThanAtLeastOneOfDetector::AddGreaterThanAtLeastOneOfConstraints(
    Model* model, bool auto_detect_clauses) {
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* clauses = model->GetOrCreate<ClauseManager>();
  auto* logger = model->GetOrCreate<SolverLogger>();

  int num_added_constraints = 0;
  SOLVER_LOG(logger, "[Precedences] num_relations=", repository_.size(),
             " num_clauses=", clauses->AllClausesInCreationOrder().size());

  // We have two possible approaches. For now, we prefer the first one except if
  // there is too many clauses in the problem.
  //
  // TODO(user): Do more extensive experiment. Remove the second approach as
  // it is more time consuming? or identify when it make sense. Note that the
  // first approach also allows to use "incomplete" at least one between arcs.
  if (!auto_detect_clauses &&
      clauses->AllClausesInCreationOrder().size() < 1e6) {
    // TODO(user): This does not take into account clause of size 2 since they
    // are stored in the BinaryImplicationGraph instead. Some ideas specific
    // to size 2:
    // - There can be a lot of such clauses, but it might be nice to consider
    //   them. we need to experiments.
    // - The automatic clause detection might be a better approach and it
    //   could be combined with probing.
    for (const SatClause* clause : clauses->AllClausesInCreationOrder()) {
      if (time_limit->LimitReached()) return num_added_constraints;
      if (solver->ModelIsUnsat()) return num_added_constraints;
      num_added_constraints += AddGreaterThanAtLeastOneOfConstraintsFromClause(
          clause->AsSpan(), model);
    }

    // It is common that there is only two alternatives to push a variable.
    // In this case, our presolve most likely made sure that the two are
    // controlled by a single Boolean. This allows to detect this and add the
    // appropriate greater than at least one of.
    const int num_booleans = solver->NumVariables();
    if (num_booleans < 1e6) {
      for (int i = 0; i < num_booleans; ++i) {
        if (time_limit->LimitReached()) return num_added_constraints;
        if (solver->ModelIsUnsat()) return num_added_constraints;
        num_added_constraints +=
            AddGreaterThanAtLeastOneOfConstraintsFromClause(
                {Literal(BooleanVariable(i), true),
                 Literal(BooleanVariable(i), false)},
                model);
      }
    }

  } else {
    num_added_constraints +=
        AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(model);
  }

  if (num_added_constraints > 0) {
    SOLVER_LOG(logger, "[Precedences] Added ", num_added_constraints,
               " GreaterThanAtLeastOneOf() constraints.");
  }

  return num_added_constraints;
}

}  // namespace sat
}  // namespace operations_research
