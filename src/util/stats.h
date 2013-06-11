// Copyright 2010-2013 Google
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
// The idea is that by default the instrumentation is off. You can also use the
// macro IF_STATS_ENABLED() that does nothing if OR_STATS is not defined or just
// translates to its argument otherwise.

#ifndef OR_TOOLS_UTIL_STATS_H_
#define OR_TOOLS_UTIL_STATS_H_

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "base/timer.h"

using std::string;

namespace operations_research {

// Forward declaration.
class StatsGroup;
class TimeDistribution;

// Base class for a statistic that can be pretty-printed.
class Stat {
 public:
  explicit Stat(const string& name) : name_(name) {}

  // Also add this stat to the given group.
  Stat(const string& name, StatsGroup * group);
  virtual ~Stat() {}

  // Only used for display purpose.
  string Name() const { return name_; }

  // Returns a human-readable formated line of the form "name: ValueAsString()".
  string StatString() const;

  // Prints information about this statistic.
  virtual string ValueAsString() const = 0;

  // Is this stat worth printing? usually false if nothing was measured.
  virtual bool WorthPrinting() const = 0;

  // Reset this statistic to the same state as if it was newly created.
  virtual void Reset() = 0;

 private:
  const string name_;
};

// Base class to print a nice summary of a group of statistics.
class StatsGroup {
 public:
  explicit StatsGroup(const string& name)
      : name_(name), stats_(), time_distributions_() {}
  ~StatsGroup();

  // Registers a Stat, which will appear in the string returned by StatString().
  // The Stat object must live as long as this StatsGroup.
  void Register(Stat* stat);

  // Returns this group name, followed by one line per Stat registered with this
  // group (this includes the ones created by LookupOrCreateTimeDistribution()).
  // Note that only the stats WorthPrinting() are printed.
  string StatString() const;

  // Returns and if needed creates and registers a TimeDistribution with the
  // given name. Note that this involve a map lookup and his thus slower than
  // directly accessing a TimeDistribution variable.
  TimeDistribution *LookupOrCreateTimeDistribution(string name);

  // Calls Reset() on all the statistics registered with this group.
  void Reset();

 private:
  string name_;
  std::vector<Stat*> stats_;
  std::map<string, TimeDistribution *> time_distributions_;

  DISALLOW_COPY_AND_ASSIGN(StatsGroup);
};

// Base class to track and compute statistics about the distribution of a
// sequence of double. We provide a few sub-classes below that differ in the way
// the values are added to the sequence and in the way the stats are printed.
class DistributionStat : public Stat {
 public:
  explicit DistributionStat(const string& name);
  DistributionStat(const string& name, StatsGroup* group);
  virtual ~DistributionStat() {}
  virtual void Reset();
  virtual bool WorthPrinting() const { return num_ != 0; }

  // Implemented by the subclasses.
  virtual string ValueAsString() const = 0;

  // Trivial statistics on all the values added so far.
  double Sum() const { return sum_; }
  double Max() const { return max_; }
  double Min() const { return min_; }
  int64 Num() const { return num_; }

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
  int64 num_;
};

// Statistic on the distribution of a sequence of running times.
// Also provides some facility to measure such time with the cpu cycle counter.
//
// TODO(user): Since we inherit from DistributionStat, we currently store the
// sum of cpu cycles as a double internally. A better option is to use int64
// because with the 53 bits of precision of a double, we will run into an issue
// if the sum of times reaches 52 days for a 2GHz processor.
class TimeDistribution : public DistributionStat {
 public:
  explicit TimeDistribution(const string& name)
      : DistributionStat(name), timer_() {}
  TimeDistribution(const string& name, StatsGroup* group)
      : DistributionStat(name, group), timer_() {}
  virtual string ValueAsString() const;

  // Internaly the TimeDistribution stores cpu cycles (to do a bit less work
  // on each StopTimerAndAddElapsedTime()). Use this function to convert
  // all the statistics of DistributionStat into seconds.
  static double CyclesToSeconds(double num_cycles);

  // Adds a time in seconds to this distribution.
  void AddTimeInSec(double value);

