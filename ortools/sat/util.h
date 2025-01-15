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

#ifndef OR_TOOLS_SAT_UTIL_H_
#define OR_TOOLS_SAT_UTIL_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log_streamer.h"
#include "absl/numeric/int128.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// A simple class with always IdentityMap[t] == t.
// This is to avoid allocating vector with std::iota() in some Apis.
template <typename T>
class IdentityMap {
 public:
  T operator[](T t) const { return t; }
};

// Small utility class to store a vector<vector<>> where one can only append new
// vector and never change previously added ones. This allows to store a static
// key -> value(s) mapping.
//
// This is a lot more compact memorywise and thus faster than vector<vector<>>.
// Note that we implement a really small subset of the vector<vector<>> API.
//
// We support int and StrongType for key K and any copyable type for value V.
template <typename K = int, typename V = int>
class CompactVectorVector {
 public:
  using value_type = V;

  // Size of the "key" space, always in [0, size()).
  size_t size() const;
  bool empty() const;
  size_t num_entries() const { return buffer_.size(); }

  // Getters, either via [] or via a wrapping to be compatible with older api.
  //
  // Warning: Spans are only valid until the next modification!
  absl::Span<V> operator[](K key);
  absl::Span<const V> operator[](K key) const;
  std::vector<absl::Span<const V>> AsVectorOfSpan() const;

  // Restore to empty vector<vector<>>.
  void clear();

  // Reserve memory if it is already known or tightly estimated.
  void reserve(int size) {
    starts_.reserve(size);
    sizes_.reserve(size);
  }
  void reserve(int size, int num_entries) {
    reserve(size);
    buffer_.reserve(num_entries);
  }

  // Given a flat mapping (keys[i] -> values[i]) with two parallel vectors, not
  // necessarily sorted by key, regroup the same key so that
  // CompactVectorVector[key] list all values in the order in which they appear.
  //
  // We only check keys.size(), so this can be used with IdentityMap() as
  // second argument.
  template <typename Keys, typename Values>
  void ResetFromFlatMapping(Keys keys, Values values);

  // Same as above but for any collections of std::pair<K, V>, or, more
  // generally, any iterable collection of objects that have a `first` and a
  // `second` members.
  template <typename Collection>
  void ResetFromPairs(const Collection& pairs, int minimum_num_nodes = 0);

  // Initialize this vector from the transpose of another.
  // IMPORTANT: This cannot be called with the vector itself.
  //
  // If min_transpose_size is given, then the transpose will have at least this
  // size even if some of the last keys do not appear in other.
  //
  // If this is called twice in a row, then it has the side effect of sorting
  // all inner vectors by values !
  void ResetFromTranspose(const CompactVectorVector<V, K>& other,
                          int min_transpose_size = 0);

  // Append a new entry.
  // Returns the previous size() as this is convenient for how we use it.
  int Add(absl::Span<const V> values);
  void AppendToLastVector(const V& value);

  // Hacky: same as Add() but for sat::Literal or any type from which we can get
  // a value type V via L.Index().value().
  template <typename L>
  int AddLiterals(const std::vector<L>& wrapped_values);

  // We lied when we said this is a pure read-only class :)
  // It is possible to shrink inner vectors with not much cost.
  //
  // Removes the element at index from this[key] by swapping it with
  // this[key].back() and then decreasing this key size. It is an error to
  // call this on an empty inner vector.
  void RemoveBySwap(K key, int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, sizes_[key]);
    const int start = starts_[key];
    std::swap(buffer_[start + index], buffer_[start + sizes_[key] - 1]);
    sizes_[key]--;
  }

  // Replace the values at the given key.
  // This will crash if there are more values than before.
  void ReplaceValuesBySmallerSet(K key, absl::Span<const V> values);

  // Interface so this can be used as an output of
  // FindStronglyConnectedComponents().
  void emplace_back(V const* begin, V const* end) {
    Add(absl::MakeSpan(begin, end - begin));
  }

 private:
  // Convert int and StrongInt to normal int.
  static int InternalKey(K key);

  std::vector<int> starts_;
  std::vector<int> sizes_;
  std::vector<V> buffer_;
};

