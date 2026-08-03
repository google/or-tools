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

%include "ortools/routing/csharp/types.i"

%{
#include "ortools/routing/index_manager.h"
%}

%ignoreall

%unignore operations_research::routing;
namespace operations_research::routing {
%unignore IndexManager;
%unignore IndexManager::~IndexManager;
%unignore IndexManager::IndexManager(
    int, int,
    NodeIndex);
%unignore IndexManager::IndexManager(
    int, int,
    const std::vector<NodeIndex>&,
    const std::vector<NodeIndex>&);
%unignore IndexManager::GetStartIndex;
%unignore IndexManager::GetEndIndex;
%rename (GetNumberOfNodes) IndexManager::num_nodes;
%rename (GetNumberOfVehicles) IndexManager::num_vehicles;
%rename (GetNumberOfIndices) IndexManager::num_indices;
%rename (GetNumberOfUniqueDepots) IndexManager::num_unique_depots;
%unignore IndexManager::IndexToNode;
%unignore IndexManager::NodeToIndex;
%unignore IndexManager::IndicesToNodes(const std::vector<int64_t>&);
%extend IndexManager {
  std::vector<NodeIndex> IndicesToNodes(
    const std::vector<int64_t>& indices) {
      return $self->IndicesToNodes(absl::Span<const int64_t>(indices));
  }
}
%unignore IndexManager::NodesToIndices;
%unignore IndexManager::kUnassigned;

}  // namespace operations_research::routing

%include "ortools/routing/index_manager.h"

%unignoreall
