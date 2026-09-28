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

#ifndef ORTOOLS_SAT_DETERMINISTIC_TIME_H_
#define ORTOOLS_SAT_DETERMINISTIC_TIME_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/source_location.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

#ifndef OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

// The calibration parameters of a deterministic timer.
struct DeterministicTimeSpec {
  double scale;
  double offset;
};

struct DeterministicTimeSpec2 {
  double scale1;
  double scale2;
  double offset;
};

// A convenience template for advancing the deterministic time of a TimeLimit.
//
// DeterministicTimers should be declared at the beginning of a syntactic scope,
// and should not be passed as parameters to functions, or stored in static or
// class variables.
template <DeterministicTimeSpec spec>
class DeterministicTimer {
 public:
  // Advances the time limit's deterministic time by spec.offset.
  explicit DeterministicTimer(TimeLimit* time_limit) : time_limit_(time_limit) {
    DCHECK(time_limit != nullptr);
    if constexpr (spec.offset > 0.0) {
      time_limit_->AdvanceDeterministicTime(spec.offset);
    }
  }

  // Advances the time limit's deterministic time by spec.scale * time_units +
  // spec.offset.
  DeterministicTimer(TimeLimit* time_limit, uint64_t time_units)
      : time_limit_(time_limit) {
    time_limit_->AdvanceDeterministicTime(spec.scale * time_units +
                                          spec.offset);
  }

  // Advances the time limit's deterministic time by spec.scale * time_units.
  void Advance(uint64_t time_units) {
    time_limit_->AdvanceDeterministicTime(spec.scale * time_units);
  }

 private:
  TimeLimit* const time_limit_;
};

// A convenience template for advancing the deterministic time of a TimeLimit.
//
// DeterministicTimers should be declared at the beginning of a syntactic scope,
// and should not be passed as parameters to functions, or stored in static or
// class variables.
template <DeterministicTimeSpec2 spec>
class DeterministicTimer2 {
 public:
  // Advances the time limit's deterministic time by spec.offset.
  explicit DeterministicTimer2(TimeLimit* time_limit)
      : time_limit_(time_limit) {
    DCHECK(time_limit != nullptr);
    if constexpr (spec.offset > 0.0) {
      time_limit_->AdvanceDeterministicTime(spec.offset);
    }
  }

  // Advances the time limit's deterministic time by spec.scale1 * time_units1 +
  // spec.scale2 * time_units2 + spec.offset.
  DeterministicTimer2(TimeLimit* time_limit, uint64_t time_units1,
                      uint64_t time_units2)
      : time_limit_(time_limit) {
    time_limit_->AdvanceDeterministicTime(
        spec.scale1 * time_units1 + spec.scale2 * time_units2 + spec.offset);
  }

  // Advances the time limit's deterministic time by spec.scale1 * time_units1 +
  // spec.scale2 * time_units2.
  void Advance(uint64_t time_units1, uint64_t time_units2) {
    time_limit_->AdvanceDeterministicTime(spec.scale1 * time_units1 +
                                          spec.scale2 * time_units2);
  }

 private:
  TimeLimit* const time_limit_;
};

#else

// An immutable compile-time source location.
// File names are truncated to 256 characters.
class CompileTimeSourceLocation {
 public:
  constexpr CompileTimeSourceLocation(const absl::SourceLocation& loc) {
    const char* value = loc.file_name();
    for (size_t i = 0; i < 256 && value[i] != 0; ++i) {
      buf_[i] = value[i];
      len_++;
    }
    line_ = loc.line();
  }
  constexpr std::string_view file_name() const { return {buf_, len_}; }
  constexpr int line() const { return line_; }

  // Must be public to be a structural type.
 public:
  char buf_[256]{};
  size_t len_ = 0;
  int line_;
};

// The calibration parameters and source location of a deterministic timer.
struct DeterministicTimeSpec {
  double scale;
  double offset;
  CompileTimeSourceLocation loc =
      CompileTimeSourceLocation(absl::SourceLocation::current());
};

