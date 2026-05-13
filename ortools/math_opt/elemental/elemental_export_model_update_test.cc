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
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::proto::Partially;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(ExportModelUpdateTest, NoChangesReturnsNullOpt) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  EXPECT_THAT(elemental.ExportModelUpdate(d), IsOkAndHolds(Eq(std::nullopt)));
}

TEST(ExportModelUpdateTest, DiffFromWrongModelError) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  Elemental elemental2;
  EXPECT_THAT(elemental2.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("another Elemental")));
}

TEST(ExportModelUpdateTest, DiffWasAlreadyDeleted) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.DeleteDiff(d);
  EXPECT_THAT(
      elemental.ExportModelUpdate(d),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("has no diff")));
}

////////////////////////////////////////////////////////////////////////////////
// Variable updates
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, NewVariable) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(x), 0.0);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 2.0);
  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(x), true);

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(0);
  vars.add_lower_bounds(0.0);
  vars.add_upper_bounds(2.0);
  vars.add_integers(true);
  vars.add_names("x");

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));

  vars.clear_names();
  EXPECT_THAT(elemental.ExportModelUpdate(d, /*remove_names=*/true),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, VarLb) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(x), 3.0);

  ModelUpdateProto expected;
  auto& lbs = *expected.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(0);
  lbs.add_values(3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, VarUb) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 3.0);

  ModelUpdateProto expected;
  auto& ubs = *expected.mutable_variable_updates()->mutable_upper_bounds();
  ubs.add_ids(0);
  ubs.add_values(3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, VarInteger) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(BoolAttr1::kVarInteger, AttrKey(x), true);

  ModelUpdateProto expected;
  auto& var_ints = *expected.mutable_variable_updates()->mutable_integers();
  var_ints.add_ids(0);
  var_ints.add_values(true);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeleteVar) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.DeleteElement(x);

  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Objective Updates
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, ObjectiveDirectionToMaximize) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_direction_update(true);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ObjectiveDirectionToMinimize) {
  Elemental elemental;
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), false);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_direction_update(false);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ObjectiveOffsetToNonzero) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.3);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_offset_update(4.3);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ObjectiveOffsetToZero) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.3);
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 0.0);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_offset_update(0.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ObjectivePriorityToNonzero) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), 4);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_priority_update(4);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ObjectivePriorityToZero) {
  Elemental elemental;
  elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), 4);
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(IntAttr0::kObjPriority, AttrKey(), 0);

  ModelUpdateProto expected;
  expected.mutable_objective_updates()->set_priority_update(0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinearObjectiveCoefficientOldVariableToNonzero) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 4.3);

  ModelUpdateProto expected;
  SparseDoubleVectorProto& obj =
      *expected.mutable_objective_updates()->mutable_linear_coefficients();
  obj.add_ids(x.value());
  obj.add_values(4.3);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinearObjectiveCoefficientOldVariableToZero) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 4.3);
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 0.0);

  ModelUpdateProto expected;
  SparseDoubleVectorProto& obj =
      *expected.mutable_objective_updates()->mutable_linear_coefficients();
  obj.add_ids(x.value());
  obj.add_values(0.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinearObjectiveCoefficientNewVariable) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(x), 4.3);

  ModelUpdateProto expected;
  VariablesProto& vars = *expected.mutable_new_variables();
  vars.add_ids(0);
  vars.add_names("x");
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  SparseDoubleVectorProto& obj =
      *expected.mutable_objective_updates()->mutable_linear_coefficients();
  obj.add_ids(x.value());
  obj.add_values(4.3);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, QuadraticObjectiveCoefficientOldOldToNonzero) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 4.3);

  ModelUpdateProto expected;
  SparseDoubleMatrixProto& obj =
      *expected.mutable_objective_updates()->mutable_quadratic_coefficients();
  obj.add_row_ids(x.value());
  obj.add_column_ids(x.value());
  obj.add_coefficients(4.3);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, QuadraticObjectiveCoefficientOldOldToZero) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 4.3);

  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 0.0);

  ModelUpdateProto expected;
  SparseDoubleMatrixProto& obj =
      *expected.mutable_objective_updates()->mutable_quadratic_coefficients();
  obj.add_row_ids(x.value());
  obj.add_column_ids(x.value());
  obj.add_coefficients(0.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, QuadraticObjectiveCoefficientOneNewVariable) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");

  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, y), 4.3);

  ModelUpdateProto expected;
  VariablesProto& vars = *expected.mutable_new_variables();
  vars.add_ids(1);
  vars.add_names("y");
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  SparseDoubleMatrixProto& obj =
      *expected.mutable_objective_updates()->mutable_quadratic_coefficients();
  obj.add_row_ids(x.value());
  obj.add_column_ids(y.value());
  obj.add_coefficients(4.3);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, QuadraticObjectiveCoefficientTwoNewVariables) {
  Elemental elemental;

  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 4.3);

  ModelUpdateProto expected;
  VariablesProto& vars = *expected.mutable_new_variables();
  vars.add_ids(x.value());
  vars.add_names("x");
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  SparseDoubleMatrixProto& obj =
      *expected.mutable_objective_updates()->mutable_quadratic_coefficients();
  obj.add_row_ids(x.value());
  obj.add_column_ids(x.value());
  obj.add_coefficients(4.3);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeletedVariableNotInUpdate) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;

  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 4.3);
  elemental.DeleteElement(x);

  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(x.value());
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, NewEmptyAuxiliaryObjective) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.AddElement<ElementType::kAuxiliaryObjective>("");

  ModelUpdateProto expected;
  (*expected.mutable_auxiliary_objectives_updates()
        ->mutable_new_objectives())[0];
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, NewAuxiliaryObjectiveFilledIn) {
  Elemental elemental;
  // Ensure x and a are different.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(a), true);
  elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 3);
  elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 4.0);
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 5.0);

  ModelUpdateProto expected;
  ObjectiveProto& obj = (*expected.mutable_auxiliary_objectives_updates()
                              ->mutable_new_objectives())[0];
  obj.set_name("a");
  obj.set_maximize(true);
  obj.set_priority(3);
  obj.set_offset(4.0);
  obj.mutable_linear_coefficients()->add_ids(1);
  obj.mutable_linear_coefficients()->add_values(5.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeleteAuxiliaryObjective) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.DeleteElement(a);

  ModelUpdateProto expected;
  expected.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, NoChangesNoAuxObjUpdates) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 3.0);
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.AddElement<ElementType::kVariable>("y");

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("y");
  // No auxiliary objective updates!

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyAuxObjOldAndNewVars) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 3.0);
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, y), 4.0);

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(1);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("y");
  ObjectiveUpdatesProto& obj =
      (*expected.mutable_auxiliary_objectives_updates()
            ->mutable_objective_updates())[0];
  SparseDoubleVectorProto& lin_coef = *obj.mutable_linear_coefficients();
  lin_coef.add_ids(0);
  lin_coef.add_ids(1);
  lin_coef.add_values(3.0);
  lin_coef.add_values(4.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, NewAuxObjAndNewVarCountOnlyOnce) {
  Elemental elemental;
  // Ensure a != x below, and we need an existing auxiliary objective to hit
  // all codepaths.
  elemental.AddElement<ElementType::kAuxiliaryObjective>("");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  elemental.SetAttr(DoubleAttr2::kAuxObjLinCoef, AttrKey(a, x), 3.0);

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(0);
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("x");
  ObjectiveProto& obj = (*expected.mutable_auxiliary_objectives_updates()
                              ->mutable_new_objectives())[1];
  obj.set_name("a");
  obj.mutable_linear_coefficients()->add_ids(0);
  obj.mutable_linear_coefficients()->add_values(3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyOffset) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(a), 4.0);

  ModelUpdateProto expected;
  ObjectiveUpdatesProto& obj =
      (*expected.mutable_auxiliary_objectives_updates()
            ->mutable_objective_updates())[0];
  obj.set_offset_update(4.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyDirection) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(a), true);

  ModelUpdateProto expected;
  ObjectiveUpdatesProto& obj =
      (*expected.mutable_auxiliary_objectives_updates()
            ->mutable_objective_updates())[0];
  obj.set_direction_update(true);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyPriority) {
  Elemental elemental;
  const AuxiliaryObjectiveId a =
      elemental.AddElement<ElementType::kAuxiliaryObjective>("a");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(a), 3);

  ModelUpdateProto expected;
  ObjectiveUpdatesProto& obj =
      (*expected.mutable_auxiliary_objectives_updates()
            ->mutable_objective_updates())[0];
  obj.set_priority_update(3);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, NewLinearConstraint) {
  Elemental elemental;
  const Elemental::DiffHandle d = elemental.AddDiff();
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kLinConLb, AttrKey(c), 0.0);
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(c), 2.0);

  ModelUpdateProto expected;
  auto& lin_cons = *expected.mutable_new_linear_constraints();
  lin_cons.add_ids(0);
  lin_cons.add_lower_bounds(0.0);
  lin_cons.add_upper_bounds(2.0);
  lin_cons.add_names("c");

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));

  lin_cons.clear_names();
  EXPECT_THAT(elemental.ExportModelUpdate(d, /*remove_names=*/true),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConLb) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kLinConLb, AttrKey(c), 3.0);

  ModelUpdateProto expected;
  auto& lbs =
      *expected.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(0);
  lbs.add_values(3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConUb) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(c), 3.0);

  ModelUpdateProto expected;
  auto& ubs =
      *expected.mutable_linear_constraint_updates()->mutable_upper_bounds();
  ubs.add_ids(0);
  ubs.add_values(3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeleteLinCon) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.DeleteElement(c);

  ModelUpdateProto expected;
  expected.add_deleted_linear_constraint_ids(0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeletedConstraintNotAlsoInUpdate) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr1::kLinConLb, AttrKey(c), -1.0);
  elemental.SetAttr(DoubleAttr1::kLinConUb, AttrKey(c), 1.0);
  elemental.DeleteElement(c);

  ModelUpdateProto expected;
  expected.add_deleted_linear_constraint_ids(0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraint Coefficients
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, LinConCoefOldVarOldConstraint) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.0);

  ModelUpdateProto expected;
  auto& mat = *expected.mutable_linear_constraint_matrix_updates();
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(3.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConCoefNewVarOldConstraint) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.0);

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(x.value());
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("x");
  auto& mat = *expected.mutable_linear_constraint_matrix_updates();
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(3.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConCoefOldVarNewConstraint) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.0);

  ModelUpdateProto expected;
  auto& lin_cons = *expected.mutable_new_linear_constraints();
  lin_cons.add_ids(c.value());
  lin_cons.add_lower_bounds(-kInf);
  lin_cons.add_upper_bounds(kInf);
  lin_cons.add_names("c");
  auto& mat = *expected.mutable_linear_constraint_matrix_updates();
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(3.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConCoefNewVarNewConstraint) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const Elemental::DiffHandle d = elemental.AddDiff();
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.0);

  ModelUpdateProto expected;
  auto& vars = *expected.mutable_new_variables();
  vars.add_ids(x.value());
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("x");
  auto& lin_cons = *expected.mutable_new_linear_constraints();
  lin_cons.add_ids(c.value());
  lin_cons.add_lower_bounds(-kInf);
  lin_cons.add_upper_bounds(kInf);
  lin_cons.add_names("c");
  auto& mat = *expected.mutable_linear_constraint_matrix_updates();
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(3.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConCoefSortsWithinAndOverGroups) {
  Elemental elemental;
  // Add some unused variables so variables so variable and constraint ids are
  // different.
  for (int i = 0; i < 10; ++i) {
    elemental.AddElement<ElementType::kVariable>("");
  }
  std::vector<VariableId> variables;
  std::vector<LinearConstraintId> constraints;
  const int kNumOld = 3;
  for (int i = 0; i < kNumOld; ++i) {
    variables.push_back(
        elemental.AddElement<ElementType::kVariable>(absl::StrCat("x_", i)));
    constraints.push_back(elemental.AddElement<ElementType::kLinearConstraint>(
        absl::StrCat("c_", i)));
  }

  const Elemental::DiffHandle d = elemental.AddDiff();
  const int kNumNew = 3;
  const int kTotal = kNumOld + kNumNew;
  for (int i = kNumNew; i < kTotal; ++i) {
    variables.push_back(
        elemental.AddElement<ElementType::kVariable>(absl::StrCat("x_", i)));
    constraints.push_back(elemental.AddElement<ElementType::kLinearConstraint>(
        absl::StrCat("c_", i)));
  }

  for (int c = 0; c < kTotal; ++c) {
    for (int x = 0; x < kTotal; ++x) {
      elemental.SetAttr(DoubleAttr2::kLinConCoef,
                        AttrKey(constraints[c], variables[x]), 10 * c + x + 1);
    }
  }

  ModelUpdateProto expected;
  SparseDoubleMatrixProto& mat =
      *expected.mutable_linear_constraint_matrix_updates();
  for (int c = 0; c < kTotal; ++c) {
    for (int x = 0; x < kTotal; ++x) {
      mat.add_row_ids(constraints[c].value());
      mat.add_column_ids(variables[x].value());
      mat.add_coefficients(10 * c + x + 1);
    }
  }
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(Partially(EqualsProto(expected)))));
}

