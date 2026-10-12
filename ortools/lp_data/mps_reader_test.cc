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

#include "ortools/lp_data/mps_reader.h"

#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/path.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/data_path_resolver.h"
#include "ortools/util/file_util.h"
#include "ortools/util/fp_utils_testing.h"
#include "re2/re2.h"

// TODO(user): remove the dependence on lp_solver.

namespace operations_research {
namespace glop {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::_;
using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr absl::string_view kMpsObjectiveOffsetModel = R"mps(
NAME
ROWS
 N  COST
RHS
    RHS     COST       -42
ENDATA
)mps";

TEST(MpsReaderTest, ObjectiveOffset) {
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       MpsDataToMPModelProto(kMpsObjectiveOffsetModel));
  EXPECT_THAT(model, EqualsProto("objective_offset: 42"));
}

TEST(MpsReaderTest, WindowsNewLineHandling) {
  const std::string mps_model_string =
      "NAME\r\n"
      "ROWS\r\n"
      " N  COST\r\n"
      "RHS\r\n"
      "    RHS     COST       -42\r\n"
      "ENDATA\r\n";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       MpsDataToMPModelProto(mps_model_string));
  EXPECT_THAT(model, EqualsProto("objective_offset: 42"));
}

constexpr absl::string_view kMpsWithIntegerVariablesFixedFormat = R"mps(
NAME          TESTPROB
ROWS
 N  COST
 L  LIM1
 G  LIM2
 E  MYEQN
COLUMNS
    X         COST                 1   LIM1                 1
    X         LIM2                 1
    Y         COST                 4   LIM1                 1
    Y         MYEQN               -1
    Z         COST                 9   LIM2                 1
    Z         MYEQN                1
    A         MYEQN                1
    MARK0000  'MARKER'      'INTORG'
    B         MYEQN                1
    C         MYEQN                1
    MARK0000  'MARKER'      'INTEND'
RHS
    RHS1      LIM1                 5   LIM2                10
    RHS1      MYEQN                7
BOUNDS
 UI BND1      X                    4
 LI BND1      Y                   -1
 LI BND1      C                    0
 BV BND1      Z
ENDATA)mps";

// This represents the same model as `kMpsWithIntegerVariablesFixedFormat` but
// in 'mixed' format, i.e. all of its lines, except the second to last, are in
// fixed format.
constexpr absl::string_view kMpsWithIntegerVariables = R"mps(
NAME          TESTPROB
ROWS
 N  COST
 L  LIM1
 G  LIM2
 E  MYEQN
COLUMNS
    X         COST                 1   LIM1                 1
    X         LIM2                 1
    Y         COST                 4   LIM1                 1
    Y         MYEQN               -1
    Z         COST                 9   LIM2                 1
    Z         MYEQN                1
    A         MYEQN                1
    MARK0000  'MARKER'      'INTORG'
    B         MYEQN                1
    C         MYEQN                1
    MARK0000  'MARKER'      'INTEND'
RHS
    RHS1      LIM1                 5   LIM2                10
    RHS1      MYEQN                7
BOUNDS
 UI BND1      X                    4
 LI BND1      Y                   -1
 LI BND1      C                    0
 BV BND1 Z
ENDATA)mps";

TEST(MpsReaderTest, IntegerVariableTestFixedFormatAsFree) {
  ASSERT_OK_AND_ASSIGN(
      LinearProgram linear_program,
      MpsDataToLinearProgram(kMpsWithIntegerVariablesFixedFormat));

  // Test the name <-> index correspondence.
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(0)), "X");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(1)), "Y");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(2)), "Z");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(3)), "A");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(4)), "B");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(5)), "C");

  // Test that the integrality is set correctly.
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(0)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(1)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(2)));
  EXPECT_FALSE(linear_program.IsVariableInteger(ColIndex(3)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(4)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(5)));

  // Test that the binary integer variable Z has the correct bounds.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(2)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(2)]);

  // Test that the integer variable B is binary by default.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(4)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(4)]);

  // Test that the integer variable C has no upper bound.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(5)]);
  EXPECT_EQ(kInfinity, linear_program.variable_upper_bounds()[ColIndex(5)]);
}