// We often have a vector with fixed capacity reserved outside the hot loops.
// Using this class instead save the capacity but most importantly link a lot
// less code for the push_back() calls which allow more inlining.
//
// TODO(user): Add more functions and unit-test.
template <typename T>
class FixedCapacityVector {
 public:
  void ClearAndReserve(size_t size) {
    size_ = 0;
    data_.reset(new T[size]);
  }

  T* data() const { return data_.get(); }
  T* begin() const { return data_.get(); }
  T* end() const { return data_.get() + size_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  T operator[](int i) const { return data_[i]; }
  T& operator[](int i) { return data_[i]; }

  T back() const { return data_[size_ - 1]; }
  T& back() { return data_[size_ - 1]; }

  void clear() { size_ = 0; }
  void resize(size_t size) { size_ = size; }
  void pop_back() { --size_; }
  void push_back(T t) { data_[size_++] = t; }

 private:
  int size_ = 0;
  std::unique_ptr<T[]> data_ = nullptr;
};

// Prints a positive number with separators for easier reading (ex: 1'348'065).
std::string FormatCounter(int64_t num);

// This is used to format our table first row entry.
inline std::string FormatName(absl::string_view name) {
  return absl::StrCat("'", name, "':");
}

// Display tabular data by auto-computing cell width. Note that we right align
// everything but the first row/col that is assumed to be the table name and is
// left aligned.
std::string FormatTable(std::vector<std::vector<std::string>>& table,
                        int spacing = 2);

// Returns a in [0, m) such that a * x = 1 modulo m.
// If gcd(x, m) != 1, there is no inverse, and it returns 0.
//
// This DCHECK that x is in [0, m).
// This is integer overflow safe.
//
// Note(user): I didn't find this in a easily usable standard library.
int64_t ModularInverse(int64_t x, int64_t m);

// Just returns x % m but with a result always in [0, m).
int64_t PositiveMod(int64_t x, int64_t m);

// If we know that X * coeff % mod = rhs % mod, this returns c such that
// PositiveMod(X, mod) = c.
//
// This requires coeff != 0, mod !=0 and gcd(coeff, mod) == 1.
// The result will be in [0, mod) but there is no other condition on the sign or
// magnitude of a and b.
//
// This is overflow safe, and when rhs == 0 or abs(mod) == 1, it returns 0.
int64_t ProductWithModularInverse(int64_t coeff, int64_t mod, int64_t rhs);

// Returns true if the equation a * X + b * Y = cte has some integer solutions.
// For now, we check that a and b are different from 0 and from int64_t min.
//
// There is actually always a solution if cte % gcd(|a|, |b|) == 0. And because
// a, b and cte fit on an int64_t, if there is a solution, there is one with X
// and Y fitting on an int64_t.
//
// We will divide everything by gcd(a, b) first, so it is why we take reference
// and the equation can change.
//
// If there are solutions, we return one of them (x0, y0).
// From any such solution, the set of all solutions is given for Z integer by:
// X = x0 + b * Z;
// Y = y0 - a * Z;
//
// Given a domain for X and Y, it is possible to compute the "exact" domain of Z
// with our Domain functions. Note however that this will only compute solution
// where both x-x0 and y-y0 do fit on an int64_t:
// DomainOf(x).SubtractionWith(x0).InverseMultiplicationBy(b).IntersectionWith(
//     DomainOf(y).SubtractionWith(y0).InverseMultiplicationBy(-a))
bool SolveDiophantineEquationOfSizeTwo(int64_t& a, int64_t& b, int64_t& cte,
                                       int64_t& x0, int64_t& y0);

// The argument must be non-negative.
int64_t FloorSquareRoot(int64_t a);
int64_t CeilSquareRoot(int64_t a);

// Converts a double to int64_t and cap large magnitudes at kint64min/max.
// We also arbitrarily returns 0 for NaNs.
//
// Note(user): This is similar to SaturatingFloatToInt(), but we use our own
// since we need to open source it and the code is simple enough.
int64_t SafeDoubleToInt64(double value);

// Returns the multiple of base closest to value. If there is a tie, we return
// the one closest to zero. This way we have ClosestMultiple(x) =
// -ClosestMultiple(-x) which is important for how this is used.
int64_t ClosestMultiple(int64_t value, int64_t base);

// Assuming n "literal" in [0, n), and a graph such that graph[i] list the
// literal in [0, n) implied to false when the literal with index i is true,
// this returns an heuristic decomposition of the literals into disjoint at most
// ones.
//
// Note(user): Symmetrize the matrix if not already, maybe rephrase in term
// of undirected graph, and clique decomposition.
std::vector<absl::Span<int>> AtMostOneDecomposition(
    const std::vector<std::vector<int>>& graph, absl::BitGenRef random,
    std::vector<int>* buffer);

// Given a linear equation "sum coeff_i * X_i <= rhs. We can rewrite it using
// ClosestMultiple() as "base * new_terms + error <= rhs" where error can be
// bounded using the provided bounds on each variables. This will return true if
// the error can be ignored and this equation is completely equivalent to
// new_terms <= new_rhs.
//
// This is useful for cases like 9'999 X + 10'0001 Y <= 155'000 where we have
// weird coefficient (maybe due to scaling). With a base of 10K, this is
// equivalent to X + Y <= 15.
//
// Preconditions: All coeffs are assumed to be positive. You can easily negate
// all the negative coeffs and corresponding bounds before calling this.
bool LinearInequalityCanBeReducedWithClosestMultiple(
    int64_t base, absl::Span<const int64_t> coeffs,
    absl::Span<const int64_t> lbs, absl::Span<const int64_t> ubs, int64_t rhs,
    int64_t* new_rhs);

// The model "singleton" random engine used in the solver.
//
// In test, we usually set use_absl_random() so that the sequence is changed at
// each invocation. This way, clients do not relly on the wrong assumption that
// a particular optimal solution will be returned if they are many equivalent
// ones.
class ModelRandomGenerator : public absl::BitGenRef {
 public:
  // We seed the strategy at creation only. This should be enough for our use
  // case since the SatParameters is set first before the solver is created. We
  // also never really need to change the seed afterwards, it is just used to
  // diversify solves with identical parameters on different Model objects.
  explicit ModelRandomGenerator(const SatParameters& params)
      : absl::BitGenRef(deterministic_random_) {
    deterministic_random_.seed(params.random_seed());
    if (params.use_absl_random()) {
      absl_random_ = absl::BitGen(absl::SeedSeq({params.random_seed()}));
      absl::BitGenRef::operator=(absl::BitGenRef(absl_random_));
    }
  }
  explicit ModelRandomGenerator(Model* model)
      : ModelRandomGenerator(*model->GetOrCreate<SatParameters>()) {}