  // Adds a time in cpu cycles to this distribution.
  void AddTimeInCycles(double value);

  // Starts the timer in preparation of a StopTimerAndAddElapsedTime().
  inline void StartTimer() {
    timer_.Restart();
  }

  // Adds the elapsed time since the last StartTimer() to the distribution and
  // returns this time in cpu cycles.
  inline double StopTimerAndAddElapsedTime() {
    const double cycles = static_cast<double>(timer_.GetCycles());
    AddToDistribution(cycles);
    return cycles;
  }

 private:
  // Converts and prints a number of cycles in an human readable way using the
  // proper time unit depending on the value (ns, us, ms, s, m or h).
  static string PrintCyclesAsTime(double cycles);
  CycleTimer timer_;
};

// Statistic on the distribution of a sequence of ratios, displayed as %.
class RatioDistribution : public DistributionStat {
 public:
  explicit RatioDistribution(const string& name) : DistributionStat(name) {}
  RatioDistribution(const string& name, StatsGroup* group)
      : DistributionStat(name, group) {}
  virtual string ValueAsString() const;
  void Add(double value);
};

// Statistic on the distribution of a sequence of doubles.
class DoubleDistribution : public DistributionStat {
 public:
  explicit DoubleDistribution(const string& name) : DistributionStat(name) {}
  DoubleDistribution(const string& name, StatsGroup* group)
      : DistributionStat(name, group) {}
  virtual string ValueAsString() const;
  void Add(double value);
};

// Statistic on the distribution of a sequence of integers.
class IntegerDistribution : public DistributionStat {
 public:
  explicit IntegerDistribution(const string& name) : DistributionStat(name) {}
  IntegerDistribution(const string& name, StatsGroup* group)
      : DistributionStat(name, group) {}
  virtual string ValueAsString() const;
  void Add(int64 value);
};

// Helper class to time a block of code and add the result to a
// TimeDistribution. Calls StartTimer() on creation and
// StopTimerAndAddElapsedTime() on destruction.
class ScopedTimeDistributionUpdater {
 public:
  // Note that this does not take ownership of the given stat.
  explicit ScopedTimeDistributionUpdater(TimeDistribution* stat)
      : stat_(stat), also_update_(NULL) {
    stat->StartTimer();
  }
  ~ScopedTimeDistributionUpdater() {
    const double cycles = stat_->StopTimerAndAddElapsedTime();
    if (also_update_ != NULL) {
      also_update_->AddTimeInCycles(cycles);
    }
  }

  // Updates another TimeDistribution on destruction. This is useful to split
  // a total time measurment in different categories:
  //
  // ScopedTimeDistributionUpdater timer(&total_timer);
  // ...
  // swith (type) {
  //   case TypeA : timer.AlsoUpdate(&typeA_timer); break;
  //   case TypeB : timer.AlsoUpdate(&typeB_timer); break;
  // }
  void AlsoUpdate(TimeDistribution* also_update) {
    also_update_ = also_update;
  }

 private:
  TimeDistribution *stat_;
  TimeDistribution *also_update_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTimeDistributionUpdater);
};

// If OR_STATS is not defined, we remove some instructions that may be time
// consuming.
#ifndef OR_STATS

#define IF_STATS_ENABLED(instructions)
#define SCOPED_TIME_STAT(stats)

#else  // OR_STATS

// Simple macro to be used by a client that want to execute costly operations
// only if OR_STATS is defined.
#define IF_STATS_ENABLED(instructions) instructions

// Measures the time from this macro line to the end of the scope and adds it
// to the distribution (from the given StatsGroup) with the same name as the
// enclosing function.
//
// Note(user): This adds more extra overhead around the measured code compared
// to defining your own TimeDistribution stat in your StatsGroup. About 80ns
// per measurement compared to about 20ns (as of 2012-06, on my workstation).
#define SCOPED_TIME_STAT(stats) \
    operations_research::ScopedTimeDistributionUpdater \
    scoped_time_stat((stats)->LookupOrCreateTimeDistribution(__FUNCTION__))

#endif  // OR_STATS

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_STATS_H_
