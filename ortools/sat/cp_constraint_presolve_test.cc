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

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"

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

}  // namespace
}  // namespace sat
}  // namespace operations_research
