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

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/parse_text_proto.h"
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
using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::ElementsAre;

// Runs the per-component functions on all the residual continuous components
// of a model, emulating whole-model scaling functions for the tests below. Each
// step returns the scaling it applied to each variable.
class ComponentPipeline {
 public:
  using Component = ResidualContinuousComponent;

  ComponentPipeline(const SatParameters& params, MPModelProto mp_model)
      : params_(params),
        wrapper_(std::move(mp_model)),
        states_(InitialVariableStates(wrapper_)),
        components_(SplitIntoResidualContinuousComponents(wrapper_)),
        var_to_local_col_(wrapper_.num_variables(), -1) {
    // Process the components in a deterministic order.
    std::sort(components_.components.begin(), components_.components.end(),
              [](const Component& a, const Component& b) {
                return a.continuous_vars.front() < b.continuous_vars.front();
              });
  }

  std::vector<double> RunTum() {
    modular_scalings_.assign(components_.components.size(), {});
    int idx = 0;
    return Run([&](const Component& c) {
      modular_scalings_[idx++] = MarkComponentTotallyUnimodularAsInteger(
          params_, c, wrapper_, absl::MakeSpan(states_), &tum_stats_);
    });
  }

  std::vector<double> ScaleForGivenPrecision() {
    return Run([&](const Component& c) {
      ScaleComponentContinuousVariablesForGivenPrecision(
          params_, c, wrapper_, absl::MakeSpan(states_),
          &precision_scaling_stats_);
    });
  }

  bool MakeBoundsOfIntegerVariablesInteger() {
    int64_t num_changes = 0;
    for (const Component& c : components_.components) {
      if (!sat::MakeBoundsOfIntegerVariablesInteger(
              params_, c.continuous_vars, absl::MakeSpan(states_), &num_changes,
              /*logger=*/nullptr)) {
        return false;
      }
    }
    return true;
  }

  const std::vector<MipVariableState>& states() const { return states_; }
  std::vector<MipVariableState>* mutable_states() { return &states_; }
  const ResidualContinuousComponents& components() const { return components_; }
  const TumStats& tum_stats() const { return tum_stats_; }

  // Returns the model with all the transformations applied.
  MPModelProto Model() const {
    MPModelProto result = wrapper_.underlying_model();
    ApplyVariableStatesToModel(states_, &result);
    return result;
  }

  // Converts the model into `cp_model`, scaling the constraints of the
  // components made integral by RunTum() with their modular scaling.
  bool ConvertToCpModel(CpModelProto* cp_model,
                        ConstraintScaler* out_scaler = nullptr) {
    cp_model->Clear();
    const int num_vars = wrapper_.num_variables();
    int num_small_domains = 0;
    for (int v = 0; v < num_vars; ++v) {
      if (!ConvertVariableToCpVariable(
              params_, v, wrapper_.variables(v).objective_coefficient(),
              &states_[v], cp_model->add_variables(), &num_small_domains)) {
        return false;
      }
    }
    ConstraintScaler local_scaler;
    ConstraintScaler& scaler =
        out_scaler != nullptr ? *out_scaler : local_scaler;
    scaler.wanted_precision = params_.mip_wanted_precision();
    scaler.use_tight_bounds = params_.mip_use_tight_bounds();
    scaler.use_proper_relaxation = params_.mip_proper_relaxation();
    scaler.tight_precision = params_.mip_tight_precision();
    scaler.scaling_target = int64_t{1} << params_.mip_max_activity_exponent();
    const auto add = [&](int c,
                         std::optional<ModularConstraintScaling> modular) {
      return scaler
          .ScaleAndAddConstraint(wrapper_.constraints(c), states_, cp_model,
                                 modular)
          .ok();
    };

    for (const int c : components_.pure_integer_constraints) {
      if (!add(c, std::nullopt)) return false;
    }
    for (int idx = 0; idx < components_.components.size(); ++idx) {
      const std::vector<int>& rows = components_.components[idx].constraints;
      const bool is_modular =
          idx < modular_scalings_.size() && !modular_scalings_[idx].empty();
      for (int r = 0; r < rows.size(); ++r) {
        if (!add(rows[r], is_modular
                              ? std::make_optional(modular_scalings_[idx][r])
                              : std::nullopt)) {
          return false;
        }
      }
    }
    PopulateCpModelFloatingPointObjective(wrapper_, states_, cp_model);
    return true;
  }

