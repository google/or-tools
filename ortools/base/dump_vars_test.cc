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

#include "ortools/base/dump_vars.h"

#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace operations_research::base {
namespace {

template <class T>
::std::string ToString(const T& t) {
  ::std::ostringstream oss;
  oss << t;
  return oss.str();
}

TEST(DumpVars, Empty) {
  EXPECT_EQ(R"()", ToString(DUMP_VARS()));
  EXPECT_EQ(R"()", DUMP_VARS().str());
}

#if !defined(__APPLE__)
TEST(DumpVars, Lvalue) {
  int a = 42;
  EXPECT_EQ(R"(a = 42)", ToString(DUMP_VARS(a)));
  ::std::string foo = "hello";
  EXPECT_EQ(R"(foo = hello)", ToString(DUMP_VARS(foo)));
  EXPECT_EQ(R"(foo = hello)", DUMP_VARS(foo).str());
  EXPECT_EQ(R"(x = hello)", ToString(DUMP_VARS(foo).as("x")));
}

TEST(DumpVars, Rvalue) {
  EXPECT_EQ("2 + 2 = 4", ToString(DUMP_VARS(2 + 2)));
  EXPECT_EQ("2 + 2 = 4", DUMP_VARS(2 + 2).str());
  EXPECT_EQ("x = 4", ToString(DUMP_VARS(2 + 2).as("x")));
}

#define FORTY_TWO 42
#define ONE_AND_TWO 1, 2

TEST(DumpVars, Macro) {
  // Macros get evaluated before they are stringized. It's not necessarily good,
  // but we'll have a test for it to serve as a documentation of facts.
  EXPECT_EQ("42 = 42", ToString(DUMP_VARS(FORTY_TWO)));
  EXPECT_EQ("42 = 42", DUMP_VARS(FORTY_TWO).str());

  EXPECT_EQ("1 = 1, 2 = 2", ToString(DUMP_VARS(ONE_AND_TWO)));
  EXPECT_EQ("1 = 1, 2 = 2", DUMP_VARS(ONE_AND_TWO).str());
  EXPECT_EQ("one = 1, two = 2",
            ToString(DUMP_VARS(ONE_AND_TWO).as("one", "two")));
}

template <int A, int B>
int Plus() {
  return A + B;
}

TEST(DumpVars, Parens) {
  EXPECT_EQ("x = 5", ToString(DUMP_VARS(Plus<2, 3>()).as("x")));
  EXPECT_EQ("(Plus<2, 3>()) = 5", ToString(DUMP_VARS((Plus<2, 3>()))));
  EXPECT_EQ("(Plus<2, 3>()) = 5", DUMP_VARS((Plus<2, 3>())).str());
  EXPECT_EQ("((Plus<2, 3>())) = 5", ToString(DUMP_VARS(((Plus<2, 3>())))));
  EXPECT_EQ("((Plus<2, 3>())) = 5", DUMP_VARS(((Plus<2, 3>()))).str());
  EXPECT_EQ("Parens = 5", DUMP_VARS(((Plus<2, 3>()))).as("Parens").str());
}

TEST(DumpVars, Bindings) {
  // Using a unique_ptr to ensure there is no copy.
  std::vector<std::pair<int, std::unique_ptr<std::string>>> v;
  v.push_back({3, std::make_unique<std::string>("hello")});
  const std::string foo = "bar";
  for (const auto& [i, s] : v) {
    EXPECT_EQ("i = 3, *s = hello, foo = bar",
              ToString(DUMP_VARS_WITH_BINDINGS((i, s), i, *s, foo)));
  }
}

TEST(DumpVars, NamesOverride) {
  EXPECT_EQ("z = 5", ToString(DUMP_VARS(5).as().as("x", "y").as("z")));
}

TEST(DumpVars, TwoValues) {
  int foo = 42;
  int bar = 24;
  EXPECT_EQ("foo = 42, bar = 24", ToString(DUMP_VARS(foo, bar)));
  EXPECT_EQ("foo = 42, bar = 24", DUMP_VARS(foo, bar).str());
  EXPECT_EQ("bar = 42, foo = 24", DUMP_VARS(foo, bar).as("bar", "foo").str());
}

TEST(DumpVars, ManyArgs) {
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 5;
  int e = 7;
  int f = 11;
  EXPECT_EQ("a = 1, b = 2, c = 3, d = 5, e = 7, f = 11",
            ToString(DUMP_VARS(a, b, c, d, e, f)));
  EXPECT_EQ("a = 1, b = 2, c = 3, d = 5, e = 7, f = 11",
            DUMP_VARS(a, b, c, d, e, f).str());
}

TEST(DumpVars, Vector) {
  std::vector<float> vec = {49.3, 3.14};
  EXPECT_EQ("vec = 49.299999,3.140000,", ToString(DUMP_VARS(vec)));
  EXPECT_EQ("vec = 49.299999,3.140000,", DUMP_VARS(vec).str());
}

TEST(DumpVars, Optional) {
  std::optional<float> of = {};
  EXPECT_EQ("of = (none)", ToString(DUMP_VARS(of)));
  EXPECT_EQ("of = (none)", DUMP_VARS(of).str());

  of = 49.3f;
  EXPECT_EQ("of = 49.299999", ToString(DUMP_VARS(of)));
  EXPECT_EQ("of = 49.299999", DUMP_VARS(of).str());
}

TEST(DumpVars, LazyEvaluation) {
  {
    int n = 0;
    auto F = [&]() { return ++n; };
    auto vars = DUMP_VARS(F());
    EXPECT_EQ(0, n);
    EXPECT_EQ("F() = 1", ToString(vars));
    EXPECT_EQ(1, n);
    EXPECT_EQ("F() = 2", ToString(vars));
    EXPECT_EQ(2, n);
    EXPECT_EQ("F() = 3", vars.str());
    EXPECT_EQ(3, n);
    EXPECT_EQ("F() = 4", vars.str());
    EXPECT_EQ(4, n);
    EXPECT_EQ("5 = 5", vars.as("5").str());
    EXPECT_EQ(5, n);
  }
  {
    int n = 0;
    auto F = [&]() { return ++n; };
    auto vars = DUMP_VARS(F()).as("x");
    EXPECT_EQ(0, n);
    EXPECT_EQ("x = 1", ToString(vars));
    EXPECT_EQ(1, n);
    EXPECT_EQ("x = 2", ToString(vars));
    EXPECT_EQ(2, n);
    EXPECT_EQ("x = 3", vars.str());
    EXPECT_EQ(3, n);
    EXPECT_EQ("x = 4", vars.str());
    EXPECT_EQ(4, n);
    EXPECT_EQ("y = 5", vars.as("y").str());
    EXPECT_EQ(5, n);
  }
}

TEST(DumpVars, TemporaryLifetime) {
  EXPECT_EQ(R"(std::string_view(std::string("hello")) = hello)",
            ToString(DUMP_VARS(std::string_view(std::string("hello")))));
  auto v = DUMP_VARS(std::string_view(std::string("hello")));
  EXPECT_EQ(R"(std::string_view(std::string("hello")) = hello)", ToString(v));
  EXPECT_EQ(R"(temp = hello)", ToString(v.as("temp")));
}
#endif  // !defined(__APPLE__)

}  // namespace
}  // namespace operations_research::base
