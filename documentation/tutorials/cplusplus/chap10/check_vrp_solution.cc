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
//  Simple program to test a VRP solution.
#include <iostream>

#include "base/commandlineflags.h"

#include "cvrp_data.h"
#include "cvrp_solution.h"
#include "routing_common/tsplib_reader.h"

DECLARE_int32(width_size);

//DEFINE_string(instance_file, "", "TSPLIB instance file.");
//DEFINE_string(solution_file, "", "(C)VRP solution file.");

//DEFINE_string(distance_file, "", "Matrix distance file.");

int main(int argc, char **argv) {
  std::string usage("Checks the feasibility of a VRP solution.\n"
                    "See Google or-tools tutorials\n"
                    "Sample usage:\n\n");
  usage += argv[0];
  usage += " -instance_file=<TSPLIB file> -solution_file=<(C)VRP solution>\n";

  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_instance_file != "" && FLAGS_solution_file != "") {
    operations_research::TSPLIBReader tsp_data_reader(FLAGS_instance_file);
    operations_research::CVRPData cvrp_data(tsp_data_reader);
    operations_research::CVRPSolution cvrp_sol(cvrp_data, FLAGS_solution_file);
    if (FLAGS_distance_file != "") {
      cvrp_data.WriteDistanceMatrix(FLAGS_distance_file);
    }
    if (cvrp_sol.IsSolution()) {
      LG << "Solution is feasible!";
      LG << "Obj value = " << cvrp_sol.ComputeObjectiveValue();
      if (cvrp_sol.IsFeasibleSolution()) {
        LG << "Solution if even CVRP feasible!!!";
      }
    } else {
      LG << "Solution is NOT feasible...";
    }
  } else {
    std::cout << google::ProgramUsage();
    exit(-1);
  }
  return 0;
}