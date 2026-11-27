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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/temp_file.h"
#include "ortools/base/zipfile.h"
#include "ortools/routing/parsers/simple_graph.h"
#include "ortools/util/data_path_resolver.h"

namespace operations_research::routing {
namespace {

using ::testing::ElementsAre;
using ::testing::status::StatusIs;

std::string WriteTempTspFile(absl::string_view contents) {
  std::string file_name;
  [&]() {
    ASSERT_OK_AND_ASSIGN(
        file_name,
        file::MakeTempFilename(::testing::TempDir(), "test_instance.tsp"));
    ASSERT_OK(file::SetContents(file_name, contents, file::Defaults()));
  }();
  return file_name;
}

TEST(TspLibParserTest, GeneratedDataSets) {
  static const char kName[] = "GoogleTest";
  static const char* const kTypes[] = {"TSP", "CVRP"};
  static const char kComment[] = "This is a test";
  static const int kDimension = 4;
  static const int kCoordSize = 2;
  static const int kCapacity = 2;
  static const char* const kEdgeWeightTypes[] = {
      "EXPLICIT", "EUC_2D", "EUC_3D",  "MAX_2D", "MAX_3D",
      "MAN_2D",   "MAN_3D", "CEIL_2D", "GEO",    "ATT"};
  static const char* const kEdgeWeightFormats[] = {
      "FULL_MATRIX",    "UPPER_ROW",      "LOWER_ROW",
      "UPPER_DIAG_ROW", "LOWER_DIAG_ROW", "UPPER_COL",
      "LOWER_COL",      "UPPER_DIAG_COL", "LOWER_DIAG_COL"};
  static const char* const kNodeCoordTypes[] = {"TWOD_COORDS", "THREED_COORDS",
                                                "NO_COORDS"};
  static const char* const kDisplayDataTypes[] = {"COORD_DISPLAY",
                                                  "TWOD_DISPLAY", "NO_DISPLAY"};
  for (int type = 0; type < ABSL_ARRAYSIZE(kTypes); ++type) {
    for (int edge_type = 0; edge_type < ABSL_ARRAYSIZE(kEdgeWeightTypes);
         ++edge_type) {
      for (int edge_format = 0;
           edge_format < ABSL_ARRAYSIZE(kEdgeWeightFormats); ++edge_format) {
        for (int node_type = 0; node_type < ABSL_ARRAYSIZE(kNodeCoordTypes);
             ++node_type) {
          if (node_type == 2 && edge_type != 0) break;
          if (node_type == 1 && edge_type != 2 && edge_type != 4 &&
              edge_type != 6)
            break;
          if (node_type == 0 && edge_type != 1 && edge_type != 3 &&
              edge_type != 5 && edge_type < 7)
            break;
          for (int display_type = 0;
               display_type < ABSL_ARRAYSIZE(kDisplayDataTypes);
               ++display_type) {
            if (display_type == 0 && node_type == 2) break;
            std::string data = absl::StrFormat("NAME: %s\n", kName);
            absl::StrAppendFormat(&data, "TYPE: %s\n", kTypes[type]);
            absl::StrAppendFormat(&data, "COMMENT: %s\n", kComment);
            absl::StrAppendFormat(&data, "DIMENSION: %d\n", kDimension);
            if (type == 1) {
              absl::StrAppendFormat(&data, "CAPACITY: %d\n", kCapacity);
            }
            absl::StrAppendFormat(&data, "EDGE_WEIGHT_TYPE: %s\n",
                                  kEdgeWeightTypes[edge_type]);
            if (edge_type == 0) {
              absl::StrAppendFormat(&data, "EDGE_WEIGHT_FORMAT: %s\n",
                                    kEdgeWeightFormats[edge_format]);
            }
            absl::StrAppendFormat(&data, "NODE_COORD_TYPE: %s\n",
                                  kNodeCoordTypes[node_type]);
            absl::StrAppendFormat(&data, "DISPLAY_DATA_TYPE: %s\n",
                                  kDisplayDataTypes[display_type]);
            if (node_type != 2) {
              data += "NODE_COORD_SECTION\n";
              for (int i = 0; i < kDimension; ++i) {
                absl::StrAppendFormat(&data, "%d %d %d", i + 1, i % kCoordSize,
                                      i / kCoordSize);
                if (node_type == 1) {
                  data += " 0";
                }
                data += '\n';
              }
            }
            if (type == 1) {
              data += "DEPOT_SECTION\n1\n-1\n";
              data += "DEMAND_SECTION\n";
              for (int i = 0; i < kDimension; ++i) {
                absl::StrAppendFormat(&data, "%d %d\n", i + 1, 1);
              }
            }
            if (display_type == 1) {
              data += "DISPLAY_DATA_SECTION\n";
              for (int i = 0; i < kDimension; ++i) {
                absl::StrAppendFormat(&data, "%d %d %d\n", i + 1,
                                      i % kCoordSize, i / kCoordSize);
              }
            }
            if (edge_type == 0) {
              data += "EDGE_WEIGHT_SECTION\n";
              // Manhattan distances
              switch (edge_format) {
                case 0:
                  for (int i = 0; i < kDimension; ++i) {
                    const int x = i % kCoordSize;
                    const int y = i / kCoordSize;
                    for (int j = 0; j < kDimension; ++j) {
                      const int distance =
                          abs(x - (j % kCoordSize)) + abs(y - (j / kCoordSize));
                      absl::StrAppendFormat(&data, "%d ", distance);
                    }
                    data += '\n';
                  }
                  break;
                case 1:
                case 6:
                  for (int i = 0; i < kDimension; ++i) {
                    const int x = i % kCoordSize;
                    const int y = i / kCoordSize;
                    for (int j = i + 1; j < kDimension; ++j) {
                      const int distance =
                          abs(x - (j % kCoordSize)) + abs(y - (j / kCoordSize));
                      absl::StrAppendFormat(&data, "%d ", distance);
                    }
                    data += '\n';
                  }
                  break;
                case 2:
                case 5:
                  for (int i = 0; i < kDimension; ++i) {
                    const int x = i % kCoordSize;
                    const int y = i / kCoordSize;
                    for (int j = 0; j < i; ++j) {
                      const int distance =
                          abs(x - (j % kCoordSize)) + abs(y - (j / kCoordSize));
                      absl::StrAppendFormat(&data, "%d ", distance);
                    }
                    data += '\n';
                  }
                  break;
                case 3:
                case 8:
                  for (int i = 0; i < kDimension; ++i) {
                    const int x = i % kCoordSize;
                    const int y = i / kCoordSize;
                    for (int j = i; j < kDimension; ++j) {
                      const int distance =
                          abs(x - (j % kCoordSize)) + abs(y - (j / kCoordSize));
                      absl::StrAppendFormat(&data, "%d ", distance);
                    }
                    data += '\n';
                  }
                  break;
                case 4:
                case 7:
                  for (int i = 0; i < kDimension; ++i) {
                    const int x = i % kCoordSize;
                    const int y = i / kCoordSize;
                    for (int j = 0; j <= i; ++j) {
                      const int distance =
                          abs(x - (j % kCoordSize)) + abs(y - (j / kCoordSize));
                      absl::StrAppendFormat(&data, "%d ", distance);
                    }
                    data += '\n';
                  }
                  break;
              }
            }
            data += "EOF";
            ASSERT_OK_AND_ASSIGN(
                const std::string file_name,
                file::MakeTempFilename(::testing::TempDir(), "dummy.tsp"));
            ASSERT_OK(file::SetContents(file_name, data, file::Defaults()));
            ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                                 TspLibParser::LoadFile(file_name));
            EXPECT_EQ(parser.size(), kDimension);
          }
        }
      }
    }
  }
}

TEST(TspLibParserTest, ParseHCPEdgeList) {
  static const char* kData =
      "NAME : test\n"
      "COMMENT : Test\n"
      "TYPE : HCP\n"
      "DIMENSION : 3\n"
      "EDGE_DATA_FORMAT : EDGE_LIST\n"
      "EDGE_DATA_SECTION\n"
      " 3    1\n"
      " 2    1\n"
      "-1\nEOF";
  ASSERT_OK_AND_ASSIGN(
      const std::string file_name,
      file::MakeTempFilename(::testing::TempDir(), "dummy.hcp"));
  ASSERT_OK(file::SetContents(file_name, kData, file::Defaults()));
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(file_name));
  EXPECT_EQ(parser.size(), 3);
  EXPECT_EQ(2, parser.edges()[0].size());
  EXPECT_EQ(1, parser.edges()[0][0]);
  EXPECT_EQ(2, parser.edges()[0][1]);
  EXPECT_EQ(0, parser.edges()[1].size());
  EXPECT_EQ(0, parser.edges()[2].size());
}

