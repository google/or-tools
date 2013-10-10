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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/map_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/saturated_arithmetic.h"
#include "util/string_array.h"

DEFINE_bool(cp_disable_expression_optimization, false,
            "Disable special optimization when creating expressions.");
DEFINE_bool(cp_share_int_consts, true, "Share IntConst's with the same value.");

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ---------- IntExpr ----------

IntVar* IntExpr::VarWithName(const string& name) {
  IntVar* const var = Var();
  var->set_name(name);
  return var;
}

// ---------- IntVar ----------

IntVar::IntVar(Solver* const s) : IntExpr(s) {}

IntVar::IntVar(Solver* const s, const string& name) : IntExpr(s) {
  set_name(name);
}

namespace {
// ---------- Subclasses of IntVar ----------

// ----- Domain Int Var: base class for variables -----
// It Contains bounds and a bitset representation of possible values.
class DomainIntVar : public IntVar {
 public:
  // Utility classes
  class BitSetIterator : public BaseObject {
   public:
    BitSetIterator(uint64* const bitset, int64 omin)
        : bitset_(bitset), omin_(omin), max_(kint64min), current_(kint64max) {}

    virtual ~BitSetIterator() {}

    void Init(int64 min, int64 max) {
      max_ = max;
      current_ = min;
    }

    bool Ok() const { return current_ <= max_; }

    int64 Value() const { return current_; }

    void Next() {
      if (++current_ <= max_) {
        current_ = UnsafeLeastSignificantBitPosition64(
                       bitset_, current_ - omin_, max_ - omin_) +
                   omin_;
      }
    }

    virtual string DebugString() const { return "BitSetIterator"; }

   private:
    uint64* const bitset_;
    const int64 omin_;
    int64 max_;
    int64 current_;
  };

  class BitSet : public BaseObject {
   public:
    explicit BitSet(Solver* const s) : solver_(s), holes_stamp_(0) {}
    virtual ~BitSet() {}

    virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) = 0;
    virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) = 0;
    virtual bool Contains(int64 val) const = 0;
    virtual bool SetValue(int64 val) = 0;
    virtual bool RemoveValue(int64 val) = 0;
    virtual uint64 Size() const = 0;
    virtual void DelayRemoveValue(int64 val) = 0;
    virtual void ApplyRemovedValues(DomainIntVar* var) = 0;
    virtual void ClearRemovedValues() = 0;
    virtual string pretty_DebugString(int64 min, int64 max) const = 0;
    virtual BitSetIterator* MakeIterator() = 0;

    void InitHoles() {
      const uint64 current_stamp = solver_->stamp();
      if (holes_stamp_ < current_stamp) {
        holes_.clear();
        holes_stamp_ = current_stamp;
      }
    }

    virtual void ClearHoles() { holes_.clear(); }

    const std::vector<int64>& Holes() { return holes_; }

    void AddHole(int64 value) { holes_.push_back(value); }

   protected:
    Solver* const solver_;

   private:
    std::vector<int64> holes_;
    uint64 holes_stamp_;
  };

  class QueueHandler : public Demon {
   public:
    explicit QueueHandler(DomainIntVar* const var) : var_(var) {}
    virtual ~QueueHandler() {}
    virtual void Run(Solver* const s) {
      s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
      var_->Process();
      s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
    }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }

   private:
    DomainIntVar* const var_;
  };

  // This class monitors the domain of the variable and updates the
  // IsEqual/IsDifferent boolean variables accordingly.
  class ValueWatcher : public Constraint {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(ValueWatcher* const watcher, int64 index)
          : value_watcher_(watcher), index_(index) {}
      virtual ~WatchDemon() {}

      virtual void Run(Solver* const solver) {
        value_watcher_->ProcessValueWatcher(index_);
      }

     private:
      ValueWatcher* const value_watcher_;
      const int64 index_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(ValueWatcher* const watcher)
          : value_watcher_(watcher) {}
      virtual ~VarDemon() {}

      virtual void Run(Solver* const solver) { value_watcher_->ProcessVar(); }

     private:
      ValueWatcher* const value_watcher_;
    };

    ValueWatcher(Solver* const solver, DomainIntVar* const variable)
        : Constraint(solver),
          variable_(variable),
          iterator_(variable_->MakeHoleIterator(true)),
          watchers_(16),
          min_range_(kint64max),
          max_range_(kint64min),
          var_demon_(nullptr),
          active_watchers_(0) {}

    ValueWatcher(Solver* const solver, DomainIntVar* const variable,
                 const std::vector<int64>& values, const std::vector<IntVar*>& vars)
        : Constraint(solver),
          variable_(variable),
          iterator_(variable_->MakeHoleIterator(true)),
          watchers_(16),
          min_range_(kint64max),
          max_range_(kint64min),
          var_demon_(nullptr),
          active_watchers_(0) {
      CHECK_EQ(vars.size(), values.size());
      for (int i = 0; i < values.size(); ++i) {
        SetValueWatcher(vars[i], values[i]);
      }
    }

    virtual ~ValueWatcher() {}

    IntVar* GetOrMakeValueWatcher(int64 value) {
      IntVar* const watcher = watchers_.At(value);
      if (watcher != nullptr) {
        return watcher;
      }
      IntVar* boolvar = nullptr;
      if (variable_->Contains(value)) {
        if (variable_->Bound()) {
          boolvar = solver()->MakeIntConst(1);
        } else {
          const string vname = variable_->HasName() ? variable_->name()
                                                    : variable_->DebugString();
          const string bname = StringPrintf("Watch<%s == %" GG_LL_FORMAT "d>",
                                            vname.c_str(), value);
          boolvar = solver()->MakeBoolVar(bname);
        }
        active_watchers_.Incr(solver());
      } else {
        boolvar = variable_->solver()->MakeIntConst(0);
      }
      min_range_.SetValue(solver(), std::min(min_range_.Value(), value));
      max_range_.SetValue(solver(), std::max(max_range_.Value(), value));
      watchers_.RevInsert(solver(), value, boolvar);
      if (posted_.Switched() && !boolvar->Bound()) {
        boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
      }
      return boolvar;
    }

    void SetValueWatcher(IntVar* const boolvar, int64 value) {
      CHECK(watchers_.At(value) == nullptr);
      active_watchers_.Incr(solver());
      min_range_.SetValue(solver(), std::min(min_range_.Value(), value));
      max_range_.SetValue(solver(), std::max(max_range_.Value(), value));
      watchers_.RevInsert(solver(), value, boolvar);
      if (posted_.Switched() && !boolvar->Bound()) {
        boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
      }
    }

    virtual void Post() {
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenDomain(var_demon_);
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      for (int64 value = min_r; value <= max_r; ++value) {
        IntVar* const boolvar = watchers_.At(value);
        if (boolvar != nullptr && !boolvar->Bound()) {
          boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
        }
      }
      posted_.Switch(solver());
    }

    virtual void InitialPropagate() {
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      for (int64 value = min_r; value <= max_r; ++value) {
        IntVar* const boolvar = watchers_.At(value);
        if (!variable_->Contains(value)) {
          Zero(value);
        }
        if (boolvar != nullptr && boolvar->Bound()) {
          if (boolvar->Min() == 0) {
            variable_->RemoveValue(value);
          } else {
            variable_->SetValue(value);
          }
        }
      }
      if (variable_->Bound()) {
        One(variable_->Min());
      }
    }

    void ProcessValueWatcher(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      DCHECK(boolvar != nullptr);
      if (boolvar->Min() == 0) {
        if (variable_->Size() < 0xFFFFFF) {
          variable_->RemoveValue(index);
        } else {
          solver()->AddConstraint(solver()->MakeNonEquality(variable_, index));
        }
      } else {
        variable_->SetValue(index);
      }
    }

    void ProcessVar() {
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      if (max_r - min_r < 5) {
        for (int64 i = min_r; i <= max_r; ++i) {
          if (!variable_->Contains(i)) {
            Zero(i);
          }
        }
      } else {
        const int64 old_min_domain = variable_->OldMin();
        const int64 min_domain = variable_->Min();
        const int64 max_domain = variable_->Max();
        for (int64 val = std::max(min_r, old_min_domain);
             val <= std::min(min_domain - 1, max_r); ++val) {
          Zero(val);
        }
        // Second iteration: "delta" domain iteration.
        IntVarIterator* const it = iterator_;
        for (it->Init(); it->Ok(); it->Next()) {
          const int64 val = it->Value();
          if (val >= min_r && val <= max_r) {
            Zero(val);
          }
        }
        // Third iteration: from max to old_max.
        const int64 old_max_domain = variable_->OldMax();
        for (int64 val = std::max(min_r, max_domain + 1);
             val <= std::min(max_r, old_max_domain); ++val) {
          Zero(val);
        }
      }
      if (variable_->Bound()) {
        One(variable_->Min());
      }
    }

    virtual void Accept(ModelVisitor* const visitor) const {
      visitor->BeginVisitConstraint(ModelVisitor::kVarValueWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      for (int64 i = min_r; i <= max_r; ++i) {
        IntVar* const boolvar = watchers_.At(i);
        if (boolvar != nullptr) {
          all_coefficients.push_back(i);
          all_bool_vars.push_back(boolvar);
        }
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarValueWatcher, this);
    }

    virtual string DebugString() const {
      return StringPrintf("ValueWatcher(%s)", variable_->DebugString().c_str());
    }

   private:
    void Zero(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      if (boolvar != nullptr) {
        if (boolvar->Max() == 1) {
          boolvar->SetValue(0);
          active_watchers_.Decr(solver());
        }
      }
      if (active_watchers_.Value() == 0) {
        var_demon_->inhibit(solver());
      }
    }

    void One(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      if (boolvar != nullptr) {
        boolvar->SetValue(1);
      }
    }

    DomainIntVar* const variable_;
    IntVarIterator* const iterator_;
    RevGrowingArray<IntVar*, void*> watchers_;
    RevSwitch posted_;
    NumericalRev<int64> min_range_;
    NumericalRev<int64> max_range_;
    Demon* var_demon_;
    NumericalRev<int> active_watchers_;
  };

  // This class watches the bounds of the variable and updates the
  // IsGreater/IsGreaterOrEqual/IsLess/IsLessOrEqual demons
  // accordingly.
  class BoundWatcher : public Constraint {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(BoundWatcher* const watcher, int64 index)
          : value_watcher_(watcher), index_(index) {}
      virtual ~WatchDemon() {}

      virtual void Run(Solver* const solver) {
        value_watcher_->ProcessBoundWatcher(index_);
      }

     private:
      BoundWatcher* const value_watcher_;
      const int64 index_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(BoundWatcher* const watcher)
          : value_watcher_(watcher) {}
      virtual ~VarDemon() {}

      virtual void Run(Solver* const solver) { value_watcher_->ProcessVar(); }

     private:
      BoundWatcher* const value_watcher_;
    };

    BoundWatcher(Solver* const solver, DomainIntVar* const variable)
        : Constraint(solver),
          variable_(variable),
          iterator_(variable_->MakeHoleIterator(true)),
          watchers_(16),
          min_range_(kint64max),
          max_range_(kint64min),
          var_demon_(nullptr),
          active_watchers_(0) {}

    BoundWatcher(Solver* const solver, DomainIntVar* const variable,
                 const std::vector<int64>& values, const std::vector<IntVar*>& vars)
        : Constraint(solver),
          variable_(variable),
          iterator_(variable_->MakeHoleIterator(true)),
          watchers_(16),
          min_range_(kint64max),
          max_range_(kint64min),
          var_demon_(nullptr),
          active_watchers_(0) {
      CHECK_EQ(vars.size(), values.size());
      for (int i = 0; i < values.size(); ++i) {
        SetBoundWatcher(vars[i], values[i]);
      }
    }

    virtual ~BoundWatcher() {}

    IntVar* GetOrMakeBoundWatcher(int64 value) {
      IntVar* const watcher = watchers_.At(value);
      if (watcher != nullptr) {
        return watcher;
      }
      IntVar* boolvar = nullptr;
      if (variable_->Max() >= value) {
        if (variable_->Min() >= value) {
          boolvar = solver()->MakeIntConst(1);
        } else {
          const string vname = variable_->HasName() ? variable_->name()
                                                    : variable_->DebugString();
          const string bname = StringPrintf("Watch<%s >= %" GG_LL_FORMAT "d>",
                                            vname.c_str(), value);
          boolvar = solver()->MakeBoolVar(bname);
        }
        active_watchers_.Incr(solver());
      } else {
        boolvar = variable_->solver()->MakeIntConst(0);
      }
      min_range_.SetValue(solver(), std::min(min_range_.Value(), value));
      max_range_.SetValue(solver(), std::max(max_range_.Value(), value));
      watchers_.RevInsert(variable_->solver(), value, boolvar);
      if (posted_.Switched() && !boolvar->Bound()) {
        boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
      }
      return boolvar;
    }

    void SetBoundWatcher(IntVar* const boolvar, int64 value) {
      CHECK(watchers_.At(value) == nullptr);
      active_watchers_.Incr(solver());
      min_range_.SetValue(solver(), std::min(min_range_.Value(), value));
      max_range_.SetValue(solver(), std::max(max_range_.Value(), value));
      watchers_.RevInsert(solver(), value, boolvar);
      if (posted_.Switched() && !boolvar->Bound()) {
        boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
      }
    }

    virtual void Post() {
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenRange(var_demon_);

      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      for (int64 value = min_r; value <= max_r; ++value) {
        IntVar* const boolvar = watchers_.At(value);
        if (boolvar != nullptr && !boolvar->Bound()) {
          boolvar->WhenBound(solver()->RevAlloc(new WatchDemon(this, value)));
        }
      }
      posted_.Switch(solver());
    }

    virtual void InitialPropagate() {
      const int64 min_r = min_range_.Value();
      const int64 max_r = max_range_.Value();
      for (int value = min_r; value <= max_r; ++value) {
        IntVar* const boolvar = watchers_.At(value);
        if (boolvar != nullptr && boolvar->Bound()) {
          if (boolvar->Min() == 0) {
            variable_->SetMax(value - 1);
          } else {
            variable_->SetMin(value);
          }
        }
      }
      const int64 vmin = variable_->Min();
      const int64 vmax = variable_->Max();
      for (int64 value = min_r; value <= std::min(vmin, max_r); ++value) {
        One(value);
      }
      for (int64 value = std::max(vmax + 1, min_r); value <= max_r; ++value) {
        Zero(value);
      }
    }

    void ProcessBoundWatcher(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      DCHECK(boolvar != nullptr);
      if (boolvar->Min() == 0) {
        variable_->SetMax(index - 1);
      } else {
        variable_->SetMin(index);
      }
    }

    void ProcessVar() {
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      const int64 min_domain = variable_->Min();
      const int64 max_domain = variable_->Max();
      if (max_r - min_r < 4) {
        for (int64 value = min_r; value <= max_r; ++value) {
          if (min_domain >= value) {
            One(value);
          } else if (max_domain < value) {
            Zero(value);
          }
        }
      } else {
        const int64 old_min_domain = variable_->OldMin();
        for (int64 val = std::max(min_r, old_min_domain);
             val <= std::min(min_domain, max_r); ++val) {
          One(val);
        }
        const int64 old_max_domain = variable_->OldMax();
        for (int64 val = std::max(min_r, max_domain + 1);
             val <= std::min(max_r, old_max_domain); ++val) {
          Zero(val);
        }
      }
    }

    virtual void Accept(ModelVisitor* const visitor) const {
      visitor->BeginVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      const int64 max_r = max_range_.Value();
      const int64 min_r = min_range_.Value();
      for (int64 i = min_r; i <= max_r; ++i) {
        IntVar* const boolvar = watchers_.At(i);
        if (boolvar != nullptr) {
          all_coefficients.push_back(i);
          all_bool_vars.push_back(boolvar);
        }
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
    }

    virtual string DebugString() const {
      return StringPrintf("BoundWatcher(%s)", variable_->DebugString().c_str());
    }

   private:
    void Zero(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      if (boolvar != nullptr) {
        if (boolvar->Max() == 1) {
          boolvar->SetValue(0);
          active_watchers_.Decr(solver());
        }
      }
      if (active_watchers_.Value() == 0) {
        var_demon_->inhibit(solver());
      }
    }

    void One(int64 index) {
      IntVar* const boolvar = watchers_.At(index);
      if (boolvar != nullptr) {
        boolvar->SetValue(1);
      }
    }

    DomainIntVar* const variable_;
    IntVarIterator* const iterator_;
    RevGrowingArray<IntVar*, void*> watchers_;
    RevSwitch posted_;
    NumericalRev<int64> min_range_;
    NumericalRev<int64> max_range_;
    Demon* var_demon_;
    NumericalRev<int> active_watchers_;
  };

  // ----- Main Class -----
  DomainIntVar(Solver* const s, int64 vmin, int64 vmax, const string& name);
  DomainIntVar(Solver* const s, const std::vector<int64>& sorted_values,
               const string& name);
  virtual ~DomainIntVar();

  virtual int64 Min() const { return min_.Value(); }
  virtual void SetMin(int64 m);
  virtual int64 Max() const { return max_.Value(); }
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const { return (min_.Value() == max_.Value()); }
  virtual int64 Value() const {
    CHECK_EQ(min_.Value(), max_.Value()) << "variable " << DebugString().c_str()
                                         << "is not bound.";
    return min_.Value();
  }
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  void CreateBits();
  virtual void WhenBound(Demon* d) {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_bound_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
      } else {
        bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }
  virtual void WhenRange(Demon* d) {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_range_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
      } else {
        range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }
  virtual void WhenDomain(Demon* d) {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_domain_demons_.PushIfNotTop(solver(),
                                            solver()->RegisterDemon(d));
      } else {
        domain_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }

  virtual IntVar* IsEqual(int64 constant) {
    Solver* const s = solver();
    if (constant == min_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsLessOrEqualCstVar(this, constant);
    }
    if (constant == max_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsGreaterOrEqualCstVar(this, constant);
    }
    if (!Contains(constant)) {
      return s->MakeIntConst(0LL);
    }
    if (Bound() && min_.Value() == constant) {
      return s->MakeIntConst(1LL);
    }
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      if (value_watcher_ == nullptr) {
        solver()->SaveAndSetValue(reinterpret_cast<void**>(&value_watcher_),
                                  reinterpret_cast<void*>(solver()->RevAlloc(
                                      new ValueWatcher(solver(), this))));
        solver()->AddConstraint(value_watcher_);
      }
      IntVar* const boolvar = value_watcher_->GetOrMakeValueWatcher(constant);
      s->Cache()->InsertExprConstantExpression(
          boolvar, this, constant, ModelCache::EXPR_CONSTANT_IS_EQUAL);
      return boolvar;
    }
  }

  Constraint* SetIsEqual(const std::vector<int64>& values,
                         const std::vector<IntVar*>& vars) {
    if (value_watcher_ == nullptr) {
      solver()->SaveAndSetValue(
          reinterpret_cast<void**>(&value_watcher_),
          reinterpret_cast<void*>(solver()->RevAlloc(
              new ValueWatcher(solver(), this, values, vars))));
    }
    return value_watcher_;
  }

  virtual IntVar* IsDifferent(int64 constant) {
    Solver* const s = solver();
    if (constant == min_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsGreaterOrEqualCstVar(this, constant + 1);
    }
    if (constant == max_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsLessOrEqualCstVar(this, constant - 1);
    }
    if (!Contains(constant)) {
      return s->MakeIntConst(1LL);
    }
    if (Bound() && min_.Value() == constant) {
      return s->MakeIntConst(0LL);
    }
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_NOT_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      IntVar* const boolvar = s->MakeDifference(1, IsEqual(constant))->Var();
      s->Cache()->InsertExprConstantExpression(
          boolvar, this, constant, ModelCache::EXPR_CONSTANT_IS_NOT_EQUAL);
      return boolvar;
    }
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    Solver* const s = solver();
    if (max_.Value() < constant) {
      return s->MakeIntConst(0LL);
    }
    if (min_.Value() >= constant) {
      return s->MakeIntConst(1LL);
    }
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      if (bound_watcher_ == nullptr) {
        solver()->SaveAndSetValue(reinterpret_cast<void**>(&bound_watcher_),
                                  reinterpret_cast<void*>(solver()->RevAlloc(
                                      new BoundWatcher(solver(), this))));
        solver()->AddConstraint(bound_watcher_);
      }
      IntVar* const boolvar = bound_watcher_->GetOrMakeBoundWatcher(constant);
      s->Cache()->InsertExprConstantExpression(
          boolvar, this, constant,
          ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
      return boolvar;
    }
  }

  Constraint* SetIsGreaterOrEqual(const std::vector<int64>& values,
                                  const std::vector<IntVar*>& vars) {
    if (bound_watcher_ == nullptr) {
      solver()->SaveAndSetValue(
          reinterpret_cast<void**>(&bound_watcher_),
          reinterpret_cast<void*>(solver()->RevAlloc(
              new BoundWatcher(solver(), this, values, vars))));
    }
    return bound_watcher_;
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    Solver* const s = solver();
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_LESS_OR_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      IntVar* const boolvar =
          s->MakeDifference(1, IsGreaterOrEqual(constant + 1))->Var();
      s->Cache()->InsertExprConstantExpression(
          boolvar, this, constant, ModelCache::EXPR_CONSTANT_IS_LESS_OR_EQUAL);
      return boolvar;
    }
  }

  void Process();
  void Push();
  void ClearInProcess();
  virtual uint64 Size() const {
    if (bits_ != nullptr) return bits_->Size();
    return (max_.Value() - min_.Value() + 1);
  }
  virtual bool Contains(int64 v) const {
    if (v < min_.Value() || v > max_.Value()) return false;
    return (bits_ == nullptr ? true : bits_->Contains(v));
  }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const;
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const;
  virtual int64 OldMin() const { return std::min(old_min_, min_.Value()); }
  virtual int64 OldMax() const { return std::max(old_max_, max_.Value()); }

  virtual string DebugString() const;
  BitSet* bitset() const { return bits_; }
  virtual int VarType() const { return DOMAIN_INT_VAR; }
  virtual string BaseName() const { return "IntegerVar"; }

  friend class PlusCstDomainIntVar;
  friend class LinkExprAndDomainIntVar;

 private:
  void CheckOldMin() {
    if (old_min_ > min_.Value()) {
      old_min_ = min_.Value();
    }
  }
  void CheckOldMax() {
    if (old_max_ < max_.Value()) {
      old_max_ = max_.Value();
    }
  }
  Rev<int64> min_;
  Rev<int64> max_;
  int64 old_min_;
  int64 old_max_;
  int64 new_min_;
  int64 new_max_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> range_demons_;
  SimpleRevFIFO<Demon*> domain_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
  SimpleRevFIFO<Demon*> delayed_range_demons_;
  SimpleRevFIFO<Demon*> delayed_domain_demons_;
  QueueHandler handler_;
  bool in_process_;
  BitSet* bits_;
  ValueWatcher* value_watcher_;
  BoundWatcher* bound_watcher_;
};

