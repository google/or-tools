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

#include "ortools/graph/dag_shortest_path.h"

#include <vector>

#include "absl/types/span.h"

namespace operations_research {

PathWithLength<double> ShortestPathsOnDag(
    const int num_nodes,
    absl::Span<const ArcWithLength<double>> arcs_with_length, const int source,
    const int destination) {
  return ShortestPathsOnDag<double>(num_nodes, arcs_with_length, source,
                                    destination);
}

std::vector<PathWithLength<double>> KShortestPathsOnDag(
    const int num_nodes,
    absl::Span<const ArcWithLength<double>> arcs_with_length, const int source,
    const int destination, const int path_count) {
  return KShortestPathsOnDag<double>(num_nodes, arcs_with_length, source,
                                     destination, path_count);
}

}  // namespace operations_research
