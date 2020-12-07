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

#ifndef OR_TOOLS_UTIL_TIME_LIMIT_H_
#define OR_TOOLS_UTIL_TIME_LIMIT_H_

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/timer.h"
#include "ortools/util/running_stat.h"
#ifdef HAS_PERF_SUBSYSTEM
#include "exegesis/exegesis/itineraries/perf_subsystem.h"
#endif  // HAS_PERF_SUBSYSTEM

/**
 * Enables changing the behavior of the TimeLimit class to use -b usertime
 * instead of \b walltime. This is mainly useful for benchmarks.
 */
ABSL_DECLARE_FLAG(bool, time_limit_use_usertime);

/**
 * Adds support to measure the number of executed instructions in the TimeLimit
 * class.
 */
ABSL_DECLARE_FLAG(bool, time_limit_use_instruction_count);

namespace operations_research {

/**
 * A simple class to enforce both an elapsed time limit and a deterministic time
 * limit in the same thread as a program.
 * The idea is to call LimitReached() as often as possible, until it returns
 * false. The program should then abort as fast as possible.
 *
 * The deterministic limit is used to ensure reproductibility. As a consequence
 * the deterministic time has to be advanced manually using the method
 * AdvanceDeterministicTime().
 *
 * The instruction counter keeps track of number of executed cpu instructions.
 * It uses Performance Monitoring Unit (PMU) counters to keep track of
 * instruction count.
 *
 * The call itself is as fast as CycleClock::Now() + a few trivial instructions,
 * unless the time_limit_use_instruction_count flag is set.
 *
 * The limit is very conservative: it returns true (i.e. the limit is reached)
 * when current_time + max(T, ε) >= limit_time, where ε is a small constant (see
 * TimeLimit::kSafetyBufferSeconds), and T is the maximum measured time interval
 * between two consecutive calls to LimitReached() over the last kHistorySize
 * calls (so that we only consider "recent" history).
 * This is made so that the probability of actually exceeding the time limit is
 * small, without aborting too early.
 *
 * The deterministic time limit can be logged at a more granular level: the
 * method TimeLimit::AdvanceDeterministicTime takes an optional string argument:
 * the name of a counter. In debug mode, the time limit object computes also the
 * elapsed time for each named counter separately, and these values can be used
 * to determine the coefficients for computing the deterministic duration from
 * the number of operations. The values of the counters can be printed using
 * TimeLimit::DebugString(). There is no API to access the values of the
 * counters directly, because they do not exist in optimized mode.
 *
 * The basic steps for determining coefficients for the deterministic time are:
 * 1. Run the code in debug mode to collect the values of the deterministic time
 *    counters. Unless the algorithm is different in optimized mode, the values
 *    of the deterministic counters in debug mode will be the same as in
 *    optimized mode.
 * 2. Run the code in optimized mode to measure the real (CPU) time of the whole
 *    benchmark.
 * 3. Determine the coefficients for deterministic time from the real time and
 *    the values of the deterministic counters, e. g. by solving the equations
 *    C_1*c_1 + C_2*c_2 + ... + C_N*c_N + Err = T
 *    where C_1 is the unknown coefficient for counter c_1, Err is the random
 *    measurement error and T is the measured real time. The equation can be
 *    solved e.g. using the least squares method.
 *
 * Note that in optimized mode, the counters are disabled for performance
 * reasons, and calling AdvanceDeterministicTime(duration, counter_name) is
 * equivalent to calling AdvanceDeterministicTime(duration).
 */
// TODO(user): The expression "deterministic time" should be replaced with
//                 "number of operations" to avoid confusion with "real" time.
class TimeLimit {
 public:
  static const double kSafetyBufferSeconds;  // See the .cc for the value.
  static const int kHistorySize;

  /**
   * Sets the elapsed, the deterministic time and the instruction count limits.
   * The elapsed time is based on the wall time and the counter starts 'now'.
   * The deterministic time has to be manually advanced using the method
   * AdvanceDeterministicTime().
   *
   * Instruction count is the number of instructions executed. It is based on
   * PMU counters and is not very acurate.
   *
   * Use an infinite limit value to ignore a limit.
   */
  explicit TimeLimit(
      double limit_in_seconds,
      double deterministic_limit = std::numeric_limits<double>::infinity(),
      double instruction_limit = std::numeric_limits<double>::infinity());

