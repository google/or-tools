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

//
// This file implements the table constraints.

#include <algorithm>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/sat_constraint.h"
#include "ortools/util/bitset.h"
#include "ortools/util/string_array.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {
// External table code.
Constraint* BuildAc4TableConstraint(Solver* const solver,
                                    const IntTupleSet& tuples,
                                    const std::vector<IntVar*>& vars);

Constraint* BuildSatTableConstraint(Solver* solver,
                                    const std::vector<IntVar*>& vars,
                                    const IntTupleSet& tuples);

Constraint* BuildAc4MddResetTableConstraint(Solver* const solver,
                                            const IntTupleSet& tuples,
                                            const std::vector<IntVar*>& vars);

namespace {
// ----- Presolve helpers -----
// TODO(user): Move this out of this file.
struct AffineTransformation {  // y == a*x + b.
  AffineTransformation() : a(1), b(0) {}
  AffineTransformation(int64 aa, int64 bb) : a(aa), b(bb) { CHECK_NE(a, 0); }
  int64 a;
  int64 b;

  bool Reverse(int64 value, int64* const reverse) const {
    const int64 temp = value - b;
    if (temp % a == 0) {
      *reverse = temp / a;
      DCHECK_EQ(Forward(*reverse), value);
      return true;
    } else {
      return false;
    }
  }

  int64 Forward(int64 value) const { return value * a + b; }

  int64 UnsafeReverse(int64 value) const { return (value - b) / a; }

  void Clear() {
    a = 1;
    b = 0;
  }

  std::string DebugString() const {
    return StringPrintf("(%" GG_LL_FORMAT "d * x + %" GG_LL_FORMAT "d)", a, b);
  }
};

// TODO(user): Move this out too.
class VarLinearizer : public ModelParser {
 public:
  VarLinearizer() : target_var_(nullptr), transformation_(nullptr) {}
  ~VarLinearizer() override {}

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    if (operation == ModelVisitor::kSumOperation) {
      AddConstant(value);
      delegate->Accept(this);
    } else if (operation == ModelVisitor::kDifferenceOperation) {
      AddConstant(value);
      PushMultiplier(-1);
      delegate->Accept(this);
      PopMultiplier();
    } else if (operation == ModelVisitor::kProductOperation) {
      PushMultiplier(value);
      delegate->Accept(this);
      PopMultiplier();
    } else if (operation == ModelVisitor::kTraceOperation) {
      *target_var_ = const_cast<IntVar*>(variable);
      transformation_->a = multipliers_.back();
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    *target_var_ = const_cast<IntVar*>(variable);
    transformation_->a = multipliers_.back();
  }

  void Visit(const IntVar* const var, IntVar** const target_var,
             AffineTransformation* const transformation) {
    target_var_ = target_var;
    transformation_ = transformation;
    transformation->Clear();
    PushMultiplier(1);
    var->Accept(this);
    PopMultiplier();
    CHECK(multipliers_.empty());
  }

  std::string DebugString() const override { return "VarLinearizer"; }

 private:
  void AddConstant(int64 constant) {
    transformation_->b += constant * multipliers_.back();
  }

  void PushMultiplier(int64 multiplier) {
    if (multipliers_.empty()) {
      multipliers_.push_back(multiplier);
    } else {
      multipliers_.push_back(multiplier * multipliers_.back());
    }
  }

  void PopMultiplier() { multipliers_.pop_back(); }

  std::vector<int64> multipliers_;
  IntVar** target_var_;
  AffineTransformation* transformation_;
};

static const int kBitsInUint64 = 64;

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
  BasePositiveTableConstraint(Solver* const s, const std::vector<IntVar*>& vars,
                              const IntTupleSet& tuples)
      : Constraint(s),
        tuple_count_(tuples.NumTuples()),
        arity_(vars.size()),
        vars_(arity_),
        holes_(arity_),
        iterators_(arity_),
        tuples_(tuples),
        transformations_(arity_) {
    // This constraint is intensive on domain and holes iterations on
    // variables.  Thus we can visit all variables to get to the
    // boolean or domain int var beneath it. Then we can reverse
    // process the tupleset to move in parallel to the simplifications
    // of the variables. This way, we can keep the memory efficient
    // nature of shared tuplesets (especially important for
    // transitions constraints which are a chain of table
    // constraints).  The cost in running time is small as the tuples
    // are read only once to construct the bitset data structures.
    VarLinearizer linearizer;
    for (int i = 0; i < arity_; ++i) {
      linearizer.Visit(vars[i], &vars_[i], &transformations_[i]);
    }
    // Create hole iterators
    for (int i = 0; i < arity_; ++i) {
      holes_[i] = vars_[i]->MakeHoleIterator(true);
      iterators_[i] = vars_[i]->MakeDomainIterator(true);
    }
  }

  ~BasePositiveTableConstraint() override {}

