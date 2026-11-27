# Migrating the Routing Solver from v9.15 to v10.0-rc.1

This guide explains how to migrate Vehicle Routing code from OR-Tools **v9.15**
to **v10.0-rc.1** across **C++**, **.NET (C#)**, **Java**, and **Python**.

In **v10.0-rc.1**, the Routing library was extracted out of `constraint_solver`
into its own dedicated `routing` module, namespace, and package across all
languages:

*   **Standalone namespace / package / module**: Routing types and protobufs
    moved from `constraint_solver` to `routing`. Shared CP types (`Assignment`,
    `Solver`, `IntVar`, `IntervalVar`, etc.) remain in `constraint_solver`.
*   **Shorter class names**: Core routing classes dropped the `Routing` prefix:
    `RoutingIndexManager` → `IndexManager`, `RoutingModel` → `Model`, and
    `RoutingDimension` → `Dimension`.
*   **Python migrated from SWIG to pybind11**:
    `ortools.constraint_solver.pywrapcp` was removed and replaced by
    `ortools.routing.python.routing` and
    `ortools.constraint_solver.python.constraint_solver`, adopting PEP 8
    `snake_case` naming and properties.
*   **Protobuf and search parameter cleanups**: Routing `.proto` files moved to
    `ortools/routing/`, obsolete LNS flags were removed, and Iterated Local
    Search (ILS) parameters were reworked.

## Contents

*   [Platform and Toolchain Requirements](#platform-and-toolchain-requirements)
*   [Quick Comparison Table](#quick-comparison-table)
*   [Protobuf Schema and Search Parameter Changes](#protobuf-schema-and-search-parameter-changes)
*   [C++ Migration Guide](#c-migration-guide)
*   [.NET (C#) Migration Guide](#net-c-migration-guide)
*   [Java Migration Guide](#java-migration-guide)
*   [Python Migration Guide](#python-migration-guide)
*   [References](#references)

---

## Platform and Toolchain Requirements

| Language | v9.15 | v10.0-rc.1 |
| :--- | :--- | :--- |
| **C++** | C++17 (Linux/macOS), CMake >= 3.24 | **C++20 required** (GCC >= 10, MSVC >= 2022); CMake >= 3.25; Bazel 8.7.0 |
| **.NET** | `.NET 8.0`, `.NET Framework` (`net462`/`net472`) | **`.NET 8.0` LTS** (official NuGet packages drop `net462`/`net472`); `Google.Protobuf` 3.35.1 |
| **Java** | JDK 8 / 11 / 17 | **JDK 21+ required**; `protobuf-java` 4.35.1 |
| **Python** | Python 3.9–3.14, SWIG (`pywrapcp`), `protobuf` 6.33.x | **Python 3.10–3.15**, **pybind11** (SWIG removed), `protobuf>=7.35.0,<8.0` |

---

## Quick Comparison Table

| Concept | Language | v9.15 | v10.0-rc.1 |
| :--- | :--- | :--- | :--- |
| **Namespace / Package / Module** | C++ | `operations_research` (`ortools/constraint_solver/routing*.h`) | `operations_research::routing` (`ortools/routing/*.h`) |
| | .NET | `Google.OrTools.ConstraintSolver` | `Google.OrTools.Routing` (plus `ConstraintSolver` for CP types) |
| | Java | `com.google.ortools.constraintsolver` | `com.google.ortools.routing` (plus `constraintsolver` for CP types) |
| | Python | `ortools.constraint_solver.pywrapcp` | `ortools.routing.python.routing` (plus `ortools.constraint_solver.python.constraint_solver`) |
| **Index Manager** | All | `RoutingIndexManager` | `IndexManager` |
| **Routing Model** | All | `RoutingModel` | `Model` |
| **Dimension** | All | `RoutingDimension` | `Dimension` |
| **Default Search Parameters** | C++ | `operations_research::DefaultRoutingSearchParameters()` | `operations_research::routing::DefaultRoutingSearchParameters()` |
| | .NET | `operations_research_constraint_solver.DefaultRoutingSearchParameters()` | `RoutingGlobals.DefaultRoutingSearchParameters()` |
| | Java | `com.google.ortools.constraintsolver.main.defaultRoutingSearchParameters()` | `com.google.ortools.routing.Globals.defaultRoutingSearchParameters()` |
| | Python | `pywrapcp.DefaultRoutingSearchParameters()` | `routing.default_routing_search_parameters()` |
| **Enums (`FirstSolutionStrategy`, `LocalSearchMetaheuristic`, `RoutingSearchStatus`)** | C++ | `ortools/constraint_solver/routing_enums.pb.h` (`operations_research::`) | `ortools/routing/enums.pb.h` (`operations_research::routing::`) |
| | .NET | `Google.OrTools.ConstraintSolver` | `Google.OrTools.Routing` |
| | Java | `com.google.ortools.constraintsolver` | `com.google.ortools.routing` |
| | Python | `ortools.constraint_solver.routing_enums_pb2` | `ortools.routing.enums_pb2` |

---

## Protobuf Schema and Search Parameter Changes

These changes apply across all four languages as well as serialized/textproto
`RoutingSearchParameters` configurations.

### 1. Moved Protobuf Files and Package Names

| v9.15 | v10.0-rc.1 |
| :--- | :--- |
| `ortools/constraint_solver/routing_enums.proto` | `ortools/routing/enums.proto` |
| `ortools/constraint_solver/routing_parameters.proto` | `ortools/routing/parameters.proto` |
| `ortools/constraint_solver/routing_ils.proto` | `ortools/routing/ils.proto` |
| `ortools/constraint_solver/routing_heuristic_parameters.proto` | `ortools/routing/heuristic_parameters.proto` |
| `package operations_research;` | `package operations_research.routing;` |
| `option java_package = "com.google.ortools.constraintsolver";` | `option java_package = "com.google.ortools.routing";` |
| `option csharp_namespace = "Google.OrTools.ConstraintSolver";` | `option csharp_namespace = "Google.OrTools.Routing";` |

### 2. Removed LNS Operator Fields

In `RoutingSearchParameters.LocalSearchNeighborhoodOperators`, the following
fields were removed (their field numbers are now reserved):

*   `use_path_lns` (field 16)
*   `use_full_path_lns` (field 17)
*   `use_inactive_lns` (field 19)

Remove these fields from any code or textproto configurations (for example,
`examples/cpp/random_tsp.cc` previously set `use_path_lns` and
`use_inactive_lns`).

### 3. Iterated Local Search (ILS) Parameters

*   `RoutingSearchParameters.use_iterated_local_search` (field 58) was removed.
    ILS is now enabled automatically whenever `iterated_local_search_parameters`
    is set.
*   In `IteratedLocalSearchParameters`, `reference_solution_acceptance_strategy`
    and `best_solution_acceptance_strategy` were replaced by composable
    `AcceptancePolicy` fields:
    *   `reference_solution_acceptance_policy` (field 6)
    *   `best_solution_acceptance_policy` (field 7)
*   In C++, a new `RecommendedIteratedLocalSearchParameters()` helper is
    provided in `ortools/routing/parameters.h` to obtain a ready-to-use ILS
    configuration.

#### C++ (ILS Parameters)

```cpp
// v9.15
RoutingSearchParameters params = DefaultRoutingSearchParameters();
params.set_use_iterated_local_search(true);
IteratedLocalSearchParameters* ils =
    params.mutable_iterated_local_search_parameters();
ils->mutable_best_solution_acceptance_strategy()->mutable_greedy_descent();
ils->mutable_reference_solution_acceptance_strategy()
    ->mutable_simulated_annealing()
    ->set_cooling_schedule_strategy(CoolingScheduleStrategy::EXPONENTIAL);
```

```cpp
// v10.0-rc.1
RoutingSearchParameters params = DefaultRoutingSearchParameters();
// Setting iterated_local_search_parameters enables ILS automatically.
// You can also start from RecommendedIteratedLocalSearchParameters().
IteratedLocalSearchParameters* ils =
    params.mutable_iterated_local_search_parameters();
ils->mutable_best_solution_acceptance_policy()
    ->add_strategies()
    ->mutable_greedy_descent();
ils->mutable_reference_solution_acceptance_policy()
    ->add_strategies()
    ->mutable_simulated_annealing()
    ->set_cooling_schedule_strategy(CoolingScheduleStrategy::EXPONENTIAL);
```

#### .NET (C#) (ILS Parameters)

```csharp
// v9.15
RoutingSearchParameters searchParameters =
    operations_research_constraint_solver.DefaultRoutingSearchParameters();
searchParameters.UseIteratedLocalSearch = true;
searchParameters.IteratedLocalSearchParameters = new IteratedLocalSearchParameters {
  BestSolutionAcceptanceStrategy = new AcceptanceStrategy {
    GreedyDescent = new GreedyDescentAcceptanceStrategy()
  }
};
```

```csharp
// v10.0-rc.1
RoutingSearchParameters searchParameters =
    RoutingGlobals.DefaultRoutingSearchParameters();
searchParameters.IteratedLocalSearchParameters = new IteratedLocalSearchParameters {
  BestSolutionAcceptancePolicy = new AcceptancePolicy {
    Strategies = {
      new AcceptanceStrategy {
        GreedyDescent = new GreedyDescentAcceptanceStrategy()
      }
    }
  }
};
```

#### Java (ILS Parameters)

```java
// v9.15
RoutingSearchParameters searchParameters =
    main.defaultRoutingSearchParameters()
        .toBuilder()
        .setUseIteratedLocalSearch(true)
        .setIteratedLocalSearchParameters(
            IteratedLocalSearchParameters.newBuilder()
                .setBestSolutionAcceptanceStrategy(
                    AcceptanceStrategy.newBuilder()
                        .setGreedyDescent(
                            GreedyDescentAcceptanceStrategy.getDefaultInstance())))
        .build();
```

```java
// v10.0-rc.1
RoutingSearchParameters searchParameters =
    Globals.defaultRoutingSearchParameters()
        .toBuilder()
        .setIteratedLocalSearchParameters(
            IteratedLocalSearchParameters.newBuilder()
                .setBestSolutionAcceptancePolicy(
                    AcceptancePolicy.newBuilder()
                        .addStrategies(
                            AcceptanceStrategy.newBuilder()
                                .setGreedyDescent(
                                    GreedyDescentAcceptanceStrategy.getDefaultInstance()))))
        .build();
```

#### Python (ILS Parameters)

```python
# v9.15
search_parameters = pywrapcp.DefaultRoutingSearchParameters()
search_parameters.use_iterated_local_search = True
ils = search_parameters.iterated_local_search_parameters
ils.best_solution_acceptance_strategy.greedy_descent.SetInParent()
```

```python
# v10.0-rc.1
search_parameters = routing.default_routing_search_parameters()
ils = search_parameters.iterated_local_search_parameters
ils.best_solution_acceptance_policy.strategies.add().greedy_descent.SetInParent()
```

---

## C++ Migration Guide

### 1. Headers and Includes

All routing headers moved from `ortools/constraint_solver/routing_*.h` to
`ortools/routing/*.h` (dropping the `routing_` filename prefix except for
`routing.h`). In addition:

*   Legacy `ortools/base/logging.h` was removed; include `"absl/log/log.h"`
    (and `"absl/log/globals.h"` / `"ortools/base/init_google.h"` when
    initializing logging in `main()`).
*   `ortools/constraint_solver/constraint_solveri.h` was removed and
    `constraint_solver.h` was split; if you use `IntervalVar` (e.g., for vehicle
    breaks), include `"ortools/constraint_solver/interval.h"`.

| v9.15 Header | v10.0-rc.1 Header |
| :--- | :--- |
| `ortools/constraint_solver/routing.h` | `ortools/routing/routing.h` |
| `ortools/constraint_solver/routing_index_manager.h` | `ortools/routing/index_manager.h` |
| `ortools/constraint_solver/routing_parameters.h` | `ortools/routing/parameters.h` |
| `ortools/constraint_solver/routing_types.h` | `ortools/routing/types.h` |
| `ortools/constraint_solver/routing_enums.pb.h` | `ortools/routing/enums.pb.h` |
| `ortools/constraint_solver/routing_parameters.pb.h` | `ortools/routing/parameters.pb.h` |
| `ortools/constraint_solver/routing_ils.pb.h` | `ortools/routing/ils.pb.h` |
| `ortools/constraint_solver/routing_breaks.h` | `ortools/routing/breaks.h` |
| `ortools/constraint_solver/routing_filters.h` | `ortools/routing/filters.h` |
| `ortools/constraint_solver/routing_neighborhoods.h` | `ortools/routing/neighborhoods.h` |
| `ortools/constraint_solver/routing_search.h` | `ortools/routing/search.h` |
| `ortools/base/logging.h` | `absl/log/log.h` |

```cpp
// v9.15
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
```

```cpp
// v10.0-rc.1
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"  // Needed if using IntervalVar (e.g., breaks)
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
```

### 2. Namespace and Class Renames

Routing declarations moved from `namespace operations_research` to
`namespace operations_research::routing`, and core types dropped the `Routing`
prefix:

| v9.15 (`operations_research::`) | v10.0-rc.1 (`operations_research::routing::`) |
| :--- | :--- |
| `RoutingIndexManager` | `IndexManager` |
| `RoutingIndexManager::NodeIndex` / `RoutingNodeIndex` | `NodeIndex` |
| `RoutingModel` | `Model` |
| `RoutingDimension` | `Dimension` |
| `RoutingModelVisitor` | `ModelVisitor` |
| `RoutingSearchStats` | `SearchStats` |
| `RoutingCostClassIndex` / `RoutingDimensionIndex` / `RoutingDisjunctionIndex` / `RoutingVehicleClassIndex` / `RoutingResourceClassIndex` | `CostClassIndex` / `DimensionIndex` / `DisjunctionIndex` / `VehicleClassIndex` / `ResourceClassIndex` |
| `RoutingTransitCallback1` / `RoutingTransitCallback2` | `TransitCallback1` / `TransitCallback2` |
| `RoutingCumulDependentTransitCallback2` | `CumulDependentTransitCallback2` |

> **Note**: Compatibility `using` aliases (`RoutingModel`, `RoutingDimension`,
> `RoutingSearchStats`, `IndexManager::NodeIndex`, `RoutingNodeIndex`, etc.) are
> still provided inside `operations_research::routing`, but the new shorter
> names are preferred. Shared CP types (`Assignment`, `Solver`, `IntVar`,
> `IntervalVar`) remain in `operations_research`.

#### IndexManager, Model, and NodeIndex

```cpp
// v9.15 (from simple_routing_program.cc / vrp_starts_ends.cc)
namespace operations_research {

void Solve() {
  const RoutingIndexManager::NodeIndex depot{0};
  RoutingIndexManager manager(num_locations, num_vehicles, depot);
  RoutingModel routing(manager);

  // Multiple starts/ends:
  const std::vector<RoutingIndexManager::NodeIndex> starts{
      RoutingIndexManager::NodeIndex{1}, RoutingIndexManager::NodeIndex{2}};
  const std::vector<RoutingIndexManager::NodeIndex> ends{
      RoutingIndexManager::NodeIndex{0}, RoutingIndexManager::NodeIndex{0}};
  RoutingIndexManager multi_manager(num_locations, 2, starts, ends);

  // Disjunction using NodeIndex:
  routing.AddDisjunction(
      {manager.NodeToIndex(RoutingIndexManager::NodeIndex(1))}, /*penalty=*/1000);
}

}  // namespace operations_research
```

```cpp
// v10.0-rc.1 (from ortools/routing/samples/simple_routing_program.cc, vrp_starts_ends.cc)
namespace operations_research::routing {

void Solve() {
  const NodeIndex depot{0};
  IndexManager manager(num_locations, num_vehicles, depot);
  Model routing(manager);

  // Multiple starts/ends:
  const std::vector<NodeIndex> starts{NodeIndex{1}, NodeIndex{2}};
  const std::vector<NodeIndex> ends{NodeIndex{0}, NodeIndex{0}};
  IndexManager multi_manager(num_locations, 2, starts, ends);

  // Disjunction using NodeIndex:
  routing.AddDisjunction({manager.NodeToIndex(NodeIndex(1))}, /*penalty=*/1000);
}

}  // namespace operations_research::routing
```

#### Dimensions and Nested Enums

```cpp
// v9.15 (from vrp_pickup_delivery_fifo.cc / vrp_time_windows.cc)
void PrintSolution(const RoutingIndexManager& manager,
                   const RoutingModel& routing, const Assignment& solution) {
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("Time");
  // ...
}

RoutingDimension* distance_dimension = routing.GetMutableDimension("Distance");
distance_dimension->SetGlobalSpanCostCoefficient(100);
routing.SetPickupAndDeliveryPolicyOfAllVehicles(
    RoutingModel::PICKUP_AND_DELIVERY_FIFO);
```

```cpp
// v10.0-rc.1 (from ortools/routing/samples/vrp_pickup_delivery_fifo.cc, vrp_time_windows.cc)
void PrintSolution(const IndexManager& manager, const Model& routing,
                   const Assignment& solution) {
  const Dimension& time_dimension = routing.GetDimensionOrDie("Time");
  // ...
}

Dimension* distance_dimension = routing.GetMutableDimension("Distance");
distance_dimension->SetGlobalSpanCostCoefficient(100);
routing.SetPickupAndDeliveryPolicyOfAllVehicles(
    Model::PICKUP_AND_DELIVERY_FIFO);
```

#### Logging Initialization in `main()`

Because `ortools/base/logging.h` was removed in favor of Abseil logging, sample
binaries now initialize logging explicitly:

```cpp
// v9.15
int main(int /*argc*/, char* /*argv*/[]) {
  operations_research::Vrp();
  return EXIT_SUCCESS;
}
```

```cpp
// v10.0-rc.1
int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::routing::Vrp();
  return EXIT_SUCCESS;
}
```

### 3. Method Signature and API Changes

*   **`absl::Span<const int64_t>` parameters**: `Model::AddRouteConstraint`
    callbacks, `Model::GetRouteCost`, and `Model::AddDisjunction` now take
    `absl::Span<const int64_t>` instead of `const std::vector<int64_t>&`.
*   **Generalized disjunctions**:
    *   `HasMandatoryDisjunctions()` and
        `HasMaxCardinalityConstrainedDisjunctions()` were replaced by
        `HasHardDisjunctions()`.
    *   `GetDisjunctionPenaltyCostBehavior(index)` was replaced by
        `GetDisjunctionSoftMinPenaltyCostBehavior(index)` and
        `GetDisjunctionSoftMaxPenaltyCostBehavior(index)`.
    *   New generalized disjunction methods are available: `MakeDisjunction`,
        `SetDisjunctionHardMinimum`, `SetDisjunctionHardMaximum`,
        `SetDisjunctionSoftMinimum`, and `SetDisjunctionSoftMaximum`.

```cpp
// v9.15
routing.AddRouteConstraint(
    [](const std::vector<int64_t>& route) -> std::optional<int64_t> {
      return route.size() > 10 ? std::nullopt : std::optional<int64_t>{0};
    });

bool has_mandatory = routing.HasMandatoryDisjunctions();
auto behavior = routing.GetDisjunctionPenaltyCostBehavior(disj_index);
```

```cpp
// v10.0-rc.1
routing.AddRouteConstraint(
    [](absl::Span<const int64_t> route) -> std::optional<int64_t> {
      return route.size() > 10 ? std::nullopt : std::optional<int64_t>{0};
    });

bool has_hard = routing.HasHardDisjunctions();
auto min_behavior = routing.GetDisjunctionSoftMinPenaltyCostBehavior(disj_index);
auto max_behavior = routing.GetDisjunctionSoftMaxPenaltyCostBehavior(disj_index);
```

### 4. Build Targets (Bazel and CMake)

#### Bazel

Routing targets moved from `//ortools/constraint_solver` to `//ortools/routing`:

```starlark
# v9.15
cc_binary(
    name = "my_vrp",
    srcs = ["my_vrp.cc"],
    deps = [
        "//ortools/base:logging",
        "//ortools/constraint_solver:cp",
        "//ortools/constraint_solver:routing",
        "//ortools/constraint_solver:routing_enums_cc_proto",
        "//ortools/constraint_solver:routing_index_manager",
        "//ortools/constraint_solver:routing_parameters",
    ],
)
```

```starlark
# v10.0-rc.1
cc_binary(
    name = "my_vrp",
    srcs = ["my_vrp.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:log_severity",
        "//ortools/constraint_solver:cp",
        "//ortools/routing",
        "//ortools/routing:enums_cc_proto",
        "//ortools/routing:index_manager",
        "//ortools/routing:parameters",
        "//ortools/routing:types",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:globals",
    ],
)
```

#### CMake

The monolithic `libortools` shared library was split into modular component
libraries (`ortools_core`, `ortools_routing`, etc.). Linking the umbrella
`ortools::ortools` target continues to work (it links `ortools_routing`
publicly), or you can link `ortools::ortools_routing` directly. Make sure your
project targets **C++20** (`CMAKE_CXX_STANDARD 20`).

---

## .NET (C#) Migration Guide

### 1. Namespaces (`using` Directives)

All routing classes, enums, and protobuf messages moved from
`Google.OrTools.ConstraintSolver` to `Google.OrTools.Routing`. Keep
`using Google.OrTools.ConstraintSolver;` whenever your code uses shared CP
classes (`Assignment`, `Solver`, `IntVar`, `IntervalVar`,
`AssignmentIntervalContainer`, `IntervalVarElement`, etc.).

```csharp
// v9.15
using System;
using Google.OrTools.ConstraintSolver;
```

```csharp
// v10.0-rc.1
using System;
using Google.OrTools.ConstraintSolver;  // Assignment, Solver, IntVar, IntervalVar
using Google.OrTools.Routing;           // IndexManager, Model, Dimension, RoutingGlobals, protos
```

### 2. Class and Enum Renames

| v9.15 (`Google.OrTools.ConstraintSolver`) | v10.0-rc.1 (`Google.OrTools.Routing`) |
| :--- | :--- |
| `RoutingIndexManager` | `IndexManager` |
| `RoutingModel` | `Model` |
| `RoutingDimension` | `Dimension` |
| `RoutingModel.PICKUP_AND_DELIVERY_FIFO` | `Model.PICKUP_AND_DELIVERY_FIFO` |
| `RoutingModel.PICKUP_AND_DELIVERY_LIFO` | `Model.PICKUP_AND_DELIVERY_LIFO` |
| `RoutingModel.PICKUP_AND_DELIVERY_NO_ORDER` | `Model.PICKUP_AND_DELIVERY_NO_ORDER` |
| `RoutingModel.PENALIZE_ONCE` / `PENALIZE_PER_INACTIVE` | `Model.PENALIZE_ONCE` / `PENALIZE_PER_INACTIVE` |
| `RoutingSearchParameters`, `RoutingModelParameters`, `FirstSolutionStrategy`, `LocalSearchMetaheuristic`, `RoutingSearchStatus` | Same names, now in `Google.OrTools.Routing` |

#### Model, IndexManager, and Dimension

```csharp
// v9.15 (from VrpTimeWindows.cs / VrpStartsEnds.cs)
RoutingIndexManager manager =
    new RoutingIndexManager(data.TimeMatrix.GetLength(0), data.VehicleNumber, data.Depot);
RoutingModel routing = new RoutingModel(manager);

// Multiple starts and ends:
RoutingIndexManager multiManager =
    new RoutingIndexManager(numLocations, vehicleNumber, data.Starts, data.Ends);

// Dimension:
RoutingDimension timeDimension = routing.GetMutableDimension("Time");
timeDimension.SetGlobalSpanCostCoefficient(100);
```

```csharp
// v10.0-rc.1 (from ortools/routing/samples/VrpTimeWindows.cs, VrpStartsEnds.cs)
IndexManager manager =
    new IndexManager(data.TimeMatrix.GetLength(0), data.VehicleNumber, data.Depot);
Model routing = new Model(manager);

// Multiple starts and ends:
IndexManager multiManager =
    new IndexManager(numLocations, vehicleNumber, data.Starts, data.Ends);

// Dimension:
Dimension timeDimension = routing.GetMutableDimension("Time");
timeDimension.SetGlobalSpanCostCoefficient(100);
```

#### Pickup & Delivery Policies and Solution Callbacks

```csharp
// v9.15 (from VrpPickupDeliveryFifo.cs / VrpSolutionCallback.cs)
routing.SetPickupAndDeliveryPolicyOfAllVehicles(RoutingModel.PICKUP_AND_DELIVERY_FIFO);

class SolutionCallback {
  private RoutingIndexManager routingManager;
  private RoutingModel routingModel;
  public SolutionCallback(ref RoutingIndexManager manager, ref RoutingModel routing, in long limit) {
    // ...
  }
}
```

```csharp
// v10.0-rc.1 (from ortools/routing/samples/VrpPickupDeliveryFifo.cs, VrpSolutionCallback.cs)
routing.SetPickupAndDeliveryPolicyOfAllVehicles(Model.PICKUP_AND_DELIVERY_FIFO);

class SolutionCallback {
  private IndexManager routingManager;
  private Model routingModel;
  public SolutionCallback(ref IndexManager manager, ref Model routing, in long limit) {
    // ...
  }
}
```

### 3. Global Helper Class Renamed (`RoutingGlobals`)

Global routing functions moved from `operations_research_constraint_solver` to
`RoutingGlobals` (and constraint solver globals moved to
`ConstraintSolverGlobals`):

| v9.15 | v10.0-rc.1 |
| :--- | :--- |
| `operations_research_constraint_solver.DefaultRoutingSearchParameters()` | `RoutingGlobals.DefaultRoutingSearchParameters()` |
| `operations_research_constraint_solver.DefaultRoutingModelParameters()` | `RoutingGlobals.DefaultRoutingModelParameters()` |
| `operations_research_constraint_solver.FindErrorInRoutingSearchParameters(...)` | `RoutingGlobals.FindErrorInRoutingSearchParameters(...)` |
| `operations_research_constraint_solver.<cp_helper>(...)` | `ConstraintSolverGlobals.<cp_helper>(...)` |

```csharp
// v9.15 (from VrpCapacity.cs)
RoutingSearchParameters searchParameters =
    operations_research_constraint_solver.DefaultRoutingSearchParameters();
searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
searchParameters.LocalSearchMetaheuristic =
    LocalSearchMetaheuristic.Types.Value.GuidedLocalSearch;
searchParameters.TimeLimit = new Duration { Seconds = 1 };
```

```csharp
// v10.0-rc.1 (from ortools/routing/samples/VrpCapacity.cs)
RoutingSearchParameters searchParameters = RoutingGlobals.DefaultRoutingSearchParameters();
searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;
searchParameters.LocalSearchMetaheuristic =
    LocalSearchMetaheuristic.Types.Value.GuidedLocalSearch;
searchParameters.TimeLimit = new Duration { Seconds = 1 };
```

#### Checking Solver Status

```csharp
// v10.0-rc.1 (from ortools/routing/samples/Vrp.cs)
RoutingSearchStatus.Types.Value status = routing.GetStatus();
Console.WriteLine("Status: {0}", status);
if (status != RoutingSearchStatus.Types.Value.RoutingOptimal &&
    status != RoutingSearchStatus.Types.Value.RoutingSuccess) {
  Console.WriteLine("No solution found!");
  return;
}
```

### 4. Fixed and Newly Exposed .NET APIs

*   **`SetAllowedVehiclesForIndex`** is fixed in v10.0-rc.1 (fixes v9.15 issue
    #4982).
*   **`AddRouteConstraint` and `GetRouteCost`** are newly exposed in C# (#5255),
    supporting both a `Func<long[], long?>` lambda overload (returning `null`
    when a route is infeasible) and subclassing `RouteConstraintCallback`.
*   **`IndexManager.IndicesToNodes(long[])`** and
    **`IndexManager.GetNumberOfUniqueDepots()`** are now exposed.

```csharp
// v10.0-rc.1
model.SetAllowedVehiclesForIndex(new int[] { 0, 2 }, manager.NodeToIndex(1));

// Route constraint using lambda convenience overload (null = infeasible):
model.AddRouteConstraint((long[] route) => route.Length > 10 ? (long?)null : 0L,
                         costs_are_homogeneous_across_vehicles: true);
long? cost = model.GetRouteCost(new long[] { 0, 2, 4, 0 });
```

---

## Java Migration Guide

### 1. Package and Imports

All routing classes, enums, and protobuf messages moved from
`com.google.ortools.constraintsolver` to `com.google.ortools.routing`. Shared
CP types (`Assignment`, `Solver`, `IntVar`, `IntervalVar`,
`AssignmentIntervalContainer`, `IntervalVarElement`, etc.) remain in
`com.google.ortools.constraintsolver`.

```java
// v9.15
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.LocalSearchMetaheuristic;
import com.google.ortools.constraintsolver.RoutingDimension;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.RoutingSearchStatus;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.main;
```

```java
// v10.0-rc.1
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.routing.Dimension;
import com.google.ortools.routing.FirstSolutionStrategy;
import com.google.ortools.routing.Globals;
import com.google.ortools.routing.IndexManager;
import com.google.ortools.routing.LocalSearchMetaheuristic;
import com.google.ortools.routing.Model;
import com.google.ortools.routing.RoutingSearchParameters;
import com.google.ortools.routing.RoutingSearchStatus;
```

### 2. Class and Enum Renames

| v9.15 (`com.google.ortools.constraintsolver`) | v10.0-rc.1 (`com.google.ortools.routing`) |
| :--- | :--- |
| `RoutingIndexManager` | `IndexManager` |
| `RoutingModel` | `Model` |
| `RoutingDimension` | `Dimension` |
| `RoutingModel.PICKUP_AND_DELIVERY_FIFO` | `Model.PICKUP_AND_DELIVERY_FIFO` |
| `RoutingModel.PICKUP_AND_DELIVERY_LIFO` | `Model.PICKUP_AND_DELIVERY_LIFO` |
| `RoutingModel.PICKUP_AND_DELIVERY_NO_ORDER` | `Model.PICKUP_AND_DELIVERY_NO_ORDER` |
| `RoutingModel.PENALIZE_ONCE` / `PENALIZE_PER_INACTIVE` | `Model.PENALIZE_ONCE` / `PENALIZE_PER_INACTIVE` |
| `RoutingSearchParameters`, `RoutingModelParameters`, `FirstSolutionStrategy`, `LocalSearchMetaheuristic`, `RoutingSearchStatus` | Same names, now in `com.google.ortools.routing` |

#### IndexManager, Model, and Dimension

```java
// v9.15 (from VrpTimeWindows.java / VrpStartsEnds.java)
RoutingIndexManager manager =
    new RoutingIndexManager(data.timeMatrix.length, data.vehicleNumber, data.depot);
RoutingModel routing = new RoutingModel(manager);

// Multiple starts and ends:
RoutingIndexManager multiManager =
    new RoutingIndexManager(numLocations, vehicleNumber, data.starts, data.ends);

// Dimension:
boolean unused = routing.addDimension(transitCallbackIndex, 30, 30, false, "Time");
RoutingDimension timeDimension = routing.getMutableDimension("Time");
```

```java
// v10.0-rc.1 (from ortools/routing/samples/java/VrpTimeWindows.java, VrpStartsEnds.java)
IndexManager manager =
    new IndexManager(data.timeMatrix.length, data.vehicleNumber, data.depot);
Model routing = new Model(manager);

// Multiple starts and ends:
IndexManager multiManager =
    new IndexManager(numLocations, vehicleNumber, data.starts, data.ends);

// Dimension (@CanIgnoreReturnValue is now annotated on addDimension*):
routing.addDimension(transitCallbackIndex, 30, 30, false, "Time");
Dimension timeDimension = routing.getMutableDimension("Time");
```

#### Pickup & Delivery Policies and Solution Callbacks

```java
// v9.15 (from VrpPickupDeliveryFifo.java / VrpSolutionCallback.java)
routing.setPickupAndDeliveryPolicyOfAllVehicles(RoutingModel.PICKUP_AND_DELIVERY_FIFO);

static class SolutionCallback implements Runnable {
  private final RoutingIndexManager routingManager;
  private final RoutingModel routingModel;

  public SolutionCallback(final RoutingIndexManager manager, final RoutingModel routing, int limit) {
    routingManager = manager;
    routingModel = routing;
  }
  // ...
}
```

```java
// v10.0-rc.1 (from ortools/routing/samples/java/VrpPickupDeliveryFifo.java, VrpSolutionCallback.java)
routing.setPickupAndDeliveryPolicyOfAllVehicles(Model.PICKUP_AND_DELIVERY_FIFO);

static class SolutionCallback implements Runnable {
  private final IndexManager routingManager;
  private final Model routingModel;

  public SolutionCallback(final IndexManager manager, final Model routing, int limit) {
    routingManager = manager;
    routingModel = routing;
  }
  // ...
}
```

### 3. Global Helper Class Renamed (`main` → `Globals`)

Routing global functions moved from `com.google.ortools.constraintsolver.main`
to `com.google.ortools.routing.Globals` (and constraint solver globals moved
from `com.google.ortools.constraintsolver.main` to
`com.google.ortools.constraintsolver.Globals`):

| v9.15 | v10.0-rc.1 |
| :--- | :--- |
| `main.defaultRoutingSearchParameters()` | `Globals.defaultRoutingSearchParameters()` |
| `main.defaultRoutingModelParameters()` | `Globals.defaultRoutingModelParameters()` |
| `main.findErrorInRoutingSearchParameters(...)` | `Globals.findErrorInRoutingSearchParameters(...)` |

> **Tip**: If the same Java file calls both routing and constraint-solver global
> helpers, qualify one of them explicitly
> (`com.google.ortools.routing.Globals` vs.
> `com.google.ortools.constraintsolver.Globals`) to avoid a class name clash.

```java
// v9.15 (from VrpCapacity.java)
RoutingSearchParameters searchParameters =
    main.defaultRoutingSearchParameters()
        .toBuilder()
        .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
        .setLocalSearchMetaheuristic(LocalSearchMetaheuristic.Value.GUIDED_LOCAL_SEARCH)
        .setTimeLimit(Duration.newBuilder().setSeconds(1).build())
        .build();
```

```java
// v10.0-rc.1 (from ortools/routing/samples/java/VrpCapacity.java)
RoutingSearchParameters searchParameters =
    Globals.defaultRoutingSearchParameters()
        .toBuilder()
        .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
        .setLocalSearchMetaheuristic(LocalSearchMetaheuristic.Value.GUIDED_LOCAL_SEARCH)
        .setTimeLimit(Duration.newBuilder().setSeconds(1).build())
        .build();
```

### 4. Annotations and Fixed / Newly Exposed Java APIs

*   `addDimension`, `addDimensionWithVehicleCapacity`,
    `addDimensionWithVehicleTransits`, and
    `addDimensionWithVehicleTransitAndCapacity` are now annotated with
    `@CanIgnoreReturnValue`, so dummy `boolean unused = ...` assignments are no
    longer required under Error Prone.
*   `model.setAllowedVehiclesForIndex(int[] vehicles, long index)` is fixed
    (#4982).
*   `manager.indicesToNodes(long[] indices)` and
    `manager.getNumberOfUniqueDepots()` are now exposed.
*   Reminder: **JDK 21+** is required.

```java
// v10.0-rc.1
model.setAllowedVehiclesForIndex(new int[] {0, 2}, manager.nodeToIndex(1));
int[] nodes = manager.indicesToNodes(new long[] {0, 1, 2});
```

---

## Python Migration Guide

Python has the largest set of changes in v10.0-rc.1 because SWIG (`pywrapcp`)
was completely removed and replaced with native **pybind11** bindings featuring
PEP 8 `snake_case` naming, Python properties, and native protobuf support.

### 1. Module and Protobuf Imports

| v9.15 (SWIG) | v10.0-rc.1 (pybind11) |
| :--- | :--- |
| `from ortools.constraint_solver import pywrapcp` | `from ortools.routing.python import routing` (and `from ortools.constraint_solver.python import constraint_solver` for CP types) |
| `from ortools.constraint_solver import routing_enums_pb2` | `from ortools.routing import enums_pb2` |
| `from ortools.constraint_solver import routing_parameters_pb2` | `from ortools.routing import parameters_pb2` |
| `from ortools.constraint_solver import routing_ils_pb2` | `from ortools.routing import ils_pb2` |

```python
# v9.15
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2
```

```python
# v10.0-rc.1
from ortools.constraint_solver.python import constraint_solver  # For Assignment, Solver, custom LS operators, etc.
from ortools.routing import enums_pb2, parameters_pb2
from ortools.routing.python import routing
```

> **Naming tip**: Because the module itself is imported as `routing`, avoid
> naming your model instance `routing` (which shadows the module). The v10.0
> samples use `routing_model` or `model`.

### 2. Class and Method Renames (`CamelCase` → `snake_case`)

Class names remain `CamelCase` (without the `Routing` prefix); all functions and
methods use PEP 8 `snake_case`.

#### Top-Level Classes and Functions

| v9.15 (`pywrapcp`) | v10.0-rc.1 (`routing`) |
| :--- | :--- |
| `pywrapcp.RoutingIndexManager(...)` | `routing.IndexManager(...)` |
| `pywrapcp.RoutingModel(manager)` | `routing.Model(manager)` |
| `pywrapcp.RoutingDimension` | `routing.Dimension` |
| `pywrapcp.DefaultRoutingSearchParameters()` | `routing.default_routing_search_parameters()` |
| `pywrapcp.DefaultRoutingModelParameters()` | `routing.default_routing_model_parameters()` |
| `pywrapcp.Solver.DefaultSolverParameters()` | `constraint_solver.Solver.default_solver_parameters()` |

#### `IndexManager` (Watch Out for Non-Trivial Renames)

Several `GetNumberOf*` methods on `IndexManager` were shortened to match C++:

| v9.15 (`RoutingIndexManager`) | v10.0-rc.1 (`IndexManager`) |
| :--- | :--- |
| `manager.GetNumberOfNodes()` | `manager.num_nodes()` |
| `manager.GetNumberOfVehicles()` | `manager.num_vehicles()` |
| `manager.GetNumberOfIndices()` | `manager.num_indices()` |
| `manager.IndexToNode(index)` | `manager.index_to_node(index)` |
| `manager.NodeToIndex(node)` | `manager.node_to_index(node)` |
| `manager.GetStartIndex(v)` | `manager.get_start_index(v)` |
| `manager.GetEndIndex(v)` | `manager.get_end_index(v)` |
| *(new)* | `routing.IndexManager.k_unassigned` / `manager.nodes_to_indices(nodes)` / `manager.indices_to_nodes(indices)` / `manager.num_unique_depots()` |

#### Common `Model` Methods

| v9.15 (`RoutingModel`) | v10.0-rc.1 (`Model`) |
| :--- | :--- |
| `RegisterTransitCallback(cb)` / `RegisterUnaryTransitCallback(cb)` | `register_transit_callback(cb)` / `register_unary_transit_callback(cb)` |
| `RegisterTransitMatrix(m)` / `RegisterUnaryTransitVector(v)` | `register_transit_matrix(m)` / `register_unary_transit_vector(v)` |
| `SetArcCostEvaluatorOfAllVehicles(idx)` / `OfVehicle(idx, v)` | `set_arc_cost_evaluator_of_all_vehicles(idx)` / `_of_vehicle(idx, v)` |
| `AddDimension(...)` / `AddDimensionWithVehicleCapacity(...)` | `add_dimension(...)` / `add_dimension_with_vehicle_capacity(...)` |
| `AddConstantDimension(...)` / `AddConstantDimensionWithSlack(...)` | `add_constant_dimension(...)` / `add_constant_dimension_with_slack(...)` |
| `GetDimensionOrDie(name)` | `get_dimension_or_die(name)` |
| `GetAllDimensionNames()` / `HasDimension(name)` | `get_all_dimension_names()` / `has_dimension(name)` |
| `AddDisjunction(indices, penalty, ...)` | `add_disjunction(indices, penalty, ...)` |
| `AddPickupAndDelivery(p, d)` | `add_pickup_and_delivery(p, d)` |
| `SetPickupAndDeliveryPolicyOfAllVehicles(p)` | `set_pickup_and_delivery_policy_of_all_vehicles(p)` |
| `SetAllowedVehiclesForIndex(vehicles, idx)` | `set_allowed_vehicles_for_index(vehicles, idx)` *(also fixed #4982)* |
| `AddToAssignment(var)` | `add_to_assignment(var)` |
| `AddVariableMinimizedByFinalizer(var)` | `add_variable_minimized_by_finalizer(var)` |
| `AddAtSolutionCallback(cb)` | `add_at_solution_callback(cb)` |
| `CloseModelWithParameters(params)` | `close_model_with_parameters(params)` |
| `ReadAssignmentFromRoutes(routes, ignore)` | `read_assignment_from_routes(routes, ignore)` |
| `SolveWithParameters(params)` | `solve_with_parameters(params)` |
| `SolveFromAssignmentWithParameters(init, params)` | `solve_from_assignment_with_parameters(init, params)` |
| `Start(v)` / `End(v)` / `IsStart(i)` / `IsEnd(i)` | `start(v)` / `end(v)` / `is_start(i)` / `is_end(i)` |
| `IsVehicleUsed(sol, v)` / `Size()` / `nodes()` / `vehicles()` / `status()` | `is_vehicle_used(sol, v)` / `size()` / `nodes()` / `vehicles()` / `status()` |
| `NextVar(i)` / `VehicleVar(i)` / `ActiveVar(i)` / `CostVar()` | `next_var(i)` / `vehicle_var(i)` / `active_var(i)` / `cost_var()` |
| `GetArcCostForVehicle(i, j, v)` | `get_arc_cost_for_vehicle(i, j, v)` |
| `GetAmortizedLinearCostFactorOfVehicles()` | `get_amortized_linear_cost_factors_of_vehicles()` *(plural `factors`)* |
| `GetAmortizedQuadraticCostFactorOfVehicles()` | `get_amortized_quadratic_cost_factors_of_vehicles()` *(plural `factors`)* |

#### Common `Dimension` and `Assignment` Methods

| v9.15 | v10.0-rc.1 |
| :--- | :--- |
| `dim.CumulVar(i)` / `SlackVar(i)` / `TransitVar(i)` | `dim.cumul_var(i)` / `slack_var(i)` / `transit_var(i)` |
| `dim.SetGlobalSpanCostCoefficient(c)` | `dim.set_global_span_cost_coefficient(c)` |
| `dim.SetSpanCostCoefficientForVehicle(c, v)` | `dim.set_span_cost_coefficient_for_vehicle(c, v)` |
| `dim.SetSlackCostCoefficientForAllVehicles(c)` | `dim.set_slack_cost_coefficient_for_all_vehicles(c)` |
| `dim.SetCumulVarSoftUpperBound(i, limit, c)` | `dim.set_cumul_var_soft_upper_bound(i, limit, c)` |
| `dim.GetTransitValue(i, j, v)` | `dim.get_transit_value(i, j, v)` |
| `dim.SetBreakIntervalsOfVehicle(breaks, v, transits)` | `dim.set_break_intervals_of_vehicle(breaks, v, transits)` |
| `solution.ObjectiveValue()` | `solution.objective_value()` |
| `solution.Value(var)` / `Min(var)` / `Max(var)` | `solution.value(var)` / `min(var)` / `max(var)` |
| `solution.IntervalVarContainer()` (`.Size()`, `.Element(i)`) | `solution.interval_var_container()` (`.size()`, `.element(i)`) |
| `var.SetRange(lo, hi)` / `SetValue(v)` / `SetValues(vals)` / `Value()` | `var.set_range(lo, hi)` / `set_value(v)` / `set_values(vals)` / `value()` |

### 3. Methods Changed to Properties

Several zero-argument getters are now Python **properties** (remove the trailing
`()`):

| v9.15 (Method Call) | v10.0-rc.1 (Property) |
| :--- | :--- |
| `routing.solver()` | `routing_model.solver` |
| `interval_var.Name()` / `int_var.Name()` | `interval_var.name` / `int_var.name` |
| `solver.Parameters()` | `solver.parameters` |
| `solver.WallTime()` | `solver.wall_time_ms` |

```python
# v9.15 (from vrp_breaks.py)
solver = routing.solver()
intervals = solution.IntervalVarContainer()
for i in range(intervals.Size()):
    brk = intervals.Element(i)
    if brk.PerformedValue():
        print(f"{brk.Var().Name()}: Start({brk.StartValue()}) Duration({brk.DurationValue()})")
```

```python
# v10.0-rc.1 (from ortools/routing/samples/vrp_breaks.py)
solver = routing_model.solver
intervals = solution.interval_var_container()
for i in range(intervals.size()):
    brk = intervals.element(i)
    if brk.performed_value():
        print(f"{brk.var().name}: Start({brk.start_value()}) Duration({brk.duration_value()})")
```

### 4. Strict Sequence Types in pybind11

Where SWIG accepted arbitrary Python iterables (such as `dict.values()`),
pybind11 requires a concrete sequence such as `list`:

```python
# v9.15 (from cvrptw_break.py)
time_dimension.SetBreakIntervalsOfVehicle(
    break_intervals[v], v, node_visit_transit.values()
)
```

```python
# v10.0-rc.1 (from ortools/routing/samples/cvrptw_break.py)
time_dimension.set_break_intervals_of_vehicle(
    break_intervals[v], v, list(node_visit_transit.values())
)
```

### 5. Enums, Search Status, and Parameters

Protobuf enums moved from `ortools.constraint_solver.routing_enums_pb2` to
`ortools.routing.enums_pb2`. Model-nested enums (such as
`PICKUP_AND_DELIVERY_FIFO` / `_LIFO`) moved from `pywrapcp.RoutingModel` to
`routing.Model`.

```python
# v9.15 (from tsp.py / vrp_capacity.py / vrp_pickup_delivery_fifo.py)
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2

routing.SetPickupAndDeliveryPolicyOfAllVehicles(
    pywrapcp.RoutingModel.PICKUP_AND_DELIVERY_FIFO
)

search_parameters = pywrapcp.DefaultRoutingSearchParameters()
search_parameters.first_solution_strategy = (
    routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
)
search_parameters.local_search_metaheuristic = (
    routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
)
search_parameters.time_limit.FromSeconds(5)

solution = routing.SolveWithParameters(search_parameters)
status = routing.status()
print(f"Status: {routing_enums_pb2.RoutingSearchStatus.Value.Name(status)}")
```

```python
# v10.0-rc.1 (from ortools/routing/samples/tsp.py, vrp_capacity.py, vrp_pickup_delivery_fifo.py)
from ortools.routing import enums_pb2
from ortools.routing.python import routing

routing_model.set_pickup_and_delivery_policy_of_all_vehicles(
    routing.Model.PICKUP_AND_DELIVERY_FIFO
)

search_parameters = routing.default_routing_search_parameters()
search_parameters.first_solution_strategy = (
    enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
)
search_parameters.local_search_metaheuristic = (
    enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
)
search_parameters.time_limit.seconds = 5

solution = routing_model.solve_with_parameters(search_parameters)
status = routing_model.status()
print(f"Status: {enums_pb2.RoutingSearchStatus.Value.Name(status)}")
if status not in (
    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
    enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS,
):
    print("No solution found!")
```

### 6. Underlying CP Solver (`routing_model.solver`): Variables and Constraints

When adding extra CP constraints on top of the routing model:

*   Variable factory methods are now prefixed with `new_` (`new_int_var`,
    `new_bool_var`, `new_interval_var`, `new_fixed_duration_interval_var`).
*   Two-step `solver.Add(solver.<Constraint>(...))` patterns now have single-step
    `solver.add_<constraint>(...)` helpers (`add_cumulative`,
    `add_sum_equality`, `add_all_different`, `add_not_member_ct`, etc.).
*   General expression constraints (`==`, `<=`, `>=`, etc.) still use
    `solver.add(...)`, and expression helpers use `snake_case` (`solver.sum`,
    `solver.max`, `solver.min`, or Python `+` directly on `IntExpr`).

#### Breaks and Depot Resource Constraints

```python
# v9.15 (from vrp_breaks_from_start.py / vrp_resources.py)
solver = routing.solver()

# Break relative to start time:
start_var = time_dimension.CumulVar(routing.Start(v))
break_start = solver.Sum([solver.IntVar(25, 45), start_var])
break_intervals = [
    solver.FixedDurationIntervalVar(break_start, 5, f"Break for vehicle {v}")
]

# Depot cumulative capacity:
intervals = [
    solver.FixedDurationIntervalVar(
        time_dimension.CumulVar(routing.Start(i)), load_time, "depot_interval"
    )
    for i in range(num_vehicles)
]
solver.Add(solver.Cumulative(intervals, [1] * len(intervals), depot_capacity, "depot"))
```

```python
# v10.0-rc.1 (from ortools/routing/samples/vrp_breaks_from_start.py, vrp_resources.py)
solver = routing_model.solver

# Break relative to start time:
start_var = time_dimension.cumul_var(routing_model.start(v))
break_start = solver.new_int_var(25, 45, f"break_start_window_{v}") + start_var
break_intervals = [
    solver.new_fixed_duration_interval_var(break_start, 5, f"Break for vehicle {v}")
]

# Depot cumulative capacity:
intervals = [
    solver.new_fixed_duration_interval_var(
        time_dimension.cumul_var(routing_model.start(i)), load_time, "depot_interval"
    )
    for i in range(num_vehicles)
]
solver.add_cumulative(intervals, [1] * len(intervals), depot_capacity, "depot")
```

#### Pickup & Delivery and Expression Constraints

```python
# v9.15 (from vrp_pickup_delivery.py / vrp_node_max.py)
routing.AddPickupAndDelivery(pickup_index, delivery_index)
routing.solver().Add(
    routing.VehicleVar(pickup_index) == routing.VehicleVar(delivery_index)
)
routing.solver().Add(
    distance_dimension.CumulVar(pickup_index)
    <= distance_dimension.CumulVar(delivery_index)
)

# Sum equality over expressions:
solver = routing.solver()
terms = [(cond * solver.Max(slack_var, node_val)).Var()]
solver.Add(solver.Sum(terms) == target_var)
```

```python
# v10.0-rc.1 (from ortools/routing/samples/vrp_pickup_delivery.py, vrp_node_max.py)
routing_model.add_pickup_and_delivery(pickup_index, delivery_index)
routing_model.solver.add(
    routing_model.vehicle_var(pickup_index)
    == routing_model.vehicle_var(delivery_index)
)
routing_model.solver.add(
    distance_dimension.cumul_var(pickup_index)
    <= distance_dimension.cumul_var(delivery_index)
)

# Sum equality over expressions (no .Var() needed):
solver = routing_model.solver
terms = [cond * solver.max(slack_var, node_val)]
solver.add_sum_equality(terms, target_var)
```

### 7. Solution Callbacks and Custom Local Search Operators

#### Solution Callback

```python
# v9.15 (from vrp_solution_callback.py)
class SolutionCallback:
    def __call__(self):
        objective = int(self._routing_model_ref().CostVar().Value())
        if self._counter > self._counter_limit:
            self._routing_model_ref().solver().FinishCurrentSearch()

routing_model.AddAtSolutionCallback(solution_callback)
```

```python
# v10.0-rc.1 (from ortools/routing/samples/vrp_solution_callback.py)
class SolutionCallback:
    def __call__(self) -> None:
        objective = int(self._routing_model_ref().cost_var().value())
        if self._counter > self._counter_limit:
            self._routing_model_ref().solver.finish_current_search()

routing_model.add_at_solution_callback(solution_callback)
```

#### Custom `IntVarLocalSearchOperator`

In v10.0-rc.1, custom Python local search operators and filters subclass
`constraint_solver.IntVarLocalSearchOperator` / `LocalSearchFilter` using
`snake_case` overrides, and object lifetimes are safely retained by
`add_local_search_operator` / `add_local_search_filter` (fixes #5083):

```python
# v9.15
class MyOperator(pywrapcp.IntVarLocalSearchOperator):
    def __init__(self, int_vars):
        super().__init__(int_vars)
        self._index = 0

    def OneNeighbor(self):
        current_value = self.OldValue(self._index)
        self.SetValue(self._index, 1 - current_value)
        self._index = (self._index + 1) % self.Size()
        return True

    def OnStart(self):
        pass

    def IsIncremental(self):
        return False

routing.AddLocalSearchOperator(MyOperator(int_vars))
```

```python
# v10.0-rc.1
from ortools.constraint_solver.python import constraint_solver

class MyOperator(constraint_solver.IntVarLocalSearchOperator):
    def __init__(self, int_vars):
        super().__init__(int_vars)
        self._index = 0

    def one_neighbor(self) -> bool:
        current_value = self.old_value(self._index)
        self.set_value(self._index, 1 - current_value)
        self._index = (self._index + 1) % self.size()
        return True

    def on_start(self) -> None:
        pass

    def is_incremental(self) -> bool:
        return False

routing_model.add_local_search_operator(MyOperator(int_vars))
```

### 8. End-to-End Python Example

#### v9.15

```python
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2


def main():
    num_locations = 5
    num_vehicles = 1
    depot = 0

    manager = pywrapcp.RoutingIndexManager(num_locations, num_vehicles, depot)
    routing = pywrapcp.RoutingModel(manager)

    def distance_callback(from_index, to_index):
        from_node = int(manager.IndexToNode(from_index))
        to_node = int(manager.IndexToNode(to_index))
        return abs(to_node - from_node)

    transit_callback_index = routing.RegisterTransitCallback(distance_callback)
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )

    assignment = routing.SolveWithParameters(search_parameters)

    print(f"Objective: {assignment.ObjectiveValue()}")
    index = routing.Start(0)
    plan_output = "Route for vehicle 0:\n"
    route_distance = 0
    while not routing.IsEnd(index):
        plan_output += f"{manager.IndexToNode(index)} -> "
        previous_index = index
        index = assignment.Value(routing.NextVar(index))
        route_distance += routing.GetArcCostForVehicle(previous_index, index, 0)
    plan_output += f"{manager.IndexToNode(index)}\n"
    plan_output += f"Distance of the route: {route_distance}m\n"
    print(plan_output)
```

#### v10.0-rc.1

```python
from ortools.routing import enums_pb2
from ortools.routing.python import routing


def main() -> None:
    num_locations = 5
    num_vehicles = 1
    depot = 0

    manager = routing.IndexManager(num_locations, num_vehicles, depot)
    routing_model = routing.Model(manager)

    def distance_callback(from_index: int, to_index: int) -> int:
        from_node = int(manager.index_to_node(from_index))
        to_node = int(manager.index_to_node(to_index))
        return abs(to_node - from_node)

    transit_callback_index = routing_model.register_transit_callback(distance_callback)
    routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)

    search_parameters = routing.default_routing_search_parameters()
    search_parameters.first_solution_strategy = (
        enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )

    assignment = routing_model.solve_with_parameters(search_parameters)

    print(f"Objective: {assignment.objective_value()}")
    index = routing_model.start(0)
    plan_output = "Route for vehicle 0:\n"
    route_distance = 0
    while not routing_model.is_end(index):
        plan_output += f"{manager.index_to_node(index)} -> "
        previous_index = index
        index = assignment.value(routing_model.next_var(index))
        route_distance += routing_model.get_arc_cost_for_vehicle(
            previous_index, index, 0
        )
    plan_output += f"{manager.index_to_node(index)}\n"
    plan_output += f"Distance of the route: {route_distance}m\n"
    print(plan_output)
```

---

## References

*   Full updated samples in C++, .NET, Java, and Python:
    [`ortools/routing/samples/`](../samples/)
*   Routing solver overview and modeling recipes: [ROUTING.md](ROUTING.md)
*   Full v10.0-rc.1 release notes:
    [`docs/CHANGELOG.md`](../../../docs/CHANGELOG.md)
