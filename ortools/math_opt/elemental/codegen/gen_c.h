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

// The C99 code generator.
#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_C_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_C_H_

#include <memory>

#include "ortools/math_opt/elemental/codegen/gen.h"

namespace operations_research::math_opt::codegen {

// Returns a generator for C99 declarations.
std::unique_ptr<CodeGenerator> C99Declarations();

// Returns a generator for C99 definitions.
std::unique_ptr<CodeGenerator> C99Definitions();

}  // namespace operations_research::math_opt::codegen

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_CODEGEN_GEN_C_H_