  std::string DebugString() const override {
    return StringPrintf("AllowedAssignments(arity = %d, tuple_count = %d)",
                        arity_, tuple_count_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kAllowedAssignments, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerMatrixArgument(ModelVisitor::kTuplesArgument, tuples_);
    visitor->EndVisitConstraint(ModelVisitor::kAllowedAssignments, this);
  }

 protected:
  bool TupleValue(int tuple_index, int var_index, int64* const value) const {
    return transformations_[var_index].Reverse(
        tuples_.Value(tuple_index, var_index), value);
  }

  int64 UnsafeTupleValue(int tuple_index, int var_index) const {
    return transformations_[var_index].UnsafeReverse(
        tuples_.Value(tuple_index, var_index));
  }

  bool IsTupleSupported(int tuple_index) {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      int64 value = 0;
      if (!TupleValue(tuple_index, var_index, &value) ||
          !vars_[var_index]->Contains(value)) {
        return false;
      }
    }
    return true;
  }

  const int tuple_count_;
  const int arity_;
  std::vector<IntVar*> vars_;
  std::vector<IntVarIterator*> holes_;
  std::vector<IntVarIterator*> iterators_;
  std::vector<int64> to_remove_;

 private:
  // All allowed tuples.
  const IntTupleSet tuples_;
  // The set of affine transformations that describe the
  // simplification of the variables.
  std::vector<AffineTransformation> transformations_;
};

class PositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  typedef std::unordered_map<int, std::vector<uint64>> ValueBitset;

  PositiveTableConstraint(Solver* const s, const std::vector<IntVar*>& vars,
                          const IntTupleSet& tuples)
      : BasePositiveTableConstraint(s, vars, tuples),
        word_length_(BitLength64(tuples.NumTuples())),
        active_tuples_(tuples.NumTuples()) {}

  ~PositiveTableConstraint() override {}

  void Post() override {
    Demon* d = MakeDelayedConstraintDemon0(
        solver(), this, &PositiveTableConstraint::Propagate, "Propagate");
    for (int i = 0; i < arity_; ++i) {
      vars_[i]->WhenDomain(d);
      Demon* u = MakeConstraintDemon1(
          solver(), this, &PositiveTableConstraint::Update, "Update", i);
      vars_[i]->WhenDomain(u);
    }
    // Initialize masks.
    masks_.clear();
    masks_.resize(arity_);
    for (int i = 0; i < tuple_count_; ++i) {
      InitializeMask(i);
    }
    // Initialize the active tuple bitset.
    std::vector<uint64> actives(word_length_, 0);
    for (int tuple_index = 0; tuple_index < tuple_count_; ++tuple_index) {
      if (IsTupleSupported(tuple_index)) {
        SetBit64(actives.data(), tuple_index);
      }
    }
    active_tuples_.Init(solver(), actives);
  }

  void InitialPropagate() override {
    // Build active_ structure.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      for (const auto& it : masks_[var_index]) {
        if (!vars_[var_index]->Contains(it.first)) {
          active_tuples_.RevSubtract(solver(), it.second);
        }
      }
    }
    if (active_tuples_.Empty()) {
      solver()->Fail();
    }
    // Remove unreached values.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      const ValueBitset& mask = masks_[var_index];
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      for (const int64 value : InitAndGetValues(iterators_[var_index])) {
        if (!ContainsKey(mask, value)) {
          to_remove_.push_back(value);
        }
      }
      if (!to_remove_.empty()) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void Propagate() {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      for (const int64 value : InitAndGetValues(iterators_[var_index])) {
        if (!Supported(var_index, value)) {
          to_remove_.push_back(value);
        }
      }
      if (!to_remove_.empty()) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void Update(int index) {
    const ValueBitset& var_masks = masks_[index];
    IntVar* const var = vars_[index];
    const int64 old_max = var->OldMax();
    const int64 vmin = var->Min();
    const int64 vmax = var->Max();
    for (int64 value = var->OldMin(); value < vmin; ++value) {
      const auto& it = var_masks.find(value);
      if (it != var_masks.end()) {
        BlankActives(it->second);
      }
    }
    for (const int64 value : InitAndGetValues(holes_[index])) {
      const auto& it = var_masks.find(value);
      if (it != var_masks.end()) {
        BlankActives(it->second);
      }
    }
    for (int64 value = vmax + 1; value <= old_max; ++value) {
      const auto& it = var_masks.find(value);
      if (it != var_masks.end()) {
        BlankActives(it->second);
      }
    }
  }

  void BlankActives(const std::vector<uint64>& mask) {
    if (!mask.empty()) {
      active_tuples_.RevSubtract(solver(), mask);
      if (active_tuples_.Empty()) {
        solver()->Fail();
      }
    }
  }

  bool Supported(int var_index, int64 value) {
    DCHECK_GE(var_index, 0);
    DCHECK_LT(var_index, arity_);
    DCHECK(ContainsKey(masks_[var_index], value));
    const std::vector<uint64>& mask = masks_[var_index][value];
    int tmp = 0;
    return active_tuples_.Intersects(mask, &tmp);
  }

  std::string DebugString() const override {
    return StringPrintf("PositiveTableConstraint([%s], %d tuples)",
                        JoinDebugStringPtr(vars_, ", ").c_str(), tuple_count_);
  }

 protected:
  void InitializeMask(int tuple_index) {
    std::vector<int64> cache(arity_);
    for (int var_index = 0; var_index < arity_; ++var_index) {
      if (!TupleValue(tuple_index, var_index, &cache[var_index])) {
        return;
      }
    }
    for (int var_index = 0; var_index < arity_; ++var_index) {
      const int64 value = cache[var_index];
      std::vector<uint64>& mask = masks_[var_index][value];
      if (mask.empty()) {
        mask.assign(word_length_, 0);
      }
      SetBit64(mask.data(), tuple_index);
    }
  }

  const int word_length_;
  UnsortedNullableRevBitset active_tuples_;
  std::vector<ValueBitset> masks_;
  std::vector<uint64> temp_mask_;
};

