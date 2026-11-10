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
      enforcement_literal: [ 4, -4 ]
      lin_max {
        target {
          vars: [ 1, 2 ]
          coeffs: [ 2, 2 ]
          offset: 5
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, 2 ]
          offset: 3
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 5
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 1
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
      EstimateCopyDeterministicTime(ComputeModelStats(proto)));

  // Even if initial copy proves the model infeasible, deterministic time for
  // the input model is still recorded.
  proto.add_constraints()->mutable_bool_or();  // Empty clause -> UNSAT.
  Model unsat_model;
  EXPECT_FALSE(CopyModel(proto, /*clear_internal_fields=*/true, &copied_proto,
                         &unsat_model));
  EXPECT_DOUBLE_EQ(
      unsat_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime(),
      EstimateCopyDeterministicTime(ComputeModelStats(proto)));
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
  EXPECT_EQ(copy.MapInnerObjectiveValue(10), 10);

  // Tighten bound on variable 0 without fixing it: [0, 10] -> [2, 8].
  shared_bounds_manager.ReportPotentialNewBounds("test", {0}, {2}, {8});
  shared_bounds_manager.Synchronize();

  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(&copy.proto(), &proto);
  EXPECT_EQ(copy.bounds_timestamp(), -1);
  EXPECT_EQ(copy.var_domains()[0].Min(), 2);
  EXPECT_EQ(copy.var_domains()[0].Max(), 8);
}

TEST(DenseModelCopyTest, MinVariableReductionRatioGatesRebuilds) {
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
  const ModelStats initial_stats = ComputeModelStats(copy.proto());
  EXPECT_EQ(initial_stats.num_variables, 20);
  EXPECT_EQ(initial_stats.num_constraints, 1);
  EXPECT_EQ(initial_stats.num_linear_terms, 22);
  const double initial_estimated_dtime =
      EstimateCopyDeterministicTime(initial_stats);
  EXPECT_GT(initial_estimated_dtime, 1e-6);

  bool updated = true;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains().size(), 20);

  // Fixing 1 out of 20 variables (x = 5%): deferred when
  // min_variable_reduction_ratio = 1/19 (~5.26%), triggered at 1/20.5 (~4.88%).
  shared_bounds_manager.ReportPotentialNewBounds("test", {0}, {5}, {5});
  shared_bounds_manager.Synchronize();
  ASSERT_TRUE(copy.MaybeUpdate(updated, 1.0 / 19.0));
  EXPECT_FALSE(updated);
  EXPECT_TRUE(copy.var_domains().IsFixed(0));
  EXPECT_EQ(copy.var_domains()[0].FixedValue(), 5);
  EXPECT_EQ(copy.var_domains().size(), 20);

  ASSERT_TRUE(copy.MaybeUpdate(updated, 1.0 / 20.5));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 19);
  EXPECT_EQ(copy.proto().variables_size(), 19);
  EXPECT_GE(copy.bounds_timestamp(), 0);
  const double prev_estimated_dtime =
      EstimateCopyDeterministicTime(ComputeModelStats(copy.proto()));
  EXPECT_LT(prev_estimated_dtime, initial_estimated_dtime);
  // 2 * x0 + 3 * x15 with x0 = 5 shifts objective offset by +10, so dense inner
  // objective for input inner objective 25 is 15.
  EXPECT_EQ(copy.MapInnerObjectiveValue(25), 15);

  // Now fix 9 of the remaining 19 variables (x = 9/19 ~ 47.4%). Deferred at
  // min_variable_reduction_ratio = 1/1.5 (~66.7%), triggered at 1/2.5 (40%).
  shared_bounds_manager.ReportPotentialNewBounds(
      "test", {1, 2, 3, 4, 5, 6, 7, 8, 9}, {6, 3, 4, 2, 2, 2, 3, 4, 5},
      {6, 3, 4, 2, 2, 2, 3, 4, 5});
  shared_bounds_manager.Synchronize();
  ASSERT_TRUE(copy.MaybeUpdate(updated, 1.0 / 1.5));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains().size(), 19);

  ASSERT_TRUE(copy.MaybeUpdate(updated, 1.0 / 2.5));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 10);
  EXPECT_LT(EstimateCopyDeterministicTime(ComputeModelStats(copy.proto())),
            prev_estimated_dtime);

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

  // Deferred when min_variable_reduction_ratio exceeds 1/10.
  ASSERT_TRUE(copy.MaybeUpdate(updated, 0.2));
  EXPECT_FALSE(updated);

  ASSERT_TRUE(copy.MaybeUpdate(updated, 0.1));
  EXPECT_TRUE(updated);
  EXPECT_EQ(copy.var_domains().size(), 9);
  EXPECT_EQ(copy.equivalences_timestamp(), 1);

  // Fix x0 = 1 incrementally without rebuilding; dense var 0 becomes [1, 1].
  shared_bounds_manager.ReportPotentialNewBounds("sat", {0}, {1}, {1});
  shared_bounds_manager.Synchronize();
  ASSERT_TRUE(copy.MaybeUpdate(updated, 0.5));
  EXPECT_FALSE(updated);
  EXPECT_EQ(copy.var_domains()[0], Domain(1));

  // Reporting x1 = 1 conflicts with x1 == not(x0) and must return false
  // cleanly without crashing.
  shared_bounds_manager.ReportPotentialNewBounds("sat", {1}, {1}, {1});
  shared_bounds_manager.Synchronize();
  EXPECT_FALSE(copy.MaybeUpdate(updated));
}

TEST(DenseModelCopyTest, ComputeModelStatsTracksSchedulingAndLinearReductions) {
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

  // Fix presence literal 0 to false (unperformed interval 0) and fix var 8 = 3.
  shared_bounds.ReportPotentialNewBounds("sat", {0, 8}, {0, 3}, {0, 3});
  shared_bounds.Synchronize();

  bool updated = false;
  ASSERT_TRUE(copy.MaybeUpdate(updated));
  EXPECT_TRUE(updated);
  const ModelStats reduced_stats = ComputeModelStats(copy.proto());
  EXPECT_EQ(reduced_stats.num_variables, 9);
  EXPECT_EQ(reduced_stats.num_intervals, 1);
  EXPECT_EQ(reduced_stats.num_interval_references, 2);
  EXPECT_EQ(reduced_stats.num_table_values, 6);
  EXPECT_LT(reduced_stats.num_linear_terms, initial_stats.num_linear_terms);
  EXPECT_LT(EstimateCopyDeterministicTime(reduced_stats),
            EstimateCopyDeterministicTime(initial_stats));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
