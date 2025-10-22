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

#include "ortools/math_opt/core/solver_interface_testing.h"

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/solver_interface.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

absl::StatusOr<std::unique_ptr<SolverInterface>> DefaultGlopFactory(
    const ModelProto& model, const SolverInterface::InitArgs& init_args) {
  return absl::UnimplementedError("glop");
}

absl::StatusOr<std::unique_ptr<SolverInterface>> DefaultCpSatFactory(
    const ModelProto& model, const SolverInterface::InitArgs& init_args) {
  return absl::UnimplementedError("cp-sat");
}

absl::StatusOr<std::unique_ptr<SolverInterface>> OverriddenFactory(
    const ModelProto& model, const SolverInterface::InitArgs& init_args) {
  return absl::UnimplementedError("overridden");
}

// Called from main() to initialize the default configuration:
// * SOLVER_TYPE_GLOP: DefaultGlopFactory
// * SOLVER_TYPE_CP_SAT: DefaultCpSatFactory
void RegisterDefaultFactories() {
  AllSolversRegistry::Instance()->Register(SOLVER_TYPE_GLOP,
                                           DefaultGlopFactory);
  AllSolversRegistry::Instance()->Register(SOLVER_TYPE_CP_SAT,
                                           DefaultCpSatFactory);
}

// Test the initial state of the original instance; i.e. that main() has
// called RegisterDefaultFactories();
//
// This will be an assumption made in all other tests.
TEST(WithAlternateAllSolversRegistryTest, ExpectedConfiguration) {
  ASSERT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
  EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                  SOLVER_TYPE_GLOP, /*model=*/{}, /*init_args=*/{}),
              StatusIs(absl::StatusCode::kUnimplemented, "glop"));
  EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                  SOLVER_TYPE_CP_SAT, /*model=*/{}, /*init_args=*/{}),
              StatusIs(absl::StatusCode::kUnimplemented, "cp-sat"));
}

TEST(WithAlternateAllSolversRegistryTest, Default) {
  AllSolversRegistry* absl_nonnull const original_registry =
      AllSolversRegistry::Instance();
  {
    const WithAlternateAllSolversRegistry alternate_registry({});
    EXPECT_NE(AllSolversRegistry::Instance(), original_registry);
    EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(), IsEmpty());
  }
  EXPECT_EQ(AllSolversRegistry::Instance(), original_registry);
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
}

TEST(WithAlternateAllSolversRegistryDeathTest, TwoInstances) {
  const WithAlternateAllSolversRegistry alternate_registry({});
  EXPECT_DEATH(WithAlternateAllSolversRegistry({}), "temporary_test_instance");
}

TEST(WithAlternateAllSolversRegistryTest, KeepRegisteredSolver) {
  AllSolversRegistry* absl_nonnull const original_registry =
      AllSolversRegistry::Instance();
  {
    const WithAlternateAllSolversRegistry alternate_registry({
        .kept = {SOLVER_TYPE_GLOP},
    });
    EXPECT_NE(AllSolversRegistry::Instance(), original_registry);
    ASSERT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
                UnorderedElementsAre(SOLVER_TYPE_GLOP));
    EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                    SOLVER_TYPE_GLOP, /*model=*/{}, /*init_args=*/{}),
                StatusIs(absl::StatusCode::kUnimplemented, "glop"));
  }
  EXPECT_EQ(AllSolversRegistry::Instance(), original_registry);
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
}

TEST(WithAlternateAllSolversRegistryDeathTest, KeepUnregisteredSolver) {
  EXPECT_DEATH(WithAlternateAllSolversRegistry({
                   .kept = {SOLVER_TYPE_GUROBI},
               }),
               "SOLVER_TYPE_GUROBI was not registered");
}

TEST(WithAlternateAllSolversRegistryDeathTest, KeepAndOverrideSolver) {
  EXPECT_DEATH(WithAlternateAllSolversRegistry({
                   .kept = {SOLVER_TYPE_CP_SAT},
                   .overridden = {{SOLVER_TYPE_CP_SAT, OverriddenFactory}},
               }),
               "SOLVER_TYPE_CP_SAT already registered");
}

TEST(WithAlternateAllSolversRegistryTest, KeepAndOverrideRegisteredSolvers) {
  AllSolversRegistry* absl_nonnull const original_registry =
      AllSolversRegistry::Instance();
  {
    const WithAlternateAllSolversRegistry alternate_registry({
        .kept = {SOLVER_TYPE_GLOP},
        .overridden = {{SOLVER_TYPE_CP_SAT, OverriddenFactory}},
    });
    EXPECT_NE(AllSolversRegistry::Instance(), original_registry);
    ASSERT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
                UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
    EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                    SOLVER_TYPE_GLOP, /*model=*/{}, /*init_args=*/{}),
                StatusIs(absl::StatusCode::kUnimplemented, "glop"));
    EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                    SOLVER_TYPE_CP_SAT, /*model=*/{}, /*init_args=*/{}),
                StatusIs(absl::StatusCode::kUnimplemented, "overridden"));
  }
  EXPECT_EQ(AllSolversRegistry::Instance(), original_registry);
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
  EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                  SOLVER_TYPE_GLOP, /*model=*/{}, /*init_args=*/{}),
              StatusIs(absl::StatusCode::kUnimplemented, "glop"));
  EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                  SOLVER_TYPE_CP_SAT, /*model=*/{}, /*init_args=*/{}),
              StatusIs(absl::StatusCode::kUnimplemented, "cp-sat"));
}

TEST(WithAlternateAllSolversRegistryTest, OverrideUnregisteredSolvers) {
  AllSolversRegistry* absl_nonnull const original_registry =
      AllSolversRegistry::Instance();
  {
    const WithAlternateAllSolversRegistry alternate_registry({
        .overridden = {{SOLVER_TYPE_GUROBI, OverriddenFactory}},
    });
    EXPECT_NE(AllSolversRegistry::Instance(), original_registry);
    ASSERT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
                UnorderedElementsAre(SOLVER_TYPE_GUROBI));
    EXPECT_THAT(AllSolversRegistry::Instance()->Create(
                    SOLVER_TYPE_GUROBI, /*model=*/{}, /*init_args=*/{}),
                StatusIs(absl::StatusCode::kUnimplemented, "overridden"));
  }
  EXPECT_EQ(AllSolversRegistry::Instance(), original_registry);
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              UnorderedElementsAre(SOLVER_TYPE_GLOP, SOLVER_TYPE_CP_SAT));
}

}  // namespace
}  // namespace operations_research::math_opt

// Custom main() that register two solvers in the default registry.
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  operations_research::math_opt::RegisterDefaultFactories();
  return RUN_ALL_TESTS();
}
