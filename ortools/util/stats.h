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

// Helper classes to track statistics of a program component.
//
// Usage example:
// // Suppose you have a class that contains a factorization of a matrix B and
// // a Solve() function to solve the linear system B.x = a.
//
// // You will hold your stats in a Stats stats_ class member:
// struct Stats : public StatsGroup {
//   Stats() : StatsGroup("BasisFactorization"),
//       solve_time("solve_time", this),
//       input_vector_density("input_vector_density", this),
//       estimated_accuracy("estimated_accuracy", this) {}
//
//   TimeDistribution solve_time;
//   RatioDistribution input_vector_density;
//
//   // Values of a few components of B.x - a, updated on each solve.
//   DoubleDistribution estimated_accuracy;
// }
//
// // You then add a few lines to your Solve() function:
// void Solve() {
//   stats_.solve_time.StartTimer();
//   stats_.input_vector_density.Add(ComputeDensity());
//   ...  // Do the work.
//   stats_.estimated_accuracy.Add(EstimateAccuracy());
//   stats_.solve_time.StopTimerAndAddElapsedTime();
// }
//
// // Now, calling stats_.StatString() will give you a summary of your stats:
// BasisFactorization {
//   solve_time           : num [min, max] average std_deviation total
//   input_vector_density : num [min, max] average std_deviation
//   estimated_accuracy   : num [min, max] average std_deviation
// }
//
// For measuring time, another alternative is to use the SCOPED_TIME_STAT macro.
// In our example above, you don't need to define the solve_time distribution
// and you can just do:
//
// void Solve() {
//   SCOPED_TIME_STAT(&stats_);
//   ...
// }
//
// This automatically adds a TimeDistribution with name "Solve" to stats_ and
// times your function calls!
//
// IMPORTANT: The SCOPED_TIME_STAT() macro only does something if OR_STATS is
// defined, so you need to build your code with blaze build --copt='-DOR_STATS'.
// The idea is that by default the instrumentation is off. You can also use the
// macro IF_STATS_ENABLED() that does nothing if OR_STATS is not defined or just
// translates to its argument otherwise.

#ifndef OR_TOOLS_UTIL_STATS_H_
#define OR_TOOLS_UTIL_STATS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/timer.h"

namespace operations_research {

// Returns the current thread's total memory usage in an human-readable string.
std::string MemoryUsage();

// Forward declaration.
class StatsGroup;
class TimeDistribution;

// Base class for a statistic that can be pretty-printed.
class Stat {
 public:
  explicit Stat(absl::string_view name) : name_(name) {}

  // Also add this stat to the given group.
  Stat(absl::string_view name, StatsGroup* group);
  virtual ~Stat() {}

  // Only used for display purposes.
  std::string Name() const { return name_; }

  // Returns a human-readable formatted line of the form "name:
  // ValueAsString()".
  std::string StatString() const;

  // At display, stats are displayed by decreasing priority, then decreasing
  // Sum(), then alphabetical order.
  // Used to group the stats per category (timing, ratio, etc..,).
  virtual int Priority() const { return 0; }

  // By default return 0 for the sum. This makes it possible to sort stats by
  // decreasing total time.
  virtual double Sum() const { return 0; }

  // Prints information about this statistic.
  virtual std::string ValueAsString() const = 0;

  // Is this stat worth printing? Usually false if nothing was measured.
  virtual bool WorthPrinting() const = 0;

  // Reset this statistic to the same state as if it was newly created.
  virtual void Reset() = 0;

 private:
  const std::string name_;
};

// Base class to print a nice summary of a group of statistics.
class StatsGroup {
 public:
  enum PrintOrder {
    SORT_BY_PRIORITY_THEN_VALUE = 0,
    SORT_BY_NAME = 1,
  };

  explicit StatsGroup(absl::string_view name)
      : name_(name), stats_(), time_distributions_() {}

  // This type is neither copyable nor movable.
  StatsGroup(const StatsGroup&) = delete;
  StatsGroup& operator=(const StatsGroup&) = delete;
  ~StatsGroup() = default;

  // Registers a Stat, which will appear in the string returned by StatString().
  // The Stat object must live as long as this StatsGroup.
  void Register(Stat* stat);

  // Returns this group name, followed by one line per Stat registered with this
  // group (this includes the ones created by LookupOrCreateTimeDistribution()).
  // Note that only the stats WorthPrinting() are printed.
  std::string StatString() const;

  // Changes the print ordering (will affect the order in which the stats
  // registered with this group are printed via StatString()).
  void SetPrintOrder(PrintOrder print_order) { print_order_ = print_order; }

  // Returns and if needed creates and registers a TimeDistribution with the
  // given name. Note that this involve a hash map lookup and is thus slower
  // than directly accessing a TimeDistribution variable.
  TimeDistribution* LookupOrCreateTimeDistribution(absl::string_view name);

  // Calls Reset() on all the statistics registered with this group.
  void Reset();

