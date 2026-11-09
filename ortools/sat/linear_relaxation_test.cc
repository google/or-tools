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

#include "ortools/sat/linear_relaxation.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(AppendRelaxationForEqualityEncodingTest, DomainOfSize2) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({4, 8})));
  encoder->FullyEncodeVariable(var);

  // Initially we don't have a view, so this should return false.
  LinearRelaxation relaxation;
  int num_tight = 0;
  int num_loose = 0;
  AppendRelaxationForEqualityEncoding(var, model, &relaxation, &num_tight,
                                      &num_loose);
  EXPECT_EQ(num_tight, 0);
  EXPECT_EQ(num_loose, 0);

  // Make sure all literals have a view.
  for (const auto literal_value : encoder->FullDomainEncoding(var)) {
    model.Add(NewIntegerVariableFromLiteral(literal_value.literal));
  }
  AppendRelaxationForEqualityEncoding(var, model, &relaxation, &num_tight,
                                      &num_loose);
  EXPECT_EQ(num_tight, 1);

  // In this case, because there are just two values, we should get a literal
  // and its negation, so just one constraint (the first one is empty).
  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].num_terms, 0);

  // The variable (0) is equal to 8 - 4 * [var == 4].
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "8 <= 1*I0 4*I1 <= 8");
}

// Convert the at_most_one to a linear constraint and call DebugString().
std::string AtMostOneAsString(absl::Span<const Literal> at_most_one,
                              Model* model) {
  LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(1));
  for (const Literal literal : at_most_one) {
    const bool unused [[maybe_unused]] =
        lc.AddLiteralTerm(literal, IntegerValue(1));
  }
  return lc.Build().DebugString();
}

TEST(AppendRelaxationForEqualityEncodingTest, DomainOfSize4) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({1, 5, 8, 9})));
  encoder->FullyEncodeVariable(var);

  // Make sure all relevant literals have a view.
  for (const auto literal_value : encoder->FullDomainEncoding(var)) {
    model.Add(NewIntegerVariableFromLiteral(literal_value.literal));
  }

  LinearRelaxation relaxation;
  int num_tight = 0;
  int num_loose = 0;
  AppendRelaxationForEqualityEncoding(var, model, &relaxation, &num_tight,
                                      &num_loose);

  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1 <= 1*I1 1*I2 1*I3 1*I4");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "1 <= 1*I0 -4*I2 -7*I3 -8*I4 <= 1");

  EXPECT_EQ(relaxation.at_most_ones.size(), 1);
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[0], &model),
            "1*I1 1*I2 1*I3 1*I4 <= 1");
}

TEST(AppendRelaxationForEqualityEncodingTest, PartialEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  for (const int value : {1, 5}) {
    encoder->AssociateToIntegerEqualValue(
        Literal(model.Add(NewBooleanVariable()), true), var,
        IntegerValue(value));
  }

  // Make sure all relevant literals have a view.
  for (const auto literal_value : encoder->PartialDomainEncoding(var)) {
    model.Add(NewIntegerVariableFromLiteral(literal_value.literal));
  }

  // The encoded values should be 0, 1 and 5, so the min/max not encoded should
  // be 2 and 10.
  LinearRelaxation relaxation;
  int num_tight = 0;
  int num_loose = 0;
  AppendRelaxationForEqualityEncoding(var, model, &relaxation, &num_tight,
                                      &num_loose);
  EXPECT_EQ(num_tight, 0);
  EXPECT_EQ(num_loose, 2);

  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "2 <= 1*I0 2*I1 1*I2 -3*I3");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "1*I0 10*I1 9*I2 5*I3 <= 10");

  EXPECT_EQ(relaxation.at_most_ones.size(), 1);
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[0], &model),
            "1*I1 1*I2 1*I3 <= 1");
}

