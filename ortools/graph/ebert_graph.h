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

// DEPRECATED: Use the graph types in //util/graph instead.

#ifndef OR_TOOLS_GRAPH_EBERT_GRAPH_H_
#define OR_TOOLS_GRAPH_EBERT_GRAPH_H_

#include <cstdint>

namespace operations_research {

// DEPRECATED: Global node and arc types for graphs. This have been retired in
// favor of parameterizing the graph types in //util/graph.
typedef int32_t NodeIndex;
typedef int32_t ArcIndex;

// DEPRECATED: Global types for flow algorithms. Thes have been retired in favor
// of directly parameterizing those algorithms.
typedef int64_t FlowQuantity;
typedef int64_t CostValue;

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_EBERT_GRAPH_H_
