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

// Unit test utilities related to graph.h.

#ifndef UTIL_GRAPH_TEST_UTIL_H_
#define UTIL_GRAPH_TEST_UTIL_H_

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "ortools/base/types.h"
#include "ortools/graph/graph.h"

namespace util {

// Generate a 2-dimensional undirected grid graph.
//
// Eg. for width=3, height=2, it generates this:
// 0 <---> 1 <---> 2
// ^       ^       ^
// |       |       |
// v       v       v
// 3 <---> 4 <---> 5
template <class Graph>
std::unique_ptr<Graph> Create2DGridGraph(int64_t width, int64_t height) {
  const int64_t num_arcs = 2L * ((width - 1) * height + width * (height - 1));
  auto graph = std::make_unique<Graph>(/*num_nodes=*/width * height,
                                       /*arc_capacity=*/num_arcs);
  // Add horizontal edges.
  for (int i = 0; i < height; ++i) {
    for (int j = 1; j < width; ++j) {
      const int left = i * width + (j - 1);
      const int right = i * width + j;
      graph->AddArc(left, right);
      graph->AddArc(right, left);
    }
  }
  // Add vertical edges.
  for (int i = 1; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int up = (i - 1) * width + j;
      const int down = i * width + j;
      graph->AddArc(up, down);
      graph->AddArc(down, up);
    }
  }
  graph->Build();
  return graph;
}

}  // namespace util

#endif  // UTIL_GRAPH_TEST_UTIL_H_
