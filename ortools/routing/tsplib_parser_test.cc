// Copyright 2010-2022 Google LLC
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

#include "ortools/routing/tsplib_parser.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/helpers.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/map_util.h"
#include "ortools/base/memfile.h"
#include "ortools/base/path.h"
#include "ortools/base/zipfile.h"

ABSL_FLAG(std::string, test_srcdir, "", "REQUIRED: src dir");

namespace operations_research {
namespace {
/*
TEST(TspLibParserTest, LoadAllDataSets) {
  static const char* kArchives[] = {
      "operations_research_data/TSPLIB95/ALL_tsp.tar.gz",
      "operations_research_data/TSPLIB95/ALL_vrp.tar",
      "operations_research_data/TSPLIB95/ALL_atsp.tar",
      "operations_research_data/TSPLIB95/ALL_sop.tar",
      "operations_research_data/TSPLIB95/ALL_hcp.tar"};
  static const char* kZipArchives[] = {
      "operations_research_data/TSPLIB95/ALL_tsp.zip",
      "operations_research_data/TSPLIB95/ALL_vrp.zip"};
  static const char* kExpectedComments[] = {
      "drilling problem (Ludwig)",
      "535 Airports around the globe (Padberg/Rinaldi)",
      "48 capitals of the US (Padberg/Rinaldi)",
      "532-city problem (Padberg/Rinaldi)",
      "29 Cities in Bavaria, "  // NOLINT(bugprone-suspicious-missing-comma)
      "geographical distances (Groetschel,Juenger,Reinelt)",
      "29 cities in Bavaria, street distances (Groetschel,Juenger,Reinelt)",
      "52 locations in Berlin (Groetschel)",
      "127 Biergaerten in Augsburg (Juenger/Reinelt)",
      "58 cities in Brazil (Ferreira)",
      "BR Deutschland in den Grenzen von 1989 (Bachem/Wottawa)",
      "Bridge tournament problem (Rinaldi)",
      "14-Staedte in Burma (Zaw Win)",
      "130 city problem (Churritz)",
      "150 city Problem (churritz)",
      "Drilling problem (Reinelt)",
      "Deutschland-Problem (A.Rohe)",
      "Drilling problem (Reinelt)",
      "Bundesrepublik Deutschland (mit Ex-DDR) (Bachem/Wottawa)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "42 cities (Dantzig)",
      "Clustered random problem (Johnson)",
      "101-city problem (Christofides/Eilon)",
      "51-city problem (Christofides/Eilon)",
      "76-city problem (Christofides/Eilon)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Die 5 neuen Laender Deutschlands (Ex-DDR) (Bachem/Wottawa)",
      "26 Staedte (Fricker)",
      "262-city problem (Gillet/Johnson)",
      "120 cities in Germany (Groetschel)",
      "America-Subproblem of 666-city TSP (Groetschel)",
      "17-city problem (Groetschel)",
      "Europe-Subproblem of 666-city TSP (Groetschel)",
      "21-city problem (Groetschel)",
      "Asia/Australia-Subproblem of 666-city TSP (Groetschel)",
      "24-city problem (Groetschel)",
      "Europe/Asia/Australia-Subproblem of 666-city TSP (Groetschel)",
      "48-city problem (Groetschel)",
      "666 cities around the world (Groetschel)",
      "Africa-Subproblem of 666-city TSP (Groetschel)",
      "48-city problem (Held/Karp)",
      "100-city problem A (Krolak/Felts/Nelson)",
      "150-city problem A (Krolak/Felts/Nelson)",
      "200-city problem A (Krolak/Felts/Nelson)",
      "100-city problem B (Krolak/Felts/Nelson)",
      "150-city problem B (Krolak/Felts/Nelson)",
      "200-city problem B (Krolak/Felts/Nelson)",
      "100-city problem C (Krolak/Felts/Nelson)",
      "100-city problem D (Krolak/Felts/Nelson)",
      "100-city problem E (Krolak/Felts/Nelson)",
      "105-city problem (Subproblem of lin318)",
      "318-city problem (Lin/Kernighan)",
      "Original 318-city problem (Lin/Kernighan)",
      "1379 Orte in Nordrhein-Westfalen (Bachem/Wottawa)",
      "Drilling problem (Reinelt)",
      "561-city problem (Kleinschmidt)",
      "Drilling problem (Juenger/Reinelt)",
      "Drilling problem (Junger/Reinelt)",
      "Drilling problem (Groetschel/Juenger/Reinelt)",
      "Programmed logic array (Johnson)",
      "Programmed logic array (Johnson)",
      "Programmed logic array (Johnson)",
      "1002-city problem (Padberg/Rinaldi)",
      "107-city problem (Padberg/Rinaldi)",
      "124-city problem (Padberg/Rinaldi)",
      "136-city problem (Padberg/Rinaldi)",
      "144-city problem (Padberg/Rinaldi)",
      "152-city problem (Padberg/Rinaldi)",
      "226-city problem (Padberg/Rinaldi)",
      "2392-city problem (Padberg/Rinaldi)",
      "264-city problem (Padberg/Rinaldi)",
      "299-city problem (Padberg/Rinaldi)",
      "439-city problem (Padberg/Rinaldi)",
      "76-city problem (Padberg/Rinaldi)",
      "Rattled grid (Pulleyblank)",
      "Rattled grid (Pulleyblank)",
      "Rattled grid (Pulleyblank)",
      "Rattled grid (Pulleyblank)",
      "100-city random TSP (Reinelt)",
      "400-city random TSP (Reinelt)",
      "11849-city TSP (Reinelt)",
      "1304-city TSP (Reinelt)",
      "1323-city TSP (Reinelt)",
      "1889-city TSP (Reinelt)",
      "5915-city TSP (Reinelt)",
      "5934-city TSP (Reinelt)",
      "",
      "",
      "",
      "70-city problem (Smith/Thompson)",
      "42 Staedte Schweiz (Fricker)",
      "225-city problem (Juenger,Raecke,Tschoecke)",
      "A TSP problem (Reinelt)",
      "Drilling problem problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Drilling problem (Reinelt)",
      "Odyssey of Ulysses (Groetschel/Padberg)",
      "Odyssey of Ulysses (Groetschel/Padberg)",
      "Cities with population at least 500 in the continental US.\nContributed"
      " by David Applegate and Andre Rohe, based on the\ndata set"
      " \"US.lat-long\" from the ftp site ftp.cs.toronto.edu.\nThe file"
      " US.lat-long.Z can be found in the directory /doc/geography.",
      "1084-city problem (Reinelt)",
      "1784-city problem (Reinelt)",
      "(Rinaldi,Yarrow/Araque)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Eilon et al.)",
      "(Gillet and Johnson)",
      "17 city problem (Repetto)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Fischetti)",
      "Asymmetric TSP (Repetto,Pekny)",
      "Stacker crane application (Ascheuer)",
      "Stacker crane application (Ascheuer)",
      "Stacker crane application (Ascheuer)",
      "Stacker crane application (Ascheuer)",
      "Asymmetric TSP (Fischetti)",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "Received by Norbert Ascheuer / Laureano Escudero",
      "br17.atsp plus random precedences (Norbert Ascheuer)",
      "br17.atsp plus random precedences (Norbert Ascheuer)",
      "Received by Norbert Ascheuer (based on ft53.atsp)",
      "Received by Norbert Ascheuer (based on ft53.atsp)",
      "Received by Norbert Ascheuer (based on ft53.atsp)",
      "Received by Norbert Ascheuer (based on ft53.atsp)",
      "Received by Norbert Ascheuer (based on ft70.atsp)",
      "Received by Norbert Ascheuer (based on ft70.atsp)",
      "Received by Norbert Ascheuer (based on ft70.atsp)",
      "Received by Norbert Ascheuer (based on ft70.atsp)",
      "Received by Norbert Ascheuer (based on kro124p.atsp)",
      "Received by Norbert Ascheuer (based on kro124p.atsp)",
      "Received by Norbert Ascheuer (based on kro124p.atsp)",
      "Received by Norbert Ascheuer (based on kro124p.atsp)",
      "Received by Norbert Ascheuer (based on p43.atsp)",
      "Received by Norbert Ascheuer (based on p43.atsp)",
      "Received by Norbert Ascheuer (based on p43.atsp)",
      "Received by Norbert Ascheuer (based on p43.atsp)",
      "Received by N. Ascheuer / M. Juenger / G. Reinelt",
      "Received by N. Ascheuer / M. Juenger / G. Reinelt",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Stacker crane application (Norbert Ascheuer)",
      "Received by Norbert Ascheuer (based on ry48p.atsp)",
      "Received by Norbert Ascheuer (based on ry48p.atsp)",
      "Received by Norbert Ascheuer (based on ry48p.atsp)",
      "Received by Norbert Ascheuer (based on ry48p.atsp)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)",
      "Hamiltonian cycle problem (Erbacci)"};
  // Skipping unsupported instances.
  const absl::btree_set<absl::string_view> exceptions = {"xray.problems",
                                                         "tspleap.c"};
  // Parsing from tar archive
  int file_index = 0;
  for (const char* const archive : kArchives) {
    std::vector<std::string> matches;
    if (file::Match(file::JoinPath("/tarfs", absl::GetFlag(FLAGS_test_srcdir),
                                   archive, "*"),
                    &matches, file::Defaults())
            .ok()) {
      for (const std::string& match : matches) {
        const absl::string_view stem = file::Stem(match);
        if (!exceptions.contains(stem) && file::Extension(stem) != "tour") {
          TspLibParser parser;
          EXPECT_TRUE(parser.LoadFile(match));
          EXPECT_EQ(kExpectedComments[file_index], parser.comments());
          EXPECT_LT(0, parser.SizeFromFile(match));
          file_index++;
        }
      }
    }
  }
  // Parsing from zip archive.
  file_index = 0;
  for (const char* const archive : kZipArchives) {
    std::shared_ptr<zipfile::ZipArchive> zip_archive(zipfile::OpenZipArchive(
        file::JoinPath(absl::GetFlag(FLAGS_test_srcdir), archive)));
    ASSERT_NE(nullptr, zip_archive);
    std::vector<std::string> matches;
    ASSERT_TRUE(
        file::Match(file::JoinPath("/zip", absl::GetFlag(FLAGS_test_srcdir),
                                   archive, "*"),
                    &matches, file::Defaults())
            .ok());
    for (const std::string& match : matches) {
      const absl::string_view stem = file::Stem(match);
      if (!exceptions.contains(stem) && file::Extension(stem) != "tour") {
        TspLibParser parser;
        EXPECT_TRUE(parser.LoadFile(match));
        EXPECT_EQ(kExpectedComments[file_index], parser.comments());
        EXPECT_LT(0, parser.SizeFromFile(match));
        file_index++;
      }
    }
  }
}
*/
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
                data += "\n";
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
                    data += "\n";
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
                    data += "\n";
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
                    data += "\n";
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
                    data += "\n";
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
                    data += "\n";
                  }
                  break;
              }
            }
            data += "EOF";
            const std::string kMMFileName{std::tmpnam(nullptr)};
            RegisteredMemFile registered(kMMFileName, data);
            TspLibParser parser;
            EXPECT_TRUE(parser.LoadFile(kMMFileName));
            EXPECT_EQ(kDimension, parser.SizeFromFile(kMMFileName));
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
  const std::string kMMFileName{std::tmpnam(nullptr)};
  RegisteredMemFile registered(kMMFileName, kData);
  TspLibParser parser;
  EXPECT_TRUE(parser.LoadFile(kMMFileName));
  EXPECT_EQ(3, parser.SizeFromFile(kMMFileName));
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
  const std::string kMMFileName{std::tmpnam(nullptr)};
  RegisteredMemFile registered(kMMFileName, kData);
  TspLibParser parser;
  EXPECT_TRUE(parser.LoadFile(kMMFileName));
  EXPECT_EQ(3, parser.SizeFromFile(kMMFileName));
  EXPECT_EQ(1, parser.edges()[0].size());
  EXPECT_EQ(2, parser.edges()[0][0]);
  EXPECT_EQ(1, parser.edges()[1].size());
  EXPECT_EQ(2, parser.edges()[1][0]);
  EXPECT_EQ(0, parser.edges()[2].size());
}

