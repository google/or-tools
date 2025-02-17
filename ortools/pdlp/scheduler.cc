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

#include "ortools/pdlp/scheduler.h"

#include <memory>

#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

// Convenience factory function.
std::unique_ptr<Scheduler> MakeScheduler(SchedulerType type, int num_threads) {
  switch (type) {
    case SchedulerType::SCHEDULER_TYPE_GOOGLE_THREADPOOL:
      return std::make_unique<GoogleThreadPoolScheduler>(num_threads);
    case SchedulerType::SCHEDULER_TYPE_EIGEN_THREADPOOL:
      return std::make_unique<EigenThreadPoolScheduler>(num_threads);
    default:
      return nullptr;
  }
}

}  // namespace operations_research::pdlp
