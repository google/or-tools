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

#include <bitset>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {

using SmallBitset = uint32_t;

// This works for num_bits == 32 too.
inline SmallBitset GetNumBitsAtOne(int num_bits) {
  DCHECK_GT(num_bits, 0);
  return ~SmallBitset(0) >> (32 - num_bits);
}

// Sort the key and modify the truth table accordingly.
//
// Note that we don't deal with identical key here, but the function
// CanonicalizeFunctionTruthTable() does, and that is sufficient for our use
// case.
template <typename VarOrLiteral>
void CanonicalizeTruthTable(absl::Span<VarOrLiteral> key,
                            SmallBitset& bitmask) {
  const int num_bits = key.size();
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
    }
  }
  DCHECK(std::is_sorted(key.begin(), key.end()));
}

// Given a clause, return the truth table corresponding to it.
// Namely, a single value should be excluded.
inline void FillKeyAndBitmask(absl::Span<const Literal> clause,
                              absl::Span<BooleanVariable> key,
                              SmallBitset& bitmask) {
  CHECK_EQ(clause.size(), key.size());
  const int num_bits = clause.size();
  SmallBitset bit_to_remove = 0;
  for (int i = 0; i < num_bits; ++i) {
    key[i] = clause[i].Variable();
    bit_to_remove |= (clause[i].IsPositive() ? 0 : 1) << i;
  }
  CHECK_LT(bit_to_remove, (1 << num_bits));
  bitmask = GetNumBitsAtOne(1 << num_bits);
  bitmask ^= SmallBitset(1) << bit_to_remove;
  CanonicalizeTruthTable<BooleanVariable>(key, bitmask);
}

// Returns true iff the truth table encoded in bitmask encode a function
// Xi = f(Xj, j != i);
inline bool IsFunction(int i, int num_bits, SmallBitset truth_table) {
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

inline int RemoveFixedInput(int i, bool at_true, absl::Span<Literal> inputs,
                            int& int_function_values) {
  DCHECK_LT(i, inputs.size());
  const int value = at_true ? 1 : 0;

  // Re-compute the bitset.
  SmallBitset values = int_function_values;
  SmallBitset new_truth_table = 0;
  const int new_size = inputs.size() - 1;
  for (int p = 0; p < (1 << new_size); ++p) {
    const int extended_p = AddHoleAtPosition(i, p) | (value << i);
    new_truth_table |= ((values >> extended_p) & 1) << p;
  }
  int_function_values = new_truth_table;

  for (int j = i + 1; j < inputs.size(); ++j) {
    inputs[j - 1] = inputs[j];
  }
  return new_size;
}

inline void MakeAllInputsPositive(absl::Span<Literal> inputs,
                                  SmallBitset& bitmask) {
  // We want to fit on a SmallBitset.
  const int num_bits = inputs.size();
  CHECK_LE(num_bits, 5);

  // Make sure all inputs are positive.
  for (int i = 0; i < num_bits; ++i) {
    if (inputs[i].IsPositive()) continue;

    inputs[i] = inputs[i].Negated();

    // Position p go to position (p ^ (1 << i)).
    SmallBitset new_truth_table = 0;
    const SmallBitset to_xor = 1 << i;
    for (int p = 0; p < (1 << num_bits); ++p) {
      new_truth_table |= ((bitmask >> p) & 1) << (p ^ to_xor);
    }
    bitmask = new_truth_table;
  }
}

// Similar to CanonicalizeTruthTable, but perform more canonicalization.
//
// TODO(user): This can be optimized with more bit twiddling if needed.
inline int FullyCanonicalizeTruthTable(absl::Span<Literal> inputs,
                                       SmallBitset& bitmask) {
  MakeAllInputsPositive(inputs, bitmask);

  // Sort the inputs now.
  CanonicalizeTruthTable<Literal>(inputs, bitmask);

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
          new_truth_table |= ((bitmask >> extended_p) & 1) << p;
        }
        bitmask = new_truth_table;
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
      if (((bitmask >> p) & 1) != ((bitmask >> (p ^ (1 << i))) & 1)) {
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
        new_truth_table |= ((bitmask >> extended_p) & 1) << p;
      }
      bitmask = new_truth_table;
    } else {
      ++i;
    }
  }

  return inputs.size();
}

