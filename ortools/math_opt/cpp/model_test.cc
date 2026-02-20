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

// Unit tests for math_opt.h on model reading and construction. The underlying
// solver is not invoked. For tests that run Solve(), see
// ortools/math_opt/solver_tests/*.

#include "ortools/math_opt/cpp/model.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/update_tracker.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/testing/stream.h"
#include "ortools/util/fp_roundtrip_conv_testing.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

//   max 2.0 * y + 3.5
//   s.t. x + y - 1 <= 0.5 (c)
//          2.0 * y >= 0.5 (d)
//            x unbounded
//            y in {0, 1}
class ModelingTest : public testing::Test {
 protected:
  ModelingTest()
      : model_("math_opt_model"),
        x_(model_.AddVariable("x")),
        y_(model_.AddBinaryVariable("y")),
        c_(model_.AddLinearConstraint(x_ + y_ - 1.0 <= 0.5, "c")),
        d_(model_.AddLinearConstraint(2.0 * y_ >= 0.5, "d")) {
    model_.Maximize(2.0 * y_ + 3.5);
  }

  Model model_;
  const Variable x_;
  const Variable y_;
  const LinearConstraint c_;
  const LinearConstraint d_;
};

TEST(ModelTest, FromValidModelProto) {
  // Here we assume Model::FromModelProto() uses ModelStorage::FromModelProto()
  // and thus we don't test everything.
  ModelProto model_proto;
  model_proto.set_name("model");
  const VariableId x_id(1);
  model_proto.mutable_variables()->add_ids(x_id.value());
  model_proto.mutable_variables()->add_lower_bounds(0.0);
  model_proto.mutable_variables()->add_upper_bounds(1.0);
  model_proto.mutable_variables()->add_integers(false);
  model_proto.mutable_variables()->add_names("x");

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<Model> model,
                       Model::FromModelProto(model_proto));
  EXPECT_THAT(model->ExportModel(), EquivToProto(model_proto));
  ASSERT_EQ(model->num_variables(), 1);
  EXPECT_EQ(model->Variables().front().typed_id(), x_id);
}

TEST(ModelTest, FromInvalidModelProto) {
  // Here we assume Model::FromModelProto() uses ValidateModel() via
  // ModelStorage::FromModelProto() and thus we don't test all possible errors.
  ModelProto model_proto;
  model_proto.set_name("model");
  model_proto.mutable_variables()->add_ids(1);
  // Missing lower_bounds entry.
  model_proto.mutable_variables()->add_upper_bounds(1.0);
  model_proto.mutable_variables()->add_integers(false);
  model_proto.mutable_variables()->add_names("x");

  EXPECT_THAT(
      Model::FromModelProto(model_proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower_bounds")));
}

TEST(ModelTest, FromStorage) {
  // In this test, we only test adding one variable. We assume here that the
  // constructor will move the provided storage in-place. Thus it is not
  // necessary to over-test this feature.
  auto storage = std::make_unique<ModelStorage>("test");

  // Here we directly delete variables since the ModelStorage won't reuse an id
  // already returned. We don't bother giving names or bounds to these
  // variables.
  storage->DeleteVariable(storage->AddVariable());
  const VariableId x_id = storage->AddVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/1.0, /*is_integer=*/true, "x");

  const Model model(std::move(storage));

  const std::vector<Variable> variables = model.Variables();
  ASSERT_EQ(variables.size(), 1);
  const Variable x = variables[0];
  EXPECT_EQ(x.typed_id(), x_id);
  EXPECT_EQ(x.name(), "x");
  EXPECT_EQ(x.lower_bound(), 0.0);
  EXPECT_EQ(x.upper_bound(), 1.0);
}

// We can't use Pointwise(Property(...)) matchers, hence here we have this
// function to extract the typed ids to use UnorderedElementsAre().
template <typename T>
std::vector<typename T::IdType> TypedIds(const std::vector<T>& v) {
  std::vector<typename T::IdType> ids;
  for (const T& e : v) {
    ids.push_back(e.typed_id());
  }
  return ids;
}

TEST_F(ModelingTest, Clone) {
  const Model& const_ref = model_;

  {
    const std::unique_ptr<Model> clone = const_ref.Clone();

    EXPECT_THAT(clone->ExportModel(), EquivToProto(model_.ExportModel()));

    EXPECT_THAT(TypedIds(clone->SortedVariables()),
                ElementsAreArray(TypedIds(model_.SortedVariables())));
    EXPECT_THAT(TypedIds(clone->SortedLinearConstraints()),
                ElementsAreArray(TypedIds(model_.SortedLinearConstraints())));
  }

  // Redo the test after removing the first variable and a new variable that we
  // just added. This should shift the new variables's IDs by one.
  {
    model_.DeleteVariable(x_);
    model_.DeleteVariable(model_.AddVariable());

    // Same with constraints.
    model_.DeleteLinearConstraint(c_);
    model_.DeleteLinearConstraint(model_.AddLinearConstraint());

    const std::unique_ptr<Model> clone = const_ref.Clone();

    EXPECT_THAT(clone->ExportModel(), EquivToProto(model_.ExportModel()));

    EXPECT_THAT(TypedIds(clone->SortedVariables()),
                ElementsAreArray(TypedIds(model_.SortedVariables())));
    EXPECT_THAT(TypedIds(clone->SortedLinearConstraints()),
                ElementsAreArray(TypedIds(model_.SortedLinearConstraints())));

    // New variables and constraints should start with the same id.
    EXPECT_EQ(clone->AddVariable().typed_id(), model_.AddVariable().typed_id());
    EXPECT_EQ(clone->AddLinearConstraint().typed_id(),
              model_.AddLinearConstraint().typed_id());
  }

  // Test renaming.
  {
    const std::unique_ptr<Model> clone = const_ref.Clone("new_name");

    ModelProto expected_proto = model_.ExportModel();
    expected_proto.set_name("new_name");
    EXPECT_THAT(clone->ExportModel(), EquivToProto(expected_proto));

    EXPECT_THAT(TypedIds(clone->SortedVariables()),
                ElementsAreArray(TypedIds(model_.SortedVariables())));
    EXPECT_THAT(TypedIds(clone->SortedLinearConstraints()),
                ElementsAreArray(TypedIds(model_.SortedLinearConstraints())));
  }
}

TEST(ModelTest, ApplyValidUpdateProto) {
  // Here we assume Model::ApplyUpdateProto() uses
  // ModelStorage::ApplyUpdateProto() and thus we don't test everything.
  ModelProto model_proto;
  model_proto.set_name("model");
  const VariableId x_id(1);
  model_proto.mutable_variables()->add_ids(x_id.value());
  model_proto.mutable_variables()->add_lower_bounds(0.0);
  model_proto.mutable_variables()->add_upper_bounds(1.0);
  model_proto.mutable_variables()->add_integers(false);
  model_proto.mutable_variables()->add_names("x");

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<Model> model,
                       Model::FromModelProto(model_proto));
  EXPECT_THAT(model->ExportModel(), EquivToProto(model_proto));

  ModelUpdateProto update_proto;
  update_proto.mutable_variable_updates()->mutable_lower_bounds()->add_ids(
      x_id.value());
  update_proto.mutable_variable_updates()->mutable_lower_bounds()->add_values(
      -3.0);
  ASSERT_OK(model->ApplyUpdateProto(update_proto));

  model_proto.mutable_variables()->mutable_lower_bounds()->Set(0, -3.0);
  EXPECT_THAT(model->ExportModel(), EquivToProto(model_proto));
}

TEST(ModelTest, ApplyInvalidUpdateProto) {
  // Here we assume Model::ApplyUpdateProto() uses
  // ModelStorage::ApplyUpdateProto() and thus we don't test everything.
  ModelProto model_proto;
  model_proto.set_name("model");
  const VariableId x_id(1);
  model_proto.mutable_variables()->add_ids(x_id.value());
  model_proto.mutable_variables()->add_lower_bounds(0.0);
  model_proto.mutable_variables()->add_upper_bounds(1.0);
  model_proto.mutable_variables()->add_integers(false);
  model_proto.mutable_variables()->add_names("x");

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<Model> model,
                       Model::FromModelProto(model_proto));
  EXPECT_THAT(model->ExportModel(), EquivToProto(model_proto));

  ModelUpdateProto update_proto;
  // Id 0 does not exist.
  update_proto.mutable_variable_updates()->mutable_lower_bounds()->add_ids(0);
  update_proto.mutable_variable_updates()->mutable_lower_bounds()->add_values(
      -3.0);
  EXPECT_THAT(model->ApplyUpdateProto(update_proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid variable id")));
}

TEST(ModelTest, VariableGetters) {
  Model model;
  const Model& const_model = model;
  {
    const Variable v =
        model.AddVariable(/*lower_bound=*/-kInf, /*upper_bound=*/kInf,
                          /*is_integer=*/false, "continuous");
    EXPECT_EQ(const_model.name(v), "continuous");
    EXPECT_EQ(const_model.lower_bound(v), -kInf);
    EXPECT_EQ(const_model.upper_bound(v), kInf);
    EXPECT_FALSE(const_model.is_integer(v));
  }
  {
    const Variable v =
        model.AddVariable(/*lower_bound=*/3.0, /*upper_bound=*/5.0,
                          /*is_integer=*/true, "integer");
    EXPECT_EQ(const_model.name(v), "integer");
    EXPECT_EQ(const_model.lower_bound(v), 3.0);
    EXPECT_EQ(const_model.upper_bound(v), 5.0);
    EXPECT_TRUE(const_model.is_integer(v));
  }
}

TEST(ModelTest, VariableSetters) {
  Model model;
  const Model& const_model = model;
  const Variable v =
      model.AddVariable(/*lower_bound=*/-kInf, /*upper_bound=*/kInf,
                        /*is_integer=*/false, "v");

  model.set_lower_bound(v, 3.0);
  model.set_upper_bound(v, 5.0);
  model.set_is_integer(v, true);

  EXPECT_EQ(const_model.lower_bound(v), 3.0);
  EXPECT_EQ(const_model.upper_bound(v), 5.0);
  EXPECT_TRUE(const_model.is_integer(v));

  model.set_continuous(v);
  EXPECT_FALSE(const_model.is_integer(v));

  model.set_integer(v);
  EXPECT_TRUE(const_model.is_integer(v));
}

TEST(ModelTest, VariableById) {
  Model model;
  const Variable x0 = model.AddBinaryVariable("x0");
  const Variable x1 = model.AddBinaryVariable("x1");
  const Variable x2 = model.AddContinuousVariable(-1.0, 2.0, "x2");
  model.DeleteVariable(x1);
  EXPECT_TRUE(model.has_variable(x0.id()));
  EXPECT_FALSE(model.has_variable(x1.id()));
  EXPECT_TRUE(model.has_variable(x2.id()));
  EXPECT_EQ(model.variable(x0.id()).name(), "x0");
  EXPECT_EQ(model.variable(x0.id()).lower_bound(), 0.0);
  EXPECT_EQ(model.variable(x0.id()).upper_bound(), 1.0);
  EXPECT_EQ(model.variable(x2.id()).name(), "x2");
  EXPECT_EQ(model.variable(x2.id()).lower_bound(), -1.0);
  EXPECT_EQ(model.variable(x2.id()).upper_bound(), 2.0);

  EXPECT_TRUE(model.has_variable(x0.typed_id()));
  EXPECT_FALSE(model.has_variable(x1.typed_id()));
  EXPECT_TRUE(model.has_variable(x2.typed_id()));
  EXPECT_EQ(model.variable(x0.typed_id()).name(), "x0");
  EXPECT_EQ(model.variable(x2.typed_id()).name(), "x2");
}

TEST(ModelTest, ValidateExistingVariableOfThisModel) {
  Model model_a;
  const Variable x0 = model_a.AddBinaryVariable("x0");
  const Variable x1 = model_a.AddBinaryVariable("x1");
  model_a.DeleteVariable(x0);

  Model model_b("b");

  EXPECT_OK(model_a.ValidateExistingVariableOfThisModel(x1));
  EXPECT_THAT(
      model_a.ValidateExistingVariableOfThisModel(x0),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("not found")));
  EXPECT_THAT(model_b.ValidateExistingVariableOfThisModel(x1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("different model")));
}

