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

#include "ortools/routing/parsers/tsplib_parser.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/base/map_util.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/types.h"
#include "ortools/base/zipfile.h"
#include "ortools/routing/parsers/simple_graph.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {
namespace {

// ----- Distances -----
// As defined by the TSPLIB95 doc

static int64_t ATTDistance(const Coordinates3<double>& from,
                           const Coordinates3<double>& to) {
  const double xd = from.x - to.x;
  const double yd = from.y - to.y;
  const double euc = sqrt((xd * xd + yd * yd) / 10.0);
  int64_t distance = std::round(euc);
  if (distance < euc) ++distance;
  return distance;
}

static double DoubleEuc2DDistance(const Coordinates3<double>& from,
                                  const Coordinates3<double>& to) {
  const double xd = from.x - to.x;
  const double yd = from.y - to.y;
  return sqrt(xd * xd + yd * yd);
}

static int64_t Euc2DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  return std::round(DoubleEuc2DDistance(from, to));
}

static int64_t Euc3DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  const double xd = from.x - to.x;
  const double yd = from.y - to.y;
  const double zd = from.z - to.z;
  const double euc = sqrt(xd * xd + yd * yd + zd * zd);
  return std::round(euc);
}

static int64_t Ceil2DDistance(const Coordinates3<double>& from,
                              const Coordinates3<double>& to) {
  return static_cast<int64_t>(ceil(DoubleEuc2DDistance(from, to)));
}

static double ToRad(double x) {
  static const double kPi = 3.141592;
  const int64_t deg = static_cast<int64_t>(x);
  const double min = x - deg;
  return kPi * (deg + 5.0 * min / 3.0) / 180.0;
}

static int64_t GeoDistance(const Coordinates3<double>& from,
                           const Coordinates3<double>& to) {
  static const double kRadius = 6378.388;
  const double q1 = cos(ToRad(from.y) - ToRad(to.y));
  const double q2 = cos(ToRad(from.x) - ToRad(to.x));
  const double q3 = cos(ToRad(from.x) + ToRad(to.x));
  const double cos_angle =
      std::clamp(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3), -1.0, 1.0);
  return static_cast<int64_t>(kRadius * acos(cos_angle) + 1.0);
}

static int64_t GeoMDistance(const Coordinates3<double>& from,
                            const Coordinates3<double>& to) {
  static const double kPi = 3.14159265358979323846264;
  static const double kRadius = 6378388.0;
  const double from_lat = kPi * from.x / 180.0;
  const double to_lat = kPi * to.x / 180.0;
  const double from_lng = kPi * from.y / 180.0;
  const double to_lng = kPi * to.y / 180.0;
  const double q1 = cos(to_lat) * sin(from_lng - to_lng);
  const double q3 = sin((from_lng - to_lng) / 2.0);
  const double q4 = cos((from_lng - to_lng) / 2.0);
  const double q2 =
      sin(from_lat + to_lat) * q3 * q3 - sin(from_lat - to_lat) * q4 * q4;
  const double q5 =
      cos(from_lat - to_lat) * q4 * q4 - cos(from_lat + to_lat) * q3 * q3;
  return static_cast<int64_t>(kRadius * atan2(sqrt(q1 * q1 + q2 * q2), q5) +
                              1.0);
}

static int64_t Man2DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  const double xd = fabs(from.x - to.x);
  const double yd = fabs(from.y - to.y);
  return std::round(xd + yd);
}

static int64_t Man3DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  const double xd = fabs(from.x - to.x);
  const double yd = fabs(from.y - to.y);
  const double zd = fabs(from.z - to.z);
  return std::round(xd + yd + zd);
}

static int64_t Max2DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  const double xd = fabs(from.x - to.x);
  const double yd = fabs(from.y - to.y);
  return std::round(std::max(xd, yd));
}

static int64_t Max3DDistance(const Coordinates3<double>& from,
                             const Coordinates3<double>& to) {
  const double xd = fabs(from.x - to.x);
  const double yd = fabs(from.y - to.y);
  const double zd = fabs(from.z - to.z);
  return std::round(std::max(xd, std::max(yd, zd)));
}

std::shared_ptr<zipfile::ZipArchive> OpenZipArchiveIfItExists(
    absl::string_view file_name) {
  const absl::string_view archive_name = file::Dirname(file_name);
  if (file::Extension(archive_name) == "zip") {
    return zipfile::OpenZipArchive(archive_name);
  } else {
    return nullptr;
  }
}

constexpr int kMaxDimension = 10'000'000;
constexpr int kMaxExplicitMatrixEntries = 100'000'000;
constexpr size_t kMaxLineLength = 10'000'000;
constexpr size_t kMaxInputBytes = 1'000'000'000;
constexpr int64_t kMaxTotalLines = 50'000'000;
constexpr int64_t kMaxEdgesOrTourEntries = 100'000'000;
// Bound coordinate magnitude so squared 3D distances fit in IEEE-754 double
// and rounded distances fit within exact double integer precision (< 2^53) and
// int64_t.
constexpr double kMaxCoordinateAbs = 1e15;

template <typename IntType = int>
absl::StatusOr<IntType> ParseInt(absl::string_view word) {
  IntType value;
  if (!absl::SimpleAtoi(word, &value)) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Invalid integer token: '" << word << "'";
  }
  return value;
}