TEST(AppendPartialGreaterThanEncodingRelaxationTest, FullEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({1, 5, 8, 9})));
  encoder->FullyEncodeVariable(var);

  // Make sure all >= literals have a view.
  for (const auto value_literal : encoder->PartialGreaterThanEncoding(var)) {
    model.Add(NewIntegerVariableFromLiteral(value_literal.literal));
  }

  LinearRelaxation relaxation;
  AppendPartialGreaterThanEncodingRelaxation(var, model, &relaxation);

  // The implications.
  EXPECT_EQ(relaxation.at_most_ones.size(), 2);
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[0], &model),
            "-1*I1 1*I2 <= 0");
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[1], &model),
            "-1*I2 1*I3 <= 0");

  // The "diffs" are 4,3,1.
  // Because here we have a full encoding, we actually have == 1.
  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1 <= 1*I0 -4*I1 -3*I2 -1*I3");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "-1 <= -1*I0 4*I1 3*I2 1*I3");
}

TEST(AppendPartialGreaterThanEncodingRelaxationTest, PartialEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));

  // Create a literal for  var >= 1, var >= 2 and var >= 6
  for (const int value : {1, 2, 6}) {
    encoder->AssociateToIntegerLiteral(
        Literal(model.Add(NewBooleanVariable()), true),
        IntegerLiteral::GreaterOrEqual(var, IntegerValue(value)));
  }

  // Make sure all >= literals have a view.
  for (const auto value_literal : encoder->PartialGreaterThanEncoding(var)) {
    model.Add(NewIntegerVariableFromLiteral(value_literal.literal));
  }

  LinearRelaxation relaxation;
  AppendPartialGreaterThanEncodingRelaxation(var, model, &relaxation);

  // The implications.
  EXPECT_EQ(relaxation.at_most_ones.size(), 2);
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[0], &model),
            "-1*I1 1*I2 <= 0");
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[1], &model),
            "-1*I2 1*I3 <= 0");

  // The first constraint is var >= 0 + (>=1) + (>=2) + 4*(>=6)
  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "0 <= 1*I0 -1*I1 -1*I2 -4*I3");

  // The second is var <= (>=1) + 4*(>=2) + 5*(>=6) which gives the bounds
  // <=0,<=1,<=5 and <=10.
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "0 <= -1*I0 1*I1 4*I2 5*I3");
}

TEST(TryToLinearizeConstraint, BoolOr) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 0
      bool_or { literals: [ -2, 2 ] }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "-1 <= -1*I0 -1*I1 1*I2");
}

TEST(TryToLinearizeConstraint, BoolOrLevel1) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 0
      bool_or { literals: [ -2, 2 ] }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
  EXPECT_EQ(relaxation.at_most_ones.size(), 0);
}

TEST(TryToLinearizeConstraint, BoolAndSingleEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ -2, 2 ] }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  EXPECT_EQ(relaxation.at_most_ones.size(), 2);
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[0], &model),
            "1*I0 1*I1 <= 1");
  EXPECT_EQ(AtMostOneAsString(relaxation.at_most_ones[1], &model),
            "1*I0 -1*I2 <= 0");
}

TEST(TryToLinearizeConstraint, BoolAndMultipleEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 3 ]
      bool_and { literals: [ -2, 2 ] }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  // I0 & I3 => I2 == 1 & not(I1) == 1:
  // I1 + (1 - I2) <= 2 * ((1 - I0) + (1 - I3)).
  ASSERT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "2*I0 1*I1 -1*I2 2*I3 <= 3");
}

TEST(TryToLinearizeConstraint, BoolAndNoEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_and { literals: [ -2, 2 ] } }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
  EXPECT_EQ(relaxation.at_most_ones.size(), 0);
}

TEST(TryToLinearizeConstraint, BoolAndLevel1) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 3 ]
      bool_and { literals: [ -2, 2 ] }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
  EXPECT_EQ(relaxation.at_most_ones.size(), 0);
}

TEST(TryToLinearizeConstraint, LinMaxLevel1Bis) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -1, 7 ] }
    variables { domain: [ -2, 9 ] }
    variables { domain: [ -5, 10 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 2 coeffs: -1 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "1*I0 -1*I3 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "1*I1 -1*I3 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(), "-1*I2 -1*I3 <= 0");
}

TEST(TryToLinearizeConstraint, EnforcedLinMaxLevel2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -1, 7 ] }
    variables { domain: [ -2, 9 ] }
    variables { domain: [ -5, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 4
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 2 coeffs: -1 }
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1*I0 -1*I3 10*I4 <= 10");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "1*I1 -1*I3 12*I4 <= 12");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "-1*I2 -1*I3 7*I4 <= 7");
}

