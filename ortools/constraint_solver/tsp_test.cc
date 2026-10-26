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

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/parsers/simple_graph.h"

ABSL_FLAG(int64_t, time_limit, kint64max, "time limit");
ABSL_FLAG(bool, trace_tsp_search, false, "trace search when solving a TSP");
ABSL_FLAG(int64_t, start, 0, "start index");
ABSL_FLAG(bool, use_cumul_model, false,
          "use cumul model instead of rank model");
ABSL_FLAG(bool, big_models, false, "solve big models too");
ABSL_FLAG(int64_t, ls_best_of, 1,
          "local search will take the best of n neighbors");
ABSL_FLAG(std::string, data, "", "TSPLIB data set in archive");
ABSL_FLAG(std::string, data_archive, "", "TSPLIB archive (tar.gz)");
ABSL_FLAG(int64_t, move_limit, 10,
          "Maximum number of moves performed by local search");

namespace operations_research {
namespace {

// ---------- Code Section ----------

// TODO(user): move TSPLIB loader to a separate lib

int64_t ParseInt(absl::string_view word) {
  int64_t value;
  CHECK(absl::SimpleAtoi(word, &value));
  return value;
}

typedef int64_t (*Distance)(const routing::Coordinates3<double>& i,
                            const routing::Coordinates3<double>& j);

int64_t ATTDistance(const routing::Coordinates3<double>& i,
                    const routing::Coordinates3<double>& j) {
  const double xd = i.x - j.x;
  const double yd = i.y - j.y;
  const double rij = sqrt((xd * xd + yd * yd) / 10.0);
  int64_t tij = std::llround(rij);
  if (tij < rij) ++tij;
  return tij;
}

double DoubleEuc2DDistance(const routing::Coordinates3<double>& i,
                           const routing::Coordinates3<double>& j) {
  const double xd = i.x - j.x;
  const double yd = i.y - j.y;
  return sqrt(xd * xd + yd * yd);
}

int64_t Euc2DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  return std::llround(DoubleEuc2DDistance(i, j));
}

int64_t Euc3DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  const double xd = i.x - j.x;
  const double yd = i.y - j.y;
  const double zd = i.z - j.z;
  const double rij = sqrt(xd * xd + yd * yd + zd * zd);
  return std::llround(rij);
}

int64_t Ceil2DDistance(const routing::Coordinates3<double>& i,
                       const routing::Coordinates3<double>& j) {
  return static_cast<int64_t>(ceil(DoubleEuc2DDistance(i, j)));
}

double ToRad(double x) {
  const double tsplib_pi = 3.141592;
  const int64_t deg = std::llround(x);
  const double min = x - deg;
  return tsplib_pi * (deg + 5.0 * min / 3.0) / 180.0;
}

int64_t GeoDistance(const routing::Coordinates3<double>& i,
                    const routing::Coordinates3<double>& j) {
  const double tsplib_radius = 6378.388;
  const double q1 = cos(ToRad(i.x) - ToRad(j.x));
  const double q2 = cos(ToRad(i.y) - ToRad(j.y));
  const double q3 = cos(ToRad(i.y) + ToRad(j.y));
  return static_cast<int64_t>(
      tsplib_radius * acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
}

int64_t Man2DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  const double xd = std::abs(i.x - j.x);
  const double yd = std::abs(i.y - j.y);
  return std::llround(xd + yd);
}

int64_t Man3DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  const double xd = std::abs(i.x - j.x);
  const double yd = std::abs(i.y - j.y);
  const double zd = std::abs(i.z - j.z);
  return std::llround(xd + yd + zd);
}

int64_t Max2DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  const int64_t xd = std::llround(std::abs(i.x - j.x));
  const int64_t yd = std::llround(std::abs(i.y - j.y));
  if (yd > xd) {
    return yd;
  }
  return xd;
}

int64_t Max3DDistance(const routing::Coordinates3<double>& i,
                      const routing::Coordinates3<double>& j) {
  const int64_t xd = std::llround(std::abs(i.x - j.x));
  const int64_t yd = std::llround(std::abs(i.y - j.y));
  const int64_t zd = std::llround(std::abs(i.z - j.z));
  int64_t dij = xd;
  if (yd > dij) {
    dij = yd;
  }
  if (zd > dij) {
    dij = zd;
  }
  return dij;
}

