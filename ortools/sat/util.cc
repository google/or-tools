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

#include "ortools/sat/util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

std::string FormatCounter(int64_t num) {
  std::string s = absl::StrCat(num);
  std::string out;
  const int size = s.size();
  for (int i = 0; i < size; ++i) {
    if (i > 0 && (size - i) % 3 == 0) {
      out.push_back('\'');
    }
    out.push_back(s[i]);
  }
  return out;
}

namespace {

inline std::string LeftAlign(std::string s, int size = 16) {
  if (s.size() >= size) return s;
  s.resize(size, ' ');
  return s;
}

inline std::string RightAlign(std::string s, int size = 16) {
  if (s.size() >= size) return s;
  return absl::StrCat(std::string(size - s.size(), ' '), s);
}

}  // namespace

std::string FormatTable(std::vector<std::vector<std::string>>& table,
                        int spacing) {
  if (table.size() > 1) {
    // We order by name.
    std::sort(table.begin() + 1, table.end());
  }

  std::vector<int> widths;
  for (const std::vector<std::string>& line : table) {
    if (line.size() > widths.size()) widths.resize(line.size(), spacing);
    for (int j = 0; j < line.size(); ++j) {
      widths[j] = std::max<int>(widths[j], line[j].size() + spacing);
    }
  }
  std::string output;
  for (int i = 0; i < table.size(); ++i) {
    for (int j = 0; j < table[i].size(); ++j) {
      if (i == 0 && j == 0) {
        // We currently only left align the table name.
        absl::StrAppend(&output, LeftAlign(table[i][j], widths[j]));
      } else {
        absl::StrAppend(&output, RightAlign(table[i][j], widths[j]));
      }
    }
    absl::StrAppend(&output, "\n");
  }
  return output;
}

void RandomizeDecisionHeuristic(absl::BitGenRef random,
                                SatParameters* parameters) {
#if !defined(__PORTABLE_PLATFORM__)
  // Random preferred variable order.
  const google::protobuf::EnumDescriptor* order_d =
      SatParameters::VariableOrder_descriptor();
  parameters->set_preferred_variable_order(
      static_cast<SatParameters::VariableOrder>(
          order_d->value(absl::Uniform(random, 0, order_d->value_count()))
              ->number()));

  // Random polarity initial value.
  const google::protobuf::EnumDescriptor* polarity_d =
      SatParameters::Polarity_descriptor();
  parameters->set_initial_polarity(static_cast<SatParameters::Polarity>(
      polarity_d->value(absl::Uniform(random, 0, polarity_d->value_count()))
          ->number()));
#endif  // __PORTABLE_PLATFORM__
  // Other random parameters.
  parameters->set_use_phase_saving(absl::Bernoulli(random, 0.5));
  parameters->set_random_polarity_ratio(absl::Bernoulli(random, 0.5) ? 0.01
                                                                     : 0.0);
  parameters->set_random_branches_ratio(absl::Bernoulli(random, 0.5) ? 0.01
                                                                     : 0.0);
}

namespace {

// This will be optimized into one division. I tested that in other places:
// 3/ortools/sat/integer_test.cc;l=1223-1228;bpv=0
//
// Note that I am not 100% sure we need the indirection for the optimization
// to kick in though, but this seemed safer given our weird r[i ^ 1] inputs.
void QuotientAndRemainder(int64_t a, int64_t b, int64_t& q, int64_t& r) {
  q = a / b;
  r = a % b;
}

}  // namespace

