// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_ACCURATE_SUM_H_
#define OR_TOOLS_BASE_ACCURATE_SUM_H_

namespace operations_research {

// Kahan summation compensation algorithm.
//
//   http://en.wikipedia.org/wiki/Kahan_summation_algorithm
template <typename FpNumber>
class AccurateSum {
 public:
  // You may copy-construct an AccurateSum.
  AccurateSum() : sum_(), error_sum_() {}

  // Adds an FpNumber to the sum.
  void Add(const FpNumber& value) {
    error_sum_ += value;
    const FpNumber new_sum = sum_ + error_sum_;
    error_sum_ += sum_ - new_sum;
    sum_ = new_sum;
  }

  // Gets the value of the sum.
  FpNumber Value() const { return sum_; }

 private:
  FpNumber sum_;
  FpNumber error_sum_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_ACCURATE_SUM_H_
