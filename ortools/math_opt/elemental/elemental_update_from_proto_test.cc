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
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

class ElementalUpdateFromProtoNameTest : public testing::TestWithParam<bool> {
 public:
  bool HasNames() const { return GetParam(); }
};

TEST(ElementalUpdateFromProtoTest, EmptyUpdateNoEffect) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  ModelUpdateProto update;

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

TEST_P(ElementalUpdateFromProtoNameTest, AddVariable) {
  Elemental elemental;
  ModelUpdateProto update;
  VariablesProto& vars = *update.mutable_new_variables();
  vars.add_ids(0);
  vars.add_lower_bounds(0.0);
  vars.add_upper_bounds(1.0);
  vars.add_integers(true);
  if (HasNames()) {
    vars.add_names("x");
  }

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>(HasNames() ? "x" : "");
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(0), 0.0);
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 1.0);
  expected.SetAttr(BoolAttr1::kVarInteger, AttrKey(0), true);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyVariableLb) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  ModelUpdateProto update;
  SparseDoubleVectorProto& var_lbs =
      *update.mutable_variable_updates()->mutable_lower_bounds();
  var_lbs.add_ids(0);
  var_lbs.add_values(-3.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(0), -3.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyVariableUb) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  ModelUpdateProto update;
  SparseDoubleVectorProto& var_ubs =
      *update.mutable_variable_updates()->mutable_upper_bounds();
  var_ubs.add_ids(0);
  var_ubs.add_values(3.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 3.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyVariableInteger) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  ModelUpdateProto update;
  SparseBoolVectorProto& var_integers =
      *update.mutable_variable_updates()->mutable_integers();
  var_integers.add_ids(0);
  var_integers.add_values(true);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.SetAttr(BoolAttr1::kVarInteger, AttrKey(0), true);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, DeleteVariable) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  elemental.AddElement<ElementType::kVariable>("y");
  ModelUpdateProto update;
  update.add_deleted_variable_ids(0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 1);
  expected.AddElement<ElementType::kVariable>("y");
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraints
////////////////////////////////////////////////////////////////////////////////

