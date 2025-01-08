// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_
#define OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/types/span.h"
#include "ortools/base/constant_divisor.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

struct OrthogonalPackingOptions {
  bool use_pairwise = true;
  bool use_dff_f0 = true;
  bool use_dff_f2 = true;
  int brute_force_threshold = 6;
  int dff2_max_number_of_parameters_to_check = std::numeric_limits<int>::max();
};

class OrthogonalPackingResult {
 public:
  explicit OrthogonalPackingResult() : result_(Status::UNKNOWN) {}
  enum class Status {
    INFEASIBLE,
    FEASIBLE,
    UNKNOWN,
  };

  Status GetResult() const { return result_; }

  struct Item {
    // Index of the item on the original sizes_x/sizes_y input.
    int index;
    // New size for item of index `i` which is smaller or equal to the initial
    // size. The subproblem remain infeasible if every item is shrinked to its
    // new size.
    IntegerValue size_x;
    IntegerValue size_y;
  };
  const std::vector<Item>& GetItemsParticipatingOnConflict() const {
    return items_participating_on_conflict_;
  }

  bool HasSlack() const { return slack_ > IntegerValue(0); }

  enum class Coord { kCoordX, kCoordY };
  // Use an eventual slack to reduce the size of item corresponding to the
  // `i`-th element on GetItemsParticipatingOnConflict(). It will not use any
  // slack to reduce it beyond lower_bound. This is a no-op if HasSlack() is
  // false.
  bool TryUseSlackToReduceItemSize(int i, Coord coord,
                                   IntegerValue lower_bound = 0);

  // If *this is identical or not easily comparable to other, returns false.
  bool IsBetterThan(const OrthogonalPackingResult& other) const {
    if (result_ == Status::UNKNOWN && other.result_ == Status::UNKNOWN) {
      return false;
    }
    if (other.result_ == Status::UNKNOWN) {
      return true;
    }
    if (result_ == Status::UNKNOWN) {
      return false;
    }
    if (other.result_ == Status::FEASIBLE) {
      CHECK(result_ != Status::INFEASIBLE);
      return result_ == Status::FEASIBLE;
    }

    // other.result_ == Status::INFEASIBLE
    CHECK(result_ == Status::INFEASIBLE);
    if (other.items_participating_on_conflict_.size() <
        items_participating_on_conflict_.size()) {
      return false;
    }
    if (other.items_participating_on_conflict_.size() >
        items_participating_on_conflict_.size()) {
      return true;
    }

    IntegerValue total_area_this = 0;
    IntegerValue total_area_other = 0;
    for (int i = 0; i < items_participating_on_conflict_.size(); ++i) {
      total_area_this += items_participating_on_conflict_[i].size_x *
                         items_participating_on_conflict_[i].size_y;
      total_area_other += other.items_participating_on_conflict_[i].size_x *
                          other.items_participating_on_conflict_[i].size_y;
    }
    return total_area_this - slack_ < total_area_other - other.slack_;
  }

 private:
  friend class OrthogonalPackingInfeasibilityDetector;

  explicit OrthogonalPackingResult(Status result) : result_(result) {}

  enum class ConflictType {
    NO_CONFLICT,
    TRIVIAL,
    PAIRWISE,
    DFF_F0,
    DFF_F2,
    BRUTE_FORCE,
  };

  Status result_;
  ConflictType conflict_type_ = ConflictType::NO_CONFLICT;
  IntegerValue slack_ = 0;
  std::vector<Item> items_participating_on_conflict_;
};

// Class for solving the orthogonal packing problem when it can be done
// efficiently (ie., not applying any heuristic slower than O(N^2)).
class OrthogonalPackingInfeasibilityDetector {
 public:
  explicit OrthogonalPackingInfeasibilityDetector(
      absl::BitGenRef random, SharedStatistics* shared_stats)
      : random_(random), shared_stats_(shared_stats) {}

  ~OrthogonalPackingInfeasibilityDetector();

  OrthogonalPackingResult TestFeasibility(
      absl::Span<const IntegerValue> sizes_x,
      absl::Span<const IntegerValue> sizes_y,
      std::pair<IntegerValue, IntegerValue> bounding_box_size,
      const OrthogonalPackingOptions& options = OrthogonalPackingOptions());

 private:
  bool RelaxConflictWithBruteForce(
      OrthogonalPackingResult& result,
      std::pair<IntegerValue, IntegerValue> bounding_box_size,
      int brute_force_threshold);