absl::StatusOr<int> ParseNodeIndex(absl::string_view word, int size) {
  ABSL_ASSIGN_OR_RETURN(const int raw_id, ParseInt(word));
  if (raw_id < 1 || raw_id > size) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Node id " << raw_id << " out of bounds [1, " << size << "]";
  }
  return raw_id - 1;
}

absl::StatusOr<double> ParseCoordinate(absl::string_view word) {
  double value = 0.0;
  if (!absl::SimpleAtod(word, &value) || !std::isfinite(value) ||
      std::abs(value) > kMaxCoordinateAbs) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Invalid or out-of-range coordinate: '" << word << "'";
  }
  return value;
}

// Tracks cumulative input byte and line counts and enforces parser limits.
class LineTracker {
 public:
  absl::Status CheckLineLimits(absl::string_view line) {
    if (line.size() > kMaxLineLength) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Line length exceeds maximum allowed (" << kMaxLineLength
             << ")";
    }
    if (total_bytes_ > kMaxInputBytes - line.size()) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Input size exceeds maximum allowed (" << kMaxInputBytes
             << " bytes)";
    }
    total_bytes_ += line.size();
    if (++total_lines_ > kMaxTotalLines) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Line count exceeds maximum allowed (" << kMaxTotalLines << ")";
    }
    return absl::OkStatus();
  }

 private:
  size_t total_bytes_ = 0;
  int64_t total_lines_ = 0;
};

}  // namespace

TspLibParser::TspLibParser()
    : size_(0),
      capacity_(std::nullopt),
      max_distance_(kint64max),
      explicit_costs_(),
      depot_(0),
      section_(UNDEFINED_SECTION),
      type_(Types::UNDEFINED_TYPE),
      edge_data_format_(UNDEFINED_EDGE_DATA_FORMAT),
      edge_weight_type_(UNDEFINED_EDGE_WEIGHT_TYPE),
      edge_weight_format_(UNDEFINED_EDGE_WEIGHT_FORMAT),
      edge_row_(0),
      edge_column_(0),
      to_read_(0) {}

absl::Status TspLibParser::SetExplicitCost(int from, int to, int64_t cost) {
  if (from < 0 || from >= size_ || to < 0 || to >= size_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Matrix indices (" << from << ", " << to
           << ") out of bounds for dimension " << size_;
  }
  if (size_ > kMaxExplicitMatrixEntries / size_ ||
      explicit_costs_.size() != size_ * size_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Explicit cost matrix is not initialized for dimension " << size_;
  }
  explicit_costs_[from * size_ + to] = cost;
  return absl::OkStatus();
}

namespace {
absl::StatusOr<File*> OpenFile(absl::string_view file_name) {
  File* file = nullptr;
  ABSL_RETURN_IF_ERROR(file::Open(file_name, "r", &file, file::Defaults()));
  return file;
}
}  // namespace

absl::StatusOr<TspLibParser> TspLibParser::LoadFile(
    absl::string_view file_name) {
  TspLibParser parser;
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ABSL_ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  LineTracker line_tracker;
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ABSL_RETURN_IF_ERROR(line_tracker.CheckLineLimits(line));
    ABSL_RETURN_IF_ERROR(parser.ProcessNewLine(line));
    if (parser.section_ == END_OF_FILE) break;
  }
  if (!parser.valid_section_found_ || parser.size_ <= 0) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Could not find any valid sections in " << file_name;
  }
  if ((parser.type_ == Types::CVRP || parser.capacity_.has_value()) &&
      parser.demands_.size() != parser.size_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Missing DEMAND_SECTION for capacitated routing problem in "
           << file_name;
  }
  ABSL_RETURN_IF_ERROR(parser.ValidateEdgeWeights());
  return parser;
}

absl::Status TspLibParser::ParseExplicitFullMatrix(
    absl::Span<const std::string> words) {
  for (const std::string& word : words) {
    if (type_ == Types::SOP && !sop_dimension_read_) {
      // Matrix size is present in SOP which is redundant with dimension and
      // must not be confused with the first cell of the matrix.
      ABSL_ASSIGN_OR_RETURN(const int sop_size, ParseInt(word));
      if (sop_size != size_) {
        return ortools::InvalidArgumentErrorBuilder()
               << "SOP matrix dimension " << sop_size
               << " does not match DIMENSION " << size_;
      }
      sop_dimension_read_ = true;
      continue;
    }
    if (to_read_ <= 0 || edge_row_ >= size_) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Too many entries in FULL_MATRIX EDGE_WEIGHT_SECTION";
    }
    ABSL_ASSIGN_OR_RETURN(const int64_t cost, ParseInt<int64_t>(word));
    ABSL_RETURN_IF_ERROR(SetExplicitCost(edge_row_, edge_column_, cost));
    ++edge_column_;
    if (edge_column_ >= size_) {
      edge_column_ = 0;
      ++edge_row_;
    }
    if (--to_read_ == 0) {
      explicit_edge_weights_complete_ = true;
    }
  }
  return absl::OkStatus();
}