TEST(ModelDeathTest, VariableByIdOutOfBounds) {
  Model model;
  model.AddBinaryVariable("x0");
  EXPECT_DEATH(model.variable(-1),
               AllOf(HasSubstr("variable"), HasSubstr("-1")));
  EXPECT_DEATH(model.variable(2), AllOf(HasSubstr("variable"), HasSubstr("2")));
}

TEST(ModelDeathTest, VariableByIdDeleted) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  EXPECT_EQ(model.variable(x.id()).name(), "x");
  model.DeleteVariable(x);
  EXPECT_DEATH(model.variable(x.id()),
               AllOf(HasSubstr("variable"), HasSubstr("0")));
}

// Old versions of `EXPECT_DEATH` don't support `absl::string_view`.
// `absl::string_view::str()` does not guarantee null terminated string so we
// create a copy for the purpose of the test.
#define EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(expression) \
  EXPECT_DEATH(expression, std::string(internal::kObjectsFromOtherModelStorage))

TEST(ModelDeathTest, VariableAccessorsInvalidModel) {
  Model model_a("a");
  const Variable a_a = model_a.AddVariable("a_a");

  Model model_b("b");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.name(a_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.lower_bound(a_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.upper_bound(a_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.is_integer(a_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_lower_bound(a_a, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_upper_bound(a_a, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_is_integer(a_a, true));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_continuous(a_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_integer(a_a));
}

TEST(ModelTest, LinearConstraintGetters) {
  Model model;
  const Model& const_model = model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");
  {
    const LinearConstraint c = model.AddLinearConstraint(
        /*lower_bound=*/-kInf, /*upper_bound=*/1.5, "upper_bounded");
    model.set_coefficient(c, x, 1.0);
    model.set_coefficient(c, y, 2.0);

    EXPECT_EQ(const_model.name(c), "upper_bounded");
    EXPECT_EQ(const_model.lower_bound(c), -kInf);
    EXPECT_EQ(const_model.upper_bound(c), 1.5);

    EXPECT_EQ(const_model.coefficient(c, x), 1.0);
    EXPECT_EQ(const_model.coefficient(c, y), 2.0);

    EXPECT_TRUE(const_model.is_coefficient_nonzero(c, x));
    EXPECT_TRUE(const_model.is_coefficient_nonzero(c, y));
    EXPECT_FALSE(const_model.is_coefficient_nonzero(c, z));

    EXPECT_THAT(model.RowNonzeros(c), UnorderedElementsAre(x, y));

    const BoundedLinearExpression c_bounded_expr =
        c.AsBoundedLinearExpression();
    // TODO(b/171883688): we should use expression matchers.
    EXPECT_EQ(c_bounded_expr.lower_bound, -kInf);
    EXPECT_EQ(c_bounded_expr.upper_bound, 1.5);
    EXPECT_THAT(c_bounded_expr.expression.terms(),
                UnorderedElementsAre(Pair(x, 1.0), Pair(y, 2.0)));
  }
  {
    const LinearConstraint c = model.AddLinearConstraint(
        /*lower_bound=*/0.5, /*upper_bound=*/kInf, "lower_bounded");
    model.set_coefficient(c, y, 2.0);

    EXPECT_EQ(const_model.name(c), "lower_bounded");
    EXPECT_EQ(const_model.lower_bound(c), 0.5);
    EXPECT_EQ(const_model.upper_bound(c), kInf);

    EXPECT_EQ(const_model.coefficient(c, x), 0.0);
    EXPECT_EQ(const_model.coefficient(c, y), 2.0);

    EXPECT_FALSE(const_model.is_coefficient_nonzero(c, x));
    EXPECT_TRUE(const_model.is_coefficient_nonzero(c, y));

    EXPECT_THAT(model.RowNonzeros(c), UnorderedElementsAre(y));

    const BoundedLinearExpression c_bounded_expr =
        c.AsBoundedLinearExpression();
    // TODO(b/171883688): we should use expression matchers.
    EXPECT_EQ(c_bounded_expr.lower_bound, 0.5);
    EXPECT_EQ(c_bounded_expr.upper_bound, kInf);
    EXPECT_THAT(c_bounded_expr.expression.terms(),
                UnorderedElementsAre(Pair(y, 2.0)));
  }
}

TEST(ModelTest, LinearConstraintSetters) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Model& const_model = model;
  const LinearConstraint c = model.AddLinearConstraint("c");
  model.set_coefficient(c, x, 1.0);

  model.set_coefficient(c, x, 2.0);
  model.set_lower_bound(c, 3.0);
  model.set_upper_bound(c, 5.0);

  EXPECT_EQ(model.coefficient(c, x), 2.0);
  EXPECT_EQ(const_model.lower_bound(c), 3.0);
  EXPECT_EQ(const_model.upper_bound(c), 5.0);
}

TEST(ModelTest, LinearConstraintById) {
  Model model;
  const LinearConstraint c0 = model.AddLinearConstraint("c0");
  const LinearConstraint c1 = model.AddLinearConstraint("c1");
  const LinearConstraint c2 = model.AddLinearConstraint("c2");
  model.DeleteLinearConstraint(c1);
  EXPECT_TRUE(model.has_linear_constraint(c0.id()));
  EXPECT_FALSE(model.has_linear_constraint(c1.id()));
  EXPECT_TRUE(model.has_linear_constraint(c2.id()));
  EXPECT_EQ(model.linear_constraint(c0.id()).name(), "c0");
  EXPECT_EQ(model.linear_constraint(c2.id()).name(), "c2");

  EXPECT_TRUE(model.has_linear_constraint(c0.typed_id()));
  EXPECT_FALSE(model.has_linear_constraint(c1.typed_id()));
  EXPECT_TRUE(model.has_linear_constraint(c2.typed_id()));
  EXPECT_EQ(model.linear_constraint(c0.typed_id()).name(), "c0");
  EXPECT_EQ(model.linear_constraint(c2.typed_id()).name(), "c2");
}

TEST(ModelTest, ValidateExistingLinearConstraintOfThisModel) {
  Model model_a;
  const LinearConstraint c0 = model_a.AddLinearConstraint("c0");
  const LinearConstraint c1 = model_a.AddLinearConstraint("c1");
  model_a.DeleteLinearConstraint(c0);

  Model model_b("b");

  EXPECT_OK(model_a.ValidateExistingLinearConstraintOfThisModel(c1));
  EXPECT_THAT(
      model_a.ValidateExistingLinearConstraintOfThisModel(c0),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("not found")));
  EXPECT_THAT(model_b.ValidateExistingLinearConstraintOfThisModel(c1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("different model")));
}

TEST(ModelDeathTest, LinearConstraintByIdOutOfBounds) {
  Model model;
  model.AddLinearConstraint("c");
  EXPECT_DEATH(model.linear_constraint(-1),
               AllOf(HasSubstr("linear constraint"), HasSubstr("-1")));
  EXPECT_DEATH(model.linear_constraint(2),
               AllOf(HasSubstr("linear constraint"), HasSubstr("2")));
}

TEST(ModelDeathTest, LinearConstraintByIdDeleted) {
  Model model;
  const LinearConstraint c = model.AddLinearConstraint("c");
  EXPECT_EQ(model.linear_constraint(c.id()).name(), "c");
  model.DeleteLinearConstraint(c);
  EXPECT_DEATH(model.linear_constraint(c.id()),
               AllOf(HasSubstr("linear constraint"), HasSubstr("0")));
}

TEST(ModelDeathTest, LinearConstraintAccessorsInvalidModel) {
  Model model_a("a");
  const Variable x_a = model_a.AddVariable("x_a");
  const LinearConstraint c_a = model_a.AddLinearConstraint("c_a");

  Model model_b("b");
  const Variable x_b = model_b.AddVariable("x_b");
  const LinearConstraint c_b = model_b.AddLinearConstraint("c_b");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.name(c_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.lower_bound(c_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.upper_bound(c_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_lower_bound(c_a, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.set_upper_bound(c_a, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_b.set_coefficient(c_a, x_b, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_b.set_coefficient(c_b, x_a, 0.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.coefficient(c_a, x_b));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.coefficient(c_b, x_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_b.is_coefficient_nonzero(c_a, x_b));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_b.is_coefficient_nonzero(c_b, x_a));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.RowNonzeros(c_a));
}

TEST_F(ModelingTest, ModelProperties) {
  EXPECT_EQ(model_.name(), "math_opt_model");
  EXPECT_EQ(model_.num_variables(), 2);
  EXPECT_EQ(model_.next_variable_id(), 2);
  EXPECT_TRUE(model_.has_variable(0));
  EXPECT_TRUE(model_.has_variable(1));
  EXPECT_FALSE(model_.has_variable(2));
  EXPECT_FALSE(model_.has_variable(3));
  EXPECT_FALSE(model_.has_variable(-1));
  EXPECT_THAT(model_.Variables(), UnorderedElementsAre(x_, y_));
  EXPECT_THAT(model_.SortedVariables(), ElementsAre(x_, y_));

  EXPECT_EQ(model_.num_linear_constraints(), 2);
  EXPECT_EQ(model_.next_linear_constraint_id(), 2);
  EXPECT_TRUE(model_.has_linear_constraint(0));
  EXPECT_TRUE(model_.has_linear_constraint(1));
  EXPECT_FALSE(model_.has_linear_constraint(2));
  EXPECT_FALSE(model_.has_linear_constraint(3));
  EXPECT_FALSE(model_.has_linear_constraint(-1));
  EXPECT_THAT(model_.LinearConstraints(), UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedLinearConstraints(), ElementsAre(c_, d_));
}

TEST(ModelTest, ColumnNonzeros) {
  Model model("math_opt_model");
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");
  const LinearConstraint c1 = model.AddLinearConstraint(x + y <= 2);
  const LinearConstraint c2 = model.AddLinearConstraint(x <= 1);
  const LinearConstraint c3 = model.AddLinearConstraint(x + y <= 2);
  model.DeleteLinearConstraint(c3);

  EXPECT_THAT(model.ColumnNonzeros(x), UnorderedElementsAre(c1, c2));
  EXPECT_THAT(model.ColumnNonzeros(y), UnorderedElementsAre(c1));
  EXPECT_THAT(model.ColumnNonzeros(z), IsEmpty());
}

template <typename T>
std::vector<std::string> SortedValueNames(
    const google::protobuf::Map<int64_t, T>& messages) {
  std::vector<std::pair<int64_t, std::string>> sorted_entries;
  for (const auto& [id, message] : messages) {
    sorted_entries.push_back({id, message.name()});
  }
  absl::c_sort(sorted_entries);
  std::vector<std::string> result;
  for (const auto& [id, name] : sorted_entries) {
    result.push_back(name);
  }
  return result;
}

TEST(ModelTest, ExportModel_RemoveNames) {
  Model model("my_model");
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(x);
  const Objective b = model.AddAuxiliaryObjective(1, "objB");
  model.set_objective_offset(b, 2.0);
  model.AddLinearConstraint(x <= 1.0, "lin_con");
  model.AddQuadraticConstraint(x * x <= 1.0, "quad_con");
  model.AddIndicatorConstraint(y, x >= 3.0, /*activate_on_zero=*/false,
                               "ind_con");
  model.AddSos1Constraint({y, 1.0 - y}, {1.0, 1.0}, "sos1");
  model.AddSos2Constraint({y, 1.0 - y}, {1.0, 1.0}, "sos2");
  model.AddSecondOrderConeConstraint({x + y}, 1.0, "soc");
  {
    ModelProto named_proto = model.ExportModel(/*remove_names=*/false);
    EXPECT_EQ(named_proto.name(), "my_model");
    EXPECT_THAT(named_proto.variables().names(), ElementsAre("x", "y"));
    EXPECT_THAT(SortedValueNames(named_proto.auxiliary_objectives()),
                ElementsAre("objB"));
    EXPECT_THAT(named_proto.linear_constraints().names(),
                ElementsAre("lin_con"));
    EXPECT_THAT(SortedValueNames(named_proto.quadratic_constraints()),
                ElementsAre("quad_con"));
    EXPECT_THAT(SortedValueNames(named_proto.indicator_constraints()),
                ElementsAre("ind_con"));
    EXPECT_THAT(SortedValueNames(named_proto.sos1_constraints()),
                ElementsAre("sos1"));
    EXPECT_THAT(SortedValueNames(named_proto.sos2_constraints()),
                ElementsAre("sos2"));
    EXPECT_THAT(SortedValueNames(named_proto.second_order_cone_constraints()),
                ElementsAre("soc"));
  }

  {
    ModelProto unnamed_proto = model.ExportModel(/*remove_names=*/true);
    EXPECT_EQ(unnamed_proto.name(), "");
    EXPECT_THAT(unnamed_proto.variables().names(), IsEmpty());
    EXPECT_THAT(SortedValueNames(unnamed_proto.auxiliary_objectives()),
                ElementsAre(""));
    EXPECT_THAT(unnamed_proto.linear_constraints().names(), IsEmpty());
    EXPECT_THAT(SortedValueNames(unnamed_proto.quadratic_constraints()),
                ElementsAre(""));
    EXPECT_THAT(SortedValueNames(unnamed_proto.indicator_constraints()),
                ElementsAre(""));
    EXPECT_THAT(SortedValueNames(unnamed_proto.sos1_constraints()),
                ElementsAre(""));
    EXPECT_THAT(SortedValueNames(unnamed_proto.sos2_constraints()),
                ElementsAre(""));
    EXPECT_THAT(SortedValueNames(unnamed_proto.second_order_cone_constraints()),
                ElementsAre(""));
  }
}

TEST(ModelDeathTest, ColumnNonzerosOtherModel) {
  Model model_a("a");
  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.ColumnNonzeros(b_x));
}

TEST(ModelDeathTest, RowNonzerosOtherModel) {
  Model model_a("a");
  Model model_b("b");
  const LinearConstraint b_c = model_b.AddLinearConstraint("c");
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.RowNonzeros(b_c));
}

TEST_F(ModelingTest, DeleteVariable) {
  model_.DeleteVariable(x_);
  EXPECT_EQ(model_.num_variables(), 1);
  EXPECT_EQ(model_.next_variable_id(), 2);
  EXPECT_FALSE(model_.has_variable(0));
  EXPECT_TRUE(model_.has_variable(1));
  EXPECT_THAT(model_.Variables(), UnorderedElementsAre(y_));
  EXPECT_THAT(model_.RowNonzeros(c_), UnorderedElementsAre(y_));
  const BoundedLinearExpression c_bounded_expr = c_.AsBoundedLinearExpression();
  // TODO(b/171883688): we should use expression matchers.
  EXPECT_DOUBLE_EQ(c_bounded_expr.lower_bound, -kInf);
  EXPECT_DOUBLE_EQ(c_bounded_expr.upper_bound, 1.5);
  EXPECT_THAT(c_bounded_expr.expression.terms(),
              UnorderedElementsAre(Pair(y_, 1.0)));
}

TEST_F(ModelingTest, DeleteLinearConstraint) {
  model_.DeleteLinearConstraint(c_);
  EXPECT_EQ(model_.num_linear_constraints(), 1);
  EXPECT_EQ(model_.next_linear_constraint_id(), 2);
  EXPECT_FALSE(model_.has_linear_constraint(0));
  EXPECT_TRUE(model_.has_linear_constraint(1));
  EXPECT_THAT(model_.LinearConstraints(), UnorderedElementsAre(d_));
}

TEST_F(ModelingTest, ExportModel) {
  ASSERT_OK_AND_ASSIGN(const ModelProto expected,
                       ParseTextProto<ModelProto>(R"pb(
                         name: "math_opt_model"
                         variables {
                           ids: [ 0, 1 ]
                           lower_bounds: [ -inf, 0.0 ]
                           upper_bounds: [ inf, 1.0 ]
                           integers: [ false, true ]
                           names: [ "x", "y" ]
                         }
                         objective {
                           offset: 3.5
                           maximize: true
                           linear_coefficients: {
                             ids: [ 1 ]
                             values: [ 2.0 ]
                           }
                         }
                         linear_constraints {
                           ids: [ 0, 1 ]
                           lower_bounds: [ -inf, 0.5 ]
                           upper_bounds: [ 1.5, inf ]
                           names: [ "c", "d" ]
                         }
                         linear_constraint_matrix {
                           row_ids: [ 0, 0, 1 ]
                           column_ids: [ 0, 1, 1 ]
                           coefficients: [ 1.0, 1.0, 2.0 ]
                         }
                       )pb"));
  EXPECT_THAT(model_.ExportModel(), EquivToProto(expected));
}

TEST(ModelTest, AddBoundedLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c =
      model.AddLinearConstraint(3 <= 2 * x - y + 2 <= 5, "c");
  EXPECT_EQ(c.coefficient(x), 2.0);
  EXPECT_EQ(c.coefficient(y), -1.0);
  EXPECT_EQ(c.lower_bound(), 3.0 - 2.0);
  EXPECT_EQ(c.upper_bound(), 5.0 - 2.0);
}

TEST(ModelTest, AddEqualityLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c = model.AddLinearConstraint(2 * x - 5 == x + y, "c");
  EXPECT_EQ(c.coefficient(x), 1.0);
  EXPECT_EQ(c.coefficient(y), -1.0);
  EXPECT_EQ(c.lower_bound(), 5.0);
  EXPECT_EQ(c.upper_bound(), 5.0);
}

TEST(ModelTest, AddVariablesEqualityLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c = model.AddLinearConstraint(x == y, "c");
  EXPECT_EQ(c.coefficient(x), 1.0);
  EXPECT_EQ(c.coefficient(y), -1.0);
  EXPECT_EQ(c.lower_bound(), 0.0);
  EXPECT_EQ(c.upper_bound(), 0.0);
}

TEST(ModelTest, AddLowerBoundedLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c = model.AddLinearConstraint(3 <= x - 1, "c");
  EXPECT_EQ(c.coefficient(x), 1.0);
  EXPECT_EQ(c.coefficient(y), 0.0);
  EXPECT_EQ(c.lower_bound(), 3.0 - -1.0);
  EXPECT_EQ(c.upper_bound(), kInf);
}

TEST(ModelTest, AddUpperBoundedLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c = model.AddLinearConstraint(y <= 5, "c");
  EXPECT_EQ(c.coefficient(x), 0.0);
  EXPECT_EQ(c.coefficient(y), 1.0);
  EXPECT_EQ(c.lower_bound(), -kInf);
  EXPECT_EQ(c.upper_bound(), 5.0);
}

TEST(ModelDeathTest, AddLinearConstraintOtherModel) {
  Model model_a("a");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");
  const Variable b_y = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddLinearConstraint(2 <= 2 * b_x - b_y + 2, "c"));
}

TEST(ModelTest, AddLinearConstraintWithoutVariables) {
  Model model;

  // Here we test a corner case that may not be very useful in practice: the
  // case of a bounded LinearExpression that have no terms but its offset.
  //
  // We want to make sure the code don't assume all LinearExpression have a
  // non-null storage().
  const LinearConstraint c =
      model.AddLinearConstraint(LinearExpression(3) <= 5, "c");
  EXPECT_EQ(c.lower_bound(), -kInf);
  EXPECT_EQ(c.upper_bound(), 2.0);
}

TEST(ModelTest, ObjectiveAccessors) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);

  const Model& const_model = model;
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y), 2.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, y), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, y), 0.0);

  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(y, x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(y, y));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 3.5);
  EXPECT_TRUE(const_model.is_maximize());

  // TODO(b/171883688): we should use expression matchers.
  EXPECT_THAT(const_model.ObjectiveAsLinearExpression().terms(),
              UnorderedElementsAre(Pair(y, 2.0)));
  EXPECT_DOUBLE_EQ(const_model.ObjectiveAsLinearExpression().offset(), 3.5);
  EXPECT_THAT(const_model.ObjectiveAsQuadraticExpression().quadratic_terms(),
              IsEmpty());
  EXPECT_THAT(const_model.ObjectiveAsQuadraticExpression().linear_terms(),
              UnorderedElementsAre(Pair(y, 2.0)));
  EXPECT_DOUBLE_EQ(const_model.ObjectiveAsQuadraticExpression().offset(), 3.5);

  // Now we add a quadratic term
  model.set_objective_coefficient(x, y, 3.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y), 2.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, y), 3.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, x), 3.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, y), 0.0);

  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y, x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(y, y));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 3.5);
  EXPECT_TRUE(const_model.is_maximize());

  // TODO(b/171883688): we should use expression matchers.
  EXPECT_THAT(const_model.ObjectiveAsQuadraticExpression().quadratic_terms(),
              UnorderedElementsAre(Pair(QuadraticTermKey(x, y), 3.0)));
  EXPECT_THAT(const_model.ObjectiveAsQuadraticExpression().linear_terms(),
              UnorderedElementsAre(Pair(y, 2.0)));
  EXPECT_DOUBLE_EQ(const_model.ObjectiveAsQuadraticExpression().offset(), 3.5);
}