// Statistics about a set of (x_i, y_i) samples allowing to compute scale and
// offset parameters for a linear model y = scale * x + offset that best fits
// these samples.
//
// We want to find the scale and offset values minimizing the squared error
//   E = sum(y_i - (scale.x_i + offset))^2
// where (x_i, y_i) are n samples.
//
// Developing this expression, and dividing by the number of samples, we get:
//   E = x^T P x + Q^T x
// where:
//   P = [[1,     x_avg],
//        [x_avg, x_times_x_avg]]
//   Q = [-2*y_avg, -2*x_times_y_avg]
//   x = [offset, scale]^T
//
// This can be solved with OSQP (osqp.org), subject to offset >= 0, scale >= 0.
struct SampleStatistics {
 public:
  uint64_t num_samples = 0;
  uint64_t x_sum = 0;
  uint64_t y_sum = 0;
  absl::uint128 x_times_x_sum = 0;
  absl::uint128 x_times_y_sum = 0;
  double y_fit_sum = 0.0;

  // y_fit is the value of the linear regression at x, using the current
  // linear regression parameters. It can be used to evaluate its deviation
  // from the actual y.
  void AddSample(uint64_t x, uint64_t y, double y_fit) {
    // TODO(user): stop accumulating samples as soon as adding one would
    // cause an overflow?
    num_samples++;
    x_sum += x;
    y_sum += y;
    x_times_x_sum += static_cast<absl::uint128>(x) * x;
    x_times_y_sum += static_cast<absl::uint128>(x) * y;
    y_fit_sum += y_fit;
  }

  void AddSamples(const SampleStatistics& other) {
    num_samples += other.num_samples;
    x_sum += other.x_sum;
    y_sum += other.y_sum;
    x_times_x_sum += other.x_times_x_sum;
    x_times_y_sum += other.x_times_y_sum;
    y_fit_sum += other.y_fit_sum;
  }

  std::string ToString() const {
    return absl::StrFormat("%d,%d,%d,%d,%d,%e", num_samples, x_sum, y_sum,
                           x_times_x_sum, x_times_y_sum, y_fit_sum);
  }
};

// The calibration parameters and source location of a deterministic timer.
struct DeterministicTimeSpec2 {
  double scale1;
  double scale2;
  double offset;
  CompileTimeSourceLocation loc =
      CompileTimeSourceLocation(absl::SourceLocation::current());
};

// Statistics about a set of (x1_i, x2_i, y_i) samples allowing to compute scale
// and offset parameters for a linear model y = scale1 * x1 + scale2 * x2 +
// offset that best fits these samples.
//
// We want to find the scale and offset values minimizing the squared error
//   E = sum(y_i - (scale1.x1_i + scale2.x2_i + offset))^2
// where (x1_i, x2_i, y_i) are n samples.
//
// Developing this expression, and dividing by the number of samples, we get:
//   E = x^T P x + Q^T x
// where:
//   P = [[1,      x1_avg,          x2_avg],
//        [x1_avg, x1_times_x1_avg, x1_times_x2_avg],
//        [x2_avg, x1_times_x2_avg, x2_times_x2_avg]]
//   Q = [-2*y_avg, -2*x1_times_y_avg, -2*x2_times_y_avg]
//   x = [offset, scale1, scale2]^T
//
// This can be solved with OSQP (osqp.org), subject to
//   offset >= 0, scale1 >= 0, scale2 >= 0.
struct SampleStatistics2 {
 public:
  uint64_t num_samples = 0;
  uint64_t x1_sum = 0;
  uint64_t x2_sum = 0;
  uint64_t y_sum = 0;
  absl::uint128 x1_times_x1_sum = 0;
  absl::uint128 x1_times_x2_sum = 0;
  absl::uint128 x2_times_x2_sum = 0;
  absl::uint128 x1_times_y_sum = 0;
  absl::uint128 x2_times_y_sum = 0;
  double y_fit_sum = 0.0;