// The function is target = function_values[inputs as bit position].
inline int CanonicalizeFunctionTruthTable(Literal& target,
                                          absl::Span<Literal> inputs,
                                          int& int_function_values) {
  // We want to fit on an int.
  CHECK_LE(inputs.size(), 4);
  SmallBitset function_values = int_function_values;
  const int new_size = FullyCanonicalizeTruthTable(inputs, function_values);

  // If we have x = f(a,b,c) and not(y) = f(a,b,c) with the same f, we have an
  // equivalence, so we need to canonicalicpze both f() and not(f()) to the same
  // function. For that we just always choose to have the lowest bit at zero.
  if (function_values & 1) {
    target = target.Negated();
    const SmallBitset all_one = GetNumBitsAtOne(1 << new_size);
    function_values = function_values ^ all_one;
    DCHECK_EQ(function_values >> (1 << new_size), 0);
  }
  DCHECK_EQ(function_values & 1, 0);

  int_function_values = function_values;
  return new_size;
}

// Combines 64 "pairs of bits" using a function on two bits given by its truth
// table. For two bits, we have result = (type >> (a + 2 * b)) & 1.
// This just apply this to all 64 positions of a and b.
inline uint64_t CombineGate2(int type, uint64_t a, uint64_t b) {
  switch (type) {
    case 0b0000:
      return uint64_t{0};
    case 0b0001:
      return ~b & ~a;
    case 0b0010:
      return ~b & a;
    case 0b0011:
      return ~b;
    case 0b0100:
      return b & ~a;
    case 0b0101:
      return ~a;
    case 0b0110:
      return b ^ a;
    case 0b0111:
      return ~(b & a);
    case 0b1000:
      return b & a;
    case 0b1001:
      return ~(b ^ a);
    case 0b1010:
      return a;
    case 0b1011:
      return ~b | a;
    case 0b1100:
      return b;
    case 0b1101:
      return b | ~a;
    case 0b1110:
      return b | a;
    case 0b1111:
      return ~uint64_t{0};
    default:
      return 0;
  };
}

// Encodes a simple binary gate target = f(a, b) where f() is given by its 4
// bits value table (we call it type here).
//
// Note that degenerate case are supported, like
// - unary function target = f(a, a)
// - zero-ary function target = 0 / 1 independently of a or b.
struct BinaryGate {
  // When target == kConstraintTarget, it means this is just a constraint
  // on a and b with a truth table given by the type of f(). This is the
  // same as having a target fixed to 1.
  static constexpr int kConstraintTarget = -1;

  BinaryGate() = default;

  BinaryGate(SmallBitset _type, int _target, int _a, int _b)
      : type(_type), target(_target), a(_a), b(_b) {
    CHECK_GE(a, 0);
    CHECK_GE(b, 0);
    Canonicalize();
  }

  std::string DebugString() const {
    return absl::StrCat(a, " ", b, " -> ", target, " ",
                        std::bitset<4>(type).to_string());
  }

  void Canonicalize() {
    if (a > b) {
      std::swap(a, b);
      if (absl::popcount(type & 0b0110) == 1) {
        type ^= 0b0110;  // swap position 10 and 01.
      }
    }
  }

  // Deal with simple simplification cases.
  void Simplify() {
    Canonicalize();
    if (type == 0b0101 || type == 0b1010) {
      // The gate does not depend on b.
      b = a;
    } else if (type == 0b0011 || type == 0b1100) {
      // The gate does not depend on a.
      a = b;
    } else if (type == 0b0000 || type == 0b1111) {
      // The gate is constant.
      // TODO(user): This is not handled super well.
      a = b = 0;
    }
  }

  // value[target] = (type >> (value[a] + 2 * value[b])) & 1.
  SmallBitset type = 0;
  int target = 0;
  int a = 0;
  int b = 0;
};

// A SAT problem only expressed in terms of binary gates.
// This encodes "outputs = f(inputs)".
//
// Variables are always sorted in topological order, and a variable v will first
// be the output of a gate with strictly lower inputs, before it is used.
//
// It is possible to have "constraints" by adding gates on already defined
// targets.
struct BinaryCircuit {
  std::string DebugString() const;

  // Resets the mapping/reverse_mapping field to an identity mapping.
  void ResetBooleanMapping() {
    mapping.resize(num_vars);
    reverse_mapping.resize(num_vars);
    for (int i = 0; i < num_vars; ++i) {
      operations_research::sat::BooleanVariable bool_var(i);
      mapping[bool_var] = i;
      reverse_mapping[i] = bool_var;
    }
  }

  // Inputs are in [0, num_inputs). The targets are in [0, num_vars).
  int num_inputs = 0;
  int num_vars = 0;

  // There should be a "defining" gate for each non-input variable, and these
  // should appear in order. This should also be a topological order.
  std::vector<BinaryGate> gates;

  // Outputs of the "circuit".
  std::vector<int> outputs;

  // Mapping from global BooleanVariable to the dense indexing used above.
  // This is not always used.
  util_intops::StrongVector<BooleanVariable, int> mapping;
  std::vector<BooleanVariable> reverse_mapping;
};

