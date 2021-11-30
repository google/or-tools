// Copyright 2010-2021 Google LLC
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

#include "ortools/util/range_query_function.h"

#include <memory>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/util/range_minimum_query.h"

namespace operations_research {
namespace {
// This implementation basically calls the function as many times as needed for
// each query.
class LinearRangeIntToIntFunction : public RangeIntToIntFunction {
 public:
  explicit LinearRangeIntToIntFunction(
      std::function<int64_t(int64_t)> base_function)
      : base_function_(std::move(base_function)) {}

  int64_t Query(int64_t argument) const override {
    return base_function_(argument);
  }

  int64_t RangeMin(int64_t range_begin, int64_t range_end) const override {
    DCHECK_LT(range_begin, range_end);
    int64_t min_val = kint64max;
    for (int64_t i = range_begin; i < range_end; ++i) {
      min_val = std::min(min_val, base_function_(i));
    }
    return min_val;
  }

  int64_t RangeMax(int64_t range_begin, int64_t range_end) const override {
    DCHECK_LT(range_begin, range_end);
    int64_t max_val = kint64min;
    for (int64_t i = range_begin; i < range_end; ++i) {
      max_val = std::max(max_val, base_function_(i));
    }
    return max_val;
  }

  int64_t RangeFirstInsideInterval(int64_t range_begin, int64_t range_end,
                                   int64_t interval_begin,
                                   int64_t interval_end) const override {
    // domain_start_ <= range_begin < range_end <= domain_start_+array().size()
    DCHECK_LT(range_begin, range_end);
    DCHECK_LT(interval_begin, interval_end);
    int64_t i = range_begin;
    for (; i < range_end; ++i) {
      const int64_t value = base_function_(i);
      if (interval_begin <= value && value < interval_end) break;
    }
    return i;
  }

  int64_t RangeLastInsideInterval(int64_t range_begin, int64_t range_end,
                                  int64_t interval_begin,
                                  int64_t interval_end) const override {
    // domain_start_ <= range_begin < range_end <= domain_start_+array().size()
    DCHECK_NE(range_begin, kint64max);
    DCHECK_LT(range_begin, range_end);
    DCHECK_LT(interval_begin, interval_end);
    int64_t i = range_end - 1;
    for (; i >= range_begin; --i) {
      const int64_t value = base_function_(i);
      if (interval_begin <= value && value < interval_end) break;
    }
    return i;
  }

 private:
  std::function<int64_t(int64_t)> base_function_;

  DISALLOW_COPY_AND_ASSIGN(LinearRangeIntToIntFunction);
};

std::vector<int64_t> FunctionToVector(const std::function<int64_t(int64_t)>& f,
                                      int64_t domain_start,
                                      int64_t domain_end) {
  CHECK_LT(domain_start, domain_end);
  std::vector<int64_t> output(domain_end - domain_start, 0);
  for (int64_t i = 0; i < domain_end - domain_start; ++i) {
    output[i] = f(i + domain_start);
  }
  return output;
}

// This implementation caches the underlying function and improves on the
// non-cached version in two ways:
// 1. It caches the values returned by the function.
// 2. It creates a data structure for quick answer to range queries.
class CachedRangeIntToIntFunction : public RangeIntToIntFunction {
 public:
  CachedRangeIntToIntFunction(
      const std::function<int64_t(int64_t)>& base_function,
      int64_t domain_start, int64_t domain_end)
      : domain_start_(domain_start),
        rmq_min_(FunctionToVector(base_function, domain_start, domain_end)),
        rmq_max_(rmq_min_.array()) {
    CHECK_LT(domain_start, domain_end);
  }