absl::Status TspLibParser::ParseExplicitUpperRow(
    absl::Span<const std::string> words, bool ignore_diagonal) {
  for (const std::string& word : words) {
    if (to_read_ <= 0 || edge_row_ >= size_ || edge_column_ >= size_) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Too many entries in "
             << (ignore_diagonal ? "UPPER_ROW/LOWER_COL"
                                 : "UPPER_DIAG_ROW/LOWER_DIAG_COL")
             << " EDGE_WEIGHT_SECTION";
    }
    ABSL_ASSIGN_OR_RETURN(const int64_t cost, ParseInt<int64_t>(word));
    ABSL_RETURN_IF_ERROR(SetExplicitCost(edge_row_, edge_column_, cost));
    ABSL_RETURN_IF_ERROR(SetExplicitCost(edge_column_, edge_row_, cost));
    ++edge_column_;
    if (edge_column_ >= size_) {
      ++edge_row_;
      edge_column_ = ignore_diagonal ? edge_row_ + 1 : edge_row_;
    }
    if (--to_read_ == 0) {
      explicit_edge_weights_complete_ = true;
    }
  }
  return absl::OkStatus();
}

absl::Status TspLibParser::ParseExplicitLowerRow(
    absl::Span<const std::string> words, bool ignore_diagonal) {
  for (const std::string& word : words) {
    if (to_read_ <= 0 || edge_row_ >= size_) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Too many entries in "
             << (ignore_diagonal ? "LOWER_ROW/UPPER_COL"
                                 : "LOWER_DIAG_ROW/UPPER_DIAG_COL")
             << " EDGE_WEIGHT_SECTION";
    }
    ABSL_ASSIGN_OR_RETURN(const int64_t cost, ParseInt<int64_t>(word));
    ABSL_RETURN_IF_ERROR(SetExplicitCost(edge_row_, edge_column_, cost));
    ABSL_RETURN_IF_ERROR(SetExplicitCost(edge_column_, edge_row_, cost));
    ++edge_column_;
    if (edge_column_ >= (ignore_diagonal ? edge_row_ : edge_row_ + 1)) {
      edge_column_ = 0;
      ++edge_row_;
    }
    if (--to_read_ == 0) {
      explicit_edge_weights_complete_ = true;
    }
  }
  return absl::OkStatus();
}

absl::Status TspLibParser::ParseNodeCoord(absl::Span<const std::string> words) {
  if (words.size() < 3 || words.size() > 4) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Expected 3 or 4 tokens in coordinate line, got " << words.size();
  }
  if (size_ <= 0 || coords_.size() != size_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Coordinate section encountered before valid DIMENSION";
  }
  ABSL_ASSIGN_OR_RETURN(const int node, ParseNodeIndex(words[0], size_));
  ABSL_ASSIGN_OR_RETURN(coords_[node].x, ParseCoordinate(words[1]));
  ABSL_ASSIGN_OR_RETURN(coords_[node].y, ParseCoordinate(words[2]));
  if (words.size() == 4) {
    ABSL_ASSIGN_OR_RETURN(coords_[node].z, ParseCoordinate(words[3]));
  } else {
    coords_[node].z = 0;
  }
  --to_read_;
  return absl::OkStatus();
}

absl::Status TspLibParser::SetUpEdgeWeightSection() {
  if (size_ <= 0) {
    return ortools::InvalidArgumentErrorBuilder()
           << "EDGE_WEIGHT_SECTION encountered before valid DIMENSION";
  }
  if (size_ > kMaxExplicitMatrixEntries / size_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "DIMENSION " << size_
           << " is too large for an explicit edge weight matrix";
  }
  explicit_costs_.assign(size_ * size_, 0);
  edge_row_ = 0;
  edge_column_ = 0;
  sop_dimension_read_ = false;
  explicit_edge_weights_complete_ = false;
  switch (edge_weight_format_) {
    case FULL_MATRIX:
      to_read_ = size_ * size_;
      break;
    case LOWER_COL:
    case UPPER_ROW:
      ABSL_RETURN_IF_ERROR(SetExplicitCost(0, 0, 0));
      ++edge_column_;
      to_read_ = ((size_ - 1) * size_) / 2;
      break;
    case UPPER_COL:
    case LOWER_ROW:
      ABSL_RETURN_IF_ERROR(SetExplicitCost(0, 0, 0));
      ++edge_row_;
      to_read_ = ((size_ - 1) * size_) / 2;
      break;
    case LOWER_DIAG_COL:
    case UPPER_DIAG_ROW:
      to_read_ = ((size_ + 1) * size_) / 2;
      break;
    case UPPER_DIAG_COL:
    case LOWER_DIAG_ROW:
      to_read_ = ((size_ + 1) * size_) / 2;
      break;
    default:
      return ortools::InvalidArgumentErrorBuilder()
             << "Unknown or unsupported EDGE_WEIGHT_FORMAT in "
                "EDGE_WEIGHT_SECTION: "
             << edge_weight_format_;
  }
  if (to_read_ == 0 &&
      !(type_ == Types::SOP && edge_weight_format_ == FULL_MATRIX)) {
    explicit_edge_weights_complete_ = true;
  }
  return absl::OkStatus();
}

void TspLibParser::CheckNodeBounds(int from, int to) const {
  CHECK_GE(from, 0);
  CHECK_LT(from, size_);
  CHECK_GE(to, 0);
  CHECK_LT(to, size_);
}

