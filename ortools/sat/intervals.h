// Copyright 2010-2025 Google LLC
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

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// This class maintains a set of intervals which correspond to three integer
// variables (start, end and size). It automatically registers with the
// PrecedencesPropagator the relation between the bounds of each interval and
// provides many helper functions to add precedences relation between intervals.
class IntervalsRepository {
 public:
  explicit IntervalsRepository(Model* model);

  // This type is neither copyable nor movable.
  IntervalsRepository(const IntervalsRepository&) = delete;
  IntervalsRepository& operator=(const IntervalsRepository&) = delete;

  // Returns the current number of intervals in the repository.
  // The interval will always be identified by an integer in [0, num_intervals).
  int NumIntervals() const { return starts_.size(); }

  // Functions to add a new interval to the repository.
  // If add_linear_relation is true, then we also link start, size and end.
  //
  // - If size == kNoIntegerVariable, then the size is fixed to fixed_size.
  // - If is_present != kNoLiteralIndex, then this is an optional interval.
  IntervalVariable CreateInterval(IntegerVariable start, IntegerVariable end,
                                  IntegerVariable size, IntegerValue fixed_size,
                                  LiteralIndex is_present);
  IntervalVariable CreateInterval(AffineExpression start, AffineExpression end,
                                  AffineExpression size,
                                  LiteralIndex is_present = kNoLiteralIndex,
                                  bool add_linear_relation = false);

  // Returns whether or not a interval is optional and the associated literal.
  bool IsOptional(IntervalVariable i) const {
    return is_present_[i] != kNoLiteralIndex;
  }
  Literal PresenceLiteral(IntervalVariable i) const {
    return Literal(is_present_[i]);
  }
  bool IsPresent(IntervalVariable i) const {
    if (!IsOptional(i)) return true;
    return assignment_.LiteralIsTrue(PresenceLiteral(i));
  }
  bool IsAbsent(IntervalVariable i) const {
    if (!IsOptional(i)) return false;
    return assignment_.LiteralIsFalse(PresenceLiteral(i));
  }

  // The 3 integer variables associated to a interval.
  // Fixed size intervals will have a kNoIntegerVariable as size.
  //
  // Note: For an optional interval, the start/end variables are propagated
  // assuming the interval is present. Because of that, these variables can
  // cross each other or have an empty domain. If any of this happen, then the
  // PresenceLiteral() of this interval will be propagated to false.
  AffineExpression Size(IntervalVariable i) const { return sizes_[i]; }
  AffineExpression Start(IntervalVariable i) const { return starts_[i]; }
  AffineExpression End(IntervalVariable i) const { return ends_[i]; }

  // Return the minimum size of the given IntervalVariable.
  IntegerValue MinSize(IntervalVariable i) const {
    return integer_trail_->LowerBound(sizes_[i]);
  }

  // Return the maximum size of the given IntervalVariable.
  IntegerValue MaxSize(IntervalVariable i) const {
    return integer_trail_->UpperBound(sizes_[i]);
  }

  // Utility function that returns a vector will all intervals.
  std::vector<IntervalVariable> AllIntervals() const {
    std::vector<IntervalVariable> result;
    for (IntervalVariable i(0); i < NumIntervals(); ++i) {
      result.push_back(i);
    }
    return result;
  }

  // Returns a SchedulingConstraintHelper corresponding to the given variables.
  // Note that the order of interval in the helper will be the same.
  //
  // It is possible to indicate that this correspond to a disjunctive constraint
  // by setting the Boolean to true. This is used by our scheduling heuristic
  // based on precedences.
  SchedulingConstraintHelper* GetOrCreateHelper(
      const std::vector<IntervalVariable>& variables,
      bool register_as_disjunctive_helper = false);

  NoOverlap2DConstraintHelper* GetOrCreate2DHelper(
      const std::vector<IntervalVariable>& x_variables,
      const std::vector<IntervalVariable>& y_variables);

  // Returns a SchedulingDemandHelper corresponding to the given helper and
  // demands. Note that the order of interval in the helper and the order of
  // demands must be the compatible.
  SchedulingDemandHelper* GetOrCreateDemandHelper(
      SchedulingConstraintHelper* helper,
      absl::Span<const AffineExpression> demands);

  // Calls InitDecomposedEnergies on all SchedulingDemandHelper created.
  void InitAllDecomposedEnergies();

  // Assuming a and b cannot overlap if they are present, this create a new
  // literal such that:
  // - literal & presences => a is before b.
  // - not(literal) & presences => b is before a.
  // - not present => literal @ true for disallowing multiple solutions.
  //
  // If such literal already exists this returns it.
  void CreateDisjunctivePrecedenceLiteral(IntervalVariable a,
                                          IntervalVariable b);
  LiteralIndex GetOrCreateDisjunctivePrecedenceLiteralIfNonTrivial(
      const IntervalDefinition& a, const IntervalDefinition& b);

  // Creates a literal l <=> y >= x.
  // Returns true if such literal is "non-trivial" and was created.
  bool CreatePrecedenceLiteralIfNonTrivial(AffineExpression x,
                                           AffineExpression y);

