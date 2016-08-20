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

#include "sat/intervals.h"

namespace operations_research {
namespace sat {

IntervalVariable IntervalsRepository::CreateNewInterval() {
  const IntervalVariable task_id(start_vars_.size());

  // TODO(user): clean-up the upper bound and make sure we don't have any
  // overflow when creating task. We cannot use std::numeric_limits<int>::max()
  // because we may add a size to this bound, and we don't want any overflow
  // when we do that.
  const int ub = std::numeric_limits<int>::max() / 2;
  start_vars_.push_back(integer_trail_->AddIntegerVariable(0, ub));
  end_vars_.push_back(integer_trail_->AddIntegerVariable(0, ub));
  size_vars_.push_back(kNoIntegerVariable);
  fixed_sizes_.push_back(0);
  is_present_.push_back(kNoLiteralIndex);
  return task_id;
}

IntervalVariable IntervalsRepository::CreateInterval(int min_size,
                                                     int max_size) {
  CHECK_LE(min_size, max_size);
  if (min_size == max_size) {
    return CreateIntervalWithFixedSize(min_size);
  }
  const IntervalVariable t = CreateNewInterval();
  size_vars_.back() = integer_trail_->AddIntegerVariable(min_size, max_size);
  precedences_->AddPrecedenceWithVariableOffset(StartVar(t), EndVar(t),
                                                LbVarOf(SizeVar(t)));
  precedences_->AddPrecedenceWithVariableOffset(EndVar(t), StartVar(t),
                                                MinusUbVarOf(SizeVar(t)));
  return t;
}

IntervalVariable IntervalsRepository::CreateIntervalWithFixedSize(int size) {
  const IntervalVariable t = CreateNewInterval();
  fixed_sizes_.back() = size;
  precedences_->AddPrecedenceWithOffset(StartVar(t), EndVar(t), size);
  precedences_->AddPrecedenceWithOffset(EndVar(t), StartVar(t), -size);
  return t;
}

IntervalVariable IntervalsRepository::CreateOptionalIntervalWithFixedSize(
    int size, Literal is_present) {
  const IntervalVariable t = CreateIntervalWithFixedSize(size);
  is_present_.back() = is_present.Index();
  precedences_->MarkIntegerVariableAsOptional(StartVar(t), is_present);
  precedences_->MarkIntegerVariableAsOptional(EndVar(t), is_present);
  return t;
}

}  // namespace sat
}  // namespace operations_research
