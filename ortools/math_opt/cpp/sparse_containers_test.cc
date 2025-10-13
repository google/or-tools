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

#include "ortools/math_opt/cpp/sparse_containers.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(DoubleVariableValuesFromProtoTest, Empty) {
  ModelStorage model;
  model.AddVariable("x");

  const SparseDoubleVectorProto proto;

  EXPECT_THAT(VariableValuesFromProto(&model, proto), IsOkAndHolds(IsEmpty()));
}

TEST(DoubleVariableValuesFromProtoTest, NonEmpty) {
  ModelStorage model;
  const VariableId x = model.AddVariable("x");
  model.AddVariable("y");
  const VariableId z = model.AddVariable("z");

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_ids(2);
  proto.add_values(3.0);
  proto.add_values(-2.0);

  VariableMap<double> expected = {{Variable(&model, x), 3.0},
                                  {Variable(&model, z), -2.0}};

  EXPECT_THAT(VariableValuesFromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0.0)));
}

TEST(DoubleVariableValuesFromProtoTest, UnequalSize) {
  ModelStorage model;
  model.AddVariable("x");

  SparseDoubleVectorProto proto;
  proto.add_ids(0);

  EXPECT_THAT(VariableValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DoubleVariableValuesFromProtoTest, OutOfOrder) {
  ModelStorage model;
  model.AddVariable("x");
  model.AddVariable("y");

  SparseDoubleVectorProto proto;
  proto.add_ids(1);
  proto.add_ids(0);
  proto.add_values(1.0);
  proto.add_values(1.0);

  EXPECT_THAT(VariableValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DoubleVariableValuesFromProtoTest, NotInModel) {
  ModelStorage model;

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_values(1.0);

  EXPECT_THAT(VariableValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no variable with id")));
}

TEST(DoubleVariableValuesFromProtoTest, SmallMap) {
  ModelStorage model;
  const VariableId x = model.AddVariable("x");
  model.AddVariable("y");
  const VariableId z = model.AddVariable("z");

  const VariableMap<double> input = {{Variable(&model, x), 3.0},
                                     {Variable(&model, z), -2.0}};

  EXPECT_THAT(VariableValuesFromProto(&model, VariableValuesToProto(input)),
              IsOkAndHolds(IsNear(input, /*tolerance=*/0.0)));
}

TEST(Int32VariableValuesFromProtoTest, UnequalSize) {
  Model model;
  model.AddVariable("x");

  SparseInt32VectorProto proto;
  proto.add_ids(0);

  EXPECT_THAT(VariableValuesFromProto(model.storage(), proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Int32VariableValuesFromProtoTest, VariableDoesNotExist) {
  Model model;
  model.AddVariable("x");

  SparseInt32VectorProto proto;
  proto.add_ids(1);
  proto.add_values(12);

  EXPECT_THAT(VariableValuesFromProto(model.storage(), proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Int32VariableValuesFromProtoTest, SmallMap) {
  Model model;
  const Variable x = model.AddVariable("x");
  model.AddVariable("y");
  const Variable z = model.AddVariable("z");

  SparseInt32VectorProto proto;
  proto.add_ids(0);
  proto.add_ids(2);
  proto.add_values(12);
  proto.add_values(11);

  EXPECT_THAT(VariableValuesFromProto(model.storage(), proto),
              IsOkAndHolds(UnorderedElementsAre(Pair(testing::Eq(x), 12),
                                                Pair(testing::Eq(z), 11))));
}

TEST(AuxiliaryObjectiveValuesFromProtoTest, Empty) {
  ModelStorage model;
  EXPECT_THAT(AuxiliaryObjectiveValuesFromProto(
                  &model, google::protobuf::Map<int64_t, double>()),
              IsOkAndHolds(IsEmpty()));
}

TEST(AuxiliaryObjectiveValuesFromProtoTest, NonEmpty) {
  ModelStorage model;
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(2));

  google::protobuf::Map<int64_t, double> proto;
  proto[o.id().value()] = 3.0;

  EXPECT_THAT(AuxiliaryObjectiveValuesFromProto(&model, proto),
              IsOkAndHolds(UnorderedElementsAre(Pair(o, 3.0))));
}

TEST(AuxiliaryObjectiveValuesFromProtoTest, NotInModel) {
  ModelStorage model;

  google::protobuf::Map<int64_t, double> proto;
  proto[0] = 3.0;

  EXPECT_THAT(AuxiliaryObjectiveValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no auxiliary objective with id")));
}

TEST(AuxiliaryObjectiveValuesToProtoTest, Empty) {
  EXPECT_THAT(AuxiliaryObjectiveValuesToProto({}), IsEmpty());
}

TEST(AuxiliaryObjectiveValuesToProtoTest, NonEmpty) {
  ModelStorage model;
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(2));

  const absl::flat_hash_map<Objective, double> input = {{o, 3.0}};

  EXPECT_THAT(AuxiliaryObjectiveValuesToProto(input),
              UnorderedElementsAre(Pair(o.id().value(), 3.0)));
}

TEST(AuxiliaryObjectiveValuesToProtoDeathTest, PrimaryObjectiveInMap) {
  ModelStorage model;
  const Objective o = Objective::Primary(&model);

  const absl::flat_hash_map<Objective, double> input = {{o, 3.0}};
  EXPECT_DEATH(AuxiliaryObjectiveValuesToProto(input), "primary");
}

TEST(LinearConstraintValuesFromProtoTest, EmptySuccess) {
  ModelStorage model;
  model.AddLinearConstraint("c");

  SparseDoubleVectorProto proto;

  EXPECT_THAT(LinearConstraintValuesFromProto(&model, proto),
              IsOkAndHolds(IsEmpty()));
}

TEST(LinearConstraintValuesFromProtoTest, NonEmptySuccess) {
  ModelStorage model;
  const LinearConstraintId c = model.AddLinearConstraint("c");
  model.AddLinearConstraint("d");
  const LinearConstraintId e = model.AddLinearConstraint("e");

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_ids(2);
  proto.add_values(3.0);
  proto.add_values(-2.0);

  LinearConstraintMap<double> expected = {{LinearConstraint(&model, c), 3.0},
                                          {LinearConstraint(&model, e), -2.0}};

  EXPECT_THAT(LinearConstraintValuesFromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0.0)));
}

TEST(LinearConstraintValuesFromProtoTest, UnequalSize) {
  ModelStorage model;
  model.AddLinearConstraint("c");

  SparseDoubleVectorProto proto;
  proto.add_ids(0);

  EXPECT_THAT(LinearConstraintValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(LinearConstraintValuesFromProtoTest, NotInModel) {
  ModelStorage model;

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_values(1.0);

  EXPECT_THAT(LinearConstraintValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no linear constraint with id")));
}

TEST(LinearConstraintValuesToProtoTest, Empty) {
  ModelStorage model;
  EXPECT_THAT(LinearConstraintValuesFromProto(
                  &model, LinearConstraintValuesToProto({})),
              IsOkAndHolds(IsEmpty()));
}

TEST(LinearConstraintValuesToProtoTest, SmallMap) {
  ModelStorage model;
  const LinearConstraintId c = model.AddLinearConstraint("c");
  model.AddLinearConstraint("d");
  const LinearConstraintId e = model.AddLinearConstraint("e");

  const LinearConstraintMap<double> input = {
      {LinearConstraint(&model, c), 3.0}, {LinearConstraint(&model, e), -2.0}};

  EXPECT_THAT(LinearConstraintValuesFromProto(
                  &model, LinearConstraintValuesToProto(input)),
              IsOkAndHolds(IsNear(input, /*tolerance=*/0.0)));
}

TEST(QuadraticConstraintValuesFromProtoTest, EmptySuccess) {
  ModelStorage model;
  model.AddAtomicConstraint(QuadraticConstraintData{.name = "c"});

  SparseDoubleVectorProto proto;

  EXPECT_THAT(QuadraticConstraintValuesFromProto(&model, proto),
              IsOkAndHolds(IsEmpty()));
}

TEST(QuadraticConstraintValuesFromProtoTest, NonEmptySuccess) {
  ModelStorage model;
  const QuadraticConstraintId c =
      model.AddAtomicConstraint(QuadraticConstraintData{.name = "c"});
  model.AddAtomicConstraint(QuadraticConstraintData{.name = "d"});
  const QuadraticConstraintId e =
      model.AddAtomicConstraint(QuadraticConstraintData{.name = "e"});

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_ids(2);
  proto.add_values(3.0);
  proto.add_values(-2.0);

  absl::flat_hash_map<QuadraticConstraint, double> expected = {
      {QuadraticConstraint(&model, c), 3.0},
      {QuadraticConstraint(&model, e), -2.0}};

  EXPECT_THAT(QuadraticConstraintValuesFromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0.0)));
}

TEST(QuadraticConstraintValuesFromProtoTest, UnequalSize) {
  ModelStorage model;
  model.AddAtomicConstraint(QuadraticConstraintData{.name = "c"});

  SparseDoubleVectorProto proto;
  proto.add_ids(0);

  EXPECT_THAT(QuadraticConstraintValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(QuadraticConstraintValuesFromProtoTest, NotInModel) {
  ModelStorage model;

  SparseDoubleVectorProto proto;
  proto.add_ids(0);
  proto.add_values(1.0);

  EXPECT_THAT(QuadraticConstraintValuesFromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no quadratic constraint with id")));
}

TEST(QuadraticConstraintValuesToProtoTest, Empty) {
  ModelStorage model;
  EXPECT_THAT(QuadraticConstraintValuesFromProto(
                  &model, QuadraticConstraintValuesToProto({})),
              IsOkAndHolds(IsEmpty()));
}

TEST(QuadraticConstraintValuesToProtoTest, SmallMap) {
  ModelStorage model;
  const QuadraticConstraintId c =
      model.AddAtomicConstraint(QuadraticConstraintData{.name = "c"});
  model.AddAtomicConstraint(QuadraticConstraintData{.name = "d"});
  const QuadraticConstraintId e =
      model.AddAtomicConstraint(QuadraticConstraintData{.name = "e"});

  const absl::flat_hash_map<QuadraticConstraint, double> input = {
      {QuadraticConstraint(&model, c), 3.0},
      {QuadraticConstraint(&model, e), -2.0}};

  EXPECT_THAT(QuadraticConstraintValuesFromProto(
                  &model, QuadraticConstraintValuesToProto(input)),
              IsOkAndHolds(IsNear(input, /*tolerance=*/0.0)));
}

TEST(VariableBasis, EmptyRoundTrip) {
  ModelStorage model;
  const VariableMap<BasisStatus> variable_basis;
  SparseBasisStatusVector proto;

  // Test one way
  EXPECT_THAT(VariableBasisFromProto(&model, proto),
              IsOkAndHolds(variable_basis));
  // Test round trip
  EXPECT_THAT(
      VariableBasisFromProto(&model, VariableBasisToProto(variable_basis)),
      IsOkAndHolds(variable_basis));
}

TEST(VariableBasis, RoundTrip) {
  ModelStorage model;
  const VariableId x = model.AddVariable("x");
  const VariableId y = model.AddVariable("y");

  const VariableMap<BasisStatus> variable_basis = {
      {Variable(&model, x), BasisStatus::kAtUpperBound},
      {Variable(&model, y), BasisStatus::kFree}};

  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_UPPER_BOUND);
  proto.add_values(BASIS_STATUS_FREE);

  // Test one way
  EXPECT_THAT(VariableBasisFromProto(&model, proto),
              IsOkAndHolds(variable_basis));
  // Test round trip
  EXPECT_THAT(
      VariableBasisFromProto(&model, VariableBasisToProto(variable_basis)),
      IsOkAndHolds(variable_basis));
}

TEST(VariableBasisFromProto, UnequalSize) {
  ModelStorage storage;
  storage.AddVariable("x");
  storage.AddVariable("y");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_LOWER_BOUND);
  EXPECT_THAT(VariableBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(VariableBasisFromProto, VariableDoesNotExist) {
  ModelStorage storage;
  storage.AddVariable("x");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_LOWER_BOUND);
  proto.add_values(BASIS_STATUS_AT_UPPER_BOUND);
  EXPECT_THAT(VariableBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no variable with id")));
}

TEST(VariableBasisFromProto, UnspecifiedStatus) {
  ModelStorage storage;
  storage.AddVariable("x");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_values(BASIS_STATUS_UNSPECIFIED);
  EXPECT_THAT(VariableBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("basis status not specified")));
}

TEST(LinearConstraintBasis, EmptyRoundTrip) {
  ModelStorage model;

  const LinearConstraintMap<BasisStatus> linear_constraint_basis;

  SparseBasisStatusVector proto;

  // Test one way
  EXPECT_THAT(LinearConstraintBasisFromProto(&model, proto),
              IsOkAndHolds(linear_constraint_basis));
  // Test round trip
  EXPECT_THAT(
      LinearConstraintBasisFromProto(
          &model, LinearConstraintBasisToProto(linear_constraint_basis)),
      IsOkAndHolds(linear_constraint_basis));
}

TEST(LinearConstraintBasis, RoundTrip) {
  ModelStorage model;
  const LinearConstraintId c = model.AddLinearConstraint("c");
  const LinearConstraintId d = model.AddLinearConstraint("d");

  const LinearConstraintMap<BasisStatus> linear_constraint_basis = {
      {LinearConstraint(&model, c), BasisStatus::kAtLowerBound},
      {LinearConstraint(&model, d), BasisStatus::kFixedValue}};

  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_LOWER_BOUND);
  proto.add_values(BASIS_STATUS_FIXED_VALUE);

  // Test one way
  EXPECT_THAT(LinearConstraintBasisFromProto(&model, proto),
              IsOkAndHolds(linear_constraint_basis));
  // Test round trip
  EXPECT_THAT(
      LinearConstraintBasisFromProto(
          &model, LinearConstraintBasisToProto(linear_constraint_basis)),
      IsOkAndHolds(linear_constraint_basis));
}

TEST(LinearConstraintBasisFromProto, UnequalSize) {
  ModelStorage storage;
  storage.AddLinearConstraint("c");
  storage.AddLinearConstraint("d");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_LOWER_BOUND);
  EXPECT_THAT(LinearConstraintBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(LinearConstraintBasisFromProto, LinearConstraintDoesNotExist) {
  ModelStorage storage;
  storage.AddLinearConstraint("c");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_ids(1);
  proto.add_values(BASIS_STATUS_AT_LOWER_BOUND);
  proto.add_values(BASIS_STATUS_AT_UPPER_BOUND);
  EXPECT_THAT(LinearConstraintBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no linear constraint with id")));
}

TEST(LinearConstraintBasisFromProto, UnspecifiedStatus) {
  ModelStorage storage;
  storage.AddLinearConstraint("c");
  SparseBasisStatusVector proto;
  proto.add_ids(0);
  proto.add_values(BASIS_STATUS_UNSPECIFIED);
  EXPECT_THAT(LinearConstraintBasisFromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("basis status not specified")));
}

}  // namespace
}  // namespace operations_research::math_opt
