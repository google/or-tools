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

#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/elemental/codegen/gen.h"

namespace operations_research::math_opt::codegen {

namespace {

// A helper to generate parameters to pass `n` key element indices, e.g:
//   ", int64_t key_0, int64_t key_1" (parameters)
void AddKeyParams(int n, std::string* out) {
  for (int i = 0; i < n; ++i) {
    absl::StrAppend(out, ", int64_t key_", i);
  }
}

// A helper to generate an AttrKey argument to pass `n` key element indices,
// e.g: "AttrKey<2, NoSymmetry>(key_0, key_1)".
void AddAttrKeyArg(int n, absl::string_view symmetry, std::string* out) {
  absl::StrAppendFormat(out, ", AttrKey<%i, %s>(", n, symmetry);
  for (int i = 0; i < n; ++i) {
    if (i != 0) {
      absl::StrAppend(out, ", ");
    }
    absl::StrAppend(out, "key_", i);
  }
  absl::StrAppend(out, ")");
}

// Returns the C99 name for the given type.
absl::string_view GetCTypeName(
    CodegenAttrTypeDescriptor::ValueType value_type) {
  switch (value_type) {
    case CodegenAttrTypeDescriptor::ValueType::kBool:
      return "_Bool";
    case CodegenAttrTypeDescriptor::ValueType::kInt64:
      return "int64_t";
    case CodegenAttrTypeDescriptor::ValueType::kDouble:
      return "double";
  }
}

// Turns an element/attribute name (e.g. "some_name") into a camel case name
// (e.g. "SomeName").
std::string NameToCamelCase(absl::string_view attr_name) {
  std::string result;
  result.reserve(attr_name.size());
  CHECK(!attr_name.empty());
  const char first = attr_name[0];
  CHECK(absl::ascii_isalpha(first) && absl::ascii_islower(first))
      << "invalid attr name: " << attr_name;
  result.push_back(absl::ascii_toupper(first));
  for (int i = 1; i < attr_name.size(); ++i) {
    const char c = attr_name[i];
    if (c == '_') {
      ++i;
      CHECK(i < attr_name.size()) << "invalid attr name: " << attr_name;
      const char next_c = attr_name[i];
      CHECK(absl::ascii_isalnum(next_c)) << "invalid attr name: " << attr_name;
      result.push_back(absl::ascii_toupper(next_c));
    } else {
      CHECK(absl::ascii_isalnum(c)) << "invalid attr name: " << attr_name;
      CHECK(absl::ascii_islower(c)) << "invalid attr name: " << attr_name;
      result.push_back(c);
    }
  }
  return result;
}

// Returns the type of the C status.
std::shared_ptr<Type> GetStatusType() { return Type::Named("int"); }

const AttrOpFunctionInfos* GetC99FunctionInfos() {
  static const auto* const kResult = new AttrOpFunctionInfos({
      // Get.
      AttrOpFunctionInfo{
          .return_type = GetStatusType(),
          .has_key_parameter = true,
          .extra_parameters = {{.type = Type::Pointer(Type::AttrValueType()),
                                .name = "value"}}},
      // Set.
      AttrOpFunctionInfo{.return_type = GetStatusType(),
                         .has_key_parameter = true,
                         .extra_parameters = {{.type = Type::AttrValueType(),
                                               .name = "value"}}},
      // IsNonDefault.
      AttrOpFunctionInfo{
          .return_type = GetStatusType(),
          .has_key_parameter = true,
          .extra_parameters = {{.type = Type::Pointer(Type::Named("_Bool")),
                                .name = "out_is_non_default"}}},
      // NumNonDefaults.
      AttrOpFunctionInfo{
          .return_type = GetStatusType(),
          .has_key_parameter = false,
          .extra_parameters = {{.type = Type::Pointer(Type::Named("int64_t")),
                                .name = "out_num_non_defaults"}}},
      // GetNonDefaults.
      AttrOpFunctionInfo{
          .return_type = GetStatusType(),
          .has_key_parameter = false,
          .extra_parameters =
              {
                  {.type = Type::Pointer(Type::Named("int64_t")),
                   .name = "out_num_non_defaults"},
                  {.type = Type::Pointer(Type::Pointer(Type::Named("int64_t"))),
                   .name = "out_non_defaults"},
              }},
  });
  return kResult;
}

class C99CodeGeneratorBase : public CodeGenerator {
 public:
  using CodeGenerator::CodeGenerator;