// Using the extended Euclidean algo, we find a and b such that
//     a x + b m = gcd(x, m)
// https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm
int64_t ModularInverse(int64_t x, int64_t m) {
  DCHECK_GE(x, 0);
  DCHECK_LT(x, m);

  int64_t r[2] = {m, x};
  int64_t t[2] = {0, 1};
  int64_t q;

  // We only keep the last two terms of the sequences with the "^1" trick:
  //
  // q = r[i-2] / r[i-1]
  // r[i] = r[i-2] % r[i-1]
  // t[i] = t[i-2] - t[i-1] * q
  //
  // We always have:
  // - gcd(r[i], r[i - 1]) = gcd(r[i - 1], r[i - 2])
  // - x * t[i] + m * t[i - 1] = r[i]
  int i = 0;
  for (; r[i ^ 1] != 0; i ^= 1) {
    QuotientAndRemainder(r[i], r[i ^ 1], q, r[i]);
    t[i] -= t[i ^ 1] * q;
  }

  // If the gcd is not one, there is no inverse, we returns 0.
  if (r[i] != 1) return 0;

  // Correct the result so that it is in [0, m). Note that abs(t[i]) is known to
  // be less than or equal to x / 2, and we have thorough unit-tests.
  if (t[i] < 0) t[i] += m;

  return t[i];
}

int64_t PositiveMod(int64_t x, int64_t m) {
  const int64_t r = x % m;
  return r < 0 ? r + m : r;
}

int64_t ProductWithModularInverse(int64_t coeff, int64_t mod, int64_t rhs) {
  DCHECK_NE(coeff, 0);
  DCHECK_NE(mod, 0);

  mod = std::abs(mod);
  if (rhs == 0 || mod == 1) return 0;
  DCHECK_EQ(std::gcd(std::abs(coeff), mod), 1);

  // Make both in [0, mod).
  coeff = PositiveMod(coeff, mod);
  rhs = PositiveMod(rhs, mod);

  // From X * coeff % mod = rhs
  // We deduce that X % mod = rhs * inverse % mod
  const int64_t inverse = ModularInverse(coeff, mod);
  CHECK_NE(inverse, 0);

  // We make the operation in 128 bits to be sure not to have any overflow here.
  const absl::int128 p = absl::int128{inverse} * absl::int128{rhs};
  return static_cast<int64_t>(p % absl::int128{mod});
}

bool SolveDiophantineEquationOfSizeTwo(int64_t& a, int64_t& b, int64_t& cte,
                                       int64_t& x0, int64_t& y0) {
  CHECK_NE(a, 0);
  CHECK_NE(b, 0);
  CHECK_NE(a, std::numeric_limits<int64_t>::min());
  CHECK_NE(b, std::numeric_limits<int64_t>::min());

  const int64_t gcd = std::gcd(std::abs(a), std::abs(b));
  if (cte % gcd != 0) return false;
  a /= gcd;
  b /= gcd;
  cte /= gcd;

  // The simple case where (0, 0) is a solution.
  if (cte == 0) {
    x0 = y0 = 0;
    return true;
  }

  // We solve a * X + b * Y = cte
  // We take a valid x0 in [0, b) by considering the equation mod b.
  x0 = ProductWithModularInverse(a, b, cte);

  // We choose x0 of the same sign as cte.
  if (cte < 0 && x0 != 0) x0 -= std::abs(b);

  // By plugging X = x0 + b * Z
  // We have a * (x0 + b * Z) + b * Y = cte
  // so a * b * Z + b * Y = cte - a * x0;
  // and y0 = (cte - a * x0) / b (with an exact division by construction).
  const absl::int128 t = absl::int128{cte} - absl::int128{a} * absl::int128{x0};
  DCHECK_EQ(t % absl::int128{b}, absl::int128{0});

  // Overflow-wise, there is two cases for cte > 0:
  // - a * x0 <= cte, in this case y0 will not overflow (<= cte).
  // - a * x0 > cte, in this case y0 will be in (-a, 0].
  const absl::int128 r = t / absl::int128{b};
  DCHECK_LE(r, absl::int128{std::numeric_limits<int64_t>::max()});
  DCHECK_GE(r, absl::int128{std::numeric_limits<int64_t>::min()});

  y0 = static_cast<int64_t>(r);
  return true;
}

// TODO(user): Find better implementation? In pratice passing via double is
// almost always correct, but the CapProd() might be a bit slow. However this
// is only called when we do propagate something.
int64_t FloorSquareRoot(int64_t a) {
  int64_t result =
      static_cast<int64_t>(std::floor(std::sqrt(static_cast<double>(a))));
  while (CapProd(result, result) > a) --result;
  while (CapProd(result + 1, result + 1) <= a) ++result;
  return result;
}

// TODO(user): Find better implementation?
int64_t CeilSquareRoot(int64_t a) {
  int64_t result =
      static_cast<int64_t>(std::ceil(std::sqrt(static_cast<double>(a))));
  while (CapProd(result, result) < a) ++result;
  while ((result - 1) * (result - 1) >= a) --result;
  return result;
}

int64_t ClosestMultiple(int64_t value, int64_t base) {
  if (value < 0) return -ClosestMultiple(-value, base);
  int64_t result = value / base * base;
  if (value - result > base / 2) result += base;
  return result;
}

bool LinearInequalityCanBeReducedWithClosestMultiple(
    int64_t base, absl::Span<const int64_t> coeffs,
    absl::Span<const int64_t> lbs, absl::Span<const int64_t> ubs, int64_t rhs,
    int64_t* new_rhs) {
  // Precompute some bounds for the equation base * X + error <= rhs.
  int64_t max_activity = 0;
  int64_t max_x = 0;
  int64_t min_error = 0;
  const int num_terms = coeffs.size();
  if (num_terms == 0) return false;
  for (int i = 0; i < num_terms; ++i) {
    const int64_t coeff = coeffs[i];
    CHECK_GT(coeff, 0);
    const int64_t closest = ClosestMultiple(coeff, base);
    max_activity += coeff * ubs[i];
    max_x += closest / base * ubs[i];

    const int64_t error = coeff - closest;
    if (error >= 0) {
      min_error += error * lbs[i];
    } else {
      min_error += error * ubs[i];
    }
  }

  if (max_activity <= rhs) {
    // The constraint is trivially true.
    *new_rhs = max_x;
    return true;
  }

  // This is the max error assuming that activity > rhs.
  int64_t max_error_if_invalid = 0;
  const int64_t slack = max_activity - rhs - 1;
  for (int i = 0; i < num_terms; ++i) {
    const int64_t coeff = coeffs[i];
    const int64_t closest = ClosestMultiple(coeff, base);
    const int64_t error = coeff - closest;
    if (error >= 0) {
      max_error_if_invalid += error * ubs[i];
    } else {
      const int64_t lb = std::max(lbs[i], ubs[i] - slack / coeff);
      max_error_if_invalid += error * lb;
    }
  }

  // We have old solution valid =>
  //     base * X + error <= rhs
  //     base * X <= rhs - error
  //     base * X <= rhs - min_error
  //     X <= new_rhs
  *new_rhs = std::min(max_x, MathUtil::FloorOfRatio(rhs - min_error, base));

  // And we have old solution invalid =>
  //     base * X + error >= rhs + 1
  //     base * X >= rhs + 1 - max_error_if_invalid
  //     X >= infeasibility_bound
  const int64_t infeasibility_bound =
      MathUtil::CeilOfRatio(rhs + 1 - max_error_if_invalid, base);

  // If the two bounds can be separated, we have an equivalence !
  return *new_rhs < infeasibility_bound;
}

int MoveOneUnprocessedLiteralLast(
    const absl::btree_set<LiteralIndex>& processed, int relevant_prefix_size,
    std::vector<Literal>* literals) {
  if (literals->empty()) return -1;
  if (!processed.contains(literals->back().Index())) {
    return std::min<int>(relevant_prefix_size, literals->size());
  }

  // To get O(n log n) size of suffixes, we will first process the last n/2
  // literals, we then move all of them first and process the n/2 literals left.
  // We use the same algorithm recursively. The sum of the suffixes' size S(n)
  // is thus S(n/2) + n + S(n/2). That gives us the correct complexity. The code
  // below simulates one step of this algorithm and is made to be "robust" when
  // from one call to the next, some literals have been removed (but the order
  // of literals is preserved).
  int num_processed = 0;
  int num_not_processed = 0;
  int target_prefix_size = literals->size() - 1;
  for (int i = literals->size() - 1; i >= 0; i--) {
    if (processed.contains((*literals)[i].Index())) {
      ++num_processed;
    } else {
      ++num_not_processed;
      target_prefix_size = i;
    }
    if (num_not_processed >= num_processed) break;
  }
  if (num_not_processed == 0) return -1;
  target_prefix_size = std::min(target_prefix_size, relevant_prefix_size);

  // Once a prefix size has been decided, it is always better to
  // enqueue the literal already processed first.
  std::stable_partition(
      literals->begin() + target_prefix_size, literals->end(),
      [&processed](Literal l) { return processed.contains(l.Index()); });
  return target_prefix_size;
}

