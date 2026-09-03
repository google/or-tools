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

#include "ortools/base/unique_array.h"

#include <cstddef>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace gtl {
namespace internal {

[[noreturn]] void CrashOnNullUniqueArrayWithNonzeroSize(size_t size) {
  // Although the caller has already determined that size != 0, we repeat the
  // comparison here to put the usual CHECK failure message into the log.
  CHECK_EQ(size, 0ul);

  LOG(FATAL) << "Bug in UniquePtr: CrashOnNullUniqueArrayWithNonzeroSize"
                " called with zero size";
}

}  // namespace internal
}  // namespace gtl
