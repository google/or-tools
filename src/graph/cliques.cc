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


#include "graph/cliques.h"

#include <algorithm>
#include "base/hash.h"
#include "base/unique_ptr.h"
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/hash.h"

namespace operations_research {

namespace {
// TODO(user) : rewrite this algorithm without the recursivity.
void Search(ResultCallback2<bool, int, int>* const graph,
            ResultCallback1<bool, const std::vector<int>&>* const callback,
            int* input_candidates, int input_size, int input_candidate_size,
            std::vector<int>* actual, bool* stop) {
  std::vector<int> actual_candidates(input_candidate_size);
  int pre_increment = 0;
  int pivot = 0;
  int actual_candidate_size;
  int actual_size;
  int start = 0;
  int index = input_candidate_size;

  // Find Pivot.
  for (int i = 0; i < input_candidate_size && index != 0; ++i) {
    int p = input_candidates[i];
    int count = 0;
    int position = 0;

    // Count disconnections.
    for (int j = input_size; j < input_candidate_size && count < index; ++j) {
      if (!graph->Run(p, input_candidates[j])) {
        count++;
        // Save position of potential candidate.
        position = j;
      }
    }

    // Test new minimum.
    if (count < index) {
      pivot = p;
      index = count;

      if (i < input_size) {
        start = position;
      } else {
        start = i;
        // pre increment obvious candidate.
        pre_increment = 1;
      }
    }
  }

  // If fixed point initially chosen from candidates then
  // number of diconnections will be preincreased by one
  // Backtracking step for all nodes in the candidate list CD.
  for (int nod = index + pre_increment; nod >= 1; nod--) {
    // Swap.
    int selected = input_candidates[start];
    input_candidates[start] = input_candidates[input_size];
    input_candidates[input_size] = selected;

    // Fill new set "not".
    actual_candidate_size = 0;

    for (int i = 0; i < input_size; ++i) {
      if (graph->Run(selected, input_candidates[i])) {
        actual_candidates[actual_candidate_size++] = input_candidates[i];
      }
    }

    // Fill new set "candidates".
    actual_size = actual_candidate_size;

    for (int i = input_size + 1; i < input_candidate_size; ++i) {
      if (graph->Run(selected, input_candidates[i])) {
        actual_candidates[actual_size++] = input_candidates[i];
      }
    }

    // Add to "actual relevant nodes".
    actual->push_back(selected);

    // We have found a maximum clique.
    if (actual_size == 0) {
      *stop = callback->Run(*actual);
    } else {
      if (actual_candidate_size < actual_size) {
        Search(graph, callback, actual_candidates.data(), actual_candidate_size,
               actual_size, actual, stop);
        if (*stop) {
          return;
        }
      }
    }

    // move node from MD to ND
    // Remove from compsub
    actual->pop_back();

    // Add to "nod"
    input_size++;

    if (nod > 1) {
      // Select a candidate disgraph to the fixed point
      start = input_size;
      while (graph->Run(pivot, input_candidates[start])) {
        start++;
      }
    }
    // end selection
  }
}

class FindAndEliminate {
 public:
  FindAndEliminate(ResultCallback2<bool, int, int>* const graph, int node_count,
                   ResultCallback1<bool, const std::vector<int>&>* const callback)
      : graph_(graph), node_count_(node_count), callback_(callback) {}

  bool GraphCallback(int node1, int node2) {
    if (visited_.find(std::make_pair(std::min(node1, node2), std::max(node1, node2))) !=
        visited_.end()) {
      return false;
    }
    return graph_->Run(node1, node2);
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
      callback_->Run(solution);
    }
    return false;
  }

 private:
  ResultCallback2<bool, int, int>* const graph_;
  int node_count_;
  ResultCallback1<bool, const std::vector<int>&>* const callback_;
#if defined(_MSC_VER)
  hash_set<std::pair<int, int>, PairIntHasher> visited_;
#else
  hash_set<std::pair<int, int> > visited_;
#endif
};
}  // namespace

// This method implements the 'version2' of the Bron-Kerbosch
// algorithm to find all maximal cliques in a undirected graph.
void FindCliques(ResultCallback2<bool, int, int>* const graph, int node_count,
                 ResultCallback1<bool, const std::vector<int>&>* const callback) {
  graph->CheckIsRepeatable();
  callback->CheckIsRepeatable();
  std::unique_ptr<int[]> initial_candidates(new int[node_count]);
  std::vector<int> actual;

  std::unique_ptr<ResultCallback2<bool, int, int> > graph_deleter(graph);
  std::unique_ptr<ResultCallback1<bool, const std::vector<int>&> > callback_deleter(
      callback);

  for (int c = 0; c < node_count; ++c) {
    initial_candidates[c] = c;
  }

  bool stop = false;
  Search(graph, callback, initial_candidates.get(), 0, node_count, &actual,
         &stop);
}

void CoverArcsByCliques(
    ResultCallback2<bool, int, int>* const graph, int node_count,
    ResultCallback1<bool, const std::vector<int>&>* const callback) {
  graph->CheckIsRepeatable();
  callback->CheckIsRepeatable();

  std::unique_ptr<ResultCallback2<bool, int, int> > graph_deleter(graph);
  std::unique_ptr<ResultCallback1<bool, const std::vector<int>&> > callback_deleter(
      callback);

  FindAndEliminate cache(graph, node_count, callback);
  std::unique_ptr<int[]> initial_candidates(new int[node_count]);
  std::vector<int> actual;

  std::unique_ptr<ResultCallback2<bool, int, int> > cached_graph(
      NewPermanentCallback(&cache, &FindAndEliminate::GraphCallback));
  std::unique_ptr<ResultCallback1<bool, const std::vector<int>&> > cached_callback(
      NewPermanentCallback(&cache, &FindAndEliminate::SolutionCallback));

  for (int c = 0; c < node_count; ++c) {
    initial_candidates[c] = c;
  }

  bool stop = false;
  Search(cached_graph.get(), cached_callback.get(), initial_candidates.get(), 0,
         node_count, &actual, &stop);
}

}  // namespace operations_research
