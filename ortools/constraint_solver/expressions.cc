// Copyright 2010-2018 Google LLC
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

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/string_array.h"

DEFINE_bool(cp_disable_expression_optimization, false,
            "Disable special optimization when creating expressions.");
DEFINE_bool(cp_share_int_consts, true, "Share IntConst's with the same value.");

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ---------- IntExpr ----------

IntVar* IntExpr::VarWithName(const std::string& name) {
  IntVar* const var = Var();
  var->set_name(name);
  return var;
}

// ---------- IntVar ----------

IntVar::IntVar(Solver* const s) : IntExpr(s), index_(s->GetNewIntVarIndex()) {}

IntVar::IntVar(Solver* const s, const std::string& name)
    : IntExpr(s), index_(s->GetNewIntVarIndex()) {
  set_name(name);
}

// ----- Boolean variable -----

const int BooleanVar::kUnboundBooleanVarValue = 2;

void BooleanVar::SetMin(int64 m) {
  if (m <= 0) return;
  if (m > 1) solver()->Fail();
  SetValue(1);
}

void BooleanVar::SetMax(int64 m) {
  if (m >= 1) return;
  if (m < 0) solver()->Fail();
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
  if (u < l) return;
  if (l <= 0 && u >= 1) {
    solver()->Fail();
  } else if (l == 1) {
    SetValue(0);
  } else if (u == 0) {
    SetValue(1);
  }
}