TEST(ExportModelUpdateTest, LinConSetOldCoefToZeroInUpdate) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.0);
  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);

  ModelUpdateProto expected;
  auto& mat = *expected.mutable_linear_constraint_matrix_updates();
  mat.add_row_ids(c.value());
  mat.add_column_ids(x.value());
  mat.add_coefficients(0.0);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, LinConSetNewCoefToZeroNotInUpdate) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d), IsOkAndHolds(Eq(std::nullopt)));
}

TEST(ExportModelUpdateTest, DeletedLinConNotInMatrix) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  elemental.DeleteElement(c);

  ModelUpdateProto expected;
  expected.add_deleted_linear_constraint_ids(c.value());
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeletedVarNotInMatrix) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  elemental.DeleteElement(x);

  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(x.value());
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic Constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ExportModelUpdateTest, AddQuadraticConstraintOldVars) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();

  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 4.0);
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr3>(c, x, x), 5.0);

  ModelUpdateProto expected;
  QuadraticConstraintProto con;
  con.set_name("c");
  con.set_lower_bound(2.0);
  con.set_upper_bound(3.0);
  con.mutable_linear_terms()->add_ids(x.value());
  con.mutable_linear_terms()->add_values(4.0);
  con.mutable_quadratic_terms()->add_row_ids(x.value());
  con.mutable_quadratic_terms()->add_column_ids(x.value());
  con.mutable_quadratic_terms()->add_coefficients(5.0);
  (*expected.mutable_quadratic_constraint_updates()
        ->mutable_new_constraints())[c.value()] = std::move(con);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, AddQuadraticConstraintNewVars) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const Elemental::DiffHandle d = elemental.AddDiff();

  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 4.0);
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr3>(c, x, x), 5.0);

  ModelUpdateProto expected;
  VariablesProto& vars = *expected.mutable_new_variables();
  vars.add_ids(x.value());
  vars.add_lower_bounds(-kInf);
  vars.add_upper_bounds(kInf);
  vars.add_integers(false);
  vars.add_names("x");
  QuadraticConstraintProto con;
  con.set_name("c");
  con.set_lower_bound(2.0);
  con.set_upper_bound(3.0);
  con.mutable_linear_terms()->add_ids(x.value());
  con.mutable_linear_terms()->add_values(4.0);
  con.mutable_quadratic_terms()->add_row_ids(x.value());
  con.mutable_quadratic_terms()->add_column_ids(x.value());
  con.mutable_quadratic_terms()->add_coefficients(5.0);
  (*expected.mutable_quadratic_constraint_updates()
        ->mutable_new_constraints())[c.value()] = std::move(con);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, DeleteQuadraticConstraint) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 4.0);
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr3>(c, x, x), 5.0);

  const Elemental::DiffHandle d = elemental.AddDiff();
  elemental.DeleteElement(c);

  ModelUpdateProto expected;
  expected.mutable_quadratic_constraint_updates()->add_deleted_constraint_ids(
      c.value());
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyQuadraticConstraintLbUnsupported) {
  Elemental elemental;
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(c), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic_constraint_lower_bound")));
}

