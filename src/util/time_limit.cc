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
#include "util/time_limit.h"

#include <algorithm>
#include <limits>

namespace operations_research {

RunningMax::RunningMax(int window_size)
    : window_size_(window_size), values_(), last_index_(0), max_index_(0) {
  DCHECK_GT(window_size, 0);
}

void RunningMax::Add(double value) {
  if (values_.size() < window_size_) {
    // Starting phase until values_ reaches its final size.
    // Note that last_index_ stays at 0 during this phase.
    if (values_.empty() || value >= GetCurrentMax()) {
      max_index_ = values_.size();
    }
    values_.push_back(value);
    return;
  }
  // We are in the steady state.
  DCHECK_EQ(values_.size(), window_size_);
  // Note the use of >= instead of > to get the O(1) behavior in presence of
  // many identical values.
  if (value >= GetCurrentMax()) {
    max_index_ = last_index_;
    values_[last_index_] = value;
  } else {
    values_[last_index_] = value;
    if (last_index_ == max_index_) {
      // We need to recompute the max.
      // Note that this happens only if value was strictly lower than
      // GetCurrentMax() in the last window_size_ updates.
      max_index_ = 0;
      double max_value = values_[max_index_];
      for (int i = 1; i < values_.size(); ++i) {
        if (values_[i] > max_value) {
          max_value = values_[i];
          max_index_ = i;
        }
      }
    }
  }
  if (++last_index_ == window_size_) {
    last_index_ = 0;
  }
}

double RunningMax::GetCurrentMax() {
  DCHECK(!values_.empty());
  return values_[max_index_];
}

TimeLimit::TimeLimit(double limit_in_seconds)
    : timer_(), last_(0.0), limit_(0.0), running_max_(kValues) {
  timer_.Start();
  last_ = timer_.Get();
  limit_ = last_ + limit_in_seconds;
}

bool TimeLimit::LimitReached() {
  const double current = timer_.Get();

  // To be defensive, always abort if there is less than 0.1ms left, even if
  // the calls to LimitReached() are closer in time than this.
  //
  // TODO(user): make this a parameter of the class?
  running_max_.Add(std::max(1e-4, current - last_));
  last_ = current;
  return (current + running_max_.GetCurrentMax() >= limit_);
}

double TimeLimit::GetTimeLeft() const {
  const double current = timer_.Get();
  return (current < limit_) ? limit_ - current : 0.0;
}

}  // namespace operations_research
