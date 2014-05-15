// Copyright 2010-2013 Google
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
#include "base/hash.h"
#include "base/unique_ptr.h"
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "base/map_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/string_array.h"
#include "util/tuple_set.h"

DEFINE_bool(cp_use_compact_table, true,
            "Use compact table constraint when possible.");
DEFINE_bool(cp_use_small_table, true,
            "Use small compact table constraint when possible.");
DEFINE_int32(cp_ac4r_table_threshold, 2048,
             "Above this size, allowed assignment constraints will use the "
             "revised AC-4 implementation of the table constraint.");

namespace operations_research {
namespace {
// ----- Presolve helpers -----
// TODO(user): Move this out of this file.
struct AffineTransformation {  // y == a*x + b.
  AffineTransformation() : a(1), b(0) {}
  AffineTransformation(int64 aa, int64 bb) : a(aa), b(bb) { CHECK_NE(a, 0); }
  int64 a;
  int64 b;

  bool Identity() const { return a == 1 && b == 0; }

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
  virtual ~VarLinearizer() {}

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const std::string& operation, int64 value,
                                    IntVar* const delegate) {
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

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    IntExpr* const delegate) {
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

  virtual std::string DebugString() const { return "VarLinearizer"; }

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

  virtual ~BasePositiveTableConstraint() {}

  virtual std::string DebugString() const {
    return StringPrintf("AllowedAssignments(arity = %d, tuple_count = %d)",
                        arity_, tuple_count_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAllowedAssignments, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerMatrixArgument(ModelVisitor::kTuplesArgument, tuples_);
    visitor->EndVisitConstraint(ModelVisitor::kAllowedAssignments, this);
  }

 protected:
  bool TupleValue(int tuple_index, int var_index, int64* const value) const {
    return transformations_[var_index]
        .Reverse(tuples_.Value(tuple_index, var_index), value);
  }

  int64 UnsafeTupleValue(int tuple_index, int var_index) const {
    return transformations_[var_index]
        .UnsafeReverse(tuples_.Value(tuple_index, var_index));
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
  typedef hash_map<int, uint64*> ValueBitset;

  PositiveTableConstraint(Solver* const s, const std::vector<IntVar*>& vars,
                          const IntTupleSet& tuples)
      : BasePositiveTableConstraint(s, vars, tuples),
        length_(BitLength64(tuples.NumTuples())),
        active_tuples_(new uint64[length_]),
        stamps_(new uint64[length_]) {
    masks_.clear();
    masks_.resize(arity_);
    for (int i = 0; i < tuple_count_; ++i) {
      InitializeMask(i);
    }
  }

  virtual ~PositiveTableConstraint() {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      for (const auto& it : masks_[var_index]) {
        delete[] it.second;
      }
    }
  }

  virtual void Post() {
    Demon* d = MakeDelayedConstraintDemon0(
        solver(), this, &PositiveTableConstraint::Propagate, "Propagate");
    for (int i = 0; i < arity_; ++i) {
      vars_[i]->WhenDomain(d);
      Demon* u = MakeConstraintDemon1(
          solver(), this, &PositiveTableConstraint::Update, "Update", i);
      vars_[i]->WhenDomain(u);
    }
    for (int i = 0; i < length_; ++i) {
      stamps_[i] = 0;
      active_tuples_[i] = ~GG_ULONGLONG(0);
    }
  }

  virtual void InitialPropagate() {
    // Build active_ structure.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      for (const auto& it : masks_[var_index]) {
        if (!vars_[var_index]->Contains(it.first)) {
          for (int i = 0; i < length_; ++i) {
            active_tuples_[i] &= ~it.second[i];
          }
        }
      }
    }
    bool found_one = false;
    for (int i = 0; i < length_; ++i) {
      if (active_tuples_[i] != 0) {
        found_one = true;
        break;
      }
    }
    if (!found_one) {
      solver()->Fail();
    }
    // Remove unreached values.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      const ValueBitset& mask = masks_[var_index];
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      IntVarIterator* const it = iterators_[var_index];
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (!ContainsKey(mask, value)) {
          to_remove_.push_back(value);
        }
      }
      if (to_remove_.size() > 0) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void Propagate() {
    for (int var_index = 0; var_index < arity_; ++var_index) {
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      IntVarIterator* const it = iterators_[var_index];
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (!Supported(var_index, value)) {
          to_remove_.push_back(value);
        }
      }
      if (to_remove_.size() > 0) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  void Update(int index) {
    const ValueBitset& mask = masks_[index];
    IntVar* const var = vars_[index];
    const int64 old_max = var->OldMax();
    const int64 vmin = var->Min();
    const int64 vmax = var->Max();
    for (int64 value = var->OldMin(); value < vmin; ++value) {
      BlankActives(FindPtrOrNull(mask, value));
    }
    for (holes_[index]->Init(); holes_[index]->Ok(); holes_[index]->Next()) {
      BlankActives(FindPtrOrNull(mask, holes_[index]->Value()));
    }
    for (int64 value = vmax + 1; value <= old_max; ++value) {
      BlankActives(FindPtrOrNull(mask, value));
    }
  }

  void BlankActives(uint64* const mask) {
    if (mask != nullptr) {
      bool empty = true;
      for (int offset = 0; offset < length_; ++offset) {
        if ((mask[offset] & active_tuples_[offset]) != 0) {
          AndActiveTuples(offset, ~mask[offset]);
        }
        if (active_tuples_[offset] != 0) {
          empty = false;
        }
      }
      if (empty) {
        solver()->Fail();
      }
    }
  }

  bool Supported(int var_index, int64 value) {
    DCHECK_GE(var_index, 0);
    DCHECK_LT(var_index, arity_);
    DCHECK(ContainsKey(masks_[var_index], value));
    uint64* const mask = masks_[var_index][value];
    for (int offset = 0; offset < length_; ++offset) {
      if ((mask[offset] & active_tuples_[offset]) != 0LL) {
        return true;
      }
    }
    return false;
  }

  virtual std::string DebugString() const {
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
      uint64* mask = FindPtrOrNull(masks_[var_index], value);
      if (mask == nullptr) {
        mask = new uint64[length_];
        memset(mask, 0, length_ * sizeof(*mask));
        masks_[var_index][value] = mask;
      }
      SetBit64(mask, tuple_index);
    }
  }

