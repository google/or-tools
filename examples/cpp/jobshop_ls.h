// Copyright 2010-2014 Google
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

// This model implements a simple jobshop problem.
//
// A jobshop is a standard scheduling problem where you must schedule a
// set of jobs on a set of machines.  Each job is a sequence of tasks
// (a task can only start when the preceding task finished), each of
// which occupies a single specific machine during a specific
// duration. Therefore, a job is simply given by a sequence of pairs
// (machine id, duration).

// The objective is to minimize the 'makespan', which is the duration
// between the start of the first task (across all machines) and the
// completion of the last task (across all machines).
//
// This will be modelled by sets of intervals variables (see class
// IntervalVar in constraint_solver/constraint_solver.h), one per
// task, representing the [start_time, end_time] of the task.  Tasks
// in the same job will be linked by precedence constraints.  Tasks on
// the same machine will be covered by Sequence constraints.
//
// Search will be implemented as local search on the sequence variables.

#ifndef OR_TOOLS_EXAMPLES_JOBSHOP_LS_H_
#define OR_TOOLS_EXAMPLES_JOBSHOP_LS_H_

#include <cstdio>
#include <cstdlib>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/base/random.h"

namespace operations_research {
// ----- Exchange 2 intervals on a sequence variable -----

class SwapIntervals : public SequenceVarLocalSearchOperator {
 public:
  explicit SwapIntervals(const std::vector<SequenceVar*>& vars)
      : SequenceVarLocalSearchOperator(vars),
        current_var_(-1),
        current_first_(-1),
        current_second_(-1) {}

  ~SwapIntervals() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "finished neighborhood";
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
  void OnStart() override {
    VLOG(1) << "start neighborhood";
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
  ShuffleIntervals(const std::vector<SequenceVar*>& vars, int max_length)
      : SequenceVarLocalSearchOperator(vars),
        max_length_(max_length),
        current_var_(-1),
        current_first_(-1),
        current_index_(-1),
        current_length_(-1) {}

  ~ShuffleIntervals() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (!Increment()) {
        VLOG(1) << "finished neighborhood";
        return false;
      }

      std::vector<int> sequence = Sequence(current_var_);
      std::vector<int> sequence_backup(current_length_);
      for (int i = 0; i < current_length_; ++i) {
        sequence_backup[i] = sequence[i + current_first_];
      }
      for (int i = 0; i < current_length_; ++i) {
        sequence[i + current_first_] = sequence_backup[current_permutation_[i]];
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
  void OnStart() override {
    VLOG(1) << "start neighborhood";
    current_var_ = 0;
    current_first_ = 0;
    current_index_ = -1;
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
      if (++current_first_ >= Var(current_var_)->size() - current_length_) {
        if (++current_var_ >= Size()) {
          return false;
        }
        current_first_ = 0;
        current_length_ = std::min(Var(current_var_)->size(), max_length_);
        current_permutation_.resize(current_length_);
      }
      current_index_ = 0;
    }
    return true;
  }

  const int64 max_length_;
  int current_var_;
  int current_first_;
  int current_index_;
  int current_length_;
  std::vector<int> current_permutation_;
};

// ----- LNS Operator -----

class SequenceLns : public SequenceVarLocalSearchOperator {
 public:
  SequenceLns(const std::vector<SequenceVar*>& vars, int seed, int max_length)
      : SequenceVarLocalSearchOperator(vars),
        random_(seed),
        max_length_(max_length) {}

  ~SequenceLns() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override {
    CHECK_NOTNULL(delta);
    while (true) {
      RevertChanges(true);
      if (random_.Uniform(2) == 0) {
        FreeTimeWindow();
      } else {
        FreeTwoResources();
      }
      if (ApplyChanges(delta, deltadelta)) {
        VLOG(1) << "Delta = " << delta->DebugString();
        return true;
      }
    }
    return false;
  }

 private:
  void FreeTimeWindow() {
    for (int i = 0; i < Size(); ++i) {
      std::vector<int> sequence = Sequence(i);
      const int current_length =
          std::min(static_cast<int>(sequence.size()), max_length_);
      const int start_position =
          random_.Uniform(sequence.size() - current_length);
      std::vector<int> forward;
      for (int j = 0; j < start_position; ++j) {
        forward.push_back(sequence[j]);
      }
      std::vector<int> backward;
      for (int j = sequence.size() - 1; j >= start_position + current_length;
           --j) {
        backward.push_back(sequence[j]);
      }
      SetForwardSequence(i, forward);
      SetBackwardSequence(i, backward);
    }
  }

  void FreeTwoResources() {
    std::vector<int> free_sequence;
    SetForwardSequence(random_.Uniform(Size()), free_sequence);
    SetForwardSequence(random_.Uniform(Size()), free_sequence);
  }

  ACMRandom random_;
  const int max_length_;
};
}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_JOBSHOP_LS_H_