TEST(ExportModelUpdateTest, ModifyQuadraticConstraintUbUnsupported) {
  Elemental elemental;
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic_constraint_upper_bound")));
}

TEST(ExportModelUpdateTest, ModifyQuadraticConstraintLinCoefsUnsupported) {
  Elemental elemental;
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic_constraint_linear_coefficient")));
}

TEST(ExportModelUpdateTest, ModifyQuadraticConstraintQuadCoefsUnsupported) {
  Elemental elemental;
  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr3>(c, x, x), 3.0);

  EXPECT_THAT(
      elemental.ExportModelUpdate(d),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("quadratic_constraint_quadratic_coefficient")));
}

TEST(ExportModelUpdateTest, DeletedVariableExcludedFromTerms) {
  Elemental elemental;
  // Add an unused variable to ensure x and c have different values.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle d = elemental.AddDiff();

  const QuadraticConstraintId c =
      elemental.AddElement<ElementType::kQuadraticConstraint>("c");
  elemental.SetAttr(DoubleAttr1::kQuadConLb, AttrKey(c), 2.0);
  elemental.SetAttr(DoubleAttr1::kQuadConUb, AttrKey(c), 3.0);
  elemental.SetAttr(DoubleAttr2::kQuadConLinCoef, AttrKey(c, x), 4.0);
  elemental.SetAttr(SymmetricDoubleAttr3::kQuadConQuadCoef,
                    AttrKeyFor<SymmetricDoubleAttr3>(c, x, x), 5.0);
  elemental.DeleteElement(x);

  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(x.value());
  QuadraticConstraintProto con;
  con.set_name("c");
  con.set_lower_bound(2.0);
  con.set_upper_bound(3.0);
  (*expected.mutable_quadratic_constraint_updates()
        ->mutable_new_constraints())[c.value()] = std::move(con);
  EXPECT_THAT(elemental.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

////////////////////////////////////////////////////////////////////////////////
// Indicator Constraints
////////////////////////////////////////////////////////////////////////////////

class IndicatorTest : public testing::Test {
 protected:
  // Call only once.
  void AddVariables() {
    CHECK(!x_.IsValid());
    // Add an unused variable to ensure x and c have different values.
    elemental_.AddElement<ElementType::kVariable>("");
    x_ = elemental_.AddElement<ElementType::kVariable>("x");
    y_ = elemental_.AddElement<ElementType::kVariable>("y");
    elemental_.SetAttr(BoolAttr1::kVarInteger, AttrKey(y_), true);
    elemental_.SetAttr(DoubleAttr1::kVarLb, AttrKey(y_), 0.0);
    elemental_.SetAttr(DoubleAttr1::kVarUb, AttrKey(y_), 1.0);
  }

  static VariablesProto MakeVariablesProto() {
    VariablesProto vars;
    vars.add_ids(0);
    vars.add_lower_bounds(-kInf);
    vars.add_upper_bounds(kInf);
    vars.add_integers(false);
    vars.add_names("");
    vars.add_ids(1);
    vars.add_lower_bounds(-kInf);
    vars.add_upper_bounds(kInf);
    vars.add_integers(false);
    vars.add_names("x");
    vars.add_ids(2);
    vars.add_lower_bounds(0);
    vars.add_upper_bounds(1);
    vars.add_integers(true);
    vars.add_names("y");
    return vars;
  }

  // Call only after AddVariables(), call only once.
  void AddIndicator() {
    CHECK(!c_.IsValid());
    c_ = elemental_.AddElement<ElementType::kIndicatorConstraint>("c");
    elemental_.SetAttr(DoubleAttr1::kIndConLb, AttrKey(c_), 2.0);
    elemental_.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c_), 3.0);
    elemental_.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c_, x_), 4.0);
    elemental_.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c_), y_);
    elemental_.SetAttr(BoolAttr1::kIndConActivateOnZero, AttrKey(c_), true);
  }

  static IndicatorConstraintProto MakeIndConProto() {
    IndicatorConstraintProto con;
    con.set_name("c");
    con.set_lower_bound(2.0);
    con.set_upper_bound(3.0);
    con.mutable_expression()->add_ids(1);  // x
    con.mutable_expression()->add_values(4.0);
    con.set_indicator_id(2);  // y
    con.set_activate_on_zero(true);
    return con;
  }

  Elemental elemental_;
  VariableId x_;
  VariableId y_;
  IndicatorConstraintId c_;
};