  // This is just used to display ABSL_RANDOM_SALT_OVERRIDE in the log so that
  // it is possible to reproduce a failure more easily while looking at a solver
  // log.
  //
  // TODO(user): I didn't find a cleaner way to log this.
  void LogSalt() const {}

 private:
  random_engine_t deterministic_random_;
  absl::BitGen absl_random_;
};

// The model "singleton" shared time limit.
class ModelSharedTimeLimit : public SharedTimeLimit {
 public:
  explicit ModelSharedTimeLimit(Model* model)
      : SharedTimeLimit(model->GetOrCreate<TimeLimit>()) {}
};

// Randomizes the decision heuristic of the given SatParameters.
void RandomizeDecisionHeuristic(absl::BitGenRef random,
                                SatParameters* parameters);

// This is equivalent of
// absl::discrete_distribution<std::size_t>(input.begin(), input.end())(random)
// but does no allocations. It is a lot faster when you need to pick just one
// elements from a distribution for instance.
int WeightedPick(absl::Span<const double> input, absl::BitGenRef random);

// Context: this function is not really generic, but required to be unit-tested.
// It is used in a clause minimization algorithm when we try to detect if any of
// the clause literals can be propagated by a subset of the other literal being
// false. For that, we want to enqueue in the solver all the subset of size n-1.
//
// This moves one of the unprocessed literal from literals to the last position.
// The function tries to do that while preserving the longest possible prefix of
// literals "amortized" through the calls assuming that we want to move each
// literal to the last position once.
//
// For a vector of size n, if we want to call this n times so that each literal
// is last at least once, the sum of the size of the changed suffixes will be
// O(n log n). If we were to use a simpler algorithm (like moving the last
// unprocessed literal to the last position), this sum would be O(n^2).
//
// Returns the size of the common prefix of literals before and after the move,
// or -1 if all the literals are already processed. The argument
// relevant_prefix_size is used as a hint when keeping more that this prefix
// size do not matter. The returned value will always be lower or equal to
// relevant_prefix_size.
int MoveOneUnprocessedLiteralLast(
    const absl::btree_set<LiteralIndex>& processed, int relevant_prefix_size,
    std::vector<Literal>* literals);

// Simple DP to compute the maximum reachable value of a "subset sum" under
// a given bound (inclusive). Note that we abort as soon as the computation
// become too important.
//
// Precondition: Both bound and all added values must be >= 0.
class MaxBoundedSubsetSum {
 public:
  MaxBoundedSubsetSum() : max_complexity_per_add_(/*default=*/50) { Reset(0); }
  explicit MaxBoundedSubsetSum(int64_t bound, int max_complexity_per_add = 50)
      : max_complexity_per_add_(max_complexity_per_add) {
    Reset(bound);
  }

