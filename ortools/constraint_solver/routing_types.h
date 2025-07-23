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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {

/// Defining common types used in the routing library outside the main
/// RoutingModel class has several purposes:
/// 1) It allows some small libraries to avoid a dependency on routing.{h,cc},
///    eg. routing_neighborhoods.h.
/// 2) It allows an easier wrapping via SWIG, which can have issues with
///    intra-class types.
///
/// Users that depend on routing.{h,cc} should just use the
/// RoutingModel:: equivalent, eg. RoutingModel::NodeIndex.
DEFINE_STRONG_INDEX_TYPE(RoutingNodeIndex);
DEFINE_STRONG_INDEX_TYPE(RoutingCostClassIndex);
DEFINE_STRONG_INDEX_TYPE(RoutingDimensionIndex);
DEFINE_STRONG_INDEX_TYPE(RoutingDisjunctionIndex);
DEFINE_STRONG_INDEX_TYPE(RoutingVehicleClassIndex);
DEFINE_STRONG_INDEX_TYPE(RoutingResourceClassIndex);

/// Pickup and delivery pair representation, including alternatives for pickups
/// and deliveries respectively.
struct PickupDeliveryPair {
  std::vector<int64_t> pickup_alternatives;
  std::vector<int64_t> delivery_alternatives;
};

typedef std::function<int64_t(int64_t)> RoutingTransitCallback1;
typedef std::function<int64_t(int64_t, int64_t)> RoutingTransitCallback2;
typedef std::function<const FloatSlopePiecewiseLinearFunction*(int64_t,
                                                               int64_t)>
    RoutingCumulDependentTransitCallback2;

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_TYPES_H_
