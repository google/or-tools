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

#include "ortools/math_opt/core/model_summary.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

TEST(IdNameBiMapTest, EmptyMap) {
  const IdNameBiMap bimap;
  EXPECT_TRUE(bimap.Empty());
  EXPECT_EQ(bimap.Size(), 0);
  EXPECT_FALSE(bimap.HasId(12));
  EXPECT_FALSE(bimap.HasName("dog"));
  EXPECT_THAT(bimap.id_to_name(), IsEmpty());
  EXPECT_THAT(bimap.nonempty_name_to_id(), Optional(IsEmpty()));
  EXPECT_EQ(bimap.next_free_id(), 0);
}

TEST(IdNameBiMapTest, Insert) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(6, "y"));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "y")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 4), Pair("y", 6))));
}

TEST(IdNameBiMapTest, InsertEmptyNames) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, ""));
  EXPECT_OK(bimap.Insert(6, ""));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(4, ""), Pair(6, "")));
  EXPECT_THAT(bimap.nonempty_name_to_id(), Optional(IsEmpty()));
}

TEST(IdNameBiMapTest, Copy) {
  std::unique_ptr<IdNameBiMap> bimap2;
  {
    IdNameBiMap bimap;
    ASSERT_OK(bimap.Insert(4, "x"));
    ASSERT_OK(bimap.Insert(6, "y"));
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    bimap2 = std::make_unique<IdNameBiMap>(bimap);
  }
  // Let the original go out of scope to make sure the string_views in bimap2
  // point do not point back into bimap.
  EXPECT_THAT(bimap2->id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "y")));
  EXPECT_THAT(bimap2->nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 4), Pair("y", 6))));
}

TEST(IdNameBiMapTest, Assign) {
  IdNameBiMap bimap2;
  ASSERT_OK(bimap2.Insert(8, "z"));
  {
    IdNameBiMap bimap;
    ASSERT_OK(bimap.Insert(4, "x"));
    ASSERT_OK(bimap.Insert(6, "y"));
    bimap2 = bimap;
  }
  // Let the original go out of scope to make sure the string_views in bimap2
  // point do not point back into bimap.
  EXPECT_THAT(bimap2.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "y")));
  EXPECT_THAT(bimap2.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 4), Pair("y", 6))));
}

TEST(IdNameBiMapTest, HasName) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(5, ""));
  EXPECT_TRUE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("y"));
  EXPECT_FALSE(bimap.HasName(""));
}

TEST(IdNameBiMapTest, HasNameAlwaysFalseWithoutCheckNames) {
  IdNameBiMap bimap(/*check_names=*/false);
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(5, ""));
  EXPECT_FALSE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("y"));
  EXPECT_FALSE(bimap.HasName(""));
}

TEST(IdNameBiMapTest, EraseNotThere) {
  IdNameBiMap bimap;
  EXPECT_THAT(bimap.Erase(12),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("delete"), HasSubstr("12"))));
}

TEST(IdNameBiMapTest, EraseExisting) {
  IdNameBiMap bimap = {{3, "x"}, {5, ""}, {6, "y"}};
  EXPECT_EQ(bimap.next_free_id(), 7);

  EXPECT_OK(bimap.Erase(6));

  EXPECT_FALSE(bimap.Empty());
  EXPECT_EQ(bimap.Size(), 2);
  EXPECT_TRUE(bimap.HasId(3));
  EXPECT_TRUE(bimap.HasId(5));
  EXPECT_FALSE(bimap.HasId(7));
  EXPECT_TRUE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("y"));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(3, "x"), Pair(5, "")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 3))));
  // The next_free_id should not have changed.
  EXPECT_EQ(bimap.next_free_id(), 7);

  // Empty the map.
  EXPECT_OK(bimap.Erase(3));
  EXPECT_OK(bimap.Erase(5));

  EXPECT_TRUE(bimap.Empty());
  EXPECT_EQ(bimap.Size(), 0);
  EXPECT_FALSE(bimap.HasId(3));
  EXPECT_FALSE(bimap.HasId(5));
  EXPECT_FALSE(bimap.HasId(7));
  EXPECT_FALSE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("y"));
  EXPECT_THAT(bimap.id_to_name(), IsEmpty());
  EXPECT_THAT(bimap.nonempty_name_to_id(), Optional(IsEmpty()));
  // The next_free_id should not have changed.
  EXPECT_EQ(bimap.next_free_id(), 7);
}

