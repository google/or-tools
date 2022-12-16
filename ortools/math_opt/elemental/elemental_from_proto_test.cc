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

#include <limits>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elemental_matcher.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(ElementalFromProtoTest, EmptyModel) {
  ModelProto proto;

  Elemental expected;
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithNames) {
  ModelProto proto;
  proto.set_name("xyz");
  proto.mutable_objective()->set_name("123");

  Elemental expected("xyz", "123");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithVariablesAndNoNames) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithLinearConstraintsAndNoNames) {
  ModelProto proto;
  LinearConstraintsProto& lin_cons = *proto.mutable_linear_constraints();
  lin_cons.add_ids(0);
  lin_cons.add_lower_bounds(-kInf);
  lin_cons.add_upper_bounds(kInf);

  Elemental expected;
  expected.AddElement<ElementType::kLinearConstraint>("");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithVariables) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(1);
  vars.add_ids(2);
  vars.add_lower_bounds(3.0);
  vars.add_lower_bounds(4.0);
  vars.add_upper_bounds(5.0);
  vars.add_upper_bounds(6.0);
  vars.add_integers(false);
  vars.add_integers(true);
  vars.add_names("x");
  vars.add_names("y");

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 1);
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(1), 3.0);
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(2), 4.0);
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(1), 5.0);
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(2), 6.0);
  expected.SetAttr(BoolAttr1::kVarInteger, AttrKey(2), true);
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithObjective) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_integers(false);
  vars.add_names("x");
  vars.add_names("y");

  ObjectiveProto& obj = *proto.mutable_objective();
  obj.set_priority(3);
  obj.set_offset(4.0);
  obj.set_maximize(true);
  SparseDoubleVectorProto& lin_obj = *obj.mutable_linear_coefficients();
  lin_obj.add_ids(0);
  lin_obj.add_ids(1);
  lin_obj.add_values(5.0);
  lin_obj.add_values(6.0);

  SparseDoubleMatrixProto& quad_obj = *obj.mutable_quadratic_coefficients();
  quad_obj.add_row_ids(0);
  quad_obj.add_row_ids(0);
  quad_obj.add_row_ids(1);
  quad_obj.add_column_ids(0);
  quad_obj.add_column_ids(1);
  quad_obj.add_column_ids(1);
  quad_obj.add_coefficients(7.0);
  quad_obj.add_coefficients(8.0);
  quad_obj.add_coefficients(9.0);

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.SetAttr(BoolAttr0::kMaximize, {}, true);
  expected.SetAttr(IntAttr0::kObjPriority, {}, 3);
  expected.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  expected.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(0), 5.0);
  expected.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(1), 6.0);
  using ObjKey = AttrKeyFor<SymmetricDoubleAttr2>;
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, ObjKey(0, 0), 7.0);
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, ObjKey(0, 1), 8.0);
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, ObjKey(1, 1), 9.0);

  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithLinearConstraints) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_integers(false);
  vars.add_names("x");
  vars.add_names("y");

  LinearConstraintsProto& lin_cons = *proto.mutable_linear_constraints();
  lin_cons.add_ids(4);
  lin_cons.add_ids(5);
  lin_cons.add_lower_bounds(-kInf);
  lin_cons.add_lower_bounds(-10.0);
  lin_cons.add_upper_bounds(kInf);
  lin_cons.add_upper_bounds(10.0);
  lin_cons.add_names("c");
  lin_cons.add_names("d");

  SparseDoubleMatrixProto& lin_con_coef =
      *proto.mutable_linear_constraint_matrix();
  lin_con_coef.add_row_ids(4);
  lin_con_coef.add_row_ids(4);
  lin_con_coef.add_row_ids(5);
  lin_con_coef.add_column_ids(0);
  lin_con_coef.add_column_ids(1);
  lin_con_coef.add_column_ids(0);
  lin_con_coef.add_coefficients(7.0);
  lin_con_coef.add_coefficients(8.0);
  lin_con_coef.add_coefficients(9.0);

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kLinearConstraint, 4);
  expected.AddElement<ElementType::kLinearConstraint>("c");
  expected.AddElement<ElementType::kLinearConstraint>("d");

  expected.SetAttr(DoubleAttr1::kLinConLb, AttrKey(5), -10.0);
  expected.SetAttr(DoubleAttr1::kLinConUb, AttrKey(5), 10.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(4, 0), 7.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(4, 1), 8.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(5, 0), 9.0);

  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithQuadraticConstraints) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_integers(false);
  vars.add_names("x");
  vars.add_names("y");

  {
    QuadraticConstraintProto& quad_con1 =
        (*proto.mutable_quadratic_constraints())[4];
    quad_con1.set_name("c");
    quad_con1.set_lower_bound(3.0);
    quad_con1.set_upper_bound(4.0);
    SparseDoubleVectorProto& lin = *quad_con1.mutable_linear_terms();
    lin.add_ids(0);
    lin.add_ids(1);
    lin.add_values(5.0);
    lin.add_values(6.0);
    SparseDoubleMatrixProto& mat = *quad_con1.mutable_quadratic_terms();
    mat.add_row_ids(0);
    mat.add_row_ids(0);
    mat.add_row_ids(1);
    mat.add_column_ids(0);
    mat.add_column_ids(1);
    mat.add_column_ids(1);
    mat.add_coefficients(7.0);
    mat.add_coefficients(8.0);
    mat.add_coefficients(9.0);
  }
  {
    QuadraticConstraintProto& quad_con3 =
        (*proto.mutable_quadratic_constraints())[6];
    quad_con3.set_name("d");
    quad_con3.set_lower_bound(-kInf);
    quad_con3.set_upper_bound(kInf);
    SparseDoubleMatrixProto& mat = *quad_con3.mutable_quadratic_terms();
    mat.add_row_ids(1);
    mat.add_column_ids(1);
    mat.add_coefficients(10.0);
  }

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kQuadraticConstraint,
                                             4);
  expected.AddElement<ElementType::kQuadraticConstraint>("c");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kQuadraticConstraint,
                                             6);
  expected.AddElement<ElementType::kQuadraticConstraint>("d");

  expected.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(4), 3.0);
  expected.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(4), 4.0);
  expected.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(4, 0), 5.0);
  expected.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(4, 1), 6.0);
  using QuadKey = AttrKeyFor<SymmetricDoubleAttr3>;
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadKey(4, 0, 0),
                   7.0);
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadKey(4, 0, 1),
                   8.0);
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadKey(4, 1, 1),
                   9.0);
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, QuadKey(6, 1, 1),
                   10.0);

  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithIndicatorConstraint) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_ids(1);
  vars.add_ids(2);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(0.0);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(1.0);
  vars.add_integers(false);
  vars.add_integers(false);
  vars.add_integers(true);
  vars.add_names("x");
  vars.add_names("y");
  vars.add_names("z");

  {
    IndicatorConstraintProto& ind_con4 =
        (*proto.mutable_indicator_constraints())[4];
    ind_con4.set_name("c");
    ind_con4.set_lower_bound(3.0);
    ind_con4.set_upper_bound(4.0);
    SparseDoubleVectorProto& lin = *ind_con4.mutable_expression();
    lin.add_ids(0);
    lin.add_ids(1);
    lin.add_values(5.0);
    lin.add_values(6.0);
    ind_con4.set_activate_on_zero(true);
    ind_con4.set_indicator_id(2);
  }
  {
    IndicatorConstraintProto& ind_con6 =
        (*proto.mutable_indicator_constraints())[6];
    ind_con6.set_name("d");
    ind_con6.set_lower_bound(-kInf);
    ind_con6.set_upper_bound(kInf);
  }

  Elemental expected;
  const VariableId x = expected.AddElement<ElementType::kVariable>("x");
  const VariableId y = expected.AddElement<ElementType::kVariable>("y");
  const VariableId z = expected.AddElement<ElementType::kVariable>("z");
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(z), 0.0);
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(z), 1.0);
  expected.SetAttr(BoolAttr1::kVarInteger, AttrKey(z), true);
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kIndicatorConstraint,
                                             4);
  const IndicatorConstraintId c =
      expected.AddElement<ElementType::kIndicatorConstraint>("c");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kIndicatorConstraint,
                                             6);
  expected.AddElement<ElementType::kIndicatorConstraint>("d");

  expected.SetAttr(DoubleAttr1::kIndConLb, AttrKey(c), 3.0);
  expected.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c), 4.0);
  expected.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, x), 5.0);
  expected.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, y), 6.0);
  expected.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c), z);
  expected.SetAttr(BoolAttr1::kIndConActivateOnZero, AttrKey(c), true);
  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, ModelWithAuxiliaryConstraints) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_integers(false);
  vars.add_names("x");
  vars.add_names("y");

  {
    ObjectiveProto& aux_obj1 = (*proto.mutable_auxiliary_objectives())[4];
    aux_obj1.set_name("o1");
    aux_obj1.set_maximize(true);
    aux_obj1.set_priority(3);
    aux_obj1.set_offset(4.0);
    SparseDoubleVectorProto& lin = *aux_obj1.mutable_linear_coefficients();
    lin.add_ids(0);
    lin.add_ids(1);
    lin.add_values(5.0);
    lin.add_values(6.0);
  }
  {
    ObjectiveProto& aux_obj2 = (*proto.mutable_auxiliary_objectives())[6];
    SparseDoubleVectorProto& lin = *aux_obj2.mutable_linear_coefficients();
    lin.add_ids(1);
    lin.add_values(7.0);
  }

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kAuxiliaryObjective,
                                             4);
  expected.AddElement<ElementType::kAuxiliaryObjective>("o1");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kAuxiliaryObjective,
                                             6);
  expected.AddElement<ElementType::kAuxiliaryObjective>("");

  expected.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(4), true);
  expected.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(4), 3);
  expected.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(4), 4.0);
  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(4, 0), 5.0);
  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(4, 1), 6.0);

  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(6, 1), 7.0);

  EXPECT_THAT(Elemental::FromModelProto(proto),
              IsOkAndHolds(EquivToElemental(expected)));
}

TEST(ElementalFromProtoTest, QuadraticAuxObjIsInvalid) {
  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  vars.add_ids(0);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("x");

  {
    ObjectiveProto& aux_obj1 = (*proto.mutable_auxiliary_objectives())[4];
    SparseDoubleMatrixProto& quad = *aux_obj1.mutable_quadratic_coefficients();
    quad.add_row_ids(0);
    quad.add_column_ids(0);
    quad.add_coefficients(5.0);
  }

  EXPECT_THAT(Elemental::FromModelProto(proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic coefficients not supported")));
}

TEST(ElementalFromProtoTest, SecondOrderConeNotSupported) {
  ModelProto proto;
  (*proto.mutable_second_order_cone_constraints())[0].set_name("c");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("second order cone")));
}

TEST(ElementalFromProtoTest, Sos1NotSupported) {
  ModelProto proto;
  (*proto.mutable_sos1_constraints())[0].set_name("c");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos1")));
}

TEST(ElementalFromProtoTest, Sos2NotSupported) {
  ModelProto proto;
  (*proto.mutable_sos2_constraints())[0].set_name("c");
  EXPECT_THAT(Elemental::FromModelProto(proto),
              StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos2")));
}

}  // namespace
}  // namespace operations_research::math_opt