  OrthogonalPackingResult TestFeasibilityImpl(
      absl::Span<const IntegerValue> sizes_x,
      absl::Span<const IntegerValue> sizes_y,
      std::pair<IntegerValue, IntegerValue> bounding_box_size,
      const OrthogonalPackingOptions& options);

  OrthogonalPackingResult GetDffConflict(
      absl::Span<const IntegerValue> sizes_x,
      absl::Span<const IntegerValue> sizes_y,
      absl::Span<const int> index_by_decreasing_x_size,
      absl::Span<const IntegerValue> g_x, IntegerValue g_max,
      IntegerValue x_bb_size, IntegerValue total_energy, IntegerValue bb_area,
      IntegerValue* best_k);

  // Returns a minimum set of values of the parameter `k` of f_2^k that is
  // sufficient to find a conflict. This function runs in
  // O(num_items * sqrt(bb_size)) operations.
  // All sizes must be positive values less than UINT16_MAX.
  // The returned bitset will contain less elements than
  // min(sqrt_bb_size * num_items, x_bb_size/4+1).
  void GetAllCandidatesForKForDff2(absl::Span<const IntegerValue> sizes,
                                   IntegerValue bb_size,
                                   IntegerValue sqrt_bb_size,
                                   Bitset64<IntegerValue>& candidates);

  OrthogonalPackingResult CheckFeasibilityWithDualFunction2(
      absl::Span<const IntegerValue> sizes_x,
      absl::Span<const IntegerValue> sizes_y,
      absl::Span<const int> index_by_decreasing_x_size, IntegerValue x_bb_size,
      IntegerValue y_bb_size, int max_number_of_parameters_to_check);

  // Buffers cleared and reused at each call of TestFeasibility()
  std::vector<int> index_by_decreasing_x_size_;
  std::vector<int> index_by_decreasing_y_size_;
  std::vector<std::pair<IntegerValue, IntegerValue>> scheduling_profile_;
  std::vector<std::pair<IntegerValue, IntegerValue>> new_scheduling_profile_;

  int64_t num_calls_ = 0;
  int64_t num_conflicts_ = 0;
  int64_t num_conflicts_two_items_ = 0;
  int64_t num_trivial_conflicts_ = 0;
  int64_t num_conflicts_dff2_ = 0;
  int64_t num_conflicts_dff0_ = 0;
  int64_t num_scheduling_possible_ = 0;
  int64_t num_brute_force_calls_ = 0;
  int64_t num_brute_force_conflicts_ = 0;
  int64_t num_brute_force_relaxation_ = 0;

  absl::BitGenRef random_;
  SharedStatistics* shared_stats_;
};

// If we have a container of size `C` and a parameter `k` taking values in
// [0, C/2], the Dual Feasible Function often named `f_0^k(x)` is equivalent to
// the operation of removing all values of size less than `k`, and symmetrically
// increasing to `C` the size of the large values. It is defined as:
//
//            / C, if x > C - k,
// f_0^k(x) = | x, if k <= x <= C - k,
//            \ 0, if x < k.
//
// This is a Maximal DFF. See for example [1] for some discussion about it.
//
// [1] Clautiaux, François, Cláudio Alves, and José Valério de Carvalho. "A
// survey of dual-feasible and superadditive functions." Annals of Operations
// Research 179 (2010): 317-342.
class DualFeasibleFunctionF0 {
 public:
  DualFeasibleFunctionF0(IntegerValue max_x, IntegerValue k)
      : k_(k), max_x_(max_x) {
    DCHECK_GE(k_, 0);
    DCHECK_LE(2 * k_, max_x_);
  }

  // `x` must be in [0, max_x].
  IntegerValue operator()(IntegerValue x) const {
    DCHECK_GE(x, 0);
    DCHECK_LE(x, max_x_);
    if (x > max_x_ - k_) {
      return max_x_;
    } else if (x < k_) {
      return 0;
    } else {
      return x;
    }
  }

  // Return the lowest integer y so that Dff(x) >= y.
  // y must be in [0, Dff(max_x)].
  IntegerValue LowestInverse(IntegerValue x) const {
    DCHECK_GE(x, 0);
    DCHECK_LE(x, max_x_);
    if (x > max_x_ - k_) {
      return max_x_ - k_ + 1;
    } else if (x == 0) {
      return 0;
    } else if (x < k_) {
      return k_;
    } else {
      return x;
    }
  }

