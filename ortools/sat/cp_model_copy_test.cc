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

#include "ortools/sat/cp_model_copy.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;
using ::testing::IsEmpty;

TEST(ModelCopyTest, IntervalsAddLinearConstraints) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      enforcement_literal: 0
      interval {
        start: { vars: 1 coeffs: 1 }
        size: { vars: 2 coeffs: 1 }
        end: { vars: 3 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  CpModelProto new_cp_model;
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      enforcement_literal: 0
      interval {
        start: { vars: 1 coeffs: 1 }
        size: { vars: 2 coeffs: 1 }
        end: { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1, 2, 3 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }

    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, IntervalsWithFixedStartAndEnd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    constraints {
      interval {
        start: { vars: 0 coeffs: 1 }
        size: { vars: 1 coeffs: 1 }
        end: { vars: 2 coeffs: 2 }
      }
    }
  )pb");

  Model model;
  CpModelProto new_cp_model;
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    constraints {
      interval {
        start { offset: 10 }
        end { offset: 20 }
        size { offset: 10 }
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, ClearInternalFields) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 2, 2 ] }
    objective {
      vars: [ 0 ]
      coeffs: [ 3 ]
      integer_before_offset: 10
      integer_after_offset: 20
      integer_scaling_factor: 5
    }
    symmetry {}
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 2, 2 ] }
    objective { offset: 6 integer_before_offset: 6 }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  EXPECT_TRUE(CopyModel(initial_model, /*clear_internal_fields=*/true,
                        &new_cp_model, &model));
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemoveDuplicateFromClauses) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3, 2 ]
      bool_or { literals: [ -4, 9, 8 ] }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -2, -3, -4, 9, 8 ] } }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemoveDuplicateFromEnforcementLiterals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3, 2 ]  # duplicate
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 100 ]
      }
    }
    constraints {
      enforcement_literal: [ 2, -3, 2 ]  # can never be enforced.
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 2, 1, 3 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3 ]
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapLiteralsInBoolOr) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    constraints {
      enforcement_literal: [ 0, -2, 2, 3 ]
      bool_or { literals: [ 4, -6, 6 ] }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {
      kNoVariableMapping, 1, -3, -3, 1, 0, kNoVariableMapping};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 1, 2, -1 ] } }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapLiteralsInBoolAnd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      bool_and { literals: [ 2, 3 ] }
    }
    constraints {
      enforcement_literal: [ 0, 1 ]
      bool_and { literals: [ 4, 5 ] }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {
      kNoVariableMapping, 0, 1, -2, 0, -2};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -1 ] } }
    constraints {
      enforcement_literal: [ 0 ]
      bool_and { literals: [ -2 ] }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapLiteralsInBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2, 3, 4, 5 ] } }
    constraints { bool_xor { literals: [ 0, 2, 3, 4, 5 ] } }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {
      kNoVariableMapping, kNoVariableMapping, 0, 1, 2, 3};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 3 ] } }
    constraints { bool_xor { literals: [ 0, 1, 3, -3 ] } }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInLinear) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 11 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 5, -6, 7, -8, 9 ]
        domain: [ 0, 50 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {kNoVariableMapping, -1, 1, 2, 2};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ -1 ]
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 7, 1 ]
        domain: [ 1, 51 ]
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInLinear_CanonicalizeSingleBoolInDomain) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 5, 6 ]
        domain: [ 6, 7 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {0, -1};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  // 5x + 6y \in [6, 7] is remapped to 5x + 6.not(x) = 6 - x \in [6, 7], whose
  // unique solution is x = 0.
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInLegacyInverse) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_direct: 0
        f_direct: 1
        f_direct: 2
        f_inverse: 3
        f_inverse: 4
        f_inverse: 5
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {kNoVariableMapping, 0, 1, 1,
                                             kNoVariableMapping, 0};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_expr_direct: { offset: 2 }
        f_expr_direct: { vars: 0 coeffs: 1 }
        f_expr_direct: { vars: 1 coeffs: 1 }
        f_expr_inverse: { vars: 1 coeffs: 1 }
        f_expr_inverse: { offset: 2 }
        f_expr_inverse: { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInInverse) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_expr_direct: { vars: 0 coeffs: -1 offset: 3 }
        f_expr_direct: { vars: 1 coeffs: 1 offset: -1 }
        f_expr_direct: { vars: 2 coeffs: 3 offset: -2 }
        f_expr_inverse: { vars: 0 coeffs: 1 offset: 0 }
        f_expr_inverse: { vars: 1 coeffs: 1 offset: 1 }
        f_expr_inverse: { vars: 2 coeffs: -2 offset: 2 }
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {kNoVariableMapping, 0, 1};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_expr_direct: { offset: 2 }
        f_expr_direct: { vars: 0 coeffs: 1 offset: -1 }
        f_expr_direct: { vars: 1 coeffs: 3 offset: -2 }
        f_expr_inverse: { offset: 1 }
        f_expr_inverse: { vars: 0 coeffs: 1 offset: 1 }
        f_expr_inverse: { vars: 1 coeffs: -2 offset: 2 }
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, ExpandNonAffineExpressionsInInverse) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_expr_direct: { vars: 0 coeffs: -1 offset: 3 }
        f_expr_direct: { vars: 1 coeffs: 1 offset: -1 }
        f_expr_direct: { vars: 2 coeffs: 3 offset: -2 }
        f_expr_inverse: { vars: 0 coeffs: 1 offset: 0 }
        f_expr_inverse: { vars: 1 coeffs: 1 offset: 1 }
        f_expr_inverse: {
          vars: [ 1, 2 ]
          coeffs: [ -1, -1 ]
          offset: 2
        }
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  ModelCopy model_copy(&new_cp_model, &model);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ -4, 0 ] }
    constraints {
      inverse {
        f_expr_direct: { vars: 0 coeffs: -1 offset: 3 }
        f_expr_direct: { vars: 1 coeffs: 1 offset: -1 }
        f_expr_direct: { vars: 2 coeffs: 3 offset: -2 }
        f_expr_inverse: { vars: 0 coeffs: 1 offset: 0 }
        f_expr_inverse: { vars: 1 coeffs: 1 offset: 1 }
        f_expr_inverse: { vars: 3 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      linear {
        vars: [ 3, 1, 2 ]
        coeffs: [ -1, -1, -1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInObjective) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 5, 6 ]
      domain: [ 0, 50 ]
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {0, -1};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.ImportEverythingExceptVariablesConstraintsAndHint(
      initial_model, /*clear_internal_fields=*/false));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  // 5x + 6y \in [0, 50] is remapped to 5x + 6.not(x) = 6 - x \in [0, 50].
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0 ]
      coeffs: [ -1 ]
      offset: 6
      domain: [ -1, 0 ]
      integer_before_offset: 6
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInFloatingPointObjective) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    floating_point_objective {
      vars: [ 0, 1 ]
      coeffs: [ 5.5, 6.25 ]
      offset: 2.75
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  const std::vector<int> variable_mapping = {0, -1};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  EXPECT_TRUE(model_copy.ImportEverythingExceptVariablesConstraintsAndHint(
      initial_model, /*clear_internal_fields=*/false));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  // 5.5x + 6.25y + 2.75 is remapped to 5.5x + 6.25 * (1 - x) + 2.75.
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    floating_point_objective {
      vars: [ 0 ]
      coeffs: [ -0.75 ]
      offset: 9.0
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemapVariablesInSearchStrategyAssumptionsAndHint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    search_strategy { variables: [ 1, 0 ] }
    assumptions: [ 0, -2 ]
    solution_hint {
      vars: [ 1 ]
      values: [ 1 ]
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  // x0, x1 mapped to x0, not(x0).
  const std::vector<int> variable_mapping = {0, -1};
  ModelCopy model_copy(&new_cp_model, &model, variable_mapping);

  EXPECT_TRUE(model_copy.ImportVariables(initial_model));
  EXPECT_TRUE(model_copy.ImportAndSimplifyConstraints(initial_model,
                                                      /*first_copy=*/true));
  model_copy.ImportSolutionHint(initial_model);
  EXPECT_TRUE(model_copy.ImportEverythingExceptVariablesConstraintsAndHint(
      initial_model, /*clear_internal_fields=*/false));
  EXPECT_TRUE(model_copy.FinishCopy(initial_model));

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    search_strategy {
      exprs { vars: 0 coeffs: -1 offset: 1 }
      exprs { vars: 0 coeffs: 1 }
    }
    assumptions: [ 0, 0 ]
    solution_hint {
      vars: [ 0 ]
      values: [ 0 ]
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, ChangeEnforcedAtMostOrExactlyOneToLinear) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      at_most_one { literals: [ 2, -4 ] }
    }
    constraints {
      enforcement_literal: [ 0, 1 ]
      exactly_one { literals: [ 2, 3 ] }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ -1, 0 ]
      }
    }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, LegacyElementConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 1 ]
      element {
        index: 0
        target: 1
        vars: [ 2, 3 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, ElementConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 1 ]
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, ExpandedNonAffineExpressionsShareVariableWhenPossible) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      all_diff {
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, 2 ]
          offset: 1
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 2, 4 ]
          offset: 3
        }
        exprs {
          vars: [ 1, 0 ]
          coeffs: [ -2, -1 ]
          offset: 6
        }
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 30 ] }
    constraints {
      all_diff {
        exprs { vars: 2 coeffs: 1 offset: 1 }
        exprs { vars: 2 coeffs: 2 offset: 3 }
        exprs { vars: 2 coeffs: -1 offset: 6 }
      }
    }
    constraints {
      linear {
        vars: [ 2, 0, 1 ]
        coeffs: [ -1, 1, 2 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CopyModel(initial_model, /*clear_internal_fields=*/false, &new_cp_model,
            &model);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, CanonicalizeLinMaxOnCopy) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 4, -4, 4 ]
      lin_max {
        target {
          vars: [ 2, 1, 2, 4 ]
          coeffs: [ 1, 2, 1, 5 ]
        }
        exprs {
          vars: [ 1, 0, 3 ]
          coeffs: [ 2, 1, 7 ]
          offset: 3
        }
        exprs {
          vars: [ 0, 1, 4 ]
          coeffs: [ 1, -1, 4 ]
          offset: 1
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 1
        }
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ -4, 4 ]
      lin_max {
        target {
          vars: [ 1, 2 ]
          coeffs: [ 2, 2 ]
          offset: 5
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 1
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 5
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, 2 ]
          offset: 3
        }
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  EXPECT_TRUE(CopyModel(initial_model, /*clear_internal_fields=*/false,
                        &new_cp_model, &model));
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, CopyModelAdvancesDeterministicTime) {
  CpModelProto proto;
  for (int i = 0; i < 30; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(10);
  }
  auto* ct = proto.add_constraints()->mutable_linear();
  for (int i = 0; i < 30; ++i) {
    ct->add_vars(i);
    ct->add_coeffs(i + 1);
  }
  ct->add_domain(0);
  ct->add_domain(100);

  CpModelProto copied_proto;
  Model model;
  ASSERT_TRUE(
      CopyModel(proto, /*clear_internal_fields=*/true, &copied_proto, &model));
  EXPECT_DOUBLE_EQ(
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime(),
      EstimateCopyDeterministicTime(ComputeModelStats(copied_proto)));

  // Even if initial copy proves the model infeasible, deterministic time for
  // the scanned prefix is still recorded.
  proto.add_constraints()->mutable_bool_or();  // Empty clause -> UNSAT.
  Model unsat_model;
  EXPECT_FALSE(CopyModel(proto, /*clear_internal_fields=*/true, &copied_proto,
                         &unsat_model));
  EXPECT_GT(unsat_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime(),
            1e-6);
}

