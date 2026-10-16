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

#include "ortools/math_opt/storage/model_storage_item.h"

#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {
namespace {

class MyItem : public ModelStorageItem {
 public:
  explicit MyItem(ModelStorageCPtr storage) : ModelStorageItem(storage) {}
};

static_assert(!is_model_storage_element<MyItem>::value);

class MyItemContainer : public ModelStorageItemContainer {
 public:
  using ModelStorageItemContainer::ModelStorageItemContainer;

  void Add(const ModelStorageItem& item) { SetOrCheckStorage(item); }

  void Append(const ModelStorageItemContainer& container) {
    SetOrCheckStorage(container);
  }
};

static_assert(std::is_default_constructible_v<MyItemContainer>);

TEST(ModelStorageItemTest, Works) {
  ModelStorage storage;
  MyItem item1(&storage);
  MyItem item2(&storage);
  EXPECT_EQ(item1.storage(), &storage);
  EXPECT_EQ(item2.storage(), &storage);

  ModelStorage alien_storage;
  MyItem alien_item(&alien_storage);
  EXPECT_EQ(alien_item.storage(), &alien_storage);

  MyItemContainer container;
  EXPECT_EQ(container.storage(), nullptr);

  // It's fine to add a bunch of items with the same storage.
  container.Add(item1);
  EXPECT_EQ(container.storage(), &storage);
  container.Add(item2);
  EXPECT_EQ(container.storage(), &storage);

  {
    // It's fine to append non-empty to empty container.
    MyItemContainer container2;
    container2.Append(container);
  }
}

TEST(ModelStorageItemContainerDeathTest,
     ContainerDiesOnItemsWithDistinctStorages) {
  ModelStorage storage;
  MyItem item(&storage);

  ModelStorage alien_storage;
  MyItem alien_item(&alien_storage);

  MyItemContainer container;

  container.Add(item);
  EXPECT_EQ(container.storage(), &storage);
  EXPECT_DEATH_IF_SUPPORTED(container.Add(alien_item),
                            "The input objects belongs to another model");
}

// Note: We could allow appending an empty container, but that's not really
// useful.
TEST(ModelStorageItemContainerDeathTest, AppendingEmptyContainerDies) {
  ModelStorage storage;
  MyItem item(&storage);

  MyItemContainer container;
  container.Add(item);
  MyItemContainer empty_container;

  EXPECT_DEATH_IF_SUPPORTED(container.Append(empty_container),
                            "appending an empty container");
}

TEST(ModelStorageItemContainerDeathTest,
     AppendingContainerWithAlienStorageDies) {
  ModelStorage storage;
  MyItem item(&storage);

  ModelStorage alien_storage;
  MyItem alien_item(&alien_storage);

  MyItemContainer container;
  container.Add(item);
  MyItemContainer alien_container;
  alien_container.Add(alien_item);

  EXPECT_DEATH_IF_SUPPORTED(container.Append(alien_container),
                            "The input objects belongs to another model");
}

TEST(ModelStorageItemTest, CopyCtor) {
  ModelStorage storage;
  const MyItem src(&storage);

  MyItem dst = src;
  EXPECT_EQ(dst.storage(), &storage);
  EXPECT_EQ(src.storage(), &storage);
}

TEST(ModelStorageItemTest, MoveCtor) {
  ModelStorage storage;
  MyItem src(&storage);

  MyItem dst = std::move(src);
  EXPECT_EQ(dst.storage(), &storage);
}

TEST(ModelStorageItemTest, CopyAssignment) {
  ModelStorage storage;
  const MyItem src(&storage);

  ModelStorage alien_storage;
  MyItem dst(&alien_storage);
  EXPECT_EQ(dst.storage(), &alien_storage);

  dst = src;
  EXPECT_EQ(dst.storage(), &storage);
  EXPECT_EQ(src.storage(), &storage);
}

TEST(ModelStorageItemTest, MoveAssignment) {
  ModelStorage storage;
  const MyItem src(&storage);

  ModelStorage alien_storage;
  MyItem dst(&alien_storage);
  EXPECT_EQ(dst.storage(), &alien_storage);

  dst = std::move(src);
  EXPECT_EQ(dst.storage(), &storage);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(src.storage(), &storage);
}

TEST(ModelStorageItemContainerTest, CopyCtor) {
  ModelStorage storage;
  const MyItem item(&storage);

  MyItemContainer src_container;
  src_container.Add(item);

  MyItemContainer dst_container = src_container;
  EXPECT_EQ(dst_container.storage(), &storage);
  EXPECT_EQ(src_container.storage(), &storage);
}

TEST(ModelStorageItemContainerTest, CopyAssignment) {
  ModelStorage storage;
  MyItem item(&storage);

  MyItemContainer src_container;
  src_container.Add(item);

  MyItemContainer dst_container;
  dst_container = src_container;

  EXPECT_EQ(dst_container.storage(), &storage);
  EXPECT_EQ(src_container.storage(), &storage);
}

TEST(ModelStorageItemContainerTest, MoveCtor) {
  ModelStorage storage;
  MyItem item(&storage);

  MyItemContainer src_container;
  src_container.Add(item);

  MyItemContainer dst_container = std::move(src_container);
  EXPECT_EQ(dst_container.storage(), &storage);
  // We additionally guarantee that moved-from containers are not associated to
  // a storage.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(src_container.storage(), nullptr);
}

TEST(ModelStorageItemContainerTest, MoveAssignment) {
  ModelStorage storage;
  MyItem item(&storage);

  MyItemContainer src_container;
  src_container.Add(item);

  MyItemContainer dst_container;
  dst_container = std::move(src_container);

  EXPECT_EQ(dst_container.storage(), &storage);
  // We additionally guarantee that moved-from containers are not associated to
  // a storage.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(src_container.storage(), nullptr);
}

class MyModelStorageElement
    : public ModelStorageElement<ElementType::kVariable,
                                 MyModelStorageElement> {
 public:
  using ModelStorageElement::ModelStorageElement;

  void set_name(const absl::string_view name) { name_ = name; }
  absl::string_view name() const { return name_; }

 private:
  std::string name_;
};

static_assert(is_model_storage_element<MyModelStorageElement>::value);

TEST(ModelStorageElementTest, OpEq) {
  ModelStorage storage1;
  ModelStorage storage2;
  EXPECT_EQ(MyModelStorageElement(&storage1, VariableId(1)),
            MyModelStorageElement(&storage1, VariableId(1)));
  EXPECT_NE(MyModelStorageElement(&storage1, VariableId(1)),
            MyModelStorageElement(&storage1, VariableId(2)));
  EXPECT_NE(MyModelStorageElement(&storage1, VariableId(1)),
            MyModelStorageElement(&storage2, VariableId(1)));
}

TEST(ModelStorageElementTest, Hashing) {
  ModelStorage storage1;
  ModelStorage storage2;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      MyModelStorageElement(&storage1, VariableId(1)),
      MyModelStorageElement(&storage2, VariableId(1)),
      MyModelStorageElement(&storage1, VariableId(2)),
      MyModelStorageElement(&storage2, VariableId(2)),
  }));
}

