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

// DEPRECATED: Please include ortools/base/graph.h directly.

#ifndef OR_TOOLS_GRAPH_GRAPH_H_
#define OR_TOOLS_GRAPH_GRAPH_H_

#include "ortools/base/graph.h"

namespace operations_research {

template <typename T>
using SVector = util::SVector<T>;

template <typename NodeIndexType = int32, typename ArcIndexType = int32,
          bool HasReverseArcs = false>
using BaseGraph = util::BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>;

template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using ListGraph = util::ListGraph<NodeIndexType, ArcIndexType>;
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using StaticGraph = util::StaticGraph<NodeIndexType, ArcIndexType>;

template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using ReverseArcListGraph =
    util::ReverseArcListGraph<NodeIndexType, ArcIndexType>;
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using ReverseArcStaticGraph =
    util::ReverseArcStaticGraph<NodeIndexType, ArcIndexType>;
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using ReverseArcMixedGraph =
    util::ReverseArcMixedGraph<NodeIndexType, ArcIndexType>;

// Imported Permute Functions
using util::Permute;
using util::PermuteWithExplicitElementType;

// Complete Graphs
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using CompleteGraph = util::CompleteGraph<NodeIndexType, ArcIndexType>;
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
using CompleteBipartiteGraph =
    util::CompleteBipartiteGraph<NodeIndexType, ArcIndexType>;

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_GRAPH_H_