  // Resets to an empty set of values.
  // We look for the maximum sum <= bound.
  void Reset(int64_t bound);

  // Returns the updated max if value was added to the subset-sum.
  int64_t MaxIfAdded(int64_t candidate) const;

  // Add a value to the base set for which subset sums will be taken.
  void Add(int64_t value);

  // Add a choice of values to the base set for which subset sums will be taken.
  // Note that even if this doesn't include zero, not taking any choices will
  // also be an option.
  void AddChoices(absl::Span<const int64_t> choices);

  // Adds [0, coeff, 2 * coeff, ... max_value * coeff].
  void AddMultiples(int64_t coeff, int64_t max_value);

  // Returns an upper bound (inclusive) on the maximum sum <= bound_.
  // This might return bound_ if we aborted the computation.
  int64_t CurrentMax() const { return current_max_; }

  int64_t Bound() const { return bound_; }

 private:
  // This assumes filtered values.
  void AddChoicesInternal(absl::Span<const int64_t> values);

  // Max_complexity we are willing to pay on each Add() call.
  const int max_complexity_per_add_;
  int64_t gcd_;
  int64_t bound_;
  int64_t current_max_;
  std::vector<int64_t> sums_;
  std::vector<bool> expanded_sums_;
  std::vector<int64_t> filtered_values_;
};

// Simple DP to keep the set of the first n reachable value (n > 1).
//
// TODO(user): Maybe modulo some prime number we can keep more info.
// TODO(user): Another common case is a bunch of really small values and larger
// ones, so we could bound the sum of the small values and keep the first few
// reachable by the big ones. This is similar to some presolve transformations.
template <int n>
class FirstFewValues {
 public:
  FirstFewValues()
      : reachable_(new int64_t[n]), new_reachable_(new int64_t[n]) {
    Reset();
  }

  void Reset() {
    for (int i = 0; i < n; ++i) {
      reachable_[i] = std::numeric_limits<int64_t>::max();
    }
    reachable_[0] = 0;
    new_reachable_[0] = 0;
  }

  // We assume the given positive value can be added as many time as wanted.
  //
  // TODO(user): Implement Add() with an upper bound on the multiplicity.
  void Add(const int64_t positive_value) {
    DCHECK_GT(positive_value, 0);
    const int64_t* reachable = reachable_.get();
    if (positive_value >= reachable[n - 1]) return;

    // We copy from reachable_[i] to new_reachable_[j].
    // The position zero is already copied.
    int i = 1;
    int j = 1;
    int64_t* new_reachable = new_reachable_.get();
    for (int base = 0; j < n && base < n; ++base) {
      const int64_t candidate = CapAdd(new_reachable[base], positive_value);
      while (j < n && i < n && reachable[i] < candidate) {
        new_reachable[j++] = reachable[i++];
      }
      if (j < n) {
        // Eliminate duplicates.
        while (i < n && reachable[i] == candidate) i++;

        // insert candidate in its final place.
        new_reachable[j++] = candidate;
      }
    }
    std::swap(reachable_, new_reachable_);
  }

  // Returns true iff sum might be expressible as a weighted sum of the added
  // value. Any sum >= LastValue() is always considered potentially reachable.
  bool MightBeReachable(int64_t sum) const {
    if (sum >= reachable_[n - 1]) return true;
    return std::binary_search(&reachable_[0], &reachable_[0] + n, sum);
  }

  int64_t LastValue() const { return reachable_[n - 1]; }

  absl::Span<const int64_t> reachable() {
    return absl::MakeSpan(reachable_.get(), n);
  }