TEST(TryToLinearizeConstraint, LinMaxSmall) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -1, 7 ] }
    variables { domain: [ -5, 10 ] }
    constraints {
      lin_max {
        target: { vars: 2 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  // Take into account the constraints added by the cut generator.
  ASSERT_EQ(relaxation.linear_constraints.size(), 5);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "1*I0 -1*I2 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "1*I1 -1*I2 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "1 <= 1*I3 1*I4 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[3].DebugString(),
            "-1*I0 1*I2 7*I3 <= 7");
  EXPECT_EQ(relaxation.linear_constraints[4].DebugString(),
            "-1*I1 1*I2 -6*I3 <= 0");
  EXPECT_EQ(relaxation.cut_generators.size(), 1);
}

TEST(TryToLinearizeConstraint, LinMaxNegativeAndOffsetTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      lin_max {
        target: { vars: 2 coeffs: -1 offset: 3 }
        exprs: { vars: 0 coeffs: -1 offset: 1 }
        exprs: { vars: 1 coeffs: -1 offset: 2 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/2, &model, &relaxation);

  ASSERT_EQ(relaxation.linear_constraints.size(), 5);
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "1 <= 1*I3 1*I4 <= 1");
  EXPECT_EQ(relaxation.cut_generators.size(), 1);
}

TEST(TryToLinearizeConstraint, IntSquare) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 1, 100 ] }
    constraints {
      int_prod {
        target: { vars: 1 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "-11*I0 1*I1 <= -10");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "-2 <= -3*I0 1*I1");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "-90 <= -19*I0 1*I1");
}

TEST(TryToLinearizeConstraint, IntAbs) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ -20, 30 ] }
    constraints {
      lin_max {
        target: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 1 coeffs: -1 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "-1*I0 1*I1 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "-1*I0 -1*I1 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "50*I0 -10*I1 <= 1200");
}

TEST(TryToLinearizeConstraint, LinMaxLevel1) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -1, 7 ] }
    variables { domain: [ -2, 9 ] }
    variables { domain: [ -5, 10 ] }
    constraints {
      lin_max {
        target: {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 3
        }
        exprs: {
          vars: [ 1 ]
          coeffs: [ 2 ]
          offset: 1
        }
        exprs: {
          vars: [ 2 ]
          coeffs: [ -1 ]
          offset: 2
        }
        exprs: {
          vars: [ 3 ]
          coeffs: [ 3 ]
          offset: 3
        }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  TryToLinearizeConstraint(initial_model, initial_model.constraints(0),
                           /*linearization_level=*/1, &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "-1*I0 2*I1 <= 2");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "-1*I0 -1*I2 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(), "-1*I0 3*I3 <= 0");
}

TEST(AppendLinMaxRelaxation, BasicBehavior) {
  Model model;
  IntegerVariable x0 = model.Add(NewIntegerVariable(0, 5));
  IntegerVariable x1 = model.Add(NewIntegerVariable(-1, 7));
  IntegerVariable x2 = model.Add(NewIntegerVariable(-2, 9));
  IntegerVariable target = model.Add(NewIntegerVariable(-5, 10));
  LinearExpression e0;
  e0.vars = {x0};
  e0.coeffs = {IntegerValue(1)};
  LinearExpression e1;
  e1.vars = {x1};
  e1.coeffs = {IntegerValue(1)};
  LinearExpression e2;
  e2.vars = {x2};
  e2.coeffs = {IntegerValue(-1)};

  const std::vector<LinearExpression> exprs = {e0, e1, e2};

  LinearRelaxation relaxation;
  const std::vector<Literal> literals =
      CreateAlternativeLiteralsWithView(exprs.size(), &model, &relaxation);
  AppendLinMaxRelaxationPart2(target, literals, exprs, &model, &relaxation);

  EXPECT_EQ(literals.size(), 3);
  ASSERT_EQ(relaxation.linear_constraints.size(), 4);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1 <= 1*I4 1*I5 1*I6 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "-1*I0 1*I3 -7*I5 -2*I6 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "-1*I1 1*I3 -6*I4 -3*I6 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[3].DebugString(),
            "1*I2 1*I3 -14*I4 -16*I5 <= 0");
}