int WeightedPick(absl::Span<const double> input, absl::BitGenRef random) {
  DCHECK(!input.empty());

  double total_weight = 0;
  for (const double w : input) {
    total_weight += w;
  }

  const double weight_point = absl::Uniform(random, 0.0f, total_weight);
  double total_weight2 = 0;
  for (int i = 0; i < input.size(); ++i) {
    total_weight2 += input[i];
    if (total_weight2 > weight_point) {
      return i;
    }
  }
  return input.size() - 1;
}

void IncrementalAverage::Reset(double reset_value) {
  num_records_ = 0;
  average_ = reset_value;
}

void IncrementalAverage::AddData(double new_record) {
  num_records_++;
  average_ += (new_record - average_) / num_records_;
}

void ExponentialMovingAverage::AddData(double new_record) {
  num_records_++;
  average_ = (num_records_ == 1)
                 ? new_record
                 : (new_record + decaying_factor_ * (average_ - new_record));
}

void Percentile::AddRecord(double record) {
  records_.push_front(record);
  if (records_.size() > record_limit_) {
    records_.pop_back();
  }
}

double Percentile::GetPercentile(double percent) {
  CHECK_GT(records_.size(), 0);
  CHECK_LE(percent, 100.0);
  CHECK_GE(percent, 0.0);

  const int num_records = records_.size();
  const double percentile_rank =
      static_cast<double>(num_records) * percent / 100.0 - 0.5;
  if (percentile_rank <= 0) {
    return *absl::c_min_element(records_);
  } else if (percentile_rank >= num_records - 1) {
    return *absl::c_max_element(records_);
  }
  std::vector<double> sorted_records;
  sorted_records.assign(records_.begin(), records_.end());
  // Interpolate.
  DCHECK_GE(num_records, 2);
  DCHECK_LT(percentile_rank, num_records - 1);
  const int lower_rank = static_cast<int>(std::floor(percentile_rank));
  DCHECK_LT(lower_rank, num_records - 1);
  auto upper_it = sorted_records.begin() + lower_rank + 1;
  // Ensure that sorted_records[lower_rank + 1] is in the correct place as if
  // records were actually sorted, the next closest is then the largest element
  // to the left of this element.
  absl::c_nth_element(sorted_records, upper_it);
  auto lower_it = std::max_element(sorted_records.begin(), upper_it);
  return *lower_it + (percentile_rank - lower_rank) * (*upper_it - *lower_it);
}

void MaxBoundedSubsetSum::Reset(int64_t bound) {
  DCHECK_GE(bound, 0);
  gcd_ = 0;
  sums_ = {0};
  expanded_sums_.clear();
  current_max_ = 0;
  bound_ = bound;
}

void MaxBoundedSubsetSum::Add(int64_t value) {
  if (value == 0) return;
  if (value > bound_) return;
  gcd_ = std::gcd(gcd_, value);
  AddChoicesInternal({value});
}

void MaxBoundedSubsetSum::AddChoices(absl::Span<const int64_t> choices) {
  if (DEBUG_MODE) {
    for (const int64_t c : choices) {
      DCHECK_GE(c, 0);
    }
  }

  // The max is already reachable or we aborted.
  if (current_max_ == bound_) return;

  // Filter out zero and values greater than bound_.
  filtered_values_.clear();
  for (const int64_t c : choices) {
    if (c == 0 || c > bound_) continue;
    filtered_values_.push_back(c);
    gcd_ = std::gcd(gcd_, c);
  }
  if (filtered_values_.empty()) return;

  // So we can abort early in the AddChoicesInternal() inner loops.
  std::sort(filtered_values_.begin(), filtered_values_.end());
  AddChoicesInternal(filtered_values_);
}

