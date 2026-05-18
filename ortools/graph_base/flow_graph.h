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
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/unique_array.h"
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
// construction is slightly slower because of this detection.
//
// A side effect of reusing reverse arc is also that we cannot anymore clearly
// separate the original arcs from the newly created one. So the algorithm needs
// to be able to handle that.
//
// TODO(user): Currently only max-flow handles this graph, but not
// min-cost-flow.
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class FlowGraph final : public BaseGraph<FlowGraph<NodeIndexType, ArcIndexType>,
                                         NodeIndexType, ArcIndexType, false> {
  // Note that we do NOT use negated indices for reverse arc. So we use false
  // for the last template argument here HasNegativeReverseArcs.
  typedef BaseGraph<FlowGraph<NodeIndexType, ArcIndexType>, NodeIndexType,
                    ArcIndexType, false>
      Base;

 public:
  class Builder;

  NodeIndexType num_nodes() const {
    DCHECK_GT(start_.size(), 0);
    return start_.size() - 1;
  }

  ArcIndexType num_arcs() const { return heads_.size(); }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs());
    return heads_[arc];
  }

  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs());

    // Note that we could trade memory for speed if this happens to be hot.
    // However, it is expected that most user will access arcs via
    // for (const int arc : graph.OutgoingArcs(tail)) {}
    // in which case all arcs already have a known tail.
    return heads_[reverse_[arc]];
  }

  // Each arc has a reverse.
  // If not added by the client, we have created one during Build().
  ArcIndexType OppositeArc(ArcIndexType arc) const {
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, num_arcs());
    return reverse_[arc];
  }

  // Iteration.
  util::IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    return OutgoingArcsStartingFrom(node, start_[node]);
  }
  util::IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK_GE(node, 0);
    DCHECK_LT(node, num_nodes());
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

 private:
  friend class Builder;

  FlowGraph() = default;

  // First outgoing arc for a node.
  // Indexed by NodeIndexType + a sentinel start_[num_nodes_] = num_arcs_.
  gtl::UniqueArray<ArcIndexType> start_;

  // Indexed by ArcIndexType and of size num_arcs_.
  // Head for an arc.
  gtl::UniqueArray<NodeIndexType> heads_;
  // Reverse arc for an arc.
  gtl::UniqueArray<ArcIndexType> reverse_;
};

namespace internal {

// Helper to permute a given span into another one.
template <class Key, class Value, auto criterion>
void PermuteInto(absl::Span<const Key> permutation,
                 absl::Span<const std::pair<Value, Value>> input,
                 absl::Span<Value> output) {
  DCHECK_EQ(permutation.size(), input.size());
  DCHECK_EQ(permutation.size(), output.size());
  for (int i = 0; i < permutation.size(); ++i) {
    output[permutation[i]] = criterion(input[i]);
  }
}

}  // namespace internal