 private:
  const IntegerValue k_;
  const IntegerValue max_x_;
};

// Dual Feasible Function based on rounding. Called f_2 on [1].
//
// The `f_2^k(x)` function for an integer x in [0, C] and a parameter `k` taking
// values in [0, C/2] is defined as:
//
//            / 2 * [ floor(C / k) - floor( (C - x) / k) ], if x > C / 2,
// f_2^k(x) = | floor(C / k), if k = C / 2,
//            \ floor(x / k),  if x < C / 2.
//
// This function is a Maximal Dual Feasible Function. Ie., it satisfies:
// - f_2 is nondecreasing,
// - f_2 is superadditive, i.e., f_2(x) + f_2(y) <= f_2(x + y),
// - f_2 is symmetric, i.e., f_2(x) + f_2(C - x) = f_2(C),
// - f_2(0) = 0.
//
// [1] Carlier, Jacques, François Clautiaux, and Aziz Moukrim. "New reduction
// procedures and lower bounds for the two-dimensional bin packing problem with
// fixed orientation." Computers & Operations Research 34.8 (2007): 2223-2250.
class RoundingDualFeasibleFunction {
 public:
  // `max_x` must fit in a uint16_t and `k` in [0, max_x/2].
  RoundingDualFeasibleFunction(IntegerValue max_x, IntegerValue k)
      : div_(k.value()), max_x_(max_x), c_k_(max_x_.value() / div_), k_(k) {
    DCHECK_GT(k, 0);
    DCHECK_LE(2 * k, max_x_);
    DCHECK_LE(max_x_, std::numeric_limits<uint16_t>::max());
  }

  // `x` must be in [0, max_x].
  IntegerValue operator()(IntegerValue x) const {
    DCHECK_GE(x, 0);
    DCHECK_LE(x, max_x_);

    if (2 * x > max_x_) {
      return 2 * (c_k_ - (max_x_.value() - x.value()) / div_);
    } else if (2 * x == max_x_) {
      return c_k_;
    } else {
      return 2 * (x.value() / div_);
    }
  }

  // Return the lowest integer y so that Dff(x) >= y.
  // y must be in [0, Dff(max_x)].
  IntegerValue LowestInverse(IntegerValue y) const;

 private:
  const ::util::math::ConstantDivisor<uint16_t> div_;
  const IntegerValue max_x_;
  const IntegerValue c_k_;
  const IntegerValue k_;
};

// Same as above for k = 2^log2_k.
class RoundingDualFeasibleFunctionPowerOfTwo {
 public:
  RoundingDualFeasibleFunctionPowerOfTwo(IntegerValue max_x,
                                         IntegerValue log2_k)
      : log2_k_(log2_k), max_x_(max_x), c_k_(max_x_ >> log2_k_) {
    DCHECK_GE(log2_k_, 0);
    DCHECK_LT(log2_k_, 63);
    DCHECK_LE(2 * (1 << log2_k), max_x_);
    DCHECK_LE(max_x_, std::numeric_limits<int64_t>::max() / 2);
  }

  IntegerValue operator()(IntegerValue x) const {
    DCHECK_GE(x, 0);
    DCHECK_LE(x, max_x_);

    if (2 * x > max_x_) {
      return 2 * (c_k_ - ((max_x_ - x) >> log2_k_));
    } else if (2 * x == max_x_) {
      return c_k_;
    } else {
      return 2 * (x >> log2_k_);
    }
  }

  // Return the lowest integer y so that Dff(x) >= y.
  // y must be in [0, Dff(max_x)].
  IntegerValue LowestInverse(IntegerValue y) const;

 private:
  const IntegerValue log2_k_;
  const IntegerValue max_x_;
  const IntegerValue c_k_;
};

// Using our definition for the inverse, composition produces a valid
// DFF with a valid inverse. This class defines f2(f0(x)).
class DFFComposedF2F0 {
 public:
  DFFComposedF2F0(IntegerValue max_x, IntegerValue k_f0, IntegerValue k_f2)
      : f0_(max_x, k_f0), f2_(max_x, k_f2) {}

  IntegerValue operator()(IntegerValue x) const { return f2_(f0_(x)); }

  IntegerValue LowestInverse(IntegerValue x) const {
    return f0_.LowestInverse(f2_.LowestInverse(x));
  }

 private:
  const DualFeasibleFunctionF0 f0_;
  const RoundingDualFeasibleFunction f2_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_