  void AndActiveTuples(int offset, uint64 mask) {
    const uint64 current_stamp = solver()->stamp();
    if (stamps_[offset] < current_stamp) {
      stamps_[offset] = current_stamp;
      solver()->SaveValue(&active_tuples_[offset]);
    }
    active_tuples_[offset] &= mask;
  }

  const int length_;
  // TODO(user): create bitset64 class and use it.
  std::unique_ptr<uint64[]> active_tuples_;
  std::unique_ptr<uint64[]> stamps_;
  std::vector<ValueBitset> masks_;
};

// ----- Compact Table. -----

class CompactPositiveTableConstraint : public BasePositiveTableConstraint {
 public:
  CompactPositiveTableConstraint(Solver* const s,
                                 const std::vector<IntVar*>& vars,
                                 const IntTupleSet& tuples)
      : BasePositiveTableConstraint(s, vars, tuples),
        length_(BitLength64(tuples.NumTuples())),
        active_tuples_(new uint64[length_]),
        stamps_(new uint64[length_]),
        original_min_(new int64[arity_]),
        temp_mask_(new uint64[length_]),
        demon_(nullptr),
        touched_var_(-1),
        var_sizes_(arity_, 0) {}

  virtual ~CompactPositiveTableConstraint() {}

  virtual void Post() {
    demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
        solver(), this, &CompactPositiveTableConstraint::Propagate,
        "Propagate"));
    for (int i = 0; i < arity_; ++i) {
      Demon* const u = MakeConstraintDemon1(
          solver(), this, &CompactPositiveTableConstraint::Update, "Update", i);
      vars_[i]->WhenDomain(u);
    }
    for (int i = 0; i < length_; ++i) {
      stamps_[i] = 0;
      active_tuples_[i] = 0;
    }
    for (int i = 0; i < arity_; ++i) {
      var_sizes_.SetValue(solver(), i, vars_[i]->Size());
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

  void BuildStructures() {
    // Build active_ structure.
    bool found_one = false;
    for (int tuple_index = 0; tuple_index < tuple_count_; ++tuple_index) {
      if (IsTupleSupported(tuple_index)) {
        SetBit64(active_tuples_.get(), tuple_index);
        found_one = true;
      }
    }
    if (!found_one) {
      solver()->Fail();
    }
  }

  void BuildMasks() {
    // Build masks.
    masks_.clear();
    masks_.resize(arity_);
    for (int i = 0; i < arity_; ++i) {
      original_min_[i] = vars_[i]->Min();
      const int64 span = vars_[i]->Max() - original_min_[i] + 1;
      masks_[i].resize(span);
      for (int j = 0; j < span; ++j) {
        masks_[i][j] = nullptr;
      }
    }
  }

  void FillMasks() {
    for (int tuple_index = 0; tuple_index < tuple_count_; ++tuple_index) {
      if (IsBitSet64(active_tuples_.get(), tuple_index)) {
        for (int var_index = 0; var_index < arity_; ++var_index) {
          const int64 value = UnsafeTupleValue(tuple_index, var_index);
          const int64 value_index = value - original_min_[var_index];
          DCHECK_GE(value_index, 0);
          DCHECK_LT(value_index, masks_[var_index].size());
          uint64* mask = masks_[var_index][value_index];
          if (!mask) {
            mask = solver()->RevAllocArray(new uint64[length_]);
            memset(mask, 0, length_ * sizeof(*mask));
            masks_[var_index][value_index] = mask;
          }
          SetBit64(mask, tuple_index);
        }
      }
    }
  }

  void ComputeMasksBoundaries() {
    // Store boundaries of non zero parts of masks.
    starts_.clear();
    starts_.resize(arity_);
    ends_.clear();
    ends_.resize(arity_);
    supports_.clear();
    supports_.resize(arity_);
    for (int var_index = 0; var_index < arity_; ++var_index) {
      const int64 span = vars_[var_index]->Max() - original_min_[var_index] + 1;
      starts_[var_index].resize(span);
      ends_[var_index].resize(span);
      supports_[var_index].resize(span);
      for (int value_index = 0; value_index < span; ++value_index) {
        const uint64* const mask = masks_[var_index][value_index];
        if (mask != nullptr) {
          starts_[var_index][value_index] = 0;
          while (!mask[starts_[var_index][value_index]]) {
            starts_[var_index][value_index]++;
          }
          supports_[var_index][value_index] = starts_[var_index][value_index];
          ends_[var_index][value_index] = length_ - 1;
          while (!mask[ends_[var_index][value_index]]) {
            ends_[var_index][value_index]--;
          }
        }
      }
    }
  }

  void RemoveUnsupportedValues() {
    // remove unreached values.
    for (int var_index = 0; var_index < arity_; ++var_index) {
      IntVar* const var = vars_[var_index];
      to_remove_.clear();
      IntVarIterator* const it = iterators_[var_index];
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (!masks_[var_index][value - original_min_[var_index]]) {
          to_remove_.push_back(value);
        }
      }
      if (to_remove_.size() > 0) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  virtual void InitialPropagate() {
    BuildStructures();
    BuildMasks();
    FillMasks();
    ComputeMasksBoundaries();
    RemoveUnsupportedValues();
  }

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
            }
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
            IntVarIterator* const it = iterators_[var_index];
            for (it->Init(); it->Ok(); it->Next()) {
              const int64 value = it->Value();
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
        }
      }
    }
  }

  void Update(int var_index) {
    // This method will update the set of active tuples by masking out all
    // tuples attached to values of the variables that have been removed.

    // We first collect the complete set of tuples to blank out in temp_mask_.
    IntVar* const var = vars_[var_index];
    bool changed = false;
    const int64 omin = original_min_[var_index];
    const int64 var_size = var->Size();

    switch (var_size) {
      case 1: {
        SetTempMask(var_index, var->Min() - omin);
        changed = AndTempMaskWithActive();
        break;
      }
      case 2: {
        SetTempMask(var_index, var->Min() - omin);
        OrTempMask(var_index, var->Max() - omin);
        changed = AndTempMaskWithActive();
        break;
      }
      default: {
        ClearTempMask();
        const int64 count = var_sizes_.Value(var_index) - var_size;
        const int64 old_min = var->OldMin();
        const int64 old_max = var->OldMax();
        const int64 var_min = var->Min();
        const int64 var_max = var->Max();
        // Rough estimation of the number of operation if we scan
        // deltas in the domain of the variable.
        const int64 number_of_operations =
            count + var_min - old_min + old_max - var_max;
        if (number_of_operations < var_size) {
          // Let's scan the removed values since last run.
          for (int64 value = old_min; value < var_min; ++value) {
            OrTempMask(var_index, value - omin);
          }
          IntVarIterator* const hole = holes_[var_index];
          for (hole->Init(); hole->Ok(); hole->Next()) {
            OrTempMask(var_index, hole->Value() - omin);
          }
          for (int64 value = var_max + 1; value <= old_max; ++value) {
            OrTempMask(var_index, value - omin);
          }
          // Then we substract this mask from the active_tuples_.
          changed = SubstractTempMaskFromActive();
        } else {
          // Let's build the mask of supported tuples from the current
          // domain.
          if (var_max - var_min + 1 == var_size) {  // Contiguous.
            for (int64 value = var_min; value <= var_max; ++value) {
              OrTempMask(var_index, value - omin);
            }
          } else {
            IntVarIterator* const it = iterators_[var_index];
            for (it->Init(); it->Ok(); it->Next()) {
              OrTempMask(var_index, it->Value() - omin);
            }
          }
          // Then we and this mask with active_tuples_.
          changed = AndTempMaskWithActive();
        }
        // We maintain the size of the variables incrementally (when it
        // is > 2).
        var_sizes_.SetValue(solver(), var_index, var_size);
      }
    }
    // And check active_tuples_ is still not empty, we fail otherwise.
    if (changed) {
      for (int offset = 0; offset < length_; ++offset) {
        if (active_tuples_[offset]) {
          // We push the propagate method only if something has changed.
          if (touched_var_ == -1 || touched_var_ == var_index) {
            touched_var_ = var_index;
          } else {
            touched_var_ = -2;  // more than one var.
          }
          EnqueueDelayedDemon(demon_);
          return;
        }
      }
      solver()->Fail();
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("CompactPositiveTableConstraint([%s], %d tuples)",
                        JoinDebugStringPtr(vars_, ", ").c_str(), tuple_count_);
  }

 private:
  bool AndTempMaskWithActive() {
    bool changed = false;
    for (int offset = 0; offset < length_; ++offset) {
      if ((~temp_mask_[offset] & active_tuples_[offset]) != 0) {
        AndActiveTuples(offset, temp_mask_[offset]);
        changed = true;
      }
    }
    return changed;
  }

  bool SubstractTempMaskFromActive() {
    bool changed = false;
    for (int offset = 0; offset < length_; ++offset) {
      if ((temp_mask_[offset] & active_tuples_[offset]) != 0) {
        AndActiveTuples(offset, ~temp_mask_[offset]);
        changed = true;
      }
    }
    return changed;
  }

  bool Supported(int var_index, int64 value_index) {
    DCHECK_GE(var_index, 0);
    DCHECK_LT(var_index, arity_);
    DCHECK_GE(value_index, 0);
    DCHECK(masks_[var_index][value_index]);
    const uint64* const mask = masks_[var_index][value_index];
    const int support = supports_[var_index][value_index];
    if ((mask[support] & active_tuples_[support]) != 0) {
      return true;
    }
    const int loop_end = ends_[var_index][value_index];
    for (int offset = starts_[var_index][value_index]; offset <= loop_end;
         ++offset) {
      if ((mask[offset] & active_tuples_[offset]) != 0) {
        supports_[var_index][value_index] = offset;
        return true;
      }
    }
    return false;
  }

  void OrTempMask(int var_index, int64 value_index) {
    const uint64* const mask = masks_[var_index][value_index];
    if (mask) {
      const int start = starts_[var_index][value_index];
      const int end = ends_[var_index][value_index];
      for (int offset = start; offset <= end; ++offset) {
        temp_mask_[offset] |= mask[offset];
      }
    }
  }

  void SetTempMask(int var_index, int64 value_index) {
    memcpy(temp_mask_.get(), masks_[var_index][value_index],
           length_ * sizeof(*temp_mask_.get()));
  }

  void ClearTempMask() {
    memset(temp_mask_.get(), 0, length_ * sizeof(*temp_mask_.get()));
  }

  void AndActiveTuples(int offset, uint64 mask) {
    const uint64 current_stamp = solver()->stamp();
    if (stamps_[offset] < current_stamp) {
      stamps_[offset] = current_stamp;
      solver()->SaveValue(&active_tuples_[offset]);
    }
    active_tuples_[offset] &= mask;
  }

  // Length of bitsets in double words.
  const int length_;
  // Bitset of active tuples.
  std::unique_ptr<uint64[]> active_tuples_;
  // Array of stamps, one per 64 tuples.
  std::unique_ptr<uint64[]> stamps_;
  // The masks per value per variable.
  std::vector<std::vector<uint64*> > masks_;
  // The min on the vars at creation time.
  std::unique_ptr<int64[]> original_min_;
  // The starts of active bitsets.
  std::vector<std::vector<int> > starts_;
  // The ends of the active bitsets.x
  std::vector<std::vector<int> > ends_;
  // A temporary mask use for computation.
  std::unique_ptr<uint64[]> temp_mask_;
  // The portion of the active tuples supporting each value per variable.
  std::vector<std::vector<int> > supports_;
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
        masks_(new uint64* [arity_]),
        original_min_(new int64[arity_]),
        demon_(nullptr),
        touched_var_(-1) {
    CHECK_GE(tuple_count_, 0);
    CHECK_GE(arity_, 0);
    CHECK_LE(tuples.NumTuples(), kBitsInUint64);
    // Zero masks
    memset(masks_.get(), 0, arity_ * sizeof(*masks_.get()));
  }

  virtual ~SmallCompactPositiveTableConstraint() {
    for (int i = 0; i < arity_; ++i) {
      delete[] masks_[i];
      masks_[i] = nullptr;
    }
  }

  virtual void Post() {
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
      masks_[i] = new uint64[span];
      memset(masks_[i], 0, span * sizeof(*masks_[i]));
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
      IntVarIterator* const it = iterators_[var_index];
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (masks_[var_index][value - original_min] == 0) {
          to_remove_.push_back(value);
        }
      }
      if (to_remove_.size() > 0) {
        var->RemoveValues(to_remove_);
      }
    }
  }

  virtual void InitialPropagate() {
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
      const uint64* const var_mask = masks_[var_index];
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
            IntVarIterator* const it = iterators_[var_index];
            for (it->Init(); it->Ok(); it->Next()) {
              // The iterator is not safe w.r.t. deletion. Thus we
              // postpone all value removals.
              const int64 value = it->Value();
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
        const uint64* const var_mask = masks_[var_index];
        const int64 old_min = var->OldMin();
        const int64 old_max = var->OldMax();
        const int64 var_min = var->Min();
        const int64 var_max = var->Max();
        const bool contiguous = var_size == var_max - var_min + 1;

        // Count the number of masks to collect to compare the deduction
        // vs the construction of the new active bitset.
        // TODO(user): Implement HolesSize() on IntVar* and use it
        // to remove this code and the var_sizes in the non_small
        // version.
        uint64 hole_mask = 0;
        IntVarIterator* const hole = holes_[var_index];
        for (hole->Init(); hole->Ok(); hole->Next()) {
          hole_mask |= var_mask[hole->Value() - original_min];
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
          } else {
            IntVarIterator* const it = iterators_[var_index];
            for (it->Init(); it->Ok(); it->Next()) {
              domain_mask |= var_mask[it->Value() - original_min];
            }
          }
          ApplyMask(var_index, domain_mask);
        }
      }
    }
  }

  virtual std::string DebugString() const {
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
  std::unique_ptr<uint64 * []> masks_;
  // The min on the vars at creation time.
  std::unique_ptr<int64[]> original_min_;
  Demon* demon_;
  int touched_var_;
};