// ----- BitSet -----

// Return whether an integer interval [a..b] (inclusive) contains at most
// K values, i.e. b - a < K, in a way that's robust to overflows.
// For performance reasons, in opt mode it doesn't check that [a, b] is a
// valid interval, nor that K is nonnegative.
inline bool ClosedIntervalNoLargerThan(int64 a, int64 b, int64 K) {
  DCHECK_LE(a, b);
  DCHECK_GE(K, 0);
  if (a > 0) {
    return a > b - K;
  } else {
    return a + K > b;
  }
}

class SimpleBitSet : public DomainIntVar::BitSet {
 public:
  SimpleBitSet(Solver* const s, int64 vmin, int64 vmax)
      : BitSet(s),
        bits_(nullptr),
        stamps_(nullptr),
        omin_(vmin),
        omax_(vmax),
        size_(vmax - vmin + 1),
        bsize_(BitLength64(size_.Value())) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 0xFFFFFFFF))
        << "Bitset too large: [" << vmin << ", " << vmax << "]";
    bits_ = new uint64[bsize_];
    stamps_ = new uint64[bsize_];
    for (int i = 0; i < bsize_; ++i) {
      const int bs =
          (i == size_.Value() - 1) ? 63 - BitPos64(size_.Value()) : 0;
      bits_[i] = kAllBits64 >> bs;
      stamps_[i] = s->stamp() - 1;
    }
  }

  SimpleBitSet(Solver* const s, const std::vector<int64>& sorted_values, int64 vmin,
               int64 vmax)
      : BitSet(s),
        bits_(nullptr),
        stamps_(nullptr),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()),
        bsize_(BitLength64(vmax - vmin + 1)) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 0xFFFFFFFF))
        << "Bitset too large: [" << vmin << ", " << vmax << "]";
    bits_ = new uint64[bsize_];
    stamps_ = new uint64[bsize_];
    for (int i = 0; i < bsize_; ++i) {
      bits_[i] = GG_ULONGLONG(0);
      stamps_[i] = s->stamp() - 1;
    }
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64 val = sorted_values[i];
      DCHECK(!bit(val));
      const int offset = BitOffset64(val - omin_);
      const int pos = BitPos64(val - omin_);
      bits_[offset] |= OneBit64(pos);
    }
  }

  virtual ~SimpleBitSet() {
    delete[] bits_;
    delete[] stamps_;
  }

  bool bit(int64 val) const { return IsBitSet64(bits_, val - omin_); }

  virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) {
    DCHECK_GE(nmin, cmin);
    DCHECK_LE(nmin, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    const int64 new_min =
        UnsafeLeastSignificantBitPosition64(bits_, nmin - omin_, cmax - omin_) +
        omin_;
    const uint64 removed_bits =
        BitCountRange64(bits_, cmin - omin_, new_min - omin_ - 1);
    size_.Add(solver_, -removed_bits);
    return new_min;
  }

  virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) {
    DCHECK_GE(nmax, cmin);
    DCHECK_LE(nmax, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    const int64 new_max =
        UnsafeMostSignificantBitPosition64(bits_, cmin - omin_, nmax - omin_) +
        omin_;
    const uint64 removed_bits =
        BitCountRange64(bits_, new_max - omin_ + 1, cmax - omin_);
    size_.Add(solver_, -removed_bits);
    return new_max;
  }

  virtual bool SetValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  virtual bool Contains(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  virtual bool RemoveValue(int64 val) {
    if (val < omin_ || val > omax_ || !bit(val)) {
      return false;
    }
    // Bitset.
    const int64 val_offset = val - omin_;
    const int offset = BitOffset64(val_offset);
    const uint64 current_stamp = solver_->stamp();
    if (stamps_[offset] < current_stamp) {
      stamps_[offset] = current_stamp;
      solver_->SaveValue(&bits_[offset]);
    }
    const int pos = BitPos64(val_offset);
    bits_[offset] &= ~OneBit64(pos);
    // Size.
    size_.Decr(solver_);
    // Holes.
    InitHoles();
    AddHole(val);
    return true;
  }
  virtual uint64 Size() const { return size_.Value(); }

  virtual string DebugString() const {
    string out;
    SStringPrintf(&out,
                  "SimpleBitSet(%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d : ",
                  omin_, omax_);
    for (int i = 0; i < bsize_; ++i) {
      StringAppendF(&out, "%llx", bits_[i]);
    }
    out += ")";
    return out;
  }

  virtual void DelayRemoveValue(int64 val) { removed_.push_back(val); }

  virtual void ApplyRemovedValues(DomainIntVar* var) {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin(); it != removed_.end();
         ++it) {
      var->RemoveValue(*it);
    }
  }

  virtual void ClearRemovedValues() { removed_.clear(); }

  virtual string pretty_DebugString(int64 min, int64 max) const {
    string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64 start_cumul = min;
      for (int64 v = min + 1; v < max; ++v) {
        if (bit(v)) {
          if (!cumul) {
            cumul = true;
            start_cumul = v;
          }
        } else {
          if (cumul) {
            if (v == start_cumul + 1) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d ", start_cumul);
            } else if (v == start_cumul + 2) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d %" GG_LL_FORMAT "d ",
                            start_cumul, v - 1);
            } else {
              StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d ",
                            start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          StringAppendF(&out, "%" GG_LL_FORMAT "d %" GG_LL_FORMAT "d",
                        start_cumul, max);
        } else {
          StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d",
                        start_cumul, max);
        }
      } else {
        StringAppendF(&out, "%" GG_LL_FORMAT "d", max);
      }
    } else {
      StringAppendF(&out, "%" GG_LL_FORMAT "d", min);
    }
    return out;
  }

  virtual DomainIntVar::BitSetIterator* MakeIterator() {
    return new DomainIntVar::BitSetIterator(bits_, omin_);
  }

 private:
  uint64* bits_;
  uint64* stamps_;
  const int64 omin_;
  const int64 omax_;
  NumericalRev<int64> size_;
  const int bsize_;
  std::vector<int64> removed_;
};

// This is a special case where the bitset fits into one 64 bit integer.
// In that case, there are no offset to compute.
// Overflows are caught by the robust ClosedIntervalNoLargerThan() method.
class SmallBitSet : public DomainIntVar::BitSet {
 public:
  SmallBitSet(Solver* const s, int64 vmin, int64 vmax)
      : BitSet(s),
        bits_(GG_ULONGLONG(0)),
        stamp_(s->stamp() - 1),
        omin_(vmin),
        omax_(vmax),
        size_(vmax - vmin + 1) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 64)) << vmin << ", " << vmax;
    bits_ = OneRange64(0, size_.Value() - 1);
  }

  SmallBitSet(Solver* const s, const std::vector<int64>& sorted_values, int64 vmin,
              int64 vmax)
      : BitSet(s),
        bits_(GG_ULONGLONG(0)),
        stamp_(s->stamp() - 1),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 64)) << vmin << ", " << vmax;
    // We know the array is sorted and does not contains duplicate values.
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64 val = sorted_values[i];
      DCHECK_GE(val, vmin);
      DCHECK_LE(val, vmax);
      DCHECK(!IsBitSet64(&bits_, val - omin_));
      bits_ |= OneBit64(val - omin_);
    }
  }

  virtual ~SmallBitSet() {}

  bool bit(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return (bits_ & OneBit64(val - omin_)) != 0;
  }

  virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) {
    DCHECK_GE(nmin, cmin);
    DCHECK_LE(nmin, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    // We do not clean the bits between cmin and nmin.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new bits
    const uint64 new_bits = bits_ & OneRange64(nmin - omin_, cmax - omin_);
    if (new_bits != GG_ULONGLONG(0)) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmin)) {  // Common case, the new min is inside the bitset
        return nmin;
      }
      return LeastSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return kint64max;
    }
  }

  virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) {
    DCHECK_GE(nmax, cmin);
    DCHECK_LE(nmax, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    // We do not clean the bits between nmax and cmax.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new_bits
    const uint64 new_bits = bits_ & OneRange64(cmin - omin_, nmax - omin_);
    if (new_bits != GG_ULONGLONG(0)) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmax)) {  // Common case, the new max is inside the bitset
        return nmax;
      }
      return MostSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return kint64min;
    }
  }

  virtual bool SetValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    // We do not clean the bits. We will use masks to ignore the bits
    // that should have been cleaned.
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  virtual bool Contains(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  virtual bool RemoveValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      // Bitset.
      const uint64 current_stamp = solver_->stamp();
      if (stamp_ < current_stamp) {
        stamp_ = current_stamp;
        solver_->SaveValue(&bits_);
      }
      bits_ &= ~OneBit64(val - omin_);
      DCHECK(!bit(val));
      // Size.
      size_.Decr(solver_);
      // Holes.
      InitHoles();
      AddHole(val);
      return true;
    } else {
      return false;
    }
  }

  virtual uint64 Size() const { return size_.Value(); }

  virtual string DebugString() const {
    return StringPrintf("SmallBitSet(%" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                        "d : %llx)",
                        omin_, omax_, bits_);
  }

  virtual void DelayRemoveValue(int64 val) {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    removed_.push_back(val);
  }

  virtual void ApplyRemovedValues(DomainIntVar* var) {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin(); it != removed_.end();
         ++it) {
      var->RemoveValue(*it);
    }
  }

  virtual void ClearRemovedValues() { removed_.clear(); }

  virtual string pretty_DebugString(int64 min, int64 max) const {
    string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64 start_cumul = min;
      for (int64 v = min + 1; v < max; ++v) {
        if (bit(v)) {
          if (!cumul) {
            cumul = true;
            start_cumul = v;
          }
        } else {
          if (cumul) {
            if (v == start_cumul + 1) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d ", start_cumul);
            } else if (v == start_cumul + 2) {
              StringAppendF(&out, "%" GG_LL_FORMAT "d %" GG_LL_FORMAT "d ",
                            start_cumul, v - 1);
            } else {
              StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d ",
                            start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          StringAppendF(&out, "%" GG_LL_FORMAT "d %" GG_LL_FORMAT "d",
                        start_cumul, max);
        } else {
          StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d",
                        start_cumul, max);
        }
      } else {
        StringAppendF(&out, "%" GG_LL_FORMAT "d", max);
      }
    } else {
      StringAppendF(&out, "%" GG_LL_FORMAT "d", min);
    }
    return out;
  }

  virtual DomainIntVar::BitSetIterator* MakeIterator() {
    return new DomainIntVar::BitSetIterator(&bits_, omin_);
  }

 private:
  uint64 bits_;
  uint64 stamp_;
  const int64 omin_;
  const int64 omax_;
  NumericalRev<int64> size_;
  std::vector<int64> removed_;
};

class EmptyIterator : public IntVarIterator {
 public:
  virtual ~EmptyIterator() {}
  virtual void Init() {}
  virtual bool Ok() const { return false; }
  virtual int64 Value() const {
    LOG(FATAL) << "Should not be called";
    return 0LL;
  }
  virtual void Next() {}
};

class RangeIterator : public IntVarIterator {
 public:
  explicit RangeIterator(const IntVar* const var)
      : var_(var), min_(kint64max), max_(kint64min), current_(-1) {}

  virtual ~RangeIterator() {}

  virtual void Init() {
    min_ = var_->Min();
    max_ = var_->Max();
    current_ = min_;
  }

  virtual bool Ok() const { return current_ <= max_; }

  virtual int64 Value() const { return current_; }

  virtual void Next() { current_++; }

 private:
  const IntVar* const var_;
  int64 min_;
  int64 max_;
  int64 current_;
};

class DomainIntVarHoleIterator : public IntVarIterator {
 public:
  explicit DomainIntVarHoleIterator(const DomainIntVar* const v)
      : var_(v), bits_(nullptr), values_(nullptr), size_(0), index_(0) {}

  ~DomainIntVarHoleIterator() {}

  virtual void Init() {
    bits_ = var_->bitset();
    if (bits_ != 0) {
      bits_->InitHoles();
      values_ = bits_->Holes().data();
      size_ = bits_->Holes().size();
    } else {
      values_ = nullptr;
      size_ = 0;
    }
    index_ = 0;
  }

  virtual bool Ok() const { return index_ < size_; }

  virtual int64 Value() const {
    DCHECK(bits_ != nullptr);
    DCHECK(index_ < size_);
    return values_[index_];
  }

  virtual void Next() { index_++; }

 private:
  const DomainIntVar* const var_;
  DomainIntVar::BitSet* bits_;
  const int64* values_;
  int size_;
  int index_;
};

class DomainIntVarDomainIterator : public IntVarIterator {
 public:
  explicit DomainIntVarDomainIterator(const DomainIntVar* const v,
                                      bool reversible)
      : var_(v),
        bitset_iterator_(nullptr),
        min_(kint64max),
        max_(kint64min),
        current_(-1),
        reversible_(reversible) {}

  virtual ~DomainIntVarDomainIterator() {
    if (!reversible_ && bitset_iterator_) {
      delete bitset_iterator_;
    }
  }

  virtual void Init() {
    if (var_->bitset() != nullptr && !var_->Bound()) {
      if (reversible_) {
        if (!bitset_iterator_) {
          Solver* const solver = var_->solver();
          solver->SaveValue(reinterpret_cast<void**>(&bitset_iterator_));
          bitset_iterator_ = solver->RevAlloc(var_->bitset()->MakeIterator());
        }
      } else {
        if (bitset_iterator_) {
          delete bitset_iterator_;
        }
        bitset_iterator_ = var_->bitset()->MakeIterator();
      }
      bitset_iterator_->Init(var_->Min(), var_->Max());
    } else {
      if (bitset_iterator_) {
        if (reversible_) {
          Solver* const solver = var_->solver();
          solver->SaveValue(reinterpret_cast<void**>(&bitset_iterator_));
        } else {
          delete bitset_iterator_;
        }
        bitset_iterator_ = nullptr;
      }
      min_ = var_->Min();
      max_ = var_->Max();
      current_ = min_;
    }
  }

  virtual bool Ok() const {
    return bitset_iterator_ ? bitset_iterator_->Ok() : (current_ <= max_);
  }

  virtual int64 Value() const {
    return bitset_iterator_ ? bitset_iterator_->Value() : current_;
  }

  virtual void Next() {
    if (bitset_iterator_) {
      bitset_iterator_->Next();
    } else {
      current_++;
    }
  }

 private:
  const DomainIntVar* const var_;
  DomainIntVar::BitSetIterator* bitset_iterator_;
  int64 min_;
  int64 max_;
  int64 current_;
  const bool reversible_;
};

class UnaryIterator : public IntVarIterator {
 public:
  UnaryIterator(const IntVar* const v, bool hole, bool reversible)
      : iterator_(hole ? v->MakeHoleIterator(reversible)
                       : v->MakeDomainIterator(reversible)),
        reversible_(reversible) {}

  virtual ~UnaryIterator() {
    if (!reversible_) {
      delete iterator_;
    }
  }

  virtual void Init() { iterator_->Init(); }

  virtual bool Ok() const { return iterator_->Ok(); }

  virtual void Next() { iterator_->Next(); }

 protected:
  IntVarIterator* const iterator_;
  const bool reversible_;
};

DomainIntVar::DomainIntVar(Solver* const s, int64 vmin, int64 vmax,
                           const string& name)
    : IntVar(s, name),
      min_(vmin),
      max_(vmax),
      old_min_(vmin),
      old_max_(vmax),
      new_min_(vmin),
      new_max_(vmax),
      handler_(this),
      in_process_(false),
      bits_(nullptr),
      value_watcher_(nullptr),
      bound_watcher_(nullptr) {}

DomainIntVar::DomainIntVar(Solver* const s, const std::vector<int64>& sorted_values,
                           const string& name)
    : IntVar(s, name),
      min_(kint64max),
      max_(kint64min),
      old_min_(kint64max),
      old_max_(kint64min),
      new_min_(kint64max),
      new_max_(kint64min),
      handler_(this),
      in_process_(false),
      bits_(nullptr),
      value_watcher_(nullptr),
      bound_watcher_(nullptr) {
  CHECK_GE(sorted_values.size(), 1);
  // We know that the vector is sorted and does not have duplicate values.
  const int64 vmin = sorted_values.front();
  const int64 vmax = sorted_values.back();
  const bool contiguous = vmax - vmin + 1 == sorted_values.size();

  min_.SetValue(solver(), vmin);
  old_min_ = vmin;
  new_min_ = vmin;
  max_.SetValue(solver(), vmax);
  old_max_ = vmax;
  new_max_ = vmax;

  if (!contiguous) {
    if (vmax - vmin + 1 < 65) {
      bits_ = solver()->RevAlloc(
          new SmallBitSet(solver(), sorted_values, vmin, vmax));
    } else {
      bits_ = solver()->RevAlloc(
          new SimpleBitSet(solver(), sorted_values, vmin, vmax));
    }
  }
}

DomainIntVar::~DomainIntVar() {}

void DomainIntVar::SetMin(int64 m) {
  if (m <= min_.Value()) return;
  if (m > max_.Value()) solver()->Fail();
  if (in_process_) {
    if (m > new_min_) {
      new_min_ = m;
      if (new_min_ > new_max_) {
        solver()->Fail();
      }
    }
  } else {
    CheckOldMin();
    const int64 new_min =
        (bits_ == nullptr ? m
                       : bits_->ComputeNewMin(m, min_.Value(), max_.Value()));
    min_.SetValue(solver(), new_min);
    if (min_.Value() > max_.Value()) {
      solver()->Fail();
    }
    Push();
  }
}

void DomainIntVar::SetMax(int64 m) {
  if (m >= max_.Value()) return;
  if (m < min_.Value()) solver()->Fail();
  if (in_process_) {
    if (m < new_max_) {
      new_max_ = m;
      if (new_max_ < new_min_) {
        solver()->Fail();
      }
    }
  } else {
    CheckOldMax();
    const int64 new_max =
        (bits_ == nullptr ? m
                       : bits_->ComputeNewMax(m, min_.Value(), max_.Value()));
    max_.SetValue(solver(), new_max);
    if (min_.Value() > max_.Value()) {
      solver()->Fail();
    }
    Push();
  }
}

