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
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/base/map_util.h"
#include "ortools/base/numbers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/zipfile.h"
#include "ortools/routing/parsers/simple_graph.h"
#include "ortools/util/filelineiter.h"
#include "re2/re2.h"

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
  return static_cast<int64_t>(
      kRadius * acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
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

}  // namespace

TspLibParser::TspLibParser()
    : size_(0),
      capacity_(std::numeric_limits<int64_t>::max()),
      max_distance_(std::numeric_limits<int64_t>::max()),
      distance_function_(nullptr),
      explicit_costs_(),
      depot_(0),
      section_(UNDEFINED_SECTION),
      type_(Types::UNDEFINED_TYPE),
      edge_weight_type_(UNDEFINED_EDGE_WEIGHT_TYPE),
      edge_weight_format_(UNDEFINED_EDGE_WEIGHT_FORMAT),
      edge_row_(0),
      edge_column_(0),
      to_read_(0) {}

namespace {
absl::StatusOr<File*> OpenFile(absl::string_view file_name) {
  File* file = nullptr;
  RETURN_IF_ERROR(file::Open(file_name, "r", &file, file::Defaults()));
  return file;
}
}  // namespace

absl::Status TspLibParser::LoadFile(absl::string_view file_name) {
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  valid_section_found_ = false;
  ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ProcessNewLine(line);
  }
  FinalizeEdgeWeights();
  if (!valid_section_found_) {
    return util::InvalidArgumentErrorBuilder()
           << "Could not find any valid sections in " << file_name;
  }
  return absl::OkStatus();
}

absl::StatusOr<int> TspLibParser::SizeFromFile(
    absl::string_view file_name) const {
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  int size = 0;
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    if (RE2::PartialMatch(line, "DIMENSION\\s*:\\s*(\\d+)", &size)) {
      return size;
    }
  }
  return util::InvalidArgumentErrorBuilder()
         << "Could not determine problem size from " << file_name;
}

void TspLibParser::ParseExplicitFullMatrix(
    absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size_);
  if (type_ == Types::SOP && to_read_ == size_ * size_) {
    // Matrix size is present in SOP which is redundant with dimension and must
    // not be confused with the first cell of the matrix.
    return;
  }
  for (const std::string& word : words) {
    SetExplicitCost(edge_row_, edge_column_, atoi64(word));
    ++edge_column_;
    if (edge_column_ >= size_) {
      edge_column_ = 0;
      ++edge_row_;
    }
    --to_read_;
  }
}

void TspLibParser::ParseExplicitUpperRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size_);
  for (const std::string& word : words) {
    SetExplicitCost(edge_row_, edge_column_, atoi64(word));
    SetExplicitCost(edge_column_, edge_row_, atoi64(word));
    ++edge_column_;
    if (edge_column_ >= size_) {
      ++edge_row_;
      SetExplicitCost(edge_row_, edge_row_, 0);
      edge_column_ = edge_row_ + 1;
    }
    --to_read_;
  }
}

void TspLibParser::ParseExplicitLowerRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size_);
  for (const std::string& word : words) {
    SetExplicitCost(edge_row_, edge_column_, atoi64(word));
    SetExplicitCost(edge_column_, edge_row_, atoi64(word));
    ++edge_column_;
    if (edge_column_ >= edge_row_) {
      SetExplicitCost(edge_column_, edge_column_, 0);
      edge_column_ = 0;
      ++edge_row_;
    }
    --to_read_;
  }
}

void TspLibParser::ParseExplicitUpperDiagRow(
    absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size_);
  for (const std::string& word : words) {
    SetExplicitCost(edge_row_, edge_column_, atoi64(word));
    SetExplicitCost(edge_column_, edge_row_, atoi64(word));
    ++edge_column_;
    if (edge_column_ >= size_) {
      ++edge_row_;
      edge_column_ = edge_row_;
    }
    --to_read_;
  }
}

void TspLibParser::ParseExplicitLowerDiagRow(
    absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size_);
  for (const std::string& word : words) {
    SetExplicitCost(edge_row_, edge_column_, atoi64(word));
    SetExplicitCost(edge_column_, edge_row_, atoi64(word));
    ++edge_column_;
    if (edge_column_ > edge_row_) {
      edge_column_ = 0;
      ++edge_row_;
    }
    --to_read_;
  }
}