TEST(IdNameBiMapTest, InsertsNotIncreasing) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, ""));
  EXPECT_THAT(bimap.Insert(2, ""), StatusIs(absl::StatusCode::kInvalidArgument,
                                            HasSubstr("strictly increasing")));
}

TEST(IdNameBiMapTest, InsertsBeforeNextFree) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.SetNextFreeId(3));
  EXPECT_THAT(bimap.Insert(2, ""), StatusIs(absl::StatusCode::kInvalidArgument,
                                            HasSubstr("strictly increasing")));
}

TEST(IdNameBiMapTest, InsertsAtNextFree) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.SetNextFreeId(3));
  EXPECT_OK(bimap.Insert(3, ""));
  EXPECT_EQ(bimap.next_free_id(), 4);
}

TEST(IdNameBiMapTest, InsertsAfterNextFree) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.SetNextFreeId(3));
  EXPECT_OK(bimap.Insert(5, ""));
  EXPECT_EQ(bimap.next_free_id(), 6);
}

TEST(IdNameBiMapTest, InsertsRepeatId) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_THAT(bimap.Insert(4, "y"), StatusIs(absl::StatusCode::kInvalidArgument,
                                             HasSubstr("strictly increasing")));
}

TEST(IdNameBiMapTest, InsertsRepeatName) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_THAT(bimap.Insert(6, "x"), StatusIs(absl::StatusCode::kInvalidArgument,
                                             HasSubstr("duplicate name")));
}

TEST(IdNameBiMapTest, InsertsRepeatNameCheckNamesOff) {
  IdNameBiMap bimap(false);
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(6, "x"));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "x")));
  EXPECT_THAT(bimap.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(IdNameBiMapTest, CopyRepeatNameCheckNamesOff) {
  IdNameBiMap bimap(false);
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(6, "x"));
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const IdNameBiMap bimap2(bimap);
  EXPECT_THAT(bimap2.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "x")));
  EXPECT_THAT(bimap2.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(IdNameBiMapTest, AssignRepeatNameCheckNamesOff) {
  IdNameBiMap bimap(false);
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(6, "x"));
  const IdNameBiMap bimap2 = bimap;
  EXPECT_THAT(bimap2.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "x")));
  EXPECT_THAT(bimap2.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(IdNameBiMapTest, AssignChangeCheckNames) {
  IdNameBiMap bimap2;
  ASSERT_OK(bimap2.Insert(8, "z"));
  {
    IdNameBiMap bimap(false);
    ASSERT_OK(bimap.Insert(4, "x"));
    ASSERT_OK(bimap.Insert(6, "y"));
    bimap2 = bimap;
  }
  // Let the original go out of scope to make sure the string_views in bimap2
  // point do not point back into bimap.
  EXPECT_THAT(bimap2.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(6, "y")));
  EXPECT_THAT(bimap2.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(IdNameBiMapTest, MapWithItems) {
  IdNameBiMap bimap;
  EXPECT_OK(bimap.Insert(4, "x"));
  EXPECT_OK(bimap.Insert(5, ""));
  EXPECT_OK(bimap.Insert(8, ""));
  EXPECT_OK(bimap.Insert(9, "y"));

  EXPECT_FALSE(bimap.Empty());
  EXPECT_EQ(bimap.Size(), 4);
  EXPECT_TRUE(bimap.HasId(4));
  EXPECT_TRUE(bimap.HasId(5));
  EXPECT_FALSE(bimap.HasId(6));
  EXPECT_TRUE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("dog"));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(5, ""), Pair(8, ""),
                                   Pair(9, "y")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 4), Pair("y", 9))));
  EXPECT_EQ(bimap.next_free_id(), 10);
}

