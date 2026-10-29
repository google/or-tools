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

#include "ortools/sat/cp_constraint_presolve.h"

#include <cstdint>
#include <string>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

CpModelProto PresolveOneConstraint(const CpModelProto& initial_model,
                                   const int constraint_index) {
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  presolver.PresolveOneConstraint(constraint_index);
  presolver.RemoveEmptyConstraints();
  for (int i = 0; i < presolved_model.variables_size(); ++i) {
    FillDomainInProto(context.DomainOf(i),
                      presolved_model.mutable_variables(i));
  }
  return presolved_model;
}

TEST(PresolveCpModelTest, LinMaxBasicPresolveExprs) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -2, -1 ] }
    variables { domain: [ -3, 0 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 2, 3 ]
          offset: -5
        }
        exprs {
          vars: [ 1, 2 ]
          coeffs: [ 2, -5 ]
          offset: -6
        }
        exprs {
          vars: [ 0, 2 ]
          coeffs: [ -2, 3 ]
        }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -2, -1 ] }
    variables { domain: [ -1, 0 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 2, 3 ]
          offset: -5
        }
        exprs {
          vars: [ 1, 2 ]
          coeffs: [ 2, -5 ]
          offset: -6
        }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, InferAllDiffsControlsNoOverlapConversion) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }

    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 1 }
        end { vars: 0 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 1 }
        end { vars: 1 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 1 }
        end { vars: 2 coeffs: 1 offset: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;

  SatParameters* params = model.GetOrCreate<SatParameters>();
  params->set_keep_all_feasible_solutions_in_presolve(true);
  params->set_infer_all_diffs(false);

  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpConstraintPresolver presolver(&context);

  context.InitializeNewDomains();

  presolver.PresolveOneConstraint(3);
  presolver.RemoveEmptyConstraints();

  bool has_no_overlap = false;
  bool has_all_diff = false;

  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.has_no_overlap()) has_no_overlap = true;
    if (ct.has_all_diff()) has_all_diff = true;
  }

  EXPECT_TRUE(has_no_overlap);
  EXPECT_FALSE(has_all_diff);
}