void TspLibParser::ParseNodeCoord(absl::Span<const std::string> words) {
  CHECK_LE(3, words.size()) << words[0];
  CHECK_GE(4, words.size()) << words[4];
  const int node(atoi32(words[0]) - 1);
  coords_[node].x = strings::ParseLeadingDoubleValue(words[1].c_str(), 0);
  coords_[node].y = strings::ParseLeadingDoubleValue(words[2].c_str(), 0);
  if (4 == words.size()) {
    coords_[node].z = strings::ParseLeadingDoubleValue(words[3].c_str(), 0);
  } else {
    coords_[node].z = 0;
  }
  --to_read_;
}

void TspLibParser::SetUpEdgeWeightSection() {
  edge_row_ = 0;
  edge_column_ = 0;
  switch (edge_weight_format_) {
    case FULL_MATRIX:
      to_read_ = size_ * size_;
      break;
    case LOWER_COL:
    case UPPER_ROW:
      SetExplicitCost(0, 0, 0);
      ++edge_column_;
      to_read_ = ((size_ - 1) * size_) / 2;
      break;
    case UPPER_COL:
    case LOWER_ROW:
      SetExplicitCost(0, 0, 0);
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
      LOG(WARNING) << "Unknown EDGE_WEIGHT_FORMAT: " << edge_weight_format_;
  }
}

// Fill in the edge weight matrix.
void TspLibParser::FinalizeEdgeWeights() {
  distance_function_ = nullptr;
  if (type_ == Types::HCP) {
    VLOG(2) << "No edge weights";
    return;
  }
  VLOG(2) << "Edge weight type: " << edge_weight_type_;
  switch (edge_weight_type_) {
    case EXPLICIT:
      distance_function_ = [this](int from, int to) {
        return explicit_costs_[from * size_ + to];
      };
      break;
    case EUC_2D:
      distance_function_ = [this](int from, int to) {
        return Euc2DDistance(coords_[from], coords_[to]);
      };
      break;
    case EUC_3D:
      distance_function_ = [this](int from, int to) {
        return Euc3DDistance(coords_[from], coords_[to]);
      };
      break;
    case MAX_2D:
      distance_function_ = [this](int from, int to) {
        return Max2DDistance(coords_[from], coords_[to]);
      };
      break;
    case MAX_3D:
      distance_function_ = [this](int from, int to) {
        return Max3DDistance(coords_[from], coords_[to]);
      };
      break;
    case MAN_2D:
      distance_function_ = [this](int from, int to) {
        return Man2DDistance(coords_[from], coords_[to]);
      };
      break;
    case MAN_3D:
      distance_function_ = [this](int from, int to) {
        return Man3DDistance(coords_[from], coords_[to]);
      };
      break;
    case CEIL_2D:
      distance_function_ = [this](int from, int to) {
        return Ceil2DDistance(coords_[from], coords_[to]);
      };
      break;
    case GEO:
      distance_function_ = [this](int from, int to) {
        return GeoDistance(coords_[from], coords_[to]);
      };
      break;
    case GEOM:
      distance_function_ = [this](int from, int to) {
        return GeoMDistance(coords_[from], coords_[to]);
      };
      break;
    case ATT:
      distance_function_ = [this](int from, int to) {
        return ATTDistance(coords_[from], coords_[to]);
      };
      break;
    case XRAY1:
      LOG(WARNING) << "XRAY1 not supported for EDGE_WEIGHT_TYPE";
      break;
    case XRAY2:
      LOG(WARNING) << "XRAY2 not supported for EDGE_WEIGHT_TYPE";
      break;
    case SPECIAL:
      LOG(WARNING) << "SPECIAL not supported for EDGE_WEIGHT_TYPE";
      break;
    default:
      LOG(WARNING) << "Unknown EDGE_WEIGHT_TYPE: " << edge_weight_type_;
  }
}

