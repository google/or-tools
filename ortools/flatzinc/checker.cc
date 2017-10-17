// Copyright 2010-2017 Google
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

#include "ortools/flatzinc/checker.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "ortools/base/map_util.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {
namespace {

// Helpers

int64 Eval(const Argument& arg,
           const std::function<int64(IntegerVariable*)>& evaluator) {
  switch (arg.type) {
    case Argument::INT_VALUE: {
      return arg.Value();
    }
    case Argument::INT_VAR_REF: {
      return evaluator(arg.Var());
    }
    default: {
      LOG(FATAL) << "Cannot evaluate " << arg.DebugString();
      return 0;
    }
  }
}

int Size(const Argument& arg) {
  return std::max(arg.values.size(), arg.variables.size());
}

int64 EvalAt(const Argument& arg, int pos,
             const std::function<int64(IntegerVariable*)>& evaluator) {
  switch (arg.type) {
    case Argument::INT_LIST: {
      return arg.ValueAt(pos);
    }
    case Argument::INT_VAR_REF_ARRAY: {
      return evaluator(arg.VarAt(pos));
    }
    default: {
      LOG(FATAL) << "Cannot evaluate " << arg.DebugString();
      return 0;
    }
  }
}

// Checkers

bool CheckAllDifferentInt(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  std::unordered_set<int64> visited;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    if (ContainsKey(visited, value)) {
      return false;
    }
    visited.insert(value);
  }

  return true;
}

bool CheckAlldifferentExcept0(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  std::unordered_set<int64> visited;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    if (value != 0 && ContainsKey(visited, value)) {
      return false;
    }
    visited.insert(value);
  }

  return true;
}

bool CheckAmong(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 expected = Eval(ct.arguments[0], evaluator);
  int64 count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    count += ct.arguments[2].Contains(value);
  }

  return count == expected;
}

bool CheckArrayBoolAnd(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 1;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::min(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolOr(const Constraint& ct,
                      const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::max(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolXor(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator);
  }

  return result % 2 == 1;
}

bool CheckArrayIntElement(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  if (ct.arguments[0].variables.size() == 2) {
    // TODO(user): Check 2D element.
    return true;
  }
  // Flatzinc arrays are 1 based.
  const int64 shifted_index = Eval(ct.arguments[0], evaluator) - 1;
  const int64 element = EvalAt(ct.arguments[1], shifted_index, evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return element == target;
}

bool CheckArrayVarIntElement(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  if (ct.arguments[0].variables.size() == 2) {
    // TODO(user): Check 2D element.
    return true;
  }
  // Flatzinc arrays are 1 based.
  const int64 shifted_index = Eval(ct.arguments[0], evaluator) - 1;
  const int64 element = EvalAt(ct.arguments[1], shifted_index, evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return element == target;
}

bool CheckAtMostInt(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 expected = Eval(ct.arguments[0], evaluator);
  const int64 value = Eval(ct.arguments[2], evaluator);

  int64 count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    count += EvalAt(ct.arguments[1], i, evaluator) == value;
  }
  return count <= expected;
}

bool CheckBoolAnd(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckBoolClause(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 0;

  // Positive variables.
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::max(result, value);
  }
  // Negative variables.
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    const int64 value = EvalAt(ct.arguments[1], i, evaluator);
    result = std::max(result, 1 - value);
  }

  return result;
}

bool CheckBoolNot(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == 1 - right;
}

bool CheckBoolOr(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckBoolXor(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == (left + right == 1);
}

bool CheckCircuit(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  // There are two versions of the constraint. 0 based and 1 based.
  // Let's try to detect which one we have.
  const int size = Size(ct.arguments[0]);
  int shift = 0;
  for (int i = 0; i < size; ++i) {
    const int64 next = EvalAt(ct.arguments[0], i, evaluator);
    if (next == 0) {  // 0 based.
      shift = 0;
      break;
    } else if (next == size) {  // 1 based.
      shift = -1;
      break;
    }
  }

  std::unordered_set<int64> visited;
  int64 current = 0;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 next = EvalAt(ct.arguments[0], current, evaluator) + shift;
    visited.insert(next);
    current = next;
  }
  return visited.size() == Size(ct.arguments[0]);
}

int64 ComputeCount(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 0;
  const int64 value = Eval(ct.arguments[1], evaluator);
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) == value;
  }
  return result;
}

