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
// While !stop_function(), we call a batch of num_threads
// generate_and_solve_function() in parallel. Each such functions should return
// an update_global_solution() function. These update functions will only be
// called sequentially (and in seed order) once the batch is done.
//
// The seed starts at zero and will be increased one by one. Each
// generate_and_solve_function() will get a different seed.
//
// Only the generate_and_solve_function(int seed) need to be thread-safe.
//
// The two templated types should behave like:
// - StopFunction: std::function<bool()>
// - SolveNeighborhoodFunction: std::function<std::function<void()>(int seed)>
//
// TODO(user): As the improving solution become more difficult to find, we
// should trigger a lot more parallel LNS than the number of threads, so that we
// have a better utilization. For this, add a simple mecanism so that we always
// have num_threads working LNS, and do the barrier sync at the end when all the
// LNS task have been executed.
template <class StopFunction, class SolveNeighborhoodFunction>
void OptimizeWithLNS(int num_threads, StopFunction stop_function,
                     SolveNeighborhoodFunction generate_and_solve_function);

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

template <class StopFunction, class SolveNeighborhoodFunction>
inline void OptimizeWithLNS(
    int num_threads, StopFunction stop_function,
    SolveNeighborhoodFunction generate_and_solve_function) {
  int64_t seed = 0;
#if !defined(__PORTABLE_PLATFORM__)
  while (!stop_function()) {
    if (num_threads > 1) {
      std::vector<std::function<void()>> update_functions(num_threads);
      {
        ThreadPool pool("Parallel_LNS", num_threads);
        pool.StartWorkers();
        for (int i = 0; i < num_threads; ++i) {
          pool.Schedule(
              [&update_functions, &generate_and_solve_function, i, seed]() {
                update_functions[i] = generate_and_solve_function(seed + i);
              });
        }
      }

      seed += num_threads;
      for (int i = 0; i < num_threads; ++i) {
        update_functions[i]();
      }
    } else {
      std::function<void()> update_function =
          generate_and_solve_function(seed++);
      update_function();
    }
  }
#else   // __PORTABLE_PLATFORM__
  while (!stop_function()) {
    std::function<void()> update_function = generate_and_solve_function(seed++);
    update_function();
  }
#endif  // __PORTABLE_PLATFORM__
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LNS_H_
