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

// This file implements the core objects of the constraint solver:
// Solver, Search, Queue, ... along with the main resolution loop.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/alldiff_cst.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/count_cst.h"
#include "ortools/constraint_solver/default_search.h"
#include "ortools/constraint_solver/deviation.h"
#include "ortools/constraint_solver/diffn.h"
#include "ortools/constraint_solver/element.h"
#include "ortools/constraint_solver/expr_array.h"
#include "ortools/constraint_solver/expr_cst.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/graph_constraints.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/model_cache.h"
#include "ortools/constraint_solver/pack.h"
#include "ortools/constraint_solver/range_cst.h"
#include "ortools/constraint_solver/table.h"
#include "ortools/constraint_solver/timetabling.h"
#include "ortools/constraint_solver/trace.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/string_array.h"
#include "ortools/util/tuple_set.h"

#if defined(_MSC_VER)  // WINDOWS
#pragma warning(disable : 4351 4355)
#endif

// Declare the flag to ensure it's visible in this file.
// Although constraint_solver.h is included, this can sometimes resolve
// "undeclared identifier" errors in certain build configurations.
ABSL_DECLARE_FLAG(bool, cp_share_int_consts);
ABSL_DECLARE_FLAG(bool, cp_disable_element_cache);
ABSL_DECLARE_FLAG(bool, cp_disable_expression_optimization);

namespace operations_research {

// ---------- Solver Factory ----------

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars) {
  return MakeAllDifferent(vars, true);
}

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars,
                                     bool stronger_propagation) {
  const int size = vars.size();
  for (int i = 0; i < size; ++i) {
    CHECK_EQ(this, vars[i]->solver());
  }
  if (size < 2) {
    return MakeTrueConstraint();
  } else if (size == 2) {
    return MakeNonEquality(const_cast<IntVar* const>(vars[0]),
                           const_cast<IntVar* const>(vars[1]));
  } else {
    if (stronger_propagation) {
      return RevAlloc(new BoundsAllDifferent(this, vars));
    } else {
      return RevAlloc(new ValueAllDifferent(this, vars));
    }
  }
}

Constraint* Solver::MakeSortingConstraint(const std::vector<IntVar*>& vars,
                                          const std::vector<IntVar*>& sorted) {
  CHECK_EQ(vars.size(), sorted.size());
  return RevAlloc(new SortConstraint(this, vars, sorted));
}

Constraint* Solver::MakeAllDifferentExcept(const std::vector<IntVar*>& vars,
                                           int64_t escape_value) {
  int escape_candidates = 0;
  for (int i = 0; i < vars.size(); ++i) {
    escape_candidates += (vars[i]->Contains(escape_value));
  }
  if (escape_candidates <= 1) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(new AllDifferentExcept(this, vars, escape_value));
  }
}

Constraint* Solver::MakeNullIntersect(const std::vector<IntVar*>& first_vars,
                                      const std::vector<IntVar*>& second_vars) {
  return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars));
}

Constraint* Solver::MakeNullIntersectExcept(
    const std::vector<IntVar*>& first_vars,
    const std::vector<IntVar*>& second_vars, int64_t escape_value) {
  int first_escape_candidates = 0;
  for (int i = 0; i < first_vars.size(); ++i) {
    first_escape_candidates += (first_vars[i]->Contains(escape_value));
  }
  int second_escape_candidates = 0;
  for (int i = 0; i < second_vars.size(); ++i) {
    second_escape_candidates += (second_vars[i]->Contains(escape_value));
  }
  if (first_escape_candidates == 0 || second_escape_candidates == 0) {
    return RevAlloc(
        new NullIntersectArrayExcept(this, first_vars, second_vars));
  } else {
    return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars,
                                                 escape_value));
  }
}

Constraint* Solver::MakeTrueConstraint() {
  DCHECK(true_constraint_ != nullptr);
  return true_constraint_;
}

Constraint* Solver::MakeFalseConstraint() {
  DCHECK(false_constraint_ != nullptr);
  return false_constraint_;
}

Constraint* Solver::MakeFalseConstraint(const std::string& explanation) {
  return RevAlloc(new FalseConstraint(this, explanation));
}

Constraint* Solver::MakeMapDomain(IntVar* var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives));
}

Constraint* Solver::MakeLexicalLess(const std::vector<IntVar*>& left,
                                    const std::vector<IntVar*>& right) {
  std::vector<IntVar*> adjusted_left = left;
  adjusted_left.push_back(MakeIntConst(1));
  std::vector<IntVar*> adjusted_right = right;
  adjusted_right.push_back(MakeIntConst(0));
  return MakeLexicalLessOrEqualWithOffsets(
      std::move(adjusted_left), std::move(adjusted_right),
      std::vector<int64_t>(left.size() + 1, 1));
}

Constraint* Solver::MakeLexicalLessOrEqual(const std::vector<IntVar*>& left,
                                           const std::vector<IntVar*>& right) {
  return MakeLexicalLessOrEqualWithOffsets(
      left, right, std::vector<int64_t>(left.size(), 1));
}

Constraint* Solver::MakeLexicalLessOrEqualWithOffsets(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets) {
  return RevAlloc(new LexicalLessOrEqual(this, std::move(left),
                                         std::move(right), std::move(offsets)));
}

Constraint* Solver::MakeIsLexicalLessOrEqualWithOffsetsCt(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets, IntVar* boolvar) {
  std::vector<IntVar*> adjusted_left = std::move(left);
  adjusted_left.insert(adjusted_left.begin(), boolvar);
  std::vector<IntVar*> adjusted_right = std::move(right);
  adjusted_right.insert(adjusted_right.begin(), MakeIntConst(1));
  std::vector<int64_t> adjusted_offsets = std::move(offsets);
  adjusted_offsets.insert(adjusted_offsets.begin(), 1);
  return MakeLexicalLessOrEqualWithOffsets(std::move(adjusted_left),
                                           std::move(adjusted_right),
                                           std::move(adjusted_offsets));
}

Constraint* Solver::MakeInversePermutationConstraint(
    const std::vector<IntVar*>& left, const std::vector<IntVar*>& right) {
  return RevAlloc(new InversePermutationConstraint(this, left, right));
}

Constraint* Solver::MakeIndexOfFirstMaxValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  return RevAlloc(new IndexOfFirstMaxValue(this, index, vars));
}

Constraint* Solver::MakeIndexOfFirstMinValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  std::vector<IntVar*> opp_vars(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    opp_vars[i] = MakeOpposite(vars[i])->Var();
  }
  return RevAlloc(new IndexOfFirstMaxValue(this, index, opp_vars));
}

Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                              int64_t max_count) {
  std::vector<IntVar*> tmp_sum;
  for (int i = 0; i < vars.size(); ++i) {
    if (vars[i]->Contains(value)) {
      if (vars[i]->Bound()) {
        max_count--;
      } else {
        tmp_sum.push_back(MakeIsEqualCstVar(vars[i], value));
      }
    }
  }
  return MakeSumEquality(tmp_sum, max_count);
}

Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                              IntVar* max_count) {
  if (max_count->Bound()) {
    return MakeCount(vars, value, max_count->Min());
  } else {
    std::vector<IntVar*> tmp_sum;
    int64_t num_vars_bound_to_v = 0;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Contains(value)) {
        if (vars[i]->Bound()) {
          ++num_vars_bound_to_v;
        } else {
          tmp_sum.push_back(MakeIsEqualCstVar(vars[i], value));
        }
      }
    }
    return MakeSumEquality(tmp_sum,
                           MakeSum(max_count, -num_vars_bound_to_v)->Var());
  }
}

Constraint* Solver::MakeAtMost(std::vector<IntVar*> vars, int64_t value,
                               int64_t max_count) {
  CHECK_GE(max_count, 0);
  if (max_count >= vars.size()) {
    return MakeTrueConstraint();
  }
  return RevAlloc(new AtMost(this, std::move(vars), value, max_count));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& values,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  CHECK_EQ(values.size(), cards.size());
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }

  // TODO(user) : we can sort values (and cards) before doing the test.
  bool fast = true;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != i) {
      fast = false;
      break;
    }
  }
  for (IntVar* card : cards) {
    CHECK_EQ(this, card->solver());
  }
  if (fast) {
    return RevAlloc(new FastDistribute(this, vars, cards));
  } else {
    return RevAlloc(new Distribute(this, vars, values, cards));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<IntVar*>& cards) {
  return MakeDistribute(vars, ToInt64Vector(values), cards);
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }
  for (IntVar* card : cards) {
    CHECK_EQ(this, card->solver());
  }
  return RevAlloc(new FastDistribute(this, vars, cards));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   int64_t card_min, int64_t card_max,
                                   int64_t card_size) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }
  if (card_min == 0 && card_max >= vsize) {
    return MakeTrueConstraint();
  } else if (card_min > vsize || card_max < 0 || card_max < card_min) {
    return MakeFalseConstraint();
  } else {
    std::vector<int64_t> mins(card_size, card_min);
    std::vector<int64_t> maxes(card_size, card_max);
    return RevAlloc(new BoundedFastDistribute(this, vars, mins, maxes));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& card_min,
                                   const std::vector<int64_t>& card_max) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  int64_t cmax = std::numeric_limits<int64_t>::max();
  int64_t cmin = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < card_max.size(); ++i) {
    cmax = std::min(cmax, card_max[i]);
    cmin = std::max(cmin, card_min[i]);
  }
  if (cmax < 0 || cmin > vsize) {
    return MakeFalseConstraint();
  } else if (cmax >= vsize && cmin == 0) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new BoundedFastDistribute(this, vars, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(card_min), ToInt64Vector(card_max));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& values,
                                   const std::vector<int64_t>& card_min,
                                   const std::vector<int64_t>& card_max) {
  CHECK_NE(vars.size(), 0);
  CHECK_EQ(card_min.size(), values.size());
  CHECK_EQ(card_min.size(), card_max.size());
  if (AreAllOnes(card_min) && AreAllOnes(card_max) &&
      values.size() == vars.size() && IsIncreasingContiguous(values) &&
      IsArrayInRange(vars, values.front(), values.back())) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(
        new BoundedDistribute(this, vars, values, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(values), ToInt64Vector(card_min),
                        ToInt64Vector(card_max));
}

Constraint* Solver::MakeDeviation(const std::vector<IntVar*>& vars,
                                  IntVar* deviation_var, int64_t total_sum) {
  return RevAlloc(new Deviation(this, vars, deviation_var, total_sum));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int64_t> x_size, absl::Span<const int64_t> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int> x_size, absl::Span<const int> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int64_t> x_size, absl::Span<const int64_t> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int> x_size, absl::Span<const int> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}

Pack* Solver::MakePack(const std::vector<IntVar*>& vars, int number_of_bins) {
  return RevAlloc(new Pack(this, vars, number_of_bins));
}

namespace {
std::string StringifyEvaluatorBare(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_start, int64_t range_end) {
  std::string out;
  for (int64_t i = range_start; i < range_end; ++i) {
    if (i != range_start) {
      out += ", ";
    }
    out += absl::StrFormat("%d -> %s", i, evaluator(i)->DebugString());
  }
  return out;
}

std::string StringifyInt64ToIntVar(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_begin, int64_t range_end) {
  std::string out;
  if (range_end - range_begin > 10) {
    out = absl::StrFormat(
        "IntToIntVar(%s, ...%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_begin + 5),
        StringifyEvaluatorBare(evaluator, range_end - 5, range_end));
  } else {
    out = absl::StrFormat(
        "IntToIntVar(%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_end));
  }
  return out;
}

// ----- Solver::MakeElement(int array, int var) -----
IntExpr* BuildElement(Solver* solver, const std::vector<int64_t>& values,
                      IntVar* index) {
  // Various checks.
  // Is array constant?
  if (IsArrayConstant(values, values[0])) {
    solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
    return solver->MakeIntConst(values[0]);
  }
  // Is array built with booleans only?
  // TODO(user): We could maintain the index of the first one.
  if (IsArrayBoolean(values)) {
    std::vector<int64_t> ones;
    int first_zero = -1;
    for (int i = 0; i < values.size(); ++i) {
      if (values[i] == 1) {
        ones.push_back(i);
      } else {
        first_zero = i;
      }
    }
    if (ones.size() == 1) {
      DCHECK_EQ(int64_t{1}, values[ones.back()]);
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsEqualCstVar(index, ones.back());
    } else if (ones.size() == values.size() - 1) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsDifferentCstVar(index, first_zero);
    } else if (ones.size() == ones.back() - ones.front() + 1) {  // contiguous.
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      IntVar* b = solver->MakeBoolVar("ContiguousBooleanElementVar");
      solver->AddConstraint(
          solver->MakeIsBetweenCt(index, ones.front(), ones.back(), b));
      return b;
    } else {
      IntVar* b = solver->MakeBoolVar("NonContiguousBooleanElementVar");
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      solver->AddConstraint(solver->MakeIsMemberCt(index, ones, b));
      return b;
    }
  }
  IntExpr* cache = nullptr;
  if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
    cache = solver->Cache()->FindVarConstantArrayExpression(
        index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    IntExpr* result = nullptr;
    if (values.size() >= 2 && index->Min() == 0 && index->Max() == 1) {
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (values.size() == 2 && index->Contains(0) && index->Contains(1)) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, 1));
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (IsIncreasingContiguous(values)) {
      result = solver->MakeSum(index, values[0]);
    } else if (IsIncreasing(values)) {
      result = RegisterIntExpr(solver->RevAlloc(
          new IncreasingIntExprElement(solver, values, index)));
    } else {
      if (solver->parameters().use_element_rmq()) {
        result = RegisterIntExpr(solver->RevAlloc(
            new RangeMinimumQueryExprElement(solver, values, index)));
      } else {
        result = RegisterIntExpr(
            solver->RevAlloc(new IntExprElement(solver, values, index)));
      }
    }
    if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
      solver->Cache()->InsertVarConstantArrayExpression(
          result, index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
    }
    return result;
  }
}

// Factory helper.

Constraint* MakeElementEqualityFunc(Solver* solver,
                                    const std::vector<int64_t>& vals,
                                    IntVar* index, IntVar* target) {
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vals.size()) {
      return solver->MakeFalseConstraint();
    } else {
      return solver->MakeEquality(target, vals[val]);
    }
  } else {
    if (IsIncreasingContiguous(vals)) {
      return solver->MakeEquality(target, solver->MakeSum(index, vals[0]));
    } else {
      return solver->RevAlloc(
          new IntElementConstraint(solver, vals, index, target));
    }
  }
}
}  // namespace