TEST(ModelDeathTest, ObjectiveAsLinearExpressionWhenObjectiveIsQuadratic) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  model.set_objective_coefficient(x, y, 3.0);

  EXPECT_DEATH(model.ObjectiveAsLinearExpression(), "quadratic terms");
}

TEST(ModelTest, AddToObjective) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);

  model.AddToObjective(5.0 * x - y + 7.0);

  const Model& const_model = model;
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y), 1.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, y), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, y), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(y, x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(y, y));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 10.5);
  EXPECT_TRUE(const_model.is_maximize());

  model.AddToObjective(6.0 * x * y + 7.0 * y * y + 8.0 * x);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x), 13.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y), 1.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x, y), 6.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, x), 6.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y, y), 7.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y, x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y, y));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 10.5);
  EXPECT_TRUE(const_model.is_maximize());
}

TEST(ObjectiveDeathTest, AddToObjectiveOtherModel) {
  Model model_a;

  Model model_b;
  const Variable x_b = model_b.AddVariable("x");
  const Variable y_b = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddToObjective(5.0 * x_b - y_b + 7.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddToObjective(5.0 * x_b * x_b - y_b + 7.0));
}

TEST(ModelTest, AddToObjectiveConstant) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);

  model.AddToObjective(7.0);

  const Model& const_model = model;
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_DOUBLE_EQ(const_model.objective_coefficient(y), 2.0);

  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 10.5);
  EXPECT_TRUE(const_model.is_maximize());
}

