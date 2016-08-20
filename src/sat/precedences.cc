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

void AppendValueIfValid(LbVar var, const IntegerTrail& i_trail,
                        std::vector<IntegerLiteral>* reason) {
  if (var != kNoLbVar) reason->push_back(i_trail.ValueAsLiteral(var));
}

}  // namespace

bool PrecedencesPropagator::Propagate(Trail* trail) {
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];

    if (literal.Index() < potential_nodes_.size()) {
      // We need to mark these nodes since they are now "present".
      for (const int node : potential_nodes_[literal.Index()]) {
        const IntegerVariable i(node);
        modified_vars_.Set(LbVarOf(i));
        modified_vars_.Set(MinusUbVarOf(i));
      }
    }

    if (literal.Index() >= potential_arcs_.size()) continue;

    // IMPORTANT: Because of the way Untrail() work, we need to add all the
    // potential arcs before we can abort. It is why we iterate twice here.
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      impacted_arcs_[arc.tail_var].push_back(arc_index);
      if (arc.offset_var != kNoLbVar) {
        impacted_arcs_[arc.offset_var].push_back(arc_index);
      }
    }

    // Iterate again to check for a propagation and indirectly update
    // modified_vars_.
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (!IsInvalidOrTrue(OptionalLiteralOf(arc.tail_var), *trail)) {
        continue;
      }
      const int new_head_lb =
          integer_trail_->Value(arc.tail_var) + ArcOffset(arc);
      if (new_head_lb > integer_trail_->Value(arc.head_var)) {
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
  modified_vars_.ClearAndResize(LbVar(integer_trail_->NumLbVars()));
  return true;
}

void PrecedencesPropagator::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ > trail_index) {
    // This means that we already propagated all there is to propagate
    // at the level trail_index, so we can safely clear modified_vars_ in case
    // it wasn't already done.
    modified_vars_.ClearAndResize(LbVar(integer_trail_->NumLbVars()));
  }
  while (propagation_trail_index_ > trail_index) {
    const Literal literal = trail[--propagation_trail_index_];
    if (literal.Index() >= potential_arcs_.size()) continue;
    for (const int arc_index : potential_arcs_[literal.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      impacted_arcs_[arc.tail_var].pop_back();
      if (arc.offset_var != kNoLbVar) {
        impacted_arcs_[arc.offset_var].pop_back();
      }
    }
  }
}

// Instead of simply sorting the LbVarPrecedences returned by .var, experiments
// showed that it is faster to regroup all the same .var "by hand" by first
// computing how many times they appear and then apply the sorting permutation.
void PrecedencesPropagator::ComputePrecedences(
    const std::vector<LbVar>& to_consider, std::vector<LbVarPrecedences>* output) {
  tmp_sorted_lbvars_.clear();
  tmp_precedences_.clear();
  for (int index = 0; index < to_consider.size(); ++index) {
    const LbVar var = to_consider[index];
    if (var == kNoLbVar || var >= impacted_arcs_.size()) continue;
    for (const int i : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[i];
      if (arc.tail_var != var) continue;  // Skip variable duration.

      // We don't handle offsets and just care about "is before or at", so we
      // skip negative offsets and just treat a positive one as zero. Because of
      // this, we may miss some propagation opportunities.
      if (arc.offset < 0) continue;
      if (lbvar_to_degree_[arc.head_var] == 0) {
        tmp_sorted_lbvars_.push_back(
            {arc.head_var, integer_trail_->ValueAsLiteral(arc.head_var).bound});
      } else {
        // This "seen" mechanism is needed because we may have multi-arc and we
        // don't want any duplicates in the "is_before" relation. Note that it
        // works because lbvar_to_last_index_ is reset by the
        // lbvar_to_degree_ == 0 case.
        if (lbvar_to_last_index_[arc.head_var] == index) continue;
      }
      lbvar_to_last_index_[arc.head_var] = index;
      lbvar_to_degree_[arc.head_var]++;
      tmp_precedences_.push_back({index, arc.head_var, arc.presence_l});
    }
  }

  // This order is a topological order for the precedences relation order
  // provided that all the offset between the involved LbVar are positive.
  //
  // TODO(user): use an order that is always topological? This is not clear
  // since it may be slower to compute and not worth it because the order below
  // is more natural and may work better.
  std::sort(tmp_sorted_lbvars_.begin(), tmp_sorted_lbvars_.end());

  // Permute tmp_precedences_ into the output to put it in the correct order.
  // For that we transform lbvar_to_degree_ to point to the first position of
  // each lbvar in the output vector.
  int start = 0;
  for (const SortedLbVar pair : tmp_sorted_lbvars_) {
    const int degree = lbvar_to_degree_[pair.var];
    lbvar_to_degree_[pair.var] = start;
    start += degree;
  }
  output->resize(start);
  for (const LbVarPrecedences& precedence : tmp_precedences_) {
    (*output)[lbvar_to_degree_[precedence.var]++] = precedence;
  }

  // Cleanup lbvar_to_degree_, note that we don't need to clean
  // lbvar_to_last_index_.
  for (const SortedLbVar pair : tmp_sorted_lbvars_) {
    lbvar_to_degree_[pair.var] = 0;
  }
}