bool CheckCountEq(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count == expected;
}

bool CheckCountGeq(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count >= expected;
}

bool CheckCountGt(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count > expected;
}

bool CheckCountLeq(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count <= expected;
}

bool CheckCountLt(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count < expected;
}

bool CheckCountNeq(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count != expected;
}

bool CheckCountReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (expected == count);
}

bool CheckCumulative(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  // TODO(user): Improve complexity for large durations.
  const int64 capacity = Eval(ct.arguments[3], evaluator);
  const int size = Size(ct.arguments[0]);
  CHECK_EQ(size, Size(ct.arguments[1]));
  CHECK_EQ(size, Size(ct.arguments[2]));
  std::unordered_map<int64, int64> usage;
  for (int i = 0; i < size; ++i) {
    const int64 start = EvalAt(ct.arguments[0], i, evaluator);
    const int64 duration = EvalAt(ct.arguments[1], i, evaluator);
    const int64 requirement = EvalAt(ct.arguments[2], i, evaluator);
    for (int64 t = start; t < start + duration; ++t) {
      usage[t] += requirement;
      if (usage[t] > capacity) {
        return false;
      }
    }
  }
  return true;
}

bool CheckDiffn(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckDiffnK(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckDiffnNonStrict(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckDiffnNonStrictK(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckDisjunctive(const Constraint& ct,
                      const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckDisjunctiveStrict(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckFalseConstraint(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  return false;
}

std::vector<int64> ComputeGlobalCardinalityCards(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  std::vector<int64> cards(Size(ct.arguments[1]), 0);
  std::unordered_map<int64, int> positions;
  for (int i = 0; i < ct.arguments[1].values.size(); ++i) {
    const int64 value = ct.arguments[1].values[i];
    CHECK(!ContainsKey(positions, value));
    positions[value] = i;
  }
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int value = EvalAt(ct.arguments[0], i, evaluator);
    if (ContainsKey(positions, value)) {
      cards[positions[value]]++;
    }
  }
  return cards;
}

bool CheckGlobalCardinality(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const std::vector<int64> cards = ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), Size(ct.arguments[2]));
  for (int i = 0; i < Size(ct.arguments[2]); ++i) {
    const int64 card = EvalAt(ct.arguments[2], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  return true;
}

bool CheckGlobalCardinalityClosed(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const std::vector<int64> cards = ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), Size(ct.arguments[2]));
  for (int i = 0; i < Size(ct.arguments[2]); ++i) {
    const int64 card = EvalAt(ct.arguments[2], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  int64 sum_of_cards = 0;
  for (int64 card : cards) {
    sum_of_cards += card;
  }
  return sum_of_cards == Size(ct.arguments[0]);
}

bool CheckGlobalCardinalityLowUp(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const std::vector<int64> cards = ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), ct.arguments[2].values.size());
  CHECK_EQ(cards.size(), ct.arguments[3].values.size());
  for (int i = 0; i < cards.size(); ++i) {
    const int64 card = cards[i];
    if (card < ct.arguments[2].values[i] || card > ct.arguments[3].values[i]) {
      return false;
    }
  }
  return true;
}

bool CheckGlobalCardinalityLowUpClosed(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const std::vector<int64> cards = ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), ct.arguments[2].values.size());
  CHECK_EQ(cards.size(), ct.arguments[3].values.size());
  for (int i = 0; i < cards.size(); ++i) {
    const int64 card = cards[i];
    if (card < ct.arguments[2].values[i] || card > ct.arguments[3].values[i]) {
      return false;
    }
  }
  int64 sum_of_cards = 0;
  for (int64 card : cards) {
    sum_of_cards += card;
  }
  return sum_of_cards == Size(ct.arguments[0]);
}

bool CheckGlobalCardinalityOld(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int size = Size(ct.arguments[1]);
  std::vector<int64> cards(size, 0);
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    if (value >= 0 && value < size) {
      cards[value]++;
    }
  }
  for (int i = 0; i < size; ++i) {
    const int64 card = EvalAt(ct.arguments[1], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  return true;
}

bool CheckIntAbs(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return std::abs(left) == right;
}

bool CheckIntDiv(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left / right;
}

bool CheckIntEq(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == right;
}

bool CheckIntEqReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left == right);
}