TEST(IdNameBiMapTest, InitializerList) {
  const IdNameBiMap bimap{{4, "x"}, {5, ""}, {8, ""}, {9, "y"}};

  EXPECT_FALSE(bimap.Empty());
  EXPECT_EQ(bimap.Size(), 4);
  EXPECT_TRUE(bimap.HasId(4));
  EXPECT_TRUE(bimap.HasId(5));
  EXPECT_FALSE(bimap.HasId(6));
  EXPECT_TRUE(bimap.HasName("x"));
  EXPECT_FALSE(bimap.HasName("dog"));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(4, "x"), Pair(5, ""), Pair(8, ""),
                                   Pair(9, "y")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("x", 4), Pair("y", 9))));
  EXPECT_EQ(bimap.next_free_id(), 10);
}

TEST(IdNameBiMapDeathTest, InitializerListNotSorted) {
  EXPECT_DEATH(IdNameBiMap({{5, ""}, {4, "not_in_order"}, {8, ""}, {9, "y"}}),
               "strictly increasing");
}

TEST(IdNameBiMapDeathTest, InitializerListDuplicates) {
  EXPECT_DEATH(IdNameBiMap({{5, ""}, {5, "is_duplicated"}, {8, ""}, {9, "y"}}),
               "strictly increasing");
}

TEST(IdNameBiMapTest, SetNextFreeIdNegative) {
  IdNameBiMap bimap;
  EXPECT_THAT(
      bimap.SetNextFreeId(-1),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("nonnegative")));
}

TEST(IdNameBiMapTest, SetNextFreeIdZero) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.SetNextFreeId(0));
  EXPECT_EQ(bimap.next_free_id(), 0);
}

TEST(IdNameBiMapTest, SetNextFreeIdEmptyMap) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.SetNextFreeId(4));
  EXPECT_EQ(bimap.next_free_id(), 4);
}

TEST(IdNameBiMapTest, SetNextFreeIdTooLow) {
  IdNameBiMap bimap = {{3, ""}, {5, ""}};
  EXPECT_THAT(
      bimap.SetNextFreeId(5),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("greater")));
}

TEST(IdNameBiMapTest, SetNextFreeIdFilledMap) {
  IdNameBiMap bimap = {{3, ""}, {5, ""}};

  ASSERT_OK(bimap.SetNextFreeId(6));
  EXPECT_EQ(bimap.next_free_id(), 6);

  // It should be able to grow.
  ASSERT_OK(bimap.SetNextFreeId(8));
  EXPECT_EQ(bimap.next_free_id(), 8);

  // It should also be able to shrink.
  ASSERT_OK(bimap.SetNextFreeId(6));
  EXPECT_EQ(bimap.next_free_id(), 6);

  // Emptying the map should also enable resetting it to zero.
  EXPECT_OK(bimap.Erase(3));
  EXPECT_OK(bimap.Erase(5));
  ASSERT_OK(bimap.SetNextFreeId(0));
  EXPECT_EQ(bimap.next_free_id(), 0);
}

TEST(IdNameBiMapTest, BulkUpdate) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  ASSERT_OK(bimap.Insert(5, "b"));
  ASSERT_OK(bimap.Insert(8, "c"));
  const std::string d = "d";
  const std::string e = "";
  const std::string f = "f";
  ASSERT_OK(bimap.BulkUpdate({3, 8}, {9, 17, 18}, {&d, &e, &f}));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(5, "b"), Pair(9, "d"), Pair(17, ""),
                                   Pair(18, "f")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("b", 5), Pair("d", 9),
                                            Pair("f", 18))));
}

