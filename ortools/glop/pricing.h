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

#ifndef OR_TOOLS_GLOP_PRICING_H_
#define OR_TOOLS_GLOP_PRICING_H_

#include <random>
#include <string>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// Maintains a set of elements in [0, n), each with an associated value and
// allows to query the element of maximum value efficiently.
//
// This is optimized for use in the pricing step of the simplex algorithm.
// Basically at each simplex iterations, you want to:
//
// 1/ Get the candidate with the maximum value. The number of candidates
//    can be close to n, or really small. You also want some randomization if
//    several elements have an equivalent (maximum) value.
//
// 2/ Update the set of candidate and their values, where the number of update
//    is usually a lot smaller than n. Note that in some corner cases, there are
//    two "updates" phases, so a position can be updated twice.
//
// The idea is to be faster than O(num_candidates) per GetMaximum(), most of the
// time. All updates should be in O(1) with as little overhead as possible. The
// algorithm here dynamically maintain the top-k (for k=32) with best effort and
// use it instead of doing a O(num_candidates) scan when possible.
//
// Note that when O(num_updates) << n, this can have a huge effect. A basic O(1)
// per update, O(num_candidates) per maximum query was taking around 60% of the
// total time on graph40-80-1rand.pb.gz ! with the top-32 algo coded here, it is
// around 3%, and the number of "fast" GetMaximum() that hit the top-k heap on
// the first 120s of that problem was 250757 / 255659. Note that n was 282624 in
// this case, which is not even the biggest size we can tackle.
//
// Note(user): This could be moved to util/ as a general class if someone wants
// to reuse it, it is however tunned for use in Glop pricing step and might
// becomes even more specific in the future.
template <typename Index>
class DynamicMaximum {
 public:
  // To simplify the APIs, we take a random number generator at construction.
  explicit DynamicMaximum(absl::BitGenRef random) : random_(random) {}

  // Prepares the class to hold up to n candidates with indices in [0, n).
  // Initially no indices is a candidate.
  void ClearAndResize(Index n);

  // Returns the index with the maximum value or Index(-1) if the set is empty
  // and there is no possible candidate. If there are more than one candidate
  // with the same maximum value, this will return a random one (not always
  // uniformly if there is a large number of ties).
  Index GetMaximum();

  // Removes the given index from the set of candidates.
  void Remove(Index position);

  // Adds an element to the set of candidate and sets its value. If the element
  // is already present, this updates its value. The value must be finite.
  void AddOrUpdate(Index position, Fractional value);

  // Optimized version of AddOrUpdate() for the dense case. If one knows that
  // there will be O(n) updates, it is possible to call StartDenseUpdates() and
  // then use DenseAddOrUpdate() instead of AddOrUpdate() which is slighlty
  // faster.
  //
  // Note that calling AddOrUpdate() will still works fine, but will cause an
  // extra test per call.
  void StartDenseUpdates();
  void DenseAddOrUpdate(Index position, Fractional value);

  // Returns the current size n that was used in the last ClearAndResize().
  void Clear() { ClearAndResize(Index(0)); }
  Index Size() const { return values_.size(); }

  // Returns some stats about this class if they are enabled.
  std::string StatString() const { return stats_.StatString(); }

 private:
  // Adds an elements to the set of top elements.
  void UpdateTopK(Index position, Fractional value);

  // Returns a random element from the set {best} U {equivalent_choices_}.
  // If equivalent_choices_ is empty, this just returns best.
  Index RandomizeIfManyChoices(Index best);

  // For tie-breaking.
  absl::BitGenRef random_;
  std::vector<Index> equivalent_choices_;

  // Set of candidates and their value.
  // Note that if is_candidate_[index] is false, values_[index] can be anything.
  StrictITIVector<Index, Fractional> values_;
  Bitset64<Index> is_candidate_;

  // We maintain the top-k current candidates for a fixed k. Note that not all
  // entries in tops_ are necessary up to date since we don't remove elements.
  // There can even be duplicate elements inside if Update() add an element
  // already inside. This is fine, since tops_ will be recomputed as soon as we
  // can't get the true maximum from there.
  //
  // The invariant is that:
  // - All elements > threshold_ are in tops_.
  // - All elements not in tops have a value <= threshold_.
  // - elements == threshold_ can be in or out.
  //
  // In particular, the threshold only increase until the heap becomes empty and
  // is recomputed from scratch by GetMaximum().
  struct HeapElement {
    HeapElement() {}
    HeapElement(Index i, Fractional v) : index(i), value(v) {}

    Index index;
    Fractional value;

    // We want a min-heap: tops_.top() actually represents the k-th value, not
    // the max.
    const double operator<(const HeapElement& other) const {
      return value > other.value;
    }
  };
  Fractional threshold_;
  std::vector<HeapElement> tops_;

  // Statistics about the class.
  struct QueryStats : public StatsGroup {
    QueryStats()
        : StatsGroup("PricingStats"),
          get_maximum("get_maximum", this),
          heap_size_on_hit("heap_size_on_hit", this),
          random_choices("random_choices", this) {}
    TimeDistribution get_maximum;
    IntegerDistribution heap_size_on_hit;
    IntegerDistribution random_choices;
  };
  QueryStats stats_;
};

template <typename Index>
inline void DynamicMaximum<Index>::ClearAndResize(Index n) {
  tops_.clear();
  threshold_ = -kInfinity;
  values_.resize(n);
  is_candidate_.ClearAndResize(n);
}

template <typename Index>
inline void DynamicMaximum<Index>::Remove(Index position) {
  is_candidate_.Clear(position);
}

template <typename Index>
inline void DynamicMaximum<Index>::StartDenseUpdates() {
  // This disable tops_ until the next GetMaximum().
  tops_.clear();
  threshold_ = kInfinity;
}

