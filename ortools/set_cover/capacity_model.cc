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

#include "ortools/set_cover/capacity_model.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
void CapacityModel::AddTerm(SubsetIndex subset, ElementIndex element,
                            CapacityWeight weight) {
  subsets_.push_back(subset);
  elements_.push_back(element);
  weights_.push_back(weight);

  CHECK_EQ(elements_.size(), subsets_.size());
  CHECK_EQ(elements_.size(), weights_.size());
}

void CapacityModel::SetMinimumCapacity(CapacityWeight min_capacity) {
  CHECK_NE(min_capacity, std::numeric_limits<CapacityWeight>::max());
  min_capacity_ = min_capacity;
}

void CapacityModel::SetMaximumCapacity(CapacityWeight max_capacity) {
  CHECK_NE(max_capacity, std::numeric_limits<CapacityWeight>::min());
  max_capacity_ = max_capacity;
}

bool CapacityModel::ComputeFeasibility() const {
  if (weights_.empty()) {
    // A sum of zero terms is zero.
    return min_capacity_ <= 0.0 && max_capacity_ >= 0.0;
  }

  // Compute the minimum and maximum constraint activations.
  CapacityWeight min_activation = 0.0;
  CapacityWeight max_activation = 0.0;
  for (const CapacityWeight weight : weights_) {
    if (weight < 0.0) {
      min_activation += weight;
    } else {
      max_activation += weight;
    }
  }

  DVLOG(1) << "[Capacity constraint] Activation bounds: [" << min_activation
           << ", " << max_activation << "]";
  DVLOG(1) << "[Capacity constraint] Capacity bounds: [" << min_capacity_
           << ", " << max_capacity_ << "]";
  return min_activation <= max_capacity_ && max_activation >= min_capacity_;
}

std::vector<CapacityTermIndex> CapacityModel::CanonicalIndexing() {
  std::vector<CapacityTermIndex> idx(num_terms());
  std::iota(idx.begin(), idx.end(), CapacityTermIndex(0));
  // TODO(user): use RadixSort when it's available. The implementation in
  // radix_sort.h does not support a lambda for comparing.
  std::sort(idx.begin(), idx.end(),
            [&](CapacityTermIndex lhs, CapacityTermIndex rhs) -> bool {
              return subsets_[lhs] < subsets_[rhs]
                         ? true
                         : elements_[lhs] < elements_[rhs];
            });
  return idx;
}

CapacityConstraintProto CapacityModel::ExportModelAsProto() {
  CapacityConstraintProto proto;
  proto.set_min_capacity(min_capacity_);
  proto.set_max_capacity(max_capacity_);

  CapacityConstraintProto::CapacityTerm* current_term = nullptr;

  for (CapacityTermIndex i : CanonicalIndexing()) {
    if (current_term == nullptr ||
        current_term->subset() != subsets_[i].value()) {
      current_term = proto.add_capacity_term();
      current_term->set_subset(subsets_[i].value());
    }
    DCHECK(current_term != nullptr);

    CapacityConstraintProto::CapacityTerm::ElementWeightPair* pair =
        current_term->add_element_weights();
    pair->set_element(elements_[i].value());
    pair->set_weight(weights_[i]);
  }

  return proto;
}

void CapacityModel::ImportModelFromProto(const CapacityConstraintProto& proto) {
  elements_.clear();
  subsets_.clear();
  weights_.clear();

  SetMinimumCapacity(proto.min_capacity());
  SetMaximumCapacity(proto.max_capacity());

  ReserveNumTerms(proto.capacity_term_size());
  for (const CapacityConstraintProto::CapacityTerm& term :
       proto.capacity_term()) {
    for (const CapacityConstraintProto::CapacityTerm::ElementWeightPair& pair :
         term.element_weights()) {
      AddTerm(SubsetIndex(term.subset()), ElementIndex(pair.element()),
              pair.weight());
    }
  }
}
}  // namespace operations_research
