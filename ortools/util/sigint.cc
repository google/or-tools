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

#include "ortools/util/sigint.h"

#include <csignal>
#include <cstdlib>
#include <functional>

#include "ortools/base/logging.h"
#include "ortools/port/os.h"

#if ORTOOLS_TARGET_OS_SUPPORTS_THREADS

namespace operations_research {

void SigintHandler::Register(const std::function<void()>& f) {
  handler_ = [this, f]() -> void {
    const int num_calls = ++num_calls_;
    if (num_calls < 3) {
      LOG(INFO)
          << "^C pressed " << num_calls << " times. "
          << "Interrupting the solver. Press 3 times to force termination.";
      if (num_calls == 1) f();
    } else if (num_calls == 3) {
      LOG(INFO) << "^C pressed 3 times. Forcing termination.";
      exit(EXIT_FAILURE);
    } else {
      // Another thread is already running exit(), do nothing.
    }
  };
  signal(SIGINT, &SigHandler);
}

// This method will be called by the system after the SIGINT signal.
// The parameter is the signal received.
void SigintHandler::SigHandler(int) { handler_(); }

// Unregister the signal handlers.
SigintHandler::~SigintHandler() {
  if (handler_ != nullptr) signal(SIGINT, SIG_DFL);
}

thread_local std::function<void()> SigintHandler::handler_;

void SigtermHandler::Register(const std::function<void()>& f) {
  handler_ = [f]() -> void { f(); };
  signal(SIGTERM, &SigHandler);
}

// This method will be called by the system after the SIGTERM signal.
// The parameter is the signal received.
void SigtermHandler::SigHandler(int) { handler_(); }

// Unregister the signal handlers.
SigtermHandler::~SigtermHandler() {
  if (handler_ != nullptr) signal(SIGTERM, SIG_DFL);
}

thread_local std::function<void()> SigtermHandler::handler_;

}  // namespace operations_research

#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS
