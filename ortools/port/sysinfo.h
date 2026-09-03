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

#ifndef ORTOOLS_PORT_SYSINFO_H_
#define ORTOOLS_PORT_SYSINFO_H_

#include <cstdint>
#include <optional>

namespace operations_research {
namespace sysinfo {

// Return the Resident Set Size (RSS) memory usage in bytes of the process if
// available on the platform, `std::nullopt` otherwise.
//
// This is currently supported on Linux, MacOS, Windows, NetBSD, OpenBSD, and
// FreeBSD. Android and IOS are not yet supported.
// Note that when called from an interpreted language like Java, the reported
// memory will include the virtual machine's memory usage.
std::optional<uint64_t> MemoryUsageProcess();

}  // namespace sysinfo
}  // namespace operations_research

#endif  // ORTOOLS_PORT_SYSINFO_H_