Constraint* Solver::MakeIfThenElseCt(IntVar* condition, IntExpr* then_expr,
                                     IntExpr* else_expr, IntVar* target_var) {
  return RevAlloc(
      new IfThenElseCt(this, condition, then_expr, else_expr, target_var));
}

IntExpr* Solver::MakeElement(const std::vector<IntVar*>& vars, IntVar* index) {
  if (index->Bound()) {
    return vars[index->Min()];
  }
  const int size = vars.size();
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(size);
    for (int i = 0; i < size; ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElement(values, index);
  }
  if (index->Size() == 2 && index->Min() + 1 == index->Max() &&
      index->Min() >= 0 && index->Max() < vars.size()) {
    // Let's get the index between 0 and 1.
    IntVar* scaled_index = MakeSum(index, -index->Min())->Var();
    IntVar* zero = vars[index->Min()];
    IntVar* one = vars[index->Max()];
    const std::string name = absl::StrFormat("ElementVar([%s], %s)",
                                             JoinNamePtr(vars), index->name());
    IntVar* target = MakeIntVar(std::min(zero->Min(), one->Min()),
                                std::max(zero->Max(), one->Max()), name);
    AddConstraint(
        RevAlloc(new IfThenElseCt(this, scaled_index, one, zero, target)));
    return target;
  }
  int64_t emin = std::numeric_limits<int64_t>::max();
  int64_t emax = std::numeric_limits<int64_t>::min();
  std::unique_ptr<IntVarIterator> iterator(index->MakeDomainIterator(false));
  for (const int64_t index_value : InitAndGetValues(iterator.get())) {
    if (index_value >= 0 && index_value < size) {
      emin = std::min(emin, vars[index_value]->Min());
      emax = std::max(emax, vars[index_value]->Max());
    }
  }
  const std::string vname =
      size > 10 ? absl::StrFormat("ElementVar(var array of size %d, %s)", size,
                                  index->DebugString())
                : absl::StrFormat("ElementVar([%s], %s)", JoinNamePtr(vars),
                                  index->name());
  IntVar* element_var = MakeIntVar(emin, emax, vname);
  AddConstraint(
      RevAlloc(new IntExprArrayElementCt(this, vars, index, element_var)));
  return element_var;
}

IntExpr* Solver::MakeElement(Int64ToIntVar vars, int64_t range_start,
                             int64_t range_end, IntVar* argument) {
  const std::string index_name =
      !argument->name().empty() ? argument->name() : argument->DebugString();
  const std::string vname = absl::StrFormat(
      "ElementVar(%s, %s)",
      StringifyInt64ToIntVar(vars, range_start, range_end), index_name);
  IntVar* element_var = MakeIntVar(std::numeric_limits<int64_t>::min(),
                                   std::numeric_limits<int64_t>::max(), vname);
  IntExprEvaluatorElementCt* evaluation_ct = new IntExprEvaluatorElementCt(
      this, std::move(vars), range_start, range_end, argument, element_var);
  AddConstraint(RevAlloc(evaluation_ct));
  evaluation_ct->Propagate();
  return element_var;
}

Constraint* Solver::MakeElementEquality(const std::vector<int64_t>& vals,
                                        IntVar* index, IntVar* target) {
  return MakeElementEqualityFunc(this, vals, index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<int>& vals,
                                        IntVar* index, IntVar* target) {
  return MakeElementEqualityFunc(this, ToInt64Vector(vals), index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* index, IntVar* target) {
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(vars.size());
    for (int i = 0; i < vars.size(); ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElementEquality(values, index, target);
  }
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vars.size()) {
      return MakeFalseConstraint();
    } else {
      return MakeEquality(target, vars[val]);
    }
  } else {
    if (target->Bound()) {
      return RevAlloc(
          new IntExprArrayElementCstCt(this, vars, index, target->Min()));
    } else {
      return RevAlloc(new IntExprArrayElementCt(this, vars, index, target));
    }
  }
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* index, int64_t target) {
  if (AreAllBound(vars)) {
    std::vector<int> valid_indices;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Value() == target) {
        valid_indices.push_back(i);
      }
    }
    return MakeMemberCt(index, valid_indices);
  }
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprArrayElementCstCt(this, vars, index, target));
  }
}

Constraint* Solver::MakeIndexOfConstraint(const std::vector<IntVar*>& vars,
                                          IntVar* index, int64_t target) {
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprIndexOfCt(this, vars, index, target));
  }
}

IntExpr* Solver::MakeIndexExpression(const std::vector<IntVar*>& vars,
                                     int64_t value) {
  IntExpr* cache = model_cache_->FindVarArrayConstantExpression(
      vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    const std::string name =
        absl::StrFormat("Index(%s, %d)", JoinNamePtr(vars), value);
    IntVar* index = MakeIntVar(0, vars.size() - 1, name);
    AddConstraint(MakeIndexOfConstraint(vars, index, value));
    model_cache_->InsertVarArrayConstantExpression(
        index, vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
    return index;
  }
}

IntExpr* Solver::MakeElement(const std::vector<int64_t>& values,
                             IntVar* index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, values, index);
}

IntExpr* Solver::MakeElement(const std::vector<int>& values, IntVar* index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, ToInt64Vector(values), index);
}

IntExpr* Solver::MakeElement(Solver::IndexEvaluator1 values, IntVar* index) {
  CHECK_EQ(this, index->solver());
  return RegisterIntExpr(
      RevAlloc(new IntExprFunctionElement(this, std::move(values), index)));
}

IntExpr* Solver::MakeMonotonicElement(Solver::IndexEvaluator1 values,
                                      bool increasing, IntVar* index) {
  CHECK_EQ(this, index->solver());
  if (increasing) {
    return RegisterIntExpr(RevAlloc(
        new IncreasingIntExprFunctionElement(this, std::move(values), index)));
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new IncreasingIntExprFunctionElement(
            this,
            [values = std::move(values)](int64_t i) { return -values(i); },
            index))));
  }
}

IntExpr* Solver::MakeElement(Solver::IndexEvaluator2 values, IntVar* index1,
                             IntVar* index2) {
  CHECK_EQ(this, index1->solver());
  CHECK_EQ(this, index2->solver());
  return RegisterIntExpr(RevAlloc(
      new IntIntExprFunctionElement(this, std::move(values), index1, index2)));
}

DecisionBuilder* Solver::MakeDefaultPhase(const std::vector<IntVar*>& vars) {
  DefaultPhaseParameters parameters;
  return MakeDefaultPhase(vars, parameters);
}

DecisionBuilder* Solver::MakeDefaultPhase(
    const std::vector<IntVar*>& vars,
    const DefaultPhaseParameters& parameters) {
  return RevAlloc(new DefaultIntegerSearch(this, vars, parameters));
}

Demon* Solver::RegisterDemon(Demon* demon) {
  CHECK(demon != nullptr);
  if (InstrumentsDemons()) {
    propagation_monitor_->RegisterDemon(demon);
  }
  return demon;
}

Constraint* Solver::MakeEquality(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

Constraint* Solver::MakeEquality(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreater(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

Constraint* Solver::MakeGreater(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLess(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

Constraint* Solver::MakeLess(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

Constraint* Solver::MakeNonEquality(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}

Constraint* Solver::MakeNonEquality(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}

IntVar* Solver::MakeIsEqualCstVar(IntExpr* var, int64_t value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualVar(left, MakeSum(right, value));
  }
  if (CapSub(var->Max(), var->Min()) == 1) {
    if (value == var->Min()) {
      return MakeDifference(value + 1, var)->Var();
    } else if (value == var->Max()) {
      return MakeSum(var, -value + 1)->Var();
    } else {
      return MakeIntConst(0);
    }
  }
  if (var->IsVar()) {
    return var->Var()->IsEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s == %d)", var->DebugString(), value));
    AddConstraint(MakeIsEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCstCt(IntExpr* var, int64_t value,
                                     IntVar* boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
      return MakeEquality(MakeDifference(value + 1, var), boolvar);
    }
    return MakeIsLessOrEqualCstCt(var, value, boolvar);
  }
  if (value == var->Max()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
      return MakeEquality(MakeSum(var, -value + 1), boolvar);
    }
    return MakeIsGreaterOrEqualCstCt(var, value, boolvar);
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeNonEquality(var, value);
    } else {
      return MakeEquality(var, value);
    }
  }
  // TODO(user) : what happens if the constraint is not posted?
  // The cache becomes tainted.
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsEqualCstCt(this, var->Var(), value, boolvar));
  }
}

IntVar* Solver::MakeIsDifferentCstVar(IntExpr* var, int64_t value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentVar(left, MakeSum(right, value));
  }
  return var->Var()->IsDifferent(value);
}

