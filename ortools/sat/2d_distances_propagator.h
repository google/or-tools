// Copyright 2010-2025 Google LLC
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

#ifndef OR_TOOLS_SAT_2D_DISTANCES_PROPAGATOR_H_
#define OR_TOOLS_SAT_2D_DISTANCES_PROPAGATOR_H_

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_map.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// This class implements a propagator for non_overlap_2d constraints that uses
// the Linear2Bounds to detect precedences between pairs of boxes and
// detect a conflict if the precedences implies an overlap between the two
// boxes. For doing this efficiently, it keeps track of pairs of boxes that have
// non-fixed precedences in the Linear2Bounds and only check those in the
// propagation.
class Precedences2DPropagator : public PropagatorInterface {
 public:
  Precedences2DPropagator(NoOverlap2DConstraintHelper* helper, Model* model);

  ~Precedences2DPropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void CollectNewPairsOfBoxesWithNonTrivialDistance();
  void UpdateVarLookups();
  IntegerValue UpperBound(
      std::variant<LinearExpression2, LinearExpression2Index> linear2) const;
  void AddOrUpdateDataForPairOfBoxes(int box1, int box2);

  struct PairData {
    // The condition must be true if ub(linear2) < ub.
    struct Condition {
      // If the expression is in the Linear2Indices it is represented by its
      // index, otherwise it is represented by the expression itself.
      std::variant<LinearExpression2, LinearExpression2Index> linear2;
      IntegerValue ub;
    };

    absl::FixedArray<Literal, 4> pair_presence_literals;
    int box1;
    int box2;
    // start1_before_end2[0==x, 1==y][0=start_1_end_2, 1=start_2_end_1]
    Condition start_before_end[2][2];
  };
  absl::flat_hash_map<std::pair<int, int>, int> non_trivial_pairs_index_;
  std::vector<PairData> pair_data_;
  struct VarUsage {
    // boxes[0=x, 1=y][0=start, 1=end]
    std::vector<int> boxes[2][2];
  };

  absl::flat_hash_map<IntegerVariable, VarUsage> var_to_box_and_coeffs_;

  NoOverlap2DConstraintHelper& helper_;
  Linear2Bounds* linear2_bounds_;
  Linear2Watcher* linear2_watcher_;
  SharedStatistics* shared_stats_;
  Linear2Indices* lin2_indices_;
  Trail* trail_;
  IntegerTrail* integer_trail_;

  int last_helper_inprocessing_count_ = -1;
  int num_known_linear2_ = 0;
  int64_t last_linear2_timestamp_ = -1;

  int64_t num_conflicts_ = 0;
  int64_t num_calls_ = 0;

  Precedences2DPropagator(const Precedences2DPropagator&) = delete;
  Precedences2DPropagator& operator=(const Precedences2DPropagator&) = delete;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_DISTANCES_PROPAGATOR_H_