EdgeWeights TspLibParser::GetEdgeWeights() const {
  if (type_ == Types::HCP || type_ == Types::TOUR) {
    return nullptr;
  }
  switch (edge_weight_type_) {
    case EXPLICIT:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return explicit_costs_[from * size_ + to];
      };
    case EUC_2D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Euc2DDistance(coords_[from], coords_[to]);
      };
    case EUC_3D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Euc3DDistance(coords_[from], coords_[to]);
      };
    case MAX_2D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Max2DDistance(coords_[from], coords_[to]);
      };
    case MAX_3D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Max3DDistance(coords_[from], coords_[to]);
      };
    case MAN_2D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Man2DDistance(coords_[from], coords_[to]);
      };
    case MAN_3D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Man3DDistance(coords_[from], coords_[to]);
      };
    case CEIL_2D:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return Ceil2DDistance(coords_[from], coords_[to]);
      };
    case GEO:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return GeoDistance(coords_[from], coords_[to]);
      };
    case GEOM:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return GeoMDistance(coords_[from], coords_[to]);
      };
    case ATT:
      return [this](int from, int to) {
        CheckNodeBounds(from, to);
        return ATTDistance(coords_[from], coords_[to]);
      };
    default:
      return nullptr;
  }
}

absl::Status TspLibParser::ValidateEdgeWeights() const {
  if (type_ == Types::HCP || type_ == Types::TOUR) {
    VLOG(2) << "No edge weights";
    return absl::OkStatus();
  }
  VLOG(2) << "Edge weight type: " << edge_weight_type_;
  switch (edge_weight_type_) {
    case EXPLICIT:
      if (size_ <= 0 || size_ > kMaxExplicitMatrixEntries / size_ ||
          !explicit_edge_weights_complete_ ||
          explicit_costs_.size() != size_ * size_) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Incomplete or missing EDGE_WEIGHT_SECTION for EXPLICIT "
                  "EDGE_WEIGHT_TYPE";
      }
      return absl::OkStatus();
    case EUC_2D:
    case EUC_3D:
    case MAX_2D:
    case MAX_3D:
    case MAN_2D:
    case MAN_3D:
    case CEIL_2D:
    case GEO:
    case GEOM:
    case ATT:
      if (size_ <= 0 || coords_.size() != size_) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Coordinate array size (" << coords_.size()
               << ") does not match DIMENSION (" << size_ << ")";
      }
      return absl::OkStatus();
    case XRAY1:
      return ortools::InvalidArgumentErrorBuilder()
             << "XRAY1 not supported for EDGE_WEIGHT_TYPE";
    case XRAY2:
      return ortools::InvalidArgumentErrorBuilder()
             << "XRAY2 not supported for EDGE_WEIGHT_TYPE";
    case SPECIAL:
      return ortools::InvalidArgumentErrorBuilder()
             << "SPECIAL not supported for EDGE_WEIGHT_TYPE";
    default:
      return ortools::InvalidArgumentErrorBuilder()
             << "Unknown or missing EDGE_WEIGHT_TYPE: " << edge_weight_type_;
  }
}