TEST(ModelTest, MinimizeLinear) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.Minimize(5.0 * x - y + 7.0);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 7.0);
  EXPECT_FALSE(const_model.is_maximize());
}

TEST(ModelTest, MinimizeQuadratic) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.Minimize(5.0 * x * y - y + 7.0);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, y), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 7.0);
  EXPECT_FALSE(const_model.is_maximize());
}

TEST(ModelDeathTest, MinimizeOtherModel) {
  Model model_a;

  Model model_b;
  const Variable x_b = model_b.AddVariable("x");
  const Variable y_b = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.Minimize(5.0 * x_b - y_b + 7.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.Minimize(5.0 * x_b * y_b - y_b + 7.0));
}

TEST(ModelTest, MaximizeLinear) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_minimize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.Maximize(5.0 * x - y + 7.0);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 7.0);
  EXPECT_TRUE(const_model.is_maximize());
}

TEST(ModelTest, MaximizeQuadratic) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_minimize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.Maximize(5.0 * x * y - y + 7.0);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, y), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_DOUBLE_EQ(const_model.objective_offset(), 7.0);
  EXPECT_TRUE(const_model.is_maximize());
}

TEST(ModelDeathTest, MaximizeOtherModel) {
  Model model_a;

  Model model_b;
  const Variable x_b = model_b.AddVariable("x");
  const Variable y_b = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.Maximize(5.0 * x_b - y_b + 7.0));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.Maximize(5.0 * x_b * y_b - y_b + 7.0));
}

TEST(ModelTest, SetObjectiveLinear) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.SetObjective(5.0 * x - y + 7.0, false);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_EQ(const_model.objective_offset(), 7.0);
  EXPECT_FALSE(const_model.is_maximize());
}

TEST(ModelTest, SetObjectiveQuadratic) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(y, 2.0);
  model.set_objective_coefficient(z, 3.0);
  model.set_objective_coefficient(x, z, 4.0);

  model.SetObjective(5.0 * x * y - y + 7.0, false);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(y), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(z), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, y), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x, z), 0.0);

  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(z));
  EXPECT_TRUE(const_model.is_objective_coefficient_nonzero(x, y));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, z));

  EXPECT_EQ(const_model.objective_offset(), 7.0);
  EXPECT_FALSE(const_model.is_maximize());
}

TEST(ModelDeathTest, SetObjectiveOtherModel) {
  Model model_a;

  Model model_b;
  const Variable x_b = model_b.AddVariable("x");
  const Variable y_b = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.SetObjective(5.0 * x_b + 7.0, true));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.SetObjective(5.0 * x_b * y_b + 7.0, true));
}

TEST(ModelTest, SetObjectiveAsConstant) {
  Model model;
  const Variable x = model.AddVariable("x");

  // Set a non trivial initial quadratic objective to test that SetObjective
  // updates the offset and linear and quadratic coefficients, and resets to
  // zero those coefficients not in the new objective.
  model.set_maximize();
  model.set_objective_offset(3.5);
  model.set_objective_coefficient(x, 2.0);
  model.set_objective_coefficient(x, x, 3.0);

  model.SetObjective(7.0, false);

  const Model& const_model = model;
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x));
  EXPECT_FALSE(const_model.is_objective_coefficient_nonzero(x, x));

  EXPECT_EQ(const_model.objective_offset(), 7.0);
  EXPECT_FALSE(const_model.is_maximize());
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsDouble) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(3.0);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 3.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.Minimize(4.0);
  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 4.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.SetObjective(5.0, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsVariable) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 1.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.Minimize(x);

  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 1.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.SetObjective(x, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 1.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsLinearTerm) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(3.0 * x);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 3.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.Minimize(4.0 * x);
  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 4.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.SetObjective(5.0 * x, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsLinearExpression) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(3.0 * x + 4);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 4.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 3.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.Minimize(5.0 * x + 6);
  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 6.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);

  model.SetObjective(7.0 * x + 8, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 8.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 7.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 0.0);
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsQuadraticTerm) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(3.0 * x * x);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 3.0);

  model.Minimize(4.0 * x * x);
  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 4.0);

  model.SetObjective(5.0 * x * x, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 0.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 5.0);
}

// TODO(b/207482515): Add tests against expression constructor counters
TEST(ModelTest, ObjectiveAsQuadraticExpression) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(3.0 * x * x + 4 * x + 5);

  const Model& const_model = model;
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 5.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 4.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 3.0);

  model.Minimize(6.0 * x * x + 7 * x + 8);
  EXPECT_FALSE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), 8.0);
  EXPECT_EQ(const_model.objective_coefficient(x), 7.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 6.0);

  model.SetObjective(9.0 * x * x - x - 2, true);
  EXPECT_TRUE(const_model.is_maximize());
  EXPECT_EQ(const_model.objective_offset(), -2.0);
  EXPECT_EQ(const_model.objective_coefficient(x), -1.0);
  EXPECT_EQ(const_model.objective_coefficient(x, x), 9.0);
}

TEST(ModelTest, NonzeroVariablesInLinearObjective) {
  Model model;
  model.AddVariable();
  const Variable y = model.AddVariable();
  const Variable z = model.AddVariable();
  model.Minimize(3.0 * y + 0.0 * z + 1.0 * z * z);
  EXPECT_THAT(model.NonzeroVariablesInLinearObjective(),
              UnorderedElementsAre(y));
}

TEST(ModelTest, NonzeroVariablesInQuadraticObjective) {
  Model model;
  model.AddVariable();
  const Variable y = model.AddVariable();
  const Variable z = model.AddVariable();
  const Variable u = model.AddVariable();
  const Variable v = model.AddVariable();
  model.Minimize(3.0 * y + 0.0 * z + 1.0 * u * v);
  EXPECT_THAT(model.NonzeroVariablesInQuadraticObjective(),
              UnorderedElementsAre(u, v));
}

