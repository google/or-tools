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

#include "sat/precedences.h"

#include "base/cleanup.h"
#include "base/stl_util.h"

namespace operations_research {
namespace sat {

namespace {

bool IsInvalidOrTrue(LiteralIndex index, const Trail& trail) {
  return index == kNoLiteralIndex ||
         trail.Assignment().LiteralIsTrue(Literal(index));
}

void AppendNegationIfValid(LiteralIndex i, std::vector<Literal>* reason) {
  if (i != kNoLiteralIndex) reason->push_back(Literal(i).Negated());
}

void AppendNegationIfValidAndAssigned(LiteralIndex i, const Trail& trail,
                                      std::vector<Literal>* reason) {
  if (i != kNoLiteralIndex) {
    DCHECK(!trail.Assignment().LiteralIsFalse(Literal(i)));
    if (trail.Assignment().LiteralIsTrue(Literal(i))) {
      reason->push_back(Literal(i).Negated());
    }
  }
}

void AppendLowerBoundReasonIfValid(IntegerVariable var,
                                   const IntegerTrail& i_trail,
                                   std::vector<IntegerLiteral>* reason) {
  if (var != kNoIntegerVariable) {
    reason->push_back(i_trail.LowerBoundAsLiteral(var));
  }
}

}  // namespace

bool PrecedencesPropagator::Propagate(Trail* trail) {
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];

    if (literal.Index() < potential_nodes_.size()) {
      // We need to mark these nodes since they are now "present".
      for (const int node : potential_nodes_[literal.Index()]) {
        const IntegerVariable i(node);
        modified_vars_.Set(i);
        modified_vars_.Set(NegationOf(i));
      }
    }

    if (literal.Index() >= potential_arcs_.size()) continue;

    // IMPORTANT: Because of the way Untrail() work, we need to add all the
    // potential arcs before we can abort. It is why we iterate twice here.
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      impacted_arcs_[arc.tail_var].push_back(arc_index);
      if (arc.offset_var != kNoIntegerVariable) {
        impacted_arcs_[arc.offset_var].push_back(arc_index);
      }
    }

    // Iterate again to check for a propagation and indirectly update
    // modified_vars_.
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (!ArcShouldPropagate(arc, *trail)) continue;
      const IntegerValue new_head_lb =
          CapAdd(integer_trail_->LowerBound(arc.tail_var), ArcOffset(arc));
      if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
        if (!EnqueueAndCheck(arc, new_head_lb, trail)) return false;
      }
      PropagateMaxOffsetIfNeeded(arc, trail);
    }
  }

  // Do the actual propagation of the IntegerVariable bounds.
  InitializeBFQueueWithModifiedNodes();
  if (!BellmanFordTarjan(trail)) return false;
  DCHECK(NoPropagationLeft(*trail));

  // Propagate the presence literal of the arcs that can't be added.
  PropagateOptionalArcs(trail);

  // Clean-up modified_vars_ to do as little as possible on the next call.
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  return true;
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
    if (literal.Index() >= potential_arcs_.size()) continue;
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      impacted_arcs_[arc.tail_var].pop_back();
      if (arc.offset_var != kNoIntegerVariable) {
        impacted_arcs_[arc.offset_var].pop_back();
      }
    }
  }
}