TEST_F(IndicatorTest, AddIndicatorConstraintOldVars) {
  AddVariables();
  const Elemental::DiffHandle d = elemental_.AddDiff();
  AddIndicator();

  ModelUpdateProto expected;

  (*expected.mutable_indicator_constraint_updates()
        ->mutable_new_constraints())[c_.value()] = MakeIndConProto();
  EXPECT_THAT(elemental_.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST_F(IndicatorTest, AddIndicatorConstraintNewVars) {
  const Elemental::DiffHandle d = elemental_.AddDiff();
  AddVariables();
  AddIndicator();

  ModelUpdateProto expected;
  *expected.mutable_new_variables() = MakeVariablesProto();
  (*expected.mutable_indicator_constraint_updates()
        ->mutable_new_constraints())[c_.value()] = MakeIndConProto();
  EXPECT_THAT(elemental_.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST_F(IndicatorTest, DeleteIndicatorConstraint) {
  AddVariables();
  AddIndicator();
  const Elemental::DiffHandle d = elemental_.AddDiff();
  elemental_.DeleteElement(c_);

  ModelUpdateProto expected;
  expected.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(
      c_.value());
  EXPECT_THAT(elemental_.ExportModelUpdate(d),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ExportModelUpdateTest, ModifyIndicatorConstraintLbUnsupported) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr1::kIndConLb, AttrKey(c), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint_lower_bound")));
}

TEST(ExportModelUpdateTest, ModifyIndicatorConstraintUbUnsupported) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr1::kIndConUb, AttrKey(c), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint_upper_bound")));
}

TEST(ExportModelUpdateTest, ModifyIndicatorConstraintExpressionUnsupported) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(DoubleAttr2::kIndConLinCoef, AttrKey(c, x), 3.0);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint_linear_coefficient")));
}

TEST(ExportModelUpdateTest, ModifyIndicatorConstraintIndicatorUnsupported) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(c), VariableId(x));

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint_indicator")));
}

TEST(ExportModelUpdateTest, ModifyIndicatorConstraintActiveOnZeroUnsupported) {
  Elemental elemental;
  const IndicatorConstraintId c =
      elemental.AddElement<ElementType::kIndicatorConstraint>("c");

  const Elemental::DiffHandle d = elemental.AddDiff();

  elemental.SetAttr(BoolAttr1::kIndConActivateOnZero, AttrKey(c), true);

  EXPECT_THAT(elemental.ExportModelUpdate(d),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint_activate_on_zero")));
}

}  // namespace

}  // namespace operations_research::math_opt