  TimeLimit() : TimeLimit(std::numeric_limits<double>::infinity()) {}
  TimeLimit(const TimeLimit&) = delete;
  TimeLimit& operator=(const TimeLimit&) = delete;

  /**
   * Creates a time limit object that uses infinite time for wall time,
   * deterministic time and instruction count limit.
   */
  static std::unique_ptr<TimeLimit> Infinite() {
    return absl::make_unique<TimeLimit>(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity());
  }

  /**
   * Creates a time limit object that puts limit only on the deterministic time.
   */
  static std::unique_ptr<TimeLimit> FromDeterministicTime(
      double deterministic_limit) {
    return absl::make_unique<TimeLimit>(
        std::numeric_limits<double>::infinity(), deterministic_limit,
        std::numeric_limits<double>::infinity());
  }

  /**
   * Creates a time limit object initialized from an object that provides
   * methods \c max_time_in_seconds() and max_deterministic_time(). This method
   * is designed specifically to work with solver parameter protos, e.g.
   * \c BopParameters, \c MipParameters and \c SatParameters.
   */
  // TODO(user): Support adding instruction count limit from parameters.
  template <typename Parameters>
  static std::unique_ptr<TimeLimit> FromParameters(
      const Parameters& parameters) {
    return absl::make_unique<TimeLimit>(
        parameters.max_time_in_seconds(), parameters.max_deterministic_time(),
        std::numeric_limits<double>::infinity());
  }

  /**
   * Sets the instruction limit. We need this method since the static
   * constructor to create time limit from parameters doesn't support setting
   * instruction limit.
   */
  void SetInstructionLimit(double instruction_limit) {
    instruction_limit_ = instruction_limit;
  }

  /**
   * Returns the number of instructions executed since the creation of this
   * object.
   */
  // TODO(user): Use an exact counter for counting instructions. The
  // PMU counter returns the instruction count value as double since there may
  // be sampling issues.
  double ReadInstructionCounter();

  /**
   * Returns true when the external limit is true, or the deterministic time is
   * over the deterministic limit or if the next time \c LimitReached() is
   * called is likely to be over the time limit. See toplevel comment. Once it
   * has returned true, it is guaranteed to always return true.
   */
  bool LimitReached();

  /**
   * Returns the time left on this limit, or 0 if the limit was reached (it
   * never returns a negative value). Note that it might return a positive
   * value even though \c LimitReached() would return true; because the latter
   * is conservative (see toplevel comment). If \c LimitReached() was actually
   * called and did return \b true, though, this will always return 0.
   *
   * If the TimeLimit was constructed with \b infinity as the limit, this will
   * always return infinity.
   *
   * Note that this function is not optimized for speed as \c LimitReached() is.
   */
  double GetTimeLeft() const;

  /**
   * Returns the remaining deterministic time before \c LimitReached() returns
   * true due to the deterministic limit.
   * If the \c TimeLimit was constructed with \b infinity as the deterministic
   * limit (default value), this will always return infinity.
   */
  double GetDeterministicTimeLeft() const {
    return std::max(0.0, deterministic_limit_ - elapsed_deterministic_time_);
  }

  /**
   * Returns the number of instructions left to reach the limit.
   */
  double GetInstructionsLeft();

  /**
   * Advances the deterministic time. For reproducibility reasons, the
   * deterministic time doesn't advance automatically as the regular elapsed
   * time does.
   */
  inline void AdvanceDeterministicTime(double deterministic_duration) {
    DCHECK_LE(0.0, deterministic_duration);
    elapsed_deterministic_time_ += deterministic_duration;
  }

  /**
   * Advances the deterministic time. For reproducibility reasons, the
   * deterministic time doesn't advance automatically as the regular elapsed
   * time does.
   *
   * In debug mode, this method also updates the deterministic time counter with
   * the given name. In optimized mode, this method is equivalent to
   * \c AdvanceDeterministicTime(double).
   */
  inline void AdvanceDeterministicTime(double deterministic_duration,
                                       const char* counter_name) {
    AdvanceDeterministicTime(deterministic_duration);
#ifndef NDEBUG
    deterministic_counters_[counter_name] += deterministic_duration;
#endif
  }

