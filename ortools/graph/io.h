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

// DEPRECATED. Use ortools/base/io.h instead.

#ifndef OR_TOOLS_GRAPH_IO_H_
#define OR_TOOLS_GRAPH_IO_H_

#include "ortools/base/io.h"

namespace operations_research {

// enum
using util::GraphToStringFormat;
using util::PRINT_GRAPH_ADJACENCY_LISTS;
using util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED;
using util::PRINT_GRAPH_ARCS;

using util::GraphToString;
using util::ReadGraphFile;
using util::WriteGraphToFile;

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_IO_H_