TEST(MpsReaderTest, IntegerVariableTestFixedFormat) {
  const std::string mps_data = std::string(kMpsWithIntegerVariablesFixedFormat);
  ASSERT_OK_AND_ASSIGN(LinearProgram linear_program,
                       MpsDataToLinearProgram(mps_data));

  // Test the name <-> index correspondence.
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(0)), "X");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(1)), "Y");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(2)), "Z");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(3)), "A");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(4)), "B");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(5)), "C");

  // Test that the integrality is set correctly.
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(0)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(1)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(2)));
  EXPECT_FALSE(linear_program.IsVariableInteger(ColIndex(3)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(4)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(5)));

  // Test that the binary integer variable Z has the correct bounds.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(2)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(2)]);

  // Test that the integer variable B is binary by default.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(4)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(4)]);

  // Test that the integer variable C has no upper bound.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(5)]);
  EXPECT_EQ(kInfinity, linear_program.variable_upper_bounds()[ColIndex(5)]);
}

TEST(MpsReaderTest, IntegerVariableTest) {
  const std::string mps_data = std::string(kMpsWithIntegerVariables);
  ASSERT_OK_AND_ASSIGN(LinearProgram linear_program,
                       MpsDataToLinearProgram(mps_data));

  // Test the name <-> index correspondence.
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(0)), "X");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(1)), "Y");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(2)), "Z");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(3)), "A");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(4)), "B");
  EXPECT_EQ(linear_program.GetVariableName(ColIndex(5)), "C");

  // Test that the integrality is set correctly.
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(0)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(1)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(2)));
  EXPECT_FALSE(linear_program.IsVariableInteger(ColIndex(3)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(4)));
  EXPECT_TRUE(linear_program.IsVariableInteger(ColIndex(5)));

  // Test that the binary integer variable Z has the correct bounds.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(2)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(2)]);

  // Test that the integer variable B is binary by default.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(4)]);
  EXPECT_EQ(1.0, linear_program.variable_upper_bounds()[ColIndex(4)]);

  // Test that the integer variable C has no upper bound.
  EXPECT_EQ(0.0, linear_program.variable_lower_bounds()[ColIndex(5)]);
  EXPECT_EQ(kInfinity, linear_program.variable_upper_bounds()[ColIndex(5)]);
}

TEST(MpsReaderTest, TabTest) {
  const std::string mps_data =
      "NAME          TESTPROB\n"
      "ROWS\n"
      " N  COST\n"
      " L  LIM1\n"
      "COLUMNS\n"
      "    X         COST                 1   LIM1                 1\n"
      "BOUNDS\n"
      " UI BND1      X\t4\n"
      "ENDATA\n";
  ASSERT_OK(MpsDataToLinearProgram(mps_data));
}

void TestMPSWithParameters(const std::string& bound_text, bool in_int_marker,
                           double lower_bound, double upper_bound,
                           bool is_integer) {
  const std::vector<std::string> split = absl::StrSplit(bound_text, ' ');
  EXPECT_EQ(0, split.size() % 2);
  std::string bound_row;
  for (int i = 0; i < split.size(); i += 2) {
    if (split[i].empty()) continue;
    absl::StrAppendFormat(&bound_row, " %-2s %-8s  %-8s  %-12s\n", split[i],
                          "BND", "VAR", split[i + 1]);
  }
  const std::string mps_header =
      "NAME          TESTPROB\n"
      "ROWS\n"
      " N  COST\n"
      "COLUMNS\n";
  const std::string mps_data = absl::StrCat(
      mps_header, in_int_marker ? "    MARK0000  'MARKER'     'INTORG'\n" : "",
      "    VAR       COST                1\n"
      "    VAR       MYEQN               1\n",
      in_int_marker ? "    MARK0000  'MARKER'     'INTEND'\n" : "",
      "RHS\n"
      "    RHS1      MYEQN               7\n"
      "BOUNDS\n",
      bound_row, "ENDATA\n");
  ASSERT_OK_AND_ASSIGN(LinearProgram linear_program,
                       MpsDataToLinearProgram(mps_data));
  // Test that the integrality is set correctly.
  EXPECT_EQ(is_integer, linear_program.IsVariableInteger(ColIndex(0)))
      << bound_row;
  EXPECT_EQ(lower_bound, linear_program.variable_lower_bounds().front())
      << bound_row;
  EXPECT_EQ(upper_bound, linear_program.variable_upper_bounds().front())
      << bound_row;
}