  // y_fit is the value of the linear regression at x, using the current
  // linear regression parameters. It can be used to evaluate its deviation
  // from the actual y.
  void AddSample(uint64_t x1, uint64_t x2, uint64_t y, double y_fit) {
    // TODO(user): stop accumulating samples as soon as adding one would
    // cause an overflow?
    num_samples++;
    x1_sum += x1;
    x2_sum += x2;
    y_sum += y;
    x1_times_x1_sum += static_cast<absl::uint128>(x1) * x1;
    x1_times_x2_sum += static_cast<absl::uint128>(x1) * x2;
    x2_times_x2_sum += static_cast<absl::uint128>(x2) * x2;
    x1_times_y_sum += static_cast<absl::uint128>(x1) * y;
    x2_times_y_sum += static_cast<absl::uint128>(x2) * y;
    y_fit_sum += y_fit;
  }

  void AddSamples(const SampleStatistics2& other) {
    num_samples += other.num_samples;
    x1_sum += other.x1_sum;
    x2_sum += other.x2_sum;
    y_sum += other.y_sum;
    x1_times_x1_sum += other.x1_times_x1_sum;
    x1_times_x2_sum += other.x1_times_x2_sum;
    x2_times_x2_sum += other.x2_times_x2_sum;
    x1_times_y_sum += other.x1_times_y_sum;
    x2_times_y_sum += other.x2_times_y_sum;
    y_fit_sum += other.y_fit_sum;
  }

  std::string ToString() const {
    return absl::StrFormat("%d,%d,%d,%d,%d,%d,%d,%d,%d,%e", num_samples, x1_sum,
                           x2_sum, y_sum, x1_times_x1_sum, x1_times_x2_sum,
                           x2_times_x2_sum, x1_times_y_sum, x2_times_y_sum,
                           y_fit_sum);
  }
};

namespace profiling {

std::vector<void*> GetTimerStackTrace();

struct ProfilingSamples {
  uint64_t count = 0;
  double total_dtime = 0;

  void AddSample(double dtime) {
    count++;
    total_dtime += dtime;
  }

  void AddSamples(const ProfilingSamples& other) {
    count += other.count;
    total_dtime += other.total_dtime;
  }
};

}  // namespace profiling

// A registry of all the deterministic timer statistics, over all threads. It is
// updated each time a thread exits, and logs all the statistics when the
// program exits.
class AllDeterministicTimeStats {
 public:
  ~AllDeterministicTimeStats();

  static AllDeterministicTimeStats& Get() {
    static auto instance = std::unique_ptr<AllDeterministicTimeStats>(
        new AllDeterministicTimeStats());
    return *instance;
  }

  // Adds the given statistics to the registry.
  //
  // - file, line: the source location of the deterministic timer.
  // - stats: statistics of its (time units, measured duration) samples.
  void AddStats(std::string_view file, int line,
                const SampleStatistics& stats) {
    absl::MutexLock lock(mutex_);
    all_stats_[{file, line}].AddSamples(stats);
  }

  void AddStats2(std::string_view file, int line,
                 const SampleStatistics2& stats) {
    absl::MutexLock lock(mutex_);
    all_stats2_[{file, line}].AddSamples(stats);
  }

  void AddProfilingStats(
      const absl::flat_hash_map<std::vector<void*>,
                                profiling::ProfilingSamples>& stats) {
    absl::MutexLock lock(mutex_);
    for (const auto& [stack, samples] : stats) {
      all_profiling_stats_[stack].AddSamples(samples);
    }
  }

 private:
  AllDeterministicTimeStats() = default;

