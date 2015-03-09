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
//  Simple CVRP instance generator.

#include "base/commandlineflags.h"

#include "cvrp_data_generator.h"
#include "cvrp_data.h"

DEFINE_int32(depot, 1, "Depot of the CVRP instance. Must be greater of equal to 1.");

int main(int argc, char **argv) {

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::SetUsageMessage(operations_research::GeneratorUsage(argv[0], "CVRP"));

  if (FLAGS_instance_name != "" && FLAGS_instance_size > 2) {
    operations_research::CVRPDataGenerator cvrp_data_generator(FLAGS_instance_name, FLAGS_instance_size);
    CHECK_GE(FLAGS_depot, 1) << "Because we use the TSPLIB format, the depot must be greater or equal to 1.";
    CHECK_LE(FLAGS_depot, FLAGS_instance_size) << "The depot must be in range 1-" << FLAGS_instance_size << ".";
    cvrp_data_generator.SetDepot(operations_research::RoutingModel::NodeIndex(FLAGS_depot - 1));
    operations_research::CVRPData cvrp_data(cvrp_data_generator);
    if (FLAGS_distance_file != "") {
      cvrp_data.WriteDistanceMatrix(FLAGS_distance_file);
    }
    if (FLAGS_instance_file == "") {
      cvrp_data.PrintTSPLIBInstance(std::cout);
    } else {
      cvrp_data.WriteTSPLIBInstance(FLAGS_instance_file);
    }
  } else {
    std::cout << google::ProgramUsage();
    exit(-1);
  }
  return 0;
}