void MaxBoundedSubsetSum::AddMultiples(int64_t coeff, int64_t max_value) {
  DCHECK_GE(coeff, 0);
  DCHECK_GE(max_value, 0);

  if (coeff == 0 || max_value == 0) return;
  if (coeff > bound_) return;
  if (current_max_ == bound_) return;
  gcd_ = std::gcd(gcd_, coeff);

  const int64_t num_values =
      std::min(max_value, MathUtil::FloorOfRatio(bound_, coeff));
  if (num_values > 10) {
    // We only keep GCD in this case.
    sums_.clear();
    expanded_sums_.clear();
    current_max_ = MathUtil::FloorOfRatio(bound_, gcd_) * gcd_;
    return;
  }

  filtered_values_.clear();
  for (int multiple = 1; multiple <= num_values; ++multiple) {
    const int64_t v = multiple * coeff;
    if (v == bound_) {
      current_max_ = bound_;
      return;
    }
    filtered_values_.push_back(v);
  }
  AddChoicesInternal(filtered_values_);
}

void MaxBoundedSubsetSum::AddChoicesInternal(absl::Span<const int64_t> values) {
  // Mode 1: vector of all possible sums (with duplicates).
  if (!sums_.empty() && sums_.size() <= max_complexity_per_add_) {
    const int old_size = sums_.size();
    for (int i = 0; i < old_size; ++i) {
      for (const int64_t value : values) {
        const int64_t s = sums_[i] + value;
        if (s > bound_) break;

        sums_.push_back(s);
        current_max_ = std::max(current_max_, s);
        if (current_max_ == bound_) return;  // Abort
      }
    }
    return;
  }

  // Mode 2: bitset of all possible sums.
  if (bound_ <= max_complexity_per_add_) {
    if (!sums_.empty()) {
      expanded_sums_.assign(bound_ + 1, false);
      for (const int64_t s : sums_) {
        expanded_sums_[s] = true;
      }
      sums_.clear();
    }

    // The reverse order is important to not add the current value twice.
    if (!expanded_sums_.empty()) {
      for (int64_t i = bound_ - 1; i >= 0; --i) {
        if (!expanded_sums_[i]) continue;
        for (const int64_t value : values) {
          if (i + value > bound_) break;

          expanded_sums_[i + value] = true;
          current_max_ = std::max(current_max_, i + value);
          if (current_max_ == bound_) return;  // Abort
        }
      }
      return;
    }
  }

  // Fall back to gcd_.
  DCHECK_NE(gcd_, 0);
  if (gcd_ == 1) {
    current_max_ = bound_;
  } else {
    current_max_ = MathUtil::FloorOfRatio(bound_, gcd_) * gcd_;
  }
}

int64_t MaxBoundedSubsetSum::MaxIfAdded(int64_t candidate) const {
  if (candidate > bound_ || current_max_ == bound_) return current_max_;

  int64_t current_max = current_max_;
  // Mode 1: vector of all possible sums (with duplicates).
  if (!sums_.empty()) {
    for (const int64_t v : sums_) {
      if (v + candidate > bound_) continue;
      if (v + candidate > current_max) {
        current_max = v + candidate;
        if (current_max == bound_) return current_max;
      }
    }
    return current_max;
  }

  // Mode 2: bitset of all possible sums.
  if (!expanded_sums_.empty()) {
    const int64_t min_useful = std::max<int64_t>(0, current_max_ - candidate);
    const int64_t max_useful = bound_ - candidate;
    for (int64_t v = max_useful; v >= min_useful; --v) {
      if (expanded_sums_[v]) return v + candidate;
    }
  }
  return current_max_;
}