template <typename NodeIndexType, typename ArcIndexType>
class FlowGraph<NodeIndexType, ArcIndexType>::Builder
    : public internal::GraphBuilderBase<Builder, FlowGraph, NodeIndexType,
                                        ArcIndexType> {
 public:
  Builder() = default;
  Builder(NodeIndexType num_nodes, ArcIndexType num_arcs)
      : num_nodes_(num_nodes) {
    ReserveArcs(num_arcs);
  }

  void ReserveArcs(ArcIndexType bound) & { tmp_arcs_.reserve(bound); }

  void AddNode(NodeIndexType node) & {
    num_nodes_ = std::max(num_nodes_, node + 1);
  }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) & {
    AddNode(tail > head ? tail : head);
    const ArcIndexType result = num_arcs();
    tmp_arcs_.push_back({tail, head});
    return result;
  }

  // If true, we will detect already existing pairs of (arc, reverse_arc) and
  // only construct new reverse arc for the one that we couldn't match.
  // Otherwise, we will construct a new reverse arc for each input arcs.
  void SetDetectReverse(bool value) & { option_detect_reverse_ = value; }
  Builder&& SetDetectReverse(bool value) && {
    SetDetectReverse(value);
    return std::move(*this);
  }

  // If true, the order of each outgoing arc will be sorted by their head.
  // Otherwise, it will be in the original order with the newly created
  // reverse arc afterwards.
  void SetSortByHead(bool value) & { option_sort_by_head_ = value; }
  Builder&& SetSortByHead(bool value) && {
    SetSortByHead(value);
    return std::move(*this);
  }

  std::unique_ptr<FlowGraph> Build(std::vector<ArcIndexType>* permutation) &&;

  NodeIndexType num_nodes() const { return num_nodes_; }
  ArcIndexType num_arcs() const { return tmp_arcs_.size(); }

 private:
  // A user-provided arc, as (tail, head) pairs.
  using TmpArc = std::pair<NodeIndexType, NodeIndexType>;

  // Accessors.
  static NodeIndexType Tail(const TmpArc& arc) { return arc.first; }
  static NodeIndexType Head(const TmpArc& arc) { return arc.second; }
  static ArcIndexType Arc(const ArcIndexType& arc) { return arc; }

  // Computes the permutation that would stable_sort input by `criterion`,
  // and fill start to correspond to the beginning of each section of identical
  // elements.
  // This assumes input only contains element in [0, num_nodes_).
  template <auto criterion, typename V>
  static void FillPermutationAndStartBy(absl::Span<const V> arcs,
                                        absl::Span<ArcIndexType> perm,
                                        absl::Span<ArcIndexType> start);

  // These are similar but tie-break identical element by tail.
  // We have two versions. It seems filling the reverse permutation instead is
  // slightly faster and require less memory.
  static void FillPermutationAndStart(absl::Span<const TmpArc> arcs,
                                      absl::Span<ArcIndexType> perm,
                                      absl::Span<ArcIndexType> start);
  static void FillReversePermutationAndStart(
      absl::Span<const TmpArc> arcs, absl::Span<ArcIndexType> reverse_perm,
      absl::Span<ArcIndexType> start);

  // Internal helpers for the Fill*() functions above.
  template <auto criterion, typename V>
  static void InitializeStart(absl::Span<const V> arcs,
                              absl::Span<ArcIndexType> start);
  static void RestoreStart(absl::Span<ArcIndexType> start);

  bool option_detect_reverse_ = true;
  bool option_sort_by_head_ = false;

  NodeIndexType num_nodes_ = 0;

  // User-provided arcs.
  std::vector<TmpArc> tmp_arcs_;
};

