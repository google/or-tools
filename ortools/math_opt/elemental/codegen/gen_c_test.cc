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

#include "ortools/math_opt/elemental/codegen/gen_c.h"

#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/elemental/codegen/gen.h"
#include "ortools/math_opt/elemental/codegen/testing.h"

namespace operations_research::math_opt::codegen {
namespace {

TEST(GenC99DeclarationsTest, EmitElements) {
  std::string code;
  C99Declarations()->EmitElements({"some_name", "other_name"}, &code);
  EXPECT_EQ(code,
            R"(// The type of an element in the model.
enum MathOptElementType {
  kMathOptSomeName,
  kMathOptOtherName,
};

)");
}

TEST(GenC99DeclarationsTest, StartAttrType) {
  std::string code;
  C99Declarations()->StartAttrType(GetTestDescriptor(), &code);
  EXPECT_EQ(code,
            R"(typedef enum {
  kMathOptTestAttr2AName,
  kMathOptTestAttr2BName,
} MathOptTestAttr2;
)");
}

TEST(GenC99DeclarationsTest, WithoutKey) {
  std::string code;
  C99Declarations()->EmitAttrOp("Op", GetTestDescriptor(),
                                GetTestFunctionInfo(false), &code);
  EXPECT_EQ(
      code,
      R"(ReturnType MathOptTestAttr2Op(struct MathOptElemental* e, int attr, ExtraParam extra_param);
)");
}

TEST(GenC99DeclarationsTest, WithKey) {
  std::string code;
  C99Declarations()->EmitAttrOp("Op", GetTestDescriptor(),
                                GetTestFunctionInfo(true), &code);
  EXPECT_EQ(
      code,
      R"(ReturnType MathOptTestAttr2Op(struct MathOptElemental* e, int attr, int64_t key_0, int64_t key_1, ExtraParam extra_param);
)");
}

TEST(GenC99DefinitionsTest, WithoutKey) {
  std::string code;
  C99Definitions()->EmitAttrOp("Op", GetTestDescriptor(),
                               GetTestFunctionInfo(false), &code);
  EXPECT_EQ(
      code,
      R"(ReturnType MathOptTestAttr2Op(struct MathOptElemental* e, int attr, ExtraParam extra_param) {
  return CAttrOp<TestAttr2>::Op(e, attr, extra_param);
}
)");
}

TEST(GenC99DefinitionsTest, WithKey) {
  std::string code;
  C99Definitions()->EmitAttrOp("Op", GetTestDescriptor(),
                               GetTestFunctionInfo(true), &code);
  EXPECT_EQ(
      code,
      R"(ReturnType MathOptTestAttr2Op(struct MathOptElemental* e, int attr, int64_t key_0, int64_t key_1, ExtraParam extra_param) {
  return CAttrOp<TestAttr2>::Op(e, attr, AttrKey<2, SomeSymmetry>(key_0, key_1), extra_param);
}
)");
}

TEST(GenC99DefinitionsTest, StartAttrType) {
  std::string code;
  C99Definitions()->StartAttrType(GetTestDescriptor(), &code);
  EXPECT_EQ(code, "");
}
}  // namespace
}  // namespace operations_research::math_opt::codegen
