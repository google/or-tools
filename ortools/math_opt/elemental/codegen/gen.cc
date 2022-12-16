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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt::codegen {

namespace {

class NamedType : public Type {
 public:
  explicit NamedType(std::string name) : name_(std::move(name)) {}

  void Print(absl::string_view, std::string* out) const final {
    absl::StrAppend(out, name_);
  }

 private:
  std::string name_;
};

class PointerType : public Type {
 public:
  explicit PointerType(std::shared_ptr<Type> pointee)
      : pointee_(std::move(pointee)) {}

  void Print(absl::string_view attr_value_type, std::string* out) const final {
    pointee_->Print(attr_value_type, out);
    absl::StrAppend(out, "*");
  }

 private:
  std::shared_ptr<Type> pointee_;
};

class AttrValueTypeType : public Type {
 public:
  void Print(absl::string_view attr_value_type, std::string* out) const final {
    absl::StrAppend(out, attr_value_type);
  }
};

}  // namespace

std::shared_ptr<Type> Type::Named(std::string name) {
  return std::make_shared<NamedType>(std::move(name));
}

std::shared_ptr<Type> Type::Pointer(std::shared_ptr<Type> pointee) {
  return std::make_shared<PointerType>(std::move(pointee));
}

std::shared_ptr<Type> Type::AttrValueType() {
  return std::make_shared<AttrValueTypeType>();
}

Type::~Type() = default;

CodegenAttrTypeDescriptor::ValueType GetValueType(bool) {
  return CodegenAttrTypeDescriptor::ValueType::kBool;
}

CodegenAttrTypeDescriptor::ValueType GetValueType(int64_t) {
  return CodegenAttrTypeDescriptor::ValueType::kInt64;
}

CodegenAttrTypeDescriptor::ValueType GetValueType(double) {
  return CodegenAttrTypeDescriptor::ValueType::kDouble;
}

template <ElementType element_type>
CodegenAttrTypeDescriptor::ValueType GetValueType(ElementId<element_type>) {
  // Element ids are untyped in wrapped APIs.
  return CodegenAttrTypeDescriptor::ValueType::kInt64;
}

template <typename Descriptor>
CodegenAttrTypeDescriptor MakeAttrTypeDescriptor() {
  CodegenAttrTypeDescriptor descriptor;
  descriptor.value_type = GetValueType(typename Descriptor::ValueType{});
  descriptor.name = Descriptor::kName;
  descriptor.num_key_elements = Descriptor::kNumKeyElements;
  descriptor.symmetry = Descriptor::Symmetry::GetName();

  descriptor.attribute_names.reserve(Descriptor::NumAttrs());
  for (const auto& attr_descriptor : Descriptor::kAttrDescriptors) {
    descriptor.attribute_names.push_back(attr_descriptor.name);
  }
  return descriptor;
}

constexpr absl::string_view kOpNames[static_cast<int>(AttrOp::kNumOps)] = {
    "Get", "Set", "IsNonDefault", "NumNonDefaults", "GetNonDefaults"};

void CodeGenerator::EmitAttrType(const CodegenAttrTypeDescriptor& descriptor,
                                 std::string* out) const {
  StartAttrType(descriptor, out);
  for (int op = 0; op < kNumAttrOps; ++op) {
    const AttrOpFunctionInfo& op_info = attr_op_function_infos_[op];
    EmitAttrOp(kOpNames[op], descriptor, op_info, out);
  }
}

void CodeGenerator::EmitAttributes(
    absl::Span<const CodegenAttrTypeDescriptor> descriptors,
    std::string* out) const {
  for (const auto& descriptor : descriptors) {
    StartAttrType(descriptor, out);
    for (int i = 0; i < kNumAttrOps; ++i) {
      EmitAttrOp(kOpNames[i], descriptor, attr_op_function_infos_[i], out);
    }
  }
}

std::string CodeGenerator::GenerateCode() const {
  std::string out;
  EmitHeader(&out);

  // Generate elements.
  EmitElements(kElementNames, &out);

  // Generate attributes.
  std::vector<CodegenAttrTypeDescriptor> attr_type_descriptors;
  ForEach(
      [&attr_type_descriptors](auto type_descriptor) {
        attr_type_descriptors.push_back(
            MakeAttrTypeDescriptor<decltype(type_descriptor)>());
      },
      AllAttrTypeDescriptors{});
  EmitAttributes(attr_type_descriptors, &out);

  return out;
}

}  // namespace operations_research::math_opt::codegen
