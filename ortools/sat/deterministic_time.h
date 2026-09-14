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
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/source_location.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

#ifndef OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

// The calibration parameters of a deterministic timer.
struct DeterministicTimeSpec {
  double scale;
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
// Deriving E with respect to scale and offset, and setting to 0, gives:
// - sum(x_i^2).scale + sum(x_i).offset = sum(x_i.y_i)
// - sum(x_i).scale + n.offset = sum(y_i)
// Dividing by n yields:
// - avg(x_i^2).scale + avg(x_i).offset = avg(x_i.y_i)
// - avg(x_i).scale + offset = avg(y_i)
// And finally:
// - scale = [avg(x_i.y_i) - avg(y_i).avg(x_i)] / [avg(x_i^2) - avg(x_i)^2]
// - offset = avg(y_i) - avg(x_i).scale
// - E/n = avg(y_i^2) + scale^2.avg(x_i^2) + offset^2 +
//         2.(scale.offset.avg(x_i) - scale.avg(x_i.y_i) - offset.avg(y_i))
//
// By noting
// - xy_cov = avg(x_i.y_i) - avg(x_i).avg(y_i)
// - x_var = avg(x_i^2) - avg(x_i)^2
// - y_var = avg(y_i^2) - avg(y_i)^2
// we can rewrite the equations as:
// - scale = xy_cov / x_var
// - offset = avg(y_i) - avg(x_i).scale
// - E/n = y_var - xy_cov^2 / x_var
class SampleStatistics {
 public:
  SampleStatistics() = default;
  SampleStatistics(uint64_t num_samples, uint64_t x_sum, uint64_t y_sum,
                   absl::uint128 x_times_x_sum, absl::uint128 x_times_y_sum)
      : num_samples_(num_samples),
        x_sum_(x_sum),
        y_sum_(y_sum),
        x_times_x_sum_(x_times_x_sum),
        x_times_y_sum_(x_times_y_sum) {}

  void AddSample(uint64_t x, uint64_t y) {
    // TODO(user): stop accumulating samples as soon as adding one would
    // cause an overflow?
    num_samples_++;
    x_sum_ += x;
    y_sum_ += y;
    x_times_x_sum_ += x * x;
    x_times_y_sum_ += x * y;
    y_times_y_sum_ += y * y;
  }

  void AddSamples(const SampleStatistics& other) {
    num_samples_ += other.num_samples_;
    x_sum_ += other.x_sum_;
    y_sum_ += other.y_sum_;
    x_times_x_sum_ += other.x_times_x_sum_;
    x_times_y_sum_ += other.x_times_y_sum_;
    y_times_y_sum_ += other.y_times_y_sum_;
  }

  double FitParameters(double* scale, double* offset) const {
    if (num_samples_ == 0) {
      *scale = 0.0;
      *offset = 0.0;
      return 0.0;
    }
    double x_avg = static_cast<double>(x_sum_) / num_samples_;
    double y_avg = static_cast<double>(y_sum_) / num_samples_;
    double x_times_x_avg = static_cast<double>(x_times_x_sum_) / num_samples_;
    double y_times_y_avg = static_cast<double>(y_times_y_sum_) / num_samples_;
    double x_times_y_avg = static_cast<double>(x_times_y_sum_) / num_samples_;
    double xy_covariance = x_times_y_avg - x_avg * y_avg;
    double x_variance = x_times_x_avg - x_avg * x_avg;
    double y_variance = y_times_y_avg - y_avg * y_avg;
    *scale = x_variance == 0.0 ? 0.0 : xy_covariance / x_variance;
    *offset = y_avg - x_avg * (*scale);
    if (*scale < 0.0 || *offset < 0.0) {
      // Fit a linear model y = scale * x instead.
      *scale = x_times_y_avg / x_times_x_avg;
      *offset = 0.0;
    }
    double mse = 0.0;
    if (x_variance != 0.0) {
      mse = y_variance - xy_covariance * xy_covariance / x_variance;
    }
    return std::sqrt(std::max(0.0, mse));
  }

  std::string ToString() const {
    double scale, offset;
    double rmse = FitParameters(&scale, &offset);
    return absl::StrFormat("%d,%d,%d,%d,%d,%d,%e,%e,%e", num_samples_, x_sum_,
                           y_sum_, x_times_x_sum_, x_times_y_sum_,
                           y_times_y_sum_, scale, offset, rmse);
  }

 private:
  uint64_t num_samples_ = 0;
  uint64_t x_sum_ = 0;
  uint64_t y_sum_ = 0;
  absl::uint128 x_times_x_sum_ = 0;
  absl::uint128 x_times_y_sum_ = 0;
  absl::uint128 y_times_y_sum_ = 0;
};

// A registry of all the deterministic timer statistics, over all threads. It is
// updated each time a thread exits, and logs all the statistics when the
// program exits.
class AllDeterministicTimeStats {
 public:
  ~AllDeterministicTimeStats() {
    for (const auto& [source_location, stats] : all_stats_) {
      const auto& [file, line] = source_location;
      std::cout << "dtime stats: " << file << "," << line << ","
                << stats.ToString() << std::endl;
    }
  }

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

 private:
  AllDeterministicTimeStats() = default;

  absl::Mutex mutex_;
  absl::btree_map<std::pair<std::string_view, int>, SampleStatistics> all_stats_
      ABSL_GUARDED_BY(mutex_);
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
  }

  // Adds a sample to the statistics.
  // - time_units: the number of time units that were advanced by the timer.
  // - measured_duration_ns: the corresponding actual duration in nanoseconds.
  void AddSample(uint64_t time_units, uint64_t measured_duration_ns) {
    stats_.AddSample(time_units, measured_duration_ns);
  }

 protected:
  const CompileTimeSourceLocation loc_;
  SampleStatistics stats_;
};

template <CompileTimeSourceLocation loc>
class DeterministicTimeStats : public BaseDeterministicTimeStats {
 public:
  DeterministicTimeStats() : BaseDeterministicTimeStats(loc) {}
};

// An abstract deterministic timer, corresponding to a single source location
// (defined in subclasses), and a single thread.
class BaseDeterministicTimer {
 public:
  explicit BaseDeterministicTimer(TimeLimit* time_limit)
      : time_limit_(time_limit), start_time_(absl::GetCurrentTimeNanos()) {
    timers_stack_.push(this);
  }

 protected:
  // Measures the elapsed time between the construction of this timer and this
  // method call, and adds it to the given statistics (with the corresponding
  // number of time units that were advanced by the timer).
  void AddStatsSample(BaseDeterministicTimeStats& stats) {
    const uint64_t end_time = absl::GetCurrentTimeNanos();
    const uint64_t elapsed_time = end_time - start_time_;
    timers_stack_.pop();
    if (!timers_stack_.empty()) {
      // Deduct the time spent in this timer from the time spent in its parent
      // (by adding it to the start time of the parent timer).
      timers_stack_.top()->start_time_ += elapsed_time;
    }
    stats.AddSample(time_units_, elapsed_time);
  }

  // The time limit to use for advancing the deterministic time.
  TimeLimit* const time_limit_;
  // The time in nanoseconds when the timer was constructed.
  uint64_t start_time_;
  // The number of time units that were advanced by the timer.
  uint64_t time_units_ = 0;

  // The stack of currently active timers in the current thread. This is used to
  // deduct the time spent in nested timers (we assume that timers are destroyed
  // in the reverse order of their construction).
  static thread_local std::stack<BaseDeterministicTimer*> timers_stack_;
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

  ~DeterministicTimer() { AddStatsSample(stats_); }

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

#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_DETERMINISTIC_TIME_H_
