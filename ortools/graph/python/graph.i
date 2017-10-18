// Copyright 2010-2017 Google
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

// This .i files exposes some of the C++ classes in ../, namely :
// - SimpleMaxFlow, from ../max_flow.h
// - SimpleMinCostFlow, from ../min_cost_flow.h
// - LinearSumAssignment, from ../assignment.h
//
// USAGE EXAMPLES:
// - examples/python/pyflow_example.py
// - examples/python/linear_assignment_api.py
//
// The rest of the code of ../ was not deemed worth exporting to python.
// This could change; contact the code owners if you would like some C++
// API to be exposed here.
//
// TODO(user): test all the APIs that are currently marked as 'untested'.

%include "ortools/base/base.i"

%import "ortools/graph/ebert_graph.h"

// Convert the "std::vector<int>* result" parameters to python outputs.
// Note: the typemap itself is in "ortools/base/base.i".
%apply std::vector<int>* OUTPUT {std::vector<int>* result}
%apply std::vector<int>* OUTPUT {std::vector<int>* nodes}

%{
#include "ortools/graph/assignment.h"
#include "ortools/graph/max_flow.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/graph/shortestpaths.h"
%}

// ############ max_flow.h ############

%ignoreall

%unignore operations_research;
%unignore operations_research::SimpleMaxFlow;
%unignore operations_research::SimpleMaxFlow::SimpleMaxFlow;
%unignore operations_research::SimpleMaxFlow::~SimpleMaxFlow;
%unignore operations_research::SimpleMaxFlow::AddArcWithCapacity;
%unignore operations_research::SimpleMaxFlow::NumNodes;  // untested
%unignore operations_research::SimpleMaxFlow::NumArcs;
%unignore operations_research::SimpleMaxFlow::Tail;
%unignore operations_research::SimpleMaxFlow::Head;
%unignore operations_research::SimpleMaxFlow::Capacity;
// Expose the "operations_research::SimpleMaxFlow::Status" enum.
%unignore operations_research::SimpleMaxFlow::Status;
%unignore operations_research::SimpleMaxFlow::OPTIMAL;
%unignore operations_research::SimpleMaxFlow::POSSIBLE_OVERFLOW;  // untested
%unignore operations_research::SimpleMaxFlow::BAD_INPUT;  // untested
%unignore operations_research::SimpleMaxFlow::BAD_RESULT;  // untested
%unignore operations_research::SimpleMaxFlow::Solve;
%unignore operations_research::SimpleMaxFlow::OptimalFlow;
%unignore operations_research::SimpleMaxFlow::Flow;
%unignore operations_research::SimpleMaxFlow::GetSourceSideMinCut;  // untested
%unignore operations_research::SimpleMaxFlow::GetSinkSideMinCut;  // untested

%include "ortools/graph/max_flow.h"

%unignoreall

// ############ min_cost_flow.h ############

%ignoreall

%unignore operations_research;

// We prefer our users to access the Status enum via the "SimpleMinCostFlow"
// class, i.e. SimpleMinCostFlow.OPTIMAL. But since they're defined on a
// base class in the .h, we must expose it.
%unignore operations_research::MinCostFlowBase;
%unignore operations_research::MinCostFlowBase::Status;
%unignore operations_research::MinCostFlowBase::OPTIMAL;
%unignore operations_research::MinCostFlowBase::NOT_SOLVED;  // untested
%unignore operations_research::MinCostFlowBase::FEASIBLE;  // untested
%unignore operations_research::MinCostFlowBase::INFEASIBLE;  // untested
%unignore operations_research::MinCostFlowBase::UNBALANCED;  // untested
%unignore operations_research::MinCostFlowBase::BAD_RESULT;  // untested
%unignore operations_research::MinCostFlowBase::BAD_COST_RANGE;  // untested

%unignore operations_research::SimpleMinCostFlow;
%unignore operations_research::SimpleMinCostFlow::SimpleMinCostFlow;
%unignore operations_research::SimpleMinCostFlow::~SimpleMinCostFlow;
%unignore operations_research::SimpleMinCostFlow::AddArcWithCapacityAndUnitCost;
%unignore operations_research::SimpleMinCostFlow::SetNodeSupply;
%unignore operations_research::SimpleMinCostFlow::Solve;
// untested
%unignore operations_research::SimpleMinCostFlow::SolveMaxFlowWithMinCost;
%unignore operations_research::SimpleMinCostFlow::OptimalCost;
%unignore operations_research::SimpleMinCostFlow::MaximumFlow;  // untested
%unignore operations_research::SimpleMinCostFlow::Flow;
%unignore operations_research::SimpleMinCostFlow::NumNodes;  // untested
%unignore operations_research::SimpleMinCostFlow::NumArcs;
%unignore operations_research::SimpleMinCostFlow::Tail;
%unignore operations_research::SimpleMinCostFlow::Head;
%unignore operations_research::SimpleMinCostFlow::Capacity;  // untested
%unignore operations_research::SimpleMinCostFlow::Supply;  // untested
%unignore operations_research::SimpleMinCostFlow::UnitCost;

%include "ortools/graph/min_cost_flow.h"

%unignoreall

// ############ assignment.h ############

%ignoreall

%unignore operations_research;
// We only expose the C++ "operations_research::SimpleLinearSumAssignment"
// class, and we rename it "LinearSumAssignment".
%rename(LinearSumAssignment) operations_research::SimpleLinearSumAssignment;
%unignore operations_research::SimpleLinearSumAssignment::SimpleLinearSumAssignment;
%unignore operations_research::SimpleLinearSumAssignment::AddArcWithCost;
%unignore operations_research::SimpleLinearSumAssignment::NumNodes;
%unignore operations_research::SimpleLinearSumAssignment::NumArcs;  // untested
%unignore operations_research::SimpleLinearSumAssignment::LeftNode;  // untested
%unignore operations_research::SimpleLinearSumAssignment::RightNode;  // untested
%unignore operations_research::SimpleLinearSumAssignment::Cost;  // untested
%unignore operations_research::SimpleLinearSumAssignment::Status;
%unignore operations_research::SimpleLinearSumAssignment::OPTIMAL;
%unignore operations_research::SimpleLinearSumAssignment::INFEASIBLE;
%unignore operations_research::SimpleLinearSumAssignment::POSSIBLE_OVERFLOW;
%unignore operations_research::SimpleLinearSumAssignment::Solve;
%unignore operations_research::SimpleLinearSumAssignment::OptimalCost;
%unignore operations_research::SimpleLinearSumAssignment::RightMate;
%unignore operations_research::SimpleLinearSumAssignment::AssignmentCost;

%include "ortools/graph/assignment.h"


%unignore operations_research::DijkstraShortestPath;
%unignore operations_research::BellmanFordShortestPath;
%unignore operations_research::AStarShortestPath;

%include "ortools/graph/shortestpaths.h"

%unignoreall
