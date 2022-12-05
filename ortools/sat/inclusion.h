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

#ifndef OR_TOOLS_SAT_INCLUSION_H_
#define OR_TOOLS_SAT_INCLUSION_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/logging.h"

namespace operations_research {
namespace sat {

// Small utility class to store a vector<vector<>> where one can only append new
// vector and never change previously added ones.
//
// Note that we implement a really small subset of the vector<vector<>> API.
template <typename T>
class CompactVectorVector {
 public:
  // Same as push_back().
  // Returns the previous size() as this is convenient for how we use it.
  int Add(absl::Span<const T> data) {
    const int index = size();
    starts_.push_back(buffer_.size());
    sizes_.push_back(data.size());
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    return index;
  }

  // Same as Add() but for sat::Literal or any type from which we can get
  // indices.
  template <typename L>
  int AddLiterals(const std::vector<L>& data) {
    const int index = size();
    starts_.push_back(buffer_.size());
    sizes_.push_back(data.size());
    for (const L literal : data) {
      buffer_.push_back(literal.Index().value());
    }
    return index;
  }

  // Warning: this is only valid until the next clear() or Add() call.
  absl::Span<const T> operator[](int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, starts_.size());
    DCHECK_LT(index, sizes_.size());
    const size_t size = static_cast<size_t>(sizes_[index]);
    if (size == 0) return {};
    return {&buffer_[starts_[index]], size};
  }

  void clear() {
    starts_.clear();
    sizes_.clear();
    buffer_.clear();
  }

  size_t size() const { return starts_.size(); }

 private:
  std::vector<int> starts_;
  std::vector<int> sizes_;
  std::vector<T> buffer_;
};

// An helper class to process many sets of integer in [0, n] and detects all the
// set included in each others. This is a common operations in presolve, and
// while it can be slow the algorithm used here is pretty efficient in practice.
//
// The algorithm is based on the SAT preprocessing algorithm to detect clauses
// that subsumes others. It uses a one-watcher scheme where each subset
// candidate has only one element watched. To identify all potential subset of a
// superset, one need to inspect the watch list for all element of the superset
// candidate.
//
// The number n will be detected automatically but we allocate various vector
// of size n, so avoid having large integer values in your sets.
//
// All set contents will be accessed via storage_[index]. And of course Storage
// can be the CompactVectorVector defined above. But it can also be something
// that return a class that support .size() and integer range iteration over the
// element in the set on the fly.
template <class Storage>
class InclusionDetector {
 public:
  explicit InclusionDetector(const Storage& storage) : storage_(storage) {}

  // Resets the class to an empty state.
  void Reset() {
    num_potential_subsets_ = 0;
    num_potential_supersets_ = 0;
    candidates_.clear();
  }

  // Adds a candidate set to consider for the next DetectInclusions() call.
  // The argument is an index that will only be used via storage_[index] to get
  // the content of the candidate set.
  //
  // Note that set with no element are just ignored and will never be returned
  // as part of an inclusion.
  void AddPotentialSubset(int index);
  void AddPotentialSuperset(int index);
  void AddPotentialSet(int index);

  // By default we will detect all inclusions. It is possible to make sure we
  // don't do more than O(work_limit) operations and eventually abort early by
  // setting this. Note that we don't reset it on Reset().
  //
  // This is needed, because for m candidates of size n, we can have O(m ^ 2)
  // inclusions, each requiring O(n) work to check.
  void SetWorkLimit(uint64_t work_limit) { work_limit_ = work_limit; }

  // Finds all subset included in a superset and call "process" on each of the
  // detected inclusion. The std::function argument corresponds to indices
  // passed to the Add*() calls.
  //
  // The order of detection will be by increasing superset size. For superset
  // with the same size, the order will be deterministic but not specified. And
  // similarly, for a given superset, the order of the included subsets is
  // deterministic but not specified.
  //
  // Note that only the candidate marked as such can be a subset/superset.
  // For the candidate than can be both and are duplicates (i.e. same set), only
  // one pair will be returned. We will also never return identity inclusion and
  // we always have subset != superset.
  void DetectInclusions(
      const std::function<void(int subset, int superset)>& process);