BasicKnapsackSolver::Result BasicKnapsackSolver::Solve(
    absl::Span<const Domain> domains, absl::Span<const int64_t> coeffs,
    absl::Span<const int64_t> costs, const Domain& rhs) {
  const int num_vars = domains.size();
  if (num_vars == 0) return {};

  int64_t min_activity = 0;
  int64_t max_domain_size = 0;
  for (int i = 0; i < num_vars; ++i) {
    max_domain_size = std::max(max_domain_size, domains[i].Size());
    if (coeffs[i] > 0) {
      min_activity += coeffs[i] * domains[i].Min();
    } else {
      min_activity += coeffs[i] * domains[i].Max();
    }
  }

  // The complexity of our DP will depends on the number of "activity" values
  // that need to be considered.
  //
  // TODO(user): We can also solve efficiently if max_activity - rhs.Min() is
  // small. Implement.
  const int64_t num_values = rhs.Max() - min_activity + 1;
  if (num_values < 0) {
    // Problem is clearly infeasible, we can report the result right away.
    Result result;
    result.solved = true;
    result.infeasible = true;
    return result;
  }

  // Abort if complexity too large.
  const int64_t max_work_per_variable = std::min(num_values, max_domain_size);
  if (rhs.Max() - min_activity > 1e6) return {};
  if (num_vars * num_values * max_work_per_variable > 1e8) return {};

  // Canonicalize to positive coeffs and non-negative variables.
  domains_.clear();
  coeffs_.clear();
  costs_.clear();
  for (int i = 0; i < num_vars; ++i) {
    if (coeffs[i] > 0) {
      domains_.push_back(domains[i].AdditionWith(Domain(-domains[i].Min())));
      coeffs_.push_back(coeffs[i]);
      costs_.push_back(costs[i]);
    } else {
      domains_.push_back(
          domains[i].Negation().AdditionWith(Domain(domains[i].Max())));
      coeffs_.push_back(-coeffs[i]);
      costs_.push_back(-costs[i]);
    }
  }

  Result result =
      InternalSolve(num_values, rhs.AdditionWith(Domain(-min_activity)));
  if (result.solved && !result.infeasible) {
    // Transform solution back.
    for (int i = 0; i < num_vars; ++i) {
      if (coeffs[i] > 0) {
        result.solution[i] += domains[i].Min();
      } else {
        result.solution[i] = domains[i].Max() - result.solution[i];
      }
    }
  }
  return result;
}

BasicKnapsackSolver::Result BasicKnapsackSolver::InternalSolve(
    int64_t num_values, const Domain& rhs) {
  const int num_vars = domains_.size();

  // The set of DP states that we will fill.
  var_activity_states_.assign(num_vars, std::vector<State>(num_values));

  // Initialize with first variable.
  for (const int64_t v : domains_[0].Values()) {
    const int64_t value = v * coeffs_[0];
    CHECK_GE(value, 0);
    if (value >= num_values) break;
    var_activity_states_[0][value].cost = v * costs_[0];
    var_activity_states_[0][value].value = v;
  }

  // Fill rest of the DP states.
  for (int i = 1; i < num_vars; ++i) {
    const std::vector<State>& prev = var_activity_states_[i - 1];
    std::vector<State>& current = var_activity_states_[i];
    for (int prev_value = 0; prev_value < num_values; ++prev_value) {
      if (prev[prev_value].cost == std::numeric_limits<int64_t>::max()) {
        continue;
      }
      for (const int64_t v : domains_[i].Values()) {
        const int64_t value = prev_value + v * coeffs_[i];
        CHECK_GE(value, 0);
        if (value >= num_values) break;
        const int64_t new_cost = prev[prev_value].cost + v * costs_[i];
        if (new_cost < current[value].cost) {
          current[value].cost = new_cost;
          current[value].value = v;
        }
      }
    }
  }

  Result result;
  result.solved = true;

  int64_t best_cost = std::numeric_limits<int64_t>::max();
  int64_t best_activity;
  for (int v = 0; v < num_values; ++v) {
    // TODO(user): optimize this?
    if (!rhs.Contains(v)) continue;
    if (var_activity_states_.back()[v].cost < best_cost) {
      best_cost = var_activity_states_.back()[v].cost;
      best_activity = v;
    }
  }

  if (best_cost == std::numeric_limits<int64_t>::max()) {
    result.infeasible = true;
    return result;
  }

  // Recover the values.
  result.solution.resize(num_vars);
  int64_t current_activity = best_activity;
  for (int i = num_vars - 1; i >= 0; --i) {
    const int64_t var_value = var_activity_states_[i][current_activity].value;
    result.solution[i] = var_value;
    current_activity -= coeffs_[i] * var_value;
  }

  return result;
}

