// Copyright 2010-2021 Google LLC
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

#include "ortools/packing/vector_bin_packing_parser.h"

#include <cstdint>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/filelineiter.h"
#include "ortools/packing/vector_bin_packing.pb.h"

namespace operations_research {
namespace packing {
namespace vbp {

bool VbpParser::ParseFile(const std::string& data_filename) {
  vbp_.Clear();

  load_status_ = DIMENSION_SECTION;
  for (const std::string& line : FileLines(data_filename)) {
    if (load_status_ == ERROR_FOUND) break;
    ProcessLine(line);
  }

  // Checks status.
  if (load_status_ == ERROR_FOUND) {
    LOG(INFO) << vbp_.DebugString();
    return false;
  }
  return vbp_.item_size() == num_declared_items_;
}

void VbpParser::ReportError(const std::string& line) {
  LOG(ERROR) << "Error: status = " << load_status_ << ", line = " << line;
  load_status_ = ERROR_FOUND;
}

void VbpParser::ProcessLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t\r"), absl::SkipEmpty());

  if (words.empty()) return;

  switch (load_status_) {
    case NOT_STARTED: {
      LOG(FATAL) << "Should not be here";
    }
    case DIMENSION_SECTION: {
      if (words.size() != 1) {
        ReportError(line);
        return;
      }
      num_resources_ = strtoint32(words[0]);
      load_status_ = BIN_SECTION;
      break;
    }
    case BIN_SECTION: {
      if (words.size() != num_resources_) {
        ReportError(line);
        return;
      }
      for (const std::string& dim_str : words) {
        vbp_.add_resource_capacity(strtoint64(dim_str));
      }
      load_status_ = NUMBER_OF_ITEMS_SECTION;
      break;
    }
    case NUMBER_OF_ITEMS_SECTION: {
      if (words.size() != 1) {
        ReportError(line);
        return;
      }
      num_declared_items_ = strtoint32(words[0]);
      load_status_ = ITEM_SECTION;
      break;
    }
    case ITEM_SECTION: {
      if (words.size() != num_resources_ + 1) {
        ReportError(line);
        return;
      }
      Item* const item = vbp_.add_item();
      for (int i = 0; i < num_resources_; ++i) {
        item->add_resource_usage(strtoint64(words[i]));
      }
      item->set_num_copies(strtoint32(words[num_resources_]));
      item->set_max_number_of_copies_per_bin(item->num_copies());
      break;
    }
    case ERROR_FOUND: {
      break;
    }
  }
}

int VbpParser::strtoint32(const std::string& word) {
  int result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

int64_t VbpParser::strtoint64(const std::string& word) {
  int64_t result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

}  // namespace vbp
}  // namespace packing
}  // namespace operations_research
