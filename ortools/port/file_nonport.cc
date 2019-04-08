// Copyright 2010-2018 Google LLC
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

#include "ortools/base/file.h"
#include "ortools/port/file.h"

namespace operations_research {

::util::Status PortableFileSetContents(absl::string_view file_name,
                                       absl::string_view content) {
  return file::SetContents(file_name, content, file::Defaults());
}

::util::Status PortableFileGetContents(absl::string_view file_name,
                                       std::string* output) {
  return file::GetContents(file_name, output, file::Defaults());
}

bool PortableTemporaryFile(const char* directory_prefix,
                           std::string* filename_out) {
  return false;
}

::util::Status PortableDeleteFile(absl::string_view file_name) {
  return file::Delete(file_name, file::Defaults());
}

}  // namespace operations_research