Constraint* Solver::MakeIsDifferentCstCt(IntExpr* var, int64_t value,
                                         IntVar* boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstCt(var, value + 1, boolvar);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstCt(var, value - 1, boolvar);
  }
  if (var->IsVar() && !var->Var()->Contains(value)) {
    return MakeEquality(boolvar, int64_t{1});
  }
  if (var->Bound() && var->Min() == value) {
    return MakeEquality(boolvar, Zero());
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeEquality(var, value);
    } else {
      return MakeNonEquality(var, value);
    }
  }
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_NOT_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsDiffCstCt(this, var->Var(), value, boolvar));
  }
}

IntVar* Solver::MakeIsGreaterOrEqualCstVar(IntExpr* var, int64_t value) {
  if (var->Min() >= value) {
    return MakeIntConst(int64_t{1});
  }
  if (var->Max() < value) {
    return MakeIntConst(int64_t{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsGreaterOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s >= %d)", var->DebugString(), value));
    AddConstraint(MakeIsGreaterOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsGreaterCstVar(IntExpr* var, int64_t value) {
  return MakeIsGreaterOrEqualCstVar(var, value + 1);
}

Constraint* Solver::MakeIsGreaterOrEqualCstCt(IntExpr* var, int64_t value,
                                              IntVar* boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeLess(var, value);
    } else {
      return MakeGreaterOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
  return RevAlloc(new IsGreaterEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsGreaterCstCt(IntExpr* v, int64_t c, IntVar* b) {
  return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
}

IntVar* Solver::MakeIsLessOrEqualCstVar(IntExpr* var, int64_t value) {
  if (var->Max() <= value) {
    return MakeIntConst(int64_t{1});
  }
  if (var->Min() > value) {
    return MakeIntConst(int64_t{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsLessOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s <= %d)", var->DebugString(), value));
    AddConstraint(MakeIsLessOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsLessCstVar(IntExpr* var, int64_t value) {
  return MakeIsLessOrEqualCstVar(var, value - 1);
}

Constraint* Solver::MakeIsLessOrEqualCstCt(IntExpr* var, int64_t value,
                                           IntVar* boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeGreater(var, value);
    } else {
      return MakeLessOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_LESS_OR_EQUAL);
  return RevAlloc(new IsLessEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsLessCstCt(IntExpr* v, int64_t c, IntVar* b) {
  return MakeIsLessOrEqualCstCt(v, c - 1, b);
}

Constraint* Solver::MakeBetweenCt(IntExpr* expr, int64_t l, int64_t u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeFalseConstraint();
    return MakeEquality(expr, l);
  }
  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeFalseConstraint();
  if (emin >= l && emax <= u) return MakeTrueConstraint();
  // Catch one-sided constraints.
  if (emax <= u) return MakeGreaterOrEqual(expr, l);
  if (emin >= l) return MakeLessOrEqual(expr, u);
  // Simplify the common factor, if any.
  int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff));
  } else {
    // No further reduction is possible.
    return RevAlloc(new BetweenCt(this, expr, l, u));
  }
}

Constraint* Solver::MakeNotBetweenCt(IntExpr* expr, int64_t l, int64_t u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty interval.
  if (l > u) {
    return MakeTrueConstraint();
  }

  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeTrueConstraint();
  if (emin >= l && emax <= u) return MakeFalseConstraint();
  // Catch one-sided constraints.
  if (emin >= l) return MakeGreater(expr, u);
  if (emax <= u) return MakeLess(expr, l);
  // TODO(user): Add back simplification code if expr is constant *
  // other_expr.
  return RevAlloc(new NotBetweenCt(this, expr, l, u));
}

Constraint* Solver::MakeIsBetweenCt(IntExpr* expr, int64_t l, int64_t u,
                                    IntVar* b) {
  CHECK_EQ(this, expr->solver());
  CHECK_EQ(this, b->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeEquality(b, Zero());
    return MakeIsEqualCstCt(expr, l, b);
  }
  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeEquality(b, Zero());
  if (emin >= l && emax <= u) return MakeEquality(b, 1);
  // Catch one-sided constraints.
  if (emax <= u) return MakeIsGreaterOrEqualCstCt(expr, l, b);
  if (emin >= l) return MakeIsLessOrEqualCstCt(expr, u, b);
  // Simplify the common factor, if any.
  int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeIsBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff),
                           b);
  } else {
    // No further reduction is possible.
    return RevAlloc(new IsBetweenCt(this, expr, l, u, b));
  }
}

IntVar* Solver::MakeIsBetweenVar(IntExpr* v, int64_t l, int64_t u) {
  CHECK_EQ(this, v->solver());
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsBetweenCt(v, l, u, b));
  return b;
}

Constraint* Solver::MakeMemberCt(IntExpr* expr,
                                 const std::vector<int64_t>& values) {
  const int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return absl::c_find(values, 0) == values.end() ? MakeFalseConstraint()
                                                   : MakeTrueConstraint();
  }
  std::vector<int64_t> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64_t v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64_t emin;
  int64_t emax;
  expr->Range(&emin, &emax);
  for (const int64_t v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeFalseConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    // Note: MakeBetweenCt() has a fast-track for trivially true constraints.
    return MakeBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // NotMemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to list the values *not* allowed.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64_t v : copied_values)
      is_among_input_values[v - emin] = true;
    // We use the zero valued indices of is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64_t v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeNonEquality(expr, copied_values[0]);
    }
    return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use MemberCt. No further reduction is possible.
  return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeMemberCt(IntExpr* expr,
                                 const std::vector<int>& values) {
  return MakeMemberCt(expr, ToInt64Vector(values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    const std::vector<int64_t>& values) {
  const int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return absl::c_find(values, 0) == values.end() ? MakeTrueConstraint()
                                                   : MakeFalseConstraint();
  }
  std::vector<int64_t> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64_t v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64_t emin;
  int64_t emax;
  expr->Range(&emin, &emax);
  for (const int64_t v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeTrueConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeNonEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    return MakeNotBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // MemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to a dense boolean vector.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64_t v : copied_values)
      is_among_input_values[v - emin] = true;
    // Use zero valued indices for is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64_t v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeEquality(expr, copied_values[0]);
    }
    return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use NotMemberCt. No further reduction is possible.
  return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    const std::vector<int>& values) {
  return MakeNotMemberCt(expr, ToInt64Vector(values));
}

Constraint* Solver::MakeIsMemberCt(IntExpr* expr,
                                   const std::vector<int64_t>& values,
                                   IntVar* boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

Constraint* Solver::MakeIsMemberCt(IntExpr* expr,
                                   const std::vector<int>& values,
                                   IntVar* boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

IntVar* Solver::MakeIsMemberVar(IntExpr* expr,
                                const std::vector<int64_t>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntExpr* expr, const std::vector<int>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr, std::vector<int64_t> starts,
                                    std::vector<int64_t> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr, std::vector<int> starts,
                                    std::vector<int> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    SortedDisjointIntervalList intervals) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), std::move(intervals)));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler,
                                bool assume_paths) {
  CHECK_EQ(nexts.size(), active.size());
  if (sink_handler == nullptr) {
    const int64_t size = nexts.size();
    sink_handler = [size](int64_t index) { return index >= size; };
  }
  return RevAlloc(
      new NoCycle(this, nexts, active, std::move(sink_handler), assume_paths));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler) {
  return MakeNoCycle(nexts, active, std::move(sink_handler), true);
}

// TODO(user): Merge NoCycle and Circuit.
Constraint* Solver::MakeCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, false));
}

Constraint* Solver::MakeSubCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, true));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new PathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2PathCumul(this, nexts, active, cumuls,
                                               std::move(transit_evaluator)));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& slacks,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2SlackPathCumul(
      this, nexts, active, cumuls, slacks, std::move(transit_evaluator)));
}

Constraint* Solver::MakeDelayedPathCumul(const std::vector<IntVar*>& nexts,
                                         const std::vector<IntVar*>& active,
                                         const std::vector<IntVar*>& cumuls,
                                         const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new DelayedPathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathConnected(std::vector<IntVar*> nexts,
                                      std::vector<int64_t> sources,
                                      std::vector<int64_t> sinks,
                                      std::vector<IntVar*> status) {
  return RevAlloc(new PathConnectedConstraint(
      this, std::move(nexts), sources, std::move(sinks), std::move(status)));
}

Constraint* Solver::MakePathPrecedenceConstraint(
    std::vector<IntVar*> nexts,
    const std::vector<std::pair<int, int>>& precedences) {
  return MakePathTransitPrecedenceConstraint(std::move(nexts), {}, precedences);
}

Constraint* Solver::MakePathPrecedenceConstraint(
    std::vector<IntVar*> nexts,
    const std::vector<std::pair<int, int>>& precedences,
    absl::Span<const int> lifo_path_starts,
    absl::Span<const int> fifo_path_starts) {
  absl::flat_hash_map<int, PathTransitPrecedenceConstraint::PrecedenceType>
      precedence_types;
  for (int start : lifo_path_starts) {
    precedence_types[start] = PathTransitPrecedenceConstraint::LIFO;
  }
  for (int start : fifo_path_starts) {
    precedence_types[start] = PathTransitPrecedenceConstraint::FIFO;
  }
  return MakePathTransitTypedPrecedenceConstraint(
      this, std::move(nexts), {}, precedences, std::move(precedence_types));
}

Constraint* Solver::MakePathTransitPrecedenceConstraint(
    std::vector<IntVar*> nexts, std::vector<IntVar*> transits,
    const std::vector<std::pair<int, int>>& precedences) {
  return MakePathTransitTypedPrecedenceConstraint(
      this, std::move(nexts), std::move(transits), precedences, {{}});
}

Constraint* Solver::MakePathEnergyCostConstraint(
    PathEnergyCostConstraintSpecification specification) {
  return RevAlloc(new PathEnergyCostConstraint(this, std::move(specification)));
}

namespace {
void DeepLinearize(Solver* solver, const std::vector<IntVar*>& pre_vars,
                   absl::Span<const int64_t> pre_coefs,
                   std::vector<IntVar*>* vars, std::vector<int64_t>* coefs,
                   int64_t* constant) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);
  CHECK(coefs != nullptr);
  CHECK(constant != nullptr);
  *constant = 0;
  vars->reserve(pre_vars.size());
  coefs->reserve(pre_coefs.size());
  // Try linear scan of the variables to check if there is nothing to do.
  bool need_linearization = false;
  for (int i = 0; i < pre_vars.size(); ++i) {
    IntVar* const variable = pre_vars[i];
    const int64_t coefficient = pre_coefs[i];
    if (variable->Bound()) {
      *constant = CapAdd(*constant, CapProd(coefficient, variable->Min()));
    } else if (solver->CastExpression(variable) == nullptr) {
      vars->push_back(variable);
      coefs->push_back(coefficient);
    } else {
      need_linearization = true;
      vars->clear();
      coefs->clear();
      break;
    }
  }
  if (need_linearization) {
    // Introspect the variables to simplify the sum.
    absl::flat_hash_map<IntVar*, int64_t> variables_to_coefficients;
    ExprLinearizer linearizer(&variables_to_coefficients);
    for (int i = 0; i < pre_vars.size(); ++i) {
      linearizer.Visit(pre_vars[i], pre_coefs[i]);
    }
    *constant = linearizer.Constant();
    for (const auto& variable_to_coefficient : variables_to_coefficients) {
      if (variable_to_coefficient.second != 0) {
        vars->push_back(variable_to_coefficient.first);
        coefs->push_back(variable_to_coefficient.second);
      }
    }
  }
}

