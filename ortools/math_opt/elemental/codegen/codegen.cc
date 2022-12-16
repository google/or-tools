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

#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/elemental/codegen/gen.h"
#include "ortools/math_opt/elemental/codegen/gen_c.h"
#include "ortools/math_opt/elemental/codegen/gen_python.h"

ABSL_FLAG(std::string, binding_type, "", "The binding type to generate.");

namespace operations_research::math_opt::codegen {

namespace {
void Main() {
  const std::string binding_type = absl::GetFlag(FLAGS_binding_type);
  if (binding_type == "c99_h") {
    std::cout << C99Declarations()->GenerateCode();
  } else if (binding_type == "c99_cc") {
    std::cout << C99Definitions()->GenerateCode();
  } else if (binding_type == "python_enums") {
    std::cout << PythonEnums()->GenerateCode();
  } else {
    LOG(FATAL) << "unknown binding type: '" << binding_type << "'";
  }
}

}  // namespace
}  // namespace operations_research::math_opt::codegen

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);
  operations_research::math_opt::codegen::Main();
  return 0;
}
