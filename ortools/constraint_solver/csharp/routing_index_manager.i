// Copyright 2010-2018 Google LLC
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

// Wrapper for RoutingIndexManager.

%include "ortools/base/base.i"
%include "ortools/util/csharp/vector.i"

%{
#include "ortools/constraint_solver/routing_index_manager.h"
%}

DEFINE_INDEX_TYPE_TYPEDEF(operations_research::RoutingNodeIndex,
                          operations_research::RoutingIndexManager::NodeIndex);

%ignoreall

%unignore operations_research;
%unignore operations_research::RoutingIndexManager;
%unignore operations_research::RoutingIndexManager::IndexToNode;
%unignore operations_research::RoutingIndexManager::NodeToIndex;
%unignore operations_research::RoutingIndexManager::NodesToIndices;
%unignore operations_research::RoutingIndexManager::RoutingIndexManager(int, int, NodeIndex);
%unignore operations_research::RoutingIndexManager::RoutingIndexManager(int, int, const std::vector<NodeIndex>&, const std::vector<NodeIndex>&);
%unignore operations_research::RoutingIndexManager::~RoutingIndexManager;

%include "ortools/constraint_solver/routing_index_manager.h"

%unignoreall
