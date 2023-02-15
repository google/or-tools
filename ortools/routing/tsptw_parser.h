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

// A TSPTW parser.
//
// Takes as input a data file, potentially gzipped. The data must
// follow the format described at
// http://lopez-ibanez.eu/tsptw-instances and
// https://homepages.dcc.ufmg.br/~rfsilva/tsptw.

#ifndef OR_TOOLS_ROUTING_TSPTW_PARSER_H_
#define OR_TOOLS_ROUTING_TSPTW_PARSER_H_

#include <functional>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {

class TspTWParser final {
 public:
  TspTWParser();
  // Loads and parses a routing problem from a given file.
  bool LoadFile(const std::string& file_name);
  // Returns a function returning the distance between nodes. On some instances
  // service times are already included in values returned by this function.
  // The actual distance of a route can be obtained by removing
  // total_service_time() from the sum of distances in that case.
  const std::function<double(int, int)>& distance_function() const {
    return distance_function_;
  }
  // Returns a function returning the time between nodes (equivalent to
  // distance_function(i, j) + service_time(j)).
  const std::function<double(int, int)>& time_function() const {
    return time_function_;
  }
  // Returns the index of the depot.
  int depot() const { return depot_; }
  // Returns the number of nodes in the current routing problem.
  int size() const { return size_; }
  // Returns the total service time already included in distance_function.
  double total_service_time() const { return total_service_time_; }
  // Returns the coordinates of the nodes in the current routing problem.
  const std::vector<Coordinates2<double>>& coordinates() const {
    return coords_;
  }
  // Returns the time windows of the nodes in the current routing problem.
  const std::vector<SimpleTimeWindow<double>>& time_windows() const {
    return time_windows_;
  }
  // Returns the service times of the nodes in the current routing problem.
  const std::vector<double>& service_times() const { return service_times_; }

 private:
#ifndef SWIG
  TspTWParser(const TspTWParser&) = delete;
  void operator=(const TspTWParser&) = delete;
#endif
  bool ParseLopezIbanezBlum(const std::string& file_name);
  bool ParseDaSilvaUrrutia(const std::string& file_name);

  int64_t size_;
  int depot_;
  double total_service_time_;
  std::function<double(int, int)> distance_function_;
  std::function<double(int, int)> time_function_;
  std::vector<Coordinates2<double>> coords_;
  std::vector<SimpleTimeWindow<double>> time_windows_;
  std::vector<double> service_times_;
  std::vector<double> distance_matrix_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_TSPTW_PARSER_H_