TEST(MpsReaderTest, VariableBoundingTest) {
  TestMPSWithParameters(" ", false, 0, kInfinity, false);
  TestMPSWithParameters("LO -1.5", false, -1.5, kInfinity, false);
  TestMPSWithParameters("UP 4.5", false, 0, 4.5, false);
  TestMPSWithParameters("LO -1.5 UP 4.5", false, -1.5, 4.5, false);
  TestMPSWithParameters("FX -1.5", false, -1.5, -1.5, false);
  TestMPSWithParameters("FR ", false, -kInfinity, kInfinity, false);
  TestMPSWithParameters("MI ", false, -kInfinity, kInfinity, false);
  TestMPSWithParameters("PL ", false, 0, kInfinity, false);
  TestMPSWithParameters("MI PL", false, -kInfinity, kInfinity, false);
  TestMPSWithParameters("LI -1.5", false, -1.5, kInfinity, true);
  TestMPSWithParameters("UI 4.5", false, 0, 4.5, true);
  TestMPSWithParameters("LI -1.5 UI 4.5", false, -1.5, 4.5, true);
  TestMPSWithParameters("BV ", false, 0, 1, true);
  TestMPSWithParameters(" ", true, 0, 1, true);
  TestMPSWithParameters("LO -1.5", true, -1.5, kInfinity, true);
  TestMPSWithParameters("UP 4.5", true, 0, 4.5, true);
  TestMPSWithParameters("LO -1.5 UP 4.5", true, -1.5, 4.5, true);
  TestMPSWithParameters("FX -1.5", true, -1.5, -1.5, true);
  TestMPSWithParameters("FR ", true, -kInfinity, kInfinity, true);
  TestMPSWithParameters("MI ", true, -kInfinity, kInfinity, true);
  TestMPSWithParameters("PL ", true, 0, kInfinity, true);
  TestMPSWithParameters("LI -1.5", true, -1.5, kInfinity, true);
  TestMPSWithParameters("LI 0", true, 0, kInfinity, true);
  TestMPSWithParameters("UI 4.5", true, 0, 4.5, true);
  TestMPSWithParameters("LI -1.5 UI 4.5", true, -1.5, 4.5, true);
  TestMPSWithParameters("BV ", true, 0, 1, true);
  // TODO(user): test more composite cases, including ones that could generate
  // errors. This is not done here are there is no document on this.
  // For example, what should happen with FX 4 and LO 7?
  // What should happen with PL and UP -1.5? MI and LO 4.5? etc...
}

void ReplaceNthWordWith(int n, absl::string_view replacement,
                        std::string* text) {
  RE2::Replace(text, absl::StrCat("^(\\s*(?:[^\\s]+\\s+){", n, "})[^\\s]+"),
               absl::StrCat("\\1", replacement));
}