 private:
  std::vector<double> Run(const std::function<void(const Component&)>& f) {
    const int num_vars = wrapper_.num_variables();
    std::vector<double> result(num_vars);
    for (int v = 0; v < num_vars; ++v) result[v] = states_[v].scaling;
    for (Component& c : components_.components) {
      const ScopedVarToLocalCol scoped(&c, &var_to_local_col_);
      f(c);
    }
    for (int v = 0; v < num_vars; ++v)
      result[v] = states_[v].scaling / result[v];
    return result;
  }

  const SatParameters params_;
  const MPModelProtoWrapper wrapper_;
  std::vector<MipVariableState> states_;
  ResidualContinuousComponents components_;
  std::vector<std::vector<ModularConstraintScaling>> modular_scalings_;
  std::vector<int> var_to_local_col_;
  TumStats tum_stats_;
  PrecisionScalingStats precision_scaling_stats_;
};

// Runs MarkComponentTotallyUnimodularAsInteger() on all the residual
// components of `mp_model`, updates it in place together with the given
// integerization errors, and returns the scaling of each variable (empty if no
// component was TUM or 2-modular).
std::vector<double> MarkAllResidualTotallyUnimodularVariablesAsInteger(
    const SatParameters& params, MPModelProto* mp_model,
    absl::Span<double> err_lb, absl::Span<double> err_ub, SolverLogger*) {
  ComponentPipeline pipeline(params, *mp_model);
  const int num_vars = mp_model->variable_size();
  for (int v = 0; v < num_vars; ++v) {
    (*pipeline.mutable_states())[v].error_lb = err_lb[v];
    (*pipeline.mutable_states())[v].error_ub = err_ub[v];
  }
  std::vector<double> scaling = pipeline.RunTum();
  for (int v = 0; v < num_vars; ++v) {
    err_lb[v] = pipeline.states()[v].error_lb;
    err_ub[v] = pipeline.states()[v].error_ub;
  }
  *mp_model = pipeline.Model();
  const TumStats& stats = pipeline.tum_stats();
  if (stats.num_tum_components + stats.num_2_modular_components == 0) {
    return {};
  }
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

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
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

  SolverLogger logger;
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  MarkAllResidualTotallyUnimodularVariablesAsInteger(
      SatParameters(), &mp_model, absl::MakeSpan(err_lb),
      absl::MakeSpan(err_ub), &logger);

  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_TRUE(mp_model.variable(2).is_integer());
  EXPECT_TRUE(mp_model.variable(3).is_integer());
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
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

  SolverLogger logger;
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  MarkAllResidualTotallyUnimodularVariablesAsInteger(
      SatParameters(), &mp_model, absl::MakeSpan(err_lb),
      absl::MakeSpan(err_ub), &logger);

  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_TRUE(var.is_integer()) << var.name();
  }
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     OddCycleIsNotTotallyUnimodular) {
  // Triangle of +1 coefficients (det = 2) and 2-cycle with mixed signs
  // (x4 + x5 <= 1, x4 - x5 <= 0, det = -2).
  const std::string text_lp =
      "x1 + x2 <= 1;"
      "x2 + x3 <= 1;"
      "x3 + x1 <= 1;"
      "x4 + x5 <= 1;"
      "x4 - x5 <= 0;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  SatParameters params;
  params.set_mip_automatically_scale_variables(false);
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  MarkAllResidualTotallyUnimodularVariablesAsInteger(
      params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
      &logger);

  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_FALSE(var.is_integer()) << var.name();
  }
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
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
      "0 <= x1 <= 1.0000001;"
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

  SolverLogger logger;
  SatParameters params;
  params.set_mip_automatically_scale_variables(false);
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  MarkAllResidualTotallyUnimodularVariablesAsInteger(
      params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
      &logger);

  for (const MPVariableProto& var : mp_model.variable()) {
    if (var.name() == "x1" || var.name() == "x2" || var.name() == "x3" ||
        var.name() == "y") {
      EXPECT_TRUE(var.is_integer()) << var.name();
    } else {
      EXPECT_FALSE(var.is_integer()) << var.name();
    }
  }

  // With automatic scaling enabled (default), components 4 (w1, w2) and 5 (z1)
  // can also be scaled to integers (by 2 and 5 respectively).
  params.set_mip_automatically_scale_variables(true);
  const std::vector<double> scaling =
      MarkAllResidualTotallyUnimodularVariablesAsInteger(
          params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
          &logger);
  for (int i = 0; i < mp_model.variable_size(); ++i) {
    const MPVariableProto& var = mp_model.variable(i);
    if (var.name() == "w1" || var.name() == "w2") {
      EXPECT_TRUE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 2.0) << var.name();
    } else if (var.name() == "z1") {
      EXPECT_TRUE(var.is_integer()) << var.name();
      EXPECT_DOUBLE_EQ(scaling[i], 5.0) << var.name();
    } else if (var.name() == "u1" || var.name() == "u2" || var.name() == "v1" ||
               var.name() == "v2" || var.name() == "v3" || var.name() == "v4") {
      EXPECT_FALSE(var.is_integer()) << var.name();
    }
  }
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest, RowWiseAndSingletonPeeling) {
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

  SolverLogger logger;
  SatParameters params;
  params.set_mip_automatically_scale_variables(false);
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  MarkAllResidualTotallyUnimodularVariablesAsInteger(
      params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
      &logger);
  for (const MPVariableProto& var : mp_model.variable()) {
    if (var.name()[0] == 'v' || var.name()[0] == 's') {
      EXPECT_TRUE(var.is_integer()) << var.name();
    } else {
      EXPECT_FALSE(var.is_integer()) << var.name();
    }
  }

  params.set_mip_automatically_scale_variables(true);
  const std::vector<double> scaling =
      MarkAllResidualTotallyUnimodularVariablesAsInteger(
          params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
          &logger);
  EXPECT_THAT(scaling,
              ElementsAre(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.0));
  for (const MPVariableProto& var : mp_model.variable()) {
    EXPECT_TRUE(var.is_integer()) << var.name();
  }
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
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

  SolverLogger logger;
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  const std::vector<double> scaling =
      MarkAllResidualTotallyUnimodularVariablesAsInteger(
          SatParameters(), &mp_model, absl::MakeSpan(err_lb),
          absl::MakeSpan(err_ub), &logger);
  EXPECT_THAT(scaling, ElementsAre(4.0, 4.0));
  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 42.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 9.0);
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest, RowAndColumnScaledComponent) {
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

  SolverLogger logger;
  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  const std::vector<double> scaling =
      MarkAllResidualTotallyUnimodularVariablesAsInteger(
          SatParameters(), &mp_model, absl::MakeSpan(err_lb),
          absl::MakeSpan(err_ub), &logger);
  EXPECT_THAT(scaling, ElementsAre(2.0, 4.0, 10.0));
  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_TRUE(mp_model.variable(2).is_integer());
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 20.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 20.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(2).upper_bound(), 20.0);
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
  // constraint (2 y1 + 3 y2 - x1 <= 4). When `only_tighten_bounds` is true, y1
  // and y2 are treated as external variables, allowing x1 and x2 to be isolated
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

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     FallbackCommonPowerOfTwoScalingWhenScaleForPrecision) {
  // Bounds 0.4142135623730951 (sqrt(2) - 1) and 3e5 have no rational factor
  // within mip_max_bound / 3e5. Even with mip_max_bound = 1000,
  // mip_scale_for_precision scales the whole TUM component together up to the
  // 1e11 precision domain cap:
  //   compo_max_scaling = 2^FloorLog2(1e11 / 3e5) = 2^18 = 262144
  // (below needed_scaling = 2^22 for 2 * 1.0 / 5e-7 = 4e6), preserving unit
  // coefficients in x1 + x2 <= 350000.
  const std::string text_lp =
      "max: x1 + x2;"
      "0 <= x1 <= 100000;"
      "0.4142135623730951 <= x2 <= 300000;"
      "x1 + x2 <= 350000;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);

  SolverLogger logger;
  SatParameters params;
  params.set_mip_max_bound(1000.0);
  params.set_mip_scale_for_precision(true);
  params.set_mip_wanted_precision(1e-6);

  ComponentPipeline pipeline(params, mp_model);
  const double expected = std::ldexp(1.0, 18);
  EXPECT_THAT(pipeline.RunTum(), ElementsAre(expected, expected));
  EXPECT_FALSE(pipeline.states()[0].is_integer);
  EXPECT_FALSE(pipeline.states()[1].is_integer);
  EXPECT_THAT(pipeline.ScaleForGivenPrecision(), ElementsAre(1.0, 1.0));

  MPModelProtoWrapper wrapper(pipeline.Model(), &logger);
  const std::vector<double> zero_errors(wrapper.num_variables(), 0.0);
  CpModelProto cp_model;
  ASSERT_TRUE(ConvertMPModelProtoToCpModelProto(
      params, wrapper, /*num_clamped=*/0, zero_errors, zero_errors, &cp_model,
      &logger));
  EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(ValidateCpModel(cp_model), "");
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     CommonPrecisionScalingRespectsHeadroomAndSharesWithOtherVariables) {
  // With mip_max_bound = 100 and mip_max_activity_exponent = 15
  // (scaling_target = 32768):
  // - TUM component {x0, x1}: 0 <= x0 <= 1, 0.4142135623730951 <= x1 <= 1
  //   (not integerizable within max_s = 100), x0 + x1 >= 1.5 (constraint
  //   headroom = 16384).
  // - Non-TUM component {y0, y1} in [0, 1]^2: 2 y0 + 3 y1 <= 4 and
  //   5 y0 - 7 y1 <= 1 (constraint headroom = 4096).
  //
  // Both x0 and x1 in the TUM component are scaled by the minimum headroom
  // across the component (16384), and ScaleContinuousVariablesForGivenPrecision
  // scales y0 and y1 by 4096.
  MPModelProto mp_model = ParseTestProto(R"pb(
    variable {
      lower_bound: 0.0
      upper_bound: 1.0
      is_integer: false
      objective_coefficient: 1.25
    }
    variable {
      lower_bound: 0.4142135623730951
      upper_bound: 1.0
      is_integer: false
    }
    variable {
      lower_bound: 0.0
      upper_bound: 1.0
      is_integer: false
      objective_coefficient: 1.0
    }
    variable { lower_bound: 0.0 upper_bound: 1.0 is_integer: false }
    constraint {
      lower_bound: 1.5
      upper_bound: inf
      var_index: [ 0, 1 ]
      coefficient: [ 1.0, 1.0 ]
    }
    constraint {
      lower_bound: -inf
      upper_bound: 4.0
      var_index: [ 2, 3 ]
      coefficient: [ 2.0, 3.0 ]
    }
    constraint {
      lower_bound: -inf
      upper_bound: 1.0
      var_index: [ 2, 3 ]
      coefficient: [ 5.0, -7.0 ]
    }
  )pb");

  SolverLogger logger;
  SatParameters params;
  params.set_mip_max_bound(100.0);
  params.set_mip_max_activity_exponent(15);
  params.set_mip_scale_for_precision(true);
  params.set_mip_wanted_precision(1e-6);

  ComponentPipeline pipeline(params, mp_model);
  EXPECT_THAT(pipeline.RunTum(), ElementsAre(16384.0, 16384.0, 1.0, 1.0));
  EXPECT_THAT(pipeline.ScaleForGivenPrecision(),
              ElementsAre(1.0, 1.0, 4096.0, 4096.0));

  // Snap bounds close to zero before CP-SAT conversion, then verify that the
  // TUM constraint keeps unit coefficients and the objective scales cleanly.
  mp_model = pipeline.Model();
  mp_model.mutable_variable(1)->set_lower_bound(
      std::floor(mp_model.variable(1).lower_bound()));
  MPModelProtoWrapper wrapper(std::move(mp_model), &logger);
  const std::vector<double> zero_errors(wrapper.num_variables(), 0.0);
  CpModelProto cp_model;
  ASSERT_TRUE(ConvertMPModelProtoToCpModelProto(
      params, wrapper, /*num_clamped=*/0, zero_errors, zero_errors, &cp_model,
      &logger));
  EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, 1));
  EXPECT_EQ(ValidateCpModel(cp_model), "");
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     MixedIntegralAndCommonPrecisionScaledComponents) {
  // With mip_max_bound = 100:
  // - Component 0 ({x0, x1}): TUM with rational RHS 1.5 -> scaled by 2 and
  //   marked integer.
  // - Component 1 ({z0, z1}): TUM with z1 lower bound 0.4142135623730951
  //   (not integerizable within max_s = 100). With mip_max_activity_exponent =
  //   10 (scaling_target = 1024), z0 + z1 >= 1.5 has constraint headroom 512.
  MPModelProto mp_model = ParseTestProto(R"pb(
    variable {
      lower_bound: 0.0
      upper_bound: 1.0
      is_integer: false
      objective_coefficient: 2.5
    }
    variable { lower_bound: 0.0 upper_bound: 1.0 is_integer: false }
    variable {
      lower_bound: 0.0
      upper_bound: 1.0
      is_integer: false
      objective_coefficient: 1.0
    }
    variable {
      lower_bound: 0.4142135623730951
      upper_bound: 1.0
      is_integer: false
    }
    constraint {
      lower_bound: 1.5
      upper_bound: inf
      var_index: [ 0, 1 ]
      coefficient: [ 1.0, 1.0 ]
    }
    constraint {
      lower_bound: 1.5
      upper_bound: inf
      var_index: [ 2, 3 ]
      coefficient: [ 1.0, 1.0 ]
    }
  )pb");

  SolverLogger logger;
  SatParameters params;
  params.set_mip_max_bound(100.0);
  params.set_mip_max_activity_exponent(10);
  params.set_mip_scale_for_precision(true);
  params.set_mip_wanted_precision(1e-6);

  std::vector<double> err_lb(mp_model.variable_size(), 0.0);
  std::vector<double> err_ub(mp_model.variable_size(), 0.0);
  const std::vector<double> tum_scaling =
      MarkAllResidualTotallyUnimodularVariablesAsInteger(
          params, &mp_model, absl::MakeSpan(err_lb), absl::MakeSpan(err_ub),
          &logger);
  EXPECT_THAT(tum_scaling, ElementsAre(2.0, 2.0, 512.0, 512.0));
  EXPECT_TRUE(mp_model.variable(0).is_integer());
  EXPECT_TRUE(mp_model.variable(1).is_integer());
  EXPECT_FALSE(mp_model.variable(2).is_integer());
  EXPECT_FALSE(mp_model.variable(3).is_integer());
  EXPECT_DOUBLE_EQ(mp_model.variable(0).upper_bound(), 2.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(1).upper_bound(), 2.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(2).upper_bound(), 512.0);
  EXPECT_DOUBLE_EQ(mp_model.variable(3).upper_bound(), 512.0);
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     ScalesModularComponentsViaConstraintScalerDirectly) {
  SatParameters params;
  params.set_mip_scale_for_precision(true);
  params.set_mip_use_tight_bounds(true);
  params.set_mip_proper_relaxation(true);

  // Approximate RHS TUM component (modeled on MIPLIB `ci-s4` c25881/c25961):
  //   x0 - x1 == -0.43165928571429, x0 - x1 <= 1.6099, x0, x1 in [0, 9].
  // TUM scales {x0, x1} by 1434 * 57 = 81738, where
  // 81738 * -0.43165928571429 = -35282.966696. Scaling the constraint directly
  // via `ScaleAndAddModularConstraint` keeps exact +/-1 coefficients and
  // relaxes the row domain outward to [-35283, -35282] under
  // `mip_proper_relaxation` (or snaps to [-35283, -35283] without
  // `mip_proper_relaxation`), recording its unrounding error on the scaler
  // without inflating variable discretization error.
  const MPModelProto cis4_model = ParseTextProtoOrDie(R"pb(
    variable { lower_bound: 0.0 upper_bound: 9.0 is_integer: false }
    variable { lower_bound: 0.0 upper_bound: 9.0 is_integer: false }
    constraint {
      lower_bound: -0.43165928571429
      upper_bound: -0.43165928571429
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

  {
    ComponentPipeline pipeline(params, cis4_model);
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(81738.0, 81738.0));
    EXPECT_DOUBLE_EQ(pipeline.states()[0].error_lb, 0.0);
    EXPECT_DOUBLE_EQ(pipeline.states()[0].error_ub, 0.0);
    ASSERT_TRUE(pipeline.MakeBoundsOfIntegerVariablesInteger());
    CpModelProto cp_model;
    ConstraintScaler scaler;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().coeffs(), ElementsAre(1, -1));
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(-35283, -35282));
    EXPECT_DOUBLE_EQ(scaler.max_discretization_error, 0.0);
    EXPECT_NEAR(scaler.max_unrounding_error,
                (35282.966696 - 35282.0) / 81738.0 - 1e-8, 1e-8);
  }

  {
    SatParameters non_relax_params = params;
    non_relax_params.set_mip_proper_relaxation(false);
    ComponentPipeline pipeline(non_relax_params, cis4_model);
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(81738.0, 81738.0));
    ASSERT_TRUE(pipeline.MakeBoundsOfIntegerVariablesInteger());
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(-35283, -35283));
  }

  // When `chosen_s * mip_wanted_precision() >= 0.5` (here `chosen_s = 524288`,
  // `524288 * 1e-6 = 0.524288`), an exact half-integer scaled equality
  // `524288 * (x0 - x1) == 0.5` is within `mip_wanted_precision` of 1 in
  // original space and snaps cleanly to `[1, 1]` without crossing.
  {
    SatParameters non_relax_params = params;
    non_relax_params.set_mip_proper_relaxation(false);
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

    ComponentPipeline pipeline(non_relax_params, std::move(half_int_model));
    EXPECT_THAT(pipeline.RunTum(), ElementsAre(kScale, kScale));
    ASSERT_TRUE(pipeline.MakeBoundsOfIntegerVariablesInteger());
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    EXPECT_THAT(cp_model.constraints(0).linear().domain(), ElementsAre(1, 1));
  }

  // 2-modular odd cycle with rational RHS 0.5, external binary variable b, and
  // an initial free constraint skipped by
  // SplitIntoResidualContinuousComponents: `chosen_s = 2` makes `w`-space
  // integer, and `modular_factor = 2` scales variables by 4, keeping exact +/-1
  // coefficients on {x0, x1, x2}, even coefficient -2 on b, and even RHS 2 so
  // LP extreme points remain integral.
  {
    const MPModelProto two_mod = ParseTextProtoOrDie(R"pb(
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
        upper_bound: inf
        var_index: [ 0, 1 ]
        coefficient: [ 3.0, -5.0 ]
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
    ASSERT_TRUE(pipeline.MakeBoundsOfIntegerVariablesInteger());
    CpModelProto cp_model;
    ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model));
    EXPECT_EQ(ValidateCpModel(cp_model), "");
    ASSERT_EQ(cp_model.constraints_size(), 3);
    EXPECT_THAT(cp_model.constraints(0).linear().coeffs(),
                ElementsAre(1, 1, -2));
    EXPECT_THAT(cp_model.constraints(0).linear().domain(),
                ElementsAre(kint64min, 2));
  }
}