class TSPTest : public testing::Test {
 protected:
  enum Section {
    NAME,
    TYPE,
    COMMENT,
    DIMENSION,
    EDGE_WEIGHT_TYPE,
    EDGE_WEIGHT_FORMAT,
    EDGE_WEIGHT_SECTION,
    DISPLAY_DATA_SECTION,
    NODE_COORD_SECTION,
    END_OF_FILE
  };
  enum Types { TSP, ATSP, SOP, HCP, CVRP, TOUR, UNDEFINED_TYPE };
  enum EdgeWeightTypes {
    EXPLICIT,
    EUC_2D,
    EUC_3D,
    MAX_2D,
    MAX_3D,
    MAN_2D,
    MAN_3D,
    CEIL_2D,
    GEO,
    ATT,
    XRAY1,
    XRAY2,
    SPECIAL,
    UNDEFINED_EDGE_WEIGHT_TYPE
  };
  enum EdgeWeightFormats {
    FUNCTION,
    FULL_MATRIX,
    UPPER_ROW,
    LOWER_ROW,
    UPPER_DIAG_ROW,
    LOWER_DIAG_ROW,
    UPPER_COL,
    LOWER_COL,
    UPPER_DIAG_COL,
    LOWER_DIAG_COL,
    UNDEFINED_EDGE_WEIGHT_FORMAT
  };

  TSPTest()
      : size_(0),
        type_(UNDEFINED_TYPE),
        edge_weight_type_(UNDEFINED_EDGE_WEIGHT_TYPE),
        edge_weight_format_(UNDEFINED_EDGE_WEIGHT_FORMAT),
        edge_weight_section_to_read_(0),
        edge_row_(0),
        edge_column_(0),
        display_data_section_to_read_(0),
        node_coord_to_read_(0) {}
  ~TSPTest() override;
  void SetUp() override;

  // Parsing
  void ParseExplicitFullMatrix(absl::Span<const std::string> words);
  void ParseExplicitUpperRow(absl::Span<const std::string> words);
  void ParseExplicitLowerRow(absl::Span<const std::string> words);
  void ParseExplicitUpperDiagRow(absl::Span<const std::string> words);
  void ParseExplicitLowerDiagRow(absl::Span<const std::string> words);
  void ParseNodeCoord(absl::Span<const std::string> words);
  void ProcessNewLine(absl::string_view line);
  virtual void SetUpData(const char* file_name, const char* data = nullptr);

  // Modeling
  void SetUpModel(const char* file_name, const char* data);
  int64_t Solve(DecisionBuilder* first_solution,
                DecisionBuilder* subgoal = nullptr);
  int64_t size() const { return size_; }

  std::unique_ptr<Solver> solver_;
  std::vector<std::vector<int64_t>> arc_weights_;
  std::vector<IntVar*> nexts_;
  IntVar* cost_;
  int64_t size_;

 private:
  std::map<std::string, Section> section_;
  std::map<std::string, Types> types_;
  Types type_;
  std::map<std::string, EdgeWeightTypes> edge_weight_types_;
  EdgeWeightTypes edge_weight_type_;
  std::map<std::string, EdgeWeightFormats> edge_weight_formats_;
  EdgeWeightFormats edge_weight_format_;
  int edge_weight_section_to_read_;
  int edge_row_;
  int edge_column_;
  int display_data_section_to_read_;
  std::vector<routing::Coordinates3<double>> coords_;
  int node_coord_to_read_;
};

TSPTest::~TSPTest() {}