void DomainIntVar::SetRange(int64 mi, int64 ma) {
  if (mi == ma) {
    SetValue(mi);
  } else {
    if (mi > ma || mi > max_.Value() || ma < min_.Value()) solver()->Fail();
    if (mi <= min_.Value() && ma >= max_.Value()) return;
    if (in_process_) {
      if (ma < new_max_) {
        new_max_ = ma;
      }
      if (mi > new_min_) {
        new_min_ = mi;
      }
      if (new_min_ > new_max_) {
        solver()->Fail();
      }
    } else {
      if (mi > min_.Value()) {
        CheckOldMin();
        const int64 new_min =
            (bits_ == nullptr ? mi : bits_->ComputeNewMin(mi, min_.Value(),
                                                       max_.Value()));
        min_.SetValue(solver(), new_min);
      }
      if (min_.Value() > ma) {
        solver()->Fail();
      }
      if (ma < max_.Value()) {
        CheckOldMax();
        const int64 new_max =
            (bits_ == nullptr ? ma : bits_->ComputeNewMax(ma, min_.Value(),
                                                       max_.Value()));
        max_.SetValue(solver(), new_max);
      }
      if (min_.Value() > max_.Value()) {
        solver()->Fail();
      }
      Push();
    }
  }
}

void DomainIntVar::SetValue(int64 v) {
  if (v != min_.Value() || v != max_.Value()) {
    if (v < min_.Value() || v > max_.Value()) {
      solver()->Fail();
    }
    if (in_process_) {
      if (v > new_max_ || v < new_min_) {
        solver()->Fail();
      }
      new_min_ = v;
      new_max_ = v;
    } else {
      if (bits_ && !bits_->SetValue(v)) {
        solver()->Fail();
      }
      CheckOldMin();
      CheckOldMax();
      min_.SetValue(solver(), v);
      max_.SetValue(solver(), v);
      Push();
    }
  }
}

void DomainIntVar::RemoveValue(int64 v) {
  if (v < min_.Value() || v > max_.Value()) return;
  if (v == min_.Value()) {
    SetMin(v + 1);
  } else if (v == max_.Value()) {
    SetMax(v - 1);
  } else {
    if (bits_ == nullptr) {
      CreateBits();
    }
    if (in_process_) {
      if (v >= new_min_ && v <= new_max_ && bits_->Contains(v)) {
        bits_->DelayRemoveValue(v);
      }
    } else {
      if (bits_->RemoveValue(v)) {
        Push();
      }
    }
  }
}

void DomainIntVar::RemoveInterval(int64 l, int64 u) {
  if (l <= min_.Value()) {
    SetMin(u + 1);
  } else if (u >= max_.Value()) {
    SetMax(l - 1);
  } else {
    for (int64 v = l; v <= u; ++v) {
      RemoveValue(v);
    }
  }
}

void DomainIntVar::CreateBits() {
  solver()->SaveValue(reinterpret_cast<void**>(&bits_));
  if (max_.Value() - min_.Value() < 64) {
    bits_ = solver()->RevAlloc(
        new SmallBitSet(solver(), min_.Value(), max_.Value()));
  } else {
    bits_ = solver()->RevAlloc(
        new SimpleBitSet(solver(), min_.Value(), max_.Value()));
  }
}

void DomainIntVar::ClearInProcess() {
  in_process_ = false;
  if (bits_ != nullptr) {
    bits_->ClearHoles();
  }
}

void DomainIntVar::Push() {
  const bool in_process = in_process_;
  EnqueueVar(&handler_);
  CHECK_EQ(in_process, in_process_);
}

void DomainIntVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  if (bits_ != nullptr) {
    bits_->ClearRemovedValues();
  }
  SetQueueCleanerOnFail(solver(), this);
  new_min_ = min_.Value();
  new_max_ = max_.Value();
  if (min_.Value() == max_.Value()) {
    ExecuteAll(bound_demons_);
    for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_bound_demons_); it.ok();
         ++it) {
      EnqueueDelayedDemon(*it);
    }
  }
  if (min_.Value() != OldMin() || max_.Value() != OldMax()) {
    ExecuteAll(range_demons_);
    for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_range_demons_); it.ok();
         ++it) {
      EnqueueDelayedDemon(*it);
    }
  }
  ExecuteAll(domain_demons_);
  for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_domain_demons_); it.ok();
       ++it) {
    EnqueueDelayedDemon(*it);
  }
  clear_queue_action_on_fail();
  ClearInProcess();
  old_min_ = min_.Value();
  old_max_ = max_.Value();
  if (min_.Value() < new_min_) {
    SetMin(new_min_);
  }
  if (max_.Value() > new_max_) {
    SetMax(new_max_);
  }
  if (bits_ != nullptr) {
    bits_->ApplyRemovedValues(this);
  }
}

#define COND_REV_ALLOC(rev, alloc) rev ? solver()->RevAlloc(alloc) : alloc;

IntVarIterator* DomainIntVar::MakeHoleIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible, new DomainIntVarHoleIterator(this));
}

IntVarIterator* DomainIntVar::MakeDomainIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible,
                        new DomainIntVarDomainIterator(this, reversible));
}

string DomainIntVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(";
  } else {
    out = "DomainIntVar(";
  }
  if (min_.Value() == max_.Value()) {
    StringAppendF(&out, "%" GG_LL_FORMAT "d", min_.Value());
  } else if (bits_ != nullptr) {
    StringAppendF(&out, "%s", bits_->pretty_DebugString(min_.Value(),
                                                        max_.Value()).c_str());
  } else {
    StringAppendF(&out, "%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d", min_.Value(),
                  max_.Value());
  }
  out += ")";
  return out;
}

// ----- Boolean variable -----

static const int kUnboundBooleanVarValue = 2;

class BooleanVar : public IntVar {
 public:
  // Utility classes
  class Handler : public Demon {
   public:
    explicit Handler(BooleanVar* const var) : Demon(), var_(var) {}
    virtual ~Handler() {}
    virtual void Run(Solver* const s) {
      s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
      var_->Process();
      s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
    }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }

   private:
    BooleanVar* const var_;
  };

  BooleanVar(Solver* const s, const string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue), handler_(this) {}
  virtual ~BooleanVar() {}

  virtual int64 Min() const { return (value_ == 1); }
  virtual void SetMin(int64 m);
  virtual int64 Max() const { return (value_ != 0); }
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const { return (value_ != kUnboundBooleanVarValue); }
  virtual int64 Value() const {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual void WhenBound(Demon* d) {
    if (value_ == kUnboundBooleanVarValue) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_bound_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
      } else {
        bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }
  virtual void WhenRange(Demon* d) { WhenBound(d); }
  virtual void WhenDomain(Demon* d) { WhenBound(d); }
  void Process();
  void Push() { EnqueueVar(&handler_); }
  virtual uint64 Size() const {
    return (1 + (value_ == kUnboundBooleanVarValue));
  }
  virtual bool Contains(int64 v) const {
    return ((v == 0 && value_ != 1) || (v == 1 && value_ != 0));
  }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new RangeIterator(this));
  }
  virtual int64 OldMin() const { return 0LL; }
  virtual int64 OldMax() const { return 1LL; }
  virtual string DebugString() const;
  virtual int VarType() const { return BOOLEAN_VAR; }

  virtual IntVar* IsEqual(int64 constant) {
    if (constant > 1 || constant < 0) {
      return solver()->MakeIntConst(0);
    }
    if (constant == 1) {
      return this;
    } else {  // constant == 0.
      return solver()->MakeDifference(1, this)->Var();
    }
  }

  virtual IntVar* IsDifferent(int64 constant) {
    if (constant > 1 || constant < 0) {
      return solver()->MakeIntConst(1);
    }
    if (constant == 1) {
      return solver()->MakeDifference(1, this)->Var();
    } else {  // constant == 0.
      return this;
    }
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    if (constant > 1) {
      return solver()->MakeIntConst(0);
    } else if (constant <= 0) {
      return solver()->MakeIntConst(1);
    } else {
      return this;
    }
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    if (constant < 0) {
      return solver()->MakeIntConst(0);
    } else if (constant >= 1) {
      return solver()->MakeIntConst(1);
    } else {
      return IsEqual(0);
    }
  }

  void RestoreValue() { value_ = kUnboundBooleanVarValue; }

  virtual string BaseName() const { return "BooleanVar"; }

  friend class TimesBooleanPosIntExpr;
  friend class TimesBooleanIntExpr;
  friend class TimesPosCstBoolVar;

 private:
  int value_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
  Handler handler_;
};

void BooleanVar::SetMin(int64 m) {
  if (m <= 0) {
    return;
  }
  if (m > 1) solver()->Fail();
  SetValue(1);
}

void BooleanVar::SetMax(int64 m) {
  if (m >= 1) {
    return;
  }
  if (m < 0) {
    solver()->Fail();
  }
  SetValue(0);
}

void BooleanVar::SetRange(int64 mi, int64 ma) {
  if (mi > 1 || ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi == 1) {
    SetValue(1);
  } else if (ma == 0) {
    SetValue(0);
  }
}

void BooleanVar::SetValue(int64 v) {
  if (value_ == kUnboundBooleanVarValue) {
    if (v == 0 || v == 1) {
      InternalSaveBooleanVarValue(solver(), this);
      value_ = static_cast<int>(v);
      Push();
      return;
    }
  } else if (v == value_) {
    return;
  }
  solver()->Fail();
}

void BooleanVar::RemoveValue(int64 v) {
  if (value_ == kUnboundBooleanVarValue) {
    if (v == 0) {
      SetValue(1);
    } else if (v == 1) {
      SetValue(0);
    }
  } else if (v == value_) {
    solver()->Fail();
  }
}

void BooleanVar::RemoveInterval(int64 l, int64 u) {
  if (l <= 0 && u >= 1) {
    solver()->Fail();
  } else if (l == 1) {
    SetValue(0);
  } else if (u == 0) {
    SetValue(1);
  }
}

void BooleanVar::Process() {
  DCHECK_NE(value_, kUnboundBooleanVarValue);
  ExecuteAll(bound_demons_);
  for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_bound_demons_); it.ok();
       ++it) {
    EnqueueDelayedDemon(*it);
  }
}

string BooleanVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(";
  } else {
    out = "BooleanVar(";
  }
  switch (value_) {
    case 0:
      out += "0";
      break;
    case 1:
      out += "1";
      break;
    case kUnboundBooleanVarValue:
      out += "0 .. 1";
      break;
  }
  out += ")";
  return out;
}

// ----- IntConst -----

class IntConst : public IntVar {
 public:
  IntConst(Solver* const s, int64 value, const string& name = "")
      : IntVar(s, name), value_(value) {}
  virtual ~IntConst() {}

  virtual int64 Min() const { return value_; }
  virtual void SetMin(int64 m) {
    if (m > value_) {
      solver()->Fail();
    }
  }
  virtual int64 Max() const { return value_; }
  virtual void SetMax(int64 m) {
    if (m < value_) {
      solver()->Fail();
    }
  }
  virtual void SetRange(int64 l, int64 u) {
    if (l > value_ || u < value_) {
      solver()->Fail();
    }
  }
  virtual void SetValue(int64 v) {
    if (v != value_) {
      solver()->Fail();
    }
  }
  virtual bool Bound() const { return true; }
  virtual int64 Value() const { return value_; }
  virtual void RemoveValue(int64 v) {
    if (v == value_) {
      solver()->Fail();
    }
  }
  virtual void RemoveInterval(int64 l, int64 u) {
    if (l <= value_ && value_ <= u) {
      solver()->Fail();
    }
  }
  virtual void WhenBound(Demon* d) {}
  virtual void WhenRange(Demon* d) {}
  virtual void WhenDomain(Demon* d) {}
  virtual uint64 Size() const { return 1; }
  virtual bool Contains(int64 v) const { return (v == value_); }
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new RangeIterator(this));
  }
  virtual int64 OldMin() const { return value_; }
  virtual int64 OldMax() const { return value_; }
  virtual string DebugString() const {
    string out;
    if (solver()->HasName(this)) {
      const string& var_name = name();
      SStringPrintf(&out, "%s(%" GG_LL_FORMAT "d)", var_name.c_str(), value_);
    } else {
      SStringPrintf(&out, "IntConst(%" GG_LL_FORMAT "d)", value_);
    }
    return out;
  }

  virtual int VarType() const { return CONST_VAR; }

  virtual IntVar* IsEqual(int64 constant) {
    if (constant == value_) {
      return solver()->MakeIntConst(1);
    } else {
      return solver()->MakeIntConst(0);
    }
  }

  virtual IntVar* IsDifferent(int64 constant) {
    if (constant == value_) {
      return solver()->MakeIntConst(0);
    } else {
      return solver()->MakeIntConst(1);
    }
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    return solver()->MakeIntConst(value_ >= constant);
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    return solver()->MakeIntConst(value_ <= constant);
  }

  virtual string name() const {
    if (solver()->HasName(this)) {
      return PropagationBaseObject::name();
    } else {
      return StringPrintf("%" GG_LL_FORMAT "d", value_);
    }
  }

 private:
  int64 value_;
};

// ----- x + c variable, optimized case -----

class PlusCstVar : public IntVar {
 public:
  PlusCstVar(Solver* const s, IntVar* v, int64 c)
      : IntVar(s), var_(v), cst_(c) {}

  virtual ~PlusCstVar() {}

  virtual void WhenRange(Demon* d) { var_->WhenRange(d); }

  virtual void WhenBound(Demon* d) { var_->WhenBound(d); }

  virtual void WhenDomain(Demon* d) { var_->WhenDomain(d); }

  virtual int64 OldMin() const { return CapAdd(var_->OldMin(), cst_); }

  virtual int64 OldMax() const { return CapAdd(var_->OldMax(), cst_); }

  virtual string DebugString() const {
    if (HasName()) {
      return StringPrintf("%s(%s + %" GG_LL_FORMAT "d)", name().c_str(),
                          var_->DebugString().c_str(), cst_);
    } else {
      return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                          var_->DebugString().c_str(), cst_);
    }
  }

  virtual int VarType() const { return VAR_ADD_CST; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this, ModelVisitor::kSumOperation, cst_,
                                  var_);
  }

  virtual IntVar* IsEqual(int64 constant) {
    return var_->IsEqual(constant - cst_);
  }

  virtual IntVar* IsDifferent(int64 constant) {
    return var_->IsDifferent(constant - cst_);
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    return var_->IsGreaterOrEqual(constant - cst_);
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    return var_->IsLessOrEqual(constant - cst_);
  }

  IntVar* SubVar() const { return var_; }

  int64 Constant() const { return cst_; }

 protected:
  IntVar* const var_;
  const int64 cst_;
};

class PlusCstIntVar : public PlusCstVar {
 public:
  class PlusCstIntVarIterator : public UnaryIterator {
   public:
    PlusCstIntVarIterator(const IntVar* const v, int64 c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}

    virtual ~PlusCstIntVarIterator() {}

    virtual int64 Value() const { return iterator_->Value() + cst_; }

   private:
    const int64 cst_;
  };

  PlusCstIntVar(Solver* const s, IntVar* v, int64 c) : PlusCstVar(s, v, c) {}

  virtual ~PlusCstIntVar() {}

  virtual int64 Min() const { return var_->Min() + cst_; }

  virtual void SetMin(int64 m) { var_->SetMin(CapSub(m, cst_)); }

  virtual int64 Max() const { return var_->Max() + cst_; }

  virtual void SetMax(int64 m) { var_->SetMax(CapSub(m, cst_)); }

  virtual void SetRange(int64 l, int64 u) {
    var_->SetRange(CapSub(l, cst_), CapSub(u, cst_));
  }

  virtual void SetValue(int64 v) { var_->SetValue(v - cst_); }

  virtual int64 Value() const { return var_->Value() + cst_; }

  virtual bool Bound() const { return var_->Bound(); }

  virtual void RemoveValue(int64 v) { var_->RemoveValue(v - cst_); }

  virtual void RemoveInterval(int64 l, int64 u) {
    var_->RemoveInterval(l - cst_, u - cst_);
  }

  virtual uint64 Size() const { return var_->Size(); }

  virtual bool Contains(int64 v) const { return var_->Contains(v - cst_); }

  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(
        reversible, new PlusCstIntVarIterator(var_, cst_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(
        reversible, new PlusCstIntVarIterator(var_, cst_, false, reversible));
  }
};

class PlusCstDomainIntVar : public PlusCstVar {
 public:
  class PlusCstDomainIntVarIterator : public UnaryIterator {
   public:
    PlusCstDomainIntVarIterator(const IntVar* const v, int64 c, bool hole,
                                bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}

    virtual ~PlusCstDomainIntVarIterator() {}

    virtual int64 Value() const { return iterator_->Value() + cst_; }

   private:
    const int64 cst_;
  };

  PlusCstDomainIntVar(Solver* const s, DomainIntVar* v, int64 c)
      : PlusCstVar(s, v, c) {}

  virtual ~PlusCstDomainIntVar() {}

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;

  DomainIntVar* domain_int_var() const {
    return reinterpret_cast<DomainIntVar*>(var_);
  }

  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new PlusCstDomainIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new PlusCstDomainIntVarIterator(
                                          var_, cst_, false, reversible));
  }
};

int64 PlusCstDomainIntVar::Min() const {
  return domain_int_var()->min_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMin(int64 m) {
  domain_int_var()->DomainIntVar::SetMin(m - cst_);
}

int64 PlusCstDomainIntVar::Max() const {
  return domain_int_var()->max_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMax(int64 m) {
  domain_int_var()->DomainIntVar::SetMax(m - cst_);
}

void PlusCstDomainIntVar::SetRange(int64 l, int64 u) {
  domain_int_var()->DomainIntVar::SetRange(l - cst_, u - cst_);
}

void PlusCstDomainIntVar::SetValue(int64 v) {
  domain_int_var()->DomainIntVar::SetValue(v - cst_);
}

bool PlusCstDomainIntVar::Bound() const {
  return domain_int_var()->min_.Value() == domain_int_var()->max_.Value();
}

int64 PlusCstDomainIntVar::Value() const {
  CHECK_EQ(domain_int_var()->min_.Value(), domain_int_var()->max_.Value())
      << "variable is not bound";
  return domain_int_var()->min_.Value() + cst_;
}

void PlusCstDomainIntVar::RemoveValue(int64 v) {
  domain_int_var()->DomainIntVar::RemoveValue(v - cst_);
}

void PlusCstDomainIntVar::RemoveInterval(int64 l, int64 u) {
  domain_int_var()->DomainIntVar::RemoveInterval(l - cst_, u - cst_);
}

uint64 PlusCstDomainIntVar::Size() const {
  return domain_int_var()->DomainIntVar::Size();
}

bool PlusCstDomainIntVar::Contains(int64 v) const {
  return domain_int_var()->DomainIntVar::Contains(v - cst_);
}

// c - x variable, optimized case

class SubCstIntVar : public IntVar {
 public:
  class SubCstIntVarIterator : public UnaryIterator {
   public:
    SubCstIntVarIterator(const IntVar* const v, int64 c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}
    virtual ~SubCstIntVarIterator() {}

    virtual int64 Value() const { return cst_ - iterator_->Value(); }

   private:
    const int64 cst_;
  };

  SubCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~SubCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(
        reversible, new SubCstIntVarIterator(var_, cst_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(
        reversible, new SubCstIntVarIterator(var_, cst_, false, reversible));
  }
  virtual int64 OldMin() const { return CapSub(cst_, var_->OldMax()); }
  virtual int64 OldMax() const { return CapSub(cst_, var_->OldMin()); }
  virtual string DebugString() const;
  virtual int VarType() const { return CST_SUB_VAR; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation,
                                  cst_, var_);
  }

  virtual IntVar* IsEqual(int64 constant) {
    return var_->IsEqual(cst_ - constant);
  }

  virtual IntVar* IsDifferent(int64 constant) {
    return var_->IsDifferent(cst_ - constant);
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    return var_->IsLessOrEqual(cst_ - constant);
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    return var_->IsGreaterOrEqual(cst_ - constant);
  }

  IntVar* SubVar() const { return var_; }
  int64 Constant() const { return cst_; }

 private:
  IntVar* const var_;
  const int64 cst_;
};

SubCstIntVar::SubCstIntVar(Solver* const s, IntVar* v, int64 c)
    : IntVar(s), var_(v), cst_(c) {}

SubCstIntVar::~SubCstIntVar() {}

int64 SubCstIntVar::Min() const { return cst_ - var_->Max(); }

void SubCstIntVar::SetMin(int64 m) { var_->SetMax(CapSub(cst_, m)); }

int64 SubCstIntVar::Max() const { return cst_ - var_->Min(); }

void SubCstIntVar::SetMax(int64 m) { var_->SetMin(CapSub(cst_, m)); }

void SubCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(CapSub(cst_, u), CapSub(cst_, l));
}

void SubCstIntVar::SetValue(int64 v) { var_->SetValue(cst_ - v); }

bool SubCstIntVar::Bound() const { return var_->Bound(); }

void SubCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64 SubCstIntVar::Value() const { return cst_ - var_->Value(); }

void SubCstIntVar::RemoveValue(int64 v) { var_->RemoveValue(cst_ - v); }

void SubCstIntVar::RemoveInterval(int64 l, int64 u) {
  var_->RemoveInterval(cst_ - u, cst_ - l);
}

void SubCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void SubCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64 SubCstIntVar::Size() const { return var_->Size(); }

bool SubCstIntVar::Contains(int64 v) const { return var_->Contains(cst_ - v); }

string SubCstIntVar::DebugString() const {
  if (cst_ == 1 && var_->VarType() == BOOLEAN_VAR) {
    return StringPrintf("Not(%s)", var_->DebugString().c_str());
  } else {
    return StringPrintf("(%" GG_LL_FORMAT "d - %s)", cst_,
                        var_->DebugString().c_str());
  }
}

// -x variable, optimized case

class OppIntVar : public IntVar {
 public:
  class OppIntVarIterator : public UnaryIterator {
   public:
    OppIntVarIterator(const IntVar* const v, bool hole, bool reversible)
        : UnaryIterator(v, hole, reversible) {}
    virtual ~OppIntVarIterator() {}

    virtual int64 Value() const { return -iterator_->Value(); }
  };

  OppIntVar(Solver* const s, IntVar* v);
  virtual ~OppIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, false, reversible));
  }
  virtual int64 OldMin() const { return CapOpp(var_->OldMax()); }
  virtual int64 OldMax() const { return CapOpp(var_->OldMin()); }
  virtual string DebugString() const;
  virtual int VarType() const { return OPP_VAR; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation, 0,
                                  var_);
  }

  virtual IntVar* IsEqual(int64 constant) { return var_->IsEqual(-constant); }

  virtual IntVar* IsDifferent(int64 constant) {
    return var_->IsDifferent(-constant);
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    return var_->IsLessOrEqual(-constant);
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    return var_->IsGreaterOrEqual(-constant);
  }

  IntVar* SubVar() const { return var_; }

 private:
  IntVar* const var_;
};

