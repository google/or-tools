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

#ifndef OR_TOOLS_GRAPH_MINIMUM_VERTEX_COVER_H_
#define OR_TOOLS_GRAPH_MINIMUM_VERTEX_COVER_H_

#include <vector>

namespace operations_research {

// This method computes a minimum vertex cover for the bipartite graph.
//
// If we define num_left=left_to_right_arcs.size(), the "left" nodes are
// integers in [0, num_left), and the "right" nodes are integers in [num_left,
// num_left + num_right).
//
// Returns a vector of size num_left+num_right, such that element #l is true if
// it is part of the minimum vertex cover and false if it is part of the maximum
// independent set (one is the complement of the other).
std::vector<bool> BipartiteMinimumVertexCover(
    const std::vector<std::vector<int>>& left_to_right_arcs, int num_right);

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_MINIMUM_VERTEX_COVER_H_