bool HasCompactDomains(const std::vector<IntVar*>& vars) {
  int64 sum_of_spans = 0LL;
  int64 sum_of_sizes = 0LL;
  for (IntVar* const var : vars) {
    IntExpr* inner = nullptr;
    int64 coef = 1;
    if (var->solver()->IsProduct(var, &inner, &coef) && inner->IsVar()) {
      IntVar* const nvar = inner->Var();
      sum_of_sizes += nvar->Size();
      sum_of_spans += nvar->Max() - nvar->Min() + 1;
    } else {
      sum_of_sizes += var->Size();
      sum_of_spans += var->Max() - var->Min() + 1;
    }
  }
  return sum_of_spans < 512 * sum_of_sizes;
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

  virtual ~TransitionConstraint() {}

  virtual void Post() {
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

    for (int var_index = 0; var_index < nb_vars; ++var_index) {
      std::vector<IntVar*> tmp_vars;
      tmp_vars.push_back(states[var_index]);
      tmp_vars.push_back(vars_[var_index]);
      tmp_vars.push_back(states[var_index + 1]);
      // We always build the compact versions of the tables.
      if (transition_table_.NumTuples() < kBitsInUint64) {
        s->AddConstraint(s->RevAlloc(new SmallCompactPositiveTableConstraint(
            s, tmp_vars, transition_table_)));
      } else {
        s->AddConstraint(s->RevAlloc(new CompactPositiveTableConstraint(
            s, tmp_vars, transition_table_)));
      }
    }
  }

  virtual void InitialPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
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

  virtual std::string DebugString() const {
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

Constraint* BuildAc4TableConstraint(Solver* const solver,
                                    const IntTupleSet& tuples,
                                    const std::vector<IntVar*>& vars);

Constraint* Solver::MakeAllowedAssignments(const std::vector<IntVar*>& vars,
                                           const IntTupleSet& tuples) {
  if (FLAGS_cp_use_compact_table && HasCompactDomains(vars)) {
    if (tuples.NumTuples() < kBitsInUint64 && FLAGS_cp_use_small_table) {
      return RevAlloc(
          new SmallCompactPositiveTableConstraint(this, vars, tuples));
    } else {
      return RevAlloc(new CompactPositiveTableConstraint(this, vars, tuples));
    }
  }
  if (tuples.NumTuples() > FLAGS_cp_ac4r_table_threshold) {
    return BuildAc4TableConstraint(this, tuples, vars);
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