TEST(IdNameBiMapTest, BulkUpdateNoNames) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  ASSERT_OK(bimap.Insert(5, "b"));
  ASSERT_OK(bimap.Insert(8, "c"));
  ASSERT_OK(bimap.BulkUpdate({3, 8}, {9, 17, 18}, {}));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(5, "b"), Pair(9, ""), Pair(17, ""),
                                   Pair(18, "")));
  EXPECT_THAT(bimap.nonempty_name_to_id(),
              Optional(UnorderedElementsAre(Pair("b", 5))));
}

TEST(IdNameBiMapTest, BulkUpdateNoCheckNames) {
  IdNameBiMap bimap(false);
  ASSERT_OK(bimap.Insert(3, "a"));
  ASSERT_OK(bimap.Insert(5, "b"));
  ASSERT_OK(bimap.Insert(8, "c"));
  const std::string d = "d";
  const std::string e = "";
  const std::string f = "f";
  ASSERT_OK(bimap.BulkUpdate({3, 8}, {9, 17, 18}, {&d, &e, &f}));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(5, "b"), Pair(9, "d"), Pair(17, ""),
                                   Pair(18, "f")));
  EXPECT_THAT(bimap.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(IdNameBiMapTest, BulkUpdateDeletesNegativeId) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  EXPECT_THAT(
      bimap.BulkUpdate({-2}, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("negative")));
}

TEST(IdNameBiMapTest, BulkUpdateDeletesMaxValueId) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  EXPECT_THAT(
      bimap.BulkUpdate({std::numeric_limits<int64_t>::max()}, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("max(int64_t)")));
}

TEST(IdNameBiMapTest, BulkUpdateDeletesNotSorted) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  ASSERT_OK(bimap.Insert(5, "b"));
  EXPECT_THAT(
      bimap.BulkUpdate({5, 3}, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST(IdNameBiMapTest, BulkUpdateDeletesDuplicate) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  EXPECT_THAT(
      bimap.BulkUpdate({3, 3}, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST(IdNameBiMapTest, BulkUpdateDeleteItemNotPresent) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "a"));
  EXPECT_THAT(
      bimap.BulkUpdate({2}, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("delete")));
}

TEST(IdNameBiMapTest, BulkUpdateNewIdsNotSorted) {
  IdNameBiMap bimap;
  std::string empty;
  EXPECT_THAT(
      bimap.BulkUpdate({}, {5, 3}, {&empty, &empty}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST(IdNameBiMapTest, BulkUpdateNewIdsIncompatibleSize) {
  IdNameBiMap bimap;
  std::string empty;
  EXPECT_THAT(
      bimap.BulkUpdate({}, {3, 5}, {&empty}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("size matching")));
}

TEST(IdNameBiMapTest, BulkUpdateRepeatOldName) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, "x"));
  std::string x = "x";
  EXPECT_THAT(
      bimap.BulkUpdate({}, {5}, {&x}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("duplicate")));
}

TEST(IdNameBiMapTest, BulkUpdateRepeatNewName) {
  IdNameBiMap bimap;
  std::string x = "x";
  EXPECT_THAT(
      bimap.BulkUpdate({}, {3, 5}, {&x, &x}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("duplicate")));
}

TEST(IdNameBiMapTest, BulkUpdateRepeatNameNoCheck) {
  IdNameBiMap bimap(false);
  std::string x = "x";
  ASSERT_OK(bimap.BulkUpdate({}, {3, 5}, {&x, &x}));
  EXPECT_THAT(bimap.id_to_name(),
              UnorderedElementsAre(Pair(3, "x"), Pair(5, "x")));
  EXPECT_THAT(bimap.nonempty_name_to_id(), Eq(std::nullopt));
}

using TestConstraintDataProto = SosConstraintProto;

TEST(UpdateBiMapFromMappedDataTest, TrivialUpdate) {
  IdNameBiMap bimap;
  EXPECT_OK(internal::UpdateBiMapFromMappedData<TestConstraintDataProto>(
      {}, {}, bimap));
}

TEST(UpdateBiMapFromMappedDataTest, UpdateOk) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(1, ""));
  google::protobuf::Map<int64_t, TestConstraintDataProto> new_constraints;
  TestConstraintDataProto new_constraint;
  new_constraint.set_name("c2");
  new_constraints[2] = new_constraint;
  EXPECT_OK(internal::UpdateBiMapFromMappedData<TestConstraintDataProto>(
      {1}, new_constraints, bimap));

  EXPECT_THAT(bimap.id_to_name(), UnorderedElementsAre(Pair(2, "c2")));
}