void BooleanVar::WhenBound(Demon* d) {
  if (value_ == kUnboundBooleanVarValue) {
    if (d->priority() == Solver::DELAYED_PRIORITY) {
      delayed_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    } else {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
}

uint64 BooleanVar::Size() const {
  return (1 + (value_ == kUnboundBooleanVarValue));
}

bool BooleanVar::Contains(int64 v) const {
  return ((v == 0 && value_ != 1) || (v == 1 && value_ != 0));
}

IntVar* BooleanVar::IsEqual(int64 constant) {
  if (constant > 1 || constant < 0) {
    return solver()->MakeIntConst(0);
  }
  if (constant == 1) {
    return this;
  } else {  // constant == 0.
    return solver()->MakeDifference(1, this)->Var();
  }
}

IntVar* BooleanVar::IsDifferent(int64 constant) {
  if (constant > 1 || constant < 0) {
    return solver()->MakeIntConst(1);
  }
  if (constant == 1) {
    return solver()->MakeDifference(1, this)->Var();
  } else {  // constant == 0.
    return this;
  }
}

IntVar* BooleanVar::IsGreaterOrEqual(int64 constant) {
  if (constant > 1) {
    return solver()->MakeIntConst(0);
  } else if (constant <= 0) {
    return solver()->MakeIntConst(1);
  } else {
    return this;
  }
}

IntVar* BooleanVar::IsLessOrEqual(int64 constant) {
  if (constant < 0) {
    return solver()->MakeIntConst(0);
  } else if (constant >= 1) {
    return solver()->MakeIntConst(1);
  } else {
    return IsEqual(0);
  }
}

std::string BooleanVar::DebugString() const {
  std::string out;
  const std::string& var_name = name();
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

    ~BitSetIterator() override {}

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

    std::string DebugString() const override { return "BitSetIterator"; }

   private:
    uint64* const bitset_;
    const int64 omin_;
    int64 max_;
    int64 current_;
  };

  class BitSet : public BaseObject {
   public:
    explicit BitSet(Solver* const s) : solver_(s), holes_stamp_(0) {}
    ~BitSet() override {}

    virtual int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) = 0;
    virtual int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) = 0;
    virtual bool Contains(int64 val) const = 0;
    virtual bool SetValue(int64 val) = 0;
    virtual bool RemoveValue(int64 val) = 0;
    virtual uint64 Size() const = 0;
    virtual void DelayRemoveValue(int64 val) = 0;
    virtual void ApplyRemovedValues(DomainIntVar* var) = 0;
    virtual void ClearRemovedValues() = 0;
    virtual std::string pretty_DebugString(int64 min, int64 max) const = 0;
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

    int NumHoles() const {
      return holes_stamp_ < solver_->stamp() ? 0 : holes_.size();
    }

   protected:
    Solver* const solver_;

   private:
    std::vector<int64> holes_;
    uint64 holes_stamp_;
  };

  class QueueHandler : public Demon {
   public:
    explicit QueueHandler(DomainIntVar* const var) : var_(var) {}
    ~QueueHandler() override {}
    void Run(Solver* const s) override {
      s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
      var_->Process();
      s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
    }
    Solver::DemonPriority priority() const override {
      return Solver::VAR_PRIORITY;
    }
    std::string DebugString() const override {
      return absl::StrFormat("Handler(%s)", var_->DebugString());
    }

   private:
    DomainIntVar* const var_;
  };

  // Bounds and Value watchers

  // This class stores the watchers variables attached to values. It is
  // reversible and it helps maintaining the set of 'active' watchers
  // (variables not bound to a single value).
  template <class T>
  class RevIntPtrMap {
   public:
    RevIntPtrMap(Solver* const solver, int64 rmin, int64 rmax)
        : solver_(solver), range_min_(rmin), start_(0) {}

    ~RevIntPtrMap() {}

    bool Empty() const { return start_.Value() == elements_.size(); }

    void SortActive() { std::sort(elements_.begin(), elements_.end()); }

    // Access with value API.

    // Add the pointer to the map attached to the given value.
    void UnsafeRevInsert(int64 value, T* elem) {
      elements_.push_back(std::make_pair(value, elem));
      if (solver_->state() != Solver::OUTSIDE_SEARCH) {
        solver_->AddBacktrackAction(
            [this, value](Solver* s) { Uninsert(value); }, false);
      }
    }

    T* FindPtrOrNull(int64 value, int* position) {
      for (int pos = start_.Value(); pos < elements_.size(); ++pos) {
        if (elements_[pos].first == value) {
          if (position != nullptr) *position = pos;
          return At(pos).second;
        }
      }
      return nullptr;
    }

    // Access map through the underlying vector.
    void RemoveAt(int position) {
      const int start = start_.Value();
      DCHECK_GE(position, start);
      DCHECK_LT(position, elements_.size());
      if (position > start) {
        // Swap the current element with the one at the start position, and
        // increase start.
        const std::pair<int64, T*> copy = elements_[start];
        elements_[start] = elements_[position];
        elements_[position] = copy;
      }
      start_.Incr(solver_);
    }

    const std::pair<int64, T*>& At(int position) const {
      DCHECK_GE(position, start_.Value());
      DCHECK_LT(position, elements_.size());
      return elements_[position];
    }

    void RemoveAll() { start_.SetValue(solver_, elements_.size()); }

    int start() const { return start_.Value(); }
    int end() const { return elements_.size(); }
    // Number of active elements.
    int Size() const { return elements_.size() - start_.Value(); }

    // Removes the object permanently from the map.
    void Uninsert(int64 value) {
      for (int pos = 0; pos < elements_.size(); ++pos) {
        if (elements_[pos].first == value) {
          DCHECK_GE(pos, start_.Value());
          const int last = elements_.size() - 1;
          if (pos != last) {  // Swap the current with the last.
            elements_[pos] = elements_.back();
          }
          elements_.pop_back();
          return;
        }
      }
      LOG(FATAL) << "The element should have been removed";
    }

   private:
    Solver* const solver_;
    const int64 range_min_;
    NumericalRev<int> start_;
    std::vector<std::pair<int64, T*>> elements_;
  };

  // Base class for value watchers
  class BaseValueWatcher : public Constraint {
   public:
    explicit BaseValueWatcher(Solver* const solver) : Constraint(solver) {}

    ~BaseValueWatcher() override {}

    virtual IntVar* GetOrMakeValueWatcher(int64 value) = 0;

    virtual void SetValueWatcher(IntVar* const boolvar, int64 value) = 0;
  };

  // This class monitors the domain of the variable and updates the
  // IsEqual/IsDifferent boolean variables accordingly.
  class ValueWatcher : public BaseValueWatcher {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(ValueWatcher* const watcher, int64 value, IntVar* var)
          : value_watcher_(watcher), value_(value), var_(var) {}
      ~WatchDemon() override {}

      void Run(Solver* const solver) override {
        value_watcher_->ProcessValueWatcher(value_, var_);
      }

     private:
      ValueWatcher* const value_watcher_;
      const int64 value_;
      IntVar* const var_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(ValueWatcher* const watcher)
          : value_watcher_(watcher) {}

      ~VarDemon() override {}

      void Run(Solver* const solver) override { value_watcher_->ProcessVar(); }

     private:
      ValueWatcher* const value_watcher_;
    };

    ValueWatcher(Solver* const solver, DomainIntVar* const variable)
        : BaseValueWatcher(solver),
          variable_(variable),
          hole_iterator_(variable_->MakeHoleIterator(true)),
          var_demon_(nullptr),
          watchers_(solver, variable->Min(), variable->Max()) {}

    ~ValueWatcher() override {}

    IntVar* GetOrMakeValueWatcher(int64 value) override {
      IntVar* const watcher = watchers_.FindPtrOrNull(value, nullptr);
      if (watcher != nullptr) return watcher;
      if (variable_->Contains(value)) {
        if (variable_->Bound()) {
          return solver()->MakeIntConst(1);
        } else {
          const std::string vname = variable_->HasName()
                                        ? variable_->name()
                                        : variable_->DebugString();
          const std::string bname =
              absl::StrFormat("Watch<%s == %d>", vname, value);
          IntVar* const boolvar = solver()->MakeBoolVar(bname);
          watchers_.UnsafeRevInsert(value, boolvar);
          if (posted_.Switched()) {
            boolvar->WhenBound(
                solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
            var_demon_->desinhibit(solver());
          }
          return boolvar;
        }
      } else {
        return variable_->solver()->MakeIntConst(0);
      }
    }

    void SetValueWatcher(IntVar* const boolvar, int64 value) override {
      CHECK(watchers_.FindPtrOrNull(value, nullptr) == nullptr);
      if (!boolvar->Bound()) {
        watchers_.UnsafeRevInsert(value, boolvar);
        if (posted_.Switched() && !boolvar->Bound()) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
          var_demon_->desinhibit(solver());
        }
      }
    }

    void Post() override {
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenDomain(var_demon_);
      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64, IntVar*>& w = watchers_.At(pos);
        const int64 value = w.first;
        IntVar* const boolvar = w.second;
        if (!boolvar->Bound() && variable_->Contains(value)) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
        }
      }
      posted_.Switch(solver());
    }

    void InitialPropagate() override {
      if (variable_->Bound()) {
        VariableBound();
      } else {
        for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
          const std::pair<int64, IntVar*>& w = watchers_.At(pos);
          const int64 value = w.first;
          IntVar* const boolvar = w.second;
          if (!variable_->Contains(value)) {
            boolvar->SetValue(0);
            watchers_.RemoveAt(pos);
          } else {
            if (boolvar->Bound()) {
              ProcessValueWatcher(value, boolvar);
              watchers_.RemoveAt(pos);
            }
          }
        }
        CheckInhibit();
      }
    }

    void ProcessValueWatcher(int64 value, IntVar* boolvar) {
      if (boolvar->Min() == 0) {
        if (variable_->Size() < 0xFFFFFF) {
          variable_->RemoveValue(value);
        } else {
          // Delay removal.
          solver()->AddConstraint(solver()->MakeNonEquality(variable_, value));
        }
      } else {
        variable_->SetValue(value);
      }
    }

    void ProcessVar() {
      const int kSmallList = 16;
      if (variable_->Bound()) {
        VariableBound();
      } else if (watchers_.Size() <= kSmallList ||
                 variable_->Min() != variable_->OldMin() ||
                 variable_->Max() != variable_->OldMax()) {
        // Brute force loop for small numbers of watchers, or if the bounds have
        // changed, which would have required a sort (n log(n)) anyway to take
        // advantage of.
        ScanWatchers();
        CheckInhibit();
      } else {
        // If there is no bitset, then there are no holes.
        // In that case, the two loops above should have performed all
        // propagation. Otherwise, scan the remaining watchers.
        BitSet* const bitset = variable_->bitset();
        if (bitset != nullptr && !watchers_.Empty()) {
          if (bitset->NumHoles() * 2 < watchers_.Size()) {
            for (const int64 hole : InitAndGetValues(hole_iterator_)) {
              int pos = 0;
              IntVar* const boolvar = watchers_.FindPtrOrNull(hole, &pos);
              if (boolvar != nullptr) {
                boolvar->SetValue(0);
                watchers_.RemoveAt(pos);
              }
            }
          } else {
            ScanWatchers();
          }
        }
        CheckInhibit();
      }
    }

    // Optimized case if the variable is bound.
    void VariableBound() {
      DCHECK(variable_->Bound());
      const int64 value = variable_->Min();
      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64, IntVar*>& w = watchers_.At(pos);
        w.second->SetValue(w.first == value);
      }
      watchers_.RemoveAll();
      var_demon_->inhibit(solver());
    }

    // Scans all the watchers to check and assign them.
    void ScanWatchers() {
      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64, IntVar*>& w = watchers_.At(pos);
        if (!variable_->Contains(w.first)) {
          IntVar* const boolvar = w.second;
          boolvar->SetValue(0);
          watchers_.RemoveAt(pos);
        }
      }
    }

    // If the set of active watchers is empty, we can inhibit the demon on the
    // main variable.
    void CheckInhibit() {
      if (watchers_.Empty()) {
        var_demon_->inhibit(solver());
      }
    }

    void Accept(ModelVisitor* const visitor) const override {
      visitor->BeginVisitConstraint(ModelVisitor::kVarValueWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      for (int position = watchers_.start(); position < watchers_.end();
           ++position) {
        const std::pair<int64, IntVar*>& w = watchers_.At(position);
        all_coefficients.push_back(w.first);
        all_bool_vars.push_back(w.second);
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarValueWatcher, this);
    }

    std::string DebugString() const override {
      return absl::StrFormat("ValueWatcher(%s)", variable_->DebugString());
    }

   private:
    DomainIntVar* const variable_;
    IntVarIterator* const hole_iterator_;
    RevSwitch posted_;
    Demon* var_demon_;
    RevIntPtrMap<IntVar> watchers_;
  };

  // Optimized case for small maps.
  class DenseValueWatcher : public BaseValueWatcher {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(DenseValueWatcher* const watcher, int64 value, IntVar* var)
          : value_watcher_(watcher), value_(value), var_(var) {}
      ~WatchDemon() override {}

      void Run(Solver* const solver) override {
        value_watcher_->ProcessValueWatcher(value_, var_);
      }

     private:
      DenseValueWatcher* const value_watcher_;
      const int64 value_;
      IntVar* const var_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(DenseValueWatcher* const watcher)
          : value_watcher_(watcher) {}

      ~VarDemon() override {}

      void Run(Solver* const solver) override { value_watcher_->ProcessVar(); }

     private:
      DenseValueWatcher* const value_watcher_;
    };

    DenseValueWatcher(Solver* const solver, DomainIntVar* const variable)
        : BaseValueWatcher(solver),
          variable_(variable),
          hole_iterator_(variable_->MakeHoleIterator(true)),
          var_demon_(nullptr),
          offset_(variable->Min()),
          watchers_(variable->Max() - variable->Min() + 1, nullptr),
          active_watchers_(0) {}

    ~DenseValueWatcher() override {}

    IntVar* GetOrMakeValueWatcher(int64 value) override {
      const int64 var_max = offset_ + watchers_.size() - 1;  // Bad cast.
      if (value < offset_ || value > var_max) {
        return solver()->MakeIntConst(0);
      }
      const int index = value - offset_;
      IntVar* const watcher = watchers_[index];
      if (watcher != nullptr) return watcher;
      if (variable_->Contains(value)) {
        if (variable_->Bound()) {
          return solver()->MakeIntConst(1);
        } else {
          const std::string vname = variable_->HasName()
                                        ? variable_->name()
                                        : variable_->DebugString();
          const std::string bname =
              absl::StrFormat("Watch<%s == %d>", vname, value);
          IntVar* const boolvar = solver()->MakeBoolVar(bname);
          RevInsert(index, boolvar);
          if (posted_.Switched()) {
            boolvar->WhenBound(
                solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
            var_demon_->desinhibit(solver());
          }
          return boolvar;
        }
      } else {
        return variable_->solver()->MakeIntConst(0);
      }
    }

    void SetValueWatcher(IntVar* const boolvar, int64 value) override {
      const int index = value - offset_;
      CHECK(watchers_[index] == nullptr);
      if (!boolvar->Bound()) {
        RevInsert(index, boolvar);
        if (posted_.Switched() && !boolvar->Bound()) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
          var_demon_->desinhibit(solver());
        }
      }
    }

    void Post() override {
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenDomain(var_demon_);
      for (int pos = 0; pos < watchers_.size(); ++pos) {
        const int64 value = pos + offset_;
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr && !boolvar->Bound() &&
            variable_->Contains(value)) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
        }
      }
      posted_.Switch(solver());
    }

    void InitialPropagate() override {
      if (variable_->Bound()) {
        VariableBound();
      } else {
        for (int pos = 0; pos < watchers_.size(); ++pos) {
          IntVar* const boolvar = watchers_[pos];
          if (boolvar == nullptr) continue;
          const int64 value = pos + offset_;
          if (!variable_->Contains(value)) {
            boolvar->SetValue(0);
            RevRemove(pos);
          } else if (boolvar->Bound()) {
            ProcessValueWatcher(value, boolvar);
            RevRemove(pos);
          }
        }
        if (active_watchers_.Value() == 0) {
          var_demon_->inhibit(solver());
        }
      }
    }

    void ProcessValueWatcher(int64 value, IntVar* boolvar) {
      if (boolvar->Min() == 0) {
        variable_->RemoveValue(value);
      } else {
        variable_->SetValue(value);
      }
    }

    void ProcessVar() {
      if (variable_->Bound()) {
        VariableBound();
      } else {
        // Brute force loop for small numbers of watchers.
        ScanWatchers();
        if (active_watchers_.Value() == 0) {
          var_demon_->inhibit(solver());
        }
      }
    }

    // Optimized case if the variable is bound.
    void VariableBound() {
      DCHECK(variable_->Bound());
      const int64 value = variable_->Min();
      for (int pos = 0; pos < watchers_.size(); ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr) {
          boolvar->SetValue(pos + offset_ == value);
          RevRemove(pos);
        }
      }
      var_demon_->inhibit(solver());
    }

    // Scans all the watchers to check and assign them.
    void ScanWatchers() {
      const int64 old_min_index = variable_->OldMin() - offset_;
      const int64 old_max_index = variable_->OldMax() - offset_;
      const int64 min_index = variable_->Min() - offset_;
      const int64 max_index = variable_->Max() - offset_;
      for (int pos = old_min_index; pos < min_index; ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr) {
          boolvar->SetValue(0);
          RevRemove(pos);
        }
      }
      for (int pos = max_index + 1; pos <= old_max_index; ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr) {
          boolvar->SetValue(0);
          RevRemove(pos);
        }
      }
      BitSet* const bitset = variable_->bitset();
      if (bitset != nullptr) {
        if (bitset->NumHoles() * 2 < active_watchers_.Value()) {
          for (const int64 hole : InitAndGetValues(hole_iterator_)) {
            IntVar* const boolvar = watchers_[hole - offset_];
            if (boolvar != nullptr) {
              boolvar->SetValue(0);
              RevRemove(hole - offset_);
            }
          }
        } else {
          for (int pos = min_index + 1; pos < max_index; ++pos) {
            IntVar* const boolvar = watchers_[pos];
            if (boolvar != nullptr && !variable_->Contains(offset_ + pos)) {
              boolvar->SetValue(0);
              RevRemove(pos);
            }
          }
        }
      }
    }

    void RevRemove(int pos) {
      solver()->SaveValue(reinterpret_cast<void**>(&watchers_[pos]));
      watchers_[pos] = nullptr;
      active_watchers_.Decr(solver());
    }

    void RevInsert(int pos, IntVar* boolvar) {
      solver()->SaveValue(reinterpret_cast<void**>(&watchers_[pos]));
      watchers_[pos] = boolvar;
      active_watchers_.Incr(solver());
    }

    void Accept(ModelVisitor* const visitor) const override {
      visitor->BeginVisitConstraint(ModelVisitor::kVarValueWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      for (int position = 0; position < watchers_.size(); ++position) {
        if (watchers_[position] != nullptr) {
          all_coefficients.push_back(position + offset_);
          all_bool_vars.push_back(watchers_[position]);
        }
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarValueWatcher, this);
    }

    std::string DebugString() const override {
      return absl::StrFormat("DenseValueWatcher(%s)", variable_->DebugString());
    }

   private:
    DomainIntVar* const variable_;
    IntVarIterator* const hole_iterator_;
    RevSwitch posted_;
    Demon* var_demon_;
    const int64 offset_;
    std::vector<IntVar*> watchers_;
    NumericalRev<int> active_watchers_;
  };

  class BaseUpperBoundWatcher : public Constraint {
   public:
    explicit BaseUpperBoundWatcher(Solver* const solver) : Constraint(solver) {}

    ~BaseUpperBoundWatcher() override {}

    virtual IntVar* GetOrMakeUpperBoundWatcher(int64 value) = 0;

    virtual void SetUpperBoundWatcher(IntVar* const boolvar, int64 value) = 0;
  };

  // This class watches the bounds of the variable and updates the
  // IsGreater/IsGreaterOrEqual/IsLess/IsLessOrEqual demons
  // accordingly.
  class UpperBoundWatcher : public BaseUpperBoundWatcher {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(UpperBoundWatcher* const watcher, int64 index,
                 IntVar* const var)
          : value_watcher_(watcher), index_(index), var_(var) {}
      ~WatchDemon() override {}

      void Run(Solver* const solver) override {
        value_watcher_->ProcessUpperBoundWatcher(index_, var_);
      }

     private:
      UpperBoundWatcher* const value_watcher_;
      const int64 index_;
      IntVar* const var_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(UpperBoundWatcher* const watcher)
          : value_watcher_(watcher) {}
      ~VarDemon() override {}

      void Run(Solver* const solver) override { value_watcher_->ProcessVar(); }

     private:
      UpperBoundWatcher* const value_watcher_;
    };

    UpperBoundWatcher(Solver* const solver, DomainIntVar* const variable)
        : BaseUpperBoundWatcher(solver),
          variable_(variable),
          var_demon_(nullptr),
          watchers_(solver, variable->Min(), variable->Max()),
          start_(0),
          end_(0),
          sorted_(false) {}

    ~UpperBoundWatcher() override {}

    IntVar* GetOrMakeUpperBoundWatcher(int64 value) override {
      IntVar* const watcher = watchers_.FindPtrOrNull(value, nullptr);
      if (watcher != nullptr) {
        return watcher;
      }
      if (variable_->Max() >= value) {
        if (variable_->Min() >= value) {
          return solver()->MakeIntConst(1);
        } else {
          const std::string vname = variable_->HasName()
                                        ? variable_->name()
                                        : variable_->DebugString();
          const std::string bname =
              absl::StrFormat("Watch<%s >= %d>", vname, value);
          IntVar* const boolvar = solver()->MakeBoolVar(bname);
          watchers_.UnsafeRevInsert(value, boolvar);
          if (posted_.Switched()) {
            boolvar->WhenBound(
                solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
            var_demon_->desinhibit(solver());
            sorted_ = false;
          }
          return boolvar;
        }
      } else {
        return variable_->solver()->MakeIntConst(0);
      }
    }

    void SetUpperBoundWatcher(IntVar* const boolvar, int64 value) override {
      CHECK(watchers_.FindPtrOrNull(value, nullptr) == nullptr);
      watchers_.UnsafeRevInsert(value, boolvar);
      if (posted_.Switched() && !boolvar->Bound()) {
        boolvar->WhenBound(
            solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
        var_demon_->desinhibit(solver());
        sorted_ = false;
      }
    }

    void Post() override {
      const int kTooSmallToSort = 8;
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenRange(var_demon_);

      if (watchers_.Size() > kTooSmallToSort) {
        watchers_.SortActive();
        sorted_ = true;
        start_.SetValue(solver(), watchers_.start());
        end_.SetValue(solver(), watchers_.end() - 1);
      }

      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64, IntVar*>& w = watchers_.At(pos);
        IntVar* const boolvar = w.second;
        const int64 value = w.first;
        if (!boolvar->Bound() && value > variable_->Min() &&
            value <= variable_->Max()) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
        }
      }
      posted_.Switch(solver());
    }

    void InitialPropagate() override {
      const int64 var_min = variable_->Min();
      const int64 var_max = variable_->Max();
      if (sorted_) {
        while (start_.Value() <= end_.Value()) {
          const std::pair<int64, IntVar*>& w = watchers_.At(start_.Value());
          if (w.first <= var_min) {
            w.second->SetValue(1);
            start_.Incr(solver());
          } else {
            break;
          }
        }
        while (end_.Value() >= start_.Value()) {
          const std::pair<int64, IntVar*>& w = watchers_.At(end_.Value());
          if (w.first > var_max) {
            w.second->SetValue(0);
            end_.Decr(solver());
          } else {
            break;
          }
        }
        for (int i = start_.Value(); i <= end_.Value(); ++i) {
          const std::pair<int64, IntVar*>& w = watchers_.At(i);
          if (w.second->Bound()) {
            ProcessUpperBoundWatcher(w.first, w.second);
          }
        }
        if (start_.Value() > end_.Value()) {
          var_demon_->inhibit(solver());
        }
      } else {
        for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
          const std::pair<int64, IntVar*>& w = watchers_.At(pos);
          const int64 value = w.first;
          IntVar* const boolvar = w.second;

          if (value <= var_min) {
            boolvar->SetValue(1);
            watchers_.RemoveAt(pos);
          } else if (value > var_max) {
            boolvar->SetValue(0);
            watchers_.RemoveAt(pos);
          } else if (boolvar->Bound()) {
            ProcessUpperBoundWatcher(value, boolvar);
            watchers_.RemoveAt(pos);
          }
        }
      }
    }

    void Accept(ModelVisitor* const visitor) const override {
      visitor->BeginVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64, IntVar*>& w = watchers_.At(pos);
        all_coefficients.push_back(w.first);
        all_bool_vars.push_back(w.second);
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
    }

    std::string DebugString() const override {
      return absl::StrFormat("UpperBoundWatcher(%s)", variable_->DebugString());
    }

   private:
    void ProcessUpperBoundWatcher(int64 value, IntVar* const boolvar) {
      if (boolvar->Min() == 0) {
        variable_->SetMax(value - 1);
      } else {
        variable_->SetMin(value);
      }
    }

    void ProcessVar() {
      const int64 var_min = variable_->Min();
      const int64 var_max = variable_->Max();
      if (sorted_) {
        while (start_.Value() <= end_.Value()) {
          const std::pair<int64, IntVar*>& w = watchers_.At(start_.Value());
          if (w.first <= var_min) {
            w.second->SetValue(1);
            start_.Incr(solver());
          } else {
            break;
          }
        }
        while (end_.Value() >= start_.Value()) {
          const std::pair<int64, IntVar*>& w = watchers_.At(end_.Value());
          if (w.first > var_max) {
            w.second->SetValue(0);
            end_.Decr(solver());
          } else {
            break;
          }
        }
        if (start_.Value() > end_.Value()) {
          var_demon_->inhibit(solver());
        }
      } else {
        for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
          const std::pair<int64, IntVar*>& w = watchers_.At(pos);
          const int64 value = w.first;
          IntVar* const boolvar = w.second;

          if (value <= var_min) {
            boolvar->SetValue(1);
            watchers_.RemoveAt(pos);
          } else if (value > var_max) {
            boolvar->SetValue(0);
            watchers_.RemoveAt(pos);
          }
        }
        if (watchers_.Empty()) {
          var_demon_->inhibit(solver());
        }
      }
    }

    DomainIntVar* const variable_;
    RevSwitch posted_;
    Demon* var_demon_;
    RevIntPtrMap<IntVar> watchers_;
    NumericalRev<int> start_;
    NumericalRev<int> end_;
    bool sorted_;
  };

  // Optimized case for small maps.
  class DenseUpperBoundWatcher : public BaseUpperBoundWatcher {
   public:
    class WatchDemon : public Demon {
     public:
      WatchDemon(DenseUpperBoundWatcher* const watcher, int64 value,
                 IntVar* var)
          : value_watcher_(watcher), value_(value), var_(var) {}
      ~WatchDemon() override {}

      void Run(Solver* const solver) override {
        value_watcher_->ProcessUpperBoundWatcher(value_, var_);
      }

     private:
      DenseUpperBoundWatcher* const value_watcher_;
      const int64 value_;
      IntVar* const var_;
    };

    class VarDemon : public Demon {
     public:
      explicit VarDemon(DenseUpperBoundWatcher* const watcher)
          : value_watcher_(watcher) {}

      ~VarDemon() override {}

      void Run(Solver* const solver) override { value_watcher_->ProcessVar(); }

     private:
      DenseUpperBoundWatcher* const value_watcher_;
    };

    DenseUpperBoundWatcher(Solver* const solver, DomainIntVar* const variable)
        : BaseUpperBoundWatcher(solver),
          variable_(variable),
          var_demon_(nullptr),
          offset_(variable->Min()),
          watchers_(variable->Max() - variable->Min() + 1, nullptr),
          active_watchers_(0) {}

    ~DenseUpperBoundWatcher() override {}

    IntVar* GetOrMakeUpperBoundWatcher(int64 value) override {
      if (variable_->Max() >= value) {
        if (variable_->Min() >= value) {
          return solver()->MakeIntConst(1);
        } else {
          const std::string vname = variable_->HasName()
                                        ? variable_->name()
                                        : variable_->DebugString();
          const std::string bname =
              absl::StrFormat("Watch<%s >= %d>", vname, value);
          IntVar* const boolvar = solver()->MakeBoolVar(bname);
          RevInsert(value - offset_, boolvar);
          if (posted_.Switched()) {
            boolvar->WhenBound(
                solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
            var_demon_->desinhibit(solver());
          }
          return boolvar;
        }
      } else {
        return variable_->solver()->MakeIntConst(0);
      }
    }

    void SetUpperBoundWatcher(IntVar* const boolvar, int64 value) override {
      const int index = value - offset_;
      CHECK(watchers_[index] == nullptr);
      if (!boolvar->Bound()) {
        RevInsert(index, boolvar);
        if (posted_.Switched() && !boolvar->Bound()) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
          var_demon_->desinhibit(solver());
        }
      }
    }

    void Post() override {
      var_demon_ = solver()->RevAlloc(new VarDemon(this));
      variable_->WhenRange(var_demon_);
      for (int pos = 0; pos < watchers_.size(); ++pos) {
        const int64 value = pos + offset_;
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr && !boolvar->Bound() &&
            value > variable_->Min() && value <= variable_->Max()) {
          boolvar->WhenBound(
              solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
        }
      }
      posted_.Switch(solver());
    }

    void InitialPropagate() override {
      for (int pos = 0; pos < watchers_.size(); ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar == nullptr) continue;
        const int64 value = pos + offset_;
        if (value <= variable_->Min()) {
          boolvar->SetValue(1);
          RevRemove(pos);
        } else if (value > variable_->Max()) {
          boolvar->SetValue(0);
          RevRemove(pos);
        } else if (boolvar->Bound()) {
          ProcessUpperBoundWatcher(value, boolvar);
          RevRemove(pos);
        }
      }
      if (active_watchers_.Value() == 0) {
        var_demon_->inhibit(solver());
      }
    }

    void ProcessUpperBoundWatcher(int64 value, IntVar* boolvar) {
      if (boolvar->Min() == 0) {
        variable_->SetMax(value - 1);
      } else {
        variable_->SetMin(value);
      }
    }

    void ProcessVar() {
      const int64 old_min_index = variable_->OldMin() - offset_;
      const int64 old_max_index = variable_->OldMax() - offset_;
      const int64 min_index = variable_->Min() - offset_;
      const int64 max_index = variable_->Max() - offset_;
      for (int pos = old_min_index; pos <= min_index; ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr) {
          boolvar->SetValue(1);
          RevRemove(pos);
        }
      }

      for (int pos = max_index + 1; pos <= old_max_index; ++pos) {
        IntVar* const boolvar = watchers_[pos];
        if (boolvar != nullptr) {
          boolvar->SetValue(0);
          RevRemove(pos);
        }
      }
      if (active_watchers_.Value() == 0) {
        var_demon_->inhibit(solver());
      }
    }

    void RevRemove(int pos) {
      solver()->SaveValue(reinterpret_cast<void**>(&watchers_[pos]));
      watchers_[pos] = nullptr;
      active_watchers_.Decr(solver());
    }

    void RevInsert(int pos, IntVar* boolvar) {
      solver()->SaveValue(reinterpret_cast<void**>(&watchers_[pos]));
      watchers_[pos] = boolvar;
      active_watchers_.Incr(solver());
    }

    void Accept(ModelVisitor* const visitor) const override {
      visitor->BeginVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
      visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                              variable_);
      std::vector<int64> all_coefficients;
      std::vector<IntVar*> all_bool_vars;
      for (int position = 0; position < watchers_.size(); ++position) {
        if (watchers_[position] != nullptr) {
          all_coefficients.push_back(position + offset_);
          all_bool_vars.push_back(watchers_[position]);
        }
      }
      visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                                 all_bool_vars);
      visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                         all_coefficients);
      visitor->EndVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
    }

    std::string DebugString() const override {
      return absl::StrFormat("DenseUpperBoundWatcher(%s)",
                             variable_->DebugString());
    }

   private:
    DomainIntVar* const variable_;
    RevSwitch posted_;
    Demon* var_demon_;
    const int64 offset_;
    std::vector<IntVar*> watchers_;
    NumericalRev<int> active_watchers_;
  };

  // ----- Main Class -----
  DomainIntVar(Solver* const s, int64 vmin, int64 vmax,
               const std::string& name);
  DomainIntVar(Solver* const s, const std::vector<int64>& sorted_values,
               const std::string& name);
  ~DomainIntVar() override;

  int64 Min() const override { return min_.Value(); }
  void SetMin(int64 m) override;
  int64 Max() const override { return max_.Value(); }
  void SetMax(int64 m) override;
  void SetRange(int64 mi, int64 ma) override;
  void SetValue(int64 v) override;
  bool Bound() const override { return (min_.Value() == max_.Value()); }
  int64 Value() const override {
    CHECK_EQ(min_.Value(), max_.Value())
        << " variable " << DebugString() << " is not bound.";
    return min_.Value();
  }
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  void CreateBits();
  void WhenBound(Demon* d) override {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_bound_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
      } else {
        bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }
  void WhenRange(Demon* d) override {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_range_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
      } else {
        range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }
  void WhenDomain(Demon* d) override {
    if (min_.Value() != max_.Value()) {
      if (d->priority() == Solver::DELAYED_PRIORITY) {
        delayed_domain_demons_.PushIfNotTop(solver(),
                                            solver()->RegisterDemon(d));
      } else {
        domain_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
      }
    }
  }

  IntVar* IsEqual(int64 constant) override {
    Solver* const s = solver();
    if (constant == min_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsLessOrEqualCstVar(this, constant);
    }
    if (constant == max_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsGreaterOrEqualCstVar(this, constant);
    }
    if (!Contains(constant)) {
      return s->MakeIntConst(int64{0});
    }
    if (Bound() && min_.Value() == constant) {
      return s->MakeIntConst(int64{1});
    }
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      if (value_watcher_ == nullptr) {
        if (CapSub(Max(), Min()) <= 256) {
          solver()->SaveAndSetValue(
              reinterpret_cast<void**>(&value_watcher_),
              reinterpret_cast<void*>(
                  solver()->RevAlloc(new DenseValueWatcher(solver(), this))));

        } else {
          solver()->SaveAndSetValue(reinterpret_cast<void**>(&value_watcher_),
                                    reinterpret_cast<void*>(solver()->RevAlloc(
                                        new ValueWatcher(solver(), this))));
        }
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
      solver()->SaveAndSetValue(reinterpret_cast<void**>(&value_watcher_),
                                reinterpret_cast<void*>(solver()->RevAlloc(
                                    new ValueWatcher(solver(), this))));
      for (int i = 0; i < vars.size(); ++i) {
        value_watcher_->SetValueWatcher(vars[i], values[i]);
      }
    }
    return value_watcher_;
  }

  IntVar* IsDifferent(int64 constant) override {
    Solver* const s = solver();
    if (constant == min_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsGreaterOrEqualCstVar(this, constant + 1);
    }
    if (constant == max_.Value() && value_watcher_ == nullptr) {
      return s->MakeIsLessOrEqualCstVar(this, constant - 1);
    }
    if (!Contains(constant)) {
      return s->MakeIntConst(int64{1});
    }
    if (Bound() && min_.Value() == constant) {
      return s->MakeIntConst(int64{0});
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

  IntVar* IsGreaterOrEqual(int64 constant) override {
    Solver* const s = solver();
    if (max_.Value() < constant) {
      return s->MakeIntConst(int64{0});
    }
    if (min_.Value() >= constant) {
      return s->MakeIntConst(int64{1});
    }
    IntExpr* const cache = s->Cache()->FindExprConstantExpression(
        this, constant, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
    if (cache != nullptr) {
      return cache->Var();
    } else {
      if (bound_watcher_ == nullptr) {
        if (CapSub(Max(), Min()) <= 256) {
          solver()->SaveAndSetValue(
              reinterpret_cast<void**>(&bound_watcher_),
              reinterpret_cast<void*>(solver()->RevAlloc(
                  new DenseUpperBoundWatcher(solver(), this))));
          solver()->AddConstraint(bound_watcher_);
        } else {
          solver()->SaveAndSetValue(
              reinterpret_cast<void**>(&bound_watcher_),
              reinterpret_cast<void*>(
                  solver()->RevAlloc(new UpperBoundWatcher(solver(), this))));
          solver()->AddConstraint(bound_watcher_);
        }
      }
      IntVar* const boolvar =
          bound_watcher_->GetOrMakeUpperBoundWatcher(constant);
      s->Cache()->InsertExprConstantExpression(
          boolvar, this, constant,
          ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
      return boolvar;
    }
  }

  Constraint* SetIsGreaterOrEqual(const std::vector<int64>& values,
                                  const std::vector<IntVar*>& vars) {
    if (bound_watcher_ == nullptr) {
      if (CapSub(Max(), Min()) <= 256) {
        solver()->SaveAndSetValue(
            reinterpret_cast<void**>(&bound_watcher_),
            reinterpret_cast<void*>(solver()->RevAlloc(
                new DenseUpperBoundWatcher(solver(), this))));
        solver()->AddConstraint(bound_watcher_);
      } else {
        solver()->SaveAndSetValue(reinterpret_cast<void**>(&bound_watcher_),
                                  reinterpret_cast<void*>(solver()->RevAlloc(
                                      new UpperBoundWatcher(solver(), this))));
        solver()->AddConstraint(bound_watcher_);
      }
      for (int i = 0; i < values.size(); ++i) {
        bound_watcher_->SetUpperBoundWatcher(vars[i], values[i]);
      }
    }
    return bound_watcher_;
  }

  IntVar* IsLessOrEqual(int64 constant) override {
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
  void CleanInProcess();
  uint64 Size() const override {
    if (bits_ != nullptr) return bits_->Size();
    return (static_cast<uint64>(max_.Value()) -
            static_cast<uint64>(min_.Value()) + 1);
  }
  bool Contains(int64 v) const override {
    if (v < min_.Value() || v > max_.Value()) return false;
    return (bits_ == nullptr ? true : bits_->Contains(v));
  }
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  int64 OldMin() const override { return std::min(old_min_, min_.Value()); }
  int64 OldMax() const override { return std::max(old_max_, max_.Value()); }

  std::string DebugString() const override;
  BitSet* bitset() const { return bits_; }
  int VarType() const override { return DOMAIN_INT_VAR; }
  std::string BaseName() const override { return "IntegerVar"; }

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
  BaseValueWatcher* value_watcher_;
  BaseUpperBoundWatcher* bound_watcher_;
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

  SimpleBitSet(Solver* const s, const std::vector<int64>& sorted_values,
               int64 vmin, int64 vmax)
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

  ~SimpleBitSet() override {
    delete[] bits_;
    delete[] stamps_;
  }

  bool bit(int64 val) const { return IsBitSet64(bits_, val - omin_); }

  int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) override {
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

  int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) override {
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

  bool SetValue(int64 val) override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  bool Contains(int64 val) const override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  bool RemoveValue(int64 val) override {
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
  uint64 Size() const override { return size_.Value(); }

  std::string DebugString() const override {
    std::string out;
    absl::StrAppendFormat(&out, "SimpleBitSet(%d..%d : ", omin_, omax_);
    for (int i = 0; i < bsize_; ++i) {
      absl::StrAppendFormat(&out, "%x", bits_[i]);
    }
    out += ")";
    return out;
  }

  void DelayRemoveValue(int64 val) override { removed_.push_back(val); }

  void ApplyRemovedValues(DomainIntVar* var) override {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin();
         it != removed_.end(); ++it) {
      var->RemoveValue(*it);
    }
  }

  void ClearRemovedValues() override { removed_.clear(); }

  std::string pretty_DebugString(int64 min, int64 max) const override {
    std::string out;
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
              absl::StrAppendFormat(&out, "%d ", start_cumul);
            } else if (v == start_cumul + 2) {
              absl::StrAppendFormat(&out, "%d %d ", start_cumul, v - 1);
            } else {
              absl::StrAppendFormat(&out, "%d..%d ", start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          absl::StrAppendFormat(&out, "%d %d", start_cumul, max);
        } else {
          absl::StrAppendFormat(&out, "%d..%d", start_cumul, max);
        }
      } else {
        absl::StrAppendFormat(&out, "%d", max);
      }
    } else {
      absl::StrAppendFormat(&out, "%d", min);
    }
    return out;
  }

  DomainIntVar::BitSetIterator* MakeIterator() override {
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

  SmallBitSet(Solver* const s, const std::vector<int64>& sorted_values,
              int64 vmin, int64 vmax)
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

  ~SmallBitSet() override {}

  bool bit(int64 val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return (bits_ & OneBit64(val - omin_)) != 0;
  }

  int64 ComputeNewMin(int64 nmin, int64 cmin, int64 cmax) override {
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

  int64 ComputeNewMax(int64 nmax, int64 cmin, int64 cmax) override {
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

  bool SetValue(int64 val) override {
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

  bool Contains(int64 val) const override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  bool RemoveValue(int64 val) override {
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

  uint64 Size() const override { return size_.Value(); }

  std::string DebugString() const override {
    return absl::StrFormat("SmallBitSet(%d..%d : %llx)", omin_, omax_, bits_);
  }

  void DelayRemoveValue(int64 val) override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    removed_.push_back(val);
  }

  void ApplyRemovedValues(DomainIntVar* var) override {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64>::iterator it = removed_.begin();
         it != removed_.end(); ++it) {
      var->RemoveValue(*it);
    }
  }

  void ClearRemovedValues() override { removed_.clear(); }

  std::string pretty_DebugString(int64 min, int64 max) const override {
    std::string out;
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
              absl::StrAppendFormat(&out, "%d ", start_cumul);
            } else if (v == start_cumul + 2) {
              absl::StrAppendFormat(&out, "%d %d ", start_cumul, v - 1);
            } else {
              absl::StrAppendFormat(&out, "%d..%d ", start_cumul, v - 1);
            }
            cumul = false;
          }
        }
      }
      if (cumul) {
        if (max == start_cumul + 1) {
          absl::StrAppendFormat(&out, "%d %d", start_cumul, max);
        } else {
          absl::StrAppendFormat(&out, "%d..%d", start_cumul, max);
        }
      } else {
        absl::StrAppendFormat(&out, "%d", max);
      }
    } else {
      absl::StrAppendFormat(&out, "%d", min);
    }
    return out;
  }

  DomainIntVar::BitSetIterator* MakeIterator() override {
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
  ~EmptyIterator() override {}
  void Init() override {}
  bool Ok() const override { return false; }
  int64 Value() const override {
    LOG(FATAL) << "Should not be called";
    return 0LL;
  }
  void Next() override {}
};

class RangeIterator : public IntVarIterator {
 public:
  explicit RangeIterator(const IntVar* const var)
      : var_(var), min_(kint64max), max_(kint64min), current_(-1) {}

  ~RangeIterator() override {}

  void Init() override {
    min_ = var_->Min();
    max_ = var_->Max();
    current_ = min_;
  }

  bool Ok() const override { return current_ <= max_; }

  int64 Value() const override { return current_; }

  void Next() override { current_++; }

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

  ~DomainIntVarHoleIterator() override {}

  void Init() override {
    bits_ = var_->bitset();
    if (bits_ != nullptr) {
      bits_->InitHoles();
      values_ = bits_->Holes().data();
      size_ = bits_->Holes().size();
    } else {
      values_ = nullptr;
      size_ = 0;
    }
    index_ = 0;
  }

  bool Ok() const override { return index_ < size_; }

  int64 Value() const override {
    DCHECK(bits_ != nullptr);
    DCHECK(index_ < size_);
    return values_[index_];
  }

  void Next() override { index_++; }

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

  ~DomainIntVarDomainIterator() override {
    if (!reversible_ && bitset_iterator_) {
      delete bitset_iterator_;
    }
  }

  void Init() override {
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

  bool Ok() const override {
    return bitset_iterator_ ? bitset_iterator_->Ok() : (current_ <= max_);
  }

  int64 Value() const override {
    return bitset_iterator_ ? bitset_iterator_->Value() : current_;
  }

  void Next() override {
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

  ~UnaryIterator() override {
    if (!reversible_) {
      delete iterator_;
    }
  }

  void Init() override { iterator_->Init(); }

  bool Ok() const override { return iterator_->Ok(); }

  void Next() override { iterator_->Next(); }

 protected:
  IntVarIterator* const iterator_;
  const bool reversible_;
};

DomainIntVar::DomainIntVar(Solver* const s, int64 vmin, int64 vmax,
                           const std::string& name)
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

DomainIntVar::DomainIntVar(Solver* const s,
                           const std::vector<int64>& sorted_values,
                           const std::string& name)
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
        (bits_ == nullptr
             ? m
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
        (bits_ == nullptr
             ? m
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
            (bits_ == nullptr
                 ? mi
                 : bits_->ComputeNewMin(mi, min_.Value(), max_.Value()));
        min_.SetValue(solver(), new_min);
      }
      if (min_.Value() > ma) {
        solver()->Fail();
      }
      if (ma < max_.Value()) {
        CheckOldMax();
        const int64 new_max =
            (bits_ == nullptr
                 ? ma
                 : bits_->ComputeNewMax(ma, min_.Value(), max_.Value()));
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

void DomainIntVar::CleanInProcess() {
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
  set_variable_to_clean_on_fail(this);
  new_min_ = min_.Value();
  new_max_ = max_.Value();
  const bool is_bound = min_.Value() == max_.Value();
  const bool range_changed =
      min_.Value() != OldMin() || max_.Value() != OldMax();
  // Process immediate demons.
  if (is_bound) {
    ExecuteAll(bound_demons_);
  }
  if (range_changed) {
    ExecuteAll(range_demons_);
  }
  ExecuteAll(domain_demons_);

  // Process delayed demons.
  if (is_bound) {
    EnqueueAll(delayed_bound_demons_);
  }
  if (range_changed) {
    EnqueueAll(delayed_range_demons_);
  }
  EnqueueAll(delayed_domain_demons_);

  // Everything went well if we arrive here. Let's clean the variable.
  set_variable_to_clean_on_fail(nullptr);
  CleanInProcess();
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

std::string DomainIntVar::DebugString() const {
  std::string out;
  const std::string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(";
  } else {
    out = "DomainIntVar(";
  }
  if (min_.Value() == max_.Value()) {
    absl::StrAppendFormat(&out, "%d", min_.Value());
  } else if (bits_ != nullptr) {
    out.append(bits_->pretty_DebugString(min_.Value(), max_.Value()));
  } else {
    absl::StrAppendFormat(&out, "%d..%d", min_.Value(), max_.Value());
  }
  out += ")";
  return out;
}

// ----- Real Boolean Var -----

class ConcreteBooleanVar : public BooleanVar {
 public:
  // Utility classes
  class Handler : public Demon {
   public:
    explicit Handler(ConcreteBooleanVar* const var) : Demon(), var_(var) {}
    ~Handler() override {}
    void Run(Solver* const s) override {
      s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
      var_->Process();
      s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
    }
    Solver::DemonPriority priority() const override {
      return Solver::VAR_PRIORITY;
    }
    std::string DebugString() const override {
      return absl::StrFormat("Handler(%s)", var_->DebugString());
    }

   private:
    ConcreteBooleanVar* const var_;
  };

  ConcreteBooleanVar(Solver* const s, const std::string& name)
      : BooleanVar(s, name), handler_(this) {}

  ~ConcreteBooleanVar() override {}

  void SetValue(int64 v) override {
    if (value_ == kUnboundBooleanVarValue) {
      if ((v & 0xfffffffffffffffe) == 0) {
        InternalSaveBooleanVarValue(solver(), this);
        value_ = static_cast<int>(v);
        EnqueueVar(&handler_);
        return;
      }
    } else if (v == value_) {
      return;
    }
    solver()->Fail();
  }

  void Process() {
    DCHECK_NE(value_, kUnboundBooleanVarValue);
    ExecuteAll(bound_demons_);
    for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_bound_demons_); it.ok();
         ++it) {
      EnqueueDelayedDemon(*it);
    }
  }

  int64 OldMin() const override { return 0LL; }
  int64 OldMax() const override { return 1LL; }
  void RestoreValue() override { value_ = kUnboundBooleanVarValue; }

 private:
  Handler handler_;
};

// ----- IntConst -----

class IntConst : public IntVar {
 public:
  IntConst(Solver* const s, int64 value, const std::string& name = "")
      : IntVar(s, name), value_(value) {}
  ~IntConst() override {}

  int64 Min() const override { return value_; }
  void SetMin(int64 m) override {
    if (m > value_) {
      solver()->Fail();
    }
  }
  int64 Max() const override { return value_; }
  void SetMax(int64 m) override {
    if (m < value_) {
      solver()->Fail();
    }
  }
  void SetRange(int64 l, int64 u) override {
    if (l > value_ || u < value_) {
      solver()->Fail();
    }
  }
  void SetValue(int64 v) override {
    if (v != value_) {
      solver()->Fail();
    }
  }
  bool Bound() const override { return true; }
  int64 Value() const override { return value_; }
  void RemoveValue(int64 v) override {
    if (v == value_) {
      solver()->Fail();
    }
  }
  void RemoveInterval(int64 l, int64 u) override {
    if (l <= value_ && value_ <= u) {
      solver()->Fail();
    }
  }
  void WhenBound(Demon* d) override {}
  void WhenRange(Demon* d) override {}
  void WhenDomain(Demon* d) override {}
  uint64 Size() const override { return 1; }
  bool Contains(int64 v) const override { return (v == value_); }
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new RangeIterator(this));
  }
  int64 OldMin() const override { return value_; }
  int64 OldMax() const override { return value_; }
  std::string DebugString() const override {
    std::string out;
    if (solver()->HasName(this)) {
      const std::string& var_name = name();
      absl::StrAppendFormat(&out, "%s(%d)", var_name, value_);
    } else {
      absl::StrAppendFormat(&out, "IntConst(%d)", value_);
    }
    return out;
  }

  int VarType() const override { return CONST_VAR; }

  IntVar* IsEqual(int64 constant) override {
    if (constant == value_) {
      return solver()->MakeIntConst(1);
    } else {
      return solver()->MakeIntConst(0);
    }
  }

  IntVar* IsDifferent(int64 constant) override {
    if (constant == value_) {
      return solver()->MakeIntConst(0);
    } else {
      return solver()->MakeIntConst(1);
    }
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    return solver()->MakeIntConst(value_ >= constant);
  }

  IntVar* IsLessOrEqual(int64 constant) override {
    return solver()->MakeIntConst(value_ <= constant);
  }

  std::string name() const override {
    if (solver()->HasName(this)) {
      return PropagationBaseObject::name();
    } else {
      return absl::StrCat(value_);
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

  ~PlusCstVar() override {}

  void WhenRange(Demon* d) override { var_->WhenRange(d); }

  void WhenBound(Demon* d) override { var_->WhenBound(d); }

  void WhenDomain(Demon* d) override { var_->WhenDomain(d); }

  int64 OldMin() const override { return CapAdd(var_->OldMin(), cst_); }

  int64 OldMax() const override { return CapAdd(var_->OldMax(), cst_); }

  std::string DebugString() const override {
    if (HasName()) {
      return absl::StrFormat("%s(%s + %d)", name(), var_->DebugString(), cst_);
    } else {
      return absl::StrFormat("(%s + %d)", var_->DebugString(), cst_);
    }
  }

  int VarType() const override { return VAR_ADD_CST; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kSumOperation, cst_,
                                  var_);
  }

  IntVar* IsEqual(int64 constant) override {
    return var_->IsEqual(constant - cst_);
  }

  IntVar* IsDifferent(int64 constant) override {
    return var_->IsDifferent(constant - cst_);
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    return var_->IsGreaterOrEqual(constant - cst_);
  }

  IntVar* IsLessOrEqual(int64 constant) override {
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

    ~PlusCstIntVarIterator() override {}

    int64 Value() const override { return iterator_->Value() + cst_; }

   private:
    const int64 cst_;
  };

  PlusCstIntVar(Solver* const s, IntVar* v, int64 c) : PlusCstVar(s, v, c) {}

  ~PlusCstIntVar() override {}

  int64 Min() const override { return var_->Min() + cst_; }

  void SetMin(int64 m) override { var_->SetMin(CapSub(m, cst_)); }

  int64 Max() const override { return var_->Max() + cst_; }

  void SetMax(int64 m) override { var_->SetMax(CapSub(m, cst_)); }

  void SetRange(int64 l, int64 u) override {
    var_->SetRange(CapSub(l, cst_), CapSub(u, cst_));
  }

  void SetValue(int64 v) override { var_->SetValue(v - cst_); }

  int64 Value() const override { return var_->Value() + cst_; }

  bool Bound() const override { return var_->Bound(); }

  void RemoveValue(int64 v) override { var_->RemoveValue(v - cst_); }

  void RemoveInterval(int64 l, int64 u) override {
    var_->RemoveInterval(l - cst_, u - cst_);
  }

  uint64 Size() const override { return var_->Size(); }

  bool Contains(int64 v) const override { return var_->Contains(v - cst_); }

  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(
        reversible, new PlusCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
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

    ~PlusCstDomainIntVarIterator() override {}

    int64 Value() const override { return iterator_->Value() + cst_; }

   private:
    const int64 cst_;
  };

  PlusCstDomainIntVar(Solver* const s, DomainIntVar* v, int64 c)
      : PlusCstVar(s, v, c) {}

  ~PlusCstDomainIntVar() override {}

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;

  DomainIntVar* domain_int_var() const {
    return reinterpret_cast<DomainIntVar*>(var_);
  }

  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new PlusCstDomainIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
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
      << " variable is not bound";
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
    ~SubCstIntVarIterator() override {}

    int64 Value() const override { return cst_ - iterator_->Value(); }

   private:
    const int64 cst_;
  };

  SubCstIntVar(Solver* const s, IntVar* v, int64 c);
  ~SubCstIntVar() override;

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(
        reversible, new SubCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(
        reversible, new SubCstIntVarIterator(var_, cst_, false, reversible));
  }
  int64 OldMin() const override { return CapSub(cst_, var_->OldMax()); }
  int64 OldMax() const override { return CapSub(cst_, var_->OldMin()); }
  std::string DebugString() const override;
  std::string name() const override;
  int VarType() const override { return CST_SUB_VAR; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation,
                                  cst_, var_);
  }

  IntVar* IsEqual(int64 constant) override {
    return var_->IsEqual(cst_ - constant);
  }

  IntVar* IsDifferent(int64 constant) override {
    return var_->IsDifferent(cst_ - constant);
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    return var_->IsLessOrEqual(cst_ - constant);
  }

  IntVar* IsLessOrEqual(int64 constant) override {
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

std::string SubCstIntVar::DebugString() const {
  if (cst_ == 1 && var_->VarType() == BOOLEAN_VAR) {
    return absl::StrFormat("Not(%s)", var_->DebugString());
  } else {
    return absl::StrFormat("(%d - %s)", cst_, var_->DebugString());
  }
}

std::string SubCstIntVar::name() const {
  if (solver()->HasName(this)) {
    return PropagationBaseObject::name();
  } else if (cst_ == 1 && var_->VarType() == BOOLEAN_VAR) {
    return absl::StrFormat("Not(%s)", var_->name());
  } else {
    return absl::StrFormat("(%d - %s)", cst_, var_->name());
  }
}

// -x variable, optimized case

class OppIntVar : public IntVar {
 public:
  class OppIntVarIterator : public UnaryIterator {
   public:
    OppIntVarIterator(const IntVar* const v, bool hole, bool reversible)
        : UnaryIterator(v, hole, reversible) {}
    ~OppIntVarIterator() override {}

    int64 Value() const override { return -iterator_->Value(); }
  };

  OppIntVar(Solver* const s, IntVar* v);
  ~OppIntVar() override;

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible,
                          new OppIntVarIterator(var_, false, reversible));
  }
  int64 OldMin() const override { return CapOpp(var_->OldMax()); }
  int64 OldMax() const override { return CapOpp(var_->OldMin()); }
  std::string DebugString() const override;
  int VarType() const override { return OPP_VAR; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation, 0,
                                  var_);
  }

  IntVar* IsEqual(int64 constant) override { return var_->IsEqual(-constant); }

  IntVar* IsDifferent(int64 constant) override {
    return var_->IsDifferent(-constant);
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    return var_->IsLessOrEqual(-constant);
  }

  IntVar* IsLessOrEqual(int64 constant) override {
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

std::string OppIntVar::DebugString() const {
  return absl::StrFormat("-(%s)", var_->DebugString());
}

// ----- Utility functions -----

// x * c variable, optimized case

class TimesCstIntVar : public IntVar {
 public:
  TimesCstIntVar(Solver* const s, IntVar* v, int64 c)
      : IntVar(s), var_(v), cst_(c) {}
  ~TimesCstIntVar() override {}

  IntVar* SubVar() const { return var_; }
  int64 Constant() const { return cst_; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kProductOperation, cst_,
                                  var_);
  }

  IntVar* IsEqual(int64 constant) override {
    if (constant % cst_ == 0) {
      return var_->IsEqual(constant / cst_);
    } else {
      return solver()->MakeIntConst(0);
    }
  }

  IntVar* IsDifferent(int64 constant) override {
    if (constant % cst_ == 0) {
      return var_->IsDifferent(constant / cst_);
    } else {
      return solver()->MakeIntConst(1);
    }
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    if (cst_ > 0) {
      return var_->IsGreaterOrEqual(PosIntDivUp(constant, cst_));
    } else {
      return var_->IsLessOrEqual(PosIntDivDown(-constant, -cst_));
    }
  }

  IntVar* IsLessOrEqual(int64 constant) override {
    if (cst_ > 0) {
      return var_->IsLessOrEqual(PosIntDivDown(constant, cst_));
    } else {
      return var_->IsGreaterOrEqual(PosIntDivUp(-constant, -cst_));
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s * %d)", var_->DebugString(), cst_);
  }

  int VarType() const override { return VAR_TIMES_CST; }

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
    ~TimesPosCstIntVarIterator() override {}

    int64 Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesPosCstIntVar(Solver* const s, IntVar* v, int64 c);
  ~TimesPosCstIntVar() override;

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new TimesPosCstIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new TimesPosCstIntVarIterator(
                                          var_, cst_, false, reversible));
  }
  int64 OldMin() const override { return CapProd(var_->OldMin(), cst_); }
  int64 OldMax() const override { return CapProd(var_->OldMax(), cst_); }
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
    ~TimesPosCstBoolVarIterator() override {}

    int64 Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c);
  ~TimesPosCstBoolVar() override;

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new EmptyIterator());
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(
        reversible,
        new TimesPosCstBoolVarIterator(boolean_var(), cst_, false, reversible));
  }
  int64 OldMin() const override { return 0; }
  int64 OldMax() const override { return cst_; }

  BooleanVar* boolean_var() const {
    return reinterpret_cast<BooleanVar*>(var_);
  }
};

