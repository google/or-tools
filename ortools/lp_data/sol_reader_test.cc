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

#include "ortools/lp_data/sol_reader.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/data_path_resolver.h"
#include "ortools/util/file_util.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr char kMpModel[] = R"pb(
  variable {
    lower_bound: -10
    upper_bound: 10
    objective_coefficient: 1
    is_integer: true
    name: "x"
  }
  variable {
    lower_bound: -10
    upper_bound: 10
    objective_coefficient: 2
    name: "y"
  }
)pb";

TEST(SolReaderTest, PartialSolFile) {
  constexpr char kSolution[] = "y  -7.31";
  constexpr char kMpSolution[] = R"pb(
    variable_value: 0 variable_value: -7.31
  )pb";
  MPModelProto model = ParseTestProto(kMpModel);

  EXPECT_THAT(ParseSolString(kSolution, model),
              IsOkAndHolds(EqualsProto(kMpSolution)));
}

TEST(SolReaderTest, InvalidSolFileUnknownVariable) {
  constexpr char kSolution[] = "z  -7.31";
  MPModelProto model = ParseTestProto(kMpModel);

  EXPECT_THAT(ParseSolString(kSolution, model),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolReaderTest, InvalidSolFileTooFewFields) {
  constexpr char kSolution[] = "x  ";
  MPModelProto model = ParseTestProto(kMpModel);

  EXPECT_THAT(ParseSolString(kSolution, model),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolReaderTest, InvalidSolFileTooManyFields) {
  constexpr char kSolution[] = "x  -7.31  42 ";
  MPModelProto model = ParseTestProto(kMpModel);

  EXPECT_THAT(ParseSolString(kSolution, model),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolReaderTest, InvalidSolFileNonNumericalValue) {
  constexpr char kSolution[] = "x  Marvin ";
  MPModelProto model = ParseTestProto(kMpModel);

  EXPECT_THAT(ParseSolString(kSolution, model),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace operations_research
