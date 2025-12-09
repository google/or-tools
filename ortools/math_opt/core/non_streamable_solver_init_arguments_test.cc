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

#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Property;

TEST(NonStreamableSolverInitArgumentsValueTest, DefaultConstructor) {
  const NonStreamableSolverInitArgumentsValue value;
  EXPECT_EQ(value.get(), nullptr);
}

class FakeNonStreamableInitArguments : public NonStreamableSolverInitArguments {
 public:
  explicit FakeNonStreamableInitArguments(const SolverTypeProto solver_type)
      : solver_type_(solver_type) {}

  SolverTypeProto solver_type() const override { return solver_type_; }

  std::unique_ptr<const NonStreamableSolverInitArguments> Clone()
      const override {
    ++num_clones_;
    return std::make_unique<FakeNonStreamableInitArguments>(*this);
  }

  // Incremented each time Clone() is called.
  //
  // This member is thread local to not prevent executing tests in parallel.
  ABSL_CONST_INIT static thread_local int num_clones_;

 private:
  SolverTypeProto solver_type_;
};

thread_local int FakeNonStreamableInitArguments::num_clones_ = 0;

TEST(NonStreamableSolverInitArgumentsValueTest, NonStreamableConstructor) {
  const FakeNonStreamableInitArguments fake(SOLVER_TYPE_GSCIP);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  const NonStreamableSolverInitArgumentsValue value = fake;

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 1);
  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));
}

TEST(NonStreamableSolverInitArgumentsValueTest, CopyConstructor) {
  const NonStreamableSolverInitArgumentsValue original_value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GSCIP);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const NonStreamableSolverInitArgumentsValue value(original_value);

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 1);
  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));
}

TEST(NonStreamableSolverInitArgumentsValueTest,
     CopyConstructorNullNonStreamable) {
  const NonStreamableSolverInitArgumentsValue original_value;
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const NonStreamableSolverInitArgumentsValue value(original_value);
  EXPECT_EQ(value.get(), nullptr);
}

TEST(NonStreamableSolverInitArgumentsValueTest, Assignment) {
  const NonStreamableSolverInitArgumentsValue original_value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GSCIP);

  NonStreamableSolverInitArgumentsValue value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GUROBI);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  value = original_value;

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 1);
  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));
}

TEST(NonStreamableSolverInitArgumentsValueTest, AssignmentNullNonStreamable) {
  const NonStreamableSolverInitArgumentsValue original_value;

  NonStreamableSolverInitArgumentsValue value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GUROBI);

  value = original_value;

  EXPECT_EQ(value.get(), nullptr);
}

TEST(NonStreamableSolverInitArgumentsValueTest, SelfAssignment) {
  NonStreamableSolverInitArgumentsValue value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GSCIP);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  value = value;

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 0);
  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));
}

TEST(NonStreamableSolverInitArgumentsValueTest, MoveConstructor) {
  NonStreamableSolverInitArgumentsValue original_value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GSCIP);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  const NonStreamableSolverInitArgumentsValue value(std::move(original_value));

  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 0);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(original_value.get(), nullptr);
}

TEST(NonStreamableSolverInitArgumentsValueTest, MoveAssignment) {
  NonStreamableSolverInitArgumentsValue original_value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GSCIP);

  NonStreamableSolverInitArgumentsValue value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GUROBI);

  FakeNonStreamableInitArguments::num_clones_ = 0;

  value = std::move(original_value);

  ASSERT_THAT(value.get(),
              Pointee(Property(&NonStreamableSolverInitArguments::solver_type,
                               SOLVER_TYPE_GSCIP)));

  EXPECT_EQ(FakeNonStreamableInitArguments::num_clones_, 0);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(original_value.get(), nullptr);
}

TEST(NonStreamableSolverInitArgumentsValueTest,
     MoveAssignmentNullNonStreamable) {
  NonStreamableSolverInitArgumentsValue original_value;

  NonStreamableSolverInitArgumentsValue value =
      FakeNonStreamableInitArguments(SOLVER_TYPE_GUROBI);

  value = std::move(original_value);

  EXPECT_EQ(value.get(), nullptr);
}

}  // namespace
}  // namespace operations_research::math_opt
