// Copyright 2010-2013 Google
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

#ifndef OR_TOOLS_UTIL_ACCURATE_SUM_H_
#define OR_TOOLS_UTIL_ACCURATE_SUM_H_

namespace operations_research {

// Kahan summation compensation algorithm.
// This summation algorithm is interesting on floating-point types, because
// its rounding error is as low as possible, and does not depend on the number
// of values summed: http://en.wikipedia.org/wiki/Kahan_summation_algorithm .
// Note however that it does not catch overflows, nor tries to prevent them.
// Any type that supports initialization to 0 and the + and - operator
// can be used (but again: it is worthless on integers).
//
// Accuracy:
// It should give a near-"perfect" numerical precision, i.e. the error
// is as low as it can be -- note that when some terms of the sum are
// larger than the sum itself, there is some intrinsic error that can
// not be corrected in any way.
// In particular, it is vastly superior to the trivial summation algorithm,
// and superior to other summation algorithms that reorder the operations.
// See the unit test and the wikipedia page.
//
// Performance:
// Time-wise, 4 times more +/- operations are performed than in the standard
// sum. Memory-wise, this uses up to 4 FpNumbers.
template<class FpNumber>
class AccurateSum {
 public:
  AccurateSum() : sum_(0.0), error_sum_(0.0) {}

  // Add a Fractional to the sum.
  inline void Add(const FpNumber& value) {
    const FpNumber new_sum = sum_ + value;
    const FpNumber error = (new_sum - sum_) - value;
    sum_ = new_sum;
    error_sum_ = error_sum_ + error;
  }

  // Get the value of the sum.
  inline FpNumber Value() { return sum_ - error_sum_; }

 private:
  FpNumber sum_;
  FpNumber error_sum_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ACCURATE_SUM_H_
