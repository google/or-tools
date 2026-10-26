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

#ifndef ORTOOLS_BASE_OPTIONS_H_
#define ORTOOLS_BASE_OPTIONS_H_

namespace file {

// A dummy Options class to match internal file API.
using Options = int;

inline Options Defaults() { return 0xBABA; }

}  // namespace file

#endif  // ORTOOLS_BASE_OPTIONS_H_