bool CheckIntGe(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left >= right;
}

bool CheckIntGeReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left >= right);
}

bool CheckIntGt(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left > right;
}

bool CheckIntGtReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left > right);
}

bool CheckIntLe(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left <= right;
}

bool CheckIntLeReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left <= right);
}

bool CheckIntLt(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left < right;
}

bool CheckIntLtReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left < right);
}

int64 ComputeIntLin(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 result = 0;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) *
              EvalAt(ct.arguments[1], i, evaluator);
  }
  return result;
}

bool CheckIntLinEq(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left == right;
}

bool CheckIntLinEqReif(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left == right);
}

bool CheckIntLinGe(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left >= right;
}

bool CheckIntLinGeReif(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left >= right);
}

bool CheckIntLinLe(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left <= right;
}

bool CheckIntLinLeReif(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left <= right);
}

bool CheckIntLinNe(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left != right;
}

bool CheckIntLinNeReif(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left != right);
}

bool CheckIntMax(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckIntMin(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckIntMinus(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left - right;
}

bool CheckIntMod(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left % right;
}

bool CheckIntNe(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left != right;
}

bool CheckIntNeReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left != right);
}

bool CheckIntNegate(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == -right;
}

bool CheckIntPlus(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left + right;
}

bool CheckIntTimes(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left * right;
}

bool CheckInverse(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  const int size = Size(ct.arguments[0]);
  // Check all bounds.
  for (int i = 0; i < size; ++i) {
    const int64 x = EvalAt(ct.arguments[0], i, evaluator) - 1;
    const int64 y = EvalAt(ct.arguments[1], i, evaluator) - 1;
    if (x < 0 || x >= size || y < 0 || y >= size) {
      return false;
    }
  }
  // Check f-1(f(i)) = i.
  for (int i = 0; i < size; ++i) {
    const int64 fi = EvalAt(ct.arguments[0], i, evaluator) - 1;
    const int64 invf_fi = EvalAt(ct.arguments[1], fi, evaluator) - 1;
    if (invf_fi != i) {
      return false;
    }
  }

  return true;
}

bool CheckLexLessInt(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 x = EvalAt(ct.arguments[0], i, evaluator);
    const int64 y = EvalAt(ct.arguments[1], i, evaluator);
    if (x < y) {
      return true;
    }
    if (x > y) {
      return false;
    }
  }
  // We are at the end of the list. The two chains are equals.
  return false;
}

bool CheckLexLesseqInt(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 x = EvalAt(ct.arguments[0], i, evaluator);
    const int64 y = EvalAt(ct.arguments[1], i, evaluator);
    if (x < y) {
      return true;
    }
    if (x > y) {
      return false;
    }
  }
  // We are at the end of the list. The two chains are equals.
  return true;
}

bool CheckMaximumArgInt(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 max_index = Eval(ct.arguments[1], evaluator) - 1;
  const int64 max_value = EvalAt(ct.arguments[0], max_index, evaluator);
  // Checks that all value before max_index are < max_value.
  for (int i = 0; i < max_index; ++i) {
    if (EvalAt(ct.arguments[0], i, evaluator) >= max_value) {
      return false;
    }
  }
  // Checks that all value after max_index are <= max_value.
  for (int i = max_index + 1; i < Size(ct.arguments[0]); i++) {
    if (EvalAt(ct.arguments[0], i, evaluator) > max_value) {
      return false;
    }
  }

  return true;
}

