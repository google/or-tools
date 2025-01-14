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

#include "ortools/graph/topologicalsorter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"

namespace util {
namespace internal {

namespace {
template <typename IntQueue>
inline void PopTop(IntQueue* q, int* top) {
  *top = q->front();
  q->pop();
}

template <typename C, typename F>
void PopTop(std::priority_queue<int, C, F>* q, int* top) {
  *top = q->top();
  q->pop();
}
}  // namespace

template <bool stable_sort>
void DenseIntTopologicalSorterTpl<stable_sort>::AddNode(int node_index) {
  CHECK(!TraversalStarted()) << "Cannot add nodes after starting traversal";
  CHECK_GE(node_index, 0) << "Index must not be negative";

  if (static_cast<std::size_t>(node_index) >= adjacency_lists_.size()) {
    adjacency_lists_.resize(node_index + 1);
  }
}

// Up to a point, we detect duplicates up front and do not insert them.
// Then we switch to using RemoveDuplicates(), see below.
//
// Note(user): I did benchmarks on this in November 2011, and while
// 32 seemed too large, I did not see very significant performance
// differences with 0, 4, 8 or 16. But since larger values of this
// threshold mean that there will be slightly less space used up by
// small adjacency lists in case there are repeated edges, I picked 16.
static const int kLazyDuplicateDetectionSizeThreshold = 16;

template <bool stable_sort>
void DenseIntTopologicalSorterTpl<stable_sort>::AddEdges(
    absl::Span<const std::pair<int, int>> edges) {
  CHECK(!TraversalStarted()) << "Cannot add edges after starting traversal";

  // Make a first pass to detect the number of nodes.
  int max_node = -1;
  for (const auto& [from, to] : edges) {
    if (from > max_node) max_node = from;
    if (to > max_node) max_node = to;
  }
  if (max_node >= 0) AddNode(max_node);

  // Make a second pass to reserve the adjacency list sizes.
  // We use indegree_ as temporary node buffer to store the node out-degrees,
  // since it isn't being used yet.
  indegree_.assign(max_node + 1, 0);
  for (const auto& [from, to] : edges) ++indegree_[from];
  for (int node = 0; node < max_node; ++node) {
    adjacency_lists_[node].reserve(indegree_[node]);
  }
  indegree_.clear();

  // Finally, add edges to the adjacency lists in a third pass. Don't bother
  // doing the duplicate detection: in the bulk API, we assume that there isn't
  // much edge duplication.
  for (const auto& [from, to] : edges) adjacency_lists_[from].push_back(to);
}

template <bool stable_sort>
void DenseIntTopologicalSorterTpl<stable_sort>::AddEdge(int from, int to) {
  CHECK(!TraversalStarted()) << "Cannot add edges after starting traversal";

  AddNode(std::max(from, to));

  AdjacencyList& adj_list = adjacency_lists_[from];
  const uint32_t adj_list_size = adj_list.size();
  if (adj_list_size <= kLazyDuplicateDetectionSizeThreshold) {
    for (AdjacencyList::const_iterator it = adj_list.begin();
         it != adj_list.end(); ++it) {
      if (*it == to) {
        return;
      }
    }
    adj_list.push_back(to);
    ++num_edges_;
  } else {
    adj_list.push_back(to);
    if (++num_edges_added_since_last_duplicate_removal_ > ++num_edges_ / 2) {
      num_edges_added_since_last_duplicate_removal_ = 0;
      // We remove all duplicates at once, but skip lists for which the
      // number of duplicates can't be too large, i.e. lists smaller than
      // kLazyDuplicateDetectionSizeThreshold * 2. The overall ratio of
      // duplicate edges remains bounded by 2/3 in the worst case.
      num_edges_ -= RemoveDuplicates(&adjacency_lists_,
                                     kLazyDuplicateDetectionSizeThreshold * 2);
    }
  }
}

template <bool stable_sort>
bool DenseIntTopologicalSorterTpl<stable_sort>::GetNext(
    int* next_node_index, bool* cyclic, std::vector<int>* output_cycle_nodes) {
  if (!TraversalStarted()) {
    StartTraversal();
  }

  *cyclic = false;
  if (num_nodes_left_ == 0) {
    return false;
  }
  if (nodes_with_zero_indegree_.empty()) {
    VLOG(2) << "Not all nodes have been visited (" << num_nodes_left_
            << " nodes left), but there aren't any zero-indegree nodes"
            << " available.  This graph is cyclic! Use ExtractCycle() for"
            << " more information.";
    *cyclic = true;
    if (output_cycle_nodes != nullptr) {
      ExtractCycle(output_cycle_nodes);
    }
    return false;
  }

  // Pop one orphan node.
  --num_nodes_left_;
  PopTop(&nodes_with_zero_indegree_, next_node_index);

  // Swap out the adjacency list, since we won't need it afterwards,
  // to decrease memory usage.
  AdjacencyList adj_list;
  adj_list.swap(adjacency_lists_[*next_node_index]);

  // Add new orphan nodes to nodes_with_zero_indegree_.
  for (std::size_t i = 0; i < adj_list.size(); ++i) {
    if (--indegree_[adj_list[i]] == 0) {
      nodes_with_zero_indegree_.push(adj_list[i]);
    }
  }
  return true;
}

template <bool stable_sort>
void DenseIntTopologicalSorterTpl<stable_sort>::StartTraversal() {
  if (TraversalStarted()) {
    return;
  }

  const int num_nodes = adjacency_lists_.size();
  indegree_.assign(num_nodes, 0);

  // Iterate over all adjacency lists, and fill the indegree[] vector.
  // Note that we don't bother removing duplicates: there can't be
  // too many, since we removed them progressively, and it is actually
  // cheaper to keep them at this point.
  for (int from = 0; from < num_nodes; ++from) {
    AdjacencyList& adj_list = adjacency_lists_[from];
    for (AdjacencyList::const_iterator it = adj_list.begin();
         it != adj_list.end(); ++it) {
      ++indegree_[*it];
    }
  }

  // Initialize the nodes_with_zero_indegree_ vector.
  for (int node = 0; node < num_nodes; ++node) {
    if (indegree_[node] == 0) {
      nodes_with_zero_indegree_.push(node);
    }
  }

  num_nodes_left_ = num_nodes;
  traversal_started_ = true;
}

// static
template <bool stable_sort>
int DenseIntTopologicalSorterTpl<stable_sort>::RemoveDuplicates(
    std::vector<AdjacencyList>* lists, int skip_lists_smaller_than) {
  // We can always skip lists with less than 2 elements.
  if (skip_lists_smaller_than < 2) {
    skip_lists_smaller_than = 2;
  }
  const int n = lists->size();
  std::vector<bool> visited(n, false);
  int num_duplicates_removed = 0;
  for (std::vector<AdjacencyList>::iterator list = lists->begin();
       list != lists->end(); ++list) {
    if (list->size() < static_cast<std::size_t>(skip_lists_smaller_than)) {
      continue;
    }
    num_duplicates_removed += list->size();
    // To optimize the duplicate removal loop, we split it in two:
    // first, find the first duplicate, then copy the rest of the shifted
    // adjacency list as we keep detecting duplicates.
    AdjacencyList::iterator it = list->begin();
    DCHECK(it != list->end());
    while (!visited[*it]) {
      visited[*(it++)] = true;
      if (it == list->end()) {
        break;
      }
    }
    // Skip the shifted copy if there were no duplicates at all.
    if (it != list->end()) {
      AdjacencyList::iterator it2 = it;
      while (++it != list->end()) {
        if (!visited[*it]) {
          visited[*it] = true;
          *(it2++) = *it;
        }
      }
      list->erase(it2, list->end());
    }
    for (it = list->begin(); it != list->end(); ++it) {
      visited[*it] = false;
    }
    num_duplicates_removed -= list->size();
  }
  return num_duplicates_removed;
}

// Note(user): as of 2012-09, this implementation works in
// O(number of edges + number of nodes), which is the theoretical best.
// It could probably be optimized to gain a significant constant speed-up;
// but at the cost of more code complexity.
template <bool stable_sort>
void DenseIntTopologicalSorterTpl<stable_sort>::ExtractCycle(
    std::vector<int>* cycle_nodes) const {
  *cycle_nodes = util::graph::FindCycleInGraph(adjacency_lists_).value();
}

// Generate the templated code.  Including these definitions allows us
// to have templated code inside the .cc file and not incur linker errors.
template class DenseIntTopologicalSorterTpl<false>;
template class DenseIntTopologicalSorterTpl<true>;

}  // namespace internal
}  // namespace util