namespace {

class CliqueDecomposition {
 public:
  CliqueDecomposition(const std::vector<std::vector<int>>& graph,
                      absl::BitGenRef random, std::vector<int>* buffer)
      : graph_(graph), random_(random), buffer_(buffer) {
    const int n = graph.size();
    permutation_.resize(n);
    buffer_->resize(n);

    // TODO(user): Start by sorting by decreasing size of adjacency list?
    for (int i = 0; i < n; ++i) permutation_[i] = i;
  }

  // This works in O(m). All possible decomposition are reachable, depending on
  // the initial permutation.
  //
  // TODO(user): It can be made faster within the same complexity though.
  void DecomposeGreedily() {
    decomposition_.clear();
    candidates_.clear();

    const int n = graph_.size();
    taken_.assign(n, false);
    temp_.assign(n, false);

    int buffer_index = 0;
    for (const int i : permutation_) {
      if (taken_[i]) continue;

      // Save start of amo and output i.
      const int start = buffer_index;
      taken_[i] = true;
      (*buffer_)[buffer_index++] = i;

      candidates_.clear();
      for (const int c : graph_[i]) {
        if (!taken_[c]) candidates_.push_back(c);
      }
      while (!candidates_.empty()) {
        // Extend at most one with lowest possible permutation index.
        int next = candidates_.front();
        for (const int n : candidates_) {
          if (permutation_[n] < permutation_[next]) next = n;
        }

        // Add to current amo.
        taken_[next] = true;
        (*buffer_)[buffer_index++] = next;

        // Filter candidates.
        // TODO(user): With a sorted graph_, same complexity, but likely faster.
        for (const int head : graph_[next]) temp_[head] = true;
        int new_size = 0;
        for (const int c : candidates_) {
          if (taken_[c]) continue;
          if (!temp_[c]) continue;
          candidates_[new_size++] = c;
        }
        candidates_.resize(new_size);
        for (const int head : graph_[next]) temp_[head] = false;
      }

      // Output amo.
      decomposition_.push_back(
          absl::MakeSpan(buffer_->data() + start, buffer_index - start));
    }
    DCHECK_EQ(buffer_index, n);
  }

  // We follow the heuristic in the paper: "Partitioning Networks into Cliques:
  // A Randomized Heuristic Approach", David Chaluppa, 2014.
  //
  // Note that by keeping the local order of each clique, we cannot make the
  // order "worse". And each new call to DecomposeGreedily() can only reduce
  // the number of clique in the cover.
  void ChangeOrder() {
    if (absl::Bernoulli(random_, 0.5)) {
      std::reverse(decomposition_.begin(), decomposition_.end());
    } else {
      std::shuffle(decomposition_.begin(), decomposition_.end(), random_);
    }

    int out_index = 0;
    for (const absl::Span<const int> clique : decomposition_) {
      for (const int i : clique) {
        permutation_[out_index++] = i;
      }
    }
  }

  const std::vector<absl::Span<int>>& decomposition() const {
    return decomposition_;
  }

 private:
  const std::vector<std::vector<int>>& graph_;
  absl::BitGenRef random_;
  std::vector<int>* buffer_;

  std::vector<absl::Span<int>> decomposition_;
  std::vector<int> candidates_;
  std::vector<int> permutation_;
  std::vector<bool> taken_;
  std::vector<bool> temp_;
};

}  // namespace

std::vector<absl::Span<int>> AtMostOneDecomposition(
    const std::vector<std::vector<int>>& graph, absl::BitGenRef random,
    std::vector<int>* buffer) {
  CliqueDecomposition decomposer(graph, random, buffer);
  for (int pass = 0; pass < 10; ++pass) {
    decomposer.DecomposeGreedily();
    if (decomposer.decomposition().size() == 1) break;
    decomposer.ChangeOrder();
  }
  return decomposer.decomposition();
}

}  // namespace sat
}  // namespace operations_research
