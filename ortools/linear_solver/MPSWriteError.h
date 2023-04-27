// Copyright 2023 RTE
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

// Initial version of this code was provided by RTE

#ifndef ORTOOLS_MPSWRITEERROR_H
#define ORTOOLS_MPSWRITEERROR_H
#include <stdexcept>
namespace operations_research {
class MPSWriteError : public std::runtime_error {
 public:
  MPSWriteError(const std::string& message) : std::runtime_error(message) {}
};
}  // namespace operations_research
#endif  // ORTOOLS_MPSWRITEERROR_H