Constraint* MakeScalProdEqualityFct(Solver* solver,
                                    const std::vector<IntVar*>& pre_vars,
                                    absl::Span<const int64_t> pre_coefs,
                                    int64_t cst) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  cst = CapSub(cst, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull(coefs)) {
    return cst == 0 ? solver->MakeTrueConstraint()
                    : solver->MakeFalseConstraint();
  }
  if (AreAllBoundOrNull(vars, coefs)) {
    int64_t sum = 0;
    for (int i = 0; i < size; ++i) {
      sum = CapAdd(sum, CapProd(coefs[i], vars[i]->Min()));
    }
    return sum == cst ? solver->MakeTrueConstraint()
                      : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumEquality(vars, cst);
  }
  if (AreAllBooleans(vars) && size > 2) {
    if (AreAllPositive(coefs)) {
      return solver->RevAlloc(
          new PositiveBooleanScalProdEqCst(solver, vars, coefs, cst));
    }
    if (AreAllNegative(coefs)) {
      std::vector<int64_t> opp_coefs(coefs.size());
      for (int i = 0; i < coefs.size(); ++i) {
        opp_coefs[i] = -coefs[i];
      }
      return solver->RevAlloc(
          new PositiveBooleanScalProdEqCst(solver, vars, opp_coefs, -cst));
    }
  }

  // Simplications.
  int constants = 0;
  int positives = 0;
  int negatives = 0;
  for (int i = 0; i < size; ++i) {
    if (coefs[i] == 0 || vars[i]->Bound()) {
      constants++;
    } else if (coefs[i] > 0) {
      positives++;
    } else {
      negatives++;
    }
  }
  if (positives > 0 && negatives > 0) {
    std::vector<IntVar*> pos_terms;
    std::vector<IntVar*> neg_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    if (negatives == 1) {
      if (rhs != 0) {
        pos_terms.push_back(solver->MakeIntConst(-rhs));
      }
      return solver->MakeSumEquality(pos_terms, neg_terms[0]);
    } else if (positives == 1) {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeSumEquality(neg_terms, pos_terms[0]);
    } else {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeEquality(solver->MakeSum(pos_terms),
                                  solver->MakeSum(neg_terms));
    }
  } else if (positives == 1) {
    IntExpr* pos_term = nullptr;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_term = solver->MakeProd(vars[i], coefs[i]);
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeEquality(pos_term, rhs);
  } else if (negatives == 1) {
    IntExpr* neg_term = nullptr;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_term = solver->MakeProd(vars[i], -coefs[i]);
      }
    }
    return solver->MakeEquality(neg_term, -rhs);
  } else if (positives > 1) {
    std::vector<IntVar*> pos_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeSumEquality(pos_terms, rhs);
  } else if (negatives > 1) {
    std::vector<IntVar*> neg_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    return solver->MakeSumEquality(neg_terms, -rhs);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumEquality(terms, solver->MakeIntConst(cst));
}

Constraint* MakeScalProdEqualityVarFct(Solver* solver,
                                       const std::vector<IntVar*>& pre_vars,
                                       absl::Span<const int64_t> pre_coefs,
                                       IntVar* target) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return solver->MakeEquality(target, constant);
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumEquality(vars,
                                   solver->MakeSum(target, -constant)->Var());
  }
  if (AreAllBooleans(vars) && AreAllPositive<int64_t>(coefs)) {
    // TODO(user) : bench BooleanScalProdEqVar with IntConst.
    return solver->RevAlloc(new PositiveBooleanScalProdEqVar(
        solver, vars, coefs, solver->MakeSum(target, -constant)->Var()));
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumEquality(terms,
                                 solver->MakeSum(target, -constant)->Var());
}

Constraint* MakeScalProdGreaterOrEqualFct(Solver* solver,
                                          const std::vector<IntVar*>& pre_vars,
                                          absl::Span<const int64_t> pre_coefs,
                                          int64_t cst) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  cst = CapSub(cst, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return cst <= 0 ? solver->MakeTrueConstraint()
                    : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumGreaterOrEqual(vars, cst);
  }
  if (cst == 1 && AreAllBooleans(vars) && AreAllPositive(coefs)) {
    // can move all coefs to 1.
    std::vector<IntVar*> terms;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] > 0) {
        terms.push_back(vars[i]);
      }
    }
    return solver->MakeSumGreaterOrEqual(terms, 1);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumGreaterOrEqual(terms, cst);
}

Constraint* MakeScalProdLessOrEqualFct(Solver* solver,
                                       const std::vector<IntVar*>& pre_vars,
                                       absl::Span<const int64_t> pre_coefs,
                                       int64_t upper_bound) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  upper_bound = CapSub(upper_bound, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return upper_bound >= 0 ? solver->MakeTrueConstraint()
                            : solver->MakeFalseConstraint();
  }
  // TODO(user) : compute constant on the fly.
  if (AreAllBoundOrNull(vars, coefs)) {
    int64_t cst = 0;
    for (int i = 0; i < size; ++i) {
      cst = CapAdd(cst, CapProd(vars[i]->Min(), coefs[i]));
    }
    return cst <= upper_bound ? solver->MakeTrueConstraint()
                              : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumLessOrEqual(vars, upper_bound);
  }
  if (AreAllBooleans(vars) && AreAllPositive<int64_t>(coefs)) {
    return solver->RevAlloc(
        new BooleanScalProdLessConstant(solver, vars, coefs, upper_bound));
  }
  // Some simplifications
  int constants = 0;
  int positives = 0;
  int negatives = 0;
  for (int i = 0; i < size; ++i) {
    if (coefs[i] == 0 || vars[i]->Bound()) {
      constants++;
    } else if (coefs[i] > 0) {
      positives++;
    } else {
      negatives++;
    }
  }
  if (positives > 0 && negatives > 0) {
    std::vector<IntVar*> pos_terms;
    std::vector<IntVar*> neg_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    if (negatives == 1) {
      IntExpr* const neg_term = solver->MakeSum(neg_terms[0], rhs);
      return solver->MakeLessOrEqual(solver->MakeSum(pos_terms), neg_term);
    } else if (positives == 1) {
      IntExpr* const pos_term = solver->MakeSum(pos_terms[0], -rhs);
      return solver->MakeGreaterOrEqual(solver->MakeSum(neg_terms), pos_term);
    } else {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeLessOrEqual(solver->MakeSum(pos_terms),
                                     solver->MakeSum(neg_terms));
    }
  } else if (positives == 1) {
    IntExpr* pos_term = nullptr;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_term = solver->MakeProd(vars[i], coefs[i]);
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeLessOrEqual(pos_term, rhs);
  } else if (negatives == 1) {
    IntExpr* neg_term = nullptr;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_term = solver->MakeProd(vars[i], -coefs[i]);
      }
    }
    return solver->MakeGreaterOrEqual(neg_term, -rhs);
  } else if (positives > 1) {
    std::vector<IntVar*> pos_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeSumLessOrEqual(pos_terms, rhs);
  } else if (negatives > 1) {
    std::vector<IntVar*> neg_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    return solver->MakeSumGreaterOrEqual(neg_terms, -rhs);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeLessOrEqual(solver->MakeSum(terms), upper_bound);
}

IntExpr* MakeSumArrayAux(Solver* solver, const std::vector<IntVar*>& vars,
                         int64_t constant) {
  const int size = vars.size();
  DCHECK_GT(size, 2);
  int64_t new_min = 0;
  int64_t new_max = 0;
  for (int i = 0; i < size; ++i) {
    if (new_min != std::numeric_limits<int64_t>::min()) {
      new_min = CapAdd(vars[i]->Min(), new_min);
    }
    if (new_max != std::numeric_limits<int64_t>::max()) {
      new_max = CapAdd(vars[i]->Max(), new_max);
    }
  }
  IntExpr* const cache =
      solver->Cache()->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM);
  if (cache != nullptr) {
    return solver->MakeSum(cache, constant);
  } else {
    const std::string name = absl::StrFormat("Sum([%s])", JoinNamePtr(vars));
    IntVar* const sum_var = solver->MakeIntVar(new_min, new_max, name);
    if (AreAllBooleans(vars)) {
      solver->AddConstraint(
          solver->RevAlloc(new SumBooleanEqualToVar(solver, vars, sum_var)));
    } else if (size <= solver->parameters().array_split_size()) {
      solver->AddConstraint(
          solver->RevAlloc(new SmallSumConstraint(solver, vars, sum_var)));
    } else {
      solver->AddConstraint(
          solver->RevAlloc(new SumConstraint(solver, vars, sum_var)));
    }
    solver->Cache()->InsertVarArrayExpression(sum_var, vars,
                                              ModelCache::VAR_ARRAY_SUM);
    return solver->MakeSum(sum_var, constant);
  }
}

IntExpr* MakeSumAux(Solver* solver, const std::vector<IntVar*>& vars,
                    int64_t constant) {
  const int size = vars.size();
  if (size == 0) {
    return solver->MakeIntConst(constant);
  } else if (size == 1) {
    return solver->MakeSum(vars[0], constant);
  } else if (size == 2) {
    return solver->MakeSum(solver->MakeSum(vars[0], vars[1]), constant);
  } else {
    return MakeSumArrayAux(solver, vars, constant);
  }
}

IntExpr* MakeScalProdAux(Solver* solver, const std::vector<IntVar*>& vars,
                         const std::vector<int64_t>& coefs, int64_t constant) {
  if (AreAllOnes(coefs)) {
    return MakeSumAux(solver, vars, constant);
  }

  const int size = vars.size();
  if (size == 0) {
    return solver->MakeIntConst(constant);
  } else if (size == 1) {
    return solver->MakeSum(solver->MakeProd(vars[0], coefs[0]), constant);
  } else if (size == 2) {
    if (coefs[0] > 0 && coefs[1] < 0) {
      return solver->MakeSum(
          solver->MakeDifference(solver->MakeProd(vars[0], coefs[0]),
                                 solver->MakeProd(vars[1], -coefs[1])),
          constant);
    } else if (coefs[0] < 0 && coefs[1] > 0) {
      return solver->MakeSum(
          solver->MakeDifference(solver->MakeProd(vars[1], coefs[1]),
                                 solver->MakeProd(vars[0], -coefs[0])),
          constant);
    } else {
      return solver->MakeSum(
          solver->MakeSum(solver->MakeProd(vars[0], coefs[0]),
                          solver->MakeProd(vars[1], coefs[1])),
          constant);
    }
  } else {
    if (AreAllBooleans(vars)) {
      if (AreAllPositive(coefs)) {
        if (vars.size() > 8) {
          return solver->MakeSum(
              RegisterIntExpr(solver->RevAlloc(new PositiveBooleanScalProd(
                                  solver, vars, coefs)))
                  ->Var(),
              constant);
        } else {
          return solver->MakeSum(
              RegisterIntExpr(solver->RevAlloc(
                  new PositiveBooleanScalProd(solver, vars, coefs))),
              constant);
        }
      } else {
        // If some coefficients are non-positive, partition coefficients in two
        // sets, one for the positive coefficients P and one for the negative
        // ones N.
        // Create two PositiveBooleanScalProd expressions, one on P (s1), the
        // other on Opposite(N) (s2).
        // The final expression is then s1 - s2.
        // If P is empty, the expression is Opposite(s2).
        std::vector<int64_t> positive_coefs;
        std::vector<int64_t> negative_coefs;
        std::vector<IntVar*> positive_coef_vars;
        std::vector<IntVar*> negative_coef_vars;
        for (int i = 0; i < size; ++i) {
          const int coef = coefs[i];
          if (coef > 0) {
            positive_coefs.push_back(coef);
            positive_coef_vars.push_back(vars[i]);
          } else if (coef < 0) {
            negative_coefs.push_back(-coef);
            negative_coef_vars.push_back(vars[i]);
          }
        }
        CHECK_GT(negative_coef_vars.size(), 0);
        IntExpr* const negatives =
            MakeScalProdAux(solver, negative_coef_vars, negative_coefs, 0);
        if (!positive_coef_vars.empty()) {
          IntExpr* const positives = MakeScalProdAux(solver, positive_coef_vars,
                                                     positive_coefs, constant);
          return solver->MakeDifference(positives, negatives);
        } else {
          return solver->MakeDifference(constant, negatives);
        }
      }
    }
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return MakeSumArrayAux(solver, terms, constant);
}

IntExpr* MakeScalProdFct(Solver* solver, const std::vector<IntVar*>& pre_vars,
                         absl::Span<const int64_t> pre_coefs) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);

  if (vars.empty()) {
    return solver->MakeIntConst(constant);
  }
  // Can we simplify using some gcd computation.
  int64_t gcd = std::abs(coefs[0]);
  for (int i = 1; i < coefs.size(); ++i) {
    gcd = std::gcd(gcd, std::abs(coefs[i]));
    if (gcd == 1) {
      break;
    }
  }
  if (constant != 0 && gcd != 1) {
    gcd = std::gcd(gcd, std::abs(constant));
  }
  if (gcd > 1) {
    for (int i = 0; i < coefs.size(); ++i) {
      coefs[i] /= gcd;
    }
    return solver->MakeProd(
        MakeScalProdAux(solver, vars, coefs, constant / gcd), gcd);
  }
  return MakeScalProdAux(solver, vars, coefs, constant);
}

