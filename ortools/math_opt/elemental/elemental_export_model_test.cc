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

#include <cstdint>
#include <limits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::EqualsProto;
using ::testing::status::IsOkAndHolds;

TEST(ExportModelProtoTest, ModelNameExports) {
  Elemental elemental("my_model");

  ModelProto expected;
  expected.set_name("my_model");

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
  EXPECT_THAT(elemental.ExportModel(/*remove_names=*/true),
              IsOkAndHolds(EqualsProto(ModelProto())));
}

TEST(ExportModelProtoTest, Variable) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(x), true);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 2.0);
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(x), -1.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  vars.add_ids(x.value());
  vars.add_integers(true);
  vars.add_lower_bounds(-1.0);
  vars.add_upper_bounds(2.0);
  vars.add_names("x");

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
  vars.clear_names();
  EXPECT_THAT(elemental.ExportModel(/*remove_names=*/true),
              IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, Objective) {
  Elemental elemental("", "my_obj");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.0);
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 3.0);
  elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), 8);
  using QuadKey = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, QuadKey(x, x), 5.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, QuadKey(x, y), 6.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, QuadKey(y, y), 7.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  vars.add_ids(y.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("y");
  ObjectiveProto& obj = *expected.mutable_objective();
  obj.set_name("my_obj");
  obj.set_maximize(true);
  obj.set_offset(4.0);
  obj.set_priority(8);
  obj.mutable_linear_coefficients()->add_ids(x.value());
  obj.mutable_linear_coefficients()->add_values(3.0);
  obj.mutable_quadratic_coefficients()->add_row_ids(x.value());
  obj.mutable_quadratic_coefficients()->add_column_ids(x.value());
  obj.mutable_quadratic_coefficients()->add_coefficients(5.0);
  obj.mutable_quadratic_coefficients()->add_row_ids(x.value());
  obj.mutable_quadratic_coefficients()->add_column_ids(y.value());
  obj.mutable_quadratic_coefficients()->add_coefficients(6.0);
  obj.mutable_quadratic_coefficients()->add_row_ids(y.value());
  obj.mutable_quadratic_coefficients()->add_column_ids(y.value());
  obj.mutable_quadratic_coefficients()->add_coefficients(7.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
  obj.clear_name();
  vars.clear_names();
  EXPECT_THAT(elemental.ExportModel(/*remove_names=*/true),
              IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, ObjectiveNameOnlyStillExports) {
  Elemental elemental("", "obj_name");

  ModelProto expected;
  ObjectiveProto& obj = *expected.mutable_objective();
  obj.set_name("obj_name");

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
  EXPECT_THAT(elemental.ExportModel(/*remove_names=*/true),
              IsOkAndHolds(EqualsProto(ModelProto())));
}

TEST(ExportModelProtoTest, ObjectiveDiretionOnlyStillExports) {
  Elemental elemental;
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);

  ModelProto expected;
  ObjectiveProto& obj = *expected.mutable_objective();
  obj.set_maximize(true);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, ObjectiveOffsetOnlyStillExports) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.0);

  ModelProto expected;
  ObjectiveProto& obj = *expected.mutable_objective();
  obj.set_offset(4.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, ObjectivePriorityOnlyStillExports) {
  Elemental elemental;
  elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), 4);

  ModelProto expected;
  ObjectiveProto& obj = *expected.mutable_objective();
  obj.set_priority(4);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, ObjectiveLinCoefOnlyStillExports) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 3.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  auto& terms = *expected.mutable_objective()->mutable_linear_coefficients();
  terms.add_ids(0);
  terms.add_values(3.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, ObjectiveQuadCoefOnlyStillExports) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr2>(x, x), 3.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  auto& terms = *expected.mutable_objective()->mutable_quadratic_coefficients();
  terms.add_row_ids(x.value());
  terms.add_column_ids(x.value());
  terms.add_coefficients(3.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, AuxiliaryObjectiveAllFields) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kAuxiliaryObjective,
                                              3);
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("aaa");
  elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(a), true);
  elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 4.0);
  elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 5);
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 6.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  ObjectiveProto& obj = (*expected.mutable_auxiliary_objectives())[3];
  obj.set_name("aaa");
  obj.set_maximize(true);
  obj.set_offset(4.0);
  obj.set_priority(5);
  obj.mutable_linear_coefficients()->add_ids(0);
  obj.mutable_linear_coefficients()->add_values(6.0);
  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, AuxiliaryObjectiveEmptyWithNoLinearTerms) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kAuxiliaryObjective,
                                              3);
  elemental.AddElement<ElementType::kAuxiliaryObjective>("");

  ModelProto expected;
  (*expected.mutable_auxiliary_objectives())[3];
  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, LinearConstraint) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kLinConLb, AttrKey(c), -1.0);

  ModelProto expected;
  LinearConstraintsProto& lin_cons = *expected.mutable_linear_constraints();
  lin_cons.add_ids(c.value());
  lin_cons.add_lower_bounds(-1.0);
  lin_cons.add_upper_bounds(2.0);
  lin_cons.add_names("c");

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
  lin_cons.clear_names();
  EXPECT_THAT(elemental.ExportModel(/*remove_names=*/true),
              IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, LinearConstraintMatrix) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("");  // Ensure x != c.
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 2.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  // add the unused variable
  vars.add_ids(0);
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("");
  // add x.
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  LinearConstraintsProto& lin_cons = *expected.mutable_linear_constraints();
  lin_cons.add_ids(c.value());
  lin_cons.add_lower_bounds(-kInf);
  lin_cons.add_upper_bounds(kInf);
  lin_cons.add_names("c");
  SparseDoubleMatrixProto& mat = *expected.mutable_linear_constraint_matrix();
  mat.add_row_ids(0);
  mat.add_column_ids(1);
  mat.add_coefficients(2.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, QuadraticConstraint) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("");  // Ensure x != c.
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 4.0);
  using QuadConKey = AttrKeyFor<SymmetricDoubleAttr3>;
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadConKey(c, x, x),
                    5.0);
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadConKey(c, x, y),
                    6.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  // add the unused variable
  vars.add_ids(0);
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("");
  // add x.
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  // add y.
  vars.add_ids(y.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("y");
  QuadraticConstraintProto& quad_con =
      (*expected.mutable_quadratic_constraints())[c.value()];
  quad_con.set_lower_bound(2.0);
  quad_con.set_upper_bound(3.0);
  quad_con.set_name("c");
  quad_con.mutable_linear_terms()->add_ids(x.value());
  quad_con.mutable_linear_terms()->add_values(4.0);
  SparseDoubleMatrixProto& mat = *quad_con.mutable_quadratic_terms();
  mat.add_row_ids(x.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(5.0);
  mat.add_row_ids(x.value());
  mat.add_column_ids(y.value());
  mat.add_coefficients(6.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, IndicatorConstraintAllSet) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("");  // Ensure x != c.
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  const VariableId z = elemental.AddElement<ElementType::kVariable>("z");
  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(z), true);
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(z), 0.0);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(z), 1.0);
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kIndConLb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c), 3.0);
  elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c), z);
  elemental.SetAttr(BoolAttr1::kIndConActivateOnZero, AttrKey(c), true);
  elemental.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, x), 4.0);
  elemental.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, y), 5.0);

  ModelProto expected;
  VariablesProto& vars = *expected.mutable_variables();
  // add the unused variable
  vars.add_ids(0);
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("");
  // add x.
  vars.add_ids(x.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("x");
  // add y.
  vars.add_ids(y.value());
  vars.add_integers(false);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_names("y");
  // add z.
  vars.add_ids(z.value());
  vars.add_integers(true);
  vars.add_lower_bounds(0);
  vars.add_upper_bounds(1);
  vars.add_names("z");
  IndicatorConstraintProto& ind_con =
      (*expected.mutable_indicator_constraints())[c.value()];
  ind_con.set_lower_bound(2.0);
  ind_con.set_upper_bound(3.0);
  ind_con.set_name("c");
  ind_con.set_activate_on_zero(true);
  ind_con.set_indicator_id(z.value());
  ind_con.mutable_expression()->add_ids(x.value());
  ind_con.mutable_expression()->add_values(4.0);
  ind_con.mutable_expression()->add_ids(y.value());
  ind_con.mutable_expression()->add_values(5.0);

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, IndicatorConstraintNoneSet) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");

  ModelProto expected;
  IndicatorConstraintProto& ind_con =
      (*expected.mutable_indicator_constraints())[c.value()];
  ind_con.set_lower_bound(-kInf);
  ind_con.set_upper_bound(kInf);
  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

