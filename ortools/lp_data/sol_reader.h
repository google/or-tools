// Copyright 2010-2022 Google LLC
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

// A reader for files in the SOL format.
// see https://en.wikipedia.org/wiki/Sol_(format)

#ifndef OR_TOOLS_LP_DATA_SOL_READER_H_
#define OR_TOOLS_LP_DATA_SOL_READER_H_

#include <string>

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {

// Parse a solution to `model` from a file.
absl::StatusOr<glop::DenseRow> ParseSolFile(const std::string& file_name,
                                            const glop::LinearProgram& model);
absl::StatusOr<MPSolutionResponse> ParseSolFile(const std::string& file_name,
                                                const MPModelProto& model);

// Parse a solution to `model` from a string.
absl::StatusOr<glop::DenseRow> ParseSolString(const std::string& solution,
                                              const glop::LinearProgram& model);
absl::StatusOr<MPSolutionResponse> ParseSolString(const std::string& solution,
                                                  const MPModelProto& model);

}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_SOL_READER_H_
