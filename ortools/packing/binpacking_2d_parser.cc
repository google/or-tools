// Copyright 2010-2025 Google LLC
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

#include "ortools/packing/binpacking_2d_parser.h"

#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
namespace packing {

BinPacking2dParser::BinPacking2dParser()
    : num_dimensions_(-1),
      load_status_(NOT_STARTED),
      num_items_(0),
      instances_seen_(0) {}

bool BinPacking2dParser::Load2BPFile(absl::string_view file_name,
                                     int instance) {
  if (load_status_ != NOT_STARTED) {
    return false;
  }

  num_dimensions_ = 2;

  for (const std::string& line : FileLines(file_name)) {
    ProcessNew2BpLine(line, instance);
    if (load_status_ == PARSING_FINISHED) {
      break;
    }
  }
  return num_items_ == problem_.items_size() && num_items_ > 0;
}

void BinPacking2dParser::ProcessNew2BpLine(const std::string& line,
                                           int instance) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t\r"), absl::SkipEmpty());
  if (words.size() == 3 && words[1] == "PROBLEM" && words[2] == "CLASS") {
    // New instance starting.
    instances_seen_++;
    if (load_status_ == NOT_STARTED && instances_seen_ == instance) {
      load_status_ = INSTANCE_FOUND;
    } else if (instances_seen_ > instance) {
      load_status_ = PARSING_FINISHED;
    }
  }

  if (load_status_ == INSTANCE_FOUND) {
    if (words.empty()) {
      return;
    } else if (words.size() == 2 || words[2] == "H(I),W(I),I=1,...,N") {
      // Reading an item.
      CHECK_NE(num_items_, 0);
      CHECK_LT(problem_.items_size(), num_items_);
      MultipleDimensionsBinPackingItem* item = problem_.add_items();
      MultipleDimensionsBinPackingShape* shape = item->add_shapes();
      int64_t dim;
      CHECK(absl::SimpleAtoi(words[0], &dim));
      shape->add_dimensions(dim);
      CHECK(absl::SimpleAtoi(words[1], &dim));
      shape->add_dimensions(dim);
      item->set_value(1);
    } else if (words[1] == "N.") {  // Reading the number of item.
      CHECK(absl::SimpleAtoi(words[0], &num_items_));
    } else if (words[2] == "RELATIVE") {
      // Just double checking.
      int local_instance;
      CHECK(absl::SimpleAtoi(words[0], &local_instance));
      CHECK_EQ(local_instance, (instance - 1) % 10 + 1);
    } else if (words[2] == "HBIN,WBIN") {
      MultipleDimensionsBinPackingShape* box_shape =
          problem_.mutable_box_shape();
      int64_t dim;
      CHECK(absl::SimpleAtoi(words[0], &dim));
      box_shape->add_dimensions(dim);
      CHECK(absl::SimpleAtoi(words[1], &dim));
      box_shape->add_dimensions(dim);
    }
  }
}

}  // namespace packing
}  // namespace operations_research
