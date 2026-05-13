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

#include "ortools/pdlp/quadratic_program.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::google::protobuf::util::ParseTextOrDie;
using ::operations_research::pdlp::internal::CombineRepeatedTripletsInPlace;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::PrintToString;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

const double kInfinity = std::numeric_limits<double>::infinity();

TEST(QuadraticProgram, DefaultConstructorWorks) { QuadraticProgram qp; }

TEST(QuadraticProgram, MoveConstructor) {
  QuadraticProgram qp1 = TestDiagonalQp1();
  QuadraticProgram qp2(std::move(qp1));
  VerifyTestDiagonalQp1(qp2);
}

TEST(QuadraticProgram, MoveAssignment) {
  QuadraticProgram qp1 = TestDiagonalQp1();
  QuadraticProgram qp2;
  qp2 = std::move(qp1);
  VerifyTestDiagonalQp1(qp2);
}

TEST(ValidateQuadraticProgramDimensions, ValidProblem) {
  const absl::Status status =
      ValidateQuadraticProgramDimensions(TestDiagonalQp1());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidateQuadraticProgramDimensions, ValidProblemWithNames) {
  QuadraticProgram qp = TestDiagonalQp1();
  qp.variable_names = {"x0", "x1"};
  qp.constraint_names = {"c0"};
  const absl::Status status =
      ValidateQuadraticProgramDimensions(TestDiagonalQp1());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidateQuadraticProgramDimensions, ConstraintLowerBoundsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.constraint_lower_bounds.resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ConstraintUpperBoundsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.constraint_upper_bounds.resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ObjectiveVectorInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.objective_vector.resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, VariableLowerBoundsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.variable_lower_bounds.resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, VariableUpperBoundsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.variable_upper_bounds.resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ConstraintMatrixRowsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.constraint_matrix.resize(10, 2);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ConstraintMatrixColsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.constraint_matrix.resize(2, 10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ObjectiveMatrixRowsInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.objective_matrix.emplace();
  qp.objective_matrix->resize(10);
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, VariableNamesInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.variable_names = {"x0"};
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateQuadraticProgramDimensions, ConstraintNamesInconsistent) {
  QuadraticProgram qp;
  qp.ResizeAndInitialize(/*num_variables=*/2, /*num_constraints=*/3);
  qp.constraint_names = {"c0"};
  EXPECT_EQ(ValidateQuadraticProgramDimensions(qp).code(),
            absl::StatusCode::kInvalidArgument);
}

class ConvertQpMpModelProtoTest : public testing::TestWithParam<bool> {};

// The LP:
//   optimize 5.5 x_0 + 2 x_1 - x_2 + x_3 - 14 s.t.
//     2 x_0 +   x_1 +     x_2 + 2 x_3  = 12
//       x_0 +             x_2          >=  7
//   3.5 x_0                            <=  -4
//               -1 <= 1.5 x_2 -   x_3  <= 1
//   -infinity <= x_0 <= infinity
//          -2 <= x_1 <= infinity
//   -infinity <= x_2 <= 6
//         2.5 <= x_3 <= 3.5
MPModelProto TestLpProto(bool maximize) {
  auto proto = ParseTextOrDie<MPModelProto>(R"pb(variable {
                                                   lower_bound: -inf
                                                   upper_bound: inf
                                                   objective_coefficient: 5.5
                                                 }
                                                 variable {
                                                   lower_bound: -2
                                                   upper_bound: inf
                                                   objective_coefficient: -2
                                                 }
                                                 variable {
                                                   lower_bound: -inf
                                                   upper_bound: 6
                                                   objective_coefficient: -1
                                                 }
                                                 variable {
                                                   lower_bound: 2.5
                                                   upper_bound: 3.5
                                                   objective_coefficient: 1
                                                 }
                                                 constraint {
                                                   lower_bound: 12
                                                   upper_bound: 12
                                                   var_index: [ 0, 1, 2, 3 ]
                                                   coefficient: [ 2, 1, 1, 2 ]
                                                 }
                                                 constraint {
                                                   lower_bound: -inf
                                                   upper_bound: 7
                                                   var_index: [ 0, 2 ]
                                                   coefficient: [ 1, 1 ]
                                                 }
                                                 constraint {
                                                   lower_bound: -4
                                                   upper_bound: inf
                                                   var_index: [ 0 ]
                                                   coefficient: [ 4 ]
                                                 }
                                                 constraint {
                                                   lower_bound: -1
                                                   upper_bound: 1
                                                   var_index: [ 2, 3 ]
                                                   coefficient: [ 1.5, -1 ]
                                                 }
                                                 objective_offset: -14)pb");
  proto.set_maximize(maximize);
  return proto;
}

