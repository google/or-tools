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
//  Common random routing stuff.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_RANDOM_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_RANDOM_H

#include <string>
#include <vector>

#include "base/random.h"
#include "constraint_solver/routing.h"

#include "common/random.h"
#include "routing_common/routing_common.h"

namespace operations_research {


class GenerateTWODCoordinates {
public:
  GenerateTWODCoordinates(int32 size): size_(size), randomizer_(GetSeed()), coordinates_(size_) {
    Generate();
  }
  const Point Coordinate(RoutingModel::NodeIndex i) const {
    return coordinates_[i.value()];
  }
  const int32 Size() const {
    return size_;
  }
private:
  void Generate() {
    bool coord_found = false;
    int32 x = 0;
    int32 y = 0;
    for (int32 i = 0; i < size_; i++) {
      coord_found = false;
      while (!coord_found) {
        x = randomizer_.Uniform(FLAGS_x_max);
        y = randomizer_.Uniform(FLAGS_y_max);
        //  test if coordinates are not already taken
        //  maybe we should ask for a minimum distance between points?
        coord_found = true;
        for (int32 j = 0; j < i; ++j) {
          if (x == coordinates_[j].x && y == coordinates_[j].y) {
            coord_found = false;
            break;
          }
        }
      }
      coordinates_[i] = Point(x, y);
    }
  }
  int32 size_;
  ACMRandom randomizer_;
  std::vector<Point> coordinates_;
  
};
}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_RANDOM_H