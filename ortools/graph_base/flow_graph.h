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

#ifndef UTIL_GRAPH_FLOW_GRAPH_H_
#define UTIL_GRAPH_FLOW_GRAPH_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph_base/graph.h"
#include "ortools/graph_base/iterators.h"

namespace util {

// Graph specialized for max-flow/min-cost-flow algorithms.
// It follows the util/graph/graph.h interface.
//
// For max-flow or min-cost-flow we need a directed graph where each arc from
// tail to head has a "reverse" arc from head to tail. In practice many input
// graphs already have such reverse arc and it can make a big difference just to
// reuse them.
//
// This is similar to ReverseArcStaticGraph but handle reverse arcs in a
// different way. Instead of always creating a NEW reverse arc for each arc of
// the input graph, this will detect if a reverse arc was already present in the
// input, and do not create a new one when this is the case. In the best case,
// this can gain a factor 2 in the final graph size, note however that the graph
// construction is slighlty slower because of this detection.
//
// A side effect of reusing reverse arc is also that we cannot anymore clearly
// separate the original arcs from the newly created one. So the algorithm needs
// to be able to handle that.
//
// TODO(user): Currently only max-flow handles this graph, but not
// min-cost-flow.
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class FlowGraph : public BaseGraph<FlowGraph<NodeIndexType, ArcIndexType>,
                                   NodeIndexType, ArcIndexType, false> {
  // Note that we do NOT use negated indices for reverse arc. So we use false
  // for the last template argument here HasNegativeReverseArcs.
  typedef BaseGraph<FlowGraph<NodeIndexType, ArcIndexType>, NodeIndexType,
                    ArcIndexType, false>
      Base;
  using Base::arc_capacity_;
  using Base::const_capacities_;
  using Base::node_capacity_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
  FlowGraph() = default;

  FlowGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(is_built_);
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs_);
    return heads_[arc];
  }

  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(is_built_);
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs_);

    // Note that we could trade memory for speed if this happens to be hot.
    // However, it is expected that most user will access arcs via
    // for (const int arc : graph.OutgoingArcs(tail)) {}
    // in which case all arcs already have a known tail.
    return heads_[reverse_[arc]];
  }

  // Each arc has a reverse.
  // If not added by the client, we have created one, see Build().
  ArcIndexType OppositeArc(ArcIndexType arc) const {
    DCHECK(is_built_);
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs_);
    return reverse_[arc];
  }

  // Iteration.
  util::IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    return OutgoingArcsStartingFrom(node, start_[node]);
  }
  util::IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(is_built_);
    DCHECK_GE(node, 0);
    DCHECK_LT(node, num_nodes_);
    DCHECK_GE(from, start_[node]);
    return util::IntegerRange<ArcIndexType>(from, start_[node + 1]);
  }

  // These are needed to use with the current max-flow implementation.
  // We don't distinguish direct from reverse arc anymore, and this is just
  // the same as OutgoingArcs()/OutgoingArcsStartingFrom().
  util::IntegerRange<ArcIndexType> OutgoingOrOppositeIncomingArcs(
      NodeIndexType node) const {
    return OutgoingArcs(node);
  }
  util::IntegerRange<ArcIndexType> OutgoingOrOppositeIncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    return OutgoingArcsStartingFrom(node, from);
  }

  absl::Span<const NodeIndexType> operator[](NodeIndexType node) const {
    const int b = start_[node];
    const size_t size = start_[node + 1] - b;
    if (size == 0) return {};
    return {&heads_[b], size};
  }

  void ReserveArcs(ArcIndexType bound) override {
    Base::ReserveArcs(bound);
    tmp_tails_.reserve(bound);
    tmp_heads_.reserve(bound);
  }

  void AddNode(NodeIndexType node) {
    num_nodes_ = std::max(num_nodes_, node + 1);
  }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    AddNode(tail > head ? tail : head);
    tmp_tails_.push_back(tail);
    tmp_heads_.push_back(head);
    return num_arcs_++;
  }

  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation) final;

  // This influence what Build() does. If true, we will detect already existing
  // pairs of (arc, reverse_arc) and only construct new reverse arc for the one
  // that we couldn't match. Otherwise, we will construct a new reverse arc for
  // each input arcs.
  void SetDetectReverse(bool value) { option_detect_reverse_ = value; }

  // This influence what Build() does. If true, the order of each outgoing arc
  // will be sorted by their head. Otherwise, it will be in the original order
  // with the newly created reverse arc afterwards.
  void SetSortByHead(bool value) { option_sort_by_head_ = value; }

 private:
  // Computes the permutation that would stable_sort input, and fill start_
  // to correspond to the beginning of each section of identical elements.
  // This assumes input only contains element in [0, num_nodes_).
  void FillPermutationAndStart(absl::Span<const NodeIndexType> input,
                               absl::Span<ArcIndexType> perm);

  // These are similar but tie-break identical element by "second_criteria".
  // We have two versions. It seems filling the reverse permutation instead is
  // slighthly faster and require less memory.
  void FillPermutationAndStart(absl::Span<const NodeIndexType> first_criteria,
                               absl::Span<const NodeIndexType> second_criteria,
                               absl::Span<ArcIndexType> perm);
  void FillReversePermutationAndStart(
      absl::Span<const NodeIndexType> first_criteria,
      absl::Span<const NodeIndexType> second_criteria,
      absl::Span<ArcIndexType> reverse_perm);

  // Internal helpers for the Fill*() functions above.
  void InitializeStart(absl::Span<const NodeIndexType> input);
  void RestoreStart();

  // Different build options.
  bool option_detect_reverse_ = true;
  bool option_sort_by_head_ = false;

  // Starts at false and set to true on Build(). This is mainly used in
  // DCHECKs() to guarantee that the graph is just built once, and no new arcs
  // are added afterwards.
  bool is_built_ = false;

  // This is just used before Build() to store the AddArc() data.
  std::vector<NodeIndexType> tmp_tails_;
  std::vector<NodeIndexType> tmp_heads_;

  // First outgoing arc for a node.
  // Indexed by NodeIndexType + a sentinel start_[num_nodes_] = num_arcs_.
  std::unique_ptr<ArcIndexType[]> start_;

  // Indexed by ArcIndexType an of size num_arcs_.
  // Head for an arc.
  std::unique_ptr<NodeIndexType[]> heads_;
  // Reverse arc for an arc.
  std::unique_ptr<ArcIndexType[]> reverse_;
};

