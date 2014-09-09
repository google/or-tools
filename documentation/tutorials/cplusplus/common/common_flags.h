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
//  Common flags.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_COMMON_FLAGS_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_COMMON_FLAGS_H

#include "base/commandlineflags.h"

#include "common/constants.h"

DEFINE_bool(deterministic_random_seed, true,
            "Use deterministic random seeds or not?");

DEFINE_int32(random_generated_graph_max_edges_percent, 80, "Maximal percent of edges allowed for a randomly generated graph.");
DEFINE_bool(print_graph_labels, true, "Print graph labels for nodes?");

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_COMMON_FLAGS_H