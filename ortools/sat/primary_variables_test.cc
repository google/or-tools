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

#include "ortools/sat/primary_variables.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::Pair;

TEST(PrimaryVariablesTest, BasicExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -100, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 4 ]
        coeffs: [ 1, 1, 1, 2 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      lin_max {
        target {
          vars: [ 5 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 2 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 5, 6 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const VariableRelationships relationships =
      ComputeVariableRelationships(model);
  EXPECT_GT(relationships.secondary_variables.size(), 1);
  EXPECT_THAT(relationships.variable_dependencies, Contains(Pair(6, 5)));
  const std::vector<int64_t> solution = {-1, 1, 0, 4, 0, 4, -4};

  std::vector<int64_t> all_variables = solution;
  for (const int var : relationships.secondary_variables) {
    all_variables[var] = -999999;
  }
  EXPECT_TRUE(ComputeAllVariablesFromPrimaryVariables(model, relationships,
                                                      &all_variables));
  EXPECT_EQ(all_variables, solution);
}

TEST(PrimaryVariablesTest, WithIntProd) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 offset: -6 }
        exprs { vars: 1 coeffs: 3 offset: 1 }
      }
    }
  )pb");
  const VariableRelationships relationships =
      ComputeVariableRelationships(model);
  const std::vector<int64_t> solution = {10, 1};

  std::vector<int64_t> all_variables = solution;
  for (const int var : relationships.secondary_variables) {
    all_variables[var] = -999999;
  }
  EXPECT_TRUE(ComputeAllVariablesFromPrimaryVariables(model, relationships,
                                                      &all_variables));
  EXPECT_EQ(all_variables, solution);
}

TEST(PrimaryVariablesTest, WithExactlyOne) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  const VariableRelationships relationships =
      ComputeVariableRelationships(model);
  EXPECT_EQ(relationships.secondary_variables.size(), 1);
  const ConstraintProto expected = ParseTestProto(R"pb(
      exactly_one { literals: [ 0, 1, 2, 3 ] }
  )pb");
  EXPECT_THAT(relationships.dependency_resolution_constraint,
              ElementsAre(EqualsProto(expected)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
