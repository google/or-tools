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


#include <functional>
#include <unordered_set>
#include <memory>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/adjustable_priority_queue.h"

namespace operations_research {
namespace {

// Priority queue element
class Element {
 public:
  Element() : heap_index_(-1), distance_(0), node_(-1) {}
  bool operator<(const Element& other) const {
    return distance_ > other.distance_;
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  void set_distance(int64 distance) { distance_ = distance; }
  int64 distance() const { return distance_; }
  void set_node(int node) { node_ = node; }
  int node() const { return node_; }

 private:
  int heap_index_;
  int64 distance_;
  int node_;
};
}  // namespace

class DijkstraSP {
 public:
  static const int64 kInfinity = kint64max / 2;

  DijkstraSP(int node_count, int start_node,
             std::function<int64(int, int)> graph, int64 disconnected_distance)
      : node_count_(node_count),
        start_node_(start_node),
        graph_(std::move(graph)),
        disconnected_distance_(disconnected_distance),
        predecessor_(new int[node_count]),
        elements_(node_count) {}
  bool ShortestPath(int end_node, std::vector<int>* nodes);

 private:
  void Initialize();
  int SelectClosestNode(int64* distance);
  void Update(int label);
  void FindPath(int dest, std::vector<int>* nodes);

  const int node_count_;
  const int start_node_;
  std::function<int64(int, int)> graph_;
  const int64 disconnected_distance_;
  std::unique_ptr<int[]> predecessor_;
  AdjustablePriorityQueue<Element> frontier_;
  std::vector<Element> elements_;
  std::unordered_set<int> not_visited_;
  std::unordered_set<int> added_to_the_frontier_;
};

void DijkstraSP::Initialize() {
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

int DijkstraSP::SelectClosestNode(int64* distance) {
  const int node = frontier_.Top()->node();
  *distance = frontier_.Top()->distance();
  frontier_.Pop();
  not_visited_.erase(node);
  added_to_the_frontier_.erase(node);
  return node;
}

void DijkstraSP::Update(int node) {
  for (std::unordered_set<int>::const_iterator it = not_visited_.begin();
       it != not_visited_.end(); ++it) {
    const int other_node = *it;
    const int64 graph_node_i = graph_(node, other_node);
    if (graph_node_i != disconnected_distance_) {
      if (added_to_the_frontier_.find(other_node) ==
          added_to_the_frontier_.end()) {
        frontier_.Add(&elements_[other_node]);
        added_to_the_frontier_.insert(other_node);
      }
      const int64 other_distance = elements_[node].distance() + graph_node_i;
      if (elements_[other_node].distance() > other_distance) {
        elements_[other_node].set_distance(other_distance);
        frontier_.NoteChangedPriority(&elements_[other_node]);
        predecessor_[other_node] = node;
      }
    }
  }
}

void DijkstraSP::FindPath(int dest, std::vector<int>* nodes) {
  int j = dest;
  nodes->push_back(j);
  while (predecessor_[j] != -1) {
    nodes->push_back(predecessor_[j]);
    j = predecessor_[j];
  }
}

bool DijkstraSP::ShortestPath(int end_node, std::vector<int>* nodes) {
  Initialize();
  bool found = false;
  while (!frontier_.IsEmpty()) {
    int64 distance;
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

bool DijkstraShortestPath(int node_count, int start_node, int end_node,
                          std::function<int64(int, int)> graph,
                          int64 disconnected_distance,
                          std::vector<int>* nodes) {
  DijkstraSP bf(node_count, start_node, std::move(graph),
                disconnected_distance);
  return bf.ShortestPath(end_node, nodes);
}
}  // namespace operations_research