TEST_P(ElementalUpdateFromProtoNameTest, AddLinearConstraint) {
  Elemental elemental;
  ModelUpdateProto update;
  LinearConstraintsProto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(0);
  lin_cons.add_lower_bounds(0.0);
  lin_cons.add_upper_bounds(1.0);
  if (HasNames()) {
    lin_cons.add_names("c");
  }

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kLinearConstraint>(HasNames() ? "c" : "");
  expected.SetAttr(DoubleAttr1::kLinConLb, AttrKey(0), 0.0);
  expected.SetAttr(DoubleAttr1::kLinConUb, AttrKey(0), 1.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyLinearConstraintLb) {
  Elemental elemental;
  elemental.AddElement<ElementType::kLinearConstraint>("c");
  ModelUpdateProto update;
  SparseDoubleVectorProto& lin_con_lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lin_con_lbs.add_ids(0);
  lin_con_lbs.add_values(-3.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kLinearConstraint>("c");
  expected.SetAttr(DoubleAttr1::kLinConLb, AttrKey(0), -3.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyLinearConstraintUb) {
  Elemental elemental;
  elemental.AddElement<ElementType::kLinearConstraint>("c");
  ModelUpdateProto update;
  SparseDoubleVectorProto& lin_con_lbs =
      *update.mutable_linear_constraint_updates()->mutable_upper_bounds();
  lin_con_lbs.add_ids(0);
  lin_con_lbs.add_values(3.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kLinearConstraint>("c");
  expected.SetAttr(DoubleAttr1::kLinConUb, AttrKey(0), 3.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, DeleteLinearConstraint) {
  Elemental elemental;
  elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.AddElement<ElementType::kLinearConstraint>("d");
  ModelUpdateProto update;
  update.add_deleted_linear_constraint_ids(0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kLinearConstraint, 1);
  expected.AddElement<ElementType::kLinearConstraint>("d");
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, ModifyLinearConstraintMatrix) {
  Elemental elemental;
  // Ensure that variable and constraint ids are different.
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 5);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);

  const VariableId z = y.Next();
  const LinearConstraintId d = c.Next();

  ModelUpdateProto update;
  VariablesProto& new_vars = *update.mutable_new_variables();
  new_vars.add_ids(z.value());
  new_vars.add_lower_bounds(-kInf);
  new_vars.add_upper_bounds(kInf);
  new_vars.add_integers(false);
  new_vars.add_names("z");
  LinearConstraintsProto& new_lin_cons =
      *update.mutable_new_linear_constraints();
  new_lin_cons.add_ids(d.value());
  new_lin_cons.add_lower_bounds(-kInf);
  new_lin_cons.add_upper_bounds(kInf);
  new_lin_cons.add_names("d");

  SparseDoubleMatrixProto& mat =
      *update.mutable_linear_constraint_matrix_updates();
  // Old constraint, old var, nonzero to zero.
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(0.0);
  // Old constraint, old var, zero to nonzero.
  mat.add_row_ids(c.value());
  mat.add_column_ids(y.value());
  mat.add_coefficients(10.0);
  // Old constraint, new var
  mat.add_row_ids(c.value());
  mat.add_column_ids(z.value());
  mat.add_coefficients(11.0);
  // New constraint, old var
  mat.add_row_ids(d.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(12.0);
  // New constraint, new var
  mat.add_row_ids(d.value());
  mat.add_column_ids(z.value());
  mat.add_coefficients(13.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 5);
  const VariableId x_expect = expected.AddElement<ElementType::kVariable>("x");
  const VariableId y_expect = expected.AddElement<ElementType::kVariable>("y");
  const VariableId z_expect = expected.AddElement<ElementType::kVariable>("z");
  const LinearConstraintId c_expect =
      expected.AddElement<ElementType::kLinearConstraint>("c");
  const LinearConstraintId d_expect =
      expected.AddElement<ElementType::kLinearConstraint>("d");
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c_expect, y_expect), 10.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c_expect, z_expect), 11.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(d_expect, x_expect), 12.0);
  expected.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(d_expect, z_expect), 13.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Primary Objective
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalUpdateFromProtoTest, UpdateObjectiveDirection) {
  Elemental elemental;
  elemental.SetAttr(BoolAttr0::kMaximize, {}, true);
  ModelUpdateProto update;
  update.mutable_objective_updates()->set_direction_update(false);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateObjectiveOffset) {
  Elemental elemental;
  ModelUpdateProto update;
  update.mutable_objective_updates()->set_offset_update(4.5);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.SetAttr(DoubleAttr0::kObjOffset, {}, 4.5);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateObjectivePriority) {
  Elemental elemental;
  ModelUpdateProto update;
  update.mutable_objective_updates()->set_priority_update(3);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.SetAttr(IntAttr0::kObjPriority, {}, 3);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateLinearObjCoef) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 3.0);

  const VariableId z = y.Next();
  ModelUpdateProto update;
  VariablesProto& new_vars = *update.mutable_new_variables();
  new_vars.add_ids(z.value());
  new_vars.add_lower_bounds(-kInf);
  new_vars.add_upper_bounds(kInf);
  new_vars.add_integers(false);
  new_vars.add_names("z");
  SparseDoubleVectorProto& lin_obj =
      *update.mutable_objective_updates()->mutable_linear_coefficients();
  // old nonzero to zero
  lin_obj.add_ids(x.value());
  lin_obj.add_values(0.0);
  // old zero to nonzero
  lin_obj.add_ids(y.value());
  lin_obj.add_values(4.0);
  // new to nonzero
  lin_obj.add_ids(z.value());
  lin_obj.add_values(5.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.AddElement<ElementType::kVariable>("z");
  expected.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(y), 4.0);
  expected.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(z), 5.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateQuadObjCoef) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 3.0);

  const VariableId z = y.Next();
  ModelUpdateProto update;
  VariablesProto& new_vars = *update.mutable_new_variables();
  new_vars.add_ids(z.value());
  new_vars.add_lower_bounds(-kInf);
  new_vars.add_upper_bounds(kInf);
  new_vars.add_integers(false);
  new_vars.add_names("z");
  SparseDoubleMatrixProto& quad_obj =
      *update.mutable_objective_updates()->mutable_quadratic_coefficients();
  // (old, old) nonzero to zero
  quad_obj.add_row_ids(x.value());
  quad_obj.add_column_ids(x.value());
  quad_obj.add_coefficients(0.0);
  // (old, old) zero to nonzero
  quad_obj.add_row_ids(x.value());
  quad_obj.add_column_ids(y.value());
  quad_obj.add_coefficients(10.0);
  // (old, new) to nonzero
  quad_obj.add_row_ids(x.value());
  quad_obj.add_column_ids(z.value());
  quad_obj.add_coefficients(11.0);
  // (new, new) to nonzero
  quad_obj.add_row_ids(z.value());
  quad_obj.add_column_ids(z.value());
  quad_obj.add_coefficients(12.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.AddElement<ElementType::kVariable>("z");
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, y), 10.0);
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, z), 11.0);
  expected.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(z, z), 12.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary Objectives
////////////////////////////////////////////////////////////////////////////////