TEST(UpdateTrackerTest, ExportModel) {
  Model model;
  model.AddVariable("x");

  std::unique_ptr<UpdateTracker> update_tracker = model.NewUpdateTracker();

  EXPECT_THAT(update_tracker->ExportModel(),
              IsOkAndHolds(EquivToProto(R"pb(variables {
                                               ids: [ 0 ]
                                               lower_bounds: [ -inf ]
                                               upper_bounds: [ inf ]
                                               integers: [ false ]
                                               names: [ "x" ]
                                             })pb")));
}

TEST(UpdateTrackerTest, ExportModelUpdate) {
  Model model;
  const Variable x = model.AddVariable("x");

  std::unique_ptr<UpdateTracker> update_tracker = model.NewUpdateTracker();

  model.set_integer(x);

  EXPECT_THAT(update_tracker->ExportModelUpdate(),
              IsOkAndHolds(Optional(EquivToProto(R"pb(variable_updates {
                                                        integers {
                                                          ids: [ 0 ]
                                                          values: [ true ]
                                                        }
                                                      })pb"))));
}

TEST(ModelTest, ExportModelUpdate_RemoveNames) {
  Model model("my_model");
  std::unique_ptr<UpdateTracker> tracker = model.NewUpdateTracker();
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(x);
  Objective b = model.AddAuxiliaryObjective(1, "objB");
  model.set_objective_offset(b, 2.0);
  model.AddLinearConstraint(x <= 1.0, "lin_con");
  model.AddQuadraticConstraint(x * x <= 1.0, "quad_con");
  model.AddIndicatorConstraint(y, x >= 3.0, /*activate_on_zero=*/false,
                               "ind_con");
  model.AddSos1Constraint({y, 1.0 - y}, {1.0, 1.0}, "sos1");
  model.AddSos2Constraint({y, 1.0 - y}, {1.0, 1.0}, "sos2");
  model.AddSecondOrderConeConstraint({x + y}, 1.0, "soc");
  {
    ASSERT_OK_AND_ASSIGN(const std::optional<ModelUpdateProto> update,
                         tracker->ExportModelUpdate(/*remove_names=*/false));
    ASSERT_TRUE(update.has_value());
    EXPECT_THAT(update->new_variables().names(), ElementsAre("x", "y"));
    EXPECT_THAT(SortedValueNames(
                    update->auxiliary_objectives_updates().new_objectives()),
                ElementsAre("objB"));
    EXPECT_THAT(update->new_linear_constraints().names(),
                ElementsAre("lin_con"));
    EXPECT_THAT(SortedValueNames(
                    update->quadratic_constraint_updates().new_constraints()),
                ElementsAre("quad_con"));
    EXPECT_THAT(SortedValueNames(
                    update->indicator_constraint_updates().new_constraints()),
                ElementsAre("ind_con"));
    EXPECT_THAT(
        SortedValueNames(update->sos1_constraint_updates().new_constraints()),
        ElementsAre("sos1"));
    EXPECT_THAT(
        SortedValueNames(update->sos2_constraint_updates().new_constraints()),
        ElementsAre("sos2"));
    EXPECT_THAT(
        SortedValueNames(
            update->second_order_cone_constraint_updates().new_constraints()),
        ElementsAre("soc"));
  }

  {
    ASSERT_OK_AND_ASSIGN(const std::optional<ModelUpdateProto> update,
                         tracker->ExportModelUpdate(/*remove_names=*/true));
    ASSERT_TRUE(update.has_value());
    EXPECT_THAT(update->new_variables().names(), IsEmpty());
    EXPECT_THAT(SortedValueNames(
                    update->auxiliary_objectives_updates().new_objectives()),
                ElementsAre(""));
    EXPECT_THAT(update->new_linear_constraints().names(), IsEmpty());
    EXPECT_THAT(SortedValueNames(
                    update->quadratic_constraint_updates().new_constraints()),
                ElementsAre(""));
    EXPECT_THAT(SortedValueNames(
                    update->indicator_constraint_updates().new_constraints()),
                ElementsAre(""));
    EXPECT_THAT(
        SortedValueNames(update->sos1_constraint_updates().new_constraints()),
        ElementsAre(""));
    EXPECT_THAT(
        SortedValueNames(update->sos2_constraint_updates().new_constraints()),
        ElementsAre(""));
    EXPECT_THAT(
        SortedValueNames(
            update->second_order_cone_constraint_updates().new_constraints()),
        ElementsAre(""));
  }
}

TEST(UpdateTrackerTest, Checkpoint) {
  Model model;
  std::unique_ptr<UpdateTracker> update_tracker = model.NewUpdateTracker();

  const Variable x = model.AddVariable("x");

  ASSERT_OK(update_tracker->AdvanceCheckpoint());

  model.set_integer(x);

  EXPECT_THAT(update_tracker->ExportModelUpdate(),
              IsOkAndHolds(Optional(EquivToProto(R"pb(variable_updates {
                                                        integers {
                                                          ids: [ 0 ]
                                                          values: [ true ]
                                                        }
                                                      })pb"))));
}

TEST(UpdateTrackerTest, DestructionAfterModelDestruction) {
  auto model = std::make_unique<Model>();
  std::unique_ptr<UpdateTracker> update_tracker = model->NewUpdateTracker();

  // Destroy the model first.
  model = nullptr;

  // Then destroy the tracker.
  update_tracker = nullptr;
}

TEST(UpdateTrackerTest, ExportModelAfterModelDestruction) {
  auto model = std::make_unique<Model>();
  const std::unique_ptr<UpdateTracker> update_tracker =
      model->NewUpdateTracker();

  model = nullptr;

  EXPECT_THAT(update_tracker->ExportModel(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       internal::kModelIsDestroyed));
}

TEST(UpdateTrackerTest, ExportModelUpdateAfterModelDestruction) {
  auto model = std::make_unique<Model>();
  const std::unique_ptr<UpdateTracker> update_tracker =
      model->NewUpdateTracker();

  model = nullptr;

  EXPECT_THAT(update_tracker->ExportModelUpdate(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       internal::kModelIsDestroyed));
}

TEST(UpdateTrackerTest, CheckpointAfterModelDestruction) {
  auto model = std::make_unique<Model>();
  const std::unique_ptr<UpdateTracker> update_tracker =
      model->NewUpdateTracker();

  model = nullptr;

  EXPECT_THAT(update_tracker->AdvanceCheckpoint(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       internal::kModelIsDestroyed));
}

TEST(OstreamTest, EmptyModel) {
  const Model model("empty_model");
  EXPECT_EQ(StreamToString(model),
            R"model(Model empty_model:
 Objective:
  minimize 0
 Linear constraints:
 Variables:
)model");
}

TEST(OstreamTest, MinimizingLinearObjective) {
  Model model("minimize_linear_objective");
  const Variable noname = model.AddVariable();
  const Variable x = model.AddVariable("x");
  const Variable z = model.AddContinuousVariable(-15, 17, "z");
  const Variable n = model.AddIntegerVariable(7, 32, "n");
  const Variable t = model.AddContinuousVariable(-kInf, 7, "t");
  const Variable u = model.AddContinuousVariable(-4, kInf, "u");
  const Variable b = model.AddBinaryVariable("b");
  const Variable yy = model.AddVariable("y_y");
  model.AddLinearConstraint(z + x == 9, "c1");
  model.AddLinearConstraint(-3 * n + 2 * t + 2 >= 8);
  model.AddLinearConstraint(7 * u - 2 * b + 7 * yy - z <= 32, "c2");
  model.AddLinearConstraint(78 <= yy + 4 * noname <= 256, "c3");
  model.Minimize(45 * z + 3 * u);
  EXPECT_EQ(StreamToString(model),
            R"model(Model minimize_linear_objective:
 Objective:
  minimize 45*z + 3*u
 Linear constraints:
  c1: x + z = 9
  __lin_con#1__: -3*n + 2*t ≥ 6
  c2: -z + 7*u - 2*b + 7*y_y ≤ 32
  c3: 78 ≤ 4*__var#0__ + y_y ≤ 256
 Variables:
  __var#0__ in (-∞, +∞)
  x in (-∞, +∞)
  z in [-15, 17]
  n (integer) in [7, 32]
  t in (-∞, 7]
  u in [-4, +∞)
  b (binary)
  y_y in (-∞, +∞)
)model");
}

TEST(OstreamTest, MaximizingLinearObjective) {
  Model model("maximize_linear_objective");
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddContinuousVariable(1, 5, "y");
  model.AddLinearConstraint(x + y == 9, "c1");
  model.Maximize(-2 * x + y);
  EXPECT_EQ(StreamToString(model),
            R"model(Model maximize_linear_objective:
 Objective:
  maximize -2*x + y
 Linear constraints:
  c1: x + y = 9
 Variables:
  x in (-∞, +∞)
  y in [1, 5]
)model");
}

TEST(OstreamTest, WithoutName) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddContinuousVariable(1, 5, "y");
  model.AddLinearConstraint(x + y == 9, "c1");
  model.Maximize(-2 * x + y);
  EXPECT_EQ(StreamToString(model),
            R"model(Model:
 Objective:
  maximize -2*x + y
 Linear constraints:
  c1: x + y = 9
 Variables:
  x in (-∞, +∞)
  y in [1, 5]
)model");
}

TEST(OstreamTest, MinimizingQuadraticObjective) {
  Model model("minimize_quadratic_objective");
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddContinuousVariable(1, 5, "y");
  model.AddLinearConstraint(x + y == 9, "c1");
  model.Minimize(-2 * x + y + 7 * y * x - 5 * x * x);
  EXPECT_EQ(StreamToString(model),
            R"model(Model minimize_quadratic_objective:
 Objective:
  minimize -5*x² + 7*x*y - 2*x + y
 Linear constraints:
  c1: x + y = 9
 Variables:
  x in (-∞, +∞)
  y in [1, 5]
)model");
}

TEST(OstreamTest, FloatingPointRoundTripVariableBounds) {
  Model model("minimize_linear_objective");
  model.AddContinuousVariable(kRoundTripTestNumber, 17, "x");
  model.AddContinuousVariable(-kInf, kRoundTripTestNumber, "y");
  EXPECT_THAT(
      StreamToString(model),
      AllOf(
          HasSubstr(absl::StrCat("x in [", kRoundTripTestNumberStr, ", 17]")),
          HasSubstr(absl::StrCat("y in (-∞, ", kRoundTripTestNumberStr, "]"))));
}

// -------------------------- Auxiliary objectives -----------------------------

//   {max x, min 3, max 2y + 1} with priorities {2, 3, 5}
//   s.t. x, y unbounded
class SimpleAuxiliaryObjectiveTest : public testing::Test {
 protected:
  SimpleAuxiliaryObjectiveTest()
      : model_("auxiliary_objectives"),
        x_(model_.AddVariable("x")),
        y_(model_.AddVariable("y")),
        primary_(model_.primary_objective()),
        secondary_(model_.AddMinimizationObjective(3.0, 3, "secondary")),
        tertiary_(model_.AddMaximizationObjective(0.0, 5, "tertiary")) {
    // Maximize and Minimize wrap SetObjective, hence this tests them.
    model_.Maximize(primary_, x_);
    model_.set_objective_priority(primary_, 2);
    // We also want to exercise AddToObjective.
    model_.set_maximize(tertiary_);
    model_.AddToObjective(tertiary_, 2.0 * y_);
    model_.AddToObjective(tertiary_, 1.0);
  }

  Model model_;
  const Variable x_;
  const Variable y_;
  const Objective primary_;
  const Objective secondary_;
  const Objective tertiary_;
};

TEST_F(SimpleAuxiliaryObjectiveTest, Properties) {
  EXPECT_EQ(model_.num_auxiliary_objectives(), 2);
  EXPECT_EQ(model_.next_auxiliary_objective_id(), 2);
  EXPECT_TRUE(model_.has_auxiliary_objective(0));
  EXPECT_TRUE(model_.has_auxiliary_objective(1));
  EXPECT_FALSE(model_.has_auxiliary_objective(2));
  EXPECT_FALSE(model_.has_auxiliary_objective(3));
  EXPECT_FALSE(model_.has_auxiliary_objective(-1));
  EXPECT_THAT(model_.AuxiliaryObjectives(),
              UnorderedElementsAre(secondary_, tertiary_));
  EXPECT_THAT(model_.SortedAuxiliaryObjectives(),
              ElementsAre(secondary_, tertiary_));

  EXPECT_EQ(model_.auxiliary_objective(secondary_.id().value()).name(),
            "secondary");
  EXPECT_EQ(model_.auxiliary_objective(tertiary_.id().value()).name(),
            "tertiary");
  EXPECT_EQ(model_.auxiliary_objective(secondary_.typed_id().value()).name(),
            "secondary");
  EXPECT_EQ(model_.auxiliary_objective(tertiary_.typed_id().value()).name(),
            "tertiary");
}

TEST(AuxiliaryObjectiveTest, SenseSetting) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(3, "o");
  // set_maximize
  ASSERT_FALSE(o.maximize());
  model.set_maximize(o);
  ASSERT_TRUE(o.maximize());

  // set_minimize
  model.set_minimize(o);
  ASSERT_FALSE(o.maximize());

  model.set_is_maximize(o, true);
  EXPECT_TRUE(o.maximize());
}

TEST(AuxiliaryObjectiveTest, PrioritySetting) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(3, "o");
  ASSERT_EQ(o.priority(), 3);
  model.set_objective_priority(o, 4);
  EXPECT_EQ(o.priority(), 4);
}

TEST(AuxiliaryObjectiveTest, OffsetSetting) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(3, "o");
  ASSERT_EQ(o.offset(), 0.0);
  model.set_objective_offset(o, 4.0);
  EXPECT_EQ(o.offset(), 4.0);
}

TEST(AuxiliaryObjectiveTest, LinearCoefficientSetting) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Objective o = model.AddAuxiliaryObjective(3, "o");
  ASSERT_EQ(o.coefficient(x), 0.0);
  model.set_objective_coefficient(o, x, 3.0);
  EXPECT_EQ(o.coefficient(x), 3.0);
}

