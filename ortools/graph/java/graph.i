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

// This .i files exposes some of the C++ classes in ../, namely :
// - MaxFlow, from SimpleMaxFlow in ../max_flow.h
// - MinCostFlow, from SimpleMinCostFlow in ../min_cost_flow.h
// - LinearSumAssignment, from SimpleLinearSumAssignment in ../assignment.h
//
// The rest of the code of ../ was not deemed worth exporting to python.
// This could change; contact the code owners if you would like some C++
// API to be exposed here.
//
// USAGE EXAMPLES:
// - java/com/google/ortools/samples/FlowExample.java
// - java/com/google/ortools/samples/LinearAssignmentAPI.java
// - javatests/com/google/ortools/graph/FlowTest.java
//
// These examples are also used as unit tests.
//
// TODO(user): test all the APIs that are currently marked as 'untested'.

%include "ortools/base/base.i"

%import "ortools/graph/ebert_graph.h"

%{
#include "ortools/graph/assignment.h"
#include "ortools/graph/max_flow.h"
#include "ortools/graph/min_cost_flow.h"
%}

// ############ max_flow.h ############

%ignoreall

%unignore operations_research;
%rename (MaxFlow) operations_research::SimpleMaxFlow;
%unignore operations_research::SimpleMaxFlow::SimpleMaxFlow;
%unignore operations_research::SimpleMaxFlow::~SimpleMaxFlow;
%rename (addArcWithCapacity) operations_research::SimpleMaxFlow::AddArcWithCapacity;
%rename (getNumNodes) operations_research::SimpleMaxFlow::NumNodes;  // untested
%rename (getNumArcs) operations_research::SimpleMaxFlow::NumArcs;
%rename (getTail) operations_research::SimpleMaxFlow::Tail;
%rename (getHead) operations_research::SimpleMaxFlow::Head;
%rename (getCapacity) operations_research::SimpleMaxFlow::Capacity;
%rename (solve) operations_research::SimpleMaxFlow::Solve;
%rename (getOptimalFlow) operations_research::SimpleMaxFlow::OptimalFlow;
%rename (getFlow) operations_research::SimpleMaxFlow::Flow;
%rename (getSourceSideMinCut) operations_research::SimpleMaxFlow::GetSourceSideMinCut;  // untested
%rename (getSinkSideMinCut) operations_research::SimpleMaxFlow::GetSinkSideMinCut;  // untested

// To expose the Status enum's values (as constant integers), we must expose the enum
// type itself.
%unignore operations_research::SimpleMaxFlow::Status;
%unignore operations_research::SimpleMaxFlow::OPTIMAL;
%unignore operations_research::SimpleMaxFlow::POSSIBLE_OVERFLOW;  // untested
%unignore operations_research::SimpleMaxFlow::BAD_INPUT;  // untested
%unignore operations_research::SimpleMaxFlow::BAD_RESULT;  // untested

%include "ortools/graph/max_flow.h"

%unignoreall

// ############ min_cost_flow.h ############

%ignoreall

%unignore operations_research;

// Users shouldn't access MinCostFlowBase directly; we only expose it here
// because we must do so in order for some versions of SWIG (like the one
// used by or-tools) to expose the enum values (as constant integers) in
// the SimpleMinCostFlow class, via its inheritance from MinCostFlowBase.
%unignore operations_research::MinCostFlowBase;
%unignore operations_research::MinCostFlowBase::Status;
%unignore operations_research::MinCostFlowBase::NOT_SOLVED;  // untested
%unignore operations_research::MinCostFlowBase::OPTIMAL;
%unignore operations_research::MinCostFlowBase::FEASIBLE;  // untested
%unignore operations_research::MinCostFlowBase::INFEASIBLE;  // untested
%unignore operations_research::MinCostFlowBase::UNBALANCED;  // untested
%unignore operations_research::MinCostFlowBase::BAD_RESULT;  // untested
%unignore operations_research::MinCostFlowBase::BAD_COST_RANGE;  // untested

%rename (MinCostFlow) operations_research::SimpleMinCostFlow;
%unignore operations_research::SimpleMinCostFlow::SimpleMinCostFlow;
%unignore operations_research::SimpleMinCostFlow::~SimpleMinCostFlow;
%rename (addArcWithCapacityAndUnitCost)
    operations_research::SimpleMinCostFlow::AddArcWithCapacityAndUnitCost;
%rename (setNodeSupply) operations_research::SimpleMinCostFlow::SetNodeSupply;
%rename (solve) operations_research::SimpleMinCostFlow::Solve;
%rename (solveMaxFlowWithMinCost)
    operations_research::SimpleMinCostFlow::SolveMaxFlowWithMinCost;  // untested
%rename (getOptimalCost) operations_research::SimpleMinCostFlow::OptimalCost;
%rename (getMaximumFlow) operations_research::SimpleMinCostFlow::MaximumFlow;  // untested
%rename (getFlow) operations_research::SimpleMinCostFlow::Flow;
%rename (getNumNodes) operations_research::SimpleMinCostFlow::NumNodes;  // untested
%rename (getNumArcs) operations_research::SimpleMinCostFlow::NumArcs;
%rename (getTail) operations_research::SimpleMinCostFlow::Tail;
%rename (getHead) operations_research::SimpleMinCostFlow::Head;
%rename (getCapacity) operations_research::SimpleMinCostFlow::Capacity;  // untested
%rename (getSupply) operations_research::SimpleMinCostFlow::Supply;  // untested
%rename (getUnitCost) operations_research::SimpleMinCostFlow::UnitCost;

%include "ortools/graph/min_cost_flow.h"

%unignoreall

// ############ assignment.h ############

%ignoreall

%unignore operations_research;
// We only expose the C++ "operations_research::SimpleLinearSumAssignment"
// class, and we rename it "LinearSumAssignment".
%rename (LinearSumAssignment) operations_research::SimpleLinearSumAssignment;
%unignore operations_research::SimpleLinearSumAssignment::SimpleLinearSumAssignment;
%unignore operations_research::SimpleLinearSumAssignment::~SimpleLinearSumAssignment;
%rename (addArcWithCost) operations_research::SimpleLinearSumAssignment::AddArcWithCost;
%rename (getNumNodes) operations_research::SimpleLinearSumAssignment::NumNodes;
%rename (getNumArcs) operations_research::SimpleLinearSumAssignment::NumArcs;  // untested
%rename (getLeftNode) operations_research::SimpleLinearSumAssignment::LeftNode;  // untested
%rename (getRightNode) operations_research::SimpleLinearSumAssignment::RightNode;  // untested
%rename (getCost) operations_research::SimpleLinearSumAssignment::Cost;  // untested
%rename (solve) operations_research::SimpleLinearSumAssignment::Solve;
%rename (getOptimalCost) operations_research::SimpleLinearSumAssignment::OptimalCost;
%rename (getRightMate) operations_research::SimpleLinearSumAssignment::RightMate;
%rename (getAssignmentCost) operations_research::SimpleLinearSumAssignment::AssignmentCost;

// To expose the Status enum's values (as constant integers), we must expose the
// enum type itself.
%unignore operations_research::SimpleLinearSumAssignment::Status;
%unignore operations_research::SimpleLinearSumAssignment::OPTIMAL;
%unignore operations_research::SimpleLinearSumAssignment::INFEASIBLE;  // untested
%unignore operations_research::SimpleLinearSumAssignment::POSSIBLE_OVERFLOW;  // untested

%include "ortools/graph/assignment.h"

%unignoreall
