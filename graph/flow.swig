// Copyright 2010-2011 Google
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

// First phase: #include the file we want to wrap.
%{
#include "util/zvector.h"
#include "graph/ebert_graph.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

#if defined(SWIGJAVA)
// Rename rules on EbertGraph.
%rename(reserve) operations_research::EbertGraphCore::Reserve;
%rename(numNodes) operations_research::EbertGraphCore::num_nodes;
%rename(numArcs) operations_research::EbertGraphCore::num_arcs;
%rename(maxNumNodes) operations_research::EbertGraphCore::max_num_nodes;
%rename(maxNumArcs) operations_research::EbertGraphCore::max_num_arcs;
%rename(addArc) operations_research::EbertGraphCore::AddArc;

// Rename rules on MaxFlow.
%rename (getSourceNodeIndex) operations_research::MaxFlow::GetSourceNodeIndex;
%rename (getSinkNodeIndex) operations_research::MaxFlow::GetSinkNodeIndex;
%rename (setArcCapacity) operations_research::MaxFlow::SetArcCapacity;
%rename (setArcFlow) operations_research::MaxFlow::SetArcFlow;
%rename (solve) operations_research::MaxFlow::Solve;
%rename (getOptimalFlow) operations_research::MaxFlow::GetOptimalFlow;
%rename (flow) operations_research::MaxFlow::Flow;
%rename (capacity) operations_research::MaxFlow::Capacity;

// Rename rules on MinCostFlow.
%rename(setNodeSupply) operations_research::MinCostFlow::SetNodeSupply;
%rename(setArcUnitCost) operations_research::MinCostFlow::SetArcUnitCost;
%rename(setArcCapacity) operations_research::MinCostFlow::SetArcCapacity;
%rename(setArcFlow) operations_research::MinCostFlow::SetArcFlow;
%rename(solve) operations_research::MinCostFlow::Solve;
%rename(getOptimalCost) operations_research::MinCostFlow::GetOptimalCost;
%rename(flow) operations_research::MinCostFlow::Flow;
%rename(capacity) operations_research::MinCostFlow::Capacity;
%rename(cost) operations_research::MinCostFlow::Cost;
%rename(supply) operations_research::MinCostFlow::Supply;
#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
// Rename rules on EbertGraph.
%rename(EndArcIndex) operations_research::EbertGraphCore::end_arc_index;
%rename(EndNodeIndex) operations_research::EbertGraphCore::end_node_index;
%rename(MaxEndArcIndex) operations_research::EbertGraphCore::max_end_arc_index;
%rename(MaxEndNodeIndex) operations_research::EbertGraphCore::max_end_node_index;
%rename(MaxNumArcs) operations_research::EbertGraphCore::max_num_arcs;
%rename(MaxNumArcs) operations_research::EbertGraphCore::max_num_arcs;
%rename(MaxNumNodes) operations_research::EbertGraphCore::max_num_nodes;
%rename(MaxNumNodes) operations_research::EbertGraphCore::max_num_nodes;
%rename(NumArcs) operations_research::EbertGraphCore::num_arcs;
%rename(NumNodes) operations_research::EbertGraphCore::num_nodes;

// Rename rules on MaxFlow.
%rename (Graph) operations_research::MaxFlow::graph;
%rename (GetStatus) operations_research::MaxFlow::status;

// Rename rules on MinCostFlow.
%rename (Graph) operations_research::MinCostFlow::graph;
%rename (GetStatus) operations_research::MinCostFlow::status;
#endif  // SWIGCSHARP

// Second phase: %include the same files.

%include "util/zvector.h"
%include "graph/ebert_graph.h"
%include "graph/max_flow.h"
%include "graph/min_cost_flow.h"

%template(StarGraphCore) operations_research::EbertGraphCore<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(StarGraph) operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex>;
%template(ForwardStarGraphCore) operations_research::EbertGraphCore<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(ForwardStarGraph) operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex>;
