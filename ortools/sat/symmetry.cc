// Copyright 2010-2017 Google
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

#include "ortools/sat/symmetry.h"

#include "ortools/base/logging.h"
#include "ortools/base/int_type.h"

namespace operations_research {
namespace sat {

SymmetryPropagator::SymmetryPropagator()
    : SatPropagator("SymmetryPropagator"),
      stats_("SymmetryPropagator"),
      num_propagations_(0),
      num_conflicts_(0) {}

SymmetryPropagator::~SymmetryPropagator() {
  IF_STATS_ENABLED({
    LOG(INFO) << stats_.StatString();
    LOG(INFO) << "num propagations by symmetry: " << num_propagations_;
    LOG(INFO) << "num conflicts by symmetry: " << num_conflicts_;
  });
}

void SymmetryPropagator::AddSymmetry(
    std::unique_ptr<SparsePermutation> permutation) {
  if (permutation->NumCycles() == 0) return;
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(propagation_trail_index_, 0);
  if (permutation->Size() > images_.size()) {
    images_.resize(permutation->Size());
  }
  for (int c = 0; c < permutation->NumCycles(); ++c) {
    int e = permutation->LastElementInCycle(c);
    for (const int image : permutation->Cycle(c)) {
      DCHECK_GE(LiteralIndex(e), 0);
      DCHECK_LE(LiteralIndex(e), images_.size());
      const int permutation_index = permutations_.size();
      images_[LiteralIndex(e)].push_back(
          ImageInfo(permutation_index, Literal(LiteralIndex(image))));
      e = image;
    }
  }
  permutation_trails_.push_back(std::vector<AssignedLiteralInfo>());
  permutation_trails_.back().reserve(permutation->Support().size());
  permutations_.emplace_back(permutation.release());
}

bool SymmetryPropagator::PropagateNext(Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  const Literal true_literal = (*trail)[propagation_trail_index_];
  if (true_literal.Index() < images_.size()) {
    const std::vector<ImageInfo>& images = images_[true_literal.Index()];
    for (int image_index = 0; image_index < images.size(); ++image_index) {
      const int p_index = images[image_index].permutation_index;

      // TODO(user): some optim ideas: no need to enqueue if a decision image is
      // already assigned to false. But then the Untrail() is more involved.
      std::vector<AssignedLiteralInfo>* p_trail =
          &(permutation_trails_[p_index]);
      if (Enqueue(*trail, true_literal, images[image_index].image, p_trail)) {
        continue;
      }

      // We have a non-symmetric literal and its image is not already assigned
      // to
      // true.
      const AssignedLiteralInfo& non_symmetric =
          (*p_trail)[p_trail->back().first_non_symmetric_info_index_so_far];

      // If the first non-symmetric literal is a decision, then we can't deduce
      // anything. Otherwise, it is either a conflict or a propagation.
      const BooleanVariable non_symmetric_var =
          non_symmetric.literal.Variable();
      const AssignmentInfo& assignment_info = trail->Info(non_symmetric_var);
      if (trail->AssignmentType(non_symmetric_var) ==
          AssignmentType::kSearchDecision) {
        continue;
      }
      if (trail->Assignment().LiteralIsFalse(non_symmetric.image)) {
        // Conflict.
        ++num_conflicts_;

        // Set the conflict on the trail.
        // Note that we need to fetch a reason for this.
        std::vector<Literal>* conflict = trail->MutableConflict();
        const gtl::Span<Literal> initial_reason =
            trail->Reason(non_symmetric.literal.Variable());
        Permute(p_index, initial_reason, conflict);
        conflict->push_back(non_symmetric.image);
        for (Literal literal : *conflict) {
          DCHECK(trail->Assignment().LiteralIsFalse(literal)) << literal;
        }

        // Backtrack over all the enqueues we just did.
        for (; image_index >= 0; --image_index) {
          permutation_trails_[images[image_index].permutation_index].pop_back();
        }
        return false;
      } else {
        // Propagation.
        if (trail->Index() >= reasons_.size()) {
          reasons_.resize(trail->Index() + 1);
        }
        reasons_[trail->Index()] = {assignment_info.trail_index, p_index};
        trail->Enqueue(non_symmetric.image, propagator_id_);
        ++num_propagations_;
      }
    }
  }
  ++propagation_trail_index_;
  return true;
}

bool SymmetryPropagator::Propagate(Trail* trail) {
  const int old_index = trail->Index();
  while (trail->Index() == old_index && propagation_trail_index_ < old_index) {
    if (!PropagateNext(trail)) return false;
  }
  return true;
}

void SymmetryPropagator::Untrail(const Trail& trail, int trail_index) {
  SCOPED_TIME_STAT(&stats_);
  while (propagation_trail_index_ > trail_index) {
    --propagation_trail_index_;
    const Literal true_literal = trail[propagation_trail_index_];
    if (true_literal.Index() < images_.size()) {
      for (ImageInfo& info : images_[true_literal.Index()]) {
        permutation_trails_[info.permutation_index].pop_back();
      }
    }
  }
}

gtl::Span<Literal> SymmetryPropagator::Reason(const Trail& trail,
                                                     int trail_index) const {
  SCOPED_TIME_STAT(&stats_);
  const ReasonInfo& reason_info = reasons_[trail_index];
  std::vector<Literal>* reason = trail.GetEmptyVectorToStoreReason(trail_index);
  Permute(reason_info.symmetry_index,
          trail.Reason(trail[reason_info.source_trail_index].Variable()),
          reason);
  return *reason;
}

bool SymmetryPropagator::Enqueue(const Trail& trail, Literal literal,
                                 Literal image,
                                 std::vector<AssignedLiteralInfo>* p_trail) {
  // Small optimization to get the trail index of literal.
  const int literal_trail_index = propagation_trail_index_;
  DCHECK_EQ(literal_trail_index, trail.Info(literal.Variable()).trail_index);

  // Push the new AssignedLiteralInfo on the permutation trail. Note that we
  // don't know yet its first_non_symmetric_info_index_so_far but we know that
  // they are increasing, so we can restart by the one of the previous
  // AssignedLiteralInfo.
  p_trail->push_back(AssignedLiteralInfo(
      literal, image,
      p_trail->empty()
          ? 0
          : p_trail->back().first_non_symmetric_info_index_so_far));
  int* index = &(p_trail->back().first_non_symmetric_info_index_so_far);

  // Compute first_non_symmetric_info_index_so_far.
  while (*index < p_trail->size() &&
         trail.Assignment().LiteralIsTrue((*p_trail)[*index].image)) {
    // This AssignedLiteralInfo is symmetric for the full solver assignment.
    // We test if it is also symmetric for the assignment so far:
    if (trail.Info((*p_trail)[*index].image.Variable()).trail_index >
        literal_trail_index) {
      // It isn't, so we can stop the function here. We will continue the loop
      // when this function is called again with an higher trail_index.
      return true;
    }
    ++(*index);
  }
  return *index == p_trail->size();
}

void SymmetryPropagator::Permute(int index, gtl::Span<Literal> input,
                                 std::vector<Literal>* output) const {
  SCOPED_TIME_STAT(&stats_);

  // Initialize tmp_literal_mapping_ (resize it if needed).
  const SparsePermutation& permutation = *(permutations_[index].get());
  if (permutation.Size() > tmp_literal_mapping_.size()) {
    tmp_literal_mapping_.resize(permutation.Size());
    for (LiteralIndex i(0); i < tmp_literal_mapping_.size(); ++i) {
      tmp_literal_mapping_[i] = Literal(i);
    }
  }
  for (int c = 0; c < permutation.NumCycles(); ++c) {
    int e = permutation.LastElementInCycle(c);
    for (const int image : permutation.Cycle(c)) {
      tmp_literal_mapping_[LiteralIndex(e)] = Literal(LiteralIndex(image));
      e = image;
    }
  }

  // Permute the input into the output.
  output->clear();
  for (Literal literal : input) {
    output->push_back(tmp_literal_mapping_[literal.Index()]);
  }

  // Clean up.
  for (int e : permutation.Support()) {
    tmp_literal_mapping_[LiteralIndex(e)] = Literal(LiteralIndex(e));
  }
}

}  // namespace sat
}  // namespace operations_research
