// Copyright 2010 Google
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

#ifndef BASE_TIMER_H_
#define BASE_TIMER_H_

#include "base/basictypes.h"
#include "base/macros.h"

namespace operations_research {
class WallTimer {
 public:
  WallTimer();

  void Start();
  void Stop();
  bool Reset();
  void Restart();
  bool IsRunning() const;
  int64 GetInMs() const;
  double Get() const;
  static int64 GetTimeInMicroSeconds();

 private:
  int64 start_usec_;  // start time in microseconds.
  int64 sum_usec_;    // sum of time diffs in microseconds.
  bool has_started_;

  DISALLOW_COPY_AND_ASSIGN(WallTimer);
};

// This class of timer is very precise and potentially more expensive than
// the WallTimer class.
class CycleTimer {
 public:
  CycleTimer();
  void Reset();
  void Start();
  void Stop();
  int64 GetInUsec() const;
  int64 GetInMs() const;
 private:
  enum State {
    INIT,
    STARTED,
    STOPPED
  };
  int64 time_in_us_;
  State state_;
};
}  // namespace operations_research
#endif  // BASE_TIMER_H_