// ----- Compact Tables -----

class CompactPositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  CompactPositiveTableConstraint(Solver* const s,
                                 const std::vector<IntVar*>& vars,
                                 const IntTupleSet& tuples)
      : BasePositiveTableConstraint(s, vars, tuples),
        word_length_(BitLength64(tuples.NumTuples())),
        active_tuples_(tuples.NumTuples()),
        masks_(arity_),
        mask_starts_(arity_),
        mask_ends_(arity_),
        original_min_(arity_, 0),
        temp_mask_(word_length_, 0),
        supports_(arity_),
        demon_(nullptr),
        touched_var_(-1),
        var_sizes_(arity_, 0) {}

  ~CompactPositiveTableConstraint() override {}

  void Post() override {
    demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
        solver(), this, &CompactPositiveTableConstraint::Propagate,
        "Propagate"));
    for (int i = 0; i < arity_; ++i) {
      Demon* const u = MakeConstraintDemon1(
          solver(), this, &CompactPositiveTableConstraint::Update, "Update", i);
      vars_[i]->WhenDomain(u);
    }
    for (int i = 0; i < arity_; ++i) {
      var_sizes_.SetValue(solver(), i, vars_[i]->Size());
    }
  }

  void InitialPropagate() override {
    BuildMasks();
    FillMasksAndActiveTuples();
    ComputeMasksBoundaries();
    BuildSupports();
    RemoveUnsupportedValues();
  }

  // ----- Propagation -----

  void Propagate() {
    // Reset touch_var_ if in mode (more than 1 variable was modified).
    if (touched_var_ == -2) {
      touched_var_ = -1;
    }
    // This methods scans all values of all variables to see if they
    // are still supported.
    // This method is not attached to any particular variable, but is pushed
    // at a delayed priority after Update(var_index) is called.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      // This demons runs in low priority. Thus we know all the
      // variables that have changed since the last time it was run.
      // In that case, if only one var was touched, as propagation is
      // exact, we do not need to recheck that variable.
      if (var_index == touched_var_) {
        touched_var_ = -1;  // Clean now, it is a 1 time flag.
        continue;
      }
      IntVar* const var = vars_[var_index];
      const int64 original_min = original_min_[var_index];
      const int64 var_size = var->Size();
      // The domain iterator is very slow, let's try to see if we can
      // work our way around.
      switch (var_size) {
        case 1: {
          if (!Supported(var_index, var->Min() - original_min)) {
            solver()->Fail();
          }
          break;
        }
        case 2: {
          const int64 var_min = var->Min();
          const int64 var_max = var->Max();
          const bool min_support = Supported(var_index, var_min - original_min);
          const bool max_support = Supported(var_index, var_max - original_min);
          if (!min_support) {
            if (!max_support) {
              solver()->Fail();
            } else {
              var->SetValue(var_max);
              var_sizes_.SetValue(solver(), var_index, 1);
            }
          } else if (!max_support) {
            var->SetValue(var_min);
            var_sizes_.SetValue(solver(), var_index, 1);
          }
          break;
        }
        default: {
          to_remove_.clear();
          const int64 var_min = var->Min();
          const int64 var_max = var->Max();
          int64 new_min = var_min;
          int64 new_max = var_max;
          // If the domain of a variable is an interval, it is much
          // faster to iterate on that interval instead of using the
          // iterator.
          if (var_max - var_min + 1 == var_size) {
            for (; new_min <= var_max; ++new_min) {
              if (Supported(var_index, new_min - original_min)) {
                break;
              }
            }
            for (; new_max >= new_min; --new_max) {
              if (Supported(var_index, new_max - original_min)) {
                break;
              }
            }
            var->SetRange(new_min, new_max);
            for (int64 value = new_min + 1; value < new_max; ++value) {
              if (!Supported(var_index, value - original_min)) {
                to_remove_.push_back(value);
              }
            }
          } else {  // Domain is sparse.
                    // Let's not collect all values below the first supported
                    // value as this can easily and more rapidly be taken care
                    // of by a SetRange() call.
            new_min = kint64max;  // escape value.
            for (const int64 value : InitAndGetValues(iterators_[var_index])) {
              if (!Supported(var_index, value - original_min)) {
                to_remove_.push_back(value);
              } else {
                if (new_min == kint64max) {
                  new_min = value;
                  // This will be covered by the SetRange.
                  to_remove_.clear();
                }
                new_max = value;
              }
            }
            var->SetRange(new_min, new_max);
            // Trim the to_remove vector.
            int index = to_remove_.size() - 1;
            while (index >= 0 && to_remove_[index] > new_max) {
              index--;
            }
            to_remove_.resize(index + 1);
          }
          var->RemoveValues(to_remove_);
          var_sizes_.SetValue(solver(), var_index, var->Size());
        }
      }
    }
  }

  void Update(int var_index) {
    if (vars_[var_index]->Size() == var_sizes_.Value(var_index)) {
      return;
    }
    // This method will update the set of active tuples by masking out all
    // tuples attached to values of the variables that have been removed.

    // We first collect the complete set of tuples to blank out in temp_mask_.
    IntVar* const var = vars_[var_index];
    bool changed = false;
    const int64 omin = original_min_[var_index];
    const int64 var_size = var->Size();
    const int64 var_min = var->Min();
    const int64 var_max = var->Max();

    switch (var_size) {
      case 1: {
        changed = AndMaskWithActive(masks_[var_index][var_min - omin]);
        break;
      }
      case 2: {
        SetTempMask(var_index, var_min - omin);
        OrTempMask(var_index, var_max - omin);
        changed = AndMaskWithActive(temp_mask_);
        break;
      }
      default: {
        const int64 estimated_hole_size =
            var_sizes_.Value(var_index) - var_size;
        const int64 old_min = var->OldMin();
        const int64 old_max = var->OldMax();
        // Rough estimation of the number of operation if we scan
        // deltas in the domain of the variable.
        const int64 number_of_operations =
            estimated_hole_size + var_min - old_min + old_max - var_max;
        if (number_of_operations < var_size) {
          // Let's scan the removed values since last run.
          for (int64 value = old_min; value < var_min; ++value) {
            changed |= SubtractMaskFromActive(masks_[var_index][value - omin]);
          }
          for (const int64 value : InitAndGetValues(holes_[var_index])) {
            changed |= SubtractMaskFromActive(masks_[var_index][value - omin]);
          }
          for (int64 value = var_max + 1; value <= old_max; ++value) {
            changed |= SubtractMaskFromActive(masks_[var_index][value - omin]);
          }
        } else {
          ClearTempMask();
          // Let's build the mask of supported tuples from the current
          // domain.
          if (var_max - var_min + 1 == var_size) {  // Contiguous.
            for (int64 value = var_min; value <= var_max; ++value) {
              OrTempMask(var_index, value - omin);
            }
          } else {
            for (const int64 value : InitAndGetValues(iterators_[var_index])) {
              OrTempMask(var_index, value - omin);
            }
          }
          // Then we and this mask with active_tuples_.
          changed = AndMaskWithActive(temp_mask_);
        }
        // We maintain the size of the variables incrementally (when it
        // is > 2).
        var_sizes_.SetValue(solver(), var_index, var_size);
      }
    }
    // We push the propagate method only if something has changed.
    if (changed) {
      if (touched_var_ == -1 || touched_var_ == var_index) {
        touched_var_ = var_index;
      } else {
        touched_var_ = -2;  // more than one var.
      }
      EnqueueDelayedDemon(demon_);
    }
  }

  std::string DebugString() const override {
    return StringPrintf("CompactPositiveTableConstraint([%s], %d tuples)",
                        JoinDebugStringPtr(vars_, ", ").c_str(), tuple_count_);
  }

 private:
  // ----- Initialization -----

  void BuildMasks() {
    // Build masks.
    for (int i = 0; i < arity_; ++i) {
      original_min_[i] = vars_[i]->Min();
      const int64 span = vars_[i]->Max() - original_min_[i] + 1;
      masks_[i].resize(span);
    }
  }

  void FillMasksAndActiveTuples() {
    std::vector<uint64> actives(word_length_, 0);
    for (int tuple_index = 0; tuple_index < tuple_count_; ++tuple_index) {
      if (IsTupleSupported(tuple_index)) {
        SetBit64(actives.data(), tuple_index);
        // Fill in all masks.
        for (int var_index = 0; var_index < arity_; ++var_index) {
          const int64 value = UnsafeTupleValue(tuple_index, var_index);
          const int64 value_index = value - original_min_[var_index];
          DCHECK_GE(value_index, 0);
          DCHECK_LT(value_index, masks_[var_index].size());
          if (masks_[var_index][value_index].empty()) {
            masks_[var_index][value_index].assign(word_length_, 0);
          }
          SetBit64(masks_[var_index][value_index].data(), tuple_index);
        }
      }
    }
    active_tuples_.Init(solver(), actives);
  }

  void RemoveUnsupportedValues() {
    // remove unreached values.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      for (const int64 value : InitAndGetValues(iterators_[var_index])) {
        if (masks_[var_index][value - original_min_[var_index]].empty()) {
          to_remove_.push_back(value);
        }
      }
      if (!to_remove_.empty()) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void ComputeMasksBoundaries() {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      mask_starts_[var_index].resize(masks_[var_index].size());
      mask_ends_[var_index].resize(masks_[var_index].size());
      for (int value_index = 0; value_index < masks_[var_index].size();
           ++value_index) {
        const std::vector<uint64>& mask = masks_[var_index][value_index];
        if (mask.empty()) {
          continue;
        }
        int start = 0;
        while (start < word_length_ && mask[start] == 0) {
          start++;
        }
        DCHECK_LT(start, word_length_);
        int end = word_length_ - 1;
        while (end > start && mask[end] == 0) {
          end--;
        }
        DCHECK_LE(start, end);
        DCHECK_NE(mask[start], 0);
        DCHECK_NE(mask[end], 0);
        mask_starts_[var_index][value_index] = start;
        mask_ends_[var_index][value_index] = end;
      }
    }
  }

  void BuildSupports() {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      supports_[var_index].resize(masks_[var_index].size());
    }
  }

  // ----- Helpers during propagation -----

  bool AndMaskWithActive(const std::vector<uint64>& mask) {
    const bool result = active_tuples_.RevAnd(solver(), mask);
    if (active_tuples_.Empty()) {
      solver()->Fail();
    }
    return result;
  }

  bool SubtractMaskFromActive(const std::vector<uint64>& mask) {
    const bool result = active_tuples_.RevSubtract(solver(), mask);
    if (active_tuples_.Empty()) {
      solver()->Fail();
    }
    return result;
  }

  bool Supported(int var_index, int64 value_index) {
    DCHECK_GE(var_index, 0);
    DCHECK_LT(var_index, arity_);
    DCHECK_GE(value_index, 0);
    DCHECK_LT(value_index, masks_[var_index].size());
    const std::vector<uint64>& mask = masks_[var_index][value_index];
    DCHECK(!mask.empty());
    return active_tuples_.Intersects(mask, &supports_[var_index][value_index]);
  }

  void OrTempMask(int var_index, int64 value_index) {
    const std::vector<uint64>& mask = masks_[var_index][value_index];
    if (!mask.empty()) {
      const int mask_span = mask_ends_[var_index][value_index] -
                            mask_starts_[var_index][value_index] + 1;
      if (active_tuples_.ActiveWordSize() < mask_span) {
        for (int i : active_tuples_.active_words()) {
          temp_mask_[i] |= mask[i];
        }
      } else {
        for (int i = mask_starts_[var_index][value_index];
             i <= mask_ends_[var_index][value_index]; ++i) {
          temp_mask_[i] |= mask[i];
        }
      }
    }
  }

  void SetTempMask(int var_index, int64 value_index) {
    // We assume memset is much faster that looping and assigning.
    // Still we do want to stay sparse if possible.
    // Thus we switch between dense and sparse initialization by
    // comparing the number of operations in both case, with constant factor.
    // TODO(user): experiment with different constant values.
    if (active_tuples_.ActiveWordSize() < word_length_ / 4) {
      for (int i : active_tuples_.active_words()) {
        temp_mask_[i] = masks_[var_index][value_index][i];
      }
    } else {
      temp_mask_ = masks_[var_index][value_index];
    }
  }

  void ClearTempMask() {
    // See comment above.
    if (active_tuples_.ActiveWordSize() < word_length_ / 4) {
      for (int i : active_tuples_.active_words()) {
        temp_mask_[i] = 0;
      }
    } else {
      temp_mask_.assign(word_length_, 0);
    }
  }

  // The length in 64 bit words of the number of tuples.
  int64 word_length_;
  // The active bitset.
  UnsortedNullableRevBitset active_tuples_;
  // The masks per value per variable.
  std::vector<std::vector<std::vector<uint64>>> masks_;
  // The range of active indices in the masks.
  std::vector<std::vector<int>> mask_starts_;
  std::vector<std::vector<int>> mask_ends_;
  // The min on the vars at creation time.
  std::vector<int64> original_min_;
  // A temporary mask use for computation.
  std::vector<uint64> temp_mask_;
  // The index of the word in the active bitset supporting each value per
  // variable.
  std::vector<std::vector<int>> supports_;
  Demon* demon_;
  int touched_var_;
  RevArray<int64> var_sizes_;
};