void TSPTest::SetUp() {
  solver_ = std::make_unique<Solver>("TSPTest");
  section_["NAME"] = NAME;
  section_["TYPE"] = TYPE;
  section_["COMMENT"] = COMMENT;
  section_["DIMENSION"] = DIMENSION;
  section_["EDGE_WEIGHT_TYPE"] = EDGE_WEIGHT_TYPE;
  section_["EDGE_WEIGHT_FORMAT"] = EDGE_WEIGHT_FORMAT;
  section_["EDGE_WEIGHT_SECTION"] = EDGE_WEIGHT_SECTION;
  section_["DISPLAY_DATA_SECTION"] = DISPLAY_DATA_SECTION;
  section_["NODE_COORD_SECTION"] = NODE_COORD_SECTION;
  section_["EOF"] = END_OF_FILE;
  types_["TSP"] = TSP;
  types_["ATSP"] = ATSP;
  types_["SOP"] = SOP;
  types_["HCP"] = HCP;
  types_["CVRP"] = CVRP;
  types_["TOUR"] = TOUR;
  edge_weight_types_["EXPLICIT"] = EXPLICIT;
  edge_weight_types_["EUC_2D"] = EUC_2D;
  edge_weight_types_["EUC_3D"] = EUC_3D;
  edge_weight_types_["MAX_2D"] = MAX_2D;
  edge_weight_types_["MAX_3D"] = MAX_3D;
  edge_weight_types_["MAN_2D"] = MAN_2D;
  edge_weight_types_["MAN_3D"] = MAN_3D;
  edge_weight_types_["CEIL_2D"] = CEIL_2D;
  edge_weight_types_["GEO"] = GEO;
  edge_weight_types_["ATT"] = ATT;
  edge_weight_types_["XRAY1"] = XRAY1;
  edge_weight_types_["XRAY2"] = XRAY2;
  edge_weight_types_["SPECIAL"] = SPECIAL;
  edge_weight_formats_["FUNCTION"] = FUNCTION;
  edge_weight_formats_["FULL_MATRIX"] = FULL_MATRIX;
  edge_weight_formats_["UPPER_ROW"] = UPPER_ROW;
  edge_weight_formats_["LOWER_ROW"] = LOWER_ROW;
  edge_weight_formats_["UPPER_DIAG_ROW"] = UPPER_DIAG_ROW;
  edge_weight_formats_["LOWER_DIAG_ROW"] = LOWER_DIAG_ROW;
  edge_weight_formats_["UPPER_COL"] = UPPER_COL;
  edge_weight_formats_["LOWER_COL"] = LOWER_COL;
  edge_weight_formats_["UPPER_DIAG_COL"] = UPPER_DIAG_COL;
  edge_weight_formats_["LOWER_DIAG_COL"] = LOWER_DIAG_COL;
}

void TSPTest::ParseExplicitFullMatrix(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size());
  const int word_size = words.size();
  for (int i = 0; i < word_size; ++i) {
    arc_weights_[edge_row_][edge_column_] = ParseInt(words[i]);
    ++edge_column_;
    if (edge_column_ >= size()) {
      edge_column_ = 0;
      ++edge_row_;
    }
    --edge_weight_section_to_read_;
  }
}

void TSPTest::ParseExplicitUpperRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size());
  const int word_size = words.size();
  for (int i = 0; i < word_size; ++i) {
    const int64_t weight = ParseInt(words[i]);
    arc_weights_[edge_row_][edge_column_] = weight;
    arc_weights_[edge_column_][edge_row_] = weight;
    ++edge_column_;
    if (edge_column_ >= size()) {
      ++edge_row_;
      arc_weights_[edge_row_][edge_row_] = 0;
      edge_column_ = edge_row_ + 1;
    }
    --edge_weight_section_to_read_;
  }
}

void TSPTest::ParseExplicitLowerRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size());
  const int word_size = words.size();
  for (int i = 0; i < word_size; ++i) {
    const int64_t weight = ParseInt(words[i]);
    arc_weights_[edge_row_][edge_column_] = weight;
    arc_weights_[edge_column_][edge_row_] = weight;
    ++edge_column_;
    if (edge_column_ >= edge_row_) {
      arc_weights_[edge_column_][edge_column_] = 0;
      edge_column_ = 0;
      ++edge_row_;
    }
    --edge_weight_section_to_read_;
  }
}

void TSPTest::ParseExplicitUpperDiagRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size());
  const int word_size = words.size();
  for (int i = 0; i < word_size; ++i) {
    const int64_t weight = ParseInt(words[i]);
    arc_weights_[edge_row_][edge_column_] = weight;
    arc_weights_[edge_column_][edge_row_] = weight;
    ++edge_column_;
    if (edge_column_ >= size()) {
      ++edge_row_;
      edge_column_ = edge_row_;
    }
    --edge_weight_section_to_read_;
  }
}

void TSPTest::ParseExplicitLowerDiagRow(absl::Span<const std::string> words) {
  CHECK_LT(edge_row_, size());
  const int word_size = words.size();
  for (int i = 0; i < word_size; ++i) {
    const int64_t weight = ParseInt(words[i]);
    arc_weights_[edge_row_][edge_column_] = weight;
    arc_weights_[edge_column_][edge_row_] = weight;
    ++edge_column_;
    if (edge_column_ > edge_row_) {
      edge_column_ = 0;
      ++edge_row_;
    }
    --edge_weight_section_to_read_;
  }
}

void TSPTest::ParseNodeCoord(absl::Span<const std::string> words) {
  CHECK_LE(3, words.size());
  CHECK_GE(4, words.size());
  const int node = ParseInt(words[0]) - 1;
  coords_[node].x = strtod(words[1].c_str(), nullptr);
  coords_[node].y = strtod(words[2].c_str(), nullptr);
  if (4 == words.size()) {
    coords_[node].z = strtod(words[3].c_str(), nullptr);
  } else {
    coords_[node].z = 0;
  }
  --node_coord_to_read_;
}

void TSPTest::ProcessNewLine(absl::string_view line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :"), absl::SkipEmpty());
  const int words_size = words.size();
  if (words_size > 0) {
    if (edge_weight_section_to_read_ > 0) {
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
          LOG(INFO) << "Unknown EDGE_WEIGHT_FORMAT: " << edge_weight_format_;
      }
    } else if (node_coord_to_read_ > 0) {
      ParseNodeCoord(words);
    } else if (display_data_section_to_read_ > 0) {
      --display_data_section_to_read_;
    } else {
      const std::string& last_word = words[words_size - 1];
      switch (section_[words[0]]) {
        case NAME:
          break;
        case TYPE:
          type_ = types_[last_word];
          if (type_ != TSP) {
            LOG(INFO) << "Only supported type is TSP";
          }
          break;
        case COMMENT:
          break;
        case DIMENSION:
          size_ = ParseInt(last_word);
          arc_weights_.assign(size(), std::vector<int64_t>(size() + 1, 0));
          coords_.resize(size_);
          break;
        case EDGE_WEIGHT_TYPE:
          edge_weight_type_ = edge_weight_types_[last_word];
          break;
        case EDGE_WEIGHT_FORMAT:
          edge_weight_format_ = edge_weight_formats_[last_word];
          break;
        case EDGE_WEIGHT_SECTION:
          edge_row_ = 0;
          edge_column_ = 0;
          switch (edge_weight_format_) {
            case FULL_MATRIX:
              edge_weight_section_to_read_ = size() * size();
              break;
            case UPPER_ROW:
              arc_weights_[0][0] = 0;
              ++edge_column_;
              edge_weight_section_to_read_ = ((size() - 1) * size()) / 2;
              break;
            case LOWER_ROW:
              arc_weights_[0][0] = 0;
              ++edge_row_;
              edge_weight_section_to_read_ = ((size() - 1) * size()) / 2;
              break;
            case UPPER_DIAG_ROW:
              edge_weight_section_to_read_ = ((size() + 1) * size()) / 2;
              break;
            case LOWER_DIAG_ROW:
              edge_weight_section_to_read_ = ((size() + 1) * size()) / 2;
              break;
            default:
              LOG(INFO) << "Unknown EDGE_WEIGHT_FORMAT: "
                        << edge_weight_format_;
          }
          break;
        case DISPLAY_DATA_SECTION:
          display_data_section_to_read_ = size();
          break;
        case NODE_COORD_SECTION:
          node_coord_to_read_ = size();
          break;
        case END_OF_FILE: {
          Distance f = nullptr;
          switch (edge_weight_type_) {
            case EXPLICIT:
              break;
            case EUC_2D:
              f = Euc2DDistance;
              break;
            case EUC_3D:
              f = Euc3DDistance;
              break;
            case MAX_2D:
              f = Max2DDistance;
              break;
            case MAX_3D:
              f = Max3DDistance;
              break;
            case MAN_2D:
              f = Man2DDistance;
              break;
            case MAN_3D:
              f = Man3DDistance;
              break;
            case CEIL_2D:
              f = Ceil2DDistance;
              break;
            case GEO:
              f = GeoDistance;
              break;
            case ATT:
              f = ATTDistance;
              break;
            case XRAY1:
              LOG(INFO) << "XRAY1 not supported for EDGE_WEIGHT_TYPE";
              break;
            case XRAY2:
              LOG(INFO) << "XRAY2 not supported for EDGE_WEIGHT_TYPE";
              break;
            case SPECIAL:
              LOG(INFO) << "SPECIAL not supported for EDGE_WEIGHT_TYPE";
              break;
            default:
              LOG(INFO) << "Unknown EDGE_WEIGHT_TYPE: " << edge_weight_type_;
          }
          if (nullptr != f) {
            for (int i = 0; i < size(); ++i) {
              for (int j = i; j < size(); ++j) {
                int64_t dij = f(coords_[i], coords_[j]);
                arc_weights_[i][j] = dij;
                arc_weights_[j][i] = dij;
              }
            }
          }
          break;
        }
        default:
          LOG(INFO) << "Unknown section: " << words[0];
      }
    }
  }
}

