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
//  A basic Large Neighborhood Search operator for
//  the Job-Shop Problem.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LNS_H_
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LNS_H_

#include <vector>
#include <algorithm>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
  
class SequenceLns : public SequenceVarLocalSearchOperator {
 public:
  SequenceLns(const std::vector<SequenceVar*>& vars,
              int seed,
              int max_length)
      : SequenceVarLocalSearchOperator(vars),
        random_(seed),
        max_length_(max_length) {}

  virtual ~SequenceLns() {}

  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) {
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
      for (int j = sequence.size() - 1;
           j >= start_position + current_length;
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

}  //  namespace operations_research
#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_LNS_H_