// Copyright 2010-2024 Google LLC
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

#include "ortools/routing/parsers/carp_parser.h"

#include <string>

#include "absl/base/log_severity.h"
#include "absl/log/scoped_mock_log.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"

#define ROOT_DIR "_main/"

namespace operations_research::routing {
namespace {
TEST(CarpParserTest, Constructor) {
  CarpParser parser;
  EXPECT_EQ(parser.name(), "");
  EXPECT_EQ(parser.comment(), "");
  EXPECT_EQ(parser.NumberOfNodes(), 0);
  EXPECT_EQ(parser.NumberOfEdgesWithServicing(), 0);
  EXPECT_EQ(parser.NumberOfEdgesWithoutServicing(), 0);
  EXPECT_EQ(parser.NumberOfEdges(), 0);
  EXPECT_EQ(parser.NumberOfVehicles(), 0);
  EXPECT_EQ(parser.capacity(), 0);
  EXPECT_EQ(parser.TotalServicingCost(), 0);
  EXPECT_EQ(parser.depot(), 0);
}

TEST(CarpParserTest, LoadEmptyFileName) {
  std::string empty_file_name;
  CarpParser parser;
  EXPECT_FALSE(parser.LoadFile(empty_file_name));
}

TEST(CarpParserTest, LoadNonExistingFile) {
  CarpParser parser;
  EXPECT_FALSE(parser.LoadFile(""));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectNumberOfNodes) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the number of nodes: -4"));
  EXPECT_CALL(
      log,
      Log(absl::LogSeverity::kError, testing::_,
          "Error when parsing the following metadata line:  VERTICES : -4"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_vertices.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectNumberOfArcsWithServicings) {
  absl::ScopedMockLog log;
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kError, testing::_,
               "Error when parsing the number of edges with servicing: -11"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the following metadata line:  "
                       "ARISTAS_REQ : -11"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_arireq.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectNumberOfArcsWithoutServicings) {
  absl::ScopedMockLog log;
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kError, testing::_,
               "Error when parsing the number of edges without servicing: a"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the following metadata line:  "
                       "ARISTAS_NOREQ : a"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_arinoreq.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectNumberOfVehicles) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the number of vehicles: 0"));
  EXPECT_CALL(
      log,
      Log(absl::LogSeverity::kError, testing::_,
          "Error when parsing the following metadata line:  VEHICULOS : 0"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_vehiculos.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectCapacity) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the capacity: 0"));
  EXPECT_CALL(
      log,
      Log(absl::LogSeverity::kError, testing::_,
          "Error when parsing the following metadata line:  CAPACIDAD : 0"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_capacidad.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectTypeOfArcCost) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Value of TIPO_COSTES_ARISTAS is unexpected, only "
                       "EXPLICITOS is supported, but IMPLICITOS was found"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the following metadata line:  "
                       "TIPO_COSTES_ARISTAS : IMPLICITOS"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_tipo.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectTotalServicingCost) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the total servicing cost: qwertz"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the following metadata line:  "
                       "COSTE_TOTAL_REQ : qwertz"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_coste.dat")));
}

TEST(CarpParserTest, LoadInvalidFileIncorrectDepot) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Could not parse node index: -1"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Error when parsing the depot: -1"));
  EXPECT_CALL(
      log,
      Log(absl::LogSeverity::kError, testing::_,
          "Error when parsing the following metadata line:  DEPOSITO :   -1"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers/testdata/"
                                     "carp_gdb19_incorrecto_deposito.dat")));
}

TEST(CarpParserTest, LoadInvalidFileNoEdgeWithServicing) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kError, testing::_,
                  "Error when parsing the number of edges with servicing: 0"));
  EXPECT_CALL(
      log,
      Log(absl::LogSeverity::kError, testing::_,
          "Error when parsing the following metadata line:  ARISTAS_REQ : 0"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(parser.LoadFile(
      file::JoinPath(::testing::SrcDir(),
                     ROOT_DIR "ortools/routing/parsers"
                              "/testdata/carp_gdb19_no_arista_req.dat")));
}

TEST(CarpParserTest, LoadInvalidFileServicingForArcsWithoutServicing) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Extraneous elements in line, starting with: demanda"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Could not parse line in LISTA_ARISTAS_NOREQ:  ( 1, 4)  "
                       "coste 3 demanda 3"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(
      parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                     "ortools/routing/parsers"
                                     "/testdata/carp_gdb19_mixed_arcs.dat")));
}

TEST(CarpParserTest, LoadInvalidFileServicingForArcsInWrongOrder) {
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Unexpected keyword: demanda"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_,
                       "Could not parse line in LISTA_ARISTAS_REQ:  ( 1, 4)  "
                       "demanda 3 coste 3"));
  log.StartCapturingLogs();

  CarpParser parser;
  EXPECT_FALSE(parser.LoadFile(
      file::JoinPath(::testing::SrcDir(),
                     ROOT_DIR "ortools/routing/parsers/testdata/"
                              "carp_gdb19_incorrecta_lista_aristas_req.dat")));
}

TEST(CarpParserTest, LoadInstanceFile) {
  std::string file_name = file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                         "ortools/routing/parsers/testdata/"
                                         "carp_gdb19.dat");
  CarpParser parser;
  EXPECT_TRUE(parser.LoadFile(file_name));
  EXPECT_EQ(parser.name(), "gdb19");
  EXPECT_EQ(parser.comment(), "10000 (cota superior)");
  EXPECT_EQ(parser.NumberOfNodes(), 8);
  EXPECT_EQ(parser.NumberOfEdgesWithServicing(), 11);
  EXPECT_EQ(parser.NumberOfEdgesWithoutServicing(), 0);
  EXPECT_EQ(parser.NumberOfEdges(), 11);
  EXPECT_EQ(parser.NumberOfVehicles(), 3);
  EXPECT_EQ(parser.capacity(), 27);
  EXPECT_EQ(parser.TotalServicingCost(), 45);
  EXPECT_EQ(parser.depot(), 0);

  EXPECT_EQ(parser.traversing_costs().size(), 11);
  EXPECT_EQ(parser.GetTraversingCost(0, 1), 4);
  EXPECT_EQ(parser.GetTraversingCost(1, 0), 4);
  EXPECT_EQ(parser.servicing_demands().size(), 11);
  EXPECT_EQ(parser.GetServicing(0, 1), 8);
  EXPECT_EQ(parser.GetServicing(1, 0), 8);
}

TEST(CarpParserTest, LoadInstanceFileWithDifferentDepot) {
  std::string file_name = file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                         "ortools/routing/parsers/testdata/"
                                         "carp_gdb19_diferente_deposito.dat");
  CarpParser parser;
  EXPECT_TRUE(parser.LoadFile(file_name));
  EXPECT_EQ(parser.depot(), 4);
}
}  // namespace
}  // namespace operations_research::routing
