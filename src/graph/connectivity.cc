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

void ConnectedComponents::Init(NodeIndex min_index, NodeIndex max_index) {
  CHECK_LT(1, max_index - min_index);
  CHECK_LE(0, min_index);
  min_index_ = min_index;
  max_index_ = max_index;
  class_.Reserve(min_index, max_index);
  class_size_.Reserve(min_index, max_index);
  for (NodeIndex node = min_index; node <= max_index; ++node) {
    class_.Set(node, node);
  }
  class_size_.SetAll(1);
}

void ConnectedComponents::AddArc(NodeIndex tail, NodeIndex head) {
  max_seen_index_ = std::max(max_seen_index_, tail);
  max_seen_index_ = std::max(max_seen_index_, head);
  NodeIndex tail_class = CompressPath(tail);
  NodeIndex head_class = CompressPath(head);
  if (tail_class != head_class) {
    MergeClasses(tail_class, head_class);
  }
}

void ConnectedComponents::AddGraph(const StarGraph& graph) {
  Init(StarGraph::kFirstNode, graph.num_nodes());
  for (StarGraph::ArcIterator arc_it(graph); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    AddArc(graph.Tail(arc), graph.Head(arc));
  }
  max_seen_index_ = graph.num_nodes() - 1;
}

NodeIndex ConnectedComponents::CompressPath(NodeIndex node) {
  DCHECK_LE(min_index_, node);
  DCHECK_GE(max_index_, node);
  while (node != class_[node]) {
    DCHECK_LE(min_index_, class_[node]);
    DCHECK_GE(max_index_, class_[node]);
    DCHECK_LE(min_index_, class_[class_[node]]);
    DCHECK_GE(max_index_, class_[class_[node]]);
    class_.Set(node, class_[class_[node]]);
    node = class_[node];
  }
  return node;
}

NodeIndex ConnectedComponents::GetClassRepresentative(NodeIndex node) {
  DCHECK_LE(min_index_, node);
  DCHECK_GE(max_index_, node);
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
  std::vector<bool> seen(max_index_);
  NodeIndex number = 0;
  for (NodeIndex node = min_index_; node <= max_seen_index_; ++node) {
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
  DCHECK_LE(min_index_, node1);
  DCHECK_GE(max_index_, node1);
  DCHECK_LE(min_index_, node2);
  DCHECK_GE(max_index_, node2);
  if (class_size_[node1] < class_size_[node2]) {
    std::swap(node1, node2);
  }
  class_.Set(node2, node1);
  class_size_.Set(node1, class_size_[node1] + class_size_[node2]);
}

}  // namespace operations_research
