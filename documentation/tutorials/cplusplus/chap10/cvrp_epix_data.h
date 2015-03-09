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
//  Common base to use the epix library to visualize
//  CVRP data (instance) and solutions.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_EPIX_DATA_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_EPIX_DATA_H

#include <string>

#include "routing_common/routing_common.h"
#include "cvrp_data.h"
#include "cvrp_solution.h"
#include "routing_common/routing_epix_helper.h"

namespace operations_research {

class CVRPEpixData {
public:
  explicit CVRPEpixData(const CVRPData & data): data_(data) {}
  void PrintInstance(std::ostream & out) const;
  void WriteInstance(const std::string & filename) const;
  void PrintSolution(std::ostream & out, const CVRPSolution & sol) const;
  void WriteSolution(const std::string & filename, const CVRPSolution & sol) const;
private:
  const CVRPData & data_;
};

void CVRPEpixData::PrintInstance(std::ostream& out) const {
  CHECK(data_.HasCoordinates());
  PrintEpixBeginFile(out);
  PrintEpixPreamble(out);
  PrintEpixBoundingBox(out, data_.RawBoundingBox());

  PrintEpixNewLine(out);
  PrintEpixComment(out, "Points:");

  for (RoutingModel::NodeIndex i(0); i < data_.Size(); ++i) {
    Point p = data_.Coordinate(i);
    PrintEpixPoint(out, p, i);
  }

  PrintEpixBeginFigure(out);
  PrintEpixDrawMultiplePoints(out, data_.Size());
  PrintEpixDepot(out, data_.Depot());

  PrintEpixEndFigure(out);

  PrintEpixEndFile(out);
}

void CVRPEpixData::WriteInstance(const std::string& filename) const {
  WriteToFile<CVRPEpixData> writer(this, filename);
  writer.SetMember(&operations_research::CVRPEpixData::PrintInstance);
  writer.Run();
}

void CVRPEpixData::PrintSolution(std::ostream& out, const operations_research::CVRPSolution& sol) const {
  CHECK(data_.HasCoordinates());
  PrintEpixBeginFile(out);
  PrintEpixPreamble(out);
  PrintEpixBoundingBox(out, data_.RawBoundingBox());

  PrintEpixNewLine(out);
  PrintEpixComment(out, "Points:");

  for (RoutingModel::NodeIndex i(0); i < data_.Size(); ++i) {
    Point p = data_.Coordinate(i);
    PrintEpixPoint(out, p, i);
  }


  PrintEpixComment(out, "Edges:");

  RoutingModel::NodeIndex from_node, to_node;
  RoutingModel::NodeIndex depot = data_.Depot();
  int segment_nbr = 0;

    for (CVRPSolution::const_vehicle_iterator v_iter = sol.vehicle_begin(); v_iter != sol.vehicle_end(); ++v_iter) {
      from_node = depot;
      for (CVRPSolution::const_node_iterator n_iter = sol.node_begin(v_iter); n_iter != sol.node_end(v_iter); ++n_iter ) {
        to_node = *n_iter;
        PrintEpixSegment(out, segment_nbr, from_node, to_node);
        from_node = to_node;
        ++segment_nbr;
      }
      //  Last arc
      PrintEpixSegment(out, segment_nbr, from_node, depot);
      ++segment_nbr;
    }


  PrintEpixNewLine(out);

  PrintEpixBeginFigure(out);


PrintEpixDrawMultipleSegments(out, segment_nbr);


  PrintEpixRaw(out, "  fill(White());");
  PrintEpixDrawMultiplePoints(out, data_.Size());
  PrintEpixDepot(out, data_.Depot());
  PrintEpixEndFigure(out);

  PrintEpixEndFile(out);
}

void CVRPEpixData::WriteSolution(const std::string& filename, const operations_research::CVRPSolution& sol) const {

  WriteToFileP1<CVRPEpixData, CVRPSolution> writer(this, filename);
  writer.SetMember(&operations_research::CVRPEpixData::PrintSolution);
  writer.Run(sol);

}


}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_CVRP_EPIX_DATA_H