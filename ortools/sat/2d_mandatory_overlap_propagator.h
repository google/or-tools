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

#ifndef ORTOOLS_SAT_2D_MANDATORY_OVERLAP_PROPAGATOR_H_
#define ORTOOLS_SAT_2D_MANDATORY_OVERLAP_PROPAGATOR_H_

#include <cstdint>
#include <vector>

#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// Propagator that checks that no mandatory area of two boxes overlap in
// O(N * log N) time.
void CreateAndRegisterMandatoryOverlapPropagator(
    NoOverlap2DConstraintHelper* helper, Model* model,
    GenericLiteralWatcher* watcher, int priority);

// Exposed for testing.
class MandatoryOverlapPropagator : public PropagatorInterface {
 public:
  MandatoryOverlapPropagator(NoOverlap2DConstraintHelper* helper, Model* model)
      : helper_(*helper),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~MandatoryOverlapPropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  NoOverlap2DConstraintHelper& helper_;
  SharedStatistics* shared_stats_;
  std::vector<Rectangle> mandatory_regions_;
  std::vector<int> mandatory_regions_index_;

  int64_t num_conflicts_ = 0;
  int64_t num_calls_zero_area_ = 0;
  int64_t num_calls_nonzero_area_ = 0;

  MandatoryOverlapPropagator(const MandatoryOverlapPropagator&) = delete;
  MandatoryOverlapPropagator& operator=(const MandatoryOverlapPropagator&) =
      delete;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_2D_MANDATORY_OVERLAP_PROPAGATOR_H_