void TSPTest::SetUpData(const char* file_name, const char* data) {
  if (data != nullptr) {
    for (absl::string_view line : absl::StrSplit(data, '\n')) {
      ProcessNewLine(line);
    }
  } else if (file_name != nullptr) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
      LOG(ERROR) << "Could not open file: " << file_name;
      return;
    }
    std::string line;
    while (std::getline(file, line)) {
      ProcessNewLine(line);
    }
  }

  // Filling up for the "last" node (cf SetUpModel())
  for (int i = 0; i < size(); ++i) {
    arc_weights_[i][size()] = arc_weights_[i][absl::GetFlag(FLAGS_start)];
  }
}

void TSPTest::SetUpModel(const char* file_name, const char* data) {
  SetUpData(file_name, data);

  std::string next_name("Cities");
  const int start = absl::GetFlag(FLAGS_start);
  CHECK(start < size());
  LOG(INFO) << "Start index: " << start;

  // Next variables: nexts_[i] represents the node immediately after node i.
  // The "start" node is an arbitrary node representing the start of the tour;
  // this is modelled by adding nexts_[i] != start for all i.
  // The "last" node is an arbitrary node located at the same place as the
  // "start" node; this is modelled by using a node index outside the range
  // of the nexts_ vector (here size()).
  solver_->MakeIntVarArray(size(), 0, size(), next_name, &nexts_);
  solver_->AddConstraint(solver_->MakeAllDifferent(nexts_, false));
  for (int i = 0; i < size(); ++i) {
    // "start" node: nexts_[i] != start
    solver_->AddConstraint(solver_->MakeNonEquality(nexts_[i], start));
    // Extra constraint to state a node can't point to itself
    solver_->AddConstraint(solver_->MakeNonEquality(nexts_[i], i));
  }

  // Arc costs: the cost of an arc (i, nexts_[i]) is arc_weights_(nexts_[i]);
  // the total cost is the sum of arc costs.
  std::vector<IntVar*> arc_costs;
  for (int i = 0; i < size(); ++i) {
    IntVar* expr = solver_->MakeElement(arc_weights_[i], nexts_[i])->Var();
    arc_costs.push_back(expr);
  }
  cost_ = solver_->MakeSum(arc_costs)->Var();

  if (!absl::GetFlag(FLAGS_use_cumul_model)) {
    // A rank is the position of node in a path (rank of "start" is 0). Rank
    // variables are accumulated along the path such that:
    // ranks[nexts_[i]] >= ranks[i] + 1
    // Rank variables can avoid sub-cycles in the path by constraining the
    // maximum value of the ranks.

    LOG(INFO) << "Using rank model";
    std::vector<IntVar*> ranks;
    solver_->MakeIntVarArray(size() + 1, 0, size(),  // max length of tour
                             "Rank", &ranks);
    for (int i = 0; i < size(); ++i) {
      IntVar* rank_plus_one = solver_->MakeSum(ranks[i], int64_t{1})->Var();
      IntVar* elem = solver_->MakeElement(ranks, nexts_[i])->Var();
      // rank[next[i]] == rank[i] + 1;
      solver_->AddConstraint(solver_->MakeEquality(elem, rank_plus_one));
    }
    // Rank of "start" node is 0
    solver_->AddConstraint(solver_->MakeEquality(
        reinterpret_cast<IntExpr*>(ranks[start]), int64_t{0}));
  } else {
    // Cumuls are the accumulated value of cost at each node such that:
    // cumuls[nexts_[i]] >= cumuls[i] + arc_cost[i]
    // Cumul variables can avoid sub-cycles in the path by constraining the
    // maximum value of the cumuls.

    LOG(INFO) << "Using cumul model";
    std::vector<IntVar*> cumuls;
    solver_->MakeIntVarArray(size() + 1, 0,
                             100000,  // arbitrary big number for upper bound
                             "Cumul", &cumuls);
    for (int i = 0; i < size() + 1; ++i) {
      IntVar* cumul = cumuls[i];
      for (int j = 0; j < size(); ++j) {
        IntVar* next_j_is_i =
            solver_->MakeIntVar(int64_t{0}, int64_t{1}, "nextji");
        solver_->AddConstraint(
            solver_->MakeIsEqualCstCt(nexts_[j], i, next_j_is_i));
        IntVar* arc_cumul = solver_->MakeSum(cumuls[j], arc_costs[j])->Var();
        IntVar* real_arc_cumul =
            solver_->MakeProd(next_j_is_i, arc_cumul)->Var();
        solver_->AddConstraint(
            solver_->MakeGreaterOrEqual(cumul, real_arc_cumul));
      }
    }
    // Cumul at "start" node is 0
    solver_->AddConstraint(solver_->MakeEquality(
        reinterpret_cast<IntExpr*>(cumuls[start]), int64_t{0}));
  }
}

