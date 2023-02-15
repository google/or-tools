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

#ifndef OR_TOOLS_SAT_UTIL_H_
#define OR_TOOLS_SAT_UTIL_H_

#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>

#include "ortools/base/logging.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "google/protobuf/descriptor.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/container/btree_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Prints a positive number with separators for easier reading (ex: 1'348'065).
std::string FormatCounter(int64_t num);

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
    int64_t base, const std::vector<int64_t>& coeffs,
    const std::vector<int64_t>& lbs, const std::vector<int64_t>& ubs,
    int64_t rhs, int64_t* new_rhs);

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
  explicit ModelRandomGenerator(Model* model)
      : absl::BitGenRef(deterministic_random_) {
    const auto& params = *model->GetOrCreate<SatParameters>();
    deterministic_random_.seed(params.random_seed());
    if (params.use_absl_random()) {
      absl_random_ = absl::BitGen(absl::SeedSeq({params.random_seed()}));
      absl::BitGenRef::operator=(absl::BitGenRef(absl_random_));
    }
  }

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
  MaxBoundedSubsetSum() { Reset(0); }
  explicit MaxBoundedSubsetSum(int64_t bound) { Reset(bound); }

  // Resets to an empty set of values.
  // We look for the maximum sum <= bound.
  void Reset(int64_t bound);

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

  static constexpr int kMaxComplexityPerAdd = 50;

  int64_t gcd_;
  int64_t bound_;
  int64_t current_max_;
  std::vector<int64_t> sums_;
  std::vector<bool> expanded_sums_;
  std::vector<int64_t> filtered_values_;
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
  Result Solve(const std::vector<Domain>& domains,
               const std::vector<int64_t>& coeffs,
               const std::vector<int64_t>& costs, const Domain& rhs);

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
  IncrementalAverage() {}

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

  // Note that this is not fast and runs in O(n log n) for n records.
  double GetPercentile(double percent);

 private:
  std::deque<double> records_;
  const int record_limit_;
};

// This method tries to compress a list of tuples by merging complementary
// tuples, that is a set of tuples that only differ on one variable, and that
// cover the domain of the variable. In that case, it will keep only one tuple,
// and replace the value for variable by any_value, the equivalent of '*' in
// regexps.
//
// This method is exposed for testing purposes.
constexpr int64_t kTableAnyValue = std::numeric_limits<int64_t>::min();
void CompressTuples(absl::Span<const int64_t> domain_sizes,
                    std::vector<std::vector<int64_t>>* tuples);

// Similar to CompressTuples() but produces a final table where each cell is
// a set of value. This should result in a table that can still be encoded
// efficiently in SAT but with less tuples and thus less extra Booleans. Note
// that if a set of value is empty, it is interpreted at "any" so we can gain
// some space.
//
// The passed tuples vector is used as temporary memory and is detroyed.
// We interpret kTableAnyValue as an "any" tuple.
//
// TODO(user): To reduce memory, we could return some absl::Span in the last
// layer instead of vector.
//
// TODO(user): The final compression is depend on the order of the variables.
// For instance the table (1,1)(1,2)(1,3),(1,4),(2,3) can either be compressed
// as (1,*)(2,3) or (1,{1,2,4})({1,3},3). More experiment are needed to devise
// a better heuristic. It might for example be good to call CompressTuples()
// first.
std::vector<std::vector<absl::InlinedVector<int64_t, 2>>> FullyCompressTuples(
    absl::Span<const int64_t> domain_sizes,
    std::vector<std::vector<int64_t>>* tuples);

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

// These functions are copied from MathUtils. However, the original ones are
// incompatible with absl::int128 as MathLimits<absl::int128>::kIsInteger ==
// false.
template <typename IntType, bool ceil>
IntType CeilOrFloorOfRatio(IntType numerator, IntType denominator) {
  static_assert(std::numeric_limits<IntType>::is_integer,
                "CeilOfRatio is only defined for integral types");
  DCHECK_NE(0, denominator) << "Division by zero is not supported.";
  DCHECK(numerator != std::numeric_limits<IntType>::min() || denominator != -1)
      << "Dividing " << numerator << "by -1 is not supported: it would SIGFPE";

  const IntType rounded_toward_zero = numerator / denominator;
  const bool needs_round = (numerator % denominator) != 0;
  const bool same_sign = (numerator >= 0) == (denominator >= 0);

  if (ceil) {  // Compile-time condition: not an actual branching
    return rounded_toward_zero + static_cast<IntType>(same_sign && needs_round);
  } else {
    return rounded_toward_zero -
           static_cast<IntType>(!same_sign && needs_round);
  }
}

template <typename IntType>
IntType CeilOfRatio(IntType numerator, IntType denominator) {
  return CeilOrFloorOfRatio<IntType, true>(numerator, denominator);
}

template <typename IntType>
IntType FloorOfRatio(IntType numerator, IntType denominator) {
  return CeilOrFloorOfRatio<IntType, false>(numerator, denominator);
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_UTIL_H_
