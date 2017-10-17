// Copyright 2010-2017 Google
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

// The header defines an interface for functions taking and returning an int64
// and supporting range queries over their domain and codomain.

#ifndef OR_TOOLS_UTIL_RANGE_QUERY_FUNCTION_H_
#define OR_TOOLS_UTIL_RANGE_QUERY_FUNCTION_H_

#include <functional>
#include <memory>

#include "ortools/base/integral_types.h"

namespace operations_research {
// RangeIntToIntFunction is an interface to int64->int64 functions supporting
// fast answer to range queries about their domain/codomain.
class RangeIntToIntFunction {
 public:
  virtual ~RangeIntToIntFunction() = default;

  // Suppose f is the abstract underlying function.
  // Returns f(argument).
  // TODO(user): Rename to Run
  virtual int64 Query(int64 argument) const = 0;
  // Returns min_x f(x), where x is in [from, to).
  virtual int64 RangeMin(int64 from, int64 to) const = 0;
  // Returns max_x f(x), where x is in [from, to).
  virtual int64 RangeMax(int64 from, int64 to) const = 0;
  // Returns the first x from [range_begin, range_end) for which f(x) is in
  // [interval_begin, interval_end), or range_end if there is no such x.
  virtual int64 RangeFirstInsideInterval(int64 range_begin, int64 range_end,
                                         int64 interval_begin,
                                         int64 interval_end) const = 0;
  // Returns the last x from [range_begin, range_end) for which f(x) is in
  // [interval_begin, interval_end), or range_begin-1 if there is no such x.
  virtual int64 RangeLastInsideInterval(int64 range_begin, int64 range_end,
                                        int64 interval_begin,
                                        int64 interval_end) const = 0;
};

// RangeMinMaxIndexFunction is different from RangeIntToIntFunction in two ways:
//
//   1. It does not support codomain or value queries.
//
//   2. For domain queries it returns an argument where the minimum/maximum is
//      attained, rather than the minimum/maximum value.
class RangeMinMaxIndexFunction {
 public:
  virtual ~RangeMinMaxIndexFunction() = default;
  // Suppose f is the abstract underlying function.
  // Returns an x from [from, to), such that f(x) => f(y) for every y from
  // [from, to).
  virtual int64 RangeMaxArgument(int64 from, int64 to) const = 0;
  // Returns an x from [from, to), such that f(x) <= f(y) for every y from
  // [from, to).
  virtual int64 RangeMinArgument(int64 from, int64 to) const = 0;
};

// A copy of f is going to be stored in the returned object, so its closure
// should remain intact as long as the returned object is being used.
RangeIntToIntFunction* MakeBareIntToIntFunction(std::function<int64(int64)> f);
// It is assumed that f is defined over the interval [domain_start, domain_end).
// The function scans f once and it is safe to destroy f and its closure after
// MakeCachedIntToIntFunction returns.
RangeIntToIntFunction* MakeCachedIntToIntFunction(
    const std::function<int64(int64)>& f, int64 domain_start, int64 domain_end);
// It is safe to destroy the first argument and its closure after
// MakeCachedRangeMinMaxIndexFunction returns.
RangeMinMaxIndexFunction* MakeCachedRangeMinMaxIndexFunction(
    const std::function<int64(int64)>& f, int64 domain_start, int64 domain_end);
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_RANGE_QUERY_FUNCTION_H_
