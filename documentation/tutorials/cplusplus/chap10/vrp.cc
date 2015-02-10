// Copyright 2011-2014 Google
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
//
//
//  Simple program to solve the VRP with Local Search in or-tools.

#include <string>
#include <fstream>

#include "base/commandlineflags.h"
#include "constraint_solver/routing.h"
#include "base/join.h"
#include "base/timer.h"

#include "cvrp_data.h"
#include "cvrp_solution.h"
#include "routing_common/tsplib_reader.h"

DEFINE_int32(depot, 1, "The starting node of the tour.");
//DEFINE_string(instance_file, "", "Input file with TSPLIB data.");
//DEFINE_string(solution_file, "", "Output file with generated solution in (C)VRP format.");
//DECLARE_int32(number_vehicles);
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms.");

namespace operations_research {

void  VRPSolver (const CVRPData & data) {

  const int size = data.Size();
  
  RoutingModel routing(size, FLAGS_number_vehicles);
  routing.SetCost(NewPermanentCallback(&data, &CVRPData::Distance));

  // Disabling Large Neighborhood Search, comment out to activate it.
  //routing.SetCommandLineOption("routing_no_lns", "true");

  if (FLAGS_time_limit_in_ms > 0) {
    routing.UpdateTimeLimit(FLAGS_time_limit_in_ms);
  }

  // Setting depot
  CHECK_GT(FLAGS_depot, 0) << " Because we use the" << " TSPLIB convention, the depot id must be > 0";
  RoutingModel::NodeIndex depot(FLAGS_depot -1);
  routing.SetDepot(depot);

  routing.CloseModel();

  //  Forbidding empty routes
  for (int vehicle_nbr = 0; vehicle_nbr < FLAGS_number_vehicles; ++vehicle_nbr) {
    IntVar* const start_var = routing.NextVar(routing.Start(vehicle_nbr));
    for (int64 node_index = routing.Size(); node_index < routing.Size() + routing.vehicles(); ++node_index) {
      start_var->RemoveValue(node_index);
    }
  }


  
  // SOLVE
  const Assignment* solution = routing.Solve();

  // INSPECT SOLUTION
  if (solution != NULL) {
    CVRPSolution cvrp_sol(data, &routing, solution);
    cvrp_sol.SetName(StrCat("Solution for instance ", data.Name(), " computed by vrp.cc"));
    // test solution
    if (!cvrp_sol.IsSolution()) {
      LOG(ERROR) << "Solution is NOT feasible!";
    } else {
      LG << "Solution is feasible and has an obj value of " << cvrp_sol.ComputeObjectiveValue();
      //  SAVE SOLUTION IN CVRP FORMAT
      if (FLAGS_solution_file != "") {
        cvrp_sol.Write(FLAGS_solution_file);
      }
    }

    // Solution cost.
    LG << "Obj value: " << solution->ObjectiveValue();
    // Inspect solution.
    std::string route;
    for (int vehicle_nbr = 0; vehicle_nbr < FLAGS_number_vehicles; ++vehicle_nbr) {
      route = "";
      for (int64 node = routing.Start(vehicle_nbr); !routing.IsEnd(node);
        node = solution->Value(routing.NextVar(node))) {
        route = StrCat(route, StrCat(routing.IndexToNode(node).value() + 1 , " -> "));
      }
      route = StrCat(route,  routing.IndexToNode(routing.End(vehicle_nbr)).value() + 1 );
      LG << "Route #" << vehicle_nbr + 1 << std::endl << route << std::endl;
    }

  } else {
    LG << "No solution found.";
  }

}  //  void  VRPSolver (CVRPData & data)


}  // namespace operations_research


int main(int argc, char **argv) {
  std::string usage("Computes a simple VRP.\n"
                     "See Google or-tools tutorials\n"
                     "Sample usage:\n\n");
  usage += argv[0];
  usage += " -instance_file=<TSPLIB file>";

  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_instance_file == "") {
    std::cout << google::ProgramUsage();
    exit(-1);
  }
  operations_research::TSPLIBReader tsplib_reader(FLAGS_instance_file);
  operations_research::CVRPData cvrp_data(tsplib_reader);
  operations_research::VRPSolver(cvrp_data);

  return 0;
}  //  main