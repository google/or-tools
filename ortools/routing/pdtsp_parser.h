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

// A TSPPD parser used to parse instances of Traveling Salesman Problems with
// pickup and delivery constraints. This format was created by Stefan Ropke.
// https://link.springer.com/article/10.1007%2Fs10107-008-0234-9

#ifndef OR_TOOLS_ROUTING_PDTSP_PARSER_H_
#define OR_TOOLS_ROUTING_PDTSP_PARSER_H_

#include <functional>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"

namespace operations_research {

class PdTspParser {
 public:
  PdTspParser();
  ~PdTspParser() = default;
  // Loads and parse a PDTSP from a given file.
  bool LoadFile(const std::string& file_name);
  // Returns the index of the depot.
  int depot() const { return depot_; }
  // Returns the number of nodes in the PDTSP.
  int Size() const { return x_.size(); }
  // Returns true if the index corresponding to a node is a pickup.
  bool IsPickup(int index) const { return deliveries_[index] >= 0; }
  // Returns the delivery corresponding to a pickup.
  int DeliveryFromPickup(int index) const { return deliveries_[index]; }
  // Returns a function returning distances between nodes.
  std::function<int64_t(int, int)> Distances() const;

 private:
  enum Sections { SIZE_SECTION, DEPOT_SECTION, NODE_SECTION, EOF_SECTION };
  void ProcessNewLine(const std::string& line);

  int depot_;
  Sections section_;
  std::vector<double> x_;
  std::vector<double> y_;
  std::vector<int> deliveries_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_PDTSP_PARSER_H_
