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

#include "ortools/graph/topologicalsorter.h"

#include <algorithm>
#include <map>
#include <queue>
#include <string>
#include <vector>

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

  if (node_index >= adjacency_lists_.size()) {
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
void DenseIntTopologicalSorterTpl<stable_sort>::AddEdge(int from, int to) {
  CHECK(!TraversalStarted()) << "Cannot add edges after starting traversal";

  AddNode(std::max(from, to));

  AdjacencyList& adj_list = adjacency_lists_[from];
  const uint32 adj_list_size = adj_list.size();
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
    if (output_cycle_nodes != NULL) {
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
  for (int i = 0; i < adj_list.size(); ++i) {
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
    if (list->size() < skip_lists_smaller_than) {
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
  const int num_nodes = adjacency_lists_.size();
  cycle_nodes->clear();
  // To find a cycle, we start a DFS from each yet-unvisited node and
  // try to find a cycle, if we don't find it then we know for sure that
  // no cycle is reachable from any of the explored nodes (so, we don't
  // explore them in later DFSs).
  std::vector<bool> no_cycle_reachable_from(num_nodes, false);
  // The DFS stack will contain a chain of nodes, from the root of the
  // DFS to the current leaf.
  struct DfsState {
    int node;
    // Points at the first child node that we did *not* yet look at.
    int adj_list_index;
    explicit DfsState(int _node) : node(_node), adj_list_index(0) {}
  };
  std::vector<DfsState> dfs_stack;
  std::vector<bool> in_cur_stack(num_nodes, false);
  for (int start_node = 0; start_node < num_nodes; ++start_node) {
    if (no_cycle_reachable_from[start_node]) {
      continue;
    }
    // Start the DFS.
    dfs_stack.push_back(DfsState(start_node));
    in_cur_stack[start_node] = true;
    while (!dfs_stack.empty()) {
      DfsState* cur_state = &dfs_stack.back();
      if (cur_state->adj_list_index >=
          adjacency_lists_[cur_state->node].size()) {
        no_cycle_reachable_from[cur_state->node] = true;
        in_cur_stack[cur_state->node] = false;
        dfs_stack.pop_back();
        continue;
      }
      // Look at the current child, and increase the current state's
      // adj_list_index.
      const int child =
          adjacency_lists_[cur_state->node][cur_state->adj_list_index];
      ++(cur_state->adj_list_index);
      if (no_cycle_reachable_from[child]) {
        continue;
      }
      if (in_cur_stack[child]) {
        // We detected a cycle! Fill it and return.
        for (;;) {
          cycle_nodes->push_back(dfs_stack.back().node);
          if (dfs_stack.back().node == child) {
            std::reverse(cycle_nodes->begin(), cycle_nodes->end());
            return;
          }
          dfs_stack.pop_back();
        }
      }
      // Push the child onto the stack.
      dfs_stack.push_back(DfsState(child));
      in_cur_stack[child] = true;
    }
  }
  // If we're here, then all the DFS stopped, and they never encountered
  // a cycle (otherwise, we would have returned). Just exit; the output
  // vector has been cleared already.
}

// Generate the templated code.  Including these definitions allows us
// to have templated code inside the .cc file and not incur linker errors.
template class DenseIntTopologicalSorterTpl<false>;
template class DenseIntTopologicalSorterTpl<true>;

}  // namespace internal

std::vector<int> FindCycleInDenseIntGraph(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  std::vector<int> cycle;
  if (num_nodes < 1) {
    return cycle;
  }
  internal::DenseIntTopologicalSorterTpl</* stable= */ false> sorter(num_nodes);
  for (const auto& arc : arcs) {
    sorter.AddEdge(arc.first, arc.second);
  }
  sorter.ExtractCycle(&cycle);
  return cycle;
}
}  // namespace util
