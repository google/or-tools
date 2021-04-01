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

#include "ortools/linear_solver/linear_solver_callback.h"

#include "ortools/base/logging.h"

namespace operations_research {

std::string ToString(MPCallbackEvent event) {
  switch (event) {
    case MPCallbackEvent::kMipSolution:
      return "MIP_SOLUTION";
    case MPCallbackEvent::kMip:
      return "MIP";
    case MPCallbackEvent::kMipNode:
      return "MIP_NODE";
    case MPCallbackEvent::kBarrier:
      return "BARRIER";
    case MPCallbackEvent::kMessage:
      return "MESSAGE";
    case MPCallbackEvent::kPresolve:
      return "PRESOLVE";
    case MPCallbackEvent::kPolling:
      return "POLLING";
    case MPCallbackEvent::kMultiObj:
      return "MULTI_OBJ";
    case MPCallbackEvent::kSimplex:
      return "SIMPLEX";
    case MPCallbackEvent::kUnknown:
      return "UNKNOWN";
    default:
      LOG(FATAL) << "Unrecognized callback event: " << static_cast<int>(event);
  }
}

namespace {

// Returns true if any of the callbacks in a list might add cuts.
bool CallbacksMightAddCuts(const std::vector<MPCallback*>& callbacks) {
  for (MPCallback* callback : callbacks) {
    if (callback->might_add_cuts()) {
      return true;
    }
  }
  return false;
}

// Returns true if any of the callbacks in a list might add lazy constraints.
bool CallbacksMightAddLazyConstraints(
    const std::vector<MPCallback*>& callbacks) {
  for (MPCallback* callback : callbacks) {
    if (callback->might_add_lazy_constraints()) {
      return true;
    }
  }
  return false;
}

}  // namespace

MPCallbackList::MPCallbackList(const std::vector<MPCallback*>& callbacks)
    : MPCallback(CallbacksMightAddCuts(callbacks),
                 CallbacksMightAddLazyConstraints(callbacks)),
      callbacks_(callbacks) {}

void MPCallbackList::RunCallback(MPCallbackContext* context) {
  for (MPCallback* callback : callbacks_) {
    callback->RunCallback(context);
  }
}

}  // namespace operations_research
