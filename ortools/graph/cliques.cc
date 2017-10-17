// Copyright 2010-2017 Google
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


#include "ortools/graph/cliques.h"

#include <algorithm>
#include <unordered_set>
#include <memory>
#include <utility>
#include <vector>

#include "ortools/base/hash.h"

namespace operations_research {
namespace {
// Encapsulates graph() to make all nodes self-connected.
inline bool Connects(std::function<bool(int, int)> graph, int i, int j) {
  return i == j || graph(i, j);
}

// Implements the recursive step of the Bron-Kerbosch algorithm with pivoting.
// - graph is a callback such that graph->Run(i, j) returns true iff there is an
//   arc between i and j.
// - callback is a callback called for all maximal cliques discovered by the
//   algorithm.
// - input_candidates is an array that contains the list of nodes connected to
//   all nodes in the current clique. It is composed of two parts; the first
//   part contains the "not" set (nodes that were already processed and must not
//   be added to the clique - see the description of the algorithm in the
//   paper), and nodes that are candidates for addition. The candidates from the
//   "not" set are at the beginning of the array.
// - first_candidate_index elements is the index of the first candidate that is
//   not in the "not" set (which is also the number of candidates in the "not"
//   set).
// - num_input_candidates is the number of elements in input_candidates,
//   including both the "not" set and the actual candidates.
// - current_clique is the current clique discovered by the algorithm.
// - stop is a stopping condition for the algorithm; if the value it points to
//   is true, the algorithm stops further exploration and returns.
// TODO(user) : rewrite this algorithm without recursion.
void Search(std::function<bool(int, int)> graph,
            std::function<bool(const std::vector<int>&)> callback,
            int* input_candidates, int first_candidate_index,
            int num_input_candidates, std::vector<int>* current_clique,
            bool* stop) {
  // The pivot is a node from input_candidates that is disconnected from the
  // minimal number of nodes in the actual candidates (excluding the "not" set);
  // the algorithm then selects only candidates that are disconnected from the
  // pivot (and the pivot itself), to reach the termination condition as quickly
  // as possible (see the original paper for more details).
  int pivot = 0;

  // A node that is disconnected from the selected pivot. This node is selected
  // during the pivot matching phase to speed up the first iteration of the
  // recursive call.
  int disconnected_node = 0;

  // The number of candidates (that are not in "not") disconnected from the
  // selected pivot. The value is computed during pivot selection. In the
  // "recursive" phase, we only need to do explore num_disconnected_candidates
  // nodes, because after this step, all remaining candidates will all be
  // connected to the pivot node (which is in "not"), so they can't form a
  // maximal clique.
  int num_disconnected_candidates = num_input_candidates;

  // If the selected pivot is not in "not", we need to process one more
  // candidate (the pivot itself). pre_increment is added to
  // num_disconnected_candidates to compensate for this fact.
  int pre_increment = 0;

  // Find Pivot.
  for (int i = 0; i < num_input_candidates && num_disconnected_candidates != 0;
       ++i) {
    int pivot_candidate = input_candidates[i];

    // Count is the number of candidates (not including nodes in the "not" set)
    // that are disconnected from the pivot candidate.
    int count = 0;

    // The index of a candidate node that is not connected to pivot_candidate.
    // This node will be used to quickly start the nested iteration (we keep
    // track of the index so that we don't have to find a node that is
    // disconnected from the pivot later in the iteration).
    int disconnected_node_candidate = 0;

    // Compute the number of candidate nodes that are disconnected from
    // pivot_candidate. Note that this computation is the same for the "not"
    // candidates and the normal candidates.
    for (int j = first_candidate_index;
         j < num_input_candidates && count < num_disconnected_candidates; ++j) {
      if (!Connects(graph, pivot_candidate, input_candidates[j])) {
        count++;
        disconnected_node_candidate = j;
      }
    }

    // Update the pivot candidate if we found a new minimum for
    // num_disconnected_candidates.
    if (count < num_disconnected_candidates) {
      pivot = pivot_candidate;
      num_disconnected_candidates = count;

      if (i < first_candidate_index) {
        disconnected_node = disconnected_node_candidate;
      } else {
        disconnected_node = i;
        // The pivot candidate is not in the "not" set. We need to pre-increment
        // the counter for the node to compensate for that.
        pre_increment = 1;
      }
    }
  }

  std::vector<int> new_candidates;
  new_candidates.reserve(num_input_candidates);
  for (int remaining_candidates = num_disconnected_candidates + pre_increment;
       remaining_candidates >= 1; remaining_candidates--) {
    // Swap a node that is disconnected from the pivot (or the pivot itself)
    // with the first candidate, so that we can later move it to "not" simply by
    // increasing the index of the first candidate that is not in "not".
    const int selected = input_candidates[disconnected_node];
    std::swap(input_candidates[disconnected_node],
              input_candidates[first_candidate_index]);

    // Fill the list of candidates and the "not" set for the recursive call:
    new_candidates.clear();
    for (int i = 0; i < first_candidate_index; ++i) {
      if (Connects(graph, selected, input_candidates[i])) {
        new_candidates.push_back(input_candidates[i]);
      }
    }
    const int new_first_candidate_index = new_candidates.size();
    for (int i = first_candidate_index + 1; i < num_input_candidates; ++i) {
      if (Connects(graph, selected, input_candidates[i])) {
        new_candidates.push_back(input_candidates[i]);
      }
    }
    const int new_candidate_size = new_candidates.size();

    // Add the selected candidate to the current clique.
    current_clique->push_back(selected);

    // If there are no remaining candidates, we have found a maximal clique.
    // Otherwise, do the recursive step.
    if (new_candidate_size == 0) {
      *stop = callback(*current_clique);
    } else {
      if (new_first_candidate_index < new_candidate_size) {
        Search(graph, callback, new_candidates.data(),
               new_first_candidate_index, new_candidate_size, current_clique,
               stop);
        if (*stop) {
          return;
        }
      }
    }

    // Remove the selected candidate from the current clique.
    current_clique->pop_back();
    // Add the selected candidate to the set "not" - we've already processed
    // all possible maximal cliques that use this node in 'current_clique'. The
    // current candidate is the element of the new candidate set, so we can move
    // it to "not" simply by increasing first_candidate_index.
    first_candidate_index++;

    // Find the next candidate that is disconnected from the pivot.
    if (remaining_candidates > 1) {
      disconnected_node = first_candidate_index;
      while (disconnected_node < num_input_candidates &&
             Connects(graph, pivot, input_candidates[disconnected_node])) {
        disconnected_node++;
      }
    }
  }
}

class FindAndEliminate {
 public:
  FindAndEliminate(std::function<bool(int, int)> graph, int node_count,
                   std::function<bool(const std::vector<int>&)> callback)
      : graph_(graph), node_count_(node_count), callback_(callback) {}

