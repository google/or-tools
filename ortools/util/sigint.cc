// Copyright 2010-2021 Google LLC
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

#include "ortools/base/logging.h"

namespace operations_research {

void SigintHandler::Register(const std::function<void()>& f) {
  handler_ = [this, f]() -> void {
    ++num_sigint_calls_;
    if (num_sigint_calls_ >= 3) {
      LOG(INFO) << "^C pressed " << num_sigint_calls_
                << " times. Forcing termination.";
      exit(EXIT_FAILURE);
    }
    LOG(INFO) << "^C pressed " << num_sigint_calls_ << " times. "
              << "Interrupting the solver. Press 3 times to force termination.";
    if (num_sigint_calls_ == 1) f();
  };
  signal(SIGINT, &ControlCHandler);
}

// This method will be called by the system after the SIGINT signal.
// The parameter is the signal received.
void SigintHandler::ControlCHandler(int sig) { handler_(); }

// Unregister the SIGINT handler.
SigintHandler::~SigintHandler() { signal(SIGINT, SIG_DFL); }

thread_local std::function<void()> SigintHandler::handler_;

}  // namespace operations_research
