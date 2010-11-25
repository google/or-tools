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

#ifndef BASE_TIMER_H
#define BASE_TIMER_H

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
 private:
  int64 start_usec_;  // start time in microseconds.
  int64 sum_usec_;    // sum of time diffs in microseconds.
  bool has_started_;

  DISALLOW_COPY_AND_ASSIGN(WallTimer);
};
}  // namespace operations_research
#endif  // BASE_TIMER_H
