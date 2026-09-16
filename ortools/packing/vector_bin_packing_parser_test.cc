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

#include "ortools/packing/vector_bin_packing_parser.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/packing/vector_bin_packing.pb.h"
#include "ortools/util/data_path_resolver.h"

namespace operations_research {
namespace packing {
namespace vbp {
namespace {

TEST(VbpParserTest, Queen5_5) {
  const std::string full_path = ortools::GetDataDependencyFilepath(
      "ortools/packing/testdata/queen5_5.vbp");

  VbpParser parser;
  ASSERT_TRUE(parser.ParseFile(full_path)) << absl::StrCat(parser.problem());
  const VectorBinPackingProblem& problem = parser.problem();
  EXPECT_EQ(25, problem.resource_capacity_size());
  EXPECT_EQ(25, problem.item_size());
  for (const Item& item : problem.item()) {
    EXPECT_EQ(25, item.resource_usage_size());
    EXPECT_EQ(1, item.num_copies());
  }
}

TEST(VbpParserTest, BPPC_5_0_10) {
  const std::string full_path = ortools::GetDataDependencyFilepath(
      "ortools/packing/testdata/BPPC_5_0_10.vbp");

  VbpParser parser;
  ASSERT_TRUE(parser.ParseFile(full_path)) << absl::StrCat(parser.problem());
  const VectorBinPackingProblem& problem = parser.problem();
  EXPECT_EQ(61, problem.resource_capacity_size());
  EXPECT_EQ(60, problem.item_size());
  for (const Item& item : problem.item()) {
    EXPECT_EQ(61, item.resource_usage_size());
  }
}

}  // namespace
}  // namespace vbp
}  // namespace packing
}  // namespace operations_research
