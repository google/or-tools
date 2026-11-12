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
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::IsEmpty;

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
    presolver.DetectTwoTermTilting(model_proto.mutable_constraints(0),
                                   context.BuildMergedCanonicalLinearConstraint(
                                       model_proto.constraints(0).linear()));

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

TEST(PresolveCpModelTest, DetectApproximateGCDWithMergedTerms) {
  // 1000*x0 + 1000*x1 + 2001*x2 <= 5005 with x0, x1, x2 in [0, 5].
  // x0 and x1 have equal magnitude 1000 and merge into a single canonical term
  // with range = 10. With divisor 1000, this reduces to x0 + x1 + 2*x2 <= 5.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1000, 1000, 2001 ]
        domain: [ 0, 5005 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 1, 2 ]
                domain: [ 0, 5 ]
              )pb"));
}

TEST(PresolveCpModelTest, DetectApproximateGCDWithMultipleDistinctMergedTerms) {
  // 1000*x0 + 2000*x1 + 2999*x2 + 2999*x3 <= 13996 with x0, x1 in [0, 1] and
  // x2, x3 in [0, 3]. x2 and x3 have equal magnitude 2999 and merge into a
  // single canonical term of range 6. With divisor 1000, this reduces to
  // x0 + 2*x1 + 3*x2 + 3*x3 <= 14.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1000, 2000, 2999, 2999 ]
        domain: [ 0, 13996 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  const auto& lin = presolved.constraints(0).linear();
  EXPECT_THAT(lin, testing::EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3 ]
                coeffs: [ 1, 2, 3, 3 ]
                domain: [ 0, 14 ]
              )pb"));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMInEquality) {
  // x + y + 100*b1 - 100*b2 == 1 with x, y, b1, b2 in [0, 1].
  // Any non-zero value of 100*(b1 - b2) pushes the LHS outside [1, 1], so the
  // constraint decouples into b1 - b2 == 0 and x + y == 1.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 100, -100 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMInNarrowRange) {
  // 1 <= x + y + z + 150*b1 - 100*b2 <= 2 with x, y, z in [0, 1],
  // b1 in {0, 2}, b2 in {0, 3}. Here gcd(150, 100) = 50 > max(2 - 0, 3 - 1),
  // so this decouples into 3*b1 - 2*b2 == 0 and 1 <= x + y + z <= 2.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 0, 3, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 150, -100 ]
        domain: [ 1, 2 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 0, 3, 3 ] }
    constraints {
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 3, -2 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 1, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMWithNonZeroForcedMultiple) {
  // x + y + 100*y1 - 100*y2 == 101 with x, y in [0, 1] and y1, y2 in [1, 3].
  // Domain propagation tightens y1 to [2, 3] and y2 to [1, 2], and big-M
  // decoupling splits the constraint into y1 - y2 == 1 and x + y == 1.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 100, -100 ]
        domain: [ 101, 101 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 2, 3 ] }
    variables { domain: [ 1, 2 ] }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMWithEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 100, -100 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMInfeasibleGap) {
  // With 10 variables, PropagateDomainsInLinear uses fast interval bounds
  // [-100, 108] which contain 50, and overall gcd is 1, so only the big-M
  // suffix check detects that 100*(b1 - b2) has no multiple in [42, 50].
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
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 10
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        coeffs: [ 1, 1, 1, 1, 1, 1, 1, 1, 100, -100 ]
        domain: [ 50, 50 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved.constraints(), IsEmpty());
  EXPECT_THAT(presolved.variables(10), EqualsProto(R"pb(domain: [ 0, 0 ])pb"));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMDoesNotTriggerAtBoundary) {
  // x + y + z + 2*b1 - 2*b2 == 2 with x, y, z, b1, b2 in [0, 1].
  // Here big_gcd = 2 == max(ub - min(R), max(R) - lb) = max(2 - 0, 3 - 2) = 2:
  // both (b1 == b2, x + y + z == 2) and (b1 = 1, b2 = 0, x = y = z = 0) are
  // feasible, so the constraint must NOT decouple.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 2, -2 ]
        domain: [ 2, 2 ]
      }
    }
  )pb");

  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(initial_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMStopsOnFirstSplit) {
  // x + y + 100*b1 - 100*b2 + 200*c1 - 200*c2 == 1 on Booleans: stopping on the
  // first valid suffix split (big_gcd = 200 > 102) immediately peels off
  // c1 - c2 == 0 rather than mixing tiers into b1 - b2 + 2*c1 - 2*c2 == 0.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5 ]
        coeffs: [ 1, 1, 100, -100, 200, -200 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 4, 5 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 100, -100 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMDividesSmallPartByGcd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 2, 2, 2, 105, -105 ]
        domain: [ 2, 4 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 1, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMInfeasibleAfterSmallGcd) {
  // Peeling forced 5769*v2 == 0 leaves -1024*v0 - 1024*v1 in [-2773, -2767],
  // whose domain becomes empty after dividing by small_gcd = 1024.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ -3, 3 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ -1024, -1024, 5769 ]
        domain: [ -2773, -2767 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved.constraints(), IsEmpty());
  EXPECT_THAT(presolved.variables(3), EqualsProto(R"pb(domain: [ 0, 0 ])pb"));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMOneSidedBounds) {
  const CpModelProto upper_only_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 10 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  const CpModelProto expected_upper_only = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(upper_only_model, 0),
              EqualsProto(expected_upper_only));

  const CpModelProto lower_only_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 10 ]
        domain: [ 24, 26 ]
      }
    }
  )pb");
  const CpModelProto expected_lower_only = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        domain: [ 4, 6 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(lower_only_model, 0),
              EqualsProto(expected_lower_only));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMClampedBounds) {
  // Exercises clamping of big_lb / big_ub when MaxBoundedSubsetSum falls back
  // to GCD mode (num_values > 10), as well as clamping small_lb / small_ub to
  // [0, max_error] when > 10 variables keep fast interval bounds.
  const CpModelProto gcd_mode_lb = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 10 ]
        domain: [ 5, 8 ]
      }
    }
  )pb");
  EXPECT_EQ(PresolveOneConstraint(gcd_mode_lb, 0).constraints_size(), 2);

  const CpModelProto gcd_mode_ub = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 10 ]
        domain: [ 122, 125 ]
      }
    }
  )pb");
  EXPECT_EQ(PresolveOneConstraint(gcd_mode_ub, 0).constraints_size(), 2);

  const CpModelProto clamp_r_lb = ParseTestProto(R"pb(
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
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 10
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        coeffs: [ 1, 1, 1, 1, 1, 1, 1, 1, 100, -100 ]
        domain: [ -50, 1 ]
      }
    }
  )pb");
  const CpModelProto clamp_r_lb_presolved =
      PresolveOneConstraint(clamp_r_lb, 0);
  ASSERT_EQ(clamp_r_lb_presolved.constraints_size(), 2);
  EXPECT_THAT(clamp_r_lb_presolved.constraints(1).linear().domain(),
              ElementsAre(0, 1));

  const CpModelProto clamp_r_ub = ParseTestProto(R"pb(
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
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 10
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        coeffs: [ 1, 1, 1, 1, 1, 1, 1, 1, 100, -100 ]
        domain: [ 7, 50 ]
      }
    }
  )pb");
  const CpModelProto clamp_r_ub_presolved =
      PresolveOneConstraint(clamp_r_ub, 0);
  ASSERT_EQ(clamp_r_ub_presolved.constraints_size(), 2);
  EXPECT_THAT(clamp_r_ub_presolved.constraints(1).linear().domain(),
              ElementsAre(7, 8));
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMSkipsNewlyFixedVar) {
  // PropagateDomainsInLinear intersects doubleton variable 2 with its second
  // constraint (fixing it to 5) without recanonicalizing constraint 0; the
  // decoupling pass must skip fixed variable 2 when splitting constraint 0.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 100, -100 ]
        domain: [ 6, 6 ]
      }
    }
    constraints {
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 5, 5 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 3, 4 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 4 ]
      }
    }
  )pb");

  CpModelProto presolved = initial_model;
  CpModelProto mapping_model;
  Model model;
  PresolveContext context(&model, &presolved, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  presolver.PresolveOneConstraint(0);
  presolver.RemoveEmptyConstraints();

  ASSERT_EQ(presolved.constraints_size(), 4);
  EXPECT_THAT(presolved.constraints(2), EqualsProto(R"pb(
                linear {
                  vars: [ 3, 4 ]
                  coeffs: [ 1, -1 ]
                  domain: [ 0, 0 ]
                }
              )pb"));
  EXPECT_THAT(presolved.constraints(3), EqualsProto(R"pb(
                linear {
                  vars: [ 0, 1 ]
                  coeffs: [ 1, 1 ]
                  domain: [ 1, 1 ]
                }
              )pb"));
}

TEST(PresolveCpModelTest, TightenBigMInBoxedLinearConstraintBoolean) {
  // 1 <= x + y + 1000 * b <= 1000 with x, y in [-5, 5] and b in [0, 1].
  // min_activity = -10 (ub_threshold = 11), max_activity = 1010
  // (lb_threshold = 10), so 1000 is tightened to max(11, 10) = 11 and the RHS
  // becomes [1, 11].
  const CpModelProto initial_model = ParseTextProtoOrDie(R"pb(
    variables { domain: [ -5, 5 ] }
    variables { domain: [ -5, 5 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1000 ]
        domain: [ 1, 1000 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 1, 11 ]
                domain: [ 1, 11 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     TightenBigMInBoxedLinearConstraintNegativeAndGeneralIntegerWithGcd) {
  // -3984 <= 2 * x - 1000 * z <= -1990 with x in [-10, 10] and z in [2, 4].
  // min_activity = -4020 (ub_threshold = 36), max_activity = -1980
  // (lb_threshold = 10), so -1000 is tightened to -36 with RHS [-128, -62],
  // and GCD division by 2 reduces this to -64 <= x - 18 * z <= -31.
  const CpModelProto initial_model = ParseTextProtoOrDie(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ 2, 4 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 2, -1000 ]
        domain: [ -3984, -1990 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, -18 ]
                domain: [ -64, -31 ]
              )pb"));
}

TEST(PresolveCpModelTest, TightenBigMInBoxedLinearConstraintMultipleTerms) {
  // -995 <= x + 1000 * b0 - 1000 * b1 <= 995 with x in [-10, 10] and
  // b0, b1 in [0, 1] tightens both big-M coefficients to 15 in one pass.
  const CpModelProto initial_model = ParseTextProtoOrDie(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1000, -1000 ]
        domain: [ -995, 995 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 15, -15 ]
                domain: [ -10, 10 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     TightenBigMInBoxedLinearConstraintSkipsMultiIntervalRhs) {
  // Multi-interval RHS domains must not be shifted/tightened as a single box,
  // even when max_coeff_magnitude (3000) exceeds max(ub_threshold,
  // lb_threshold) = max(2900, 2516) = 2900.
  CpModelProto hole_rhs_model;
  LinearConstraintProto* hole_lin =
      hole_rhs_model.add_constraints()->mutable_linear();
  for (int i = 0; i < 8; ++i) {
    IntegerVariableProto* var = hole_rhs_model.add_variables();
    var->add_domain(0);
    var->add_domain(2);
    hole_lin->add_vars(i);
    hole_lin->add_coeffs(1);
  }
  for (const int64_t coeff : {500, 3000}) {
    const int idx = hole_rhs_model.variables_size();
    IntegerVariableProto* b = hole_rhs_model.add_variables();
    b->add_domain(0);
    b->add_domain(1);
    hole_lin->add_vars(idx);
    hole_lin->add_coeffs(coeff);
  }
  for (const int64_t bound : {1, 1000, 2900, 3515}) {
    hole_lin->add_domain(bound);
  }
  const CpModelProto presolved_hole_rhs =
      PresolveOneConstraint(hole_rhs_model, 0);
  ASSERT_EQ(presolved_hole_rhs.constraints_size(), 1);
  EXPECT_THAT(presolved_hole_rhs.constraints(0).linear(),
              EqualsProto(hole_rhs_model.constraints(0).linear()));
}

TEST(PresolveCpModelTest, TightenBigMInBoxedLinearConstraintDetectsUnsat) {
  // With 10 variables (above PropagateDomainsInLinear's slow_mode cutoff):
  // sum_{i=0..8} y_i in [0, 18] and b in [0, 1].
  // 40 <= sum y_i + 100 * b <= 45 has threshold = max(40, 118 - 45) = 73 < 100.
  // Tightening 100 -> 73 shifts the RHS to [40, 18], proving infeasibility.
  CpModelProto initial_model;
  LinearConstraintProto* lin =
      initial_model.add_constraints()->mutable_linear();
  for (int i = 0; i < 9; ++i) {
    IntegerVariableProto* var = initial_model.add_variables();
    var->add_domain(0);
    var->add_domain(2);
    lin->add_vars(i);
    lin->add_coeffs(1);
  }
  IntegerVariableProto* b = initial_model.add_variables();
  b->add_domain(0);
  b->add_domain(1);
  lin->add_vars(9);
  lin->add_coeffs(100);
  lin->add_domain(40);
  lin->add_domain(45);

  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  presolver.PresolveOneConstraint(0);
  EXPECT_TRUE(context.ModelIsUnsat());
}

TEST(PresolveCpModelTest,
     TightenBigMInBoxedLinearConstraintSkipsFixedVariable) {
  // PropagateDomainsInLinear fixes var 3 to [1, 1] in context without removing
  // it from the proto yet; ExtractEnforcementLiteralFromLinearConstraint skips
  // fixed var 3 while tightening var 2 from 100 to 2, and PresolveSmallLinear
  // then canonicalizes fixed var 3 out of the constraint.
  const CpModelProto fixed_var_model = ParseTextProtoOrDie(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 2, 100, 500 ]
        domain: [ 502, 613 ]
      }
    }
  )pb");
  const CpModelProto presolved_fixed =
      PresolveOneConstraint(fixed_var_model, 0);
  EXPECT_THAT(presolved_fixed.variables(3).domain(), ElementsAre(1, 1));
  ASSERT_EQ(presolved_fixed.constraints_size(), 1);
  EXPECT_THAT(presolved_fixed.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 2, 2 ]
                domain: [ 2, 15 ]
              )pb"));
}

TEST(PresolveCpModelTest, TightenBigMInBoxedLinearConstraintRandomEquivalence) {
  absl::BitGen random;
  int num_tightened = 0;
  for (int iter = 0; iter < 200; ++iter) {
    CpModelProto initial_model;
    const int num_vars = 3;
    std::vector<Domain> orig_domains(num_vars);
    for (int i = 0; i < num_vars; ++i) {
      const int64_t lb = absl::Uniform<int64_t>(random, -4, 4);
      const int64_t ub = lb + absl::Uniform<int64_t>(random, 1, 5);
      Domain d(lb, ub);
      if (ub - lb >= 2 && absl::Bernoulli(random, 0.3)) {
        d = d.IntersectionWith(Domain(lb + 1, lb + 1).Complement());
      }
      orig_domains[i] = d;
      FillDomainInProto(d, initial_model.add_variables());
    }

    ConstraintProto* ct = initial_model.add_constraints();
    LinearConstraintProto* lin = ct->mutable_linear();
    std::vector<int64_t> coeffs(num_vars);
    int64_t min_act = 0;
    int64_t max_act = 0;
    for (int i = 0; i < num_vars; ++i) {
      int64_t c = (i == num_vars - 1) ? absl::Uniform<int64_t>(random, 20, 80)
                                      : absl::Uniform<int64_t>(random, 1, 6);
      if (absl::Bernoulli(random, 0.5)) c = -c;
      coeffs[i] = c;
      lin->add_vars(i);
      lin->add_coeffs(c);
      min_act += c > 0 ? c * orig_domains[i].Min() : c * orig_domains[i].Max();
      max_act += c > 0 ? c * orig_domains[i].Max() : c * orig_domains[i].Min();
    }
    if (max_act - min_act < 2) continue;
    const int64_t rhs_lb = absl::Uniform<int64_t>(random, min_act + 1, max_act);
    const int64_t rhs_ub =
        absl::Bernoulli(random, 0.2)
            ? rhs_lb
            : absl::Uniform<int64_t>(random, rhs_lb, max_act);
    lin->add_domain(rhs_lb);
    lin->add_domain(rhs_ub);

    CpModelProto presolved_model = initial_model;
    CpModelProto mapping_model;
    Model model;
    SatParameters* params = model.GetOrCreate<SatParameters>();
    params->set_keep_all_feasible_solutions_in_presolve(true);
    params->set_enumerate_all_solutions(true);
    params->set_presolve_extract_integer_enforcement(false);
    PresolveContext context(&model, &presolved_model, &mapping_model);
    CpConstraintPresolver presolver(&context);
    context.InitializeNewDomains();
    presolver.PresolveOneConstraint(0);
    presolver.RemoveEmptyConstraints();

    const bool is_unsat = context.ModelIsUnsat();
    if (!is_unsat && presolved_model.constraints_size() == 1 &&
        presolved_model.constraints(0).constraint_case() ==
            ConstraintProto::kLinear &&
        presolved_model.constraints(0).linear().coeffs_size() == num_vars &&
        std::abs(presolved_model.constraints(0).linear().coeffs(num_vars - 1)) <
            std::abs(coeffs[num_vars - 1])) {
      ++num_tightened;
    }

    const int total_vars = presolved_model.variables_size();
    auto eval_lit = [](int lit, const std::vector<int64_t>& vals) -> bool {
      const int var = PositiveRef(lit);
      return RefIsPositive(lit) ? (vals[var] != 0) : (vals[var] == 0);
    };
    auto eval_ct = [&](const ConstraintProto& c_proto,
                       const std::vector<int64_t>& vals) -> bool {
      for (const int lit : c_proto.enforcement_literal()) {
        if (!eval_lit(lit, vals)) return true;
      }
      switch (c_proto.constraint_case()) {
        case ConstraintProto::CONSTRAINT_NOT_SET:
          return true;
        case ConstraintProto::kBoolOr: {
          for (const int lit : c_proto.bool_or().literals()) {
            if (eval_lit(lit, vals)) return true;
          }
          return false;
        }
        case ConstraintProto::kBoolAnd: {
          for (const int lit : c_proto.bool_and().literals()) {
            if (!eval_lit(lit, vals)) return false;
          }
          return true;
        }
        case ConstraintProto::kLinear: {
          int64_t sum = 0;
          for (int k = 0; k < c_proto.linear().vars_size(); ++k) {
            const int var = c_proto.linear().vars(k);
            CHECK_GE(var, 0);
            sum += c_proto.linear().coeffs(k) * vals[var];
          }
          return ReadDomainFromProto(c_proto.linear()).Contains(sum);
        }
        default:
          ADD_FAILURE() << "Unexpected constraint case: "
                        << c_proto.constraint_case();
          return false;
      }
    };
    auto check_full_assignment = [&](const std::vector<int64_t>& vals) -> bool {
      for (int j = 0; j < total_vars; ++j) {
        if (!context.DomainOf(j).Contains(vals[j])) return false;
        const AffineRelation::Relation r = context.GetAffineRelation(j);
        if (r.representative != j &&
            vals[j] != r.coeff * vals[r.representative] + r.offset) {
          return false;
        }
      }
      for (const ConstraintProto& c_proto : presolved_model.constraints()) {
        if (!eval_ct(c_proto, vals)) return false;
      }
      for (const ConstraintProto& c_proto : mapping_model.constraints()) {
        if (!eval_ct(c_proto, vals)) return false;
      }
      return true;
    };
    auto eval_presolved = [&](std::vector<int64_t> vals) -> bool {
      if (is_unsat) return false;
      for (int i = 0; i < num_vars; ++i) {
        if (!context.DomainOf(i).Contains(vals[i])) return false;
      }
      vals.resize(total_vars, 0);
      const auto extend = [&](auto& self, int var_idx) -> bool {
        if (var_idx == total_vars) return check_full_assignment(vals);
        for (const int64_t v : context.DomainOf(var_idx).Values()) {
          vals[var_idx] = v;
          if (self(self, var_idx + 1)) return true;
        }
        return false;
      };
      return extend(extend, num_vars);
    };

    const Domain orig_rhs(rhs_lb, rhs_ub);
    for (const int64_t x0 : orig_domains[0].Values()) {
      for (const int64_t x1 : orig_domains[1].Values()) {
        for (const int64_t x2 : orig_domains[2].Values()) {
          const int64_t act = coeffs[0] * x0 + coeffs[1] * x1 + coeffs[2] * x2;
          const bool orig_feasible = orig_rhs.Contains(act);
          const bool presolved_feasible = eval_presolved({x0, x1, x2});
          ASSERT_EQ(orig_feasible, presolved_feasible)
              << "x=(" << x0 << "," << x1 << "," << x2 << ") initial:\n"
              << initial_model << "\npresolved:\n"
              << presolved_model;
        }
      }
    }
  }
  EXPECT_GT(num_tightened, 20);
}

TEST(PresolveCpModelTest,
     CumulativeConvertsTwoValueDemandToOptionalAndNoOverlap) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 10, 20 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 10, 20 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 10 }
        end { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 10 }
        end { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 3 }
        intervals: [ 0, 1 ]
        demands { vars: 4 coeffs: 1 }
        demands { vars: 5 coeffs: 1 }
      }
    }
  )pb");

  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  presolver.PresolveOneConstraint(/*c=*/2);
  presolver.RemoveEmptyConstraints();

  bool has_no_overlap = false;
  bool has_cumulative = false;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kNoOverlap) {
      has_no_overlap = true;
    }
    if (ct.constraint_case() == ConstraintProto::kCumulative) {
      has_cumulative = true;
    }
  }
  EXPECT_TRUE(has_no_overlap);
  EXPECT_FALSE(has_cumulative);
}