TEST(MarkComponentTotallyUnimodularAsIntegerTest,
     LargeApproximateTumComponentEnlargesBoundsWithoutCrossColumnAccumulation) {
  // Modeled on MIPLIB `osorio-cta`: 6000 continuous flow variables in 5 balance
  // equalities (`sum = 0`), with upper bounds `0.0222609232214646` where
  // `FindRationalFactor` picks `chosen_s = 584` (`584 * ub = 13.000379...`).
  // Enlarging bounds directly to `[0, 14]` and scaling the balance rows via
  // `ScaleAndAddModularConstraint` keeps lower bounds at 0 (0 snapped bounds!)
  // and exact bounds `[0, 1168]`.
  constexpr int kNumPairs = 3000;
  MPModelProto base_model;
  for (int i = 0; i < kNumPairs; ++i) {
    MPVariableProto* v_small = base_model.add_variable();
    v_small->set_lower_bound(0.0);
    v_small->set_upper_bound(0.0222609232214646);
    v_small->set_objective_coefficient(-1.0);

    MPVariableProto* v_large = base_model.add_variable();
    v_large->set_lower_bound(0.0);
    v_large->set_upper_bound(2.0);
    v_large->set_objective_coefficient(0.1);
  }
  constexpr int kNumRows = 5;
  for (int r = 0; r < kNumRows; ++r) {
    MPConstraintProto* ct = base_model.add_constraint();
    ct->set_lower_bound(0.0);
    ct->set_upper_bound(0.0);
    for (int i = r * (kNumPairs / kNumRows);
         i < (r + 1) * (kNumPairs / kNumRows); ++i) {
      ct->add_var_index(2 * i);
      ct->add_coefficient(1.0);
      ct->add_var_index(2 * i + 1);
      ct->add_coefficient(-1.0);
    }
    if (r + 1 < kNumRows) {
      ct->add_var_index(2 * ((r + 1) * (kNumPairs / kNumRows)));
      ct->add_coefficient(-1.0);
    }
  }

  SatParameters params;
  params.set_mip_scale_for_precision(true);
  params.set_mip_use_tight_bounds(true);
  params.set_mip_proper_relaxation(true);

  ComponentPipeline pipeline(params, std::move(base_model));
  const std::vector<double> scaling = pipeline.RunTum();
  ASSERT_EQ(scaling.size(), 2 * kNumPairs);
  EXPECT_DOUBLE_EQ(scaling[0], 584.0);

  int64_t num_snapped = 0;
  ASSERT_TRUE(MakeBoundsOfIntegerVariablesInteger(
      params, pipeline.components().components[0].continuous_vars,
      absl::MakeSpan(*pipeline.mutable_states()), &num_snapped,
      /*logger=*/nullptr));
  EXPECT_EQ(num_snapped, 0);

  CpModelProto cp_model;
  ConstraintScaler scaler;
  ASSERT_TRUE(pipeline.ConvertToCpModel(&cp_model, &scaler));
  EXPECT_EQ(ValidateCpModel(cp_model), "");
  EXPECT_DOUBLE_EQ(scaler.max_discretization_error, 0.0);
  EXPECT_THAT(cp_model.variables(0).domain(), ElementsAre(0, 14));
  EXPECT_THAT(cp_model.variables(1).domain(), ElementsAre(0, 1168));
  for (int r = 0; r < kNumRows; ++r) {
    EXPECT_THAT(cp_model.constraints(r).linear().domain(), ElementsAre(0, 0));
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