OppIntVar::OppIntVar(Solver* const s, IntVar* v) : IntVar(s), var_(v) {}

OppIntVar::~OppIntVar() {}

int64 OppIntVar::Min() const { return -var_->Max(); }

void OppIntVar::SetMin(int64 m) { var_->SetMax(CapOpp(m)); }

int64 OppIntVar::Max() const { return -var_->Min(); }

void OppIntVar::SetMax(int64 m) { var_->SetMin(CapOpp(m)); }

void OppIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(CapOpp(u), CapOpp(l));
}

void OppIntVar::SetValue(int64 v) { var_->SetValue(CapOpp(v)); }

bool OppIntVar::Bound() const { return var_->Bound(); }

void OppIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64 OppIntVar::Value() const { return -var_->Value(); }

void OppIntVar::RemoveValue(int64 v) { var_->RemoveValue(-v); }

void OppIntVar::RemoveInterval(int64 l, int64 u) {
  var_->RemoveInterval(-u, -l);
}

void OppIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void OppIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64 OppIntVar::Size() const { return var_->Size(); }

bool OppIntVar::Contains(int64 v) const { return var_->Contains(-v); }

string OppIntVar::DebugString() const {
  return StringPrintf("-(%s)", var_->DebugString().c_str());
}

// ----- Utility functions -----

// x * c variable, optimized case

class TimesCstIntVar : public IntVar {
 public:
  TimesCstIntVar(Solver* const s, IntVar* v, int64 c)
      : IntVar(s), var_(v), cst_(c) {}
  virtual ~TimesCstIntVar() {}

  IntVar* SubVar() const { return var_; }
  int64 Constant() const { return cst_; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntegerVariable(this, ModelVisitor::kProductOperation, cst_,
                                  var_);
  }

  virtual IntVar* IsEqual(int64 constant) {
    if (constant % cst_ == 0) {
      return var_->IsEqual(constant / cst_);
    } else {
      return solver()->MakeIntConst(0);
    }
  }

  virtual IntVar* IsDifferent(int64 constant) {
    if (constant % cst_ == 0) {
      return var_->IsDifferent(constant / cst_);
    } else {
      return solver()->MakeIntConst(1);
    }
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    if (cst_ > 0) {
      return var_->IsGreaterOrEqual(PosIntDivUp(constant, cst_));
    } else {
      return var_->IsLessOrEqual(PosIntDivDown(-constant, -cst_));
    }
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
    if (cst_ > 0) {
      return var_->IsLessOrEqual(PosIntDivDown(constant, cst_));
    } else {
      return var_->IsGreaterOrEqual(PosIntDivUp(-constant, -cst_));
    }
  }

  virtual string DebugString() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), cst_);
  }

  virtual int VarType() const { return VAR_TIMES_CST; }

 protected:
  IntVar* const var_;
  const int64 cst_;
};

class TimesPosCstIntVar : public TimesCstIntVar {
 public:
  class TimesPosCstIntVarIterator : public UnaryIterator {
   public:
    TimesPosCstIntVarIterator(const IntVar* const v, int64 c, bool hole,
                              bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    virtual ~TimesPosCstIntVarIterator() {}

    virtual int64 Value() const { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesPosCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~TimesPosCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new TimesPosCstIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new TimesPosCstIntVarIterator(
                                          var_, cst_, false, reversible));
  }
  virtual int64 OldMin() const { return CapProd(var_->OldMin(), cst_); }
  virtual int64 OldMax() const { return CapProd(var_->OldMax(), cst_); }
};

// ----- TimesPosCstIntVar -----

TimesPosCstIntVar::TimesPosCstIntVar(Solver* const s, IntVar* v, int64 c)
    : TimesCstIntVar(s, v, c) {}

TimesPosCstIntVar::~TimesPosCstIntVar() {}

int64 TimesPosCstIntVar::Min() const { return CapProd(var_->Min(), cst_); }

void TimesPosCstIntVar::SetMin(int64 m) {
  if (m != kint64min) {
    var_->SetMin(PosIntDivUp(m, cst_));
  }
}

int64 TimesPosCstIntVar::Max() const { return CapProd(var_->Max(), cst_); }

void TimesPosCstIntVar::SetMax(int64 m) {
  if (m != kint64max) {
    var_->SetMax(PosIntDivDown(m, cst_));
  }
}

void TimesPosCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(PosIntDivUp(l, cst_), PosIntDivDown(u, cst_));
}

void TimesPosCstIntVar::SetValue(int64 v) {
  if (v % cst_ != 0) {
    solver()->Fail();
  }
  var_->SetValue(v / cst_);
}

bool TimesPosCstIntVar::Bound() const { return var_->Bound(); }

void TimesPosCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64 TimesPosCstIntVar::Value() const { return CapProd(var_->Value(), cst_); }

void TimesPosCstIntVar::RemoveValue(int64 v) {
  if (v % cst_ == 0) {
    var_->RemoveValue(v / cst_);
  }
}

void TimesPosCstIntVar::RemoveInterval(int64 l, int64 u) {
  for (int64 v = l; v <= u; ++v) {
    RemoveValue(v);
  }
  // TODO(user) : Improve me
}

void TimesPosCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void TimesPosCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64 TimesPosCstIntVar::Size() const { return var_->Size(); }

bool TimesPosCstIntVar::Contains(int64 v) const {
  return (v % cst_ == 0 && var_->Contains(v / cst_));
}

// b * c variable, optimized case

class TimesPosCstBoolVar : public TimesCstIntVar {
 public:
  class TimesPosCstBoolVarIterator : public UnaryIterator {
   public:
    // TODO(user) : optimize this.
    TimesPosCstBoolVarIterator(const IntVar* const v, int64 c, bool hole,
                               bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    virtual ~TimesPosCstBoolVarIterator() {}

    virtual int64 Value() const { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c);
  virtual ~TimesPosCstBoolVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(
        reversible,
        new TimesPosCstBoolVarIterator(boolean_var(), cst_, false, reversible));
  }
  virtual int64 OldMin() const { return 0; }
  virtual int64 OldMax() const { return cst_; }

  BooleanVar* boolean_var() const {
    return reinterpret_cast<BooleanVar*>(var_);
  }
};

// ----- TimesPosCstBoolVar -----

TimesPosCstBoolVar::TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c)
    : TimesCstIntVar(s, v, c) {}

TimesPosCstBoolVar::~TimesPosCstBoolVar() {}

int64 TimesPosCstBoolVar::Min() const {
  return (boolean_var()->value_ == 1) * cst_;
}

void TimesPosCstBoolVar::SetMin(int64 m) {
  if (m > cst_) {
    solver()->Fail();
  } else if (m > 0) {
    boolean_var()->SetMin(1);
  }
}

int64 TimesPosCstBoolVar::Max() const {
  return (boolean_var()->value_ != 0) * cst_;
}

