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

#include "flatzinc/checker.h"

#include <unordered_map>
#include <unordered_set>

#include "base/map_util.h"
#include "flatzinc/logging.h"
#include "flatzinc/model.h"

namespace operations_research {
namespace fz {
namespace {

// Helpers

int64 Eval(const Argument& arg,
           std::function<int64(IntegerVariable*)> evaluator) {
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
             std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckAllDifferentInt(const Constraint& ct,
                          std::function<int64(IntegerVariable*)> evaluator) {
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
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 expected = Eval(ct.arguments[0], evaluator);
  int64 count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    count += ct.arguments[2].Contains(value);
  }

  return count == expected;
}

bool CheckArrayBoolAnd(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  int64 result = 1;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::min(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolOr(const Constraint& ct,
                      std::function<int64(IntegerVariable*)> evaluator) {
  int64 result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64 value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::max(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolXor(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  int64 result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator);
  }

  return result % 2 == 1;
}

bool CheckArrayIntElement(const Constraint& ct,
                          std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckArrayVarIntElement(const Constraint& ct,
                             std::function<int64(IntegerVariable*)> evaluator) {
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
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 expected = Eval(ct.arguments[0], evaluator);
  const int64 value = Eval(ct.arguments[2], evaluator);

  int64 count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    count += EvalAt(ct.arguments[1], i, evaluator) == value;
  }
  return count <= expected;
}

bool CheckBoolAnd(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckBoolClause(const Constraint& ct,
                     std::function<int64(IntegerVariable*)> evaluator) {
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
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == 1 - right;
}

bool CheckBoolOr(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckBoolXor(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == (left + right == 1);
}

bool CheckCircuit(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
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
                   std::function<int64(IntegerVariable*)> evaluator) {
  int64 result = 0;
  const int64 value = Eval(ct.arguments[1], evaluator);
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) == value;
  }
  return result;
}

bool CheckCountEq(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count == expected;
}

bool CheckCountGeq(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count >= expected;
}

bool CheckCountGt(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count > expected;
}

bool CheckCountLeq(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count <= expected;
}

bool CheckCountLt(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count < expected;
}

bool CheckCountNeq(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  return count != expected;
}

bool CheckCountReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = ComputeCount(ct, evaluator);
  const int64 expected = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (expected == count);
}

bool CheckCumulative(const Constraint& ct,
                     std::function<int64(IntegerVariable*)> evaluator) {
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
                std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckDiffnK(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckDiffnNonStrict(const Constraint& ct,
                         std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckDiffnNonStrictK(const Constraint& ct,
                          std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckDisjunctive(const Constraint& ct,
                      std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckDisjunctiveStrict(const Constraint& ct,
                            std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckFalseConstraint(const Constraint& ct,
                          std::function<int64(IntegerVariable*)> evaluator) {
  return false;
}

std::vector<int64> ComputeGlobalCardinalityCards(
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckGlobalCardinality(const Constraint& ct,
                            std::function<int64(IntegerVariable*)> evaluator) {
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
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return std::abs(left) == right;
}

bool CheckIntDiv(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left / right;
}

bool CheckIntEq(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == right;
}

bool CheckIntEqReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left == right);
}

bool CheckIntGe(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left >= right;
}

bool CheckIntGeReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left >= right);
}

bool CheckIntGt(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left > right;
}

bool CheckIntGtReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left > right);
}

bool CheckIntLe(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left <= right;
}

bool CheckIntLeReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left <= right);
}

bool CheckIntLt(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left < right;
}

bool CheckIntLtReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left < right);
}

int64 ComputeIntLin(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  int64 result = 0;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) *
              EvalAt(ct.arguments[1], i, evaluator);
  }
  return result;
}

bool CheckIntLinEq(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left == right;
}

bool CheckIntLinEqReif(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left == right);
}

bool CheckIntLinGe(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left >= right;
}

bool CheckIntLinGeReif(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left >= right);
}

bool CheckIntLinLe(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left <= right;
}

bool CheckIntLinLeReif(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left <= right);
}

bool CheckIntLinNe(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  return left != right;
}

bool CheckIntLinNeReif(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = ComputeIntLin(ct, evaluator);
  const int64 right = Eval(ct.arguments[2], evaluator);
  const int64 status = Eval(ct.arguments[3], evaluator);
  return status == (left != right);
}