bool CheckMaximumInt(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 max_value = kint64min;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    max_value = std::max(max_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return max_value == Eval(ct.arguments[0], evaluator);
}

bool CheckMinimumArgInt(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 min_index = Eval(ct.arguments[1], evaluator) - 1;
  const int64 min_value = EvalAt(ct.arguments[0], min_index, evaluator);
  // Checks that all value before min_index are > min_value.
  for (int i = 0; i < min_index; ++i) {
    if (EvalAt(ct.arguments[0], i, evaluator) <= min_value) {
      return false;
    }
  }
  // Checks that all value after min_index are >= min_value.
  for (int i = min_index + 1; i < Size(ct.arguments[0]); i++) {
    if (EvalAt(ct.arguments[0], i, evaluator) < min_value) {
      return false;
    }
  }

  return true;
}

bool CheckMinimumInt(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  int64 min_value = kint64max;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    min_value = std::min(min_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return min_value == Eval(ct.arguments[0], evaluator);
}

bool CheckNetworkFlowConservation(
    const Argument& arcs, const Argument& balance_input,
    const Argument& flow_vars,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  std::vector<int64> balance(balance_input.values);

  const int num_arcs = Size(arcs) / 2;
  for (int arc = 0; arc < num_arcs; arc++) {
    const int tail = arcs.values[arc * 2] - 1;
    const int head = arcs.values[arc * 2 + 1] - 1;
    const int64 flow = EvalAt(flow_vars, arc, evaluator);
    balance[tail] -= flow;
    balance[head] += flow;
  }

  for (const int64 value : balance) {
    if (value != 0) return false;
  }

  return true;
}

bool CheckNetworkFlow(const Constraint& ct,
                      const std::function<int64(IntegerVariable*)>& evaluator) {
  return CheckNetworkFlowConservation(ct.arguments[0], ct.arguments[1],
                                      ct.arguments[2], evaluator);
}

bool CheckNetworkFlowCost(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  if (!CheckNetworkFlowConservation(ct.arguments[0], ct.arguments[1],
                                    ct.arguments[3], evaluator)) {
    return false;
  }

  int64 total_cost = 0;
  const int num_arcs = Size(ct.arguments[3]);
  for (int arc = 0; arc < num_arcs; arc++) {
    const int64 flow = EvalAt(ct.arguments[3], arc, evaluator);
    const int64 cost = EvalAt(ct.arguments[2], arc, evaluator);
    total_cost += flow * cost;
  }

  return total_cost == Eval(ct.arguments[4], evaluator);
}

bool CheckNvalue(const Constraint& ct,
                 const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 count = Eval(ct.arguments[0], evaluator);
  std::unordered_set<int64> all_values;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    all_values.insert(EvalAt(ct.arguments[1], i, evaluator));
  }

  return count == all_values.size();
}

bool CheckRegular(const Constraint& ct,
                  const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckRegularNfa(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckSetIn(const Constraint& ct,
                const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  return ct.arguments[1].Contains(value);
}

bool CheckSetNotIn(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  return !ct.arguments[1].Contains(value);
}

bool CheckSetInReif(const Constraint& ct,
                    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == ct.arguments[1].Contains(value);
}

bool CheckSlidingSum(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  const int64 low = Eval(ct.arguments[0], evaluator);
  const int64 up = Eval(ct.arguments[1], evaluator);
  const int64 length = Eval(ct.arguments[2], evaluator);
  // Compute initial sum.
  int64 sliding_sum = 0;
  for (int i = 0; i < std::min<int64>(length, Size(ct.arguments[3])); ++i) {
    sliding_sum += EvalAt(ct.arguments[3], i, evaluator);
  }
  if (sliding_sum < low || sliding_sum > up) {
    return false;
  }
  for (int i = length; i < Size(ct.arguments[3]); ++i) {
    sliding_sum += EvalAt(ct.arguments[3], i, evaluator) -
                   EvalAt(ct.arguments[3], i - length, evaluator);
    if (sliding_sum < low || sliding_sum > up) {
      return false;
    }
  }
  return true;
}

bool CheckSort(const Constraint& ct,
               const std::function<int64(IntegerVariable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  std::unordered_map<int64, int> init_count;
  std::unordered_map<int64, int> sorted_count;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    init_count[EvalAt(ct.arguments[0], i, evaluator)]++;
    sorted_count[EvalAt(ct.arguments[1], i, evaluator)]++;
  }
  if (init_count != sorted_count) {
    return false;
  }
  for (int i = 0; i < Size(ct.arguments[1]) - 1; ++i) {
    if (EvalAt(ct.arguments[1], i, evaluator) >
        EvalAt(ct.arguments[1], i, evaluator)) {
      return false;
    }
  }
  return true;
}

bool CheckSubCircuit(const Constraint& ct,
                     const std::function<int64(IntegerVariable*)>& evaluator) {
  std::unordered_set<int64> visited;
  // Find inactive nodes (pointing to themselves).
  int64 current = -1;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 next = EvalAt(ct.arguments[0], i, evaluator) - 1;
    if (next != i && current == -1) {
      current = next;
    } else if (next == i) {
      visited.insert(next);
    }
  }

  // Try to find a path of length 'residual_size'.
  const int residual_size = Size(ct.arguments[0]) - visited.size();
  for (int i = 0; i < residual_size; ++i) {
    const int64 next = EvalAt(ct.arguments[0], current, evaluator) - 1;
    visited.insert(next);
    if (next == current) {
      return false;
    }
    current = next;
  }

  // Have we visited all nodes?
  return visited.size() == Size(ct.arguments[0]);
}

bool CheckTableInt(const Constraint& ct,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  return true;
}

bool CheckSymmetricAllDifferent(
    const Constraint& ct,
    const std::function<int64(IntegerVariable*)>& evaluator) {
  const int size = Size(ct.arguments[0]);
  for (int i = 0; i < size; ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator) - 1;
    if (value < 0 || value >= size) {
      return false;
    }
    const int64 reverse_value = EvalAt(ct.arguments[0], value, evaluator) - 1;
    if (reverse_value != i) {
      return false;
    }
  }
  return true;
}

using CallMap = std::unordered_map<
    std::string, std::function<bool(const Constraint& ct,
                               std::function<int64(IntegerVariable*)>)>>;

CallMap CreateCallMap() {
  CallMap m;
  m["all_different_int"] = CheckAllDifferentInt;
  m["alldifferent_except_0"] = CheckAlldifferentExcept0;
  m["among"] = CheckAmong;
  m["array_bool_and"] = CheckArrayBoolAnd;
  m["array_bool_element"] = CheckArrayIntElement;
  m["array_bool_or"] = CheckArrayBoolOr;
  m["array_bool_xor"] = CheckArrayBoolXor;
  m["array_int_element"] = CheckArrayIntElement;
  m["array_var_bool_element"] = CheckArrayVarIntElement;
  m["array_var_int_element"] = CheckArrayVarIntElement;
  m["at_most_int"] = CheckAtMostInt;
  m["bool_and"] = CheckBoolAnd;
  m["bool_clause"] = CheckBoolClause;
  m["bool_eq"] = CheckIntEq;
  m["bool2int"] = CheckIntEq;
  m["bool_eq_reif"] = CheckIntEqReif;
  m["bool_ge"] = CheckIntGe;
  m["bool_ge_reif"] = CheckIntGeReif;
  m["bool_gt"] = CheckIntGt;
  m["bool_gt_reif"] = CheckIntGtReif;
  m["bool_le"] = CheckIntLe;
  m["bool_le_reif"] = CheckIntLeReif;
  m["bool_left_imp"] = CheckIntLe;
  m["bool_lin_eq"] = CheckIntLinEq;
  m["bool_lin_le"] = CheckIntLinLe;
  m["bool_lt"] = CheckIntLt;
  m["bool_lt_reif"] = CheckIntLtReif;
  m["bool_ne"] = CheckIntNe;
  m["bool_ne_reif"] = CheckIntNeReif;
  m["bool_not"] = CheckBoolNot;
  m["bool_or"] = CheckBoolOr;
  m["bool_right_imp"] = CheckIntGe;
  m["bool_xor"] = CheckBoolXor;
  m["circuit"] = CheckCircuit;
  m["count_eq"] = CheckCountEq;
  m["count"] = CheckCountEq;
  m["count_geq"] = CheckCountGeq;
  m["count_gt"] = CheckCountGt;
  m["count_leq"] = CheckCountLeq;
  m["count_lt"] = CheckCountLt;
  m["count_neq"] = CheckCountNeq;
  m["count_reif"] = CheckCountReif;
  m["cumulative"] = CheckCumulative;
  m["var_cumulative"] = CheckCumulative;
  m["variable_cumulative"] = CheckCumulative;
  m["fixed_cumulative"] = CheckCumulative;
  m["diffn"] = CheckDiffn;
  m["diffn_k_with_sizes"] = CheckDiffnK;
  m["diffn_nonstrict"] = CheckDiffnNonStrict;
  m["diffn_nonstrict_k_with_sizes"] = CheckDiffnNonStrictK;
  m["disjunctive"] = CheckDisjunctive;
  m["disjunctive_strict"] = CheckDisjunctiveStrict;
  m["false_constraint"] = CheckFalseConstraint;
  m["global_cardinality"] = CheckGlobalCardinality;
  m["global_cardinality_closed"] = CheckGlobalCardinalityClosed;
  m["global_cardinality_low_up"] = CheckGlobalCardinalityLowUp;
  m["global_cardinality_low_up_closed"] = CheckGlobalCardinalityLowUpClosed;
  m["global_cardinality_old"] = CheckGlobalCardinalityOld;
  m["int_abs"] = CheckIntAbs;
  m["int_div"] = CheckIntDiv;
  m["int_eq"] = CheckIntEq;
  m["int_eq_reif"] = CheckIntEqReif;
  m["int_ge"] = CheckIntGe;
  m["int_ge_reif"] = CheckIntGeReif;
  m["int_gt"] = CheckIntGt;
  m["int_gt_reif"] = CheckIntGtReif;
  m["int_le"] = CheckIntLe;
  m["int_le_reif"] = CheckIntLeReif;
  m["int_lin_eq"] = CheckIntLinEq;
  m["int_lin_eq_reif"] = CheckIntLinEqReif;
  m["int_lin_ge"] = CheckIntLinGe;
  m["int_lin_ge_reif"] = CheckIntLinGeReif;
  m["int_lin_le"] = CheckIntLinLe;
  m["int_lin_le_reif"] = CheckIntLinLeReif;
  m["int_lin_ne"] = CheckIntLinNe;
  m["int_lin_ne_reif"] = CheckIntLinNeReif;
  m["int_lt"] = CheckIntLt;
  m["int_lt_reif"] = CheckIntLtReif;
  m["int_max"] = CheckIntMax;
  m["int_min"] = CheckIntMin;
  m["int_minus"] = CheckIntMinus;
  m["int_mod"] = CheckIntMod;
  m["int_ne"] = CheckIntNe;
  m["int_ne_reif"] = CheckIntNeReif;
  m["int_negate"] = CheckIntNegate;
  m["int_plus"] = CheckIntPlus;
  m["int_times"] = CheckIntTimes;
  m["inverse"] = CheckInverse;
  m["lex_less_bool"] = CheckLexLessInt;
  m["lex_less_int"] = CheckLexLessInt;
  m["lex_lesseq_bool"] = CheckLexLesseqInt;
  m["lex_lesseq_int"] = CheckLexLesseqInt;
  m["maximum_arg_int"] = CheckMaximumArgInt;
  m["maximum_int"] = CheckMaximumInt;
  m["array_int_maximum"] = CheckMaximumInt;
  m["minimum_arg_int"] = CheckMinimumArgInt;
  m["minimum_int"] = CheckMinimumInt;
  m["array_int_minimum"] = CheckMinimumInt;
  m["network_flow"] = CheckNetworkFlow;
  m["network_flow_cost"] = CheckNetworkFlowCost;
  m["nvalue"] = CheckNvalue;
  m["regular"] = CheckRegular;
  m["regular_nfa"] = CheckRegularNfa;
  m["set_in"] = CheckSetIn;
  m["int_in"] = CheckSetIn;
  m["set_not_in"] = CheckSetNotIn;
  m["int_not_in"] = CheckSetNotIn;
  m["set_in_reif"] = CheckSetInReif;
  m["sliding_sum"] = CheckSlidingSum;
  m["sort"] = CheckSort;
  m["subcircuit"] = CheckSubCircuit;
  m["symmetric_all_different"] = CheckSymmetricAllDifferent;
  m["table_bool"] = CheckTableInt;
  m["table_int"] = CheckTableInt;
  return m;
}

}  // namespace

bool CheckSolution(const Model& model,
                   const std::function<int64(IntegerVariable*)>& evaluator) {
  bool ok = true;
  const CallMap call_map = CreateCallMap();
  for (Constraint* ct : model.constraints()) {
    if (!ct->active) continue;
    const auto& checker = FindOrDie(call_map, ct->type);
    if (!checker(*ct, evaluator)) {
      FZLOG << "Failing constraint " << ct->DebugString() << FZENDL;
      ok = false;
    }
  }
  return ok;
}

}  // namespace fz
}  // namespace operations_research
