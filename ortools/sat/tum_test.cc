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

#include "ortools/sat/tum.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/types.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;

// Runs DetectAndConvertTotallyUnimodularComponent() on all residual continuous
// components of a model and converts the remaining variables and constraints to
// a CpModelProto.
class ComponentPipeline {
 public:
  using Component = ResidualContinuousComponent;

  ComponentPipeline(const SatParameters& params, MPModelProto mp_model)
      : params_(params),
        original_model_(mp_model),
        wrapper_(std::move(mp_model)),
        states_(InitialVariableStates(wrapper_)),
        components_(SplitIntoResidualContinuousComponents(wrapper_)),
        var_to_local_col_(wrapper_.num_variables(), -1) {
    // Process the components in a deterministic order.
    std::sort(components_.components.begin(), components_.components.end(),
              [](const Component& a, const Component& b) {
                return a.continuous_vars.front() < b.continuous_vars.front();
              });
    for (int v = 0; v < wrapper_.num_variables(); ++v) {
      cp_model_.add_variables();
    }
    scaler_.wanted_precision = params_.mip_wanted_precision();
    scaler_.use_proper_relaxation = params_.mip_proper_relaxation();
    scaler_.tight_precision = params_.mip_tight_precision();
    scaler_.scaling_target = int64_t{1} << params_.mip_max_activity_exponent();
  }

  std::vector<double> RunTum() {
    const int num_vars = wrapper_.num_variables();
    std::vector<double> scaling(num_vars);
    for (int v = 0; v < num_vars; ++v) scaling[v] = states_[v].scaling;
    is_converted_.assign(components_.components.size(), false);
    for (int idx = 0; idx < components_.components.size(); ++idx) {
      Component& c = components_.components[idx];
      const ScopedVarToLocalCol scoped(&c, &var_to_local_col_);
      is_converted_[idx] = DetectAndConvertTotallyUnimodularComponent(
          params_, c, wrapper_, absl::MakeSpan(states_), &scaler_, &cp_model_,
          &tum_stats_);
    }
    for (int v = 0; v < num_vars; ++v) {
      scaling[v] = states_[v].scaling / scaling[v];
    }
    return scaling;
  }

  const std::vector<MipVariableState>& states() const { return states_; }
  const TumStats& tum_stats() const { return tum_stats_; }

  // Returns the input model with the variable states applied.
  MPModelProto Model() const {
    MPModelProto result = original_model_;
    ApplyVariableStatesToModel(states_, &result);
    return result;
  }

  // Returns the value of each CP-SAT variable given its value before the shift
  // (see MipVariableState).
  std::vector<int64_t> Shifted(std::vector<int64_t> values) const {
    for (int v = 0; v < values.size(); ++v) {
      values[v] -= static_cast<int64_t>(states_[v].shift);
    }
    return values;
  }

  // Converts the variables and the constraints that were not already added by
  // RunTum(), and copies the resulting model into `cp_model`. This can only be
  // called once.
  bool ConvertToCpModel(CpModelProto* cp_model,
                        ConstraintScaler* out_scaler = nullptr) {
    const int num_vars = wrapper_.num_variables();
    int num_small_domains = 0;
    for (int v = 0; v < num_vars; ++v) {
      if (!ConvertVariableToCpVariable(
              params_, v, wrapper_.variables(v).objective_coefficient(),
              &states_[v], cp_model_.mutable_variables(v),
              &num_small_domains)) {
        return false;
      }
    }
    const auto add = [&](int c) {
      return scaler_
          .ScaleAndAddConstraint(wrapper_.constraints(c), states_, &cp_model_)
          .ok();
    };
    for (const int c : components_.pure_integer_constraints) {
      if (!add(c)) return false;
    }
    for (int idx = 0; idx < components_.components.size(); ++idx) {
      if (idx < is_converted_.size() && is_converted_[idx]) continue;
      for (const int c : components_.components[idx].constraints) {
        if (!add(c)) return false;
      }
    }
    if (!PopulateCpModelFloatingPointObjective(wrapper_, states_, &cp_model_)) {
      return false;
    }
    *cp_model = cp_model_;
    if (out_scaler != nullptr) *out_scaler = scaler_;
    return true;
  }

 private:
  const SatParameters params_;
  const MPModelProto original_model_;
  const MPModelProtoWrapper wrapper_;
  std::vector<MipVariableState> states_;
  ResidualContinuousComponents components_;
  std::vector<bool> is_converted_;
  std::vector<int> var_to_local_col_;
  CpModelProto cp_model_;
  ConstraintScaler scaler_;
  TumStats tum_stats_;
};

// Runs DetectAndConvertTotallyUnimodularComponent() on all the residual
// components of `mp_model`, updates it in place, and returns the scaling of
// each variable.
std::vector<double> DetectAndConvertAllTotallyUnimodularComponents(
    const SatParameters& params, MPModelProto* mp_model) {
  ComponentPipeline pipeline(params, *mp_model);
  std::vector<double> scaling = pipeline.RunTum();
  *mp_model = pipeline.Model();
  return scaling;
}