 private:
  std::unique_ptr<int64_t[]> reachable_;
  std::unique_ptr<int64_t[]> new_reachable_;
};

// Use Dynamic programming to solve a single knapsack. This is used by the
// presolver to simplify variables appearing in a single linear constraint.
//
// Complexity is the best of
// - O(num_variables * num_relevant_values ^ 2) or
// - O(num_variables * num_relevant_values * max_domain_size).
class BasicKnapsackSolver {
 public:
  // Solves the problem:
  //   - minimize sum costs * X[i]
  //   - subject to sum coeffs[i] * X[i] \in rhs, with X[i] \in Domain(i).
  //
  // Returns:
  //   - (solved = false) if complexity is too high.
  //   - (solved = true, infeasible = true) if proven infeasible.
  //   - (solved = true, infeasible = false, solution) otherwise.
  struct Result {
    bool solved = false;
    bool infeasible = false;
    std::vector<int64_t> solution;
  };
  Result Solve(absl::Span<const Domain> domains,
               absl::Span<const int64_t> coeffs,
               absl::Span<const int64_t> costs, const Domain& rhs);

 private:
  Result InternalSolve(int64_t num_values, const Domain& rhs);

  // Canonicalized version.
  std::vector<Domain> domains_;
  std::vector<int64_t> coeffs_;
  std::vector<int64_t> costs_;

  // We only need to keep one state with the same activity.
  struct State {
    int64_t cost = std::numeric_limits<int64_t>::max();
    int64_t value = 0;
  };
  std::vector<std::vector<State>> var_activity_states_;
};

// Manages incremental averages.
class IncrementalAverage {
 public:
  // Initializes the average with 'initial_average' and number of records to 0.
  explicit IncrementalAverage(double initial_average)
      : average_(initial_average) {}
  IncrementalAverage() = default;

  // Sets the number of records to 0 and average to 'reset_value'.
  void Reset(double reset_value);

  double CurrentAverage() const { return average_; }
  int64_t NumRecords() const { return num_records_; }

  void AddData(double new_record);

 private:
  double average_ = 0.0;
  int64_t num_records_ = 0;
};

// Manages exponential moving averages defined as
// new_average = decaying_factor * old_average
//               + (1 - decaying_factor) * new_record.
// where 0 < decaying_factor < 1.
class ExponentialMovingAverage {
 public:
  explicit ExponentialMovingAverage(double decaying_factor)
      : decaying_factor_(decaying_factor) {
    DCHECK_GE(decaying_factor, 0.0);
    DCHECK_LE(decaying_factor, 1.0);
  }

  // Returns exponential moving average for all the added data so far.
  double CurrentAverage() const { return average_; }

  // Returns the total number of added records so far.
  int64_t NumRecords() const { return num_records_; }

  void AddData(double new_record);

 private:
  double average_ = 0.0;
  int64_t num_records_ = 0;
  const double decaying_factor_;
};

// Utility to calculate percentile (First variant) for limited number of
// records. Reference: https://en.wikipedia.org/wiki/Percentile
//
// After the vector is sorted, we assume that the element with index i
// correspond to the percentile 100*(i+0.5)/size. For percentiles before the
// first element (resp. after the last one) we return the first element (resp.
// the last). And otherwise we do a linear interpolation between the two element
// around the asked percentile.
class Percentile {
 public:
  explicit Percentile(int record_limit) : record_limit_(record_limit) {}

  void AddRecord(double record);

  // Returns number of stored records.
  int64_t NumRecords() const { return records_.size(); }

  // Note that this runs in O(n) for n records.
  double GetPercentile(double percent);

 private:
  std::deque<double> records_;
  const int record_limit_;
};

// Keep the top n elements from a stream of elements.
//
// TODO(user): We could use gtl::TopN when/if it gets open sourced. Note that
// we might be slighlty faster here since we use an indirection and don't move
// the Element class around as much.
template <typename Element, typename Score>
class TopN {
 public:
  explicit TopN(int n) : n_(n) {}

  void Clear() {
    heap_.clear();
    elements_.clear();
  }