TEST_P(ElementalUpdateFromProtoNameTest, AddAuxObjective) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  ModelUpdateProto update;
  ObjectiveProto& aux_obj = (*update.mutable_auxiliary_objectives_updates()
                                  ->mutable_new_objectives())[0];
  aux_obj.set_maximize(true);
  aux_obj.set_offset(3.0);
  aux_obj.set_priority(4);
  aux_obj.mutable_linear_coefficients()->add_ids(x.value());
  aux_obj.mutable_linear_coefficients()->add_values(5.0);
  if (HasNames()) {
    aux_obj.set_name("a");
  }

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  expected.AddElement<ElementType::kVariable>("x");
  const AuxiliaryObjectiveId a =
      expected.AddElement<ElementType::kAuxiliaryObjective>(HasNames() ? "a"
                                                                       : "");
  expected.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(a), true);
  expected.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 3.0);
  expected.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 4);
  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 5.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateAuxObjectiveDirection) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(a), true);
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[0]
      .set_direction_update(false);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kAuxiliaryObjective>("a");
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateAuxObjectiveOffset) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 4.5);
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[0]
      .set_offset_update(4.5);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kAuxiliaryObjective>("a");
  expected.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 4.5);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateAuxObjectivePriority) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 5);
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[0]
      .set_priority_update(5);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kAuxiliaryObjective>("a");
  expected.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 5);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateAuxLinearObjCoef) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 3.0);

  const VariableId z = y.Next();
  ModelUpdateProto update;
  VariablesProto& new_vars = *update.mutable_new_variables();
  new_vars.add_ids(z.value());
  new_vars.add_lower_bounds(-kInf);
  new_vars.add_upper_bounds(kInf);
  new_vars.add_integers(false);
  new_vars.add_names("z");
  SparseDoubleVectorProto& lin_obj =
      *(*update.mutable_auxiliary_objectives_updates()
             ->mutable_objective_updates())[0]
           .mutable_linear_coefficients();
  // old nonzero to zero
  lin_obj.add_ids(x.value());
  lin_obj.add_values(0.0);
  // old zero to nonzero
  lin_obj.add_ids(y.value());
  lin_obj.add_values(4.0);
  // new to nonzero
  lin_obj.add_ids(z.value());
  lin_obj.add_values(5.0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.AddElement<ElementType::kAuxiliaryObjective>("a");
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.AddElement<ElementType::kVariable>("z");
  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, y), 4.0);
  expected.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, z), 5.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, UpdateAuxQuadObjCoefNotSupported) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  ModelUpdateProto update;
  SparseDoubleMatrixProto& quad_obj =
      *(*update.mutable_auxiliary_objectives_updates()
             ->mutable_objective_updates())[a.value()]
           .mutable_quadratic_coefficients();
  // old nonzero to zero
  quad_obj.add_row_ids(x.value());
  quad_obj.add_column_ids(x.value());
  quad_obj.add_coefficients(1.0);

  EXPECT_THAT(elemental.ApplyUpdateProto(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic coefficients are not supported")));
}

TEST(ElementalUpdateFromProtoTest, DeleteAuxObj) {
  Elemental elemental;
  elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.AddElement<ElementType::kAuxiliaryObjective>("b");
  ModelUpdateProto update;
  update.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(0);

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kAuxiliaryObjective,
                                             1);
  expected.AddElement<ElementType::kAuxiliaryObjective>("b");
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic Constraints
////////////////////////////////////////////////////////////////////////////////

TEST_P(ElementalUpdateFromProtoNameTest, AddQuadraticConstraint) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  ModelUpdateProto update;
  QuadraticConstraintProto& quad_con =
      (*update.mutable_quadratic_constraint_updates()
            ->mutable_new_constraints())[0];
  quad_con.set_lower_bound(3.0);
  quad_con.set_upper_bound(4.0);
  SparseDoubleVectorProto& lin_terms = *quad_con.mutable_linear_terms();
  lin_terms.add_ids(x.value());
  lin_terms.add_values(5.0);
  lin_terms.add_ids(y.value());
  lin_terms.add_values(6.0);
  SparseDoubleMatrixProto& quad_terms = *quad_con.mutable_quadratic_terms();
  quad_terms.add_row_ids(x.value());
  quad_terms.add_column_ids(x.value());
  quad_terms.add_coefficients(7.0);
  quad_terms.add_row_ids(x.value());
  quad_terms.add_column_ids(y.value());
  quad_terms.add_coefficients(8.0);
  if (HasNames()) {
    quad_con.set_name("q");
  }

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  const QuadraticConstraintId q =
      expected.AddElement<ElementType::kQuadraticConstraint>(HasNames() ? "q"
                                                                        : "");
  expected.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(q), 3.0);
  expected.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(q), 4.0);
  expected.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(q, x), 5.0);
  expected.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(q, y), 6.0);
  using Key = AttrKeyFor<SymmetricDoubleAttr3>;
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, Key(q, x, x), 7.0);
  expected.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, Key(q, x, y), 8.0);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, DeleteQuadraticConstraint) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const QuadraticConstraintId q =
      elemental.AddElement<ElementType::kQuadraticConstraint>("q");
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(q), 4.0);
  using Key = AttrKeyFor<SymmetricDoubleAttr3>;
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef, Key(q, x, x), 7.0);

  ModelUpdateProto update;
  update.mutable_quadratic_constraint_updates()->add_deleted_constraint_ids(
      q.value());

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  expected.AddElement<ElementType::kVariable>("x");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kQuadraticConstraint,
                                             1);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Indicator Constraints