TEST(PartitionGraphTest, ValidAndInvalidComponents) {
  // Component 1 (nodes 0, 1, 2): 0 == 1, 1 != 2, 0 != 2 (valid: 0, 0, 1).
  // Component 2 (nodes 3, 4, 5, 6): triangle 3 != 4, 4 != 5, 5 != 3, plus
  // tail 5 == 6 (invalid: all nodes 3, 4, 5, 6 must be marked -1 even if
  // conflict is detected before visiting node 6).
  // Component 3 (node 7): isolated node (valid: 0).
  const std::vector<EdgeConstraintForPartition> edges = {
      {0, 1, /*same_partition=*/true},  {1, 2, /*same_partition=*/false},
      {0, 2, /*same_partition=*/false}, {3, 4, /*same_partition=*/false},
      {4, 5, /*same_partition=*/false}, {5, 3, /*same_partition=*/false},
      {5, 6, /*same_partition=*/true},
  };
  EXPECT_THAT(PartitionGraph(8, edges),
              ElementsAre(0, 0, 1, -1, -1, -1, -1, 0));
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     NetworkMatrixIsTotallyUnimodular) {
  const std::string text_lp =
      "x1 - x2 <= 1;"
      "-x1 + x2 - x3 <= 2;"
      "x3 + 2 y <= 5;"
      "int y";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  DetectAndConvertAllTotallyUnimodularComponents(SatParameters(), &mp_model);

  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_TRUE(mp_model.variable(2).is_integer());
  EXPECT_TRUE(mp_model.variable(3).is_integer());
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     BipartiteEvenCycleAndSingletonRow) {
  // Even cycle of +1 coefficients (bipartite row partition {0, 2} vs {1, 3}),
  // plus a 5th row where x1 is the only continuous variable (singleton row).
  const std::string text_lp =
      "x1 + x2 + 3 y <= 4;"
      "x2 + x3 <= 2;"
      "x3 + x4 <= 3;"
      "x4 + x1 <= 5;"
      "x1 - 2 y <= 1;"
      "int y";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  DetectAndConvertAllTotallyUnimodularComponents(SatParameters(), &mp_model);

  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_TRUE(var.is_integer()) << var.name();
  }
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     MultipleComponentsIsolateBadSubmatrix) {
  // Component 1 (x1, x2, x3): valid TU component (with almost-integer bound on
  // x1 and integer variable y).
  // Component 2 (u1, u2): non-unit continuous coefficient (2 u1 + u2 <= 3).
  // Component 3 (v1, v2, v3, v4): 2-core where v1 has column degree 3 and all
  // 3 rows have row degree 3 (fails both column-wise and row-wise checks).
  // Component 4 (w1, w2): w1 has fractional upper bound 1.5.
  // Component 5 (z1): unconstrained continuous variable with fractional bound.
  const std::string text_lp =
      "min: z1 - w1;"
      "0 <= x1 <= 1.0000000001;"
      "x1 + x2 + 2 y <= 3;"
      "x2 + x3 <= 2;"
      "2 u1 + u2 <= 3;"
      "u1 + u2 <= 2;"
      "v1 + v2 + v3 <= 1;"
      "v1 + v2 + v4 <= 1;"
      "v1 + v3 + v4 <= 1;"
      "0 <= w1 <= 1.5;"
      "w1 + w2 <= 1;"
      "0.2 <= z1 <= 0.8;"
      "int y";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SatParameters params;
  params.set_mip_automatically_scale_variables(false);
  DetectAndConvertAllTotallyUnimodularComponents(params, &mp_model);
  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_EQ(var.is_integer(), var.name() == "y") << var.name();
  }

  // With automatic scaling enabled (default), components 1 (x1, x2, x3),
  // 4 (w1, w2), and 5 (z1) are made integer (scaled by 1, 2, and 5).
  params.set_mip_automatically_scale_variables(true);
  const std::vector<double> scaling =
      DetectAndConvertAllTotallyUnimodularComponents(params, &mp_model);
  for (int i = 0; i < mp_model.variable_size(); ++i) {
    const MPVariableProto& var = mp_model.variable(i);
    if (var.name() == "x1" || var.name() == "x2" || var.name() == "x3" ||
        var.name() == "y") {
      EXPECT_TRUE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 1.0) << var.name();
    } else if (var.name() == "w1" || var.name() == "w2") {
      EXPECT_TRUE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 2.0) << var.name();
    } else if (var.name() == "z1") {
      EXPECT_TRUE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 5.0) << var.name();
    } else {
      EXPECT_FALSE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 1.0) << var.name();
    }
  }
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     RowWiseAndSingletonPeeling) {
  // Component 1 (v1..v5): K_{2,3} where v1, v5 have column degree 3, v2..v4
  // have column degree 2, and every row has 2 entries (bipartite -> row-wise
  // TUM, plus singleton slack s1 on the first row).
  // Component 2 (u1..u4): every column has degree >= 2 (u1, u2 have degree 3),
  // every row has 2 entries, and (u1, u2, u3) is an odd cycle (row-wise
  // 2-modular -> scaled by 2).
  const std::string text_lp =
      "0 <= v1 <= 10;"
      "0 <= v2 <= 10;"
      "0 <= v3 <= 10;"
      "0 <= v4 <= 10;"
      "0 <= v5 <= 10;"
      "0 <= s1 <= 10;"
      "v1 + v2 + s1 <= 1;"
      "v2 + v5 <= 1;"
      "v1 + v3 <= 1;"
      "v3 + v5 <= 1;"
      "v1 + v4 <= 1;"
      "v4 + v5 <= 1;"
      "0 <= u1 <= 10;"
      "0 <= u2 <= 10;"
      "0 <= u3 <= 10;"
      "0 <= u4 <= 10;"
      "u1 + u2 <= 1;"
      "u1 + u3 <= 1;"
      "u2 + u3 <= 1;"
      "u1 + u4 <= 1;"
      "u2 + u4 <= 1;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  const std::vector<double> scaling =
      DetectAndConvertAllTotallyUnimodularComponents(SatParameters(),
                                                     &mp_model);
  EXPECT_THAT(scaling,
              ElementsAre(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.0));
  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_TRUE(var.is_integer()) << var.name();
  }
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     ScalesComponentWithRationalFactor) {
  const std::string text_lp =
      "max: x1 + x2;"
      "0 <= x1 <= 10.5;"
      "0 <= x2 <= 2.25;"
      "x1 - x2 <= 0.5;"
      "x1 >= 0.25;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  const std::vector<double> scaling =
      DetectAndConvertAllTotallyUnimodularComponents(SatParameters(),
                                                     &mp_model);
  EXPECT_THAT(scaling, ElementsAre(4.0, 4.0));
  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  // The domain [1, 42] of x1 is shifted to start at zero.
  EXPECT_DOUBLE_EQ(mp_model.variable(0).lower_bound(), 0.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 41.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 9.0);
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     RowAndColumnScaledComponent) {
  // Component with col_scale = [1, 2, 5] and row_scale = [3, 2]:
  //   Row 0:  3 x1 - 6 x2        <= 9   (divided by 3: x1 - 2 x2 <= 3)
  //   Row 1: -2 x1 + 4 x2 - 10 x3 <= 3  (divided by 2: -x1 + 2 x2 - 5 x3
  //   <= 1.5)
  // Because Row 1's pre-scaled RHS is 1.5, FindRationalFactor finds s = 2,
  // giving final variable scaling s * col_scale = [2, 4, 10].
  const std::string text_lp =
      "max: x1 + x2 + x3;"
      "0 <= x1 <= 10;"
      "0 <= x2 <= 5;"
      "0 <= x3 <= 2;"
      "3 x1 - 6 x2 <= 9;"
      "-2 x1 + 4 x2 - 10 x3 <= 3;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  const std::vector<double> scaling =
      DetectAndConvertAllTotallyUnimodularComponents(SatParameters(),
                                                     &mp_model);
  EXPECT_THAT(scaling, ElementsAre(2.0, 4.0, 10.0));
  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_TRUE(mp_model.variable(2).is_integer());
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 20.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 20.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(2).upper_bound(), 20.0);
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     BestEffortPowerOfTwoModularScalingWhenDomainCapped) {
  // Irrational bounds `sqrt(3) - 1`, `sqrt(2) - 1`, and fractional RHS `.3`
  // with `ub = 3e5` have no common rational factor within `1e11 / 3e5`, and
  // `2^20` (needed for 1e-6 outward bound precision) exceeds the 1e11 TUM
  // domain cap (`3e5 * 2^19 > 1e11`). With `mip_proper_relaxation`, the best
  // effort fallback integerizes the TUM component with the largest power of two
  // within the cap (`2^18 = 262144`), preserving unit coefficients `[1, 1]` and
  // zero discretization error.
  const std::string text_lp =
      "max: x1 + x2;"
      "0.7320508075688772 <= x1 <= 100000;"
      "0.4142135623730951 <= x2 <= 300000;"
      "x1 + x2 <= 350000.3;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SatParameters params;
  params.set_mip_proper_relaxation(true);
  params.set_mip_wanted_precision(1e-6);

  ComponentPipeline pipeline(params, mp_model);
  const double expected = std::ldexp(1.0, 18);
  EXPECT_THAT(pipeline.RunTum(), ElementsAre(expected, expected));
  EXPECT_EQ(pipeline.tum_stats().num_relaxed_modular_components, 1);

  CpModelProto cp_model;
  ConstraintScaler scaler;
  ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
  EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(scaler.max_discretization_error, 0.0);
  EXPECT_EQ(ValidateCpModel(cp_model), "");
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     ExactRationalScalingGoesBeyondMipMaxBoundUpToTumDomainCap) {
  // Exact decimal RHS `29000.00002` with domain `[0, 1.45e6]`: the largest
  // power of two within the domain cap (`2^16 = 65536`) cannot meet
  // `mip_wanted_precision = 1e-6`, whereas the exact rational multiplier
  // `50000` (`50000 * 1.45e6 = 7.25e10 > mip_max_bound = 1e7`) integerizes the
  // component exactly within the 1e11 TUM domain cap.
  const std::string text_lp =
      "max: x1 + x2;"
      "0 <= x1 <= 1450000;"
      "0 <= x2 <= 1450000;"
      "x1 + x2 <= 29000.00002;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SatParameters params;
  params.set_mip_wanted_precision(1e-6);

  ComponentPipeline pipeline(params, mp_model);
  EXPECT_THAT(pipeline.RunTum(), ElementsAre(50000.0, 50000.0));
  EXPECT_EQ(pipeline.tum_stats().num_new_tum_integers, 2);
  EXPECT_EQ(pipeline.tum_stats().num_relaxed_modular_components, 0);
  EXPECT_DOUBLE_EQ(pipeline.states()[0].ub, 7.25e10);
  EXPECT_DOUBLE_EQ(pipeline.states()[1].ub, 7.25e10);

  CpModelProto cp_model;
  ConstraintScaler scaler;
  ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
  EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(cp_model.constraints(0).linear().domain(1), 1450000001);
  EXPECT_EQ(scaler.max_discretization_error, 0.0);
  EXPECT_EQ(ValidateCpModel(cp_model), "");
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     OutwardPowerOfTwoScalingAndExternalVariables) {
  // Non-integral TUM component (no common rational factor within `1e11 / 3e4`
  // for `sqrt(3) - 1`, `sqrt(2) - 1`, and `.3`): TUM makes the component
  // integer with the smallest power-of-two scaling (`2^20 = 1048576`) whose
  // outward variable and row bound errors stay within `mip_wanted_precision`.
  const std::string text_lp =
      "max: x1 + x2 + x3;"
      "0.7320508075688772 <= x1 <= 10000;"
      "0.4142135623730951 <= x2 <= 30000;"
      "0 <= x3 <= 10000;"
      "x1 + x2 <= 35000;"
      "x2 + x3 <= 20000.3;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SatParameters params;
  params.set_mip_wanted_precision(1e-6);

  ComponentPipeline pipeline(params, mp_model);
  const double s = std::ldexp(1.0, 20);
  EXPECT_THAT(pipeline.RunTum(), ElementsAre(s, s, s));
  EXPECT_EQ(pipeline.tum_stats().num_relaxed_modular_components, 1);
  EXPECT_EQ(pipeline.tum_stats().num_relaxed_modular_integers, 3);
  const double shift0 = std::floor(0.7320508075688772 * s);
  const double shift1 = std::floor(0.4142135623730951 * s);
  EXPECT_DOUBLE_EQ(pipeline.states()[0].shift, shift0);
  EXPECT_DOUBLE_EQ(pipeline.states()[1].shift, shift1);
  EXPECT_DOUBLE_EQ(pipeline.states()[0].lb, 0.0);
  EXPECT_DOUBLE_EQ(pipeline.states()[1].lb, 0.0);
  EXPECT_DOUBLE_EQ(pipeline.states()[1].ub, 30000.0 * s - shift1);

  CpModelProto cp_model;
  ConstraintScaler scaler;
  ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
  EXPECT_EQ(ValidateCpModel(cp_model), "");
  EXPECT_EQ(scaler.max_discretization_error, 0.0);
  EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(cp_model.constraints(0).linear().domain(1),
            static_cast<int64_t>(35000.0 * s - shift0 - shift1));
  EXPECT_THAT(cp_model.constraints(1).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(cp_model.constraints(1).linear().domain(1),
            static_cast<int64_t>(std::ceil(20000.3 * s) - shift1));

  // Singleton row with a non-power-of-two rational `row_scale` (`300 x1 - y >=
  // 0`, odd factor `75`): `s = 75 * 2^14 = 1228800` integerizes the component
  // with exact external coefficient `-4096`. When the external coefficient is
  // irrational (`0.4142135623730951` with `y <= 1e5`), TUM rejects modular
  // scaling rather than degrading the external coefficient.
  {
    glop::LinearProgram row_scale_lp;
    CHECK(
        ParseLp("max: x1 + x2;"
                "0 <= x1 <= 10000;"
                "0.4142135623730951 <= x2 <= 30000;"
                "0 <= y <= 1000;"
                "x1 + x2 <= 35000;"
                "300 x1 - y >= 0;"
                "int y;",
                &row_scale_lp));
    MPModelProto row_scale_model;
    LinearProgramToMPModelProto(row_scale_lp, &row_scale_model);
    ComponentPipeline row_scale_pipeline(params, row_scale_model);
    const double expected_s = 75.0 * std::ldexp(1.0, 14);
    EXPECT_THAT(row_scale_pipeline.RunTum(),
                ElementsAre(expected_s, expected_s, 1.0));
    CpModelProto row_scale_cp;
    ASSERT_TRUE(row_scale_pipeline.ConvertToCpModel(&row_scale_cp));
    EXPECT_EQ(ValidateCpModel(row_scale_cp), "");
    EXPECT_THAT(row_scale_cp.constraints(1).linear().coeffs(),
                ElementsAre(1, -4096));
  }
  {
    glop::LinearProgram irr_ext_lp;
    CHECK(
        ParseLp("max: x1 + x2;"
                "0 <= x1 <= 10000;"
                "0.4142135623730951 <= x2 <= 30000;"
                "0 <= y <= 100000;"
                "x1 + x2 <= 35000;"
                "300 x1 - 0.4142135623730951 y >= 0;"
                "int y;",
                &irr_ext_lp));
    MPModelProto irr_ext_model;
    LinearProgramToMPModelProto(irr_ext_lp, &irr_ext_model);
    ComponentPipeline irr_ext_pipeline(params, irr_ext_model);
    EXPECT_THAT(irr_ext_pipeline.RunTum(), ElementsAre(1.0, 1.0, 1.0));
    EXPECT_FALSE(irr_ext_pipeline.states()[0].is_integer);
  }

  // Slightly rounded external coefficient (`0.3333333 * y`): the row bound is
  // relaxed by the coefficient rounding error before outward rounding (`s` ->
  // `s - 1 = 1048575`, before the shift of `x1` and `x2`).
  {
    glop::LinearProgram ext_lp;
    CHECK(
        ParseLp("max: x1 + x2;"
                "0.141592653589793 <= x1 <= 1;"
                "0.4142135623730951 <= x2 <= 0.7320508075688772;"
                "0 <= y <= 1;"
                "x1 + x2 + 0.3333333 y = 1;"
                "int y;",
                &ext_lp));
    MPModelProto ext_model;
    LinearProgramToMPModelProto(ext_lp, &ext_model);
    ComponentPipeline ext_pipeline(params, ext_model);
    EXPECT_THAT(ext_pipeline.RunTum(), ElementsAre(s, s, 1.0));
    CpModelProto ext_cp;
    ASSERT_TRUE(ext_pipeline.ConvertToCpModel(&ext_cp));
    const int64_t offset = static_cast<int64_t>(ext_pipeline.states()[0].shift +
                                                ext_pipeline.states()[1].shift);
    EXPECT_EQ(offset, 148470 + 434334);
    EXPECT_THAT(ext_cp.constraints(0).linear().domain(),
                ElementsAre(1048575 - offset, 1048576 - offset));
  }

  // Even when `s * mip_tight_precision > 0.5`, outward mode (`snap_within_tol
  // = false`) rounds variable bounds strictly outward rather than snapping
  // inward, keeping `x = (0.6666666, 0.6666666, 0.6666666, 0.5)` feasible.
  {
    glop::LinearProgram outward_lp;
    CHECK(
        ParseLp("max: x1 + x2 + x3 + x4;"
                "0.6666666 <= x1 <= 1;"
                "0.6666666 <= x2 <= 1;"
                "0.6666666 <= x3 <= 1;"
                "0.4142135623730951 <= x4 <= 0.8;"
                "0 <= y <= 100000;"
                "x1 + x2 + x3 <= 2;"
                "x4 - x1 + 0.00048828125 y <= 0;"
                "int y;",
                &outward_lp));
    MPModelProto outward_model;
    LinearProgramToMPModelProto(outward_lp, &outward_model);
    SatParameters outward_params = params;
    outward_params.set_mip_tight_precision(5e-7);
    ComponentPipeline outward_pipeline(outward_params, outward_model);
    EXPECT_THAT(outward_pipeline.RunTum(), ElementsAre(s, s, s, s, 1.0));
    CpModelProto outward_cp;
    ASSERT_TRUE(outward_pipeline.ConvertToCpModel(&outward_cp));
    EXPECT_TRUE(SolutionIsFeasible(
        outward_cp,
        outward_pipeline.Shifted({699050, 699050, 699050, 434335, 0})));
  }

  // Greedy `< 1` tightening budget on a chain of 4 equality rows at `s = 2^20`:
  // the first 3 rows stay exact (`[0, 0]`, total tightening `3 * 0.3146 < 1`)
  // and only the 4th row is relaxed by 1 scaled unit (`[-1, 1]`, shifted by
  // `x4`'s lower bound).
  {
    MPModelProto chain_model;
    for (int v = 0; v < 6; ++v) {
      MPVariableProto* var = chain_model.add_variable();
      var->set_lower_bound(v == 4 ? 0.25 + 1.1e-6 : 0.0);
      var->set_upper_bound(v == 0 ? 0.25 : 1.0);
      var->set_objective_coefficient(v == 0 ? 1.0 : 0.0);
    }
    for (int r = 0; r < 4; ++r) {
      MPConstraintProto* ct = chain_model.add_constraint();
      ct->set_lower_bound(0.0);
      ct->set_upper_bound(0.0);
      ct->add_var_index(r);
      ct->add_coefficient(1.0);
      ct->add_var_index(r + 1);
      ct->add_coefficient(-1.0);
    }
    MPConstraintProto* irr_ct = chain_model.add_constraint();
    irr_ct->set_lower_bound(0.4142135623730951);
    irr_ct->set_upper_bound(std::numeric_limits<double>::infinity());
    irr_ct->add_var_index(5);
    irr_ct->add_coefficient(1.0);
    irr_ct->add_var_index(0);
    irr_ct->add_coefficient(-1.0);

    SatParameters chain_params = params;
    chain_params.set_mip_tight_precision(3e-7);
    ComponentPipeline chain_pipeline(chain_params, std::move(chain_model));
    EXPECT_THAT(chain_pipeline.RunTum(), ElementsAre(s, s, s, s, s, s));
    CpModelProto chain_cp;
    ASSERT_TRUE(chain_pipeline.ConvertToCpModel(&chain_cp));
    for (int r = 0; r < 3; ++r) {
      EXPECT_THAT(chain_cp.constraints(r).linear().domain(), ElementsAre(0, 0));
    }
    const int64_t shift4 =
        static_cast<int64_t>(chain_pipeline.states()[4].shift);
    EXPECT_EQ(shift4, 262145);
    EXPECT_THAT(chain_cp.constraints(3).linear().domain(),
                ElementsAre(shift4 - 1, shift4 + 1));
    EXPECT_TRUE(SolutionIsFeasible(
        chain_cp, chain_pipeline.Shifted(
                      {262144, 262144, 262144, 262144, 262145, 1048576})));
    EXPECT_GE(chain_pipeline.tum_stats().objective_error_bound, 9e-7);
    EXPECT_LT(chain_pipeline.tum_stats().objective_error_bound, 1e-6);
  }
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     RationalAndTwoModularConstraintScaling) {
  SatParameters params;
  params.set_mip_proper_relaxation(true);

  // Approximate RHS TUM component (modeled on MIPLIB `ci-s4` c25881/c25961):
  //   x0 - x1 == -15497 / 35901, x0 - x1 <= 1.6099, x0, x1 in [0, 9].
  // After `TightenComponentBounds` tightens `x0, x1` to `[0, 1]`, Row 1 is
  // redundant and skipped, so `FindRationalFactor` picks `35901`, snapping
  // Row 0 to `[-15497, -15497]`. When an active fractional bound (`ub(x0) =
  // 1.6099`) pushes the common rational denominator (`1400000`) past `2^20 =
  // 1048576`, TUM prefers the smaller power of two `2^20` and relaxes Row 0
  // outward to `[-452628, -452627]`.
  {
    const MPModelProto cis4_model = ParseTestProto(R"pb(
      variable { lower_bound: 0.0 upper_bound: 9.0 is_integer: false }
      variable { lower_bound: 0.0 upper_bound: 9.0 is_integer: false }
      constraint {
        lower_bound: -0.4316592852566781
        upper_bound: -0.4316592852566781
        var_index: [ 0, 1 ]
        coefficient: [ 1.0, -1.0 ]
      }
      constraint {
        lower_bound: -inf
        upper_bound: 1.6099
        var_index: [ 0, 1 ]
        coefficient: [ 1.0, -1.0 ]
      }
    )pb");
    ComponentPipeline pipeline(params, cis4_model);
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(35901.0, 35901.0));
    CpModelProto cp_model;
    ConstraintScaler scaler;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, -1));
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(-15497, -15497));
    EXPECT_LE(scaler.max_unrounding_error, 1e-9);
  }
  {
    const MPModelProto active_cis4_model = ParseTestProto(R"pb(
      maximize: true
      variable {
        lower_bound: 0.0
        upper_bound: 1.6099
        objective_coefficient: 1.0
        is_integer: false
      }
      variable {
        lower_bound: 0.0
        upper_bound: 2.0
        objective_coefficient: 1.0
        is_integer: false
      }
      constraint {
        lower_bound: -0.43165928571429
        upper_bound: -0.43165928571429
        var_index: [ 0, 1 ]
        coefficient: [ 1.0, -1.0 ]
      }
    )pb");
    ComponentPipeline pipeline(params, active_cis4_model);
    const double s20 = std::ldexp(1.0, 20);
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(s20, s20));
    CpModelProto cp_model;
    ConstraintScaler scaler;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(-452628, -452627));
    EXPECT_LE(scaler.max_unrounding_error, 1e-6);
  }

  // An exact half-integer scaled equality `524288 * (x0 - x1) == 0.5` is scaled
  // by `2 * 524288 = 1048576` to preserve `mip_tight_precision`-feasibility,
  // producing the exact domain `[1, 1]`.
  {
    MPModelProto half_int_model;
    half_int_model.set_maximize(true);
    constexpr double kScale = 524288.0;
    for (int i = 0; i < 2; ++i) {
      MPVariableProto* v = half_int_model.add_variable();
      v->set_lower_bound(-2.0);
      v->set_upper_bound(1.0 / kScale);
      v->set_objective_coefficient(1.0);
    }
    MPConstraintProto* ct = half_int_model.add_constraint();
    ct->set_lower_bound(0.5 / kScale);
    ct->set_upper_bound(0.5 / kScale);
    ct->add_var_index(0);
    ct->add_coefficient(1.0);
    ct->add_var_index(1);
    ct->add_coefficient(-1.0);

    ComponentPipeline pipeline(params, std::move(half_int_model));
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(2.0 * kScale, 2.0 * kScale));
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().domain(), ElementsAre(1, 1));
  }

  // 2-modular odd cycle with rational RHS 0.5 and external binary variable b:
  // the multiplier `s = 2` makes the data integer, and `modular_factor = 2`
  // scales variables by 4, keeping +/-1 coefficients on {x0, x1, x2}, even
  // coefficient -2 on b, and even RHS 2 so LP extreme points remain integral.
  {
    const MPModelProto two_mod = ParseTestProto(R"pb(
      maximize: true
      variable {
        lower_bound: 0.0
        upper_bound: 2.0
        objective_coefficient: 1.0
        is_integer: false
      }
      variable {
        lower_bound: 0.0
        upper_bound: 2.0
        objective_coefficient: 1.0
        is_integer: false
      }
      variable {
        lower_bound: 0.0
        upper_bound: 2.0
        objective_coefficient: 1.0
        is_integer: false
      }
      variable {
        lower_bound: 0.0
        upper_bound: 1.0
        objective_coefficient: 1.0
        is_integer: true
      }
      constraint {
        lower_bound: -inf
        upper_bound: 0.5
        var_index: [ 0, 1, 3 ]
        coefficient: [ 1.0, 1.0, -0.5 ]
      }
      constraint {
        lower_bound: -inf
        upper_bound: 1.0
        var_index: [ 1, 2 ]
        coefficient: [ 1.0, 1.0 ]
      }
      constraint {
        lower_bound: -inf
        upper_bound: 1.0
        var_index: [ 0, 2 ]
        coefficient: [ 1.0, 1.0 ]
      }
    )pb");
    ComponentPipeline pipeline(params, two_mod);
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(4.0, 4.0, 4.0, 1.0));
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().coeffs(),
                ElementsAre(1, 1, -2));
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(kint64min, 2));
  }
}