TEST(TspLibParserTest, ParseHCPAdjList) {
  static const char* kData =
      "NAME : test\n"
      "COMMENT : Test\n"
      "TYPE : HCP\n"
      "DIMENSION : 3\n"
      "EDGE_DATA_FORMAT : ADJ_LIST\n"
      "EDGE_DATA_SECTION\n"
      " 3    1     2    -1\n"
      "-1\nEOF";
  ASSERT_OK_AND_ASSIGN(
      const std::string file_name,
      file::MakeTempFilename(::testing::TempDir(), "dummy.hcp"));
  ASSERT_OK(file::SetContents(file_name, kData, file::Defaults()));
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(file_name));
  EXPECT_EQ(parser.size(), 3);
  EXPECT_EQ(1, parser.edges()[0].size());
  EXPECT_EQ(2, parser.edges()[0][0]);
  EXPECT_EQ(1, parser.edges()[1].size());
  EXPECT_EQ(2, parser.edges()[1][0]);
  EXPECT_EQ(0, parser.edges()[2].size());
}

TEST(TspLibParserTest, ParseKytojoki33Depot) {
  // This file inverts EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
  std::string file_name = ortools::GetDataDependencyFilepath(
      "ortools/routing/parsers/testdata/"
      "tsplib_Kytojoki_33.vrp");
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(file_name));
  // The depot is a new node, given by its coordinates, instead of an existing
  // node in the graph.
  EXPECT_EQ(2400, parser.depot());
  EXPECT_EQ(0, parser.edges().size());
  EXPECT_EQ(0.0, parser.coordinates()[parser.depot()].x);
  EXPECT_EQ(0.0, parser.coordinates()[parser.depot()].y);
}

