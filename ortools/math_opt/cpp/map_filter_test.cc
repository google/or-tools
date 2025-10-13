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

#include "ortools/math_opt/cpp/map_filter.h"

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

TEST(MapFilterTest, VariableDefault) {
  const ModelStorage model;
  const MapFilter<Variable> filter;
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_EQ(filter.filtered_keys, std::nullopt);
  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(""));
}

TEST(MapFilterTest, LinearConstraintDefault) {
  const ModelStorage model;
  const MapFilter<LinearConstraint> filter;
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_EQ(filter.filtered_keys, std::nullopt);
  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(""));
}

TEST(MapFilterTest, SkipZeros) {
  const ModelStorage model;

  // Setting skip_zero_values to true.
  MapFilter<Variable> filter;
  filter.skip_zero_values = true;
  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(R"pb(skip_zero_values: true)pb"));

  // Setting skip_zero_values to false.
  filter.skip_zero_values = false;
  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(R"pb(skip_zero_values: false)pb"));
}

TEST(MapFilterTest, SetEmptyFilteredKeys) {
  const ModelStorage model;
  MapFilter<Variable> filter;
  filter.filtered_keys.emplace();

  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(R"pb(filter_by_ids: true)pb"));
}

TEST(MapFilterTest, SetFilteredKeysWithInitializerList) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const Variable c(&model, model.AddVariable("c"));

  MapFilter<Variable> filter;
  filter.filtered_keys = {a, c};

  EXPECT_OK(filter.CheckModelStorage(&model));
  SparseVectorFilterProto expected;
  expected.set_filter_by_ids(true);
  expected.add_filtered_ids(a.id());
  expected.add_filtered_ids(c.id());
  EXPECT_THAT(filter.Proto(), EquivToProto(expected));
}

TEST(MapFilterTest, SetFilteredKeysWithVector) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const Variable c(&model, model.AddVariable("c"));

  MapFilter<Variable> filter;
  const std::vector<Variable> vars = {a, c};
  filter.filtered_keys.emplace(vars.begin(), vars.end());

  EXPECT_OK(filter.CheckModelStorage(&model));
  SparseVectorFilterProto expected;
  expected.set_filter_by_ids(true);
  expected.add_filtered_ids(a.id());
  expected.add_filtered_ids(c.id());
  EXPECT_THAT(filter.Proto(), EquivToProto(expected));
}

TEST(MapFilterTest, CheckModelStorageWithMixedModels) {
  ModelStorage model_1;
  const Variable a(&model_1, model_1.AddVariable("a"));
  const Variable b(&model_1, model_1.AddVariable("b"));

  ModelStorage model_2;
  const Variable c(&model_2, model_2.AddVariable("c"));

  MapFilter<Variable> filter;
  const std::vector<Variable> vars = {a, c};
  filter.filtered_keys = {a, c};

  EXPECT_THAT(filter.CheckModelStorage(&model_1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(MapFilterTest, UnsetNonEmptyFilteredKeys) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const Variable c(&model, model.AddVariable("c"));

  MapFilter<Variable> filter;
  filter.filtered_keys = {a, b, c};
  filter.filtered_keys.reset();

  EXPECT_OK(filter.CheckModelStorage(&model));
  EXPECT_THAT(filter.Proto(), EquivToProto(""));
}

TEST(MakeSkipAllFilterTest, Variables) {
  const auto filter = MakeSkipAllFilter<Variable>();
  EXPECT_THAT(filter.Proto(), EquivToProto(R"pb(filter_by_ids: true)pb"));
}

TEST(MakeSkipZerosFilterTest, Variables) {
  const auto filter = MakeSkipZerosFilter<Variable>();
  EXPECT_THAT(filter.Proto(), EquivToProto(R"pb(skip_zero_values: true)pb"));
}

TEST(MakeKeepKeysFilterTest, Vector) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const Variable c(&model, model.AddVariable("c"));

  const std::vector<Variable> vars = {a, c};
  const auto filter = MakeKeepKeysFilter(vars);

  SparseVectorFilterProto expected;
  expected.set_filter_by_ids(true);
  expected.add_filtered_ids(a.id());
  expected.add_filtered_ids(c.id());

  EXPECT_THAT(filter.Proto(), EquivToProto(expected));
}

TEST(MakeKeepKeysFilterTest, InitializerList) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const Variable c(&model, model.AddVariable("c"));

  const auto filter = MakeKeepKeysFilter({a, c});

  SparseVectorFilterProto expected;
  expected.set_filter_by_ids(true);
  expected.add_filtered_ids(a.id());
  expected.add_filtered_ids(c.id());

  EXPECT_THAT(filter.Proto(), EquivToProto(expected));
}