TEST(PresolveCpModelTest,
     PropagateDomainsInLinearKeepsMultiEnforcedNonBooleansUnsplit) {
  // Minimum activity of 2*b0 - 3*b1 + 4*y0 - 5*y1 with b_j in [0, 1] and y_j in
  // [0, 3] is -3 - 15 = -18. Because there is more than one enforcement literal
  // and not all variables are Booleans, the constraint is never split: all
  // terms stay in a single normalized +-1 linear equality and reach a fixpoint
  // on the next pass.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3, 4, 5 ]
        coeffs: [ 2, -3, 4, -5 ]
        domain: [ -100, -18 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3, 4, 5 ]
        coeffs: [ 1, -1, 1, -1 ]
        domain: [ -4, -4 ]
      }
    }
  )pb");

  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  presolver.PresolveOneConstraint(0);
  EXPECT_GT(context.num_presolve_operations, 0);
  const int64_t ops_after_first_pass = context.num_presolve_operations;
  EXPECT_FALSE(presolver.PresolveOneConstraint(0));
  EXPECT_EQ(context.num_presolve_operations, ops_after_first_pass);
  presolver.RemoveEmptyConstraints();
  EXPECT_THAT(presolved_model, EqualsProto(expected_model));
}

TEST(PresolveCpModelTest,
     PropagateDomainsInLinearCombinesMultiEnforcedBooleansIntoBoolAnd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3, 4 ]
        coeffs: [ 2, -3, 4 ]
        domain: [ -10, -3 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      bool_and { literals: [ -3, 3, -5 ] }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_model));
}

