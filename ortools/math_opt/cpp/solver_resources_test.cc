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

#include "ortools/math_opt/cpp/solver_resources.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::Optional;

TEST(SolverResourcesTest, Proto) {
  {
    const SolverResources empty;
    EXPECT_THAT(empty.Proto(), EqualsProto(SolverResourcesProto{}));
  }
  {
    const SolverResources resources = {.cpu = 3.5};
    SolverResourcesProto expected;
    expected.set_cpu(3.5);
    EXPECT_THAT(resources.Proto(), EqualsProto(expected));
  }
  {
    const SolverResources resources = {.ram = 70.0 * 1024.0 * 1024.0};
    SolverResourcesProto expected;
    expected.set_ram(70.0 * 1024.0 * 1024.0);
    EXPECT_THAT(resources.Proto(), EqualsProto(expected));
  }
}

TEST(SolverResourcesTest, FromProto) {
  {
    ASSERT_OK_AND_ASSIGN(const SolverResources resources,
                         SolverResources::FromProto({}));
    EXPECT_EQ(resources.cpu, std::nullopt);
  }
  {
    SolverResourcesProto resources_proto;
    resources_proto.set_cpu(3.5);
    ASSERT_OK_AND_ASSIGN(const SolverResources resources,
                         SolverResources::FromProto(resources_proto));
    EXPECT_THAT(resources.cpu, Optional(3.5));
  }
  {
    SolverResourcesProto resources_proto;
    resources_proto.set_ram(70.0 * 1024.0 * 1024.0);
    ASSERT_OK_AND_ASSIGN(const SolverResources resources,
                         SolverResources::FromProto(resources_proto));
    EXPECT_THAT(resources.ram, Optional(70.0 * 1024.0 * 1024.0));
  }
}

TEST(SolverResourcesTest, AbslParseFlag) {
  // Empty value.
  {
    SolverResources resources;
    std::string error;
    EXPECT_TRUE(AbslParseFlag("", &resources, &error));
    EXPECT_EQ(resources.cpu, std::nullopt);
    EXPECT_EQ(error, "");
  }
  // Test valid SolverResources::cpu.
  {
    SolverResources resources;
    std::string error;
    EXPECT_TRUE(AbslParseFlag("cpu:3.5", &resources, &error));
    EXPECT_THAT(resources.cpu, Optional(3.5));
    EXPECT_EQ(error, "");
  }
  // Test invalid SolverResources::cpu.
  {
    SolverResources resources;
    std::string error;
    EXPECT_FALSE(AbslParseFlag("cpu:a", &resources, &error));
    EXPECT_EQ(resources.cpu, std::nullopt);
    EXPECT_THAT(error, HasSubstr("double"));
  }
}

TEST(SolverResourcesTest, AbslUnparseFlag) {
  EXPECT_EQ(AbslUnparseFlag({}), "");
  EXPECT_EQ(AbslUnparseFlag({.cpu = 3.5}), "cpu: 3.5");
}

}  // namespace
}  // namespace operations_research::math_opt