 private:
  std::string name_;
  PrintOrder print_order_ = SORT_BY_PRIORITY_THEN_VALUE;
  std::vector<Stat*> stats_;
  absl::flat_hash_map<std::string, std::unique_ptr<TimeDistribution>>
      time_distributions_;
};

// Base class to track and compute statistics about the distribution of a
// sequence of double. We provide a few sub-classes below that differ in the way
// the values are added to the sequence and in the way the stats are printed.
class DistributionStat : public Stat {
 public:
  explicit DistributionStat(absl::string_view name);
  DistributionStat() : DistributionStat("") {}
  DistributionStat(absl::string_view name, StatsGroup* group);
  ~DistributionStat() override {}
  void Reset() override;
  bool WorthPrinting() const override { return num_ != 0; }

  // Implemented by the subclasses.
  std::string ValueAsString() const override = 0;

  // Trivial statistics on all the values added so far.
  double Sum() const override { return sum_; }
  double Max() const { return max_; }
  double Min() const { return min_; }
  int64_t Num() const { return num_; }

  // Get the average of the distribution or 0.0 if empty.
  double Average() const;

  // Get the standard deviation of the distribution or 0.0 if empty.
  // We use the on-line algorithm of Welford described at
  // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
  // TODO(user): We could also use on top the Kahan summation algorithm to be
  // even more precise but a bit slower too.
  double StdDeviation() const;

 protected:
  // Adds a value to this sequence and updates the stats.
  void AddToDistribution(double value);
  double sum_;
  double average_;
  double sum_squares_from_average_;
  double min_;
  double max_;
  int64_t num_;
};

// Statistic on the distribution of a sequence of running times.
// Also provides some facility to measure such time with the CPU cycle counter.
//
// TODO(user): Since we inherit from DistributionStat, we currently store the
// sum of CPU cycles as a double internally. A better option is to use int64_t
// because with the 53 bits of precision of a double, we will run into an issue
// if the sum of times reaches 52 days for a 2GHz processor.
class TimeDistribution : public DistributionStat {
 public:
  explicit TimeDistribution(absl::string_view name)
      : DistributionStat(name), timer_() {}
  TimeDistribution() : TimeDistribution("") {}
  TimeDistribution(absl::string_view name, StatsGroup* group)
      : DistributionStat(name, group), timer_() {}
  std::string ValueAsString() const override;

  // Time distributions have a high priority to be displayed first.
  int Priority() const override { return 100; }

  // Internally the TimeDistribution stores CPU cycles (to do a bit less work
  // on each StopTimerAndAddElapsedTime()). Use this function to convert
  // all the statistics of DistributionStat into seconds.
  static double CyclesToSeconds(double num_cycles);

  // Adds a time in seconds to this distribution.
  void AddTimeInSec(double seconds);
  void AddTime(absl::Duration duration) {
    AddTimeInSec(absl::ToDoubleSeconds(duration));
  }

  // Adds a time in CPU cycles to this distribution.
  void AddTimeInCycles(double cycles);

  // Starts the timer in preparation of a StopTimerAndAddElapsedTime().
  inline void StartTimer() { timer_.Restart(); }

  // Adds the elapsed time since the last StartTimer() to the distribution and
  // returns this time in CPU cycles.
  inline double StopTimerAndAddElapsedTime() {
    const double cycles = static_cast<double>(timer_.GetCycles());
    AddToDistribution(cycles);
    return cycles;
  }

 private:
  // Converts and prints a number of cycles in an human readable way using the
  // proper time unit depending on the value (ns, us, ms, s, m or h).
  static std::string PrintCyclesAsTime(double cycles);
  CycleTimer timer_;
};

// Statistic on the distribution of a sequence of ratios, displayed as %.
class RatioDistribution : public DistributionStat {
 public:
  explicit RatioDistribution(absl::string_view name) : DistributionStat(name) {}
  RatioDistribution() : RatioDistribution("") {}
  RatioDistribution(absl::string_view name, StatsGroup* group)
      : DistributionStat(name, group) {}
  std::string ValueAsString() const override;
  void Add(double value);
};

// Statistic on the distribution of a sequence of doubles.
class DoubleDistribution : public DistributionStat {
 public:
  explicit DoubleDistribution(absl::string_view name)
      : DistributionStat(name) {}
  DoubleDistribution() : DoubleDistribution("") {}
  DoubleDistribution(absl::string_view name, StatsGroup* group)
      : DistributionStat(name, group) {}
  std::string ValueAsString() const override;
  void Add(double value);
};

// Statistic on the distribution of a sequence of integers.
class IntegerDistribution : public DistributionStat {
 public:
  explicit IntegerDistribution(absl::string_view name)
      : DistributionStat(name) {}
  IntegerDistribution() : IntegerDistribution("") {}
  IntegerDistribution(absl::string_view name, StatsGroup* group)
      : DistributionStat(name, group) {}
  std::string ValueAsString() const override;
  void Add(int64_t value);
};

// Helper classes to time a block of code and add the result to a
// TimeDistribution. Calls StartTimer() on creation and
// StopTimerAndAddElapsedTime() on destruction.
//
// There are three classes with the same interface:
// * EnabledScopedTimeDistributionUpdater always collects the time stats of the
//   scope in which it is defined. This class is used for stats that are always
//   collected.
// * ScopedTimeDistributionUpdater collects the time stats only when OR_STATS is
//   defined. This symbol should be used for collecting stats in places where
//   the overhead of collecting the stats may hurt the performance of the
//   algorithm.
// * DisabledScopedTimeDistributionUpdater is used to implement
//   ScopedTimeDistributionUpdater when OR_STATS is not defined.
class EnabledScopedTimeDistributionUpdater {
 public:
  // Note that this does not take ownership of the given stat.
  explicit EnabledScopedTimeDistributionUpdater(TimeDistribution* stat)
      : stat_(stat), also_update_(nullptr) {
    stat->StartTimer();
  }

