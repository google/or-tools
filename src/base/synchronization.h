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

#ifndef OR_TOOLS_BASE_SYNCHRONIZATION_H_
#define OR_TOOLS_BASE_SYNCHRONIZATION_H_

#include "base/logging.h"
#include "base/mutex.h"

namespace operations_research {
class Barrier {
 public:
  explicit Barrier(int num_threads)
  : num_to_block_(num_threads), num_to_exit_(num_threads) {}

  bool Block() {
    MutexLock l(&this->lock_);
    this->num_to_block_--;
    CHECK_GE(this->num_to_block_, 0);
    if (num_to_block_ > 0) {
      while (num_to_block_ > 0) {
        condition_.Wait(&lock_);
      }
    } else {
      condition_.SignalAll();
    }
    this->num_to_exit_--;
    CHECK_GE(this->num_to_exit_, 0);
    return this->num_to_exit_ == 0;
  }

 private:
  Mutex lock_;
  CondVar condition_;
  int num_to_block_;
  int num_to_exit_;
  DISALLOW_COPY_AND_ASSIGN(Barrier);
};

template<class T, class TT>
inline T ThreadSafeIncrement(T* value, Mutex* sm, TT inc) {
  MutexLock l(sm);
  return (*value) += inc;
}
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_SYNCHRONIZATION_H_
