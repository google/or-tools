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

#ifndef ORTOOLS_ROUTING_PYTHON_INDEX_MANAGER_DOC_H_
#define ORTOOLS_ROUTING_PYTHON_INDEX_MANAGER_DOC_H_
// clang-format off
/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define MKD_EXPAND(x)                                      x
#define MKD_COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define MKD_VA_SIZE(...)                                   MKD_EXPAND(MKD_COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define MKD_CAT1(a, b)                                     a ## b
#define MKD_CAT2(a, b)                                     MKD_CAT1(a, b)
#define MKD_DOC1(n1)                                       mkd_doc_##n1
#define MKD_DOC2(n1, n2)                                   mkd_doc_##n1##_##n2
#define MKD_DOC3(n1, n2, n3)                               mkd_doc_##n1##_##n2##_##n3
#define MKD_DOC4(n1, n2, n3, n4)                           mkd_doc_##n1##_##n2##_##n3##_##n4
#define MKD_DOC5(n1, n2, n3, n4, n5)                       mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5
#define MKD_DOC6(n1, n2, n3, n4, n5, n6)                   mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define MKD_DOC7(n1, n2, n3, n4, n5, n6, n7)               mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                           MKD_EXPAND(MKD_EXPAND(MKD_CAT2(MKD_DOC, MKD_VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *mkd_doc_operations_research_routing_IndexManager =
R"doc(Manager for any NodeIndex <-> variable index conversion.


The routing solver uses variable indices internally and through its
API. These variable indices are tricky to manage directly because one
Node can correspond to a multitude of variables, depending on the
number of times they appear in the model, and if they're used as start
and/or end points. This class aims to simplify variable index usage,
allowing users to use NodeIndex instead.

Usage:
  ```
{.cpp}
  auto starts_ends = ...;  /// These are NodeIndex.
  IndexManager manager(10, 4, starts_ends);  // 10 nodes, 4 vehicles.
  Model model(manager);
```




Then, use 'manager.NodeToIndex(node)' whenever model requires a
variable index.

Note: the mapping between node indices and variables indices is
      subject to
change so no assumption should be made on it. The only guarantee is
that indices range between 0 and n-1, where n = number of vehicles * 2
(for start and end nodes) + number of non-start or end nodes.)doc";

static const char *mkd_doc_operations_research_routing_IndexManager_GetEndIndex = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_GetIndexToNodeMap = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_GetStartIndex = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_IndexManager =
R"doc(Creates a NodeIndex to variable index mapping for a problem containing
'num_nodes', 'num_vehicles' and the given starts and ends for each
vehicle. If used, any start/end arrays have to have exactly
'num_vehicles' elements.
Args:
    num_nodes: Number of nodes in the problem.
    num_vehicles: Number of vehicles in the problem.
    depot: GetStartIndex "start" and GetEndIndex "end" NodeIndex for
           all vehicles.)doc";

static const char *mkd_doc_operations_research_routing_IndexManager_IndexManager_2 =
R"doc(Creates a NodeIndex to variable index mapping.
Args:
    num_nodes: Number of nodes in the problem.
    num_vehicles: Number of vehicles in the problem.
    starts: Array containing the start NodeIndex for each vehicle.
    ends: Array containing the end NodeIndex for each vehicle.

@note **starts** and **ends** arrays must have **exactly**
num_vehicles elements.)doc";

static const char *mkd_doc_operations_research_routing_IndexManager_IndexManager_3 =
R"doc(Creates a NodeIndex to variable index mapping.
Args:
    num_nodes: Number of nodes in the problem.
    num_vehicles: Number of vehicles in the problem.
    starts_ends: Array containing a pair [start,end] NodeIndex for
                 each vehicle.

@note **starts_ends** arrays must have **exactly** num_vehicles
elements.)doc";

static const char *mkd_doc_operations_research_routing_IndexManager_IndexToNode = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_IndicesToNodes = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_Initialize = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_NodeToIndex = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_NodesToIndices = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_index_to_node = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_node_to_index = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_indices = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_nodes = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_nodes_2 = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_unique_depots = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_unique_depots_2 = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_vehicles = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_num_vehicles_2 = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_vehicle_to_end = R"doc()doc";

static const char *mkd_doc_operations_research_routing_IndexManager_vehicle_to_start = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

// clang-format on
#endif  // ORTOOLS_ROUTING_PYTHON_INDEX_MANAGER_DOC_H_
