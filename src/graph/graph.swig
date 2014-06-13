// Copyright 2010-2013 Google
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
#include "graph/assignment.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"
%}

// Instructs swig to ignore the std::vector<int>* parameters as inputs since
// they are actually outputs. Note that the name "result" is important and
// only such parameters will be ignored.
%typemap(in, numinputs=0) std::vector<int>* result(std::vector<int> temp) {
  $1 = &temp;
}

// Apply std::vector<int> to python list conversion.
%apply std::vector<int>* OUTPUT {std::vector<int>* result}

// Use the SimpleLinearSumAssignment in SWIG (java and python).
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