// This is tested for both minimization and maximization.
TEST_P(ConvertQpMpModelProtoTest, LpFromMpModelProto) {
  const bool maximize = GetParam();
  MPModelProto proto = TestLpProto(maximize);
  const auto lp = QpFromMpModelProto(proto, /*relax_integer_variables=*/false);
  ASSERT_TRUE(lp.ok()) << lp.status();

  VerifyTestLp(*lp, maximize);
}

// The QP:
//   optimize x_0^2 + x_1^2 + 3 x_0 - 4 s.t.
//       x_0 + x_1          <= 42
//   -1 <= x_0 <= 2
//          -2 <= x_1 <= 3
MPModelProto TestQpProto(bool maximize) {
  auto proto = ParseTextOrDie<MPModelProto>(
      R"pb(variable { lower_bound: -1 upper_bound: 2 objective_coefficient: 3 }
           variable { lower_bound: -2 upper_bound: 3 objective_coefficient: 0 }
           constraint {
             lower_bound: -inf
             upper_bound: 42
             var_index: [ 0, 1 ]
             coefficient: [ 1, 1 ]
           }
           objective_offset: -4
           quadratic_objective {
             qvar1_index: [ 0, 1 ]
             qvar2_index: [ 0, 1 ]
             coefficient: [ 1, 1 ]
           }
      )pb");
  proto.set_maximize(maximize);
  return proto;
}

// This is tested for both minimization and maximization.
TEST_P(ConvertQpMpModelProtoTest, QpFromMpModelProto) {
  const bool maximize = GetParam();
  MPModelProto proto = TestQpProto(maximize);
  const auto qp = QpFromMpModelProto(proto, /*relax_integer_variables=*/false);
  ASSERT_TRUE(qp.ok()) << qp.status();

  EXPECT_THAT(qp->constraint_lower_bounds, ElementsAre(-kInfinity));
  EXPECT_THAT(qp->constraint_upper_bounds, ElementsAre(42));
  EXPECT_THAT(qp->variable_lower_bounds, ElementsAre(-1, -2));
  EXPECT_THAT(qp->variable_upper_bounds, ElementsAre(2, 3));
  EXPECT_THAT(ToDense(qp->constraint_matrix), EigenArrayEq<double>({{1, 1}}));
  EXPECT_TRUE(qp->constraint_matrix.isCompressed());

  double sign = maximize ? -1 : 1;
  EXPECT_EQ(sign * qp->objective_offset, -4);
  EXPECT_EQ(qp->objective_scaling_factor, sign);
  EXPECT_THAT(sign * qp->objective_vector, ElementsAre(3, 0));
  EXPECT_THAT(sign * (qp->objective_matrix->diagonal()),
              EigenArrayEq<double>({2, 2}));
}