TEST(PresolveCpModelTest,
     PresolveEnforcedLinear2EqCstCombinesBooleansIntoSingleBoolAnd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 2, 3 ]
        domain: [ 5, 5 ]
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
      bool_and { literals: [ 2, 3 ] }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_model));
}

TEST(PresolveCpModelTest,
     PresolveEnforcedLinear2EqCstKeepsMultiEnforcedNonBooleansUnsplit) {
  // 2*x + 5*y == 10 over x in [0, 3], y in [0, 2] has the unique solution
  // (x, y) = (0, 2), which lies at domain extrema, so it normalizes in place to
  // x - y == -2 without splitting across [0, 1].
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 2, 5 ]
        domain: [ 10, 10 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ -2, -2 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveOneConstraint(initial_model, 0),
              EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, LowerThanCoeffStrengtheningPartialAmoGcd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 2, 2, 4, 4, 6 ]
        domain: [ 0, 7 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3, 4 ]
                coeffs: [ 1, 1, 2, 2, 3 ]
                domain: [ 0, 3 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     LowerThanCoeffStrengtheningPartialAmoGcdUpdatesCanonical) {
  // Enforced constraint: (x3 && x4) => 2*x0 + 6*x1 + 11*x2 <= 9
  // with x0 in [0, 1], x1 in [0, 2], x2 in [0, 1].
  // 1. LowerThanCoeffStrengthening ("linear with partial amo: gcd" with gcd=2)
  //    rewrites the constraint to x0 + 3*x1 + 5*x2 <= 4 and updates
  //    `canonical` in place.
  // 2. TryToReduceCoefficientsOfLinearConstraint then uses the updated
  //    `canonical` to decouple the forced big-M term 5*x2 into x2 == 0 and
  //    x0 + 3*x1 <= 4.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3, 4 ]
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 6, 11 ]
        domain: [ 0, 9 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 2);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 2 ]
                coeffs: [ 1 ]
                domain: [ 0, 0 ]
              )pb"));
  EXPECT_THAT(presolved.constraints(1).linear(), EqualsProto(R"pb(
                vars: [ 0, 1 ]
                coeffs: [ 1, 3 ]
                domain: [ 0, 4 ]
              )pb"));
}