TEST(DenseModelCopyTest, IncrementalBoundUpdatesTightenDomainsWithoutRebuild) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 5 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
      integer_scaling_factor: 1
    }
  )pb");

  SharedBoundsManager shared_bounds_manager(proto);
  SharedClausesManager shared_clauses_manager(true);

  DenseModelCopy copy("test", proto, &shared_bounds_manager,
                      &shared_clauses_manager);
  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(&copy.proto(), &proto);
  EXPECT_EQ(copy.bounds_timestamp(), -1);
  EXPECT_EQ(copy.equivalences_timestamp(), -1);
  EXPECT_GT(copy.estimated_copy_dtime(), 0.0);
  EXPECT_EQ(copy.MapInnerObjectiveValue(10), 10);

  // Tighten bound on variable 0 without fixing it: [0, 10] -> [2, 8].
  shared_bounds_manager.ReportPotentialNewBounds("test", {0}, {2}, {8});
  shared_bounds_manager.Synchronize();
  copy.AddSearchDeterministicTime(100.0 * copy.estimated_copy_dtime());

  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(&copy.proto(), &proto);
  EXPECT_EQ(copy.bounds_timestamp(), -1);
  EXPECT_EQ(copy.var_domains()[0].Min(), 2);
  EXPECT_EQ(copy.var_domains()[0].Max(), 8);
}

