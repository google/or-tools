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

#include "ortools/sat/opb_reader.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

TEST(LoadAndValidateBooleanProblemTest, Ok) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "1 x1 1 x2 >= 1 ;\n"
      "1 x1 1 x2 >= 1 ;\n";
  CpModelProto problem;
  OpbReader reader;
  const std::string filename = file::JoinPath(::testing::TempDir(), "file.opb");
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  CHECK(reader.LoadAndValidate(filename, &problem));
  EXPECT_TRUE(ValidateCpModel(problem).empty());
  EXPECT_EQ(problem.variables_size(), 2);
  EXPECT_EQ(reader.num_variables(), 2);
}

TEST(LoadAndValidateBooleanProblemTest, NonLinear) {
  std::string file =
      "min: 1 x1 1 x2 ~x3;\n"
      "1 x1 1 x2 >= 1 ;\n"
      "1 x1 x2 x3 >= 1 ;\n";
  CpModelProto problem;
  OpbReader reader;
  const std::string filename = file::JoinPath(::testing::TempDir(), "file.opb");
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  CHECK(reader.LoadAndValidate(filename, &problem));
  EXPECT_TRUE(ValidateCpModel(problem).empty());
  EXPECT_EQ(problem.variables_size(), 5);
  EXPECT_EQ(reader.num_variables(), 3);
}

TEST(LoadAndValidateBooleanProblemTest, Weighted) {
  std::string file =
      "1 x1 1 x2 >= 1 ;\n"
      "[32] 2 x1 = 2 ;\n";
  CpModelProto problem;
  OpbReader reader;
  const std::string filename = file::JoinPath(::testing::TempDir(), "file.opb");
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  CHECK(reader.LoadAndValidate(filename, &problem));
  EXPECT_TRUE(ValidateCpModel(problem).empty());
  EXPECT_EQ(problem.variables_size(), 3);
  EXPECT_EQ(reader.num_variables(), 2);
  ASSERT_TRUE(problem.has_objective());
  EXPECT_EQ(problem.objective().vars_size(), 1);
  EXPECT_EQ(problem.objective().vars(0), 2);
  EXPECT_EQ(problem.objective().coeffs(0), 32);
}

TEST(LoadAndValidateBooleanProblemTest, ZeroCoefficients) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "1 x1 0 x2 >= 1 ;\n"
      "1 x1 1 x2 >= 1 ;\n";
  CpModelProto problem;
  OpbReader reader;
  const std::string filename =
      file::JoinPath(::testing::TempDir(), "file2.opb");
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  EXPECT_FALSE(reader.LoadAndValidate(filename, &problem));
}

TEST(LoadAndValidateBooleanProblemTest, IntegerOverflow) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "1 x1 2 x2 >= 1 ;\n"
      "1 x1 123456789123456789123456789 x2 >= 1 ;\n";
  CpModelProto problem;
  OpbReader reader;
  const std::string filename =
      file::JoinPath(::testing::TempDir(), "file2.opb");
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  EXPECT_TRUE(reader.LoadAndValidate(filename, &problem));
  EXPECT_FALSE(reader.model_is_supported());
}

void FindSymmetries(
    absl::string_view file,
    std::vector<std::unique_ptr<SparsePermutation>>* generators) {
  CpModelProto problem;
  OpbReader reader;
  static int counter = 4;
  ++counter;
  const std::string filename = file::JoinPath(
      ::testing::TempDir(), absl::StrCat("file", counter, ".opb"));
  CHECK_OK(file::SetContents(filename, file, file::Defaults()));
  CHECK(reader.LoadAndValidate(filename, &problem));
  SatParameters params;
  params.set_symmetry_level(2);
  params.set_symmetry_detection_deterministic_time_limit(1000);
  SolverLogger logger;
  logger.EnableLogging(true);
  TimeLimit time_limit;
  FindCpModelSymmetries(params, problem, generators, &logger, &time_limit);
}

TEST(FindLinearBooleanProblemSymmetriesTest, ProblemWithSymmetry1) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "1 x1 1 x2 >= 1 ;\n"
      "1 x1 1 x2 >= 1 ;\n";
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);

  EXPECT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1)");
}

TEST(DISABLED_FindLinearBooleanProblemSymmetriesTest, ProblemWithSymmetry2) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "-3 x1 -2 x2 >= -1 ;\n";  // This is simplified to both x1 and x2 false.
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);
  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2) (1 3)");
}

TEST(FindLinearBooleanProblemSymmetriesTest, ProblemWithSymmetry3) {
  std::string file =
      "min: 1 x1 1 x2 1 x3;\n"
      " 1 x1 2 x2 3 x3 >= 2 ;\n"
      " 1 x2 2 x3 3 x1 >= 2 ;\n"
      " 1 x3 2 x1 3 x2 >= 2 ;\n";
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);
  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2 1)");
}

TEST(DISABLED_FindLinearBooleanProblemSymmetriesTest, ProblemWithSymmetry4) {
  std::string file =
      "min: 1 x1;\n"
      " 1 x1 2 x2 >= 2 ;\n"
      " 1 x1 -2 x3 >= 0 ;\n";
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);
  ASSERT_EQ(generators.size(), 1);

  // x2 and not(x3) are equivalent.
  EXPECT_EQ(generators[0]->DebugString(), "(2 5) (3 4)");
}

TEST(FindLinearBooleanProblemSymmetriesTest, ProblemWithoutSymmetry1) {
  std::string file =
      "min: 1 x1 2 x2 ;\n"
      "1 x1 1 x2 >= 1 ;\n";
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);
  EXPECT_EQ(generators.size(), 0);
}

TEST(FindLinearBooleanProblemSymmetriesTest, ProblemWithoutSymmetry2) {
  std::string file =
      "min: 1 x1 1 x2 ;\n"
      "1 x1 2 x2 >= 2 ;\n";
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);
  EXPECT_EQ(generators.size(), 0);
}

TEST(FindLinearBooleanProblemSymmetriesTest, PigeonHole) {
  // This is the problem of putting 3 pigeons into 2 holes (UNSAT).
  // x1: pigeon 1 is in hole 1
  // x2: pigeon 1 is in hole 2
  // x3: pigeon 2 is in hole 1
  // ...
  std::string file =
      "min: ;\n"
      "1 x1 1 x2 >= 1 ;\n"            // pigeon 1 should go into one hole
      "1 x3 1 x4 >= 1 ;\n"            // pigeon 2 should go into one hole
      "1 x5 1 x6 >= 1 ;\n"            // pigeon 3 should go into one hole
      "-1 x1 -1 x3 -1 x5 >= -1 ;\n"   // At most 1 pigeon in hole 1
      "-1 x2 -1 x4 -1 x6 >= -1 ;\n";  // At most 1 pigeon in hole 2
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  FindSymmetries(file, &generators);

  // The minimal support size is obtained with 3 generators:
  // - The two holes are symmetric (x1, x2) (x3, x4) (x5, x6).
  // - 2 generators for all the permutations of the 3 pigeons.
  //
  // But as of 2014-05, the symmetry finder isn't great at reducing the support
  // size, but rather performs well at finding few generators, so it finds a
  // solution with 2 generators.
  EXPECT_EQ(generators.size(), 2);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