TEST(ExportModelProtoTest, IndicatorConstraintDeleteIndicator) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("");
  elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c), x);
  elemental.DeleteElement(x);

  ModelProto expected;
  IndicatorConstraintProto& ind_con =
      (*expected.mutable_indicator_constraints())[c.value()];
  ind_con.set_lower_bound(-kInf);
  ind_con.set_upper_bound(kInf);
  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

////////////////////////////////////////////////////////////////////////////////
// Larger tests
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelProtoTest, SimpleMip) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");

  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const LinearConstraintId d =
      elemental.AddElement<ElementType::kLinearConstraint>("d");

  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(y), true);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 2.0);
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(x), -1.0);
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(y), -2.0);

  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.0);
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(y), 3.0);

  elemental.SetAttr(DoubleAttr1::kLinConLb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(d), 5.0);

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 7.0);
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, y), 8.0);
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(d, y), 9.0);

  ModelProto expected;
  expected.mutable_objective()->set_maximize(true);
  expected.mutable_objective()->set_offset(4.0);
  expected.mutable_objective()->mutable_linear_coefficients()->add_ids(
      y.value());
  expected.mutable_objective()->mutable_linear_coefficients()->add_values(3.0);

  {
    VariablesProto& vars = *expected.mutable_variables();
    vars.add_ids(x.value());
    vars.add_integers(false);
    vars.add_lower_bounds(-1.0);
    vars.add_upper_bounds(2.0);
    vars.add_names("x");

    vars.add_ids(y.value());
    vars.add_integers(true);
    vars.add_lower_bounds(-2.0);
    vars.add_upper_bounds(kInf);
    vars.add_names("y");
  }

  {
    LinearConstraintsProto& lin_cons = *expected.mutable_linear_constraints();
    lin_cons.add_ids(c.value());
    lin_cons.add_lower_bounds(3.0);
    lin_cons.add_upper_bounds(3.0);
    lin_cons.add_names("c");

    lin_cons.add_ids(d.value());
    lin_cons.add_lower_bounds(-kInf);
    lin_cons.add_upper_bounds(5.0);
    lin_cons.add_names("d");
  }
  {
    SparseDoubleMatrixProto& mat = *expected.mutable_linear_constraint_matrix();
    mat.add_row_ids(c.value());
    mat.add_column_ids(x.value());
    mat.add_coefficients(7.0);

    mat.add_row_ids(c.value());
    mat.add_column_ids(y.value());
    mat.add_coefficients(8.0);

    mat.add_row_ids(d.value());
    mat.add_column_ids(y.value());
    mat.add_coefficients(9.0);
  }

  EXPECT_THAT(elemental.ExportModel(), IsOkAndHolds(EqualsProto(expected)));
}

}  // namespace
}  // namespace operations_research::math_opt