TEST(DenseModelCopyTest,
     RateLimiterGatesRebuildsByRemovableVariablesAndSearchTime) {
  CpModelProto proto;
  for (int i = 0; i < 20; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(10);
  }
  auto* ct = proto.add_constraints()->mutable_linear();
  for (int i = 0; i < 20; ++i) {
    ct->add_vars(i);
    ct->add_coeffs(1);
  }
  ct->add_domain(0);
  ct->add_domain(100);
  auto* obj = proto.mutable_objective();
  obj->add_vars(0);
  obj->add_coeffs(2);
  obj->add_vars(15);
  obj->add_coeffs(3);
  obj->set_integer_scaling_factor(1);

  SharedBoundsManager shared_bounds_manager(proto);
  SharedClausesManager shared_clauses_manager(true);

  DenseModelCopy copy("test", proto, &shared_bounds_manager,
                      &shared_clauses_manager);
  EXPECT_EQ(copy.model_stats().num_variables, 20);
  EXPECT_EQ(copy.model_stats().num_constraints, 1);
  EXPECT_EQ(copy.model_stats().num_linear_terms, 22);
  const double initial_estimated_dtime = copy.estimated_copy_dtime();
  EXPECT_GT(initial_estimated_dtime, 1e-6);

  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains().size(), 20);

  // Fixing 1 out of 20 variables (x = 5%) requires search_dtime >= 20 *
  // estimated_copy_dtime(). At 19x it stays in place; reaching 20x triggers
  // the rebuild.
  shared_bounds_manager.ReportPotentialNewBounds("test", {0}, {5}, {5});
  shared_bounds_manager.Synchronize();
  copy.AddSearchDeterministicTime(19.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_TRUE(copy.var_domains().IsFixed(0));
  EXPECT_EQ(copy.var_domains()[0].FixedValue(), 5);
  EXPECT_EQ(copy.var_domains().size(), 20);

  copy.AddSearchDeterministicTime(1.5 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 19);
  EXPECT_EQ(copy.proto().variables_size(), 19);
  EXPECT_GE(copy.bounds_timestamp(), 0);
  EXPECT_EQ(copy.search_dtime_since_rebuild(), 0.0);
  EXPECT_LT(copy.estimated_copy_dtime(), initial_estimated_dtime);
  // 2 * x0 + 3 * x15 with x0 = 5 shifts objective offset by +10, so dense inner
  // objective for input inner objective 25 is 15.
  EXPECT_EQ(copy.MapInnerObjectiveValue(25), 15);

  // Now fix 9 of the remaining 19 variables (x = 9/19 ~ 47.4%). With 1.5x
  // search dtime (9/19 * 1.5 < 1.0), rebuild is still deferred; adding another
  // 1.0x (total 2.5x, so 9/19 * 2.5 > 1.0) triggers the rebuild much sooner
  // than 10x.
  shared_bounds_manager.ReportPotentialNewBounds(
      "test", {1, 2, 3, 4, 5, 6, 7, 8, 9}, {6, 3, 4, 2, 2, 2, 3, 4, 5},
      {6, 3, 4, 2, 2, 2, 3, 4, 5});
  shared_bounds_manager.Synchronize();
  copy.AddSearchDeterministicTime(1.5 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains().size(), 19);

  const double prev_estimated_dtime = copy.estimated_copy_dtime();
  copy.AddSearchDeterministicTime(1.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 10);
  EXPECT_EQ(copy.search_dtime_since_rebuild(), 0.0);
  EXPECT_LT(copy.estimated_copy_dtime(), prev_estimated_dtime);

  std::vector<int64_t> input_sol = {5, 6, 3, 4, 2, 2, 2, 3, 4, 5,
                                    7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
  EXPECT_EQ(copy.ReverseMapSolution(copy.MapSolution(input_sol)), input_sol);
}

TEST(DenseModelCopyTest, BinaryEquivalencesAndConflictingBounds) {
  CpModelProto proto;
  for (int i = 0; i < 10; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  auto* ct = proto.add_constraints()->mutable_bool_or();
  ct->add_literals(0);
  ct->add_literals(2);

  SharedBoundsManager shared_bounds_manager(proto);
  SharedClausesManager shared_clauses_manager(true);
  const int clause_id =
      shared_clauses_manager.RegisterNewId("sat", /*may_terminate_early=*/true);

  DenseModelCopy copy("test", proto, &shared_bounds_manager,
                      &shared_clauses_manager);
  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);

  // Export x1 == not(x0) via binary clauses (x0 V x1) and (~x0 V ~x1).
  shared_clauses_manager.AddBinaryClause(clause_id, 0, 1);
  shared_clauses_manager.AddBinaryClause(clause_id, NegatedRef(0),
                                         NegatedRef(1));
  ASSERT_EQ(shared_clauses_manager.NumEquivalences(), 1);

  // Rate-limited before search dtime accumulates.
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);

  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 9);
  EXPECT_EQ(copy.equivalences_timestamp(), 1);

  // Fix x0 = 1 incrementally; dense var 0 becomes [1, 1].
  shared_bounds_manager.ReportPotentialNewBounds("sat", {0}, {1}, {1});
  shared_bounds_manager.Synchronize();
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains()[0], Domain(1));

  // Reporting x1 = 1 conflicts with x1 == not(x0) and must return false
  // cleanly without crashing.
  shared_bounds_manager.ReportPotentialNewBounds("sat", {1}, {1}, {1});
  shared_bounds_manager.Synchronize();
  EXPECT_FALSE(copy.MaybeUpdate(updated));
}

