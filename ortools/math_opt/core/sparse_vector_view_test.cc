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

#include "ortools/math_opt/core/sparse_vector_view.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/types.h"
#include "ortools/math_opt/core/sparse_vector.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

// TODO(b/171883688): remove duplicated code from math_opt_proto_utils_test.cc
SparseDoubleVectorProto MakeSparseDoubleVector(
    std::initializer_list<std::pair<int64_t, double>> pairs) {
  SparseDoubleVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

SparseBoolVectorProto MakeSparseBoolVector(
    std::initializer_list<std::pair<int64_t, bool>> pairs) {
  SparseBoolVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

VariablesProto ParseVariables(absl::string_view model) {
  VariablesProto result;
  CHECK(google::protobuf::TextFormat::ParseFromString(model, &result));
  return result;
}

TEST(SparseVectorViewTest, RepeatedFieldAndRepeatedPtrField) {
  const VariablesProto variables = ParseVariables(R"pb(
    ids: [ 3, 5 ],
    lower_bounds: [ 2.0, 4.0 ]
    upper_bounds: [ 3.0, 5.0 ]
    integers: [ true, false ]
    names: [ "x3", "" ]
  )pb");
  auto value_view = MakeView(variables.ids(), variables.lower_bounds());
  EXPECT_THAT(value_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(value_view.values(), ElementsAre(2.0, 4.0));
  auto name_view = MakeView(variables.ids(), variables.names());
  EXPECT_THAT(name_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(name_view.values(),
              ElementsAre(Pointee(StrEq("x3")), Pointee(StrEq(""))));
}

TEST(SparseVectorViewTest, Vector) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<const std::string*> names({&a, &b});
  const std::vector<int64_t> ids({3, 5});
  const std::vector<double> values({2.0, 4.0});
  auto value_view = MakeView(ids, values);
  EXPECT_THAT(value_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(value_view.values(), ElementsAre(2.0, 4.0));
  auto name_view = MakeView(ids, names);
  EXPECT_THAT(name_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(name_view.values(),
              ElementsAre(Pointee(StrEq("x3")), Pointee(StrEq(""))));
}

TEST(SparseVectorViewTest, SparseVectorProtos) {
  const SparseDoubleVectorProto double_vector =
      MakeSparseDoubleVector({{3, 2.0}, {5, 4.0}});
  auto double_view = MakeView(double_vector);
  EXPECT_THAT(double_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(double_view.values(), ElementsAre(2.0, 4.0));
  const SparseBoolVectorProto bool_vector =
      MakeSparseBoolVector({{3, true}, {5, false}});
  auto bool_view = MakeView(bool_vector);
  EXPECT_THAT(bool_view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(bool_view.values(), ElementsAre(true, false));
}

TEST(SparseVectorViewTest, SparseVector) {
  const SparseVector<double> sparse_vector = {.ids = {3, 5},
                                              .values = {2.0, 4.0}};
  const auto view = MakeView(sparse_vector);
  EXPECT_THAT(view.ids(), ElementsAre(3, 5));
  EXPECT_THAT(view.values(), ElementsAre(2.0, 4.0));
}

TEST(SparseVectorViewTest, IteratorSparseVectorView) {
  const std::vector<int64_t> ids({3, 5});
  const std::vector<double> values({2.0, 4.0});
  auto value_view = MakeView(ids, values);
  EXPECT_THAT(value_view, ElementsAre(Pair(3, 2.0), Pair(5, 4.0)));
}

TEST(SparseVectorViewTest, IteratorComparison) {
  const std::vector<int64_t> ids({3, 5});
  const std::vector<double> values({2.0, 4.0});
  const auto value_view = MakeView(ids, values);

  const auto it0 = value_view.begin();
  auto it1 = value_view.begin();
  ++it1;
  auto it2 = it1;
  ++it2;

  EXPECT_TRUE(it0 == value_view.begin());

  EXPECT_TRUE(it0 == it0);
  EXPECT_FALSE(it0 != it0);

  EXPECT_TRUE(it1 == it1);
  EXPECT_FALSE(it1 != it1);

  EXPECT_TRUE(it2 == it2);
  EXPECT_FALSE(it2 != it2);

  EXPECT_FALSE(it0 == it1);
  EXPECT_FALSE(it1 == it0);
  EXPECT_TRUE(it0 != it1);
  EXPECT_TRUE(it1 != it0);

  EXPECT_FALSE(it0 == it2);
  EXPECT_FALSE(it2 == it0);
  EXPECT_TRUE(it0 != it2);
  EXPECT_TRUE(it2 != it0);

  EXPECT_FALSE(it1 == it2);
  EXPECT_FALSE(it2 == it1);
  EXPECT_TRUE(it1 != it2);
  EXPECT_TRUE(it2 != it1);

  EXPECT_TRUE(it2 == value_view.end());
}

TEST(SparseVectorViewTest, IteratorDereference) {
  const std::vector<int64_t> ids({3, 5});
  const std::vector<double> values({2.0, 4.0});
  const auto value_view = MakeView(ids, values);

  auto it = value_view.begin();
  ++it;

  ASSERT_THAT(*it, Pair(5, 4.0));
}

TEST(SparseVectorViewTest, IteratorArrow) {
  const std::vector<int64_t> ids({3, 5});
  const std::vector<double> values({2.0, 4.0});
  const auto value_view = MakeView(ids, values);

  auto it = value_view.begin();
  ++it;

  ASSERT_EQ(it->first, 5);
  ASSERT_EQ(it->second, 4.0);
}

TEST(SparseVectorViewTest, IteratorEmptySparseDoubleVectorProto) {
  EXPECT_THAT(MakeView(SparseDoubleVectorProto()), ElementsAre());
}

TEST(SparseVectorViewTest, IteratorSparseDoubleVectorProto) {
  const SparseDoubleVectorProto v = MakeSparseDoubleVector(
      {{3, 3.33}, {5, 5.55}, {98765432109876543, 99.99}});
  EXPECT_THAT(MakeView(v),
              ElementsAre(Pair(3, 3.33), Pair(5, 5.55),
                          Pair(int64_t{98765432109876543}, 99.99)));
}

TEST(SparseVectorViewTest, IteratorSparseBoolVectorProto) {
  SparseBoolVectorProto v =
      MakeSparseBoolVector({{3, true}, {5, false}, {98765432109876543, true}});
  EXPECT_THAT(MakeView(v), ElementsAre(Pair(3, true), Pair(5, false),
                                       Pair(int64_t{98765432109876543}, true)));
}

TEST(SparseVectorViewTest, IteratorSparseBoolVectorProtoWithDirectIteration) {
  std::vector<int64_t> ids;
  std::vector<bool> values;
  const SparseBoolVectorProto v = MakeSparseBoolVector({{3, true}, {4, false}});
  for (const auto [id, value] : MakeView(v)) {
    ids.push_back(id);
    values.push_back(value);
  }
  EXPECT_THAT(ids, ElementsAre(3, 4));
  EXPECT_THAT(values, ElementsAre(true, false));
}

TEST(SparseVectorViewTest, IteratorProperForwardIterator) {
  const SparseBoolVectorProto sparse_vector =
      MakeSparseBoolVector({{3, true}, {4, false}});
  const auto pairs = MakeView(sparse_vector);

  // Here we use std::vector to validate that the iterators implements the
  // expected interface.
  const std::vector v(pairs.begin(), pairs.end());
  EXPECT_THAT(v, UnorderedElementsAre(Pair(3, true), Pair(4, false)));
}

struct AsMapTestStorage {};

struct AsMapTestKey {
  DEFINE_STRONG_INT_TYPE(IdType, int64_t);

  AsMapTestKey(const AsMapTestStorage* const storage, const IdType id)
      : storage(storage), id(id) {}

  template <typename H>
  friend H AbslHashValue(H h, const AsMapTestKey& k) {
    return H::combine(std::move(h), k.storage, k.id);
  }

  friend bool operator==(const AsMapTestKey& lhs, const AsMapTestKey& rhs) {
    return lhs.storage == rhs.storage && lhs.id == rhs.id;
  }

  friend std::ostream& operator<<(std::ostream& out, const AsMapTestKey& k) {
    out << "{ storage: " << k.storage << ", id: " << k.id << " }";
    return out;
  }

  const AsMapTestStorage* storage;
  IdType id;
};

TEST(SparseVectorViewTest, AsMapEmpty) {
  const AsMapTestStorage storage;
  const absl::flat_hash_map<AsMapTestKey, double> m =
      MakeView(SparseDoubleVectorProto()).as_map<AsMapTestKey>(&storage);
  EXPECT_THAT(m, IsEmpty());
}

TEST(SparseVectorViewTest, AsMapNonEmpty) {
  const AsMapTestStorage storage;
  const SparseDoubleVectorProto v =
      MakeSparseDoubleVector({{3, 3.33}, {5, 5.55}});
  const absl::flat_hash_map<AsMapTestKey, double> m =
      MakeView(v).as_map<AsMapTestKey>(&storage);
  EXPECT_THAT(m,
              UnorderedElementsAre(
                  Pair(AsMapTestKey(&storage, AsMapTestKey::IdType(3)), 3.33),
                  Pair(AsMapTestKey(&storage, AsMapTestKey::IdType(5)), 5.55)));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