  absl::Time start_time_ = absl::Now();
  absl::Mutex mutex_;
  absl::flat_hash_map<std::pair<std::string_view, int>, SampleStatistics>
      all_stats_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::pair<std::string_view, int>, SampleStatistics2>
      all_stats2_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::vector<void*>, profiling::ProfilingSamples>
      all_profiling_stats_ ABSL_GUARDED_BY(mutex_);
};

// Statistics from a single deterministic timer (identified by an ID
// corresponding to a specific source location), and a single thread.
class BaseDeterministicTimeStats {
 public:
  explicit BaseDeterministicTimeStats(const CompileTimeSourceLocation& loc)
      : loc_(loc) {}

  // This is called when the thread exits (stats are stored in thread_local
  // variables, one per source location).
  ~BaseDeterministicTimeStats() {
    AllDeterministicTimeStats::Get().AddStats(loc_.file_name(), loc_.line(),
                                              stats_);
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    AllDeterministicTimeStats::Get().AddProfilingStats(call_stack_to_stats_);
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  }

  // Adds a sample to the statistics.
  // - time_units: the number of time units that were advanced by the timer.
  // - measured_duration_ns: the corresponding actual duration in nanoseconds.
  // - fitted_duration: the duration predicted by the current linear regression.
  void AddSample(uint64_t time_units, uint64_t measured_duration_ns,
                 double fitted_duration,
                 std::vector<void*>* call_stack = nullptr) {
    stats_.AddSample(time_units, measured_duration_ns, fitted_duration);
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    call_stack_to_stats_[*call_stack].AddSample(fitted_duration);
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  }

 protected:
  const CompileTimeSourceLocation loc_;
  SampleStatistics stats_;
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  absl::flat_hash_map<std::vector<void*>, profiling::ProfilingSamples>
      call_stack_to_stats_;
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
};

template <CompileTimeSourceLocation loc>
class DeterministicTimeStats : public BaseDeterministicTimeStats {
 public:
  DeterministicTimeStats() : BaseDeterministicTimeStats(loc) {}
};

// An abstract deterministic timer, corresponding to a single source location
// (defined in subclasses), and a single thread.
class AbstractDeterministicTimer {
 public:
  explicit AbstractDeterministicTimer(TimeLimit* time_limit)
      : time_limit_(time_limit), start_time_(absl::GetCurrentTimeNanos()) {
    timers_stack_.push(this);
  }

  void SubtractNestedTimerDuration(uint64_t duration) {
    // Deduct the time spent in a nested timer from the time spent in this one
    // by adding its duration to the start time.
    start_time_ += duration;
  }

 protected:
  // The time limit to use for advancing the deterministic time.
  TimeLimit* const time_limit_;
  // The time in nanoseconds when the timer was constructed.
  uint64_t start_time_;
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  std::vector<void*> call_stack_ = profiling::GetTimerStackTrace();
#endif

  // The stack of currently active timers in the current thread. This is used to
  // deduct the time spent in nested timers (we assume that timers are destroyed
  // in the reverse order of their construction).
  static thread_local std::stack<AbstractDeterministicTimer*> timers_stack_;
};

// A deterministic timer with a single scale, corresponding to an abstract
// source location (defined in subclasses), and a single thread.
class BaseDeterministicTimer : public AbstractDeterministicTimer {
 public:
  explicit BaseDeterministicTimer(TimeLimit* time_limit)
      : AbstractDeterministicTimer(time_limit) {}

 protected:
  // Measures the elapsed time between the construction of this timer and this
  // method call, and adds it to the given statistics (with the corresponding
  // number of time units that were advanced by the timer).
  void AddStatsSample(double fitted_elapsed_time,
                      BaseDeterministicTimeStats& stats) {
    const uint64_t end_time = absl::GetCurrentTimeNanos();
    const uint64_t elapsed_time = end_time - start_time_;
    timers_stack_.pop();
    if (!timers_stack_.empty()) {
      // Deduct the time spent in this timer from the time spent in its parent.
      timers_stack_.top()->SubtractNestedTimerDuration(elapsed_time);
    }
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    stats.AddSample(time_units_, elapsed_time, fitted_elapsed_time,
                    &call_stack_);
#else
    stats.AddSample(time_units_, elapsed_time, fitted_elapsed_time);
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  }