TEST_F(SimpleAuxiliaryObjectiveTest, DeleteAuxiliaryObjective) {
  model_.DeleteAuxiliaryObjective(secondary_);
  EXPECT_EQ(model_.num_auxiliary_objectives(), 1);
  EXPECT_EQ(model_.next_auxiliary_objective_id(), 2);
  EXPECT_FALSE(model_.has_auxiliary_objective(0));
  EXPECT_TRUE(model_.has_auxiliary_objective(1));
  EXPECT_THAT(model_.AuxiliaryObjectives(), UnorderedElementsAre(tertiary_));
}

TEST(AuxiliaryObjectiveTest, NewObjectiveMethods) {
  Model model;
  const Variable x = model.AddVariable("x");
  {
    const Objective a = model.AddAuxiliaryObjective(1);
    model.Maximize(a, x + 2.0);
    EXPECT_TRUE(a.maximize());
    EXPECT_EQ(a.offset(), 2.0);
    EXPECT_EQ(a.coefficient(x), 1.0);
  }
  {
    const Objective b = model.AddAuxiliaryObjective(2);
    model.Minimize(b, x + 2.0);
    EXPECT_FALSE(b.maximize());
    EXPECT_EQ(b.offset(), 2.0);
    EXPECT_EQ(b.coefficient(x), 1.0);
  }
  {
    const Objective c = model.AddMaximizationObjective(x + 2.0, 3);
    EXPECT_TRUE(c.maximize());
    EXPECT_EQ(c.offset(), 2.0);
    EXPECT_EQ(c.coefficient(x), 1.0);
  }
  {
    const Objective d = model.AddMinimizationObjective(x + 2.0, 4);
    EXPECT_FALSE(d.maximize());
    EXPECT_EQ(d.offset(), 2.0);
    EXPECT_EQ(d.coefficient(x), 1.0);
  }
  {
    const Objective e = model.AddAuxiliaryObjective(x + 2.0, true, 4);
    EXPECT_TRUE(e.maximize());
    EXPECT_EQ(e.offset(), 2.0);
    EXPECT_EQ(e.coefficient(x), 1.0);
  }
  {
    const Objective f = model.AddMinimizationObjective(7.0 * x - 3.0, 4);
    model.AddToObjective(f, -6.0 * x);
    model.AddToObjective(f, 5.0);
    EXPECT_FALSE(f.maximize());
    EXPECT_EQ(f.offset(), 2.0);
    EXPECT_EQ(f.coefficient(x), 1.0);
  }
}

TEST_F(SimpleAuxiliaryObjectiveTest, ExportModel) {
  EXPECT_THAT(
      model_.ExportModel(), EquivToProto(R"pb(
        name: "auxiliary_objectives"
        variables {
          ids: [ 0, 1 ]
          lower_bounds: [ -inf, -inf ]
          upper_bounds: [ inf, inf ]
          integers: [ false, false ]
          names: [ "x", "y" ]
        }
        objective {
          maximize: true
          linear_coefficients {
            ids: [ 0 ]
            values: [ 1.0 ]
          }
          priority: 2
        }
        auxiliary_objectives {
          key: 0
          value { maximize: false offset: 3.0 priority: 3 name: "secondary" }
        }
        auxiliary_objectives {
          key: 1
          value {
            maximize: true
            offset: 1.0
            linear_coefficients {
              ids: [ 1 ]
              values: [ 2.0 ]
            }
            priority: 5
            name: "tertiary"
          }
        }
      )pb"));
}

TEST_F(SimpleAuxiliaryObjectiveTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model auxiliary_objectives:
 Objectives:
  __primary_obj__ (priority 2): maximize x
  secondary (priority 3): minimize 3
  tertiary (priority 5): maximize 2*y + 1
 Linear constraints:
 Variables:
  x in (-∞, +∞)
  y in (-∞, +∞)
)model");
}

TEST(AuxiliaryObjectiveDeathTest, ObjectiveByIdOutOfBounds) {
  Model model;
  model.AddAuxiliaryObjective(1);
  EXPECT_DEATH(model.auxiliary_objective(-1),
               AllOf(HasSubstr("auxiliary objective"), HasSubstr("-1")));
  EXPECT_DEATH(model.auxiliary_objective(2),
               AllOf(HasSubstr("auxiliary objective"), HasSubstr("2")));
}

TEST(AuxiliaryObjectiveDeathTest, ObjectiveByIdDeleted) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(1, "o");
  EXPECT_EQ(model.auxiliary_objective(o.id().value()).name(), "o");
  model.DeleteAuxiliaryObjective(o);
  EXPECT_DEATH(model.auxiliary_objective(o.id().value()),
               AllOf(HasSubstr("auxiliary objective"), HasSubstr("0")));
}

TEST(AuxiliaryObjectiveDeathTest, DeletePrimaryObjective) {
  Model model;
  EXPECT_DEATH(model.DeleteAuxiliaryObjective(model.primary_objective()),
               HasSubstr("primary objective"));
}

TEST(AuxiliaryObjectiveDeathTest, DoubleDeleteObjective) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(3, "o");
  model.DeleteAuxiliaryObjective(o);
  EXPECT_DEATH(model.DeleteAuxiliaryObjective(o),
               HasSubstr("unrecognized auxiliary objective"));
}

TEST(AuxiliaryObjectiveDeathTest, DeleteInvalidObjective) {
  Model model;
  const Objective o =
      Objective::Auxiliary(model.storage(), AuxiliaryObjectiveId(0));
  EXPECT_DEATH(model.DeleteAuxiliaryObjective(o),
               HasSubstr("unrecognized auxiliary objective"));
}

TEST(AuxiliaryObjectiveDeathTest, SetObjectiveOtherModel) {
  Model model_a("a");
  const Objective o_a = model_a.AddAuxiliaryObjective(3);

  Model model_b("b");
  const Variable x_b = model_b.AddVariable();

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.SetObjective(o_a, x_b, false));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.SetObjective(o_a, x_b, false));
}

TEST(AuxiliaryObjectiveDeathTest, AddToObjectiveOtherModel) {
  Model model_a("a");
  const Objective o_a = model_a.AddAuxiliaryObjective(3);

  Model model_b("b");
  const Variable x_b = model_b.AddVariable();

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.AddToObjective(o_a, x_b));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_b.AddToObjective(o_a, x_b));
}

TEST(AuxiliaryObjectiveTest, NonzeroVariablesInLinarObjective) {
  Model model;
  const Objective o = model.AddAuxiliaryObjective(3);
  model.AddVariable();
  const Variable y = model.AddVariable();
  const Variable z = model.AddVariable();
  model.set_objective_coefficient(o, y, 3.0);
  model.set_objective_coefficient(o, z, 0.0);
  EXPECT_THAT(model.NonzeroVariablesInLinearObjective(o),
              UnorderedElementsAre(y));
}

// ------------------------- Quadratic constraints -----------------------------

//   max 0
//   s.t. x^2 + y^2 <= 1.0 (c)
//        2x*y + 3x >= 0.5 (d)
//            x unbounded
//            y in {0, 1}
class SimpleQuadraticConstraintTest : public testing::Test {
 protected:
  SimpleQuadraticConstraintTest()
      : model_("quadratic_constraints"),
        x_(model_.AddVariable("x")),
        y_(model_.AddBinaryVariable("y")),
        c_(model_.AddQuadraticConstraint(x_ * x_ + y_ * y_ <= 1.0, "c")),
        d_(model_.AddQuadraticConstraint(2.0 * x_ * y_ + 3.0 * y_ >= 0.5,
                                         "d")) {}

  Model model_;
  const Variable x_;
  const Variable y_;
  const QuadraticConstraint c_;
  const QuadraticConstraint d_;
};

TEST_F(SimpleQuadraticConstraintTest, Properties) {
  EXPECT_EQ(model_.num_quadratic_constraints(), 2);
  EXPECT_EQ(model_.next_quadratic_constraint_id(), 2);
  EXPECT_TRUE(model_.has_quadratic_constraint(0));
  EXPECT_TRUE(model_.has_quadratic_constraint(1));
  EXPECT_FALSE(model_.has_quadratic_constraint(2));
  EXPECT_FALSE(model_.has_quadratic_constraint(3));
  EXPECT_FALSE(model_.has_quadratic_constraint(-1));
  EXPECT_THAT(model_.QuadraticConstraints(), UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedQuadraticConstraints(), ElementsAre(c_, d_));

  EXPECT_EQ(model_.quadratic_constraint(c_.id()).name(), "c");
  EXPECT_EQ(model_.quadratic_constraint(d_.id()).name(), "d");
  EXPECT_EQ(model_.quadratic_constraint(c_.typed_id()).name(), "c");
  EXPECT_EQ(model_.quadratic_constraint(d_.typed_id()).name(), "d");
}

TEST_F(SimpleQuadraticConstraintTest, DeleteConstraint) {
  model_.DeleteQuadraticConstraint(c_);
  EXPECT_EQ(model_.num_quadratic_constraints(), 1);
  EXPECT_EQ(model_.next_quadratic_constraint_id(), 2);
  EXPECT_FALSE(model_.has_quadratic_constraint(0));
  EXPECT_TRUE(model_.has_quadratic_constraint(1));
  EXPECT_THAT(model_.QuadraticConstraints(), UnorderedElementsAre(d_));
}

