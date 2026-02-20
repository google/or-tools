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

#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"
#include "ortools/math_opt/cpp/enums.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

MATCHER_P2(ProtoBoundsAre, lower, upper, "") {
  return arg.lower() == lower && arg.upper() == upper;
}

ModelSubsetProto::Bounds BuildBoundsProto(const bool lower, const bool upper) {
  ModelSubsetProto::Bounds proto;
  proto.set_lower(lower);
  proto.set_upper(upper);
  return proto;
}

TEST(ModelSubsetBoundsTest, FromProto) {
  EXPECT_EQ(ModelSubset::Bounds::FromProto(ModelSubsetProto::Bounds()),
            (ModelSubset::Bounds{.lower = false, .upper = false}));
  EXPECT_EQ(ModelSubset::Bounds::FromProto(BuildBoundsProto(false, true)),
            (ModelSubset::Bounds{.lower = false, .upper = true}));
  EXPECT_EQ(ModelSubset::Bounds::FromProto(BuildBoundsProto(true, false)),
            (ModelSubset::Bounds{.lower = true, .upper = false}));
  EXPECT_EQ(ModelSubset::Bounds::FromProto(BuildBoundsProto(true, true)),
            (ModelSubset::Bounds{.lower = true, .upper = true}));
}

TEST(ModelSubsetBoundsTest, Proto) {
  EXPECT_THAT(ModelSubset::Bounds().Proto(), ProtoBoundsAre(false, false));
  EXPECT_THAT((ModelSubset::Bounds{.lower = false, .upper = true}.Proto()),
              ProtoBoundsAre(false, true));
  EXPECT_THAT((ModelSubset::Bounds{.lower = true, .upper = false}.Proto()),
              ProtoBoundsAre(true, false));
  EXPECT_THAT((ModelSubset::Bounds{.lower = true, .upper = true}.Proto()),
              ProtoBoundsAre(true, true));
}

TEST(ModelSubsetBoundsTest, Streaming) {
  EXPECT_EQ(StreamToString(ModelSubset::Bounds()),
            "{lower: false, upper: false}");
  EXPECT_EQ((StreamToString(ModelSubset::Bounds{.lower = true, .upper = true})),
            "{lower: true, upper: true}");
}

struct SimpleModel {
  SimpleModel()
      : var(model.AddVariable("var")),
        lin(model.AddLinearConstraint("lin")),
        quad(model.AddQuadraticConstraint(
            BoundedQuadraticExpression{0.0, 0.0, 0.0}, "quad")),
        soc(model.AddSecondOrderConeConstraint({}, 0.0, "soc")),
        sos1(model.AddSos1Constraint({}, {}, "sos1")),
        sos2(model.AddSos2Constraint({}, {}, "sos2")),
        indicator(model.AddIndicatorConstraint(
            var, BoundedLinearExpression{0.0, 0.0, 0.0}, false, "indicator")) {}

  ModelSubset SimpleModelSubset() const {
    ModelSubset model_subset;
    {
      const ModelSubset::Bounds bounds{.lower = true, .upper = false};
      model_subset.variable_bounds[var] = bounds;
      model_subset.linear_constraints[lin] = bounds;
      model_subset.quadratic_constraints[quad] = bounds;
    }
    model_subset.variable_integrality.insert(var);
    model_subset.second_order_cone_constraints.insert(soc);
    model_subset.sos1_constraints.insert(sos1);
    model_subset.sos2_constraints.insert(sos2);
    model_subset.indicator_constraints.insert(indicator);
    return model_subset;
  }

  ModelSubsetProto SimpleModelSubsetProto() const {
    ModelSubsetProto proto;
    {
      ModelSubsetProto::Bounds bounds;
      bounds.set_lower(true);
      bounds.set_upper(false);
      (*proto.mutable_variable_bounds())[var.id()] = bounds;
      proto.add_variable_integrality(var.id());
      (*proto.mutable_linear_constraints())[lin.id()] = bounds;
      (*proto.mutable_quadratic_constraints())[quad.id()] = bounds;
    }
    proto.add_second_order_cone_constraints(soc.id());
    proto.add_sos1_constraints(sos1.id());
    proto.add_sos2_constraints(sos2.id());
    proto.add_indicator_constraints(indicator.id());
    return proto;
  }

