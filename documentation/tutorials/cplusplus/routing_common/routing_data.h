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
//  Common minimalistic base for routing data (instance) classes.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_H

#include <ostream>

#include "constraint_solver/routing.h"

#include "routing_common/routing_common.h"
#include "routing_common/routing_data_generator.h"

DECLARE_int32(width_size);

namespace operations_research {

//  Forward declaration.
class TSPLIBReader;


//  Base class with Routing Data (instances).
class RoutingData {
public:
  explicit RoutingData(int32 size = 0) : size_(size), name_("no name"), is_routing_data_created_(false), is_routing_data_instanciated_(false),
    has_coordinates_(false), has_diplay_coords_(false), raw_bbox_(BoundingBox())
     {
    if (size > 0) {
      CreateRoutingData(size);
    }
  }
  explicit RoutingData(const RoutingDataGenerator & g): size_(g.Size()), name_(g.InstanceName()), is_routing_data_created_(false), is_routing_data_instanciated_(false),
    has_coordinates_(false), has_diplay_coords_(false), raw_bbox_(BoundingBox())
     {
    if (Size() > 0) {
      CreateRoutingData(Size());
      for (RoutingModel::NodeIndex i(0); i < Size(); ++i) {
        Coordinate(i) = g.Coordinate(i);
        for (RoutingModel::NodeIndex j(0); j < Size(); ++j) {
          InternalDistance(i,j) = g.Distance(i,j);
        }
      }
      SetRoutingDataInstanciated();
      SetHasCoordinates();
    }
    
  }

  explicit RoutingData(const RoutingData & other) {
    CreateRoutingData(other.Size());
    name_ = other.Name();
    comment_ = other.Comment();
    for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
      for (RoutingModel::NodeIndex j(RoutingModel::kFirstNode); j < Size(); ++j) {
        InternalDistance(i,j) = other.Distance(i,j);
      }
    }

    if (other.HasDisplayCoordinates()) {
      for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
        Coordinate(i) = other.Coordinate(i);
      }
    }

    if (other.HasCoordinates()) {
      for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
        DisplayCoordinate(i) = other.DisplayCoordinate(i);
      }
    }
    SetHasCoordinates(other.HasCoordinates());
    SetHasDisplayCoordinates(other.HasDisplayCoordinates());
    SetRoutingDataInstanciated();
  }

 
  virtual ~RoutingData() {}

  void SetHasCoordinates(const bool coordinates = true) {
    has_coordinates_ = coordinates;
  }
  
  void SetHasDisplayCoordinates(const bool display_coordinates = true) {
    has_diplay_coords_ = display_coordinates;
  }
  
  bool HasCoordinates() const {
    return has_coordinates_;
  }
  
  bool HasDisplayCoordinates() const {
    return has_diplay_coords_;
  }

  bool IsVisualizable() const {
    return HasCoordinates() || HasDisplayCoordinates();
  }
  
  int32 Size() const {
    return size_;
  }

  std::string Name() const {
    return name_;
  }

  std::string Comment() const {
    return comment_;
  }
  
  int64 Distance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
    CheckNodeIsValid(i);
    return distances_.Cost(i,j);
  }
  
  Point Coordinate(RoutingModel::NodeIndex i) const {
    CheckNodeIsValid(i);
    return coordinates_[i.value()];
  }

  Point DisplayCoordinate(RoutingModel::NodeIndex i) const {
    CheckNodeIsValid(i);
    return display_coords_[i.value()];
  }

  BoundingBox RawBoundingBox() const {
    return raw_bbox_;
  }

  void PrintDistanceMatrix(std::ostream& out, const int32 & width = FLAGS_width_size) const;
  void WriteDistanceMatrix(const std::string & filename, const int32 & width = FLAGS_width_size) const;

protected:  
  void CreateRoutingData(int32 size) {
    size_ = size;
    distances_.Create(size);
    coordinates_.resize(size);
    display_coords_.resize(size);
    is_routing_data_created_ = true;
  }
  
  bool IsRoutingDataCreated() const {
    return is_routing_data_created_;
  }

  bool IsRoutingDataInstanciated() const {
    return is_routing_data_instanciated_;
  }
  
  void SetRoutingDataInstanciated() {
    is_routing_data_instanciated_ = true;
    distances_.SetIsInstanciated();
    //Find bounding box
    if (HasDisplayCoordinates()) {
      for (int32 i = 0; i < Size(); ++i  ) {
        raw_bbox_.Update(display_coords_[i]);
      }
    } else if (HasCoordinates()) {
      for (int32 i = 0; i < Size(); ++i  ) {
        raw_bbox_.Update(coordinates_[i]);
      } 
    }
  }

  void CheckNodeIsValid(const RoutingModel::NodeIndex i) const {
    CHECK_GE(i.value(), 0) << "Internal node " << i.value() << " should be greater than 0!";
    CHECK_LT(i.value(), Size()) << "Internal node " << i.value() << " should be less than " << Size();
  }

  int64& InternalDistance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) {
    CheckNodeIsValid(i);
    CheckNodeIsValid(j);
    return distances_.Cost(i,j);
  }

  Point& Coordinate(RoutingModel::NodeIndex i) {
    CheckNodeIsValid(i);
    return coordinates_[i.value()];
  }

  Point& DisplayCoordinate(RoutingModel::NodeIndex i) {
    CheckNodeIsValid(i);
    return display_coords_[i.value()];
  }

protected:
  int32 size_;
  std::string name_;
  std::string comment_;
private:
  bool is_routing_data_created_;
  bool is_routing_data_instanciated_;
  bool has_coordinates_;
  bool has_diplay_coords_;
protected:
  CompleteGraphArcCost distances_;
  std::vector<Point> coordinates_;
  std::vector<Point> display_coords_;
  BoundingBox raw_bbox_;
  
};

void RoutingData::PrintDistanceMatrix(std::ostream& out, const int32 & width) const {
  distances_.Print(out, width);
}

void RoutingData::WriteDistanceMatrix(const std::string& filename, const int32 & width) const {
  WriteToFileP1<RoutingData, const int> writer(this, filename);
  writer.SetMember(&operations_research::RoutingData::PrintDistanceMatrix);
  writer.Run(width);
}


}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_DATA_H