// ----- Small Compact Table. -----

// TODO(user): regroup code with CompactPositiveTableConstraint.

class SmallCompactPositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  SmallCompactPositiveTableConstraint(Solver* const s,
                                      const std::vector<IntVar*>& vars,
                                      const IntTupleSet& tuples)
      : BasePositiveTableConstraint(s, vars, tuples),
        active_tuples_(0),
        stamp_(0),
        masks_(arity_),
        original_min_(arity_, 0),
        demon_(nullptr),
        touched_var_(-1) {
    CHECK_GE(tuple_count_, 0);
    CHECK_GE(arity_, 0);
    CHECK_LE(tuples.NumTuples(), kBitsInUint64);
  }

  ~SmallCompactPositiveTableConstraint() override {}

  void Post() override {
    demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
        solver(), this, &SmallCompactPositiveTableConstraint::Propagate,
        "Propagate"));
    for (int i = 0; i < arity_; ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const update_demon = MakeConstraintDemon1(
            solver(), this, &SmallCompactPositiveTableConstraint::Update,
            "Update", i);
        vars_[i]->WhenDomain(update_demon);
      }
    }
    stamp_ = 0;
  }

  void InitMasks() {
    // Build masks.
    for (int i = 0; i < arity_; ++i) {
      original_min_[i] = vars_[i]->Min();
      const int64 span = vars_[i]->Max() - original_min_[i] + 1;
      masks_[i].assign(span, 0);
    }
  }

  bool IsTupleSupported(int tuple_index) {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      int64 value = 0;
      if (!TupleValue(tuple_index, var_index, &value) ||
          !vars_[var_index]->Contains(value)) {
        return false;
      }
    }
    return true;
  }

  void ComputeActiveTuples() {
    active_tuples_ = 0;
    // Compute active_tuples_ and update masks.
    for (int tuple_index = 0; tuple_index < tuple_count_; ++tuple_index) {
      if (IsTupleSupported(tuple_index)) {
        const uint64 local_mask = OneBit64(tuple_index);
        active_tuples_ |= local_mask;
        for (int var_index = 0; var_index < arity_; ++var_index) {
          const int64 value = UnsafeTupleValue(tuple_index, var_index);
          masks_[var_index][value - original_min_[var_index]] |= local_mask;
        }
      }
    }
    if (!active_tuples_) {
      solver()->Fail();
    }
  }

  void RemoveUnsupportedValues() {
    // remove unreached values.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      IntVar* const var = vars_[var_index];
      const int64 original_min = original_min_[var_index];
      to_remove_.clear();
      for (const int64 value : InitAndGetValues(iterators_[var_index])) {
        if (masks_[var_index][value - original_min] == 0) {
          to_remove_.push_back(value);
        }
      }
      if (!to_remove_.empty()) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void InitialPropagate() override {
    InitMasks();
    ComputeActiveTuples();
    RemoveUnsupportedValues();
  }

  void Propagate() {
    // This methods scans all the values of all the variables to see if they
    // are still supported.
    // This method is not attached to any particular variable, but is pushed
    // at a delayed priority and awakened by Update(var_index).

    // Reset touch_var_ if in mode (more than 1 variable was modified).
    if (touched_var_ == -2) {
      touched_var_ = -1;
    }

    // We cache active_tuples_.
    const uint64 actives = active_tuples_;

    // We scan all variables and check their domains.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      // This demons runs in low priority. Thus we know all the
      // variables that have changed since the last time it was run.
      // In that case, if only one var was touched, as propagation is
      // exact, we do not need to recheck that variable.
      if (var_index == touched_var_) {
        touched_var_ = -1;  // Clean it, it is a one time flag.
        continue;
      }
      const std::vector<uint64>& var_mask = masks_[var_index];
      const int64 original_min = original_min_[var_index];
      IntVar* const var = vars_[var_index];
      const int64 var_size = var->Size();
      switch (var_size) {
        case 1: {
          if ((var_mask[var->Min() - original_min] & actives) == 0) {
            // The difference with the non-small version of the table
            // is that checking the validity of the resulting active
            // tuples is cheap. Therefore we do not delay the check
            // code.
            solver()->Fail();
          }
          break;
        }
        case 2: {
          const int64 var_min = var->Min();
          const int64 var_max = var->Max();
          const bool min_support =
              (var_mask[var_min - original_min] & actives) != 0;
          const bool max_support =
              (var_mask[var_max - original_min] & actives) != 0;
          if (!min_support && !max_support) {
            solver()->Fail();
          } else if (!min_support) {
            var->SetValue(var_max);
          } else if (!max_support) {
            var->SetValue(var_min);
          }
          break;
        }
        default: {
          to_remove_.clear();
          const int64 var_min = var->Min();
          const int64 var_max = var->Max();
          int64 new_min = var_min;
          int64 new_max = var_max;
          if (var_max - var_min + 1 == var_size) {
            // Contiguous case.
            for (; new_min <= var_max; ++new_min) {
              if ((var_mask[new_min - original_min] & actives) != 0) {
                break;
              }
            }
            for (; new_max >= new_min; --new_max) {
              if ((var_mask[new_max - original_min] & actives) != 0) {
                break;
              }
            }
            var->SetRange(new_min, new_max);
            for (int64 value = new_min + 1; value < new_max; ++value) {
              if ((var_mask[value - original_min] & actives) == 0) {
                to_remove_.push_back(value);
              }
            }
          } else {
            bool min_set = false;
            int last_size = 0;
            for (const int64 value : InitAndGetValues(iterators_[var_index])) {
              // The iterator is not safe w.r.t. deletion. Thus we
              // postpone all value removals.
              if ((var_mask[value - original_min] & actives) == 0) {
                if (min_set) {
                  to_remove_.push_back(value);
                }
              } else {
                if (!min_set) {
                  new_min = value;
                  min_set = true;
                }
                new_max = value;
                last_size = to_remove_.size();
              }
            }
            if (min_set) {
              var->SetRange(new_min, new_max);
            } else {
              solver()->Fail();
            }
            to_remove_.resize(last_size);
          }
          var->RemoveValues(to_remove_);
        }
      }
    }
  }

  void Update(int var_index) {
    // This method updates the set of active tuples by masking out all
    // tuples attached to values of the variables that have been removed.

    IntVar* const var = vars_[var_index];
    const int64 original_min = original_min_[var_index];
    const int64 var_size = var->Size();
    switch (var_size) {
      case 1: {
        ApplyMask(var_index, masks_[var_index][var->Min() - original_min]);
        return;
      }
      case 2: {
        ApplyMask(var_index, masks_[var_index][var->Min() - original_min] |
                                 masks_[var_index][var->Max() - original_min]);
        return;
      }
      default: {
        // We first collect the complete set of tuples to blank out in
        // temp_mask.
        const std::vector<uint64>& var_mask = masks_[var_index];
        const int64 old_min = var->OldMin();
        const int64 old_max = var->OldMax();
        const int64 var_min = var->Min();
        const int64 var_max = var->Max();
        const bool contiguous = var_size == var_max - var_min + 1;
        const bool nearly_contiguous =
            var_size > (var_max - var_min + 1) * 7 / 10;

        // Count the number of masks to collect to compare the deduction
        // vs the construction of the new active bitset.
        // TODO(user): Implement HolesSize() on IntVar* and use it
        // to remove this code and the var_sizes in the non_small
        // version.
        uint64 hole_mask = 0;
        if (!contiguous) {
          for (const int64 value : InitAndGetValues(holes_[var_index])) {
            hole_mask |= var_mask[value - original_min];
          }
        }
        const int64 hole_operations = var_min - old_min + old_max - var_max;
        // We estimate the domain iterator to be 4x slower.
        const int64 domain_operations = contiguous ? var_size : 4 * var_size;
        if (hole_operations < domain_operations) {
          for (int64 value = old_min; value < var_min; ++value) {
            hole_mask |= var_mask[value - original_min];
          }
          for (int64 value = var_max + 1; value <= old_max; ++value) {
            hole_mask |= var_mask[value - original_min];
          }
          // We reverse the mask as this was negative information.
          ApplyMask(var_index, ~hole_mask);
        } else {
          uint64 domain_mask = 0;
          if (contiguous) {
            for (int64 value = var_min; value <= var_max; ++value) {
              domain_mask |= var_mask[value - original_min];
            }
          } else if (nearly_contiguous) {
            for (int64 value = var_min; value <= var_max; ++value) {
              if (var->Contains(value)) {
                domain_mask |= var_mask[value - original_min];
              }
            }
          } else {
            for (const int64 value : InitAndGetValues(iterators_[var_index])) {
              domain_mask |= var_mask[value - original_min];
            }
          }
          ApplyMask(var_index, domain_mask);
        }
      }
    }
  }

  std::string DebugString() const override {
    return StringPrintf("SmallCompactPositiveTableConstraint([%s], %d tuples)",
                        JoinDebugStringPtr(vars_, ", ").c_str(), tuple_count_);
  }

 private:
  void ApplyMask(int var_index, uint64 mask) {
    if ((~mask & active_tuples_) != 0) {
      // Check if we need to save the active_tuples in this node.
      const uint64 current_stamp = solver()->stamp();
      if (stamp_ < current_stamp) {
        stamp_ = current_stamp;
        solver()->SaveValue(&active_tuples_);
      }
      active_tuples_ &= mask;
      if (active_tuples_) {
        // Maintain touched_var_.
        if (touched_var_ == -1 || touched_var_ == var_index) {
          touched_var_ = var_index;
        } else {
          touched_var_ = -2;  // more than one var.
        }
        EnqueueDelayedDemon(demon_);
      } else {
        // Clean it before failing.
        touched_var_ = -1;
        solver()->Fail();
      }
    }
  }

  // Bitset of active tuples.
  uint64 active_tuples_;
  // Stamp of the active_tuple bitset.
  uint64 stamp_;
  // The masks per value per variable.
  std::vector<std::vector<uint64>> masks_;
  // The min on the vars at creation time.
  std::vector<int64> original_min_;
  Demon* demon_;
  int touched_var_;
};