  Model model;
  Variable var;
  LinearConstraint lin;
  QuadraticConstraint quad;
  SecondOrderConeConstraint soc;
  Sos1Constraint sos1;
  Sos2Constraint sos2;
  IndicatorConstraint indicator;
};

TEST(ModelSubsetTest, ProtoEmptyOk) {
  EXPECT_THAT(ModelSubset().Proto(), EqualsProto(ModelSubsetProto()));
}

TEST(ModelSubsetTest, ProtoSimpleOk) {
  const SimpleModel model;
  const ModelSubset model_subset = model.SimpleModelSubset();
  const ModelSubsetProto proto = model_subset.Proto();
  EXPECT_THAT(proto.variable_bounds(),
              ElementsAre(Pair(model.var.id(), ProtoBoundsAre(true, false))));
  EXPECT_THAT(proto.variable_integrality(), ElementsAre(model.var.id()));
  EXPECT_THAT(proto.linear_constraints(),
              ElementsAre(Pair(model.lin.id(), ProtoBoundsAre(true, false))));
  EXPECT_THAT(proto.quadratic_constraints(),
              ElementsAre(Pair(model.quad.id(), ProtoBoundsAre(true, false))));
  EXPECT_THAT(proto.second_order_cone_constraints(),
              ElementsAre(model.soc.id()));
  EXPECT_THAT(proto.sos1_constraints(), ElementsAre(model.sos1.id()));
  EXPECT_THAT(proto.sos2_constraints(), ElementsAre(model.sos2.id()));
  EXPECT_THAT(proto.indicator_constraints(), ElementsAre(model.indicator.id()));
}

TEST(ModelSubsetTest, FromProtoEmptyOk) {
  ModelStorage storage;
  ASSERT_OK_AND_ASSIGN(const ModelSubset subset,
                       ModelSubset::FromProto(&storage, ModelSubsetProto()));
  EXPECT_TRUE(subset.empty());
}

TEST(ModelSubsetTest, FromProtoSimpleOk) {
  const SimpleModel model;
  ASSERT_OK_AND_ASSIGN(const ModelSubset subset,
                       ModelSubset::FromProto(model.model.storage(),
                                              model.SimpleModelSubsetProto()));
  EXPECT_THAT(subset.variable_bounds,
              ElementsAre(Pair(model.var, ModelSubset::Bounds{
                                              .lower = true, .upper = false})));
  EXPECT_THAT(subset.variable_integrality, ElementsAre(model.var));
  EXPECT_THAT(subset.linear_constraints,
              ElementsAre(Pair(model.lin, ModelSubset::Bounds{
                                              .lower = true, .upper = false})));
  EXPECT_THAT(
      subset.quadratic_constraints,
      ElementsAre(Pair(model.quad,
                       ModelSubset::Bounds{.lower = true, .upper = false})));
  EXPECT_THAT(subset.sos1_constraints, ElementsAre(model.sos1));
  EXPECT_THAT(subset.sos2_constraints, ElementsAre(model.sos2));
  EXPECT_THAT(subset.indicator_constraints, ElementsAre(model.indicator));
}

