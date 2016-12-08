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

IntervalVariable IntervalsRepository::CreateNewInterval(IntegerValue min_start,
                                                        IntegerValue max_end) {
  const IntervalVariable task_id(start_vars_.size());

  start_vars_.push_back(integer_trail_->AddIntegerVariable(min_start, max_end));
  end_vars_.push_back(integer_trail_->AddIntegerVariable(min_start, max_end));
  size_vars_.push_back(kNoIntegerVariable);
  fixed_sizes_.push_back(IntegerValue(0));
  is_present_.push_back(kNoLiteralIndex);
  return task_id;
}

IntervalVariable IntervalsRepository::CreateInterval(IntegerValue min_start,
                                                     IntegerValue max_end,
                                                     IntegerValue min_size,
                                                     IntegerValue max_size) {
  CHECK_LE(min_size, max_size);
  if (min_size == max_size) {
    return CreateIntervalWithFixedSize(min_start, max_end, min_size);
  }
  const IntervalVariable t = CreateNewInterval(min_start, max_end);
  size_vars_.back() = integer_trail_->AddIntegerVariable(min_size, max_size);
  precedences_->AddPrecedenceWithVariableOffset(StartVar(t), EndVar(t),
                                                SizeVar(t));
  precedences_->AddPrecedenceWithVariableOffset(EndVar(t), StartVar(t),
                                                NegationOf(SizeVar(t)));
  return t;
}

IntervalVariable IntervalsRepository::CreateIntervalFromStartAndSizeVars(
    IntegerVariable start, IntegerVariable size) {
  const IntervalVariable t(start_vars_.size());
  start_vars_.push_back(start);
  size_vars_.push_back(size);
  fixed_sizes_.push_back(IntegerValue(0));
  is_present_.push_back(kNoLiteralIndex);

  // Create the "end" variable.
  // TODO(user): deal with overflow.
  const IntegerValue end_lb =
      integer_trail_->LowerBound(start) + integer_trail_->LowerBound(size);
  const IntegerValue end_ub =
      integer_trail_->UpperBound(start) + integer_trail_->UpperBound(size);
  end_vars_.push_back(integer_trail_->AddIntegerVariable(end_lb, end_ub));

  // Link the 3 integer variables together.
  precedences_->AddPrecedenceWithVariableOffset(StartVar(t), EndVar(t),
                                                SizeVar(t));
  precedences_->AddPrecedenceWithVariableOffset(EndVar(t), StartVar(t),
                                                NegationOf(SizeVar(t)));
  return t;
}

IntervalVariable IntervalsRepository::CreateIntervalWithFixedSize(
    IntegerValue min_start, IntegerValue max_end, IntegerValue size) {
  const IntervalVariable t = CreateNewInterval(min_start, max_end);
  fixed_sizes_.back() = size;
  precedences_->AddPrecedenceWithOffset(StartVar(t), EndVar(t), size);
  precedences_->AddPrecedenceWithOffset(EndVar(t), StartVar(t), -size);
  return t;
}

IntervalVariable IntervalsRepository::CreateOptionalIntervalWithFixedSize(
    IntegerValue min_start, IntegerValue max_end, IntegerValue size,
    Literal is_present) {
  const IntervalVariable t =
      CreateIntervalWithFixedSize(min_start, max_end, size);
  is_present_.back() = is_present.Index();
  precedences_->MarkIntegerVariableAsOptional(StartVar(t), is_present);
  integer_trail_->MarkIntegerVariableAsOptional(StartVar(t), is_present);
  precedences_->MarkIntegerVariableAsOptional(EndVar(t), is_present);
  integer_trail_->MarkIntegerVariableAsOptional(EndVar(t), is_present);
  return t;
}

}  // namespace sat
}  // namespace operations_research
