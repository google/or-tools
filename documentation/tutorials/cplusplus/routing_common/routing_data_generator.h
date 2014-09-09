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
//  Common base to generate routing data (instances).

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_GENERATOR_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_GENERATOR_H

#include <string>

#include "base/join.h"
#include "base/integral_types.h"
#include "constraint_solver/routing.h"

#include "common/random.h"
#include "routing_common/routing_common_flags.h"
#include "routing_common/routing_common.h"
#include "routing_common/routing_random.h"
#include "routing_common/routing_distance.h"

namespace operations_research {
  class RoutingDataGenerator {
  public:
    RoutingDataGenerator(std::string problem_name, std::string instance_name, int32 size) :
      problem_name_(problem_name), instance_name_(instance_name), size_(size), randomizer_(GetSeed()),
      coordinates_(size_), dist_coords_(coordinates_) {}
    int64 Distance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
      return dist_coords_.Distance(i,j);
    }

    Point Coordinate(RoutingModel::NodeIndex i) const {
      return coordinates_.Coordinate(i);
    }
    std::string ProblemName() const {
      return problem_name_;
    }
    std::string InstanceName() const {
      return instance_name_;
    }
    int32 Size() const {
      return size_;
    }

    void ReplaceDistance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j, int64 dist) {
      dist_coords_.ReplaceDistance(i, j, dist);
    }
  protected:
    std::string problem_name_;
    std::string instance_name_;
    int32 size_;
    ACMRandom randomizer_;
    GenerateTWODCoordinates coordinates_;
    DistancesFromTWODCoordinates dist_coords_;
    
  };

  //  Common usage message for instance generators.
  std::string GeneratorUsage(std::string invocation, std::string problem_name) {
      return StrCat("Generates a ", problem_name, " instance.\n"
                        "See Google or-tools tutorials\n"
                        "Sample usage:\n\n",
                        invocation,
                        " -instance_name=<name> -instance_size=<size>\n\n");
  }
}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_GENERATOR_H


