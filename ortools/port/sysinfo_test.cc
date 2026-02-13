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

#include "ortools/port/sysinfo.h"

#include <cstdint>
#include <optional>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/macros/os.h"

namespace operations_research {
namespace sysinfo {
namespace {

using ::testing::Gt;
using ::testing::Optional;

TEST(SysinfoTest, MemoryUsageProcess) {
  const std::optional<int64_t> memory_usage = MemoryUsageProcess();
  switch (kTargetOs) {
    case TargetOs::kLinux:
    case TargetOs::kMacOS:
    case TargetOs::kFreeBsd:
    case TargetOs::kNetBsd:
    case TargetOs::kOpenBsd:
    case TargetOs::kWindows:
      ASSERT_THAT(memory_usage, Optional(Gt(0)));
      break;
    default:
      ASSERT_EQ(memory_usage, std::nullopt);
      break;
  }
}

}  // namespace
}  // namespace sysinfo
}  // namespace operations_research