TEST(DetectAndConvertTotallyUnimodularComponentTest,
     PreservesEpsilonFeasibilityUnderProperRelaxation) {
  SatParameters params;
  params.set_mip_tight_precision(1e-8);
  params.set_mip_wanted_precision(1e-6);
  params.set_mip_proper_relaxation(true);

  // 1. Almost-TUM external coefficient rounding error crossing an integer:
  // Row 0 (`10 * (x0 - x1) == 2.5`) requires `s = 4`, and Row 1 is
  // `x0 + 1.0000004 * y >= 1000000.25` with integer `y in [0, 1000000]`.
  // At `(x0, x1, y) = (0.0, -0.25, 1000000)`, both rows are satisfied, while
  // at `factor = 4` the external coefficient `4.0000016` rounds to `4`.
  // Relaxing `4 * 1000000.25 = 4000001` by `lb_err = -1.6` before modular
  // rounding keeps `(x0', x1', y) = (0, -1, 1000000)` feasible.
  {
    MPModelProto almost_tum = ParseTestProto(R"pb(
      variable { lower_bound: -2.0 upper_bound: 2.0 is_integer: false }
      variable { lower_bound: -2.0 upper_bound: 2.0 is_integer: false }
      variable { lower_bound: 0.0 upper_bound: 1000000.0 is_integer: true }
      constraint {
        lower_bound: 2.5
        upper_bound: 2.5
        var_index: [ 0, 1 ]
        coefficient: [ 10.0, -10.0 ]
      }
      constraint {
        lower_bound: 1000000.25
        upper_bound: inf
        var_index: [ 0, 2 ]
        coefficient: [ 1.0, 1.0000004 ]
      }
    )pb");
    SatParameters almost_tum_params = params;
    almost_tum_params.set_mip_wanted_precision(0.45);
    almost_tum_params.set_mip_tight_precision(1e-9);
    ComponentPipeline pipeline(almost_tum_params, std::move(almost_tum));
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(4.0, 4.0, 1.0));
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_TRUE(SolutionIsFeasible(cp_model, {0, -1, 1000000}));
  }

  // 2. Tiny `row_scale` network: `2^-27 * (x0 - x1) >= 2 * 2^-27` and
  // `2^-27 * (x0 - x1) <= 0` with `x0, x1 in [0, 1]`. At `tight_precision =
  // 1.2e-8`, `(x0, x1) = (0.5, 0.0)` is `1.2e-8`-feasible. Since snapping the
  // row bounds at `s = 1` would exceed a tightening of 1 (and make the rows
  // conflicting), they are relaxed by `mip_tight_precision` and rounded outward
  // instead.
  {
    const double tiny = std::ldexp(1.0, -27);
    MPModelProto tiny_net;
    for (int v = 0; v < 2; ++v) {
      MPVariableProto* var = tiny_net.add_variable();
      var->set_lower_bound(0.0);
      var->set_upper_bound(1.0);
    }
    MPConstraintProto* ct_ge = tiny_net.add_constraint();
    ct_ge->set_lower_bound(2.0 * tiny);
    ct_ge->set_upper_bound(std::numeric_limits<double>::infinity());
    ct_ge->add_var_index(0);
    ct_ge->add_coefficient(tiny);
    ct_ge->add_var_index(1);
    ct_ge->add_coefficient(-tiny);

    MPConstraintProto* ct_le = tiny_net.add_constraint();
    ct_le->set_lower_bound(-std::numeric_limits<double>::infinity());
    ct_le->set_upper_bound(0.0);
    ct_le->add_var_index(0);
    ct_le->add_coefficient(tiny);
    ct_le->add_var_index(1);
    ct_le->add_coefficient(-tiny);

    SatParameters tiny_params = params;
    tiny_params.set_mip_tight_precision(1.2e-8);
    ComponentPipeline pipeline(tiny_params, std::move(tiny_net));
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(1.0, 1.0));
    EXPECT_EQ(pipeline.tum_stats().num_relaxed_modular_components, 1);
    CpModelProto tiny_cp;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&tiny_cp));
    EXPECT_EQ(ValidateCpModel(tiny_cp), "");
    EXPECT_TRUE(SolutionIsFeasible(tiny_cp, {1, 0}));
  }

  // 3. TightenComponentBounds() with tiny `row_scale`: `2^-27 * (x0 + x1) == 0`
  // and `2^-27 * x1 == 0` with `x0 in [-1000, 1000]`, `x1 in [0, 0]`, and
  // objective `min -x0`. The point `(x0, x1) = (1.0, 0.0)` is `1e-8`-feasible
  // (`2^-27 * 1.0 < 1e-8`), so `states[0].ub` must not be tightened to `0.0`.
  {
    const double tiny = std::ldexp(1.0, -27);
    MPModelProto tighten_model;
    MPVariableProto* v0 = tighten_model.add_variable();
    v0->set_lower_bound(-1000.0);
    v0->set_upper_bound(1000.0);
    v0->set_objective_coefficient(-1.0);
    MPVariableProto* v1 = tighten_model.add_variable();
    v1->set_lower_bound(0.0);
    v1->set_upper_bound(0.0);

    MPConstraintProto* ct0 = tighten_model.add_constraint();
    ct0->set_lower_bound(0.0);
    ct0->set_upper_bound(0.0);
    ct0->add_var_index(0);
    ct0->add_coefficient(tiny);
    ct0->add_var_index(1);
    ct0->add_coefficient(tiny);

    MPConstraintProto* ct1 = tighten_model.add_constraint();
    ct1->set_lower_bound(0.0);
    ct1->set_upper_bound(0.0);
    ct1->add_var_index(1);
    ct1->add_coefficient(tiny);

    ComponentPipeline pipeline(params, std::move(tighten_model));
    pipeline.RunTum();
    EXPECT_GE(pipeline.states()[0].ub, 1.0);
  }
}