////////////////////////////////////////////////////////////////////////////////

TEST_P(ElementalUpdateFromProtoNameTest, AddIndicatorConstraint) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  const VariableId z = elemental.AddElement<ElementType::kVariable>("z");
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(z), 0.0);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(z), 1.0);
  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(z), true);
  ModelUpdateProto update;
  IndicatorConstraintProto& ind_con =
      (*update.mutable_indicator_constraint_updates()
            ->mutable_new_constraints())[0];
  ind_con.set_lower_bound(3.0);
  ind_con.set_upper_bound(4.0);
  SparseDoubleVectorProto& lin_terms = *ind_con.mutable_expression();
  lin_terms.add_ids(x.value());
  lin_terms.add_values(5.0);
  lin_terms.add_ids(y.value());
  lin_terms.add_values(6.0);
  ind_con.set_activate_on_zero(true);
  ind_con.set_indicator_id(z.value());
  if (HasNames()) {
    ind_con.set_name("c");
  }

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  expected.AddElement<ElementType::kVariable>("x");
  expected.AddElement<ElementType::kVariable>("y");
  expected.AddElement<ElementType::kVariable>("z");
  expected.SetAttr(DoubleAttr1::kVarLb, AttrKey(z), 0.0);
  expected.SetAttr(DoubleAttr1::kVarUb, AttrKey(z), 1.0);
  expected.SetAttr(BoolAttr1::kVarInteger, AttrKey(z), true);
  const IndicatorConstraintId c =
      expected.AddElement<ElementType::kIndicatorConstraint>(HasNames() ? "c"
                                                                        : "");
  expected.SetAttr(DoubleAttr1::kIndConLb, AttrKey(c), 3.0);
  expected.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c), 4.0);
  expected.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, x), 5.0);
  expected.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, y), 6.0);
  expected.SetAttr(BoolAttr1::kIndConActivateOnZero, AttrKey(c), true);
  expected.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c), VariableId(z));
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

TEST(ElementalUpdateFromProtoTest, DeleteIndicatorConstraint) {
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c), 4.0);
  elemental.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, x), 5.0);

  ModelUpdateProto update;
  update.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(
      c.value());

  ASSERT_OK(elemental.ApplyUpdateProto(update));

  Elemental expected;
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 10);
  expected.AddElement<ElementType::kVariable>("x");
  expected.EnsureNextElementIdAtLeastUntyped(ElementType::kIndicatorConstraint,
                                             1);
  EXPECT_THAT(elemental, EquivToElemental(expected));
}

////////////////////////////////////////////////////////////////////////////////
// Unsupported features
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalUpdateFromProtoTest, SecondOrderConeNotSupported) {
  Elemental elemental;
  ModelUpdateProto update_proto;
  update_proto.mutable_second_order_cone_constraint_updates()
      ->add_deleted_constraint_ids(0);
  EXPECT_THAT(elemental.ApplyUpdateProto(update_proto),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("second order cone")));
}

TEST(ElementalUpdateFromProtoTest, Sos1NotSupported) {
  Elemental elemental;
  ModelUpdateProto update_proto;
  update_proto.mutable_sos1_constraint_updates()->add_deleted_constraint_ids(0);
  EXPECT_THAT(elemental.ApplyUpdateProto(update_proto),
              StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos1")));
}

TEST(ElementalUpdateFromProtoTest, Sos2NotSupported) {
  Elemental elemental;
  ModelUpdateProto update_proto;
  update_proto.mutable_sos2_constraint_updates()->add_deleted_constraint_ids(0);
  EXPECT_THAT(elemental.ApplyUpdateProto(update_proto),
              StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos2")));
}

// Note: the value is HasNames() in the test.
INSTANTIATE_TEST_SUITE_P(ElementalUpdateFromProtoNameTestSuite,
                         ElementalUpdateFromProtoNameTest,
                         testing::Values(false, true));

}  // namespace
}  // namespace operations_research::math_opt
