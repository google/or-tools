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

#ifndef ORTOOLS_SAT_INCLUSION_H_
#define ORTOOLS_SAT_INCLUSION_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

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
// All set contents will be accessed via storage_[index].
// This can be used with a vector<vector<>> or our CompactVectorVector that we
// use in a few place. But it can also be anything that support:
// - storage_.size()
// - range iteration over storage_[index]
// - storage_[index].size()
template <class Storage>
class InclusionDetector {
 public:
  InclusionDetector(const Storage& storage, TimeLimit* time_limit)
      : storage_(storage), time_limit_(time_limit) {}

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
  // Stop will abort the current search. The other two will cause the
  // corresponding candidate set to never appear in any future inclusion.
  void StopProcessingCurrentSubset() { stop_with_current_subset_ = true; }
  void StopProcessingCurrentSuperset() { stop_with_current_superset_ = true; }
  void Stop() {
    stop_ = true;
    signatures_.clear();
    one_watcher_.clear();
    is_in_superset_.resize(0);
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
  bool Stopped() const { return stop_; }

 private:
  // Allows to access the elements of each candidates via storage_[index];
  const Storage& storage_;

  TimeLimit* time_limit_;

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

  bool stop_ = false;
  bool stop_with_current_subset_ = false;
  bool stop_with_current_superset_ = false;
  std::vector<uint64_t> signatures_;
  std::vector<std::vector<int>> one_watcher_;  // Index in candidates_.
  std::vector<int> superset_elements_;
  Bitset64<int> is_in_superset_;
};

// Similar API and purpose to InclusionDetector. But this one is a bit simpler
// and faster if it fit your usage. This assume an initial given set of
// potential subsets, that will be queried against supersets one by one.
template <class Storage>
class SubsetsDetector {
 public:
  SubsetsDetector(const Storage& storage, TimeLimit* time_limit)
      : storage_(storage), time_limit_(time_limit) {}

  void SetWorkLimit(uint64_t work_limit) { work_limit_ = work_limit; }
  void StopProcessingCurrentSubset() { stop_with_current_subset_ = true; }
  void StopProcessingCurrentSuperset() { stop_with_current_superset_ = true; }
  void Stop() {
    stop_ = true;
    one_watcher_.clear();
    is_in_superset_.resize(0);
  }

  uint64_t work_done() const { return work_done_; }
  bool Stopped() const { return stop_; }

  // Different API than InclusionDetector.
  // 1/ Add all potential subset to the storage_.
  // 2/ Call IndexAllStorageAsSubsets()
  // 3/ Call one or more time FindSubsets().
  //    - process() can call StopProcessingCurrentSuperset() to abort early
  //    - process() can call StopProcessingCurrentSubset() to never consider
  //      that subset again.
  // 4/ Call Stop() to reclaim some memory.
  //
  // Optimization: next_index_to_try is an index in superset that can be used
  // to skip some position for which we already called FindSubsets().
  void IndexAllStorageAsSubsets();
  void FindSubsets(absl::Span<const int> superset, int* next_index_to_try,
                   const std::function<void(int subset)>& process);

 private:
  // Allows to access the elements of each subsets via storage_[index];
  const Storage& storage_;

  TimeLimit* time_limit_;
  uint64_t work_done_ = 0;
  uint64_t work_limit_ = std::numeric_limits<uint64_t>::max();

  struct OneWatcherData {
    int index;
    int other_element;
    uint64_t signature;
  };