TEST(ModelStorageElementTest, Stream) {
  ModelStorage storage;
  MyModelStorageElement element(&storage, VariableId(1));
  {
    std::stringstream ss;
    ss << element;
    EXPECT_EQ(ss.str(), "__var#1__");
  }
  element.set_name("name");
  {
    std::stringstream ss;
    ss << element;
    EXPECT_EQ(ss.str(), "name");
  }
}

class MyModelStorageElementWithCustomEquality
    : public ModelStorageElement<
          ElementType::kVariable, MyModelStorageElementWithCustomEquality,
          ModelStorageElementEquality::kWithoutEquality> {
 public:
  using ModelStorageElement::ModelStorageElement;

  absl::string_view name() const { return "weird object identity"; }

  friend bool operator==(const MyModelStorageElementWithCustomEquality&,
                         const MyModelStorageElementWithCustomEquality&) {
    return true;
  }
};

TEST(MyModelStorageElementWithCustomEquality, OpEq) {
  ModelStorage storage1;
  ModelStorage storage2;
  // NOTE: using `EXPECT_TRUE(l == r)` instead of `EXPECT_EQ(l, r)` because
  // clang has a false positive on the `==` operator being unused with the
  // latter.
  EXPECT_TRUE(
      MyModelStorageElementWithCustomEquality(&storage1, VariableId(1)) ==
      MyModelStorageElementWithCustomEquality(&storage1, VariableId(1)));
  EXPECT_TRUE(
      MyModelStorageElementWithCustomEquality(&storage1, VariableId(1)) ==
      MyModelStorageElementWithCustomEquality(&storage1, VariableId(2)));
  EXPECT_TRUE(
      MyModelStorageElementWithCustomEquality(&storage1, VariableId(1)) ==
      MyModelStorageElementWithCustomEquality(&storage2, VariableId(1)));
}

}  // namespace
}  // namespace operations_research::math_opt