TEST(QpFromMpModelProto, ErrorsOnOffDiagonalTerms) {
  auto proto = ParseTextOrDie<MPModelProto>(
      R"pb(variable { lower_bound: -1 upper_bound: 2 objective_coefficient: 3 }
           variable { lower_bound: -2 upper_bound: 3 objective_coefficient: 0 }
           constraint {
             lower_bound: -inf
             upper_bound: 42
             var_index: [ 0, 1 ]
             coefficient: [ 1, 1 ]
           }
           objective_offset: -4
           quadratic_objective {
             qvar1_index: [ 0 ]
             qvar2_index: [ 1 ]
             coefficient: [ 1 ]
           }
      )pb");
  EXPECT_EQ(QpFromMpModelProto(proto, /*relax_integer_variables=*/false)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(CanFitInMpModelProto, SmallQpOk) {
  // `QpFromMpModelProtoTest` verifies that `qp` is as expected.
  const auto qp = QpFromMpModelProto(TestQpProto(/*maximize=*/false),
                                     /*relax_integer_variables=*/false);
  ASSERT_TRUE(qp.ok()) << qp.status();
  EXPECT_TRUE(CanFitInMpModelProto(*qp).ok());
}

// The ILP:
//   optimize x_0 + 2 * x_1 s.t.
//       x_0 + x_1 <= 1
//       -1 <= x_0 <= 2
//       -2 <= x_1 <= 3
//       x_1 integer
// This is tested for both minimization and maximization.
TEST_P(ConvertQpMpModelProtoTest, IntegerVariablesFromMpModelProto) {
  const bool maximize = GetParam();
  auto proto = ParseTextOrDie<MPModelProto>(
      R"pb(variable { lower_bound: -1 upper_bound: 2 objective_coefficient: 1 }
           variable {
             lower_bound: -2
             upper_bound: 3
             objective_coefficient: 2
             is_integer: true
           }
           constraint {
             lower_bound: -inf
             upper_bound: 1
             var_index: [ 0, 1 ]
             coefficient: [ 1, 1 ]
           }
      )pb");
  proto.set_maximize(maximize);
  EXPECT_EQ(QpFromMpModelProto(proto, /*relax_integer_variables=*/false)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
  const auto lp = QpFromMpModelProto(proto, /*relax_integer_variables=*/true);
  ASSERT_TRUE(lp.ok()) << lp.status();

  EXPECT_THAT(lp->constraint_lower_bounds, ElementsAre(-kInfinity));
  EXPECT_THAT(lp->constraint_upper_bounds, ElementsAre(1));
  EXPECT_THAT(lp->variable_lower_bounds, ElementsAre(-1, -2));
  EXPECT_THAT(lp->variable_upper_bounds, ElementsAre(2, 3));
  EXPECT_THAT(ToDense(lp->constraint_matrix), EigenArrayEq<double>({{1, 1}}));
  EXPECT_TRUE(lp->constraint_matrix.isCompressed());

  double sign = maximize ? -1 : 1;
  EXPECT_EQ(lp->objective_offset, 0);
  EXPECT_THAT(sign * lp->objective_vector, ElementsAre(1, 2));
  EXPECT_FALSE(lp->objective_matrix.has_value());
}

MPModelProto TinyModelWithNames() {
  return ParseTextOrDie<MPModelProto>(
      R"pb(name: "problem"
           variable {
             name: "x_0"
             lower_bound: -1
             upper_bound: 2
             objective_coefficient: 1
           }
           variable {
             name: "x_1"
             lower_bound: -2
             upper_bound: 3
             objective_coefficient: 2
           }
           constraint {
             name: "c_0"
             lower_bound: -inf
             upper_bound: 1
             var_index: [ 0, 1 ]
             coefficient: [ 1, 1 ]
           }
      )pb");
}

TEST(QpFromMpModelProtoTest, EmptyQp) {
  MPModelProto proto;
  const auto qp = QpFromMpModelProto(proto, /*relax_integer_variables=*/false);
  ASSERT_TRUE(qp.ok()) << qp.status();

  EXPECT_THAT(qp->constraint_lower_bounds, ElementsAre());
  EXPECT_THAT(qp->constraint_upper_bounds, ElementsAre());
  EXPECT_THAT(qp->variable_lower_bounds, ElementsAre());
  EXPECT_THAT(qp->variable_upper_bounds, ElementsAre());
  EXPECT_EQ(qp->constraint_matrix.cols(), 0);
  EXPECT_EQ(qp->constraint_matrix.rows(), 0);
  EXPECT_EQ(qp->objective_offset, 0);
  EXPECT_EQ(qp->objective_scaling_factor, 1);
  EXPECT_FALSE(qp->objective_matrix.has_value());
  EXPECT_THAT(qp->objective_vector, ElementsAre());
}

TEST(QpFromMpModelProtoTest, DoesNotIncludeNames) {
  const auto lp =
      QpFromMpModelProto(TinyModelWithNames(), /*relax_integer_variables=*/true,
                         /*include_names=*/false);
  ASSERT_TRUE(lp.ok()) << lp.status();
  EXPECT_EQ(lp->problem_name, std::nullopt);
  EXPECT_EQ(lp->variable_names, std::nullopt);
  EXPECT_EQ(lp->constraint_names, std::nullopt);
}

TEST(QpFromMpModelProtoTest, IncludesNames) {
  const auto lp =
      QpFromMpModelProto(TinyModelWithNames(), /*relax_integer_variables=*/true,
                         /*include_names=*/true);
  ASSERT_TRUE(lp.ok()) << lp.status();
  EXPECT_THAT(lp->problem_name, Optional(Eq("problem")));
  EXPECT_THAT(lp->variable_names, Optional(ElementsAre("x_0", "x_1")));
  EXPECT_THAT(lp->constraint_names, Optional(ElementsAre("c_0")));
}

INSTANTIATE_TEST_SUITE_P(
    ConvertQpMpModelProtoTests, ConvertQpMpModelProtoTest, testing::Bool(),
    [](const testing::TestParamInfo<ConvertQpMpModelProtoTest::ParamType>&
           info) {
      if (info.param) {
        return "maximize";
      } else {
        return "minimize";
      }
    });

TEST(QuadraticProgramToStringTest, TestLpIsCorrect) {
  QuadraticProgram qp = TestLp();
  EXPECT_THAT(ToString(qp),
              StrEq("minimize 1 * (-14 + 5.5 x0 + -2 x1 + -1 x2 + 1 x3)\n"
                    "c0: 12 <= + 2 x0 + 1 x1 + 1 x2 + 2 x3 <= 12\n"
                    "c1: + 1 x0 + 1 x2 <= 7\n"
                    "c2: -4 <= + 4 x0\n"
                    "c3: -1 <= + 1.5 x2 + -1 x3 <= 1\n"
                    "Bounds\n"
                    "x0 free\n"
                    "x1 >= -2\n"
                    "x2 <= 6\n"
                    "2.5 <= x3 <= 3.5\n"));
}

TEST(QuadraticProgramToStringTest, TestLpIsCorrectWithMaximization) {
  QuadraticProgram qp = TestLp();
  qp.objective_scaling_factor = -1;
  EXPECT_THAT(ToString(qp),
              StrEq("maximize -1 * (-14 + 5.5 x0 + -2 x1 + -1 x2 + 1 x3)\n"
                    "c0: 12 <= + 2 x0 + 1 x1 + 1 x2 + 2 x3 <= 12\n"
                    "c1: + 1 x0 + 1 x2 <= 7\n"
                    "c2: -4 <= + 4 x0\n"
                    "c3: -1 <= + 1.5 x2 + -1 x3 <= 1\n"
                    "Bounds\n"
                    "x0 free\n"
                    "x1 >= -2\n"
                    "x2 <= 6\n"
                    "2.5 <= x3 <= 3.5\n"));
}

TEST(QuadraticProgramToStringTest, TestLpTruncatesCorrectly) {
  QuadraticProgram qp = TestLp();
  EXPECT_THAT(ToString(qp, 100),
              AllOf(SizeIs(100), EndsWith("...\n"),
                    StrEq("minimize 1 * (-14 + 5.5 x0 + -2 x1 + -1 x2 + 1 x3)\n"
                          "c0: 12 <= + 2 x0 + 1 x1 + 1 x2 + 2 x3 <= 12\n"
                          "c...\n")));
}

TEST(QuadraticProgramToStringTest, TestDiagonalQp1IsCorrect) {
  QuadraticProgram qp = TestDiagonalQp1();
  EXPECT_THAT(
      ToString(qp),
      StrEq("minimize 1 * (5 + -1 x0 + -1 x1 + 1/2 * ( + 4 x0^2 + 1 x1^2))\n"
            "c0: + 1 x0 + 1 x1 <= 1\n"
            "Bounds\n"
            "1 <= x0 <= 2\n"
            "-2 <= x1 <= 4\n"));
}

TEST(QuadraticProgramToStringTest, UsesVariableAndConstraintNames) {
  QuadraticProgram qp = TestDiagonalQp1();
  qp.problem_name = "test";
  qp.variable_names = {"x", "y"};
  qp.constraint_names = {"total"};
  EXPECT_THAT(
      ToString(qp),
      StrEq("test:\n"
            "minimize 1 * (5 + -1 x + -1 y + 1/2 * ( + 4 x^2 + 1 y^2))\n"
            "total: + 1 x + 1 y <= 1\n"
            "Bounds\n"
            "1 <= x <= 2\n"
            "-2 <= y <= 4\n"));
}

TEST(QuadraticProgramToStringTest, InvalidLpVectorSizes) {
  QuadraticProgram qp = TestLp();
  qp.variable_lower_bounds.resize(3);
  EXPECT_THAT(ToString(qp),
              StartsWith("Quadratic program with inconsistent dimensions: "));
}

// A matcher for Eigen Triplets.
MATCHER_P3(IsEigenTriplet, row, col, value,
           std::string(negation ? "isn't" : "is") + " the triplet " +
               PrintToString(row) + "," + PrintToString(col) + "=" +
               PrintToString(value)) {
  return arg.row() == row && arg.col() == col && arg.value() == value;
}

TEST(CombineRepeatedTripletsInPlace, HandlesEmptyTriplets) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets;
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets, IsEmpty());
}

TEST(CombineRepeatedTripletsInPlace, CorrectForSingleTriplet) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {{1, 2, 3.0}};
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets, ElementsAre(IsEigenTriplet(1, 2, 3.0)));
}

