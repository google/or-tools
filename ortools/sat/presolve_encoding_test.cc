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

#include "ortools/sat/presolve_encoding.h"

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(CreateVariableEncodingLocalModelsTest, TrivialTest) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 0, 1 ]
      }
    }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  const std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);
  ASSERT_EQ(local_models.size(), 1);
  ASSERT_EQ(local_models[0].var, 1);
  EXPECT_THAT(local_models[0].linear1_constraints, ElementsAre(0));
}

TEST(CreateVariableEncodingLocalModelsTest, BasicTest) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints { bool_or { literals: [ 0, 1 ] } }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  const std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);
  ASSERT_EQ(local_models.size(), 1);
  ASSERT_EQ(local_models[0].var, 2);
  EXPECT_THAT(local_models[0].linear1_constraints, ElementsAre(0, 1));
  EXPECT_THAT(local_models[0].constraints_linking_two_encoding_booleans,
              ElementsAre(2));
  EXPECT_THAT(local_models[0].bools_only_used_inside_the_local_model,
              UnorderedElementsAre(0, 1));
}

TEST(CreateVariableEncodingLocalModelsTest, OneBooleanUsedElsewhere) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    objective {
      vars: [ 1 ]
      coeffs: [ 2 ]
    }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  const std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);
  ASSERT_EQ(local_models.size(), 1);
  ASSERT_EQ(local_models[0].var, 3);
  EXPECT_THAT(local_models[0].linear1_constraints, ElementsAre(0, 1, 2));
  EXPECT_THAT(local_models[0].constraints_linking_two_encoding_booleans,
              ElementsAre(3, 4));
  EXPECT_THAT(local_models[0].bools_only_used_inside_the_local_model,
              UnorderedElementsAre(0));
}

TEST(CreateVariableEncodingLocalModelsTest, TwoVars) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 4 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 4 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints { bool_or { literals: [ 0, 1, 5 ] } }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    objective {
      vars: [ 3 ]
      coeffs: [ 2 ]
    }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);
  ASSERT_EQ(local_models.size(), 2);
  ASSERT_EQ(local_models[0].var, 3);
  ASSERT_EQ(local_models[1].var, 4);
  EXPECT_THAT(local_models[0].linear1_constraints, ElementsAre(0, 1, 2));
  EXPECT_THAT(local_models[1].linear1_constraints, ElementsAre(3, 4));
  EXPECT_THAT(local_models[0].constraints_linking_two_encoding_booleans,
              ElementsAre(5, 6));
  EXPECT_THAT(local_models[1].constraints_linking_two_encoding_booleans,
              ElementsAre(6));
  EXPECT_THAT(local_models[0].bools_only_used_inside_the_local_model,
              UnorderedElementsAre(0));
  EXPECT_THAT(local_models[1].bools_only_used_inside_the_local_model,
              UnorderedElementsAre());
  EXPECT_EQ(local_models[0].variable_coeff_in_objective, 2);
  EXPECT_EQ(local_models[1].variable_coeff_in_objective, 0);

  absl::flat_hash_map<int, Domain> fully_encoded_domains;
  bool changed = false;
  CHECK(BasicPresolveAndGetFullyEncodedDomains(
      &context, local_models[0], &fully_encoded_domains, &changed));
  EXPECT_THAT(
      fully_encoded_domains,
      UnorderedElementsAre(Pair(0, Domain(0, 0)), Pair(-1, Domain(1, 1))));
}

TEST(BasicPresolveAndGetFullyEncodedDomainsTest, EncodingWithBoolOr) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 4 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    objective {
      vars: [ 3 ]
      coeffs: [ 2 ]
    }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);

  absl::flat_hash_map<int, Domain> fully_encoded_domains;
  bool changed = false;
  CHECK(BasicPresolveAndGetFullyEncodedDomains(
      &context, local_models[0], &fully_encoded_domains, &changed));
  EXPECT_THAT(fully_encoded_domains,
              UnorderedElementsAre(Pair(0, Domain(0)), Pair(1, Domain(1)),
                                   Pair(2, Domain(2)),
                                   Pair(-1, Domain::FromValues({1, 2})),
                                   Pair(-2, Domain::FromValues({0, 2})),
                                   Pair(-3, Domain::FromValues({0, 1}))));
  ConstraintProto expected_exactly_one = ParseTestProto(R"pb(
    exactly_one { literals: [ 0, 1, 2 ] }
  )pb");
  EXPECT_THAT(context.working_model->constraints(),
              testing::Contains(testing::EqualsProto(expected_exactly_one)));
}

TEST(DetectAllEncodedComplexDomainTest, BasicTest) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 1 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 5 ]
      }
    }
    # Note that the var=3 is missing from both this encoding and its negation.
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 1, 2 ]
      }
    }
    constraints {
      enforcement_literal: -2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 0, 4, 5 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    objective {
      vars: [ 3 ]
      coeffs: [ 2 ]
    }
  )pb");
  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &model_proto, &mapping_model);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(&context);
  ASSERT_TRUE(DetectAllEncodedComplexDomain(&context, local_models[0]));
  context.WriteVariableDomainsToProto();
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    # var=1 is forbidden by the at_most_one
    variables { domain: [ 0, 0, 2, 2, 4, 5 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {}
    constraints {}
    constraints {}
    constraints {}
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints { at_most_one { literals: [ 2, 5 ] } }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 0, 2 ]
      }
    }
    constraints {
      enforcement_literal: -6
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 4, 5 ]
      }
    }
    objective {
      vars: [ 3 ]
      coeffs: [ 2 ]
    }
  )pb");
  EXPECT_THAT(context.working_model, testing::EqualsProto(expected_model));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
