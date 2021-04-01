// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/restart.h"

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace sat {

void RestartPolicy::Reset() {
  num_restarts_ = 0;
  strategy_counter_ = 0;
  strategy_change_conflicts_ =
      parameters_.num_conflicts_before_strategy_changes();
  conflicts_until_next_strategy_change_ = strategy_change_conflicts_;

  luby_count_ = 0;
  conflicts_until_next_restart_ = parameters_.restart_period();

  dl_running_average_.Reset(parameters_.restart_running_window_size());
  lbd_running_average_.Reset(parameters_.restart_running_window_size());
  trail_size_running_average_.Reset(parameters_.blocking_restart_window_size());

  // Compute the list of restart algorithms to cycle through.
  //
  // TODO(user): for some reason, strategies_.assign() does not work as the
  // returned type of the proto enum iterator is int ?!
  strategies_.clear();
  for (int i = 0; i < parameters_.restart_algorithms_size(); ++i) {
    strategies_.push_back(parameters_.restart_algorithms(i));
  }
  if (strategies_.empty()) {
    const std::vector<std::string> string_values = absl::StrSplit(
        parameters_.default_restart_algorithms(), ',', absl::SkipEmpty());
    for (const std::string& string_value : string_values) {
      SatParameters::RestartAlgorithm tmp;
#if defined(__PORTABLE_PLATFORM__)
      if (string_value == "NO_RESTART") {
        tmp = SatParameters::NO_RESTART;
      } else if (string_value == "LUBY_RESTART") {
        tmp = SatParameters::LUBY_RESTART;
      } else if (string_value == "DL_MOVING_AVERAGE_RESTART") {
        tmp = SatParameters::DL_MOVING_AVERAGE_RESTART;
      } else if (string_value == "LBD_MOVING_AVERAGE_RESTART") {
        tmp = SatParameters::LBD_MOVING_AVERAGE_RESTART;
      } else if (string_value == "FIXED_RESTART") {
        tmp = SatParameters::FIXED_RESTART;
      } else {
        LOG(WARNING) << "Couldn't parse the RestartAlgorithm name: '"
                     << string_value << "'.";
        continue;
      }
#else   // __PORTABLE_PLATFORM__
      if (!SatParameters::RestartAlgorithm_Parse(string_value, &tmp)) {
        LOG(WARNING) << "Couldn't parse the RestartAlgorithm name: '"
                     << string_value << "'.";
        continue;
      }
#endif  // !__PORTABLE_PLATFORM__
      strategies_.push_back(tmp);
    }
  }
  if (strategies_.empty()) {
    strategies_.push_back(SatParameters::NO_RESTART);
  }
}

bool RestartPolicy::ShouldRestart() {
  bool should_restart = false;
  switch (strategies_[strategy_counter_ % strategies_.size()]) {
    case SatParameters::NO_RESTART:
      break;
    case SatParameters::LUBY_RESTART:
      if (conflicts_until_next_restart_ == 0) {
        luby_count_++;
        should_restart = true;
      }
      break;
    case SatParameters::DL_MOVING_AVERAGE_RESTART:
      if (dl_running_average_.IsWindowFull() &&
          dl_running_average_.GlobalAverage() <
              parameters_.restart_dl_average_ratio() *
                  dl_running_average_.WindowAverage()) {
        should_restart = true;
      }
      break;
    case SatParameters::LBD_MOVING_AVERAGE_RESTART:
      if (lbd_running_average_.IsWindowFull() &&
          lbd_running_average_.GlobalAverage() <
              parameters_.restart_lbd_average_ratio() *
                  lbd_running_average_.WindowAverage()) {
        should_restart = true;
      }
      break;
    case SatParameters::FIXED_RESTART:
      if (conflicts_until_next_restart_ == 0) {
        should_restart = true;
      }
      break;
  }
  if (should_restart) {
    num_restarts_++;

    // Strategy switch?
    if (conflicts_until_next_strategy_change_ == 0) {
      strategy_counter_++;
      strategy_change_conflicts_ +=
          static_cast<int>(parameters_.strategy_change_increase_ratio() *
                           strategy_change_conflicts_);
      conflicts_until_next_strategy_change_ = strategy_change_conflicts_;

      // The LUBY_RESTART strategy is considered the "stable" mode and we change
      // the polariy heuristic while under it.
      decision_policy_->SetStablePhase(
          strategies_[strategy_counter_ % strategies_.size()] ==
          SatParameters::LUBY_RESTART);
    }

    // Reset the various restart strategies.
    dl_running_average_.ClearWindow();
    lbd_running_average_.ClearWindow();
    conflicts_until_next_restart_ = parameters_.restart_period();
    if (strategies_[strategy_counter_ % strategies_.size()] ==
        SatParameters::LUBY_RESTART) {
      conflicts_until_next_restart_ *= SUniv(luby_count_ + 1);
    }
  }
  return should_restart;
}

void RestartPolicy::OnConflict(int conflict_trail_index,
                               int conflict_decision_level, int conflict_lbd) {
  // Decrement the restart counter if needed.
  if (conflicts_until_next_restart_ > 0) {
    --conflicts_until_next_restart_;
  }
  if (conflicts_until_next_strategy_change_ > 0) {
    --conflicts_until_next_strategy_change_;
  }

  trail_size_running_average_.Add(conflict_trail_index);
  dl_running_average_.Add(conflict_decision_level);
  lbd_running_average_.Add(conflict_lbd);

  // Block the restart.
  // Note(user): glucose only activate this after 10000 conflicts.
  if (parameters_.use_blocking_restart()) {
    if (lbd_running_average_.IsWindowFull() &&
        dl_running_average_.IsWindowFull() &&
        trail_size_running_average_.IsWindowFull() &&
        conflict_trail_index >
            parameters_.blocking_restart_multiplier() *
                trail_size_running_average_.WindowAverage()) {
      dl_running_average_.ClearWindow();
      lbd_running_average_.ClearWindow();
    }
  }
}

std::string RestartPolicy::InfoString() const {
  std::string result =
      absl::StrFormat("  num restarts: %d\n", num_restarts_) +
      absl::StrFormat(
          "  current_strategy: %s\n",
          ProtoEnumToString<SatParameters::RestartAlgorithm>(
              strategies_[strategy_counter_ % strategies_.size()])) +
      absl::StrFormat("  conflict decision level avg: %f window: %f\n",
                      dl_running_average_.GlobalAverage(),
                      dl_running_average_.WindowAverage()) +
      absl::StrFormat("  conflict lbd avg: %f window: %f\n",
                      lbd_running_average_.GlobalAverage(),
                      lbd_running_average_.WindowAverage()) +
      absl::StrFormat("  conflict trail size avg: %f window: %f\n",
                      trail_size_running_average_.GlobalAverage(),
                      trail_size_running_average_.WindowAverage());
  return result;
}

}  // namespace sat
}  // namespace operations_research