template <typename Index>
inline void DynamicMaximum<Index>::DenseAddOrUpdate(Index position,
                                                    Fractional value) {
  DCHECK(IsFinite(value));
  DCHECK(tops_.empty());
  is_candidate_.Set(position);
  values_[position] = value;
}

template <typename Index>
inline void DynamicMaximum<Index>::AddOrUpdate(Index position,
                                               Fractional value) {
  DCHECK(IsFinite(value));
  is_candidate_.Set(position);
  values_[position] = value;
  if (value >= threshold_) UpdateTopK(position, value);
}

template <typename Index>
inline Index DynamicMaximum<Index>::RandomizeIfManyChoices(Index best) {
  if (equivalent_choices_.empty()) return best;
  equivalent_choices_.push_back(best);
  stats_.random_choices.Add(equivalent_choices_.size());

  return equivalent_choices_[std::uniform_int_distribution<int>(
      0, equivalent_choices_.size() - 1)(random_)];
}

template <typename Index>
inline Index DynamicMaximum<Index>::GetMaximum() {
  SCOPED_TIME_STAT(&stats_);
  Fractional best_value = -kInfinity;
  Index best_position(-1);
  equivalent_choices_.clear();

  // Optimized version if the maximum is in tops_ already.
  //
  // We do two things here:
  // 1/ Filter tops_ to only contain valid entries. This is because we never
  //    remove element, so the value of one of the element in tops might have
  //    decreased now. Note that we leave threshold_ untouched, so it
  //    can actually be lower than the minimum of the element in tops.
  // 2/ Get the maximum of the valid elements.
  if (!tops_.empty()) {
    int new_size = 0;
    for (const HeapElement e : tops_) {
      // The two possible sources of "invalidity".
      if (!is_candidate_[e.index]) continue;
      if (values_[e.index] != e.value) continue;

      tops_[new_size++] = e;
      if (e.value >= best_value) {
        if (e.value == best_value) {
          equivalent_choices_.push_back(e.index);
          continue;
        }
        equivalent_choices_.clear();
        best_value = e.value;
        best_position = e.index;
      }
    }
    tops_.resize(new_size);
    if (new_size != 0) {
      stats_.heap_size_on_hit.Add(new_size);
      return RandomizeIfManyChoices(best_position);
    }
  }

  // We need to iterate over all the candidates.
  threshold_ = -kInfinity;
  DCHECK(tops_.empty());
  for (const Index position : is_candidate_) {
    const Fractional value = values_[position];

    // TODO(user): Add a mode when we do not maintain the TopK for small sizes
    // (like n < 1000) ? The gain might not be worth the extra code though.
    if (value < threshold_) continue;
    UpdateTopK(position, value);

    if (value >= best_value) {
      if (value == best_value) {
        equivalent_choices_.push_back(position);
        continue;
      }
      equivalent_choices_.clear();
      best_value = value;
      best_position = position;
    }
  }

  return RandomizeIfManyChoices(best_position);
}

template <typename Index>
inline void DynamicMaximum<Index>::UpdateTopK(Index position,
                                              Fractional value) {
  // Note that this should only be called when an update is required.
  DCHECK_GE(value, threshold_);

  // We use a compile time size of the form 2^n - 1 to have a full binary heap.
  //
  // TODO(user): Adapt the size depending on the problem size? Note sure it is
  // worth it. To experiment more.
  constexpr int k = 31;
  static_assert(((k + 1) & k) == 0, "k + 1 should be a power of 2.");

  // Simply grow the vector until we hit a size of k.
  if (tops_.size() < k) {
    tops_.emplace_back(position, value);
    if (tops_.size() == k) {
      std::make_heap(tops_.begin(), tops_.end());
      threshold_ = tops_[0].value;
    }
    return;
  }

  // If the value is equal, we randomly replace it. Having some randomness can
  // also be important to increase the chance of keeping the true maximum in the
  // top k set.
  //
  // TODO(user): use proper probability by counting the number of ties seen and
  // replacing a random minimum element to get an uniform distribution? Note
  // that it will never be truly uniform since once the top k structure is
  // constructed, we will reuse it as much as possible, so it will be biased
  // towards elements already inside.
  if (value == tops_[0].value) {
    if (absl::Bernoulli(random_, 0.5)) {
      tops_[0].index = position;
    }
    return;
  }

  // The code below is basically a custom implementation of this. It is however
  // only slighlty faster for such a small heap. So it might not be completely
  // worth it.
  if (/*DISABLES CODE*/ (false)) {
    std::pop_heap(tops_.begin(), tops_.end());
    tops_.back() = HeapElement(position, value);
    std::push_heap(tops_.begin(), tops_.end());
    threshold_ = tops_[0].value;
    return;
  }

  // To not have to do std::pop_heap() and then std::push_heap(), we code our
  // own update. Note that we exploit the fact that k is of the form 2^n - 1 to
  // save one test per update.
  int i = 0;
  DCHECK_EQ(tops_.size(), k);
  constexpr int limit = k / 2;
  for (; i < limit;) {
    const int left_child = 2 * i + 1;
    const int right_child = left_child + 1;
    const Fractional l_value = tops_[left_child].value;
    const Fractional r_value = tops_[right_child].value;
    if (l_value > r_value) {
      if (value <= r_value) break;
      tops_[i] = tops_[right_child];
      i = right_child;
    } else {
      if (value <= l_value) break;
      tops_[i] = tops_[left_child];
      i = left_child;
    }
  }
  tops_[i] = HeapElement(position, value);
  threshold_ = tops_[0].value;
  DCHECK(std::is_heap(tops_.begin(), tops_.end()));
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_PRICING_H_