TEST(AppendLinMaxRelaxation, BasicBehaviorExprs) {
  Model model;
  IntegerVariable x0 = model.Add(NewIntegerVariable(-1, 1));
  IntegerVariable x1 = model.Add(NewIntegerVariable(-1, 1));
  IntegerVariable target = model.Add(NewIntegerVariable(-100, 100));
  LinearExpression e0;
  e0.offset = IntegerValue(1);
  LinearExpression e1;
  e1.vars = {x0, x1};
  e1.coeffs = {IntegerValue(-1), IntegerValue(-2)};
  LinearExpression e2;
  e2.vars = {x0, x1};
  e2.coeffs = {IntegerValue(-1), IntegerValue(1)};

  const std::vector<LinearExpression> exprs = {e0, e1, e2};

  LinearRelaxation relaxation;
  const std::vector<Literal> literals =
      CreateAlternativeLiteralsWithView(exprs.size(), &model, &relaxation);
  AppendLinMaxRelaxationPart2(target, literals, exprs, &model, &relaxation);

  EXPECT_EQ(literals.size(), 3);
  ASSERT_EQ(relaxation.linear_constraints.size(), 4);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1 <= 1*I3 1*I4 1*I5 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "1*I2 -1*I3 -3*I4 -2*I5 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "1*I0 2*I1 1*I2 -4*I3 -3*I5 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[3].DebugString(),
            "1*I0 -1*I1 1*I2 -3*I3 -3*I4 <= 0");
}

TEST(AppendLinMaxRelaxation, BasicBehaviorExprs2) {
  Model model;
  IntegerVariable x0 = model.Add(NewIntegerVariable(1, 2));
  IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  IntegerVariable x2 = model.Add(NewIntegerVariable(-2, -1));
  IntegerVariable target = model.Add(NewIntegerVariable(-3, 0));
  LinearExpression e0;
  e0.vars = {x0, x1};
  e0.coeffs = {IntegerValue(-2), IntegerValue(-3)};
  e0.offset = IntegerValue(5);
  LinearExpression e1;
  e1.vars = {x1, x2};
  e1.coeffs = {IntegerValue(-2), IntegerValue(-5)};
  e1.offset = IntegerValue(-6);
  LinearExpression e2;
  e2.vars = {x0, x2};
  e2.coeffs = {IntegerValue(-2), IntegerValue(-3)};

  const std::vector<LinearExpression> exprs = {e0, e1, e2};

  LinearRelaxation relaxation;
  const std::vector<Literal> literals =
      CreateAlternativeLiteralsWithView(exprs.size(), &model, &relaxation);
  AppendLinMaxRelaxationPart2(NegationOf(target), literals, exprs, &model,
                              &relaxation);

  EXPECT_EQ(literals.size(), 3);
  ASSERT_EQ(relaxation.linear_constraints.size(), 4);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1 <= 1*I4 1*I5 1*I6 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "2*I0 3*I1 -1*I3 -5*I4 -9*I5 -9*I6 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(),
            "2*I1 5*I2 -1*I3 2*I4 6*I5 2*I6 <= 0");
  EXPECT_EQ(relaxation.linear_constraints[3].DebugString(),
            "2*I0 3*I2 -1*I3 -2*I4 -2*I5 <= 0");
}

void AppendNoOverlapRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.no_overlap().intervals());
  const IntegerValue one(1);
  std::vector<AffineExpression> demands(intervals.size(), one);
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repository->GetOrCreateHelper(/*enforcement_literals=*/{}, intervals);
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, model);
  model->TakeOwnership(demands_helper);

  AddCumulativeRelaxation(/*capacity=*/one, helper, demands_helper,
                          /*makespan=*/std::nullopt, model, relaxation);
}

TEST(AppendNoOverlapRelaxation, IntersectingIntervals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    constraints { no_overlap { intervals: [ 1, 2 ] } }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendNoOverlapRelaxation(initial_model.constraints(0), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "1*I1 1*I4 <= 12");
}

TEST(AppendNoOverlapRelaxation, NoIntersection) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 2, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendNoOverlapRelaxation(initial_model.constraints(2), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "1*I4 <= 11");
}

