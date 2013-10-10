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
#ifndef OR_TOOLS_UTIL_TIME_LIMIT_H_
#define OR_TOOLS_UTIL_TIME_LIMIT_H_

#include <vector>
#include "base/timer.h"

namespace operations_research {

// Simple class to compute efficiently the maximum over a fixed size window
// of a double stream. This works in constant average amortized time.
class RunningMax {
 public:
  // Takes the size of the running window. The size must be positive.
  explicit RunningMax(int window_size);

  // Processes a new element from the stream.
  void Add(double value);

  // Returns the current maximum element in the window.
  // An element must have been added before calling this function.
  double GetCurrentMax();

 private:
  const int window_size_;

  // Values in the current window.
  std::vector<double> values_;

  // Index of the last added element in the window.
  int last_index_;

  // Index of the current maximum element.
  int max_index_;
};

// A simple class to enforce a time limit in the same thread as a program. The
// idea is to call LimitReached() as often as possible (but not too often if you
// care about performance) until it returns false. By analysing the time
// distribution between two calls, the function should return false before the
// time limit is reached. The program should then abort as fast as possible.
class TimeLimit {
 public:
  // Takes the wanted time limit in seconds (relative to "now", and based on the
  // wall time). If limit_in_seconds is infinity, then LimitReached() will
  // always return false.
  explicit TimeLimit(double limit_in_seconds);

  // Returns true if the next time LimitReached() is called is likely to be over
  // the time limit. The prediction is based on the interval between previous
  // succesive calls to LimitReached().
  //
  // TODO(user): if this is called too often, do nothing for some number of
  // calls to speed the test?
  bool LimitReached();

  // Returns the time left on this limit.
  // Returns 0.0 if the limit was reached.
  double GetTimeLeft() const;

 private:
  // Size of the running window of measurements taken into account.
  static const int kValues = 100;

  WallTimer timer_;
  double last_;
  double limit_;
  RunningMax running_max_;

  DISALLOW_COPY_AND_ASSIGN(TimeLimit);
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_TIME_LIMIT_H_
