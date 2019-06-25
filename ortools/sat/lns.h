// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_LNS_H_
#define OR_TOOLS_SAT_LNS_H_

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/threadpool.h"
#endif  // __PORTABLE_PLATFORM__

namespace operations_research {
namespace sat {

// A simple deterministic and multithreaded LNS design.
//
// While !synchronize_and_maybe_stop(), we call a batch of batch_size
// generate_and_solve() in parallel using num_threads threads. The
// two given functions must be thread-safe.
//
// The general idea to enforce determinism is that each
// generate_and_solve() can update a global state asynchronously, but
// should still use the past state until the synchronize_and_maybe_stop() call
// has been done.
//
// The seed starts at zero and will be increased one by one, so it also
// represent the number of calls to generate_and_solve(). Each
// generate_and_solve() will get a different seed.
//
// The two templated types should behave like:
// - StopFunction: std::function<bool()>
// - SubSolveFunction: std::function<void(int64 seed)>
template <class StopFunction, class SubSolveFunction>
void OptimizeWithLNS(int num_threads, int batch_size,
                     const StopFunction& synchronize_and_maybe_stop,
                     const SubSolveFunction& generate_and_solve);

// This one just keep num_threads tasks always in flight and call
// synchronize_and_maybe_stop() before each generate_and_solve(). It is not
// deterministic.
template <class StopFunction, class SubSolveFunction>
void NonDeterministicOptimizeWithLNS(
    int num_threads, const StopFunction& synchronize_and_maybe_stop,
    const SubSolveFunction& generate_and_solve);

// Basic adaptive [0.0, 1.0] parameter that can be increased or decreased with a
// step that get smaller and smaller with the number of updates.
//
// Note(user): The current logic work well in practice, but has no theoretical
// foundation. So it might be possible to use better formulas depending on the
// situation.
//
// TODO(user): In multithread, we get Increase()/Decrease() signal from
// different thread potentially working on different difficulty. The class need
// to be updated to properly handle this case. Increase()/Decrease() should take
// in the difficulty at which the signal was computed, and the update formula
// should be changed accordingly.
class AdaptiveParameterValue {
 public:
  // Initial value is in [0.0, 1.0], both 0.0 and 1.0 are valid.
  explicit AdaptiveParameterValue(double initial_value)
      : value_(initial_value) {}

  void Reset() { num_changes_ = 0; }

  void Increase() {
    const double factor = IncreaseNumChangesAndGetFactor();
    value_ = std::min(1.0 - (1.0 - value_) / factor, value_ * factor);
  }

  void Decrease() {
    const double factor = IncreaseNumChangesAndGetFactor();
    value_ = std::max(value_ / factor, 1.0 - (1.0 - value_) * factor);
  }

  double value() const { return value_; }

 private:
  // We want to change the parameters more and more slowly.
  double IncreaseNumChangesAndGetFactor() {
    ++num_changes_;
    return 1.0 + 1.0 / std::sqrt(num_changes_ + 1);
  }

  double value_;
  int64_t num_changes_ = 0;
};

// ============================================================================
// Implementation.
// ============================================================================

template <class StopFunction, class SubSolveFunction>
inline void OptimizeWithLNS(int num_threads, int batch_size,
                            const StopFunction& synchronize_and_maybe_stop,
                            const SubSolveFunction& generate_and_solve) {
  int64_t seed = 0;

#if defined(__PORTABLE_PLATFORM__)
  while (!synchronize_and_maybe_stop()) generate_and_solve(seed++);
  return;
#else   // __PORTABLE_PLATFORM__

  if (num_threads == 1) {
    while (!synchronize_and_maybe_stop()) generate_and_solve(seed++);
    return;
  }

  // TODO(user): We could reuse the same ThreadPool as long as we wait for all
  // the task in a batch to finish before scheduling new ones. Not sure how
  // to easily do that, so for now we just recreate the pool for each batch.
  while (!synchronize_and_maybe_stop()) {
    // We simply rely on the fact that a ThreadPool will only schedule
    // num_threads workers in parallel.
    ThreadPool pool("Deterministic_Parallel_LNS", num_threads);
    pool.StartWorkers();
    for (int i = 0; i < batch_size; ++i) {
      pool.Schedule(
          [&generate_and_solve, seed]() { generate_and_solve(seed); });
      ++seed;
    }
  }
#endif  // __PORTABLE_PLATFORM__
}

template <class StopFunction, class SubSolveFunction>
inline void NonDeterministicOptimizeWithLNS(
    int num_threads, const StopFunction& synchronize_and_maybe_stop,
    const SubSolveFunction& generate_and_solve) {
  int64_t seed = 0;

#if defined(__PORTABLE_PLATFORM__)
  while (!synchronize_and_maybe_stop()) {
    generate_and_solve(seed++);
  }
#else  // __PORTABLE_PLATFORM__

  if (num_threads == 1) {
    while (!synchronize_and_maybe_stop()) generate_and_solve(seed++);
    return;
  }

  // The lambda below are using little space, but there is no reason
  // to create millions of them, so we use the blocking nature of
  // pool.Schedule() when the queue capacity is set.
  ThreadPool pool("Parallel_LNS", num_threads);
  pool.SetQueueCapacity(10 * num_threads);
  pool.StartWorkers();
  while (!synchronize_and_maybe_stop()) {
    pool.Schedule([&generate_and_solve, seed]() { generate_and_solve(seed); });
    ++seed;
  }

#endif  // __PORTABLE_PLATFORM__
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LNS_H_
