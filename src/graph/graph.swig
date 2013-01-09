// Copyright 2010-2012 Google
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
#include "graph/linear_assignment.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

#if defined(SWIGJAVA)
// Rename rules on EbertGraph.
%rename(reserve) operations_research::EbertGraphBase::Reserve;
%rename(numNodes) operations_research::StarGraphBase::num_nodes;
%rename(numArcs) operations_research::StarGraphBase::num_arcs;
%rename(maxNumNodes) operations_research::StarGraphBase::max_num_nodes;
%rename(maxNumArcs) operations_research::StarGraphBase::max_num_arcs;
%rename(addArc) operations_research::EbertGraphBase::AddArc;

// Rename rules on MaxFlow.
%rename (getSourceNodeIndex) operations_research::GenericMaxFlow::GetSourceNodeIndex;
%rename (getSinkNodeIndex) operations_research::GenericMaxFlow::GetSinkNodeIndex;
%rename (setArcCapacity) operations_research::GenericMaxFlow::SetArcCapacity;
%rename (setArcFlow) operations_research::GenericMaxFlow::SetArcFlow;
%rename (solve) operations_research::GenericMaxFlow::Solve;
%rename (getOptimalFlow) operations_research::GenericMaxFlow::GetOptimalFlow;
%rename (flow) operations_research::GenericMaxFlow::Flow;
%rename (capacity) operations_research::GenericMaxFlow::Capacity;

// Rename rules on MinCostFlow.
%rename(setNodeSupply) operations_research::GenericMinCostFlow::SetNodeSupply;
%rename(setArcUnitCost) operations_research::GenericMinCostFlow::SetArcUnitCost;
%rename(setArcCapacity) operations_research::GenericMinCostFlow::SetArcCapacity;
%rename(setArcFlow) operations_research::GenericMinCostFlow::SetArcFlow;
%rename(solve) operations_research::GenericMinCostFlow::Solve;
%rename(getOptimalCost) operations_research::GenericMinCostFlow::GetOptimalCost;
%rename(flow) operations_research::GenericMinCostFlow::Flow;
%rename(capacity) operations_research::GenericMinCostFlow::Capacity;
%rename(cost) operations_research::GenericMinCostFlow::Cost;
%rename(supply) operations_research::GenericMinCostFlow::Supply;

// Rename rules on SimpleMinCostFlow.
%rename(addArc) operations_research::SimpleMinCostFlow::AddArc;
%rename(setNodeSupply) operations_research::SimpleMinCostFlow::SetNodeSupply;
%rename(setArcUnitCost) operations_research::SimpleMinCostFlow::SetArcUnitCost;
%rename(setArcCapacity) operations_research::SimpleMinCostFlow::SetArcCapacity;
%rename(setArcFlow) operations_research::SimpleMinCostFlow::SetArcFlow;
%rename(solve) operations_research::SimpleMinCostFlow::Solve;
%rename(getOptimalCost) operations_research::SimpleMinCostFlow::GetOptimalCost;
%rename(getArcFlow) operations_research::SimpleMinCostFlow::Flow;
%rename(getNumNodes) operations_research::SimpleMinCostFlow::NumNodes;
%rename(getNumArcs) operations_research::SimpleMinCostFlow::NumArcs;
%rename(getArcTail) operations_research::SimpleMinCostFlow::Tail;
%rename(getArcHead) operations_research::SimpleMinCostFlow::Head;
%rename(getArcUnitCost) operations_research::SimpleMinCostFlow::UnitCost;
%rename(getNodeSupply) operations_research::SimpleMinCostFlow::Supply;

// Rename rules on LinearSumAssignment.
%rename(setCostScalingDivisor) operations_research::LinearSumAssignment::SetCostScalingDivisor;
%rename(optimizeGraphLayout) operations_research::LinearSumAssignment::OptimizeGraphLayout;
%rename(graph) operations_research::LinearSumAssignment::Graph;
%rename(head) operations_research::LinearSumAssignment::Head;
%rename(setArcCost) operations_research::LinearSumAssignment::SetArcCost;
%rename(finalizeSetup) operations_research::LinearSumAssignment::FinalizeSetup;
%rename(computeAssignment) operations_research::LinearSumAssignment::ComputeAssignment;
%rename(getCost) operations_research::LinearSumAssignment::GetCost;
%rename(numNodes) operations_research::LinearSumAssignment::NumNodes;
%rename(numLeftNodes) operations_research::LinearSumAssignment::NumLeftNodes;
%rename(getAssignmentArc) operations_research::LinearSumAssignment::GetAssignmentArc;
%rename(getAssignmentCost) operations_research::LinearSumAssignment::GetAssignmentCost;
%rename(getMate) operations_research::LinearSumAssignment::GetMate;
%rename(statsString) operations_research::LinearSumAssignment::StatsString;

#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
// Rename rules on EbertGraph.
%rename(EndArcIndex) operations_research::StarGraphBase::end_arc_index;
%rename(EndNodeIndex) operations_research::StarGraphBase::end_node_index;
%rename(MaxEndArcIndex) operations_research::StarGraphBase::max_end_arc_index;
%rename(MaxEndNodeIndex) operations_research::StarGraphBase::max_end_node_index;
%rename(MaxNumArcs) operations_research::StarGraphBase::max_num_arcs;
%rename(MaxNumArcs) operations_research::StarGraphBase::max_num_arcs;
%rename(MaxNumNodes) operations_research::StarGraphBase::max_num_nodes;
%rename(MaxNumNodes) operations_research::StarGraphBase::max_num_nodes;
%rename(NumArcs) operations_research::StarGraphBase::num_arcs;
%rename(NumNodes) operations_research::StarGraphBase::num_nodes;

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
%include "graph/linear_assignment.h"
%include "graph/max_flow.h"
%include "graph/min_cost_flow.h"

%template(StarStarGraphBase) operations_research::StarGraphBase<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(StarGraphCore) operations_research::EbertGraphBase<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(StarGraph) operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex>;
%template(ForwardStarStarGraphBase) operations_research::StarGraphBase<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(ForwardStarGraphCore) operations_research::EbertGraphBase<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(ForwardStarGraph) operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex>;
%template(ForwardStarStaticStarGraphBase) operations_research::StarGraphBase<operations_research::NodeIndex, operations_research::ArcIndex, operations_research::ForwardStaticGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(ForwardStarStaticGraph) operations_research::ForwardStaticGraph<operations_research::NodeIndex, operations_research::ArcIndex>;
%template(ForwardEbertLinearSumAssignment) operations_research::LinearSumAssignment<operations_research::ForwardEbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(EbertLinearSumAssignment) operations_research::LinearSumAssignment<operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(MaxFlow) operations_research::GenericMaxFlow<operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(MinCostFlow) operations_research::GenericMinCostFlow<operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
