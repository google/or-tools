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
//  Simple program to visualize a CVRP solution.

#include <iostream>

#include "base/commandlineflags.h"

#include "cvrp_data.h"
#include "cvrp_solution.h"
#include "routing_common/tsplib_reader.h"
#include "cvrp_epix_data.h"

int main(int argc, char **argv) {
  std::string usage("Prints a CVRP solution in ePiX format.\n"
                    "See Google or-tools tutorials\n"
                    "Sample usage:\n\n");
  usage += argv[0];
  usage += " -instance_file=<TSPLIB file> -solution_file=<CVRP solution> > epix_file.xp\n\n";
  usage += " ./elaps -pdf epix_file.xp\n";

  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_instance_file != "" && FLAGS_solution_file != "") {
    operations_research::TSPLIBReader tsplib_reader(FLAGS_instance_file);
    operations_research::CVRPData cvrp_data(tsplib_reader);
    operations_research::CVRPSolution cvrp_sol(cvrp_data, FLAGS_solution_file);
    if (cvrp_sol.IsFeasibleSolution()) {
      if (cvrp_data.IsVisualizable()) {
        operations_research::CVRPEpixData epix_data(cvrp_data);
        epix_data.PrintSolution(std::cout, cvrp_sol);
      } else {
        LG << "Solution is not visualizable!";
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