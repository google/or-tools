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
//  Common base for (c)vrp data (instance) classes.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_VRP_DATA_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_VRP_DATA_H

#include <ostream>
#include <iomanip>

#include "base/bitmap.h"
#include "base/logging.h"
#include "base/file.h"
#include "base/split.h"
#include "base/filelinereader.h"
#include "base/join.h"
#include "base/strtoint.h"

#include "constraint_solver/routing.h"

#include "routing_common/routing_common.h"
#include "routing_common/routing_data.h"
#include "routing_common/tsplib_reader.h"
#include "routing_common/tsplib.h"
#include "cvrp_data_generator.h"

DECLARE_int32(width_size);

namespace operations_research {

class CVRPData : public RoutingData {
public:
  explicit CVRPData(CVRPDataGenerator & g) : RoutingData(g), depot_(g.Depot()), capacity_(g.Capacity()),
    demands_(Size()),
    node_coord_type_(g.NodeCoordinateType()),
    display_data_type_(g.DisplayDataType()),
    two_dimension_(g.HasDimensionTwo()),
    total_demand_(0){

    for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
      demands_[i.value()] = g.Demand(i);
      total_demand_ += g.Demand(i);
    }
  }
  explicit CVRPData(const TSPLIBReader & reader) :
    RoutingData(reader),
    depot_(reader.Depot()),
    capacity_(reader.Capacity()),
    demands_(Size()),
    node_coord_type_(reader.NodeCoordinateType()),
    display_data_type_(reader.DisplayDataType()),
    two_dimension_(reader.HasDimensionTwo()),
    total_demand_(0) {
      CHECK(reader.TSPLIBType() == CVRP);
      for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
        demands_[i.value()] = reader.Demand(i);
        total_demand_ += reader.Demand(i);
      }
      if (node_coord_type_ == TWOD_COORDS || node_coord_type_ == THREED_COORDS) {
        for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
          Coordinate(i) = reader.Coordinate(i);
        }
        SetHasCoordinates();
      }
      if (display_data_type_ == TWOD_DISPLAY) {
        for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
          DisplayCoordinate(i) = reader.DisplayCoordinate(i);
        }
        SetHasDisplayCoordinates();
      }
      SetRoutingDataInstanciated();
  }

  void SetDepot(RoutingModel::NodeIndex d) {
    CheckNodeIsValid(d);
    depot_ = d;
  }

  RoutingModel::NodeIndex Depot() const {
    return depot_;
  }

  void SetDemand(const RoutingModel::NodeIndex i, int64 demand) {
    CheckNodeIsValid(i);
    demands_[i.value()] = demand;
  }

  int64 Demand(const RoutingModel::NodeIndex i) const {
    CheckNodeIsValid(i);
    return demands_[i.value()];
  }

int64 TotalDemand() const {
  return total_demand_;
}

  void PrintTSPLIBInstance(std::ostream & out) const;

  void WriteTSPLIBInstance(const std::string & filename) const {
    WriteToFile<CVRPData> writer(this, filename);
    writer.SetMember(&operations_research::CVRPData::PrintTSPLIBInstance);
    writer.Run();
  }

void SetCapacity(int64 capacity) {
  capacity_ = capacity;
}

int64 Capacity() const {
  return capacity_;
}

  //  Helper function
  int64& SetDistance(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) {
    return distances_.Cost(i, j);
  }

private:
  
  RoutingModel::NodeIndex depot_;
  std::vector<int64> demands_;
  int64 total_demand_;
  TSPLIB_NODE_COORD_TYPE_TYPES_enum node_coord_type_;
  TSPLIB_DISPLAY_DATA_TYPE_TYPES_enum display_data_type_;
  bool two_dimension_;
  int64 capacity_;

};

void CVRPData::PrintTSPLIBInstance(std::ostream& out) const {
  out << TSPLIB_STATES_KEYWORDS[NAME] << " : " << Name() << std::endl;
  out << TSPLIB_STATES_KEYWORDS[COMMENT] << " : " << Comment() << std::endl;
  out << TSPLIB_STATES_KEYWORDS[TYPE] << " : CVRP" << std::endl;
  out << TSPLIB_STATES_KEYWORDS[DIMENSION] << " : " << Size() << std::endl;
  out << TSPLIB_STATES_KEYWORDS[EDGE_WEIGHT_TYPE] << " : " << "EXPLICIT" << std::endl;
  out << TSPLIB_STATES_KEYWORDS[EDGE_WEIGHT_FORMAT] << " : " << "FULL_MATRIX" << std::endl;
  if (HasCoordinates()) {
    out << TSPLIB_STATES_KEYWORDS[NODE_COORD_TYPE] << " : " << TSPLIB_NODE_COORD_TYPE_TYPES_KEYWORDS[node_coord_type_]  << std::endl;
  }
  if (HasDisplayCoordinates()) {
    out << TSPLIB_STATES_KEYWORDS[DISPLAY_DATA_TYPE] << " : " << TSPLIB_DISPLAY_DATA_TYPE_TYPES_KEYWORDS[display_data_type_] << std::endl;
  }
  if (Depot() != RoutingModel::kFirstNode) {
    out << TSPLIB_STATES_KEYWORDS[DEPOT_SECTION] << std::endl;
    out << Depot().value() + 1 << std::endl;
    out << kTSPLIBDelimiter << std::endl;
  }
  out << TSPLIB_STATES_KEYWORDS[EDGE_WEIGHT_SECTION] << std::endl;
  distances_.Print(out);
  if (HasCoordinates()) {
    out << TSPLIB_STATES_KEYWORDS[NODE_COORD_SECTION] << std::endl;
    for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
      out.width(FLAGS_width_size);
      out << std::right << i.value() + 1;
      out.width(FLAGS_width_size);
      out << std::right << Coordinate(i).x;
      out.width(FLAGS_width_size);
      out << std::right << Coordinate(i).y;
      if (!two_dimension_) {  // 3D
        out.width(FLAGS_width_size);
        out << std::right << Coordinate(i).z;
      }
      out  << std::endl;
    }
  }
  if (HasDisplayCoordinates()) {
    out << TSPLIB_STATES_KEYWORDS[DISPLAY_DATA_SECTION] << std::endl;
    for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
      out.width(FLAGS_width_size);
      out << std::right << i.value() + 1;
      out.width(FLAGS_width_size + 4);
      out << std::setprecision(2) << std::fixed << std::right << DisplayCoordinate(i).x;
      out.width(FLAGS_width_size + 4);
      out << std::right << DisplayCoordinate(i).y << std::endl;
    }
  }
  out << TSPLIB_STATES_KEYWORDS[DEMAND_SECTION] << std::endl;
  for (RoutingModel::NodeIndex i(RoutingModel::kFirstNode); i < Size(); ++i) {
    out.width(FLAGS_width_size);
    out << std::right << i.value() + 1;
    out.width(FLAGS_width_size);
    out << std::right << Demand(i) << std::endl;
  }
  out << kTSPLIBEndFileDelimiter << std::endl;
}

}  //  namespace operations_research


#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_VRP_DATA_H