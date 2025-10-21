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

// TODO(user): Refactor this file to adhere to the SWIG style guide.
//swiglint: disable full-signature

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

// PY_CONVERT_HELPER_* macros.
%include "ortools/constraint_solver/python/constraint_solver_helpers.swig"

%include "ortools/util/python/functions.swig"
%include "ortools/util/python/pair.swig"
%include "ortools/util/python/vector.swig"

// Ignore warnings for operators like: Unreferenced %newobject *::operator -=
#pragma SWIG nowarn=551

// While the module name will be overridden by the one specified on the cmd line,
// without this, derived classes (e.g. TypeRequirementChecker) will import base
// class from the module specified in the following %import.
%module pywraprouting
%import(module="ortools.constraint_solver.pywrapcp") "ortools/constraint_solver/python/constraint_solver.swig"
%include "ortools/routing/python/types.swig"
%include "ortools/routing/python/index_manager.swig"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../routing.h" %}.
namespace operations_research::routing {
class LocalSearchOperator;
}  // namespace operations_research::routing
namespace operations_research::routing {
class RoutingModelParameters;
class RoutingSearchParameters;
class RoutingSearchStatus;
}  // namespace operations_research::routing::routing

// Include the files we want to wrap a first time.
%{
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/types.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/routing.h"
#include "ortools/util/optional_boolean.pb.h"
%}

DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingCostClassIndex,
    operations_research::routing::RoutingModel::CostClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingDimensionIndex,
    operations_research::routing::RoutingModel::DimensionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingDisjunctionIndex,
    operations_research::routing::RoutingModel::DisjunctionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingVehicleClassIndex,
    operations_research::routing::RoutingModel::VehicleClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingResourceClassIndex,
    operations_research::routing::RoutingModel::ResourceClassIndex);

PY_PROTO_TYPEMAP(ortools.routing.parameters_pb2,
                 RoutingModelParameters,
                 operations_research::routing::RoutingModelParameters)
PY_PROTO_TYPEMAP(ortools.routing.parameters_pb2,
                 RoutingSearchParameters,
                 operations_research::routing::RoutingSearchParameters)

// Wrap types.h, parameters.h according to the SWIG style guide.
%ignoreall
%unignore RoutingTransitCallback1;
%unignore RoutingTransitCallback2;
%unignore RoutingIndexPair;
%unignore RoutingIndexPairs;

%unignore DefaultRoutingSearchParameters;
%unignore DefaultRoutingModelParameters;
%unignore FindErrorInRoutingSearchParameters;

%include "ortools/routing/types.h"
%include "ortools/routing/parameters.h"
%unignoreall

%unignore operations_research::routing;
namespace operations_research::routing {

// %including a .proto.h is frowned upon (for good general reasons), so we
// have to duplicate the OptionalBoolean enum here to give it to python users.
enum OptionalBoolean {
  BOOL_UNSPECIFIED = 0,
  BOOL_FALSE = 2,
  BOOL_TRUE = 3,
};
}  // namespace operations_research::routing

%unignore operations_research::routing;
namespace operations_research::routing {

struct FirstSolutionStrategy {
  enum Value {};
};

struct LocalSearchMetaheuristic {
  enum Value {};
};

struct RoutingSearchStatus {
  enum Value {};
};

// SimpleBoundCosts
%unignore BoundCost;

%unignore SimpleBoundCosts;
%unignore SimpleBoundCosts::bound_cost;
%rename("size") SimpleBoundCosts::Size;

}  // namespace operations_research::routing::routing

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
//swiglint: disable include-h-allglobals
%include "ortools/routing/routing.h"