TEST(PresolveCpModelTest, LowerThanCoeffStrengtheningFixesVariableToBound) {
  // PropagateDomainsInLinear narrows x9 from {0, 3, 4} to [3, 4] (not fixed,
  // so the constraint is not recanonicalized), raising min_activity from 0 to
  // 33 and reducing the effective upper-bound slack rhs = 44 - 33 = 11.
  // LowerThanCoeffStrengthening then sees coeff(x8) = 12 > rhs = 11 on an
  // unenforced constraint and fixes x8 to its lower bound 0.
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
    variables { domain: [ 0, 0, 3, 4 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        coeffs: [ 2, 2, 2, 2, 2, 2, 2, 3, 12, 11 ]
        domain: [ 32, 44 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved.variables(8).domain(), ElementsAre(0, 0));
  EXPECT_THAT(presolved.variables(9).domain(), ElementsAre(3, 4));
}

TEST(PresolveCpModelTest, LowerThanCoeffStrengtheningPhaseBUpperAndLowerBound) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 2, 3 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5 ]
        coeffs: [ 2, 3, 3, 3, 9, 10 ]
        domain: [ -100, 30 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5 ]
        coeffs: [ 2, 3, 3, 3, -9, 10 ]
        domain: [ -5, 100 ]
      }
    }
  )pb");

  const CpModelProto presolved_ub = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved_ub.constraints_size(), 2);
  EXPECT_THAT(presolved_ub.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3, 4, 5 ]
                coeffs: [ 2, 3, 3, 3, 10, 10 ]
                domain: [ 22, 32 ]
              )pb"));

  const CpModelProto presolved_lb = PresolveOneConstraint(initial_model, 1);
  ASSERT_EQ(presolved_lb.constraints_size(), 2);
  EXPECT_THAT(presolved_lb.constraints(1).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3, 4, 5 ]
                coeffs: [ 2, 3, 3, 3, -10, 10 ]
                domain: [ -7, 3 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     LowerThanCoeffStrengtheningSetAllToOneDecouplesForcedBigM) {
  // Enforced constraint: (x4 && x5) => 4*x0 + 5*x1 + 5*x2 + 20*x3 <= 10
  // with all variables in [0, 1].
  // 1. LowerThanCoeffStrengthening ("linear with partial amo: constant coeff"
  //    with 10/4 == 10/5 == 2) sets `set_all_to_one`, rewrites the constraint
  //    to x0 + x1 + x2 + 3*x3 <= 2, and updates `canonical` in place.
  // 2. TryToReduceCoefficientsOfLinearConstraint then uses the updated
  //    `canonical` to decouple the forced big-M term 3*x3 into x3 == 0 and
  //    x0 + x1 + x2 <= 2.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 4, 5 ]
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 4, 5, 5, 20 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 2);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 3 ]
                coeffs: [ 1 ]
                domain: [ 0, 0 ]
              )pb"));
  EXPECT_THAT(presolved.constraints(1).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 1, 1 ]
                domain: [ 0, 2 ]
              )pb"));
}