bool HasCompactDomains(const std::vector<IntVar*>& vars) {
  return true; // Always assume compact table.
}

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
  TransitionConstraint(Solver* const s, const std::vector<IntVar*>& vars,
                       const IntTupleSet& transition_table, int64 initial_state,
                       const std::vector<int64>& final_states)
      : Constraint(s),
        vars_(vars),
        transition_table_(transition_table),
        initial_state_(initial_state),
        final_states_(final_states) {}

  TransitionConstraint(Solver* const s, const std::vector<IntVar*>& vars,
                       const IntTupleSet& transition_table, int64 initial_state,
                       const std::vector<int>& final_states)
      : Constraint(s),
        vars_(vars),
        transition_table_(transition_table),
        initial_state_(initial_state),
        final_states_(final_states.size()) {
    for (int i = 0; i < final_states.size(); ++i) {
      final_states_[i] = final_states[i];
    }
  }

  ~TransitionConstraint() override {}

  void Post() override {
    Solver* const s = solver();
    int64 state_min = kint64max;
    int64 state_max = kint64min;
    const int nb_vars = vars_.size();
    for (int i = 0; i < transition_table_.NumTuples(); ++i) {
      state_max =
          std::max(state_max, transition_table_.Value(i, kStatePosition));
      state_max =
          std::max(state_max, transition_table_.Value(i, kNextStatePosition));
      state_min =
          std::min(state_min, transition_table_.Value(i, kStatePosition));
      state_min =
          std::min(state_min, transition_table_.Value(i, kNextStatePosition));
    }

    std::vector<IntVar*> states;
    states.push_back(s->MakeIntConst(initial_state_));
    for (int var_index = 1; var_index < nb_vars; ++var_index) {
      states.push_back(s->MakeIntVar(state_min, state_max));
    }
    states.push_back(s->MakeIntVar(final_states_));
    CHECK_EQ(nb_vars + 1, states.size());

    const int num_tuples = transition_table_.NumTuples();

    for (int var_index = 0; var_index < nb_vars; ++var_index) {
      std::vector<IntVar*> tmp_vars(3);
      tmp_vars[0] = states[var_index];
      tmp_vars[1] = vars_[var_index];
      tmp_vars[2] = states[var_index + 1];
      // We always build the compact versions of the tables.
      const ConstraintSolverParameters& params = solver()->parameters();
      if (num_tuples <= kBitsInUint64) {
        s->AddConstraint(s->RevAlloc(new SmallCompactPositiveTableConstraint(
            s, tmp_vars, transition_table_)));
      } else if (params.use_sat_table() &&
                 num_tuples > params.ac4r_table_threshold()) {
        s->AddConstraint(
            BuildSatTableConstraint(s, tmp_vars, transition_table_));
      } else if (params.use_mdd_table() &&
                 num_tuples > params.ac4r_table_threshold()) {
        s->AddConstraint(
            BuildAc4MddResetTableConstraint(s, transition_table_, tmp_vars));
      } else {
        s->AddConstraint(s->RevAlloc(new CompactPositiveTableConstraint(
            s, tmp_vars, transition_table_)));
      }
    }
  }

  void InitialPropagate() override {}

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kTransition, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kInitialState, initial_state_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kFinalStatesArgument,
                                       final_states_);
    visitor->VisitIntegerMatrixArgument(ModelVisitor::kTuplesArgument,
                                        transition_table_);
    visitor->EndVisitConstraint(ModelVisitor::kTransition, this);
  }

  std::string DebugString() const override {
    return StringPrintf(
        "TransitionConstraint([%s], %d transitions, initial = %" GG_LL_FORMAT
        "d, final = [%s])",
        JoinDebugStringPtr(vars_, ", ").c_str(), transition_table_.NumTuples(),
        initial_state_, strings::Join(final_states_, ", ").c_str());
  }

 private:
  // Variable representing transitions between states. See header file.
  const std::vector<IntVar*> vars_;
  // The transition as tuples (state, value, next_state).
  const IntTupleSet transition_table_;
  // The initial state before the first transition.
  const int64 initial_state_;
  // Vector of final state after the last transision.
  std::vector<int64> final_states_;
};