// Runs TightenResidualTotallyUnimodularVariableBounds() on a MPModelProto and
// copies the tightened bounds back.
void TightenResidualTumBoundsOnProto(const SatParameters& params,
                                     MPModelProto* mp_model,
                                     SolverLogger* logger) {
  MPModelProtoWrapper wrapper(*mp_model, logger);
  TightenResidualTotallyUnimodularVariableBounds(params, &wrapper, logger);
  for (int var = 0; var < mp_model->variable_size(); ++var) {
    const MPVariableProto& wrapper_var = wrapper.variables(var);
    MPVariableProto* mp_var = mp_model->mutable_variable(var);
    mp_var->set_lower_bound(wrapper_var.lower_bound());
    mp_var->set_upper_bound(wrapper_var.upper_bound());
  }
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     DoesNotDropBoundsWhenImprovingDirectionUnbounded) {
  // Here x2 is minimized (c_2 = 1 > 0) and has lower_bound = -inf, while x1 has
  // c_1 = 0 and bounds [0, 100] with x1 + x2 = 0. With mip_max_bound = 500,
  // x1's upper bound 100 exceeds 0.1 * mip_max_bound (50), so x1 stays in the
  // component. The unique optimum is (x1, x2) = (100, -100), so x1's upper
  // bound 100 must not be dropped, and x2's lower bound must be tightened to
  // -100 (not 0).
  const std::string text_lp =
      "min: x2;"
      "-inf <= x2 <= 0;"
      "0 <= x1 <= 100;"
      "x1 + x2 = 0;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  SatParameters params;
  params.set_mip_max_bound(500.0);
  TightenResidualTumBoundsOnProto(params, &mp_model, &logger);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).lower_bound(), -100.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 0.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).lower_bound(), 0.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 100.0);
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     TightenBoundsTreatsSmallBoundedContinuousVariablesAsExternal) {
  // Unbounded continuous variables x1, x2 form a TUM chain connected to a
  // non-TUM continuous subproblem (y1, y2 with bounds [0, 10]) via a bridge
  // constraint (2 y1 + 3 y2 - x1 <= 4). Because y1 and y2 have small bounds,
  // they are treated as external variables, allowing x1 and x2 to be isolated
  // as a TUM component and tightened from +inf to 46 + 5 = 51.
  const std::string text_lp =
      "min: x1 + x2;"
      "0 <= x1 <= inf;"
      "0 <= x2 <= inf;"
      "0 <= y1 <= 10;"
      "0 <= y2 <= 10;"
      "2 y1 + 3 y2 <= 20;"
      "5 y1 - 7 y2 <= 15;"
      "2 y1 + 3 y2 - x1 <= 4;"
      "x1 - x2 <= 5;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 51.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 51.0);
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     TightenBoundsOfIntegerVariablesInBigMSchedulingComponent) {
  // Three jobs with processing times p = {3, 4, 5} and due dates d = p + 2.
  // Integer start times s_i and continuous tardiness t_i are unbounded above,
  // and binary x_ij selects the order of jobs i and j via big-M (M = 1000)
  // disjunctive constraints. The 6 unbounded variables form a single TUM
  // component (entries in {-1, +1}) with integer data, whose external
  // variables are the 3 binaries.
  //
  // For each pair, the two big-M rows contribute their tightest RHS (p_i and
  // p_j), while their relaxed RHS (p - M) is droppable. The tardiness rows
  // `t_i - s_i >= -2` have a drop limit of 1, so they must be kept and have
  // magnitude 2. The bound is the sum of the n = 6 largest magnitudes:
  // 5 + 5 + 4 + 4 + 3 + 3 = 24 (instead of staying infinite as before, or of
  // being ~6000 if the relaxed big-M RHS were taken into account).
  const std::string text_lp =
      "min: s1 + s2 + s3 + t1 + t2 + t3;"
      "0 <= s1 <= inf; 0 <= s2 <= inf; 0 <= s3 <= inf;"
      "0 <= t1 <= inf; 0 <= t2 <= inf; 0 <= t3 <= inf;"
      "s2 - s1 + 1000 x12 >= 3; s1 - s2 - 1000 x12 >= -996;"
      "s3 - s1 + 1000 x13 >= 3; s1 - s3 - 1000 x13 >= -995;"
      "s3 - s2 + 1000 x23 >= 4; s2 - s3 - 1000 x23 >= -995;"
      "t1 - s1 >= -2; t2 - s2 >= -2; t3 - s3 >= -2;"
      "int s1, s2, s3; bin x12, x13, x23;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  for (const MPVariableProto& var : mp_model.variable()) {
    if (var.name()[0] == 'x') continue;
    EXPECT_DOUBLE_EQ(var.lower_bound(), 0.0) << var.name();
    EXPECT_DOUBLE_EQ(var.upper_bound(), 24.0) << var.name();
  }
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     TightenBoundsKeepsForcingBigMConstraint) {
  // `x >= 1000 b` is a big-M constraint that is active when b = 1, so the
  // tightest RHS (1000) must be accounted for: x is tightened to [0, 1000] and
  // not below (the optimal solution with b = 1 is x = 1000).
  const std::string text_lp =
      "min: x - 2000 b;"
      "0 <= x <= inf;"
      "x - 1000 b >= 0;"
      "int x; bin b;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 1000.0);
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     DoesNotTightenIntegerVariablesWithoutIntegralVertices) {
  // Component 1 (x1, x2): integer variables with a 2-modular matrix.
  // Component 2 (y1, y2): integer variables with a fractional scaled RHS
  // (2 y1 - 2 y2 >= 1).
  // Component 3 (z1, z2): integer variables with a continuous external variable
  // (w in [0, 1]).
  // Component 4 (u1, u2): integer variables with a non-unit column scaling
  // (2 u1 - u2 >= 0, 2 u1 + u2 <= 10).
  // Component 5 (v1, v2, v3): valid integral TUM component. Row `v1 - v2 + 5 b
  // >= 7` contributes 7, and the two other rows (RHS -3 and -2 once negated)
  // have drop limits 1.5 and 1 < 7 so they are kept: 7 + 3 + 2 = 12.
  const std::string text_lp =
      "min: x1 + x2 + y1 + y2 + z1 + z2 + u1 + u2 + v1 + v2 + v3;"
      "0 <= x1 <= inf; 0 <= x2 <= inf;"
      "x1 + x2 >= 1; x1 - x2 >= 0;"
      "0 <= y1 <= inf; 0 <= y2 <= inf;"
      "2 y1 - 2 y2 >= 1;"
      "0 <= z1 <= inf; 0 <= z2 <= inf; 0 <= w <= 1;"
      "z1 - z2 - 0.5 w >= 0;"
      "0 <= u1 <= inf; 0 <= u2 <= inf;"
      "2 u1 - u2 >= 0; 2 u1 + u2 <= 10;"
      "0 <= v1 <= inf; 0 <= v2 <= inf; 0 <= v3 <= inf;"
      "v1 - v2 + 5 b >= 7; v1 - v3 <= 3; v2 - v3 <= 2;"
      "int x1, x2, y1, y2, z1, z2, u1, u2, v1, v2, v3; bin b;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  for (const MPVariableProto& var : mp_model.variable()) {
    if (var.name()[0] == 'v') {
      EXPECT_DOUBLE_EQ(var.upper_bound(), 12.0) << var.name();
    } else if (var.name()[0] != 'w' && var.name()[0] != 'b') {
      EXPECT_EQ(var.upper_bound(), std::numeric_limits<double>::infinity())
          << var.name();
    }
  }
}