void TimesPosCstBoolVar::SetMax(int64 m) {
  if (m < 0) {
    solver()->Fail();
  } else if (m < cst_) {
    boolean_var()->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetRange(int64 l, int64 u) {
  if (u < 0 || l > cst_ || l > u) {
    solver()->Fail();
  }
  if (l > 0) {
    boolean_var()->SetMin(1);
  } else if (u < cst_) {
    boolean_var()->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetValue(int64 v) {
  if (v == 0) {
    boolean_var()->SetValue(0);
  } else if (v == cst_) {
    boolean_var()->SetValue(1);
  } else {
    solver()->Fail();
  }
}

bool TimesPosCstBoolVar::Bound() const {
  return boolean_var()->value_ != kUnboundBooleanVarValue;
}

void TimesPosCstBoolVar::WhenRange(Demon* d) { boolean_var()->WhenRange(d); }

int64 TimesPosCstBoolVar::Value() const {
  CHECK_NE(boolean_var()->value_, kUnboundBooleanVarValue)
      << "variable is not bound";
  return boolean_var()->value_ * cst_;
}

void TimesPosCstBoolVar::RemoveValue(int64 v) {
  if (v == 0) {
    boolean_var()->RemoveValue(0);
  } else if (v == cst_) {
    boolean_var()->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::RemoveInterval(int64 l, int64 u) {
  if (l <= 0 && u >= 0) {
    boolean_var()->RemoveValue(0);
  }
  if (l <= cst_ && u >= cst_) {
    boolean_var()->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::WhenBound(Demon* d) { boolean_var()->WhenBound(d); }

void TimesPosCstBoolVar::WhenDomain(Demon* d) { boolean_var()->WhenDomain(d); }

uint64 TimesPosCstBoolVar::Size() const {
  return (1 + (boolean_var()->value_ == kUnboundBooleanVarValue));
}

bool TimesPosCstBoolVar::Contains(int64 v) const {
  if (v == 0) {
    return boolean_var()->value_ != 1;
  } else if (v == cst_) {
    return boolean_var()->value_ != 0;
  }
  return false;
}

// TimesNegCstIntVar

class TimesNegCstIntVar : public TimesCstIntVar {
 public:
  class TimesNegCstIntVarIterator : public UnaryIterator {
   public:
    TimesNegCstIntVarIterator(const IntVar* const v, int64 c, bool hole,
                              bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    virtual ~TimesNegCstIntVarIterator() {}

    virtual int64 Value() const { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesNegCstIntVar(Solver* const s, IntVar* v, int64 c);
  virtual ~TimesNegCstIntVar();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 l, int64 u);
  virtual void SetValue(int64 v);
  virtual bool Bound() const;
  virtual int64 Value() const;
  virtual void RemoveValue(int64 v);
  virtual void RemoveInterval(int64 l, int64 u);
  virtual uint64 Size() const;
  virtual bool Contains(int64 v) const;
  virtual void WhenRange(Demon* d);
  virtual void WhenBound(Demon* d);
  virtual void WhenDomain(Demon* d);
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new TimesNegCstIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return COND_REV_ALLOC(reversible, new TimesNegCstIntVarIterator(
                                          var_, cst_, false, reversible));
  }
  virtual int64 OldMin() const { return CapProd(var_->OldMax(), cst_); }
  virtual int64 OldMax() const { return CapProd(var_->OldMin(), cst_); }
};

// ----- TimesNegCstIntVar -----

TimesNegCstIntVar::TimesNegCstIntVar(Solver* const s, IntVar* v, int64 c)
    : TimesCstIntVar(s, v, c) {}

TimesNegCstIntVar::~TimesNegCstIntVar() {}

int64 TimesNegCstIntVar::Min() const { return CapProd(var_->Max(), cst_); }

void TimesNegCstIntVar::SetMin(int64 m) {
  if (m != kint64min) {
    var_->SetMax(PosIntDivDown(-m, -cst_));
  }
}

int64 TimesNegCstIntVar::Max() const { return CapProd(var_->Min(), cst_); }

void TimesNegCstIntVar::SetMax(int64 m) {
  if (m != kint64max) {
    var_->SetMin(PosIntDivUp(-m, -cst_));
  }
}

void TimesNegCstIntVar::SetRange(int64 l, int64 u) {
  var_->SetRange(PosIntDivUp(-u, -cst_), PosIntDivDown(-l, -cst_));
}

void TimesNegCstIntVar::SetValue(int64 v) {
  if (v % cst_ != 0) {
    solver()->Fail();
  }
  var_->SetValue(v / cst_);
}

bool TimesNegCstIntVar::Bound() const { return var_->Bound(); }

void TimesNegCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64 TimesNegCstIntVar::Value() const { return CapProd(var_->Value(), cst_); }

void TimesNegCstIntVar::RemoveValue(int64 v) {
  if (v % cst_ == 0) {
    var_->RemoveValue(v / cst_);
  }
}

void TimesNegCstIntVar::RemoveInterval(int64 l, int64 u) {
  for (int64 v = l; v <= u; ++v) {
    RemoveValue(v);
  }
  // TODO(user) : Improve me
}

void TimesNegCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void TimesNegCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64 TimesNegCstIntVar::Size() const { return var_->Size(); }

bool TimesNegCstIntVar::Contains(int64 v) const {
  return (v % cst_ == 0 && var_->Contains(v / cst_));
}

// ---------- arithmetic expressions ----------

// ----- PlusIntExpr -----

class PlusIntExpr : public BaseIntExpr {
 public:
  PlusIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}

  virtual ~PlusIntExpr() {}

  virtual int64 Min() const { return left_->Min() + right_->Min(); }

  virtual void SetMin(int64 m) {
    if (m > left_->Min() + right_->Min()) {
      left_->SetMin(m - right_->Max());
      right_->SetMin(m - left_->Max());
    }
  }

  virtual void SetRange(int64 l, int64 u) {
    const int64 left_min = left_->Min();
    const int64 right_min = right_->Min();
    const int64 left_max = left_->Max();
    const int64 right_max = right_->Max();
    if (l > left_min + right_min) {
      left_->SetMin(l - right_max);
      right_->SetMin(l - left_max);
    }
    if (u < left_max + right_max) {
      left_->SetMax(u - right_min);
      right_->SetMax(u - left_min);
    }
  }

  virtual int64 Max() const { return left_->Max() + right_->Max(); }

  virtual void SetMax(int64 m) {
    if (m < left_->Max() + right_->Max()) {
      left_->SetMax(m - right_->Min());
      right_->SetMax(m - left_->Min());
    }
  }

  virtual bool Bound() const { return (left_->Bound() && right_->Bound()); }

  virtual void Range(int64* const mi, int64* const ma) {
    *mi = left_->Min() + right_->Min();
    *ma = left_->Max() + right_->Max();
  }

  virtual string name() const {
    return StringPrintf("(%s + %s)", left_->name().c_str(),
                        right_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("(%s + %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

class SafePlusIntExpr : public BaseIntExpr {
 public:
  SafePlusIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}

  virtual ~SafePlusIntExpr() {}

  virtual int64 Min() const { return CapAdd(left_->Min(), right_->Min()); }

  virtual void SetMin(int64 m) {
    left_->SetMin(CapSub(m, right_->Max()));
    right_->SetMin(CapSub(m, left_->Max()));
  }

  virtual void SetRange(int64 l, int64 u) {
    const int64 left_min = left_->Min();
    const int64 right_min = right_->Min();
    const int64 left_max = left_->Max();
    const int64 right_max = right_->Max();
    if (l > CapAdd(left_min, right_min)) {
      left_->SetMin(CapSub(l, right_max));
      right_->SetMin(CapSub(l, left_max));
    }
    if (u < CapAdd(left_max, right_max)) {
      left_->SetMax(CapSub(u, right_min));
      right_->SetMax(CapSub(u, left_min));
    }
  }

  virtual int64 Max() const { return CapAdd(left_->Max(), right_->Max()); }

  virtual void SetMax(int64 m) {
    if (!SubOverflows(m, right_->Min())) {
      left_->SetMax(CapSub(m, right_->Min()));
    }
    if (!SubOverflows(m, left_->Min())) {
      right_->SetMax(CapSub(m, left_->Min()));
    }
  }

  virtual bool Bound() const { return (left_->Bound() && right_->Bound()); }

  virtual string name() const {
    return StringPrintf("(%s + %s)", left_->name().c_str(),
                        right_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("(%s + %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- PlusIntCstExpr -----

class PlusIntCstExpr : public BaseIntExpr {
 public:
  PlusIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~PlusIntCstExpr() {}
  virtual int64 Min() const { return CapAdd(expr_->Min(), value_); }
  virtual void SetMin(int64 m) { expr_->SetMin(CapSub(m, value_)); }
  virtual int64 Max() const { return CapAdd(expr_->Max(), value_); }
  virtual void SetMax(int64 m) { expr_->SetMax(CapSub(m, value_)); }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%s + %" GG_LL_FORMAT "d)", expr_->name().c_str(),
                        value_);
  }
  virtual string DebugString() const {
    return StringPrintf("(%s + %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }
  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }
  virtual IntVar* CastToVar();
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

IntVar* PlusIntCstExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var = expr_->Var();
  IntVar* cast = nullptr;
  if (AddOverflows(value_, expr_->Max()) ||
      AddUnderflows(value_, expr_->Min())) {
    return BaseIntExpr::CastToVar();
  }
  switch (var->VarType()) {
    case DOMAIN_INT_VAR:
      cast = s->RegisterIntVar(s->RevAlloc(new PlusCstDomainIntVar(
          s, reinterpret_cast<DomainIntVar*>(var), value_)));
      // FIXME: Break was inserted during fallthrough cleanup. Please check.
      break;
    default:
      cast = s->RegisterIntVar(s->RevAlloc(new PlusCstIntVar(s, var, value_)));
      break;
  }
  return cast;
}

// ----- SubIntExpr -----

class SubIntExpr : public BaseIntExpr {
 public:
  SubIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}

  virtual ~SubIntExpr() {}

  virtual int64 Min() const { return left_->Min() - right_->Max(); }

  virtual void SetMin(int64 m) {
    left_->SetMin(CapAdd(m, right_->Min()));
    right_->SetMax(CapSub(left_->Max(), m));
  }

  virtual int64 Max() const { return left_->Max() - right_->Min(); }

  virtual void SetMax(int64 m) {
    left_->SetMax(CapAdd(m, right_->Max()));
    right_->SetMin(CapSub(left_->Min(), m));
  }

  virtual void Range(int64* mi, int64* ma) {
    *mi = left_->Min() - right_->Max();
    *ma = left_->Max() - right_->Min();
  }

  virtual void SetRange(int64 l, int64 u) {
    const int64 left_min = left_->Min();
    const int64 right_min = right_->Min();
    const int64 left_max = left_->Max();
    const int64 right_max = right_->Max();
    if (l > left_min - right_max) {
      left_->SetMin(CapAdd(l, right_min));
      right_->SetMax(CapSub(left_max, l));
    }
    if (u < left_max - right_min) {
      left_->SetMax(CapAdd(u, right_max));
      right_->SetMin(CapSub(left_min, u));
    }
  }

  virtual bool Bound() const { return (left_->Bound() && right_->Bound()); }

  virtual string name() const {
    return StringPrintf("(%s - %s)", left_->name().c_str(),
                        right_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("(%s - %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
  }

  IntExpr* left() const { return left_; }
  IntExpr* right() const { return right_; }

 protected:
  IntExpr* const left_;
  IntExpr* const right_;
};

class SafeSubIntExpr : public SubIntExpr {
 public:
  SafeSubIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : SubIntExpr(s, l, r) {}

  virtual ~SafeSubIntExpr() {}

  virtual int64 Min() const { return CapSub(left_->Min(), right_->Max()); }

  virtual void SetMin(int64 m) {
    left_->SetMin(CapAdd(m, right_->Min()));
    right_->SetMax(CapSub(left_->Max(), m));
  }

  virtual void SetRange(int64 l, int64 u) {
    const int64 left_min = left_->Min();
    const int64 right_min = right_->Min();
    const int64 left_max = left_->Max();
    const int64 right_max = right_->Max();
    if (l > CapSub(left_min, right_max)) {
      left_->SetMin(CapAdd(l, right_min));
      right_->SetMax(CapSub(left_max, l));
    }
    if (u < CapSub(left_max, right_min)) {
      left_->SetMax(CapAdd(u, right_max));
      right_->SetMin(CapSub(left_min, u));
    }
  }

  virtual void Range(int64* mi, int64* ma) {
    *mi = CapSub(left_->Min(), right_->Max());
    *ma = CapSub(left_->Max(), right_->Min());
  }

  virtual int64 Max() const { return CapSub(left_->Max(), right_->Min()); }

  virtual void SetMax(int64 m) {
    left_->SetMax(CapAdd(m, right_->Max()));
    right_->SetMin(CapSub(left_->Min(), m));
  }
};

// l - r

// ----- SubIntCstExpr -----

class SubIntCstExpr : public BaseIntExpr {
 public:
  SubIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}
  virtual ~SubIntCstExpr() {}
  virtual int64 Min() const { return CapSub(value_, expr_->Max()); }
  virtual void SetMin(int64 m) { expr_->SetMax(CapSub(value_, m)); }
  virtual int64 Max() const { return CapSub(value_, expr_->Min()); }
  virtual void SetMax(int64 m) { expr_->SetMin(CapSub(value_, m)); }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(%" GG_LL_FORMAT "d - %s)", value_,
                        expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%" GG_LL_FORMAT "d - %s)", value_,
                        expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }
  virtual IntVar* CastToVar();

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

IntVar* SubIntCstExpr::CastToVar() {
  if (SubOverflows(value_, expr_->Min()) ||
      SubUnderflows(value_, expr_->Max())) {
    return BaseIntExpr::CastToVar();
  }
  Solver* const s = solver();
  IntVar* const var =
      s->RegisterIntVar(s->RevAlloc(new SubCstIntVar(s, expr_->Var(), value_)));
  return var;
}

// ----- OppIntExpr -----

class OppIntExpr : public BaseIntExpr {
 public:
  OppIntExpr(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~OppIntExpr() {}
  virtual int64 Min() const { return (-expr_->Max()); }
  virtual void SetMin(int64 m) { expr_->SetMax(-m); }
  virtual int64 Max() const { return (-expr_->Min()); }
  virtual void SetMax(int64 m) { expr_->SetMin(-m); }
  virtual bool Bound() const { return (expr_->Bound()); }
  virtual string name() const {
    return StringPrintf("(-%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(-%s)", expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }
  virtual IntVar* CastToVar();

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kOpposite, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kOpposite, this);
  }

 private:
  IntExpr* const expr_;
};

IntVar* OppIntExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var =
      s->RegisterIntVar(s->RevAlloc(new OppIntVar(s, expr_->Var())));
  return var;
}

// ----- TimesIntCstExpr -----

class TimesIntCstExpr : public BaseIntExpr {
 public:
  TimesIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}

  virtual ~TimesIntCstExpr() {}

  virtual bool Bound() const { return (expr_->Bound()); }

  virtual string name() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)", expr_->name().c_str(),
                        value_);
  }

  virtual string DebugString() const {
    return StringPrintf("(%s * %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  IntExpr* Expr() const { return expr_; }

  int64 Constant() const { return value_; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 protected:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- TimesPosIntCstExpr -----

class TimesPosIntCstExpr : public TimesIntCstExpr {
 public:
  TimesPosIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : TimesIntCstExpr(s, e, v) {
    CHECK_GT(v, 0);
  }

  virtual ~TimesPosIntCstExpr() {}

  virtual int64 Min() const { return expr_->Min() * value_; }

  virtual void SetMin(int64 m) { expr_->SetMin(PosIntDivUp(m, value_)); }

  virtual int64 Max() const { return expr_->Max() * value_; }

  virtual void SetMax(int64 m) { expr_->SetMax(PosIntDivDown(m, value_)); }

  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    IntVar* var = nullptr;
    if (expr_->IsVar() &&
        reinterpret_cast<IntVar*>(expr_)->VarType() == BOOLEAN_VAR) {
      var = s->RegisterIntVar(s->RevAlloc(new TimesPosCstBoolVar(
          s, reinterpret_cast<BooleanVar*>(expr_), value_)));
    } else {
      var = s->RegisterIntVar(
          s->RevAlloc(new TimesPosCstIntVar(s, expr_->Var(), value_)));
    }
    return var;
  }
};

// This expressions adds safe arithmetic (w.r.t. overflows) compared
// to the previous one.
class SafeTimesPosIntCstExpr : public TimesIntCstExpr {
 public:
  SafeTimesPosIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : TimesIntCstExpr(s, e, v) {
    CHECK_GT(v, 0);
  }

  virtual ~SafeTimesPosIntCstExpr() {}

  virtual int64 Min() const { return CapProd(expr_->Min(), value_); }

  virtual void SetMin(int64 m) {
    if (m != kint64min) {
      expr_->SetMin(PosIntDivUp(m, value_));
    }
  }

  virtual int64 Max() const { return CapProd(expr_->Max(), value_); }

  virtual void SetMax(int64 m) {
    if (m != kint64max) {
      expr_->SetMax(PosIntDivDown(m, value_));
    }
  }

  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    IntVar* var = nullptr;
    if (expr_->IsVar() &&
        reinterpret_cast<IntVar*>(expr_)->VarType() == BOOLEAN_VAR) {
      var = s->RegisterIntVar(s->RevAlloc(new TimesPosCstBoolVar(
          s, reinterpret_cast<BooleanVar*>(expr_), value_)));
    } else {
      // TODO(user): Check overflows.
      var = s->RegisterIntVar(
          s->RevAlloc(new TimesPosCstIntVar(s, expr_->Var(), value_)));
    }
    return var;
  }
};

// ----- TimesIntNegCstExpr -----

class TimesIntNegCstExpr : public TimesIntCstExpr {
 public:
  TimesIntNegCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : TimesIntCstExpr(s, e, v) {
    CHECK_LT(v, 0);
  }

  virtual ~TimesIntNegCstExpr() {}

  virtual int64 Min() const { return CapProd(expr_->Max(), value_); }

  virtual void SetMin(int64 m) {
    if (m != kint64min) {
      expr_->SetMax(PosIntDivDown(-m, -value_));
    }
  }

  virtual int64 Max() const { return CapProd(expr_->Min(), value_); }

  virtual void SetMax(int64 m) {
    if (m != kint64max) {
      expr_->SetMin(PosIntDivUp(-m, -value_));
    }
  }

  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    IntVar* var = nullptr;
    var = s->RegisterIntVar(
        s->RevAlloc(new TimesNegCstIntVar(s, expr_->Var(), value_)));
    return var;
  }
};

// ----- Utilities for product expression -----

// Propagates set_min on left * right, left and right >= 0.
void SetPosPosMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64 lmax = left->Max();
  const int64 rmax = right->Max();
  if (m > CapProd(lmax, rmax)) {
    left->solver()->Fail();
  }
  if (m > CapProd(left->Min(), right->Min())) {
    // Ok for m == 0 due to left and right being positive
    if (0 != rmax) {
      left->SetMin(PosIntDivUp(m, rmax));
    }
    if (0 != lmax) {
      right->SetMin(PosIntDivUp(m, lmax));
    }
  }
}

// Propagates set_max on left * right, left and right >= 0.
void SetPosPosMaxExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64 lmin = left->Min();
  const int64 rmin = right->Min();
  if (m < CapProd(lmin, rmin)) {
    left->solver()->Fail();
  }
  if (m < CapProd(left->Max(), right->Max())) {
    if (0 != lmin) {
      right->SetMax(PosIntDivDown(m, lmin));
    }
    if (0 != rmin) {
      left->SetMax(PosIntDivDown(m, rmin));
    }
    // else do nothing: 0 is supporting any value from other expr.
  }
}

// Propagates set_min on left * right, left >= 0, right across 0.
void SetPosGenMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64 lmax = left->Max();
  const int64 rmax = right->Max();
  if (m > CapProd(lmax, rmax)) {
    left->solver()->Fail();
  }
  if (left->Max() == 0) {  // left is bound to 0, product is bound to 0.
    DCHECK_EQ(0, left->Min());
    DCHECK_LE(m, 0);
  } else {
    if (m > 0) {  // We deduce right > 0.
      left->SetMin(PosIntDivUp(m, rmax));
      right->SetMin(PosIntDivUp(m, lmax));
    } else if (m == 0) {
      const int64 lmin = left->Min();
      if (lmin > 0) {
        right->SetMin(0);
      }
    } else {  // m < 0
      const int64 lmin = left->Min();
      if (0 != lmin) {  // We cannot deduce anything if 0 is in the domain.
        right->SetMin(-PosIntDivDown(-m, lmin));
      }
    }
  }
}

// Propagates set_min on left * right, left and right across 0.
void SetGenGenMinExpr(IntExpr* const left, IntExpr* const right, int64 m) {
  DCHECK_LT(left->Min(), 0);
  DCHECK_GT(left->Max(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64 lmin = left->Min();
  const int64 lmax = left->Max();
  const int64 rmin = right->Min();
  const int64 rmax = right->Max();
  if (m > std::max(CapProd(lmin, rmin), CapProd(lmax, rmax))) {
    left->solver()->Fail();
  }
  if (m > lmin * rmin) {  // Must be positive section * positive section.
    left->SetMin(PosIntDivUp(m, rmax));
    right->SetMin(PosIntDivUp(m, lmax));
  } else if (m > CapProd(lmax, rmax)) {  // Negative section * negative section.
    left->SetMax(-PosIntDivUp(m, -rmin));
    right->SetMax(-PosIntDivUp(m, -lmin));
  }
}

void TimesSetMin(IntExpr* const left, IntExpr* const right,
                 IntExpr* const minus_left, IntExpr* const minus_right,
                 int64 m) {
  if (left->Min() >= 0) {
    if (right->Min() >= 0) {
      SetPosPosMinExpr(left, right, m);
    } else if (right->Max() <= 0) {
      SetPosPosMaxExpr(left, minus_right, -m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(left, right, m);
    }
  } else if (left->Max() <= 0) {
    if (right->Min() >= 0) {
      SetPosPosMaxExpr(right, minus_left, -m);
    } else if (right->Max() <= 0) {
      SetPosPosMinExpr(minus_left, minus_right, m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(minus_left, minus_right, m);
    }
  } else if (right->Min() >= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(right, left, m);
  } else if (right->Max() <= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(minus_right, minus_left, m);
  } else {  // left->Min() < 0 && left->Max() > 0 &&
            // right->Min() < 0 && right->Max() > 0
    SetGenGenMinExpr(left, right, m);
  }
}

class TimesIntExpr : public BaseIntExpr {
 public:
  TimesIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s),
        left_(l),
        right_(r),
        minus_left_(s->MakeOpposite(left_)),
        minus_right_(s->MakeOpposite(right_)) {}
  virtual ~TimesIntExpr() {}
  virtual int64 Min() const {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::min(std::min(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                    std::min(CapProd(lmax, rmin), CapProd(lmin, rmax)));
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::max(std::max(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                    std::max(CapProd(lmax, rmin), CapProd(lmin, rmax)));
  }
  virtual void SetMax(int64 m);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  IntExpr* const minus_left_;
  IntExpr* const minus_right_;
};

void TimesIntExpr::SetMin(int64 m) {
  if (m != kint64min) {
    TimesSetMin(left_, right_, minus_left_, minus_right_, m);
  }
}

void TimesIntExpr::SetMax(int64 m) {
  if (m != kint64max) {
    TimesSetMin(left_, minus_right_, minus_left_, right_, -m);
  }
}

bool TimesIntExpr::Bound() const {
  const bool left_bound = left_->Bound();
  const bool right_bound = right_->Bound();
  return ((left_bound && left_->Max() == 0) ||
          (right_bound && right_->Max() == 0) || (left_bound && right_bound));
}

// ----- TimesPosIntExpr -----

class TimesPosIntExpr : public BaseIntExpr {
 public:
  TimesPosIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~TimesPosIntExpr() {}
  virtual int64 Min() const { return (left_->Min() * right_->Min()); }
  virtual void SetMin(int64 m);
  virtual int64 Max() const { return (left_->Max() * right_->Max()); }
  virtual void SetMax(int64 m);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

void TimesPosIntExpr::SetMin(int64 m) { SetPosPosMinExpr(left_, right_, m); }

void TimesPosIntExpr::SetMax(int64 m) { SetPosPosMaxExpr(left_, right_, m); }

bool TimesPosIntExpr::Bound() const {
  return (left_->Max() == 0 || right_->Max() == 0 ||
          (left_->Bound() && right_->Bound()));
}

// ----- SafeTimesPosIntExpr -----

class SafeTimesPosIntExpr : public BaseIntExpr {
 public:
  SafeTimesPosIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~SafeTimesPosIntExpr() {}
  virtual int64 Min() const { return CapProd(left_->Min(), right_->Min()); }
  virtual void SetMin(int64 m) {
    if (m != kint64min) {
      SetPosPosMinExpr(left_, right_, m);
    }
  }
  virtual int64 Max() const { return CapProd(left_->Max(), right_->Max()); }
  virtual void SetMax(int64 m) {
    if (m != kint64max) {
      SetPosPosMaxExpr(left_, right_, m);
    }
  }
  virtual bool Bound() const {
    return (left_->Max() == 0 || right_->Max() == 0 ||
            (left_->Bound() && right_->Bound()));
  }
  virtual string name() const {
    return StringPrintf("(%s * %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- TimesBooleanPosIntExpr -----

class TimesBooleanPosIntExpr : public BaseIntExpr {
 public:
  TimesBooleanPosIntExpr(Solver* const s, BooleanVar* const b, IntExpr* const e)
      : BaseIntExpr(s), boolvar_(b), expr_(e) {}
  virtual ~TimesBooleanPosIntExpr() {}
  virtual int64 Min() const {
    return (boolvar_->value_ == 1 ? expr_->Min() : 0);
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    return (boolvar_->value_ == 0 ? 0 : expr_->Max());
  }
  virtual void SetMax(int64 m);
  virtual void Range(int64* mi, int64* ma);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)", boolvar_->name().c_str(),
                        expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)", boolvar_->DebugString().c_str(),
                        expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            boolvar_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

void TimesBooleanPosIntExpr::SetMin(int64 m) {
  if (m > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(m);
  }
}

void TimesBooleanPosIntExpr::SetMax(int64 m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->value_ == 1) {
    expr_->SetMax(m);
  }
}

void TimesBooleanPosIntExpr::Range(int64* mi, int64* ma) {
  const int value = boolvar_->value_;
  if (value == 0) {
    *mi = 0;
    *ma = 0;
  } else if (value == 1) {
    expr_->Range(mi, ma);
  } else {
    *mi = 0;
    *ma = expr_->Max();
  }
}

void TimesBooleanPosIntExpr::SetRange(int64 mi, int64 ma) {
  if (ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(mi);
  }
  if (ma < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->value_ == 1) {
    expr_->SetMax(ma);
  }
}

bool TimesBooleanPosIntExpr::Bound() const {
  return (boolvar_->value_ == 0 || expr_->Max() == 0 ||
          (boolvar_->value_ != kUnboundBooleanVarValue && expr_->Bound()));
}

// ----- TimesBooleanIntExpr -----

class TimesBooleanIntExpr : public BaseIntExpr {
 public:
  TimesBooleanIntExpr(Solver* const s, BooleanVar* const b, IntExpr* const e)
      : BaseIntExpr(s), boolvar_(b), expr_(e) {}
  virtual ~TimesBooleanIntExpr() {}
  virtual int64 Min() const {
    switch (boolvar_->value_) {
      case 0: { return 0LL; }
      case 1: { return expr_->Min(); }
      default: {
        DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
        return std::min(0LL, expr_->Min());
      }
    }
  }
  virtual void SetMin(int64 m);
  virtual int64 Max() const {
    switch (boolvar_->value_) {
      case 0: { return 0LL; }
      case 1: { return expr_->Max(); }
      default: {
        DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
        return std::max(0LL, expr_->Max());
      }
    }
  }
  virtual void SetMax(int64 m);
  virtual void Range(int64* mi, int64* ma);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("(%s * %s)", boolvar_->name().c_str(),
                        expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s * %s)", boolvar_->DebugString().c_str(),
                        expr_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                            boolvar_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
  }

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

void TimesBooleanIntExpr::SetMin(int64 m) {
  switch (boolvar_->value_) {
    case 0: {
      if (m > 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMin(m);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (m > 0) {  // 0 is no longer possible for boolvar because min > 0.
        boolvar_->SetValue(1);
        expr_->SetMin(m);
      } else if (m <= 0 && expr_->Max() < m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::SetMax(int64 m) {
  switch (boolvar_->value_) {
    case 0: {
      if (m < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMax(m);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (m < 0) {  // 0 is no longer possible for boolvar because max < 0.
        boolvar_->SetValue(1);
        expr_->SetMax(m);
      } else if (m >= 0 && expr_->Min() > m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::Range(int64* mi, int64* ma) {
  switch (boolvar_->value_) {
    case 0: {
      *mi = 0;
      *ma = 0;
      break;
    }
    case 1: {
      *mi = expr_->Min();
      *ma = expr_->Max();
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      *mi = std::min(0LL, expr_->Min());
      *ma = std::max(0LL, expr_->Max());
      break;
    }
  }
}

void TimesBooleanIntExpr::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  switch (boolvar_->value_) {
    case 0: {
      if (mi > 0 || ma < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetRange(mi, ma);
      break;
    }
    default: {
      DCHECK_EQ(kUnboundBooleanVarValue, boolvar_->value_);
      if (mi > 0) {
        boolvar_->SetValue(1);
        expr_->SetMin(mi);
      } else if (mi == 0 && expr_->Max() < 0) {
        boolvar_->SetValue(0);
      }
      if (ma < 0) {
        boolvar_->SetValue(1);
        expr_->SetMax(ma);
      } else if (ma == 0 && expr_->Min() > 0) {
        boolvar_->SetValue(0);
      }
      break;
    }
  }
}

bool TimesBooleanIntExpr::Bound() const {
  return (boolvar_->value_ == 0 ||
          (expr_->Bound() &&
           (boolvar_->value_ != kUnboundBooleanVarValue || expr_->Max() == 0)));
}

// ----- DivPosIntCstExpr -----

class DivPosIntCstExpr : public BaseIntExpr {
 public:
  DivPosIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {
    CHECK_GE(v, 0);
  }
  virtual ~DivPosIntCstExpr() {}

  virtual int64 Min() const { return expr_->Min() / value_; }

  virtual void SetMin(int64 m) {
    if (m > 0) {
      expr_->SetMin(m * value_);
    } else {
      expr_->SetMin((m - 1) * value_ + 1);
    }
  }
  virtual int64 Max() const { return expr_->Max() / value_; }

  virtual void SetMax(int64 m) {
    if (m >= 0) {
      expr_->SetMax((m + 1) * value_ - 1);
    } else {
      expr_->SetMax(m * value_);
    }
  }

  virtual string name() const {
    return StringPrintf("(%s div %" GG_LL_FORMAT "d)", expr_->name().c_str(),
                        value_);
  }

  virtual string DebugString() const {
    return StringPrintf("(%s div %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// DivPosIntExpr

class DivPosIntExpr : public BaseIntExpr {
 public:
  DivPosIntExpr(Solver* const s, IntExpr* const num, IntExpr* const denom)
      : BaseIntExpr(s),
        num_(num),
        denom_(denom),
        opp_num_(s->MakeOpposite(num)) {}

  virtual ~DivPosIntExpr() {}

  virtual int64 Min() const {
    return num_->Min() >= 0
               ? num_->Min() / denom_->Max()
               : (denom_->Min() == 0 ? num_->Min()
                                     : num_->Min() / denom_->Min());
  }

  virtual int64 Max() const {
    return num_->Max() >= 0 ? (denom_->Min() == 0 ? num_->Max()
                                                  : num_->Max() / denom_->Min())
                            : num_->Max() / denom_->Max();
  }

  static void SetPosMin(IntExpr* const num, IntExpr* const denom, int64 m) {
    num->SetMin(m * denom->Min());
    denom->SetMax(num->Max() / m);
  }

  static void SetPosMax(IntExpr* const num, IntExpr* const denom, int64 m) {
    num->SetMax((m + 1) * denom->Max() - 1);
    denom->SetMin(num->Min() / (m + 1) + 1);
  }

  virtual void SetMin(int64 m) {
    if (m > 0) {
      SetPosMin(num_, denom_, m);
    } else {
      SetPosMax(opp_num_, denom_, -m);
    }
  }

  virtual void SetMax(int64 m) {
    if (m >= 0) {
      SetPosMax(num_, denom_, m);
    } else {
      SetPosMin(opp_num_, denom_, -m);
    }
  }

  virtual string name() const {
    return StringPrintf("(%s div %s)", num_->name().c_str(),
                        denom_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s div %s)", num_->DebugString().c_str(),
                        denom_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            denom_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
  }

 private:
  IntExpr* const num_;
  IntExpr* const denom_;
  IntExpr* const opp_num_;
};

class DivPosPosIntExpr : public BaseIntExpr {
 public:
  DivPosPosIntExpr(Solver* const s, IntExpr* const num, IntExpr* const denom)
      : BaseIntExpr(s), num_(num), denom_(denom) {}

  virtual ~DivPosPosIntExpr() {}

  virtual int64 Min() const { return num_->Min() / denom_->Max(); }

  virtual int64 Max() const {
    if (denom_->Min() == 0) {
      return num_->Max();
    } else {
      return num_->Max() / denom_->Min();
    }
  }

  virtual void SetMin(int64 m) {
    if (m > 0) {
      num_->SetMin(m * denom_->Min());
      denom_->SetMax(num_->Max() / m);
    }
  }

  virtual void SetMax(int64 m) {
    if (m >= 0) {
      num_->SetMax((m + 1) * denom_->Max() - 1);
      denom_->SetMin(num_->Min() / (m + 1) + 1);
    } else {
      solver()->Fail();
    }
  }

  virtual string name() const {
    return StringPrintf("(%s div %s)", num_->name().c_str(),
                        denom_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("(%s div %s)", num_->DebugString().c_str(),
                        denom_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            denom_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
  }

 private:
  IntExpr* const num_;
  IntExpr* const denom_;
};

// DivIntExpr

class DivIntExpr : public BaseIntExpr {
 public:
  DivIntExpr(Solver* const s, IntExpr* const num, IntExpr* const denom)
      : BaseIntExpr(s),
        num_(num),
        denom_(denom),
        opp_num_(s->MakeOpposite(num)) {}

  virtual ~DivIntExpr() {}

  virtual int64 Min() const {
    const int64 num_min = num_->Min();
    const int64 num_max = num_->Max();
    const int64 denom_min = denom_->Min();
    const int64 denom_max = denom_->Max();

    if (denom_min == 0 && denom_max == 0) {
      return kint64max;  // TODO(user): Check this convention.
    }

    if (denom_min >= 0) {  // Denominator strictly positive.
      DCHECK_GT(denom_max, 0);
      const int64 adjusted_denom_min = denom_min == 0 ? 1 : denom_min;
      return num_min >= 0 ? num_min / denom_max : num_min / adjusted_denom_min;
    } else if (denom_max <= 0) {  // Denominator strictly negative.
      DCHECK_LT(denom_min, 0);
      const int64 adjusted_denom_max = denom_max == 0 ? -1 : denom_max;
      return num_max >= 0 ? num_max / adjusted_denom_max : num_max / denom_min;
    } else {  // Denominator across 0.
      return std::min(num_min, -num_max);
    }
  }

  virtual int64 Max() const {
    const int64 num_min = num_->Min();
    const int64 num_max = num_->Max();
    const int64 denom_min = denom_->Min();
    const int64 denom_max = denom_->Max();

    if (denom_min == 0 && denom_max == 0) {
      return kint64min;  // TODO(user): Check this convention.
    }

    if (denom_min >= 0) {  // Denominator strictly positive.
      DCHECK_GT(denom_max, 0);
      const int64 adjusted_denom_min = denom_min == 0 ? 1 : denom_min;
      return num_max >= 0 ? num_max / adjusted_denom_min : num_max / denom_max;
    } else if (denom_max <= 0) {  // Denominator strictly negative.
      DCHECK_LT(denom_min, 0);
      const int64 adjusted_denom_max = denom_max == 0 ? -1 : denom_max;
      return num_min >= 0 ? num_min / denom_min
                          : -num_min / -adjusted_denom_max;
    } else {  // Denominator across 0.
      return std::max(num_max, -num_min);
    }
  }

  void AdjustDenominator() {
    if (denom_->Min() == 0) {
      denom_->SetMin(1);
    } else if (denom_->Max() == 0) {
      denom_->SetMax(-1);
    }
  }

  // m > 0.
  static void SetPosMin(IntExpr* const num, IntExpr* const denom, int64 m) {
    DCHECK_GT(m, 0);
    const int64 num_min = num->Min();
    const int64 num_max = num->Max();
    const int64 denom_min = denom->Min();
    const int64 denom_max = denom->Max();
    DCHECK_NE(denom_min, 0);
    DCHECK_NE(denom_max, 0);
    if (denom_min > 0) {  // Denominator strictly positive.
      num->SetMin(m * denom_min);
      denom->SetMax(num_max / m);
    } else if (denom_max < 0) {  // Denominator strictly negative.
      num->SetMax(m * denom_max);
      denom->SetMin(num_min / m);
    } else {  // Denominator across 0.
      if (num_min >= 0) {
        num->SetMin(m);
        denom->SetRange(1, num_max / m);
      } else if (num_max <= 0) {
        num->SetMax(-m);
        denom->SetRange(num_min / m, -1);
      } else {
        if (m > -num_min) {  // Denominator is forced positive.
          num->SetMin(m);
          denom->SetRange(1, num_max / m);
        } else if (m > num_max) {  // Denominator is forced negative.
          num->SetMax(-m);
          denom->SetRange(num_min / m, -1);
        } else {
          denom->SetRange(num_min / m, num_max / m);
        }
      }
    }
  }

  // m >= 0.
  static void SetPosMax(IntExpr* const num, IntExpr* const denom, int64 m) {
    DCHECK_GE(m, 0);
    const int64 num_min = num->Min();
    const int64 num_max = num->Max();
    const int64 denom_min = denom->Min();
    const int64 denom_max = denom->Max();
    DCHECK_NE(denom_min, 0);
    DCHECK_NE(denom_max, 0);
    if (denom_min > 0) {  // Denominator strictly positive.
      num->SetMax((m + 1) * denom_max - 1);
      denom->SetMin((num_min / (m + 1)) + 1);
    } else if (denom_max < 0) {
      num->SetMin((m + 1) * denom_min + 1);
      denom->SetMax(num_max / (m + 1) - 1);
    } else if (num_min > (m + 1) * denom_max - 1) {
      denom->SetMax(-1);
    } else if (num_max < (m + 1) * denom_min + 1) {
      denom->SetMin(1);
    }
  }

  virtual void SetMin(int64 m) {
    AdjustDenominator();
    if (m > 0) {
      SetPosMin(num_, denom_, m);
    } else {
      SetPosMax(opp_num_, denom_, -m);
    }
  }

  virtual void SetMax(int64 m) {
    AdjustDenominator();
    if (m >= 0) {
      SetPosMax(num_, denom_, m);
    } else {
      SetPosMin(opp_num_, denom_, -m);
    }
  }

  virtual string name() const {
    return StringPrintf("(%s div %s)", num_->name().c_str(),
                        denom_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("(%s div %s)", num_->DebugString().c_str(),
                        denom_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            denom_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
  }

 private:
  IntExpr* const num_;
  IntExpr* const denom_;
  IntExpr* const opp_num_;
};

// ----- IntAbs And IntAbsConstraint ------

class IntAbsConstraint : public CastConstraint {
 public:
  IntAbsConstraint(Solver* const s, IntVar* const sub, IntVar* const target)
      : CastConstraint(s, target), sub_(sub) {}

  virtual ~IntAbsConstraint() {}

  virtual void Post() {
    Demon* const sub_demon = MakeConstraintDemon0(
        solver(), this, &IntAbsConstraint::PropagateSub, "PropagateSub");
    sub_->WhenRange(sub_demon);
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &IntAbsConstraint::PropagateTarget, "PropagateTarget");
    target_var_->WhenRange(target_demon);
  }

  virtual void InitialPropagate() {
    PropagateSub();
    PropagateTarget();
  }

  void PropagateSub() {
    int64 smin = 0;
    int64 smax = 0;
    sub_->Range(&smin, &smax);
    if (smax <= 0) {
      target_var_->SetRange(-smax, -smin);
    } else if (smin >= 0) {
      target_var_->SetRange(smin, smax);
    } else {
      target_var_->SetRange(0, std::max(-smin, smax));
    }
  }

  void PropagateTarget() {
    const int64 target_min = std::max(target_var_->Min(), 0LL);
    const int64 target_max = target_var_->Max();
    int64 smin = 0;
    int64 smax = 0;
    sub_->Range(&smin, &smax);
    if (smax < target_min || smax == 0) {
      sub_->SetRange(-target_max, -target_min);
    } else if (smin > -target_min || smin == 0) {
      sub_->SetRange(target_min, target_max);
    } else {
      sub_->SetRange(-target_max, target_max);
    }
  }

  virtual string DebugString() const {
    return StringPrintf("IntAbsConstraint(%s, %s)", sub_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAbsEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            sub_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kAbsEqual, this);
  }

 private:
  IntVar* const sub_;
};

class IntAbs : public BaseIntExpr {
 public:
  IntAbs(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}

  virtual ~IntAbs() {}

  virtual int64 Min() const {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin >= 0) {
      return emin;
    }
    if (emax <= 0) {
      return -emax;
    }
    return 0;
  }

  virtual void SetMin(int64 m) {
    if (m > 0) {
      int64 emin = 0;
      int64 emax = 0;
      expr_->Range(&emin, &emax);
      if (emin > -m) {
        expr_->SetMin(m);
      } else if (emax < m) {
        expr_->SetMax(-m);
      }
    }
  }

  virtual int64 Max() const {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin >= 0) {
      return emax;
    }
    if (emax <= 0) {
      return -emin;
    }
    return std::max(-emin, emax);
  }

  virtual void SetMax(int64 m) { expr_->SetRange(-m, m); }

  virtual void SetRange(int64 mi, int64 ma) {
    expr_->SetRange(-ma, ma);
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin > -mi) {
      expr_->SetMin(mi);
    } else if (emax < mi) {
      expr_->SetMax(-mi);
    }
  }

  virtual void Range(int64* mi, int64* ma) {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin >= 0) {
      *mi = emin;
      *ma = emax;
    } else if (emax <= 0) {
      *mi = -emax;
      *ma = -emin;
    } else {
      *mi = 0;
      *ma = std::max(-emin, emax);
    }
  }

  virtual bool Bound() const { return expr_->Bound(); }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual string name() const {
    return StringPrintf("IntAbs(%s)", expr_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("IntAbs(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kAbs, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kAbs, this);
  }

  virtual IntVar* CastToVar() {
    int64 min_value = 0;
    int64 max_value = 0;
    Range(&min_value, &max_value);
    Solver* const s = solver();
    const string name = StringPrintf("AbsVar(%s)", expr_->name().c_str());
    IntVar* const target = s->MakeIntVar(min_value, max_value, name);
    CastConstraint* const ct =
        s->RevAlloc(new IntAbsConstraint(s, expr_->Var(), target));
    s->AddCastConstraint(ct, target, this);
    return target;
  }

 private:
  IntExpr* const expr_;
};

// ----- Square -----

// TODO(user): shouldn't we compare to kint32max^2 instead of kint64max?
class IntSquare : public BaseIntExpr {
 public:
  IntSquare(Solver* const s, IntExpr* const e) : BaseIntExpr(s), expr_(e) {}
  virtual ~IntSquare() {}

  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    if (emin >= 0) {
      return emin >= kint32max ? kint64max : emin * emin;
    }
    const int64 emax = expr_->Max();
    if (emax < 0) {
      return emax <= -kint32max ? kint64max : emax * emax;
    }
    return 0LL;
  }
  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    // TODO(user): What happens if m is kint64max?
    const int64 emin = expr_->Min();
    const int64 emax = expr_->Max();
    const int64 root = static_cast<int64>(ceil(sqrt(static_cast<double>(m))));
    if (emin >= 0) {
      expr_->SetMin(root);
    } else if (emax <= 0) {
      expr_->SetMax(-root);
    } else if (expr_->IsVar()) {
      reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-root + 1, root - 1);
    }
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    const int64 emin = expr_->Min();
    if (emax >= kint32max || emin <= -kint32max) {
      return kint64max;
    }
    return std::max(emin * emin, emax * emax);
  }
  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    const int64 root = static_cast<int64>(floor(sqrt(static_cast<double>(m))));
    expr_->SetRange(-root, root);
  }
  virtual bool Bound() const { return expr_->Bound(); }
  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }
  virtual string name() const {
    return StringPrintf("IntSquare(%s)", expr_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("IntSquare(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSquare, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSquare, this);
  }

  IntExpr* expr() const { return expr_; }

 protected:
  IntExpr* const expr_;
};

class PosIntSquare : public IntSquare {
 public:
  PosIntSquare(Solver* const s, IntExpr* const e) : IntSquare(s, e) {}
  virtual ~PosIntSquare() {}

  virtual int64 Min() const {
    const int64 emin = expr_->Min();
    return emin >= kint32max ? kint64max : emin * emin;
  }
  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    const int64 root = static_cast<int64>(ceil(sqrt(static_cast<double>(m))));
    expr_->SetMin(root);
  }
  virtual int64 Max() const {
    const int64 emax = expr_->Max();
    return emax >= kint32max ? kint64max : emax * emax;
  }
  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    const int64 root = static_cast<int64>(floor(sqrt(static_cast<double>(m))));
    expr_->SetMax(root);
  }
};

// ----- EvenPower -----

int64 IntPower(int64 value, int64 power) {
  int64 result = value;
  // TODO(user): Speed that up.
  for (int i = 1; i < power; ++i) {
    result *= value;
  }
  return result;
}

int64 OverflowLimit(int64 power) {
  return static_cast<int64>(
      floor(exp(log(static_cast<double>(kint64max)) / power)));
}

class BasePower : public BaseIntExpr {
 public:
  BasePower(Solver* const s, IntExpr* const e, int64 n)
      : BaseIntExpr(s), expr_(e), pow_(n), limit_(OverflowLimit(n)) {
    CHECK_GT(n, 0);
  }

  virtual ~BasePower() {}

  virtual bool Bound() const { return expr_->Bound(); }

  IntExpr* expr() const { return expr_; }

  int64 exponant() const { return pow_; }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual string name() const {
    return StringPrintf("IntPower(%s, %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), pow_);
  }

  virtual string DebugString() const {
    return StringPrintf("IntPower(%s, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), pow_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kPower, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, pow_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kPower, this);
  }

 protected:
  int64 Pown(int64 value) const {
    if (value >= limit_) {
      return kint64max;
    }
    if (value <= -limit_) {
      if (pow_ % 2 == 0) {
        return kint64max;
      } else {
        return kint64min;
      }
    }
    return IntPower(value, pow_);
  }

  int64 SqrnDown(int64 value) const {
    if (value == kint64min) {
      return kint64min;
    }
    if (value == kint64max) {
      return kint64max;
    }
    int64 res = 0;
    const double d_value = static_cast<double>(value);
    if (value >= 0) {
      const double sq = exp(log(d_value) / pow_);
      res = static_cast<int64>(floor(sq));
    } else {
      CHECK_EQ(1, pow_ % 2);
      const double sq = exp(log(-d_value) / pow_);
      res = -static_cast<int64>(ceil(sq));
    }
    const int64 pow_res = Pown(res + 1);
    if (pow_res <= value) {
      return res + 1;
    } else {
      return res;
    }
  }

  int64 SqrnUp(int64 value) const {
    if (value == kint64min) {
      return kint64min;
    }
    if (value == kint64max) {
      return kint64max;
    }
    int64 res = 0;
    const double d_value = static_cast<double>(value);
    if (value >= 0) {
      const double sq = exp(log(d_value) / pow_);
      res = static_cast<int64>(ceil(sq));
    } else {
      CHECK_EQ(1, pow_ % 2);
      const double sq = exp(log(-d_value) / pow_);
      res = -static_cast<int64>(floor(sq));
    }
    const int64 pow_res = Pown(res - 1);
    if (pow_res >= value) {
      return res - 1;
    } else {
      return res;
    }
  }

  IntExpr* const expr_;
  const int64 pow_;
  const int64 limit_;
};

class IntEvenPower : public BasePower {
 public:
  IntEvenPower(Solver* const s, IntExpr* const e, int64 n)
      : BasePower(s, e, n) {
    CHECK_EQ(0, n % 2);
  }

  virtual ~IntEvenPower() {}

  virtual int64 Min() const {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin >= 0) {
      return Pown(emin);
    }
    if (emax < 0) {
      return Pown(emax);
    }
    return 0LL;
  }
  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    const int64 root = SqrnUp(m);
    if (emin > -root) {
      expr_->SetMin(root);
    } else if (emax < root) {
      expr_->SetMax(-root);
    } else if (expr_->IsVar()) {
      reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-root + 1, root - 1);
    }
  }

  virtual int64 Max() const {
    return std::max(Pown(expr_->Min()), Pown(expr_->Max()));
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    const int64 root = SqrnDown(m);
    expr_->SetRange(-root, root);
  }
};

class PosIntEvenPower : public BasePower {
 public:
  PosIntEvenPower(Solver* const s, IntExpr* const e, int64 pow)
      : BasePower(s, e, pow) {
    CHECK_EQ(0, pow % 2);
  }

  virtual ~PosIntEvenPower() {}

  virtual int64 Min() const { return Pown(expr_->Min()); }

  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    expr_->SetMin(SqrnUp(m));
  }
  virtual int64 Max() const { return Pown(expr_->Max()); }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    expr_->SetMax(SqrnDown(m));
  }
};

class IntOddPower : public BasePower {
 public:
  IntOddPower(Solver* const s, IntExpr* const e, int64 n) : BasePower(s, e, n) {
    CHECK_EQ(1, n % 2);
  }

  virtual ~IntOddPower() {}

  virtual int64 Min() const { return Pown(expr_->Min()); }

  virtual void SetMin(int64 m) { expr_->SetMin(SqrnUp(m)); }

  virtual int64 Max() const { return Pown(expr_->Max()); }

  virtual void SetMax(int64 m) { expr_->SetMax(SqrnDown(m)); }
};

// ----- Min(expr, expr) -----

class MinIntExpr : public BaseIntExpr {
 public:
  MinIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  virtual ~MinIntExpr() {}
  virtual int64 Min() const {
    const int64 lmin = left_->Min();
    const int64 rmin = right_->Min();
    return std::min(lmin, rmin);
  }
  virtual void SetMin(int64 m) {
    left_->SetMin(m);
    right_->SetMin(m);
  }
  virtual int64 Max() const {
    const int64 lmax = left_->Max();
    const int64 rmax = right_->Max();
    return std::min(lmax, rmax);
  }
  virtual void SetMax(int64 m) {
    if (left_->Min() > m) {
      right_->SetMax(m);
    }
    if (right_->Min() > m) {
      left_->SetMax(m);
    }
  }
  virtual string name() const {
    return StringPrintf("MinIntExpr(%s, %s)", left_->name().c_str(),
                        right_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("MinIntExpr(%s, %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Min(expr, constant) -----

class MinCstIntExpr : public BaseIntExpr {
 public:
  MinCstIntExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}

  virtual ~MinCstIntExpr() {}

  virtual int64 Min() const { return std::min(expr_->Min(), value_); }

  virtual void SetMin(int64 m) {
    if (m > value_) {
      solver()->Fail();
    }
    expr_->SetMin(m);
  }

  virtual int64 Max() const { return std::min(expr_->Max(), value_); }

  virtual void SetMax(int64 m) {
    if (value_ > m) {
      expr_->SetMax(m);
    }
  }

  virtual bool Bound() const {
    return (expr_->Bound() || expr_->Min() >= value_);
  }

  virtual string name() const {
    return StringPrintf("MinCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }

  virtual string DebugString() const {
    return StringPrintf("MinCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- Max(expr, expr) -----

class MaxIntExpr : public BaseIntExpr {
 public:
  MaxIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}

  virtual ~MaxIntExpr() {}

  virtual int64 Min() const { return std::max(left_->Min(), right_->Min()); }

  virtual void SetMin(int64 m) {
    if (left_->Max() < m) {
      right_->SetMin(m);
    } else {
      if (right_->Max() < m) {
        left_->SetMin(m);
      }
    }
  }

  virtual int64 Max() const { return std::max(left_->Max(), right_->Max()); }

  virtual void SetMax(int64 m) {
    left_->SetMax(m);
    right_->SetMax(m);
  }

  virtual string name() const {
    return StringPrintf("MaxIntExpr(%s, %s)", left_->name().c_str(),
                        right_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("MaxIntExpr(%s, %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Max(expr, constant) -----

class MaxCstIntExpr : public BaseIntExpr {
 public:
  MaxCstIntExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {}

  virtual ~MaxCstIntExpr() {}

  virtual int64 Min() const { return std::max(expr_->Min(), value_); }

  virtual void SetMin(int64 m) {
    if (value_ < m) {
      expr_->SetMin(m);
    }
  }

  virtual int64 Max() const { return std::max(expr_->Max(), value_); }

  virtual void SetMax(int64 m) {
    if (m < value_) {
      solver()->Fail();
    }
    expr_->SetMax(m);
  }

  virtual bool Bound() const {
    return (expr_->Bound() || expr_->Max() <= value_);
  }

  virtual string name() const {
    return StringPrintf("MaxCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), value_);
  }

  virtual string DebugString() const {
    return StringPrintf("MaxCstIntExpr(%s, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), value_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
  }

 private:
  IntExpr* const expr_;
  const int64 value_;
};

// ----- Convex Piecewise -----

// This class is a very simple convex piecewise linear function.  The
// argument of the function is the expression.  Between early_date and
// late_date, the value of the function is 0.  Before early date, it
// is affine and the cost is early_cost * (early_date - x). After
// late_date, the cost is late_cost * (x - late_date).

class SimpleConvexPiecewiseExpr : public BaseIntExpr {
 public:
  SimpleConvexPiecewiseExpr(Solver* const s, IntExpr* const e, int64 ec,
                            int64 ed, int64 ld, int64 lc)
      : BaseIntExpr(s),
        expr_(e),
        early_cost_(ec),
        early_date_(ec == 0 ? kint64min : ed),
        late_date_(lc == 0 ? kint64max : ld),
        late_cost_(lc) {
    DCHECK_GE(ec, 0LL);
    DCHECK_GE(lc, 0LL);
    DCHECK_GE(ld, ed);

    // If the penalty is 0, we can push the "confort zone or zone
    // of no cost towards infinity.
  }

  virtual ~SimpleConvexPiecewiseExpr() {}

  virtual int64 Min() const {
    const int64 vmin = expr_->Min();
    const int64 vmax = expr_->Max();
    if (vmin >= late_date_) {
      return (vmin - late_date_) * late_cost_;
    } else if (vmax <= early_date_) {
      return (early_date_ - vmax) * early_cost_;
    } else {
      return 0LL;
    }
  }

  virtual void SetMin(int64 m) {
    if (m <= 0) {
      return;
    }
    int64 vmin = 0;
    int64 vmax = 0;
    expr_->Range(&vmin, &vmax);

    const int64 rb =
        (late_cost_ == 0 ? vmax : late_date_ + PosIntDivUp(m, late_cost_) - 1);
    const int64 lb =
        (early_cost_ == 0 ? vmin
                          : early_date_ - PosIntDivUp(m, early_cost_) + 1);

    if (expr_->IsVar()) {
      expr_->Var()->RemoveInterval(lb, rb);
    }
  }

  virtual int64 Max() const {
    const int64 vmin = expr_->Min();
    const int64 vmax = expr_->Max();
    const int64 mr = vmax > late_date_ ? (vmax - late_date_) * late_cost_ : 0;
    const int64 ml =
        vmin < early_date_ ? (early_date_ - vmin) * early_cost_ : 0;
    return std::max(mr, ml);
  }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (late_cost_ != 0LL) {
      const int64 rb = late_date_ + PosIntDivDown(m, late_cost_);
      if (early_cost_ != 0LL) {
        const int64 lb = early_date_ - PosIntDivDown(m, early_cost_);
        expr_->SetRange(lb, rb);
      } else {
        expr_->SetMax(rb);
      }
    } else {
      if (early_cost_ != 0LL) {
        const int64 lb = early_date_ - PosIntDivDown(m, early_cost_);
        expr_->SetMin(lb);
      }
    }
  }

  virtual string name() const {
    return StringPrintf("ConvexPiecewiseExpr(%s, ec = %" GG_LL_FORMAT
                        "d, ed = %" GG_LL_FORMAT "d, ld = %" GG_LL_FORMAT
                        "d, lc = %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), early_cost_, early_date_,
                        late_date_, late_cost_);
  }

  virtual string DebugString() const {
    return StringPrintf("ConvexPiecewiseExpr(%s, ec = %" GG_LL_FORMAT
                        "d, ed = %" GG_LL_FORMAT "d, ld = %" GG_LL_FORMAT
                        "d, lc = %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), early_cost_, early_date_,
                        late_date_, late_cost_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kEarlyCostArgument,
                                  early_cost_);
    visitor->VisitIntegerArgument(ModelVisitor::kEarlyDateArgument,
                                  early_date_);
    visitor->VisitIntegerArgument(ModelVisitor::kLateCostArgument, late_cost_);
    visitor->VisitIntegerArgument(ModelVisitor::kLateDateArgument, late_date_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
  }

 private:
  IntExpr* const expr_;
  const int64 early_cost_;
  const int64 early_date_;
  const int64 late_date_;
  const int64 late_cost_;
};

// ----- Semi Continuous -----

class SemiContinuousExpr : public BaseIntExpr {
 public:
  SemiContinuousExpr(Solver* const s, IntExpr* const e, int64 fixed_charge,
                     int64 step)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge), step_(step) {
    DCHECK_GE(fixed_charge, 0LL);
    DCHECK_GT(step, 0LL);
  }

  virtual ~SemiContinuousExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return CapAdd(fixed_charge_, CapProd(x, step_));
    }
  }

  virtual int64 Min() const { return Value(expr_->Min()); }

  virtual void SetMin(int64 m) {
    if (m >= CapAdd(fixed_charge_, step_)) {
      const int64 y = PosIntDivUp(CapSub(m, fixed_charge_), step_);
      expr_->SetMin(y);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const { return Value(expr_->Max()); }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    if (m < CapAdd(fixed_charge_, step_)) {
      expr_->SetMax(0);
    } else {
      const int64 y = PosIntDivDown(CapSub(m, fixed_charge_), step_);
      expr_->SetMax(y);
    }
  }

  virtual string name() const {
    return StringPrintf("SemiContinuous(%s, fixed_charge = %" GG_LL_FORMAT
                        "d, step = %" GG_LL_FORMAT "d)",
                        expr_->name().c_str(), fixed_charge_, step_);
  }

  virtual string DebugString() const {
    return StringPrintf("SemiContinuous(%s, fixed_charge = %" GG_LL_FORMAT
                        "d, step = %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), fixed_charge_, step_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, step_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
  const int64 step_;
};

class SemiContinuousStepOneExpr : public BaseIntExpr {
 public:
  SemiContinuousStepOneExpr(Solver* const s, IntExpr* const e,
                            int64 fixed_charge)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
    DCHECK_GE(fixed_charge, 0LL);
  }

  virtual ~SemiContinuousStepOneExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_ + x;
    }
  }

  virtual int64 Min() const { return Value(expr_->Min()); }

  virtual void SetMin(int64 m) {
    if (m >= fixed_charge_ + 1) {
      expr_->SetMin(m - fixed_charge_);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const { return Value(expr_->Max()); }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_ + 1) {
      expr_->SetMax(0);
    } else {
      expr_->SetMax(m - fixed_charge_);
    }
  }

  virtual string name() const {
    return StringPrintf(
        "SemiContinuousStepOne(%s, fixed_charge = %" GG_LL_FORMAT "d)",
        expr_->name().c_str(), fixed_charge_);
  }

  virtual string DebugString() const {
    return StringPrintf(
        "SemiContinuousStepOne(%s, fixed_charge = %" GG_LL_FORMAT "d)",
        expr_->DebugString().c_str(), fixed_charge_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 1);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
};

class SemiContinuousStepZeroExpr : public BaseIntExpr {
 public:
  SemiContinuousStepZeroExpr(Solver* const s, IntExpr* const e,
                             int64 fixed_charge)
      : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
    DCHECK_GT(fixed_charge, 0LL);
  }

  virtual ~SemiContinuousStepZeroExpr() {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_;
    }
  }

  virtual int64 Min() const { return Value(expr_->Min()); }

  virtual void SetMin(int64 m) {
    if (m >= fixed_charge_) {
      solver()->Fail();
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  virtual int64 Max() const { return Value(expr_->Max()); }

  virtual void SetMax(int64 m) {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_) {
      expr_->SetMax(0);
    }
  }

  virtual string name() const {
    return StringPrintf(
        "SemiContinuousStepZero(%s, fixed_charge = %" GG_LL_FORMAT "d)",
        expr_->name().c_str(), fixed_charge_);
  }

  virtual string DebugString() const {
    return StringPrintf(
        "SemiContinuousStepZero(%s, fixed_charge = %" GG_LL_FORMAT "d)",
        expr_->DebugString().c_str(), fixed_charge_);
  }

  virtual void WhenRange(Demon* d) { expr_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                  fixed_charge_);
    visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 0);
    visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  }

 private:
  IntExpr* const expr_;
  const int64 fixed_charge_;
};

// This constraints links an expression and the variable it is casted into
class LinkExprAndVar : public CastConstraint {
 public:
  LinkExprAndVar(Solver* const s, IntExpr* const expr, IntVar* const var)
      : CastConstraint(s, var), expr_(expr) {}

  virtual ~LinkExprAndVar() {}

  virtual void Post() {
    Solver* const s = solver();
    Demon* d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    target_var_->WhenRange(d);
  }

  virtual void InitialPropagate() {
    expr_->SetRange(target_var_->Min(), target_var_->Max());
    int64 l, u;
    expr_->Range(&l, &u);
    target_var_->SetRange(l, u);
  }

  virtual string DebugString() const {
    return StringPrintf("cast(%s, %s)", expr_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLinkExprVar, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kLinkExprVar, this);
  }

 private:
  IntExpr* const expr_;
};

// ----- This is a specialized case when the variable exact type is known -----
class LinkExprAndDomainIntVar : public CastConstraint {
 public:
  LinkExprAndDomainIntVar(Solver* const s, IntExpr* const expr,
                          DomainIntVar* const var)
      : CastConstraint(s, var),
        expr_(expr),
        cached_min_(kint64min),
        cached_max_(kint64max),
        fail_stamp_(GG_ULONGLONG(0)) {}

  virtual ~LinkExprAndDomainIntVar() {}

  DomainIntVar* var() const {
    return reinterpret_cast<DomainIntVar*>(target_var_);
  }

  virtual void Post() {
    Solver* const s = solver();
    Demon* const d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    Demon* const target_var_demon = MakeConstraintDemon0(
        solver(), this, &LinkExprAndDomainIntVar::Propagate, "Propagate");
    target_var_->WhenRange(target_var_demon);
  }

  virtual void InitialPropagate() {
    expr_->SetRange(var()->min_.Value(), var()->max_.Value());
    expr_->Range(&cached_min_, &cached_max_);
    var()->DomainIntVar::SetRange(cached_min_, cached_max_);
  }

  void Propagate() {
    if (var()->min_.Value() > cached_min_ ||
        var()->max_.Value() < cached_max_ ||
        solver()->fail_stamp() != fail_stamp_) {
      InitialPropagate();
      fail_stamp_ = solver()->fail_stamp();
    }
  }

  virtual string DebugString() const {
    return StringPrintf("cast(%s, %s)", expr_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLinkExprVar, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kLinkExprVar, this);
  }

 private:
  IntExpr* const expr_;
  int64 cached_min_;
  int64 cached_max_;
  uint64 fail_stamp_;
};

// ----- Utilities -----

// Variable-based queue cleaner. It is used to put a domain int var in
// a clean state after a failure occuring during its process() method.
class VariableQueueCleaner : public Action {
 public:
  VariableQueueCleaner() : var_(nullptr) {}

  virtual ~VariableQueueCleaner() {}

  virtual void Run(Solver* const solver) {
    DCHECK(var_ != nullptr);
    var_->ClearInProcess();
  }

  void set_var(DomainIntVar* const var) { var_ = var; }

 private:
  DomainIntVar* var_;
};

}  //  namespace

Action* NewDomainIntVarCleaner() { return new VariableQueueCleaner; }

Constraint* SetIsEqual(IntVar* const var, const std::vector<int64>& values,
                       const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsEqual(values, vars);
}

Constraint* SetIsGreaterOrEqual(IntVar* const var, const std::vector<int64>& values,
                                const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsGreaterOrEqual(values, vars);
}

void Solver::set_queue_cleaner_on_fail(IntVar* const var) {
  DCHECK_EQ(DOMAIN_INT_VAR, var->VarType());
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  VariableQueueCleaner* const cleaner =
      reinterpret_cast<VariableQueueCleaner*>(variable_cleaner_.get());
  cleaner->set_var(dvar);
  set_queue_action_on_fail(cleaner);
}

void RestoreBoolValue(IntVar* const var) {
  DCHECK_EQ(BOOLEAN_VAR, var->VarType());
  BooleanVar* const boolean_var = reinterpret_cast<BooleanVar*>(var);
  boolean_var->RestoreValue();
}

// ----- API -----

IntVar* Solver::MakeIntVar(int64 min, int64 max, const string& name) {
  if (min == max) {
    return RevAlloc(new IntConst(this, min, name));
  }
  if (min == 0 && max == 1) {
    return RegisterIntVar(RevAlloc(new BooleanVar(this, name)));
  } else if (max - min == 1) {
    const string inner_name = "inner_" + name;
    return RegisterIntVar(MakeSum(RevAlloc(new BooleanVar(this, inner_name)),
                                  min)->VarWithName(name));
  } else {
    return RegisterIntVar(RevAlloc(new DomainIntVar(this, min, max, name)));
  }
}

IntVar* Solver::MakeIntVar(int64 min, int64 max) {
  return MakeIntVar(min, max, "");
}

IntVar* Solver::MakeBoolVar(const string& name) {
  return RegisterIntVar(RevAlloc(new BooleanVar(this, name)));
}

IntVar* Solver::MakeBoolVar() {
  return RegisterIntVar(RevAlloc(new BooleanVar(this, "")));
}

IntVar* Solver::MakeIntVar(const std::vector<int64>& values, const string& name) {
  return RegisterIntVar(
      RevAlloc(new DomainIntVar(this, SortedNoDuplicates(values), name)));
}

IntVar* Solver::MakeIntVar(const std::vector<int64>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values, const string& name) {
  return RegisterIntVar(RevAlloc(
      new DomainIntVar(this, SortedNoDuplicates(ToInt64Vector(values)), name)));
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntConst(int64 val, const string& name) {
  // If IntConst is going to be named after its creation,
  // cp_share_int_consts should be set to false otherwise names can potentially
  // be overwritten.
  if (FLAGS_cp_share_int_consts && name.empty() &&
      val >= MIN_CACHED_INT_CONST && val <= MAX_CACHED_INT_CONST) {
    return cached_constants_[val - MIN_CACHED_INT_CONST];
  }
  return RevAlloc(new IntConst(this, val, name));
}

IntVar* Solver::MakeIntConst(int64 val) { return MakeIntConst(val, ""); }

// ----- Int Var and associated methods -----

namespace {
string IndexedName(const string& prefix, int index, int max_index) {
#if 0
#if defined(_MSC_VER)
  const int digits = max_index > 0 ?
      static_cast<int>(log(1.0L * max_index) / log(10.0L)) + 1 :
      1;
#else
  const int digits = max_index > 0 ? static_cast<int>(log10(max_index)) + 1: 1;
#endif
  return StringPrintf("%s%0*d", prefix.c_str(), digits, index);
#else
  return StringPrintf("%s%d", prefix.c_str(), index);
#endif
}
}  // namespace

void Solver::MakeIntVarArray(int var_count, int64 vmin, int64 vmax,
                             const string& name, std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax, IndexedName(name, i, var_count)));
  }
}

void Solver::MakeIntVarArray(int var_count, int64 vmin, int64 vmax,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax));
  }
}

IntVar** Solver::MakeIntVarArray(int var_count, int64 vmin, int64 vmax,
                                 const string& name) {
  IntVar** vars = new IntVar* [var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeIntVar(vmin, vmax, IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::MakeBoolVarArray(int var_count, const string& name,
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

IntVar** Solver::MakeBoolVarArray(int var_count, const string& name) {
  IntVar** vars = new IntVar* [var_count];
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

IntExpr* Solver::MakeSum(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (r->Bound()) {
    return MakeSum(l, r->Min());
  }
  if (l->Bound()) {
    return MakeSum(r, l->Min());
  }
  if (l == r) {
    return MakeProd(l, 2);
  }
  IntExpr* cache =
      model_cache_->FindExprExprExpression(l, r, ModelCache::EXPR_EXPR_SUM);
  if (cache == nullptr) {
    cache =
        model_cache_->FindExprExprExpression(r, l, ModelCache::EXPR_EXPR_SUM);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    IntExpr* const result =
        AddOverflows(l->Max(), r->Max()) || AddUnderflows(l->Min(), r->Min())
            ? RegisterIntExpr(RevAlloc(new SafePlusIntExpr(this, l, r)))
            : RegisterIntExpr(RevAlloc(new PlusIntExpr(this, l, r)));
    model_cache_->InsertExprExprExpression(result, l, r,
                                           ModelCache::EXPR_EXPR_SUM);
    return result;
  }
}

IntExpr* Solver::MakeSum(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(e->Min() + v);
  }
  if (v == 0) {
    return e;
  }
  IntExpr* result =
      Cache()->FindExprConstantExpression(e, v, ModelCache::EXPR_CONSTANT_SUM);
  if (result == nullptr) {
    if (e->IsVar() && !AddOverflows(v, e->Max()) &&
        !AddUnderflows(v, e->Min())) {
      IntVar* const var = e->Var();
      switch (var->VarType()) {
        case DOMAIN_INT_VAR: {
          result = RegisterIntExpr(RevAlloc(new PlusCstDomainIntVar(
              this, reinterpret_cast<DomainIntVar*>(var), v)));
          break;
        }
        case CONST_VAR: {
          result = RegisterIntExpr(MakeIntConst(var->Min() + v));
          break;
        }
        case VAR_ADD_CST: {
          PlusCstVar* const add_var = reinterpret_cast<PlusCstVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = v + add_var->Constant();
          if (new_constant == 0) {
            result = sub_var;
          } else {
            if (sub_var->VarType() == DOMAIN_INT_VAR) {
              DomainIntVar* const dvar =
                  reinterpret_cast<DomainIntVar*>(sub_var);
              result = RegisterIntExpr(
                  RevAlloc(new PlusCstDomainIntVar(this, dvar, new_constant)));
            } else {
              result = RegisterIntExpr(
                  RevAlloc(new PlusCstIntVar(this, sub_var, new_constant)));
            }
          }
          break;
        }
        case CST_SUB_VAR: {
          SubCstIntVar* const add_var = reinterpret_cast<SubCstIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = v + add_var->Constant();
          result = RegisterIntExpr(
              RevAlloc(new SubCstIntVar(this, sub_var, new_constant)));
          break;
        }
        case OPP_VAR: {
          OppIntVar* const add_var = reinterpret_cast<OppIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          result =
              RegisterIntExpr(RevAlloc(new SubCstIntVar(this, sub_var, v)));
          break;
        }
        default:
          result = RegisterIntExpr(RevAlloc(new PlusCstIntVar(this, var, v)));
      }
    } else {
      result = RegisterIntExpr(RevAlloc(new PlusIntCstExpr(this, e, v)));
    }
    Cache()->InsertExprConstantExpression(result, e, v,
                                          ModelCache::EXPR_CONSTANT_SUM);
  }
  return result;
}

IntExpr* Solver::MakeDifference(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeDifference(l->Min(), r);
  }
  if (r->Bound()) {
    return MakeSum(l, -r->Min());
  }
  IntExpr* result =
      Cache()->FindExprExprExpression(l, r, ModelCache::EXPR_EXPR_DIFFERENCE);
  if (result == nullptr) {
    if (!SubOverflows(l->Min(), r->Max()) &&
        !SubUnderflows(l->Min(), r->Max()) &&
        !SubOverflows(l->Max(), r->Min()) &&
        !SubUnderflows(l->Max(), r->Min())) {
      result = RegisterIntExpr(RevAlloc(new SubIntExpr(this, l, r)));
    } else {
      result = RegisterIntExpr(RevAlloc(new SafeSubIntExpr(this, l, r)));
    }
    Cache()->InsertExprExprExpression(result, l, r,
                                      ModelCache::EXPR_EXPR_DIFFERENCE);
  }
  return result;
}

// warning: this is 'v - e'.
IntExpr* Solver::MakeDifference(int64 v, IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(v - e->Min());
  }
  if (v == 0) {
    return MakeOpposite(e);
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      e, v, ModelCache::EXPR_CONSTANT_DIFFERENCE);
  if (result == nullptr) {
    if (e->IsVar() && e->Min() != kint64min && !SubOverflows(v, e->Min()) &&
        !SubUnderflows(v, e->Max())) {
      IntVar* const var = e->Var();
      switch (var->VarType()) {
        case VAR_ADD_CST: {
          PlusCstVar* const add_var = reinterpret_cast<PlusCstVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = v - add_var->Constant();
          if (new_constant == 0) {
            result = sub_var;
          } else {
            result = RegisterIntExpr(
                RevAlloc(new SubCstIntVar(this, sub_var, new_constant)));
          }
          break;
        }
        case CST_SUB_VAR: {
          SubCstIntVar* const add_var = reinterpret_cast<SubCstIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = v - add_var->Constant();
          result = MakeSum(sub_var, new_constant);
          break;
        }
        case OPP_VAR: {
          OppIntVar* const add_var = reinterpret_cast<OppIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          result = MakeSum(sub_var, v);
          break;
        }
        default:
          result = RegisterIntExpr(RevAlloc(new SubCstIntVar(this, var, v)));
      }
    } else {
      result = RegisterIntExpr(RevAlloc(new SubIntCstExpr(this, e, v)));
    }
    Cache()->InsertExprConstantExpression(result, e, v,
                                          ModelCache::EXPR_CONSTANT_DIFFERENCE);
  }
  return result;
}

IntExpr* Solver::MakeOpposite(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(-e->Min());
  }
  IntExpr* result = Cache()->FindExprExpression(e, ModelCache::EXPR_OPPOSITE);
  if (result == nullptr) {
    if (e->IsVar()) {
      result = RegisterIntVar(RevAlloc(new OppIntExpr(this, e))->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new OppIntExpr(this, e)));
    }
    Cache()->InsertExprExpression(result, e, ModelCache::EXPR_OPPOSITE);
  }
  return result;
}

IntExpr* Solver::MakeProd(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  IntExpr* result =
      Cache()->FindExprConstantExpression(e, v, ModelCache::EXPR_CONSTANT_PROD);
  if (result != nullptr) {
    return result;
  } else {
    if (e->Bound()) {
      return MakeIntConst(v * e->Min());
    } else if (v == 1) {
      return e;
    } else if (v == -1) {
      return MakeOpposite(e);
    } else if (v > 0) {
      if (e->Max() > kint64max / v || e->Min() < kint64min / v) {
        result =
            RegisterIntExpr(RevAlloc(new SafeTimesPosIntCstExpr(this, e, v)));
      } else {
        result = RegisterIntExpr(RevAlloc(new TimesPosIntCstExpr(this, e, v)));
      }
    } else if (v == 0) {
      result = MakeIntConst(0);
    } else {  // v < 0.
      result = RegisterIntExpr(RevAlloc(new TimesIntNegCstExpr(this, e, v)));
    }
    if (e->IsVar() && !FLAGS_cp_disable_expression_optimization) {
      result = result->Var();
    }
    Cache()->InsertExprConstantExpression(result, e, v,
                                          ModelCache::EXPR_CONSTANT_PROD);
    return result;
  }
}

namespace {
void ExtractPower(IntExpr** const expr, int64* const exponant) {
  if (dynamic_cast<BasePower*>(*expr) != nullptr) {
    BasePower* const power = dynamic_cast<BasePower*>(*expr);
    *expr = power->expr();
    *exponant = power->exponant();
  }
  if (dynamic_cast<IntSquare*>(*expr) != nullptr) {
    IntSquare* const power = dynamic_cast<IntSquare*>(*expr);
    *expr = power->expr();
    *exponant = 2;
  }
  if ((*expr)->IsVar()) {
    IntVar* const var = (*expr)->Var();
    IntExpr* const sub = var->solver()->CastExpression(var);
    if (sub != nullptr && dynamic_cast<BasePower*>(sub) != nullptr) {
      BasePower* const power = dynamic_cast<BasePower*>(sub);
      *expr = power->expr();
      *exponant = power->exponant();
    }
    if (sub != nullptr && dynamic_cast<IntSquare*>(sub) != nullptr) {
      IntSquare* const power = dynamic_cast<IntSquare*>(sub);
      *expr = power->expr();
      *exponant = 2;
    }
  }
}

void ExtractProduct(IntExpr** const expr, int64* const coefficient,
                    bool* modified) {
  if (dynamic_cast<TimesCstIntVar*>(*expr) != nullptr) {
    TimesCstIntVar* const left_prod = dynamic_cast<TimesCstIntVar*>(*expr);
    *coefficient *= left_prod->Constant();
    *expr = left_prod->SubVar();
    *modified = true;
  } else if (dynamic_cast<TimesIntCstExpr*>(*expr) != nullptr) {
    TimesIntCstExpr* const left_prod = dynamic_cast<TimesIntCstExpr*>(*expr);
    *coefficient *= left_prod->Constant();
    *expr = left_prod->Expr();
    *modified = true;
  }
}
}  // namespace

IntExpr* Solver::MakeProd(IntExpr* const l, IntExpr* const r) {
  if (l->Bound()) {
    return MakeProd(r, l->Min());
  }

  if (r->Bound()) {
    return MakeProd(l, r->Min());
  }

  // ----- Discover squares and powers -----

  IntExpr* left = l;
  IntExpr* right = r;
  int64 left_exponant = 1;
  int64 right_exponant = 1;
  ExtractPower(&left, &left_exponant);
  ExtractPower(&right, &right_exponant);

  if (left == right) {
    return MakePower(left, left_exponant + right_exponant);
  }

  // ----- Discover nested products -----

  left = l;
  right = r;
  int64 coefficient = 1;
  bool modified = false;

  ExtractProduct(&left, &coefficient, &modified);
  ExtractProduct(&right, &coefficient, &modified);
  if (modified) {
    return MakeProd(MakeProd(left, right), coefficient);
  }

  // ----- Standart build -----

  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  IntExpr* result =
      model_cache_->FindExprExprExpression(l, r, ModelCache::EXPR_EXPR_PROD);
  if (result == nullptr) {
    result =
        model_cache_->FindExprExprExpression(r, l, ModelCache::EXPR_EXPR_PROD);
  }
  if (result != nullptr) {
    return result;
  }
  if (l->IsVar() && l->Var()->VarType() == BOOLEAN_VAR) {
    if (r->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(l), r)));
    } else {
      result = RegisterIntExpr(RevAlloc(
          new TimesBooleanIntExpr(this, reinterpret_cast<BooleanVar*>(l), r)));
    }
  } else if (r->IsVar() &&
             reinterpret_cast<IntVar*>(r)->VarType() == BOOLEAN_VAR) {
    if (l->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(r), l)));
    } else {
      result = RegisterIntExpr(RevAlloc(
          new TimesBooleanIntExpr(this, reinterpret_cast<BooleanVar*>(r), l)));
    }
  } else if (l->Min() >= 0 && r->Min() >= 0) {
    if (CapProd(l->Max(), r->Max()) == kint64max) {  // Potential overflow.
      result = RegisterIntExpr(RevAlloc(new SafeTimesPosIntExpr(this, l, r)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesPosIntExpr(this, l, r)));
    }
  } else {
    result = RegisterIntExpr(RevAlloc(new TimesIntExpr(this, l, r)));
  }
  model_cache_->InsertExprExprExpression(result, l, r,
                                         ModelCache::EXPR_EXPR_PROD);
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* const numerator, IntExpr* const denominator) {
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

IntExpr* Solver::MakeDiv(IntExpr* const e, int64 v) {
  CHECK(e != nullptr);
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(e->Min() / v);
  } else if (v == 1) {
    return e;
  } else if (v == -1) {
    return MakeOpposite(e);
  } else if (v > 0) {
    return RegisterIntExpr(RevAlloc(new DivPosIntCstExpr(this, e, v)));
  } else if (v == 0) {
    LOG(FATAL) << "Cannot divide by 0";
    return nullptr;
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new DivPosIntCstExpr(this, e, -v))));
    // TODO(user) : implement special case.
  }
}

Constraint* Solver::MakeAbsEquality(IntVar* const sub, IntVar* const abs_var) {
  if (Cache()->FindExprExpression(sub, ModelCache::EXPR_ABS) == nullptr) {
    Cache()->InsertExprExpression(abs_var, sub, ModelCache::EXPR_ABS);
  }
  return RevAlloc(new IntAbsConstraint(this, sub, abs_var));
}

IntExpr* Solver::MakeAbs(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= 0) {
    return e;
  } else if (e->Max() <= 0) {
    return MakeOpposite(e);
  }
  IntExpr* result = Cache()->FindExprExpression(e, ModelCache::EXPR_ABS);
  if (result == nullptr) {
    int64 coefficient = 1;
    IntExpr* expr = nullptr;
    if (IsProduct(e, &expr, &coefficient)) {
      result = MakeProd(MakeAbs(expr), std::abs(coefficient));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntAbs(this, e)));
    }
    Cache()->InsertExprExpression(result, e, ModelCache::EXPR_ABS);
  }
  return result;
}

IntExpr* Solver::MakeSquare(IntExpr* const e) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    const int64 v = e->Min();
    return MakeIntConst(v * v);
  }
  IntExpr* result = Cache()->FindExprExpression(e, ModelCache::EXPR_SQUARE);
  if (result == nullptr) {
    if (e->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new PosIntSquare(this, e)));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntSquare(this, e)));
    }
    Cache()->InsertExprExpression(result, e, ModelCache::EXPR_SQUARE);
  }
  return result;
}

IntExpr* Solver::MakePower(IntExpr* const e, int64 n) {
  CHECK_EQ(this, e->solver());
  CHECK_GE(n, 0);
  if (e->Bound()) {
    const int64 v = e->Min();
    if (v >= OverflowLimit(n)) {  // Overflow.
      return MakeIntConst(kint64max);
    }
    return MakeIntConst(IntPower(v, n));
  }
  switch (n) {
    case 0:
      return MakeIntConst(1);
    case 1:
      return e;
    case 2:
      return MakeSquare(e);
    default: {
      IntExpr* result = nullptr;
      if (n % 2 == 0) {  // even.
        if (e->Min() >= 0) {
          result = RegisterIntExpr(RevAlloc(new PosIntEvenPower(this, e, n)));
        } else {
          result = RegisterIntExpr(RevAlloc(new IntEvenPower(this, e, n)));
        }
      } else {
        result = RegisterIntExpr(RevAlloc(new IntOddPower(this, e, n)));
      }
      return result;
    }
  }
}

IntExpr* Solver::MakeMin(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeMin(r, l->Min());
  }
  if (r->Bound()) {
    return MakeMin(l, r->Min());
  }
  if (l->Min() > r->Max()) {
    return r;
  }
  if (r->Min() > l->Max()) {
    return l;
  }
  return RegisterIntExpr(RevAlloc(new MinIntExpr(this, l, r)));
}

IntExpr* Solver::MakeMin(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (v < e->Min()) {
    return MakeIntConst(v);
  }
  if (e->Bound()) {
    return MakeIntConst(std::min(e->Min(), v));
  }
  if (e->Max() <= v) {
    return e;
  }
  return RegisterIntExpr(RevAlloc(new MinCstIntExpr(this, e, v)));
}

IntExpr* Solver::MakeMin(IntExpr* const e, int v) {
  return MakeMin(e, static_cast<int64>(v));
}

IntExpr* Solver::MakeMax(IntExpr* const l, IntExpr* const r) {
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeMax(r, l->Min());
  }
  if (r->Bound()) {
    return MakeMax(l, r->Min());
  }
  if (l->Min() > r->Max()) {
    return l;
  }
  if (r->Min() > l->Max()) {
    return r;
  }
  return RegisterIntExpr(RevAlloc(new MaxIntExpr(this, l, r)));
}