TEST(ModelSubsetTest, FromProtoInvalidVariableBounds) {
  ModelStorage storage;
  ModelSubsetProto proto;
  (*proto.mutable_variable_bounds())[2] = ModelSubsetProto::Bounds();
  EXPECT_THAT(
      ModelSubset::FromProto(&storage, proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no variable")));
}

TEST(ModelSubsetTest, FromProtoInvalidVariableIntegrality) {
  ModelStorage storage;
  ModelSubsetProto proto;
  proto.add_variable_integrality(2);
  EXPECT_THAT(
      ModelSubset::FromProto(&storage, proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no variable")));
}

TEST(ModelSubsetTest, FromProtoInvalidLinearConstraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  (*proto.mutable_linear_constraints())[2] = ModelSubsetProto::Bounds();
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no linear constraint")));
}

TEST(ModelSubsetTest, FromProtoInvalidQuadraticConstraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  (*proto.mutable_quadratic_constraints())[2] = ModelSubsetProto::Bounds();
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no quadratic constraint")));
}

TEST(ModelSubsetTest, FromProtoInvalidSecondOrderConeConstraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  proto.add_second_order_cone_constraints(2);
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no second-order cone constraint")));
}

TEST(ModelSubsetTest, FromProtoInvalidSos1Constraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  proto.add_sos1_constraints(2);
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no SOS1 constraint")));
}

TEST(ModelSubsetTest, FromProtoInvalidSos2Constraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  proto.add_sos2_constraints(2);
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no SOS2 constraint")));
}

TEST(ModelSubsetTest, FromProtoInvalidIndicatorConstraint) {
  ModelStorage storage;
  ModelSubsetProto proto;
  proto.add_indicator_constraints(2);
  EXPECT_THAT(ModelSubset::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no indicator constraint")));
}

TEST(ModelSubsetTest, CheckModelStorageEmptyOk) {
  ModelStorage storage;
  EXPECT_OK(ModelSubset().CheckModelStorage(&storage));
}

TEST(ModelSubsetTest, CheckModelStorageSimpleOk) {
  SimpleModel model;
  EXPECT_OK(model.SimpleModelSubset().CheckModelStorage(model.model.storage()));
}

TEST(ModelSubsetTest, CheckModelStorageBadVariable) {
  const SimpleModel model;
  ModelSubset subset;
  subset.variable_bounds[model.var] = ModelSubset::Bounds{};
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable_bounds")));
}

TEST(ModelSubsetTest, CheckModelStorageBadVariableIntegrality) {
  const SimpleModel model;
  ModelSubset subset;
  subset.variable_integrality.insert(model.var);
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable_integrality")));
}

TEST(ModelSubsetTest, CheckModelStorageBadLinearConstraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.linear_constraints[model.lin] = ModelSubset::Bounds{};
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("linear_constraints")));
}

TEST(ModelSubsetTest, CheckModelStorageBadQuadraticConstraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.quadratic_constraints[model.quad] = ModelSubset::Bounds{};
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic_constraints")));
}

TEST(ModelSubsetTest, CheckModelStorageBadSecondOrderConeConstraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.second_order_cone_constraints.insert(model.soc);
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("second_order_cone_constraints")));
}

TEST(ModelSubsetTest, CheckModelStorageBadSos1Constraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.sos1_constraints.insert(model.sos1);
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos1_constraints")));
}

TEST(ModelSubsetTest, CheckModelStorageBadSos2Constraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.sos2_constraints.insert(model.sos2);
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos2_constraints")));
}

TEST(ModelSubsetTest, CheckModelStorageBadIndicatorConstraint) {
  const SimpleModel model;
  ModelSubset subset;
  subset.indicator_constraints.insert(model.indicator);
  const ModelStorage other;
  EXPECT_THAT(subset.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraints")));
}

TEST(ModelSubsetTest, EmptyEmptyOk) { EXPECT_TRUE(ModelSubset().empty()); }

TEST(ModelSubsetTest, EmptyWithVariableBounds) {
  ModelSubset subset;
  subset.variable_bounds[SimpleModel().var] = ModelSubset::Bounds{};
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithVariableIntegrality) {
  ModelSubset subset;
  subset.variable_integrality.insert(SimpleModel().var);
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithLinearConstraint) {
  ModelSubset subset;
  subset.linear_constraints[SimpleModel().lin] = ModelSubset::Bounds{};
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithQuadraticConstraint) {
  ModelSubset subset;
  subset.quadratic_constraints[SimpleModel().quad] = ModelSubset::Bounds{};
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithSocConstraint) {
  ModelSubset subset;
  subset.second_order_cone_constraints.insert(SimpleModel().soc);
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithSos1Constraint) {
  ModelSubset subset;
  subset.sos1_constraints.insert(SimpleModel().sos1);
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithSos2Constraint) {
  ModelSubset subset;
  subset.sos2_constraints.insert(SimpleModel().sos2);
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, EmptyWithIndicatorConstraint) {
  ModelSubset subset;
  subset.indicator_constraints.insert(SimpleModel().indicator);
  EXPECT_FALSE(subset.empty());
}

TEST(ModelSubsetTest, ToStringEmptySubset) {
  EXPECT_EQ(ModelSubset().ToString(),
            R"subset(Model Subset:
 Variable bounds:
 Variable integrality:
 Linear constraints:
)subset");
}

TEST(ModelSubsetTest, ToStringSimpleSubset) {
  EXPECT_EQ(SimpleModel().SimpleModelSubset().ToString(),
            R"subset(Model Subset:
 Variable bounds:
  var: var ≤ inf
 Variable integrality:
  var
 Linear constraints:
  lin: 0 ≤ inf
 Quadratic constraints:
  quad: 0 ≥ 0
 Second-order cone constraints:
  soc: ||{}||₂ ≤ 0
 SOS1 constraints:
  sos1: {} is SOS1
 SOS2 constraints:
  sos2: {} is SOS2
 Indicator constraints:
  indicator: var = 1 ⇒ 0 = 0
)subset");
}

TEST(ModelSubsetTest, ToStringBoundedConstraintWithEmptyBounds) {
  Model model;
  const Variable x = model.AddVariable("x");
  const ModelSubset subset{.variable_bounds = {{x, ModelSubset::Bounds{}}}};
  EXPECT_EQ(subset.ToString(),
            R"subset(Model Subset:
 Variable bounds:
 Variable integrality:
 Linear constraints:
)subset");
}

TEST(ModelSubsetTest, ToStringBoundedConstraintWithNonzeroExpressionOffset) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c =
      model.AddLinearConstraint(BoundedLinearExpression(x + 1, 2, 3), "c");
  const LinearConstraint d =
      model.AddLinearConstraint(BoundedLinearExpression(x + 4, 5, 6), "d");
  const ModelSubset subset{
      .linear_constraints = {
          {c, ModelSubset::Bounds{.lower = true, .upper = false}},
          {d, ModelSubset::Bounds{.lower = false, .upper = true}}}};
  EXPECT_EQ(subset.ToString(),
            R"subset(Model Subset:
 Variable bounds:
 Variable integrality:
 Linear constraints:
  c: x ≥ 1
  d: x ≤ 2
)subset");
}

TEST(ModelSubsetTest, Streaming) {
  EXPECT_EQ(StreamToString(ModelSubset()),
            "{variable_bounds: {}, variable_integrality: {}, "
            "linear_constraints: {}, quadratic_constraints: {}, "
            "second_order_cone_constraints: {}, sos1_constraints: {}, "
            "sos2_constraints: {}, indicator_constraints: {}}");
  EXPECT_EQ(
      StreamToString(SimpleModel().SimpleModelSubset()),
      "{variable_bounds: {{var, {lower: true, upper: false}}}, "
      "variable_integrality: {var}, linear_constraints: {{lin, {lower: true, "
      "upper: false}}}, quadratic_constraints: {{quad, {lower: true, upper: "
      "false}}}, second_order_cone_constraints: {soc}, sos1_constraints: "
      "{sos1}, sos2_constraints: {sos2}, indicator_constraints: {indicator}}");
  // We test only one "map-of-bounds" field (all use the same helper).
  {
    SimpleModel model;
    ModelSubset subset = model.SimpleModelSubset();
    const Variable x = model.model.AddVariable("x");
    subset.variable_bounds.insert({x, {false, true}});
    EXPECT_THAT(StreamToString(subset),
                HasSubstr("{variable_bounds: {{var, {lower: true, upper: "
                          "false}}, {x, {lower: false, upper: true}}}"));
  }
  // We test only one "set-of-repeated-IDs" field (all use the same helper).
  {
    SimpleModel model;
    ModelSubset subset = model.SimpleModelSubset();
    const Sos1Constraint c = model.model.AddSos1Constraint({}, {}, "c");
    subset.sos1_constraints.insert(c);
    EXPECT_THAT(StreamToString(subset),
                HasSubstr("sos1_constraints: {sos1, c}"));
  }
}

TEST(ComputeInfeasibleSubsystemResultTest, ProtoEmptyOk) {
  ComputeInfeasibleSubsystemResultProto expected;
  expected.set_feasibility(FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_THAT(ComputeInfeasibleSubsystemResult{}.Proto(),
              EqualsProto(expected));
}

TEST(ComputeInfeasibleSubsystemResultTest, ProtoSimpleOk) {
  const SimpleModel model;
  ComputeInfeasibleSubsystemResult result{
      .feasibility = FeasibilityStatus::kInfeasible,
      .infeasible_subsystem = model.SimpleModelSubset(),
      .is_minimal = true};
  ComputeInfeasibleSubsystemResultProto expected;
  expected.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
  expected.set_is_minimal(true);
  *expected.mutable_infeasible_subsystem() = model.SimpleModelSubsetProto();
  EXPECT_THAT(result.Proto(), EqualsProto(expected));
}

TEST(ComputeInfeasibleSubsystemResultTest, FromProtoTrivialOk) {
  ModelStorage storage;
  ComputeInfeasibleSubsystemResultProto result_proto;
  result_proto.set_feasibility(FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ComputeInfeasibleSubsystemResult::FromProto(&storage, result_proto),
      IsOkAndHolds(IsUndetermined()));
}

TEST(ComputeInfeasibleSubsystemResultTest, FromProtoSimpleOk) {
  const SimpleModel model;
  ComputeInfeasibleSubsystemResultProto result_proto;
  result_proto.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
  result_proto.set_is_minimal(true);
  *result_proto.mutable_infeasible_subsystem() = model.SimpleModelSubsetProto();
  ASSERT_THAT(ComputeInfeasibleSubsystemResult::FromProto(model.model.storage(),
                                                          result_proto),
              IsOkAndHolds(IsInfeasible(/*expected_is_minimal=*/true,
                                        model.SimpleModelSubset())));
}

// Integration test for ValidateInfeasibleSubsystemResultNoModel().
TEST(ComputeInfeasibleSubsystemResultTest, FromProtoInvalidNoModel) {
  ModelStorage storage;
  EXPECT_THAT(ComputeInfeasibleSubsystemResult::FromProto(
                  &storage, ComputeInfeasibleSubsystemResultProto{}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("feasibility must be specified")));
}

TEST(ComputeInfeasibleSubsystemResultTest,
     FromProtoInvalidInfeasibleSubsystem) {
  ModelStorage storage;
  ComputeInfeasibleSubsystemResultProto result_proto;
  result_proto.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
  (*result_proto.mutable_infeasible_subsystem()->mutable_variable_bounds())[0];
  EXPECT_THAT(
      ComputeInfeasibleSubsystemResult::FromProto(&storage, result_proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no variable")));
}

TEST(ComputeInfeasibleSubsystemResultTest, Streaming) {
  EXPECT_EQ(
      StreamToString(ComputeInfeasibleSubsystemResult()),
      "{feasibility: undetermined, infeasible_subsystem: {variable_bounds: {}, "
      "variable_integrality: {}, linear_constraints: {}, "
      "quadratic_constraints: {}, "
      "second_order_cone_constraints: {}, sos1_constraints: {}, "
      "sos2_constraints: {}, indicator_constraints: {}}, is_minimal: false}");
}

TEST(ComputeInfeasibleSubsystemResultTest, CheckModelStorage) {
  const SimpleModel model;
  ComputeInfeasibleSubsystemResult result;
  result.infeasible_subsystem.variable_bounds[model.var] =
      ModelSubset::Bounds{};
  const ModelStorage other;
  EXPECT_THAT(result.CheckModelStorage(&other),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable_bounds")));
}

}  // namespace
}  // namespace operations_research::math_opt