  // This type is neither copyable nor movable.
  EnabledScopedTimeDistributionUpdater(
      const EnabledScopedTimeDistributionUpdater&) = delete;
  EnabledScopedTimeDistributionUpdater& operator=(
      const EnabledScopedTimeDistributionUpdater&) = delete;
  ~EnabledScopedTimeDistributionUpdater() {
    const double cycles = stat_->StopTimerAndAddElapsedTime();
    if (also_update_ != nullptr) {
      also_update_->AddTimeInCycles(cycles);
    }
  }

  // Updates another TimeDistribution on destruction. This is useful to split
  // a total time measurement in different categories:
  //
  // EnabledScopedTimeDistributionUpdater timer(&total_timer);
  // ...
  // switch (type) {
  //   case TypeA : timer.AlsoUpdate(&typeA_timer); break;
  //   case TypeB : timer.AlsoUpdate(&typeB_timer); break;
  // }
  void AlsoUpdate(TimeDistribution* also_update) { also_update_ = also_update; }

 private:
  TimeDistribution* stat_;
  TimeDistribution* also_update_;
};

class DisabledScopedTimeDistributionUpdater {
 public:
  explicit DisabledScopedTimeDistributionUpdater(TimeDistribution*) {}

  // This type is neither copyable nor movable.
  DisabledScopedTimeDistributionUpdater(
      const DisabledScopedTimeDistributionUpdater&) = delete;
  DisabledScopedTimeDistributionUpdater& operator=(
      const DisabledScopedTimeDistributionUpdater&) = delete;
  void AlsoUpdate(TimeDistribution*) {}
};

class DisabledScopedTimeStats {
 public:
  explicit DisabledScopedTimeStats(StatsGroup*, const char*) {}
  DisabledScopedTimeStats(const DisabledScopedTimeStats&) = delete;
  DisabledScopedTimeStats& operator=(const DisabledScopedTimeStats&) = delete;
  DisabledScopedTimeStats(DisabledScopedTimeStats&&) = delete;
  DisabledScopedTimeStats& operator=(DisabledScopedTimeStats&&) = delete;
};

// Measures the time from this macro line to the end of the scope and adds it
// to the distribution (from the given StatsGroup) with the same name as the
// enclosing function.
//
// Note(user): This adds more extra overhead around the measured code compared
// to defining your own TimeDistribution stat in your StatsGroup. About 80ns
// per measurement compared to about 20ns (as of 2012-06, on my workstation).
class EnabledScopedTimeStats {
 public:
  explicit EnabledScopedTimeStats(StatsGroup* stats,
                                  absl::string_view function_name)
      : scoped_time_stat_(
            stats->LookupOrCreateTimeDistribution(function_name)) {}
  EnabledScopedTimeStats(const EnabledScopedTimeStats&) = delete;
  EnabledScopedTimeStats& operator=(const EnabledScopedTimeStats&) = delete;
  EnabledScopedTimeStats(EnabledScopedTimeStats&&) = delete;
  EnabledScopedTimeStats& operator=(EnabledScopedTimeStats&&) = delete;

 private:
  operations_research::EnabledScopedTimeDistributionUpdater scoped_time_stat_;
};

#ifdef OR_STATS

using ScopedTimeDistributionUpdater = EnabledScopedTimeDistributionUpdater;
using ScopedTimeStats = EnabledScopedTimeStats;

// Simple macro to be used by a client that want to execute costly operations
// only if OR_STATS is defined.
#define IF_STATS_ENABLED(instructions) instructions

#else  // !OR_STATS
// If OR_STATS is not defined, we remove some instructions that may be time
// consuming.

using ScopedTimeDistributionUpdater = DisabledScopedTimeDistributionUpdater;
using ScopedTimeStats = DisabledScopedTimeStats;

// Defining it that way makes sure that the compiler still sees the code and
// checks that the syntax & types are valid.
#define IF_STATS_ENABLED(instructions) \
  if constexpr (false) {               \
    instructions;                      \
  }

#endif  // OR_STATS

#define SCOPED_TIME_STAT(stats) \
  operations_research::ScopedTimeStats scoped_time_stat(stats, __FUNCTION__);

#define SCOPED_INSTRUCTION_COUNT(time_limit)

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_STATS_H_
