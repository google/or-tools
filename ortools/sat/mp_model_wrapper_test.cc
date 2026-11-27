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

#include "ortools/sat/mp_model_wrapper.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/util/full_precision_inequalities.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::EqualsProto;

MipModelProto ModelWithContractFields() {
  return ParseTestProto(R"pb(
    variables { lower_bound: 0 upper_bound: 1 is_integer: true }
    variables {
      lower_bound: -1.5
      upper_bound: 2.5
      objective_coefficient: 3
      maximum_distance_to_integer: 0.25
    }
    constraints {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
      lower_bound: 1
      tolerance: 1e-6
    }
    constraints {
      vars: [ 1 ]
      coeffs: [ 1 ]
      upper_bound: 2
      enforcement_vars: [ 0 ]
      enforcement_values: [ 1 ]
    }
    objective_offset: 4
    objective_scaling: -1
    objective_max_rounding_error: 1e-9
  )pb");
}

TEST(MPModelProtoWrapperTest, FromMipModelProtoWrapsAsIs) {
  const MipModelProto model = ModelWithContractFields();
  const MPModelProtoWrapper wrapper =
      MPModelProtoWrapper::FromMipModelProto(model);
  EXPECT_THAT(wrapper.proto(), EqualsProto(model));
  EXPECT_TRUE(wrapper.has_non_constant_objective());
  EXPECT_TRUE(wrapper.is_original_model_maximize());
  EXPECT_EQ(wrapper.objective_offset(), 4.0);
  EXPECT_EQ(wrapper.objective_max_rounding_error(), 1e-9);
  EXPECT_EQ(wrapper.variables(1).maximum_distance_to_integer(), 0.25);
  ASSERT_EQ(wrapper.num_constraints(), 2);
  EXPECT_EQ(wrapper.constraints(0).tolerance(), 1e-6);
  EXPECT_THAT(wrapper.constraints(0).vars(), ElementsAre(0, 1));
  EXPECT_EQ(wrapper.constraints(1).tolerance(), 0.0);
  EXPECT_THAT(wrapper.constraints(1).enforcement_vars(), ElementsAre(0));
}

