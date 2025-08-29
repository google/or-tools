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

#include "ortools/math_opt/cpp/parameters.h"

#include <optional>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/enums_testing.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/glpk.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::status::StatusIs;

INSTANTIATE_TYPED_TEST_SUITE_P(SolverType, EnumTest, SolverType);
INSTANTIATE_TYPED_TEST_SUITE_P(LPAlgorithm, EnumTest, LPAlgorithm);
INSTANTIATE_TYPED_TEST_SUITE_P(Emphasis, EnumTest, Emphasis);

TEST(SolverTypeTest, Flags) {
  {
    SolverType value = SolverType::kGlop;
    std::string error;
    EXPECT_TRUE(AbslParseFlag("glpk", &value, &error));
    EXPECT_EQ(value, SolverType::kGlpk);
    EXPECT_EQ(error, "");

    EXPECT_EQ(AbslUnparseFlag(value), "glpk");
  }
  {
    SolverType value = SolverType::kGlpk;
    std::string error;
    EXPECT_FALSE(AbslParseFlag("unknown", &value, &error));
    EXPECT_NE(error, "");
  }
}

TEST(LPAlgorithmTest, Flags) {
  {
    LPAlgorithm value = LPAlgorithm::kPrimalSimplex;
    std::string error;
    EXPECT_TRUE(AbslParseFlag("primal_simplex", &value, &error));
    EXPECT_EQ(value, LPAlgorithm::kPrimalSimplex);
    EXPECT_EQ(error, "");

    EXPECT_EQ(AbslUnparseFlag(value), "primal_simplex");
  }
  {
    LPAlgorithm value = LPAlgorithm::kPrimalSimplex;
    std::string error;
    EXPECT_FALSE(AbslParseFlag("unknown", &value, &error));
    EXPECT_NE(error, "");
  }
}

TEST(EmphasisTest, Flags) {
  {
    Emphasis value = Emphasis::kHigh;
    std::string error;
    EXPECT_TRUE(AbslParseFlag("high", &value, &error));
    EXPECT_EQ(value, Emphasis::kHigh);
    EXPECT_EQ(error, "");

    EXPECT_EQ(AbslUnparseFlag(value), "high");
  }
  {
    Emphasis value = Emphasis::kHigh;
    std::string error;
    EXPECT_FALSE(AbslParseFlag("unknown", &value, &error));
    EXPECT_NE(error, "");
  }
}

TEST(GurobiParameters, Empty) {
  GurobiParameters gurobi;
  EXPECT_TRUE(gurobi.empty());
  gurobi.param_values["x"] = "dog";
  EXPECT_FALSE(gurobi.empty());
}