TEST(CombineRepeatedTripletsInPlace, CorrectForDistinctTriplets) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {1, 2, 3.0}, {2, 1, 1.0}, {1, 1, 0.0}};
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets,
              ElementsAre(IsEigenTriplet(1, 2, 3.0), IsEigenTriplet(2, 1, 1.0),
                          IsEigenTriplet(1, 1, 0.0)));
}

TEST(CombineRepeatedTripletsInPlace, CombinesDuplicatesAtStart) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {1, 2, 3.0}, {1, 2, -1.0}, {1, 1, 0.0}};
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets, ElementsAre(IsEigenTriplet(1, 2, 2.0),
                                    IsEigenTriplet(1, 1, 0.0)));
}

TEST(CombineRepeatedTripletsInPlace, CombinesDuplicatesAtEnd) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {1, 2, 3.0}, {2, 1, 1.0}, {2, 1, 1.0}};
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets, ElementsAre(IsEigenTriplet(1, 2, 3.0),
                                    IsEigenTriplet(2, 1, 2.0)));
}

TEST(CombineRepeatedTripletsInPlace, CombinesToSingleton) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {1, 2, 3.0}, {1, 2, 1.0}, {1, 2, 2.0}};
  CombineRepeatedTripletsInPlace(triplets);
  EXPECT_THAT(triplets, ElementsAre(IsEigenTriplet(1, 2, 6.0)));
}

TEST(SetEigenMatrixFromTriplets, HandlesEmptyMatrix) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets;
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> matrix(2, 2);
  SetEigenMatrixFromTriplets(std::move(triplets), matrix);
  EXPECT_THAT(ToDense(matrix), EigenArrayEq<double>({{0, 0},  //
                                                     {0, 0}}));
}

TEST(SetEigenMatrixFromTriplets, CorrectForTinyMatrix) {
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {0, 0, 1.0}, {1, 0, -1.0}, {0, 0, 0.0}, {1, 1, 1.0}, {0, 0, 1.0}};
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> matrix(2, 2);
  SetEigenMatrixFromTriplets(std::move(triplets), matrix);
  EXPECT_THAT(ToDense(matrix), EigenArrayEq<double>({{2, 0},  //
                                                     {-1, 1}}));
}

}  // namespace
}  // namespace operations_research::pdlp
