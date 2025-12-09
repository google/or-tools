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

#ifndef ORTOOLS_MATH_OPT_IO_LP_LP_NAME_H_
#define ORTOOLS_MATH_OPT_IO_LP_LP_NAME_H_

#include "absl/status/status.h"

namespace operations_research::lp_format {

// Returns true if c is valid character to be included the name of a variable
// or constraint in an LP file, where is_leading indicates if c is the first
// character of the name.
bool ValidateCharInName(unsigned char c, bool is_leading);

// Checks if `name` is a valid name for a variable or constraint in an LP file.
absl::Status ValidateName(absl::string_view name);

}  // namespace operations_research::lp_format

#endif  // ORTOOLS_MATH_OPT_IO_LP_LP_NAME_H_
