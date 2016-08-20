// Copyright 2010-2014 Google
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

#include "sat/integer_sum.h"

namespace operations_research {
namespace sat {

IntegerSum::IntegerSum(const std::vector<IntegerVariable>& vars,
                       const std::vector<int>& coeffs, IntegerVariable sum,
                       IntegerTrail* integer_trail)
    : vars_(vars), coeffs_(coeffs), sum_(sum), integer_trail_(integer_trail) {}

bool IntegerSum::Propagate(Trail* trail) {
  if (vars_.empty()) return true;

  int new_lb = integer_trail_->LowerBound(vars_[0]) * coeffs_[0];
  for (int i = 1; i < vars_.size(); ++i) {
    new_lb += integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
  }

  // Update the sum lower-bound.
  if (new_lb > integer_trail_->LowerBound(sum_)) {
    integer_reason_.clear();
    for (const IntegerVariable& var : vars_) {
      integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
    }
    integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(sum_, new_lb),
                            literal_reason_, integer_reason_);
    if (integer_trail_->DomainIsEmpty(sum_, trail)) return false;
  }

  // Update the variables upper-bound.
  const int sum_upper_bound = integer_trail_->UpperBound(sum_);
  for (int i = 0; i < vars_.size(); ++i) {
    const int new_term_ub = sum_upper_bound - new_lb +
                            integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
    const int new_ub = new_term_ub / coeffs_[i];
    if (new_ub < integer_trail_->UpperBound(vars_[i])) {
      integer_reason_.clear();
      for (int j = 0; j < vars_.size(); ++j) {
        if (i == j) continue;
        integer_reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(vars_[j]));
      }
      integer_reason_.push_back(integer_trail_->UpperBoundAsLiteral(sum_));
      integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(vars_[i], new_ub),
                              literal_reason_, integer_reason_);
      if (integer_trail_->DomainIsEmpty(vars_[i], trail)) return false;
    }
  }

  return true;
}

void IntegerSum::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchIntegerVariable(var, id);
  }
  watcher->WatchIntegerVariable(sum_, id);
}

}  // namespace sat
}  // namespace operations_research
