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

#include "ortools/math_opt/constraints/quadratic/storage.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::no_adl::FieldsAre;

QuadraticConstraintProto SimpleProto() {
  QuadraticConstraintProto proto;
  proto.set_lower_bound(-1.0);
  proto.set_upper_bound(1.0);
  proto.set_name("q");
  proto.mutable_quadratic_terms()->add_row_ids(1);
  proto.mutable_quadratic_terms()->add_column_ids(2);
  proto.mutable_quadratic_terms()->add_coefficients(3.0);
  proto.mutable_quadratic_terms()->add_row_ids(4);
  proto.mutable_quadratic_terms()->add_column_ids(4);
  proto.mutable_quadratic_terms()->add_coefficients(5.0);
  proto.mutable_linear_terms()->add_ids(6);
  proto.mutable_linear_terms()->add_values(7.0);
  return proto;
}

QuadraticConstraintData SimpleData() {
  QuadraticConstraintData data;
  data.lower_bound = -1.0;
  data.upper_bound = 1.0;
  data.name = "q";
  data.linear_terms.set(VariableId(6), 7.0);
  data.quadratic_terms.set(VariableId(1), VariableId(2), 3.0);
  data.quadratic_terms.set(VariableId(4), VariableId(4), 5.0);
  return data;
}

TEST(QuadraticStorageDataTest, RelatedVariables) {
  EXPECT_THAT(SimpleData().RelatedVariables(),
              UnorderedElementsAre(VariableId(1), VariableId(2), VariableId(4),
                                   VariableId(6)));
}

TEST(QuadraticStorageDataTest, DeleteVariable) {
  QuadraticConstraintData data = SimpleData();
  data.DeleteVariable(VariableId(2));
  EXPECT_THAT(data.linear_terms.terms(),
              UnorderedElementsAre(Pair(VariableId(6), 7.0)));
  EXPECT_THAT(
      data.quadratic_terms.Terms(),
      UnorderedElementsAre(FieldsAre(VariableId(4), VariableId(4), 5.0)));

  data.DeleteVariable(VariableId(6));
  EXPECT_THAT(data.linear_terms.terms(), IsEmpty());
  EXPECT_THAT(
      data.quadratic_terms.Terms(),
      UnorderedElementsAre(FieldsAre(VariableId(4), VariableId(4), 5.0)));
}

TEST(QuadraticStorageDataTest, FromProto) {
  const auto data = QuadraticConstraintData::FromProto(SimpleProto());
  EXPECT_EQ(data.lower_bound, -1.0);
  EXPECT_EQ(data.upper_bound, 1.0);
  EXPECT_EQ(data.name, "q");
  EXPECT_THAT(data.linear_terms.terms(),
              UnorderedElementsAre(Pair(VariableId(6), 7.0)));
  EXPECT_THAT(
      data.quadratic_terms.Terms(),
      UnorderedElementsAre(FieldsAre(VariableId(1), VariableId(2), 3.0),
                           FieldsAre(VariableId(4), VariableId(4), 5.0)));
}

TEST(QuadraticStorageDataTest, Proto) {
  EXPECT_THAT(SimpleData().Proto(), EquivToProto(SimpleProto()));
}

}  // namespace
}  // namespace operations_research::math_opt
