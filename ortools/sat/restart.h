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

#ifndef OR_TOOLS_SAT_RESTART_H_
#define OR_TOOLS_SAT_RESTART_H_

#include <vector>

#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/running_stat.h"

namespace operations_research {
namespace sat {

// Contain the logic to decide when to restart a SAT tree search.
class RestartPolicy {
 public:
  explicit RestartPolicy(Model* model)
      : parameters_(*(model->GetOrCreate<SatParameters>())) {
    Reset();
  }

  // Resets the policy using the current model parameters.
  void Reset();

  // Returns true if the solver should be restarted before the next decision is
  // taken. Note that this will return true just once and then assumes that
  // the search was restarted and starts worrying about the next restart.
  bool ShouldRestart();

  // This will be called by the solver engine after each conflict. The arguments
  // reflect the state of the solver when the conflict was detected and before
  // the backjump.
  void OnConflict(int conflict_trail_index, int conflict_decision_level,
                  int conflict_lbd);

  // Returns the number of restarts since the last Reset().
  int NumRestarts() const { return num_restarts_; }

  // Returns a std::string with the current restart statistics.
  std::string InfoString() const;

 private:
  const SatParameters& parameters_;

  int num_restarts_;
  int conflicts_until_next_strategy_change_;
  int strategy_change_conflicts_;

  int strategy_counter_;
  std::vector<SatParameters::RestartAlgorithm> strategies_;

  int luby_count_;
  int conflicts_until_next_restart_;

  RunningAverage dl_running_average_;
  RunningAverage lbd_running_average_;
  RunningAverage trail_size_running_average_;
};

// Returns the ith element of the strategy S^univ proposed by M. Luby et al. in
// Optimal Speedup of Las Vegas Algorithms, Information Processing Letters 1993.
// This is used to decide the number of conflicts allowed before the next
// restart. This method, used by most SAT solvers, is usually referenced as
// Luby.
// Returns 2^{k-1} when i == 2^k - 1
//    and  SUniv(i - 2^{k-1} + 1) when 2^{k-1} <= i < 2^k - 1.
// The sequence is defined for i > 0 and starts with:
//   {1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, ...}
inline int SUniv(int i) {
  DCHECK_GT(i, 0);
  while (i > 2) {
    const int most_significant_bit_position =
        MostSignificantBitPosition64(i + 1);
    if ((1 << most_significant_bit_position) == i + 1) {
      return 1 << (most_significant_bit_position - 1);
    }
    i -= (1 << most_significant_bit_position) - 1;
  }
  return 1;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_RESTART_H_