// ----- TimesPosCstBoolVar -----

TimesPosCstBoolVar::TimesPosCstBoolVar(Solver* const s, BooleanVar* v, int64 c)
    : TimesCstIntVar(s, v, c) {}

TimesPosCstBoolVar::~TimesPosCstBoolVar() {}

int64 TimesPosCstBoolVar::Min() const {
  return (boolean_var()->RawValue() == 1) * cst_;
}

void TimesPosCstBoolVar::SetMin(int64 m) {
  if (m > cst_) {
    solver()->Fail();
  } else if (m > 0) {
    boolean_var()->SetMin(1);
  }
}

int64 TimesPosCstBoolVar::Max() const {
  return (boolean_var()->RawValue() != 0) * cst_;
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
  return boolean_var()->RawValue() != BooleanVar::kUnboundBooleanVarValue;
}

void TimesPosCstBoolVar::WhenRange(Demon* d) { boolean_var()->WhenRange(d); }

int64 TimesPosCstBoolVar::Value() const {
  CHECK_NE(boolean_var()->RawValue(), BooleanVar::kUnboundBooleanVarValue)
      << " variable is not bound";
  return boolean_var()->RawValue() * cst_;
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
  return (1 +
          (boolean_var()->RawValue() == BooleanVar::kUnboundBooleanVarValue));
}