TEST(TspLibParserTest, ParseKytojoki33Depot) {
  // This file inverts EDGE_WEIGHT_TYPE and EDGE_WEIGHT_FORMAT.
  std::string file_name =
      file::JoinPath(absl::GetFlag(FLAGS_test_srcdir),
                     "ortools/routing/testdata/", "tsplib_Kytojoki_33.vrp");
  TspLibParser parser;
  EXPECT_TRUE(parser.LoadFile(file_name));
  // The depot is a new node, given by its coordinates, instead of an existing
  // node in the graph.
  EXPECT_EQ(2400, parser.depot());
  EXPECT_EQ(0, parser.edges().size());
  EXPECT_EQ(0.0, parser.coordinates()[parser.depot()].x);
  EXPECT_EQ(0.0, parser.coordinates()[parser.depot()].y);
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
  if (file::Match(file::JoinPath("/tarfs", absl::GetFlag(FLAGS_test_srcdir),
                                 kArchive, "*\\.opt\\.tour\\.gz"),
                  &matches, file::Defaults())
          .ok()) {
    for (const std::string& match : matches) {
      TspLibTourParser parser;
      EXPECT_TRUE(parser.LoadFile(match));
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
  if (file::Match(file::JoinPath("/zip", absl::GetFlag(FLAGS_test_srcdir),
                                 kArchive, "opt-A-\\.*"),
                  &matches, file::Defaults())
          .ok()) {
    for (const std::string& match : matches) {
      CVRPToursParser parser;
      EXPECT_TRUE(parser.LoadFile(match));
      EXPECT_EQ(kExpectedCosts[file_index], parser.cost());
      file_index++;
    }
  }
}
}  // namespace
}  // namespace operations_research