// Instead of simply sorting the IntegerPrecedences returned by .var,
// experiments showed that it is faster to regroup all the same .var "by hand"
// by first computing how many times they appear and then apply the sorting
// permutation.
void PrecedencesPropagator::ComputePrecedences(
    const std::vector<IntegerVariable>& vars, const std::vector<bool>& to_consider,
    std::vector<IntegerPrecedences>* output) {
  tmp_sorted_vars_.clear();
  tmp_precedences_.clear();
  for (int index = 0; index < vars.size(); ++index) {
    const IntegerVariable var = vars[index];
    CHECK_NE(kNoIntegerVariable, var);
    if (!to_consider[index] || var >= impacted_arcs_.size()) continue;
    for (const int i : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[i];
      if (arc.tail_var != var) continue;  // Skip variable duration.

      // We don't handle offsets and just care about "is before or at", so we
      // skip negative offsets and just treat a positive one as zero. Because of
      // this, we may miss some propagation opportunities.
      if (arc.offset < 0) continue;
      if (var_to_degree_[arc.head_var] == 0) {
        tmp_sorted_vars_.push_back(
            {arc.head_var, integer_trail_->LowerBound(arc.head_var)});
      } else {
        // This "seen" mechanism is needed because we may have multi-arc and we
        // don't want any duplicates in the "is_before" relation. Note that it
        // works because var_to_last_index_ is reset by the var_to_degree_ == 0
        // case.
        if (var_to_last_index_[arc.head_var] == index) continue;
      }
      var_to_last_index_[arc.head_var] = index;
      var_to_degree_[arc.head_var]++;
      tmp_precedences_.push_back({index, arc.head_var, arc.presence_l});
    }
  }

  // This order is a topological order for the precedences relation order
  // provided that all the offset between the involved IntegerVariable are
  // positive.
  //
  // TODO(user): use an order that is always topological? This is not clear
  // since it may be slower to compute and not worth it because the order below
  // is more natural and may work better.
  std::sort(tmp_sorted_vars_.begin(), tmp_sorted_vars_.end());

  // Permute tmp_precedences_ into the output to put it in the correct order.
  // For that we transform var_to_degree_ to point to the first position of
  // each lbvar in the output vector.
  int start = 0;
  for (const SortedVar pair : tmp_sorted_vars_) {
    const int degree = var_to_degree_[pair.var];
    var_to_degree_[pair.var] = start;
    start += degree;
  }
  output->resize(start);
  for (const IntegerPrecedences& precedence : tmp_precedences_) {
    (*output)[var_to_degree_[precedence.var]++] = precedence;
  }

  // Cleanup var_to_degree_, note that we don't need to clean
  // var_to_last_index_.
  for (const SortedVar pair : tmp_sorted_vars_) {
    var_to_degree_[pair.var] = 0;
  }
}

void PrecedencesPropagator::AdjustSizeFor(IntegerVariable i) {
  const int index = std::max(i.value(), NegationOf(i).value());
  if (index >= impacted_arcs_.size()) {
    modified_vars_.Resize(IntegerVariable(index) + 1);
    for (IntegerVariable var(impacted_arcs_.size()); var <= index; ++var) {
      watcher_->WatchLowerBound(var, watcher_id_);
    }
    optional_literals_.resize(index + 1, kNoLiteralIndex);
    impacted_arcs_.resize(index + 1);
    impacted_potential_arcs_.resize(index + 1);
    var_to_degree_.resize(index + 1);
    var_to_last_index_.resize(index + 1);
  }
}

void PrecedencesPropagator::MarkIntegerVariableAsOptional(IntegerVariable i,
                                                          Literal is_present) {
  AdjustSizeFor(i);
  optional_literals_[i] = is_present.Index();
  optional_literals_[NegationOf(i)] = is_present.Index();
  if (is_present.Index() >= potential_nodes_.size()) {
    potential_nodes_.resize(is_present.Index().value() + 1);
  }
  potential_nodes_[is_present.Index()].push_back(i.value());
}

void PrecedencesPropagator::AddArc(IntegerVariable tail, IntegerVariable head,
                                   IntegerValue offset,
                                   IntegerVariable offset_var, LiteralIndex l) {
  AdjustSizeFor(tail);
  AdjustSizeFor(head);
  if (offset_var != kNoIntegerVariable) AdjustSizeFor(offset_var);

  for (const bool forward : {true, false}) {
    const IntegerVariable tail_var = forward ? tail : NegationOf(head);
    const IntegerVariable head_var = forward ? head : NegationOf(tail);

    // Since we add a new arc, we will need to consider its tail during the next
    // propagation.
    //
    // TODO(user): Adding arcs and then calling Untrail() before Propagate()
    // will cause this mecanism to break. Find a more robust implementation.
    modified_vars_.Set(tail_var);

    const int arc_index = arcs_.size();
    if (l == kNoLiteralIndex) {
      impacted_arcs_[tail_var].push_back(arc_index);
      if (offset_var != kNoIntegerVariable) {
        impacted_arcs_[offset_var].push_back(arc_index);
      }
    } else {
      impacted_potential_arcs_[tail_var].push_back(arc_index);
      if (offset_var != kNoIntegerVariable) {
        impacted_potential_arcs_[offset_var].push_back(arc_index);
      }

      if (l.value() >= potential_arcs_.size()) {
        potential_arcs_.resize(l.value() + 1);
      }
      potential_arcs_[l].push_back(arc_index);
    }
    arcs_.push_back({tail_var, head_var, offset, offset_var, l});
  }
}

