// Copyright 2010-2018 Google LLC
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

#include "ortools/data/set_covering_parser.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/filelineiter.h"

namespace operations_research {
namespace scp {

ScpParser::ScpParser() : section_(INIT), line_(0), remaining_(0), current_(0) {}

bool ScpParser::LoadProblem(const std::string &filename, Format format,
                            ScpData *data) {
  section_ = INIT;
  line_ = 0;
  remaining_ = 0;
  current_ = 0;

  for (const std::string &line : FileLines(filename)) {
    ProcessLine(line, format, data);
    if (section_ == ERROR) return false;
  }
  return section_ == END;
}

void ScpParser::ProcessLine(const std::string &line, Format format,
                            ScpData *data) {
  line_++;
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t\r"), absl::SkipEmpty());
  switch (section_) {
    case INIT: {
      if (words.size() != 2) {
        LogError(line, "Problem reading the size of the problem");
        return;
      }
      const int num_rows = strtoint32(words[0]);
      const int num_columns = strtoint32(words[1]);
      data->SetProblemSize(num_rows, num_columns);
      current_ = 0;
      switch (format) {
        case SCP_FORMAT: {
          section_ = COSTS;
          break;
        }
        case RAILROAD_FORMAT: {
          section_ = COLUMN;
          break;
        }
        case TRIPLET_FORMAT: {
          section_ = COLUMN;
          break;
        }
        case SPP_FORMAT: {
          section_ = COLUMN;
          data->set_is_set_partitioning(true);
          break;
        }
      }
      break;
    }
    case COSTS: {
      const int num_items = words.size();
      if (current_ + num_items > data->num_columns()) {
        LogError(line, "Too many cost items");
        return;
      }
      for (int i = 0; i < num_items; ++i) {
        data->SetColumnCost(current_++, strtoint32(words[i]));
      }
      if (current_ == data->num_columns()) {
        section_ = NUM_COLUMNS_IN_ROW;
        current_ = 0;
      }
      break;
    }
    case COLUMN: {
      switch (format) {
        case SCP_FORMAT: {
          LogError(line, "Wrong state in the loader");
          return;
        }
        case RAILROAD_FORMAT:
          ABSL_FALLTHROUGH_INTENDED;
        case SPP_FORMAT: {
          if (words.size() < 2) {
            LogError(line, "Column declaration too short");
            return;
          }
          const int cost = strtoint32(words[0]);
          data->SetColumnCost(current_, cost);
          const int num_items = strtoint32(words[1]);
          if (words.size() != 2 + num_items) {
            LogError(line, "Mistatch in column declaration");
            return;
          }
          for (int i = 0; i < num_items; ++i) {
            const int row = strtoint32(words[i + 2]) - 1;  // 1 based.
            data->AddRowInColumn(row, current_);
          }
          current_++;
          if (current_ == data->num_columns()) {
            section_ = format == RAILROAD_FORMAT ? END : NUM_NON_ZEROS;
          }
          break;
        }
        case TRIPLET_FORMAT: {
          if (words.size() != 3) {
            LogError(line, "Column declaration does not contain 3 rows");
            break;
          }
          data->SetColumnCost(current_, 1);
          for (int i = 0; i < 3; ++i) {
            const int row = strtoint32(words[i]) - 1;  // 1 based.
            data->AddRowInColumn(row, current_);
          }
          current_++;
          if (current_ == data->num_columns()) {
            section_ = END;
          }
          break;
        }
      }
      break;
    }
    case NUM_COLUMNS_IN_ROW: {
      if (words.size() != 1) {
        LogError(line, "The header of a column should be one number");
        return;
      }
      remaining_ = strtoint32(words[0]);
      section_ = ROW;
      break;
    }
    case ROW: {
      const int num_items = words.size();
      if (num_items > remaining_) {
        LogError(line, "Too many columns in a row declaration");
        return;
      }
      for (const std::string &w : words) {
        remaining_--;
        const int column = strtoint32(w) - 1;  // 1 based.
        data->AddRowInColumn(current_, column);
      }
      if (remaining_ == 0) {
        current_++;
        if (current_ == data->num_rows()) {
          section_ = END;
        } else {
          section_ = NUM_COLUMNS_IN_ROW;
        }
      }
      break;
    }
    case NUM_NON_ZEROS: {
      if (words.size() != 1) {
        LogError(line, "The header of a column should be one number");
        return;
      }
      section_ = END;
      break;
    }
    case END: {
      break;
    }
    case ERROR: {
      break;
    }
  }
}

void ScpParser::LogError(const std::string &line, const std::string &message) {
  LOG(ERROR) << "Error on line " << line_ << ": " << message << "(" << line
             << ")";
  section_ = ERROR;
}

int ScpParser::strtoint32(const std::string &word) {
  int result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

int64 ScpParser::strtoint64(const std::string &word) {
  int64 result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

}  // namespace scp
}  // namespace operations_research