TEST(GurobiParameters, ProtoRoundTrip) {
  GurobiParameters gurobi;
  gurobi.param_values["x"] = "dog";
  gurobi.param_values["ab"] = "7";
  const GurobiParametersProto proto = gurobi.Proto();
  EXPECT_THAT(proto, EquivToProto(R"pb(parameters:
                                       [ { name: "x" value: "dog" }
                                         , { name: "ab" value: "7" }])pb"));
  GurobiParameters end = GurobiParameters::FromProto(proto);
  EXPECT_THAT(end.param_values, ElementsAre(Pair("x", "dog"), Pair("ab", "7")));
}

TEST(GurobiParameters, EmptyProtoRoundTrip) {
  GurobiParameters gurobi;
  const GurobiParametersProto proto = gurobi.Proto();
  EXPECT_THAT(proto, EquivToProto(GurobiParametersProto()));
  GurobiParameters end = GurobiParameters::FromProto(proto);
  EXPECT_THAT(end.param_values, ::testing::IsEmpty());
}

TEST(GlpkParameters, ProtoRoundTrip) {
  // Test with `optional bool` set to true.
  {
    GlpkParametersProto proto;
    proto.set_compute_unbound_rays_if_possible(true);
    EXPECT_THAT(GlpkParameters::FromProto(proto).Proto(), EqualsProto(proto));
  }
  // Test with `optional bool` set to false.
  {
    GlpkParametersProto proto;
    proto.set_compute_unbound_rays_if_possible(false);
    EXPECT_THAT(GlpkParameters::FromProto(proto).Proto(), EqualsProto(proto));
  }
}

TEST(GlpkParameters, EmptyProtoRoundTrip) {
  const GlpkParametersProto empty;
  EXPECT_THAT(GlpkParameters::FromProto(empty).Proto(), EqualsProto(empty));
}

TEST(SolveParameters, EmptyProtoRoundTrip) {
  const SolveParametersProto start;
  ASSERT_OK_AND_ASSIGN(SolveParameters cpp_params,
                       SolveParameters::FromProto(start));
  const SolveParametersProto end = cpp_params.Proto();
  EXPECT_THAT(end, EquivToProto(start));
}

TEST(SolveParameters, CommonParamsRoundTrip) {
  SolveParameters start;
  start.enable_output = true;
  start.time_limit = absl::Seconds(1);
  start.iteration_limit = 7;
  start.node_limit = 3;
  start.relative_gap_tolerance = 1.0;
  start.absolute_gap_tolerance = 0.1;
  start.cutoff_limit = 50.1;
  start.objective_limit = 51.1;
  start.best_bound_limit = 52.1;
  start.solution_limit = 17;
  start.threads = 3;
  start.random_seed = 12;
  start.solution_pool_size = 44;
  start.lp_algorithm = LPAlgorithm::kDualSimplex;
  start.presolve = Emphasis::kMedium;
  start.cuts = Emphasis::kOff;
  start.scaling = Emphasis::kLow;
  start.heuristics = Emphasis::kVeryHigh;
  const SolveParametersProto proto = start.Proto();
  EXPECT_THAT(proto, EquivToProto(R"pb(
                enable_output: true
                time_limit: { seconds: 1 }
                iteration_limit: 7
                node_limit: 3
                relative_gap_tolerance: 1.0
                absolute_gap_tolerance: 0.1
                cutoff_limit: 50.1
                objective_limit: 51.1
                best_bound_limit: 52.1
                solution_limit: 17
                threads: 3
                random_seed: 12
                solution_pool_size: 44
                lp_algorithm: LP_ALGORITHM_DUAL_SIMPLEX
                presolve: EMPHASIS_MEDIUM
                cuts: EMPHASIS_OFF
                scaling: EMPHASIS_LOW
                heuristics: EMPHASIS_VERY_HIGH
              )pb"));
  ASSERT_OK_AND_ASSIGN(const SolveParameters end,
                       SolveParameters::FromProto(proto));
  EXPECT_TRUE(end.enable_output);
  EXPECT_EQ(end.time_limit, absl::Seconds(1));
  EXPECT_THAT(end.iteration_limit, Optional(7));
  EXPECT_THAT(end.node_limit, Optional(3));
  EXPECT_THAT(end.relative_gap_tolerance, Optional(1.0));
  EXPECT_THAT(end.absolute_gap_tolerance, Optional(0.1));
  EXPECT_THAT(end.cutoff_limit, Optional(50.1));
  EXPECT_THAT(end.objective_limit, Optional(51.1));
  EXPECT_THAT(end.best_bound_limit, Optional(52.1));
  EXPECT_THAT(end.solution_limit, Optional(17));
  EXPECT_THAT(end.threads, Optional(3));
  EXPECT_THAT(end.random_seed, Optional(12));
  EXPECT_THAT(end.solution_pool_size, Optional(44));
  EXPECT_THAT(end.lp_algorithm, Optional(LPAlgorithm::kDualSimplex));
  EXPECT_THAT(end.presolve, Optional(Emphasis::kMedium));
  EXPECT_THAT(end.cuts, Optional(Emphasis::kOff));
  EXPECT_THAT(end.scaling, Optional(Emphasis::kLow));
  EXPECT_THAT(end.heuristics, Optional(Emphasis::kVeryHigh));
}

TEST(SolveParameters, SolverSpecificParamsRoundTrip) {
  SolveParametersProto start;
  start.set_random_seed(7);
  start.mutable_gscip()->set_num_solutions(12);
  auto* p = start.mutable_gurobi()->add_parameters();
  p->set_name("x");
  p->set_value("7");
  start.mutable_glop()->set_random_seed(45);
  start.mutable_cp_sat()->set_num_workers(50);
  start.mutable_osqp()->set_alpha(1.2);
  start.mutable_glpk()->set_compute_unbound_rays_if_possible(true);
  (*start.mutable_highs()->mutable_int_options())["test_param"] = 3;
  ASSERT_OK_AND_ASSIGN(SolveParameters cpp_params,
                       SolveParameters::FromProto(start));
  const SolveParametersProto end = cpp_params.Proto();
  EXPECT_THAT(end, EquivToProto(start));
}

TEST(SolveParameters, FromProtoBadTimeLimit) {
  SolveParametersProto proto;
  proto.mutable_time_limit()->set_seconds(1);
  proto.mutable_time_limit()->set_nanos(-1);
  EXPECT_THAT(
      SolveParameters::FromProto(proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("time_limit")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
