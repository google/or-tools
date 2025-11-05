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

// Test descriptors. This avoids depending on attributes from `attributes.h`
// in the tests to decouple the codegen tests from `attributes.h`.
#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_TESTING_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_TESTING_H_

#include "ortools/math_opt/elemental/codegen/gen.h"

namespace operations_research::math_opt::codegen {

inline CodegenAttrTypeDescriptor GetTestDescriptor() {
  return {.name = "TestAttr2",
          .value_type = CodegenAttrTypeDescriptor::ValueType::kDouble,
          .num_key_elements = 2,
          .symmetry = "SomeSymmetry",
          .attribute_names = {"a_name", "b_name"}};
}

inline AttrOpFunctionInfo GetTestFunctionInfo(bool with_key_parameter) {
  return {.return_type = Type::Named("ReturnType"),
          .has_key_parameter = with_key_parameter,
          .extra_parameters = {
              {{.type = Type::Named("ExtraParam"), .name = "extra_param"}}}};
}

}  // namespace operations_research::math_opt::codegen

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_TESTING_H_
