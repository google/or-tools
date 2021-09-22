// Copyright 2010-2021 Google LLC
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
#include <cstdint>
#include <limits>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/map_util.h"
#include "ortools/flatzinc/model.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace fz {
namespace {

// Helpers

int64_t Eval(const Argument& arg,
             const std::function<int64_t(Variable*)>& evaluator) {
  switch (arg.type) {
    case Argument::INT_VALUE: {
      return arg.Value();
    }
    case Argument::VAR_REF: {
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

int64_t EvalAt(const Argument& arg, int pos,
               const std::function<int64_t(Variable*)>& evaluator) {
  switch (arg.type) {
    case Argument::INT_LIST: {
      return arg.ValueAt(pos);
    }
    case Argument::VAR_REF_ARRAY: {
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
                          const std::function<int64_t(Variable*)>& evaluator) {
  absl::flat_hash_set<int64_t> visited;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    if (visited.contains(value)) {
      return false;
    }
    visited.insert(value);
  }

  return true;
}

bool CheckAlldifferentExcept0(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  absl::flat_hash_set<int64_t> visited;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    if (value != 0 && visited.contains(value)) {
      return false;
    }
    visited.insert(value);
  }

  return true;
}

bool CheckAmong(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t expected = Eval(ct.arguments[0], evaluator);
  int64_t count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    count += ct.arguments[2].Contains(value);
  }

  return count == expected;
}

bool CheckArrayBoolAnd(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 1;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::min(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolOr(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::max(result, value);
  }

  return result == Eval(ct.arguments[1], evaluator);
}

bool CheckArrayBoolXor(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 0;

  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator);
  }

  return result % 2 == 1;
}

bool CheckArrayIntElement(const Constraint& ct,
                          const std::function<int64_t(Variable*)>& evaluator) {
  if (ct.arguments[0].variables.size() == 2) {
    // TODO(user): Check 2D element.
    return true;
  }
  // Flatzinc arrays are 1 based.
  const int64_t shifted_index = Eval(ct.arguments[0], evaluator) - 1;
  const int64_t element = EvalAt(ct.arguments[1], shifted_index, evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return element == target;
}

bool CheckArrayIntElementNonShifted(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  CHECK_EQ(ct.arguments[0].variables.size(), 1);
  const int64_t index = Eval(ct.arguments[0], evaluator);
  const int64_t element = EvalAt(ct.arguments[1], index, evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return element == target;
}

bool CheckArrayVarIntElement(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  if (ct.arguments[0].variables.size() == 2) {
    // TODO(user): Check 2D element.
    return true;
  }
  // Flatzinc arrays are 1 based.
  const int64_t shifted_index = Eval(ct.arguments[0], evaluator) - 1;
  const int64_t element = EvalAt(ct.arguments[1], shifted_index, evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return element == target;
}

bool CheckAtMostInt(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t expected = Eval(ct.arguments[0], evaluator);
  const int64_t value = Eval(ct.arguments[2], evaluator);

  int64_t count = 0;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    count += EvalAt(ct.arguments[1], i, evaluator) == value;
  }
  return count <= expected;
}

bool CheckBoolAnd(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckBoolClause(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 0;

  // Positive variables.
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    result = std::max(result, value);
  }
  // Negative variables.
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    const int64_t value = EvalAt(ct.arguments[1], i, evaluator);
    result = std::max(result, 1 - value);
  }

  return result;
}

bool CheckBoolNot(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left == 1 - right;
}

bool CheckBoolOr(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckBoolXor(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == (left + right == 1);
}

bool CheckCircuit(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int size = Size(ct.arguments[0]);
  const int base = ct.arguments[1].Value();

  absl::flat_hash_set<int64_t> visited;
  int64_t current = 0;
  for (int i = 0; i < size; ++i) {
    const int64_t next = EvalAt(ct.arguments[0], current, evaluator) - base;
    visited.insert(next);
    current = next;
  }
  return visited.size() == size;
}

int64_t ComputeCount(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 0;
  const int64_t value = Eval(ct.arguments[1], evaluator);
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) == value;
  }
  return result;
}

bool CheckCountEq(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count == expected;
}

bool CheckCountGeq(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count >= expected;
}

bool CheckCountGt(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count > expected;
}

bool CheckCountLeq(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count <= expected;
}

bool CheckCountLt(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count < expected;
}

bool CheckCountNeq(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  return count != expected;
}

bool CheckCountReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = ComputeCount(ct, evaluator);
  const int64_t expected = Eval(ct.arguments[2], evaluator);
  const int64_t status = Eval(ct.arguments[3], evaluator);
  return status == (expected == count);
}

bool CheckCumulative(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  // TODO(user): Improve complexity for large durations.
  const int64_t capacity = Eval(ct.arguments[3], evaluator);
  const int size = Size(ct.arguments[0]);
  CHECK_EQ(size, Size(ct.arguments[1]));
  CHECK_EQ(size, Size(ct.arguments[2]));
  absl::flat_hash_map<int64_t, int64_t> usage;
  for (int i = 0; i < size; ++i) {
    const int64_t start = EvalAt(ct.arguments[0], i, evaluator);
    const int64_t duration = EvalAt(ct.arguments[1], i, evaluator);
    const int64_t requirement = EvalAt(ct.arguments[2], i, evaluator);
    for (int64_t t = start; t < start + duration; ++t) {
      usage[t] += requirement;
      if (usage[t] > capacity) {
        return false;
      }
    }
  }
  return true;
}

bool CheckDiffn(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckDiffnK(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckDiffnNonStrict(const Constraint& ct,
                         const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckDiffnNonStrictK(const Constraint& ct,
                          const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckDisjunctive(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckDisjunctiveStrict(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckFalseConstraint(const Constraint& ct,
                          const std::function<int64_t(Variable*)>& evaluator) {
  return false;
}

std::vector<int64_t> ComputeGlobalCardinalityCards(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  std::vector<int64_t> cards(Size(ct.arguments[1]), 0);
  absl::flat_hash_map<int64_t, int> positions;
  for (int i = 0; i < ct.arguments[1].values.size(); ++i) {
    const int64_t value = ct.arguments[1].values[i];
    CHECK(!positions.contains(value));
    positions[value] = i;
  }
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int value = EvalAt(ct.arguments[0], i, evaluator);
    if (positions.contains(value)) {
      cards[positions[value]]++;
    }
  }
  return cards;
}

bool CheckGlobalCardinality(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const std::vector<int64_t> cards =
      ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), Size(ct.arguments[2]));
  for (int i = 0; i < Size(ct.arguments[2]); ++i) {
    const int64_t card = EvalAt(ct.arguments[2], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  return true;
}

bool CheckGlobalCardinalityClosed(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const std::vector<int64_t> cards =
      ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), Size(ct.arguments[2]));
  for (int i = 0; i < Size(ct.arguments[2]); ++i) {
    const int64_t card = EvalAt(ct.arguments[2], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  int64_t sum_of_cards = 0;
  for (int64_t card : cards) {
    sum_of_cards += card;
  }
  return sum_of_cards == Size(ct.arguments[0]);
}

bool CheckGlobalCardinalityLowUp(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const std::vector<int64_t> cards =
      ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), ct.arguments[2].values.size());
  CHECK_EQ(cards.size(), ct.arguments[3].values.size());
  for (int i = 0; i < cards.size(); ++i) {
    const int64_t card = cards[i];
    if (card < ct.arguments[2].values[i] || card > ct.arguments[3].values[i]) {
      return false;
    }
  }
  return true;
}

bool CheckGlobalCardinalityLowUpClosed(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const std::vector<int64_t> cards =
      ComputeGlobalCardinalityCards(ct, evaluator);
  CHECK_EQ(cards.size(), ct.arguments[2].values.size());
  CHECK_EQ(cards.size(), ct.arguments[3].values.size());
  for (int i = 0; i < cards.size(); ++i) {
    const int64_t card = cards[i];
    if (card < ct.arguments[2].values[i] || card > ct.arguments[3].values[i]) {
      return false;
    }
  }
  int64_t sum_of_cards = 0;
  for (int64_t card : cards) {
    sum_of_cards += card;
  }
  return sum_of_cards == Size(ct.arguments[0]);
}

bool CheckGlobalCardinalityOld(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const int size = Size(ct.arguments[1]);
  std::vector<int64_t> cards(size, 0);
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator);
    if (value >= 0 && value < size) {
      cards[value]++;
    }
  }
  for (int i = 0; i < size; ++i) {
    const int64_t card = EvalAt(ct.arguments[1], i, evaluator);
    if (card != cards[i]) {
      return false;
    }
  }
  return true;
}

bool CheckIntAbs(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return std::abs(left) == right;
}

bool CheckIntDiv(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == left / right;
}

bool CheckIntEq(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left == right;
}

bool CheckIntEqImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left == right)) || !status;
}

