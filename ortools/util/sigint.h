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

#ifndef ORTOOLS_UTIL_SIGINT_H_
#define ORTOOLS_UTIL_SIGINT_H_

#include <atomic>
#include <functional>

#include "ortools/base/macros/os_support.h"

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
static_assert(operations_research::kTargetOsSupportsThreads);

namespace operations_research {

class SigintHandler {
 public:
  SigintHandler() = default;
  ~SigintHandler();

  // Catches ^C and call f() the first time this happen. If ^C is pressed 3
  // times, kill the program.
  void Register(const std::function<void()>& f);

 private:
  std::atomic<int> num_calls_ = 0;

  static void SigHandler(int s);
  thread_local static std::function<void()> handler_;
};

class SigtermHandler {
 public:
  SigtermHandler() = default;
  ~SigtermHandler();

  // Catches SIGTERM and call f(). It is recommended that f() calls exit() to
  // terminate the program.
  void Register(const std::function<void()>& f);

 private:
  static void SigHandler(int s);
  thread_local static std::function<void()> handler_;
};

}  // namespace operations_research

#else
static_assert(!operations_research::kTargetOsSupportsThreads);
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)

#endif  // ORTOOLS_UTIL_SIGINT_H_
