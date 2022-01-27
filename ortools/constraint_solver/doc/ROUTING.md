# Using the Vehicle Routing solver

https://developers.google.com/optimization/routing

## Documentation structure

This document presents modeling recipes for the Vehicle routing solver.
These are grouped by type:

* [Travelling Salesman Problem](TSP.md)
* [Vehicle Routing Problem](VRP.md)
* [Pickup and Delivery Problem](PDP.md)

OR-Tools comes with lots of vehicle routing samples given in C++, Python, Java
and .Net. Each language have different requirements for the code samples.

## Basic example

### C++ code samples

```cpp
#include <cmath>
#include <cstdint>

#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"

namespace operations_research {

void SimpleRoutingProgram() {
  // Instantiate the data problem.
  int num_location = 5;
  int num_vehicles = 1;
  RoutingIndexManager::NodeIndex depot{0};

  // Create Routing Index Manager
  RoutingIndexManager manager(num_location, num_vehicles, depot);

  // Create Routing Model.
  RoutingModel routing(manager);

  // Define cost of each arc.
  int distance_call_index = routing.RegisterTransitCallback(
      [&manager](int64_t from_index, int64_t to_index) -> int64_t {
        // Convert from routing variable Index to user NodeIndex.
        auto from_node = manager.IndexToNode(from_index).value();
        auto to_node = manager.IndexToNode(to_index).value();
        return std::abs(to_node - from_node);
      });
  routing.SetArcCostEvaluatorOfAllVehicles(distance_call_index);

  // Setting first solution heuristic.
  RoutingSearchParameters searchParameters = DefaultRoutingSearchParameters();
  searchParameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);

  // Solve the problem.
  const Assignment* solution = routing.SolveWithParameters(searchParameters);

  // Print solution on console.
  LOG(INFO) << "Objective: " << solution->ObjectiveValue();
  // Inspect solution.
  int64_t index = routing.Start(0);
  LOG(INFO) << "Route for Vehicle 0:";
  int64_t route_distance{0};
  std::ostringstream route;
  while (routing.IsEnd(index) == false) {
    route << manager.IndexToNode(index).value() << " -> ";
    int64_t previous_index = index;
    index = solution->Value(routing.NextVar(index));
    route_distance +=
        routing.GetArcCostForVehicle(previous_index, index, int64_t{0});
  }
  LOG(INFO) << route.str() << manager.IndexToNode(index).value();
  LOG(INFO) << "Distance of the route: " << route_distance << "m";
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::SimpleRoutingProgram();
  return EXIT_SUCCESS;
}
```

### Python code samples

```python
"""Vehicle Routing example."""

from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp


def main():
  """Entry point of the program."""
  # Instantiate the data problem.
  num_locations = 5
  num_vehicles = 1
  depot = 0

  # Create the routing index manager.
  manager = pywrapcp.RoutingIndexManager(
      num_locations,
      num_vehicles,
      depot)

  # Create Routing Model.
  routing = pywrapcp.RoutingModel(manager)

  # Create and register a transit callback.
  def distance_callback(from_index, to_index):
    """Returns the absolute difference between the two nodes."""
    # Convert from routing variable Index to user NodeIndex.
    from_node = int(manager.IndexToNode(from_index))
    to_node = int(manager.IndexToNode(to_index))
    return abs(to_node - from_node)

  transit_callback_index = routing.RegisterTransitCallback(distance_callback)

  # Define cost of each arc.
  routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

  # Setting first solution heuristic.
  search_parameters = pywrapcp.DefaultRoutingSearchParameters()
  search_parameters.first_solution_strategy = (
      routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)  # pylint: disable=no-member

  # Solve the problem.
  assignment = routing.SolveWithParameters(search_parameters)

  # Print solution on console.
  print('Objective: {}'.format(assignment.ObjectiveValue()))
  index = routing.Start(0)
  plan_output = 'Route for vehicle 0:\n'
  route_distance = 0
  while not routing.IsEnd(index):
    plan_output += '{} -> '.format(manager.IndexToNode(index))
    previous_index = index
    index = assignment.Value(routing.NextVar(index))
    route_distance += routing.GetArcCostForVehicle(previous_index, index, 0)
  plan_output += '{}\n'.format(manager.IndexToNode(index))
  plan_output += 'Distance of the route: {}m\n'.format(route_distance)
  print(plan_output)


if __name__ == '__main__':
  main()
```