int64_t TSPTest::Solve(DecisionBuilder* first_solution,
                       DecisionBuilder* subgoal) {
  std::unique_ptr<Assignment> assignment(new Assignment(solver_.get()));
  assignment->Add(nexts_);
  assignment->AddObjective(cost_);

  std::vector<SearchMonitor*> monitors;
  SolutionCollector* collect_assignments =
      solver_->MakeLastSolutionCollector(assignment.get());
  monitors.push_back(collect_assignments);

  OptimizeVar* optimize = solver_->MakeMinimize(cost_, 1);
  monitors.push_back(optimize);

  SearchLimit* limit =
      solver_->MakeLimit(absl::Milliseconds(absl::GetFlag(FLAGS_time_limit)),
                         kint64max, kint64max, absl::GetFlag(FLAGS_move_limit));
  monitors.push_back(limit);

  RegularLimit* ls_limit =
      solver_->MakeLimit(absl::Milliseconds(absl::GetFlag(FLAGS_time_limit)),
                         kint64max, kint64max, absl::GetFlag(FLAGS_ls_best_of));

  std::vector<LocalSearchOperator*> operators;
  if (nullptr == subgoal) {
    operators.push_back(solver_->MakeOperator(nexts_, Solver::TWOOPT));
    operators.push_back(solver_->MakeOperator(nexts_, Solver::OROPT));
    operators.push_back(solver_->MakeOperator(nexts_, Solver::EXCHANGE));
  } else {
    operators.push_back(solver_->MakeOperator(nexts_, Solver::PATHLNS));
    operators.push_back(solver_->MakeOperator(nexts_, Solver::TWOOPT));
    operators.push_back(solver_->MakeOperator(nexts_, Solver::OROPT));
  }

  LocalSearchOperator* local_search_operator =
      solver_->ConcatenateOperators(operators);

  LocalSearchPhaseParameters* ls_params =
      solver_->MakeLocalSearchPhaseParameters(cost_, local_search_operator,
                                              subgoal, ls_limit);
  DecisionBuilder* db =
      solver_->MakeLocalSearchPhase(nexts_, first_solution, ls_params);

  if (absl::GetFlag(FLAGS_trace_tsp_search)) {
    monitors.push_back(solver_->MakeSearchLog(1, cost_));
  }
  solver_->Solve(db, monitors);
  EXPECT_EQ(1, collect_assignments->solution_count());

  std::string tour = "{";
  int node = 0;
  for (int i = 0; i < size(); ++i) {
    tour = absl::StrFormat("%s%d,", tour, node + 1);
    node = collect_assignments->solution(0)->Value(nexts_[node]);
    if (node == size()) node = absl::GetFlag(FLAGS_start);
  }
  tour = tour + "-1}";
  LOG(INFO) << tour;

  return collect_assignments->solution(0)->ObjectiveValue();
}