// Removes the constraints from the problem by only keeping the first gate
// that define a new target. Constraints are always of the form of multiple
// definitions for the same variable.
void RemoveConstraints(BinaryCircuit* circuit);

// Removes intermediate variables that are not useful. For instance this can
// "detect" XOR gates encoded with AND gate and just use the XOR variant.
void ReduceGates(BinaryCircuit* circuit);

// Simplifies the circuit using the given equivalences.
// Note that this arbitrarily choose one way to express two equivalent literals.
//
// This also detect new equivalence if gate.a == gate.b.
void RemoveEquivalences(absl::Span<const std::pair<Literal, Literal>> equiv,
                        BinaryCircuit* circuit,
                        absl::Span<const Literal> extra_fixing = {});

// Constructs a problem to prove the equivalence of both circuits,
// the num_inputs and the outputs size must be equivalent.
// This will create a new circuit where the output are 1 iff the output of
// the two different circuit at that position are different.
//
// To prove equivalence, one would need to also enforce that at least one of
// the new output is one, and show infeasibility.
BinaryCircuit ConstructMitter(const BinaryCircuit& circuit_a,
                              const BinaryCircuit& circuit_b);

// Output a "dot" file representation of the given circuit. This tries to
// simplify the final graph by removing all intermediate node that are used only
// in one place.
//
// Note that for large file, it is better to split the positioning from the line
// rendering with a command like:
// dot -Tdot -Gsplines=none -Grankdir=LR  /tmp/circuit.dot -o /tmp/cmap.dot
//   && neato -Tsvg -n -Goutputorder=edgesfirst -Gsplines=line /tmp/cmap.dot
//   -o /tmp/circuit.svg && google-chrome /tmp/circuit.svg
std::string ToDotFile(const BinaryCircuit& circuit,
                      absl::Span<const int> special_nodes = {});

// Writes a BinaryCircuit in the "bench" format, using only LUT of size 2. This
// can be consumed by tools like ABC.
std::string ToBenchFile(const BinaryCircuit& circuit);

// Sample random inputs for literal equivalences.
CompactVectorVector<int, Literal> SampleForEquivalences(
    const BinaryCircuit& circuit, absl::BitGenRef random,
    const std::vector<std::vector<BooleanVariable>>& saved_solutions);

// Find equivalences using sampling, and then proove using either exhaustive
// enumeration or sat solving via the solve() function.
void SimplifyCircuit(
    int max_num_solve, absl::BitGenRef random,
    std::function<CpSolverResponse(const CpModelProto& cp_model)> solve,
    std::vector<std::vector<BooleanVariable>>* saved_solutions,
    BinaryCircuit* circuit);

// Add the constraint that we must have a != b. This is used to show that a
// and b are equivalent by proving the underlying problem is UNSAT.
void AddNotEquivalentConstraint(Literal a, Literal b, BinaryCircuit* circuit);

// Utility class to extract the part of a BinaryCircuit that is enough
// to define a given subset of literals. This is also called the fan-in of
// a set of variables.
class SubcircuitExtractor {
 public:
  // The constructor does some precomputations, so that all the subsequent
  // Extract() call are a bit faster.
  explicit SubcircuitExtractor(const BinaryCircuit& circuit);

  // Returns the subproblem sufficient to define all the given literals.
  // These literals will be the outputs of the new circuit.
  BinaryCircuit Extract(absl::Span<const Literal> literals);

  // Same as above but use local indices instead of literals.
  BinaryCircuit Extract(absl::Span<const int> new_outputs);

 private:
  const BinaryCircuit& mitter_;

  CompactVectorVector<int, int> dependency_;
  std::vector<bool> seen_;
  std::vector<int> queue_;
};

// Check feasibility by enumeration.
// This is similar to SampleForEquivalences() but we are exact.
// Must be called on a problem with less than 20 inputs (checked), it can take
// too long otherwise.
bool BinaryCircuitIsFeasible(const BinaryCircuit& circuit);

// Constructs a CpModelProto encoding of the given circuit.
// This can be used to prove that a circuit is "infeasible" for instance.
//
// If enforce_one_output is true, we will add a "at least one output"
// constraint. See ConstructMitter() to see the usage of this.
CpModelProto ConstructCpModelFromBinaryCircuit(const BinaryCircuit& circuit,
                                               bool enforce_one_output = false);

// Detects n-ary and-gate encoded by binary and-gate.
// This can lead to a more compact SAT model, but experiments did not show a
// big gain, even a small degradation compared to using only binary gates.
// Left around for experimentation.
CpModelProto CpModelUsingLargeAnds(const BinaryCircuit& circuit,
                                   bool enforce_one_output = false);

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_GATE_UTILS_H_
