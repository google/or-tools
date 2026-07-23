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

#include "ortools/util/lazy_mutable_copy.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

// An object that statically tracks the number of times its copy constructor
// was called. Not thread-safe at all, but for our purpose here it's fine.
class MyTestObject {
 public:
  explicit MyTestObject(absl::string_view data) : data_(data) {}

  MyTestObject(const MyTestObject& other) {
    ++num_copies_;
    data_ = other.data_;
  }

  MyTestObject(MyTestObject&& other) {
    ++num_moves_;
    data_ = std::move(other.data_);
  }

  ~MyTestObject() { ++num_deletes_; }

  const std::string& data() const { return data_; }
  void set_data(absl::string_view data) { data_ = data; }

  static void ResetCounters() {
    num_copies_ = 0;
    num_moves_ = 0;
    num_deletes_ = 0;
  }

  static int num_copies_;
  static int num_moves_;
  static int num_deletes_;

  std::string data_;
};

// static
int MyTestObject::num_copies_ = 0;
int MyTestObject::num_moves_ = 0;
int MyTestObject::num_deletes_ = 0;

TEST(LazyMutableCopyTest, SimpleOperations) {
  // Create my baseline object.
  MyTestObject::ResetCounters();
  MyTestObject obj("Hello");
  ASSERT_EQ(obj.data(), "Hello");

  // Create a LazyMutableCopy. For now, don't mutate it: it doesn't copy.
  LazyMutableCopy<MyTestObject> copy(obj);
  ASSERT_FALSE(copy.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(copy->data(), "Hello");
  obj.set_data("World");
  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(copy->data(), "World");
  ASSERT_FALSE(copy.has_ownership());

  // Mutate it a first time: it performs a copy.
  copy.get_mutable()->set_data("Foo");
  ASSERT_TRUE(copy.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(copy->data(), "Foo");
  EXPECT_EQ(obj.data(), "World");  // It did not affect the original object.
  obj.set_data("Baz");             // Changing the original doesn't affect us.
  EXPECT_EQ(copy->data(), "Foo");
  ASSERT_TRUE(copy.has_ownership());

  // Mutate it a second time: no additional copy.
  copy.get_mutable()->set_data("Bar");
  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(copy->data(), "Bar");
  ASSERT_TRUE(copy.has_ownership());
}

TEST(LazyMutableCopyTest, MoveWorks) {
  // We rely on segfaults and the heapchecker to detect duplicate deletes and
  // memory leaks, respectively.
  MyTestObject::ResetCounters();
  MyTestObject obj("Hello");

  LazyMutableCopy<MyTestObject> initial_copy(obj);
  ASSERT_FALSE(initial_copy.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(initial_copy->data(), "Hello");
  LazyMutableCopy<MyTestObject> moved0 = std::move(initial_copy);
  ASSERT_FALSE(moved0.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(moved0->data(), "Hello");
  obj.set_data("World");
  ASSERT_FALSE(moved0.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(moved0->data(), "World");

  // Now, make it mutable, and move it again.
  moved0.get_mutable()->set_data("Foo");
  ASSERT_TRUE(moved0.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(moved0->data(), "Foo");
  LazyMutableCopy<MyTestObject> moved1 = std::move(moved0);
  ASSERT_TRUE(moved1.has_ownership());
  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(moved1->data(), "Foo");
  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(moved1->data(), "Foo");

  // At destruction, nothing bad should happen.
}

TEST(LazyMutableCopyTest, ReferenceAndClear) {
  MyTestObject::ResetCounters();
  MyTestObject a("1234");

  {
    LazyMutableCopy<MyTestObject> wrapper(a);
    EXPECT_EQ(&*wrapper, &a);  // Same object.
    EXPECT_EQ(&*wrapper, &a);  // Still same object.

    // Does nothing, but still nullptr after.
    std::move(wrapper).dispose();

    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(wrapper.get_mutable(), nullptr);
  }

  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(MyTestObject::num_moves_, 0);
  EXPECT_EQ(MyTestObject::num_deletes_, 0);
}

TEST(LazyMutableCopyTest, MoveAndClear) {
  MyTestObject::ResetCounters();
  MyTestObject a("1234");

  {
    LazyMutableCopy<MyTestObject> wrapper(std::move(a));
    EXPECT_EQ(wrapper->data(), "1234");
    EXPECT_NE(&*wrapper, &a);  // different object.
    EXPECT_EQ(MyTestObject::num_moves_, 1);

    // This destroys the moved object.
    std::move(wrapper).dispose();

    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(wrapper.get_mutable(), nullptr);
  }

  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(MyTestObject::num_moves_, 1);
  EXPECT_EQ(MyTestObject::num_deletes_, 1);
}

TEST(LazyMutableCopyTest, ExtractAsUniquePtr) {
  MyTestObject::ResetCounters();
  MyTestObject a("1234");

  {
    LazyMutableCopy<MyTestObject> wrapper(std::move(a));
    EXPECT_EQ(wrapper->data(), "1234");
    EXPECT_NE(&*wrapper, &a);  // different object.
    EXPECT_EQ(MyTestObject::num_moves_, 1);

    // This destroys the moved object at the end, still no copy.
    std::unique_ptr<MyTestObject> up =
        std::move(wrapper).copy_or_move_as_unique_ptr();

    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(wrapper.get(), nullptr);
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(wrapper.get_mutable(), nullptr);
  }

  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(MyTestObject::num_moves_, 1);
  EXPECT_EQ(MyTestObject::num_deletes_, 1);
}

TEST(LazyMutableCopyTest, ReferenceAndCopy) {
  MyTestObject::ResetCounters();
  MyTestObject a("1234");

  {
    LazyMutableCopy<MyTestObject> wrapper(a);
    EXPECT_EQ(&*wrapper, &a);  // Same object.
    EXPECT_EQ(&*wrapper, &a);  // Still same object.

    wrapper.get_mutable()->set_data("hello");
    EXPECT_NE(&*wrapper, &a);  // different object.
    EXPECT_EQ(wrapper->data(), "hello");
  }

  EXPECT_EQ(MyTestObject::num_copies_, 1);
  EXPECT_EQ(MyTestObject::num_moves_, 0);
  EXPECT_EQ(MyTestObject::num_deletes_, 1);
}

TEST(LazyMutableCopyTest, MoveAndCopy) {
  MyTestObject::ResetCounters();
  MyTestObject a("1234");

  {
    // Test that we can move the wrapper.
    LazyMutableCopy<MyTestObject> w1(std::move(a));
    LazyMutableCopy<MyTestObject> wrapper(std::move(w1));

    EXPECT_EQ(wrapper->data(), "1234");
    EXPECT_NE(&*wrapper, &a);  // different object.
    EXPECT_EQ(MyTestObject::num_moves_, 1);

    // We can mutate it without issue and not further work.
    wrapper.get_mutable()->set_data("hello");
    EXPECT_EQ(wrapper->data(), "hello");
    wrapper.get_mutable()->set_data("other");
    EXPECT_EQ(wrapper->data(), "other");
  }

  EXPECT_EQ(MyTestObject::num_copies_, 0);
  EXPECT_EQ(MyTestObject::num_moves_, 1);
  EXPECT_EQ(MyTestObject::num_deletes_, 1);
}

}  // namespace
}  // namespace operations_research