  /**
   * Returns the time elapsed in seconds since the construction of this object.
   */
  double GetElapsedTime() const {
    return 1e-9 * (absl::GetCurrentTimeNanos() - start_ns_);
  }

  /**
   * Returns the elapsed deterministic time since the construction of this
   * object. That corresponds to the sum of all deterministic durations passed
   * as an argument to \c AdvanceDeterministicTime() calls.
   */
  double GetElapsedDeterministicTime() const {
    return elapsed_deterministic_time_;
  }

  /**
   * Registers the external Boolean to check when LimitReached() is called.
   * This is used to mark the limit as reached through an external Boolean,
   * i.e. \c LimitReached() returns true when the value of
   * external_boolean_as_limit is true whatever the time limits are.
   *
   * Note : The external_boolean_as_limit can be modified during solve.
   */
  void RegisterExternalBooleanAsLimit(
      std::atomic<bool>* external_boolean_as_limit) {
    external_boolean_as_limit_ = external_boolean_as_limit;
  }

  /**
   * Returns the current external Boolean limit.
   */
  std::atomic<bool>* ExternalBooleanAsLimit() const {
    return external_boolean_as_limit_;
  }

  /**
   * Sets new time limits. Note that this does not reset the running max nor
   * any registered external Boolean.
   */
  template <typename Parameters>
  void ResetLimitFromParameters(const Parameters& parameters);
  void MergeWithGlobalTimeLimit(TimeLimit* other);

  /**
   * Returns information about the time limit object in a human-readable form.
   */
  std::string DebugString() const;

 private:
  void ResetTimers(double limit_in_seconds, double deterministic_limit,
                   double instruction_limit);

  std::string GetInstructionRetiredEventName() const {
    return "inst_retired:any_p:u";
  }

  mutable int64 start_ns_;  // Not const! this is initialized after instruction
                            // counter initialization.
  int64 last_ns_;
  int64 limit_ns_;  // Not const! See the code of LimitReached().
  const int64 safety_buffer_ns_;
  RunningMax<int64> running_max_;

  // Only used when FLAGS_time_limit_use_usertime is true.
  UserTimer user_timer_;
  double limit_in_seconds_;

  double deterministic_limit_;
  double elapsed_deterministic_time_;

  std::atomic<bool>* external_boolean_as_limit_;

#ifdef HAS_PERF_SUBSYSTEM
  // PMU counter to help count the instructions.
  exegesis::PerfSubsystem perf_subsystem_;
#endif  // HAS_PERF_SUBSYSTEM
  // Given limit in terms of number of instructions.
  double instruction_limit_;

#ifndef NDEBUG
  // Contains the values of the deterministic time counters.
  absl::flat_hash_map<std::string, double> deterministic_counters_;
#endif

  friend class NestedTimeLimit;
  friend class ParallelTimeLimit;
};

// Wrapper around TimeLimit to make it thread safe and add Stop() support.
class SharedTimeLimit {
 public:
  explicit SharedTimeLimit(TimeLimit* time_limit)
      : time_limit_(time_limit), stopped_boolean_(false) {
    // We use the one already registered if present or ours otherwise.
    stopped_ = time_limit->ExternalBooleanAsLimit();
    if (stopped_ == nullptr) {
      stopped_ = &stopped_boolean_;
      time_limit->RegisterExternalBooleanAsLimit(stopped_);
    }
  }

  ~SharedTimeLimit() {
    if (stopped_ == &stopped_boolean_) {
      time_limit_->RegisterExternalBooleanAsLimit(nullptr);
    }
  }

  bool LimitReached() const {
    // Note, time_limit_->LimitReached() is not const, and changes internal
    // state of time_limit_, hence we need a writer's lock.
    absl::MutexLock lock(&mutex_);
    return time_limit_->LimitReached();
  }

  void Stop() {
    absl::MutexLock lock(&mutex_);
    *stopped_ = true;
  }

  void UpdateLocalLimit(TimeLimit* local_limit) {
    absl::MutexLock lock(&mutex_);
    local_limit->MergeWithGlobalTimeLimit(time_limit_);
  }

