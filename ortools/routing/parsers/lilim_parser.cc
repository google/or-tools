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

#include "ortools/routing/parsers/lilim_parser.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/zipfile.h"
#include "ortools/routing/parsers/simple_graph.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {

bool LiLimParser::LoadFile(absl::string_view file_name) {
  Initialize();
  return ParseFile(file_name);
}

bool LiLimParser::LoadFile(absl::string_view file_name,
                           absl::string_view archive_name) {
  Initialize();
  if (!absl::StartsWith(archive_name, "/")) {
    return false;
  }
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      zipfile::OpenZipArchive(archive_name));
  if (nullptr == zip_archive) {
    return false;
  }
  return ParseFile(file::JoinPath("/zip", archive_name, file_name));
}

std::optional<int> LiLimParser::GetDelivery(int node) const {
  const int delivery = deliveries_[node];
  if (delivery == 0) return std::nullopt;
  return delivery;
}

std::optional<int> LiLimParser::GetPickup(int node) const {
  const int pickup = pickups_[node];
  if (pickup == 0) return std::nullopt;
  return pickup;
}

void LiLimParser::Initialize() {
  vehicles_ = 0;
  coordinates_.clear();
  pickups_.clear();
  deliveries_.clear();
  capacity_ = 0;
  speed_ = 0;
  demands_.clear();
  time_windows_.clear();
  service_times_.clear();
}

bool LiLimParser::ParseFile(absl::string_view file_name) {
  File* file = nullptr;
  if (!file::Open(file_name, "r", &file, file::Defaults()).ok()) {
    return false;
  }
  const auto parse_non_negative = [](absl::string_view str, auto& dest) {
    return absl::SimpleAtoi(str, &dest) && dest >= 0;
  };
  bool vehicles_initialized = false;
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<absl::string_view> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
    // Skip blank lines
    if (words.empty()) continue;
    if (!vehicles_initialized) {
      if (words.size() != 3 || !parse_non_negative(words[0], vehicles_) ||
          !parse_non_negative(words[1], capacity_) ||
          !parse_non_negative(words[2], speed_)) {
        return false;
      }
      vehicles_initialized = true;
      continue;
    }
    // Reading node data.
    Coordinates2<int64_t>& coords = coordinates_.emplace_back();
    int64_t& demand = demands_.emplace_back();
    SimpleTimeWindow<int64_t>& tw = time_windows_.emplace_back();
    int64_t& service_time = service_times_.emplace_back();
    int& pickup = pickups_.emplace_back();
    int& delivery = deliveries_.emplace_back();
    if (words.size() != 9 || !parse_non_negative(words[1], coords.x) ||
        !parse_non_negative(words[2], coords.y) ||
        !absl::SimpleAtoi(words[3], &demand) ||
        !parse_non_negative(words[4], tw.start) ||
        !parse_non_negative(words[5], tw.end) ||
        !parse_non_negative(words[6], service_time) ||
        !parse_non_negative(words[7], pickup) ||
        !parse_non_negative(words[8], delivery)) {
      return false;
    }
  }
  return vehicles_initialized;
}

}  // namespace operations_research::routing
