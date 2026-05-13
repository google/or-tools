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
#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// This class implements a propagator for non_overlap_2d constraints that uses
// the BinaryRelationsMaps to detect precedences between pairs of boxes and
// detect a conflict if the precedences implies an overlap between the two
// boxes. For doing this efficiently, it keep track of pairs of boxes that have
// non-fixed precedences in the BinaryRelationsMaps and only check those in the
// propagation.
class Precedences2DPropagator : public PropagatorInterface {
 public:
  Precedences2DPropagator(NoOverlap2DConstraintHelper* helper, Model* model);

  ~Precedences2DPropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void CollectPairsOfBoxesWithNonTrivialDistance();

  std::vector<std::pair<int, int>> non_trivial_pairs_;

  NoOverlap2DConstraintHelper& helper_;
  BinaryRelationsMaps* binary_relations_maps_;
  SharedStatistics* shared_stats_;

  int last_helper_inprocessing_count_ = -1;
  int last_num_expressions_ = -1;

  int64_t num_conflicts_ = 0;
  int64_t num_calls_ = 0;

  Precedences2DPropagator(const Precedences2DPropagator&) = delete;
  Precedences2DPropagator& operator=(const Precedences2DPropagator&) = delete;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_DISTANCES_PROPAGATOR_H_