bool PrecedencesPropagator::ArcShouldPropagate(const ArcInfo& arc,
                                               const Trail& trail) const {
  const LiteralIndex index = OptionalLiteralOf(arc.tail_var);
  if (index == kNoLiteralIndex) return true;
  if (trail.Assignment().LiteralIsFalse(Literal(index))) return false;
  return trail.Assignment().LiteralIsTrue(Literal(index)) ||
         index == OptionalLiteralOf(arc.head_var);
}

// TODO(user): On jobshop problems with a lot of tasks per machine (500), this
// takes up a big chunck of the running time even before we find a solution.
// This is because, for each lower bound changed, we inspect 500 arcs even
// though they will never be propagated because the other bound is still at the
// horizon. Find an even sparser algorithm?
void PrecedencesPropagator::PropagateOptionalArcs(Trail* trail) {
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= impacted_potential_arcs_.size()) continue;

    // We can't deduce anything if this is the case.
    if (!IsInvalidOrTrue(OptionalLiteralOf(var), *trail)) continue;

    // Note that we can currently do the same computation up to 3 times:
    // - if tail_var changed
    // - if OtherLbVar(head_var) changed, we will process the "reverse" arc,
    //   but it will lead to the same computation and the same presence literal
    //   to be propagated.
    // - if offset_var changed.
    //
    // Note(user): I tried another option, but it was slower:
    // - keep the unique and even (i.e direct arc) arc_index.
    // - Only call PropagateArcIfNeeded() on them.
    for (const int arc_index : impacted_potential_arcs_[var]) {
      const ArcInfo& arc = arcs_[arc_index];
      const Literal is_present(arc.presence_l);
      if (trail->Assignment().VariableIsAssigned(is_present.Variable())) {
        continue;
      }

      // We can't deduce anything if the head node is optional and unassigned.
      if (!IsInvalidOrTrue(OptionalLiteralOf(arc.head_var), *trail)) {
        continue;
      }

      // We want the other bound of head to test infeasibility of the head
      // IntegerVariable.
      const IntegerValue tail_lb = integer_trail_->LowerBound(arc.tail_var);
      const IntegerValue head_ub = integer_trail_->UpperBound(arc.head_var);
      if (CapAdd(tail_lb, ArcOffset(arc)) > head_ub) {
        std::vector<Literal>* literal_reason;
        std::vector<IntegerLiteral>* integer_reason;
        integer_trail_->EnqueueLiteral(is_present.Negated(), &literal_reason,
                                       &integer_reason);
        integer_reason->push_back(
            integer_trail_->LowerBoundAsLiteral(arc.tail_var));
        integer_reason->push_back(
            integer_trail_->UpperBoundAsLiteral(arc.head_var));
        AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                      integer_reason);
        AppendNegationIfValid(OptionalLiteralOf(arc.tail_var), literal_reason);
        AppendNegationIfValid(OptionalLiteralOf(arc.head_var), literal_reason);
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
  DCHECK_GT(new_head_lb, integer_trail_->LowerBound(arc.head_var));

  // Compute the reason for new_head_lb.
  literal_reason_.clear();
  AppendNegationIfValid(arc.presence_l, &literal_reason_);
  AppendNegationIfValidAndAssigned(OptionalLiteralOf(arc.tail_var), *trail,
                                   &literal_reason_);

  integer_reason_.clear();
  integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(arc.tail_var));
  AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                &integer_reason_);

  // Propagate.
  //
  // Subtle: we need Enqueue() to still propagate a bound of an optional
  // variable that we know cannot be present. We need this so that:
  //   1. Our sparse cleaning algo in CleanUpMarkedArcsAndParents() works.
  //   2. NoPropagationLeft() is happy too.
  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(arc.head_var, new_head_lb),
      literal_reason_, integer_reason_);
}

