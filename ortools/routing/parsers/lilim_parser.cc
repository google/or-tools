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
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/numbers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/zipfile.h"
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

#define PARSE_AND_RETURN_IF_NEGATIVE(str, dest_type, dest)   \
  dest_type dest = strings::ParseLeadingInt32Value(str, -1); \
  do {                                                       \
    if (dest < 0) return false;                              \
  } while (false)

bool LiLimParser::ParseFile(absl::string_view file_name) {
  const int64_t kInvalidDemand = std::numeric_limits<int64_t>::min();
  bool vehicles_initialized = false;
  File* file = nullptr;
  if (!file::Open(file_name, "r", &file, file::Defaults()).ok()) {
    return false;
  }
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
    // Skip blank lines
    if (words.empty()) continue;
    if (!vehicles_initialized) {
      if (words.size() != 3) return false;
      PARSE_AND_RETURN_IF_NEGATIVE(words[0], , vehicles_);
      PARSE_AND_RETURN_IF_NEGATIVE(words[1], , capacity_);
      PARSE_AND_RETURN_IF_NEGATIVE(words[2], , speed_);
      vehicles_initialized = true;
      continue;
    }
    // Reading node data.
    if (words.size() != 9) return false;
    PARSE_AND_RETURN_IF_NEGATIVE(words[1], const int64_t, x);
    PARSE_AND_RETURN_IF_NEGATIVE(words[2], const int64_t, y);
    coordinates_.push_back({x, y});
    const int64_t demand =
        strings::ParseLeadingInt64Value(words[3], kInvalidDemand);
    if (demand == kInvalidDemand) return false;
    demands_.push_back(demand);
    PARSE_AND_RETURN_IF_NEGATIVE(words[4], const int64_t, open_time);
    PARSE_AND_RETURN_IF_NEGATIVE(words[5], const int64_t, close_time);
    time_windows_.push_back({open_time, close_time});
    PARSE_AND_RETURN_IF_NEGATIVE(words[6], const int64_t, service_time);
    service_times_.push_back(service_time);
    PARSE_AND_RETURN_IF_NEGATIVE(words[7], const int64_t, pickup);
    pickups_.push_back(pickup);
    PARSE_AND_RETURN_IF_NEGATIVE(words[8], const int64_t, delivery);
    deliveries_.push_back(delivery);
  }
  return vehicles_initialized;
}

#undef PARSE_AND_RETURN_IF_NEGATIVE

}  // namespace operations_research::routing