TEST(UpdateBiMapFromMappedDataTest, BadDeletedIdNotStrictlyIncreasing) {
  IdNameBiMap bimap;
  EXPECT_THAT(
      internal::UpdateBiMapFromMappedData<TestConstraintDataProto>({2, 1}, {},
                                                                   bimap),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected ids to be strictly increasing"),
                     HasSubstr("invalid deleted ids"))));
}

TEST(UpdateBiMapFromMappedDataTest, BadDeletedIdWhileErasing) {
  IdNameBiMap bimap;
  EXPECT_THAT(internal::UpdateBiMapFromMappedData<TestConstraintDataProto>(
                  {1}, {}, bimap),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("cannot delete missing id")));
}

TEST(UpdateBiMapFromMappedDataTest, BadNewIdInMap) {
  IdNameBiMap bimap;
  ASSERT_OK(bimap.Insert(3, ""));
  google::protobuf::Map<int64_t, TestConstraintDataProto> new_constraints;
  new_constraints[2] = TestConstraintDataProto();
  EXPECT_THAT(
      internal::UpdateBiMapFromMappedData<TestConstraintDataProto>(
          {}, new_constraints, bimap),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("expected id=2 to be at least next_free_id_=4")));
}

TEST(ModelSummaryTest, CreateFromTrivialModel) {
  EXPECT_OK(ModelSummary::Create(ModelProto(), /*check_names=*/false));
  EXPECT_OK(ModelSummary::Create(ModelProto(), /*check_names=*/true));
}

TEST(ModelSummaryTest, CreateFromComplexModel) {
  ASSERT_OK_AND_ASSIGN(const auto model, ParseTextProto<ModelProto>(R"pb(
                         variables {
                           ids: [ 3 ]
                           names: [ "x3" ]
                         }
                         objective { maximize: true name: "obj1" }
                         auxiliary_objectives {
                           key: 2
                           value: { name: "obj2" }
                         }
                         linear_constraints {
                           ids: [ 4 ]
                           names: [ "c4" ]
                         }
                         quadratic_constraints {
                           key: 5
                           value { name: "c5" }
                         }
                         second_order_cone_constraints {
                           key: 9
                           value: { name: "c9" }
                         }
                         sos1_constraints {
                           key: 6
                           value { name: "c6" }
                         }
                         sos2_constraints {
                           key: 7
                           value { name: "c7" }
                         }
                         indicator_constraints {
                           key: 8
                           value { name: "c8" }
                         }
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary, ModelSummary::Create(model));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "x3")));
  EXPECT_EQ(summary.primary_objective_name, "obj1");
  EXPECT_TRUE(summary.maximize);
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(2, "obj2")));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(4, "c4")));
  EXPECT_THAT(summary.quadratic_constraints.id_to_name(),
              UnorderedElementsAre(Pair(5, "c5")));
  EXPECT_THAT(summary.second_order_cone_constraints.id_to_name(),
              UnorderedElementsAre(Pair(9, "c9")));
  EXPECT_THAT(summary.sos1_constraints.id_to_name(),
              UnorderedElementsAre(Pair(6, "c6")));
  EXPECT_THAT(summary.sos2_constraints.id_to_name(),
              UnorderedElementsAre(Pair(7, "c7")));
  EXPECT_THAT(summary.indicator_constraints.id_to_name(),
              UnorderedElementsAre(Pair(8, "c8")));
}

