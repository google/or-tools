// Copyright 2010-2022 Google LLC
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

// This code builds an arc-flow generator for vector-bin-packing problems.
//   see https://people.math.gatech.edu/~tetali/PUBLIS/CKPT.pdf
// It implements a non-recursive version of algorithm 1 described in:
//   http://www.dcc.fc.up.pt/~fdabrandao/papers/arcflow_manuscript.pdf
// And in (poster version):
//   http://www.dcc.fc.up.pt/~fdabrandao/papers/arcflow_poster.pdf
// Available at:
//   https://drive.google.com/open?id=1y-Vs1orv-QHO4lb2sjVWrZr9GQd5d2st
//   https://drive.google.com/open?id=1fsWRqgNJ_3ClrhoKIeVc1EOd5s8Mj33i (poster)
// Some improvements are not yet implemented:
//   - Lifted stated: when storing a state of the dynamic programming forward
//     pass, one can lift a state. A lifted state of a state S is a maximal
//     increase of S that does not lose any state in the forward pass.
//     A simple example is the following:
//       bin, 1 dimension, capacity 5
//       2 item of size 2.
//       After adding item 1 in the DP pass, the state is (2).
//       The lifted state is (3) that is (5) - (2) which is the maximal increase
//       of (2) that does not loose any state.
//     To limit time spent computing this, one can lift a state only if the
//     remaining number of item is below a threshold.
//   - Disable the backward pass (compress state towards the bin capacity).
//     Although this reduces the graph a lot, this simplication is not valid
//     when the cost is not the number of bins, but a function of the capacity
//     used (useful for fair allocation).

#ifndef OR_TOOLS_PACKING_ARC_FLOW_BUILDER_H_
#define OR_TOOLS_PACKING_ARC_FLOW_BUILDER_H_

#include <cstdint>
#include <set>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/integral_types.h"

namespace operations_research {
namespace packing {

// Arc flow gragh built from a vector bin packing problem.
// The first node will always be the source. The last will always be the sink
// of the arc-flow graph.
struct ArcFlowGraph {
  struct Arc {
    int source;
    int destination;
    int item_index;

    // Needed for std::set.
    bool operator<(const Arc& other) const;
  };

  std::vector<Arc> arcs;
  // All the nodes explored during the DP phase.
  // In the forward pass, these are the consumed capacity of the bin at this
  // state. In the backward pass, this is pushed up towards the max capacity
  // of the bin. In the final compression phase, this is pushed down towards
  // the initial zero state.
  std::vector<std::vector<int>> nodes;
  // Debug info.
  int64_t num_dp_states;
};

// Main method.

// Arc flow builder. The input must enforce the following constraints:
//  - item_dimensions_by_type.size() == demand_by_type.size() == num types
//  - for each type t:
//       item_dimensions_by_type[t].size() == bin_dimensions.size() ==
//           num_dimensions
ArcFlowGraph BuildArcFlowGraph(
    const std::vector<int>& bin_dimensions,
    const std::vector<std::vector<int>>& item_dimensions_by_type,
    const std::vector<int>& demand_by_type);

}  // namespace packing
}  // namespace operations_research

#endif  // OR_TOOLS_PACKING_ARC_FLOW_BUILDER_H_