IntExpr* MakeSumFct(Solver* solver, const std::vector<IntVar*>& pre_vars) {
  absl::flat_hash_map<IntVar*, int64_t> variables_to_coefficients;
  ExprLinearizer linearizer(&variables_to_coefficients);
  for (int i = 0; i < pre_vars.size(); ++i) {
    linearizer.Visit(pre_vars[i], 1);
  }
  const int64_t constant = linearizer.Constant();
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  for (const auto& variable_to_coefficient : variables_to_coefficients) {
    if (variable_to_coefficient.second != 0) {
      vars.push_back(variable_to_coefficient.first);
      coefs.push_back(variable_to_coefficient.second);
    }
  }
  return MakeScalProdAux(solver, vars, coefs, constant);
}
}  // namespace

// ----- API -----

IntExpr* Solver::MakeSum(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    return MakeIntConst(int64_t{0});
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeSum(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM);
    if (cache != nullptr) {
      return cache;
    } else {
      int64_t new_min = 0;
      int64_t new_max = 0;
      for (int i = 0; i < size; ++i) {
        if (new_min != std::numeric_limits<int64_t>::min()) {
          new_min = CapAdd(vars[i]->Min(), new_min);
        }
        if (new_max != std::numeric_limits<int64_t>::max()) {
          new_max = CapAdd(vars[i]->Max(), new_max);
        }
      }
      IntExpr* sum_expr = nullptr;
      const bool all_booleans = AreAllBooleans(vars);
      if (all_booleans) {
        const std::string name =
            absl::StrFormat("BooleanSum([%s])", JoinNamePtr(vars));
        sum_expr = MakeIntVar(new_min, new_max, name);
        AddConstraint(
            RevAlloc(new SumBooleanEqualToVar(this, vars, sum_expr->Var())));
      } else if (new_min != std::numeric_limits<int64_t>::min() &&
                 new_max != std::numeric_limits<int64_t>::max()) {
        sum_expr = MakeSumFct(this, vars);
      } else {
        const std::string name =
            absl::StrFormat("Sum([%s])", JoinNamePtr(vars));
        sum_expr = MakeIntVar(new_min, new_max, name);
        AddConstraint(
            RevAlloc(new SafeSumConstraint(this, vars, sum_expr->Var())));
      }
      model_cache_->InsertVarArrayExpression(sum_expr, vars,
                                             ModelCache::VAR_ARRAY_SUM);
      return sum_expr;
    }
  }
}

IntExpr* Solver::MakeMin(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    LOG(WARNING) << "operations_research::Solver::MakeMin() was called with an "
                    "empty list of variables. Was this intentional?";
    return MakeIntConst(std::numeric_limits<int64_t>::max());
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMin(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_MIN);
    if (cache != nullptr) {
      return cache;
    } else {
      if (AreAllBooleans(vars)) {
        IntVar* const new_var = MakeBoolVar();
        AddConstraint(RevAlloc(new ArrayBoolAndEq(this, vars, new_var)));
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      } else {
        int64_t new_min = std::numeric_limits<int64_t>::max();
        int64_t new_max = std::numeric_limits<int64_t>::max();
        for (int i = 0; i < size; ++i) {
          new_min = std::min(new_min, vars[i]->Min());
          new_max = std::min(new_max, vars[i]->Max());
        }
        IntVar* const new_var = MakeIntVar(new_min, new_max);
        if (size <= parameters_.array_split_size()) {
          AddConstraint(RevAlloc(new SmallMinConstraint(this, vars, new_var)));
        } else {
          AddConstraint(RevAlloc(new MinConstraint(this, vars, new_var)));
        }
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      }
    }
  }
}

IntExpr* Solver::MakeMax(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    LOG(WARNING) << "operations_research::Solver::MakeMax() was called with an "
                    "empty list of variables. Was this intentional?";
    return MakeIntConst(std::numeric_limits<int64_t>::min());
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMax(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_MAX);
    if (cache != nullptr) {
      return cache;
    } else {
      if (AreAllBooleans(vars)) {
        IntVar* const new_var = MakeBoolVar();
        AddConstraint(RevAlloc(new ArrayBoolOrEq(this, vars, new_var)));
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      } else {
        int64_t new_min = std::numeric_limits<int64_t>::min();
        int64_t new_max = std::numeric_limits<int64_t>::min();
        for (int i = 0; i < size; ++i) {
          new_min = std::max(new_min, vars[i]->Min());
          new_max = std::max(new_max, vars[i]->Max());
        }
        IntVar* const new_var = MakeIntVar(new_min, new_max);
        if (size <= parameters_.array_split_size()) {
          AddConstraint(RevAlloc(new SmallMaxConstraint(this, vars, new_var)));
        } else {
          AddConstraint(RevAlloc(new MaxConstraint(this, vars, new_var)));
        }
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MAX);
        return new_var;
      }
    }
  }
}

Constraint* Solver::MakeMinEquality(const std::vector<IntVar*>& vars,
                                    IntVar* min_var) {
  const int size = vars.size();
  if (size > 2) {
    if (AreAllBooleans(vars)) {
      return RevAlloc(new ArrayBoolAndEq(this, vars, min_var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallMinConstraint(this, vars, min_var));
    } else {
      return RevAlloc(new MinConstraint(this, vars, min_var));
    }
  } else if (size == 2) {
    return MakeEquality(MakeMin(vars[0], vars[1]), min_var);
  } else if (size == 1) {
    return MakeEquality(vars[0], min_var);
  } else {
    LOG(WARNING) << "operations_research::Solver::MakeMinEquality() was called "
                    "with an empty list of variables. Was this intentional?";
    return MakeEquality(min_var, std::numeric_limits<int64_t>::max());
  }
}

Constraint* Solver::MakeMaxEquality(const std::vector<IntVar*>& vars,
                                    IntVar* max_var) {
  const int size = vars.size();
  if (size > 2) {
    if (AreAllBooleans(vars)) {
      return RevAlloc(new ArrayBoolOrEq(this, vars, max_var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallMaxConstraint(this, vars, max_var));
    } else {
      return RevAlloc(new MaxConstraint(this, vars, max_var));
    }
  } else if (size == 2) {
    return MakeEquality(MakeMax(vars[0], vars[1]), max_var);
  } else if (size == 1) {
    return MakeEquality(vars[0], max_var);
  } else {
    LOG(WARNING) << "operations_research::Solver::MakeMaxEquality() was called "
                    "with an empty list of variables. Was this intentional?";
    return MakeEquality(max_var, std::numeric_limits<int64_t>::min());
  }
}

Constraint* Solver::MakeSumLessOrEqual(const std::vector<IntVar*>& vars,
                                       int64_t cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanLessOrEqualToOne(this, vars));
  } else {
    return MakeLessOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumGreaterOrEqual(const std::vector<IntVar*>& vars,
                                          int64_t cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanGreaterOrEqualToOne(this, vars));
  } else {
    return MakeGreaterOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars,
                                    int64_t cst) {
  const int size = vars.size();
  if (size == 0) {
    return cst == 0 ? MakeTrueConstraint() : MakeFalseConstraint();
  }
  if (AreAllBooleans(vars) && size > 2) {
    if (cst == 1) {
      return RevAlloc(new SumBooleanEqualToOne(this, vars));
    } else if (cst < 0 || cst > size) {
      return MakeFalseConstraint();
    } else {
      return RevAlloc(new SumBooleanEqualToVar(this, vars, MakeIntConst(cst)));
    }
  } else {
    if (vars.size() == 1) {
      return MakeEquality(vars[0], cst);
    } else if (vars.size() == 2) {
      return MakeEquality(vars[0], MakeDifference(cst, vars[1]));
    }
    if (DetectSumOverflow(vars)) {
      return RevAlloc(new SafeSumConstraint(this, vars, MakeIntConst(cst)));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallSumConstraint(this, vars, MakeIntConst(cst)));
    } else {
      return RevAlloc(new SumConstraint(this, vars, MakeIntConst(cst)));
    }
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars,
                                    IntVar* var) {
  const int size = vars.size();
  if (size == 0) {
    return MakeEquality(var, Zero());
  }
  if (AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanEqualToVar(this, vars, var));
  } else if (size == 0) {
    return MakeEquality(var, Zero());
  } else if (size == 1) {
    return MakeEquality(vars[0], var);
  } else if (size == 2) {
    return MakeEquality(MakeSum(vars[0], vars[1]), var);
  } else {
    if (DetectSumOverflow(vars)) {
      return RevAlloc(new SafeSumConstraint(this, vars, var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallSumConstraint(this, vars, var));
    } else {
      return RevAlloc(new SumConstraint(this, vars, var));
    }
  }
}

Constraint* Solver::MakeScalProdEquality(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct(this, vars, coefficients, cst);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct(this, vars, ToInt64Vector(coefficients), cst);
}

Constraint* Solver::MakeScalProdEquality(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    IntVar* target) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityVarFct(this, vars, coefficients, target);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         IntVar* target) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityVarFct(this, vars, ToInt64Vector(coefficients),
                                    target);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coeffs,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct(this, vars, coeffs, cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int>& coeffs,
                                               int64_t cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct(this, vars, ToInt64Vector(coeffs), cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct(this, vars, coefficients, cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct(this, vars, ToInt64Vector(coefficients),
                                    cst);
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int64_t>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct(this, vars, coefs);
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct(this, vars, ToInt64Vector(coefs));
}

IntExpr* Solver::MakeSum(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (right->Bound()) {
    return MakeSum(left, right->Min());
  }
  if (left->Bound()) {
    return MakeSum(right, left->Min());
  }
  if (left == right) {
    return MakeProd(left, 2);
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_SUM);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(right, left,
                                                 ModelCache::EXPR_EXPR_SUM);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    bool may_overflow = false;
    may_overflow |= AddOverflows(left->Max(), right->Max());
    may_overflow |= AddOverflows(left->Min(), right->Min());
    if (!may_overflow) {
      // The result itself would not overflow, but intermediate computations
      // could, needing a safe implementation.
      // For example: l in [kint64min, 0], r in [0, 5].
      // sum->SetMax(4) implies r->SetMax(4 - left->Min()), which overflows.
      const int64_t min_sum = left->Min() + right->Min();
      const int64_t max_sum = left->Max() + right->Max();
      may_overflow |= SubOverflows(max_sum, left->Min());
      may_overflow |= SubOverflows(max_sum, right->Min());
      may_overflow |= SubOverflows(min_sum, left->Max());
      may_overflow |= SubOverflows(min_sum, right->Max());
    }
    IntExpr* const result =
        may_overflow
            ? RegisterIntExpr(RevAlloc(new SafePlusIntExpr(this, left, right)))
            : RegisterIntExpr(RevAlloc(new PlusIntExpr(this, left, right)));
    model_cache_->InsertExprExprExpression(result, left, right,
                                           ModelCache::EXPR_EXPR_SUM);
    return result;
  }
}

IntExpr* Solver::MakeSum(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(CapAdd(expr->Min(), value));
  }
  if (value == 0) {
    return expr;
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_SUM);
  if (result == nullptr) {
    if (expr->IsVar() && !AddOverflows(value, expr->Max()) &&
        !AddOverflows(value, expr->Min())) {
      result = NewVarPlusInt(this, expr->Var(), value);
    } else {
      result = RegisterIntExpr(RevAlloc(new PlusIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_SUM);
  }
  return result;
}

IntExpr* Solver::MakeDifference(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeDifference(left->Min(), right);
  }
  if (right->Bound()) {
    return MakeSum(left, -right->Min());
  }
  IntExpr* sub_left = nullptr;
  IntExpr* sub_right = nullptr;
  int64_t left_coef = 1;
  int64_t right_coef = 1;
  if (IsExprProduct(left, &sub_left, &left_coef) &&
      IsExprProduct(right, &sub_right, &right_coef)) {
    const int64_t abs_gcd = std::gcd(std::abs(left_coef), std::abs(right_coef));
    if (abs_gcd != 0 && abs_gcd != 1) {
      return MakeProd(MakeDifference(MakeProd(sub_left, left_coef / abs_gcd),
                                     MakeProd(sub_right, right_coef / abs_gcd)),
                      abs_gcd);
    }
  }

  IntExpr* result = Cache()->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_DIFFERENCE);
  if (result == nullptr) {
    if (!SubOverflows(left->Min(), right->Max()) &&
        !SubOverflows(left->Max(), right->Min())) {
      result = RegisterIntExpr(RevAlloc(new SubIntExpr(this, left, right)));
    } else {
      result = RegisterIntExpr(RevAlloc(new SafeSubIntExpr(this, left, right)));
    }
    Cache()->InsertExprExprExpression(result, left, right,
                                      ModelCache::EXPR_EXPR_DIFFERENCE);
  }
  return result;
}