  // The number of time units that were advanced by the timer.
  uint64_t time_units_ = 0;
};

// A deterministic timer which collects statistics about the actual time
// corresponding to the advanced time units. This allows updating its scale and
// offset parameters with a linear regression.
//
// DeterministicTimers should be declared at the beginning of a syntactic scope,
// and should not be passed as parameters to functions, or stored in static or
// class variables. This ensures that the timers are destroyed in the reverse
// order of their construction. This property is used to deduct the time spent
// in nested timers, which is necessary to obtain accurate scale and offset
// parameters.
template <DeterministicTimeSpec spec>
class DeterministicTimer : public BaseDeterministicTimer {
 public:
  // Advances the time limit's deterministic time by the given offset.
  explicit DeterministicTimer(TimeLimit* time_limit)
      : BaseDeterministicTimer(time_limit) {
    DCHECK(time_limit != nullptr);
    if constexpr (spec.offset > 0.0) {
      time_limit_->AdvanceDeterministicTime(spec.offset);
    }
  }

  // Advances the time limit's deterministic time by spec.scale * time_units +
  // spec.offset.
  DeterministicTimer(TimeLimit* time_limit, uint64_t time_units)
      : BaseDeterministicTimer(time_limit) {
    DCHECK(time_limit != nullptr);
    time_limit_->AdvanceDeterministicTime(spec.scale * time_units +
                                          spec.offset);
    time_units_ += time_units;
  }

  ~DeterministicTimer() {
    AddStatsSample(spec.offset + spec.scale * time_units_, stats_);
  }

  // Advances the time limit's deterministic time by spec.scale * time_units.
  void Advance(uint64_t time_units) {
    time_limit_->AdvanceDeterministicTime(spec.scale * time_units);
    time_units_ += time_units;
  }

 private:
  static thread_local DeterministicTimeStats<spec.loc> stats_;
};

template <DeterministicTimeSpec spec>
thread_local DeterministicTimeStats<spec.loc> DeterministicTimer<spec>::stats_;

// Statistics from a single deterministic timer (identified by an ID
// corresponding to a specific source location), and a single thread.
class BaseDeterministicTimeStats2 {
 public:
  explicit BaseDeterministicTimeStats2(const CompileTimeSourceLocation& loc)
      : loc_(loc) {}

  // This is called when the thread exits (stats are stored in thread_local
  // variables, one per source location).
  ~BaseDeterministicTimeStats2() {
    AllDeterministicTimeStats::Get().AddStats2(loc_.file_name(), loc_.line(),
                                               stats_);
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    AllDeterministicTimeStats::Get().AddProfilingStats(call_stack_to_stats_);
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  }

  // Adds a sample to the statistics.
  // - time_units: the number of time units that were advanced by the timer.
  // - measured_duration_ns: the corresponding actual duration in nanoseconds.
  // - fitted_duration: the duration predicted by the current linear regression.
  void AddSample(uint64_t time_units1, uint64_t time_units2,
                 uint64_t measured_duration_ns, double fitted_duration,
                 std::vector<void*>* call_stack = nullptr) {
    stats_.AddSample(time_units1, time_units2, measured_duration_ns,
                     fitted_duration);
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    call_stack_to_stats_[*call_stack].AddSample(fitted_duration);
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  }

 protected:
  const CompileTimeSourceLocation loc_;
  SampleStatistics2 stats_;
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
  absl::flat_hash_map<std::vector<void*>, profiling::ProfilingSamples>
      call_stack_to_stats_;
#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
};

template <CompileTimeSourceLocation loc>
class DeterministicTimeStats2 : public BaseDeterministicTimeStats2 {
 public:
  DeterministicTimeStats2() : BaseDeterministicTimeStats2(loc) {}
};

