// Copyright 2010-2017 Google
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

#include "ortools/base/status.h"
#include "ortools/base/string_view.h"

namespace operations_research {

// See ortools/base/file.h
::util::Status FileSetContents(absl::string_view file_name,
                               absl::string_view content);

::util::Status FileGetContents(absl::string_view file_name,
                               std::string* output);

::util::Status DeleteFile(absl::string_view file_name);

// Returns true if successful.  Outputs temp file to filename.
bool PortableTemporaryFile(const char* directory_prefix,
                           std::string* filename_out);

}  // namespace operations_research

#endif  // OR_TOOLS_PORT_FILE_H_