  void EmitHeader(std::string* out) const final {
    absl::StrAppend(out, R"(
/* DO NOT EDIT: This file is autogenerated. */
#ifndef MATHOPTH_GENERATED
#error "this file is intended to be included, do not use directly"
#endif
)");
  }
};

// Emits the prototype for a function.
void EmitPrototype(absl::string_view op_name,
                   const CodegenAttrTypeDescriptor& descriptor,
                   const AttrOpFunctionInfo& info, std::string* out) {
  absl::string_view attr_value_type = GetCTypeName(descriptor.value_type);
  // Adds the return type, function name and common parameters.
  info.return_type->Print(attr_value_type, out);
  absl::StrAppendFormat(out,
                        " MathOpt%s%s(struct "
                        "MathOptElemental* e, int attr",
                        descriptor.name, op_name);
  // Add the key.
  if (info.has_key_parameter) {
    AddKeyParams(descriptor.num_key_elements, out);
  }
  // Add extra parameters.
  for (const auto& extra_param : info.extra_parameters) {
    absl::StrAppend(out, ", ");
    extra_param.type->Print(attr_value_type, out);
    absl::StrAppend(out, " ", extra_param.name);
  }
  // Finish prototype.
  absl::StrAppend(out, ")");
}

class C99DeclarationsGenerator : public C99CodeGeneratorBase {
 public:
  C99DeclarationsGenerator() : C99CodeGeneratorBase(GetC99FunctionInfos()) {}

  void EmitElements(absl::Span<const absl::string_view> elements,
                    std::string* out) const override {
    // Generate an enum for the elements.
    absl::StrAppend(out,
                    "// The type of an element in the model.\n"
                    "enum MathOptElementType {\n");
    for (const auto& element_name : elements) {
      absl::StrAppendFormat(out, "  kMathOpt%s,\n",
                            NameToCamelCase(element_name));
    }
    absl::StrAppend(out, "};\n\n");
  }

  void EmitAttrOp(absl::string_view op_name,
                  const CodegenAttrTypeDescriptor& descriptor,
                  const AttrOpFunctionInfo& info,
                  std::string* out) const override {
    // Just emit a prototype.
    EmitPrototype(op_name, descriptor, info, out);
    absl::StrAppend(out, ";\n");
  }

  void StartAttrType(const CodegenAttrTypeDescriptor& descriptor,
                     std::string* out) const override {
    // Generate an enum for the attribute type.
    absl::StrAppendFormat(out, "typedef enum {\n");
    for (absl::string_view attr_name : descriptor.attribute_names) {
      absl::StrAppendFormat(out, "  kMathOpt%s%s,\n", descriptor.name,
                            NameToCamelCase(attr_name));
    }
    absl::StrAppendFormat(out, "} MathOpt%s;\n", descriptor.name);
  }
};

class C99DefinitionsGenerator : public C99CodeGeneratorBase {
 public:
  C99DefinitionsGenerator() : C99CodeGeneratorBase(GetC99FunctionInfos()) {}

  void EmitAttrOp(absl::string_view op_name,
                  const CodegenAttrTypeDescriptor& descriptor,
                  const AttrOpFunctionInfo& info,
                  std::string* out) const override {
    EmitPrototype(op_name, descriptor, info, out);
    // Emit a call to the wrapper (e.g. `CAttrOp<Descriptor>::Op`).
    absl::StrAppendFormat(out, " {\n  return CAttrOp<%s>::%s(e, attr",
                          descriptor.name, op_name);
    // Add the key argument.
    if (info.has_key_parameter) {
      AddAttrKeyArg(descriptor.num_key_elements, descriptor.symmetry, out);
    }
    // Add extra parameter arguments.
    for (const auto& extra_param : info.extra_parameters) {
      absl::StrAppend(out, ", ", extra_param.name);
    }
    absl::StrAppend(out, ");\n}\n");
  }
};
}  // namespace

std::unique_ptr<CodeGenerator> C99Declarations() {
  return std::make_unique<C99DeclarationsGenerator>();
}

std::unique_ptr<CodeGenerator> C99Definitions() {
  return std::make_unique<C99DefinitionsGenerator>();
}

}  // namespace operations_research::math_opt::codegen
