// Copyright 2010-2021 Google LLC
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

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/integral_types.h"
#include "ortools/graph/shortestpaths.h"

namespace operations_research {
namespace {

// Priority queue element
class Element {
 public:
  bool operator<(const Element& other) const {
    return distance_ != other.distance_ ? distance_ > other.distance_
                                        : node_ > other.node_;
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  void set_distance(int64_t distance) { distance_ = distance; }
  int64_t distance() const { return distance_; }
  void set_node(int node) { node_ = node; }
  int node() const { return node_; }

 private:
  int64_t distance_ = 0;
  int heap_index_ = -1;
  int node_ = -1;
};
}  // namespace

template <class S>
class DijkstraSP {
 public:
  static constexpr int64_t kInfinity = std::numeric_limits<int64_t>::max() / 2;

  DijkstraSP(int node_count, int start_node,
             std::function<int64_t(int, int)> graph,
             int64_t disconnected_distance)
      : node_count_(node_count),
        start_node_(start_node),
        graph_(std::move(graph)),
        disconnected_distance_(disconnected_distance),
        predecessor_(new int[node_count]),
        elements_(node_count) {}

  bool ShortestPath(int end_node, std::vector<int>* nodes) {
    Initialize();
    bool found = false;
    while (!frontier_.IsEmpty()) {
      int64_t distance;
      int node = SelectClosestNode(&distance);
      if (distance == kInfinity) {
        found = false;
        break;
      } else if (node == end_node) {
        found = true;
        break;
      }
      Update(node);
    }
    if (found) {
      FindPath(end_node, nodes);
    }
    return found;
  }

 private:
  void Initialize() {
    for (int i = 0; i < node_count_; i++) {
      elements_[i].set_node(i);
      if (i == start_node_) {
        predecessor_[i] = -1;
        elements_[i].set_distance(0);
        frontier_.Add(&elements_[i]);
      } else {
        elements_[i].set_distance(kInfinity);
        predecessor_[i] = start_node_;
        not_visited_.insert(i);
      }
    }
  }

  int SelectClosestNode(int64_t* distance) {
    const int node = frontier_.Top()->node();
    *distance = frontier_.Top()->distance();
    frontier_.Pop();
    not_visited_.erase(node);
    added_to_the_frontier_.erase(node);
    return node;
  }

  void Update(int node) {
    for (const auto& other_node : not_visited_) {
      const int64_t graph_node_i = graph_(node, other_node);
      if (graph_node_i != disconnected_distance_) {
        if (added_to_the_frontier_.find(other_node) ==
            added_to_the_frontier_.end()) {
          frontier_.Add(&elements_[other_node]);
          added_to_the_frontier_.insert(other_node);
        }
        const int64_t other_distance =
            elements_[node].distance() + graph_node_i;
        if (elements_[other_node].distance() > other_distance) {
          elements_[other_node].set_distance(other_distance);
          frontier_.NoteChangedPriority(&elements_[other_node]);
          predecessor_[other_node] = node;
        }
      }
    }
  }

  void FindPath(int dest, std::vector<int>* nodes) {
    int j = dest;
    nodes->push_back(j);
    while (predecessor_[j] != -1) {
      nodes->push_back(predecessor_[j]);
      j = predecessor_[j];
    }
  }

  const int node_count_;
  const int start_node_;
  std::function<int64_t(int, int)> graph_;
  const int64_t disconnected_distance_;
  std::unique_ptr<int[]> predecessor_;
  AdjustablePriorityQueue<Element> frontier_;
  std::vector<Element> elements_;
  S not_visited_;
  S added_to_the_frontier_;
};

bool DijkstraShortestPath(int node_count, int start_node, int end_node,
                          std::function<int64_t(int, int)> graph,
                          int64_t disconnected_distance,
                          std::vector<int>* nodes) {
  DijkstraSP<absl::flat_hash_set<int>> bf(
      node_count, start_node, std::move(graph), disconnected_distance);
  return bf.ShortestPath(end_node, nodes);
}

bool StableDijkstraShortestPath(int node_count, int start_node, int end_node,
                                std::function<int64_t(int, int)> graph,
                                int64_t disconnected_distance,
                                std::vector<int>* nodes) {
  DijkstraSP<std::set<int>> bf(node_count, start_node, std::move(graph),
                               disconnected_distance);
  return bf.ShortestPath(end_node, nodes);
}
}  // namespace operations_research
