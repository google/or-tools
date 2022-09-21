// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_ITERATORS_H_
#define OR_TOOLS_MATH_OPT_STORAGE_ITERATORS_H_

#include <iterator>

#include "ortools/math_opt/storage/range.h"

namespace operations_research::math_opt {

// An iterator for iterating over a field of the update trackers stored by
// ModelStorage. Typically create with MakeUpdateDataFieldIterator() below to
// avoid manually giving all type parameters, or better, with
// MakeUpdateDataFieldRange().
//
// Sample use:
//  const std::vector<std::pair<
//      UpdateTrackerId, std::unique_ptr<UpdateTrackerData>>>& data = ...;
//  for(LinearConstraintStorage::Diff& diff :
//      MakeUpdateDataFieldRange<
//        &UpdateTrackerData::dirty_linear_constraints>(data)) {
//    ...
//  }
//
// BaseIter must iterate over
//   const std::pair<XXX, std::unique_ptr<UpdateData>>&
// The returned iterator will be over non-const references to Field as read off
// the UpdateData values.
template <typename UpdateData, typename V, V UpdateData::*Field,
          typename BaseIter>
class UpdateDataFieldIterator {
 public:
  using value_type = V;
  using reference = V&;
  using pointer = V*;
  using difference_type = typename BaseIter::difference_type;
  using iterator_category = std::forward_iterator_tag;

  explicit UpdateDataFieldIterator(BaseIter base_iter)
      : base_iter_(std::move(base_iter)) {}

  reference operator*() const { return base_iter_->second.get()->*Field; }
  pointer operator->() const { return &base_iter_->second->*Field; }
  // Prefix
  UpdateDataFieldIterator& operator++() {
    ++base_iter_;
    return *this;
  }
  // Postfix
  UpdateDataFieldIterator operator++(int) {
    UpdateDataFieldIterator result = *this;
    ++base_iter_;
    return result;
  }

  friend bool operator==(const UpdateDataFieldIterator& lhs,
                         const UpdateDataFieldIterator& rhs) {
    return lhs.base_iter_ == rhs.base_iter_;
  }
  friend bool operator!=(const UpdateDataFieldIterator& lhs,
                         const UpdateDataFieldIterator& rhs) {
    return lhs.base_iter_ != rhs.base_iter_;
  }

 private:
  BaseIter base_iter_;
};

// The "functions" are used to extract the types of the field and of the class
// from pointers-to-member. They have no implementation since they are only used
// with `decltype`.
//
// Example:
//   int A::* ptr = &A::x;
//   decltype(MemberPointerValueType(PointerToMember)) v;
//   decltype(MemberPointerClassType(PointerToMember)) c;
//   // The type of `v` is `int` and the type of `c` is `A`.
template <typename V, typename C>
V MemberPointerValueType(V C::*);
template <typename V, typename C>
C MemberPointerClassType(V C::*);

// Returns a new iterator on the field identified by the PointerToMember
// wrapping an iterator of a:
//   std::vector<std::pair<ID, std::unique_ptr<UpdateData>>
//
// Example:
//  std::vector<std::pair<Id, std::unique_ptr<UpdateData>> update_trackers;
//  const auto begin =
//    MakeUpdateDataFieldIterator<&UpdateData::field>(update_trackers.cbegin());
template <auto PointerToMember, typename BaseIter>
auto MakeUpdateDataFieldIterator(BaseIter base_iter) {
  return UpdateDataFieldIterator<
      decltype(MemberPointerClassType(PointerToMember)),
      decltype(MemberPointerValueType(PointerToMember)), PointerToMember,
      BaseIter>(std::move(base_iter));
}

// Returns a range (begin and end iterator) pointing to the field identified by
// the PointerToMember from an iterable of
//   std::pair<ID, std::unique_ptr<UpdateData>
//
// Example:
//   std::vector<std::pair<Id, std::unique_ptr<UpdateData>> trackers;
//   for(auto& f : MakeUpdateDataFieldRange<&UpdateData::field>(trackers)) {
//     ...
//   }
template <auto PointerToMember, typename UpdateTrackers>
auto MakeUpdateDataFieldRange(const UpdateTrackers& trackers) {
  return make_range(
      MakeUpdateDataFieldIterator<PointerToMember>(trackers.begin()),
      MakeUpdateDataFieldIterator<PointerToMember>(trackers.end()));
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_ITERATORS_H_