  // Function that should only be used from within "process()".
  // Returns the bitset corresponsing to the elements of the current superset
  // passed to the process() function.
  const std::vector<bool> IsInSuperset() const { return is_in_superset_; }

  // Function that should only be used from within "process()".
  // Stop will abort the current search. The other two will cause the
  // corresponding candidate set to never appear in any future inclusion.
  void StopProcessingCurrentSubset() { stop_with_current_subset_ = true; }
  void StopProcessingCurrentSuperset() { stop_with_current_superset_ = true; }
  void Stop() {
    stop_ = true;
    signatures_.clear();
    one_watcher_.clear();
    is_in_superset_.clear();
  }

  // The algorithm here can detect many small set included in a big set while
  // only scanning the superset once. So if we do scan the superset in the
  // process function, we can do a lot more work. This is here to reuse the
  // deterministic limit mechanism.
  void IncreaseWorkDone(uint64_t increase) { work_done_ += increase; }

  // Stats.
  int num_potential_subsets() const { return num_potential_subsets_; }
  int num_potential_supersets() const { return num_potential_supersets_; }
  uint64_t work_done() const { return work_done_; }

 private:
  // Allows to access the elements of each candidates via storage_[index];
  const Storage& storage_;

  // List of candidates, this will be sorted.
  struct Candidate {
    int index;  // Storage index.
    int size;

    // For identical sizes, we need this order for correctness
    // 0: subset only, 1: both, 2: superset only.
    int order = 1;

    bool CanBeSubset() const { return order <= 1; }
    bool CanBeSuperset() const { return order >= 1; }

    // We use this with stable_sort, so no need to add the index.
    bool operator<(const Candidate& other) const {
      return std::tie(size, order) < std::tie(other.size, other.order);
    }
  };
  std::vector<Candidate> candidates_;

  int num_potential_subsets_ = 0;
  int num_potential_supersets_ = 0;
  uint64_t work_done_ = 0;
  uint64_t work_limit_ = std::numeric_limits<uint64_t>::max();

  // Temporary data only used by DetectInclusions().
  bool stop_;
  bool stop_with_current_subset_;
  bool stop_with_current_superset_;
  std::vector<uint64_t> signatures_;
  std::vector<std::vector<int>> one_watcher_;  // Index in candidates_.
  std::vector<bool> is_in_superset_;
};

// Deduction guide.
template <typename Storage>
InclusionDetector(const Storage& storage) -> InclusionDetector<Storage>;

template <typename Storage>
inline void InclusionDetector<Storage>::AddPotentialSet(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.size());
  const int num_elements = storage_[index].size();
  if (num_elements == 0) return;

  ++num_potential_subsets_;
  ++num_potential_supersets_;
  candidates_.push_back({index, num_elements, /*order=*/1});
}

template <typename Storage>
inline void InclusionDetector<Storage>::AddPotentialSubset(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.size());
  const int num_elements = storage_[index].size();
  if (num_elements == 0) return;

  ++num_potential_subsets_;
  candidates_.push_back({index, num_elements, /*order=*/0});
}

template <typename Storage>
inline void InclusionDetector<Storage>::AddPotentialSuperset(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.size());
  const int num_elements = storage_[index].size();
  if (num_elements == 0) return;

  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.size());
  ++num_potential_supersets_;
  candidates_.push_back({index, num_elements, /*order=*/2});
}

