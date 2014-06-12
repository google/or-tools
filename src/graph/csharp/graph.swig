// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include util/csharp/data.swig

// First phase: #include the file we want to wrap.
%{
#include "util/zvector.h"
#include "graph/ebert_graph.h"
#include "graph/assignment.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

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
