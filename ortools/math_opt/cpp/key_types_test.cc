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

#include "ortools/math_opt/cpp/key_types.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt::internal {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::status::StatusIs;

TEST(CheckModelStorageTest, NullExpected) {
  ModelStorage model;
  // The compiler will prevent us from passing nullptr to a function expecting
  // a `ModelStorageCPtr`. So we launder the `nullptr` to test the behavior of
  // `CheckModelStorage()` in that case.
  ModelStorage* laundered_nullptr = nullptr;
  benchmark::DoNotOptimize(laundered_nullptr);
  EXPECT_THAT(
      CheckModelStorage(/*storage=*/nullptr,
                        /*expected_storage=*/laundered_nullptr),
      StatusIs(absl::StatusCode::kInternal, HasSubstr("expected_storage")));
  EXPECT_THAT(
      CheckModelStorage(/*storage=*/&model,
                        /*expected_storage=*/laundered_nullptr),
      StatusIs(absl::StatusCode::kInternal, HasSubstr("expected_storage")));
}

TEST(CheckModelStorageTest, SingleModel) {
  ModelStorage model;
  EXPECT_OK(CheckModelStorage(/*storage=*/nullptr,
                              /*expected_storage=*/&model));
  EXPECT_OK(CheckModelStorage(/*storage=*/&model,
                              /*expected_storage=*/&model));
}

TEST(CheckModelStorageTest, TwoModels) {
  ModelStorage model_a;
  ModelStorage model_b;
  EXPECT_THAT(CheckModelStorage(/*storage=*/&model_a,
                                /*expected_storage=*/&model_b),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       kInputFromInvalidModelStorage));
}

TEST(SortedKeysTest, EmptyFlatHashMap) {
  const absl::flat_hash_map<Variable, double> vmap;
  EXPECT_THAT(SortedKeys(vmap), IsEmpty());
}

TEST(SortedKeysTest, MultipleModelsFlatHashMap) {
  // Here we use std::make_unique<> instead of using the stack as using the
  // stack may for the order of the pointers on each model.
  const auto model_1 = std::make_unique<ModelStorage>();
  const Variable first1(model_1.get(), model_1->AddVariable("first1"));
  const Variable second1(model_1.get(), model_1->AddVariable("second1"));
  const Variable third1(model_1.get(), model_1->AddVariable("third1"));
  const Variable fourth1(model_1.get(), model_1->AddVariable("fourth1"));

  const auto model_2 = std::make_unique<ModelStorage>();
  const Variable first2(model_2.get(), model_2->AddVariable("first2"));
  const Variable second2(model_2.get(), model_2->AddVariable("second2"));
  const Variable third2(model_2.get(), model_2->AddVariable("third2"));
  const Variable fourth2(model_2.get(), model_2->AddVariable("fourth2"));

  const absl::flat_hash_map<Variable, double> vmap = {
      {second1, 3.0}, {third1, 5.0},  {second2, -3.5}, {fourth1, 0.0},
      {third2, 8.0},  {first2, 1.25}, {fourth2, 12.0}, {first1, -5.0},
  };

  // The sort order between models depends on the pointer values.
  if (model_1 < model_2) {
    EXPECT_THAT(SortedKeys(vmap),
                ElementsAre(first1, second1, third1, fourth1, first2, second2,
                            third2, fourth2));
  } else {
    EXPECT_THAT(SortedKeys(vmap),
                ElementsAre(first2, second2, third2, fourth2, first1, second1,
                            third1, fourth1));
  }
}

TEST(SortedElementsTest, EmptyFlatHashSet) {
  const absl::flat_hash_set<Variable> vset;
  EXPECT_THAT(SortedElements(vset), IsEmpty());
}

TEST(SortedElementsTest, MultipleModelsFlatHashSet) {
  // Here we use std::make_unique<> instead of using the stack as using the
  // stack may for the order of the pointers on each model.
  const auto model_1 = std::make_unique<ModelStorage>();
  const Variable first1(model_1.get(), model_1->AddVariable("first1"));
  const Variable second1(model_1.get(), model_1->AddVariable("second1"));
  const Variable third1(model_1.get(), model_1->AddVariable("third1"));
  const Variable fourth1(model_1.get(), model_1->AddVariable("fourth1"));

  const auto model_2 = std::make_unique<ModelStorage>();
  const Variable first2(model_2.get(), model_2->AddVariable("first2"));
  const Variable second2(model_2.get(), model_2->AddVariable("second2"));
  const Variable third2(model_2.get(), model_2->AddVariable("third2"));
  const Variable fourth2(model_2.get(), model_2->AddVariable("fourth2"));

  const absl::flat_hash_set<Variable> vset = {second1, third1, second2, fourth1,
                                              third2,  first2, fourth2, first1};

  // The sort order between models depends on the pointer values.
  if (model_1 < model_2) {
    EXPECT_THAT(SortedElements(vset),
                ElementsAre(first1, second1, third1, fourth1, first2, second2,
                            third2, fourth2));
  } else {
    EXPECT_THAT(SortedElements(vset),
                ElementsAre(first2, second2, third2, fourth2, first1, second1,
                            third1, fourth1));
  }
}

TEST(ValuesTest, MultipleModelsFlatHashMap) {
  ModelStorage model_1;
  const Variable first1(&model_1, model_1.AddVariable("first1"));
  const Variable second1(&model_1, model_1.AddVariable("second1"));
  const Variable third1(&model_1, model_1.AddVariable("third1"));
  const Variable fourth1(&model_1, model_1.AddVariable("fourth1"));

  ModelStorage model_2;
  const Variable first2(&model_2, model_2.AddVariable("first2"));
  const Variable second2(&model_2, model_2.AddVariable("second2"));
  const Variable third2(&model_2, model_2.AddVariable("third2"));
  const Variable fourth2(&model_2, model_2.AddVariable("fourth2"));

  const absl::flat_hash_map<Variable, double> vmap = {
      {second1, 3.0}, {third1, 5.0},  {second2, -3.5}, {fourth1, 0.0},
      {third2, 8.0},  {first2, 1.25}, {fourth2, 12.0}, {first1, -5.0},
  };

  const std::vector<Variable> vars = {second2, first1, fourth2};
  EXPECT_THAT(Values(vmap, vars), ElementsAre(-3.5, -5.0, 12.0));
}

}  // namespace
}  // namespace operations_research::math_opt::internal
