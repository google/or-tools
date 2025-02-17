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

#include "ortools/sat/model.h"

#include <string>

#include "gtest/gtest.h"

namespace operations_research {
namespace sat {
namespace {

struct A {
  A() = default;
  explicit A(Model* model) {}
  std::string name;
};

class B {
 public:
  explicit B(A* a) : a_(a) {}
  explicit B(Model* model) : a_(model->GetOrCreate<A>()) {}

  std::string name() const { return a_->name; }

 private:
  A* a_;
};

TEST(ModelTest, RecursiveCreationTest) {
  Model model;
  B* b = model.GetOrCreate<B>();
  model.GetOrCreate<A>()->name = "test";
  EXPECT_EQ("test", b->name());
}

struct C1 {
  C1() = default;
};
struct C2 {
  explicit C2(Model* model) {}
};
struct C3 {
  C3() : name("no_arg") {}
  explicit C3(Model*) : name("model") {}
  std::string name;
};

TEST(ModelTest, DefaultConstructorFallback) {
  Model model;
  model.GetOrCreate<C1>();
  model.GetOrCreate<C2>();
  EXPECT_EQ(model.GetOrCreate<C3>()->name, "model");
}

TEST(ModelTest, Register) {
  Model model;
  C3 c3;
  c3.name = "Shared struct";
  model.Register(&c3);
  EXPECT_EQ(model.GetOrCreate<C3>()->name, c3.name);
}

TEST(ModelTest, RegisterDeathTest) {
  Model model;
  C3 c3;
  model.Register(&c3);
  C3 c3_2;
  EXPECT_DEATH(model.Register(&c3_2), "");
}

TEST(ModelTest, RegisterDeathTest2) {
  Model model;
  model.GetOrCreate<C3>();
  C3 c3;
  EXPECT_DEATH(model.Register(&c3), "");
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