TEST_F(SimpleQuadraticConstraintTest, ExportModel) {
  EXPECT_THAT(model_.ExportModel(), EquivToProto(R"pb(
                name: "quadratic_constraints"
                variables {
                  ids: [ 0, 1 ]
                  lower_bounds: [ -inf, 0.0 ]
                  upper_bounds: [ inf, 1.0 ]
                  integers: [ false, true ]
                  names: [ "x", "y" ]
                }
                quadratic_constraints {
                  key: 0
                  value: {
                    lower_bound: -inf
                    upper_bound: 1.0
                    quadratic_terms {
                      row_ids: [ 0, 1 ]
                      column_ids: [ 0, 1 ]
                      coefficients: [ 1.0, 1.0 ]
                    }
                    name: "c"
                  }
                }
                quadratic_constraints {
                  key: 1
                  value: {
                    lower_bound: 0.5
                    upper_bound: inf
                    linear_terms {
                      ids: [ 1 ]
                      values: [ 3.0 ]
                    }
                    quadratic_terms {
                      row_ids: [ 0 ]
                      column_ids: [ 1 ]
                      coefficients: [ 2.0 ]
                    }
                    name: "d"
                  }
                }
              )pb"));
}

TEST_F(SimpleQuadraticConstraintTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model quadratic_constraints:
 Objective:
  minimize 0
 Linear constraints:
 Quadratic constraints:
  c: x² + y² ≤ 1
  d: 2*x*y + 3*y ≥ 0.5
 Variables:
  x in (-∞, +∞)
  y (binary)
)model");
}

TEST(QuadraticConstraintTest, AddQuadraticConstraintWithoutVariables) {
  Model model;

  // Here we test a corner case that may not be very useful in practice: the
  // case of a bounded LinearExpression that have no terms but its offset.
  //
  // We want to make sure the code don't assume all LinearExpression have a
  // non-null storage().
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(BoundedQuadraticExpression(0.0, 1.0, 2.0));
  EXPECT_EQ(c.lower_bound(), 1.0);
  EXPECT_EQ(c.upper_bound(), 2.0);
  EXPECT_THAT(c.NonzeroVariables(), IsEmpty());
}

TEST(QuadraticConstraintDeathTest, ConstraintByIdOutOfBounds) {
  Model model;
  model.AddQuadraticConstraint(BoundedQuadraticExpression(0, 0, 0));
  EXPECT_DEATH(model.quadratic_constraint(-1),
               AllOf(HasSubstr("quadratic constraint"), HasSubstr("-1")));
  EXPECT_DEATH(model.quadratic_constraint(2),
               AllOf(HasSubstr("quadratic constraint"), HasSubstr("2")));
}

TEST(QuadraticConstraintDeathTest, ConstraintByIdDeleted) {
  Model model;
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(BoundedQuadraticExpression(0, 0, 0), "c");
  EXPECT_EQ(model.quadratic_constraint(c.id()).name(), "c");
  model.DeleteQuadraticConstraint(c);
  EXPECT_DEATH(model.quadratic_constraint(c.id()),
               AllOf(HasSubstr("quadratic constraint"), HasSubstr("0")));
}

TEST(QuadraticConstraintDeathTest, AddConstraintOtherModel) {
  Model model_a("a");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");
  const Variable b_y = model_b.AddVariable("y");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddQuadraticConstraint(2 <= 2 * b_x * b_y + 2, "c"));
}

// --------------------- Second-order cone constraints -------------------------

//   max  0
//   s.t. ||{x, y}||_2 <= 1.0 (c)
//        ||{1, 2x - y}||_2 <= 3y - 4 (d)
//        x, y unbounded
class SimpleSecondOrderConeConstraintTest : public testing::Test {
 protected:
  SimpleSecondOrderConeConstraintTest()
      : model_("soc_constraints"),
        x_(model_.AddVariable("x")),
        y_(model_.AddVariable("y")),
        c_(model_.AddSecondOrderConeConstraint({x_, y_}, 1.0, "c")),
        d_(model_.AddSecondOrderConeConstraint({1.0, 2.0 * x_ - y_},
                                               3.0 * y_ - 4.0, "d")) {}

  Model model_;
  const Variable x_;
  const Variable y_;
  const SecondOrderConeConstraint c_;
  const SecondOrderConeConstraint d_;
};

TEST_F(SimpleSecondOrderConeConstraintTest, Properties) {
  EXPECT_EQ(model_.num_second_order_cone_constraints(), 2);
  EXPECT_EQ(model_.next_second_order_cone_constraint_id(), 2);
  EXPECT_TRUE(model_.has_second_order_cone_constraint(0));
  EXPECT_TRUE(model_.has_second_order_cone_constraint(1));
  EXPECT_FALSE(model_.has_second_order_cone_constraint(2));
  EXPECT_FALSE(model_.has_second_order_cone_constraint(3));
  EXPECT_FALSE(model_.has_second_order_cone_constraint(-1));
  EXPECT_THAT(model_.SecondOrderConeConstraints(),
              UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedSecondOrderConeConstraints(), ElementsAre(c_, d_));

  EXPECT_EQ(model_.second_order_cone_constraint(c_.id()).name(), "c");
  EXPECT_EQ(model_.second_order_cone_constraint(d_.id()).name(), "d");
  EXPECT_EQ(model_.second_order_cone_constraint(c_.typed_id()).name(), "c");
  EXPECT_EQ(model_.second_order_cone_constraint(d_.typed_id()).name(), "d");
}

TEST_F(SimpleSecondOrderConeConstraintTest, DeleteConstraint) {
  model_.DeleteSecondOrderConeConstraint(c_);
  EXPECT_EQ(model_.num_second_order_cone_constraints(), 1);
  EXPECT_EQ(model_.next_second_order_cone_constraint_id(), 2);
  EXPECT_FALSE(model_.has_second_order_cone_constraint(0));
  EXPECT_TRUE(model_.has_second_order_cone_constraint(1));
  EXPECT_THAT(model_.SecondOrderConeConstraints(), UnorderedElementsAre(d_));
}

TEST_F(SimpleSecondOrderConeConstraintTest, ExportModel) {
  EXPECT_THAT(model_.ExportModel(), EquivToProto(R"pb(
                name: "soc_constraints"
                variables {
                  ids: [ 0, 1 ]
                  lower_bounds: [ -inf, -inf ]
                  upper_bounds: [ inf, inf ]
                  integers: [ false, false ]
                  names: [ "x", "y" ]
                }
                second_order_cone_constraints {
                  key: 0
                  value: {
                    upper_bound { offset: 1.0 }
                    arguments_to_norm {
                      ids: [ 0 ]
                      coefficients: [ 1.0 ]
                    }
                    arguments_to_norm {
                      ids: [ 1 ]
                      coefficients: [ 1.0 ]
                    }
                    name: "c"
                  }
                }
                second_order_cone_constraints {
                  key: 1
                  value: {
                    upper_bound {
                      ids: [ 1 ]
                      coefficients: [ 3.0 ]
                      offset: -4.0
                    }
                    arguments_to_norm { offset: 1.0 }
                    arguments_to_norm {
                      ids: [ 0, 1 ]
                      coefficients: [ 2.0, -1.0 ]
                    }
                    name: "d"
                  }
                }
              )pb"));
}

TEST_F(SimpleSecondOrderConeConstraintTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model soc_constraints:
 Objective:
  minimize 0
 Linear constraints:
 Second-order cone constraints:
  c: ||{x, y}||₂ ≤ 1
  d: ||{1, 2*x - y}||₂ ≤ 3*y - 4
 Variables:
  x in (-∞, +∞)
  y in (-∞, +∞)
)model");
}

TEST(SecondOrderConeConstraintTest,
     AddSecondOrderConeConstraintWithoutVariables) {
  Model model;

  // Here we test a corner case that may not be very useful in practice: the
  // case of a LinearExpression that has no terms but its offset.
  //
  // We want to make sure the code don't assume all LinearExpressions have a
  // non-null storage().
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({2.0}, 1.0, "c");
  {
    const LinearExpression ub = c.UpperBound();
    EXPECT_EQ(ub.offset(), 1.0);
    EXPECT_THAT(ub.terms(), IsEmpty());
  }
  {
    const std::vector<LinearExpression> args = c.ArgumentsToNorm();
    ASSERT_EQ(args.size(), 1);
    EXPECT_EQ(args[0].offset(), 2.0);
    EXPECT_THAT(args[0].terms(), IsEmpty());
  }
}

TEST(SecondOrderConeConstraintDeathTest, ConstraintByIdOutOfBounds) {
  Model model;
  model.AddSecondOrderConeConstraint({}, 1.0, "c");
  EXPECT_DEATH(
      model.second_order_cone_constraint(-1),
      AllOf(HasSubstr("second-order cone constraint"), HasSubstr("-1")));
  EXPECT_DEATH(
      model.second_order_cone_constraint(2),
      AllOf(HasSubstr("second-order cone constraint"), HasSubstr("2")));
}

TEST(SecondOrderConeConstraintDeathTest, ConstraintByIdDeleted) {
  Model model;
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({}, 1.0, "c");
  EXPECT_EQ(model.second_order_cone_constraint(c.id()).name(), "c");
  model.DeleteSecondOrderConeConstraint(c);
  EXPECT_DEATH(
      model.second_order_cone_constraint(c.id()),
      AllOf(HasSubstr("second-order cone constraint"), HasSubstr("0")));
}

TEST(SecondOrderConeConstraintDeathTest, AddConstraintOtherModel) {
  Model model_a("a");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddSecondOrderConeConstraint({b_x}, 1.0, "c"));
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddSecondOrderConeConstraint({1.0}, b_x, "c"));
}

// --------------------------- SOS1 constraints --------------------------------

//   max  0
//   s.t. {x, y} is SOS1 with weights {3, 2} (c)
//        {2 * y - 1, 1} is SOS1 (d)
//        x, y unbounded
class SimpleSos1ConstraintTest : public testing::Test {
 protected:
  SimpleSos1ConstraintTest()
      : model_("sos1_constraints"),
        x_(model_.AddVariable("x")),
        y_(model_.AddVariable("y")),
        c_(model_.AddSos1Constraint({x_, y_}, {3.0, 2.0}, "c")),
        d_(model_.AddSos1Constraint({2 * y_ - 1, 1.0}, {}, "d")) {}

  Model model_;
  const Variable x_;
  const Variable y_;
  const Sos1Constraint c_;
  const Sos1Constraint d_;
};

TEST_F(SimpleSos1ConstraintTest, Properties) {
  EXPECT_EQ(model_.num_sos1_constraints(), 2);
  EXPECT_EQ(model_.next_sos1_constraint_id(), 2);
  EXPECT_TRUE(model_.has_sos1_constraint(0));
  EXPECT_TRUE(model_.has_sos1_constraint(1));
  EXPECT_FALSE(model_.has_sos1_constraint(2));
  EXPECT_FALSE(model_.has_sos1_constraint(3));
  EXPECT_FALSE(model_.has_sos1_constraint(-1));
  EXPECT_THAT(model_.Sos1Constraints(), UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedSos1Constraints(), ElementsAre(c_, d_));

  EXPECT_EQ(model_.sos1_constraint(c_.id()).name(), "c");
  EXPECT_EQ(model_.sos1_constraint(d_.id()).name(), "d");
  EXPECT_EQ(model_.sos1_constraint(c_.typed_id()).name(), "c");
  EXPECT_EQ(model_.sos1_constraint(d_.typed_id()).name(), "d");
}

