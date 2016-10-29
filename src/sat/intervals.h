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

#ifndef OR_TOOLS_SAT_INTERVALS_H_
#define OR_TOOLS_SAT_INTERVALS_H_

#include "sat/integer.h"
#include "sat/integer_expr.h"
#include "sat/model.h"
#include "sat/precedences.h"
#include "sat/sat_base.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

DEFINE_INT_TYPE(IntervalVariable, int32);
const IntervalVariable kNoIntervalVariable(-1);

// This class maintains a set of intervals which correspond to three integer
// variables (start, end and size). It automatically registers with the
// PrecedencesPropagator the relation between the bounds of each interval and
// provides many helper functions to add precedences relation between intervals.
class IntervalsRepository {
 public:
  IntervalsRepository(IntegerTrail* integer_trail,
                      PrecedencesPropagator* precedences)
      : integer_trail_(integer_trail), precedences_(precedences) {}

  static IntervalsRepository* CreateInModel(Model* model) {
    IntervalsRepository* intervals =
        new IntervalsRepository(model->GetOrCreate<IntegerTrail>(),
                                model->GetOrCreate<PrecedencesPropagator>());
    model->TakeOwnership(intervals);
    return intervals;
  }

  // Returns the current number of intervals in the repository.
  // The interval will always be identified by an integer in [0, num_intervals).
  int NumIntervals() const { return start_vars_.size(); }

  // Functions to add a new interval to the repository.
  IntervalVariable CreateInterval(IntegerValue min_size, IntegerValue max_size);
  IntervalVariable CreateIntervalWithFixedSize(IntegerValue size);
  IntervalVariable CreateOptionalIntervalWithFixedSize(IntegerValue size,
                                                       Literal is_present);
  IntervalVariable CreateIntervalFromStartAndSizeVars(IntegerVariable start,
                                                      IntegerVariable size);

  // Returns whether or not a interval is optional and the associated literal.
  bool IsOptional(IntervalVariable i) const {
    return is_present_[i] != kNoLiteralIndex;
  }
  Literal IsPresentLiteral(IntervalVariable i) const {
    return Literal(is_present_[i]);
  }

  // The 3 integer variables associated to a interval.
  // Fixed size intervals will have a kNoIntegerVariable as size.
  //
  // Note: For an optional interval, the start/end variables are propagated
  // asssuming the interval is present. Because of that, these variables can
  // cross each other or have an empty domain. If any of this happend, then the
  // IsPresentLiteral() of this interval will be propagated to false.
  IntegerVariable SizeVar(IntervalVariable i) const { return size_vars_[i]; }
  IntegerVariable StartVar(IntervalVariable i) const { return start_vars_[i]; }
  IntegerVariable EndVar(IntervalVariable i) const { return end_vars_[i]; }

  // Only meaningfull if SizeVar(i) == kNoIntegerVariable.
  IntegerValue FixedSize(IntervalVariable i) const { return fixed_sizes_[i]; }

 private:
  // Creates a new interval and returns its id.
  IntervalVariable CreateNewInterval();

  // External classes needed.
  IntegerTrail* integer_trail_;
  PrecedencesPropagator* precedences_;

  // Literal indicating if the tasks is executed. Tasks that are always executed
  // will have a kNoLiteralIndex entry in this vector.
  ITIVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  ITIVector<IntervalVariable, IntegerVariable> start_vars_;
  ITIVector<IntervalVariable, IntegerVariable> end_vars_;
  ITIVector<IntervalVariable, IntegerVariable> size_vars_;
  ITIVector<IntervalVariable, IntegerValue> fixed_sizes_;

  DISALLOW_COPY_AND_ASSIGN(IntervalsRepository);
};

// =============================================================================
// Model based functions.
// =============================================================================

inline std::function<IntegerVariable(const Model&)> StartVar(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->StartVar(v);
  };
}

inline std::function<IntegerVariable(const Model&)> EndVar(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->EndVar(v);
  };
}

inline std::function<IntegerVariable(const Model&)> SizeVar(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->SizeVar(v);
  };
}

inline std::function<IntervalVariable(Model*)> NewInterval(int64 size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()
        ->CreateIntervalWithFixedSize(IntegerValue(size));
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalWithVariableSize(
    int64 min_size, int64 max_size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        IntegerValue(min_size), IntegerValue(max_size));
  };
}

inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    int64 size, Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()
        ->CreateOptionalIntervalWithFixedSize(IntegerValue(size), is_present);
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalFromStartAndSizeVars(
    IntegerVariable start, IntegerVariable size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()
        ->CreateIntervalFromStartAndSizeVars(start, size);
  };
}

inline std::function<void(Model*)> EndBefore(IntervalVariable i1,
                                             IntegerVariable ivar) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(intervals->EndVar(i1), ivar);
  };
}

inline std::function<void(Model*)> EndBeforeWithOffset(IntervalVariable i1,
                                                       IntegerVariable ivar,
                                                       int64 offset) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedenceWithOffset(intervals->EndVar(i1), ivar,
                                         IntegerValue(offset));
  };
}

inline std::function<void(Model*)> EndBeforeStart(IntervalVariable i1,
                                                  IntervalVariable i2) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(intervals->EndVar(i1), intervals->StartVar(i2));
  };
}

inline std::function<void(Model*)> StartAtEnd(IntervalVariable i1,
                                              IntervalVariable i2) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(intervals->EndVar(i1), intervals->StartVar(i2));
    precedences->AddPrecedence(intervals->StartVar(i2), intervals->EndVar(i1));
  };
}

inline std::function<void(Model*)> StartAtStart(IntervalVariable i1,
                                                IntervalVariable i2) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(intervals->StartVar(i1),
                               intervals->StartVar(i2));
    precedences->AddPrecedence(intervals->StartVar(i2),
                               intervals->StartVar(i1));
  };
}

inline std::function<void(Model*)> EndAtEnd(IntervalVariable i1,
                                            IntervalVariable i2) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(intervals->EndVar(i1), intervals->EndVar(i2));
    precedences->AddPrecedence(intervals->EndVar(i2), intervals->EndVar(i1));
  };
}

// This requires that all the alternatives are optional tasks.
inline std::function<void(Model*)> IntervalWithAlternatives(
    IntervalVariable master, const std::vector<IntervalVariable>& members) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();

    std::vector<Literal> presences;
    std::vector<IntegerValue> durations;

    // Create an "exactly one executed" constraint on the alternatives.
    std::vector<LiteralWithCoeff> sat_ct;
    for (const IntervalVariable member : members) {
      CHECK(intervals->IsOptional(member));
      const Literal is_present = intervals->IsPresentLiteral(member);
      sat_ct.push_back({is_present, Coefficient(1)});
      model->Add(StartAtStart(master, member));
      model->Add(EndAtEnd(member, master));

      // TODO(user): This only work for members with fixed size. Generalize.
      CHECK_EQ(intervals->SizeVar(member), kNoIntegerVariable);
      presences.push_back(is_present);
      durations.push_back(intervals->FixedSize(member));
    }
    if (intervals->SizeVar(master) != kNoIntegerVariable) {
      model->Add(IsOneOf(intervals->SizeVar(master), presences, durations));
    }
    model->Add(BooleanLinearConstraint(1, 1, &sat_ct));
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTERVALS_H_