TEST(DenseModelCopyTest,
     ModelStatsFilledDuringCopyTrackSchedulingAndLinearReductions) {
  // Variables:
  //   0, 1: optional presence literals [0, 1]
  //   2, 3, 4: start0, size0, end0 [0, 20]
  //   5, 6, 7: start1, size1, end1 [0, 20]
  //   8, 9: integer vars [0, 20]
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 2, 10 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 2, 10 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      enforcement_literal: [ 0 ]
      interval {
        start {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        end {
          vars: [ 4 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      enforcement_literal: [ 1 ]
      interval {
        start {
          vars: [ 5 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 6 ]
          coeffs: [ 1 ]
        }
        end {
          vars: [ 7 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
    constraints {
      cumulative {
        capacity { offset: 10 }
        intervals: [ 0, 1 ]
        demands { offset: 3 }
        demands {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      linear {
        vars: [ 2, 5, 8, 9 ]
        coeffs: [ 1, 2, 3, 4 ]
        domain: [ 0, 100 ]
      }
    }
    constraints { bool_or { literals: [ 0, 1 ] } }
    constraints {
      table {
        exprs {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 9 ]
          coeffs: [ 1 ]
        }
        values: [ 1, 2, 3, 4, 5, 6 ]
      }
    }
    objective {
      vars: [ 4, 7, 9 ]
      coeffs: [ 1, 1, 2 ]
      integer_scaling_factor: 1
    }
  )pb");

  CpModelProto copied_proto;
  Model sat_model;
  ASSERT_TRUE(CopyModel(proto, /*clear_internal_fields=*/false, &copied_proto,
                        &sat_model));
  const ModelStats initial_stats = ComputeModelStats(copied_proto);
  EXPECT_EQ(initial_stats.num_variables, 10);
  EXPECT_EQ(initial_stats.num_intervals, 2);
  EXPECT_EQ(initial_stats.num_interval_references, 4);
  EXPECT_EQ(initial_stats.num_table_values, 6);
  // 16 terms in `proto` + 6 terms from the 2 `start + size - end == 0`
  // constraints added by `CopyModel` on `first_copy`.
  EXPECT_EQ(initial_stats.num_linear_terms, 22);

  SharedBoundsManager shared_bounds(copied_proto);
  DenseModelCopy copy("test", copied_proto, &shared_bounds,
                      /*shared_clauses=*/nullptr);
  EXPECT_EQ(copy.model_stats(), initial_stats);
  EXPECT_DOUBLE_EQ(copy.estimated_copy_dtime(),
                   EstimateCopyDeterministicTime(initial_stats));

  // Fix presence literal 0 to false (unperformed interval 0) and fix var 8 = 3.
  shared_bounds.ReportPotentialNewBounds("sat", {0, 8}, {0, 3}, {0, 3});
  shared_bounds.Synchronize();
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());

  bool updated = false;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.model_stats().num_variables, 9);
  EXPECT_EQ(copy.model_stats().num_intervals, 1);
  EXPECT_EQ(copy.model_stats().num_interval_references, 2);
  EXPECT_EQ(copy.model_stats().num_table_values, 6);
  EXPECT_LT(copy.model_stats().num_linear_terms,
            initial_stats.num_linear_terms);
  EXPECT_LT(copy.estimated_copy_dtime(),
            EstimateCopyDeterministicTime(initial_stats));
}

TEST(DenseModelCopyTest, ChainedRebuildsMatchSingleShotAndShrinkWorkUnits) {
  // Variables:
  //   0..5: Booleans [0, 1] (v0 fixed to 1 via non-reified bool_and)
  //   6..15: integers [0, 20]
  const CpModelProto proto = ParseTestProto(R"pb(
    variables {
      name: "v0"
      domain: [ 0, 1 ]
    }
    variables {
      name: "v1"
      domain: [ 0, 1 ]
    }
    variables {
      name: "v2"
      domain: [ 0, 1 ]
    }
    variables {
      name: "v3"
      domain: [ 0, 1 ]
    }
    variables {
      name: "v4"
      domain: [ 0, 1 ]
    }
    variables {
      name: "v5"
      domain: [ 0, 1 ]
    }
    variables {
      name: "x6"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x7"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x8"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x9"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x10"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x11"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x12"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x13"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x14"
      domain: [ 0, 20 ]
    }
    variables {
      name: "x15"
      domain: [ 0, 20 ]
    }
    # Constraint 0: filtered out by active_constraints.
    constraints {
      linear {
        vars: [ 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 ]
        coeffs: [ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ]
        domain: [ 0, 200 ]
      }
    }
    # Constraint 1: non-reified bool_and fixing v0 = 1 during copy.
    constraints { bool_and { literals: [ 0 ] } }
    # Constraints 2..7: mix of clauses and linear constraints over all vars.
    constraints { bool_or { literals: [ -1, 1, 2, 3 ] } }
    constraints {
      enforcement_literal: [ 2 ]
      bool_or { literals: [ 3, -5, 1 ] }
    }
    constraints {
      linear {
        vars: [ 0, 1, 4, 6, 7, 8 ]
        coeffs: [ 2, 3, -4, 1, 2, -1 ]
        domain: [ -10, 40 ]
      }
    }
    constraints {
      linear {
        vars: [ 2, 5, 9, 10, 11 ]
        coeffs: [ 5, -2, 1, 3, 2 ]
        domain: [ 0, 50 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 12, 13, 14, 15 ]
        coeffs: [ 7, 1, 2, -3, 1, 4 ]
        domain: [ -20, 60 ]
      }
    }
    constraints {
      linear {
        vars: [ 6, 8, 10, 12, 14 ]
        coeffs: [ 1, 1, 1, 1, 1 ]
        domain: [ 5, 100 ]
      }
    }
    objective {
      vars: [ 0, 1, 2, 3, 4, 5, 6, 8, 9, 12, 14 ]
      coeffs: [ 4, 3, -2, 5, 6, -1, 2, 1, 3, 2, 4 ]
      offset: 10
      integer_before_offset: 10
      integer_scaling_factor: 1
    }
  )pb");

  auto active_constraints = [](int c) { return c != 0; };

  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(/*always_synchronize=*/true);
  const int sat_id =
      shared_clauses.RegisterNewId("sat", /*may_terminate_early=*/true);

  DenseModelCopy chained("chained", proto, &shared_bounds, &shared_clauses,
                         active_constraints);

  // Rebuild 1: triggered immediately by active_constraints_ != nullptr.
  // Filters constraint 0 and propagates bool_and { [0] } -> v0 = 1.
  bool updated = false;
  ASSERT_TRUE(chained.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(chained.var_domains().size(), 16);
  EXPECT_TRUE(chained.var_domains().IsFixed(0));
  EXPECT_EQ(chained.var_domains()[0].FixedValue(), 1);
  EXPECT_EQ(chained.proto().constraints_size(), 6);
  const double dtime1 = chained.estimated_copy_dtime();
  EXPECT_GT(dtime1, 1e-6);

  // Rebuild 2: fix x6 = 4, x7 = 2 and add negated + transitive equivalences
  //   v2 == v1, v3 == ~v2 (=> v3 == ~v1), v4 == v0 (so bool_and fixing v0 = 1
  // fixes v4 = 1 via PropagateInputEquivalences!).
  shared_bounds.ReportPotentialNewBounds("sat", {6, 7}, {4, 2}, {4, 2});
  shared_bounds.Synchronize();
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(1), 2);
  shared_clauses.AddBinaryClause(sat_id, 1, NegatedRef(2));
  shared_clauses.AddBinaryClause(sat_id, 2, 3);
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(2), NegatedRef(3));
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(0), 4);
  shared_clauses.AddBinaryClause(sat_id, 0, NegatedRef(4));
  chained.AddSearchDeterministicTime(10.0 * chained.estimated_copy_dtime());

  ASSERT_TRUE(chained.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  // Removed: x6, x7, v2, v3, v4 (v0 kept as first_fixed_literal) => 11 left.
  EXPECT_EQ(chained.var_domains().size(), 11);
  const double dtime2 = chained.estimated_copy_dtime();
  EXPECT_LT(dtime2, dtime1);
  EXPECT_GT(dtime2, 1e-6);

  // Rebuild 3: fix x8 = 5, x9 = 3, x12 = 3 and add transitive negated
  // equivalence v5 == ~v3 (so v5 == v1). Fixing x12 = 3 avoids making x12 an
  // extra `first_fixed_literal` in the single-shot baseline before bool_and
  // fixes v0 = 1.
  shared_bounds.ReportPotentialNewBounds("sat", {8, 9, 12}, {5, 3, 3},
                                         {5, 3, 3});
  shared_bounds.Synchronize();
  shared_clauses.AddBinaryClause(sat_id, 3, 5);
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(3), NegatedRef(5));
  chained.AddSearchDeterministicTime(10.0 * chained.estimated_copy_dtime());

  ASSERT_TRUE(chained.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  // Removed additionally: x8, x9, x12, v5 => 7 vars remaining.
  EXPECT_EQ(chained.var_domains().size(), 7);
  const double dtime3 = chained.estimated_copy_dtime();
  EXPECT_LT(dtime3, dtime2);

  // Compare everything against a fresh single-shot DenseModelCopy seeing the
  // accumulated bounds & equivalences all at once.
  DenseModelCopy single_shot("single_shot", proto, &shared_bounds,
                             &shared_clauses, active_constraints);
  bool single_shot_updated = false;
  ASSERT_TRUE(single_shot.MaybeUpdate(single_shot_updated));
  EXPECT_TRUE(single_shot_updated);

  EXPECT_THAT(chained.proto(), EqualsProto(single_shot.proto()));
  EXPECT_EQ(chained.model_stats(), single_shot.model_stats());
  ASSERT_EQ(chained.var_domains().size(), single_shot.var_domains().size());
  for (int v = 0; v < chained.var_domains().size(); ++v) {
    EXPECT_EQ(chained.var_domains()[v], single_shot.var_domains()[v])
        << "var " << v;
  }

  // Test solution mapping round-trip & objective mapping consistency.
  const std::vector<int64_t> input_sol = {
      /*v0=*/1,  /*v1=*/0,  /*v2=*/0,  /*v3=*/1,   /*v4=*/1,  /*v5=*/0,
      /*x6=*/4,  /*x7=*/2,  /*x8=*/5,  /*x9=*/3,   /*x10=*/7, /*x11=*/8,
      /*x12=*/3, /*x13=*/9, /*x14=*/6, /*x15=*/10,
  };
  const std::vector<int64_t> chained_dense = chained.MapSolution(input_sol);
  const std::vector<int64_t> single_dense = single_shot.MapSolution(input_sol);
  EXPECT_EQ(chained_dense, single_dense);
  EXPECT_EQ(chained.ReverseMapSolution(chained_dense), input_sol);
  EXPECT_EQ(single_shot.ReverseMapSolution(single_dense), input_sol);
  EXPECT_EQ(chained.MapInnerObjectiveValue(100),
            single_shot.MapInnerObjectiveValue(100));
}

TEST(DenseModelCopyTest, RedundantEquivalencesOnRemovedVarsSkipRebuild) {
  CpModelProto proto;
  for (int i = 0; i < 10; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int i = 0; i < 9; ++i) {
    auto* ct = proto.add_constraints()->mutable_bool_or();
    ct->add_literals(i);
    ct->add_literals(i + 1);
  }

  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(/*always_synchronize=*/true);
  const int sat_id =
      shared_clauses.RegisterNewId("sat", /*may_terminate_early=*/true);

  DenseModelCopy copy("test", proto, &shared_bounds, &shared_clauses);
  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);

  // Fix variables 0, 1, 2: builds custom dense copy keeping var 0 as
  // first_fixed_literal and removing vars 1 and 2 (8 dense vars remain).
  shared_bounds.ReportPotentialNewBounds("sat", {0, 1, 2}, {1, 0, 1},
                                         {1, 0, 1});
  shared_bounds.Synchronize();
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 8);
  EXPECT_EQ(copy.equivalences_timestamp(), 0);

  // Export consistent equivalence between already-removed fixed vars 1 and 2
  // (x1 == ~x2). Even with sufficient search dtime, MaybeUpdate should skip
  // rebuilding (`updated == false`) and keep `equivalences_timestamp()`
  // unchanged so callers do not reset search weights.
  shared_clauses.AddBinaryClause(sat_id, 1, 2);
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(1), NegatedRef(2));
  ASSERT_EQ(shared_clauses.NumEquivalences(), 1);
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());

  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.equivalences_timestamp(), 0);
  EXPECT_EQ(copy.var_domains().size(), 8);

  // A subsequent call without new equivalences also stays a no-op.
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);

  // Also check redundant equivalence between removed var 1 and
  // first_fixed_literal var 0 (x1 == ~x0).
  shared_clauses.AddBinaryClause(sat_id, 0, 1);
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(0), NegatedRef(1));
  ASSERT_EQ(shared_clauses.NumEquivalences(), 2);
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());

  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.equivalences_timestamp(), 0);
  EXPECT_EQ(copy.var_domains().size(), 8);
}

