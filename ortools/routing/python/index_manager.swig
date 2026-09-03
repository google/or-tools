// Copyright 2010-2025 Google LLC
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

// Wrapper for IndexManager.

%include "ortools/routing/python/types.swig"

%{
#include "ortools/routing/index_manager.h"
%}

%ignoreall

%unignore operations_research;
%unignore operations_research::routing;

namespace operations_research::routing {

class NodeIndex;

%unignore IndexManager;
%unignore IndexManager::GetStartIndex;
%unignore IndexManager::GetEndIndex;
%unignore IndexManager::IndexToNode;
%unignore IndexManager::NodeToIndex;
%unignore IndexManager::IndexManager(
    int, int,
    NodeIndex);
%unignore IndexManager::IndexManager(
    int, int,
    const std::vector<NodeIndex>&,
    const std::vector<NodeIndex>&);
%rename (GetNumberOfNodes) IndexManager::num_nodes;
%rename (GetNumberOfVehicles) IndexManager::num_vehicles;
%rename (GetNumberOfIndices) IndexManager::num_indices;
%unignore IndexManager::~IndexManager;

}  // namespace operations_research::routing

%include "ortools/routing/index_manager.h"

%unignoreall
