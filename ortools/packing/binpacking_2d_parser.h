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

#ifndef OR_TOOLS_PACKING_BINPACKING_2D_PARSER_H_
#define OR_TOOLS_PACKING_BINPACKING_2D_PARSER_H_

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/packing/multiple_dimensions_bin_packing.pb.h"

namespace operations_research {
namespace packing {

// A BinPacking parser.
// It supports the following file format:
//  - 2bp:
//    see http://or.dei.unibo.it/library/two-dimensional-bin-packing-problem
//  - Binpacking with conflicts:
//    see http://or.dei.unibo.it/library/bin-packing-problem-conflicts
//
// The generated problems have the following characteristics:
//
// You have one box with n dimensions. The size of the box is stored in the
// field box_shape().
// You need to fit items into this box. Each item has the same number of
// dimensions and one or more possible shapes (this usually means that
// you can rotate the item). Each item has a value, and a possible list of
// conflicts (items you cannot put alongside this item).
// The objective of the problem is to fit as many items as possible in the box
// while maximizing the sum of values of selected items. For each item, you need
// to select the shape and the position of the item in the box.
// Each item must not overlap (in n dimensions) with any other item.
class BinPacking2dParser {
 public:
  BinPacking2dParser();

  // Loads the 'instance'th instance of the bin packing problem if the given
  // file. The instance are 1 based (first is 1).
  // Only one call to a Load*() function is supported. All the subsequent
  // calls will do nothing and return false.
  bool Load2BPFile(const std::string& file_name, int instance);
  MultipleDimensionsBinPackingProblem problem() const { return problem_; }

 private:
  enum LoadStatus { NOT_STARTED = 0, INSTANCE_FOUND = 1, PARSING_FINISHED = 2 };

  void ProcessNew2BpLine(const std::string& line, int instance);

  MultipleDimensionsBinPackingProblem problem_;
  int num_dimensions_;

  // Temporary.
  LoadStatus load_status_;
  int num_items_;
  int instances_seen_;
};

}  // namespace packing
}  // namespace operations_research

#endif  // OR_TOOLS_PACKING_BINPACKING_2D_PARSER_H_
