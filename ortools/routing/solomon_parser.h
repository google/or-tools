// Copyright 2010-2022 Google LLC
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

// A parser for "Solomon" instances. The Solomon file library is a set of
// Capacitated Vehicle Routing Problems with Time Windows created by
// Pr. Marius Solomon.
//
// The goal is to find routes starting and ending at a depot which visit a
// set of nodes. The objective is first to minimize the number of routes and
// then to minimize the total distance traveled, distances being measured with
// the Euclidean distance. There are two types of constraints limiting the
// route lengths:
// - time windows restricting the time during which a node can be visited
// - vehicle capacity which limits the load of the vehicles performing the
//   routes (each node has a corresponding demand which must be picked up
//   by the vehicle).
//
// The format of the data is the following:
//
// <instance_name>
// VEHICLE
// NUMBER             CAPACITY
// <number of nodes>  <vehicle capacity>
// CUSTOMER
// CUST NO.  XCOORD. YCOORD. DEMAND   READY TIME   DUE DATE   SERVICE TIME
// <node id> <x>     <y>     <demand> <ready time> <due date> <service time>
//
// The parser supports both standard instance files and zipped archives
// containing multiple instances.
//

#ifndef OR_TOOLS_ROUTING_SOLOMON_PARSER_H_
#define OR_TOOLS_ROUTING_SOLOMON_PARSER_H_

#include <math.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {

// Solomon parser class.
class SolomonParser {
 public:
  SolomonParser();

#ifndef SWIG
  SolomonParser(const SolomonParser&) = delete;
  const SolomonParser& operator=(const SolomonParser&) = delete;
#endif

  // Loading an instance. Both methods return false in case of failure to read
  // the instance. Loading a new instance clears the previously loaded instance.

  // Loads instance from a file.
  bool LoadFile(const std::string& file_name);
  // Loads instance from a file contained in a zipped archive; the archive can
  // contain multiple files.
  bool LoadFile(const std::string& file_name, const std::string& archive_name);

  // Returns the name of the instance being solved.
  const std::string& name() const { return name_; }
  // Returns the index of the depot.
  int Depot() const { return 0; }
  // Returns the number of nodes in the current routing problem.
  int NumberOfNodes() const { return coordinates_.size(); }
  // Returns the maximum number of vehicles to use.
  int NumberOfVehicles() const { return vehicles_; }
  // Returns the coordinates of the nodes in the current routing problem.
  const std::vector<Coordinates2<int64_t>>& coordinates() const {
    return coordinates_;
  }
  // Returns the capacity of the vehicles.
  int64_t capacity() const { return capacity_; }
  // Returns the demand of the nodes in the current routing problem.
  const std::vector<int64_t>& demands() const { return demands_; }
  // Returns the time windows of the nodes in the current routing problem.
  const std::vector<SimpleTimeWindow<int64_t>>& time_windows() const {
    return time_windows_;
  }
  // Returns the service times of the nodes in the current routing problem.
  const std::vector<int64_t>& service_times() const { return service_times_; }
  // Returns the distance between two nodes.
  double GetDistance(int from, int to) const {
    const Coordinates2<int64_t>& from_coords = coordinates_[from];
    const Coordinates2<int64_t>& to_coords = coordinates_[to];
    const double xd = from_coords.x - to_coords.x;
    const double yd = from_coords.y - to_coords.y;
    return sqrt(xd * xd + yd * yd);
  }
  // Returns the travel time between two nodes.
  double GetTravelTime(int from, int to) const {
    return service_times_[from] + GetDistance(from, to);
  }

 private:
  enum Section { UNKNOWN, NAME, VEHICLE, CUSTOMER };

  // Parsing
  void Initialize();
  bool ParseFile(const std::string& file_name);

  // Parsing data
  const std::map<std::string, Section> sections_;
  Section section_;
  int to_read_;
  // Input data
  // Using int64_t to represent different dimension values of the problem:
  // - demand and vehicle capacity,
  // - distances and node coordinates,
  // - time, including time window values and service times.
  std::string name_;
  int vehicles_;
  std::vector<Coordinates2<int64_t>> coordinates_;
  int64_t capacity_;
  std::vector<int64_t> demands_;
  std::vector<SimpleTimeWindow<int64_t>> time_windows_;
  std::vector<int64_t> service_times_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_SOLOMON_PARSER_H_
