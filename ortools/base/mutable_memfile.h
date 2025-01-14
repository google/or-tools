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

#ifndef OR_TOOLS_BASE_MUTABLE_MEMFILE_H_
#define OR_TOOLS_BASE_MUTABLE_MEMFILE_H_

#include <string>
#include <string_view>

class RegisteredMutableMemFile {
 public:
  RegisteredMutableMemFile(std::string_view filename) : filename_(filename) {}

  ~RegisteredMutableMemFile() { std::remove(filename_.c_str()); }

 private:
  const std::string filename_;
};

#endif  // OR_TOOLS_BASE_MUTABLE_MEMFILE_H_