bool TspLibParser::ParseSections(absl::Span<const std::string> words) {
  const int words_size = words.size();
  CHECK_GT(words_size, 0);
  if (!gtl::FindCopy(*kSections, words[0], &section_)) {
    LOG(WARNING) << "Unknown section: " << words[0];
    return false;
  }
  const std::string& last_word = words[words_size - 1];
  switch (section_) {
    case NAME: {
      name_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
      break;
    }
    case TYPE: {
      CHECK_LE(2, words.size());
      const std::string& type = words[1];
      if (!gtl::FindCopy(*kTypes, type, &type_)) {
        LOG(WARNING) << "Unknown TYPE: " << type;
      }
      break;
    }
    case COMMENT: {
      if (!comments_.empty()) {
        comments_ += "\n";
      }
      comments_ += absl::StrJoin(words.begin() + 1, words.end(), " ");
      break;
    }
    case DIMENSION: {
      size_ = atoi32(last_word);
      coords_.resize(size_);
      break;
    }
    case DISTANCE: {
      // This is non-standard but is supported as an upper bound on the length
      // of each route.
      max_distance_ = atoi64(last_word);
      break;
    }
    case CAPACITY: {
      capacity_ = atoi64(last_word);
      break;
    }
    case EDGE_DATA_FORMAT: {
      CHECK(Types::HCP == type_);
      if (!gtl::FindCopy(*kEdgeDataFormats, last_word, &edge_data_format_)) {
        LOG(WARNING) << "Unknown EDGE_DATA_FORMAT: " << last_word;
      }
      break;
    }
    case EDGE_DATA_SECTION: {
      CHECK(Types::HCP == type_);
      edges_.resize(size_);
      to_read_ = 1;
      break;
    }
    case EDGE_WEIGHT_TYPE: {
      if (!gtl::FindCopy(*kEdgeWeightTypes, last_word, &edge_weight_type_)) {
        // Some data sets invert EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
        LOG(WARNING) << "Unknown EDGE_WEIGHT_TYPE: " << last_word;
        LOG(WARNING) << "Trying in EDGE_WEIGHT_FORMAT formats";
        if (!gtl::FindCopy(*kEdgeWeightFormats, last_word,
                           &edge_weight_format_)) {
          LOG(WARNING) << "Unknown EDGE_WEIGHT_FORMAT: " << last_word;
        }
      }
      break;
    }
    case EDGE_WEIGHT_FORMAT: {
      if (!gtl::FindCopy(*kEdgeWeightFormats, last_word,
                         &edge_weight_format_)) {
        // Some data sets invert EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
        LOG(WARNING) << "Unknown EDGE_WEIGHT_FORMAT: " << last_word;
        LOG(WARNING) << "Trying in EDGE_WEIGHT_TYPE types";
        if (!gtl::FindCopy(*kEdgeWeightTypes, last_word, &edge_weight_type_)) {
          LOG(WARNING) << "Unknown EDGE_WEIGHT_TYPE: " << last_word;
        }
      }
      break;
    }
    case EDGE_WEIGHT_SECTION: {
      SetUpEdgeWeightSection();
      break;
    }
    case FIXED_EDGES_SECTION: {
      to_read_ = std::numeric_limits<int64_t>::max();
      break;
    }
    case NODE_COORD_TYPE: {
      break;
    }
    case DISPLAY_DATA_TYPE: {
      break;
    }
    case DISPLAY_DATA_SECTION: {
      to_read_ = size_;
      break;
    }
    case NODE_COORD_SECTION: {
      to_read_ = size_;
      break;
    }
    case DEPOT_SECTION: {
      to_read_ = std::numeric_limits<int64_t>::max();
      break;
    }
    case DEMAND_SECTION: {
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
  return true;
}

void TspLibParser::ProcessNewLine(const std::string& line) {
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
          CHECK(!words.empty());
          switch (edge_data_format_) {
            case EDGE_LIST: {
              if (words[0] == "-1") {
                CHECK_EQ(words.size(), 1);
                // Remove duplicate edges
                for (auto& edges : edges_) {
                  std::sort(edges.begin(), edges.end());
                  const auto it = std::unique(edges.begin(), edges.end());
                  edges.resize(std::distance(edges.begin(), it));
                }
                to_read_ = 0;
              } else {
                CHECK_EQ(words.size(), 2);
                const int from = atoi64(words[0]) - 1;
                const int to = atoi64(words[1]) - 1;
                edges_[std::min(from, to)].push_back(std::max(from, to));
              }
              break;
            }
            case ADJ_LIST: {
              const int from = atoi64(words[0]) - 1;
              for (int i = 1; i < words.size(); ++i) {
                const int to = atoi64(words[i]) - 1;
                if (to != -2) {
                  edges_[std::min(from, to)].push_back(std::max(from, to));
                } else {
                  CHECK_EQ(i, words.size() - 1);
                }
              }
              if (atoi64(words.back()) != -1) {
                LOG(WARNING) << "Missing -1 at the end of ADJ_LIST";
              }
              break;
            }
            default:
              LOG(WARNING) << "Unknown EDGE_DATA_FORMAT: " << edge_data_format_;
          }
          break;
        }
        case EDGE_WEIGHT_SECTION: {
          switch (edge_weight_format_) {
            case FULL_MATRIX:
              ParseExplicitFullMatrix(words);
              break;
            case UPPER_ROW:
            case LOWER_COL:
              ParseExplicitUpperRow(words);
              break;
            case LOWER_ROW:
            case UPPER_COL:
              ParseExplicitLowerRow(words);
              break;
            case UPPER_DIAG_ROW:
            case LOWER_DIAG_COL:
              ParseExplicitUpperDiagRow(words);
              break;
            case LOWER_DIAG_ROW:
            case UPPER_DIAG_COL:
              ParseExplicitLowerDiagRow(words);
              break;
            default:
              LOG(WARNING) << "Unknown EDGE_WEIGHT_FORMAT: "
                           << edge_weight_format_;
          }
          break;
        }
        case FIXED_EDGES_SECTION: {
          if (words.size() == 1) {
            CHECK_EQ(-1, atoi64(words[0]));
            to_read_ = 0;
          } else {
            CHECK_EQ(2, words.size());
            fixed_edges_.insert(
                std::make_pair(atoi64(words[0]) - 1, atoi64(words[1]) - 1));
          }
          break;
        }
        case NODE_COORD_SECTION: {
          ParseNodeCoord(words);
          break;
        }
        case DEPOT_SECTION: {
          if (words.size() == 1) {
            const int depot(atoi64(words[0]) - 1);
            if (depot >= 0) {
              VLOG(2) << "Depot: " << depot;
              depot_ = depot;
            } else {
              to_read_ = 0;
            }
          } else if (words.size() >= 2) {
            CHECK_GE(3, words.size()) << words[3];
            const int depot(size_ - 1);
            VLOG(2) << "Depot: " << depot;
            depot_ = depot;
            coords_[depot].x =
                strings::ParseLeadingDoubleValue(words[0].c_str(), 0);
            coords_[depot].y =
                strings::ParseLeadingDoubleValue(words[1].c_str(), 0);
            if (3 == words.size()) {
              coords_[depot].z =
                  strings::ParseLeadingDoubleValue(words[2].c_str(), 0);
            } else {
              coords_[depot].z = 0;
            }
          }
          break;
        }
        case DEMAND_SECTION: {
          const int64_t node = atoi64(words[0]) - 1;
          demands_[node] = atoi64(words[1]);
          --to_read_;
          break;
        }
        case DISPLAY_DATA_SECTION: {
          ParseNodeCoord(words);
          break;
        }
        default: {
          LOG(ERROR) << "Reading data outside section";
        }
      }
    } else {
      // TODO(user): Check that proper sections were read (necessary and
      // non-overlapping ones).
      valid_section_found_ |= ParseSections(words);
    }
  }
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
  comments_.clear();
  tour_.clear();
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ProcessNewLine(line);
  }
  return absl::OkStatus();
}

