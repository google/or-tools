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
#include <memory>
#include <vector>

#include "ortools/base/integral_types.h"

namespace operations_research {
class BellmanFord {
 public:
  static const int64 kInfinity = kint64max / 2;

  BellmanFord(int node_count, int start_node,
              std::function<int64(int, int)> graph, int64 disconnected_distance)
      : node_count_(node_count),
        start_node_(start_node),
        graph_(std::move(graph)),
        disconnected_distance_(disconnected_distance),
        distance_(new int64[node_count_]),
        predecessor_(new int[node_count_]) {}
  bool ShortestPath(int end_node, std::vector<int>* nodes);

 private:
  void Initialize();
  void Update();
  bool Check() const;
  void FindPath(int dest, std::vector<int>* nodes) const;

  const int node_count_;
  const int start_node_;
  std::function<int64(int, int)> graph_;
  const int64 disconnected_distance_;
  std::unique_ptr<int64[]> distance_;
  std::unique_ptr<int[]> predecessor_;
};

void BellmanFord::Initialize() {
  for (int i = 0; i < node_count_; i++) {
    distance_[i] = kint64max / 2;
    predecessor_[i] = -1;
  }
  distance_[start_node_] = 0;
}

void BellmanFord::Update() {
  for (int i = 0; i < node_count_ - 1; i++) {
    for (int u = 0; u < node_count_; u++) {
      for (int v = 0; v < node_count_; v++) {
        const int64 graph_u_v = graph_(u, v);
        if (graph_u_v != disconnected_distance_) {
          const int64 other_distance = distance_[u] + graph_u_v;
          if (distance_[v] > other_distance) {
            distance_[v] = other_distance;
            predecessor_[v] = u;
          }
        }
      }
    }
  }
}

bool BellmanFord::Check() const {
  for (int u = 0; u < node_count_; u++) {
    for (int v = 0; v < node_count_; v++) {
      const int graph_u_v = graph_(u, v);
      if (graph_u_v != disconnected_distance_) {
        if (distance_[v] > distance_[u] + graph_u_v) {
          return false;
        }
      }
    }
  }
  return true;
}

void BellmanFord::FindPath(int dest, std::vector<int>* nodes) const {
  int j = dest;
  nodes->push_back(j);
  while (predecessor_[j] != -1) {
    nodes->push_back(predecessor_[j]);
    j = predecessor_[j];
  }
}

bool BellmanFord::ShortestPath(int end_node, std::vector<int>* nodes) {
  Initialize();
  Update();
  if (distance_[end_node] == kInfinity) {
    return false;
  }
  if (!Check()) {
    return false;
  }
  FindPath(end_node, nodes);
  return true;
}

bool BellmanFordShortestPath(int node_count, int start_node, int end_node,
                             std::function<int64(int, int)> graph,
                             int64 disconnected_distance,
                             std::vector<int>* nodes) {
  BellmanFord bf(node_count, start_node, std::move(graph),
                 disconnected_distance);
  return bf.ShortestPath(end_node, nodes);
}
}  // namespace operations_research