IntExpr* Solver::MakeMax(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Bound()) {
    return MakeIntConst(std::max(e->Min(), v));
  }
  if (v <= e->Min()) {
    return e;
  }
  if (e->Max() < v) {
    return MakeIntConst(v);
  }
  return RegisterIntExpr(RevAlloc(new MaxCstIntExpr(this, e, v)));
}

IntExpr* Solver::MakeMax(IntExpr* const e, int v) {
  return MakeMax(e, static_cast<int64>(v));
}

IntExpr* Solver::MakeConvexPiecewiseExpr(IntExpr* e, int64 early_cost,
                                         int64 early_date, int64 late_date,
                                         int64 late_cost) {
  return RegisterIntExpr(RevAlloc(new SimpleConvexPiecewiseExpr(
      this, e, early_cost, early_date, late_date, late_cost)));
}

IntExpr* Solver::MakeSemiContinuousExpr(IntExpr* const e, int64 fixed_charge,
                                        int64 step) {
  if (step == 0) {
    if (fixed_charge == 0) {
      return MakeIntConst(0LL);
    } else {
      return RegisterIntExpr(
          RevAlloc(new SemiContinuousStepZeroExpr(this, e, fixed_charge)));
    }
  } else if (step == 1) {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousStepOneExpr(this, e, fixed_charge)));
  } else {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousExpr(this, e, fixed_charge, step)));
  }
  // TODO(user) : benchmark with virtualization of
  // PosIntDivDown and PosIntDivUp - or function pointers.
}