template <typename NodeIndexType, typename ArcIndexType>
template <auto criterion, typename V>
void FlowGraph<NodeIndexType, ArcIndexType>::Builder::InitializeStart(
    absl::Span<const V> arcs, absl::Span<ArcIndexType> start) {
  // Computes the number of each elements.
  DCHECK(!start.empty());
  const NodeIndexType num_nodes = start.size() - 1;
  std::fill(start.data(), start.data() + num_nodes, 0);
  start[num_nodes] = arcs.size();  // sentinel.

  for (const auto arc : arcs) start[criterion(arc)]++;

  // Compute the cumulative sums (shifted one element to the right).
  int sum = 0;
  for (int i = 0; i < num_nodes; ++i) {
    int temp = start[i];
    start[i] = sum;
    sum += temp;
  }
  DCHECK_EQ(sum, arcs.size());
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::Builder::RestoreStart(
    absl::Span<ArcIndexType> start) {
  // Restore in start[i] the index of the first arc with tail >= i.
  DCHECK(!start.empty());
  for (int i = start.size() - 1; --i > 0;) {
    start[i] = start[i - 1];
  }
  start[0] = 0;
}

template <typename NodeIndexType, typename ArcIndexType>
template <auto criterion, typename V>
void FlowGraph<NodeIndexType, ArcIndexType>::Builder::FillPermutationAndStartBy(
    absl::Span<const V> arcs, absl::Span<ArcIndexType> perm,
    absl::Span<ArcIndexType> start) {
  DCHECK_EQ(arcs.size(), perm.size());
  InitializeStart<criterion>(arcs, start);

  // Computes the permutation.
  // Note that this temporarily alters the start_ vector.
  for (int i = 0; i < arcs.size(); ++i) {
    perm[i] = start[criterion(arcs[i])]++;
  }

  RestoreStart(start);
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::Builder::FillPermutationAndStart(
    absl::Span<const TmpArc> arcs, absl::Span<ArcIndexType> perm,
    absl::Span<ArcIndexType> start) {
  const auto num_arcs = perm.size();
  // First sort by second_criteria.
  FillPermutationAndStartBy<Head>(arcs, perm, start);

  // Create a temporary permuted version of first_criteria.
  const auto tmp_storage = gtl::MakeUniqueArray<ArcIndexType>(num_arcs);
  internal::PermuteInto<ArcIndexType, NodeIndexType, Tail>(
      perm, arcs, absl::MakeSpan(tmp_storage.data(), num_arcs));

  // Now sort by permuted first_criteria.
  const auto second_perm_storage = gtl::MakeUniqueArray<ArcIndexType>(num_arcs);
  const auto second_perm = absl::MakeSpan(second_perm_storage.data(), num_arcs);
  FillPermutationAndStartBy<Arc>(
      absl::MakeConstSpan(tmp_storage.data(), num_arcs), second_perm, start);

  // Update the final permutation. This guarantee that for the same
  // first_criteria, the second_criteria will be used.
  for (ArcIndexType& image : perm) {
    image = second_perm[image];
  }
}

template <typename NodeIndexType, typename ArcIndexType>
void FlowGraph<NodeIndexType, ArcIndexType>::Builder::
    FillReversePermutationAndStart(absl::Span<const TmpArc> arcs,
                                   absl::Span<ArcIndexType> reverse_perm,
                                   absl::Span<ArcIndexType> start) {
  // Compute the reverse perm according to the second criteria.
  const auto num_arcs = reverse_perm.size();
  InitializeStart<Head>(arcs, start);
  const auto r_perm_storage = gtl::MakeUniqueArray<ArcIndexType>(num_arcs);
  const auto r_perm = absl::MakeSpan(r_perm_storage.data(), num_arcs);
  for (int i = 0; i < arcs.size(); ++i) {
    r_perm[start[Head(arcs[i])]++] = i;
  }

  // Now radix-sort by the first criteria and combine the two permutations.
  InitializeStart<Tail>(arcs, start);
  for (const int i : r_perm) {
    reverse_perm[start[Tail(arcs[i])]++] = i;
  }
  RestoreStart(start);
}

template <typename NodeIndexType, typename ArcIndexType>
std::unique_ptr<FlowGraph<NodeIndexType, ArcIndexType>>
FlowGraph<NodeIndexType, ArcIndexType>::Builder::Build(
    std::vector<ArcIndexType>* permutation) && {
  const NodeIndexType num_nodes = num_nodes_;
  const ArcIndexType num_orig_arcs = num_arcs();
  auto start = gtl::MakeUniqueArray<ArcIndexType>(num_nodes + 1);
  std::vector<ArcIndexType> perm(num_orig_arcs);

  const int kNoExplicitReverseArc = -1;
  std::vector<ArcIndexType> reverse(num_orig_arcs, kNoExplicitReverseArc);

  bool fix_final_permutation = false;
  if (option_detect_reverse_) {
    // Step 1 we only keep a "canonical version" of each arc where tail <= head.
    // This will allow us to detect reverse as duplicates instead.
    std::vector<bool> was_reversed(num_orig_arcs, false);
    for (int arc = 0; arc < num_orig_arcs; ++arc) {
      auto& [tail, head] = tmp_arcs_[arc];
      if (head < tail) {
        std::swap(head, tail);
        was_reversed[arc] = true;
      }
    }

    // Step 2, compute the perm to sort by tail then head.
    // This is something we do a few times here, we compute the permutation with
    // a kind of radix sort by computing the degree of each node.
    auto reverse_perm = absl::MakeSpan(perm);  // we reuse space
    FillReversePermutationAndStart(tmp_arcs_, absl::MakeSpan(reverse_perm),
                                   absl::MakeSpan(start));

    // Identify arc pairs that are reverse of each other and fill reverse for
    // them. The others position will stay at -1.
    int candidate_i = 0;
    int num_filled = 0;
    for (int i = 0; i < num_orig_arcs; ++i) {
      const int arc = reverse_perm[i];
      const int candidate_arc = reverse_perm[candidate_i];
      const auto [tail, head] = tmp_arcs_[arc];
      const auto [candidate_tail, candidate_head] = tmp_arcs_[candidate_arc];
      if (head != candidate_head || tail != candidate_tail) {
        candidate_i = i;
        continue;
      }

      if (was_reversed[arc] != was_reversed[candidate_arc]) {
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
      const int extra_size = num_orig_arcs - num_filled;
      tmp_arcs_.resize(num_orig_arcs + extra_size);
      reverse.resize(num_orig_arcs + extra_size);
      int new_index = num_orig_arcs;
      for (int i = 0; i < num_orig_arcs; ++i) {
        // Fix the initial swap.
        if (was_reversed[i]) {
          auto& [tail, head] = tmp_arcs_[i];
          std::swap(tail, head);
        }

        if (reverse[i] != kNoExplicitReverseArc) continue;
        reverse[i] = new_index;
        reverse[new_index] = i;
        const auto [tail, head] = tmp_arcs_[i];
        tmp_arcs_[new_index] = {head, tail};
        ++new_index;
      }
      CHECK_EQ(new_index, num_orig_arcs + extra_size);
    }
  } else {
    // Just create a reverse for each arc.
    tmp_arcs_.resize(2 * num_orig_arcs);
    reverse.resize(2 * num_orig_arcs);
    for (int arc = 0; arc < num_orig_arcs; ++arc) {
      const int image = num_orig_arcs + arc;
      const auto [tail, head] = tmp_arcs_[arc];
      tmp_arcs_[image] = {head, tail};
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
    for (int arc = 0; arc < num_orig_arcs; ++arc) {
      const int image = num_orig_arcs + arc;
      std::swap(tmp_arcs_[image], tmp_arcs_[arc]);
    }
  }

  const ArcIndexType num_flow_arcs = tmp_arcs_.size();
  perm.resize(num_flow_arcs);

  // Do we sort each OutgoingArcs(node) by head ?
  // Or is it better to keep all new reverse arc before or after ?
  //
  // TODO(user): For now we only support sorting, or all new reverse after
  // and keep the original arc order.
  if (option_sort_by_head_) {
    FillPermutationAndStart(tmp_arcs_, absl::MakeSpan(perm),
                            absl::MakeSpan(start));
  } else {
    FillPermutationAndStartBy<Tail>(absl::MakeConstSpan(tmp_arcs_),
                                    absl::MakeSpan(perm),
                                    absl::MakeSpan(start));
  }

  auto graph = absl::WrapUnique(new FlowGraph());
  graph->start_ = std::move(start);

  // Create the final heads_.
  graph->heads_ = gtl::MakeUniqueArray<NodeIndexType>(num_flow_arcs);
  internal::PermuteInto<ArcIndexType, NodeIndexType, Head>(
      perm, tmp_arcs_, absl::MakeSpan(graph->heads_.data(), num_flow_arcs));

  // We now create reverse_, this needs the permutation on both sides.
  graph->reverse_ = gtl::MakeUniqueArray<ArcIndexType>(num_flow_arcs);
  for (int i = 0; i < num_flow_arcs; ++i) {
    graph->reverse_[perm[i]] = perm[reverse[i]];
  }

  if (permutation != nullptr) {
    if (fix_final_permutation) {
      for (int i = 0; i < num_flow_arcs / 2; ++i) {
        std::swap(perm[i], perm[num_flow_arcs / 2 + i]);
      }
    }
    permutation->swap(perm);
  }
  return std::move(graph);
}

}  // namespace util

#endif  // UTIL_GRAPH_FLOW_GRAPH_H_