bool CheckIntMax(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckIntMin(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckIntMinus(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left - right;
}

bool CheckIntMod(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left % right;
}

bool CheckIntNe(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left != right;
}

bool CheckIntNeReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == (left != right);
}

bool CheckIntNegate(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  return left == -right;
}

bool CheckIntPlus(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left + right;
}

bool CheckIntTimes(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 left = Eval(ct.arguments[0], evaluator);
  const int64 right = Eval(ct.arguments[1], evaluator);
  const int64 target = Eval(ct.arguments[2], evaluator);
  return target == left * right;
}

bool CheckInverse(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
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
                     std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckLexLesseqInt(const Constraint& ct,
                       std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckMaximumArgInt(const Constraint& ct,
                        std::function<int64(IntegerVariable*)> evaluator) {
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
                     std::function<int64(IntegerVariable*)> evaluator) {
  int64 max_value = kint64min;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    max_value = std::max(max_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return max_value == Eval(ct.arguments[0], evaluator);
}

bool CheckMinimumArgInt(const Constraint& ct,
                        std::function<int64(IntegerVariable*)> evaluator) {
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
                     std::function<int64(IntegerVariable*)> evaluator) {
  int64 min_value = kint64max;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    min_value = std::min(min_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return min_value == Eval(ct.arguments[0], evaluator);
}

bool CheckNvalue(const Constraint& ct,
                 std::function<int64(IntegerVariable*)> evaluator) {
  const int64 count = Eval(ct.arguments[0], evaluator);
  std::unordered_set<int64> all_values;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    all_values.insert(EvalAt(ct.arguments[1], i, evaluator));
  }

  return count == all_values.size();
}

bool CheckRegular(const Constraint& ct,
                  std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckRegularNfa(const Constraint& ct,
                     std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckSetIn(const Constraint& ct,
                std::function<int64(IntegerVariable*)> evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  return ct.arguments[1].Contains(value);
}

bool CheckSetNotIn(const Constraint& ct,
                   std::function<int64(IntegerVariable*)> evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  return !ct.arguments[1].Contains(value);
}

bool CheckSetInReif(const Constraint& ct,
                    std::function<int64(IntegerVariable*)> evaluator) {
  const int64 value = Eval(ct.arguments[0], evaluator);
  const int64 status = Eval(ct.arguments[2], evaluator);
  return status == ct.arguments[1].Contains(value);
}

bool CheckSlidingSum(const Constraint& ct,
                     std::function<int64(IntegerVariable*)> evaluator) {
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
               std::function<int64(IntegerVariable*)> evaluator) {
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
                     std::function<int64(IntegerVariable*)> evaluator) {
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
                   std::function<int64(IntegerVariable*)> evaluator) {
  return true;
}

bool CheckSymmetricAllDifferent(
    const Constraint& ct, std::function<int64(IntegerVariable*)> evaluator) {
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

bool CheckConstraint(const Constraint& ct,
                     std::function<int64(IntegerVariable*)> evaluator) {
  FZDLOG << "Checking " << ct.DebugString() << std::endl;
  const std::string& type = ct.type;
  if (type == "all_different_int") {
    return CheckAllDifferentInt(ct, evaluator);
  } else if (type == "alldifferent_except_0") {
    return CheckAlldifferentExcept0(ct, evaluator);
  } else if (type == "among") {
    return CheckAmong(ct, evaluator);
  } else if (type == "array_bool_and") {
    return CheckArrayBoolAnd(ct, evaluator);
  } else if (type == "array_bool_element") {
    return CheckArrayIntElement(ct, evaluator);
  } else if (type == "array_bool_or") {
    return CheckArrayBoolOr(ct, evaluator);
  } else if (type == "array_bool_xor") {
    return CheckArrayBoolXor(ct, evaluator);
  } else if (type == "array_int_element") {
    return CheckArrayIntElement(ct, evaluator);
  } else if (type == "array_var_bool_element") {
    return CheckArrayVarIntElement(ct, evaluator);
  } else if (type == "array_var_int_element") {
    return CheckArrayVarIntElement(ct, evaluator);
  } else if (type == "at_most_int") {
    return CheckAtMostInt(ct, evaluator);
  } else if (type == "bool_and") {
    return CheckBoolAnd(ct, evaluator);
  } else if (type == "bool_clause") {
    return CheckBoolClause(ct, evaluator);
  } else if (type == "bool_eq" || type == "bool2int") {
    return CheckIntEq(ct, evaluator);
  } else if (type == "bool_eq_reif") {
    return CheckIntEqReif(ct, evaluator);
  } else if (type == "bool_ge") {
    return CheckIntGe(ct, evaluator);
  } else if (type == "bool_ge_reif") {
    return CheckIntGeReif(ct, evaluator);
  } else if (type == "bool_gt") {
    return CheckIntGt(ct, evaluator);
  } else if (type == "bool_gt_reif") {
    return CheckIntGtReif(ct, evaluator);
  } else if (type == "bool_le") {
    return CheckIntLe(ct, evaluator);
  } else if (type == "bool_le_reif") {
    return CheckIntLeReif(ct, evaluator);
  } else if (type == "bool_left_imp") {
    return CheckIntLe(ct, evaluator);
  } else if (type == "bool_lin_eq") {
    return CheckIntLinEq(ct, evaluator);
  } else if (type == "bool_lin_le") {
    return CheckIntLinLe(ct, evaluator);
  } else if (type == "bool_lt") {
    return CheckIntLt(ct, evaluator);
  } else if (type == "bool_lt_reif") {
    return CheckIntLtReif(ct, evaluator);
  } else if (type == "bool_ne") {
    return CheckIntNe(ct, evaluator);
  } else if (type == "bool_ne_reif") {
    return CheckIntNeReif(ct, evaluator);
  } else if (type == "bool_not") {
    return CheckBoolNot(ct, evaluator);
  } else if (type == "bool_or") {
    return CheckBoolOr(ct, evaluator);
  } else if (type == "bool_right_imp") {
    return CheckIntGe(ct, evaluator);
  } else if (type == "bool_xor") {
    return CheckBoolXor(ct, evaluator);
  } else if (type == "circuit") {
    return CheckCircuit(ct, evaluator);
  } else if (type == "count_eq" || type == "count") {
    return CheckCountEq(ct, evaluator);
  } else if (type == "count_geq") {
    return CheckCountGeq(ct, evaluator);
  } else if (type == "count_gt") {
    return CheckCountGt(ct, evaluator);
  } else if (type == "count_leq") {
    return CheckCountLeq(ct, evaluator);
  } else if (type == "count_lt") {
    return CheckCountLt(ct, evaluator);
  } else if (type == "count_neq") {
    return CheckCountNeq(ct, evaluator);
  } else if (type == "count_reif") {
    return CheckCountReif(ct, evaluator);
  } else if (type == "cumulative" || type == "var_cumulative" ||
             type == "variable_cumulative" || type == "fixed_cumulative") {
    return CheckCumulative(ct, evaluator);
  } else if (type == "diffn") {
    return CheckDiffn(ct, evaluator);
  } else if (type == "diffn_k_with_sizes") {
    return CheckDiffnK(ct, evaluator);
  } else if (type == "diffn_nonstrict") {
    return CheckDiffnNonStrict(ct, evaluator);
  } else if (type == "diffn_nonstrict_k_with_sizes") {
    return CheckDiffnNonStrictK(ct, evaluator);
  } else if (type == "disjunctive") {
    return CheckDisjunctive(ct, evaluator);
  } else if (type == "disjunctive_strict") {
    return CheckDisjunctiveStrict(ct, evaluator);
  } else if (type == "false_constraint") {
    return CheckFalseConstraint(ct, evaluator);
  } else if (type == "global_cardinality") {
    return CheckGlobalCardinality(ct, evaluator);
  } else if (type == "global_cardinality_closed") {
    return CheckGlobalCardinalityClosed(ct, evaluator);
  } else if (type == "global_cardinality_low_up") {
    return CheckGlobalCardinalityLowUp(ct, evaluator);
  } else if (type == "global_cardinality_low_up_closed") {
    return CheckGlobalCardinalityLowUpClosed(ct, evaluator);
  } else if (type == "global_cardinality_old") {
    return CheckGlobalCardinalityOld(ct, evaluator);
  } else if (type == "int_abs") {
    return CheckIntAbs(ct, evaluator);
  } else if (type == "int_div") {
    return CheckIntDiv(ct, evaluator);
  } else if (type == "int_eq") {
    return CheckIntEq(ct, evaluator);
  } else if (type == "int_eq_reif") {
    return CheckIntEqReif(ct, evaluator);
  } else if (type == "int_ge") {
    return CheckIntGe(ct, evaluator);
  } else if (type == "int_ge_reif") {
    return CheckIntGeReif(ct, evaluator);
  } else if (type == "int_gt") {
    return CheckIntGt(ct, evaluator);
  } else if (type == "int_gt_reif") {
    return CheckIntGtReif(ct, evaluator);
  } else if (type == "int_le") {
    return CheckIntLe(ct, evaluator);
  } else if (type == "int_le_reif") {
    return CheckIntLeReif(ct, evaluator);
  } else if (type == "int_lin_eq") {
    return CheckIntLinEq(ct, evaluator);
  } else if (type == "int_lin_eq_reif") {
    return CheckIntLinEqReif(ct, evaluator);
  } else if (type == "int_lin_ge") {
    return CheckIntLinGe(ct, evaluator);
  } else if (type == "int_lin_ge_reif") {
    return CheckIntLinGeReif(ct, evaluator);
  } else if (type == "int_lin_le") {
    return CheckIntLinLe(ct, evaluator);
  } else if (type == "int_lin_le_reif") {
    return CheckIntLinLeReif(ct, evaluator);
  } else if (type == "int_lin_ne") {
    return CheckIntLinNe(ct, evaluator);
  } else if (type == "int_lin_ne_reif") {
    return CheckIntLinNeReif(ct, evaluator);
  } else if (type == "int_lt") {
    return CheckIntLt(ct, evaluator);
  } else if (type == "int_lt_reif") {
    return CheckIntLtReif(ct, evaluator);
  } else if (type == "int_max") {
    return CheckIntMax(ct, evaluator);
  } else if (type == "int_min") {
    return CheckIntMin(ct, evaluator);
  } else if (type == "int_minus") {
    return CheckIntMinus(ct, evaluator);
  } else if (type == "int_mod") {
    return CheckIntMod(ct, evaluator);
  } else if (type == "int_ne") {
    return CheckIntNe(ct, evaluator);
  } else if (type == "int_ne_reif") {
    return CheckIntNeReif(ct, evaluator);
  } else if (type == "int_negate") {
    return CheckIntNegate(ct, evaluator);
  } else if (type == "int_plus") {
    return CheckIntPlus(ct, evaluator);
  } else if (type == "int_times") {
    return CheckIntTimes(ct, evaluator);
  } else if (type == "inverse") {
    return CheckInverse(ct, evaluator);
  } else if (type == "lex_less_bool" || type == "lex_less_int") {
    return CheckLexLessInt(ct, evaluator);
  } else if (type == "lex_lesseq_bool" || type == "lex_lesseq_int") {
    return CheckLexLesseqInt(ct, evaluator);
  } else if (type == "maximum_arg_int") {
    return CheckMaximumArgInt(ct, evaluator);
  } else if (type == "maximum_int" || type == "array_int_maximum") {
    return CheckMaximumInt(ct, evaluator);
  } else if (type == "minimum_arg_int") {
    return CheckMinimumArgInt(ct, evaluator);
  } else if (type == "minimum_int" || type == "array_int_minimum") {
    return CheckMinimumInt(ct, evaluator);
  } else if (type == "nvalue") {
    return CheckNvalue(ct, evaluator);
  } else if (type == "regular") {
    return CheckRegular(ct, evaluator);
  } else if (type == "regular_nfa") {
    return CheckRegularNfa(ct, evaluator);
  } else if (type == "set_in" || type == "int_in") {
    return CheckSetIn(ct, evaluator);
  } else if (type == "set_not_in" || type == "int_not_in") {
    return CheckSetNotIn(ct, evaluator);
  } else if (type == "set_in_reif") {
    return CheckSetInReif(ct, evaluator);
  } else if (type == "sliding_sum") {
    return CheckSlidingSum(ct, evaluator);
  } else if (type == "sort") {
    return CheckSort(ct, evaluator);
  } else if (type == "subcircuit") {
    return CheckSubCircuit(ct, evaluator);
  } else if (type == "symmetric_all_different") {
    return CheckSymmetricAllDifferent(ct, evaluator);
  } else if (type == "table_bool" || type == "table_int") {
    return CheckTableInt(ct, evaluator);
  } else if (type == "true_constraint") {
    // Nothing to do.
    return true;
  } else {
    LOG(FATAL) << "Unknown predicate: " << type;
    return false;
  }
}
}  // namespace

bool CheckSolution(const Model& model,
                   std::function<int64(IntegerVariable*)> evaluator) {
  bool ok = true;
  for (Constraint* ct : model.constraints()) {
    if (ct->active && !CheckConstraint(*ct, evaluator)) {
      FZLOG << "Failing constraint " << ct->DebugString() << FZENDL;
      ok = false;
    }
  }
  return ok;
}

}  // namespace fz
}  // namespace operations_research