absl::Status TspLibParser::ParseSections(absl::Span<const std::string> words) {
  const int words_size = words.size();
  if (words_size == 0) {
    return absl::OkStatus();
  }
  if (!gtl::FindCopy(*kSections, words[0], &section_)) {
    LOG(WARNING) << "Unknown section: " << words[0];
    return absl::OkStatus();
  }
  valid_section_found_ = true;
  const std::string& last_word = words[words_size - 1];
  switch (section_) {
    case NAME: {
      name_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
      break;
    }
    case TYPE: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for TYPE section";
      }
      const std::string& type = words[1];
      if (!gtl::FindCopy(*kTypes, type, &type_)) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Unknown TYPE: " << type;
      }
      break;
    }
    case COMMENT: {
      if (!comments_.empty()) {
        comments_ += '\n';
      }
      comments_ += absl::StrJoin(words.begin() + 1, words.end(), " ");
      break;
    }
    case DIMENSION: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for DIMENSION section";
      }
      if (size_ > 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Duplicate DIMENSION section";
      }
      ABSL_ASSIGN_OR_RETURN(const int parsed_size, ParseInt(last_word));
      if (parsed_size <= 0 || parsed_size > kMaxDimension) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Invalid DIMENSION: " << parsed_size;
      }
      size_ = parsed_size;
      coords_.resize(size_);
      break;
    }
    case DISTANCE: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for DISTANCE section";
      }
      // This is non-standard but is supported as an upper bound on the length
      // of each route.
      ABSL_ASSIGN_OR_RETURN(max_distance_, ParseInt<int64_t>(last_word));
      break;
    }
    case CAPACITY: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for CAPACITY section";
      }
      ABSL_ASSIGN_OR_RETURN(capacity_, ParseInt<int64_t>(last_word));
      break;
    }
    case EDGE_DATA_FORMAT: {
      if (type_ != Types::HCP || words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Invalid EDGE_DATA_FORMAT section";
      }
      if (!gtl::FindCopy(*kEdgeDataFormats, last_word, &edge_data_format_)) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Unknown EDGE_DATA_FORMAT: " << last_word;
      }
      break;
    }
    case EDGE_DATA_SECTION: {
      if (type_ != Types::HCP || size_ <= 0 ||
          edge_data_format_ == UNDEFINED_EDGE_DATA_FORMAT) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Invalid EDGE_DATA_SECTION state";
      }
      edges_.resize(size_);
      to_read_ = 1;
      break;
    }
    case EDGE_WEIGHT_TYPE: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for EDGE_WEIGHT_TYPE section";
      }
      if (!gtl::FindCopy(*kEdgeWeightTypes, last_word, &edge_weight_type_)) {
        // Some data sets invert EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
        LOG(WARNING) << "Unknown EDGE_WEIGHT_TYPE: " << last_word;
        LOG(WARNING) << "Trying in EDGE_WEIGHT_FORMAT formats";
        if (!gtl::FindCopy(*kEdgeWeightFormats, last_word,
                           &edge_weight_format_)) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Unknown EDGE_WEIGHT_TYPE: " << last_word;
        }
      }
      break;
    }
    case EDGE_WEIGHT_FORMAT: {
      if (words.size() < 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Missing value for EDGE_WEIGHT_FORMAT section";
      }
      if (!gtl::FindCopy(*kEdgeWeightFormats, last_word,
                         &edge_weight_format_)) {
        // Some data sets invert EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
        LOG(WARNING) << "Unknown EDGE_WEIGHT_FORMAT: " << last_word;
        LOG(WARNING) << "Trying in EDGE_WEIGHT_TYPE types";
        if (!gtl::FindCopy(*kEdgeWeightTypes, last_word, &edge_weight_type_)) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Unknown EDGE_WEIGHT_FORMAT: " << last_word;
        }
      }
      break;
    }
    case EDGE_WEIGHT_SECTION: {
      ABSL_RETURN_IF_ERROR(SetUpEdgeWeightSection());
      break;
    }
    case FIXED_EDGES_SECTION: {
      if (size_ <= 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "FIXED_EDGES_SECTION encountered before valid DIMENSION";
      }
      to_read_ = kint64max;
      break;
    }
    case NODE_COORD_TYPE: {
      break;
    }
    case DISPLAY_DATA_TYPE: {
      break;
    }
    case DISPLAY_DATA_SECTION: {
      if (size_ <= 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "DISPLAY_DATA_SECTION encountered before valid DIMENSION";
      }
      to_read_ = size_;
      break;
    }
    case NODE_COORD_SECTION: {
      if (size_ <= 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "NODE_COORD_SECTION encountered before valid DIMENSION";
      }
      to_read_ = size_;
      break;
    }
    case DEPOT_SECTION: {
      if (size_ <= 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "DEPOT_SECTION encountered before valid DIMENSION";
      }
      to_read_ = kint64max;
      break;
    }
    case DEMAND_SECTION: {
      if (size_ <= 0) {
        return ortools::InvalidArgumentErrorBuilder()
               << "DEMAND_SECTION encountered before valid DIMENSION";
      }
      demands_.resize(size_, 0);
      to_read_ = size_;
      break;
    }
    case END_OF_FILE: {
      break;
    }
    default: {
      LOG(WARNING) << "Unknown section: " << words[0];
    }
  }
  return absl::OkStatus();
}

