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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_TABLE_H_
#define ORTOOLS_CONSTRAINT_SOLVER_TABLE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/visitor.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

constexpr int kBitsInUint64 = 64;

bool HasCompactDomains(const std::vector<IntVar*>& vars);

// TODO(user): Move this out of this file.
struct AffineTransformation {  // y == a*x + b.
  AffineTransformation();
  AffineTransformation(int64_t aa, int64_t bb);

  int64_t a;
  int64_t b;

  bool Reverse(int64_t value, int64_t* reverse) const;
  int64_t Forward(int64_t value) const;
  int64_t UnsafeReverse(int64_t value) const;
  void Clear();
  std::string DebugString() const;
};

// TODO(user): Move this out too.
class VarLinearizer : public ModelParser {
 public:
  VarLinearizer();
  ~VarLinearizer() override;

  void VisitIntegerVariable(const IntVar* variable,
                            const std::string& operation, int64_t value,
                            IntVar* delegate) override;
  void VisitIntegerVariable(const IntVar* variable, IntExpr* delegate) override;
  void Visit(const IntVar* var, IntVar** target_var,
             AffineTransformation* transformation);
  std::string DebugString() const override;

 private:
  void AddConstant(int64_t constant);
  void PushMultiplier(int64_t multiplier);
  void PopMultiplier();

  std::vector<int64_t> multipliers_;
  IntVar** target_var_;
  AffineTransformation* transformation_;
};

// ----- Positive Table Constraint -----

// Structure of the constraint:

// Tuples are indexed, we maintain a bitset for active tuples.

// For each var and each value, we maintain a bitset mask of tuples
// containing this value for this variable.

// Propagation: When a value is removed, blank all active tuples using the
// var-value mask.
// Then we scan all other variable/values to see if there is an active
// tuple that supports it.

class BasePositiveTableConstraint : public Constraint {
 public:
  BasePositiveTableConstraint(Solver* s, const std::vector<IntVar*>& vars,
                              const IntTupleSet& tuples);
  ~BasePositiveTableConstraint() override;

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 protected:
  bool TupleValue(int tuple_index, int var_index, int64_t* value) const;
  int64_t UnsafeTupleValue(int tuple_index, int var_index) const;
  bool IsTupleSupported(int tuple_index);

  const int tuple_count_;
  const int arity_;
  std::vector<IntVar*> vars_;
  std::vector<IntVarIterator*> holes_;
  std::vector<IntVarIterator*> iterators_;
  std::vector<int64_t> to_remove_;

 private:
  // All allowed tuples.
  const IntTupleSet tuples_;
  // The set of affine transformations that describe the
  // simplification of the variables.
  std::vector<AffineTransformation> transformations_;
};

class PositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  typedef absl::flat_hash_map<int, std::vector<uint64_t>> ValueBitset;

  PositiveTableConstraint(Solver* s, const std::vector<IntVar*>& vars,
                          const IntTupleSet& tuples);
  ~PositiveTableConstraint() override;

  void Post() override;
  void InitialPropagate() override;
  void Propagate();
  void Update(int var_index);
  void BlankActives(const std::vector<uint64_t>& mask);
  bool Supported(int var_index, int64_t value);
  std::string DebugString() const override;

 protected:
  void InitializeMask(int tuple_index);

  const int word_length_;
  UnsortedNullableRevBitset active_tuples_;
  std::vector<ValueBitset> masks_;
  std::vector<uint64_t> temp_mask_;
};

// ----- Compact Tables -----

class CompactPositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  CompactPositiveTableConstraint(Solver* s, const std::vector<IntVar*>& vars,
                                 const IntTupleSet& tuples);
  ~CompactPositiveTableConstraint() override;

  void Post() override;
  void InitialPropagate() override;
  void Propagate();
  void Update(int var_index);
  std::string DebugString() const override;

 private:
  void BuildMasks();
  void FillMasksAndActiveTuples();
  void RemoveUnsupportedValues();
  void ComputeMasksBoundaries();
  void BuildSupports();

  // ----- Helpers during propagation -----

  bool AndMaskWithActive(const std::vector<uint64_t>& mask);
  bool SubtractMaskFromActive(const std::vector<uint64_t>& mask);
  bool Supported(int var_index, int64_t value_index);
  void OrTempMask(int var_index, int64_t value_index);
  void SetTempMask(int var_index, int64_t value_index);
  void ClearTempMask();

  // The length in 64 bit words of the number of tuples.
  int64_t word_length_;
  // The active bitset.
  UnsortedNullableRevBitset active_tuples_;
  // The masks per value per variable.
  std::vector<std::vector<std::vector<uint64_t>>> masks_;
  // The range of active indices in the masks.
  std::vector<std::vector<int>> mask_starts_;
  std::vector<std::vector<int>> mask_ends_;
  // The min on the vars at creation time.
  std::vector<int64_t> original_min_;
  // A temporary mask use for computation.
  std::vector<uint64_t> temp_mask_;
  // The index of the word in the active bitset supporting each value per
  // variable.
  std::vector<std::vector<int>> supports_;
  Demon* demon_;
  int touched_var_;
  RevArray<int64_t> var_sizes_;
};

// ----- Small Compact Table. -----

// TODO(user): regroup code with CompactPositiveTableConstraint.

class SmallCompactPositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  SmallCompactPositiveTableConstraint(Solver* s,
                                      const std::vector<IntVar*>& vars,
                                      const IntTupleSet& tuples);
  ~SmallCompactPositiveTableConstraint() override;

  void Post() override;
  void InitMasks();
  bool IsTupleSupported(int tuple_index);
  void ComputeActiveTuples();
  void RemoveUnsupportedValues();
  void InitialPropagate() override;
  void Propagate();
  void Update(int var_index);
  std::string DebugString() const override;

 private:
  void ApplyMask(int var_index, uint64_t mask);

  // Bitset of active tuples.
  uint64_t active_tuples_;
  // Stamp of the active_tuple bitset.
  uint64_t stamp_;
  // The masks per value per variable.
  std::vector<std::vector<uint64_t>> masks_;
  // The min on the vars at creation time.
  std::vector<int64_t> original_min_;
  Demon* demon_;
  int touched_var_;
};

// ---------- Deterministic Finite Automaton ----------

// This constraint implements a finite automaton when transitions are
// the values of the variables in the array.
// that is state[i+1] = transition[var[i]][state[i]] if
// (state[i], var[i],  state[i+1]) in the transition table.
// There is only one possible transition for a state/value pair.
class TransitionConstraint : public Constraint {
 public:
  static const int kStatePosition;
  static const int kNextStatePosition;
  static const int kTransitionTupleSize;

  TransitionConstraint(Solver* s, const std::vector<IntVar*>& vars,
                       const IntTupleSet& transition_table,
                       int64_t initial_state,
                       const std::vector<int64_t>& final_states);
  TransitionConstraint(Solver* s, const std::vector<IntVar*>& vars,
                       const IntTupleSet& transition_table,
                       int64_t initial_state,
                       absl::Span<const int> final_states);
  ~TransitionConstraint() override;

  void Post() override;
  void InitialPropagate() override;
  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;

 private:
  // Variable representing transitions between states. See header file.
  const std::vector<IntVar*> vars_;
  // The transition as tuples (state, value, next_state).
  const IntTupleSet transition_table_;
  // The initial state before the first transition.
  const int64_t initial_state_;
  // Vector of final state after the last transition.
  std::vector<int64_t> final_states_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_TABLE_H_