const int TransitionConstraint::kStatePosition = 0;
const int TransitionConstraint::kNextStatePosition = 2;
const int TransitionConstraint::kTransitionTupleSize = 3;
}  // namespace

// --------- API ----------

Constraint* Solver::MakeAllowedAssignments(const std::vector<IntVar*>& vars,
                                           const IntTupleSet& tuples) {
  if (parameters_.use_sat_table()) {
    return BuildSatTableConstraint(this, vars, tuples);
  }
  if (parameters_.use_compact_table() && HasCompactDomains(vars)) {
    if (tuples.NumTuples() < kBitsInUint64 && parameters_.use_small_table()) {
      return RevAlloc(
          new SmallCompactPositiveTableConstraint(this, vars, tuples));
    } else {
      return RevAlloc(new CompactPositiveTableConstraint(this, vars, tuples));
    }
  }
  if (tuples.NumTuples() > parameters_.ac4r_table_threshold()) {
    if (parameters_.use_mdd_table()) {
      return BuildAc4MddResetTableConstraint(this, tuples, vars);
    } else {
      return BuildAc4TableConstraint(this, tuples, vars);
    }
  } else {
    return RevAlloc(new PositiveTableConstraint(this, vars, tuples));
  }
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64 initial_state, const std::vector<int64>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64 initial_state, const std::vector<int>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

}  // namespace operations_research