absl::Status TspLibParser::ProcessNewLine(absl::string_view line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());
  if (!words.empty()) {
    // New section detected.
    if (kSections->contains(words[0])) {
      to_read_ = 0;
    }
    if (to_read_ > 0) {
      switch (section_) {
        case EDGE_DATA_SECTION: {
          switch (edge_data_format_) {
            case EDGE_LIST: {
              if (words[0] == "-1") {
                if (words.size() != 1) {
                  return ortools::InvalidArgumentErrorBuilder()
                         << "Unexpected tokens after -1 in EDGE_LIST";
                }
                // Remove duplicate edges
                for (auto& edges : edges_) {
                  std::sort(edges.begin(), edges.end());
                  const auto it = std::unique(edges.begin(), edges.end());
                  edges.resize(std::distance(edges.begin(), it));
                }
                to_read_ = 0;
              } else {
                if (words.size() != 2) {
                  return ortools::InvalidArgumentErrorBuilder()
                         << "Expected 2 node ids in EDGE_LIST, got "
                         << words.size();
                }
                if (edge_count_ >= kMaxEdgesOrTourEntries) {
                  return ortools::InvalidArgumentErrorBuilder()
                         << "Edge count exceeds maximum allowed";
                }
                ABSL_ASSIGN_OR_RETURN(const int from,
                                      ParseNodeIndex(words[0], size_));
                ABSL_ASSIGN_OR_RETURN(const int to,
                                      ParseNodeIndex(words[1], size_));
                edges_[std::min(from, to)].push_back(std::max(from, to));
                ++edge_count_;
              }
              break;
            }
            case ADJ_LIST: {
              if (words[0] == "-1") {
                if (words.size() != 1) {
                  return ortools::InvalidArgumentErrorBuilder()
                         << "Unexpected tokens after -1 in ADJ_LIST";
                }
                for (auto& edges : edges_) {
                  std::sort(edges.begin(), edges.end());
                  const auto it = std::unique(edges.begin(), edges.end());
                  edges.resize(std::distance(edges.begin(), it));
                }
                to_read_ = 0;
                break;
              }
              ABSL_ASSIGN_OR_RETURN(const int from,
                                    ParseNodeIndex(words[0], size_));
              for (size_t i = 1; i < words.size(); ++i) {
                ABSL_ASSIGN_OR_RETURN(const int raw_to, ParseInt(words[i]));
                if (raw_to != -1) {
                  if (raw_to < 1 || raw_to > size_) {
                    return ortools::InvalidArgumentErrorBuilder()
                           << "Node id " << raw_to << " out of bounds [1, "
                           << size_ << "]";
                  }
                  if (edge_count_ >= kMaxEdgesOrTourEntries) {
                    return ortools::InvalidArgumentErrorBuilder()
                           << "Edge count exceeds maximum allowed";
                  }
                  const int to = raw_to - 1;
                  edges_[std::min(from, to)].push_back(std::max(from, to));
                  ++edge_count_;
                } else if (i != words.size() - 1) {
                  return ortools::InvalidArgumentErrorBuilder()
                         << "Unexpected tokens after -1 in ADJ_LIST row";
                }
              }
              ABSL_ASSIGN_OR_RETURN(const int last_val, ParseInt(words.back()));
              if (last_val != -1) {
                LOG(WARNING) << "Missing -1 at the end of ADJ_LIST";
              }
              break;
            }
            default:
              return ortools::InvalidArgumentErrorBuilder()
                     << "Unknown EDGE_DATA_FORMAT: " << edge_data_format_;
          }
          break;
        }
        case EDGE_WEIGHT_SECTION: {
          switch (edge_weight_format_) {
            case FULL_MATRIX:
              ABSL_RETURN_IF_ERROR(ParseExplicitFullMatrix(words));
              break;
            case UPPER_ROW:
            case LOWER_COL:
              ABSL_RETURN_IF_ERROR(
                  ParseExplicitUpperRow(words, /*ignore_diagonal=*/true));
              break;
            case LOWER_ROW:
            case UPPER_COL:
              ABSL_RETURN_IF_ERROR(
                  ParseExplicitLowerRow(words, /*ignore_diagonal=*/true));
              break;
            case UPPER_DIAG_ROW:
            case LOWER_DIAG_COL:
              ABSL_RETURN_IF_ERROR(
                  ParseExplicitUpperRow(words, /*ignore_diagonal=*/false));
              break;
            case LOWER_DIAG_ROW:
            case UPPER_DIAG_COL:
              ABSL_RETURN_IF_ERROR(
                  ParseExplicitLowerRow(words, /*ignore_diagonal=*/false));
              break;
            default:
              return ortools::InvalidArgumentErrorBuilder()
                     << "Unknown EDGE_WEIGHT_FORMAT: " << edge_weight_format_;
          }
          break;
        }
        case FIXED_EDGES_SECTION: {
          if (words.size() == 1) {
            ABSL_ASSIGN_OR_RETURN(const int sentinel, ParseInt(words[0]));
            if (sentinel != -1) {
              return ortools::InvalidArgumentErrorBuilder()
                     << "Expected -1 terminator in FIXED_EDGES_SECTION, got "
                     << sentinel;
            }
            to_read_ = 0;
          } else if (words.size() == 2) {
            if (fixed_edges_.size() >= kMaxEdgesOrTourEntries) {
              return ortools::InvalidArgumentErrorBuilder()
                     << "Fixed edge count exceeds maximum allowed";
            }
            ABSL_ASSIGN_OR_RETURN(const int from,
                                  ParseNodeIndex(words[0], size_));
            ABSL_ASSIGN_OR_RETURN(const int to,
                                  ParseNodeIndex(words[1], size_));
            fixed_edges_.insert(std::make_pair(from, to));
          } else {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Invalid line in FIXED_EDGES_SECTION with "
                   << words.size() << " tokens";
          }
          break;
        }
        case NODE_COORD_SECTION: {
          ABSL_RETURN_IF_ERROR(ParseNodeCoord(words));
          break;
        }
        case DEPOT_SECTION: {
          if (words.size() == 1) {
            ABSL_ASSIGN_OR_RETURN(const int raw_depot, ParseInt(words[0]));
            if (raw_depot == -1) {
              to_read_ = 0;
            } else if (raw_depot >= 1 && raw_depot <= size_) {
              depot_ = raw_depot - 1;
              VLOG(2) << "Depot: " << depot_;
            } else {
              return ortools::InvalidArgumentErrorBuilder()
                     << "Depot index " << raw_depot << " out of bounds [1, "
                     << size_ << "]";
            }
          } else if (words.size() == 2 || words.size() == 3) {
            if (size_ <= 0 || coords_.size() != size_) {
              return ortools::InvalidArgumentErrorBuilder()
                     << "DEPOT_SECTION coordinates require valid DIMENSION";
            }
            const int depot = size_ - 1;
            VLOG(2) << "Depot: " << depot;
            depot_ = depot;
            ABSL_ASSIGN_OR_RETURN(coords_[depot].x, ParseCoordinate(words[0]));
            ABSL_ASSIGN_OR_RETURN(coords_[depot].y, ParseCoordinate(words[1]));
            if (words.size() == 3) {
              ABSL_ASSIGN_OR_RETURN(coords_[depot].z,
                                    ParseCoordinate(words[2]));
            } else {
              coords_[depot].z = 0;
            }
          } else {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Invalid line in DEPOT_SECTION with " << words.size()
                   << " tokens";
          }
          break;
        }
        case DEMAND_SECTION: {
          if (words.size() < 2) {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Expected at least 2 tokens in DEMAND_SECTION, got "
                   << words.size();
          }
          ABSL_ASSIGN_OR_RETURN(const int node,
                                ParseNodeIndex(words[0], demands_.size()));
          ABSL_ASSIGN_OR_RETURN(demands_[node], ParseInt<int64_t>(words[1]));
          --to_read_;
          break;
        }
        case DISPLAY_DATA_SECTION: {
          ABSL_RETURN_IF_ERROR(ParseNodeCoord(words));
          break;
        }
        default: {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Reading data outside valid section";
        }
      }
    } else {
      ABSL_RETURN_IF_ERROR(ParseSections(words));
    }
  }
  return absl::OkStatus();
}