TEST(AppendNoOverlapRelaxation, IntervalWithEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 2, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 2, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 6
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendNoOverlapRelaxation(initial_model.constraints(2), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "1*I1 1*I6 <= 10");
}

TEST(AppendNoOverlapRelaxation, ZeroMinEnergy) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 5 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 5 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 7
      interval {
        start { vars: 4 coeffs: 1 }
        size { vars: 5 coeffs: 1 }
        end { vars: 6 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendNoOverlapRelaxation(initial_model.constraints(2), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
}

TEST(AppendNoOverlapRelaxation, OneInterval) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 2 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: 0 } }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendNoOverlapRelaxation(initial_model.constraints(1), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
}

void AppendCumulativeRelaxation(const ConstraintProto& ct, Model* model,
                                LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const std::vector<AffineExpression> demands =
      mapping->Affines(ct.cumulative().demands());
  const AffineExpression capacity = mapping->Affine(ct.cumulative().capacity());
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repository->GetOrCreateHelper(/*enforcement_literals=*/{}, intervals);
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, model);
  model->TakeOwnership(demands_helper);

  AddCumulativeRelaxation(capacity, helper, demands_helper,
                          /*makespan=*/std::nullopt, model, relaxation);
}

TEST(AppendCumulativeRelaxation, GcdOnFixedDemandsSizesAndCapacity) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 1, 4 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 4 }
        end { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 4 }
        end { vars: 1 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 4
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ]
        demands { offset: 3 }
        demands { offset: 6 }
        demands { offset: 3 }
        capacity { offset: 7 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendCumulativeRelaxation(initial_model.constraints(3), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "4*I3 1*I4 <= 6");
}

TEST(AppendCumulativeRelaxation, IgnoreZeroDemandOrSize) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 1, 4 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 4 }
        end { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 4 }
        end { vars: 1 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 4
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      enforcement_literal: 5
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 0 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 6
      interval {
        start { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 3 }
        end { vars: 2 coeffs: 1 offset: 8 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2, 3, 4 ]
        demands { offset: 3 }
        demands { offset: 6 }
        demands { offset: 3 }
        demands { offset: 3 }
        demands { offset: 0 }
        capacity { offset: 7 }
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendCumulativeRelaxation(initial_model.constraints(5), &model, &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(), "4*I3 1*I4 <= 6");
}

TEST(AppendLinearConstraintRelaxation, NoEnforcementLiteral) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 3, 4 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "3 <= 2*I0 1*I2 <= 4");
}

TEST(AppendLinearConstraintRelaxation, SmallLinearizationLevel) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 3, 5 ]
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);
  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
}

TEST(AppendLinearConstraintRelaxation, PbConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 1, 3 ]
        domain: [ 3, 5 ]
      }
    }
  )pb");

  Model model;
  LoadVariables(initial_model, false, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);
  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "3 <= 2*I0 1*I1 3*I2 <= 5");
}

TEST(AppendLinearConstraintRelaxation, SmallConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: 0
        coeffs: 2
        domain: [ 3, 5 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
}

TEST(AppendLinearConstraintRelaxation, SingleEnforcementLiteralLowerBound) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 3, 9223372036854775807 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "0 <= 2*I0 -3*I1 1*I2");
}

TEST(AppendLinearConstraintRelaxation, SingleEnforcementLiteralUpperBound) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ -9223372036854775808, 3 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 1);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "2*I0 1*I1 1*I2 <= 4");
}

TEST(AppendLinearConstraintRelaxation, SingleEnforcementLiteralBothBounds) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 2, 3 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "0 <= 2*I0 -2*I1 1*I2");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "2*I0 1*I1 1*I2 <= 4");
}

TEST(AppendLinearConstraintRelaxation, MultipleEnforcementLiteral) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 1, 3, 4 ]
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 2, 3 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "-4 <= 2*I0 -2*I1 1*I2 -2*I3 -2*I4");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "2*I0 1*I1 1*I2 1*I3 1*I4 <= 6");
}

// This used to generate the completely wrong constraint:
// 1*I0 -8*I1 1*I2 -8*I3 <= -6 before.
TEST(AppendLinearConstraintRelaxation, BoundsNotTight) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 1
      enforcement_literal: 3
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 10 ]  # 10 > implied ub of 2.
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(initial_model, true, &model);

  LinearRelaxation relaxation;
  AppendLinearConstraintRelaxation(initial_model.constraints(0), &model,
                                   &relaxation);

  EXPECT_EQ(relaxation.linear_constraints.size(), 0);
}