void TspLibTourParser::ProcessNewLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  const int word_size = words.size();
  if (word_size > 0) {
    if (section_ == TOUR_SECTION) {
      for (const std::string& word : words) {
        const int node = atoi32(word);
        if (node >= 0) {
          tour_.push_back(atoi32(word) - 1);
        } else {
          section_ = UNDEFINED_SECTION;
        }
      }
    } else {
      const std::string& last_word = words[word_size - 1];
      if (!gtl::FindCopy(*kSections, words[0], &section_)) {
        LOG(WARNING) << "Unknown section: " << words[0];
        return;
      }
      switch (section_) {
        case NAME:
          break;
        case TYPE:
          CHECK_EQ("TOUR", last_word);
          break;
        case COMMENT: {
          comments_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
          break;
        }
        case DIMENSION:
          size_ = atoi32(last_word);
          break;
        case TOUR_SECTION:
          break;
        case END_OF_FILE:
          break;
        default:
          LOG(WARNING) << "Unknown key word: " << words[0];
      }
    }
  }
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
  std::shared_ptr<zipfile::ZipArchive> zip_archive(
      OpenZipArchiveIfItExists(file_name));
  ASSIGN_OR_RETURN(File* const file, OpenFile(file_name));
  for (const std::string& line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    ProcessNewLine(line);
  }
  return absl::OkStatus();
}

void CVRPToursParser::ProcessNewLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  const int word_size = words.size();
  if (word_size > 0) {
    if (absl::AsciiStrToUpper(words[0]) == "COST") {
      CHECK_EQ(word_size, 2);
      cost_ = atoi32(words[1]);
      return;
    }
    if (absl::AsciiStrToUpper(words[0]) == "ROUTE") {
      CHECK_GT(word_size, 2);
      tours_.resize(tours_.size() + 1);
      for (int i = 2; i < word_size; ++i) {
        tours_.back().push_back(atoi32(words[i]));
      }
      return;
    }
    LOG(WARNING) << "Unknown key word: " << words[0];
  }
}

}  // namespace operations_research::routing