namespace internal {

// Helper to permute a given span into another one.
template <class Key, class Value>
void PermuteInto(absl::Span<const Key> permutation,
                 absl::Span<const Value> input, absl::Span<Value> output) {
  DCHECK_EQ(permutation.size(), input.size());
  DCHECK_EQ(permutation.size(), output.size());
  for (int i = 0; i < permutation.size(); ++i) {
    output[permutation[i]] = input[i];
  }
}

}  // namespace internal

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::InitializeStart(
    absl::Span<const NodeIndexType> input) {
  // Computes the number of each elements.
  std::fill(start_.get(), start_.get() + num_nodes_, 0);
  start_[num_nodes_] = input.size();  // sentinel.

  for (const NodeIndexType node : input) start_[node]++;

  // Compute the cumulative sums (shifted one element to the right).
  int sum = 0;
  for (int i = 0; i < num_nodes_; ++i) {
    int temp = start_[i];
    start_[i] = sum;
    sum += temp;
  }
  DCHECK_EQ(sum, input.size());
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::RestoreStart() {
  // Restore in start[i] the index of the first arc with tail >= i.
  for (int i = num_nodes_; --i > 0;) {
    start_[i] = start_[i - 1];
  }
  start_[0] = 0;
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::FillPermutationAndStart(
    absl::Span<const NodeIndexType> input, absl::Span<ArcIndexType> perm) {
  DCHECK_EQ(input.size(), perm.size());
  InitializeStart(input);

  // Computes the permutation.
  // Note that this temporarily alters the start_ vector.
  for (int i = 0; i < input.size(); ++i) {
    perm[i] = start_[input[i]]++;
  }

  RestoreStart();
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::FillPermutationAndStart(
    absl::Span<const NodeIndexType> first_criteria,
    absl::Span<const NodeIndexType> second_criteria,
    absl::Span<ArcIndexType> perm) {
  // First sort by second_criteria.
  FillPermutationAndStart(second_criteria, absl::MakeSpan(perm));

  // Create a temporary permuted version of first_criteria.
  const auto tmp_storage = std::make_unique<ArcIndexType[]>(num_arcs_);
  const auto tmp = absl::MakeSpan(tmp_storage.get(), num_arcs_);
  internal::PermuteInto<ArcIndexType, NodeIndexType>(perm, first_criteria, tmp);

  // Now sort by permuted first_criteria.
  const auto second_perm_storage = std::make_unique<ArcIndexType[]>(num_arcs_);
  const auto second_perm = absl::MakeSpan(second_perm_storage.get(), num_arcs_);
  FillPermutationAndStart(tmp, second_perm);

  // Update the final permutation. This guarantee that for the same
  // first_criteria, the second_criteria will be used.
  for (ArcIndexType& image : perm) {
    image = second_perm[image];
  }
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::FillReversePermutationAndStart(
    absl::Span<const NodeIndexType> first_criteria,
    absl::Span<const NodeIndexType> second_criteria,
    absl::Span<ArcIndexType> reverse_perm) {
  // Compute the reverse perm according to the second criteria.
  InitializeStart(second_criteria);
  const auto r_perm_storage = std::make_unique<ArcIndexType[]>(num_arcs_);
  const auto r_perm = absl::MakeSpan(r_perm_storage.get(), num_arcs_);
  auto* start = start_.get();
  for (int i = 0; i < second_criteria.size(); ++i) {
    r_perm[start[second_criteria[i]]++] = i;
  }

  // Now radix-sort by the first criteria and combine the two permutations.
  InitializeStart(first_criteria);
  for (const int i : r_perm) {
    reverse_perm[start[first_criteria[i]]++] = i;
  }
  RestoreStart();
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;

  start_ = std::make_unique<ArcIndexType[]>(num_nodes_ + 1);
  std::vector<ArcIndexType> perm(num_arcs_);

  const int kNoExplicitReverseArc = -1;
  std::vector<ArcIndexType> reverse(num_arcs_, kNoExplicitReverseArc);

  bool fix_final_permutation = false;
  if (option_detect_reverse_) {
    // Step 1 we only keep a "canonical version" of each arc where tail <= head.
    // This will allow us to detect reverse as duplicates instead.
    std::vector<bool> was_reversed_(num_arcs_, false);
    for (int arc = 0; arc < num_arcs_; ++arc) {
      if (tmp_heads_[arc] < tmp_tails_[arc]) {
        std::swap(tmp_heads_[arc], tmp_tails_[arc]);
        was_reversed_[arc] = true;
      }
    }

    // Step 2, compute the perm to sort by tail then head.
    // This is something we do a few times here, we compute the permutation with
    // a kind of radix sort by computing the degree of each node.
    auto reverse_perm = absl::MakeSpan(perm);  // we reuse space
    FillReversePermutationAndStart(tmp_tails_, tmp_heads_,
                                   absl::MakeSpan(reverse_perm));

    // Identify arc pairs that are reverse of each other and fill reverse for
    // them. The others position will stay at -1.
    int candidate_i = 0;
    int num_filled = 0;
    for (int i = 0; i < num_arcs_; ++i) {
      const int arc = reverse_perm[i];
      const int candidate_arc = reverse_perm[candidate_i];
      if (tmp_heads_[arc] != tmp_heads_[candidate_arc] ||
          tmp_tails_[arc] != tmp_tails_[candidate_arc]) {
        candidate_i = i;
        continue;
      }

      if (was_reversed_[arc] != was_reversed_[candidate_arc]) {
        DCHECK_EQ(reverse[arc], -1);
        DCHECK_EQ(reverse[candidate_arc], -1);
        reverse[arc] = candidate_arc;
        reverse[candidate_arc] = arc;
        num_filled += 2;

        // Find the next candidate without reverse if there is a gap.
        for (; ++candidate_i < i;) {
          if (reverse[reverse_perm[candidate_i]] == -1) break;
        }
        if (candidate_i == i) {
          candidate_i = ++i;  // Advance by two.
        }
      }
    }

    // Create the extra reversed arcs needed.
    {
      const int extra_size = num_arcs_ - num_filled;
      tmp_tails_.resize(num_arcs_ + extra_size);
      tmp_heads_.resize(num_arcs_ + extra_size);
      reverse.resize(num_arcs_ + extra_size);
      int new_index = num_arcs_;
      for (int i = 0; i < num_arcs_; ++i) {
        // Fix the initial swap.
        if (was_reversed_[i]) {
          std::swap(tmp_tails_[i], tmp_heads_[i]);
        }

        if (reverse[i] != kNoExplicitReverseArc) continue;
        reverse[i] = new_index;
        reverse[new_index] = i;
        tmp_tails_[new_index] = tmp_heads_[i];
        tmp_heads_[new_index] = tmp_tails_[i];
        ++new_index;
      }
      CHECK_EQ(new_index, num_arcs_ + extra_size);
    }
  } else {
    // Just create a reverse for each arc.
    tmp_tails_.resize(2 * num_arcs_);
    tmp_heads_.resize(2 * num_arcs_);
    reverse.resize(2 * num_arcs_);
    for (int arc = 0; arc < num_arcs_; ++arc) {
      const int image = num_arcs_ + arc;
      tmp_heads_[image] = tmp_tails_[arc];
      tmp_tails_[image] = tmp_heads_[arc];
      reverse[image] = arc;
      reverse[arc] = image;
    }

    // It seems better to put all the reverse first instead of last in
    // the adjacency list, so lets do that here. Note that we need to fix
    // the permutation returned to the user in this case.
    //
    // With this, we should have almost exactly the same behavior
    // as ReverseArcStaticGraph().
    fix_final_permutation = true;
    for (int arc = 0; arc < num_arcs_; ++arc) {
      const int image = num_arcs_ + arc;
      std::swap(tmp_heads_[image], tmp_heads_[arc]);
      std::swap(tmp_tails_[image], tmp_tails_[arc]);
    }
  }

  num_arcs_ = tmp_heads_.size();
  perm.resize(num_arcs_);

  // Do we sort each OutgoingArcs(node) by head ?
  // Or is it better to keep all new reverse arc before or after ?
  //
  // TODO(user): For now we only support sorting, or all new reverse after
  // and keep the original arc order.
  if (option_sort_by_head_) {
    FillPermutationAndStart(tmp_tails_, tmp_heads_, absl::MakeSpan(perm));
  } else {
    FillPermutationAndStart(tmp_tails_, absl::MakeSpan(perm));
  }

  // Create the final heads_.
  heads_ = std::make_unique<NodeIndexType[]>(num_arcs_);
  internal::PermuteInto<ArcIndexType, NodeIndexType>(
      perm, tmp_heads_, absl::MakeSpan(heads_.get(), num_arcs_));

  // No longer needed.
  gtl::STLClearObject(&tmp_tails_);
  gtl::STLClearObject(&tmp_heads_);

  // We now create reverse_, this needs the permutation on both sides.
  reverse_ = std::make_unique<ArcIndexType[]>(num_arcs_);
  for (int i = 0; i < num_arcs_; ++i) {
    reverse_[perm[i]] = perm[reverse[i]];
  }

  if (permutation != nullptr) {
    if (fix_final_permutation) {
      for (int i = 0; i < num_arcs_ / 2; ++i) {
        std::swap(perm[i], perm[num_arcs_ / 2 + i]);
      }
    }
    permutation->swap(perm);
  }

  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();
}

}  // namespace util

#endif  // UTIL_GRAPH_FLOW_GRAPH_H_