TEST(ComputeLinearRelaxationTest, InfersShiftedNoOverlapCutGenerators) {
  // Four tasks with release-date shifts r = {0, 12, 25, 40} and processing
  // times p = {10, 15, 14, 12}, modeled with shifted non-negative start
  // variables y_i = t_i - r_i in [0, 100]. Five of the six pairs allow
  // y_i - y_j == 0 (interior exclusion holes) while pair (1, 2) excludes 0.
  const int64_t r[4] = {0, 12, 25, 40};
  const int64_t p[4] = {10, 15, 14, 12};

  CpModelProto cp_model;
  for (int i = 0; i < 4; ++i) {
    IntegerVariableProto* var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(100);
  }
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const int lit = cp_model.variables_size();
      IntegerVariableProto* b = cp_model.add_variables();
      b->add_domain(0);
      b->add_domain(1);

      // lit => y_i - y_j <= r_j - r_i - p_i.
      ConstraintProto* ct1 = cp_model.add_constraints();
      ct1->add_enforcement_literal(lit);
      ct1->mutable_linear()->add_vars(i);
      ct1->mutable_linear()->add_vars(j);
      ct1->mutable_linear()->add_coeffs(1);
      ct1->mutable_linear()->add_coeffs(-1);
      ct1->mutable_linear()->add_domain(-100);
      ct1->mutable_linear()->add_domain(r[j] - r[i] - p[i]);

      // ~lit => y_i - y_j >= r_j - r_i + p_j.
      ConstraintProto* ct2 = cp_model.add_constraints();
      ct2->add_enforcement_literal(-lit - 1);
      ct2->mutable_linear()->add_vars(i);
      ct2->mutable_linear()->add_vars(j);
      ct2->mutable_linear()->add_coeffs(1);
      ct2->mutable_linear()->add_coeffs(-1);
      ct2->mutable_linear()->add_domain(r[j] - r[i] + p[j]);
      ct2->mutable_linear()->add_domain(100);
    }
  }

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  EXPECT_EQ(relaxation.cut_generators.size(), 2);

  auto* repository = model.GetOrCreate<IntervalsRepository>();
  ASSERT_EQ(repository->NumIntervals(), 4);
  for (int i = 0; i < 4; ++i) {
    const IntervalVariable iv(i);
    EXPECT_EQ(repository->Start(iv).constant, IntegerValue(r[i]));
    EXPECT_EQ(repository->Size(iv).constant, IntegerValue(p[i]));
    EXPECT_EQ(repository->End(iv).constant, IntegerValue(r[i] + p[i]));
  }
}

TEST(ComputeLinearRelaxationTest, InfersNoOverlapFromIncompatibleLinears) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 91, 905 ] }
    variables { domain: [ 638, 937 ] }
    variables { domain: [ 0, 69 ] }
    variables { domain: [ 575, 930 ] }
    constraints {
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ -863, -506 ]
      }
    }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 569, 909 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, -1 ]
        domain: [ -846, -32 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 3 ]
        coeffs: [ -1, 1 ]
        domain: [ -868, -22 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ -839, -300 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 1, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ 310, 330 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 2, 4 ]
        coeffs: [ -1, 1 ]
        domain: [ 275, 292 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2, 4 ]
        coeffs: [ -1, 1 ]
        domain: [ -362, -123 ]
      }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  EXPECT_EQ(relaxation.cut_generators.size(), 2);
  auto* repository = model.GetOrCreate<IntervalsRepository>();
  EXPECT_EQ(repository->NumIntervals(), 4);
}

