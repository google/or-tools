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

#include "ortools/packing/binpacking_2d_parser.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/packing/multiple_dimensions_bin_packing.pb.h"
#include "ortools/util/data_path_resolver.h"

namespace operations_research {
namespace packing {
namespace {

TEST(BpParserText, LoadSecondDataSet) {
  const std::string full_path = ortools::GetDataDependencyFilepath(
      "ortools/packing/testdata/Class_01.2bp");
  BinPacking2dParser parser;
  ASSERT_TRUE(parser.Load2BPFile(full_path, 2));
  const MultipleDimensionsBinPackingProblem problem = parser.problem();
  EXPECT_EQ(20, problem.items_size());
  EXPECT_EQ(2, problem.box_shape().dimensions_size());
  EXPECT_EQ(10, problem.box_shape().dimensions(0));
  EXPECT_EQ(10, problem.box_shape().dimensions(1));
}

TEST(BpParserText, LoadWrongDataSet) {
  const std::string full_path = ortools::GetDataDependencyFilepath(
      "ortools/packing/testdata/Class_01.2bp");
  BinPacking2dParser parser;
  ASSERT_FALSE(parser.Load2BPFile(full_path, 4));
}

TEST(BpParserText, LoadWrongPath) {
  const std::string full_path = ortools::GetDataDependencyFilepath(
      "ortools/packing/testdata/Class_01.2bp_wrong");
  BinPacking2dParser parser;
  ASSERT_FALSE(parser.Load2BPFile(full_path, 2));
}

}  // namespace
}  // namespace packing
}  // namespace operations_research