// warning: this is 'value - expr'.
IntExpr* Solver::MakeDifference(int64_t value, IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    DCHECK(!SubOverflows(value, expr->Min()));
    return MakeIntConst(value - expr->Min());
  }
  if (value == 0) {
    return MakeOpposite(expr);
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_DIFFERENCE);
  if (result == nullptr) {
    if (expr->IsVar() && expr->Min() != std::numeric_limits<int64_t>::min() &&
        !SubOverflows(value, expr->Min()) &&
        !SubOverflows(value, expr->Max())) {
      result = NewIntMinusVar(this, value, expr->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new SubIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_DIFFERENCE);
  }
  return result;
}

IntExpr* Solver::MakeOpposite(IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(CapOpp(expr->Min()));
  }
  IntExpr* result =
      Cache()->FindExprExpression(expr, ModelCache::EXPR_OPPOSITE);
  if (result == nullptr) {
    if (expr->IsVar()) {
      result = NewIntMinusVar(this, 0, expr->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new OppIntExpr(this, expr)));
    }
    Cache()->InsertExprExpression(result, expr, ModelCache::EXPR_OPPOSITE);
  }
  return result;
}

IntExpr* Solver::MakeProd(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_PROD);
  if (result != nullptr) {
    return result;
  } else {
    IntExpr* m_expr = nullptr;
    int64_t coefficient = 1;
    if (IsExprProduct(expr, &m_expr, &coefficient)) {
      coefficient = CapProd(coefficient, value);
    } else {
      m_expr = expr;
      coefficient = value;
    }
    if (m_expr->Bound()) {
      return MakeIntConst(CapProd(coefficient, m_expr->Min()));
    } else if (coefficient == 1) {
      return m_expr;
    } else if (coefficient == -1) {
      return MakeOpposite(m_expr);
    } else if (coefficient > 0) {
      if (m_expr->Max() > std::numeric_limits<int64_t>::max() / coefficient ||
          m_expr->Min() < std::numeric_limits<int64_t>::min() / coefficient) {
        result = RegisterIntExpr(
            RevAlloc(new SafeTimesPosIntCstExpr(this, m_expr, coefficient)));
      } else {
        result = RegisterIntExpr(
            RevAlloc(new TimesPosIntCstExpr(this, m_expr, coefficient)));
      }
    } else if (coefficient == 0) {
      result = MakeIntConst(0);
    } else {  // coefficient < 0.
      result = RegisterIntExpr(
          RevAlloc(new TimesIntNegCstExpr(this, m_expr, coefficient)));
    }
    if (m_expr->IsVar() &&
        !absl::GetFlag(FLAGS_cp_disable_expression_optimization)) {
      result = result->Var();
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_PROD);
    return result;
  }
}

IntExpr* Solver::MakeProd(IntExpr* left, IntExpr* right) {
  if (left->Bound()) {
    return MakeProd(right, left->Min());
  }

  if (right->Bound()) {
    return MakeProd(left, right->Min());
  }

  // ----- Discover squares and powers -----

  IntExpr* tmp_left = left;
  IntExpr* tmp_right = right;
  int64_t left_exponant = 1;
  int64_t right_exponant = 1;
  IsExprPower(left, &tmp_left, &left_exponant);
  IsExprPower(right, &tmp_right, &right_exponant);

  if (tmp_left == tmp_right) {
    return MakePower(tmp_left, left_exponant + right_exponant);
  }

  // ----- Discover nested products -----

  tmp_left = left;
  tmp_right = right;
  int64_t coefficient = 1;

  // Parse left.
  for (;;) {
    IntExpr* sub_expr = nullptr;
    int64_t sub_coefficient = 1;
    if (IsExprProduct(tmp_left, &sub_expr, &sub_coefficient)) {
      coefficient = CapProd(coefficient, sub_coefficient);
      tmp_left = sub_expr;
    } else {
      break;
    }
  }

  // Parse right.
  for (;;) {
    IntExpr* sub_expr = nullptr;
    int64_t sub_coefficient = 1;
    if (IsExprProduct(tmp_right, &sub_expr, &sub_coefficient)) {
      coefficient = CapProd(coefficient, sub_coefficient);
      tmp_right = sub_expr;
    } else {
      break;
    }
  }

  if (coefficient != 1) {
    return MakeProd(MakeProd(tmp_left, tmp_right), coefficient);
  }

  // ----- Standard build -----

  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  IntExpr* result = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_PROD);
  if (result == nullptr) {
    result = model_cache_->FindExprExprExpression(right, left,
                                                  ModelCache::EXPR_EXPR_PROD);
  }
  if (result != nullptr) {
    return result;
  }
  if (left->IsVar() && left->Var()->VarType() == IntVar::BOOLEAN_VAR) {
    if (right->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    }
  } else if (right->IsVar() && reinterpret_cast<IntVar*>(right)->VarType() ==
                                   IntVar::BOOLEAN_VAR) {
    if (left->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    }
  } else if (left->Min() >= 0 && right->Min() >= 0) {
    if (CapProd(left->Max(), right->Max()) ==
        std::numeric_limits<int64_t>::max()) {  // Potential overflow.
      result =
          RegisterIntExpr(RevAlloc(new SafeTimesPosIntExpr(this, left, right)));
    } else {
      result =
          RegisterIntExpr(RevAlloc(new TimesPosIntExpr(this, left, right)));
    }
  } else {
    result = RegisterIntExpr(RevAlloc(new TimesIntExpr(this, left, right)));
  }
  model_cache_->InsertExprExprExpression(result, left, right,
                                         ModelCache::EXPR_EXPR_PROD);
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* numerator, IntExpr* denominator) {
  CHECK(numerator != nullptr);
  CHECK(denominator != nullptr);
  if (denominator->Bound()) {
    return MakeDiv(numerator, denominator->Min());
  }
  IntExpr* result = model_cache_->FindExprExprExpression(
      numerator, denominator, ModelCache::EXPR_EXPR_DIV);
  if (result != nullptr) {
    return result;
  }

  if (denominator->Min() <= 0 && denominator->Max() >= 0) {
    AddConstraint(MakeNonEquality(denominator, 0));
  }

  if (denominator->Min() >= 0) {
    if (numerator->Min() >= 0) {
      result = RevAlloc(new DivPosPosIntExpr(this, numerator, denominator));
    } else {
      result = RevAlloc(new DivPosIntExpr(this, numerator, denominator));
    }
  } else if (denominator->Max() <= 0) {
    if (numerator->Max() <= 0) {
      result = RevAlloc(new DivPosPosIntExpr(this, MakeOpposite(numerator),
                                             MakeOpposite(denominator)));
    } else {
      result = MakeOpposite(RevAlloc(
          new DivPosIntExpr(this, numerator, MakeOpposite(denominator))));
    }
  } else {
    result = RevAlloc(new DivIntExpr(this, numerator, denominator));
  }
  model_cache_->InsertExprExprExpression(result, numerator, denominator,
                                         ModelCache::EXPR_EXPR_DIV);
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* expr, int64_t value) {
  CHECK(expr != nullptr);
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(expr->Min() / value);
  } else if (value == 1) {
    return expr;
  } else if (value == -1) {
    return MakeOpposite(expr);
  } else if (value > 0) {
    return RegisterIntExpr(RevAlloc(new DivPosIntCstExpr(this, expr, value)));
  } else if (value == 0) {
    LOG(FATAL) << "Cannot divide by 0";
    return nullptr;
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new DivPosIntCstExpr(this, expr, -value))));
    // TODO(user) : implement special case.
  }
}

Constraint* Solver::MakeAbsEquality(IntVar* var, IntVar* abs_var) {
  if (Cache()->FindExprExpression(var, ModelCache::EXPR_ABS) == nullptr) {
    Cache()->InsertExprExpression(abs_var, var, ModelCache::EXPR_ABS);
  }
  return RevAlloc(new IntAbsConstraint(this, var, abs_var));
}

IntExpr* Solver::MakeAbs(IntExpr* e) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= 0) {
    return e;
  } else if (e->Max() <= 0) {
    return MakeOpposite(e);
  }
  IntExpr* result = Cache()->FindExprExpression(e, ModelCache::EXPR_ABS);
  if (result == nullptr) {
    int64_t coefficient = 1;
    IntExpr* expr = nullptr;
    if (IsExprProduct(e, &expr, &coefficient)) {
      result = MakeProd(MakeAbs(expr), std::abs(coefficient));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntAbs(this, e)));
    }
    Cache()->InsertExprExpression(result, e, ModelCache::EXPR_ABS);
  }
  return result;
}

IntExpr* Solver::MakeSquare(IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    const int64_t v = expr->Min();
    return MakeIntConst(v * v);
  }
  IntExpr* result = Cache()->FindExprExpression(expr, ModelCache::EXPR_SQUARE);
  if (result == nullptr) {
    if (expr->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new PosIntSquare(this, expr)));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntSquare(this, expr)));
    }
    Cache()->InsertExprExpression(result, expr, ModelCache::EXPR_SQUARE);
  }
  return result;
}

