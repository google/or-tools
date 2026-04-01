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

#include "ortools/routing/parsers/solomon_parser.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/map_util.h"
#include "ortools/base/numbers.h"
#include "ortools/base/path.h"
#include "ortools/base/zipfile.h"
#include "ortools/util/filelineiter.h"
#include "re2/re2.h"

namespace operations_research::routing {

SolomonParser::SolomonParser()
    : sections_({{"VEHICLE", VEHICLE}, {"CUSTOMER", CUSTOMER}}) {
  Initialize();
}

bool SolomonParser::LoadFile(absl::string_view file_name) {
  Initialize();
  return ParseFile(file_name);
}

bool SolomonParser::LoadFile(absl::string_view file_name,
                             const std::string& archive_name) {
  Initialize();
  if (!absl::StartsWith(archive_name, "/")) {
    return false;
  }
  const std::string fake_zip_path = "/zip" + archive_name;
  std::shared_ptr<zipfile::ZipArchive> fake_zip_closer(
      zipfile::OpenZipArchive(archive_name));
  if (nullptr == fake_zip_closer) return false;
  const std::string zip_filename = file::JoinPath(fake_zip_path, file_name);
  return ParseFile(zip_filename);
}

void SolomonParser::Initialize() {
  name_.clear();
  vehicles_ = 0;
  coordinates_.clear();
  capacity_ = 0;
  demands_.clear();
  time_windows_.clear();
  service_times_.clear();
  section_ = NAME;
  to_read_ = 1;
}

bool SolomonParser::ParseFile(absl::string_view file_name) {
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
    // Skip blank lines
    if (words.empty()) continue;
    if (to_read_ > 0) {
      switch (section_) {
        case NAME: {
          name_ = words[0];
          break;
        }
        case VEHICLE: {
          if (to_read_ == 1) {
            if (words.size() != 2) return false;
            vehicles_ = strings::ParseLeadingInt32Value(words[0], -1);
            if (vehicles_ < 0) return false;
            capacity_ = strings::ParseLeadingInt32Value(words[1], -1);
            if (capacity_ < 0) return false;
          }
          break;
        }
        case CUSTOMER: {
          if (to_read_ < 2) {
            std::vector<int64_t> values;
            for (int i = 1; i < words.size(); ++i) {
              const int64_t value =
                  strings::ParseLeadingInt64Value(words[i], -1);
              if (value < 0) return false;
              values.push_back(value);
            }
            coordinates_.push_back({values[0], values[1]});
            demands_.push_back(values[2]);
            time_windows_.push_back({values[3], values[4]});
            service_times_.push_back(values[5]);
            ++to_read_;
          }
          break;
        }
        default: {
          LOG(ERROR) << "Reading data outside section";
          return false;
        }
      }
      --to_read_;
    } else {  // New section
      section_ = gtl::FindWithDefault(sections_, words[0], UNKNOWN);
      switch (section_) {
        case VEHICLE: {
          // Two rows: header and data.
          to_read_ = 2;
          break;
        }
        case CUSTOMER: {
          to_read_ = 2;
          break;
        }
        default: {
          LOG(ERROR) << "Unknown section: " << section_;
          return false;
        }
      }
    }
  }
  return section_ == CUSTOMER;
}

SolomonSolutionParser::SolomonSolutionParser() { Initialize(); }

bool SolomonSolutionParser::LoadFile(absl::string_view file_name) {
  Initialize();
  return ParseFile(file_name);
}

void SolomonSolutionParser::Initialize() {
  routes_.clear();
  key_values_.clear();
}

bool SolomonSolutionParser::ParseFile(absl::string_view file_name) {
  bool success = false;
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    success = true;
    const std::vector<std::string> words =
        absl::StrSplit(line, ':', absl::SkipEmpty());
    // Skip blank lines
    if (words.empty()) continue;
    std::string key = words[0];
    std::string value = words.size() > 1
                            ? absl::StrJoin(words.begin() + 1, words.end(), ":")
                            : "";
    if (!RE2::FullMatch(key, "Route\\s*(\\d+)\\s*")) {
      absl::StripAsciiWhitespace(&key);
      absl::StripAsciiWhitespace(&value);
      key_values_[key] = value;
      // Note: the "Solution" key will be captured here. That key has no actual
      // usefulness and serves as a separator before reading routes.
      continue;
    }
    routes_.push_back(std::vector<int>());
    for (const auto item :
         absl::StrSplit(value, absl::ByAnyChar(" \t"), absl::SkipEmpty())) {
      routes_.back().push_back(strings::ParseLeadingInt32Value(item, -1));
    }
  }
  return success;
}

}  // namespace operations_research::routing
