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
//  Simple program to solve the CVRP with Local Search in or-tools.

#include <string>
#include <fstream>

#include "base/commandlineflags.h"
#include "constraint_solver/routing.h"
#include "base/join.h"
#include "base/timer.h"

#include "constraint_solver/routing.h"

#include "common/limits.h"

#include "cvrp_data.h"
#include "cvrp_solution.h"
#include "routing_common/tsplib_reader.h"

DEFINE_int32(depot, 1, "The starting node of the tour.");
DEFINE_string(initial_solution_file, "", "Input file with a valid feasible solution.");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms. 0 means no limit.");
DEFINE_int32(no_solution_improvement_limit, 200, "Number of allowed solutions without improving the objective value.");

namespace operations_research {

void  CVRPBasicSolver (const CVRPData & data) {

  const int size = data.Size();
  const int64 capacity = data.Capacity();

  CHECK_GT(FLAGS_number_vehicles, 0) << "We need at least one vehicle!";
  // Little check to see if we have enough vehicles
  CHECK_GT(capacity, data.TotalDemand()/FLAGS_number_vehicles) << "No enough vehicles to cover all the demands";
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

  // add capacities constraints
  std::vector<int64> demands(size);
  for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < size; ++i) {
    demands[i.value()] = data.Demand(i);
  }
  routing.AddVectorDimension(&demands[0], capacity, true, "Demand");

  routing.CloseModel();

  //  Use initial solution if provided
  Assignment * initial_sol = NULL;
  if (FLAGS_initial_solution_file != "") {
    initial_sol = routing.solver()->MakeAssignment();//needed by RoutesToAssignment but actually doesn't do much... to detail
    CVRPSolution cvrp_init_sol(data, FLAGS_initial_solution_file);
 
    routing.RoutesToAssignment(cvrp_init_sol.Routes(), true, true, initial_sol);
 
    if (routing.solver()->CheckAssignment(initial_sol)) {// just in case and to fill the complementary variables
      CVRPSolution temp_sol(data, &routing, initial_sol);
      LG << "Initial solution provided is feasible with obj = " << temp_sol.ComputeObjectiveValue();//
    } else {
      LG << "Initial solution provided is NOT feasible... exit!";
      return;
    }
  }

  NoImprovementLimit * const no_improvement_limit =  MakeNoImprovementLimit(routing.solver(), FLAGS_no_solution_improvement_limit);

  SearchLimit * const ctrl_break =  MakeCatchCTRLBreakLimit(routing.solver());
  routing.AddSearchMonitor(ctrl_break);
  const Assignment* solution = routing.Solve(initial_sol);// if initial_sol == NULL, solves from scratch

  // INSPECT SOLUTION
  if (solution != NULL) {
    CVRPSolution cvrp_sol(data, &routing, solution);
    cvrp_sol.SetName(StrCat("Solution for instance ", data.Name(), " computed by vrp.cc"));
    // test solution
    if (!cvrp_sol.IsFeasibleSolution()) {
      LOG(ERROR) << "Solution is NOT feasible!";
    } else {
      LG << "Solution is feasible and has an obj value of " << cvrp_sol.ComputeObjectiveValue();
      //  SAVE SOLUTION IN CVRP FORMAT
      if (FLAGS_solution_file != "") {
        cvrp_sol.Write(FLAGS_solution_file);
      } else {
        cvrp_sol.Print(std::cout);
      }
    }

  } else {
    LG << "No solution found.";
  }

}  //  void  VRPSolver (CVRPData & data)


}  // namespace operations_research


int main(int argc, char **argv) {
  std::string usage("Computes a simple CVRP.\n"
                     "See Google or-tools tutorials\n"
                     "Sample usage:\n\n");
  usage += argv[0];
  usage += " -instance_file=<TSPLIB file>\n";

  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_instance_file == "") {
    std::cout << google::ProgramUsage();
    exit(-1);
  }
  operations_research::TSPLIBReader tsplib_reader(FLAGS_instance_file);
  operations_research::CVRPData cvrp_data(tsplib_reader);
  operations_research::CVRPBasicSolver(cvrp_data);

  return 0;
}  //  main