TEST(VariableFilterFromProtoTest, EmptyFilter) {
  Model model;
  SparseVectorFilterProto proto;
  ASSERT_OK_AND_ASSIGN(MapFilter<Variable> filter,
                       VariableFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_EQ(filter.filtered_keys, std::nullopt);
}

TEST(VariableFilterFromProtoTest, NonemptyFilter) {
  Model model;
  Variable x = model.AddBinaryVariable();
  model.AddBinaryVariable();
  Variable z = model.AddBinaryVariable();
  SparseVectorFilterProto proto;
  proto.set_skip_zero_values(true);
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(0);
  proto.add_filtered_ids(2);
  ASSERT_OK_AND_ASSIGN(MapFilter<Variable> filter,
                       VariableFilterFromProto(model, proto));
  EXPECT_TRUE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(UnorderedElementsAre(x, z)));
}

TEST(VariableFilterFromProtoTest, FilterByIdsEmpty) {
  Model model;
  model.AddBinaryVariable();
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  ASSERT_OK_AND_ASSIGN(MapFilter<Variable> filter,
                       VariableFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(IsEmpty()));
}

TEST(VariableFilterFromProtoTest, BadFilterIdFails) {
  Model model;
  model.AddBinaryVariable();
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(3);
  EXPECT_THAT(VariableFilterFromProto(model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("3")));
}

TEST(LinearConstraintFilterFromProtoTest, EmptyFilter) {
  Model model;
  SparseVectorFilterProto proto;
  ASSERT_OK_AND_ASSIGN(MapFilter<LinearConstraint> filter,
                       LinearConstraintFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_EQ(filter.filtered_keys, std::nullopt);
}

TEST(LinearConstraintFilterFromProtoTest, NonemptyFilter) {
  Model model;
  LinearConstraint c = model.AddLinearConstraint();
  model.AddLinearConstraint();
  LinearConstraint e = model.AddLinearConstraint();
  SparseVectorFilterProto proto;
  proto.set_skip_zero_values(true);
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(0);
  proto.add_filtered_ids(2);
  ASSERT_OK_AND_ASSIGN(MapFilter<LinearConstraint> filter,
                       LinearConstraintFilterFromProto(model, proto));
  EXPECT_TRUE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(UnorderedElementsAre(c, e)));
}

TEST(LinearConstraintFilterFromProtoTest, FilterByIdsEmpty) {
  Model model;
  model.AddLinearConstraint();
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  ASSERT_OK_AND_ASSIGN(MapFilter<LinearConstraint> filter,
                       LinearConstraintFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(IsEmpty()));
}

TEST(LinearConstraintFilterFromProtoTest, BadFilterIdFails) {
  Model model;
  model.AddLinearConstraint();
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(3);
  EXPECT_THAT(LinearConstraintFilterFromProto(model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("3")));
}

TEST(QuadraticConstraintFilterFromProtoTest, EmptyFilter) {
  Model model;
  SparseVectorFilterProto proto;
  ASSERT_OK_AND_ASSIGN(MapFilter<QuadraticConstraint> filter,
                       QuadraticConstraintFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_EQ(filter.filtered_keys, std::nullopt);
}

TEST(QuadraticConstraintFilterFromProtoTest, NonemptyFilter) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  QuadraticConstraint c = model.AddQuadraticConstraint(x * x <= 0.0);
  model.AddQuadraticConstraint(x * x <= 0.0);
  QuadraticConstraint e = model.AddQuadraticConstraint(x * x <= 0.0);
  SparseVectorFilterProto proto;
  proto.set_skip_zero_values(true);
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(0);
  proto.add_filtered_ids(2);
  ASSERT_OK_AND_ASSIGN(MapFilter<QuadraticConstraint> filter,
                       QuadraticConstraintFilterFromProto(model, proto));
  EXPECT_TRUE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(UnorderedElementsAre(c, e)));
}

TEST(QuadraticConstraintFilterFromProtoTest, FilterByIdsEmpty) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.AddQuadraticConstraint(x * x <= 0.0);
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  ASSERT_OK_AND_ASSIGN(MapFilter<QuadraticConstraint> filter,
                       QuadraticConstraintFilterFromProto(model, proto));
  EXPECT_FALSE(filter.skip_zero_values);
  EXPECT_THAT(filter.filtered_keys, Optional(IsEmpty()));
}

TEST(QuadraticConstraintFilterFromProtoTest, BadFilterIdFails) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.AddQuadraticConstraint(x * x <= 0.0);
  SparseVectorFilterProto proto;
  proto.set_filter_by_ids(true);
  proto.add_filtered_ids(3);
  EXPECT_THAT(QuadraticConstraintFilterFromProto(model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("3")));
}

}  // namespace
}  // namespace operations_research::math_opt