std::string TspLibParser::BuildTourFromRoutes(
    absl::Span<const std::vector<int>> routes) const {
  std::string tours = absl::StrCat(
      "NAME : ", name_, "\nCOMMENT :\nTYPE : TOUR\nDIMENSION : ", size(),
      "\nTOUR_SECTION\n");
  for (const auto& route : routes) {
    for (const int node : route) {
      absl::StrAppend(&tours, node + 1, "\n");
    }
    absl::StrAppend(&tours, "-1\n");
  }
  return absl::StrCat(tours, "EOF");
}

const absl::flat_hash_map<std::string, TspLibParser::Sections>* const
    TspLibParser::kSections =
        new absl::flat_hash_map<std::string, TspLibParser::Sections>(
            {{"NAME", NAME},
             {"TYPE", TYPE},
             {"COMMENT", COMMENT},
             {"DIMENSION", DIMENSION},
             {"DISTANCE", DISTANCE},
             {"CAPACITY", CAPACITY},
             {"EDGE_DATA_FORMAT", EDGE_DATA_FORMAT},
             {"EDGE_DATA_SECTION", EDGE_DATA_SECTION},
             {"EDGE_WEIGHT_TYPE", EDGE_WEIGHT_TYPE},
             {"EDGE_WEIGHT_FORMAT", EDGE_WEIGHT_FORMAT},
             {"EDGE_WEIGHT_SECTION", EDGE_WEIGHT_SECTION},
             {"FIXED_EDGES_SECTION", FIXED_EDGES_SECTION},
             {"FIXED_EDGES", FIXED_EDGES_SECTION},
             {"DISPLAY_DATA_SECTION", DISPLAY_DATA_SECTION},
             {"NODE_COORD_TYPE", NODE_COORD_TYPE},
             {"DISPLAY_DATA_TYPE", DISPLAY_DATA_TYPE},
             {"NODE_COORD_SECTION", NODE_COORD_SECTION},
             {"DEPOT_SECTION", DEPOT_SECTION},
             {"DEMAND_SECTION", DEMAND_SECTION},
             {"EOF", END_OF_FILE}});

const absl::flat_hash_map<std::string, TspLibParser::Types>* const
    TspLibParser::kTypes =
        new absl::flat_hash_map<std::string, TspLibParser::Types>(
            {{"TSP", Types::TSP},
             {"ATSP", Types::ATSP},
             {"SOP", Types::SOP},
             {"HCP", Types::HCP},
             {"CVRP", Types::CVRP},
             {"TOUR", Types::TOUR}});

const absl::flat_hash_map<std::string, TspLibParser::EdgeDataFormat>* const
    TspLibParser::kEdgeDataFormats =
        new absl::flat_hash_map<std::string, TspLibParser::EdgeDataFormat>(
            {{"EDGE_LIST", EDGE_LIST}, {"ADJ_LIST", ADJ_LIST}});

const absl::flat_hash_map<std::string, TspLibParser::EdgeWeightTypes>* const
    TspLibParser::kEdgeWeightTypes =
        new absl::flat_hash_map<std::string, TspLibParser::EdgeWeightTypes>(
            {{"EXPLICIT", EXPLICIT},
             {"EUC_2D", EUC_2D},
             {"EUC_3D", EUC_3D},
             {"MAX_2D", MAX_2D},
             {"MAX_3D", MAX_3D},
             {"MAN_2D", MAN_2D},
             {"MAN_3D", MAN_3D},
             {"CEIL_2D", CEIL_2D},
             {"GEO", GEO},
             {"GEOM", GEOM},
             {"ATT", ATT},
             {"XRAY1", XRAY1},
             {"XRAY2", XRAY2},
             {"SPECIAL", SPECIAL}});

const absl::flat_hash_map<std::string, TspLibParser::EdgeWeightFormats>* const
    TspLibParser::kEdgeWeightFormats =
        new absl::flat_hash_map<std::string, TspLibParser::EdgeWeightFormats>(
            {{"FUNCTION", FUNCTION},
             {"FULL_MATRIX", FULL_MATRIX},
             {"UPPER_ROW", UPPER_ROW},
             {"LOWER_ROW", LOWER_ROW},
             {"UPPER_DIAG_ROW", UPPER_DIAG_ROW},
             {"LOWER_DIAG_ROW", LOWER_DIAG_ROW},
             {"UPPER_COL", UPPER_COL},
             {"LOWER_COL", LOWER_COL},
             {"UPPER_DIAG_COL", UPPER_DIAG_COL},
             {"LOWER_DIAG_COL", LOWER_DIAG_COL}});