  int64_t Query(int64_t argument) const override {
    DCHECK_LE(domain_start_, argument);
    DCHECK_LE(argument, domain_start_ + static_cast<int64_t>(array().size()));
    return array()[argument - domain_start_];
  }
  int64_t RangeMin(int64_t from, int64_t to) const override {
    DCHECK_LE(domain_start_, from);
    DCHECK_LT(from, to);
    DCHECK_LE(to, domain_start_ + static_cast<int64_t>(array().size()));
    return rmq_min_.GetMinimumFromRange(from - domain_start_,
                                        to - domain_start_);
  }
  int64_t RangeMax(int64_t from, int64_t to) const override {
    DCHECK_LE(domain_start_, from);
    DCHECK_LT(from, to);
    DCHECK_LE(to, domain_start_ + static_cast<int64_t>(array().size()));
    return rmq_max_.GetMinimumFromRange(from - domain_start_,
                                        to - domain_start_);
  }
  int64_t RangeFirstInsideInterval(int64_t range_begin, int64_t range_end,
                                   int64_t interval_begin,
                                   int64_t interval_end) const override {
    // domain_start_ <= range_begin < range_end <= domain_start_+array().size()
    DCHECK_LE(domain_start_, range_begin);
    DCHECK_LT(range_begin, range_end);
    DCHECK_LE(range_end, domain_start_ + array().size());
    DCHECK_LT(interval_begin, interval_end);
    int64_t i = range_begin;
    for (; i < range_end; ++i) {
      const int64_t value = array()[i - domain_start_];
      if (interval_begin <= value && value < interval_end) break;
    }
    return i;
  }
  int64_t RangeLastInsideInterval(int64_t range_begin, int64_t range_end,
                                  int64_t interval_begin,
                                  int64_t interval_end) const override {
    // domain_start_ <= range_begin < range_end <= domain_start_+array().size()
    DCHECK_LE(domain_start_, range_begin);
    DCHECK_LT(range_begin, range_end);
    DCHECK_LE(range_end, domain_start_ + array().size());
    DCHECK_LT(interval_begin, interval_end);
    int64_t i = range_end - 1;
    for (; i >= range_begin; --i) {
      const int64_t value = array()[i - domain_start_];
      if (interval_begin <= value && value < interval_end) break;
    }
    return i;
  }

 private:
  const std::vector<int64_t>& array() const { return rmq_min_.array(); }

  const int64_t domain_start_;
  const RangeMinimumQuery<int64_t, std::less<int64_t>> rmq_min_;
  const RangeMinimumQuery<int64_t, std::greater<int64_t>> rmq_max_;

  DISALLOW_COPY_AND_ASSIGN(CachedRangeIntToIntFunction);
};

class CachedRangeMinMaxIndexFunction : public RangeMinMaxIndexFunction {
 public:
  CachedRangeMinMaxIndexFunction(const std::function<int64_t(int64_t)>& f,
                                 int64_t domain_start, int64_t domain_end)
      : domain_start_(domain_start),
        domain_end_(domain_end),
        index_rmq_min_(FunctionToVector(f, domain_start, domain_end)),
        index_rmq_max_(index_rmq_min_.array()) {
    CHECK_LT(domain_start, domain_end);
  }

  inline int64_t RangeMinArgument(int64_t from, int64_t to) const override {
    DCHECK_LE(domain_start_, from);
    DCHECK_LT(from, to);
    DCHECK_LE(to, domain_end_);
    return index_rmq_min_.GetMinimumIndexFromRange(from - domain_start_,
                                                   to - domain_start_) +
           domain_start_;
  }
  inline int64_t RangeMaxArgument(int64_t from, int64_t to) const override {
    DCHECK_LE(domain_start_, from);
    DCHECK_LT(from, to);
    DCHECK_LE(to, domain_end_);
    return index_rmq_max_.GetMinimumIndexFromRange(from - domain_start_,
                                                   to - domain_start_) +
           domain_start_;
  }

 private:
  const int64_t domain_start_;
  const int64_t domain_end_;
  const RangeMinimumIndexQuery<int64_t, std::less<int64_t>> index_rmq_min_;
  const RangeMinimumIndexQuery<int64_t, std::greater<int64_t>> index_rmq_max_;

  DISALLOW_COPY_AND_ASSIGN(CachedRangeMinMaxIndexFunction);
};
}  // namespace

RangeIntToIntFunction* MakeBareIntToIntFunction(
    std::function<int64_t(int64_t)> f) {
  return new LinearRangeIntToIntFunction(std::move(f));
}

RangeIntToIntFunction* MakeCachedIntToIntFunction(
    const std::function<int64_t(int64_t)>& f, int64_t domain_start,
    int64_t domain_end) {
  return new CachedRangeIntToIntFunction(f, domain_start, domain_end);
}

RangeMinMaxIndexFunction* MakeCachedRangeMinMaxIndexFunction(
    const std::function<int64_t(int64_t)>& f, int64_t domain_start,
    int64_t domain_end) {
  return new CachedRangeMinMaxIndexFunction(f, domain_start, domain_end);
}
}  // namespace operations_research
