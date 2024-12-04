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

#include "ortools/sat/zero_half_cuts.h"

#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(SymmetricDifferenceTest, BasicExample) {
  ZeroHalfCutHelper helper;
  std::vector<int> a = {2, 1, 4};
  std::vector<int> b = {4, 3, 2, 7};
  helper.Reset(10);
  helper.SymmetricDifference(a, &b);
  EXPECT_THAT(b, ElementsAre(3, 7, 1));
}

TEST(SymmetricDifferenceTest, BasicExample2) {
  ZeroHalfCutHelper helper;
  std::vector<int> a = {2, 1, 4};
  std::vector<int> b = {};
  helper.Reset(10);
  helper.SymmetricDifference(a, &b);
  EXPECT_THAT(b, ElementsAre(2, 1, 4));
}

TEST(EliminateVarUsingRowTest, BasicExample) {
  // We need to construct a binary matrix for this test.
  ZeroHalfCutHelper helper;
  helper.ProcessVariables({0.0, 0.0, 0.0, 0.0, 0.12, 0.0, 0.0, 0.0, 0.0},
                          std::vector<IntegerValue>(9, IntegerValue(0)),
                          std::vector<IntegerValue>(9, IntegerValue(1)));
  helper.AddBinaryRow({{{glop::RowIndex(1), IntegerValue(1)}},
                       {0, 2, 3, 4, 7},
                       /*rhs*/ 1,
                       /*slack*/ 0.1});
  helper.AddBinaryRow({{{glop::RowIndex(2), IntegerValue(1)}},
                       {0, 2, 3, 4, 7},
                       /*rhs*/ 0,
                       /*slack*/ 0.0});
  helper.AddBinaryRow({{{glop::RowIndex(1), IntegerValue(1)},
                        {glop::RowIndex(3), IntegerValue(1)}},
                       {0, 5, 4, 8},
                       /*rhs*/ 1,
                       /*slack*/ 0.0});

  typedef std::vector<std::pair<glop::RowIndex, IntegerValue>> MultiplierType;
  typedef std::vector<int> VectorType;

  // Let use row with index 2 to eliminate the variable 4.
  helper.EliminateVarUsingRow(4, 2);

  // The multipliers, cols and parity behave like a xor.
  EXPECT_EQ(helper.MatrixRow(0).multipliers,
            MultiplierType({{glop::RowIndex(3), IntegerValue(1)}}));
  EXPECT_EQ(helper.MatrixRow(0).cols, VectorType({2, 3, 7, 5, 8}));
  EXPECT_EQ(helper.MatrixRow(0).rhs_parity, 0);
  EXPECT_EQ(helper.MatrixRow(0).slack, 0.1);

  EXPECT_EQ(helper.MatrixRow(1).multipliers,
            MultiplierType({{glop::RowIndex(1), IntegerValue(1)},
                            {glop::RowIndex(2), IntegerValue(1)},
                            {glop::RowIndex(3), IntegerValue(1)}}));
  EXPECT_EQ(helper.MatrixRow(1).cols, VectorType({2, 3, 7, 5, 8}));
  EXPECT_EQ(helper.MatrixRow(1).rhs_parity, 1);
  EXPECT_EQ(helper.MatrixRow(1).slack, 0.0);

  // The column is eliminated like a singleton column and the lp value become
  // the slack.
  EXPECT_EQ(helper.MatrixRow(2).multipliers,
            MultiplierType({{glop::RowIndex(1), IntegerValue(1)},
                            {glop::RowIndex(3), IntegerValue(1)}}));
  EXPECT_EQ(helper.MatrixRow(2).cols, VectorType({5, 8}));
  EXPECT_EQ(helper.MatrixRow(2).rhs_parity, 1);
  EXPECT_EQ(helper.MatrixRow(2).slack, 0.12);

  // The transposed information is up to date.
  EXPECT_THAT(helper.MatrixCol(0), IsEmpty());
  EXPECT_THAT(helper.MatrixCol(1), IsEmpty());
  EXPECT_THAT(helper.MatrixCol(2), UnorderedElementsAre(0, 1));
  EXPECT_THAT(helper.MatrixCol(3), UnorderedElementsAre(0, 1));
  EXPECT_THAT(helper.MatrixCol(4), IsEmpty());
  EXPECT_THAT(helper.MatrixCol(5), UnorderedElementsAre(0, 1, 2));
  EXPECT_THAT(helper.MatrixCol(6), IsEmpty());
  EXPECT_THAT(helper.MatrixCol(7), UnorderedElementsAre(0, 1));
  EXPECT_THAT(helper.MatrixCol(8), UnorderedElementsAre(0, 1, 2));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