IntExpr* Solver::MakePower(IntExpr* expr, int64_t n) {
  CHECK_EQ(this, expr->solver());
  CHECK_GE(n, 0);
  if (expr->Bound()) {
    const int64_t v = expr->Min();
    if (v >= IntPowerOverflowLimit(n)) {  // Overflow.
      return MakeIntConst(std::numeric_limits<int64_t>::max());
    }
    return MakeIntConst(IntPowerValue(v, n));
  }
  switch (n) {
    case 0:
      return MakeIntConst(1);
    case 1:
      return expr;
    case 2:
      return MakeSquare(expr);
    default: {
      IntExpr* result = nullptr;
      if (n % 2 == 0) {  // even.
        if (expr->Min() >= 0) {
          result =
              RegisterIntExpr(RevAlloc(new PosIntEvenPower(this, expr, n)));
        } else {
          result = RegisterIntExpr(RevAlloc(new IntEvenPower(this, expr, n)));
        }
      } else {
        result = RegisterIntExpr(RevAlloc(new IntOddPower(this, expr, n)));
      }
      return result;
    }
  }
}

IntExpr* Solver::MakeMin(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeMin(right, left->Min());
  }
  if (right->Bound()) {
    return MakeMin(left, right->Min());
  }
  if (left->Min() >= right->Max()) {
    return right;
  }
  if (right->Min() >= left->Max()) {
    return left;
  }
  return RegisterIntExpr(RevAlloc(new MinIntExpr(this, left, right)));
}

IntExpr* Solver::MakeMin(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (value <= expr->Min()) {
    return MakeIntConst(value);
  }
  if (expr->Bound()) {
    return MakeIntConst(std::min(expr->Min(), value));
  }
  if (expr->Max() <= value) {
    return expr;
  }
  return RegisterIntExpr(RevAlloc(new MinCstIntExpr(this, expr, value)));
}

IntExpr* Solver::MakeMin(IntExpr* expr, int value) {
  return MakeMin(expr, static_cast<int64_t>(value));
}

IntExpr* Solver::MakeMax(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeMax(right, left->Min());
  }
  if (right->Bound()) {
    return MakeMax(left, right->Min());
  }
  if (left->Min() >= right->Max()) {
    return left;
  }
  if (right->Min() >= left->Max()) {
    return right;
  }
  return RegisterIntExpr(RevAlloc(new MaxIntExpr(this, left, right)));
}

IntExpr* Solver::MakeMax(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(std::max(expr->Min(), value));
  }
  if (value <= expr->Min()) {
    return expr;
  }
  if (expr->Max() <= value) {
    return MakeIntConst(value);
  }
  return RegisterIntExpr(RevAlloc(new MaxCstIntExpr(this, expr, value)));
}

IntExpr* Solver::MakeMax(IntExpr* expr, int value) {
  return MakeMax(expr, static_cast<int64_t>(value));
}

IntExpr* Solver::MakeConvexPiecewiseExpr(IntExpr* expr, int64_t early_cost,
                                         int64_t early_date, int64_t late_date,
                                         int64_t late_cost) {
  return RegisterIntExpr(RevAlloc(new SimpleConvexPiecewiseExpr(
      this, expr, early_cost, early_date, late_date, late_cost)));
}

IntExpr* Solver::MakeSemiContinuousExpr(IntExpr* expr, int64_t fixed_charge,
                                        int64_t step) {
  if (step == 0) {
    if (fixed_charge == 0) {
      return MakeIntConst(int64_t{0});
    } else {
      return RegisterIntExpr(
          RevAlloc(new SemiContinuousStepZeroExpr(this, expr, fixed_charge)));
    }
  } else if (step == 1) {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousStepOneExpr(this, expr, fixed_charge)));
  } else {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousExpr(this, expr, fixed_charge, step)));
  }
  // TODO(user) : benchmark with virtualization of
  // PosIntDivDown and PosIntDivUp - or function pointers.
}

IntExpr* Solver::MakePiecewiseLinearExpr(IntExpr* expr,
                                         const PiecewiseLinearFunction& f) {
  return RegisterIntExpr(RevAlloc(new PiecewiseLinearExpr(this, expr, f)));
}

// ----- Conditional Expression -----

IntExpr* Solver::MakeConditionalExpression(IntVar* condition, IntExpr* expr,
                                           int64_t unperformed_value) {
  if (condition->Min() == 1) {
    return expr;
  } else if (condition->Max() == 0) {
    return MakeIntConst(unperformed_value);
  } else {
    IntExpr* cache = Cache()->FindExprExprConstantExpression(
        condition, expr, unperformed_value,
        ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL);
    if (cache == nullptr) {
      cache = RevAlloc(
          new ExprWithEscapeValue(this, condition, expr, unperformed_value));
      Cache()->InsertExprExprConstantExpression(
          cache, condition, expr, unperformed_value,
          ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL);
    }
    return cache;
  }
}

// ----- Modulo -----

IntExpr* Solver::MakeModulo(IntExpr* x, int64_t mod) {
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  if (mod >= 0) {
    AddConstraint(MakeBetweenCt(result, 0, mod - 1));
  } else {
    AddConstraint(MakeBetweenCt(result, mod + 1, 0));
  }
  return result;
}

IntExpr* Solver::MakeModulo(IntExpr* x, IntExpr* mod) {
  if (mod->Bound()) {
    return MakeModulo(x, mod->Min());
  }
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  AddConstraint(MakeLess(result, MakeAbs(mod)));
  AddConstraint(MakeGreater(result, MakeOpposite(MakeAbs(mod))));
  return result;
}

// It's good to have the two extreme values being symmetrical around zero:
// it makes mirroring easier.
const int64_t IntervalVar::kMaxValidValue =
    std::numeric_limits<int64_t>::max() >> 2;
const int64_t IntervalVar::kMinValidValue = -kMaxValidValue;

void IntervalVar::WhenAnything(Demon* d) {
  WhenStartRange(d);
  WhenDurationRange(d);
  WhenEndRange(d);
  WhenPerformedBound(d);
}

IntervalVar* Solver::MakeMirrorInterval(IntervalVar* interval_var) {
  return RegisterIntervalVar(
      RevAlloc(new MirrorIntervalVar(this, interval_var)));
}

IntervalVar* Solver::MakeIntervalRelaxedMax(IntervalVar* interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMax(interval_var)));
  }
}

IntervalVar* Solver::MakeIntervalRelaxedMin(IntervalVar* interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMin(interval_var)));
  }
}

IntervalVar* Solver::MakeFixedInterval(int64_t start, int64_t duration,
                                       const std::string& name) {
  return RevAlloc(new FixedInterval(this, start, duration, name));
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(int64_t start_min,
                                                  int64_t start_max,
                                                  int64_t duration,
                                                  bool optional,
                                                  const std::string& name) {
  if (start_min == start_max && !optional) {
    return MakeFixedInterval(start_min, duration, name);
  } else if (!optional) {
    return RegisterIntervalVar(RevAlloc(new FixedDurationPerformedIntervalVar(
        this, start_min, start_max, duration, name)));
  }
  return RegisterIntervalVar(RevAlloc(new FixedDurationIntervalVar(
      this, start_min, start_max, duration, optional, name)));
}

void Solver::MakeFixedDurationIntervalVarArray(
    int count, int64_t start_min, int64_t start_max, int64_t duration,
    bool optional, absl::string_view name, std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_min, start_max, duration, optional, var_name));
  }
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* start_variable,
                                                  int64_t duration,
                                                  const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK_GE(duration, 0);
  return RegisterIntervalVar(RevAlloc(
      new StartVarPerformedIntervalVar(this, start_variable, duration, name)));
}

// Creates an interval var with a fixed duration, and performed var.
// The duration must be greater than 0.
IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* start_variable,
                                                  int64_t duration,
                                                  IntVar* performed_variable,
                                                  const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK(performed_variable != nullptr);
  CHECK_GE(duration, 0);
  if (!performed_variable->Bound()) {
    StartVarIntervalVar* const interval =
        reinterpret_cast<StartVarIntervalVar*>(
            RegisterIntervalVar(RevAlloc(new StartVarIntervalVar(
                this, start_variable, duration, performed_variable, name))));
    AddConstraint(RevAlloc(new LinkStartVarIntervalVar(
        this, interval, start_variable, performed_variable)));
    return interval;
  } else if (performed_variable->Min() == 1) {
    return RegisterIntervalVar(RevAlloc(new StartVarPerformedIntervalVar(
        this, start_variable, duration, name)));
  }
  return nullptr;
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables, int64_t duration,
    absl::string_view name, std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(
        MakeFixedDurationIntervalVar(start_variables[i], duration, var_name));
  }
}

// This method fills the vector with interval variables built with
// the corresponding start variables.
void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int64_t> durations, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int> durations, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int> durations,
    const std::vector<IntVar*>& performed_variables, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int64_t> durations,
    const std::vector<IntVar*>& performed_variables, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

// Variable Duration Interval Var

IntervalVar* Solver::MakeIntervalVar(int64_t start_min, int64_t start_max,
                                     int64_t duration_min, int64_t duration_max,
                                     int64_t end_min, int64_t end_max,
                                     bool optional, const std::string& name) {
  return RegisterIntervalVar(RevAlloc(new VariableDurationIntervalVar(
      this, start_min, start_max, duration_min, duration_max, end_min, end_max,
      optional, name)));
}

void Solver::MakeIntervalVarArray(int count, int64_t start_min,
                                  int64_t start_max, int64_t duration_min,
                                  int64_t duration_max, int64_t end_min,
                                  int64_t end_max, bool optional,
                                  absl::string_view name,
                                  std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeIntervalVar(start_min, start_max, duration_min,
                                     duration_max, end_min, end_max, optional,
                                     var_name));
  }
}

// Synced Interval Vars
IntervalVar* Solver::MakeFixedDurationStartSyncedOnStartIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationStartSyncedOnEndIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(interval_var,
                                                            duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnStartIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, CapSub(offset, duration))));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnEndIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(
          interval_var, duration, CapSub(offset, duration))));
}

// ----- Factories from timetabling.cc and table.cc -----

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t,
                                            Solver::UnaryIntervalRelation r,
                                            int64_t d) {
  return RevAlloc(new IntervalUnaryRelation(this, t, d, r));
}

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t1,
                                            Solver::BinaryIntervalRelation r,
                                            IntervalVar* const t2) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, 0));
}

Constraint* Solver::MakeIntervalVarRelationWithDelay(
    IntervalVar* const t1, Solver::BinaryIntervalRelation r,
    IntervalVar* const t2, int64_t delay) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, delay));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2,
                                            IntVar* const alt) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, alt));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, nullptr));
}

Constraint* Solver::MakeAllowedAssignments(const std::vector<IntVar*>& vars,
                                           const IntTupleSet& tuples) {
  if (HasCompactDomains(vars)) {
    if (tuples.NumTuples() < kBitsInUint64 && parameters_.use_small_table()) {
      return RevAlloc(
          new SmallCompactPositiveTableConstraint(this, vars, tuples));
    } else {
      return RevAlloc(new CompactPositiveTableConstraint(this, vars, tuples));
    }
  }
  return RevAlloc(new PositiveTableConstraint(this, vars, tuples));
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64_t initial_state, const std::vector<int64_t>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64_t initial_state, const std::vector<int>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

IntVar* Solver::MakeIntVar(int64_t min, int64_t max, const std::string& name) {
  if (min == max) {
    return MakeIntConst(min, name);
  }
  if (min == 0 && max == 1) {
    return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, name)));
  } else if (CapSub(max, min) == 1) {
    const std::string inner_name = "inner_" + name;
    return RegisterIntVar(
        MakeSum(RevAlloc(new ConcreteBooleanVar(this, inner_name)), min)
            ->VarWithName(name));
  } else {
    return RegisterIntVar(RevAlloc(new DomainIntVar(this, min, max, name)));
  }
}

IntVar* Solver::MakeIntVar(int64_t min, int64_t max) {
  return MakeIntVar(min, max, "");
}

IntVar* Solver::MakeBoolVar(const std::string& name) {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, name)));
}

IntVar* Solver::MakeBoolVar() {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, "")));
}

