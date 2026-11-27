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

#ifndef ORTOOLS_UTIL_DETERMINISTIC_TIME_H_
#define ORTOOLS_UTIL_DETERMINISTIC_TIME_H_

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

namespace operations_research {

#ifndef OR_TOOLS_UTIL_DETERMINISTIC_TIME_PROFILING

// The calibration parameters of a deterministic timer.
struct LinearDeterministicTimeSpec {
  double scale;
};

struct DeterministicTimeSpec {
  double scale;
  double offset;
};

struct DeterministicTimeSpec2 {
  double scale1;
  double scale2;
  double offset;
};

// A convenience template for advancing a deterministic time counter.
//
// This timer can be used in instance fields and/or passed to functions, unlike
// the ones below. On the other hand, it does not provide automatic support for
// nested timers.
template <LinearDeterministicTimeSpec spec>
class LinearDeterministicTimer {
 public:
  void Reset() {
#ifndef NDEBUG
    CHECK(!started_);
#endif  // NDEBUG
    time_units_ = 0;
  }

  // Starts or restarts the timer. This has no effect if profiling is disabled,
  // but must be called anyway so that, when profiling is enabled, wall time
  // samples and their corresponding deterministic time can be collected for
  // fitting.
  void Start() {
#ifndef NDEBUG
    CHECK(!started_);
    started_ = true;
#endif  // NDEBUG
  }

  // Advances the deterministic time by time_units * spec.scale. If profiling is
  // enabled, this also records the elapsed wall time since the last call to
  // Start() and the corresponding time units.
  void Stop(uint64_t time_units) {
#ifndef NDEBUG
    CHECK(started_);
    started_ = false;
#endif  // NDEBUG
    time_units_ += time_units;
  }

  // Returns the total elapsed deterministic time in seconds between the Start()
  // and Stop() calls since the construction of the timer, or since the last
  // call to Reset().
  double GetDeterministicTime() const { return spec.scale * time_units_; }

 private:
#ifndef NDEBUG
  bool started_ = false;
#endif  // NDEBUG
  uint64_t time_units_ = 0;
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

