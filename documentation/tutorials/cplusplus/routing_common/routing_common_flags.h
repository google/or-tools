// Copyright 2011-2014 Google
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
//
//
//  Common routing flags.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_FLAGS_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_FLAGS_H

#include "base/commandlineflags.h"

#include "common/constants.h"

DEFINE_int32(instance_size, 10, "Number of nodes, including the depot.");
DEFINE_string(instance_name, "Dummy instance", "Instance name.");

DEFINE_int32(instance_depot, 0, "Depot for instance.");

DEFINE_string(instance_file, "", "TSPLIB instance file.");
DEFINE_string(solution_file, "", "TSPLIB solution file.");

DEFINE_string(distance_file, "", "TSP matrix distance file.");


DEFINE_int32(edge_min, 1, "Minimum edge value.");
DEFINE_int32(edge_max, 100, "Maximum edge value.");


DEFINE_int32(instance_edges_percent, 20, "Percent of edges in the graph.");

DEFINE_int32(x_max, 100, "Maximum x coordinate.");
DEFINE_int32(y_max, 100, "Maximum y coordinate.");


DEFINE_int32(width_size, 6, "Width size of fields in output.");

DEFINE_int32(epix_width, 10, "Width of the pictures in cm.");
DEFINE_int32(epix_height, 10, "Height  of the pictures in cm.");

DEFINE_double(epix_radius, 0.3, "Radius of circles.");

DEFINE_bool(epix_node_labels, false, "Print node labels?");

DEFINE_int32(percentage_forbidden_arcs_max, 94, "Maximum percentage of arcs to forbid.");
DEFINE_int64(M, operations_research::kPostiveInfinityInt64, "Big m value to represent infinity.");

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_FLAGS_H