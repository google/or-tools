// Copyright 2011-2014 Google
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
//
//
// Two basic LocalSearchOperators for the job-shop problem.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LS_H_
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LS_H_

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/bitmap.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

  // ----- Exchange 2 intervals on a sequence variable -----
class SwapIntervals : public SequenceVarLocalSearchOperator {
 public:
  SwapIntervals(const std::vector<operations_research::SequenceVar*>& vars)
      : SequenceVarLocalSearchOperator(vars),
        current_var_(-1),
        current_first_(-1),
        current_second_(-1) {}

  virtual ~SwapIntervals() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "End neighborhood search";
        return false;
      }

      std::vector<int> sequence = Sequence(current_var_);
      const int tmp = sequence[current_first_];
      sequence[current_first_] = sequence[current_second_];
      sequence[current_second_] = tmp;
      SetForwardSequence(current_var_, sequence);
      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }

 protected:
  virtual void OnStart() {
    VLOG(1) << "Start neighborhood search";
    current_var_ = 0;
    current_first_ = 0;
    current_second_ = 0;
  }

 private:
  bool Increment() {
    const SequenceVar* const var = Var(current_var_);
    if (++current_second_ >= var->size()) {
      if (++current_first_ >= var->size() - 1) {
        current_var_++;
        current_first_ = 0;
      }
      current_second_ = current_first_ + 1;
    }
    return current_var_ < Size();
  }

  int current_var_;
  int current_first_;
  int current_second_;
};

// ----- Shuffle a fixed-length sub-sequence on one sequence variable -----
class ShuffleIntervals : public SequenceVarLocalSearchOperator {
 public:
  ShuffleIntervals(const std::vector<operations_research::SequenceVar*>& vars, const int64 max_length)
      : SequenceVarLocalSearchOperator(vars),
        max_length_(max_length),
        current_var_(-1),
        current_first_(-1),
        current_length_(-1) {
          CHECK_GE(max_length_, 2)
          << "The suffle length should be greater or equal to 2.";
        }

  virtual ~ShuffleIntervals() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "Finish neighborhood search";
        return false;
      }

      std::vector<int> sequence = Sequence(current_var_);
      std::vector<int> sequence_backup(current_length_);

      for (int i = 0; i < current_length_; ++i) {
        sequence_backup[i] = sequence[i + current_first_];
      }
      for (int i = 0; i < current_length_; ++i) {
        sequence[i + current_first_] =
            sequence_backup[current_permutation_[i]];
      }

      SetForwardSequence(current_var_, sequence);
      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }

 protected:
  virtual void OnStart() {
    VLOG(1) << "Start neighborhood search";
    current_var_ = 0;
    current_first_ = 0;
    current_length_ = std::min(Var(current_var_)->size(), max_length_);
    current_permutation_.resize(current_length_);
    for (int i = 0; i < current_length_; ++i) {
      current_permutation_[i] = i;
    }
  }

 private:
  bool Increment() {
    if (!std::next_permutation(current_permutation_.begin(),
                              current_permutation_.end())) {
      // No permutation anymore -> update indices
      if (++current_first_ > Var(current_var_)->size() - current_length_) {
        if (++current_var_ >= Size()) {
          return false;
        }
        current_first_ = 0;
        current_length_ = std::min(Var(current_var_)->size(), max_length_);
        current_permutation_.resize(current_length_);
      }
      for (int i = 0; i < current_length_; ++i) {
          current_permutation_[i] = i;
      }
      // start with the next permutation, not the identity just constructed
      if (!std::next_permutation(current_permutation_.begin(),
                                 current_permutation_.end())) {
        LOG(FATAL) << "Should never happen!";
      }
    }
    return true;
  }

  const int64 max_length_;
  int current_var_;
  int current_first_;
  int current_length_;
  std::vector<int> current_permutation_;
};

}  //  namespace operations_research

#endif  //  OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LS_H_