  // Returns the deterministic time in seconds for the given number of time
  // units.
  double dtime(uint64_t time_units) const {
    return spec.scale * time_units + spec.offset;
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

  // Returns the deterministic time in seconds for the given number of time
  // units.
  double dtime(uint64_t time_units1, uint64_t time_units2) const {
    return spec.scale1 * time_units1 + spec.scale2 * time_units2 + spec.offset;
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
struct LinearDeterministicTimeSpec {
  double scale;
  CompileTimeSourceLocation loc =
      CompileTimeSourceLocation(absl::SourceLocation::current());
};

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
  absl::uint128 y_times_y_sum = 0;

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
    y_times_y_sum += static_cast<absl::uint128>(y) * y;
  }

  void AddSamples(const SampleStatistics& other) {
    num_samples += other.num_samples;
    x_sum += other.x_sum;
    y_sum += other.y_sum;
    x_times_x_sum += other.x_times_x_sum;
    x_times_y_sum += other.x_times_y_sum;
    y_fit_sum += other.y_fit_sum;
    y_times_y_sum += other.y_times_y_sum;
  }

  std::string ToString() const {
    return absl::StrFormat("%d,%d,%d,%d,%d,%e,%d", num_samples, x_sum, y_sum,
                           x_times_x_sum, x_times_y_sum, y_fit_sum,
                           y_times_y_sum);
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
  absl::uint128 y_times_y_sum = 0;

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
    y_times_y_sum += static_cast<absl::uint128>(y) * y;
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
    y_times_y_sum += other.y_times_y_sum;
  }

  std::string ToString() const {
    return absl::StrFormat("%d,%d,%d,%d,%d,%d,%d,%d,%d,%e,%d", num_samples,
                           x1_sum, x2_sum, y_sum, x1_times_x1_sum,
                           x1_times_x2_sum, x2_times_x2_sum, x1_times_y_sum,
                           x2_times_y_sum, y_fit_sum, y_times_y_sum);
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

  // Returns the statistics collected so far (from exited threads only) for the
  // single scale timer at the given source location. For tests.
  SampleStatistics GetStatsForTesting(std::string_view file, int line) {
    absl::MutexLock lock(mutex_);
    const auto it = all_stats_.find({file, line});
    return it == all_stats_.end() ? SampleStatistics() : it->second;
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
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    AllDeterministicTimeStats::Get().AddProfilingStats(call_stack_to_stats_);
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
  }

  // Adds a sample to the statistics.
  // - time_units: the number of time units that were advanced by the timer.
  // - measured_duration_ns: the corresponding actual duration in nanoseconds.
  // - fitted_duration: the duration predicted by the current linear regression.
  void AddSample(uint64_t time_units, uint64_t measured_duration_ns,
                 double fitted_duration,
                 std::vector<void*>* call_stack = nullptr) {
    stats_.AddSample(time_units, measured_duration_ns, fitted_duration);
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    call_stack_to_stats_[*call_stack].AddSample(fitted_duration);
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
  }

 protected:
  const CompileTimeSourceLocation& loc_;
  SampleStatistics stats_;
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
  absl::flat_hash_map<std::vector<void*>, profiling::ProfilingSamples>
      call_stack_to_stats_;
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
};

template <CompileTimeSourceLocation loc>
class DeterministicTimeStats : public BaseDeterministicTimeStats {
 public:
  DeterministicTimeStats() : BaseDeterministicTimeStats(loc) {}
};

// An abstract deterministic timer, corresponding to a single source location
// (defined in subclasses), and a single thread.
//
// The profiling bookkeeping of a timer (clock reads, timer stack updates,
// statistics updates) does not exist in non-profiling builds, and must thus be
// excluded from the measured durations. For this, each timer measures both its
// own duration, from the end of its constructor to the start of its stats
// update, and its "footprint", from the start of its constructor to the end of
// its stats update. The footprint of a timer (plus one clock read) is
// subtracted from the duration of its parent timer, and the cost of one clock
// read is subtracted from its own duration.
class AbstractDeterministicTimer {
 public:
  explicit AbstractDeterministicTimer(TimeLimit* time_limit)
      : footprint_start_time_(absl::GetCurrentTimeNanos()),
        time_limit_(time_limit) {
    timers_stack_.push(this);
    // Must be last, to exclude the above bookkeeping from this timer's own
    // duration.
    start_time_ = absl::GetCurrentTimeNanos();
  }

  void SubtractNestedTimerDuration(uint64_t duration) {
    // Deduct the time spent in a nested timer from the time spent in this one
    // by adding its duration to the start time.
    start_time_ += duration;
  }

  // Deducts the given duration from the innermost active timer of the current
  // thread, if any. This can be used to exclude profiling bookkeeping done
  // outside of AbstractDeterministicTimer subclasses.
  static void SubtractFromInnermostTimer(uint64_t duration) {
    if (!timers_stack_.empty()) {
      timers_stack_.top()->SubtractNestedTimerDuration(duration);
    }
  }

  // Returns the estimated duration in nanoseconds of one call to
  // absl::GetCurrentTimeNanos(), computed once per thread.
  static uint64_t ClockReadDuration();

 protected:
  // Returns the own duration of this timer, i.e., the time elapsed since the
  // end of its construction, minus the duration of its nested timers and of one
  // clock read. Must be called at the start of AddStatsSample().
  uint64_t GetOwnElapsedTime() const {
    const uint64_t end_time = absl::GetCurrentTimeNanos();
    // Signed, because the subtracted estimates of the nested timers' overhead
    // can make start_time_ slightly larger than end_time.
    const int64_t elapsed_time = static_cast<int64_t>(end_time - start_time_) -
                                 static_cast<int64_t>(ClockReadDuration());
    return elapsed_time > 0 ? static_cast<uint64_t>(elapsed_time) : 0;
  }

  // Removes this timer from the timers stack, and deducts its whole footprint
  // from its parent timer, if any. Must be called at the end of
  // AddStatsSample(). The footprint is measured between the timestamps of the
  // first and last clock reads, which misses about half of each of these two
  // reads, i.e., one clock read in total, which is thus added.
  void PopAndSubtractFootprintFromParent() {
    timers_stack_.pop();
    SubtractFromInnermostTimer(absl::GetCurrentTimeNanos() -
                               footprint_start_time_ + ClockReadDuration());
  }

  // The time in nanoseconds at the start of the construction of this timer.
  // Must be declared first, to be initialized first.
  const uint64_t footprint_start_time_;
  // The time limit to use for advancing the deterministic time.
  TimeLimit* const time_limit_;
  // The time in nanoseconds at the end of the construction of this timer, plus
  // the duration of the nested timers.
  uint64_t start_time_;
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
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
  // Measures the own duration of this timer (see AbstractDeterministicTimer),
  // and adds it to the given statistics (with the corresponding number of time
  // units that were advanced by the timer).
  void AddStatsSample(double fitted_elapsed_time,
                      BaseDeterministicTimeStats& stats) {
    const uint64_t elapsed_time = GetOwnElapsedTime();
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    stats.AddSample(time_units_, elapsed_time, fitted_elapsed_time,
                    &call_stack_);
#else
    stats.AddSample(time_units_, elapsed_time, fitted_elapsed_time);
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    PopAndSubtractFootprintFromParent();
  }

  // The number of time units that were advanced by the timer.
  uint64_t time_units_ = 0;
};

template <LinearDeterministicTimeSpec spec>
class LinearDeterministicTimer {
 public:
  void Reset() { time_units_ = 0; }

  void Start() { start_time_ = absl::GetCurrentTimeNanos(); }

  void Stop(uint64_t time_units) {
    const uint64_t stop_time = absl::GetCurrentTimeNanos();
    const uint64_t elapsed_time = stop_time - start_time_;
    const uint64_t clock_read_duration =
        AbstractDeterministicTimer::ClockReadDuration();
    time_units_ += time_units;
    stats_.AddSample(time_units,
                     elapsed_time > clock_read_duration
                         ? elapsed_time - clock_read_duration
                         : 0,
                     spec.scale * time_units);
    // Exclude the above bookkeeping from the enclosing timer, if any: the time
    // since `stop_time`, plus about half of the `stop_time` clock read and half
    // of the last one, plus the whole clock read in Start().
    AbstractDeterministicTimer::SubtractFromInnermostTimer(
        absl::GetCurrentTimeNanos() - stop_time + 2 * clock_read_duration);
  }

  double GetDeterministicTime() const { return spec.scale * time_units_; }

 private:
  // The time in nanoseconds when the timer was started for the last time.
  uint64_t start_time_;
  uint64_t time_units_ = 0;
  static thread_local DeterministicTimeStats<spec.loc> stats_;
};

template <LinearDeterministicTimeSpec spec>
thread_local DeterministicTimeStats<spec.loc>
    LinearDeterministicTimer<spec>::stats_;

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

  // Returns the deterministic time in seconds for the given number of time
  // units.
  double dtime(uint64_t time_units) const {
    return spec.scale * time_units + spec.offset;
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
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    AllDeterministicTimeStats::Get().AddProfilingStats(call_stack_to_stats_);
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
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
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    call_stack_to_stats_[*call_stack].AddSample(fitted_duration);
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
  }

 protected:
  const CompileTimeSourceLocation& loc_;
  SampleStatistics2 stats_;
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
  absl::flat_hash_map<std::vector<void*>, profiling::ProfilingSamples>
      call_stack_to_stats_;
#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
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
  // Measures the own duration of this timer (see AbstractDeterministicTimer),
  // and adds it to the given statistics (with the corresponding number of time
  // units that were advanced by the timer).
  void AddStatsSample(double fitted_elapsed_time,
                      BaseDeterministicTimeStats2& stats) {
    const uint64_t elapsed_time = GetOwnElapsedTime();
#ifdef OR_TOOLS_UTIL_DETERMINISTIC_TIME_SAMPLING
    stats.AddSample(time_units1_, time_units2_, elapsed_time,
                    fitted_elapsed_time, &call_stack_);
#else
    stats.AddSample(time_units1_, time_units2_, elapsed_time,
                    fitted_elapsed_time);
#endif
    PopAndSubtractFootprintFromParent();
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

  // Returns the deterministic time in seconds for the given number of time
  // units.
  double dtime(uint64_t time_units1, uint64_t time_units2) const {
    return spec.scale1 * time_units1 + spec.scale2 * time_units2 + spec.offset;
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

#endif  // OR_TOOLS_UTIL_DETERMINISTIC_TIME_PROFILING

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_DETERMINISTIC_TIME_H_
