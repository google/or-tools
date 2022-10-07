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

#ifndef OR_TOOLS_MATH_OPT_IO_MPS_CONVERTER_H_
#define OR_TOOLS_MATH_OPT_IO_MPS_CONVERTER_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

// Returns the model in MPS format.
//
// The RemoveNames() function can be used on the model to remove names if they
// should not be exported.
absl::StatusOr<std::string> ModelProtoToMps(const ModelProto& model);

// Reads an MPS file and converts it to a ModelProto (like MpsToModelProto
// above, but takes a file name instead of the file contents and reads the file.
//
//
// The file can be stored as plain text or gzipped (with the .gz extension).
//
absl::StatusOr<ModelProto> ReadMpsFile(absl::string_view filename);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_IO_MPS_CONVERTER_H_