  bool stop_ = false;
  bool stop_with_current_subset_ = false;
  bool stop_with_current_superset_ = false;
  CompactVectorVector<int, OneWatcherData> one_watcher_;
  Bitset64<int> is_in_superset_;
};

// Deduction guide.
template <typename Storage>
InclusionDetector(const Storage& storage) -> InclusionDetector<Storage>;

template <typename Storage>
SubsetsDetector(const Storage& storage) -> SubsetsDetector<Storage>;

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

// Compute the signature and the maximum element. We want a
// signature that is order invariant and is compatible with inclusion.
inline std::pair<uint64_t, int> ComputeSignatureAndMaxElement(
    absl::Span<const int> elements) {
  uint64_t signature = 0;
  int max_element = 0;
  for (const int e : elements) {
    DCHECK_GE(e, 0);
    max_element = std::max(max_element, e);
    signature |= (int64_t{1} << (e & 63));
  }
  return {signature, max_element};
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
  DCHECK(signatures_.empty());
  DCHECK(one_watcher_.empty());

  // We check each time our work_done_ has increased by more than this.
  constexpr int64_t kCheckTimeLimitInterval = 1000;
  int64_t next_time_limit_check = kCheckTimeLimitInterval;

  // Main algo.
  work_done_ = 0;
  std::stable_sort(candidates_.begin(), candidates_.end());
  for (const Candidate& candidate : candidates_) {
    const auto& candidate_elements = storage_[candidate.index];
    const int candidate_index = signatures_.size();

    const auto [signature, max_element] =
        ComputeSignatureAndMaxElement(candidate_elements);
    signatures_.push_back(signature);
    DCHECK_EQ(is_in_superset_.size(), one_watcher_.size());
    if (max_element >= is_in_superset_.size()) {
      is_in_superset_.resize(max_element + 1);
      one_watcher_.resize(max_element + 1);
    }

    stop_with_current_superset_ = false;
    if (candidate.CanBeSuperset()) {
      const Candidate& superset = candidate;

      // Bitset should be cleared.
      DCHECK(std::all_of(is_in_superset_.begin(), is_in_superset_.end(),
                         [](bool b) { return !b; }));

      // Find any subset included in current superset.
      work_done_ += 2 * superset.size;
      if (work_done_ > work_limit_) return Stop();
      if (work_done_ > next_time_limit_check) {
        if (time_limit_->LimitReached()) return Stop();
        next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
      }

      // We make a copy because process() might alter the content of the
      // storage when it returns "stop_with_current_superset_" and we need
      // to clean is_in_superset_ properly.
      //
      // TODO(user): Alternatively, we could clean is_in_superset_ in the
      // call to StopProcessingCurrentSuperset() and force client to call it
      // before altering the superset content.
      superset_elements_.assign(candidate_elements.begin(),
                                candidate_elements.end());
      for (const int e : superset_elements_) {
        is_in_superset_.Set(e);
      }

      const uint64_t superset_signature = signatures_.back();
      const auto is_in_superset_view = is_in_superset_.const_view();
      for (const int superset_e : superset_elements_) {
        work_done_ += one_watcher_[superset_e].size();
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
          if (work_done_ > next_time_limit_check) {
            if (time_limit_->LimitReached()) return Stop();
            next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
          }
          for (const int subset_e : storage_[subset.index]) {
            if (!is_in_superset_view[subset_e]) {
              is_included = false;
              break;
            }
          }
          if (!is_included) continue;

          stop_with_current_subset_ = false;
          process(subset.index, superset.index);

          if (stop_) return;
          if (work_done_ > work_limit_) return Stop();
          if (work_done_ > next_time_limit_check) {
            if (time_limit_->LimitReached()) return Stop();
            next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
          }

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
      for (const int e : superset_elements_) {
        is_in_superset_.ClearBucket(e);
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

template <typename Storage>
inline void SubsetsDetector<Storage>::IndexAllStorageAsSubsets() {
  stop_ = false;

  // Flat representation of one_watcher_, we will fill it in one go from there.
  std::vector<int> tmp_keys;
  std::vector<OneWatcherData> tmp_values;
  std::vector<int> element_to_num_watched;

  work_done_ = 0;
  for (int index = 0; index < storage_.size(); ++index) {
    const auto& subset = storage_[index];
    CHECK_GE(subset.size(), 2);

    const auto [signature, max_element] = ComputeSignatureAndMaxElement(subset);
    if (max_element >= is_in_superset_.size()) {
      is_in_superset_.resize(max_element + 1);
    }
    if (max_element >= element_to_num_watched.size()) {
      element_to_num_watched.resize(max_element + 1);
    }

    // Choose to watch the one with smallest list so far.
    int best_choice = -1;
    int best_value = -1;
    int second_choice = -1;
    int second_value = -1;
    work_done_ += subset.size();
    if (work_done_ > work_limit_) return Stop();
    for (const int e : subset) {
      DCHECK_GE(e, 0);
      DCHECK_LT(e, element_to_num_watched.size());
      const int value = element_to_num_watched[e];
      if (value >= best_value) {
        second_choice = best_choice;
        second_value = best_value;
        best_choice = e;
        best_value = value;
      } else if (value > second_value) {
        second_choice = e;
        second_value = value;
      }
    }
    DCHECK_NE(best_choice, -1);
    DCHECK_NE(second_choice, -1);
    DCHECK_NE(best_choice, second_choice);

    element_to_num_watched[best_choice]++;
    tmp_keys.push_back(best_choice);
    tmp_values.push_back({index, second_choice, signature});
  }

  one_watcher_.ResetFromFlatMapping(tmp_keys, tmp_values);
}

template <typename Storage>
inline void SubsetsDetector<Storage>::FindSubsets(
    absl::Span<const int> superset, int* next_index_to_try,
    const std::function<void(int subset)>& process) {
  // We check each time our work_done_ has increased by more than this.
  constexpr int64_t kCheckTimeLimitInterval = 1000;
  int64_t next_time_limit_check = kCheckTimeLimitInterval;

  // Compute the signature and also resize vector if needed. We want a
  // signature that is order invariant and is compatible with inclusion.
  const auto [superset_signature, max_element] =
      ComputeSignatureAndMaxElement(superset);
  if (max_element >= is_in_superset_.size()) {
    is_in_superset_.resize(max_element + 1);
  }

  // Find any subset included in current superset.
  work_done_ += 2 * superset.size();
  if (work_done_ > work_limit_) return Stop();
  if (work_done_ > next_time_limit_check) {
    if (time_limit_->LimitReached()) return Stop();
    next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
  }

  // Bitset should be cleared.
  DCHECK(std::all_of(is_in_superset_.begin(), is_in_superset_.end(),
                     [](bool b) { return !b; }));
  for (const int e : superset) {
    is_in_superset_.Set(e);
  }

  stop_with_current_superset_ = false;
  const auto is_in_superset_view = is_in_superset_.const_view();
  for (; *next_index_to_try < superset.size(); ++*next_index_to_try) {
    const int superset_e = superset[*next_index_to_try];
    if (superset_e >= one_watcher_.size()) continue;
    auto cached_span = one_watcher_[superset_e];
    for (int i = 0; i < cached_span.size(); ++i) {
      ++work_done_;

      // Do a bunch of quick checks. The second one is optimized for size 2
      // which happens a lot in our usage of merging clique with implications.
      const auto [subset_index, other_e, subset_signature] = cached_span[i];
      if ((subset_signature & ~superset_signature) != 0) continue;
      if (!is_in_superset_view[other_e]) continue;

      // Long check with bitset.
      const absl::Span<const int> subset = storage_[subset_index];
      if (subset.size() > superset.size()) continue;

      // TODO(user): Technically we do not need to check the watched position or
      // the "other element" position, we could do that by permuting them first
      // or last and iterating on a subspan. However, in many slow situation, we
      // have millions of size 2 sets, and the time is dominated by the first
      // check.
      bool is_included = true;
      work_done_ += subset.size();
      if (work_done_ > work_limit_) return Stop();
      if (work_done_ > next_time_limit_check) {
        if (time_limit_->LimitReached()) return Stop();
        next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
      }
      for (const int subset_e : subset) {
        if (!is_in_superset_view[subset_e]) {
          is_included = false;
          break;
        }
      }
      if (!is_included) continue;

      stop_with_current_subset_ = false;
      process(subset_index);

      // TODO(user): Remove this and the more complex API need once we move
      // class.
      if (stop_) return;
      if (work_done_ > work_limit_) return Stop();
      if (work_done_ > next_time_limit_check) {
        if (time_limit_->LimitReached()) return Stop();
        next_time_limit_check = work_done_ + kCheckTimeLimitInterval;
      }

      if (stop_with_current_subset_) {
        one_watcher_.RemoveBySwap(superset_e, i);
        cached_span.remove_suffix(1);
        --i;
      }
      if (stop_with_current_superset_) break;
    }
    if (stop_with_current_superset_) break;
  }

  // Cleanup.
  for (const int e : superset) {
    is_in_superset_.ClearBucket(e);
  }
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_INCLUSION_H_
