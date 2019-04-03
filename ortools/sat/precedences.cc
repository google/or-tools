// Copyright 2010-2018 Google LLC
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

#include <algorithm>
#include <memory>

#include "ortools/base/cleanup.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_constraints.h"

namespace operations_research {
namespace sat {

namespace {

void AppendLowerBoundReasonIfValid(IntegerVariable var,
                                   const IntegerTrail& i_trail,
                                   std::vector<IntegerLiteral>* reason) {
  if (var != kNoIntegerVariable) {
    reason->push_back(i_trail.LowerBoundAsLiteral(var));
  }
}

}  // namespace

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
        impacted_arcs_[arc.tail_var].push_back(arc_index);
      }
    }

    // Iterate again to check for a propagation and indirectly update
    // modified_vars_.
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (arc_counts_[arc_index] > 0) continue;
      const ArcInfo& arc = arcs_[arc_index];
      if (integer_trail_->IsCurrentlyIgnored(arc.head_var)) continue;
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
  if (propagation_trail_index_ == trail_->Index()) {
    DCHECK(NoPropagationLeft(*trail_));
  }

  // Propagate the presence literals of the arcs that can't be added.
  PropagateOptionalArcs(trail_);

  // Clean-up modified_vars_ to do as little as possible on the next call.
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  return true;
}

bool PrecedencesPropagator::PropagateOutgoingArcs(IntegerVariable var) {
  for (const ArcIndex arc_index : impacted_arcs_[var]) {
    const ArcInfo& arc = arcs_[arc_index];
    if (integer_trail_->IsCurrentlyIgnored(arc.head_var)) continue;
    const IntegerValue new_head_lb =
        integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc);
    if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
      if (!EnqueueAndCheck(arc, new_head_lb, trail_)) return false;
    }
  }
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

