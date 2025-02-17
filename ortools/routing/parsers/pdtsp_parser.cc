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

#include "ortools/routing/parsers/pdtsp_parser.h"

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/gzipfile.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/numbers.h"
#include "ortools/base/path.h"
#include "ortools/base/strtoint.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {
namespace {

using absl::ByAnyChar;

File* OpenReadOnly(absl::string_view file_name) {
  File* file = nullptr;
  if (file::Open(file_name, "r", &file, file::Defaults()).ok() &&
      file::Extension(file_name) == "gz") {
    file = GZipFileReader(file_name, file, TAKE_OWNERSHIP);
  }
  return file;
}
}  // namespace

PdTspParser::PdTspParser() : section_(SIZE_SECTION) {}

bool PdTspParser::LoadFile(absl::string_view file_name) {
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    ProcessNewLine(line);
  }
  return true;
}

std::function<int64_t(int, int)> PdTspParser::Distances() const {
  std::function<int64_t(int, int)> distances = [this](int from, int to) {
    const double xd = x_[from] - x_[to];
    const double yd = y_[from] - y_[to];
    const double d = sqrt(xd * xd + yd * yd);
    return MathUtil::FastInt64Round(d);
  };
  return distances;
}

void PdTspParser::ProcessNewLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ByAnyChar(" :\t"), absl::SkipEmpty());
  if (!words.empty()) {
    switch (section_) {
      case SIZE_SECTION: {
        const int size = atoi64(words[0]);
        x_.resize(size, 0);
        y_.resize(size, 0);
        deliveries_.resize(size, -1);
        section_ = DEPOT_SECTION;
        break;
      }
      case DEPOT_SECTION:
        depot_ = atoi64(words[0]) - 1;
        x_[depot_] = atoi64(words[1]);
        y_[depot_] = atoi64(words[2]);
        deliveries_[depot_] = -1;
        section_ = NODE_SECTION;
        break;
      case NODE_SECTION: {
        const int kEof = -999;
        const int id = atoi64(words[0]) - 1;
        if (id + 1 == kEof) {
          section_ = EOF_SECTION;
        } else {
          x_[id] = atoi64(words[1]);
          y_[id] = atoi64(words[2]);
          const bool is_pickup = atoi64(words[3]) == 0;
          if (is_pickup) {
            deliveries_[id] = atoi64(words[4]) - 1;
          }
        }
        break;
      }
      case EOF_SECTION:
        break;
      default:
        break;
    }
  }
}

}  // namespace operations_research::routing
