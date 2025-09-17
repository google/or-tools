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

#include "ortools/routing/parsers/tsptw_parser.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/numbers.h"
#include "ortools/base/path.h"
#include "ortools/base/zipfile.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {

namespace {

double DoubleEuc2DDistance(const Coordinates2<double>& from,
                           const Coordinates2<double>& to) {
  const double xd = from.x - to.x;
  const double yd = from.y - to.y;
  return sqrt(xd * xd + yd * yd);
}

double Euc2DDistance(const Coordinates2<double>& from,
                     const Coordinates2<double>& to) {
  return std::floor(DoubleEuc2DDistance(from, to));
}

constexpr double kInfinity = std::numeric_limits<double>::infinity();

std::shared_ptr<zipfile::ZipArchive> OpenZipArchiveIfItExists(
    absl::string_view file_name) {
  const absl::string_view archive_name = file::Dirname(file_name);
  if (file::Extension(archive_name) == "zip") {
    return zipfile::OpenZipArchive(archive_name);
  } else {
    return nullptr;
  }
}

}  // namespace

TspTWParser::TspTWParser()
    : size_(0),
      depot_(0),
      total_service_time_(0),
      distance_function_(nullptr),
      time_function_(nullptr) {}

bool TspTWParser::LoadFile(absl::string_view file_name) {
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  coords_.clear();
  time_windows_.clear();
  service_times_.clear();
  distance_matrix_.clear();
  size_ = 0;
  depot_ = 0;
  total_service_time_ = 0;
  distance_function_ = nullptr;
  time_function_ = nullptr;
  return ParseLopezIbanezBlum(file_name) || ParseDaSilvaUrrutia(file_name);
}

bool TspTWParser::ParseLopezIbanezBlum(absl::string_view file_name) {
  int section = 0;
  int entry_count = 0;
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
    if (words.empty()) continue;
    // Parsing comments.
    if (words[0] == "#") {
      if (absl::StrContains(line, "service times")) {
        const double total_service_time =
            strings::ParseLeadingDoubleValue(words.back(), kInfinity);
        if (total_service_time != kInfinity) {
          total_service_time_ = MathUtil::FastInt64Round(total_service_time);
        }
      }
      continue;
    }
    switch (section) {
      case 0: {  // Parsing size.
        if (words.size() != 1) return false;
        size_ = strings::ParseLeadingInt32Value(words[0], -1);
        if (size_ < 0) return false;
        distance_matrix_.reserve(size_ * size_);
        ++section;
        entry_count = 0;
        break;
      }
      case 1: {  // Parsing distances.
        if (words.size() != size_) return false;
        for (const std::string& word : words) {
          const double distance =
              strings::ParseLeadingDoubleValue(word, kInfinity);
          if (distance == kInfinity) return false;
          distance_matrix_.push_back(distance);
        }
        ++entry_count;
        if (entry_count == size_) {
          ++section;
          entry_count = 0;
        }
        break;
      }
      case 2: {  // Parsing time windows.
        if (words.size() != 2) return false;
        std::vector<double> values;
        for (const std::string& word : words) {
          const double value =
              strings::ParseLeadingDoubleValue(word, kInfinity);
          if (value == kInfinity) return false;
          values.push_back(value);
        }
        time_windows_.push_back({values[0], values[1]});
        service_times_.push_back(0);
        ++entry_count;
        if (entry_count == size_) {
          ++section;
        }
        break;
      }
      default: {
        return false;
      }
    }
  }
  distance_function_ = [this](int from, int to) {
    return distance_matrix_[from * size_ + to];
  };
  time_function_ = distance_function_;
  return entry_count == size_;
}

bool TspTWParser::ParseDaSilvaUrrutia(absl::string_view file_name) {
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    // Skip header.
    if (absl::StartsWith(line, "CUST NO.")) continue;
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
    // Skip comments and empty lines.
    if (words.empty() || words[0] == "!!" || words[0][0] == '#') continue;
    if (words.size() != 7) return false;
    // Check that all field values are doubles, except first which must be a
    // positive integer.
    const int value = strings::ParseLeadingInt32Value(words[0], -1);
    if (value < 0) return false;
    // 999 represents the eof.
    if (value == 999) continue;
    std::vector<double> values;
    for (int i = 1; i < words.size(); ++i) {
      const double value = strings::ParseLeadingDoubleValue(
          words[i], std::numeric_limits<double>::infinity());
      if (value == std::numeric_limits<double>::infinity()) return false;
      values.push_back(value);
    }
    coords_.push_back({values[0], values[1]});
    time_windows_.push_back({values[3], values[4]});
    service_times_.push_back(values[5]);
  }
  size_ = coords_.size();

  // Enforce the triangular inequality (needed due to rounding).
  distance_matrix_.reserve(size_ * size_);
  for (int i = 0; i < size_; ++i) {
    for (int j = 0; j < size_; ++j) {
      distance_matrix_.push_back(Euc2DDistance(coords_[i], coords_[j]));
    }
  }
  for (int i = 0; i < size_; i++) {
    for (int j = 0; j < size_; j++) {
      for (int k = 0; k < size_; k++) {
        if (distance_matrix_[i * size_ + j] >
            distance_matrix_[i * size_ + k] + distance_matrix_[k * size_ + j]) {
          distance_matrix_[i * size_ + j] =
              distance_matrix_[i * size_ + k] + distance_matrix_[k * size_ + j];
        }
      }
    }
  }

  distance_function_ = [this](int from, int to) {
    return distance_matrix_[from * size_ + to];
  };
  time_function_ = [this](int from, int to) {
    return distance_matrix_[from * size_ + to] + service_times_[from];
  };
  return true;
}

}  // namespace operations_research::routing