// ----- Decision builder which restores the best know solution -----

class MakeOptimalTour : public DecisionBuilder {
 public:
  MakeOptimalTour(const std::vector<IntVar*>& vars, const char* file_name,
                  const char* data)
      : vars_(vars), file_name_(file_name), data_(data), last_node_(-1) {}
  ~MakeOptimalTour() override {}
  void ProcessNewLine(absl::string_view line) {
    const std::vector<std::string> words =
        absl::StrSplit(line, ' ', absl::SkipEmpty());
    const int word_size = words.size();
    if (word_size > 0) {
      if (last_node_ >= 0) {
        int64_t next_node = ParseInt(words[0]) - 1;
        const bool end_of_tour = next_node < 0;
        if (0 != next_node) {  // Not the first node in TSPLIB tour
          if (end_of_tour) next_node = 0;
          if (next_node == absl::GetFlag(FLAGS_start)) {
            vars_[last_node_]->SetValue(vars_.size());
          } else {
            vars_[last_node_]->SetValue(next_node);
          }
        }
        if (end_of_tour) {
          last_node_ = -1;
        } else {
          last_node_ = next_node;
        }
      } else {
        const std::string& last_word = words[word_size - 1];
        switch (section_[words[0]]) {
          case COMMENT:
            break;
          case TYPE:
            CHECK_EQ("TOUR", last_word);
            break;
          case DIMENSION:
            CHECK_EQ(ParseInt(last_word), vars_.size());
            break;
          case TOUR_SECTION:
            last_node_ = 0;
            break;
          case END_OF_FILE:
            break;
          default:
            LOG(INFO) << "Unknown key word: " << words[0];
        }
      }
    }
  }
  Decision* Next(Solver* const solver) override {
    section_["COMMENT"] = COMMENT;
    section_["TYPE"] = TYPE;
    section_["DIMENSION"] = DIMENSION;
    section_["TOUR_SECTION"] = TOUR_SECTION;
    section_["EOF"] = END_OF_FILE;

    last_node_ = -1;

    if (data_ != nullptr) {
      for (absl::string_view line : absl::StrSplit(data_, '\n')) {
        ProcessNewLine(line);
      }
    } else if (file_name_ != nullptr) {
      std::ifstream file(file_name_);
      if (!file.is_open()) {
        LOG(ERROR) << "Could not open file: " << file_name_;
        return nullptr;
      }
      std::string line;
      while (std::getline(file, line)) {
        ProcessNewLine(line);
      }
    }
    return nullptr;
  }
  virtual void Display(std::ostream& out) const {  // NOLINT
    out << "SetAllToValue";
  }

 private:
  enum Section { COMMENT, TYPE, DIMENSION, TOUR_SECTION, END_OF_FILE };

  std::map<std::string, Section> section_;
  const std::vector<IntVar*>& vars_;
  const char* const file_name_;
  const char* const data_;
  int last_node_;
};
}  // namespace
}  // namespace operations_research