bool CheckIntEqReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left == right);
}

bool CheckIntGe(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left >= right;
}

bool CheckIntGeImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left >= right)) || !status;
}

bool CheckIntGeReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left >= right);
}

bool CheckIntGt(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left > right;
}

bool CheckIntGtImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left > right)) || !status;
}

bool CheckIntGtReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left > right);
}

bool CheckIntLe(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left <= right;
}

bool CheckIntLeImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left <= right)) || !status;
}

bool CheckIntLeReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left <= right);
}

bool CheckIntLt(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left < right;
}

bool CheckIntLtImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left < right)) || !status;
}

bool CheckIntLtReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left < right);
}

int64_t ComputeIntLin(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  int64_t result = 0;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    result += EvalAt(ct.arguments[0], i, evaluator) *
              EvalAt(ct.arguments[1], i, evaluator);
  }
  return result;
}

bool CheckIntLinEq(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  return left == right;
}

bool CheckIntLinEqImp(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return (status && (left == right)) || !status;
}

bool CheckIntLinEqReif(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return status == (left == right);
}

bool CheckIntLinGe(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  return left >= right;
}

bool CheckIntLinGeImp(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return (status && (left >= right)) || !status;
}

bool CheckIntLinGeReif(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return status == (left >= right);
}

