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

#include "ortools/math_opt/elemental/codegen/gen.h"

#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt::codegen {
namespace {
using testing::HasSubstr;
using testing::StartsWith;

const AttrOpFunctionInfos* GetFunctionInfos() {
  static const auto* const kResult = new AttrOpFunctionInfos({
      {.return_type = Type::Named("TypeForGet"),
       .has_key_parameter = false,
       .extra_parameters = {}},
      {.return_type = Type::Pointer(Type::AttrValueType()),
       .has_key_parameter = true,
       .extra_parameters = {}},
      {.return_type = Type::Named("T"),
       .has_key_parameter = false,
       .extra_parameters = {}},
      {.return_type = Type::Named("T"),
       .has_key_parameter = false,
       .extra_parameters = {}},
      {.return_type = Type::Named("T"),
       .has_key_parameter = false,
       .extra_parameters = {}},
  });
  return kResult;
}

class TestCodeGenerator : public CodeGenerator {
 public:
  TestCodeGenerator() : CodeGenerator(GetFunctionInfos()) {}

  void EmitHeader(std::string* out) const override {
    absl::StrAppend(out, "# DO NOT EDIT: Test\n");
  }

  void EmitElements(absl::Span<const absl::string_view> elements,
                    std::string* out) const override {
    absl::StrAppend(out, "Elements: ", absl::StrJoin(elements, ", "), "\n");
  }

  void StartAttrType(const CodegenAttrTypeDescriptor&,
                     std::string* out) const override {
    absl::StrAppend(out, "\n");
  }

  void EmitAttrOp(absl::string_view op_name,
                  const CodegenAttrTypeDescriptor& descriptor,
                  const AttrOpFunctionInfo& info,
                  std::string* out) const override {
    info.return_type->Print("fake_type", out);
    absl::StrAppend(out, " ", descriptor.name, op_name, "\n");
  }
};

TEST(GenerateCodeTest, Attrs) {
  const std::string code = TestCodeGenerator().GenerateCode();
  EXPECT_THAT(code, StartsWith("# DO NOT EDIT: Test\n"));
  EXPECT_THAT(code, HasSubstr("Elements: variable, linear_constraint, "));
  EXPECT_THAT(code, HasSubstr("TypeForGet BoolAttr0Get\n"
                              "fake_type* BoolAttr0Set\n"
                              "T BoolAttr0IsNonDefault\n"
                              "T BoolAttr0NumNonDefaults\n"
                              "T BoolAttr0GetNonDefaults\n"));
  EXPECT_THAT(code, HasSubstr("TypeForGet DoubleAttr1Get\n"
                              "fake_type* DoubleAttr1Set\n"
                              "T DoubleAttr1IsNonDefault\n"
                              "T DoubleAttr1NumNonDefaults\n"
                              "T DoubleAttr1GetNonDefaults\n"));
}

}  // namespace
}  // namespace operations_research::math_opt::codegen