// A deterministic timer with two scales, corresponding to an abstract source
// location (defined in subclasses), and a single thread.
class BaseDeterministicTimer2 : public AbstractDeterministicTimer {
 public:
  explicit BaseDeterministicTimer2(TimeLimit* time_limit)
      : AbstractDeterministicTimer(time_limit) {}

 protected:
  // Measures the elapsed time between the construction of this timer and this
  // method call, and adds it to the given statistics (with the corresponding
  // number of time units that were advanced by the timer).
  void AddStatsSample(double fitted_elapsed_time,
                      BaseDeterministicTimeStats2& stats) {
    const uint64_t end_time = absl::GetCurrentTimeNanos();
    const uint64_t elapsed_time = end_time - start_time_;
    timers_stack_.pop();
    if (!timers_stack_.empty()) {
      // Deduct the time spent in this timer from the time spent in its parent.
      timers_stack_.top()->SubtractNestedTimerDuration(elapsed_time);
    }
#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_SAMPLING
    stats.AddSample(time_units1_, time_units2_, elapsed_time,
                    fitted_elapsed_time, &call_stack_);
#else
    stats.AddSample(time_units1_, time_units2_, elapsed_time,
                    fitted_elapsed_time);
#endif
  }

  // The number of time units that were advanced by the timer.
  uint64_t time_units1_ = 0;
  uint64_t time_units2_ = 0;
};

// A deterministic timer which collects statistics about the actual time
// corresponding to the advanced time units. This allows updating its scale and
// offset parameters with a linear regression.
//
// DeterministicTimers should be declared at the beginning of a syntactic scope,
// and should not be passed as parameters to functions, or stored in static or
// class variables. This ensures that the timers are destroyed in the reverse
// order of their construction. This property is used to deduct the time spent
// in nested timers, which is necessary to obtain accurate scale and offset
// parameters.
template <DeterministicTimeSpec2 spec>
class DeterministicTimer2 : public BaseDeterministicTimer2 {
 public:
  // Advances the time limit's deterministic time by the given offset.
  explicit DeterministicTimer2(TimeLimit* time_limit)
      : BaseDeterministicTimer2(time_limit) {
    DCHECK(time_limit != nullptr);
    if constexpr (spec.offset > 0.0) {
      time_limit_->AdvanceDeterministicTime(spec.offset);
    }
  }

  // Advances the time limit's deterministic time by spec.scale1 * time_units1 +
  // spec.scale2 * time_units2 + spec.offset.
  DeterministicTimer2(TimeLimit* time_limit, uint64_t time_units1,
                      uint64_t time_units2)
      : BaseDeterministicTimer2(time_limit) {
    DCHECK(time_limit != nullptr);
    time_limit_->AdvanceDeterministicTime(
        spec.scale1 * time_units1 + spec.scale2 * time_units2 + spec.offset);
    time_units1_ += time_units1;
    time_units2_ += time_units2;
  }

  ~DeterministicTimer2() {
    AddStatsSample(
        spec.scale1 * time_units1_ + spec.scale2 * time_units2_ + spec.offset,
        stats_);
  }

  // Advances the time limit's deterministic time by spec.scale1 * time_units1 +
  // spec.scale2 * time_units2.
  void Advance(uint64_t time_units1, uint64_t time_units2) {
    time_limit_->AdvanceDeterministicTime(spec.scale1 * time_units1 +
                                          spec.scale2 * time_units2);
    time_units1_ += time_units1;
    time_units2_ += time_units2;
  }

 private:
  static thread_local DeterministicTimeStats2<spec.loc> stats_;
};

template <DeterministicTimeSpec2 spec>
thread_local DeterministicTimeStats2<spec.loc>
    DeterministicTimer2<spec>::stats_;

#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_DETERMINISTIC_TIME_H_
