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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_

#include <utility>
#include <vector>
#include "ortools/base/callback.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/int_type.h"

namespace operations_research {

// Defining common types used in the routing library outside the main
// RoutingModel class has several purposes:
// 1) It allows some small libraries to avoid a dependency on routing.{h,cc},
//    eg. routing_neighborhoods.h.
// 2) It allows an easier wrapping via SWIG, which can have issues with
//    intra-class types.
//
// Users that depend on routing.{h,cc} should just use the
// RoutingModel:: equivalent, eg. RoutingModel::NodeIndex.
DEFINE_INT_TYPE(RoutingNodeIndex, int);
DEFINE_INT_TYPE(RoutingCostClassIndex, int);
DEFINE_INT_TYPE(RoutingDimensionIndex, int);
DEFINE_INT_TYPE(RoutingDisjunctionIndex, int);
DEFINE_INT_TYPE(RoutingVehicleClassIndex, int);

typedef ResultCallback2<int64, RoutingNodeIndex, RoutingNodeIndex>
    RoutingNodeEvaluator2;
typedef std::function<int64(int64, int64)> RoutingTransitEvaluator2;
// NOTE(user): keep the "> >" for SWIG.
typedef std::pair<std::vector<int64>, std::vector<int64> > RoutingNodePair;
typedef std::vector<RoutingNodePair> RoutingNodePairs;

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_