bool CheckIntLinLe(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  return left <= right;
}

bool CheckIntLinLeImp(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return (status && (left <= right)) || !status;
}

bool CheckIntLinLeReif(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return status == (left <= right);
}

bool CheckIntLinNe(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  return left != right;
}

bool CheckIntLinNeImp(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return (status && (left != right)) || !status;
}

bool CheckIntLinNeReif(const Constraint& ct,
                       const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = ComputeIntLin(ct, evaluator);
  const int64_t right = Eval(ct.arguments[2], evaluator);
  const bool status = Eval(ct.arguments[3], evaluator) != 0;
  return status == (left != right);
}

bool CheckIntMax(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t status = Eval(ct.arguments[2], evaluator);
  return status == std::max(left, right);
}

bool CheckIntMin(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t status = Eval(ct.arguments[2], evaluator);
  return status == std::min(left, right);
}

bool CheckIntMinus(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == left - right;
}

bool CheckIntMod(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == left % right;
}

bool CheckIntNe(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left != right;
}

bool CheckIntNeImp(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return (status && (left != right)) || !status;
}

bool CheckIntNeReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const bool status = Eval(ct.arguments[2], evaluator) != 0;
  return status == (left != right);
}

bool CheckIntNegate(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  return left == -right;
}

bool CheckIntPlus(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == left + right;
}

bool CheckIntTimes(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t left = Eval(ct.arguments[0], evaluator);
  const int64_t right = Eval(ct.arguments[1], evaluator);
  const int64_t target = Eval(ct.arguments[2], evaluator);
  return target == left * right;
}

bool CheckInverse(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  const int size = Size(ct.arguments[0]);
  const int f_base = ct.arguments[2].Value();
  const int invf_base = ct.arguments[3].Value();
  // Check all bounds.
  for (int i = 0; i < size; ++i) {
    const int64_t x = EvalAt(ct.arguments[0], i, evaluator) - invf_base;
    const int64_t y = EvalAt(ct.arguments[1], i, evaluator) - f_base;
    if (x < 0 || x >= size || y < 0 || y >= size) {
      return false;
    }
  }

  // Check f-1(f(i)) = i.
  for (int i = 0; i < size; ++i) {
    const int64_t fi = EvalAt(ct.arguments[0], i, evaluator) - invf_base;
    const int64_t invf_fi = EvalAt(ct.arguments[1], fi, evaluator) - f_base;
    if (invf_fi != i) {
      return false;
    }
  }

  return true;
}

