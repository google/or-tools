// Copyright 2010-2021 Google LLC
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

// TODO(user): Refactor this file to adhere to the SWIG style guide.
//swiglint: disable full-signature

%include "ortools/base/base.i"

%include "ortools/util/python/pair.i"

%include "ortools/constraint_solver/python/constraint_solver.i"
%include "ortools/constraint_solver/python/routing_types.i"
%include "ortools/constraint_solver/python/routing_index_manager.i"
%include "ortools/util/python/sorted_interval_list.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../routing.h" %}.
namespace operations_research {
class RoutingModelParameters;
class RoutingSearchParameters;
}  // namespace operations_research

// Include the files we want to wrap a first time.
%{
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/util/optional_boolean.pb.h"
%}

DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingCostClassIndex,
    operations_research::RoutingModel::CostClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingDimensionIndex,
    operations_research::RoutingModel::DimensionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingDisjunctionIndex,
    operations_research::RoutingModel::DisjunctionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingVehicleClassIndex,
    operations_research::RoutingModel::VehicleClassIndex);


%ignore operations_research::RoutingModel::RegisterStateDependentTransitCallback;
%ignore operations_research::RoutingModel::StateDependentTransitCallback;
%ignore operations_research::RoutingModel::MakeStateDependentTransit;
%ignore operations_research::RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;

PY_PROTO_TYPEMAP(ortools.constraint_solver.routing_parameters_pb2,
                 RoutingModelParameters,
                 operations_research::RoutingModelParameters)
PY_PROTO_TYPEMAP(ortools.constraint_solver.routing_parameters_pb2,
                 RoutingSearchParameters,
                 operations_research::RoutingSearchParameters)

// Wrap routing_types.h, routing_parameters.h according to the SWIG styleguide.
%ignoreall
%unignore RoutingTransitCallback1;
%unignore RoutingTransitCallback2;
%unignore RoutingIndexPair;
%unignore RoutingIndexPairs;

%unignore DefaultRoutingSearchParameters;
%unignore DefaultRoutingModelParameters;
%unignore FindErrorInRoutingSearchParameters;

%include "ortools/constraint_solver/routing_types.h"
%include "ortools/constraint_solver/routing_parameters.h"
%unignoreall

// %including a .proto.h is frowned upon (for good general reasons), so we
// have to duplicate the OptionalBoolean enum here to give it to python users.
namespace operations_research {
enum OptionalBoolean {
  BOOL_UNSPECIFIED = 0,
  BOOL_FALSE = 2,
  BOOL_TRUE = 3,
};
}  // namespace operations_research

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
//swiglint: disable include-h-allglobals
%include "ortools/constraint_solver/routing.h"