  void Add(Element e, Score score) {
    if (heap_.size() < n_) {
      const int index = elements_.size();
      heap_.push_back({index, score});
      elements_.push_back(std::move(e));
      if (heap_.size() == n_) {
        // TODO(user): We could delay that on the n + 1 push.
        std::make_heap(heap_.begin(), heap_.end());
      }
    } else {
      if (score <= heap_.front().score) return;
      const int index_to_replace = heap_.front().index;
      elements_[index_to_replace] = std::move(e);

      // If needed, we could be faster here with an update operation.
      std::pop_heap(heap_.begin(), heap_.end());
      heap_.back() = {index_to_replace, score};
      std::push_heap(heap_.begin(), heap_.end());
    }
  }

  bool empty() const { return elements_.empty(); }

  const std::vector<Element>& UnorderedElements() const { return elements_; }
  std::vector<Element>* MutableUnorderedElements() { return &elements_; }

 private:
  const int n_;

  // We keep a heap of the n highest score.
  struct HeapElement {
    int index;  // in elements_;
    Score score;
    bool operator<(const HeapElement& other) const {
      return score > other.score;
    }
  };
  std::vector<HeapElement> heap_;
  std::vector<Element> elements_;
};

// ============================================================================
// Implementation.
// ============================================================================

inline int64_t SafeDoubleToInt64(double value) {
  if (std::isnan(value)) return 0;
  if (value >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  if (value <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(value);
}

// Tells whether a int128 can be casted to a int64_t that can be negated.
inline bool IsNegatableInt64(absl::int128 x) {
  return x <= absl::int128(std::numeric_limits<int64_t>::max()) &&
         x > absl::int128(std::numeric_limits<int64_t>::min());
}

template <typename K, typename V>
inline int CompactVectorVector<K, V>::Add(absl::Span<const V> values) {
  const int index = size();
  starts_.push_back(buffer_.size());
  sizes_.push_back(values.size());
  buffer_.insert(buffer_.end(), values.begin(), values.end());
  return index;
}

template <typename K, typename V>
inline void CompactVectorVector<K, V>::AppendToLastVector(const V& value) {
  sizes_.back()++;
  buffer_.push_back(value);
}

template <typename K, typename V>
inline void CompactVectorVector<K, V>::ReplaceValuesBySmallerSet(
    K key, absl::Span<const V> values) {
  CHECK_LE(values.size(), sizes_[key]);
  sizes_[key] = values.size();
  if (values.empty()) return;
  memcpy(&buffer_[starts_[key]], values.data(), sizeof(V) * values.size());
}

template <typename K, typename V>
template <typename L>
inline int CompactVectorVector<K, V>::AddLiterals(
    const std::vector<L>& wrapped_values) {
  const int index = size();
  starts_.push_back(buffer_.size());
  sizes_.push_back(wrapped_values.size());
  for (const L wrapped_value : wrapped_values) {
    buffer_.push_back(wrapped_value.Index().value());
  }
  return index;
}

// We need to support both StrongType and normal int.
template <typename K, typename V>
inline int CompactVectorVector<K, V>::InternalKey(K key) {
  if constexpr (std::is_same_v<K, int>) {
    return key;
  } else {
    return key.value();
  }
}

template <typename K, typename V>
inline absl::Span<const V> CompactVectorVector<K, V>::operator[](K key) const {
  DCHECK_GE(key, 0);
  DCHECK_LT(key, starts_.size());
  DCHECK_LT(key, sizes_.size());
  const int k = InternalKey(key);
  const size_t size = static_cast<size_t>(sizes_.data()[k]);
  if (size == 0) return {};
  return {&buffer_.data()[starts_.data()[k]], size};
}

template <typename K, typename V>
inline absl::Span<V> CompactVectorVector<K, V>::operator[](K key) {
  DCHECK_GE(key, 0);
  DCHECK_LT(key, starts_.size());
  DCHECK_LT(key, sizes_.size());
  const int k = InternalKey(key);
  const size_t size = static_cast<size_t>(sizes_.data()[k]);
  if (size == 0) return {};
  return absl::MakeSpan(&buffer_.data()[starts_.data()[k]], size);
}

template <typename K, typename V>
inline std::vector<absl::Span<const V>>
CompactVectorVector<K, V>::AsVectorOfSpan() const {
  std::vector<absl::Span<const V>> result(starts_.size());
  for (int k = 0; k < starts_.size(); ++k) {
    result[k] = (*this)[k];
  }
  return result;
}

template <typename K, typename V>
inline void CompactVectorVector<K, V>::clear() {
  starts_.clear();
  sizes_.clear();
  buffer_.clear();
}

template <typename K, typename V>
inline size_t CompactVectorVector<K, V>::size() const {
  return starts_.size();
}

template <typename K, typename V>
inline bool CompactVectorVector<K, V>::empty() const {
  return starts_.empty();
}

template <typename K, typename V>
template <typename Keys, typename Values>
inline void CompactVectorVector<K, V>::ResetFromFlatMapping(Keys keys,
                                                            Values values) {
  if (keys.empty()) return clear();

  // Compute maximum index.
  int max_key = 0;
  for (const K key : keys) {
    max_key = std::max(max_key, InternalKey(key) + 1);
  }

  // Compute sizes_;
  sizes_.assign(max_key, 0);
  for (const K key : keys) {
    sizes_[InternalKey(key)]++;
  }

  // Compute starts_;
  starts_.assign(max_key, 0);
  for (int k = 1; k < max_key; ++k) {
    starts_[k] = starts_[k - 1] + sizes_[k - 1];
  }

  // Copy data and uses starts as temporary indices.
  buffer_.resize(keys.size());
  for (int i = 0; i < keys.size(); ++i) {
    buffer_[starts_[InternalKey(keys[i])]++] = values[i];
  }

  // Restore starts_.
  for (int k = max_key - 1; k > 0; --k) {
    starts_[k] = starts_[k - 1];
  }
  starts_[0] = 0;
}

// Similar to ResetFromFlatMapping().
template <typename K, typename V>
template <typename Collection>
inline void CompactVectorVector<K, V>::ResetFromPairs(const Collection& pairs,
                                                      int minimum_num_nodes) {
  // Compute maximum index.
  int max_key = minimum_num_nodes;
  for (const auto& [key, _] : pairs) {
    max_key = std::max(max_key, InternalKey(key) + 1);
  }

  if (pairs.empty()) {
    clear();
    sizes_.assign(minimum_num_nodes, 0);
    starts_.assign(minimum_num_nodes, 0);
    return;
  }

  // Compute sizes_;
  sizes_.assign(max_key, 0);
  for (const auto& [key, _] : pairs) {
    sizes_[InternalKey(key)]++;
  }

  // Compute starts_;
  starts_.assign(max_key, 0);
  for (int k = 1; k < max_key; ++k) {
    starts_[k] = starts_[k - 1] + sizes_[k - 1];
  }

  // Copy data and uses starts as temporary indices.
  buffer_.resize(pairs.size());
  for (int i = 0; i < pairs.size(); ++i) {
    const auto& [key, value] = pairs[i];
    buffer_[starts_[InternalKey(key)]++] = value;
  }

  // Restore starts_.
  for (int k = max_key - 1; k > 0; --k) {
    starts_[k] = starts_[k - 1];
  }
  starts_[0] = 0;
}

// Similar to ResetFromFlatMapping().
template <typename K, typename V>
inline void CompactVectorVector<K, V>::ResetFromTranspose(
    const CompactVectorVector<V, K>& other, int min_transpose_size) {
  if (other.empty()) {
    clear();
    if (min_transpose_size > 0) {
      starts_.assign(min_transpose_size, 0);
      sizes_.assign(min_transpose_size, 0);
    }
    return;
  }

  // Compute maximum index.
  int max_key = min_transpose_size;
  for (V v = 0; v < other.size(); ++v) {
    for (const K k : other[v]) {
      max_key = std::max(max_key, InternalKey(k) + 1);
    }
  }

  // Compute sizes_;
  sizes_.assign(max_key, 0);
  for (V v = 0; v < other.size(); ++v) {
    for (const K k : other[v]) {
      sizes_[InternalKey(k)]++;
    }
  }

  // Compute starts_;
  starts_.assign(max_key, 0);
  for (int k = 1; k < max_key; ++k) {
    starts_[k] = starts_[k - 1] + sizes_[k - 1];
  }

  // Copy data and uses starts as temporary indices.
  buffer_.resize(other.buffer_.size());
  for (V v = 0; v < other.size(); ++v) {
    for (const K k : other[v]) {
      buffer_[starts_[InternalKey(k)]++] = v;
    }
  }

  // Restore starts_.
  for (int k = max_key - 1; k > 0; --k) {
    starts_[k] = starts_[k - 1];
  }
  starts_[0] = 0;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_UTIL_H_