  void AdvanceDeterministicTime(double deterministic_duration) {
    absl::MutexLock lock(&mutex_);
    time_limit_->AdvanceDeterministicTime(deterministic_duration);
  }

  double GetTimeLeft() const {
    absl::ReaderMutexLock lock(&mutex_);
    return time_limit_->GetTimeLeft();
  }

  double GetElapsedDeterministicTime() const {
    absl::ReaderMutexLock lock(&mutex_);
    return time_limit_->GetElapsedDeterministicTime();
  }

 private:
  mutable absl::Mutex mutex_;
  TimeLimit* time_limit_ ABSL_GUARDED_BY(mutex_);
  std::atomic<bool> stopped_boolean_ ABSL_GUARDED_BY(mutex_);
  std::atomic<bool>* stopped_ ABSL_GUARDED_BY(mutex_);
};

/**
 * Provides a way to nest time limits for algorithms where a certain part of
 * the computation is bounded not just by the overall time limit, but also by a
 * stricter time limit specific just for this particular part.
 *
 * This class takes a base time limit object (the overall time limit) and the
 * part-specific time limit, and creates a new time limit object for the part.
 * This new time limit object will expire when either the overall time limit
 * expires or when the part-specific time limit expires.
 *
 * Example usage:
 * \code
 TimeLimit overall_time_limit(...);
 NestedTimeLimit subalgorith_time_limit(&overall_time_limit,
                                        subalgorithm_limit_in_seconds,
                                        subalgorithm_deterministic_limit);
 RunTheSubalgorithm(subalgorithm_time_limit.GetTimeLimit());
 \endcode
 *
 * Note that remaining wall time in the base time limit is decreasing
 * "automatically", but the deterministic time needs to be updated manually.
 * This update is done only once, during the destruction of the nested time
 * limit object. To track the deterministic time properly, the user must avoid
 * modifying the base time limit object when a nested time limit exists.
 *
 * The nested time limits supports the external time limit condition in the
 * sense, that if the overall time limit has an external boolean registered, the
 * nested time limit object will use the same boolean value as an external time
 * limit too.
 */
class NestedTimeLimit {
 public:
  /**
   * Creates the nested time limit. Note that 'base_time_limit' must remain
   * valid for the whole lifetime of the nested time limit object.
   */
  NestedTimeLimit(TimeLimit* base_time_limit, double limit_in_seconds,
                  double deterministic_limit);

  /**
   * Updates elapsed deterministic time in the base time limit object.
   */
  ~NestedTimeLimit();

  /**
   * Creates a time limit object initialized from a base time limit and an
   * object that provides methods max_time_in_seconds() and
   * max_deterministic_time(). This method is designed specifically to work with
   * solver parameter protos, e.g. BopParameters, MipParameters and
   * SatParameters.
   */
  template <typename Parameters>
  static std::unique_ptr<NestedTimeLimit> FromBaseTimeLimitAndParameters(
      TimeLimit* time_limit, const Parameters& parameters) {
    return absl::make_unique<NestedTimeLimit>(
        time_limit, parameters.max_time_in_seconds(),
        parameters.max_deterministic_time());
  }

  /**
   * Returns a time limit object that represents the combination of the overall
   * time limit and the part-specific time limit. The returned time limit object
   * is owned by the nested time limit object that returns it, and it will
   * remain valid until the nested time limit object is destroyed.
   */
  TimeLimit* GetTimeLimit() { return &time_limit_; }

 private:
  TimeLimit* const base_time_limit_;
  TimeLimit time_limit_;

