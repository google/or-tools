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

#include "sat/cp_constraints.h"
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
  // - If size == kNoIntegerVariable, then the size is assumed to be fixed
  //   to fixed_size.
  // - If is_present != kNoLiteralIndex, then this is an optional interval.
  IntervalVariable CreateInterval(IntegerVariable start, IntegerVariable end,
                                  IntegerVariable size, IntegerValue fixed_size,
                                  LiteralIndex is_present);

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
  // cross each other or have an empty domain. If any of this happen, then the
  // IsPresentLiteral() of this interval will be propagated to false.
  IntegerVariable SizeVar(IntervalVariable i) const { return size_vars_[i]; }
  IntegerVariable StartVar(IntervalVariable i) const { return start_vars_[i]; }
  IntegerVariable EndVar(IntervalVariable i) const { return end_vars_[i]; }

  // Return the minimum size of the given IntervalVariable.
  IntegerValue MinSize(IntervalVariable i) const {
    const IntegerVariable size_var = size_vars_[i];
    if (size_var == kNoIntegerVariable) return fixed_sizes_[i];
    return integer_trail_->LowerBound(size_var);
  }

  // Return the maximum size of the given IntervalVariable.
  IntegerValue MaxSize(IntervalVariable i) const {
    const IntegerVariable size_var = size_vars_[i];
    if (size_var == kNoIntegerVariable) return fixed_sizes_[i];
    return integer_trail_->UpperBound(size_var);
  }

 private:
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

inline std::function<IntervalVariable(Model*)> NewInterval(int64 min_start,
                                                           int64 max_end,
                                                           int64 size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)), kNoIntegerVariable,
        IntegerValue(size), kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewInterval(
    IntegerVariable start, IntegerVariable end, IntegerVariable size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalWithVariableSize(
    int64 min_start, int64 max_end, int64 min_size, int64 max_size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    int64 min_start, int64 max_end, int64 size, Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)), kNoIntegerVariable,
        IntegerValue(size), is_present.Index());
  };
}

inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    IntegerVariable start, IntegerVariable end, IntegerVariable size,
    Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), is_present.Index());
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalFromStartAndSizeVars(
    IntegerVariable start, IntegerVariable size) {
  return [=](Model* model) {
    // Create the "end" variable.
    // TODO(user): deal with overflow.
    IntegerTrail* t = model->GetOrCreate<IntegerTrail>();
    const IntegerValue end_lb = t->LowerBound(start) + t->LowerBound(size);
    const IntegerValue end_ub = t->UpperBound(start) + t->UpperBound(size);
    const IntegerVariable end = t->AddIntegerVariable(end_lb, end_ub);
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), kNoLiteralIndex);
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
      model->Add(
          Equality(model->Get(StartVar(master)), model->Get(StartVar(member))));
      model->Add(
          Equality(model->Get(EndVar(master)), model->Get(EndVar(member))));

      // TODO(user): IsOneOf() only work for members with fixed size.
      // Generalize to an "int_var_element" constraint.
      CHECK_EQ(intervals->SizeVar(member), kNoIntegerVariable);
      presences.push_back(is_present);
      durations.push_back(intervals->MinSize(member));
    }
    if (intervals->SizeVar(master) != kNoIntegerVariable) {
      model->Add(IsOneOf(intervals->SizeVar(master), presences, durations));
    }
    model->Add(BooleanLinearConstraint(1, 1, &sat_ct));

    // Propagate from the candidate bounds to the master interval ones.
    {
      std::vector<IntegerVariable> starts;
      for (const IntervalVariable member : members) {
        starts.push_back(intervals->StartVar(member));
      }
      model->Add(
          PartialIsOneOfVar(intervals->StartVar(master), starts, presences));
    }
    {
      std::vector<IntegerVariable> ends;
      for (const IntervalVariable member : members) {
        ends.push_back(intervals->EndVar(member));
      }
      model->Add(PartialIsOneOfVar(intervals->EndVar(master), ends, presences));
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTERVALS_H_