TspLibTourParser::TspLibTourParser() : section_(UNDEFINED_SECTION), size_(0) {}

absl::Status TspLibTourParser::LoadFile(absl::string_view file_name) {
  section_ = UNDEFINED_SECTION;
  size_ = 0;
  comments_.clear();
  tour_.clear();
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ABSL_ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  LineTracker line_tracker;
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ABSL_RETURN_IF_ERROR(line_tracker.CheckLineLimits(line));
    ABSL_RETURN_IF_ERROR(ProcessNewLine(line));
    if (section_ == END_OF_FILE) break;
  }
  return absl::OkStatus();
}

absl::Status TspLibTourParser::ProcessNewLine(absl::string_view line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  const int word_size = words.size();
  if (word_size > 0) {
    if (section_ == TOUR_SECTION) {
      for (const std::string& word : words) {
        if (word == "EOF") {
          section_ = END_OF_FILE;
          break;
        }
        ABSL_ASSIGN_OR_RETURN(const int node, ParseInt(word));
        if (node == -1) {
          section_ = UNDEFINED_SECTION;
          break;
        }
        if (node < 1 || node > kMaxDimension || (size_ > 0 && node > size_)) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Tour node " << node << " out of bounds";
        }
        if (tour_.size() >= kMaxEdgesOrTourEntries) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Tour length exceeds maximum allowed";
        }
        tour_.push_back(node - 1);
      }
    } else {
      const std::string& last_word = words[word_size - 1];
      if (!gtl::FindCopy(*kSections, words[0], &section_)) {
        LOG(WARNING) << "Unknown section: " << words[0];
        return absl::OkStatus();
      }
      switch (section_) {
        case NAME:
          break;
        case TYPE:
          if (word_size < 2 || last_word != "TOUR") {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Expected TYPE : TOUR, got '" << line << "'";
          }
          break;
        case COMMENT: {
          comments_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
          break;
        }
        case DIMENSION: {
          if (word_size < 2) {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Missing value for DIMENSION section";
          }
          if (size_ > 0) {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Duplicate DIMENSION section";
          }
          if (!tour_.empty()) {
            return ortools::InvalidArgumentErrorBuilder()
                   << "DIMENSION section after TOUR_SECTION";
          }
          ABSL_ASSIGN_OR_RETURN(const int parsed_size, ParseInt(last_word));
          if (parsed_size <= 0 || parsed_size > kMaxDimension) {
            return ortools::InvalidArgumentErrorBuilder()
                   << "Invalid DIMENSION: " << parsed_size;
          }
          size_ = parsed_size;
          break;
        }
        case TOUR_SECTION:
          break;
        case END_OF_FILE:
          break;
        default:
          LOG(WARNING) << "Unknown key word: " << words[0];
      }
    }
  }
  return absl::OkStatus();
}

const absl::flat_hash_map<std::string, TspLibTourParser::Sections>* const
    TspLibTourParser::kSections =
        new absl::flat_hash_map<std::string, TspLibTourParser::Sections>(
            {{"NAME", NAME},
             {"TYPE", TYPE},
             {"COMMENT", COMMENT},
             {"DIMENSION", DIMENSION},
             {"TOUR_SECTION", TOUR_SECTION},
             {"EOF", END_OF_FILE}});

CVRPToursParser::CVRPToursParser() : cost_(0) {}

absl::Status CVRPToursParser::LoadFile(absl::string_view file_name) {
  tours_.clear();
  cost_ = 0;
  total_nodes_ = 0;
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ABSL_ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  LineTracker line_tracker;
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ABSL_RETURN_IF_ERROR(line_tracker.CheckLineLimits(line));
    ABSL_RETURN_IF_ERROR(ProcessNewLine(line));
  }
  return absl::OkStatus();
}

absl::Status CVRPToursParser::ProcessNewLine(absl::string_view line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  const int word_size = words.size();
  if (word_size > 0) {
    if (absl::AsciiStrToUpper(words[0]) == "COST") {
      if (word_size != 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Expected 2 tokens in Cost line, got " << word_size;
      }
      ABSL_ASSIGN_OR_RETURN(cost_, ParseInt<int64_t>(words[1]));
      return absl::OkStatus();
    }
    if (absl::AsciiStrToUpper(words[0]) == "ROUTE") {
      if (word_size <= 2) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Expected at least 3 tokens in Route line, got " << word_size;
      }
      tours_.emplace_back();
      for (int i = 2; i < word_size; ++i) {
        if (total_nodes_ >= kMaxEdgesOrTourEntries) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "CVRP tour node count exceeds maximum allowed";
        }
        ABSL_ASSIGN_OR_RETURN(const int node, ParseInt(words[i]));
        if (node < 0 || node > kMaxDimension) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "CVRP route node " << node << " out of bounds";
        }
        tours_.back().push_back(node);
        ++total_nodes_;
      }
      return absl::OkStatus();
    }
    LOG(WARNING) << "Unknown key word: " << words[0];
  }
  return absl::OkStatus();
}

}  // namespace operations_research::routing
