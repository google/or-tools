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


#include "graph/connectivity.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"

namespace operations_research {

#define DCHECK_NODE_BOUNDS(node_index, num_nodes) \
  do {                                            \
    DCHECK_LE(0, node_index);                     \
    DCHECK_LT(node_index, num_nodes);             \
  } while (false)

void ConnectedComponents::Init(NodeIndex num_nodes) {
  CHECK_GE(num_nodes, 0);
  num_nodes_ = num_nodes;
  class_.Reserve(0, num_nodes_ - 1);
  class_size_.Reserve(0, num_nodes_ - 1);
  for (NodeIndex node = 0; node < num_nodes_; ++node) {
    class_.Set(node, node);
  }
  class_size_.SetAll(1);
}

void ConnectedComponents::AddArc(NodeIndex tail, NodeIndex head) {
  const NodeIndex tail_class = CompressPath(tail);
  const NodeIndex head_class = CompressPath(head);
  if (tail_class != head_class) {
    MergeClasses(tail_class, head_class);
  }
}

void ConnectedComponents::AddGraph(const StarGraph& graph) {
  Init(graph.max_num_nodes());
  for (StarGraph::ArcIterator arc_it(graph); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    AddArc(graph.Tail(arc), graph.Head(arc));
  }
}

NodeIndex ConnectedComponents::CompressPath(NodeIndex node) {
  DCHECK_NODE_BOUNDS(node, num_nodes_);
  while (node != class_[node]) {
    DCHECK_NODE_BOUNDS(class_[node], num_nodes_);
    DCHECK_NODE_BOUNDS(class_[class_[node]], num_nodes_);
    node = class_[node];
  }
  return node;
}

NodeIndex ConnectedComponents::GetClassRepresentative(NodeIndex node) {
  DCHECK_NODE_BOUNDS(node, num_nodes_);
  NodeIndex representative = node;
  while (class_[representative] != representative) {
    representative = class_[representative];
  }
  // The following line is an optimization that brings about 20% when using
  // Union-Find for counting the number of connected components with
  // GetNumberOfConnectedComponents.
  class_.Set(node, representative);
  return representative;
}

NodeIndex ConnectedComponents::GetNumberOfConnectedComponents() {
  std::vector<bool> seen(num_nodes_, false);
  NodeIndex number = 0;
  for (NodeIndex node = 0; node < num_nodes_; ++node) {
    NodeIndex representative = GetClassRepresentative(node);
    if (!seen[representative]) {
      seen[representative] = true;
      ++number;
    }
  }
  return number;
}

void ConnectedComponents::MergeClasses(NodeIndex node1, NodeIndex node2) {
  // It's faster (~10%) to swap the two values and have a single piece of
  // code for merging the classes.
  DCHECK_NODE_BOUNDS(node1, num_nodes_);
  DCHECK_NODE_BOUNDS(node2, num_nodes_);
  if (class_size_[node1] < class_size_[node2]) {
    std::swap(node1, node2);
  }
  class_.Set(node2, node1);
  class_size_.Set(node1, class_size_[node1] + class_size_[node2]);
}

#undef DCHECK_NODE_BOUNDS

}  // namespace operations_research