TEST(MPModelProtoWrapperTest, FromMipModelProtoWithConstantObjective) {
  const MPModelProtoWrapper wrapper =
      MPModelProtoWrapper::FromMipModelProto(ParseTestProto(R"pb(
        variables { lower_bound: 0 upper_bound: 1 }
        objective_offset: 2
      )pb"));
  EXPECT_FALSE(wrapper.has_non_constant_objective());
  EXPECT_FALSE(wrapper.is_original_model_maximize());
  EXPECT_EQ(wrapper.num_constraints(), 0);
}

TEST(MPModelProtoWrapperTest, ConstructionFromMPModelProtoHasDefaultFields) {
  const MPModelProto mp_model = ParseTestProto(R"pb(
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    variable { lower_bound: 0 upper_bound: 1 }
    constraint {
      var_index: [ 0, 1 ]
      coefficient: [ 1, 1 ]
      upper_bound: 1
    }
  )pb");
  const MPModelProtoWrapper wrapper(mp_model);
  EXPECT_EQ(wrapper.objective_max_rounding_error(), 0.0);
  EXPECT_EQ(wrapper.variables(1).maximum_distance_to_integer(), 0.5);
  ASSERT_EQ(wrapper.num_constraints(), 1);
  EXPECT_EQ(wrapper.constraints(0).tolerance(), 0.0);
}

TEST(MPModelProtoWrapperTest, SettersWriteTheProto) {
  MPModelProtoWrapper wrapper =
      MPModelProtoWrapper::FromMipModelProto(ModelWithContractFields());
  wrapper.mutable_constraints(1)->set_tolerance(0.5);
  wrapper.set_objective_offset(5.0);
  wrapper.set_objective_max_rounding_error(0.25);
  EXPECT_EQ(wrapper.proto().constraints(1).tolerance(), 0.5);
  EXPECT_EQ(wrapper.proto().objective_offset(), 5.0);
  EXPECT_EQ(wrapper.proto().objective_max_rounding_error(), 0.25);
}

TEST(MPModelProtoWrapperTest, CopyAndMoveRebuildTheConstraintViews) {
  MPModelProtoWrapper wrapper =
      MPModelProtoWrapper::FromMipModelProto(ModelWithContractFields());

  MPModelProtoWrapper copy(wrapper);
  copy.mutable_constraints(0)->set_tolerance(0.5);
  EXPECT_EQ(copy.proto().constraints(0).tolerance(), 0.5);
  EXPECT_EQ(wrapper.constraints(0).tolerance(), 1e-6);

  MPModelProtoWrapper assigned =
      MPModelProtoWrapper::FromMipModelProto(MipModelProto());
  assigned = copy;
  assigned.mutable_constraints(0)->set_tolerance(0.25);
  EXPECT_EQ(assigned.proto().constraints(0).tolerance(), 0.25);
  EXPECT_EQ(copy.constraints(0).tolerance(), 0.5);

  MPModelProtoWrapper moved(std::move(copy));
  moved.mutable_constraints(0)->set_tolerance(0.125);
  EXPECT_EQ(moved.proto().constraints(0).tolerance(), 0.125);

  assigned = std::move(moved);
  assigned.mutable_constraints(1)->set_tolerance(2.0);
  EXPECT_EQ(assigned.proto().constraints(0).tolerance(), 0.125);
  EXPECT_EQ(assigned.proto().constraints(1).tolerance(), 2.0);
  EXPECT_EQ(assigned.num_constraints(), 2);
}

TEST(MPModelProtoWrapperTest,
     RelaxAndSetToleranceBudgetApplyUniformlyToAllConstraints) {
  const MPModelProto mp_model = ParseTestProto(R"pb(
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    variable { lower_bound: 0 upper_bound: 10 }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true }
    constraint {
      var_index: [ 0, 2 ]
      coefficient: [ 1, 1 ]
      lower_bound: 1
      upper_bound: 2
    }
    constraint {
      var_index: [ 0 ]
      coefficient: [ 1 ]
      lower_bound: -inf
      upper_bound: inf
    }
    general_constraint {
      indicator_constraint {
        var_index: 0
        var_value: 1
        constraint {
          var_index: [ 2 ]
          coefficient: [ 1 ]
          lower_bound: 3
          upper_bound: inf
        }
      }
    }
    general_constraint {
      and_constraint {
        resultant_var_index: 1
        var_index: [ 0, 3 ]
      }
    }
    general_constraint {
      or_constraint {
        resultant_var_index: 1
        var_index: [ 0, 3 ]
      }
    }
  )pb");
  MPModelProtoWrapper wrapper(mp_model);
  wrapper.RelaxAllConstraintBounds(/*tight_precision=*/0.25);
  wrapper.SetAllConstraintToleranceBudget(/*tolerance=*/0.5);
  // All values are exact powers of two, so the split is exact. Both methods
  // apply uniformly to all constraints (including rows from And/Or general
  // constraints) while leaving variable bounds untouched.
  EXPECT_THAT(wrapper.proto(), EqualsProto(R"pb(
                variables { lower_bound: 0 upper_bound: 1 is_integer: true }
                variables { lower_bound: 0 upper_bound: 1 is_integer: true }
                variables { lower_bound: 0 upper_bound: 10 }
                variables { lower_bound: 0 upper_bound: 1 is_integer: true }
                constraints {
                  vars: [ 0, 2 ]
                  coeffs: [ 1, 1 ]
                  lower_bound: 0.75
                  upper_bound: 2.25
                  tolerance: 0.5
                }
                constraints {
                  vars: [ 2 ]
                  coeffs: [ 1 ]
                  lower_bound: 2.75
                  upper_bound: inf
                  tolerance: 0.5
                  enforcement_vars: [ 0 ]
                  enforcement_values: [ 1 ]
                }
                constraints {
                  vars: [ 0, 3 ]
                  coeffs: [ 1, 1 ]
                  lower_bound: 1.75
                  upper_bound: inf
                  tolerance: 0.5
                  enforcement_vars: [ 1 ]
                  enforcement_values: [ 1 ]
                }
                constraints {
                  vars: [ 0, 3 ]
                  coeffs: [ 1, 1 ]
                  lower_bound: -inf
                  upper_bound: 1.25
                  tolerance: 0.5
                  enforcement_vars: [ 1 ]
                  enforcement_values: [ 0 ]
                }
                constraints {
                  vars: [ 0, 3 ]
                  coeffs: [ 1, 1 ]
                  lower_bound: 0.75
                  upper_bound: inf
                  tolerance: 0.5
                  enforcement_vars: [ 1 ]
                  enforcement_values: [ 1 ]
                }
                constraints {
                  vars: [ 0, 3 ]
                  coeffs: [ 1, 1 ]
                  lower_bound: -inf
                  upper_bound: 0.25
                  tolerance: 0.5
                  enforcement_vars: [ 1 ]
                  enforcement_values: [ 0 ]
                }
                objective_scaling: 1
              )pb"));
  EXPECT_EQ(wrapper.objective_max_rounding_error(), 0.0);
  for (int v = 0; v < wrapper.num_variables(); ++v) {
    EXPECT_FALSE(wrapper.variables(v).has_maximum_distance_to_integer());
    EXPECT_EQ(wrapper.variables(v).maximum_distance_to_integer(), 0.5);
  }

  // Setting a zero tolerance budget clears the field on every constraint.
  wrapper.SetAllConstraintToleranceBudget(0.0);
  for (int c = 0; c < wrapper.num_constraints(); ++c) {
    EXPECT_FALSE(wrapper.constraints(c).has_tolerance());
    EXPECT_EQ(wrapper.constraints(c).tolerance(), 0.0);
  }
}

// Checks the per-constraint tolerance split with RelaxAllConstraintBounds()
// and SetAllConstraintToleranceBudget(): finite bounds round inward from
// `bound -/+ tight`, the tolerance is `ExactSumLb(wanted, -tight)`, F_in
// contains the original constraint, and F_out stays within the original
// constraint relaxed by `wanted`.
TEST(MPModelProtoWrapperTest,
     RelaxAndToleranceBudgetRoundInwardAndPreserveContract) {
  constexpr double kDoubleMax = std::numeric_limits<double>::max();
  for (const double bound :
       {2.0000005, 0.1, -3.0, 1e20, 123456.789, kDoubleMax, -kDoubleMax}) {
    for (const auto [tight, wanted] :
         std::vector<std::pair<double, double>>{{1e-9, 1e-6},
                                                {1e-6, 1e-6},
                                                {0.0, 1e-6},
                                                {0.0, 0.0},
                                                {0.3, 0.7},
                                                {1e-300, 1e-6}}) {
      MPModelProto model;
      model.add_variable()->set_upper_bound(10);
      MPConstraintProto* ct = model.add_constraint();
      ct->add_var_index(0);
      ct->add_coefficient(1.0);
      ct->set_lower_bound(bound);
      ct->set_upper_bound(bound);
      MPModelProtoWrapper wrapper(std::move(model), /*logger=*/nullptr);
      const double expected_tol = ExactSumLb(wanted, -tight);
      wrapper.RelaxAllConstraintBounds(tight);
      wrapper.SetAllConstraintToleranceBudget(expected_tol);
      ASSERT_EQ(wrapper.num_constraints(), 1);
      const EnforcedLinearConstraintProto& relaxed = wrapper.constraints(0);
      const double tol = relaxed.tolerance();
      EXPECT_EQ(tol, expected_tol) << bound << " " << tight << " " << wanted;
      EXPECT_GE(tol, 0.0);
      EXPECT_FALSE(std::signbit(tol));
      // Bounds stay finite even at +/-DBL_MAX and round inward.
      EXPECT_TRUE(std::isfinite(relaxed.lower_bound()));
      EXPECT_TRUE(std::isfinite(relaxed.upper_bound()));
      EXPECT_EQ(relaxed.lower_bound(),
                TightSum().Add(bound).Add(-tight).TightUpperBound())
          << bound << " " << tight << " " << wanted;
      EXPECT_EQ(relaxed.upper_bound(),
                TightSum().Add(bound).Add(tight).TightLowerBound())
          << bound << " " << tight << " " << wanted;
      // Inner region contains the original constraint and never relaxes beyond
      // `tight`.
      EXPECT_LE(relaxed.lower_bound(), bound);
      EXPECT_GE(relaxed.upper_bound(), bound);
      EXPECT_TRUE(
          TightSum().Add(relaxed.lower_bound()).Add(-bound).Add(tight) >= 0.0)
          << bound << " " << tight << " " << wanted;
      EXPECT_TRUE(
          TightSum().Add(relaxed.upper_bound()).Add(-bound).Add(-tight) <= 0.0)
          << bound << " " << tight << " " << wanted;
      // Outer region stays within the original constraint relaxed by `wanted`.
      EXPECT_TRUE(TightSum()
                      .Add(relaxed.lower_bound())
                      .Add(-tol)
                      .Add(-bound)
                      .Add(wanted) >= 0.0)
          << bound << " " << tight << " " << wanted;
      EXPECT_TRUE(TightSum()
                      .Add(relaxed.upper_bound())
                      .Add(tol)
                      .Add(-bound)
                      .Add(-wanted) <= 0.0)
          << bound << " " << tight << " " << wanted;
    }
  }
}

TEST(MPModelProtoWrapperTest,
     RelaxAllConstraintBoundsExtremeDoubleMaxStaysFinite) {
  constexpr double kDoubleMax = std::numeric_limits<double>::max();
  MPModelProto model;
  model.add_variable()->set_upper_bound(10);
  MPConstraintProto* ct = model.add_constraint();
  ct->add_var_index(0);
  ct->add_coefficient(1.0);
  ct->set_lower_bound(-kDoubleMax);
  ct->set_upper_bound(kDoubleMax);

  MPModelProtoWrapper wrapper(std::move(model), /*logger=*/nullptr);
  wrapper.RelaxAllConstraintBounds(/*tight_precision=*/1e308);
  wrapper.SetAllConstraintToleranceBudget(ExactSumLb(1.5e308, -1e308));
  ASSERT_EQ(wrapper.num_constraints(), 1);
  const EnforcedLinearConstraintProto& relaxed = wrapper.constraints(0);
  EXPECT_EQ(relaxed.lower_bound(), -kDoubleMax);
  EXPECT_EQ(relaxed.upper_bound(), kDoubleMax);
  EXPECT_EQ(relaxed.tolerance(), ExactSumLb(1.5e308, -1e308));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