bool TimesPosCstBoolVar::Contains(int64 v) const {
  if (v == 0) {
    return boolean_var()->RawValue() != 1;
  } else if (v == cst_) {
    return boolean_var()->RawValue() != 0;
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
    ~TimesNegCstIntVarIterator() override {}

    int64 Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64 cst_;
  };

  TimesNegCstIntVar(Solver* const s, IntVar* v, int64 c);
  ~TimesNegCstIntVar() override;

  int64 Min() const override;
  void SetMin(int64 m) override;
  int64 Max() const override;
  void SetMax(int64 m) override;
  void SetRange(int64 l, int64 u) override;
  void SetValue(int64 v) override;
  bool Bound() const override;
  int64 Value() const override;
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new TimesNegCstIntVarIterator(
                                          var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return COND_REV_ALLOC(reversible, new TimesNegCstIntVarIterator(
                                          var_, cst_, false, reversible));
  }
  int64 OldMin() const override { return CapProd(var_->OldMax(), cst_); }
  int64 OldMax() const override { return CapProd(var_->OldMin(), cst_); }
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

  ~PlusIntExpr() override {}

  int64 Min() const override { return left_->Min() + right_->Min(); }

  void SetMin(int64 m) override {
    if (m > left_->Min() + right_->Min()) {
      left_->SetMin(m - right_->Max());
      right_->SetMin(m - left_->Max());
    }
  }

  void SetRange(int64 l, int64 u) override {
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

  int64 Max() const override { return left_->Max() + right_->Max(); }

  void SetMax(int64 m) override {
    if (m < left_->Max() + right_->Max()) {
      left_->SetMax(m - right_->Min());
      right_->SetMax(m - left_->Min());
    }
  }

  bool Bound() const override { return (left_->Bound() && right_->Bound()); }

  void Range(int64* const mi, int64* const ma) override {
    *mi = left_->Min() + right_->Min();
    *ma = left_->Max() + right_->Max();
  }

  std::string name() const override {
    return absl::StrFormat("(%s + %s)", left_->name(), right_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s + %s)", left_->DebugString(),
                           right_->DebugString());
  }

  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void ExpandPlusIntExpr(IntExpr* const expr, std::vector<IntExpr*>* subs) {
    PlusIntExpr* const casted = dynamic_cast<PlusIntExpr*>(expr);
    if (casted != nullptr) {
      ExpandPlusIntExpr(casted->left_, subs);
      ExpandPlusIntExpr(casted->right_, subs);
    } else {
      subs->push_back(expr);
    }
  }

  IntVar* CastToVar() override {
    if (dynamic_cast<PlusIntExpr*>(left_) != nullptr ||
        dynamic_cast<PlusIntExpr*>(right_) != nullptr) {
      std::vector<IntExpr*> sub_exprs;
      ExpandPlusIntExpr(left_, &sub_exprs);
      ExpandPlusIntExpr(right_, &sub_exprs);
      if (sub_exprs.size() >= 3) {
        std::vector<IntVar*> sub_vars(sub_exprs.size());
        for (int i = 0; i < sub_exprs.size(); ++i) {
          sub_vars[i] = sub_exprs[i]->Var();
        }
        return solver()->MakeSum(sub_vars)->Var();
      }
    }
    return BaseIntExpr::CastToVar();
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~SafePlusIntExpr() override {}

  int64 Min() const override { return CapAdd(left_->Min(), right_->Min()); }

  void SetMin(int64 m) override {
    left_->SetMin(CapSub(m, right_->Max()));
    right_->SetMin(CapSub(m, left_->Max()));
  }

  void SetRange(int64 l, int64 u) override {
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

  int64 Max() const override { return CapAdd(left_->Max(), right_->Max()); }

  void SetMax(int64 m) override {
    left_->SetMax(CapSub(m, right_->Min()));
    right_->SetMax(CapSub(m, left_->Min()));
  }

  bool Bound() const override { return (left_->Bound() && right_->Bound()); }

  std::string name() const override {
    return absl::StrFormat("(%s + %s)", left_->name(), right_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s + %s)", left_->DebugString(),
                           right_->DebugString());
  }

  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~PlusIntCstExpr() override {}
  int64 Min() const override { return CapAdd(expr_->Min(), value_); }
  void SetMin(int64 m) override { expr_->SetMin(CapSub(m, value_)); }
  int64 Max() const override { return CapAdd(expr_->Max(), value_); }
  void SetMax(int64 m) override { expr_->SetMax(CapSub(m, value_)); }
  bool Bound() const override { return (expr_->Bound()); }
  std::string name() const override {
    return absl::StrFormat("(%s + %d)", expr_->name(), value_);
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s + %d)", expr_->DebugString(), value_);
  }
  void WhenRange(Demon* d) override { expr_->WhenRange(d); }
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* const visitor) const override {
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
      AddOverflows(value_, expr_->Min())) {
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

  ~SubIntExpr() override {}

  int64 Min() const override { return left_->Min() - right_->Max(); }

  void SetMin(int64 m) override {
    left_->SetMin(CapAdd(m, right_->Min()));
    right_->SetMax(CapSub(left_->Max(), m));
  }

  int64 Max() const override { return left_->Max() - right_->Min(); }

  void SetMax(int64 m) override {
    left_->SetMax(CapAdd(m, right_->Max()));
    right_->SetMin(CapSub(left_->Min(), m));
  }

  void Range(int64* mi, int64* ma) override {
    *mi = left_->Min() - right_->Max();
    *ma = left_->Max() - right_->Min();
  }

  void SetRange(int64 l, int64 u) override {
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

  bool Bound() const override { return (left_->Bound() && right_->Bound()); }

  std::string name() const override {
    return absl::StrFormat("(%s - %s)", left_->name(), right_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s - %s)", left_->DebugString(),
                           right_->DebugString());
  }

  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~SafeSubIntExpr() override {}

  int64 Min() const override { return CapSub(left_->Min(), right_->Max()); }

  void SetMin(int64 m) override {
    left_->SetMin(CapAdd(m, right_->Min()));
    right_->SetMax(CapSub(left_->Max(), m));
  }

  void SetRange(int64 l, int64 u) override {
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

  void Range(int64* mi, int64* ma) override {
    *mi = CapSub(left_->Min(), right_->Max());
    *ma = CapSub(left_->Max(), right_->Min());
  }

  int64 Max() const override { return CapSub(left_->Max(), right_->Min()); }

  void SetMax(int64 m) override {
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
  ~SubIntCstExpr() override {}
  int64 Min() const override { return CapSub(value_, expr_->Max()); }
  void SetMin(int64 m) override { expr_->SetMax(CapSub(value_, m)); }
  int64 Max() const override { return CapSub(value_, expr_->Min()); }
  void SetMax(int64 m) override { expr_->SetMin(CapSub(value_, m)); }
  bool Bound() const override { return (expr_->Bound()); }
  std::string name() const override {
    return absl::StrFormat("(%d - %s)", value_, expr_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%d - %s)", value_, expr_->DebugString());
  }
  void WhenRange(Demon* d) override { expr_->WhenRange(d); }
  IntVar* CastToVar() override;

  void Accept(ModelVisitor* const visitor) const override {
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
      SubOverflows(value_, expr_->Max())) {
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
  ~OppIntExpr() override {}
  int64 Min() const override { return (-expr_->Max()); }
  void SetMin(int64 m) override { expr_->SetMax(-m); }
  int64 Max() const override { return (-expr_->Min()); }
  void SetMax(int64 m) override { expr_->SetMin(-m); }
  bool Bound() const override { return (expr_->Bound()); }
  std::string name() const override {
    return absl::StrFormat("(-%s)", expr_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(-%s)", expr_->DebugString());
  }
  void WhenRange(Demon* d) override { expr_->WhenRange(d); }
  IntVar* CastToVar() override;

  void Accept(ModelVisitor* const visitor) const override {
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

  ~TimesIntCstExpr() override {}

  bool Bound() const override { return (expr_->Bound()); }

  std::string name() const override {
    return absl::StrFormat("(%s * %d)", expr_->name(), value_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s * %d)", expr_->DebugString(), value_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  IntExpr* Expr() const { return expr_; }

  int64 Constant() const { return value_; }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~TimesPosIntCstExpr() override {}

  int64 Min() const override { return expr_->Min() * value_; }

  void SetMin(int64 m) override { expr_->SetMin(PosIntDivUp(m, value_)); }

  int64 Max() const override { return expr_->Max() * value_; }

  void SetMax(int64 m) override { expr_->SetMax(PosIntDivDown(m, value_)); }

  IntVar* CastToVar() override {
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

  ~SafeTimesPosIntCstExpr() override {}

  int64 Min() const override { return CapProd(expr_->Min(), value_); }

  void SetMin(int64 m) override {
    if (m != kint64min) {
      expr_->SetMin(PosIntDivUp(m, value_));
    }
  }

  int64 Max() const override { return CapProd(expr_->Max(), value_); }

  void SetMax(int64 m) override {
    if (m != kint64max) {
      expr_->SetMax(PosIntDivDown(m, value_));
    }
  }

  IntVar* CastToVar() override {
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

  ~TimesIntNegCstExpr() override {}

  int64 Min() const override { return CapProd(expr_->Max(), value_); }

  void SetMin(int64 m) override {
    if (m != kint64min) {
      expr_->SetMax(PosIntDivDown(-m, -value_));
    }
  }

  int64 Max() const override { return CapProd(expr_->Min(), value_); }

  void SetMax(int64 m) override {
    if (m != kint64max) {
      expr_->SetMin(PosIntDivUp(-m, -value_));
    }
  }

  IntVar* CastToVar() override {
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
  ~TimesIntExpr() override {}
  int64 Min() const override {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::min(std::min(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                    std::min(CapProd(lmax, rmin), CapProd(lmin, rmax)));
  }
  void SetMin(int64 m) override;
  int64 Max() const override {
    const int64 lmin = left_->Min();
    const int64 lmax = left_->Max();
    const int64 rmin = right_->Min();
    const int64 rmax = right_->Max();
    return std::max(std::max(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                    std::max(CapProd(lmax, rmin), CapProd(lmin, rmax)));
  }
  void SetMax(int64 m) override;
  bool Bound() const override;
  std::string name() const override {
    return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s * %s)", left_->DebugString(),
                           right_->DebugString());
  }
  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~TimesPosIntExpr() override {}
  int64 Min() const override { return (left_->Min() * right_->Min()); }
  void SetMin(int64 m) override;
  int64 Max() const override { return (left_->Max() * right_->Max()); }
  void SetMax(int64 m) override;
  bool Bound() const override;
  std::string name() const override {
    return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s * %s)", left_->DebugString(),
                           right_->DebugString());
  }
  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~SafeTimesPosIntExpr() override {}
  int64 Min() const override { return CapProd(left_->Min(), right_->Min()); }
  void SetMin(int64 m) override {
    if (m != kint64min) {
      SetPosPosMinExpr(left_, right_, m);
    }
  }
  int64 Max() const override { return CapProd(left_->Max(), right_->Max()); }
  void SetMax(int64 m) override {
    if (m != kint64max) {
      SetPosPosMaxExpr(left_, right_, m);
    }
  }
  bool Bound() const override {
    return (left_->Max() == 0 || right_->Max() == 0 ||
            (left_->Bound() && right_->Bound()));
  }
  std::string name() const override {
    return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s * %s)", left_->DebugString(),
                           right_->DebugString());
  }
  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~TimesBooleanPosIntExpr() override {}
  int64 Min() const override {
    return (boolvar_->RawValue() == 1 ? expr_->Min() : 0);
  }
  void SetMin(int64 m) override;
  int64 Max() const override {
    return (boolvar_->RawValue() == 0 ? 0 : expr_->Max());
  }
  void SetMax(int64 m) override;
  void Range(int64* mi, int64* ma) override;
  void SetRange(int64 mi, int64 ma) override;
  bool Bound() const override;
  std::string name() const override {
    return absl::StrFormat("(%s * %s)", boolvar_->name(), expr_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s * %s)", boolvar_->DebugString(),
                           expr_->DebugString());
  }
  void WhenRange(Demon* d) override {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  if (boolvar_->RawValue() == 1) {
    expr_->SetMax(m);
  }
}

void TimesBooleanPosIntExpr::Range(int64* mi, int64* ma) {
  const int value = boolvar_->RawValue();
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
  if (boolvar_->RawValue() == 1) {
    expr_->SetMax(ma);
  }
}

bool TimesBooleanPosIntExpr::Bound() const {
  return (boolvar_->RawValue() == 0 || expr_->Max() == 0 ||
          (boolvar_->RawValue() != BooleanVar::kUnboundBooleanVarValue &&
           expr_->Bound()));
}

// ----- TimesBooleanIntExpr -----

class TimesBooleanIntExpr : public BaseIntExpr {
 public:
  TimesBooleanIntExpr(Solver* const s, BooleanVar* const b, IntExpr* const e)
      : BaseIntExpr(s), boolvar_(b), expr_(e) {}
  ~TimesBooleanIntExpr() override {}
  int64 Min() const override {
    switch (boolvar_->RawValue()) {
      case 0: {
        return 0LL;
      }
      case 1: {
        return expr_->Min();
      }
      default: {
        DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
        return std::min(int64{0}, expr_->Min());
      }
    }
  }
  void SetMin(int64 m) override;
  int64 Max() const override {
    switch (boolvar_->RawValue()) {
      case 0: {
        return 0LL;
      }
      case 1: {
        return expr_->Max();
      }
      default: {
        DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
        return std::max(int64{0}, expr_->Max());
      }
    }
  }
  void SetMax(int64 m) override;
  void Range(int64* mi, int64* ma) override;
  void SetRange(int64 mi, int64 ma) override;
  bool Bound() const override;
  std::string name() const override {
    return absl::StrFormat("(%s * %s)", boolvar_->name(), expr_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s * %s)", boolvar_->DebugString(),
                           expr_->DebugString());
  }
  void WhenRange(Demon* d) override {
    boolvar_->WhenRange(d);
    expr_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  switch (boolvar_->RawValue()) {
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
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
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
  switch (boolvar_->RawValue()) {
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
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
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
  switch (boolvar_->RawValue()) {
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
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      *mi = std::min(int64{0}, expr_->Min());
      *ma = std::max(int64{0}, expr_->Max());
      break;
    }
  }
}

void TimesBooleanIntExpr::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  switch (boolvar_->RawValue()) {
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
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
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
  return (boolvar_->RawValue() == 0 ||
          (expr_->Bound() &&
           (boolvar_->RawValue() != BooleanVar::kUnboundBooleanVarValue ||
            expr_->Max() == 0)));
}

// ----- DivPosIntCstExpr -----

class DivPosIntCstExpr : public BaseIntExpr {
 public:
  DivPosIntCstExpr(Solver* const s, IntExpr* const e, int64 v)
      : BaseIntExpr(s), expr_(e), value_(v) {
    CHECK_GE(v, 0);
  }
  ~DivPosIntCstExpr() override {}

  int64 Min() const override { return expr_->Min() / value_; }

  void SetMin(int64 m) override {
    if (m > 0) {
      expr_->SetMin(m * value_);
    } else {
      expr_->SetMin((m - 1) * value_ + 1);
    }
  }
  int64 Max() const override { return expr_->Max() / value_; }

  void SetMax(int64 m) override {
    if (m >= 0) {
      expr_->SetMax((m + 1) * value_ - 1);
    } else {
      expr_->SetMax(m * value_);
    }
  }

  std::string name() const override {
    return absl::StrFormat("(%s div %d)", expr_->name(), value_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s div %d)", expr_->DebugString(), value_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~DivPosIntExpr() override {}

  int64 Min() const override {
    return num_->Min() >= 0
               ? num_->Min() / denom_->Max()
               : (denom_->Min() == 0 ? num_->Min()
                                     : num_->Min() / denom_->Min());
  }

  int64 Max() const override {
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

  void SetMin(int64 m) override {
    if (m > 0) {
      SetPosMin(num_, denom_, m);
    } else {
      SetPosMax(opp_num_, denom_, -m);
    }
  }

  void SetMax(int64 m) override {
    if (m >= 0) {
      SetPosMax(num_, denom_, m);
    } else {
      SetPosMin(opp_num_, denom_, -m);
    }
  }

  std::string name() const override {
    return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s div %s)", num_->DebugString(),
                           denom_->DebugString());
  }
  void WhenRange(Demon* d) override {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~DivPosPosIntExpr() override {}

  int64 Min() const override {
    if (denom_->Max() == 0) {
      solver()->Fail();
    }
    return num_->Min() / denom_->Max();
  }

  int64 Max() const override {
    if (denom_->Min() == 0) {
      return num_->Max();
    } else {
      return num_->Max() / denom_->Min();
    }
  }

  void SetMin(int64 m) override {
    if (m > 0) {
      num_->SetMin(m * denom_->Min());
      denom_->SetMax(num_->Max() / m);
    }
  }

  void SetMax(int64 m) override {
    if (m >= 0) {
      num_->SetMax((m + 1) * denom_->Max() - 1);
      denom_->SetMin(num_->Min() / (m + 1) + 1);
    } else {
      solver()->Fail();
    }
  }

  std::string name() const override {
    return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("(%s div %s)", num_->DebugString(),
                           denom_->DebugString());
  }

  void WhenRange(Demon* d) override {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~DivIntExpr() override {}

  int64 Min() const override {
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

  int64 Max() const override {
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

  void SetMin(int64 m) override {
    AdjustDenominator();
    if (m > 0) {
      SetPosMin(num_, denom_, m);
    } else {
      SetPosMax(opp_num_, denom_, -m);
    }
  }

  void SetMax(int64 m) override {
    AdjustDenominator();
    if (m >= 0) {
      SetPosMax(num_, denom_, m);
    } else {
      SetPosMin(opp_num_, denom_, -m);
    }
  }

  std::string name() const override {
    return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("(%s div %s)", num_->DebugString(),
                           denom_->DebugString());
  }
  void WhenRange(Demon* d) override {
    num_->WhenRange(d);
    denom_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~IntAbsConstraint() override {}

  void Post() override {
    Demon* const sub_demon = MakeConstraintDemon0(
        solver(), this, &IntAbsConstraint::PropagateSub, "PropagateSub");
    sub_->WhenRange(sub_demon);
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &IntAbsConstraint::PropagateTarget, "PropagateTarget");
    target_var_->WhenRange(target_demon);
  }

  void InitialPropagate() override {
    PropagateSub();
    PropagateTarget();
  }

  void PropagateSub() {
    const int64 smin = sub_->Min();
    const int64 smax = sub_->Max();
    if (smax <= 0) {
      target_var_->SetRange(-smax, -smin);
    } else if (smin >= 0) {
      target_var_->SetRange(smin, smax);
    } else {
      target_var_->SetRange(0, std::max(-smin, smax));
    }
  }

  void PropagateTarget() {
    const int64 target_max = target_var_->Max();
    sub_->SetRange(-target_max, target_max);
    const int64 target_min = target_var_->Min();
    if (target_min > 0) {
      if (sub_->Min() > -target_min) {
        sub_->SetMin(target_min);
      } else if (sub_->Max() < target_min) {
        sub_->SetMax(-target_min);
      }
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("IntAbsConstraint(%s, %s)", sub_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~IntAbs() override {}

  int64 Min() const override {
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

  void SetMin(int64 m) override {
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

  int64 Max() const override {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    return std::max(-emin, emax);
  }

  void SetMax(int64 m) override { expr_->SetRange(-m, m); }

  void SetRange(int64 mi, int64 ma) override {
    expr_->SetRange(-ma, ma);
    if (mi > 0) {
      int64 emin = 0;
      int64 emax = 0;
      expr_->Range(&emin, &emax);
      if (emin > -mi) {
        expr_->SetMin(mi);
      } else if (emax < mi) {
        expr_->SetMax(-mi);
      }
    }
  }

  void Range(int64* mi, int64* ma) override {
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

  bool Bound() const override { return expr_->Bound(); }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  std::string name() const override {
    return absl::StrFormat("IntAbs(%s)", expr_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("IntAbs(%s)", expr_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kAbs, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kAbs, this);
  }

  IntVar* CastToVar() override {
    int64 min_value = 0;
    int64 max_value = 0;
    Range(&min_value, &max_value);
    Solver* const s = solver();
    const std::string name = absl::StrFormat("AbsVar(%s)", expr_->name());
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
  ~IntSquare() override {}

  int64 Min() const override {
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
  void SetMin(int64 m) override {
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
  int64 Max() const override {
    const int64 emax = expr_->Max();
    const int64 emin = expr_->Min();
    if (emax >= kint32max || emin <= -kint32max) {
      return kint64max;
    }
    return std::max(emin * emin, emax * emax);
  }
  void SetMax(int64 m) override {
    if (m < 0) {
      solver()->Fail();
    }
    if (m == kint64max) {
      return;
    }
    const int64 root = static_cast<int64>(floor(sqrt(static_cast<double>(m))));
    expr_->SetRange(-root, root);
  }
  bool Bound() const override { return expr_->Bound(); }
  void WhenRange(Demon* d) override { expr_->WhenRange(d); }
  std::string name() const override {
    return absl::StrFormat("IntSquare(%s)", expr_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("IntSquare(%s)", expr_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~PosIntSquare() override {}

  int64 Min() const override {
    const int64 emin = expr_->Min();
    return emin >= kint32max ? kint64max : emin * emin;
  }
  void SetMin(int64 m) override {
    if (m <= 0) {
      return;
    }
    const int64 root = static_cast<int64>(ceil(sqrt(static_cast<double>(m))));
    expr_->SetMin(root);
  }
  int64 Max() const override {
    const int64 emax = expr_->Max();
    return emax >= kint32max ? kint64max : emax * emax;
  }
  void SetMax(int64 m) override {
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

  ~BasePower() override {}

  bool Bound() const override { return expr_->Bound(); }

  IntExpr* expr() const { return expr_; }

  int64 exponant() const { return pow_; }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  std::string name() const override {
    return absl::StrFormat("IntPower(%s, %d)", expr_->name(), pow_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("IntPower(%s, %d)", expr_->DebugString(), pow_);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~IntEvenPower() override {}

  int64 Min() const override {
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
  void SetMin(int64 m) override {
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

  int64 Max() const override {
    return std::max(Pown(expr_->Min()), Pown(expr_->Max()));
  }

  void SetMax(int64 m) override {
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

  ~PosIntEvenPower() override {}

  int64 Min() const override { return Pown(expr_->Min()); }

  void SetMin(int64 m) override {
    if (m <= 0) {
      return;
    }
    expr_->SetMin(SqrnUp(m));
  }
  int64 Max() const override { return Pown(expr_->Max()); }

  void SetMax(int64 m) override {
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

  ~IntOddPower() override {}

  int64 Min() const override { return Pown(expr_->Min()); }

  void SetMin(int64 m) override { expr_->SetMin(SqrnUp(m)); }

  int64 Max() const override { return Pown(expr_->Max()); }

  void SetMax(int64 m) override { expr_->SetMax(SqrnDown(m)); }
};

// ----- Min(expr, expr) -----

class MinIntExpr : public BaseIntExpr {
 public:
  MinIntExpr(Solver* const s, IntExpr* const l, IntExpr* const r)
      : BaseIntExpr(s), left_(l), right_(r) {}
  ~MinIntExpr() override {}
  int64 Min() const override {
    const int64 lmin = left_->Min();
    const int64 rmin = right_->Min();
    return std::min(lmin, rmin);
  }
  void SetMin(int64 m) override {
    left_->SetMin(m);
    right_->SetMin(m);
  }
  int64 Max() const override {
    const int64 lmax = left_->Max();
    const int64 rmax = right_->Max();
    return std::min(lmax, rmax);
  }
  void SetMax(int64 m) override {
    if (left_->Min() > m) {
      right_->SetMax(m);
    }
    if (right_->Min() > m) {
      left_->SetMax(m);
    }
  }
  std::string name() const override {
    return absl::StrFormat("MinIntExpr(%s, %s)", left_->name(), right_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("MinIntExpr(%s, %s)", left_->DebugString(),
                           right_->DebugString());
  }
  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~MinCstIntExpr() override {}

  int64 Min() const override { return std::min(expr_->Min(), value_); }

  void SetMin(int64 m) override {
    if (m > value_) {
      solver()->Fail();
    }
    expr_->SetMin(m);
  }

  int64 Max() const override { return std::min(expr_->Max(), value_); }

  void SetMax(int64 m) override {
    if (value_ > m) {
      expr_->SetMax(m);
    }
  }

  bool Bound() const override {
    return (expr_->Bound() || expr_->Min() >= value_);
  }

  std::string name() const override {
    return absl::StrFormat("MinCstIntExpr(%s, %d)", expr_->name(), value_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("MinCstIntExpr(%s, %d)", expr_->DebugString(),
                           value_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~MaxIntExpr() override {}

  int64 Min() const override { return std::max(left_->Min(), right_->Min()); }

  void SetMin(int64 m) override {
    if (left_->Max() < m) {
      right_->SetMin(m);
    } else {
      if (right_->Max() < m) {
        left_->SetMin(m);
      }
    }
  }

  int64 Max() const override { return std::max(left_->Max(), right_->Max()); }

  void SetMax(int64 m) override {
    left_->SetMax(m);
    right_->SetMax(m);
  }

  std::string name() const override {
    return absl::StrFormat("MaxIntExpr(%s, %s)", left_->name(), right_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("MaxIntExpr(%s, %s)", left_->DebugString(),
                           right_->DebugString());
  }

  void WhenRange(Demon* d) override {
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~MaxCstIntExpr() override {}

  int64 Min() const override { return std::max(expr_->Min(), value_); }

  void SetMin(int64 m) override {
    if (value_ < m) {
      expr_->SetMin(m);
    }
  }

  int64 Max() const override { return std::max(expr_->Max(), value_); }

  void SetMax(int64 m) override {
    if (m < value_) {
      solver()->Fail();
    }
    expr_->SetMax(m);
  }

  bool Bound() const override {
    return (expr_->Bound() || expr_->Max() <= value_);
  }

  std::string name() const override {
    return absl::StrFormat("MaxCstIntExpr(%s, %d)", expr_->name(), value_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("MaxCstIntExpr(%s, %d)", expr_->DebugString(),
                           value_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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
    DCHECK_GE(ec, int64{0});
    DCHECK_GE(lc, int64{0});
    DCHECK_GE(ld, ed);

    // If the penalty is 0, we can push the "confort zone or zone
    // of no cost towards infinity.
  }

  ~SimpleConvexPiecewiseExpr() override {}

  int64 Min() const override {
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

  void SetMin(int64 m) override {
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

  int64 Max() const override {
    const int64 vmin = expr_->Min();
    const int64 vmax = expr_->Max();
    const int64 mr = vmax > late_date_ ? (vmax - late_date_) * late_cost_ : 0;
    const int64 ml =
        vmin < early_date_ ? (early_date_ - vmin) * early_cost_ : 0;
    return std::max(mr, ml);
  }

  void SetMax(int64 m) override {
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

  std::string name() const override {
    return absl::StrFormat(
        "ConvexPiecewiseExpr(%s, ec = %d, ed = %d, ld = %d, lc = %d)",
        expr_->name(), early_cost_, early_date_, late_date_, late_cost_);
  }

  std::string DebugString() const override {
    return absl::StrFormat(
        "ConvexPiecewiseExpr(%s, ec = %d, ed = %d, ld = %d, lc = %d)",
        expr_->DebugString(), early_cost_, early_date_, late_date_, late_cost_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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
    DCHECK_GE(fixed_charge, int64{0});
    DCHECK_GT(step, int64{0});
  }

  ~SemiContinuousExpr() override {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return CapAdd(fixed_charge_, CapProd(x, step_));
    }
  }

  int64 Min() const override { return Value(expr_->Min()); }

  void SetMin(int64 m) override {
    if (m >= CapAdd(fixed_charge_, step_)) {
      const int64 y = PosIntDivUp(CapSub(m, fixed_charge_), step_);
      expr_->SetMin(y);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  int64 Max() const override { return Value(expr_->Max()); }

  void SetMax(int64 m) override {
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

  std::string name() const override {
    return absl::StrFormat("SemiContinuous(%s, fixed_charge = %d, step = %d)",
                           expr_->name(), fixed_charge_, step_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("SemiContinuous(%s, fixed_charge = %d, step = %d)",
                           expr_->DebugString(), fixed_charge_, step_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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
    DCHECK_GE(fixed_charge, int64{0});
  }

  ~SemiContinuousStepOneExpr() override {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_ + x;
    }
  }

  int64 Min() const override { return Value(expr_->Min()); }

  void SetMin(int64 m) override {
    if (m >= fixed_charge_ + 1) {
      expr_->SetMin(m - fixed_charge_);
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  int64 Max() const override { return Value(expr_->Max()); }

  void SetMax(int64 m) override {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_ + 1) {
      expr_->SetMax(0);
    } else {
      expr_->SetMax(m - fixed_charge_);
    }
  }

  std::string name() const override {
    return absl::StrFormat("SemiContinuousStepOne(%s, fixed_charge = %d)",
                           expr_->name(), fixed_charge_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("SemiContinuousStepOne(%s, fixed_charge = %d)",
                           expr_->DebugString(), fixed_charge_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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
    DCHECK_GT(fixed_charge, int64{0});
  }

  ~SemiContinuousStepZeroExpr() override {}

  int64 Value(int64 x) const {
    if (x <= 0) {
      return 0;
    } else {
      return fixed_charge_;
    }
  }

  int64 Min() const override { return Value(expr_->Min()); }

  void SetMin(int64 m) override {
    if (m >= fixed_charge_) {
      solver()->Fail();
    } else if (m > 0) {
      expr_->SetMin(1);
    }
  }

  int64 Max() const override { return Value(expr_->Max()); }

  void SetMax(int64 m) override {
    if (m < 0) {
      solver()->Fail();
    }
    if (m < fixed_charge_) {
      expr_->SetMax(0);
    }
  }

  std::string name() const override {
    return absl::StrFormat("SemiContinuousStepZero(%s, fixed_charge = %d)",
                           expr_->name(), fixed_charge_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("SemiContinuousStepZero(%s, fixed_charge = %d)",
                           expr_->DebugString(), fixed_charge_);
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~LinkExprAndVar() override {}

  void Post() override {
    Solver* const s = solver();
    Demon* d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    target_var_->WhenRange(d);
  }

  void InitialPropagate() override {
    expr_->SetRange(target_var_->Min(), target_var_->Max());
    int64 l, u;
    expr_->Range(&l, &u);
    target_var_->SetRange(l, u);
  }

  std::string DebugString() const override {
    return absl::StrFormat("cast(%s, %s)", expr_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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

// ----- Conditional Expression -----

class ExprWithEscapeValue : public BaseIntExpr {
 public:
  ExprWithEscapeValue(Solver* const s, IntVar* const c, IntExpr* const e,
                      int64 unperformed_value)
      : BaseIntExpr(s),
        condition_(c),
        expression_(e),
        unperformed_value_(unperformed_value) {}

  ~ExprWithEscapeValue() override {}

  int64 Min() const override {
    if (condition_->Min() == 1) {
      return expression_->Min();
    } else if (condition_->Max() == 1) {
      return std::min(unperformed_value_, expression_->Min());
    } else {
      return unperformed_value_;
    }
  }

  void SetMin(int64 m) override {
    if (m > unperformed_value_) {
      condition_->SetValue(1);
      expression_->SetMin(m);
    } else if (condition_->Min() == 1) {
      expression_->SetMin(m);
    } else if (m > expression_->Max()) {
      condition_->SetValue(0);
    }
  }

  int64 Max() const override {
    if (condition_->Min() == 1) {
      return expression_->Max();
    } else if (condition_->Max() == 1) {
      return std::max(unperformed_value_, expression_->Max());
    } else {
      return unperformed_value_;
    }
  }

  void SetMax(int64 m) override {
    if (m < unperformed_value_) {
      condition_->SetValue(1);
      expression_->SetMax(m);
    } else if (condition_->Min() == 1) {
      expression_->SetMax(m);
    } else if (m < expression_->Min()) {
      condition_->SetValue(0);
    }
  }

  void SetRange(int64 mi, int64 ma) override {
    if (ma < unperformed_value_ || mi > unperformed_value_) {
      condition_->SetValue(1);
      expression_->SetRange(mi, ma);
    } else if (condition_->Min() == 1) {
      expression_->SetRange(mi, ma);
    } else if (ma < expression_->Min() || mi > expression_->Max()) {
      condition_->SetValue(0);
    }
  }

  void SetValue(int64 v) override {
    if (v != unperformed_value_) {
      condition_->SetValue(1);
      expression_->SetValue(v);
    } else if (condition_->Min() == 1) {
      expression_->SetValue(v);
    } else if (v < expression_->Min() || v > expression_->Max()) {
      condition_->SetValue(0);
    }
  }

  bool Bound() const override {
    return condition_->Max() == 0 || expression_->Bound();
  }

  void WhenRange(Demon* d) override {
    expression_->WhenRange(d);
    condition_->WhenBound(d);
  }

  std::string DebugString() const override {
    return absl::StrFormat("ConditionExpr(%s, %s, %d)",
                           condition_->DebugString(),
                           expression_->DebugString(), unperformed_value_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kConditionalExpr, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            condition_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expression_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  unperformed_value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kConditionalExpr, this);
  }

 private:
  IntVar* const condition_;
  IntExpr* const expression_;
  const int64 unperformed_value_;
  DISALLOW_COPY_AND_ASSIGN(ExprWithEscapeValue);
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

  ~LinkExprAndDomainIntVar() override {}

  DomainIntVar* var() const {
    return reinterpret_cast<DomainIntVar*>(target_var_);
  }

  void Post() override {
    Solver* const s = solver();
    Demon* const d = s->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
    Demon* const target_var_demon = MakeConstraintDemon0(
        solver(), this, &LinkExprAndDomainIntVar::Propagate, "Propagate");
    target_var_->WhenRange(target_var_demon);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return absl::StrFormat("cast(%s, %s)", expr_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
}  //  namespace

// ----- Misc -----

IntVarIterator* BooleanVar::MakeHoleIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible, new EmptyIterator());
}
IntVarIterator* BooleanVar::MakeDomainIterator(bool reversible) const {
  return COND_REV_ALLOC(reversible, new RangeIterator(this));
}

// ----- API -----

void CleanVariableOnFail(IntVar* const var) {
  DCHECK_EQ(DOMAIN_INT_VAR, var->VarType());
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  dvar->CleanInProcess();
}

Constraint* SetIsEqual(IntVar* const var, const std::vector<int64>& values,
                       const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsEqual(values, vars);
}

Constraint* SetIsGreaterOrEqual(IntVar* const var,
                                const std::vector<int64>& values,
                                const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsGreaterOrEqual(values, vars);
}

void RestoreBoolValue(IntVar* const var) {
  DCHECK_EQ(BOOLEAN_VAR, var->VarType());
  BooleanVar* const boolean_var = reinterpret_cast<BooleanVar*>(var);
  boolean_var->RestoreValue();
}

// ----- API -----

IntVar* Solver::MakeIntVar(int64 min, int64 max, const std::string& name) {
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

IntVar* Solver::MakeIntVar(int64 min, int64 max) {
  return MakeIntVar(min, max, "");
}

IntVar* Solver::MakeBoolVar(const std::string& name) {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, name)));
}

IntVar* Solver::MakeBoolVar() {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, "")));
}

IntVar* Solver::MakeIntVar(const std::vector<int64>& values,
                           const std::string& name) {
  DCHECK(!values.empty());
  // Fast-track the case where we have a single value.
  if (values.size() == 1) return MakeIntConst(values[0], name);
  // Sort and remove duplicates.
  std::vector<int64> unique_sorted_values = values;
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
  int64 gcd = 0;
  for (const int64 v : unique_sorted_values) {
    if (gcd == 0) {
      gcd = std::abs(v);
    } else {
      gcd = MathUtil::GCD64(gcd, std::abs(v));  // Supports v==0.
    }
    if (gcd == 1) {
      // If it's 1, though, we can't do anything special, so we
      // immediately return a new DomainIntVar.
      return RegisterIntVar(
          RevAlloc(new DomainIntVar(this, unique_sorted_values, name)));
    }
  }
  DCHECK_GT(gcd, 1);
  for (int64& v : unique_sorted_values) {
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

IntVar* Solver::MakeIntVar(const std::vector<int64>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values,
                           const std::string& name) {
  return MakeIntVar(ToInt64Vector(values), name);
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntConst(int64 val, const std::string& name) {
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
std::string IndexedName(const std::string& prefix, int index, int max_index) {
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

void Solver::MakeIntVarArray(int var_count, int64 vmin, int64 vmax,
                             const std::string& name,
                             std::vector<IntVar*>* vars) {
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

IntExpr* Solver::MakeSum(IntExpr* const left, IntExpr* const right) {
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
    IntExpr* const result =
        AddOverflows(left->Max(), right->Max()) ||
                AddOverflows(left->Min(), right->Min())
            ? RegisterIntExpr(RevAlloc(new SafePlusIntExpr(this, left, right)))
            : RegisterIntExpr(RevAlloc(new PlusIntExpr(this, left, right)));
    model_cache_->InsertExprExprExpression(result, left, right,
                                           ModelCache::EXPR_EXPR_SUM);
    return result;
  }
}

IntExpr* Solver::MakeSum(IntExpr* const expr, int64 value) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(expr->Min() + value);
  }
  if (value == 0) {
    return expr;
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_SUM);
  if (result == nullptr) {
    if (expr->IsVar() && !AddOverflows(value, expr->Max()) &&
        !AddOverflows(value, expr->Min())) {
      IntVar* const var = expr->Var();
      switch (var->VarType()) {
        case DOMAIN_INT_VAR: {
          result = RegisterIntExpr(RevAlloc(new PlusCstDomainIntVar(
              this, reinterpret_cast<DomainIntVar*>(var), value)));
          break;
        }
        case CONST_VAR: {
          result = RegisterIntExpr(MakeIntConst(var->Min() + value));
          break;
        }
        case VAR_ADD_CST: {
          PlusCstVar* const add_var = reinterpret_cast<PlusCstVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = value + add_var->Constant();
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
          const int64 new_constant = value + add_var->Constant();
          result = RegisterIntExpr(
              RevAlloc(new SubCstIntVar(this, sub_var, new_constant)));
          break;
        }
        case OPP_VAR: {
          OppIntVar* const add_var = reinterpret_cast<OppIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          result =
              RegisterIntExpr(RevAlloc(new SubCstIntVar(this, sub_var, value)));
          break;
        }
        default:
          result =
              RegisterIntExpr(RevAlloc(new PlusCstIntVar(this, var, value)));
      }
    } else {
      result = RegisterIntExpr(RevAlloc(new PlusIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_SUM);
  }
  return result;
}

IntExpr* Solver::MakeDifference(IntExpr* const left, IntExpr* const right) {
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
  int64 left_coef = 1;
  int64 right_coef = 1;
  if (IsProduct(left, &sub_left, &left_coef) &&
      IsProduct(right, &sub_right, &right_coef)) {
    const int64 abs_gcd =
        MathUtil::GCD64(std::abs(left_coef), std::abs(right_coef));
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
IntExpr* Solver::MakeDifference(int64 value, IntExpr* const expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(value - expr->Min());
  }
  if (value == 0) {
    return MakeOpposite(expr);
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_DIFFERENCE);
  if (result == nullptr) {
    if (expr->IsVar() && expr->Min() != kint64min &&
        !SubOverflows(value, expr->Min()) &&
        !SubOverflows(value, expr->Max())) {
      IntVar* const var = expr->Var();
      switch (var->VarType()) {
        case VAR_ADD_CST: {
          PlusCstVar* const add_var = reinterpret_cast<PlusCstVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          const int64 new_constant = value - add_var->Constant();
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
          const int64 new_constant = value - add_var->Constant();
          result = MakeSum(sub_var, new_constant);
          break;
        }
        case OPP_VAR: {
          OppIntVar* const add_var = reinterpret_cast<OppIntVar*>(var);
          IntVar* const sub_var = add_var->SubVar();
          result = MakeSum(sub_var, value);
          break;
        }
        default:
          result =
              RegisterIntExpr(RevAlloc(new SubCstIntVar(this, var, value)));
      }
    } else {
      result = RegisterIntExpr(RevAlloc(new SubIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_DIFFERENCE);
  }
  return result;
}

IntExpr* Solver::MakeOpposite(IntExpr* const expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(-expr->Min());
  }
  IntExpr* result =
      Cache()->FindExprExpression(expr, ModelCache::EXPR_OPPOSITE);
  if (result == nullptr) {
    if (expr->IsVar()) {
      result = RegisterIntVar(RevAlloc(new OppIntExpr(this, expr))->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new OppIntExpr(this, expr)));
    }
    Cache()->InsertExprExpression(result, expr, ModelCache::EXPR_OPPOSITE);
  }
  return result;
}

IntExpr* Solver::MakeProd(IntExpr* const expr, int64 value) {
  CHECK_EQ(this, expr->solver());
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_PROD);
  if (result != nullptr) {
    return result;
  } else {
    IntExpr* m_expr = nullptr;
    int64 coefficient = 1;
    if (IsProduct(expr, &m_expr, &coefficient)) {
      coefficient *= value;
    } else {
      m_expr = expr;
      coefficient = value;
    }
    if (m_expr->Bound()) {
      return MakeIntConst(coefficient * m_expr->Min());
    } else if (coefficient == 1) {
      return m_expr;
    } else if (coefficient == -1) {
      return MakeOpposite(m_expr);
    } else if (coefficient > 0) {
      if (m_expr->Max() > kint64max / coefficient ||
          m_expr->Min() < kint64min / coefficient) {
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
    if (m_expr->IsVar() && !FLAGS_cp_disable_expression_optimization) {
      result = result->Var();
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
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

IntExpr* Solver::MakeProd(IntExpr* const left, IntExpr* const right) {
  if (left->Bound()) {
    return MakeProd(right, left->Min());
  }

  if (right->Bound()) {
    return MakeProd(left, right->Min());
  }

  // ----- Discover squares and powers -----

  IntExpr* m_left = left;
  IntExpr* m_right = right;
  int64 left_exponant = 1;
  int64 right_exponant = 1;
  ExtractPower(&m_left, &left_exponant);
  ExtractPower(&m_right, &right_exponant);

  if (m_left == m_right) {
    return MakePower(m_left, left_exponant + right_exponant);
  }

  // ----- Discover nested products -----

  m_left = left;
  m_right = right;
  int64 coefficient = 1;
  bool modified = false;

  ExtractProduct(&m_left, &coefficient, &modified);
  ExtractProduct(&m_right, &coefficient, &modified);
  if (modified) {
    return MakeProd(MakeProd(m_left, m_right), coefficient);
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
  if (left->IsVar() && left->Var()->VarType() == BOOLEAN_VAR) {
    if (right->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    }
  } else if (right->IsVar() &&
             reinterpret_cast<IntVar*>(right)->VarType() == BOOLEAN_VAR) {
    if (left->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    }
  } else if (left->Min() >= 0 && right->Min() >= 0) {
    if (CapProd(left->Max(), right->Max()) ==
        kint64max) {  // Potential overflow.
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

IntExpr* Solver::MakeDiv(IntExpr* const expr, int64 value) {
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

Constraint* Solver::MakeAbsEquality(IntVar* const var, IntVar* const abs_var) {
  if (Cache()->FindExprExpression(var, ModelCache::EXPR_ABS) == nullptr) {
    Cache()->InsertExprExpression(abs_var, var, ModelCache::EXPR_ABS);
  }
  return RevAlloc(new IntAbsConstraint(this, var, abs_var));
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

IntExpr* Solver::MakeSquare(IntExpr* const expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    const int64 v = expr->Min();
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

IntExpr* Solver::MakePower(IntExpr* const expr, int64 n) {
  CHECK_EQ(this, expr->solver());
  CHECK_GE(n, 0);
  if (expr->Bound()) {
    const int64 v = expr->Min();
    if (v >= OverflowLimit(n)) {  // Overflow.
      return MakeIntConst(kint64max);
    }
    return MakeIntConst(IntPower(v, n));
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

IntExpr* Solver::MakeMin(IntExpr* const left, IntExpr* const right) {
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

IntExpr* Solver::MakeMin(IntExpr* const expr, int64 value) {
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

IntExpr* Solver::MakeMin(IntExpr* const expr, int value) {
  return MakeMin(expr, static_cast<int64>(value));
}

IntExpr* Solver::MakeMax(IntExpr* const left, IntExpr* const right) {
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

IntExpr* Solver::MakeMax(IntExpr* const expr, int64 value) {
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

IntExpr* Solver::MakeMax(IntExpr* const expr, int value) {
  return MakeMax(expr, static_cast<int64>(value));
}

IntExpr* Solver::MakeConvexPiecewiseExpr(IntExpr* expr, int64 early_cost,
                                         int64 early_date, int64 late_date,
                                         int64 late_cost) {
  return RegisterIntExpr(RevAlloc(new SimpleConvexPiecewiseExpr(
      this, expr, early_cost, early_date, late_date, late_cost)));
}

IntExpr* Solver::MakeSemiContinuousExpr(IntExpr* const expr, int64 fixed_charge,
                                        int64 step) {
  if (step == 0) {
    if (fixed_charge == 0) {
      return MakeIntConst(int64{0});
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

// ----- Piecewise Linear -----

class PiecewiseLinearExpr : public BaseIntExpr {
 public:
  PiecewiseLinearExpr(Solver* solver, IntExpr* expr,
                      const PiecewiseLinearFunction& f)
      : BaseIntExpr(solver), expr_(expr), f_(f) {}
  ~PiecewiseLinearExpr() override {}
  int64 Min() const override {
    return f_.GetMinimum(expr_->Min(), expr_->Max());
  }
  void SetMin(int64 m) override {
    const auto& range =
        f_.GetSmallestRangeGreaterThanValue(expr_->Min(), expr_->Max(), m);
    expr_->SetRange(range.first, range.second);
  }

  int64 Max() const override {
    return f_.GetMaximum(expr_->Min(), expr_->Max());
  }

  void SetMax(int64 m) override {
    const auto& range =
        f_.GetSmallestRangeLessThanValue(expr_->Min(), expr_->Max(), m);
    expr_->SetRange(range.first, range.second);
  }

  void SetRange(int64 l, int64 u) override {
    const auto& range =
        f_.GetSmallestRangeInValueRange(expr_->Min(), expr_->Max(), l, u);
    expr_->SetRange(range.first, range.second);
  }
  std::string name() const override {
    return absl::StrFormat("PiecewiseLinear(%s, f = %s)", expr_->name(),
                           f_.DebugString());
  }

  std::string DebugString() const override {
    return absl::StrFormat("PiecewiseLinear(%s, f = %s)", expr_->DebugString(),
                           f_.DebugString());
  }

  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
    // TODO(user): Implement visitor.
  }

 private:
  IntExpr* const expr_;
  const PiecewiseLinearFunction f_;
};

IntExpr* Solver::MakePiecewiseLinearExpr(IntExpr* expr,
                                         const PiecewiseLinearFunction& f) {
  return RegisterIntExpr(RevAlloc(new PiecewiseLinearExpr(this, expr, f)));
}

// ----- Conditional Expression -----

IntExpr* Solver::MakeConditionalExpression(IntVar* const condition,
                                           IntExpr* const expr,
                                           int64 unperformed_value) {
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
    case 0: {
      return;
    }
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
          if (u > l + 1) {
            RemoveInterval(l + 1, u - 1);
          }
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
      // STLSortAndRemoveDuplicates from ortools/base/stl_util.h to the
      // existing open_source/base/stl_util.h and using it here.
      // TODO(user): We could filter out values not in the var.
      std::vector<int64>& tmp = solver()->tmp_vector_;
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
  SubIntExpr* const sub_expr = dynamic_cast<SubIntExpr*>(expr);
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
  if (dynamic_cast<TimesCstIntVar*>(expr) != nullptr) {
    TimesCstIntVar* const var = dynamic_cast<TimesCstIntVar*>(expr);
    *coefficient = var->Constant();
    *inner_expr = var->SubVar();
    return true;
  } else if (dynamic_cast<TimesIntCstExpr*>(expr) != nullptr) {
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
