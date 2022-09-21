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

#ifndef OR_TOOLS_PORT_FILE_H_
#define OR_TOOLS_PORT_FILE_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace operations_research {

// See ortools/base/helpers.h
::absl::Status PortableFileSetContents(absl::string_view file_name,
                                       absl::string_view content);

::absl::Status PortableFileGetContents(absl::string_view file_name,
                                       std::string* output);

::absl::Status PortableDeleteFile(absl::string_view file_name);

// Returns true if successful.  Outputs temp file to filename.
bool PortableTemporaryFile(const char* directory_prefix,
                           std::string* filename_out);

}  // namespace operations_research

#endif  // OR_TOOLS_PORT_FILE_H_