// Instead of simply sorting the IntegerPrecedences returned by .var,
// experiments showed that it is faster to regroup all the same .var "by hand"
// by first computing how many times they appear and then apply the sorting
// permutation.
void PrecedencesPropagator::ComputePrecedences(
    const std::vector<IntegerVariable>& vars,
    std::vector<IntegerPrecedences>* output) {
  tmp_sorted_vars_.clear();
  tmp_precedences_.clear();
  for (int index = 0; index < vars.size(); ++index) {
    const IntegerVariable var = vars[index];
    CHECK_NE(kNoIntegerVariable, var);
    if (var >= impacted_arcs_.size()) continue;
    for (const ArcIndex arc_index : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (integer_trail_->IsCurrentlyIgnored(arc.head_var)) continue;

      IntegerValue offset = arc.offset;
      if (arc.offset_var != kNoIntegerVariable) {
        offset += integer_trail_->LowerBound(arc.offset_var);
      }

      // TODO(user): it seems better to ignore negative min offset as we will
      // often have relation of the form interval_start >= interval_end -
      // offset, and such relation are usually not useful. Revisit this in case
      // we see problems where we can propagate more without this test.
      if (offset < 0) continue;

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
      tmp_precedences_.push_back(
          {index, arc.head_var, arc_index.value(), offset});
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
    if (degree > 1) {
      var_to_degree_[pair.var] = start;
      start += degree;
    } else {
      // Optimization: we remove degree one relations.
      var_to_degree_[pair.var] = -1;
    }
  }
  output->resize(start);
  for (const IntegerPrecedences& precedence : tmp_precedences_) {
    if (var_to_degree_[precedence.var] < 0) continue;
    (*output)[var_to_degree_[precedence.var]++] = precedence;
  }

  // Cleanup var_to_degree_, note that we don't need to clean
  // var_to_last_index_.
  for (const SortedVar pair : tmp_sorted_vars_) {
    var_to_degree_[pair.var] = 0;
  }
}

void PrecedencesPropagator::AddPrecedenceReason(
    int arc_index, IntegerValue min_offset,
    std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) const {
  const ArcInfo& arc = arcs_[ArcIndex(arc_index)];
  for (const Literal l : arc.presence_literals) {
    literal_reason->push_back(l.Negated());
  }
  if (arc.offset_var != kNoIntegerVariable) {
    // Reason for ArcOffset(arc) to be >= min_offset.
    integer_reason->push_back(IntegerLiteral::GreaterOrEqual(
        arc.offset_var, min_offset - arc.offset));
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
    var_to_degree_.resize(index + 1);
    var_to_last_index_.resize(index + 1);
  }
}

void PrecedencesPropagator::AddArc(
    IntegerVariable tail, IntegerVariable head, IntegerValue offset,
    IntegerVariable offset_var, absl::Span<const Literal> presence_literals) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  AdjustSizeFor(tail);
  AdjustSizeFor(head);
  if (offset_var != kNoIntegerVariable) AdjustSizeFor(offset_var);

  // This arc is present iff all the literals here are true.
  absl::InlinedVector<Literal, 6> enforcement_literals;
  {
    for (const Literal l : presence_literals) {
      enforcement_literals.push_back(l);
    }
    if (integer_trail_->IsOptional(tail)) {
      enforcement_literals.push_back(
          integer_trail_->IsIgnoredLiteral(tail).Negated());
    }
    if (integer_trail_->IsOptional(head)) {
      enforcement_literals.push_back(
          integer_trail_->IsIgnoredLiteral(head).Negated());
    }
    if (offset_var != kNoIntegerVariable &&
        integer_trail_->IsOptional(offset_var)) {
      enforcement_literals.push_back(
          integer_trail_->IsIgnoredLiteral(offset_var).Negated());
    }
    gtl::STLSortAndRemoveDuplicates(&enforcement_literals);
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

  if (head == tail) {
    // A self-arc is either plain SAT or plain UNSAT or it forces something on
    // the given offset_var or presence_literal_index. In any case it could be
    // presolved in something more efficent.
    VLOG(1) << "Self arc! This could be presolved. "
            << "var:" << tail << " offset:" << offset
            << " offset_var:" << offset_var
            << " conditioned_by:" << presence_literals;
  }

  // Remove the offset_var if it is fixed.
  // TODO(user): We should also handle the case where tail or head is fixed.
  if (offset_var != kNoIntegerVariable) {
    const IntegerValue lb = integer_trail_->LowerBound(offset_var);
    if (lb == integer_trail_->UpperBound(offset_var)) {
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
    // IntegerTrail in this class contructor.
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
    if (integer_trail_->IsOptional(a.head_var)) {
      // TODO(user): More generally, we can remove any literal that is implied
      // by to_remove.
      const Literal to_remove =
          integer_trail_->IsIgnoredLiteral(a.head_var).Negated();
      const auto it = std::find(presence_literals.begin(),
                                presence_literals.end(), to_remove);
      if (it != presence_literals.end()) presence_literals.erase(it);
    }

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
    arc_counts_.push_back(presence_literals.size());
  }
}

// TODO(user): On jobshop problems with a lot of tasks per machine (500), this
// takes up a big chunck of the running time even before we find a solution.
// This is because, for each lower bound changed, we inspect 500 arcs even
// though they will never be propagated because the other bound is still at the
// horizon. Find an even sparser algorithm?
void PrecedencesPropagator::PropagateOptionalArcs(Trail* trail) {
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= impacted_potential_arcs_.size()) break;

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

    if (!integer_trail_->IsOptional(arc.head_var)) {
      return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
    } else {
      CHECK(!integer_trail_->IsCurrentlyIgnored(arc.head_var));
      const Literal l = integer_trail_->IsIgnoredLiteral(arc.head_var);
      if (trail->Assignment().LiteralIsFalse(l)) {
        literal_reason_.push_back(l);
        return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
      } else {
        integer_trail_->EnqueueLiteral(l, literal_reason_, integer_reason_);
        return true;
      }
    }
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
      if (integer_trail_->IsCurrentlyIgnored(arc.head_var)) continue;
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

    // If the cycle happens to contain optional variable not yet ignored, then
    // it is not a conflict anymore, but we can infer that these variable must
    // all be ignored. This is because since we propagated them even if they
    // where not present for sure, their presence literal must form a cycle
    // together (i.e. they are all absent or present at the same time).
    if (integer_trail_->IsOptional(arc.head_var)) {
      must_be_all_true->push_back(
          integer_trail_->IsIgnoredLiteral(arc.head_var));
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
// still room for improvments.
bool PrecedencesPropagator::BellmanFordTarjan(Trail* trail) {
  const int num_nodes = impacted_arcs_.size();

  // These vector are reset by CleanUpMarkedArcsAndParents() so resize is ok.
  bf_can_be_skipped_.resize(num_nodes, false);
  bf_parent_arc_of_.resize(num_nodes, ArcIndex(-1));
  const auto cleanup =
      ::gtl::MakeCleanup([this]() { CleanUpMarkedArcsAndParents(); });

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

    const IntegerValue tail_lb =
        integer_trail_->LowerBound(IntegerVariable(node));
    for (const ArcIndex arc_index : impacted_arcs_[IntegerVariable(node)]) {
      const ArcInfo& arc = arcs_[arc_index];
      DCHECK_EQ(arc.tail_var, node);
      const IntegerValue candidate = tail_lb + ArcOffset(arc);
      if (candidate > integer_trail_->LowerBound(arc.head_var)) {
        if (integer_trail_->IsCurrentlyIgnored(arc.head_var)) continue;
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
        if (integer_trail_->LowerBound(arc.head_var) == candidate) {
          bf_parent_arc_of_[arc.head_var.value()] = arc_index;
          arcs_[arc_index].is_marked = true;
        } else {
          // We still unmark any previous dependency, since we have pushed the
          // value of arc.head_var further.
          bf_parent_arc_of_[arc.head_var.value()] = -1;
        }

        bf_can_be_skipped_[arc.head_var.value()] = false;
        if (!bf_in_queue_[arc.head_var.value()]) {
          bf_queue_.push_back(arc.head_var.value());
          bf_in_queue_[arc.head_var.value()] = true;
        }
      }
    }
  }
  return true;
}

int PrecedencesPropagator::AddGreaterThanAtLeastOneOfConstraintsFromClause(
    const absl::Span<const Literal> clause, Model* model) {
  CHECK_EQ(model->GetOrCreate<Trail>()->CurrentDecisionLevel(), 0);
  if (clause.size() < 2) return 0;

  // Collect all arcs impacted by this clause.
  std::vector<ArcInfo> infos;
  for (const Literal l : clause) {
    if (l.Index() >= literal_to_new_impacted_arcs_.size()) continue;
    for (const ArcIndex arc_index : literal_to_new_impacted_arcs_[l.Index()]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (arc.presence_literals.size() != 1) continue;

      // TODO(user): Support variable offset.
      if (arc.offset_var != kNoIntegerVariable) continue;
      infos.push_back(arc);
    }
  }
  if (infos.size() <= 1) return 0;

  // Stable sort by head_var so that for a same head_var, the entry are sorted
  // by Literal as they appear in clause.
  std::stable_sort(infos.begin(), infos.end(),
                   [](const ArcInfo& a, const ArcInfo& b) {
                     return a.head_var < b.head_var;
                   });

  // We process ArcInfo with the same head_var toghether.
  int num_added_constraints = 0;
  auto* solver = model->GetOrCreate<SatSolver>();
  for (int i = 0; i < infos.size();) {
    const int start = i;
    const IntegerVariable head_var = infos[start].head_var;
    for (i++; i < infos.size() && infos[i].head_var == head_var; ++i) {
    }
    const absl::Span<ArcInfo> arcs(&infos[start], i - start);

    // Skip single arcs since it will already be fully propagated.
    if (arcs.size() < 2) continue;

    // Heuristic. Look for full or almost full clauses. We could add
    // GreaterThanAtLeastOneOf() with more enforcement literals. TODO(user):
    // experiments.
    if (arcs.size() + 1 < clause.size()) continue;

    std::vector<IntegerVariable> vars;
    std::vector<IntegerValue> offsets;
    std::vector<Literal> selectors;
    std::vector<Literal> enforcements;

    int j = 0;
    for (const Literal l : clause) {
      bool added = false;
      for (; j < arcs.size() && l == arcs[j].presence_literals.front(); ++j) {
        added = true;
        vars.push_back(arcs[j].tail_var);
        offsets.push_back(arcs[j].offset);

        // Note that duplicate selector are supported.
        //
        // TODO(user): If we support variable offset, we should regroup the arcs
        // into one (tail + offset <= head) though, instead of having too
        // identical entries.
        selectors.push_back(l);
      }
      if (!added) {
        enforcements.push_back(l.Negated());
      }
    }

    // No point adding a constraint if there is not at least two different
    // literals in selectors.
    if (enforcements.size() + 1 == clause.size()) continue;

    ++num_added_constraints;
    model->Add(GreaterThanAtLeastOneOf(head_var, vars, offsets, selectors,
                                       enforcements));
    if (!solver->FinishPropagation()) return num_added_constraints;
  }
  return num_added_constraints;
}

int PrecedencesPropagator::
    AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(Model* model) {
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();

  // Fill the set of incoming conditional arcs for each variables.
  gtl::ITIVector<IntegerVariable, std::vector<ArcIndex>> incoming_arcs_;
  for (ArcIndex arc_index(0); arc_index < arcs_.size(); ++arc_index) {
    const ArcInfo& arc = arcs_[arc_index];

    // Only keep arc that have a fixed offset and a single presence_literals.
    if (arc.offset_var != kNoIntegerVariable) continue;
    if (arc.tail_var == arc.head_var) continue;
    if (arc.presence_literals.size() != 1) continue;

    if (arc.head_var >= incoming_arcs_.size()) {
      incoming_arcs_.resize(arc.head_var.value() + 1);
    }
    incoming_arcs_[arc.head_var].push_back(arc_index);
  }

  int num_added_constraints = 0;
  for (IntegerVariable target(0); target < incoming_arcs_.size(); ++target) {
    if (incoming_arcs_[target].size() <= 1) continue;
    if (time_limit->LimitReached()) return num_added_constraints;

    // Detect set of incoming arcs for which at least one must be present.
    // TODO(user): Find more than one disjoint set of incoming arcs.
    // TODO(user): call MinimizeCoreWithPropagation() on the clause.
    solver->Backtrack(0);
    if (solver->IsModelUnsat()) return num_added_constraints;
    std::vector<Literal> clause;
    for (const ArcIndex arc_index : incoming_arcs_[target]) {
      const Literal literal = arcs_[arc_index].presence_literals.front();
      if (solver->Assignment().LiteralIsFalse(literal)) continue;

      const int old_level = solver->CurrentDecisionLevel();
      solver->EnqueueDecisionAndBacktrackOnConflict(literal.Negated());
      if (solver->IsModelUnsat()) return num_added_constraints;
      const int new_level = solver->CurrentDecisionLevel();
      if (new_level <= old_level) {
        clause = solver->GetLastIncompatibleDecisions();
        break;
      }
    }
    solver->Backtrack(0);

    if (clause.size() > 1) {
      // Extract the set of arc for which at least one must be present.
      const std::set<Literal> clause_set(clause.begin(), clause.end());
      std::vector<ArcIndex> arcs_in_clause;
      for (const ArcIndex arc_index : incoming_arcs_[target]) {
        const Literal literal(arcs_[arc_index].presence_literals.front());
        if (gtl::ContainsKey(clause_set, literal.Negated())) {
          arcs_in_clause.push_back(arc_index);
        }
      }

      VLOG(2) << arcs_in_clause.size() << "/" << incoming_arcs_[target].size();

      ++num_added_constraints;
      std::vector<IntegerVariable> vars;
      std::vector<IntegerValue> offsets;
      std::vector<Literal> selectors;
      for (const ArcIndex a : arcs_in_clause) {
        vars.push_back(arcs_[a].tail_var);
        offsets.push_back(arcs_[a].offset);
        selectors.push_back(Literal(arcs_[a].presence_literals.front()));
      }
      model->Add(GreaterThanAtLeastOneOf(target, vars, offsets, selectors));
      if (!solver->FinishPropagation()) return num_added_constraints;
    }
  }

  return num_added_constraints;
}

int PrecedencesPropagator::AddGreaterThanAtLeastOneOfConstraints(Model* model) {
  VLOG(1) << "Detecting GreaterThanAtLeastOneOf() constraints...";
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* clauses = model->GetOrCreate<LiteralWatchers>();
  int num_added_constraints = 0;

  // We have two possible approaches. For now, we prefer the first one except if
  // there is too many clauses in the problem.
  //
  // TODO(user): Do more extensive experiment. Remove the second approach as
  // it is more time consuming? or identify when it make sense. Note that the
  // first approach also allows to use "incomplete" at least one between arcs.
  if (clauses->AllClausesInCreationOrder().size() < 1e6) {
    // TODO(user): This does not take into account clause of size 2 since they
    // are stored in the BinaryImplicationGraph instead. Some ideas specific
    // to size 2:
    // - There can be a lot of such clauses, but it might be nice to consider
    //   them. we need to experiments.
    // - The automatic clause detection might be a better approach and it
    //   could be combined with probing.
    for (const SatClause* clause : clauses->AllClausesInCreationOrder()) {
      if (time_limit->LimitReached()) return num_added_constraints;
      if (solver->IsModelUnsat()) return num_added_constraints;
      num_added_constraints += AddGreaterThanAtLeastOneOfConstraintsFromClause(
          clause->AsSpan(), model);
    }
  } else {
    num_added_constraints +=
        AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(model);
  }

  VLOG(1) << "Added " << num_added_constraints
          << " GreaterThanAtLeastOneOf() constraints.";
  return num_added_constraints;
}

}  // namespace sat
}  // namespace operations_research
