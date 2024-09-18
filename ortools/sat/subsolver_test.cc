// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/subsolver.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace sat {
namespace {

// Just a trivial example showing how to use the DeterministicLoop() and
// NonDeterministicLoop() functions.
template <bool deterministic>
void TestLoopFunction() {
  struct GlobalState {
    int num_task = 0;
    const int limit = 100;

    absl::Mutex mutex;
    std::vector<int64_t> updates;

    // This one will be always the same after each batch of task.
    int64_t max_update_value = 0;
  };

  class TestSubSolver : public SubSolver {
   public:
    explicit TestSubSolver(GlobalState* state)
        : SubSolver("test", FULL_PROBLEM), state_(state) {}

    bool TaskIsAvailable() override {
      // Note that the lock is only needed for the non-deterministic test.
      absl::MutexLock mutex_lock(&state_->mutex);
      return state_->num_task < state_->limit;
    }

    std::function<void()> GenerateTask(int64_t id) override {
      {
        // Note that the lock is only needed for the non-deterministic test.
        absl::MutexLock mutex_lock(&state_->mutex);
        state_->num_task++;
      }
      return [this, id] {
        absl::MutexLock mutex_lock(&state_->mutex);
        state_->updates.push_back(id);
      };
    }

    void Synchronize() override {
      // Note that the lock is only needed for the non-deterministic test.
      absl::MutexLock mutex_lock(&state_->mutex);
      for (const int64_t i : state_->updates) {
        state_->max_update_value = std::max(state_->max_update_value, i);
      }
      state_->updates.clear();
    }

   private:
    GlobalState* state_;
  };

  GlobalState state;

  // The number of subsolver can be independent of the number of threads. Here
  // there is actually no need to have 3 of them except for testing the feature.
  std::vector<std::unique_ptr<SubSolver>> subsolvers;
  for (int i = 0; i < 3; ++i) {
    subsolvers.push_back(std::make_unique<TestSubSolver>(&state));
  }

  const int num_threads = 4;
  if (deterministic) {
    const int batch_size = 20;
    DeterministicLoop(subsolvers, num_threads, batch_size);
  } else {
    NonDeterministicLoop(subsolvers, num_threads);
  }
  EXPECT_EQ(state.max_update_value, state.limit - 1);
}

TEST(DeterministicLoop, BasicTest) { TestLoopFunction<true>(); }

TEST(NonDeterministicLoop, BasicTest) { TestLoopFunction<false>(); }

}  // namespace
}  // namespace sat
}  // namespace operations_research