// Because when the tests are not trivial, we need to test the test.
TEST(MpsReaderTest, ReplaceNthWordWithTest) {
  constexpr char kMutation[] = "BANANA!";
  constexpr char kLine[] = "   X4    ROW3     7.31      ROW7       7.31";
  constexpr const char* kExpectedLines[]{
      "   BANANA!    ROW3     7.31      ROW7       7.31",
      "   X4    BANANA!     7.31      ROW7       7.31",
      "   X4    ROW3     BANANA!      ROW7       7.31",
      "   X4    ROW3     7.31      BANANA!       7.31",
      "   X4    ROW3     7.31      ROW7       BANANA!",
  };

  for (int i = 0; i < std::size(kExpectedLines); ++i) {
    std::string mutated_line(kLine);
    ReplaceNthWordWith(i, kMutation, &mutated_line);
    EXPECT_EQ(mutated_line, kExpectedLines[i]);
  }
}

TEST(MpsReaderTest, SemiContinuous) {
  const std::string mps_data =
      "NAME          TestSemiContinuous\n"
      "ROWS\n"
      " N  obj1\n"
      "COLUMNS\n"
      "    x1         obj1                 1\n"
      "    x2         obj1                 1\n"
      "BOUNDS\n"
      " SC bnd       x1                       10000\n"
      " LO bnd       x1                       200000\n"
      " SC bnd       x2                       300\n"
      "ENDATA\n";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  const MPModelProto expected_proto = ParseTestProto(R"pb(
    variable {
      lower_bound: 0
      upper_bound: 10000
      objective_coefficient: 1
      name: "x1"
    }
    variable {
      lower_bound: 0
      upper_bound: 300
      objective_coefficient: 1
      name: "x2"
    }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    constraint {
      lower_bound: 0
      upper_bound: inf
      var_index: 0
      var_index: 2
      coefficient: 1
      coefficient: -200000
    }
    constraint {
      lower_bound: -inf
      upper_bound: 0
      var_index: 0
      var_index: 2
      coefficient: 1
      coefficient: -10000
    }
    constraint {
      lower_bound: 0
      upper_bound: inf
      var_index: 1
      var_index: 3
      coefficient: 1
      coefficient: -1
    }
    constraint {
      lower_bound: -inf
      upper_bound: 0
      var_index: 1
      var_index: 3
      coefficient: 1
      coefficient: -300
    }
    name: "TestSemiContinuous"
  )pb");
  EXPECT_THAT(model, testing::EqualsProto(expected_proto));
}

TEST(MpsReaderTest, MissingFile) {
  LinearProgram data;
  ASSERT_THAT(
      glop::MpsFileToLinearProgram(file::JoinPathRespectAbsolute(
          ::testing::SrcDir(), "missing_file.mps")),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr("missing_file.mps")));
}

TEST(MpsReaderTest, ObjsenseMaxFixedFormat) {
  const std::string mps_data = R"mps(
OBJSENSE
 MAX
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: true"));
}

TEST(MpsReaderTest, ObjsenseMinFixedFormat) {
  const std::string mps_data = R"mps(
OBJSENSE
 MIN
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: false"));
}

TEST(MpsReaderTest, ObjsenseMaxFreeFormat) {
  const std::string mps_data = R"mps(
OBJSENSE
 MAX
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: true"));
}

TEST(MpsReaderTest, ObjsenseMinFreeFormat) {
  const std::string mps_data = R"mps(
OBJSENSE
 MIN
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: false"));
}

TEST(MpsReaderTest, ObjsenseMaxSingleLine) {
  const std::string mps_data = R"mps(
OBJSENSE MAX
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: true"));
}

TEST(MpsReaderTest, ObjsenseMinSingleLine) {
  const std::string mps_data = R"mps(
OBJSENSE MIN
ENDATA
)mps";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("maximize: false"));
}

TEST(MpsReaderTest, NameWithSpacesInFreeFormat) {
  const std::string mps_data = "NAME         My Problem Phrase Name Here";
  ASSERT_OK_AND_ASSIGN(MPModelProto model,
                       glop::MpsDataToMPModelProto(mps_data));
  EXPECT_THAT(model, EquivToProto("name: \"My\""));
}
}  // namespace
}  // namespace glop
}  // namespace operations_research