TEST(ModelSummaryTest, CreateBadVariables) {
  ModelProto model;
  model.mutable_variables()->add_ids(-1);
  EXPECT_THAT(ModelSummary::Create(model),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected ids to be nonnegative"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelSummaryTest, CreateBadAuxiliaryObjectives) {
  ModelProto model;
  model.mutable_auxiliary_objectives()->insert({-1, ObjectiveProto()});
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr("ModelProto.auxiliary_objectives are invalid"))));
}

TEST(ModelSummaryTest, BadPrimaryObjectiveName) {
  ModelProto model;
  ObjectiveProto& aux_obj = (*model.mutable_auxiliary_objectives())[1];
  aux_obj.set_name("obj");
  model.mutable_objective()->set_name("obj");
  EXPECT_THAT(ModelSummary::Create(model),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("duplicate objective name")));
}

TEST(ModelSummaryTest, DuplicatePrimaryObjectiveNameOk) {
  ModelProto model;
  ObjectiveProto& aux_obj = (*model.mutable_auxiliary_objectives())[1];
  aux_obj.set_name("obj");
  model.mutable_objective()->set_name("obj");
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ModelSummary::Create(model, /*check_names=*/false));
  EXPECT_EQ(summary.primary_objective_name, "obj");
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(1, "obj")));
}

TEST(ModelSummaryTest, CreateBadLinearConstraints) {
  ModelProto model;
  model.mutable_linear_constraints()->add_ids(-1);
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected ids to be nonnegative"),
                     HasSubstr("ModelProto.linear_constraints are invalid"))));
}

TEST(ModelSummaryTest, CreateBadQuadraticConstraints) {
  ModelProto model;
  (*model.mutable_quadratic_constraints())[-1] = {};
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr("ModelProto.quadratic_constraints are invalid"))));
}

TEST(ModelSummaryTest, CreateBadSecondOrderConeConstraints) {
  ModelProto model;
  (*model.mutable_second_order_cone_constraints())[-1] = {};
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr(
                    "ModelProto.second_order_cone_constraints are invalid"))));
}

TEST(ModelSummaryTest, CreateBadSos1Constraints) {
  ModelProto model;
  (*model.mutable_sos1_constraints())[-1] = {};
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ids should be nonnegative"),
                     HasSubstr("ModelProto.sos1_constraints are invalid"))));
}

TEST(ModelSummaryTest, CreateBadSos2Constraints) {
  ModelProto model;
  (*model.mutable_sos2_constraints())[-1] = {};
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ids should be nonnegative"),
                     HasSubstr("ModelProto.sos2_constraints are invalid"))));
}

TEST(ModelSummaryTest, CreateBadIndicatorConstraints) {
  ModelProto model;
  (*model.mutable_indicator_constraints())[-1] = {};
  EXPECT_THAT(
      ModelSummary::Create(model),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr("ModelProto.indicator_constraints are invalid"))));
}

TEST(ModelSummaryTest, UpdateFromTrivialModelUpdate) {
  EXPECT_OK(ModelSummary().Update(ModelUpdateProto()));
}