  // Returns a literal l <=> y >= x if it exist or kNoLiteralIndex
  // otherwise. This could be the one created by
  // CreateDisjunctivePrecedenceLiteral() or
  // CreatePrecedenceLiteralIfNonTrivial().
  LiteralIndex GetPrecedenceLiteral(AffineExpression x,
                                    AffineExpression y) const;

  // Combines the two calls. Note that we will only create literals when the
  // relation is not known.
  Literal GetOrCreatePrecedenceLiteral(AffineExpression x, AffineExpression y);

  const std::vector<SchedulingConstraintHelper*>& AllDisjunctiveHelpers()
      const {
    return disjunctive_helpers_;
  }

  // We register cumulative at load time so that our search heuristic can loop
  // over all cumulative constraints easily.
  struct CumulativeHelper {
    AffineExpression capacity;
    SchedulingConstraintHelper* task_helper;
    SchedulingDemandHelper* demand_helper;
  };
  void RegisterCumulative(CumulativeHelper helper) {
    cumulative_helpers_.push_back(helper);
  }
  const std::vector<CumulativeHelper>& AllCumulativeHelpers() const {
    return cumulative_helpers_;
  }

 private:
  // External classes needed.
  Model* model_;
  const VariablesAssignment& assignment_;
  SatSolver* sat_solver_;
  BinaryImplicationGraph* implications_;
  IntegerTrail* integer_trail_;
  ReifiedLinear2Bounds* reified_precedences_;

  // Literal indicating if the tasks is executed. Tasks that are always executed
  // will have a kNoLiteralIndex entry in this vector.
  util_intops::StrongVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  util_intops::StrongVector<IntervalVariable, AffineExpression> starts_;
  util_intops::StrongVector<IntervalVariable, AffineExpression> ends_;
  util_intops::StrongVector<IntervalVariable, AffineExpression> sizes_;

  // We can share the helper for all the propagators that work on the same set
  // of intervals.
  absl::flat_hash_map<std::vector<IntervalVariable>,
                      SchedulingConstraintHelper*>
      helper_repository_;
  absl::flat_hash_map<
      std::pair<std::vector<IntervalVariable>, std::vector<IntervalVariable>>,
      NoOverlap2DConstraintHelper*>
      no_overlap_2d_helper_repository_;
  absl::flat_hash_map<
      std::pair<SchedulingConstraintHelper*, std::vector<AffineExpression>>,
      SchedulingDemandHelper*>
      demand_helper_repository_;

  // Disjunctive precedences.
  absl::flat_hash_map<std::pair<IntervalDefinition, IntervalDefinition>,
                      Literal>
      disjunctive_precedences_;

  // Disjunctive/Cumulative helpers_.
  std::vector<SchedulingConstraintHelper*> disjunctive_helpers_;
  std::vector<CumulativeHelper> cumulative_helpers_;
};

// =============================================================================
// Model based functions.
// =============================================================================

inline std::function<int64_t(const Model&)> MinSize(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->MinSize(v).value();
  };
}

inline std::function<int64_t(const Model&)> MaxSize(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->MaxSize(v).value();
  };
}

inline std::function<bool(const Model&)> IsOptional(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->IsOptional(v);
  };
}

inline std::function<Literal(const Model&)> IsPresentLiteral(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->PresenceLiteral(v);
  };
}

inline std::function<IntervalVariable(Model*)> NewInterval(int64_t min_start,
                                                           int64_t max_end,
                                                           int64_t size) {
  return [=](Model* model) {
    CHECK_LE(min_start + size, max_end);
    const IntegerVariable start =
        model->Add(NewIntegerVariable(min_start, max_end - size));
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        AffineExpression(start),
        AffineExpression(start, IntegerValue(1), IntegerValue(size)),
        AffineExpression(IntegerValue(size)), kNoLiteralIndex,
        /*add_linear_relation=*/false);
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
    int64_t min_start, int64_t max_end, int64_t min_size, int64_t max_size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        kNoLiteralIndex);
  };
}

// Note that this should only be used in tests.
inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    int64_t min_start, int64_t max_end, int64_t size, Literal is_present) {
  return [=](Model* model) {
    CHECK_LE(min_start + size, max_end);
    const IntegerVariable start =
        model->Add(NewIntegerVariable(min_start, max_end - size));
    const IntervalVariable interval =
        model->GetOrCreate<IntervalsRepository>()->CreateInterval(
            AffineExpression(start),
            AffineExpression(start, IntegerValue(1), IntegerValue(size)),
            AffineExpression(IntegerValue(size)), is_present.Index(),
            /*add_linear_relation=*/false);

    // To not have too many solutions during enumeration, we force the
    // start at its min value for absent interval.
    model->Add(Implication({is_present.Negated()},
                           IntegerLiteral::LowerOrEqual(start, min_start)));
    return interval;
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

inline std::function<IntervalVariable(Model*)>
NewOptionalIntervalWithVariableSize(int64_t min_start, int64_t max_end,
                                    int64_t min_size, int64_t max_size,
                                    Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        is_present.Index());
  };
}

void AppendVariablesFromCapacityAndDemands(
    const AffineExpression& capacity, SchedulingDemandHelper* demands_helper,
    Model* model, std::vector<IntegerVariable>* vars);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTERVALS_H_