void PrecedencesPropagator::AdjustSizeFor(IntegerVariable i) {
  if (i.value() >= optional_literals_.size()) {
    optional_literals_.resize(i.value() + 1, kNoLiteralIndex);
  }
  const int index = std::max(LbVarOf(i).value(), MinusUbVarOf(i).value());
  if (index >= impacted_arcs_.size()) {
    modified_vars_.Resize(LbVar(index) + 1);
    for (LbVar var(impacted_arcs_.size()); var <= index; ++var) {
      watcher_->WatchLbVar(var, watcher_id_);
    }
    impacted_arcs_.resize(index + 1);
    impacted_potential_arcs_.resize(index + 1);
    lbvar_to_degree_.resize(index + 1);
    lbvar_to_last_index_.resize(index + 1);
  }
}

void PrecedencesPropagator::MarkIntegerVariableAsOptional(IntegerVariable i,
                                                          Literal is_present) {
  AdjustSizeFor(i);
  optional_literals_[i] = is_present.Index();
  if (is_present.Index() >= potential_nodes_.size()) {
    potential_nodes_.resize(is_present.Index().value() + 1);
  }
  potential_nodes_[is_present.Index()].push_back(i.value());
}

void PrecedencesPropagator::AddArc(IntegerVariable tail, IntegerVariable head,
                                   int offset, LbVar offset_var,
                                   LiteralIndex l) {
  AdjustSizeFor(tail);
  AdjustSizeFor(head);
  if (offset_var != kNoLbVar) {
    AdjustSizeFor(IntegerVariableOf(LbVar(offset_var)));
  }
  for (const bool forward : {true, false}) {
    const LbVar tail_var = forward ? LbVarOf(tail) : MinusUbVarOf(head);
    const LbVar head_var = forward ? LbVarOf(head) : MinusUbVarOf(tail);

    // Since we add a new arc, we will need to consider its tail during the next
    // propagation.
    //
    // TODO(user): Adding arcs and then calling Untrail() before Propagate()
    // will cause this mecanism to break. Find a more robust implementation.
    modified_vars_.Set(tail_var);

    const int arc_index = arcs_.size();
    if (l == kNoLiteralIndex) {
      impacted_arcs_[tail_var].push_back(arc_index);
      if (offset_var != kNoLbVar) {
        impacted_arcs_[offset_var].push_back(arc_index);
      }
    } else {
      impacted_potential_arcs_[tail_var].push_back(arc_index);
      if (offset_var != kNoLbVar) {
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

// TODO(user): On jobshop problems with a lot of tasks per machine (500), this
// takes up a big chunck of the running time even before we find a solution.
// This is because, for each LbVar changed, we inspect 500 arcs even though they
// will never be propagated because the other bound is still at the horizon.
// Find and even sparser algorithm?
void PrecedencesPropagator::PropagateOptionalArcs(Trail* trail) {
  for (const LbVar var : modified_vars_.PositionsSetAtLeastOnce()) {
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
      const LbVar other_head_var = OtherLbVar(arc.head_var);
      const int tail_value = integer_trail_->Value(arc.tail_var);
      const int head_value = integer_trail_->Value(other_head_var);
      if (tail_value + ArcOffset(arc) > -head_value) {
        std::vector<Literal>* literal_reason;
        std::vector<IntegerLiteral>* integer_reason;
        integer_trail_->EnqueueLiteral(is_present.Negated(), &literal_reason,
                                       &integer_reason, trail);
        integer_reason->push_back(integer_trail_->ValueAsLiteral(arc.tail_var));
        integer_reason->push_back(
            integer_trail_->ValueAsLiteral(other_head_var));
        AppendValueIfValid(arc.offset_var, *integer_trail_, integer_reason);
        AppendNegationIfValid(
            optional_literals_[IntegerVariableOf(arc.tail_var)],
            literal_reason);
        AppendNegationIfValid(
            optional_literals_[IntegerVariableOf(arc.head_var)],
            literal_reason);
      }
    }
  }
}

int PrecedencesPropagator::ArcOffset(const ArcInfo& arc) const {
  return arc.offset + (arc.offset_var == kNoLbVar ? 0 : integer_trail_->Value(
                                                            arc.offset_var));
}

bool PrecedencesPropagator::EnqueueAndCheck(const ArcInfo& arc, int new_head_lb,
                                            Trail* trail) {
  DCHECK_GT(new_head_lb, integer_trail_->Value(arc.head_var));

  // Compute the reason for new_head_lb.
  literal_reason_.clear();
  AppendNegationIfValid(arc.presence_l, &literal_reason_);
  AppendNegationIfValid(optional_literals_[IntegerVariableOf(arc.tail_var)],
                        &literal_reason_);

  integer_reason_.clear();
  integer_reason_.push_back(integer_trail_->ValueAsLiteral(arc.tail_var));
  AppendValueIfValid(arc.offset_var, *integer_trail_, &integer_reason_);

  // Conflict?
  const LbVar other_head_var = OtherLbVar(arc.head_var);
  if (new_head_lb > -integer_trail_->Value(other_head_var)) {
    integer_reason_.push_back(integer_trail_->ValueAsLiteral(other_head_var));
    // This test if the head integer variable is either optional and present or
    // always present, so if this is true then we have a conflict.
    if (IsInvalidOrTrue(OptionalLiteralOf(arc.head_var), *trail)) {
      std::vector<Literal>* conflict = trail->MutableConflict();
      *conflict = literal_reason_;
      integer_trail_->MergeReasonInto(integer_reason_, conflict);
      return false;
    } else {
      // It is actually not a conflict if head is optional and not assigned.
      const Literal lit(optional_literals_[IntegerVariableOf(arc.head_var)]);
      if (!trail->Assignment().LiteralIsFalse(lit)) {
        // TODO(user): better interface.
        std::vector<Literal>* literal_reason_ptr;
        std::vector<IntegerLiteral>* integer_reason_ptr;
        integer_trail_->EnqueueLiteral(lit.Negated(), &literal_reason_ptr,
                                       &integer_reason_ptr, trail);
        *literal_reason_ptr = literal_reason_;
        integer_reason_ptr->clear();
        for (const IntegerLiteral l : integer_reason_) {
          integer_reason_ptr->push_back(l);
        }
      }
    }

    // TRICKY: It is not really needed, but we still enqueue the new bound in
    // this case so that:
    // 1. Our sparse cleaning algo in CleanUpMarkedArcsAndParents() works.
    // 2. NoPropagationLeft() is happy too.
  }

  // Propagate.
  integer_trail_->Enqueue(IntegerLiteral::FromLbVar(arc.head_var, new_head_lb),
                          literal_reason_, integer_reason_);
  return true;
}

bool PrecedencesPropagator::PropagateMaxOffsetIfNeeded(const ArcInfo& arc,
                                                       Trail* trail) {
  if (arc.offset_var == kNoLbVar) return false;
  if (!IsInvalidOrTrue(OptionalLiteralOf(arc.head_var), *trail)) return false;

  const LbVar other_head_var = OtherLbVar(arc.head_var);
  const int max_duration = -integer_trail_->Value(other_head_var) -
                           integer_trail_->Value(arc.tail_var);
  const LbVar offset_ub = OtherLbVar(LbVar(arc.offset_var));
  if (max_duration < -integer_trail_->Value(offset_ub)) {
    literal_reason_.clear();
    AppendNegationIfValid(arc.presence_l, &literal_reason_);
    AppendNegationIfValid(optional_literals_[IntegerVariableOf(arc.tail_var)],
                          &literal_reason_);
    integer_reason_.clear();
    integer_reason_.push_back(integer_trail_->ValueAsLiteral(arc.tail_var));
    integer_reason_.push_back(integer_trail_->ValueAsLiteral(other_head_var));
    integer_trail_->Enqueue(IntegerLiteral::FromLbVar(offset_ub, -max_duration),
                            literal_reason_, integer_reason_);
    return true;
  }
  return false;
}

bool PrecedencesPropagator::NoPropagationLeft(const Trail& trail) const {
  const int num_nodes = impacted_arcs_.size();
  for (LbVar var(0); var < num_nodes; ++var) {
    for (const int index : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[index];
      if (!IsInvalidOrTrue(OptionalLiteralOf(arc.tail_var), trail)) continue;
      if (integer_trail_->Value(arc.tail_var) + ArcOffset(arc) >
          integer_trail_->Value(arc.head_var)) {
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
  for (const LbVar var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= num_nodes) continue;
    bf_queue_.push_back(var.value());
    bf_in_queue_[var.value()] = true;
  }
}

void PrecedencesPropagator::CleanUpMarkedArcsAndParents() {
  // To be sparse, we use the fact that each node with a parent must be in
  // modified_vars_.
  const int num_nodes = impacted_arcs_.size();
  for (const LbVar var : modified_vars_.PositionsSetAtLeastOnce()) {
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
    for (const int arc_index : impacted_arcs_[LbVar(tail)]) {
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
// equal to kNoLbVar).
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
    for (const int arc_index : impacted_arcs_[LbVar(node)]) {
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
  int sum = 0;
  for (const int arc_index : arc_on_cycle) {
    const ArcInfo& arc = arcs_[arc_index];
    sum += ArcOffset(arc);
    AppendValueIfValid(arc.offset_var, *integer_trail_, &integer_reason_);
    AppendNegationIfValid(arc.presence_l, conflict);
    AppendNegationIfValid(optional_literals_[IntegerVariableOf(arc.tail_var)],
                          conflict);
  }
  integer_trail_->MergeReasonInto(integer_reason_, conflict);
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
    for (const int arc_index : impacted_arcs_[LbVar(node)]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (!IsInvalidOrTrue(OptionalLiteralOf(arc.tail_var), *trail)) continue;

      if (PropagateMaxOffsetIfNeeded(arc, trail)) {
        const LbVar offset_ub = OtherLbVar(LbVar(arc.offset_var));

        // TODO(user): We currently don't have any cycle detection here.
        bf_can_be_skipped_[offset_ub.value()] = false;
        if (!bf_in_queue_[offset_ub.value()]) {
          bf_queue_.push_back(offset_ub.value());
          bf_in_queue_[offset_ub.value()] = true;
        }
      }

      const int candidate =
          integer_trail_->Value(arc.tail_var) + ArcOffset(arc);
      if (candidate > integer_trail_->Value(arc.head_var)) {
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