// ---------- Modulo ----------

// ----- Modulo -----

IntExpr* Solver::MakeModulo(IntExpr* const x, int64 mod) {
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  if (mod >= 0) {
    AddConstraint(MakeBetweenCt(result, 0, mod - 1));
  } else {
    AddConstraint(MakeBetweenCt(result, mod + 1, 0));
  }
  return result;
}

IntExpr* Solver::MakeModulo(IntExpr* const x, IntExpr* const mod) {
  if (mod->Bound()) {
    return MakeModulo(x, mod->Min());
  }
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  AddConstraint(MakeLess(result, MakeAbs(mod)));
  AddConstraint(MakeGreater(result, MakeOpposite(MakeAbs(mod))));
  return result;
}

// --------- IntVar ---------

int IntVar::VarType() const { return UNSPECIFIED; }

void IntVar::RemoveValues(const std::vector<int64>& values) {
  // TODO(user): Check and maybe inline this code.
  const int size = values.size();
  DCHECK_GE(size, 0);
  switch (size) {
    case 0: { return; }
    case 1: {
      RemoveValue(values[0]);
      return;
    }
    case 2: {
      RemoveValue(values[0]);
      RemoveValue(values[1]);
      return;
    }
    case 3: {
      RemoveValue(values[0]);
      RemoveValue(values[1]);
      RemoveValue(values[2]);
      return;
    }
    default: {
      // 4 values, let's start doing some more clever things.
      // TODO(user) : Sort values!
      int start_index = 0;
      int64 new_min = Min();
      if (values[start_index] <= new_min) {
        while (start_index < size - 1 &&
               values[start_index + 1] == values[start_index] + 1) {
          new_min = values[start_index + 1] + 1;
          start_index++;
        }
      }
      int end_index = size - 1;
      int64 new_max = Max();
      if (values[end_index] >= new_max) {
        while (end_index > start_index + 1 &&
               values[end_index - 1] == values[end_index] - 1) {
          new_max = values[end_index - 1] - 1;
          end_index--;
        }
      }
      SetRange(new_min, new_max);
      for (int i = start_index; i <= end_index; ++i) {
        RemoveValue(values[i]);
      }
    }
  }
}