TEST(PresolveCpModelTest, TwoTermTiltingRunsBeforeApproximateGcd) {
  // Enforced constraint: (x3 && x4) => 7*x0 + 7*x1 + 19*x2 <= 26 with
  // x0, x1 in [0, 1] and x2 in [0, 2].
  // LowerThanCoeffStrengthening skips 2-magnitude constraints (< 3), and
  // DetectTwoTermTilting runs before TryToReduceCoefficientsOfLinearConstraint
  // so it yields the optimal x0 + x1 + 2*x2 <= 3 instead of approximate GCD's
  // (base = 7) x0 + x1 + 3*x2 <= 4.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3, 4 ]
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 7, 7, 19 ]
        domain: [ 0, 26 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2 ]
                coeffs: [ 1, 1, 2 ]
                domain: [ 0, 3 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     LowerThanCoeffStrengtheningFixesVarEnablingTwoTermTilting) {
  // With 10 variables, PropagateDomainsInLinear runs in fast mode and narrows
  // x9 from {0, 9, 10} to [9, 10] (not fixed, so the constraint is not
  // recanonicalized), raising min_activity from 0 to 45 and reducing the
  // effective upper-bound slack rhs = 53 - 45 = 8.
  // LowerThanCoeffStrengthening fixes x8 (coeff = 12 > 8) to 0 and updates
  // `canonical` in place from 3 magnitudes {3, 5, 12} down to 2 magnitudes
  // {3, 5}. DetectTwoTermTilting then immediately tilts
  // 3*(x0+...+x5) + 5*(x6+x7+(x9-9)) <= 8 to
  // 2*(x0+...+x5) + 3*(x6+x7+(x9-9)) <= 5.
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
    variables { domain: [ 0, 0, 9, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
        coeffs: [ 3, 3, 3, 3, 3, 3, 5, 5, 12, 5 ]
        domain: [ 41, 53 ]
      }
    }
  )pb");

  const CpModelProto presolved = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved.variables(8).domain(), ElementsAre(0, 0));
  EXPECT_THAT(presolved.variables(9).domain(), ElementsAre(9, 10));
  ASSERT_EQ(presolved.constraints_size(), 1);
  EXPECT_THAT(presolved.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
                coeffs: [ 2, 2, 2, 2, 2, 2, 3, 3, 3, 3 ]
                domain: [ 27, 32 ]
              )pb"));
}

TEST(PresolveCpModelTest,
     PresolveLinearOfSizeOnePreservesSimplifiedRhsOnGappedDomain) {
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0, 2, 2, 4, 4, 6, 6, 8, 8 ] }
    constraints {
      enforcement_literal: [ 0 ]
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 0, 0, 4, 8 ]
      }
    }
    constraints {
      enforcement_literal: [ -1 ]
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
  )pb");
  CpModelProto mapping_model;
  Model model;
  PresolveContext context(&model, &working_model, &mapping_model);
  CpConstraintPresolver presolver(&context);
  context.InitializeNewDomains();
  context.CanonicalizeConstraintsLazily();

  EXPECT_FALSE(presolver.PresolveOneConstraint(0));
  EXPECT_FALSE(presolver.PresolveOneConstraint(1));
  EXPECT_FALSE(context.NeedsCanonicalization());
  context.CanonicalizeConstraintsLazily();
  EXPECT_TRUE(context.AllConstraintsAreCanonical());
  EXPECT_THAT(working_model.constraints(0).linear().domain(),
              ElementsAre(0, 0, 4, 8));
  EXPECT_TRUE(context.HasVarValueEncoding(1, 2, nullptr));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