TEST(DenseModelCopyTest, ChainedRebuildFixesNonHeadMergedVariable) {
  CpModelProto proto;
  for (int i = 0; i < 6; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  {
    auto* ct = proto.add_constraints()->mutable_bool_or();
    ct->add_literals(1);
    ct->add_literals(4);
    ct->add_literals(5);
  }

  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(/*always_synchronize=*/true);
  const int sat_id =
      shared_clauses.RegisterNewId("sat", /*may_terminate_early=*/true);

  DenseModelCopy copy("test", proto, &shared_bounds, &shared_clauses);
  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);

  // Rebuild 1: fix v0 = 0 (kept as first_fixed_literal) and merge v2 == v1 and
  // v3 == ~v2 into dense var 1 (active list: v1 -> v2 -> v3). Remaining dense
  // vars: [v0=0, v1, v4, v5] (size 4).
  shared_bounds.ReportPotentialNewBounds("sat", {0}, {0}, {0});
  shared_bounds.Synchronize();
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(1), 2);
  shared_clauses.AddBinaryClause(sat_id, 1, NegatedRef(2));
  shared_clauses.AddBinaryClause(sat_id, 2, 3);
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(2), NegatedRef(3));
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 4);

  // Rebuild 2: fix non-head member v3 = 0 (mapping to ~d1, so d1 = 1) and add
  // equivalence v4 == v2 referencing non-head member v2. Propagate across the
  // linked list should fix v1 = 1, v2 = 1, v3 = 0, v4 = 1 and remove the entire
  // class plus v4 (leaving only [v0=0, v5]).
  shared_bounds.ReportPotentialNewBounds("sat", {3}, {0}, {0});
  shared_bounds.Synchronize();
  shared_clauses.AddBinaryClause(sat_id, NegatedRef(2), 4);
  shared_clauses.AddBinaryClause(sat_id, 2, NegatedRef(4));
  copy.AddSearchDeterministicTime(10.0 * copy.estimated_copy_dtime());
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 2);

  EXPECT_EQ(copy.ReverseMapSolution({0, 1}),
            (std::vector<int64_t>{0, 1, 1, 0, 1, 1}));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
