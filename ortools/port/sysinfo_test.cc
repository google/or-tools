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
#include "ortools/base/macros/os.h"

#if defined(ORTOOLS_TARGET_OS_LINUX) || defined(ORTOOLS_TARGET_OS_MACOS) || \
    defined(ORTOOLS_TARGET_OS_ANY_BSD) || defined(ORTOOLS_TARGET_OS_WINDOWS)
#define ORTOOLS_SYSINFO_SUPPORTED
#endif

namespace operations_research {
namespace sysinfo {
namespace {

TEST(SysinfoTest, MemoryUsageProcess) {
  std::optional<int64_t> memory_usage = MemoryUsageProcess();
#if defined(ORTOOLS_SYSINFO_SUPPORTED)
  ASSERT_TRUE(memory_usage.has_value());
  EXPECT_GT(*memory_usage, 0);
#else
  ASSERT_FALSE(memory_usage.has_value());
#endif
}

}  // namespace
}  // namespace sysinfo
}  // namespace operations_research