### Java code samples

```java
package com.google.ortools.constraintsolver.samples;
import static java.lang.Math.abs;

import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingSearchParameters;
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.RoutingIndexManager;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.main;
import java.util.logging.Logger;

/** Minimal Routing example to showcase calling the solver.*/
public class SimpleRoutingProgram {
  private static final Logger logger =
      Logger.getLogger(SimpleRoutingProgram.class.getName());

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Instantiate the data problem.
    final int numLocation = 5;
    final int numVehicles = 1;
    final int depot = 0;

    // Create Routing Index Manager
    RoutingIndexManager manager = new RoutingIndexManager(numLocation, numVehicles, depot);

    // Create Routing Model.
    RoutingModel routing = new RoutingModel(manager);

    // Create and register a transit callback.
    final int transitCallbackIndex = routing.registerTransitCallback(
        (long fromIndex, long toIndex) -> {
          // Convert from routing variable Index to user NodeIndex.
          int fromNode = manager.indexToNode(fromIndex);
          int toNode = manager.indexToNode(toIndex);
          return abs(toNode - fromNode);
        });

    // Define cost of each arc.
    routing.setArcCostEvaluatorOfAllVehicles(transitCallbackIndex);

    // Setting first solution heuristic.
    RoutingSearchParameters searchParameters =
        main.defaultRoutingSearchParameters()
            .toBuilder()
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();

    // Solve the problem.
    Assignment solution = routing.solveWithParameters(searchParameters);

    // Print solution on console.
    logger.info("Objective: " + solution.objectiveValue());
    // Inspect solution.
    long index = routing.start(0);
    logger.info("Route for Vehicle 0:");
    long routeDistance = 0;
    String route = "";
    while (!routing.isEnd(index)) {
      route += manager.indexToNode(index) + " -> ";
      long previousIndex = index;
      index = solution.value(routing.nextVar(index));
      routeDistance += routing.getArcCostForVehicle(previousIndex, index, 0);
    }
    route += manager.indexToNode(index);
    logger.info(route);
    logger.info("Distance of the route: " + routeDistance + "m");
  }
}
```

### .Net code samples


```cs
using System;
using Google.OrTools.ConstraintSolver;

/// <summary>
///   This is a sample using the routing library .Net wrapper.
/// </summary>
public class SimpleRoutingProgram
{
    public static void Main(String[] args)
    {
        // Instantiate the data problem.
        const int numLocation = 5;
        const int numVehicles = 1;
        const int depot = 0;

        // Create Routing Index Manager
        RoutingIndexManager manager = new RoutingIndexManager(numLocation, numVehicles, depot);

        // Create Routing Model.
        RoutingModel routing = new RoutingModel(manager);

        // Create and register a transit callback.
        int transitCallbackIndex = routing.RegisterTransitCallback((long fromIndex, long toIndex) => {
            // Convert from routing variable Index to distance matrix NodeIndex.
            var fromNode = manager.IndexToNode(fromIndex);
            var toNode = manager.IndexToNode(toIndex);
            return Math.Abs(toNode - fromNode);
        });

        // Define cost of each arc.
        routing.SetArcCostEvaluatorOfAllVehicles(transitCallbackIndex);

        // Setting first solution heuristic.
        RoutingSearchParameters searchParameters =
            operations_research_constraint_solver.DefaultRoutingSearchParameters();
        searchParameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.PathCheapestArc;

        // Solve the problem.
        Assignment solution = routing.SolveWithParameters(searchParameters);

        // Print solution on console.
        Console.WriteLine("Objective: {0}", solution.ObjectiveValue());
        // Inspect solution.
        long index = routing.Start(0);
        Console.WriteLine("Route for Vehicle 0:");
        long route_distance = 0;
        while (routing.IsEnd(index) == false)
        {
            Console.Write("{0} -> ", manager.IndexToNode((int)index));
            long previousIndex = index;
            index = solution.Value(routing.NextVar(index));
            route_distance += routing.GetArcCostForVehicle(previousIndex, index, 0);
        }
        Console.WriteLine("{0}", manager.IndexToNode(index));
        Console.WriteLine("Distance of the route: {0}m", route_distance);
    }
}
```

## Misc
Images have been generated using [routing_svg.py](routing_svg.py) through bash
 script [generate_svg.sh](generate_svg.sh).
