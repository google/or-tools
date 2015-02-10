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
//  Common routing distance stuff.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DISTANCE_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DISTANCE_H

#include <string>

#include "constraint_solver/routing.h"

#include "routing_random.h"
#include "tsplib.h"

DECLARE_int32(width_size);

namespace operations_research {

  class CompleteGraphDistances {
  public:
    CompleteGraphDistances(int32 size): size_(size), costs_(size_) {}
    virtual int64 Distance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const = 0;
    int32 Size() const {
      return size_;
    }
    void Print(std::ostream & out, const int width = FLAGS_width_size);
    void ReplaceDistance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j, int64 dist) {
      costs_.Cost(i,j) = dist;
    }
  private:
    int32 size_;
  protected:
    CompleteGraphArcCost costs_;
  };

  void CompleteGraphDistances::Print(std::ostream & out, const int width) {
    costs_.Print(out, width);
  }

  //  Basic distance class on "complete" graphs from coordinates.
  class DistancesFromTWODCoordinates : public CompleteGraphDistances {
  public:
    explicit DistancesFromTWODCoordinates(const GenerateTWODCoordinates& coords): CompleteGraphDistances(coords.Size()), coords_(coords) {
      for (RoutingModel::NodeIndex i(0); i < Size(); ++i) {
        costs_.Cost(i,i) = 0;
        for (RoutingModel::NodeIndex j(i.value()+1); j < Size(); ++j) {
          int64 dist = TSPLIBDistanceFunctions::TWOD_euc_2d_distance(coords_.Coordinate(i), coords_.Coordinate(j));
          costs_.Cost(i,j) = dist;
          costs_.Cost(j,i) = dist;
        }
      }
    }
    virtual int64 Distance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
      return costs_.Cost(i,j);
    }
    void ReplaceDistance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j, int64 dist) {
      costs_.Cost(i,j) = dist;
    }
  private:
    const GenerateTWODCoordinates& coords_;
  };
}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_COMMON_H