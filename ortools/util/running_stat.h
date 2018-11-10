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

#ifndef OR_TOOLS_UTIL_RUNNING_STAT_H_
#define OR_TOOLS_UTIL_RUNNING_STAT_H_

#include <deque>

#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Simple class to compute the average over a fixed size window of an integer
// stream.
class RunningAverage {
 public:
  // Initialize the class with the maximum window size.
  // It must be positive (this is CHECKed).
  explicit RunningAverage(int window_size = 1);

  // Resets the class to the exact same state as if it was just constructed with
  // the given window size.
  void Reset(int window_size);

  // Adds the next integer of the stream.
  void Add(int value);

  // Returns the average of all the values added so far or zero if no values
  // where added.
  double GlobalAverage() const;

  // Returns the average of the values in the current window or zero if the
  // current window is empty.
  double WindowAverage() const;

  // Returns true iff the current window size is equal to the one specified in
  // the constructor.
  bool IsWindowFull() const;

  // Clears the current window.
  void ClearWindow();

 private:
  int window_size_;
  int num_adds_;
  double global_sum_;
  double local_sum_;
  std::deque<int> values_;

  DISALLOW_COPY_AND_ASSIGN(RunningAverage);
};

// Simple class to compute efficiently the maximum over a fixed size window
// of a numeric stream. This works in constant average amortized time.
template <class Number = double>
class RunningMax {
 public:
  // Takes the size of the running window. The size must be positive.
  explicit RunningMax(int window_size);

  // Processes a new element from the stream.
  void Add(Number value);

  // Returns the current maximum element in the window.
  // An element must have been added before calling this function.
  Number GetCurrentMax();

 private:
  const int window_size_;

  // Values in the current window.
  std::vector<Number> values_;

  // Index of the last added element in the window.
  int last_index_;

  // Index of the current maximum element.
  int max_index_;

  DISALLOW_COPY_AND_ASSIGN(RunningMax);
};

// ################## Implementations below #####################

inline RunningAverage::RunningAverage(int window_size)
    : window_size_(window_size),
      num_adds_(0),
      global_sum_(0.0),
      local_sum_(0.0) {
  CHECK_GT(window_size_, 0);
}

inline void RunningAverage::Reset(int window_size) {
  window_size_ = window_size;
  num_adds_ = 0;
  global_sum_ = 0.0;
  ClearWindow();
}

inline void RunningAverage::Add(int value) {
  ++num_adds_;
  global_sum_ += value;
  local_sum_ += value;
  values_.push_back(value);
  if (values_.size() > window_size_) {
    local_sum_ -= values_.front();
    values_.pop_front();
  }
}

inline double RunningAverage::GlobalAverage() const {
  return num_adds_ == 0 ? 0.0 : global_sum_ / static_cast<double>(num_adds_);
}

inline double RunningAverage::WindowAverage() const {
  return values_.empty() ? 0.0
                         : local_sum_ / static_cast<double>(values_.size());
}

inline void RunningAverage::ClearWindow() {
  local_sum_ = 0.0;
  values_.clear();
}

inline bool RunningAverage::IsWindowFull() const {
  return values_.size() == window_size_;
}

template <class Number>
RunningMax<Number>::RunningMax(int window_size)
    : window_size_(window_size), values_(), last_index_(0), max_index_(0) {
  DCHECK_GT(window_size, 0);
}

template <class Number>
void RunningMax<Number>::Add(Number value) {
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
      Number max_value = values_[max_index_];
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

template <class Number>
Number RunningMax<Number>::GetCurrentMax() {
  DCHECK(!values_.empty());
  return values_[max_index_];
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_RUNNING_STAT_H_