bool PrecedencesPropagator::PropagateMaxOffsetIfNeeded(const ArcInfo& arc,
                                                       Trail* trail) {
  if (arc.offset_var == kNoIntegerVariable) return false;
  if (!IsInvalidOrTrue(OptionalLiteralOf(arc.head_var), *trail)) return false;

  const IntegerValue max_duration =
      CapSub(integer_trail_->UpperBound(arc.head_var),
             integer_trail_->LowerBound(arc.tail_var));
  if (max_duration < integer_trail_->UpperBound(arc.offset_var)) {
    literal_reason_.clear();
    AppendNegationIfValid(arc.presence_l, &literal_reason_);
    AppendNegationIfValid(OptionalLiteralOf(arc.tail_var), &literal_reason_);
    integer_reason_.clear();
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(arc.tail_var));
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(arc.head_var));
    return integer_trail_->Enqueue(
        IntegerLiteral::LowerOrEqual(arc.offset_var, max_duration),
        literal_reason_, integer_reason_);
  }
  return false;
}

bool PrecedencesPropagator::NoPropagationLeft(const Trail& trail) const {
  const int num_nodes = impacted_arcs_.size();
  for (IntegerVariable var(0); var < num_nodes; ++var) {
    for (const int index : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[index];
      if (!ArcShouldPropagate(arc, trail)) continue;
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
    const int parent_arc_index = bf_parent_arc_of_[var.value()];
    if (parent_arc_index != -1) {
      arcs_[parent_arc_index].is_marked = false;
      bf_parent_arc_of_[var.value()] = -1;
      bf_can_be_skipped_[var.value()] = false;
    }
  }
  DCHECK(std::none_of(bf_parent_arc_of_.begin(), bf_parent_arc_of_.end(),
                      [](int v) { return v != -1; }));
  DCHECK(std::none_of(bf_can_be_skipped_.begin(), bf_can_be_skipped_.end(),
                      [](bool v) { return v; }));
}

bool PrecedencesPropagator::DisassembleSubtree(int source, int target,
                                               std::vector<bool>* can_be_skipped) {
  // Note that we explore a tree, so we can do it in any order, and the one
  // below seems to be the fastest.
  tmp_vector_.clear();
  tmp_vector_.push_back(source);
  while (!tmp_vector_.empty()) {
    const int tail = tmp_vector_.back();
    tmp_vector_.pop_back();
    for (const int arc_index : impacted_arcs_[IntegerVariable(tail)]) {
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

// Note(user): because of our "special" graph, we can't just follow the
// bf_parent_arc_of_[] to reconstruct the cycle, this is because given an arc,
// we don't know if it was impacted by its tail or by it offset_var (if not
// equal to kNoIntegerVariable).
//
// Our solution is to walk again the forward graph using a DFS to reconstruct
// the arcs that form a positive cycle.
void PrecedencesPropagator::ReportPositiveCycle(int first_arc, Trail* trail) {
  // TODO(user): I am not sure we have a theoretical guarantee than the
  // set of arcs appearing in bf_parent_arc_of_[] form a tree here because
  // we consider all of them, not just the non-maked one. Because of that,
  // for now we use an extra vector to be on the safe side.
  std::vector<bool> in_queue(impacted_arcs_.size(), false);

  const int first_node = arcs_[first_arc].head_var.value();
  tmp_vector_.clear();
  tmp_vector_.push_back(first_node);
  in_queue[first_node] = true;
  std::vector<int> arc_queue{first_arc};
  std::vector<int> arc_on_cycle{first_arc};
  bool found = false;
  while (!found && !tmp_vector_.empty()) {
    const int node = tmp_vector_.back();
    const int incoming_arc = arc_queue.back();
    if (node == -1) {
      // We are coming back up in the tree.
      tmp_vector_.pop_back();
      arc_queue.pop_back();
      arc_on_cycle.pop_back();
      continue;
    }

    // We are going down in the tree.
    tmp_vector_.back() = -1;  // Mark as explored.
    arc_on_cycle.push_back(incoming_arc);
    for (const int arc_index : impacted_arcs_[IntegerVariable(node)]) {
      if (arc_index == first_arc) {
        // The cycle is complete.
        found = true;
        break;
      }
      const ArcInfo& arc = arcs_[arc_index];

      // We already cleared is_marked, so we use a slower detection of the
      // arc in the tree. Note that this code is called a lot less often than
      // DisassembleSubtree(), so this is probably good enough.
      const int head_node = arc.head_var.value();
      if (!in_queue[head_node] && bf_parent_arc_of_[head_node] == arc_index) {
        tmp_vector_.push_back(head_node);
        arc_queue.push_back(arc_index);
        in_queue[head_node] = true;
      }
    }
  }
  CHECK(found);
  CHECK_EQ(arc_on_cycle.front(), first_arc);

  // Report the positive cycle.
  std::vector<Literal>* conflict = trail->MutableConflict();
  integer_reason_.clear();
  conflict->clear();
  IntegerValue sum(0);
  for (const int arc_index : arc_on_cycle) {
    const ArcInfo& arc = arcs_[arc_index];
    sum += ArcOffset(arc);
    AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                  &integer_reason_);
    AppendNegationIfValid(arc.presence_l, conflict);

    // On a cycle, all the optional integer should necessarily be present.
    CHECK(IsInvalidOrTrue(OptionalLiteralOf(arc.tail_var), *trail));
    AppendNegationIfValid(OptionalLiteralOf(arc.tail_var), conflict);
  }
  integer_trail_->MergeReasonInto(integer_reason_, conflict);

  // TODO(user): what if the sum overflow? this is just a check so I guess
  // we don't really care, but fix the issue.
  CHECK_GT(sum, 0);

  // We don't want any duplicates.
  // TODO(user): I think we could handle them, so maybe this is not needed.
  STLSortAndRemoveDuplicates(conflict);
}

// Note that in our settings it is important to use an algorithm that tries to
// minimize the number of integer_trail_->Enqueue() as much as possible.
//
// TODO(user): The current algorithm is quite efficient, but there is probably
// still room for improvments.
bool PrecedencesPropagator::BellmanFordTarjan(Trail* trail) {
  const int num_nodes = impacted_arcs_.size();

  // These vector are reset by CleanUpMarkedArcsAndParents() so resize is ok.
  bf_can_be_skipped_.resize(num_nodes, false);
  bf_parent_arc_of_.resize(num_nodes, -1);
  const auto cleanup =
      ::operations_research::util::MakeCleanup([this]() { CleanUpMarkedArcsAndParents(); });

  // The queue initialization is done by InitializeBFQueueWithModifiedNodes().
  while (!bf_queue_.empty()) {
    const int node = bf_queue_.front();
    bf_queue_.pop_front();
    bf_in_queue_[node] = false;

    // TODO(user): we don't need bf_can_be_skipped_ since we can detect this
    // if this node has a parent arc which is not marked. Investigate if it is
    // faster without the std::vector<bool>.
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

    // Note that while this look like a graph traversal, it is slightly trickier
    // because we may iterate on arcs without the same tail here (when the node
    // appear as an offset_var). The algo should still work fine though.
    for (const int arc_index : impacted_arcs_[IntegerVariable(node)]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (!ArcShouldPropagate(arc, *trail)) continue;

      if (PropagateMaxOffsetIfNeeded(arc, trail)) {
        const IntegerVariable minus_offset = NegationOf(arc.offset_var);

        // TODO(user): We currently don't have any cycle detection here.
        bf_can_be_skipped_[minus_offset.value()] = false;
        if (!bf_in_queue_[minus_offset.value()]) {
          bf_queue_.push_back(minus_offset.value());
          bf_in_queue_[minus_offset.value()] = true;
        }
      }

      const IntegerValue candidate =
          CapAdd(integer_trail_->LowerBound(arc.tail_var), ArcOffset(arc));
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
          ReportPositiveCycle(arc_index, trail);
          return false;
        }

        // We need to enforce the invariant that only the arc_index in
        // bf_parent_arc_of_[] are marked (but not necessarily all of them since
        // we unmark some in DisassembleSubtree()).
        if (bf_parent_arc_of_[arc.head_var.value()] != -1) {
          arcs_[bf_parent_arc_of_[arc.head_var.value()]].is_marked = false;
        }
        bf_parent_arc_of_[arc.head_var.value()] = arc_index;
        bf_can_be_skipped_[arc.head_var.value()] = false;
        arcs_[arc_index].is_marked = true;

        if (!bf_in_queue_[arc.head_var.value()]) {
          bf_queue_.push_back(arc.head_var.value());
          bf_in_queue_[arc.head_var.value()] = true;
        }
      }
    }
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
