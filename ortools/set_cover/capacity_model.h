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

#ifndef OR_TOOLS_SET_COVER_CAPACITY_MODEL_H_
#define OR_TOOLS_SET_COVER_CAPACITY_MODEL_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/capacity.pb.h"
#include "ortools/set_cover/set_cover_model.h"

// Representation class for the capacity side-constraint for a weighted
// set-covering problem.
//
// This constraint restricts the selection of elements within subsets that
// respect the constraint. Such a constraint can mix elements in any subset.
//
// Using the same mixed-integer-programming formulation as `set_cover_model.h`,
// this class corresponds to the following constraint:
//     min_capacity <= \sum_{e in elements} weight_e * x_e <= max_capacity

namespace operations_research {
// Basic type for weights. For now, the same as `Cost` for the set covering.
using CapacityWeight = int64_t;

// Term index in a capacity constraint.
DEFINE_STRONG_INT_TYPE(CapacityTermIndex, BaseInt);

// The terms are represented as three aligned vectors: the element, the subset,
// and the weight. Each vector is indexed by the term.
using CapacityElements =
    util_intops::StrongVector<CapacityTermIndex, ElementIndex>;
using CapacitySubsets =
    util_intops::StrongVector<CapacityTermIndex, SubsetIndex>;
using CapacityWeights =
    util_intops::StrongVector<CapacityTermIndex, CapacityWeight>;

// Main class for describing a single capacity constraint in the context of a
// set-covering problem.
class CapacityModel {
 public:
  // Builds an empty capacity constraint.
  //
  // Use either WithMinimumWeight or WithMaximumWeight to set only one of the
  // two bounds.
  CapacityModel(CapacityWeight min, CapacityWeight max)
      : elements_(),
        subsets_(),
        weights_(),
        min_capacity_(min),
        max_capacity_(max) {
    // At least one bound must be set. Otherwise, the constraint is vacuous.
    CHECK(min_capacity_ != std::numeric_limits<CapacityWeight>::min() ||
          max_capacity_ != std::numeric_limits<CapacityWeight>::max());
  }

  static CapacityModel WithMinimumWeight(CapacityWeight min) {
    return CapacityModel(min, std::numeric_limits<CapacityWeight>::max());
  }

  static CapacityModel WithMaximumWeight(CapacityWeight max) {
    return CapacityModel(std::numeric_limits<CapacityWeight>::min(), max);
  }

  // Returns the current number of terms in the constraint.
  BaseInt num_terms() const { return elements_.size(); }

  // Returns the range of terms.
  util_intops::StrongIntRange<CapacityTermIndex> TermRange() const {
    return util_intops::StrongIntRange<CapacityTermIndex>(
        CapacityTermIndex(num_terms()));
  }

  // Adds a new term to the constraint.
  void AddTerm(SubsetIndex subset, ElementIndex element, CapacityWeight weight);

  // Returns the element, subset, or capacity of the given term.
  ElementIndex GetTermElementIndex(CapacityTermIndex term) const {
    return elements_[term];
  }
  SubsetIndex GetTermSubsetIndex(CapacityTermIndex term) const {
    return subsets_[term];
  }
  CapacityWeight GetTermCapacityWeight(CapacityTermIndex term) const {
    return weights_[term];
  }

  // Sets the lower/upper bounds for the constraint.
  // This will CHECK-fail if a capacity is a NaN.
  void SetMinimumCapacity(CapacityWeight min_capacity);
  void SetMaximumCapacity(CapacityWeight max_capacity);

  // Returns the lower/upper bounds for the constraint.
  CapacityWeight GetMinimumCapacity() const { return min_capacity_; }
  CapacityWeight GetMaximumCapacity() const { return max_capacity_; }

  // Returns true if the constraint is feasible, i.e. there is at least one
  // assignment that satisfies the constraint.
  bool ComputeFeasibility() const;

  // Reserves num_terms terms in the model.
  void ReserveNumTerms(BaseInt num_terms) {
    ReserveNumTerms(CapacityTermIndex(num_terms));
  }

  void ReserveNumTerms(CapacityTermIndex num_terms) {
    subsets_.reserve(num_terms);
    elements_.reserve(num_terms);
    weights_.reserve(num_terms);
  }

  // Returns the model as a CapacityConstraintProto.
  //
  // The function is not const because the terms need to be sorted for the
  // representation as a protobuf to be canonical.
  CapacityConstraintProto ExportModelAsProto();

  // Imports the model from a CapacityConstraintProto.
  void ImportModelFromProto(const CapacityConstraintProto& proto);

 private:
  // The terms in the constraint.
  CapacityElements elements_;
  CapacitySubsets subsets_;
  CapacityWeights weights_;

  // The bounds of the constraint. Both are always active at the same time.
  // An inactive constraint corresponds to a capacity set to ±∞.
  CapacityWeight min_capacity_;
  CapacityWeight max_capacity_;

  // Returns a canonical indexing of the constraint, i.e. reading the terms in
  // this order yields the order that is explained in the proto.
  std::vector<CapacityTermIndex> CanonicalIndexing();
};
}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_CAPACITY_MODEL_H_
