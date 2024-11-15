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

#include "ortools/routing/parsers/tsptw_parser.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/path.h"

#define ROOT_DIR "_main/"

namespace operations_research::routing {
namespace {

TEST(TspTWParserTest, LoadDataSet) {
  const int sizes[] = {26, 21, 21};
  const double distances[] = {25166.316, 9538, 9006};
  const double times[] = {25166.316, 9538, 9006};
  const double starts[] = {9362, 2388, 2392};
  const double ends[] = {13322, 3131, 3146};
  const double service_times[] = {250, 0, 0};
  const bool has_coordinates[] = {false, false, true};
  int count = 0;
  for (const std::string& data :
       {ROOT_DIR "ortools/routing/parsers/testdata/rc201.0",
        ROOT_DIR "ortools/routing/parsers/testdata/"
                 "n20w20.001.txt",
        ROOT_DIR "ortools/routing/parsers/testdata/"
                 "n20w20.002.txt"}) {
    TspTWParser parser;
    EXPECT_TRUE(parser.LoadFile(file::JoinPath(::testing::SrcDir(), data)));
    EXPECT_EQ(0, parser.depot());
    const int size = sizes[count];
    EXPECT_EQ(size, parser.size());
    double total_distances = 0;
    double total_times = 0;
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        total_distances += parser.distance_function()(i, j);
        total_times += parser.time_function()(i, j);
      }
    }
    EXPECT_NEAR(distances[count], total_distances, 1e-6);
    EXPECT_NEAR(times[count], total_times, 1e-6);
    EXPECT_EQ(service_times[count], parser.total_service_time());
    EXPECT_EQ(has_coordinates[count], !parser.coordinates().empty());
    double total_starts = 0;
    double total_ends = 0;
    for (int i = 0; i < size; ++i) {
      EXPECT_EQ(0, parser.service_times()[i]);
      total_starts += parser.time_windows()[i].start;
      total_ends += parser.time_windows()[i].end;
    }
    EXPECT_EQ(starts[count], total_starts);
    EXPECT_EQ(ends[count], total_ends);
    ++count;
  }
}

}  // namespace
}  // namespace operations_research::routing