TEST(PresolveCpModelTest, IntProdWithLeftConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 12 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 0, 100 ]
    }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 12 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 20, 24 ]
    }
    constraints {
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EnforcedIntProdWithLeftConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithRightConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 14 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 0, 100 ]
    }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 14 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 20, 28 ]
    }
    constraints {
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithXEqualX2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -3, 5 ] }
    variables { domain: [ -30, 30 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -3, 5 ] }
    variables { domain: [ 0, 1, 4, 4, 9, 9, 16, 16, 25, 25 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareLargeDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ -200000, 200000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ 0, 12100 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareExprDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ -9000, 9000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 94 ] }
    variables { domain: [ 0, 9000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdCoeffDividesTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 1000 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 10 offset: 20 }
        exprs { vars: 0 coeffs: 1 offset: 3 }
        exprs { vars: 1 coeffs: 5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 1, 58 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 2 offset: 4 }
        exprs { vars: 0 coeffs: 1 offset: 3 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 10, 4, 3 ]
        domain: [ 0, 29 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 9, 9, 4, 3 ]
        domain: [ 0, 26 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms3) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 10, 7 ]
        domain: [ 0, 17 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DetectApproximateGCD) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1001, 999 ]
        domain: [ 0, 28500 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 28 ] }
    variables { domain: [ 0, 28 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 28 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     LinearConstraintWithGcdFalseConstraintWithEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 4, 4 ]
        domain: [ 9, 9 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntervalPresolveNegativeSize) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -7, -7, 0, 0 ] }
    constraints {
      interval {
        start { offset: 0 }
        end { vars: 0 coeffs: 1 }
        size { vars: 0 coeffs: 1 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolXorNotPresolvedIfEnforcementUnknown) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      bool_xor { literals: [ 0, 1 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(initial_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor {}
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor { literals: [ 1, 1 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced3) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor { literals: [ 1, -2, 3 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DetectSimpleSum) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 10, 10, 3 ]
        domain: [ 0, 22 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3 ]
                coeffs: [ 1, 1, 1, 1 ]
                domain: [ 0, 2 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTilting) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 17, 40 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, 2 ]
                domain: [ 0, 5 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTiltingMixedSignsAndOffset) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -5, 0 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 17, -40 ]
        domain: [ -1000, 100 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, -2 ]
                domain: [ 0, 5 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTiltingLargeDomain) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 100001, 200003 ]
        domain: [ 0, 200053000 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, 2 ]
                domain: [ 0, 2000 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTiltingShiftedQuadrant) {
  // Here a1 * R1 = 170 and a2 * R2 = 400, so B = 450 triggers both shifts:
  // s1 = floor((450 - 400) / 17) = 2 and s2 = floor((450 - 170) / 40) = 7.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 17, 40 ]
        domain: [ 0, 450 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 2, 5 ]
                domain: [ 0, 55 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTiltingLowerBound) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 17, 40 ]
        domain: [ 185, 285 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, 2 ]
                domain: [ 10, 15 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectTwoTermTiltingRandom) {
  absl::BitGen random;
  for (int iter = 0; iter < 100; ++iter) {
    const int64_t a = absl::Uniform<int64_t>(random, 1, 101);
    const int64_t b = absl::Uniform<int64_t>(random, 1, 101);
    const int64_t ub_x = absl::Uniform<int64_t>(random, 0, 101);
    const int64_t ub_y = absl::Uniform<int64_t>(random, 0, 101);
    const int64_t c = absl::Uniform<int64_t>(random, 0, 101);

    CpModelProto model_proto;
    FillDomainInProto(Domain(0, ub_x), model_proto.add_variables());
    FillDomainInProto(Domain(0, ub_y), model_proto.add_variables());
    LinearConstraintProto* lin =
        model_proto.add_constraints()->mutable_linear();
    lin->add_vars(0);
    lin->add_vars(1);
    lin->add_coeffs(a);
    lin->add_coeffs(b);
    FillDomainInProto(Domain(0, c), lin);

    CpModelProto mapping_model;
    Model model;
    PresolveContext context(&model, &model_proto, &mapping_model);
    CpConstraintPresolver presolver(&context);
    context.InitializeNewDomains();
    presolver.DetectTwoTermTilting(0, model_proto.mutable_constraints(0));

    const auto& new_lin = model_proto.constraints(0).linear();
    const Domain rhs_domain = ReadDomainFromProto(new_lin);
    const int64_t new_a = new_lin.coeffs(0);
    const int64_t new_b = new_lin.coeffs(1);
    ASSERT_LE(new_a, a);
    ASSERT_LE(new_b, b);

    for (int64_t x = 0; x <= ub_x; ++x) {
      for (int64_t y = 0; y <= ub_y; ++y) {
        const bool orig_feasible = (a * x + b * y <= c);
        const bool presolved_feasible =
            rhs_domain.Contains(new_a * x + new_b * y);
        ASSERT_EQ(orig_feasible, presolved_feasible);
      }
    }
  }
}

TEST(PresolveCpModelTest, TiltTwoTermConstraint) {
  {
    const TwoTermTilt tilt = TiltTwoTermConstraint(
        /*a1=*/17, /*a2=*/40, /*B=*/100, /*R1=*/5, /*R2=*/5);
    EXPECT_EQ(tilt.new_a1, 1);
    EXPECT_EQ(tilt.new_a2, 2);
    EXPECT_EQ(tilt.best_ub, 5);
  }
  {
    const TwoTermTilt tilt = TiltTwoTermConstraint(
        /*a1=*/17, /*a2=*/40, /*B=*/450, /*R1=*/10, /*R2=*/10);
    EXPECT_EQ(tilt.new_a1, 2);
    EXPECT_EQ(tilt.new_a2, 5);
    EXPECT_EQ(tilt.best_ub, 55);
  }

  absl::BitGen random;
  for (int iter = 0; iter < 500; ++iter) {
    const int64_t a1 = absl::Uniform<int64_t>(random, 1, 100);
    const int64_t a2 = absl::Uniform<int64_t>(random, a1 + 1, 200);
    const int64_t R1 = absl::Uniform<int64_t>(random, 1, 50);
    const int64_t R2 = absl::Uniform<int64_t>(random, 1, 50);
    const int64_t B = absl::Uniform<int64_t>(random, 0, a1 * R1 + a2 * R2);

    const TwoTermTilt tilt = TiltTwoTermConstraint(a1, a2, B, R1, R2);
    ASSERT_GT(tilt.new_a1, 0);
    ASSERT_GT(tilt.new_a2, 0);
    ASSERT_LE(tilt.new_a1, a1);
    ASSERT_LE(tilt.new_a2, a2);

    bool tight = false;
    for (int64_t x1 = 0; x1 <= R1; ++x1) {
      for (int64_t x2 = 0; x2 <= R2; ++x2) {
        const bool orig_feasible = (a1 * x1 + a2 * x2 <= B);
        const int64_t tilted_lhs = tilt.new_a1 * x1 + tilt.new_a2 * x2;
        ASSERT_EQ(orig_feasible, tilted_lhs <= tilt.best_ub);
        if (orig_feasible && tilted_lhs == tilt.best_ub) tight = true;
      }
    }
    EXPECT_TRUE(tight);
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
