// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include util/java/data.swig

// First phase: #include the file we want to wrap.
%{
#include "util/zvector.h"
#include "graph/ebert_graph.h"
#include "graph/assignment.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

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

// Rename rules on SimpleMaxFlow.
%rename(addArcWithCapacity) operations_research::SimpleMaxFlow::AddArcWithCapacity;
%rename(getNumNodes) operations_research::SimpleMaxFlow::NumNodes;
%rename(getNumArcs) operations_research::SimpleMaxFlow::NumArcs;
%rename(getTail) operations_research::SimpleMaxFlow::Tail;
%rename(getHead) operations_research::SimpleMaxFlow::Head;
%rename(getCapacity) operations_research::SimpleMaxFlow::Capacity;
%rename(solve) operations_research::SimpleMaxFlow::Solve;
%rename(getOptimalFlow) operations_research::SimpleMaxFlow::OptimalFlow;
%rename(getFlow) operations_research::SimpleMaxFlow::Flow;
%rename(getSourceSideMinCut) operations_research::SimpleMaxFlow::GetSourceSideMinCut;
%rename(getSinkSideMinCut) operations_research::SimpleMaxFlow::GetSinkSideMinCut;

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
%rename(addArcWithCapacityAndUnitCost) operations_research::SimpleMinCostFlow::AddArcWithCapacityAndUnitCost;
%rename(setNodeSupply) operations_research::SimpleMinCostFlow::SetNodeSupply;
%rename(solve) operations_research::SimpleMinCostFlow::Solve;
%rename(getOptimalCost) operations_research::SimpleMinCostFlow::OptimalCost;
%rename(getNumNodes) operations_research::SimpleMinCostFlow::NumNodes;
%rename(getNumArcs) operations_research::SimpleMinCostFlow::NumArcs;
%rename(getTail) operations_research::SimpleMinCostFlow::Tail;
%rename(getHead) operations_research::SimpleMinCostFlow::Head;
%rename(getFlow) operations_research::SimpleMinCostFlow::Flow;
%rename(getCapacity) operations_research::SimpleMinCostFlow::Capacity;
%rename(getUnitCost) operations_research::SimpleMinCostFlow::UnitCost;
%rename(getSupply) operations_research::SimpleMinCostFlow::Supply;

// Rename rules on SimpleLinearSumAssignment.
%rename(addArcWithCost) operations_research::SimpleLinearSumAssignment::AddArcWithCost;
%rename(getNumNodes) operations_research::SimpleLinearSumAssignment::NumNodes;
%rename(getNumArcs) operations_research::SimpleLinearSumAssignment::NumArcs;
%rename(getLeftNode) operations_research::SimpleLinearSumAssignment::LeftNode;
%rename(getRightNode) operations_research::SimpleLinearSumAssignment::RightNode;
%rename(getCost) operations_research::SimpleLinearSumAssignment::Cost;
%rename(solve) operations_research::SimpleLinearSumAssignment::Solve;
%rename(getOptimalCost) operations_research::SimpleLinearSumAssignment::OptimalCost;
%rename(getRightMate) operations_research::SimpleLinearSumAssignment::RightMate;
%rename(getAssignmentCost) operations_research::SimpleLinearSumAssignment::AssignmentCost;

// Use the SimpleLinearSumAssignment.
%rename(ComplexLinearSumAssignment) operations_research::LinearSumAssignment;
%rename(LinearSumAssignment) operations_research::SimpleLinearSumAssignment;

// Second phase: %include the same files.
%include "util/zvector.h"
%include "graph/ebert_graph.h"
%include "graph/assignment.h"
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
%template(MaxFlow) operations_research::GenericMaxFlow<operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex> >;
%template(MinCostFlow) operations_research::GenericMinCostFlow<operations_research::EbertGraph<operations_research::NodeIndex, operations_research::ArcIndex>, operations_research::FlowQuantity, operations_research::CostValue>;