IntVar* Solver::MakeIntVar(const std::vector<int64_t>& values,
                           const std::string& name) {
  DCHECK(!values.empty());
  // Fast-track the case where we have a single value.
  if (values.size() == 1) return MakeIntConst(values[0], name);
  // Sort and remove duplicates.
  std::vector<int64_t> unique_sorted_values = values;
  gtl::STLSortAndRemoveDuplicates(&unique_sorted_values);
  // Case when we have a single value, after clean-up.
  if (unique_sorted_values.size() == 1) return MakeIntConst(values[0], name);
  // Case when the values are a dense interval of integers.
  if (unique_sorted_values.size() ==
      unique_sorted_values.back() - unique_sorted_values.front() + 1) {
    return MakeIntVar(unique_sorted_values.front(), unique_sorted_values.back(),
                      name);
  }
  // Compute the GCD: if it's not 1, we can express the variable's domain as
  // the product of the GCD and of a domain with smaller values.
  int64_t gcd = 0;
  for (const int64_t v : unique_sorted_values) {
    if (gcd == 0) {
      gcd = std::abs(v);
    } else {
      gcd = std::gcd(gcd, std::abs(v));  // Supports v==0.
    }
    if (gcd == 1) {
      // If it's 1, though, we can't do anything special, so we
      // immediately return a new DomainIntVar.
      return RegisterIntVar(
          RevAlloc(new DomainIntVar(this, unique_sorted_values, name)));
    }
  }
  DCHECK_GT(gcd, 1);
  for (int64_t& v : unique_sorted_values) {
    DCHECK_EQ(0, v % gcd);
    v /= gcd;
  }
  const std::string new_name = name.empty() ? "" : "inner_" + name;
  // Catch the case where the divided values are a dense set of integers.
  IntVar* inner_intvar = nullptr;
  if (unique_sorted_values.size() ==
      unique_sorted_values.back() - unique_sorted_values.front() + 1) {
    inner_intvar = MakeIntVar(unique_sorted_values.front(),
                              unique_sorted_values.back(), new_name);
  } else {
    inner_intvar = RegisterIntVar(
        RevAlloc(new DomainIntVar(this, unique_sorted_values, new_name)));
  }
  return MakeProd(inner_intvar, gcd)->Var();
}

IntVar* Solver::MakeIntVar(const std::vector<int64_t>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values,
                           const std::string& name) {
  return MakeIntVar(ToInt64Vector(values), name);
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntConst(int64_t val, const std::string& name) {
  // If IntConst is going to be named after its creation,
  // cp_share_int_consts should be set to false otherwise names can potentially
  // be overwritten.
  if (absl::GetFlag(FLAGS_cp_share_int_consts) && name.empty() &&
      val >= MIN_CACHED_INT_CONST && val <= MAX_CACHED_INT_CONST) {
    return cached_constants_[val - MIN_CACHED_INT_CONST];
  }
  return RevAlloc(new IntConst(this, val, name));
}

IntVar* Solver::MakeIntConst(int64_t val) { return MakeIntConst(val, ""); }

namespace {
std::string IndexedName(absl::string_view prefix, int index, int max_index) {
#if 0
#if defined(_MSC_VER)
  const int digits = max_index > 0 ?
      static_cast<int>(log(1.0L * max_index) / log(10.0L)) + 1 :
      1;
#else
  const int digits = max_index > 0 ? static_cast<int>(log10(max_index)) + 1: 1;
#endif
  return absl::StrFormat("%s%0*d", prefix, digits, index);
#else
  return absl::StrCat(prefix, index);
#endif
}
}  // namespace

void Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                             const std::string& name,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax, IndexedName(name, i, var_count)));
  }
}

void Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax));
  }
}

IntVar** Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                                 const std::string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeIntVar(vmin, vmax, IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::MakeBoolVarArray(int var_count, const std::string& name,
                              std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar(IndexedName(name, i, var_count)));
  }
}

void Solver::MakeBoolVarArray(int var_count, std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar());
  }
}

IntVar** Solver::MakeBoolVarArray(int var_count, const std::string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeBoolVar(IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::InitCachedIntConstants() {
  for (int i = MIN_CACHED_INT_CONST; i <= MAX_CACHED_INT_CONST; ++i) {
    cached_constants_[i - MIN_CACHED_INT_CONST] =
        RevAlloc(new IntConst(this, i, ""));  // note the empty name
  }
}

Assignment* Solver::MakeAssignment() { return RevAlloc(new Assignment(this)); }

Assignment* Solver::MakeAssignment(const Assignment* const a) {
  return RevAlloc(new Assignment(a));
}

Constraint* Solver::MakeEquality(IntExpr* l, IntExpr* r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeEquality(r, l->Min());
  } else if (r->Bound()) {
    return MakeEquality(l, r->Min());
  } else {
    return RevAlloc(new RangeEquality(this, l, r));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* l, IntExpr* r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l == r) {
    return MakeTrueConstraint();
  } else if (l->Bound()) {
    return MakeGreaterOrEqual(r, l->Min());
  } else if (r->Bound()) {
    return MakeLessOrEqual(l, r->Min());
  } else {
    return RevAlloc(new RangeLessOrEqual(this, l, r));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* l, IntExpr* r) {
  return MakeLessOrEqual(r, l);
}

Constraint* Solver::MakeLess(IntExpr* l, IntExpr* r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeGreater(r, l->Min());
  } else if (r->Bound()) {
    return MakeLess(l, r->Min());
  } else {
    return RevAlloc(new RangeLess(this, l, r));
  }
}

Constraint* Solver::MakeGreater(IntExpr* l, IntExpr* r) {
  return MakeLess(r, l);
}

Constraint* Solver::MakeNonEquality(IntExpr* l, IntExpr* r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeNonEquality(r, l->Min());
  } else if (r->Bound()) {
    return MakeNonEquality(l, r->Min());
  }
  return RevAlloc(new DiffVar(this, l->Var(), r->Var()));
}

IntVar* Solver::MakeIsEqualVar(IntExpr* v1, IntExpr* v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstVar(v2, v1->Min());
  } else if (v2->Bound()) {
    return MakeIsEqualCstVar(v1, v2->Min());
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      v1, v2, ModelCache::EXPR_EXPR_IS_EQUAL);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(
        v2, v1, ModelCache::EXPR_EXPR_IS_EQUAL);
  }
  if (cache != nullptr) {
    return cache->Var();
  } else {
    IntVar* boolvar = nullptr;
    IntExpr* reverse_cache = model_cache_->FindExprExprExpression(
        v1, v2, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    if (reverse_cache == nullptr) {
      reverse_cache = model_cache_->FindExprExprExpression(
          v2, v1, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    }
    if (reverse_cache != nullptr) {
      boolvar = MakeDifference(1, reverse_cache)->Var();
    } else {
      std::string name1 = v1->name();
      if (name1.empty()) {
        name1 = v1->DebugString();
      }
      std::string name2 = v2->name();
      if (name2.empty()) {
        name2 = v2->DebugString();
      }
      boolvar =
          MakeBoolVar(absl::StrFormat("IsEqualVar(%s, %s)", name1, name2));
      AddConstraint(MakeIsEqualCt(v1, v2, boolvar));
      model_cache_->InsertExprExprExpression(boolvar, v1, v2,
                                             ModelCache::EXPR_EXPR_IS_EQUAL);
    }
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCt(IntExpr* v1, IntExpr* v2, IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstCt(v2, v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsEqualCstCt(v1, v2->Min(), b);
  }
  if (b->Bound()) {
    if (b->Min() == 0) {
      return MakeNonEquality(v1, v2);
    } else {
      return MakeEquality(v1, v2);
    }
  }
  return RevAlloc(new IsEqualCt(this, v1, v2, b));
}

IntVar* Solver::MakeIsDifferentVar(IntExpr* v1, IntExpr* v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstVar(v2, v1->Min());
  } else if (v2->Bound()) {
    return MakeIsDifferentCstVar(v1, v2->Min());
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      v1, v2, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(
        v2, v1, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
  }
  if (cache != nullptr) {
    return cache->Var();
  } else {
    IntVar* boolvar = nullptr;
    IntExpr* reverse_cache = model_cache_->FindExprExprExpression(
        v1, v2, ModelCache::EXPR_EXPR_IS_EQUAL);
    if (reverse_cache == nullptr) {
      reverse_cache = model_cache_->FindExprExprExpression(
          v2, v1, ModelCache::EXPR_EXPR_IS_EQUAL);
    }
    if (reverse_cache != nullptr) {
      boolvar = MakeDifference(1, reverse_cache)->Var();
    } else {
      std::string name1 = v1->name();
      if (name1.empty()) {
        name1 = v1->DebugString();
      }
      std::string name2 = v2->name();
      if (name2.empty()) {
        name2 = v2->DebugString();
      }
      boolvar =
          MakeBoolVar(absl::StrFormat("IsDifferentVar(%s, %s)", name1, name2));
      AddConstraint(MakeIsDifferentCt(v1, v2, boolvar));
    }
    model_cache_->InsertExprExprExpression(boolvar, v1, v2,
                                           ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsDifferentCt(IntExpr* v1, IntExpr* v2, IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstCt(v2, v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsDifferentCstCt(v1, v2->Min(), b);
  }
  return RevAlloc(new IsDifferentCt(this, v1, v2, b));
}

IntVar* Solver::MakeIsLessOrEqualVar(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstVar(right, left->Min());
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstVar(left, right->Min());
  }
  IntExpr* const cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_IS_LESS_OR_EQUAL);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    std::string name1 = left->name();
    if (name1.empty()) {
      name1 = left->DebugString();
    }
    std::string name2 = right->name();
    if (name2.empty()) {
      name2 = right->DebugString();
    }
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("IsLessOrEqual(%s, %s)", name1, name2));

    AddConstraint(RevAlloc(new IsLessOrEqualCt(this, left, right, boolvar)));
    model_cache_->InsertExprExprExpression(
        boolvar, left, right, ModelCache::EXPR_EXPR_IS_LESS_OR_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessOrEqualCt(IntExpr* left, IntExpr* right,
                                        IntVar* b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstCt(right, left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstCt(left, right->Min(), b);
  }
  return RevAlloc(new IsLessOrEqualCt(this, left, right, b));
}

IntVar* Solver::MakeIsLessVar(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstVar(right, left->Min());
  } else if (right->Bound()) {
    return MakeIsLessCstVar(left, right->Min());
  }
  IntExpr* const cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_IS_LESS);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    std::string name1 = left->name();
    if (name1.empty()) {
      name1 = left->DebugString();
    }
    std::string name2 = right->name();
    if (name2.empty()) {
      name2 = right->DebugString();
    }
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("IsLessOrEqual(%s, %s)", name1, name2));

    AddConstraint(RevAlloc(new IsLessCt(this, left, right, boolvar)));
    model_cache_->InsertExprExprExpression(boolvar, left, right,
                                           ModelCache::EXPR_EXPR_IS_LESS);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessCt(IntExpr* left, IntExpr* right, IntVar* b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstCt(right, left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessCstCt(left, right->Min(), b);
  }
  return RevAlloc(new IsLessCt(this, left, right, b));
}

IntVar* Solver::MakeIsGreaterOrEqualVar(IntExpr* left, IntExpr* right) {
  return MakeIsLessOrEqualVar(right, left);
}

Constraint* Solver::MakeIsGreaterOrEqualCt(IntExpr* left, IntExpr* right,
                                           IntVar* b) {
  return MakeIsLessOrEqualCt(right, left, b);
}

IntVar* Solver::MakeIsGreaterVar(IntExpr* left, IntExpr* right) {
  return MakeIsLessVar(right, left);
}

Constraint* Solver::MakeIsGreaterCt(IntExpr* left, IntExpr* right, IntVar* b) {
  return MakeIsLessCt(right, left, b);
}

}  // namespace operations_research