  bool GraphCallback(int node1, int node2) {
    if (visited_.find(
            std::make_pair(std::min(node1, node2), std::max(node1, node2))) !=
        visited_.end()) {
      return false;
    }
    return Connects(graph_, node1, node2);
  }

  bool SolutionCallback(const std::vector<int>& solution) {
    const int size = solution.size();
    if (size > 1) {
      for (int i = 0; i < size - 1; ++i) {
        for (int j = i + 1; j < size; ++j) {
          visited_.insert(std::make_pair(std::min(solution[i], solution[j]),
                                         std::max(solution[i], solution[j])));
        }
      }
      callback_(solution);
    }
    return false;
  }

 private:
  std::function<bool(int, int)> graph_;
  int node_count_;
  std::function<bool(const std::vector<int>&)> callback_;
  std::unordered_set<std::pair<int, int> > visited_;
};
}  // namespace

// This method implements the 'version2' of the Bron-Kerbosch
// algorithm to find all maximal cliques in a undirected graph.
void FindCliques(std::function<bool(int, int)> graph, int node_count,
                 std::function<bool(const std::vector<int>&)> callback) {
  std::unique_ptr<int[]> initial_candidates(new int[node_count]);
  std::vector<int> actual;

  for (int c = 0; c < node_count; ++c) {
    initial_candidates[c] = c;
  }

  bool stop = false;
  Search(graph, callback, initial_candidates.get(), 0, node_count, &actual,
         &stop);
}

void CoverArcsByCliques(std::function<bool(int, int)> graph, int node_count,
                        std::function<bool(const std::vector<int>&)> callback) {
  FindAndEliminate cache(graph, node_count, callback);
  std::unique_ptr<int[]> initial_candidates(new int[node_count]);
  std::vector<int> actual;

  std::function<bool(int, int)> cached_graph = [&cache](int i, int j) {
    return cache.GraphCallback(i, j);
  };
  std::function<bool(const std::vector<int>&)> cached_callback =
      [&cache](const std::vector<int>& res) { return cache.SolutionCallback(res); };

  for (int c = 0; c < node_count; ++c) {
    initial_candidates[c] = c;
  }

  bool stop = false;
  Search(cached_graph, cached_callback, initial_candidates.get(), 0, node_count,
         &actual, &stop);
}

}  // namespace operations_research
