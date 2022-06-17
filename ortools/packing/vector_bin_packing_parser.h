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

// Parses vector packing data files, and creates a VectorBinPackingProblem
//
// The supported file formats are:
//   - vector packing solver: (.vbp files)
//         http://www.dcc.fc.up.pt/~fdabrandao/Vector_Packing_Solver

#ifndef OR_TOOLS_PACKING_VECTOR_BIN_PACKING_PARSER_H_
#define OR_TOOLS_PACKING_VECTOR_BIN_PACKING_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/packing/vector_bin_packing.pb.h"

namespace operations_research {
namespace packing {
namespace vbp {

class VbpParser {
 public:
  // Return true iff there were no error, in which case problem() can be
  // called to retrieve the parsed problem.
  bool ParseFile(const std::string& data_filename);

  // We keep the fully qualified name for SWIG.
  ::operations_research::packing::vbp::VectorBinPackingProblem problem() const {
    return vbp_;
  }

 private:
  enum LoadStatus {
    NOT_STARTED,
    DIMENSION_SECTION,
    BIN_SECTION,
    NUMBER_OF_ITEMS_SECTION,
    ITEM_SECTION,
    ERROR_FOUND
  };

  void ProcessLine(const std::string& line);
  void ReportError(const std::string& line);
  int strtoint32(const std::string& word);
  int64_t strtoint64(const std::string& word);

  LoadStatus load_status_ = NOT_STARTED;
  int num_declared_items_ = -1;
  int num_resources_ = -1;

  VectorBinPackingProblem vbp_;
};

}  // namespace vbp
}  // namespace packing
}  // namespace operations_research

#endif  // OR_TOOLS_PACKING_VECTOR_BIN_PACKING_PARSER_H_
