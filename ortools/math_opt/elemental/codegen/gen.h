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

// Language-agnostic utilities for `Elemental` codegen.
#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace operations_research::math_opt::codegen {

// The list of attribute operations supported by `Elemental`.
enum class AttrOp {
  kGet,
  kSet,
  kIsNonDefault,
  kNumNonDefaults,
  kGetNonDefaults,
  // Do not use.
  kNumOps,
};

static constexpr int kNumAttrOps = static_cast<int>(AttrOp::kNumOps);

// A struct to represent an attribute type descriptor during codegen.
struct CodegenAttrTypeDescriptor {
  // The attribute type name.
  absl::string_view name;
  // The value type of the attribute.
  enum class ValueType {
    kBool,
    kInt64,
    kDouble,
  };
  ValueType value_type;
  // The number of key elements.
  int num_key_elements;
  // The key symmetry.
  std::string symmetry;

  // The names of the attributes of this type.
  std::vector<absl::string_view> attribute_names;
};

// Representations for types.
class Type {
 public:
  // A named type, e.g. "double".
  static std::shared_ptr<Type> Named(std::string name);
  // A pointer type.
  static std::shared_ptr<Type> Pointer(std::shared_ptr<Type> pointee);
  // A placeholder for the attribute value type, which is yet unknown when types
  // are defined. This gets replaced by `attr_value_type` when calling `Print`.
  static std::shared_ptr<Type> AttrValueType();

  virtual ~Type();

  // Prints the type to `out`, replacing `AttrValueType` placeholders with
  // `attr_value_type`.
  virtual void Print(absl::string_view attr_value_type,
                     std::string* out) const = 0;
};

// Information about how to codegen a given `AttrOp` in a given language.
struct AttrOpFunctionInfo {
  // The return type of the function.
  std::shared_ptr<Type> return_type;

  // If true, the function has an `AttrKey` parameter.
  bool has_key_parameter;

  // Extra parameters (e.g. {"double", "value"} for `Set` operations).
  struct ExtraParameter {
    std::shared_ptr<Type> type;
    std::string name;
  };
  std::vector<ExtraParameter> extra_parameters;
};

using AttrOpFunctionInfos = std::array<AttrOpFunctionInfo, kNumAttrOps>;

// The code generator interface.
class CodeGenerator {
 public:
  explicit CodeGenerator(const AttrOpFunctionInfos* attr_op_function_infos)
      : attr_op_function_infos_(*attr_op_function_infos) {}

  virtual ~CodeGenerator() = default;

  // Generates code.
  std::string GenerateCode() const;

  // Emits the header for the generated code.
  virtual void EmitHeader(std::string* out) const {}

  // Emits code for elements.
  virtual void EmitElements(absl::Span<const absl::string_view> elements,
                            std::string* out) const {}

  // Emits code for attributes. By default, this iterates attributes and for
  // each attribute:
  //   - calls `StartAttrType`, and
  //   - calls `EmitAttrOp` for each operation.
  virtual void EmitAttributes(
      absl::Span<const CodegenAttrTypeDescriptor> descriptors,
      std::string* out) const;

  // Called before generating code for an attribute type.
  virtual void StartAttrType(const CodegenAttrTypeDescriptor& descriptor,
                             std::string* out) const {}

  // Emits code for operation `info` for attribute described by `descriptor`.
  virtual void EmitAttrOp(absl::string_view op_name,
                          const CodegenAttrTypeDescriptor& descriptor,
                          const AttrOpFunctionInfo& info,
                          std::string* out) const {}

 private:
  void EmitAttrType(const CodegenAttrTypeDescriptor& descriptor,
                    std::string* out) const;

  const AttrOpFunctionInfos& attr_op_function_infos_;
};

}  // namespace operations_research::math_opt::codegen

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_H_