bool CheckLexLessInt(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t x = EvalAt(ct.arguments[0], i, evaluator);
    const int64_t y = EvalAt(ct.arguments[1], i, evaluator);
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
                       const std::function<int64_t(Variable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t x = EvalAt(ct.arguments[0], i, evaluator);
    const int64_t y = EvalAt(ct.arguments[1], i, evaluator);
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
                        const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t max_index = Eval(ct.arguments[1], evaluator) - 1;
  const int64_t max_value = EvalAt(ct.arguments[0], max_index, evaluator);
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
                     const std::function<int64_t(Variable*)>& evaluator) {
  int64_t max_value = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    max_value = std::max(max_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return max_value == Eval(ct.arguments[0], evaluator);
}

bool CheckMinimumArgInt(const Constraint& ct,
                        const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t min_index = Eval(ct.arguments[1], evaluator) - 1;
  const int64_t min_value = EvalAt(ct.arguments[0], min_index, evaluator);
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
                     const std::function<int64_t(Variable*)>& evaluator) {
  int64_t min_value = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    min_value = std::min(min_value, EvalAt(ct.arguments[1], i, evaluator));
  }
  return min_value == Eval(ct.arguments[0], evaluator);
}

bool CheckNetworkFlowConservation(
    const Argument& arcs, const Argument& balance_input,
    const Argument& flow_vars,
    const std::function<int64_t(Variable*)>& evaluator) {
  std::vector<int64_t> balance(balance_input.values);

  const int num_arcs = Size(arcs) / 2;
  for (int arc = 0; arc < num_arcs; arc++) {
    const int tail = arcs.values[arc * 2] - 1;
    const int head = arcs.values[arc * 2 + 1] - 1;
    const int64_t flow = EvalAt(flow_vars, arc, evaluator);
    balance[tail] -= flow;
    balance[head] += flow;
  }

  for (const int64_t value : balance) {
    if (value != 0) return false;
  }

  return true;
}

bool CheckNetworkFlow(const Constraint& ct,
                      const std::function<int64_t(Variable*)>& evaluator) {
  return CheckNetworkFlowConservation(ct.arguments[0], ct.arguments[1],
                                      ct.arguments[2], evaluator);
}

bool CheckNetworkFlowCost(const Constraint& ct,
                          const std::function<int64_t(Variable*)>& evaluator) {
  if (!CheckNetworkFlowConservation(ct.arguments[0], ct.arguments[1],
                                    ct.arguments[3], evaluator)) {
    return false;
  }

  int64_t total_cost = 0;
  const int num_arcs = Size(ct.arguments[3]);
  for (int arc = 0; arc < num_arcs; arc++) {
    const int64_t flow = EvalAt(ct.arguments[3], arc, evaluator);
    const int64_t cost = EvalAt(ct.arguments[2], arc, evaluator);
    total_cost += flow * cost;
  }

  return total_cost == Eval(ct.arguments[4], evaluator);
}

bool CheckNvalue(const Constraint& ct,
                 const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t count = Eval(ct.arguments[0], evaluator);
  absl::flat_hash_set<int64_t> all_values;
  for (int i = 0; i < Size(ct.arguments[1]); ++i) {
    all_values.insert(EvalAt(ct.arguments[1], i, evaluator));
  }

  return count == all_values.size();
}

bool CheckRegular(const Constraint& ct,
                  const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckRegularNfa(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckSetIn(const Constraint& ct,
                const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t value = Eval(ct.arguments[0], evaluator);
  return ct.arguments[1].Contains(value);
}

bool CheckSetNotIn(const Constraint& ct,
                   const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t value = Eval(ct.arguments[0], evaluator);
  return !ct.arguments[1].Contains(value);
}

bool CheckSetInReif(const Constraint& ct,
                    const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t value = Eval(ct.arguments[0], evaluator);
  const int64_t status = Eval(ct.arguments[2], evaluator);
  return status == ct.arguments[1].Contains(value);
}

bool CheckSlidingSum(const Constraint& ct,
                     const std::function<int64_t(Variable*)>& evaluator) {
  const int64_t low = Eval(ct.arguments[0], evaluator);
  const int64_t up = Eval(ct.arguments[1], evaluator);
  const int64_t length = Eval(ct.arguments[2], evaluator);
  // Compute initial sum.
  int64_t sliding_sum = 0;
  for (int i = 0; i < std::min<int64_t>(length, Size(ct.arguments[3])); ++i) {
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
               const std::function<int64_t(Variable*)>& evaluator) {
  CHECK_EQ(Size(ct.arguments[0]), Size(ct.arguments[1]));
  absl::flat_hash_map<int64_t, int> init_count;
  absl::flat_hash_map<int64_t, int> sorted_count;
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
                     const std::function<int64_t(Variable*)>& evaluator) {
  absl::flat_hash_set<int64_t> visited;
  const int base = ct.arguments[1].Value();
  // Find inactive nodes (pointing to themselves).
  int64_t current = -1;
  for (int i = 0; i < Size(ct.arguments[0]); ++i) {
    const int64_t next = EvalAt(ct.arguments[0], i, evaluator) - base;
    if (next != i && current == -1) {
      current = next;
    } else if (next == i) {
      visited.insert(next);
    }
  }

  // Try to find a path of length 'residual_size'.
  const int residual_size = Size(ct.arguments[0]) - visited.size();
  for (int i = 0; i < residual_size; ++i) {
    const int64_t next = EvalAt(ct.arguments[0], current, evaluator) - base;
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
                   const std::function<int64_t(Variable*)>& evaluator) {
  return true;
}

bool CheckSymmetricAllDifferent(
    const Constraint& ct, const std::function<int64_t(Variable*)>& evaluator) {
  const int size = Size(ct.arguments[0]);
  for (int i = 0; i < size; ++i) {
    const int64_t value = EvalAt(ct.arguments[0], i, evaluator) - 1;
    if (value < 0 || value >= size) {
      return false;
    }
    const int64_t reverse_value = EvalAt(ct.arguments[0], value, evaluator) - 1;
    if (reverse_value != i) {
      return false;
    }
  }
  return true;
}

using CallMap =
    absl::flat_hash_map<std::string,
                        std::function<bool(const Constraint& ct,
                                           std::function<int64_t(Variable*)>)>>;

// Creates a map between flatzinc predicates and CP-SAT builders.
//
// Predicates starting with fzn_ are predicates with the same name in flatzinc
// and in minizinc. The fzn_ prefix is added to differentiate them.
//
// Predicates starting with ortools_ are predicates defined only in or-tools.
// They are created at compilation time when using the or-tools mzn library.
CallMap CreateCallMap() {
  CallMap m;
  m["fzn_all_different_int"] = CheckAllDifferentInt;
  m["alldifferent_except_0"] = CheckAlldifferentExcept0;
  m["among"] = CheckAmong;
  m["array_bool_and"] = CheckArrayBoolAnd;
  m["array_bool_element"] = CheckArrayIntElement;
  m["array_bool_or"] = CheckArrayBoolOr;
  m["array_bool_xor"] = CheckArrayBoolXor;
  m["array_int_element"] = CheckArrayIntElement;
  m["array_int_element_nonshifted"] = CheckArrayIntElementNonShifted;
  m["array_var_bool_element"] = CheckArrayVarIntElement;
  m["array_var_int_element"] = CheckArrayVarIntElement;
  m["at_most_int"] = CheckAtMostInt;
  m["bool_and"] = CheckBoolAnd;
  m["bool_clause"] = CheckBoolClause;
  m["bool_eq"] = CheckIntEq;
  m["bool2int"] = CheckIntEq;
  m["bool_eq_imp"] = CheckIntEqImp;
  m["bool_eq_reif"] = CheckIntEqReif;
  m["bool_ge"] = CheckIntGe;
  m["bool_ge_imp"] = CheckIntGeImp;
  m["bool_ge_reif"] = CheckIntGeReif;
  m["bool_gt"] = CheckIntGt;
  m["bool_gt_imp"] = CheckIntGtImp;
  m["bool_gt_reif"] = CheckIntGtReif;
  m["bool_le"] = CheckIntLe;
  m["bool_le_imp"] = CheckIntLeImp;
  m["bool_le_reif"] = CheckIntLeReif;
  m["bool_left_imp"] = CheckIntLe;
  m["bool_lin_eq"] = CheckIntLinEq;
  m["bool_lin_le"] = CheckIntLinLe;
  m["bool_lt"] = CheckIntLt;
  m["bool_lt_imp"] = CheckIntLtImp;
  m["bool_lt_reif"] = CheckIntLtReif;
  m["bool_ne"] = CheckIntNe;
  m["bool_ne_imp"] = CheckIntNeImp;
  m["bool_ne_reif"] = CheckIntNeReif;
  m["bool_not"] = CheckBoolNot;
  m["bool_or"] = CheckBoolOr;
  m["bool_right_imp"] = CheckIntGe;
  m["bool_xor"] = CheckBoolXor;
  m["ortools_circuit"] = CheckCircuit;
  m["count_eq"] = CheckCountEq;
  m["count"] = CheckCountEq;
  m["count_geq"] = CheckCountGeq;
  m["count_gt"] = CheckCountGt;
  m["count_leq"] = CheckCountLeq;
  m["count_lt"] = CheckCountLt;
  m["count_neq"] = CheckCountNeq;
  m["count_reif"] = CheckCountReif;
  m["fzn_cumulative"] = CheckCumulative;
  m["var_cumulative"] = CheckCumulative;
  m["variable_cumulative"] = CheckCumulative;
  m["fixed_cumulative"] = CheckCumulative;
  m["fzn_diffn"] = CheckDiffn;
  m["diffn_k_with_sizes"] = CheckDiffnK;
  m["fzn_diffn_nonstrict"] = CheckDiffnNonStrict;
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
  m["int_eq_imp"] = CheckIntEqImp;
  m["int_eq_reif"] = CheckIntEqReif;
  m["int_ge"] = CheckIntGe;
  m["int_ge_imp"] = CheckIntGeImp;
  m["int_ge_reif"] = CheckIntGeReif;
  m["int_gt"] = CheckIntGt;
  m["int_gt_imp"] = CheckIntGtImp;
  m["int_gt_reif"] = CheckIntGtReif;
  m["int_le"] = CheckIntLe;
  m["int_le_imp"] = CheckIntLeImp;
  m["int_le_reif"] = CheckIntLeReif;
  m["int_lin_eq"] = CheckIntLinEq;
  m["int_lin_eq_imp"] = CheckIntLinEqImp;
  m["int_lin_eq_reif"] = CheckIntLinEqReif;
  m["int_lin_ge"] = CheckIntLinGe;
  m["int_lin_ge_imp"] = CheckIntLinGeImp;
  m["int_lin_ge_reif"] = CheckIntLinGeReif;
  m["int_lin_le"] = CheckIntLinLe;
  m["int_lin_le_imp"] = CheckIntLinLeImp;
  m["int_lin_le_reif"] = CheckIntLinLeReif;
  m["int_lin_ne"] = CheckIntLinNe;
  m["int_lin_ne_imp"] = CheckIntLinNeImp;
  m["int_lin_ne_reif"] = CheckIntLinNeReif;
  m["int_lt"] = CheckIntLt;
  m["int_lt_imp"] = CheckIntLtImp;
  m["int_lt_reif"] = CheckIntLtReif;
  m["int_max"] = CheckIntMax;
  m["int_min"] = CheckIntMin;
  m["int_minus"] = CheckIntMinus;
  m["int_mod"] = CheckIntMod;
  m["int_ne"] = CheckIntNe;
  m["int_ne_imp"] = CheckIntNeImp;
  m["int_ne_reif"] = CheckIntNeReif;
  m["int_negate"] = CheckIntNegate;
  m["int_plus"] = CheckIntPlus;
  m["int_times"] = CheckIntTimes;
  m["ortools_inverse"] = CheckInverse;
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
  m["ortools_network_flow"] = CheckNetworkFlow;
  m["ortools_network_flow_cost"] = CheckNetworkFlowCost;
  m["nvalue"] = CheckNvalue;
  m["ortools_regular"] = CheckRegular;
  m["regular_nfa"] = CheckRegularNfa;
  m["set_in"] = CheckSetIn;
  m["int_in"] = CheckSetIn;
  m["set_not_in"] = CheckSetNotIn;
  m["int_not_in"] = CheckSetNotIn;
  m["set_in_reif"] = CheckSetInReif;
  m["sliding_sum"] = CheckSlidingSum;
  m["sort"] = CheckSort;
  m["ortools_subcircuit"] = CheckSubCircuit;
  m["symmetric_all_different"] = CheckSymmetricAllDifferent;
  m["ortools_table_bool"] = CheckTableInt;
  m["ortools_table_int"] = CheckTableInt;
  return m;
}

}  // namespace

bool CheckSolution(const Model& model,
                   const std::function<int64_t(Variable*)>& evaluator,
                   SolverLogger* logger) {
  bool ok = true;
  const CallMap call_map = CreateCallMap();
  for (Constraint* ct : model.constraints()) {
    if (!ct->active) continue;
    const auto& checker = gtl::FindOrDie(call_map, ct->type);
    if (!checker(*ct, evaluator)) {
      SOLVER_LOG(logger, "Failing constraint ", ct->DebugString());
      ok = false;
    }
  }
  return ok;
}

}  // namespace fz
}  // namespace operations_research
