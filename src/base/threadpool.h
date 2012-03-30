// Copyright 2010-2012 Google
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

#ifndef OR_TOOLS_BASE_THREADPOOL_H_
#define OR_TOOLS_BASE_THREADPOOL_H__

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/synchronization.h"

namespace operations_research {
class ThreadPool {
 public:
  explicit ThreadPool(int num_threads);
  ~ThreadPool();

  void StartWorkers();
  void Add(Closure* closure);

};
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_THREADPOOL_H__
