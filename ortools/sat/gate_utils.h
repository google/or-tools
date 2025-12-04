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

// Functions to manipulate a "small" truth table where
// f(X0, X1, X2) is true iff bitmask[X0 + (X1 << 1) + (X2 << 2)] is true.
#ifndef ORTOOLS_SAT_GATE_UTILS_H_
#define ORTOOLS_SAT_GATE_UTILS_H_

#include <cstdint>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/sat_base.h"

namespace operations_research::sat {

using SmallBitset = uint32_t;

// Sort the key and modify the truth table accordingly.
//
// Note that we don't deal with identical key here, but the function
// CanonicalizeFunctionTruthTable() does, and that is sufficient for our use
// case.
template <typename VarOrLiteral>
void CanonicalizeTruthTable(absl::Span<VarOrLiteral> key,
                            SmallBitset& bitmask) {
  const int num_bits = key.size();
  CHECK_EQ(bitmask >> (1 << num_bits), 0);
  for (int i = 0; i < num_bits; ++i) {
    for (int j = i + 1; j < num_bits; ++j) {
      if (key[i] <= key[j]) continue;

      std::swap(key[i], key[j]);

      // We need to swap bit positions i and j in bitmask.
      SmallBitset new_bitmask = 0;
      for (int p = 0; p < (1 << num_bits); ++p) {
        const int value_i = (p >> i) & 1;
        const int value_j = (p >> j) & 1;
        int new_p = p;
        new_p ^= (value_i << i) ^ (value_j << j);  // Clear.
        new_p ^= (value_i << j) ^ (value_j << i);  // Swap.
        new_bitmask |= ((bitmask >> p) & 1) << new_p;
      }
      bitmask = new_bitmask;
      CHECK_EQ(bitmask >> (1 << num_bits), 0)
          << i << " " << j << " " << num_bits;
    }
  }
  CHECK(std::is_sorted(key.begin(), key.end()));
}

// Given a clause, return the truth table corresponding to it.
// Namely, a single value should be excluded.
inline void FillKeyAndBitmask(absl::Span<const Literal> clause,
                              absl::Span<BooleanVariable> key,
                              SmallBitset& bitmask) {
  CHECK_EQ(clause.size(), key.size());
  const int num_bits = clause.size();
  bitmask = ~SmallBitset(0) >> (32 - (1 << num_bits));  // All allowed;
  CHECK_EQ(bitmask >> (1 << num_bits), 0) << num_bits;
  SmallBitset bit_to_remove = 0;
  for (int i = 0; i < num_bits; ++i) {
    key[i] = clause[i].Variable();
    bit_to_remove |= (clause[i].IsPositive() ? 0 : 1) << i;
  }
  bitmask ^= (SmallBitset(1) << bit_to_remove);
  CHECK_EQ(bitmask >> (1 << num_bits), 0) << bit_to_remove << " " << num_bits;
  CanonicalizeTruthTable<BooleanVariable>(key, bitmask);
}

// Returns true iff the truth table encoded in bitmask encode a function
// Xi = f(Xj, j != i);
template <int num_bits>
bool IsFunction(int i, SmallBitset truth_table) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, num_bits);

  // We need to check that there is never two possibilities for Xi.
  for (int p = 0; p < (1 << num_bits); ++p) {
    if ((truth_table >> p) & (truth_table >> (p ^ (1 << i))) & 1) return false;
  }

  return true;
}

inline int AddHoleAtPosition(int i, int bitset) {
  return (bitset & ((1 << i) - 1)) + ((bitset >> i) << (i + 1));
}

// The function is target = function_values[inputs as bit position].
//
// TODO(user): This can be optimized with more bit twiddling if needed.
inline int CanonicalizeFunctionTruthTable(LiteralIndex& target,
                                          absl::Span<LiteralIndex> inputs,
                                          int& int_function_values) {
  // We want to fit on an int.
  CHECK_LE(inputs.size(), 4);

  // We assume smaller type.
  SmallBitset function_values = int_function_values;

  const int num_bits = inputs.size();
  CHECK_LE(num_bits, 4);  // Truth table must fit on an int.
  const SmallBitset all_one = (1 << (1 << num_bits)) - 1;
  CHECK_EQ(function_values & ~all_one, 0);

  // Make sure target is positive.
  if (!Literal(target).IsPositive()) {
    target = Literal(target).Negated();
    function_values = function_values ^ all_one;
    CHECK_EQ(function_values >> (1 << num_bits), 0);
  }

  // Make sure all inputs are positive.
  for (int i = 0; i < num_bits; ++i) {
    if (Literal(inputs[i]).IsPositive()) continue;

    inputs[i] = Literal(inputs[i]).NegatedIndex();

    // Position p go to position (p ^ (1 << i)).
    SmallBitset new_truth_table = 0;
    const SmallBitset to_xor = 1 << i;
    for (int p = 0; p < (1 << num_bits); ++p) {
      new_truth_table |= ((function_values >> p) & 1) << (p ^ to_xor);
    }
    function_values = new_truth_table;
    CHECK_EQ(function_values >> (1 << num_bits), 0);
  }

  // Sort the inputs now.
  CanonicalizeTruthTable<LiteralIndex>(inputs, function_values);
  CHECK_EQ(function_values >> (1 << num_bits), 0);

  // Merge identical variables.
  for (int i = 0; i < inputs.size(); ++i) {
    for (int j = i + 1; j < inputs.size();) {
      if (inputs[i] == inputs[j]) {
        // Lets remove input j.
        for (int k = j; k + 1 < inputs.size(); ++k) inputs[k] = inputs[k + 1];
        inputs.remove_suffix(1);

        SmallBitset new_truth_table = 0;
        for (int p = 0; p < (1 << inputs.size()); ++p) {
          int extended_p = AddHoleAtPosition(j, p);
          extended_p |= ((p >> i) & 1) << j;  // fill it with bit i.
          new_truth_table |= ((function_values >> extended_p) & 1) << p;
        }
        function_values = new_truth_table;
      } else {
        ++j;
      }
    }
  }

  // Lower arity?
  // This can happen if the output do not depend on one of the inputs.
  for (int i = 0; i < inputs.size();) {
    bool remove = true;
    for (int p = 0; p < (1 << inputs.size()); ++p) {
      if (((function_values >> p) & 1) !=
          ((function_values >> (p ^ (1 << i))) & 1)) {
        remove = false;
        break;
      }
    }
    if (remove) {
      // Lets remove input i.
      for (int k = i; k + 1 < inputs.size(); ++k) inputs[k] = inputs[k + 1];
      inputs.remove_suffix(1);

      SmallBitset new_truth_table = 0;
      for (int p = 0; p < (1 << inputs.size()); ++p) {
        const int extended_p = AddHoleAtPosition(i, p);
        new_truth_table |= ((function_values >> extended_p) & 1) << p;
      }
      function_values = new_truth_table;
    } else {
      ++i;
    }
  }

  int_function_values = function_values;
  return inputs.size();
}

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_GATE_UTILS_H_