void IntVar::Accept(ModelVisitor* const visitor) const {
  IntExpr* const casted = solver()->CastExpression(this);
  visitor->VisitIntegerVariable(this, casted);
}

void IntVar::SetValues(const std::vector<int64>& values) {
  switch (values.size()) {
    case 0: {
      solver()->Fail();
      break;
    }
    case 1: {
      SetValue(values.back());
      break;
    }
    case 2: {
      if (Contains(values[0])) {
        if (Contains(values[1])) {
          const int64 l = std::min(values[0], values[1]);
          const int64 u = std::max(values[0], values[1]);
          SetRange(l, u);
          RemoveInterval(l + 1, u - 1);
        } else {
          SetValue(values[0]);
        }
      } else {
        SetValue(values[1]);
      }
      break;
    }
    default: {
      // TODO(user): use a clean and safe SortedUniqueCopy() class
      // that uses a global, static shared (and locked) storage.
      // TODO(user): [optional] consider porting
      // STLSortAndRemoveDuplicates from base/stl_util.h to the
      // existing open_source/base/stl_util.h and using it here.
      // TODO(user): We could filter out values not in the var.
      std::vector<int64>& tmp = solver()->tmp_vector_;  // NOLINT
      tmp.clear();
      tmp.insert(tmp.end(), values.begin(), values.end());
      std::sort(tmp.begin(), tmp.end());
      tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
      const int size = tmp.size();
      const int64 vmin = Min();
      const int64 vmax = Max();
      int first = 0;
      int last = size - 1;
      if (tmp.front() > vmax || tmp.back() < vmin) {
        solver()->Fail();
      }
      // TODO(user) : We could find the first position >= vmin by dichotomy.
      while (tmp[first] < vmin || !Contains(tmp[first])) {
        ++first;
        if (first > last || tmp[first] > vmax) {
          solver()->Fail();
        }
      }
      while (last > first && (tmp[last] > vmax || !Contains(tmp[last]))) {
        // Note that last >= first implies tmp[last] >= vmin.
        --last;
      }
      DCHECK_GE(last, first);
      SetRange(tmp[first], tmp[last]);
      while (first < last) {
        const int64 start = tmp[first] + 1;
        const int64 end = tmp[first + 1] - 1;
        if (start <= end) {
          RemoveInterval(start, end);
        }
        first++;
      }
    }
  }
}
// ---------- BaseIntExpr ---------

void LinkVarExpr(Solver* const s, IntExpr* const expr, IntVar* const var) {
  if (!var->Bound()) {
    if (var->VarType() == DOMAIN_INT_VAR) {
      DomainIntVar* dvar = reinterpret_cast<DomainIntVar*>(var);
      s->AddCastConstraint(
          s->RevAlloc(new LinkExprAndDomainIntVar(s, expr, dvar)), dvar, expr);
    } else {
      s->AddCastConstraint(s->RevAlloc(new LinkExprAndVar(s, expr, var)), var,
                           expr);
    }
  }
}

IntVar* BaseIntExpr::Var() {
  if (var_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&var_));
    var_ = CastToVar();
  }
  return var_;
}

IntVar* BaseIntExpr::CastToVar() {
  int64 vmin, vmax;
  Range(&vmin, &vmax);
  IntVar* const var = solver()->MakeIntVar(vmin, vmax);
  LinkVarExpr(solver(), this, var);
  return var;
}

// Discovery methods
bool Solver::IsADifference(IntExpr* expr, IntExpr** const left,
                           IntExpr** const right) {
  if (expr->IsVar()) {
    IntVar* const expr_var = expr->Var();
    expr = CastExpression(expr_var);
  }
  // This is a dynamic cast to check the type of expr.
  // It returns nullptr is expr is not a subclass of SubIntExpr.
  SubIntExpr* const sub_expr = dynamic_cast<SubIntExpr*>(expr);  // NOLINT
  if (sub_expr != nullptr) {
    *left = sub_expr->left();
    *right = sub_expr->right();
    return true;
  }
  return false;
}

bool Solver::IsBooleanVar(IntExpr* const expr, IntVar** inner_var,
                          bool* is_negated) const {
  if (expr->IsVar() && expr->Var()->VarType() == BOOLEAN_VAR) {
    *inner_var = expr->Var();
    *is_negated = false;
    return true;
  } else if (expr->IsVar() && expr->Var()->VarType() == CST_SUB_VAR) {
    SubCstIntVar* const sub_var = reinterpret_cast<SubCstIntVar*>(expr);
    if (sub_var != nullptr && sub_var->Constant() == 1 &&
        sub_var->SubVar()->VarType() == BOOLEAN_VAR) {
      *is_negated = true;
      *inner_var = sub_var->SubVar();
      return true;
    }
  }
  return false;
}

bool Solver::IsProduct(IntExpr* const expr, IntExpr** inner_expr,
                       int64* coefficient) {
  if (dynamic_cast<TimesCstIntVar*>(expr) != NULL) {
    TimesCstIntVar* const var = dynamic_cast<TimesCstIntVar*>(expr);
    *coefficient = var->Constant();
    *inner_expr = var->SubVar();
    return true;
  } else if (dynamic_cast<TimesIntCstExpr*>(expr) != NULL) {
    TimesIntCstExpr* const prod = dynamic_cast<TimesIntCstExpr*>(expr);
    *coefficient = prod->Constant();
    *inner_expr = prod->Expr();
    return true;
  }
  *inner_expr = expr;
  *coefficient = 1;
  return false;
}

#undef COND_REV_ALLOC

}  // namespace operations_research