  DISALLOW_COPY_AND_ASSIGN(NestedTimeLimit);
};

// ################## Implementations below #####################

inline TimeLimit::TimeLimit(double limit_in_seconds, double deterministic_limit,
                            double instruction_limit)
    : safety_buffer_ns_(static_cast<int64>(kSafetyBufferSeconds * 1e9)),
      running_max_(kHistorySize),
      external_boolean_as_limit_(nullptr) {
  ResetTimers(limit_in_seconds, deterministic_limit, instruction_limit);
}

inline void TimeLimit::ResetTimers(double limit_in_seconds,
                                   double deterministic_limit,
                                   double instruction_limit) {
  elapsed_deterministic_time_ = 0.0;
  deterministic_limit_ = deterministic_limit;
  instruction_limit_ = instruction_limit;

  if (absl::GetFlag(FLAGS_time_limit_use_usertime)) {
    user_timer_.Start();
    limit_in_seconds_ = limit_in_seconds;
  }
#ifdef HAS_PERF_SUBSYSTEM
  if (absl::GetFlag(FLAGS_time_limit_use_instruction_count)) {
    perf_subsystem_.CleanUp();
    perf_subsystem_.AddEvent(GetInstructionRetiredEventName());
    perf_subsystem_.StartCollecting();
  }
#endif  // HAS_PERF_SUBSYSTEM
  start_ns_ = absl::GetCurrentTimeNanos();
  last_ns_ = start_ns_;
  limit_ns_ = limit_in_seconds >= 1e-9 * (kint64max - start_ns_)
                  ? kint64max
                  : static_cast<int64>(limit_in_seconds * 1e9) + start_ns_;
}

template <typename Parameters>
inline void TimeLimit::ResetLimitFromParameters(const Parameters& parameters) {
  ResetTimers(parameters.max_time_in_seconds(),
              parameters.max_deterministic_time(),
              std::numeric_limits<double>::infinity());
}

inline void TimeLimit::MergeWithGlobalTimeLimit(TimeLimit* other) {
  if (other == nullptr) return;
  ResetTimers(
      std::min(GetTimeLeft(), other->GetTimeLeft()),
      std::min(GetDeterministicTimeLeft(), other->GetDeterministicTimeLeft()),
      std::numeric_limits<double>::infinity());
  if (other->ExternalBooleanAsLimit() != nullptr) {
    RegisterExternalBooleanAsLimit(other->ExternalBooleanAsLimit());
  }
}

inline double TimeLimit::ReadInstructionCounter() {
#ifdef HAS_PERF_SUBSYSTEM
  if (absl::GetFlag(FLAGS_time_limit_use_instruction_count)) {
    return perf_subsystem_.ReadCounters().GetScaledOrDie(
        GetInstructionRetiredEventName());
  }
#endif  // HAS_PERF_SUBSYSTEM
  return 0;
}

inline bool TimeLimit::LimitReached() {
  if (external_boolean_as_limit_ != nullptr &&
      external_boolean_as_limit_->load()) {
    return true;
  }

  if (GetDeterministicTimeLeft() <= 0.0) {
    return true;
  }

#ifdef HAS_PERF_SUBSYSTEM
  if (ReadInstructionCounter() >= instruction_limit_) {
    return true;
  }
#endif  // HAS_PERF_SUBSYSTEM

  const int64 current_ns = absl::GetCurrentTimeNanos();
  running_max_.Add(std::max(safety_buffer_ns_, current_ns - last_ns_));
  last_ns_ = current_ns;
  if (current_ns + running_max_.GetCurrentMax() >= limit_ns_) {
    if (absl::GetFlag(FLAGS_time_limit_use_usertime)) {
      // To avoid making many system calls, we only check the user time when
      // the "absolute" time limit has been reached. Note that the user time
      // should advance more slowly, so this is correct.
      const double time_left_s = limit_in_seconds_ - user_timer_.Get();
      if (time_left_s > kSafetyBufferSeconds) {
        limit_ns_ = static_cast<int64>(time_left_s * 1e9) + last_ns_;
        return false;
      }
    }

    // To ensure that future calls to LimitReached() will return true.
    limit_ns_ = 0;
    return true;
  }
  return false;
}

inline double TimeLimit::GetTimeLeft() const {
  if (limit_ns_ == kint64max) return std::numeric_limits<double>::infinity();
  const int64 delta_ns = limit_ns_ - absl::GetCurrentTimeNanos();
  if (delta_ns < 0) return 0.0;
  if (absl::GetFlag(FLAGS_time_limit_use_usertime)) {
    return std::max(limit_in_seconds_ - user_timer_.Get(), 0.0);
  } else {
    return delta_ns * 1e-9;
  }
}

inline double TimeLimit::GetInstructionsLeft() {
  return std::max(instruction_limit_ - ReadInstructionCounter(), 0.0);
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_TIME_LIMIT_H_
