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

IntervalVariable IntervalsRepository::CreateInterval(IntegerVariable start,
                                                     IntegerVariable end,
                                                     IntegerVariable size,
                                                     IntegerValue fixed_size,
                                                     LiteralIndex is_present) {
  // Create the interval.
  const IntervalVariable i(start_vars_.size());
  start_vars_.push_back(start);
  end_vars_.push_back(end);
  size_vars_.push_back(size);
  fixed_sizes_.push_back(fixed_size);
  is_present_.push_back(is_present);

  // Link properly all its components.
  if (SizeVar(i) != kNoIntegerVariable) {
    precedences_->AddPrecedenceWithVariableOffset(StartVar(i), EndVar(i),
                                                  SizeVar(i));
    precedences_->AddPrecedenceWithVariableOffset(EndVar(i), StartVar(i),
                                                  NegationOf(SizeVar(i)));
  } else {
    precedences_->AddPrecedenceWithOffset(StartVar(i), EndVar(i), fixed_size);
    precedences_->AddPrecedenceWithOffset(EndVar(i), StartVar(i), -fixed_size);
  }
  if (IsOptional(i)) {
    const Literal literal(is_present);
    precedences_->MarkIntegerVariableAsOptional(StartVar(i), literal);
    integer_trail_->MarkIntegerVariableAsOptional(StartVar(i), literal);
    precedences_->MarkIntegerVariableAsOptional(EndVar(i), literal);
    integer_trail_->MarkIntegerVariableAsOptional(EndVar(i), literal);
    if (SizeVar(i) != kNoIntegerVariable) {
      // TODO(user): This is not currently fully supported in precedences_
      // if the size is not a constant variable.
      CHECK_EQ(integer_trail_->LowerBound(SizeVar(i)),
               integer_trail_->UpperBound(SizeVar(i)));
      precedences_->MarkIntegerVariableAsOptional(SizeVar(i), literal);
      integer_trail_->MarkIntegerVariableAsOptional(SizeVar(i), literal);
    }
  }
  return i;
}

}  // namespace sat
}  // namespace operations_research
