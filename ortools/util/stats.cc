// Copyright 2010-2022 Google LLC
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

#include "ortools/util/stats.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/sysinfo.h"
#include "ortools/port/utf8.h"

namespace operations_research {

std::string MemoryUsage() {
  const int64_t mem = operations_research::sysinfo::MemoryUsageProcess();
  static const int64_t kDisplayThreshold = 2;
  static const int64_t kKiloByte = 1024;
  static const int64_t kMegaByte = kKiloByte * kKiloByte;
  static const int64_t kGigaByte = kMegaByte * kKiloByte;
  if (mem > kDisplayThreshold * kGigaByte) {
    return absl::StrFormat("%.2lf GB", mem * 1.0 / kGigaByte);
  } else if (mem > kDisplayThreshold * kMegaByte) {
    return absl::StrFormat("%.2lf MB", mem * 1.0 / kMegaByte);
  } else if (mem > kDisplayThreshold * kKiloByte) {
    return absl::StrFormat("%2lf KB", mem * 1.0 / kKiloByte);
  } else {
    return absl::StrFormat("%d", mem);
  }
}

Stat::Stat(const std::string& name, StatsGroup* group) : name_(name) {
  group->Register(this);
}

std::string Stat::StatString() const { return name_ + ": " + ValueAsString(); }

StatsGroup::~StatsGroup() { gtl::STLDeleteValues(&time_distributions_); }

void StatsGroup::Register(Stat* stat) { stats_.push_back(stat); }

void StatsGroup::Reset() {
  for (int i = 0; i < stats_.size(); ++i) {
    stats_[i]->Reset();
  }
}

namespace {

bool CompareStatPointers(const Stat* s1, const Stat* s2) {
  if (s1->Priority() == s2->Priority()) {
    if (s1->Sum() == s2->Sum()) return s1->Name() < s2->Name();
    return (s1->Sum() > s2->Sum());
  } else {
    return (s1->Priority() > s2->Priority());
  }
}

}  // namespace

std::string StatsGroup::StatString() const {
  // Computes the longest name of all the stats we want to display.
  // Also create a temporary vector so we can sort the stats by names.
  int longest_name_size = 0;
  std::vector<Stat*> sorted_stats;
  for (int i = 0; i < stats_.size(); ++i) {
    if (!stats_[i]->WorthPrinting()) continue;
    // We support UTF8 characters in the stat names.
    const int size = operations_research::utf8::UTF8StrLen(stats_[i]->Name());
    longest_name_size = std::max(longest_name_size, size);
    sorted_stats.push_back(stats_[i]);
  }
  switch (print_order_) {
    case SORT_BY_PRIORITY_THEN_VALUE:
      std::sort(sorted_stats.begin(), sorted_stats.end(), CompareStatPointers);
      break;
    case SORT_BY_NAME:
      std::sort(sorted_stats.begin(), sorted_stats.end(),
                [](const Stat* s1, const Stat* s2) -> bool {
                  return s1->Name() < s2->Name();
                });
      break;
    default:
      LOG(FATAL) << "Unknown print order: " << print_order_;
  }

  // Do not display groups without print-worthy stats.
  if (sorted_stats.empty()) return "";

  // Pretty-print all the stats.
  std::string result(name_ + " {\n");
  for (int i = 0; i < sorted_stats.size(); ++i) {
    result += "  ";
    result += sorted_stats[i]->Name();
    result.append(longest_name_size - operations_research::utf8::UTF8StrLen(
                                          sorted_stats[i]->Name()),
                  ' ');
    result += " : " + sorted_stats[i]->ValueAsString();
  }
  result += "}\n";
  return result;
}

TimeDistribution* StatsGroup::LookupOrCreateTimeDistribution(std::string name) {
  TimeDistribution*& ref = time_distributions_[name];
  if (ref == nullptr) {
    ref = new TimeDistribution(name);
    Register(ref);
  }
  return ref;
}

DistributionStat::DistributionStat(const std::string& name)
    : Stat(name),
      sum_(0.0),
      average_(0.0),
      sum_squares_from_average_(0.0),
      min_(0.0),
      max_(0.0),
      num_(0) {}

DistributionStat::DistributionStat(const std::string& name, StatsGroup* group)
    : Stat(name, group),
      sum_(0.0),
      average_(0.0),
      sum_squares_from_average_(0.0),
      min_(0.0),
      max_(0.0),
      num_(0) {}

void DistributionStat::Reset() {
  sum_ = 0.0;
  average_ = 0.0;
  sum_squares_from_average_ = 0.0;
  min_ = 0.0;
  max_ = 0.0;
  num_ = 0;
}

void DistributionStat::AddToDistribution(double value) {
  if (num_ == 0) {
    min_ = value;
    max_ = value;
    sum_ = value;
    average_ = value;
    num_ = 1;
    return;
  }
  min_ = std::min(min_, value);
  max_ = std::max(max_, value);
  sum_ += value;
  ++num_;
  const double delta = value - average_;
  average_ = sum_ / num_;
  sum_squares_from_average_ += delta * (value - average_);
}

double DistributionStat::Average() const { return average_; }

double DistributionStat::StdDeviation() const {
  if (num_ == 0) return 0.0;
  return sqrt(sum_squares_from_average_ / num_);
}

double TimeDistribution::CyclesToSeconds(double cycles) {
  const double seconds_per_cycles = CycleTimerBase::CyclesToSeconds(1);
  return cycles * seconds_per_cycles;
}

std::string TimeDistribution::PrintCyclesAsTime(double cycles) {
  DCHECK_GE(cycles, 0.0);
  // This epsilon is just to avoid displaying 1000.00ms instead of 1.00s.
  double eps1 = 1 + 1e-3;
  double sec = CyclesToSeconds(cycles);
  if (sec * eps1 >= 3600.0) return absl::StrFormat("%.2fh", sec / 3600.0);
  if (sec * eps1 >= 60.0) return absl::StrFormat("%.2fm", sec / 60.0);
  if (sec * eps1 >= 1.0) return absl::StrFormat("%.2fs", sec);
  if (sec * eps1 >= 1e-3) return absl::StrFormat("%.2fms", sec * 1e3);
  if (sec * eps1 >= 1e-6) return absl::StrFormat("%.2fus", sec * 1e6);
  return absl::StrFormat("%.2fns", sec * 1e9);
}

void TimeDistribution::AddTimeInSec(double seconds) {
  DCHECK_GE(seconds, 0.0);
  const double cycles_per_seconds = 1.0 / CycleTimerBase::CyclesToSeconds(1);
  AddToDistribution(seconds * cycles_per_seconds);
}

void TimeDistribution::AddTimeInCycles(double cycles) {
  DCHECK_GE(cycles, 0.0);
  AddToDistribution(cycles);
}

std::string TimeDistribution::ValueAsString() const {
  return absl::StrFormat(
      "%8u [%8s, %8s] %8s %8s %8s\n", num_, PrintCyclesAsTime(min_),
      PrintCyclesAsTime(max_), PrintCyclesAsTime(Average()),
      PrintCyclesAsTime(StdDeviation()), PrintCyclesAsTime(sum_));
}

void RatioDistribution::Add(double value) {
  DCHECK_GE(value, 0.0);
  AddToDistribution(value);
}

std::string RatioDistribution::ValueAsString() const {
  return absl::StrFormat("%8u [%7.2f%%, %7.2f%%] %7.2f%% %7.2f%%\n", num_,
                         100.0 * min_, 100.0 * max_, 100.0 * Average(),
                         100.0 * StdDeviation());
}

void DoubleDistribution::Add(double value) { AddToDistribution(value); }

std::string DoubleDistribution::ValueAsString() const {
  return absl::StrFormat("%8u [%8.1e, %8.1e] %8.1e %8.1e\n", num_, min_, max_,
                         Average(), StdDeviation());
}

void IntegerDistribution::Add(int64_t value) {
  AddToDistribution(static_cast<double>(value));
}

std::string IntegerDistribution::ValueAsString() const {
  return absl::StrFormat("%8u [%8.f, %8.f] %8.2f %8.2f %8.f\n", num_, min_,
                         max_, Average(), StdDeviation(), sum_);
}

#ifdef HAS_PERF_SUBSYSTEM
EnabledScopedInstructionCounter::EnabledScopedInstructionCounter(
    const std::string& name, TimeLimit* time_limit)
    : time_limit_(time_limit), name_(name) {
  starting_count_ =
      time_limit_ != nullptr ? time_limit_->ReadInstructionCounter() : 0;
}

EnabledScopedInstructionCounter::~EnabledScopedInstructionCounter() {
  ending_count_ =
      time_limit_ != nullptr ? time_limit_->ReadInstructionCounter() : 0;
  LOG(INFO) << name_ << ", Instructions: " << ending_count_ - starting_count_;
}
#endif  // HAS_PERF_SUBSYSTEM

}  // namespace operations_research
