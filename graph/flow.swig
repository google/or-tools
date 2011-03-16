// Copyright 2010 Google
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

%include util/data.swig

// Include the file we want to wrap a first time.
%{
#include "util/packed_array.h"
#include "graph/ebert_graph.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

%include "util/packed_array.h"
%include "graph/ebert_graph.h"
%include "graph/max_flow.h"
%include "graph/min_cost_flow.h"

namespace operations_research {
%template(StarGraph) EbertGraph<kStarGraphNodeIndexSize, kStarGraphArcIndexSize>;
%template(NodeIndexArray) PackedArray<kStarGraphNodeIndexSize>;
%template(ArcIndexArray) PackedArray<kStarGraphArcIndexSize>;
}