TEST(TightenResidualTotallyUnimodularVariableBoundsTest,
     TightenBoundsDropsRelaxedRowSidesWithIntegerExternalVariables) {
  // Continuous component (x1, x2, x3) with an external integer variable y in
  // [0, 100]: the row `x1 - x2 + 1000 y >= 5` has tightest RHS 5 (y = 0) and
  // its second tightest RHS is -995 (y = 1), which is redundant for any vertex
  // limit <= 995 / 2. So only 5 counts, and with `x2 - x3 >= 8` all variables
  // are tightened to 5 + 8 = 13.
  const std::string integer_y_lp =
      "min: x1 + x2 + x3;"
      "0 <= x1 <= inf; 0 <= x2 <= inf; 0 <= x3 <= inf; 0 <= y <= 100;"
      "x1 - x2 + 1000 y >= 5;"
      "x2 - x3 >= 8;"
      "int y;";
  glop::LinearProgram lp;
  CHECK(ParseLp(integer_y_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 13.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 13.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(2).upper_bound(), 13.0);

  // If y is continuous, the RHS can take any value in [-99995, 5], and since
  // the tightest one is not droppable, the full magnitude 99995 must be used:
  // 99995 + 8 = 100003.
  const std::string continuous_y_lp =
      "min: x1 + x2 + x3;"
      "0 <= x1 <= inf; 0 <= x2 <= inf; 0 <= x3 <= inf; 0 <= y <= 100;"
      "x1 - x2 + 1000 y >= 5;"
      "x2 - x3 >= 8;";
  CHECK(ParseLp(continuous_y_lp, &lp));
  mp_model.Clear();
  LinearProgramToMPModelProto(lp, &mp_model);
  TightenResidualTumBoundsOnProto(SatParameters(), &mp_model, &logger);
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 100003.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 100003.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(2).upper_bound(), 100003.0);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