template <typename Storage>
inline void InclusionDetector<Storage>::DetectInclusions(
    const std::function<void(int subset, int superset)>& process) {
  // No need to do any work in these cases.
  if (candidates_.size() <= 1) return;
  if (num_potential_subsets_ == 0) return;
  if (num_potential_supersets_ == 0) return;

  // Temp data must be ready to use.
  stop_ = false;
  DCHECK(is_in_superset_.empty());
  DCHECK(signatures_.empty());
  DCHECK(one_watcher_.empty());

  // Main algo.
  work_done_ = 0;
  std::stable_sort(candidates_.begin(), candidates_.end());
  for (const Candidate& candidate : candidates_) {
    const auto& candidate_elements = storage_[candidate.index];
    const int candidate_index = signatures_.size();

    // Compute the signature and also resize vector if needed. We want a
    // signature that is order invariant and is compatible with inclusion.
    uint64_t signature = 0;
    int max_element = 0;
    for (const int e : candidate_elements) {
      DCHECK_GE(e, 0);
      max_element = std::max(max_element, e);
      signature |= (int64_t{1} << (e & 63));
    }
    DCHECK_EQ(is_in_superset_.size(), one_watcher_.size());
    if (max_element >= is_in_superset_.size()) {
      is_in_superset_.resize(max_element + 1, false);
      one_watcher_.resize(max_element + 1);
    }
    signatures_.push_back(signature);

    stop_with_current_superset_ = false;
    if (candidate.CanBeSuperset()) {
      const Candidate& superset = candidate;
      const auto& superset_elements = candidate_elements;

      // Bitset should be cleared.
      DCHECK(std::all_of(is_in_superset_.begin(), is_in_superset_.end(),
                         [](bool b) { return !b; }));

      // Find any subset included in current superset.
      work_done_ += 2 * superset.size;
      if (work_done_ > work_limit_) return Stop();
      for (const int e : superset_elements) {
        is_in_superset_[e] = true;
      }

      const uint64_t superset_signature = signatures_.back();
      for (const int superset_e : superset_elements) {
        for (int i = 0; i < one_watcher_[superset_e].size(); ++i) {
          const int c_index = one_watcher_[superset_e][i];
          const Candidate& subset = candidates_[c_index];
          DCHECK_LE(subset.size, superset.size);

          // Quick check with signature.
          if ((signatures_[c_index] & ~superset_signature) != 0) continue;

          // Long check with bitset.
          bool is_included = true;
          work_done_ += subset.size;
          if (work_done_ > work_limit_) return Stop();
          for (const int subset_e : storage_[subset.index]) {
            if (!is_in_superset_[subset_e]) {
              is_included = false;
              break;
            }
          }
          if (!is_included) continue;

          stop_with_current_subset_ = false;
          process(subset.index, superset.index);

          if (stop_) return;
          if (work_done_ > work_limit_) return Stop();

          if (stop_with_current_subset_) {
            // Remove from the watcher list.
            std::swap(one_watcher_[superset_e][i],
                      one_watcher_[superset_e].back());
            one_watcher_[superset_e].pop_back();
            --i;
          }
          if (stop_with_current_superset_) break;
        }
        if (stop_with_current_superset_) break;
      }

      // Cleanup.
      for (const int e : superset_elements) {
        is_in_superset_[e] = false;
      }
    }

    // Add new subset candidate to the watchers.
    //
    // Tricky: If this was also a superset and has been removed, we don't want
    // to watch it!
    if (candidate.CanBeSubset() && !stop_with_current_superset_) {
      // Choose to watch the one with smallest list.
      int best_choice = -1;
      work_done_ += candidate.size;
      if (work_done_ > work_limit_) return Stop();
      for (const int e : candidate_elements) {
        DCHECK_GE(e, 0);
        DCHECK_LT(e, one_watcher_.size());
        if (best_choice == -1 ||
            one_watcher_[e].size() < one_watcher_[best_choice].size()) {
          best_choice = e;
        }
      }
      DCHECK_NE(best_choice, -1);
      one_watcher_[best_choice].push_back(candidate_index);
    }
  }

  // Stop also performs some cleanup.
  Stop();
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INCLUSION_H_