TEST(ModelSummaryTest, UpdateFromComplexModelUpdate) {
  ModelSummary summary;
  summary.maximize = true;
  ASSERT_OK(summary.variables.Insert(3, ""));
  ASSERT_OK(summary.auxiliary_objectives.Insert(2, ""));
  ASSERT_OK(summary.linear_constraints.Insert(4, ""));
  ASSERT_OK(summary.quadratic_constraints.Insert(5, ""));
  ASSERT_OK(summary.second_order_cone_constraints.Insert(9, ""));
  ASSERT_OK(summary.sos1_constraints.Insert(6, ""));
  ASSERT_OK(summary.sos2_constraints.Insert(7, ""));
  ASSERT_OK(summary.indicator_constraints.Insert(8, ""));
  ASSERT_OK_AND_ASSIGN(const auto update, ParseTextProto<ModelUpdateProto>(R"pb(
                         deleted_variable_ids: [ 3 ]
                         new_variables {
                           ids: [ 7 ]
                           names: [ "x7" ]
                         }
                         objective_updates { direction_update: false }
                         auxiliary_objectives_updates {
                           deleted_objective_ids: [ 2 ]
                           new_objectives {
                             key: 6
                             value: {}
                           }
                         }
                         deleted_linear_constraint_ids: [ 4 ]
                         new_linear_constraints {
                           ids: [ 8 ]
                           names: [ "c8" ]
                         }
                         quadratic_constraint_updates {
                           deleted_constraint_ids: [ 5 ]
                           new_constraints {
                             key: 9
                             value { name: "c9" }
                           }
                         }
                         second_order_cone_constraint_updates {
                           deleted_constraint_ids: [ 9 ]
                           new_constraints {
                             key: 13
                             value: { name: "c13" }
                           }
                         }
                         sos1_constraint_updates {
                           deleted_constraint_ids: [ 6 ]
                           new_constraints {
                             key: 10
                             value { name: "c10" }
                           }
                         }
                         sos2_constraint_updates {
                           deleted_constraint_ids: [ 7 ]
                           new_constraints {
                             key: 11
                             value { name: "c11" }
                           }
                         }
                         indicator_constraint_updates {
                           deleted_constraint_ids: [ 8 ]
                           new_constraints {
                             key: 12
                             value { name: "c12" }
                           }
                         }
                       )pb"));
  EXPECT_OK(summary.Update(update));
  EXPECT_FALSE(summary.maximize);
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(7, "x7")));
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(6, "")));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(8, "c8")));
  EXPECT_THAT(summary.quadratic_constraints.id_to_name(),
              UnorderedElementsAre(Pair(9, "c9")));
  EXPECT_THAT(summary.second_order_cone_constraints.id_to_name(),
              UnorderedElementsAre(Pair(13, "c13")));
  EXPECT_THAT(summary.sos1_constraints.id_to_name(),
              UnorderedElementsAre(Pair(10, "c10")));
  EXPECT_THAT(summary.sos2_constraints.id_to_name(),
              UnorderedElementsAre(Pair(11, "c11")));
  EXPECT_THAT(summary.indicator_constraints.id_to_name(),
              UnorderedElementsAre(Pair(12, "c12")));
}

TEST(ModelSummaryTest, UpdateBadVariables) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_variable_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid variables"))));
}

TEST(ModelSummaryTest, UpdateBadAuxiliaryObjectives) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid auxiliary objectives"))));
}

TEST(ModelSummaryTest, UpdateDupPrimaryObjectiveName) {
  ModelSummary summary;
  summary.primary_objective_name = "obj";
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[1]
      .set_name("obj");
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("duplicate objective name")));
}

TEST(ModelSummaryTest, UpdateDupPrimaryObjectiveNameOk) {
  ModelSummary summary(/*check_names=*/false);
  summary.primary_objective_name = "obj";
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[1]
      .set_name("obj");
  EXPECT_OK(summary.Update(update));
  EXPECT_EQ(summary.primary_objective_name, "obj");
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(1, "obj")));
}

TEST(ModelSummaryTest, UpdateBadLinearConstraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_linear_constraint_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid linear constraints"))));
}

TEST(ModelSummaryTest, UpdateBadQuadraticConstraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_quadratic_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid quadratic constraints"))));
}

TEST(ModelSummaryTest, UpdateBadSecondOrderConeConstraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_second_order_cone_constraint_updates()
      ->add_deleted_constraint_ids(1);
  EXPECT_THAT(
      summary.Update(update),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("cannot delete missing id"),
                     HasSubstr("invalid second-order cone constraints"))));
}

TEST(ModelSummaryTest, UpdateBadSos1Constraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_sos1_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid sos1 constraints"))));
}

TEST(ModelSummaryTest, UpdateBadSos2Constraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_sos2_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid sos2 constraints"))));
}

TEST(ModelSummaryTest, UpdateBadIndicatorConstraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_THAT(summary.Update(update),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id"),
                             HasSubstr("invalid indicator constraints"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