TEST(ComputeLinearRelaxationTest, InfersShiftedAllDiffCutGenerator) {
  // Four variables y_i in [0, 6] with pairwise (y_i + r_i) != (y_j + r_j)
  // for shifts r = {0, 2, 4, 5}. Every pair allows y_i - y_j == 0 and has its
  // forbidden difference strictly inside [-6, 6].
  const int64_t r[4] = {0, 2, 4, 5};
  CpModelProto cp_model;
  for (int i = 0; i < 4; ++i) {
    IntegerVariableProto* var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(6);
  }
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const int64_t forbidden = r[j] - r[i];
      ConstraintProto* ct = cp_model.add_constraints();
      ct->mutable_linear()->add_vars(i);
      ct->mutable_linear()->add_vars(j);
      ct->mutable_linear()->add_coeffs(1);
      ct->mutable_linear()->add_coeffs(-1);
      ct->mutable_linear()->add_domain(-6);
      ct->mutable_linear()->add_domain(forbidden - 1);
      ct->mutable_linear()->add_domain(forbidden + 1);
      ct->mutable_linear()->add_domain(6);
    }
  }

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  EXPECT_EQ(relaxation.cut_generators.size(), 1);

  // Cyclic exclusions x_0 - x_1 != 1, x_1 - x_2 != 1, x_2 - x_0 != 1 do not
  // correspond to any shifted all_diff; ComputeLinearRelaxation must not infer
  // a cut generator.
  CpModelProto cycle_model;
  for (int i = 0; i < 3; ++i) {
    IntegerVariableProto* var = cycle_model.add_variables();
    var->add_domain(0);
    var->add_domain(3);
  }
  for (int i = 0; i < 3; ++i) {
    ConstraintProto* ct = cycle_model.add_constraints();
    ct->mutable_linear()->add_vars(i);
    ct->mutable_linear()->add_vars((i + 1) % 3);
    ct->mutable_linear()->add_coeffs(1);
    ct->mutable_linear()->add_coeffs(-1);
    ct->mutable_linear()->add_domain(-3);
    ct->mutable_linear()->add_domain(0);
    ct->mutable_linear()->add_domain(2);
    ct->mutable_linear()->add_domain(3);
  }
  Model cycle_local_model;
  cycle_local_model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cycle_model, true, &cycle_local_model);
  const LinearRelaxation cycle_relaxation =
      ComputeLinearRelaxation(cycle_model, &cycle_local_model);
  EXPECT_TRUE(cycle_relaxation.cut_generators.empty());
}

TEST(ComputeLinearRelaxationTest,
     MultiEnforcedBoolAndSingleConstraintWithSingleAmo) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 3, 4, 5 ] } }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_and { literals: [ -4, -5, -6 ] }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);
  for (const ConstraintProto& ct : cp_model.constraints()) {
    LoadConstraint(ct, &model);
  }

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  ASSERT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1*I0 1*I1 1*I2 1*I3 1*I4 1*I5 <= 3");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(),
            "1*I3 1*I4 1*I5 <= 1");
}

TEST(ComputeLinearRelaxationTest,
     MultiEnforcedBoolAndSingleConstraintAcrossMultipleAmos) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 3, 4 ] } }
    constraints { at_most_one { literals: [ 5, 6 ] } }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_and { literals: [ -4, -5, -6, -7 ] }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);
  for (const ConstraintProto& ct : cp_model.constraints()) {
    LoadConstraint(ct, &model);
  }

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  ASSERT_EQ(relaxation.linear_constraints.size(), 3);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "2*I0 2*I1 2*I2 1*I3 1*I4 1*I5 1*I6 <= 6");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "1*I3 1*I4 <= 1");
  EXPECT_EQ(relaxation.linear_constraints[2].DebugString(), "1*I5 1*I6 <= 1");
}

TEST(ComputeLinearRelaxationTest, MultiEnforcedBoolAndDeduplicatesLiterals) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 2, 3 ] } }
    constraints {
      enforcement_literal: [ 0, 1, 0 ]
      bool_and { literals: [ -3, -3 ] }
    }
  )pb");

  Model model;
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  LoadVariables(cp_model, true, &model);
  for (const ConstraintProto& ct : cp_model.constraints()) {
    LoadConstraint(ct, &model);
  }

  const LinearRelaxation relaxation = ComputeLinearRelaxation(cp_model, &model);
  ASSERT_EQ(relaxation.linear_constraints.size(), 2);
  EXPECT_EQ(relaxation.linear_constraints[0].DebugString(),
            "1*I0 1*I1 1*I2 <= 2");
  EXPECT_EQ(relaxation.linear_constraints[1].DebugString(), "1*I2 1*I3 <= 1");
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