// Make sure we properly fail when reading an invalid file. To test this,
// reading from a raw zip file instead of the included subfiles.
TEST(TspLibParserTest, ReadFailOnInvalidFile) {
  std::string file_name = ortools::GetDataDependencyFilepath(
      "ortools/routing/parsers/testdata/solomon.zip");
  EXPECT_THAT(TspLibParser::LoadFile(file_name),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(TspLibTourParserTest, LoadAllDataSets) {
  static const char kArchive[] =
      "operations_research_data/TSPLIB95/ALL_tsp.tar.gz";
  static const char* kExpectedComments[] = {
      "",
      ": Optimum solution for att48",
      ": Optimum solution of bayg29",
      ": Optimum solution of bays29",
      "",
      "",
      ": Length 6110",
      ": Length 6528",
      ": Optimum tour for eil101.tsp (Length 629)",
      ": Optimal tour for eil51.tsp (426)",
      ": Optimum tour for eil76.tsp (538)",
      ": optimal tour for fri26 (937)",
      ": Optimal tour for gr120 (6942)",
      ": Optimal solution for gr202 (40160)",
      ": Optimal solution for gr24 (1272)",
      ": Optimal solution for gr48 (5046)",
      ": Optimal solution of gr666 (294358)",
      ": Optimal tour for gr96 (55209)",
      ": Optimum tour for kroA100 (21282)",
      ": Optimal tour for kroC100 (20749)",
      ": Optimal tour for kroD100 (21294)",
      ": Optimal tour for lin105 (14379)",
      ": Optimal tour for pa561 (2763)",
      ": Optimal solution for pcb442 (50778)",
      ": optimal tour for pr1002 (259045)",
      ": Optimal solution for pr2392 (378032)",
      ": Optimal tour for pr76 (108159)",
      ": Optimal solution for rd100 (7910)",
      ": Optimal tour for st70 (675)",
      ": Optimal solution for tsp225 (3919)",
      ": Optimal solution for ulysses16 (6859)",
      ": Optimal solution of ulysses22 (7013)"};
  int file_index = 0;
  std::vector<std::string> matches;
  const std::string opt_archive_path =
      ortools::GetDataDependencyFilepath(kArchive);
  if (file::Match(
          file::JoinPath("/tarfs", opt_archive_path, "*\\.opt\\.tour\\.gz"),
          &matches, file::Defaults())
          .ok()) {
    for (const std::string& match : matches) {
      TspLibTourParser parser;
      EXPECT_OK(parser.LoadFile(match));
      EXPECT_EQ(kExpectedComments[file_index], parser.comments());
      file_index++;
    }
  }
}

TEST(CVRPToursParserTest, LoadAllDataSets) {
  static const char kArchive[] =
      "operations_research_data/CVRP/Augerat/A-VRP-sol.zip";
  static const int kExpectedCosts[] = {/*opt-A-n32-k5*/ 784,
                                       /*opt-A-n33-k5*/ 661,
                                       /*opt-A-n33-k6*/ 742,
                                       /*opt-A-n34-k5*/ 778,
                                       /*opt-A-n36-k5*/ 799,
                                       /*opt-A-n37-k5*/ 669,
                                       /*opt-A-n37-k6*/ 949,
                                       /*opt-A-n38-k5*/ 730,
                                       /*opt-A-n39-k5*/ 822,
                                       /*opt-A-n39-k6*/ 831,
                                       /*opt-A-n44-k6*/ 937,
                                       /*opt-A-n45-k6*/ 944,
                                       /*opt-A-n45-k7*/ 1146,
                                       /*opt-A-n46-k7*/ 914,
                                       /*opt-A-n48-k7*/ 1073,
                                       /*opt-A-n53-k7*/ 1010,
                                       /*opt-A-n55-k9*/ 1073};
  int file_index = 0;
  std::vector<std::string> matches;
  const std::string cvrp_archive_path =
      ortools::GetDataDependencyFilepath(kArchive);
  if (file::Match(file::JoinPath("/zip", cvrp_archive_path, "opt-A-\\.*"),
                  &matches, file::Defaults())
          .ok()) {
    for (const std::string& match : matches) {
      CVRPToursParser parser;
      EXPECT_OK(parser.LoadFile(match));
      EXPECT_EQ(kExpectedCosts[file_index], parser.cost());
      file_index++;
    }
  }
}

TEST(TspLibParserTest, ParseSOPFullMatrixPopulatesEdgeWeights) {
  const std::string valid_sop = WriteTempTspFile(
      "NAME: sample.sop\n"
      "TYPE: SOP\n"
      "DIMENSION: 3\n"
      "EDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
      "EDGE_WEIGHT_SECTION\n"
      "3\n"
      "0 7 9\n"
      "-1 0 2\n"
      "-1 -1 0\n"
      "EOF\n");
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(valid_sop));
  EXPECT_EQ(parser.type(), TspLibParser::Types::SOP);
  EXPECT_EQ(parser.size(), 3);
  const EdgeWeights weights = parser.GetEdgeWeights();
  EXPECT_EQ(weights(0, 1), 7);
  EXPECT_EQ(weights(0, 2), 9);
  EXPECT_EQ(weights(1, 0), -1);
  EXPECT_EQ(weights(1, 2), 2);
  EXPECT_EQ(weights(2, 2), 0);

  const std::string mismatched_sop = WriteTempTspFile(
      "NAME: bad.sop\n"
      "TYPE: SOP\n"
      "DIMENSION: 3\n"
      "EDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
      "EDGE_WEIGHT_SECTION\n"
      "4\n"
      "0 7 9\n"
      "-1 0 2\n"
      "-1 -1 0\n"
      "EOF\n");
  EXPECT_THAT(TspLibParser::LoadFile(mismatched_sop),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(TspLibParserTest, MoveParserPreservesStateAndEdgeWeights) {
  const std::string first_file = WriteTempTspFile(
      "NAME: first\n"
      "COMMENT: first comment\n"
      "TYPE: CVRP\n"
      "DIMENSION: 3\n"
      "CAPACITY: 10\n"
      "DISTANCE: 100\n"
      "EDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n"
      "1 0 0\n"
      "2 3 4\n"
      "3 6 8\n"
      "DEMAND_SECTION\n"
      "1 0\n"
      "2 4\n"
      "3 5\n"
      "DEPOT_SECTION\n"
      "2\n"
      "-1\n"
      "FIXED_EDGES_SECTION\n"
      "1 2\n"
      "-1\n"
      "EOF\n");
  const std::string second_file = WriteTempTspFile(
      "NAME: second\n"
      "TYPE: TSP\n"
      "DIMENSION: 2\n"
      "EDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n"
      "1 0 0\n"
      "2 10 0\n"
      "EOF\n");

  ASSERT_OK_AND_ASSIGN(TspLibParser parser, TspLibParser::LoadFile(first_file));
  EXPECT_EQ(parser.depot(), 1);
  EXPECT_EQ(parser.capacity(), 10);
  EXPECT_EQ(parser.max_distance(), 100);
  EXPECT_FALSE(parser.demands().empty());
  EXPECT_FALSE(parser.fixed_edges().empty());
  EXPECT_EQ(parser.comments(), "first comment");
  EXPECT_EQ(parser.GetEdgeWeights()(0, 1), 5);

  ASSERT_OK_AND_ASSIGN(parser, TspLibParser::LoadFile(second_file));
  EXPECT_EQ(parser.name(), "second");
  EXPECT_EQ(parser.type(), TspLibParser::Types::TSP);
  EXPECT_EQ(parser.size(), 2);
  EXPECT_EQ(parser.depot(), 0);
  EXPECT_EQ(parser.capacity(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(parser.max_distance(), std::numeric_limits<int64_t>::max());
  EXPECT_TRUE(parser.demands().empty());
  EXPECT_TRUE(parser.fixed_edges().empty());
  EXPECT_TRUE(parser.comments().empty());
  EXPECT_EQ(parser.GetEdgeWeights()(0, 1), 10);
}

TEST(TspLibParserTest, GeoDistanceClampsDomain) {
  const std::string geo_file = WriteTempTspFile(
      "NAME: geo\n"
      "TYPE: TSP\n"
      "DIMENSION: 3\n"
      "EDGE_WEIGHT_TYPE: GEO\n"
      "NODE_COORD_SECTION\n"
      "1 38.24 20.42\n"
      "2 38.24 20.42\n"
      "3 -38.24 -159.18\n"
      "EOF\n");
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(geo_file));
  const EdgeWeights weights = parser.GetEdgeWeights();
  EXPECT_GE(weights(0, 1), 0);
  EXPECT_GT(weights(0, 2), 0);
}

TEST(TspLibParserTest, SupportsInt64WeightsCapacitiesAndDemands) {
  const std::string file_name = WriteTempTspFile(
      "NAME: large_weights\n"
      "TYPE: CVRP\n"
      "DIMENSION: 2\n"
      "CAPACITY: 10000000000\n"
      "DISTANCE: 20000000000\n"
      "EDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
      "EDGE_WEIGHT_SECTION\n"
      "0 5000000000\n"
      "5000000000 0\n"
      "DEMAND_SECTION\n"
      "1 0\n"
      "2 3000000000\n"
      "EOF\n");
  ASSERT_OK_AND_ASSIGN(const TspLibParser parser,
                       TspLibParser::LoadFile(file_name));
  EXPECT_EQ(parser.capacity(), 10'000'000'000LL);
  EXPECT_EQ(parser.max_distance(), 20'000'000'000LL);
  EXPECT_EQ(parser.GetEdgeWeights()(0, 1), 5'000'000'000LL);
  EXPECT_THAT(parser.demands(), ElementsAre(0, 3'000'000'000LL));
}

TEST(TspLibParserTest, RejectsMalformedDimensionsAndBareKeywords) {
  static const char* const kInvalidInputs[] = {
      // Missing DIMENSION.
      "NAME: test\nTYPE: TSP\nEDGE_WEIGHT_TYPE: EUC_2D\nEOF\n",
      // Zero, negative, excessive, and int32-overflowing DIMENSION.
      "TYPE: TSP\nDIMENSION: 0\nEDGE_WEIGHT_TYPE: EUC_2D\nEOF\n",
      "TYPE: TSP\nDIMENSION: -5\nEDGE_WEIGHT_TYPE: EUC_2D\nEOF\n",
      "TYPE: TSP\nDIMENSION: 10000001\nEDGE_WEIGHT_TYPE: EUC_2D\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2147483648\nEOF\n",
      // Duplicate DIMENSION.
      "TYPE: TSP\nDIMENSION: 2\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\nEOF\n",
      // Bare section keywords without values.
      "TYPE\n",
      "DIMENSION\n",
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY\n",
      "TYPE: TSP\nDIMENSION: 2\nDISTANCE\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_FORMAT\n",
  };
  for (const char* input : kInvalidInputs) {
    const std::string path = WriteTempTspFile(input);
    EXPECT_THAT(TspLibParser::LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Input was:\n"
        << input;
  }
}

TEST(TspLibParserTest, RejectsSectionsBeforeDimensionAndMissingDemands) {
  static const char* const kInvalidInputs[] = {
      // DEPOT_SECTION with coordinates before DIMENSION (regression for OOB
      // write at coords_[size_ - 1] when size_ == 0).
      "TYPE: CVRP\nDEPOT_SECTION\n0.0 0.0\n-1\n",
      // Other data sections before DIMENSION.
      "TYPE: TSP\nNODE_COORD_SECTION\n1 0 0\n",
      "TYPE: CVRP\nDEMAND_SECTION\n1 5\n",
      "TYPE: TSP\nFIXED_EDGES_SECTION\n1 2\n-1\n",
      "TYPE: TSP\nDISPLAY_DATA_SECTION\n1 0 0\n",
      "TYPE: TSP\nEDGE_WEIGHT_TYPE: EXPLICIT\nEDGE_WEIGHT_FORMAT: FULL_MATRIX\n"
      "EDGE_WEIGHT_SECTION\n0\n",
      // HCP EDGE_DATA_SECTION without EDGE_DATA_FORMAT (uninitialized format).
      "TYPE: HCP\nDIMENSION: 2\nEDGE_DATA_SECTION\n1 2\n-1\nEOF\n",
      // Capacitated problem without DEMAND_SECTION.
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY: 5\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nCAPACITY: 9223372036854775807\n"
      "EDGE_WEIGHT_TYPE: EUC_2D\nNODE_COORD_SECTION\n1 0 0\n2 1 1\nEOF\n",
  };
  for (const char* input : kInvalidInputs) {
    const std::string path = WriteTempTspFile(input);
    EXPECT_THAT(TspLibParser::LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Input was:\n"
        << input;
  }
}

TEST(TspLibParserTest, RejectsInvalidExplicitMatrices) {
  static const char* const kInvalidInputs[] = {
      // Missing EDGE_WEIGHT_SECTION for EXPLICIT.
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\nEOF\n",
      // Truncated FULL_MATRIX (3 entries instead of 4).
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\nEDGE_WEIGHT_SECTION\n0 1 1\nEOF\n",
      // Extra entries in FULL_MATRIX, UPPER_ROW, LOWER_ROW, UPPER_DIAG_ROW,
      // and LOWER_DIAG_ROW on a single line.
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\nEDGE_WEIGHT_SECTION\n0 1 1 0 9\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: UPPER_ROW\nEDGE_WEIGHT_SECTION\n5 6\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: LOWER_ROW\nEDGE_WEIGHT_SECTION\n5 6\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: UPPER_DIAG_ROW\nEDGE_WEIGHT_SECTION\n0 5 0 6\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: LOWER_DIAG_ROW\nEDGE_WEIGHT_SECTION\n0 5 0 6\nEOF\n",
      // Explicit matrix dimension exceeding maximum matrix entry cap.
      "TYPE: TSP\nDIMENSION: 20000\nEDGE_WEIGHT_TYPE: EXPLICIT\n"
      "EDGE_WEIGHT_FORMAT: FULL_MATRIX\nEDGE_WEIGHT_SECTION\n0\nEOF\n",
  };
  for (const char* input : kInvalidInputs) {
    const std::string path = WriteTempTspFile(input);
    EXPECT_THAT(TspLibParser::LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Input was:\n"
        << input;
  }
}

TEST(TspLibParserTest, RejectsOutOfBoundsAndInvalidNodeIndices) {
  static const char* const kInvalidInputs[] = {
      // NODE_COORD_SECTION node id 0, > size, INT_MIN, and INT_MAX + 1.
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n0 0 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n3 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n-2147483648 0 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n2147483648 0 0\n2 1 1\nEOF\n",
      // DEMAND_SECTION out-of-bounds and int32-overflowing node ids.
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY: 5\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\n"
      "DEMAND_SECTION\n1 1\n3 1\nEOF\n",
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY: 5\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\n"
      "DEMAND_SECTION\n1 1\n2147483648 1\nEOF\n",
      // DEPOT_SECTION out-of-bounds depot id.
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY: 5\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\nDEMAND_SECTION\n1 0\n2 1\n"
      "DEPOT_SECTION\n3\n-1\nEOF\n",
      "TYPE: CVRP\nDIMENSION: 2\nCAPACITY: 5\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\nDEMAND_SECTION\n1 0\n2 1\n"
      "DEPOT_SECTION\n0\n-1\nEOF\n",
      // FIXED_EDGES_SECTION out-of-bounds node id.
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0 0\n2 1 1\n"
      "FIXED_EDGES_SECTION\n1 3\n-1\nEOF\n",
      // HCP EDGE_LIST and ADJ_LIST out-of-bounds and int32-overflowing node
      // ids.
      "TYPE: HCP\nDIMENSION: 2\nEDGE_DATA_FORMAT: EDGE_LIST\n"
      "EDGE_DATA_SECTION\n1 3\n-1\nEOF\n",
      "TYPE: HCP\nDIMENSION: 2\nEDGE_DATA_FORMAT: ADJ_LIST\n"
      "EDGE_DATA_SECTION\n1 3 -1\n-1\nEOF\n",
      "TYPE: HCP\nDIMENSION: 2\nEDGE_DATA_FORMAT: ADJ_LIST\n"
      "EDGE_DATA_SECTION\n1 2147483648 -1\n-1\nEOF\n",
  };
  for (const char* input : kInvalidInputs) {
    const std::string path = WriteTempTspFile(input);
    EXPECT_THAT(TspLibParser::LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Input was:\n"
        << input;
  }
}

TEST(TspLibParserTest, RejectsInvalidCoordinates) {
  static const char* const kInvalidInputs[] = {
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 nan 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 inf 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 1e100 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 abc 0\n2 1 1\nEOF\n",
      "TYPE: TSP\nDIMENSION: 2\nEDGE_WEIGHT_TYPE: EUC_2D\n"
      "NODE_COORD_SECTION\n1 0\n2 1 1\nEOF\n",
  };
  for (const char* input : kInvalidInputs) {
    const std::string path = WriteTempTspFile(input);
    EXPECT_THAT(TspLibParser::LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Input was:\n"
        << input;
  }
}

TEST(TspLibTourParserTest, RejectsMalformedInputsAndResetsOnReuse) {
  static const char* const kInvalidTours[] = {
      "TYPE : TSP\nDIMENSION : 2\nTOUR_SECTION\n1\n2\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION\n",
      "TYPE : TOUR\nDIMENSION : 0\nTOUR_SECTION\n1\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 3\nDIMENSION : 2\n"
      "TOUR_SECTION\n1\n2\n-1\nEOF\n",
      "TYPE : TOUR\nTOUR_SECTION\n1\n3\n-1\nDIMENSION : 2\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\n0\n2\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\n-2\n2\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\n1\n3\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\n2147483648\n2\n-1\nEOF\n",
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\nabc\n-1\nEOF\n",
  };
  for (const char* tour : kInvalidTours) {
    const std::string path = WriteTempTspFile(tour);
    TspLibTourParser parser;
    EXPECT_THAT(parser.LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "Tour input was:\n"
        << tour;
  }

  const std::string valid1 = WriteTempTspFile(
      "COMMENT : first\nTYPE : TOUR\nDIMENSION : 3\n"
      "TOUR_SECTION\n1\n2\n3\n-1\nEOF\n");
  const std::string valid2 = WriteTempTspFile(
      "TYPE : TOUR\nDIMENSION : 2\nTOUR_SECTION\n2\n1\n-1\nEOF\n");
  TspLibTourParser parser;
  ASSERT_OK(parser.LoadFile(valid1));
  EXPECT_EQ(parser.size(), 3);
  EXPECT_EQ(parser.comments(), ": first");
  ASSERT_OK(parser.LoadFile(valid2));
  EXPECT_EQ(parser.size(), 2);
  EXPECT_TRUE(parser.comments().empty());
  EXPECT_THAT(parser.tour(), ElementsAre(1, 0));
}

TEST(CVRPToursParserTest, RejectsMalformedInputsAndResetsOnReuse) {
  static const char* const kInvalidTours[] = {
      "Route #1:\nCost 10\n",           "Route #1: -1\nCost 10\n",
      "Route #1: 10000001\nCost 10\n",  "Route #1: 2147483648\nCost 10\n",
      "Route #1: node\nCost 10\n",      "Route #1: 1 2\nCost\n",
      "Route #1: 1 2\nCost 10 extra\n", "Route #1: 1 2\nCost not_an_int\n",
  };
  for (const char* tour : kInvalidTours) {
    const std::string path = WriteTempTspFile(tour);
    CVRPToursParser parser;
    EXPECT_THAT(parser.LoadFile(path),
                StatusIs(absl::StatusCode::kInvalidArgument))
        << "CVRP tour input was:\n"
        << tour;
  }

  const std::string valid1 =
      WriteTempTspFile("Route #1: 1 2\nCost 5000000000\n");
  const std::string valid2 = WriteTempTspFile("Route #1: 3\nCost 20\n");
  CVRPToursParser parser;
  ASSERT_OK(parser.LoadFile(valid1));
  EXPECT_EQ(parser.cost(), 5'000'000'000LL);
  EXPECT_EQ(parser.tours().size(), 1);
  ASSERT_OK(parser.LoadFile(valid2));
  EXPECT_EQ(parser.cost(), 20);
  ASSERT_EQ(parser.tours().size(), 1);
  EXPECT_THAT(parser.tours()[0], ElementsAre(3));
}
}  // namespace
}  // namespace operations_research::routing
