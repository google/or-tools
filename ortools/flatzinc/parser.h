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

#ifndef ORTOOLS_FLATZINC_PARSER_H_
#define ORTOOLS_FLATZINC_PARSER_H_

#include "absl/strings/string_view.h"
#include "ortools/flatzinc/model.h"

namespace operations_research::fz {

// Parses the FlatZinc (.fzn) file at `filename` into `*model` (which must be
// non-null). On I/O, lexical, syntax, or semantic errors, logs a diagnostic
// via `LOG(ERROR)` and returns false (`*model` may be partially populated).
bool ParseFlatzincFile(absl::string_view filename, Model* model);

// Parses an in-memory FlatZinc (.fzn) model text into `*model` (which must be
// non-null). Logs `line:column` diagnostics via `LOG(ERROR)` and returns false
// on malformed input (`*model` may be partially populated).
bool ParseFlatzincString(absl::string_view input, Model* model);

}  // namespace operations_research::fz

#endif  // ORTOOLS_FLATZINC_PARSER_H_
