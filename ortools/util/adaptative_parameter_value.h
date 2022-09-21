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

#ifndef OR_TOOLS_UTIL_ADAPTATIVE_PARAMETER_VALUE_H_
#define OR_TOOLS_UTIL_ADAPTATIVE_PARAMETER_VALUE_H_

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ortools/base/integral_types.h"

namespace operations_research {

// Basic adaptive [0.0, 1.0] parameter that can be increased or decreased with a
// step that get smaller and smaller with the number of updates.
//
// After a while, if the probability of getting a Decrease() vs Increase() when
// running at a given value is f(value), then this should converge towards a
// value such that f(value) = 0.5 provided f is a non-decreasing function over
// [0.0, 1.0].
//
// TODO(user): The current logic work well in practice, but has no strong
// theoretical foundation. We should be able to come up with a better understood
// formula that converge way faster. It will also be nice to generalize the 0.5
// above to a target probability p.
class AdaptiveParameterValue {
 public:
  // Initial value is in [0.0, 1.0], both 0.0 and 1.0 are valid.
  explicit AdaptiveParameterValue(double initial_value)
      : value_(initial_value) {}

  void Reset() { num_changes_ = 0; }

  void Increase() {
    const double factor = IncreaseNumChangesAndGetFactor();
    value_ = std::min(1.0 - (1.0 - value_) / factor, value_ * factor);
  }

  void Decrease() {
    const double factor = IncreaseNumChangesAndGetFactor();
    value_ = std::max(value_ / factor, 1.0 - (1.0 - value_) * factor);
  }

  // If we get more than 1 datapoints from the same value(), we use a formula
  // that is more sound than calling n times Increase()/Decrease() which depends
  // on the order of calls.
  void Update(int num_decreases, int num_increases) {
    if (num_decreases == num_increases) {
      num_changes_ += num_decreases + num_increases;
    } else if (num_decreases < num_increases) {
      for (int i = num_decreases; i < num_increases; ++i) Increase();
      num_changes_ += 2 * num_decreases;
    } else {
      for (int i = num_increases; i < num_decreases; ++i) Decrease();
      num_changes_ += 2 * num_increases;
    }
  }

  double value() const { return value_; }

 private:
  // We want to change the parameters more and more slowly.
  double IncreaseNumChangesAndGetFactor() {
    ++num_changes_;
    return 1.0 + 1.0 / std::sqrt(num_changes_ + 1);
  }

  double value_;
  int64_t num_changes_ = 0;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ADAPTATIVE_PARAMETER_VALUE_H_