TEST_F(SimpleSos1ConstraintTest, DeleteConstraint) {
  model_.DeleteSos1Constraint(c_);
  EXPECT_EQ(model_.num_sos1_constraints(), 1);
  EXPECT_EQ(model_.next_sos1_constraint_id(), 2);
  EXPECT_FALSE(model_.has_sos1_constraint(0));
  EXPECT_TRUE(model_.has_sos1_constraint(1));
  EXPECT_THAT(model_.Sos1Constraints(), UnorderedElementsAre(d_));
}

TEST_F(SimpleSos1ConstraintTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model sos1_constraints:
 Objective:
  minimize 0
 Linear constraints:
 SOS1 constraints:
  c: {x, y} is SOS1 with weights {3, 2}
  d: {2*y - 1, 1} is SOS1
 Variables:
  x in (-∞, +∞)
  y in (-∞, +∞)
)model");
}

TEST(SimpleSos1ConstraintDeathTest, ConstraintByIdOutOfBounds) {
  Model model;
  model.AddSos1Constraint({}, {});
  EXPECT_DEATH(model.sos1_constraint(-1),
               AllOf(HasSubstr("SOS1 constraint"), HasSubstr("-1")));
  EXPECT_DEATH(model.sos1_constraint(2),
               AllOf(HasSubstr("SOS1 constraint"), HasSubstr("2")));
}

TEST(SimpleSos1ConstraintDeathTest, ConstraintByIdDeleted) {
  Model model;
  const Sos1Constraint c = model.AddSos1Constraint({}, {}, "c");
  EXPECT_EQ(model.sos1_constraint(c.id()).name(), "c");
  model.DeleteSos1Constraint(c);
  EXPECT_DEATH(model.sos1_constraint(c.id()),
               AllOf(HasSubstr("SOS1 constraint"), HasSubstr("0")));
}

TEST(SimpleSos1ConstraintDeathTest, AddConstraintOtherModel) {
  Model model_a("a");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.AddSos1Constraint({b_x}, {}));
}

// --------------------------- SOS2 constraints --------------------------------

//   max  0
//   s.t. {x, y} is SOS2 with weights {3, 2} (c)
//        {2 * y - 1, 1} is SOS2 (d)
//        x, y unbounded
class SimpleSos2ConstraintTest : public testing::Test {
 protected:
  SimpleSos2ConstraintTest()
      : model_("sos2_constraints"),
        x_(model_.AddVariable("x")),
        y_(model_.AddVariable("y")),
        c_(model_.AddSos2Constraint({x_, y_}, {3.0, 2.0}, "c")),
        d_(model_.AddSos2Constraint({2 * y_ - 1, 1.0}, {}, "d")) {}

  Model model_;
  const Variable x_;
  const Variable y_;
  const Sos2Constraint c_;
  const Sos2Constraint d_;
};

TEST_F(SimpleSos2ConstraintTest, Properties) {
  EXPECT_EQ(model_.num_sos2_constraints(), 2);
  EXPECT_EQ(model_.next_sos2_constraint_id(), 2);
  EXPECT_TRUE(model_.has_sos2_constraint(0));
  EXPECT_TRUE(model_.has_sos2_constraint(1));
  EXPECT_FALSE(model_.has_sos2_constraint(2));
  EXPECT_FALSE(model_.has_sos2_constraint(3));
  EXPECT_FALSE(model_.has_sos2_constraint(-1));
  EXPECT_THAT(model_.Sos2Constraints(), UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedSos2Constraints(), ElementsAre(c_, d_));

  EXPECT_EQ(model_.sos2_constraint(c_.id()).name(), "c");
  EXPECT_EQ(model_.sos2_constraint(d_.id()).name(), "d");
  EXPECT_EQ(model_.sos2_constraint(c_.typed_id()).name(), "c");
  EXPECT_EQ(model_.sos2_constraint(d_.typed_id()).name(), "d");
}

TEST_F(SimpleSos2ConstraintTest, DeleteConstraint) {
  model_.DeleteSos2Constraint(c_);
  EXPECT_EQ(model_.num_sos2_constraints(), 1);
  EXPECT_EQ(model_.next_sos2_constraint_id(), 2);
  EXPECT_FALSE(model_.has_sos2_constraint(0));
  EXPECT_TRUE(model_.has_sos2_constraint(1));
  EXPECT_THAT(model_.Sos2Constraints(), UnorderedElementsAre(d_));
}

TEST_F(SimpleSos2ConstraintTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model sos2_constraints:
 Objective:
  minimize 0
 Linear constraints:
 SOS2 constraints:
  c: {x, y} is SOS2 with weights {3, 2}
  d: {2*y - 1, 1} is SOS2
 Variables:
  x in (-∞, +∞)
  y in (-∞, +∞)
)model");
}

TEST(SimpleSos2ConstraintDeathTest, ConstraintByIdOutOfBounds) {
  Model model;
  model.AddSos2Constraint({}, {});
  EXPECT_DEATH(model.sos2_constraint(-1),
               AllOf(HasSubstr("SOS2 constraint"), HasSubstr("-1")));
  EXPECT_DEATH(model.sos2_constraint(2),
               AllOf(HasSubstr("SOS2 constraint"), HasSubstr("2")));
}

TEST(SimpleSos2ConstraintDeathTest, ConstraintByIdDeleted) {
  Model model;
  const Sos2Constraint c = model.AddSos2Constraint({}, {}, "c");
  EXPECT_EQ(model.sos2_constraint(c.id()).name(), "c");
  model.DeleteSos2Constraint(c);
  EXPECT_DEATH(model.sos2_constraint(c.id()),
               AllOf(HasSubstr("SOS2 constraint"), HasSubstr("0")));
}

TEST(SimpleSos2ConstraintDeathTest, AddConstraintOtherModel) {
  Model model_a("a");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");

  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(model_a.AddSos2Constraint({b_x}, {}));
}

// ------------------------ Indicator constraints ------------------------------

//   max  0
//   s.t. x = 1 --> z + 2 <= 3 (c)
//        y = 0 --> 1 <= 2 * z + 3 <= 4 (d)
//        x, y in {0,1}
//        z unbounded
class SimpleIndicatorConstraintTest : public testing::Test {
 protected:
  SimpleIndicatorConstraintTest()
      : model_("indicator_constraints"),
        x_(model_.AddBinaryVariable("x")),
        y_(model_.AddBinaryVariable("y")),
        z_(model_.AddVariable("z")),
        c_(model_.AddIndicatorConstraint(x_, z_ + 2 <= 3,
                                         /*activate_on_zero=*/false, "c")),
        d_(model_.AddIndicatorConstraint(y_, 1 <= 2 * z_ + 3 <= 4,
                                         /*activate_on_zero=*/true, "d")) {}

  Model model_;
  const Variable x_;
  const Variable y_;
  const Variable z_;
  const IndicatorConstraint c_;
  const IndicatorConstraint d_;
};

TEST_F(SimpleIndicatorConstraintTest, Properties) {
  EXPECT_EQ(model_.num_indicator_constraints(), 2);
  EXPECT_EQ(model_.next_indicator_constraint_id(), 2);
  EXPECT_TRUE(model_.has_indicator_constraint(0));
  EXPECT_TRUE(model_.has_indicator_constraint(1));
  EXPECT_FALSE(model_.has_indicator_constraint(2));
  EXPECT_FALSE(model_.has_indicator_constraint(3));
  EXPECT_FALSE(model_.has_indicator_constraint(-1));
  EXPECT_THAT(model_.IndicatorConstraints(), UnorderedElementsAre(c_, d_));
  EXPECT_THAT(model_.SortedIndicatorConstraints(), ElementsAre(c_, d_));

  EXPECT_EQ(model_.indicator_constraint(c_.id()).name(), "c");
  EXPECT_EQ(model_.indicator_constraint(d_.id()).name(), "d");
  EXPECT_EQ(model_.indicator_constraint(c_.typed_id()).name(), "c");
  EXPECT_EQ(model_.indicator_constraint(d_.typed_id()).name(), "d");
}

TEST_F(SimpleIndicatorConstraintTest, DeleteConstraint) {
  model_.DeleteIndicatorConstraint(c_);
  EXPECT_EQ(model_.num_indicator_constraints(), 1);
  EXPECT_EQ(model_.next_indicator_constraint_id(), 2);
  EXPECT_FALSE(model_.has_indicator_constraint(0));
  EXPECT_TRUE(model_.has_indicator_constraint(1));
  EXPECT_THAT(model_.IndicatorConstraints(), UnorderedElementsAre(d_));
}

TEST_F(SimpleIndicatorConstraintTest, Streaming) {
  EXPECT_EQ(StreamToString(model_),
            R"model(Model indicator_constraints:
 Objective:
  minimize 0
 Linear constraints:
 Indicator constraints:
  c: x = 1 ⇒ z ≤ 1
  d: y = 0 ⇒ -2 ≤ 2*z ≤ 1
 Variables:
  x (binary)
  y (binary)
  z in (-∞, +∞)
)model");
}

TEST(SimpleIndicatorConstraintDeathTest, ConstraintByIdOutOfBounds) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.AddIndicatorConstraint(x, x <= 1);

  EXPECT_DEATH(model.indicator_constraint(-1),
               AllOf(HasSubstr("indicator_constraint"), HasSubstr("-1")));
  EXPECT_DEATH(model.indicator_constraint(2),
               AllOf(HasSubstr("indicator_constraint"), HasSubstr("2")));
}

TEST(SimpleIndicatorConstraintDeathTest, ConstraintByIdDeleted) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(x, x <= 1, /*activate_on_zero=*/false, "c");

  EXPECT_EQ(model.indicator_constraint(c.id()).name(), "c");
  model.DeleteIndicatorConstraint(c);
  EXPECT_DEATH(model.indicator_constraint(c.id()),
               AllOf(HasSubstr("indicator constraint"), HasSubstr("0")));
}

TEST(SimpleIndicatorConstraintDeathTest, AddConstraintOtherModel) {
  Model model_a("a");
  const Variable a_x = model_a.AddVariable("x");

  Model model_b("b");
  const Variable b_x = model_b.AddVariable("x");

  // The indicator variable should trigger the crash.
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddIndicatorConstraint(b_x, a_x <= 1));

  // The implied constraint should trigger the crash.
  EXPECT_DEATH_OBJECT_FROM_OTHER_STORAGE(
      model_a.AddIndicatorConstraint(a_x, b_x <= 1));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
