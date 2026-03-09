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

#include "ortools/constraint_solver/variables.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/model_cache.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/constraint_solver/trace.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355)
#endif

ABSL_DECLARE_FLAG(bool, cp_share_int_consts);

namespace operations_research {

template <typename T>
T* CondRevAlloc(Solver* solver, bool reversible, T* object) {
  return reversible ? solver->RevAlloc(object) : object;
}

// ----- Boolean variable -----

void BooleanVar::SetMin(int64_t m) {
  if (m <= 0) return;
  if (m > 1) solver()->Fail();
  SetValue(1);
}

void BooleanVar::SetMax(int64_t m) {
  if (m >= 1) return;
  if (m < 0) solver()->Fail();
  SetValue(0);
}

void BooleanVar::SetRange(int64_t mi, int64_t ma) {
  if (mi > 1 || ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi == 1) {
    SetValue(1);
  } else if (ma == 0) {
    SetValue(0);
  }
}

void BooleanVar::RemoveValue(int64_t v) {
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

void BooleanVar::RemoveInterval(int64_t l, int64_t u) {
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

uint64_t BooleanVar::Size() const {
  return (1 + (value_ == kUnboundBooleanVarValue));
}

bool BooleanVar::Contains(int64_t v) const {
  return ((v == 0 && value_ != 1) || (v == 1 && value_ != 0));
}

IntVar* BooleanVar::IsEqual(int64_t constant) {
  if (constant > 1 || constant < 0) {
    return solver()->MakeIntConst(0);
  }
  if (constant == 1) {
    return this;
  } else {  // constant == 0.
    return solver()->MakeDifference(1, this)->Var();
  }
}

IntVar* BooleanVar::IsDifferent(int64_t constant) {
  if (constant > 1 || constant < 0) {
    return solver()->MakeIntConst(1);
  }
  if (constant == 1) {
    return solver()->MakeDifference(1, this)->Var();
  } else {  // constant == 0.
    return this;
  }
}

IntVar* BooleanVar::IsGreaterOrEqual(int64_t constant) {
  if (constant > 1) {
    return solver()->MakeIntConst(0);
  } else if (constant <= 0) {
    return solver()->MakeIntConst(1);
  } else {
    return this;
  }
}

IntVar* BooleanVar::IsLessOrEqual(int64_t constant) {
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

// ----- Misc -----

class EmptyIterator : public IntVarIterator {
 public:
  ~EmptyIterator() override {}
  void Init() override {}
  bool Ok() const override { return false; }
  int64_t Value() const override {
    LOG(FATAL) << "Should not be called";
    return 0LL;
  }
  void Next() override {}
};

class RangeIterator : public IntVarIterator {
 public:
  explicit RangeIterator(const IntVar* var)
      : var_(var),
        min_(std::numeric_limits<int64_t>::max()),
        max_(std::numeric_limits<int64_t>::min()),
        current_(-1) {}

  ~RangeIterator() override {}

  void Init() override {
    min_ = var_->Min();
    max_ = var_->Max();
    current_ = min_;
  }

  bool Ok() const override { return current_ <= max_; }

  int64_t Value() const override { return current_; }

  void Next() override { current_++; }

 private:
  const IntVar* const var_;
  int64_t min_;
  int64_t max_;
  int64_t current_;
};

IntVarIterator* BooleanVar::MakeHoleIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible, new EmptyIterator());
}
IntVarIterator* BooleanVar::MakeDomainIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible, new RangeIterator(this));
}

// ----- Domain Int Var: base class for variables -----
// It Contains bounds and a bitset representation of possible values.
// DomainIntVar Out-Of-Line Methods

class DomainIntVar::BitSetIterator : public BaseObject {
 public:
  BitSetIterator(uint64_t* bitset, int64_t omin)
      : bitset_(bitset),
        omin_(omin),
        max_(std::numeric_limits<int64_t>::min()),
        current_(std::numeric_limits<int64_t>::max()) {}

  ~BitSetIterator() override {}

  void Init(int64_t min, int64_t max) {
    max_ = max;
    current_ = min;
  }

  bool Ok() const { return current_ <= max_; }

  int64_t Value() const { return current_; }

  void Next() {
    if (++current_ <= max_) {
      current_ = UnsafeLeastSignificantBitPosition64(bitset_, current_ - omin_,
                                                     max_ - omin_) +
                 omin_;
    }
  }

  std::string DebugString() const override { return "BitSetIterator"; }

 private:
  uint64_t* const bitset_;
  const int64_t omin_;
  int64_t max_;
  int64_t current_;
};

class DomainIntVar::BitSet : public BaseObject {
 public:
  explicit BitSet(Solver* s) : solver_(s), holes_stamp_(0) {}
  ~BitSet() override {}

  virtual int64_t ComputeNewMin(int64_t nmin, int64_t cmin, int64_t cmax) = 0;
  virtual int64_t ComputeNewMax(int64_t nmax, int64_t cmin, int64_t cmax) = 0;
  virtual bool Contains(int64_t val) const = 0;
  virtual bool SetValue(int64_t val) = 0;
  virtual bool RemoveValue(int64_t val) = 0;
  virtual uint64_t Size() const = 0;
  virtual void DelayRemoveValue(int64_t val) = 0;
  virtual void ApplyRemovedValues(DomainIntVar* var) = 0;
  virtual void ClearRemovedValues() = 0;
  virtual std::string pretty_DebugString(int64_t min, int64_t max) const = 0;
  virtual BitSetIterator* MakeIterator() = 0;

  void InitHoles() {
    const uint64_t current_stamp = solver_->stamp();
    if (holes_stamp_ < current_stamp) {
      holes_.clear();
      holes_stamp_ = current_stamp;
    }
  }

  virtual void ClearHoles() { holes_.clear(); }

  const std::vector<int64_t>& Holes() { return holes_; }

  void AddHole(int64_t value) { holes_.push_back(value); }

  int NumHoles() const {
    return holes_stamp_ < solver_->stamp() ? 0 : holes_.size();
  }

 protected:
  Solver* const solver_;

 private:
  std::vector<int64_t> holes_;
  uint64_t holes_stamp_;
};

DomainIntVar::QueueHandler::QueueHandler(DomainIntVar* var) : var_(var) {}
DomainIntVar::QueueHandler::~QueueHandler() {}
void DomainIntVar::QueueHandler::Run(Solver* s) {
  s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
  var_->Process();
  s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
}
Solver::DemonPriority DomainIntVar::QueueHandler::priority() const {
  return Solver::VAR_PRIORITY;
}
std::string DomainIntVar::QueueHandler::DebugString() const {
  return absl::StrFormat("Handler(%s)", var_->DebugString());
}

// This class stores the watchers variables attached to values. It is
// reversible and it helps maintaining the set of 'active' watchers
// (variables not bound to a single value).
template <class T>
class DomainIntVar::RevIntPtrMap {
 public:
  RevIntPtrMap(Solver* solver, int64_t rmin, int64_t)
      : solver_(solver), range_min_(rmin), start_(0) {}

  ~RevIntPtrMap() {}

  bool Empty() const { return start_.Value() == elements_.size(); }

  void SortActive() { std::sort(elements_.begin(), elements_.end()); }

  // Access with value API.

  // Add the pointer to the map attached to the given value.
  void UnsafeRevInsert(int64_t value, T* elem) {
    elements_.push_back(std::make_pair(value, elem));
    if (solver_->state() != Solver::OUTSIDE_SEARCH) {
      solver_->AddBacktrackAction([this, value](Solver*) { Uninsert(value); },
                                  false);
    }
  }

  T* FindPtrOrNull(int64_t value, int* position) {
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
      const std::pair<int64_t, T*> copy = elements_[start];
      elements_[start] = elements_[position];
      elements_[position] = copy;
    }
    start_.Incr(solver_);
  }

  const std::pair<int64_t, T*>& At(int position) const {
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
  void Uninsert(int64_t value) {
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
  const int64_t range_min_;
  NumericalRev<int> start_;
  std::vector<std::pair<int64_t, T*>> elements_;
};

// Base class for value watchers
class DomainIntVar::BaseValueWatcher : public Constraint {
 public:
  explicit BaseValueWatcher(Solver* solver) : Constraint(solver) {}

  ~BaseValueWatcher() override {}

  virtual IntVar* GetOrMakeValueWatcher(int64_t value) = 0;

  virtual void SetValueWatcher(IntVar* boolvar, int64_t value) = 0;
};

// This class monitors the domain of the variable and updates the
// IsEqual/IsDifferent boolean variables accordingly.
class DomainIntVar::ValueWatcher : public DomainIntVar::BaseValueWatcher {
 public:
  class WatchDemon : public Demon {
   public:
    WatchDemon(ValueWatcher* watcher, int64_t value, IntVar* var)
        : value_watcher_(watcher), value_(value), var_(var) {}
    ~WatchDemon() override {}

    void Run(Solver*) override {
      value_watcher_->ProcessValueWatcher(value_, var_);
    }

   private:
    ValueWatcher* const value_watcher_;
    const int64_t value_;
    IntVar* const var_;
  };

  class VarDemon : public Demon {
   public:
    explicit VarDemon(ValueWatcher* watcher) : value_watcher_(watcher) {}

    ~VarDemon() override {}

    void Run(Solver*) override { value_watcher_->ProcessVar(); }

   private:
    ValueWatcher* const value_watcher_;
  };

  ValueWatcher(Solver* solver, DomainIntVar* variable)
      : BaseValueWatcher(solver),
        variable_(variable),
        hole_iterator_(variable_->MakeHoleIterator(true)),
        var_demon_(nullptr),
        watchers_(solver, variable->Min(), variable->Max()) {}

  ~ValueWatcher() override {}

  IntVar* GetOrMakeValueWatcher(int64_t value) override {
    IntVar* const watcher = watchers_.FindPtrOrNull(value, nullptr);
    if (watcher != nullptr) return watcher;
    if (variable_->Contains(value)) {
      if (variable_->Bound()) {
        return solver()->MakeIntConst(1);
      } else {
        const std::string vname =
            variable_->HasName() ? variable_->name() : variable_->DebugString();
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

  void SetValueWatcher(IntVar* boolvar, int64_t value) override {
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
      const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
      const int64_t value = w.first;
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
        const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
        const int64_t value = w.first;
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

  void ProcessValueWatcher(int64_t value, IntVar* boolvar) {
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
          for (const int64_t hole : InitAndGetValues(hole_iterator_)) {
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
    const int64_t value = variable_->Min();
    for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
      const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
      w.second->SetValue(w.first == value);
    }
    watchers_.RemoveAll();
    var_demon_->inhibit(solver());
  }

  // Scans all the watchers to check and assign them.
  void ScanWatchers() {
    for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
      const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kVarValueWatcher, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            variable_);
    std::vector<int64_t> all_coefficients;
    std::vector<IntVar*> all_bool_vars;
    for (int position = watchers_.start(); position < watchers_.end();
         ++position) {
      const std::pair<int64_t, IntVar*>& w = watchers_.At(position);
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
class DomainIntVar::DenseValueWatcher : public DomainIntVar::BaseValueWatcher {
 public:
  class WatchDemon : public Demon {
   public:
    WatchDemon(DenseValueWatcher* watcher, int64_t value, IntVar* var)
        : value_watcher_(watcher), value_(value), var_(var) {}
    ~WatchDemon() override {}

    void Run(Solver*) override {
      value_watcher_->ProcessValueWatcher(value_, var_);
    }

   private:
    DenseValueWatcher* const value_watcher_;
    const int64_t value_;
    IntVar* const var_;
  };

  class VarDemon : public Demon {
   public:
    explicit VarDemon(DenseValueWatcher* watcher) : value_watcher_(watcher) {}

    ~VarDemon() override {}

    void Run(Solver*) override { value_watcher_->ProcessVar(); }

   private:
    DenseValueWatcher* const value_watcher_;
  };

  DenseValueWatcher(Solver* solver, DomainIntVar* variable)
      : BaseValueWatcher(solver),
        variable_(variable),
        hole_iterator_(variable_->MakeHoleIterator(true)),
        var_demon_(nullptr),
        offset_(variable->Min()),
        watchers_(variable->Max() - variable->Min() + 1, nullptr),
        active_watchers_(0) {}

  ~DenseValueWatcher() override {}

  IntVar* GetOrMakeValueWatcher(int64_t value) override {
    const int64_t var_max = offset_ + watchers_.size() - 1;  // Bad cast.
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
        const std::string vname =
            variable_->HasName() ? variable_->name() : variable_->DebugString();
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

  void SetValueWatcher(IntVar* boolvar, int64_t value) override {
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
      const int64_t value = pos + offset_;
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
        const int64_t value = pos + offset_;
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

  void ProcessValueWatcher(int64_t value, IntVar* boolvar) {
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
    const int64_t value = variable_->Min();
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
    const int64_t old_min_index = variable_->OldMin() - offset_;
    const int64_t old_max_index = variable_->OldMax() - offset_;
    const int64_t min_index = variable_->Min() - offset_;
    const int64_t max_index = variable_->Max() - offset_;
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
        for (const int64_t hole : InitAndGetValues(hole_iterator_)) {
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kVarValueWatcher, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            variable_);
    std::vector<int64_t> all_coefficients;
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
  const int64_t offset_;
  std::vector<IntVar*> watchers_;
  NumericalRev<int> active_watchers_;
};

class DomainIntVar::BaseUpperBoundWatcher : public Constraint {
 public:
  explicit BaseUpperBoundWatcher(Solver* solver) : Constraint(solver) {}

  ~BaseUpperBoundWatcher() override {}

  virtual IntVar* GetOrMakeUpperBoundWatcher(int64_t value) = 0;

  virtual void SetUpperBoundWatcher(IntVar* boolvar, int64_t value) = 0;
};

// This class watches the bounds of the variable and updates the
// IsGreater/IsGreaterOrEqual/IsLess/IsLessOrEqual demons
// accordingly.
class DomainIntVar::UpperBoundWatcher
    : public DomainIntVar::BaseUpperBoundWatcher {
 public:
  class WatchDemon : public Demon {
   public:
    WatchDemon(UpperBoundWatcher* watcher, int64_t index, IntVar* var)
        : value_watcher_(watcher), index_(index), var_(var) {}
    ~WatchDemon() override {}

    void Run(Solver*) override {
      value_watcher_->ProcessUpperBoundWatcher(index_, var_);
    }

   private:
    UpperBoundWatcher* const value_watcher_;
    const int64_t index_;
    IntVar* const var_;
  };

  class VarDemon : public Demon {
   public:
    explicit VarDemon(UpperBoundWatcher* watcher) : value_watcher_(watcher) {}
    ~VarDemon() override {}

    void Run(Solver*) override { value_watcher_->ProcessVar(); }

   private:
    UpperBoundWatcher* const value_watcher_;
  };

  UpperBoundWatcher(Solver* solver, DomainIntVar* variable)
      : BaseUpperBoundWatcher(solver),
        variable_(variable),
        var_demon_(nullptr),
        watchers_(solver, variable->Min(), variable->Max()),
        start_(0),
        end_(0),
        sorted_(false) {}

  ~UpperBoundWatcher() override {}

  IntVar* GetOrMakeUpperBoundWatcher(int64_t value) override {
    IntVar* const watcher = watchers_.FindPtrOrNull(value, nullptr);
    if (watcher != nullptr) {
      return watcher;
    }
    if (variable_->Max() >= value) {
      if (variable_->Min() >= value) {
        return solver()->MakeIntConst(1);
      } else {
        const std::string vname =
            variable_->HasName() ? variable_->name() : variable_->DebugString();
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

  void SetUpperBoundWatcher(IntVar* boolvar, int64_t value) override {
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
      const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
      IntVar* const boolvar = w.second;
      const int64_t value = w.first;
      if (!boolvar->Bound() && value > variable_->Min() &&
          value <= variable_->Max()) {
        boolvar->WhenBound(
            solver()->RevAlloc(new WatchDemon(this, value, boolvar)));
      }
    }
    posted_.Switch(solver());
  }

  void InitialPropagate() override {
    const int64_t var_min = variable_->Min();
    const int64_t var_max = variable_->Max();
    if (sorted_) {
      while (start_.Value() <= end_.Value()) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(start_.Value());
        if (w.first <= var_min) {
          w.second->SetValue(1);
          start_.Incr(solver());
        } else {
          break;
        }
      }
      while (end_.Value() >= start_.Value()) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(end_.Value());
        if (w.first > var_max) {
          w.second->SetValue(0);
          end_.Decr(solver());
        } else {
          break;
        }
      }
      for (int i = start_.Value(); i <= end_.Value(); ++i) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(i);
        if (w.second->Bound()) {
          ProcessUpperBoundWatcher(w.first, w.second);
        }
      }
      if (start_.Value() > end_.Value()) {
        var_demon_->inhibit(solver());
      }
    } else {
      for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
        const int64_t value = w.first;
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            variable_);
    std::vector<int64_t> all_coefficients;
    std::vector<IntVar*> all_bool_vars;
    for (int pos = watchers_.start(); pos < watchers_.end(); ++pos) {
      const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
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
  void ProcessUpperBoundWatcher(int64_t value, IntVar* boolvar) {
    if (boolvar->Min() == 0) {
      variable_->SetMax(value - 1);
    } else {
      variable_->SetMin(value);
    }
  }

  void ProcessVar() {
    const int64_t var_min = variable_->Min();
    const int64_t var_max = variable_->Max();
    if (sorted_) {
      while (start_.Value() <= end_.Value()) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(start_.Value());
        if (w.first <= var_min) {
          w.second->SetValue(1);
          start_.Incr(solver());
        } else {
          break;
        }
      }
      while (end_.Value() >= start_.Value()) {
        const std::pair<int64_t, IntVar*>& w = watchers_.At(end_.Value());
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
        const std::pair<int64_t, IntVar*>& w = watchers_.At(pos);
        const int64_t value = w.first;
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
class DomainIntVar::DenseUpperBoundWatcher
    : public DomainIntVar::BaseUpperBoundWatcher {
 public:
  class WatchDemon : public Demon {
   public:
    WatchDemon(DenseUpperBoundWatcher* watcher, int64_t value, IntVar* var)
        : value_watcher_(watcher), value_(value), var_(var) {}
    ~WatchDemon() override {}

    void Run(Solver*) override {
      value_watcher_->ProcessUpperBoundWatcher(value_, var_);
    }

   private:
    DenseUpperBoundWatcher* const value_watcher_;
    const int64_t value_;
    IntVar* const var_;
  };

  class VarDemon : public Demon {
   public:
    explicit VarDemon(DenseUpperBoundWatcher* watcher)
        : value_watcher_(watcher) {}

    ~VarDemon() override {}

    void Run(Solver*) override { value_watcher_->ProcessVar(); }

   private:
    DenseUpperBoundWatcher* const value_watcher_;
  };

  DenseUpperBoundWatcher(Solver* solver, DomainIntVar* variable)
      : BaseUpperBoundWatcher(solver),
        variable_(variable),
        var_demon_(nullptr),
        offset_(variable->Min()),
        watchers_(variable->Max() - variable->Min() + 1, nullptr),
        active_watchers_(0) {}

  ~DenseUpperBoundWatcher() override {}

  IntVar* GetOrMakeUpperBoundWatcher(int64_t value) override {
    if (variable_->Max() >= value) {
      if (variable_->Min() >= value) {
        return solver()->MakeIntConst(1);
      } else {
        const std::string vname =
            variable_->HasName() ? variable_->name() : variable_->DebugString();
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

  void SetUpperBoundWatcher(IntVar* boolvar, int64_t value) override {
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
      const int64_t value = pos + offset_;
      IntVar* const boolvar = watchers_[pos];
      if (boolvar != nullptr && !boolvar->Bound() && value > variable_->Min() &&
          value <= variable_->Max()) {
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
      const int64_t value = pos + offset_;
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

  void ProcessUpperBoundWatcher(int64_t value, IntVar* boolvar) {
    if (boolvar->Min() == 0) {
      variable_->SetMax(value - 1);
    } else {
      variable_->SetMin(value);
    }
  }

  void ProcessVar() {
    const int64_t old_min_index = variable_->OldMin() - offset_;
    const int64_t old_max_index = variable_->OldMax() - offset_;
    const int64_t min_index = variable_->Min() - offset_;
    const int64_t max_index = variable_->Max() - offset_;
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kVarBoundWatcher, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            variable_);
    std::vector<int64_t> all_coefficients;
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
  const int64_t offset_;
  std::vector<IntVar*> watchers_;
  NumericalRev<int> active_watchers_;
};

// ----- Main Class -----

int64_t DomainIntVar::Min() const { return min_.Value(); }
int64_t DomainIntVar::Max() const { return max_.Value(); }
bool DomainIntVar::Bound() const { return (min_.Value() == max_.Value()); }
int64_t DomainIntVar::Value() const {
  CHECK_EQ(min_.Value(), max_.Value())
      << " variable " << DebugString() << " is not bound.";
  return min_.Value();
}
void DomainIntVar::WhenBound(Demon* d) {
  if (min_.Value() != max_.Value()) {
    if (d->priority() == Solver::DELAYED_PRIORITY) {
      delayed_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    } else {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
}
void DomainIntVar::WhenRange(Demon* d) {
  if (min_.Value() != max_.Value()) {
    if (d->priority() == Solver::DELAYED_PRIORITY) {
      delayed_range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    } else {
      range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
}
void DomainIntVar::WhenDomain(Demon* d) {
  if (min_.Value() != max_.Value()) {
    if (d->priority() == Solver::DELAYED_PRIORITY) {
      delayed_domain_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    } else {
      domain_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
}

IntVar* DomainIntVar::IsEqual(int64_t constant) {
  Solver* const s = solver();
  if (constant == min_.Value() && value_watcher_ == nullptr) {
    return s->MakeIsLessOrEqualCstVar(this, constant);
  }
  if (constant == max_.Value() && value_watcher_ == nullptr) {
    return s->MakeIsGreaterOrEqualCstVar(this, constant);
  }
  if (!Contains(constant)) {
    return s->MakeIntConst(int64_t{0});
  }
  if (Bound() && min_.Value() == constant) {
    return s->MakeIntConst(int64_t{1});
  }
  IntExpr* const cache = s->Cache()->FindExprConstantExpression(
      this, constant, ModelCache::EXPR_CONSTANT_IS_EQUAL);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    if (value_watcher_ == nullptr) {
      if (CapSub(Max(), Min()) <= 256) {
        solver()->SaveAndSetValue(reinterpret_cast<void**>(&value_watcher_),
                                  reinterpret_cast<void*>(solver()->RevAlloc(
                                      new DenseValueWatcher(solver(), this))));

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

Constraint* DomainIntVar::SetIsEqual(absl::Span<const int64_t> values,
                                     const std::vector<IntVar*>& vars) {
  if (value_watcher_ == nullptr) {
    solver()->SaveAndSetValue(reinterpret_cast<void**>(&value_watcher_),
                              reinterpret_cast<void*>(solver()->RevAlloc(
                                  new ValueWatcher(solver(), this))));
    DCHECK(value_watcher_ != nullptr);
    for (int i = 0; i < vars.size(); ++i) {
      value_watcher_->SetValueWatcher(vars[i], values[i]);
    }
  }
  return value_watcher_;
}

IntVar* DomainIntVar::IsDifferent(int64_t constant) {
  Solver* const s = solver();
  if (constant == min_.Value() && value_watcher_ == nullptr) {
    return s->MakeIsGreaterOrEqualCstVar(this, constant + 1);
  }
  if (constant == max_.Value() && value_watcher_ == nullptr) {
    return s->MakeIsLessOrEqualCstVar(this, constant - 1);
  }
  if (!Contains(constant)) {
    return s->MakeIntConst(int64_t{1});
  }
  if (Bound() && min_.Value() == constant) {
    return s->MakeIntConst(int64_t{0});
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

IntVar* DomainIntVar::IsGreaterOrEqual(int64_t constant) {
  Solver* const s = solver();
  if (max_.Value() < constant) {
    return s->MakeIntConst(int64_t{0});
  }
  if (min_.Value() >= constant) {
    return s->MakeIntConst(int64_t{1});
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
        solver()->SaveAndSetValue(reinterpret_cast<void**>(&bound_watcher_),
                                  reinterpret_cast<void*>(solver()->RevAlloc(
                                      new UpperBoundWatcher(solver(), this))));
        solver()->AddConstraint(bound_watcher_);
      }
    }
    IntVar* const boolvar =
        bound_watcher_->GetOrMakeUpperBoundWatcher(constant);
    s->Cache()->InsertExprConstantExpression(
        boolvar, this, constant, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
    return boolvar;
  }
}

Constraint* DomainIntVar::SetIsGreaterOrEqual(
    absl::Span<const int64_t> values, const std::vector<IntVar*>& vars) {
  if (bound_watcher_ == nullptr) {
    if (CapSub(Max(), Min()) <= 256) {
      solver()->SaveAndSetValue(
          reinterpret_cast<void**>(&bound_watcher_),
          reinterpret_cast<void*>(
              solver()->RevAlloc(new DenseUpperBoundWatcher(solver(), this))));
      DCHECK(bound_watcher_ != nullptr);
      solver()->AddConstraint(bound_watcher_);
    } else {
      solver()->SaveAndSetValue(reinterpret_cast<void**>(&bound_watcher_),
                                reinterpret_cast<void*>(solver()->RevAlloc(
                                    new UpperBoundWatcher(solver(), this))));
      DCHECK(bound_watcher_ != nullptr);
      solver()->AddConstraint(bound_watcher_);
    }
    for (int i = 0; i < values.size(); ++i) {
      bound_watcher_->SetUpperBoundWatcher(vars[i], values[i]);
    }
  }
  return bound_watcher_;
}

IntVar* DomainIntVar::IsLessOrEqual(int64_t constant) {
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

uint64_t DomainIntVar::Size() const {
  if (bits_ != nullptr) return bits_->Size();
  return (static_cast<uint64_t>(max_.Value()) -
          static_cast<uint64_t>(min_.Value()) + 1);
}
bool DomainIntVar::Contains(int64_t v) const {
  if (v < min_.Value() || v > max_.Value()) return false;
  return (bits_ == nullptr ? true : bits_->Contains(v));
}
int64_t DomainIntVar::OldMin() const {
  return std::min(old_min_, min_.Value());
}
int64_t DomainIntVar::OldMax() const {
  return std::max(old_max_, max_.Value());
}

DomainIntVar::BitSet* DomainIntVar::bitset() const { return bits_; }
int DomainIntVar::VarType() const { return DOMAIN_INT_VAR; }
std::string DomainIntVar::BaseName() const { return "IntegerVar"; }

void DomainIntVar::CheckOldMin() {
  if (old_min_ > min_.Value()) {
    old_min_ = min_.Value();
  }
}
void DomainIntVar::CheckOldMax() {
  if (old_max_ < max_.Value()) {
    old_max_ = max_.Value();
  }
}

// ----- BitSet -----

// Return whether an integer interval [a..b] (inclusive) contains at most
// K values, i.e. b - a < K, in a way that's robust to overflows.
// For performance reasons, in opt mode it doesn't check that [a, b] is a
// valid interval, nor that K is nonnegative.
inline bool ClosedIntervalNoLargerThan(int64_t a, int64_t b, int64_t K) {
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
  SimpleBitSet(Solver* s, int64_t vmin, int64_t vmax)
      : BitSet(s),
        bits_(nullptr),
        stamps_(nullptr),
        omin_(vmin),
        omax_(vmax),
        size_(vmax - vmin + 1),
        bsize_(BitLength64(size_.Value())) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 0xFFFFFFFF))
        << "Bitset too large: [" << vmin << ", " << vmax << "]";
    bits_ = std::unique_ptr<uint64_t[]>(new uint64_t[bsize_]);
    stamps_ = std::unique_ptr<uint64_t[]>(new uint64_t[bsize_]);
    for (int i = 0; i < bsize_; ++i) {
      const int bs =
          (i == size_.Value() - 1) ? 63 - BitPos64(size_.Value()) : 0;
      bits_[i] = kAllBits64 >> bs;
      stamps_[i] = s->stamp() - 1;
    }
  }

  SimpleBitSet(Solver* s, absl::Span<const int64_t> sorted_values, int64_t vmin,
               int64_t vmax)
      : BitSet(s),
        bits_(nullptr),
        stamps_(nullptr),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()),
        bsize_(BitLength64(vmax - vmin + 1)) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 0xFFFFFFFF))
        << "Bitset too large: [" << vmin << ", " << vmax << "]";
    bits_ = std::unique_ptr<uint64_t[]>(new uint64_t[bsize_]);
    stamps_ = std::unique_ptr<uint64_t[]>(new uint64_t[bsize_]);
    for (int i = 0; i < bsize_; ++i) {
      bits_[i] = uint64_t{0};
      stamps_[i] = s->stamp() - 1;
    }
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64_t val = sorted_values[i];
      DCHECK(!bit(val));
      const int offset = BitOffset64(val - omin_);
      const int pos = BitPos64(val - omin_);
      bits_[offset] |= OneBit64(pos);
    }
  }

  ~SimpleBitSet() override = default;

  bool bit(int64_t val) const { return IsBitSet64(bits_.get(), val - omin_); }

  int64_t ComputeNewMin(int64_t nmin, int64_t cmin, int64_t cmax) override {
    DCHECK_GE(nmin, cmin);
    DCHECK_LE(nmin, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    const int64_t new_min = UnsafeLeastSignificantBitPosition64(
                                bits_.get(), nmin - omin_, cmax - omin_) +
                            omin_;
    const uint64_t removed_bits =
        BitCountRange64(bits_.get(), cmin - omin_, new_min - omin_ - 1);
    size_.Add(solver_, -removed_bits);
    return new_min;
  }

  int64_t ComputeNewMax(int64_t nmax, int64_t cmin, int64_t cmax) override {
    DCHECK_GE(nmax, cmin);
    DCHECK_LE(nmax, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    const int64_t new_max = UnsafeMostSignificantBitPosition64(
                                bits_.get(), cmin - omin_, nmax - omin_) +
                            omin_;
    const uint64_t removed_bits =
        BitCountRange64(bits_.get(), new_max - omin_ + 1, cmax - omin_);
    size_.Add(solver_, -removed_bits);
    return new_max;
  }

  bool SetValue(int64_t val) override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      size_.SetValue(solver_, 1);
      return true;
    }
    return false;
  }

  bool Contains(int64_t val) const override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  bool RemoveValue(int64_t val) override {
    if (val < omin_ || val > omax_ || !bit(val)) {
      return false;
    }
    // Bitset.
    const int64_t val_offset = val - omin_;
    const int offset = BitOffset64(val_offset);
    const uint64_t current_stamp = solver_->stamp();
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
  uint64_t Size() const override { return size_.Value(); }

  std::string DebugString() const override {
    std::string out;
    absl::StrAppendFormat(&out, "SimpleBitSet(%d..%d : ", omin_, omax_);
    for (int i = 0; i < bsize_; ++i) {
      absl::StrAppendFormat(&out, "%x", bits_[i]);
    }
    out += ")";
    return out;
  }

  void DelayRemoveValue(int64_t val) override { removed_.push_back(val); }

  void ApplyRemovedValues(DomainIntVar* var) override {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64_t>::iterator it = removed_.begin();
         it != removed_.end(); ++it) {
      var->RemoveValue(*it);
    }
  }

  void ClearRemovedValues() override { removed_.clear(); }

  std::string pretty_DebugString(int64_t min, int64_t max) const override {
    std::string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64_t start_cumul = min;
      for (int64_t v = min + 1; v < max; ++v) {
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
    return new DomainIntVar::BitSetIterator(bits_.get(), omin_);
  }

 private:
  std::unique_ptr<uint64_t[]> bits_;
  std::unique_ptr<uint64_t[]> stamps_;
  const int64_t omin_;
  const int64_t omax_;
  NumericalRev<int64_t> size_;
  const int bsize_;
  std::vector<int64_t> removed_;
};

// This is a special case where the bitset fits into one 64 bit integer.
// In that case, there are no offset to compute.
// Overflows are caught by the robust ClosedIntervalNoLargerThan() method.
class SmallBitSet : public DomainIntVar::BitSet {
 public:
  SmallBitSet(Solver* s, int64_t vmin, int64_t vmax)
      : BitSet(s),
        bits_(uint64_t{0}),
        stamp_(s->stamp() - 1),
        omin_(vmin),
        omax_(vmax),
        size_(vmax - vmin + 1) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 64)) << vmin << ", " << vmax;
    bits_ = OneRange64(0, size_.Value() - 1);
  }

  SmallBitSet(Solver* s, absl::Span<const int64_t> sorted_values, int64_t vmin,
              int64_t vmax)
      : BitSet(s),
        bits_(uint64_t{0}),
        stamp_(s->stamp() - 1),
        omin_(vmin),
        omax_(vmax),
        size_(sorted_values.size()) {
    CHECK(ClosedIntervalNoLargerThan(vmin, vmax, 64)) << vmin << ", " << vmax;
    // We know the array is sorted and does not contains duplicate values.
    for (int i = 0; i < sorted_values.size(); ++i) {
      const int64_t val = sorted_values[i];
      DCHECK_GE(val, vmin);
      DCHECK_LE(val, vmax);
      DCHECK(!IsBitSet64(&bits_, val - omin_));
      bits_ |= OneBit64(val - omin_);
    }
  }

  ~SmallBitSet() override {}

  bool bit(int64_t val) const {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return (bits_ & OneBit64(val - omin_)) != 0;
  }

  int64_t ComputeNewMin(int64_t nmin, int64_t cmin, int64_t cmax) override {
    DCHECK_GE(nmin, cmin);
    DCHECK_LE(nmin, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    // We do not clean the bits between cmin and nmin.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new bits
    const uint64_t new_bits = bits_ & OneRange64(nmin - omin_, cmax - omin_);
    if (new_bits != uint64_t{0}) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmin)) {  // Common case, the new min is inside the bitset
        return nmin;
      }
      return LeastSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return std::numeric_limits<int64_t>::max();
    }
  }

  int64_t ComputeNewMax(int64_t nmax, int64_t cmin, int64_t cmax) override {
    DCHECK_GE(nmax, cmin);
    DCHECK_LE(nmax, cmax);
    DCHECK_LE(cmin, cmax);
    DCHECK_GE(cmin, omin_);
    DCHECK_LE(cmax, omax_);
    // We do not clean the bits between nmax and cmax.
    // But we use mask to look only at 'active' bits.

    // Create the mask and compute new_bits
    const uint64_t new_bits = bits_ & OneRange64(cmin - omin_, nmax - omin_);
    if (new_bits != uint64_t{0}) {
      // Compute new size and new min
      size_.SetValue(solver_, BitCount64(new_bits));
      if (bit(nmax)) {  // Common case, the new max is inside the bitset
        return nmax;
      }
      return MostSignificantBitPosition64(new_bits) + omin_;
    } else {  // == 0 -> Fail()
      solver_->Fail();
      return std::numeric_limits<int64_t>::min();
    }
  }

  bool SetValue(int64_t val) override {
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

  bool Contains(int64_t val) const override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    return bit(val);
  }

  bool RemoveValue(int64_t val) override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    if (bit(val)) {
      // Bitset.
      const uint64_t current_stamp = solver_->stamp();
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

  uint64_t Size() const override { return size_.Value(); }

  std::string DebugString() const override {
    return absl::StrFormat("SmallBitSet(%d..%d : %llx)", omin_, omax_, bits_);
  }

  void DelayRemoveValue(int64_t val) override {
    DCHECK_GE(val, omin_);
    DCHECK_LE(val, omax_);
    removed_.push_back(val);
  }

  void ApplyRemovedValues(DomainIntVar* var) override {
    std::sort(removed_.begin(), removed_.end());
    for (std::vector<int64_t>::iterator it = removed_.begin();
         it != removed_.end(); ++it) {
      var->RemoveValue(*it);
    }
  }

  void ClearRemovedValues() override { removed_.clear(); }

  std::string pretty_DebugString(int64_t min, int64_t max) const override {
    std::string out;
    DCHECK(bit(min));
    DCHECK(bit(max));
    if (max != min) {
      int cumul = true;
      int64_t start_cumul = min;
      for (int64_t v = min + 1; v < max; ++v) {
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
  uint64_t bits_;
  uint64_t stamp_;
  const int64_t omin_;
  const int64_t omax_;
  NumericalRev<int64_t> size_;
  std::vector<int64_t> removed_;
};

class DomainIntVarHoleIterator : public IntVarIterator {
 public:
  explicit DomainIntVarHoleIterator(const DomainIntVar* v)
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

  int64_t Value() const override {
    DCHECK(bits_ != nullptr);
    DCHECK(index_ < size_);
    return values_[index_];
  }

  void Next() override { index_++; }

 private:
  const DomainIntVar* const var_;
  DomainIntVar::BitSet* bits_;
  const int64_t* values_;
  int size_;
  int index_;
};

class DomainIntVarDomainIterator : public IntVarIterator {
 public:
  explicit DomainIntVarDomainIterator(const DomainIntVar* v, bool reversible)
      : var_(v),
        bitset_iterator_(nullptr),
        min_(std::numeric_limits<int64_t>::max()),
        max_(std::numeric_limits<int64_t>::min()),
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

  int64_t Value() const override {
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
  int64_t min_;
  int64_t max_;
  int64_t current_;
  const bool reversible_;
};

class UnaryIterator : public IntVarIterator {
 public:
  UnaryIterator(const IntVar* v, bool hole, bool reversible)
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

DomainIntVar::DomainIntVar(Solver* s, int64_t vmin, int64_t vmax,
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

DomainIntVar::DomainIntVar(Solver* s, absl::Span<const int64_t> sorted_values,
                           const std::string& name)
    : IntVar(s, name),
      min_(std::numeric_limits<int64_t>::max()),
      max_(std::numeric_limits<int64_t>::min()),
      old_min_(std::numeric_limits<int64_t>::max()),
      old_max_(std::numeric_limits<int64_t>::min()),
      new_min_(std::numeric_limits<int64_t>::max()),
      new_max_(std::numeric_limits<int64_t>::min()),
      handler_(this),
      in_process_(false),
      bits_(nullptr),
      value_watcher_(nullptr),
      bound_watcher_(nullptr) {
  CHECK_GE(sorted_values.size(), 1);
  // We know that the vector is sorted and does not have duplicate values.
  const int64_t vmin = sorted_values.front();
  const int64_t vmax = sorted_values.back();
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

void DomainIntVar::SetMin(int64_t m) {
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
    const int64_t new_min =
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

void DomainIntVar::SetMax(int64_t m) {
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
    const int64_t new_max =
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

void DomainIntVar::SetRange(int64_t mi, int64_t ma) {
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
        const int64_t new_min =
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
        const int64_t new_max =
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

void DomainIntVar::SetValue(int64_t v) {
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

void DomainIntVar::RemoveValue(int64_t v) {
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

void DomainIntVar::RemoveInterval(int64_t l, int64_t u) {
  if (l <= min_.Value()) {
    SetMin(u + 1);
  } else if (u >= max_.Value()) {
    SetMax(l - 1);
  } else {
    for (int64_t v = l; v <= u; ++v) {
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

IntVarIterator* DomainIntVar::MakeHoleIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible, new DomainIntVarHoleIterator(this));
}

IntVarIterator* DomainIntVar::MakeDomainIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible,
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

// ConcreteBooleanVar Out-Of-Line Methods

ConcreteBooleanVar::Handler::Handler(ConcreteBooleanVar* var)
    : Demon(), var_(var) {}
ConcreteBooleanVar::Handler::~Handler() {}
void ConcreteBooleanVar::Handler::Run(Solver* s) {
  s->GetPropagationMonitor()->StartProcessingIntegerVariable(var_);
  var_->Process();
  s->GetPropagationMonitor()->EndProcessingIntegerVariable(var_);
}
Solver::DemonPriority ConcreteBooleanVar::Handler::priority() const {
  return Solver::VAR_PRIORITY;
}
std::string ConcreteBooleanVar::Handler::DebugString() const {
  return absl::StrFormat("Handler(%s)", var_->DebugString());
}

ConcreteBooleanVar::ConcreteBooleanVar(Solver* s, const std::string& name)
    : BooleanVar(s, name), handler_(this) {}
ConcreteBooleanVar::~ConcreteBooleanVar() {}

void ConcreteBooleanVar::SetValue(int64_t v) {
  if (value_ == kUnboundBooleanVarValue) {
    if ((v & 0xfffffffffffffffe) == 0) {
      solver()->InternalSaveBooleanVarValue(this);
      value_ = static_cast<int>(v);
      EnqueueVar(&handler_);
      return;
    }
  } else if (v == value_) {
    return;
  }
  solver()->Fail();
}

void ConcreteBooleanVar::Process() {
  DCHECK_NE(value_, kUnboundBooleanVarValue);
  ExecuteAll(bound_demons_);
  for (SimpleRevFIFO<Demon*>::Iterator it(&delayed_bound_demons_); it.ok();
       ++it) {
    EnqueueDelayedDemon(*it);
  }
}

int64_t ConcreteBooleanVar::OldMin() const { return 0LL; }
int64_t ConcreteBooleanVar::OldMax() const { return 1LL; }
void ConcreteBooleanVar::RestoreBooleanValue() {
  value_ = kUnboundBooleanVarValue;
}

// ----- IntConst -----

IntConst::IntConst(Solver* s, int64_t value, const std::string& name)
    : IntVar(s, name), value_(value) {}
IntConst::~IntConst() {}

int64_t IntConst::Min() const { return value_; }
void IntConst::SetMin(int64_t m) {
  if (m > value_) {
    solver()->Fail();
  }
}
int64_t IntConst::Max() const { return value_; }
void IntConst::SetMax(int64_t m) {
  if (m < value_) {
    solver()->Fail();
  }
}
void IntConst::SetRange(int64_t l, int64_t u) {
  if (l > value_ || u < value_) {
    solver()->Fail();
  }
}
void IntConst::SetValue(int64_t v) {
  if (v != value_) {
    solver()->Fail();
  }
}
bool IntConst::Bound() const { return true; }
int64_t IntConst::Value() const { return value_; }
void IntConst::RemoveValue(int64_t v) {
  if (v == value_) {
    solver()->Fail();
  }
}
void IntConst::RemoveInterval(int64_t l, int64_t u) {
  if (l <= value_ && value_ <= u) {
    solver()->Fail();
  }
}
void IntConst::WhenBound(Demon*) {}
void IntConst::WhenRange(Demon*) {}
void IntConst::WhenDomain(Demon*) {}
uint64_t IntConst::Size() const { return 1; }
bool IntConst::Contains(int64_t v) const { return (v == value_); }
IntVarIterator* IntConst::MakeHoleIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible, new EmptyIterator());
}
IntVarIterator* IntConst::MakeDomainIterator(bool reversible) const {
  return CondRevAlloc(solver(), reversible, new RangeIterator(this));
}
int64_t IntConst::OldMin() const { return value_; }
int64_t IntConst::OldMax() const { return value_; }
std::string IntConst::DebugString() const {
  std::string out;
  if (solver()->HasName(this)) {
    const std::string& var_name = name();
    absl::StrAppendFormat(&out, "%s(%d)", var_name, value_);
  } else {
    absl::StrAppendFormat(&out, "IntConst(%d)", value_);
  }
  return out;
}

int IntConst::VarType() const { return CONST_VAR; }

IntVar* IntConst::IsEqual(int64_t constant) {
  if (constant == value_) {
    return solver()->MakeIntConst(1);
  } else {
    return solver()->MakeIntConst(0);
  }
}

IntVar* IntConst::IsDifferent(int64_t constant) {
  if (constant == value_) {
    return solver()->MakeIntConst(0);
  } else {
    return solver()->MakeIntConst(1);
  }
}

IntVar* IntConst::IsGreaterOrEqual(int64_t constant) {
  return solver()->MakeIntConst(value_ >= constant);
}

IntVar* IntConst::IsLessOrEqual(int64_t constant) {
  return solver()->MakeIntConst(value_ <= constant);
}

std::string IntConst::name() const {
  if (solver()->HasName(this)) {
    return PropagationBaseObject::name();
  } else {
    return absl::StrCat(value_);
  }
}

// ----- x + c variable, optimized case -----

class PlusCstVar : public IntVar {
 public:
  PlusCstVar(Solver* s, IntVar* v, int64_t c) : IntVar(s), var_(v), cst_(c) {}

  ~PlusCstVar() override {}

  void WhenRange(Demon* d) override { var_->WhenRange(d); }

  void WhenBound(Demon* d) override { var_->WhenBound(d); }

  void WhenDomain(Demon* d) override { var_->WhenDomain(d); }

  int64_t OldMin() const override { return CapAdd(var_->OldMin(), cst_); }

  int64_t OldMax() const override { return CapAdd(var_->OldMax(), cst_); }

  std::string DebugString() const override {
    if (HasName()) {
      return absl::StrFormat("%s(%s + %d)", name(), var_->DebugString(), cst_);
    } else {
      return absl::StrFormat("(%s + %d)", var_->DebugString(), cst_);
    }
  }

  int VarType() const override { return VAR_ADD_CST; }

  void Accept(ModelVisitor* visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kSumOperation, cst_,
                                  var_);
  }

  IntVar* IsEqual(int64_t constant) override {
    return var_->IsEqual(constant - cst_);
  }

  IntVar* IsDifferent(int64_t constant) override {
    return var_->IsDifferent(constant - cst_);
  }

  IntVar* IsGreaterOrEqual(int64_t constant) override {
    return var_->IsGreaterOrEqual(constant - cst_);
  }

  IntVar* IsLessOrEqual(int64_t constant) override {
    return var_->IsLessOrEqual(constant - cst_);
  }

  IntVar* SubVar() const { return var_; }

  int64_t Constant() const { return cst_; }

 protected:
  IntVar* const var_;
  const int64_t cst_;
};

class PlusCstIntVar : public PlusCstVar {
 public:
  class PlusCstIntVarIterator : public UnaryIterator {
   public:
    PlusCstIntVarIterator(const IntVar* v, int64_t c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}

    ~PlusCstIntVarIterator() override {}

    int64_t Value() const override { return iterator_->Value() + cst_; }

   private:
    const int64_t cst_;
  };

  PlusCstIntVar(Solver* s, IntVar* v, int64_t c) : PlusCstVar(s, v, c) {}

  ~PlusCstIntVar() override {}

  int64_t Min() const override { return var_->Min() + cst_; }

  void SetMin(int64_t m) override { var_->SetMin(CapSub(m, cst_)); }

  int64_t Max() const override { return var_->Max() + cst_; }

  void SetMax(int64_t m) override { var_->SetMax(CapSub(m, cst_)); }

  void SetRange(int64_t l, int64_t u) override {
    var_->SetRange(CapSub(l, cst_), CapSub(u, cst_));
  }

  void SetValue(int64_t v) override { var_->SetValue(v - cst_); }

  int64_t Value() const override { return var_->Value() + cst_; }

  bool Bound() const override { return var_->Bound(); }

  void RemoveValue(int64_t v) override { var_->RemoveValue(v - cst_); }

  void RemoveInterval(int64_t l, int64_t u) override {
    var_->RemoveInterval(l - cst_, u - cst_);
  }

  uint64_t Size() const override { return var_->Size(); }

  bool Contains(int64_t v) const override { return var_->Contains(v - cst_); }

  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new PlusCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new PlusCstIntVarIterator(var_, cst_, false, reversible));
  }
};

class PlusCstDomainIntVar : public PlusCstVar {
 public:
  class PlusCstDomainIntVarIterator : public UnaryIterator {
   public:
    PlusCstDomainIntVarIterator(const IntVar* v, int64_t c, bool hole,
                                bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}

    ~PlusCstDomainIntVarIterator() override {}

    int64_t Value() const override { return iterator_->Value() + cst_; }

   private:
    const int64_t cst_;
  };

  PlusCstDomainIntVar(Solver* s, DomainIntVar* v, int64_t c)
      : PlusCstVar(s, v, c) {}

  ~PlusCstDomainIntVar() override {}

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;

  DomainIntVar* domain_int_var() const {
    return reinterpret_cast<DomainIntVar*>(var_);
  }

  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new PlusCstDomainIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new PlusCstDomainIntVarIterator(var_, cst_, false, reversible));
  }
};

int64_t PlusCstDomainIntVar::Min() const {
  return domain_int_var()->min_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMin(int64_t m) {
  domain_int_var()->DomainIntVar::SetMin(CapSub(m, cst_));
}

int64_t PlusCstDomainIntVar::Max() const {
  return domain_int_var()->max_.Value() + cst_;
}

void PlusCstDomainIntVar::SetMax(int64_t m) {
  domain_int_var()->DomainIntVar::SetMax(CapSub(m, cst_));
}

void PlusCstDomainIntVar::SetRange(int64_t l, int64_t u) {
  domain_int_var()->DomainIntVar::SetRange(CapSub(l, cst_), CapSub(u, cst_));
}

void PlusCstDomainIntVar::SetValue(int64_t v) {
  domain_int_var()->DomainIntVar::SetValue(v - cst_);
}

bool PlusCstDomainIntVar::Bound() const {
  return domain_int_var()->min_.Value() == domain_int_var()->max_.Value();
}

int64_t PlusCstDomainIntVar::Value() const {
  CHECK_EQ(domain_int_var()->min_.Value(), domain_int_var()->max_.Value())
      << " variable is not bound";
  return domain_int_var()->min_.Value() + cst_;
}

void PlusCstDomainIntVar::RemoveValue(int64_t v) {
  domain_int_var()->DomainIntVar::RemoveValue(v - cst_);
}

void PlusCstDomainIntVar::RemoveInterval(int64_t l, int64_t u) {
  domain_int_var()->DomainIntVar::RemoveInterval(l - cst_, u - cst_);
}

uint64_t PlusCstDomainIntVar::Size() const {
  return domain_int_var()->DomainIntVar::Size();
}

bool PlusCstDomainIntVar::Contains(int64_t v) const {
  return domain_int_var()->DomainIntVar::Contains(v - cst_);
}

// c - x variable, optimized case

class SubCstIntVar : public IntVar {
 public:
  class SubCstIntVarIterator : public UnaryIterator {
   public:
    SubCstIntVarIterator(const IntVar* v, int64_t c, bool hole, bool rev)
        : UnaryIterator(v, hole, rev), cst_(c) {}
    ~SubCstIntVarIterator() override {}

    int64_t Value() const override { return cst_ - iterator_->Value(); }

   private:
    const int64_t cst_;
  };

  SubCstIntVar(Solver* s, IntVar* v, int64_t c);
  ~SubCstIntVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(solver(), reversible,
                        new SubCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new SubCstIntVarIterator(var_, cst_, false, reversible));
  }
  int64_t OldMin() const override { return CapSub(cst_, var_->OldMax()); }
  int64_t OldMax() const override { return CapSub(cst_, var_->OldMin()); }
  std::string DebugString() const override;
  std::string name() const override;
  int VarType() const override { return CST_SUB_VAR; }

  void Accept(ModelVisitor* visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation,
                                  cst_, var_);
  }

  IntVar* IsEqual(int64_t constant) override {
    return var_->IsEqual(cst_ - constant);
  }

  IntVar* IsDifferent(int64_t constant) override {
    return var_->IsDifferent(cst_ - constant);
  }

  IntVar* IsGreaterOrEqual(int64_t constant) override {
    return var_->IsLessOrEqual(cst_ - constant);
  }

  IntVar* IsLessOrEqual(int64_t constant) override {
    return var_->IsGreaterOrEqual(cst_ - constant);
  }

  IntVar* SubVar() const { return var_; }
  int64_t Constant() const { return cst_; }

 private:
  IntVar* const var_;
  const int64_t cst_;
};

SubCstIntVar::SubCstIntVar(Solver* s, IntVar* v, int64_t c)
    : IntVar(s), var_(v), cst_(c) {}

SubCstIntVar::~SubCstIntVar() {}

int64_t SubCstIntVar::Min() const { return cst_ - var_->Max(); }

void SubCstIntVar::SetMin(int64_t m) { var_->SetMax(CapSub(cst_, m)); }

int64_t SubCstIntVar::Max() const { return cst_ - var_->Min(); }

void SubCstIntVar::SetMax(int64_t m) { var_->SetMin(CapSub(cst_, m)); }

void SubCstIntVar::SetRange(int64_t l, int64_t u) {
  var_->SetRange(CapSub(cst_, u), CapSub(cst_, l));
}

void SubCstIntVar::SetValue(int64_t v) { var_->SetValue(cst_ - v); }

bool SubCstIntVar::Bound() const { return var_->Bound(); }

void SubCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64_t SubCstIntVar::Value() const { return cst_ - var_->Value(); }

void SubCstIntVar::RemoveValue(int64_t v) { var_->RemoveValue(cst_ - v); }

void SubCstIntVar::RemoveInterval(int64_t l, int64_t u) {
  var_->RemoveInterval(cst_ - u, cst_ - l);
}

void SubCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void SubCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64_t SubCstIntVar::Size() const { return var_->Size(); }

bool SubCstIntVar::Contains(int64_t v) const {
  return var_->Contains(cst_ - v);
}

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
    OppIntVarIterator(const IntVar* v, bool hole, bool reversible)
        : UnaryIterator(v, hole, reversible) {}
    ~OppIntVarIterator() override {}

    int64_t Value() const override { return -iterator_->Value(); }
  };

  OppIntVar(Solver* s, IntVar* v);
  ~OppIntVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(solver(), reversible,
                        new OppIntVarIterator(var_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(solver(), reversible,
                        new OppIntVarIterator(var_, false, reversible));
  }
  int64_t OldMin() const override { return CapOpp(var_->OldMax()); }
  int64_t OldMax() const override { return CapOpp(var_->OldMin()); }
  std::string DebugString() const override;
  int VarType() const override { return OPP_VAR; }

  void Accept(ModelVisitor* visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kDifferenceOperation, 0,
                                  var_);
  }

  IntVar* IsEqual(int64_t constant) override {
    return var_->IsEqual(-constant);
  }

  IntVar* IsDifferent(int64_t constant) override {
    return var_->IsDifferent(-constant);
  }

  IntVar* IsGreaterOrEqual(int64_t constant) override {
    return var_->IsLessOrEqual(-constant);
  }

  IntVar* IsLessOrEqual(int64_t constant) override {
    return var_->IsGreaterOrEqual(-constant);
  }

  IntVar* SubVar() const { return var_; }

 private:
  IntVar* const var_;
};

OppIntVar::OppIntVar(Solver* s, IntVar* v) : IntVar(s), var_(v) {}

OppIntVar::~OppIntVar() {}

int64_t OppIntVar::Min() const { return -var_->Max(); }

void OppIntVar::SetMin(int64_t m) { var_->SetMax(CapOpp(m)); }

int64_t OppIntVar::Max() const { return -var_->Min(); }

void OppIntVar::SetMax(int64_t m) { var_->SetMin(CapOpp(m)); }

void OppIntVar::SetRange(int64_t l, int64_t u) {
  var_->SetRange(CapOpp(u), CapOpp(l));
}

void OppIntVar::SetValue(int64_t v) { var_->SetValue(CapOpp(v)); }

bool OppIntVar::Bound() const { return var_->Bound(); }

void OppIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64_t OppIntVar::Value() const { return -var_->Value(); }

void OppIntVar::RemoveValue(int64_t v) { var_->RemoveValue(-v); }

void OppIntVar::RemoveInterval(int64_t l, int64_t u) {
  var_->RemoveInterval(-u, -l);
}

void OppIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void OppIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64_t OppIntVar::Size() const { return var_->Size(); }

bool OppIntVar::Contains(int64_t v) const { return var_->Contains(-v); }

std::string OppIntVar::DebugString() const {
  return absl::StrFormat("-(%s)", var_->DebugString());
}

// ----- Utility functions -----

// x * c variable, optimized case

class TimesCstIntVar : public IntVar {
 public:
  TimesCstIntVar(Solver* s, IntVar* v, int64_t c)
      : IntVar(s), var_(v), cst_(c) {}
  ~TimesCstIntVar() override {}

  IntVar* SubVar() const { return var_; }
  int64_t Constant() const { return cst_; }

  void Accept(ModelVisitor* visitor) const override {
    visitor->VisitIntegerVariable(this, ModelVisitor::kProductOperation, cst_,
                                  var_);
  }

  IntVar* IsEqual(int64_t constant) override {
    if (constant % cst_ == 0) {
      return var_->IsEqual(constant / cst_);
    } else {
      return solver()->MakeIntConst(0);
    }
  }

  IntVar* IsDifferent(int64_t constant) override {
    if (constant % cst_ == 0) {
      return var_->IsDifferent(constant / cst_);
    } else {
      return solver()->MakeIntConst(1);
    }
  }

  IntVar* IsGreaterOrEqual(int64_t constant) override {
    if (cst_ > 0) {
      return var_->IsGreaterOrEqual(PosIntDivUp(constant, cst_));
    } else {
      return var_->IsLessOrEqual(PosIntDivDown(-constant, -cst_));
    }
  }

  IntVar* IsLessOrEqual(int64_t constant) override {
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
  const int64_t cst_;
};

class TimesPosCstIntVar : public TimesCstIntVar {
 public:
  class TimesPosCstIntVarIterator : public UnaryIterator {
   public:
    TimesPosCstIntVarIterator(const IntVar* v, int64_t c, bool hole,
                              bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    ~TimesPosCstIntVarIterator() override {}

    int64_t Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64_t cst_;
  };

  TimesPosCstIntVar(Solver* s, IntVar* v, int64_t c);
  ~TimesPosCstIntVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new TimesPosCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new TimesPosCstIntVarIterator(var_, cst_, false, reversible));
  }
  int64_t OldMin() const override { return CapProd(var_->OldMin(), cst_); }
  int64_t OldMax() const override { return CapProd(var_->OldMax(), cst_); }
};

// ----- TimesPosCstIntVar -----

TimesPosCstIntVar::TimesPosCstIntVar(Solver* s, IntVar* v, int64_t c)
    : TimesCstIntVar(s, v, c) {}

TimesPosCstIntVar::~TimesPosCstIntVar() {}

int64_t TimesPosCstIntVar::Min() const { return CapProd(var_->Min(), cst_); }

void TimesPosCstIntVar::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    var_->SetMin(PosIntDivUp(m, cst_));
  }
}

int64_t TimesPosCstIntVar::Max() const { return CapProd(var_->Max(), cst_); }

void TimesPosCstIntVar::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    var_->SetMax(PosIntDivDown(m, cst_));
  }
}

void TimesPosCstIntVar::SetRange(int64_t l, int64_t u) {
  var_->SetRange(PosIntDivUp(l, cst_), PosIntDivDown(u, cst_));
}

void TimesPosCstIntVar::SetValue(int64_t v) {
  if (v % cst_ != 0) {
    solver()->Fail();
  }
  var_->SetValue(v / cst_);
}

bool TimesPosCstIntVar::Bound() const { return var_->Bound(); }

void TimesPosCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64_t TimesPosCstIntVar::Value() const {
  return CapProd(var_->Value(), cst_);
}

void TimesPosCstIntVar::RemoveValue(int64_t v) {
  if (v % cst_ == 0) {
    var_->RemoveValue(v / cst_);
  }
}

void TimesPosCstIntVar::RemoveInterval(int64_t l, int64_t u) {
  for (int64_t v = l; v <= u; ++v) {
    RemoveValue(v);
  }
  // TODO(user) : Improve me
}

void TimesPosCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void TimesPosCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64_t TimesPosCstIntVar::Size() const { return var_->Size(); }

bool TimesPosCstIntVar::Contains(int64_t v) const {
  return (v % cst_ == 0 && var_->Contains(v / cst_));
}

// b * c variable, optimized case

class TimesPosCstBoolVar : public TimesCstIntVar {
 public:
  class TimesPosCstBoolVarIterator : public UnaryIterator {
   public:
    // TODO(user) : optimize this.
    TimesPosCstBoolVarIterator(const IntVar* v, int64_t c, bool hole,
                               bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    ~TimesPosCstBoolVarIterator() override {}

    int64_t Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64_t cst_;
  };

  TimesPosCstBoolVar(Solver* s, BooleanVar* v, int64_t c);
  ~TimesPosCstBoolVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(solver(), reversible, new EmptyIterator());
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new TimesPosCstBoolVarIterator(boolean_var(), cst_, false, reversible));
  }
  int64_t OldMin() const override { return 0; }
  int64_t OldMax() const override { return cst_; }

  BooleanVar* boolean_var() const {
    return reinterpret_cast<BooleanVar*>(var_);
  }
};

// ----- TimesPosCstBoolVar -----

TimesPosCstBoolVar::TimesPosCstBoolVar(Solver* s, BooleanVar* v, int64_t c)
    : TimesCstIntVar(s, v, c) {}

TimesPosCstBoolVar::~TimesPosCstBoolVar() {}

int64_t TimesPosCstBoolVar::Min() const {
  return (boolean_var()->RawValue() == 1) * cst_;
}

void TimesPosCstBoolVar::SetMin(int64_t m) {
  if (m > cst_) {
    solver()->Fail();
  } else if (m > 0) {
    boolean_var()->SetMin(1);
  }
}

int64_t TimesPosCstBoolVar::Max() const {
  return (boolean_var()->RawValue() != 0) * cst_;
}

void TimesPosCstBoolVar::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  } else if (m < cst_) {
    boolean_var()->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetRange(int64_t l, int64_t u) {
  if (u < 0 || l > cst_ || l > u) {
    solver()->Fail();
  }
  if (l > 0) {
    boolean_var()->SetMin(1);
  } else if (u < cst_) {
    boolean_var()->SetMax(0);
  }
}

void TimesPosCstBoolVar::SetValue(int64_t v) {
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

int64_t TimesPosCstBoolVar::Value() const {
  CHECK_NE(boolean_var()->RawValue(), BooleanVar::kUnboundBooleanVarValue)
      << " variable is not bound";
  return boolean_var()->RawValue() * cst_;
}

void TimesPosCstBoolVar::RemoveValue(int64_t v) {
  if (v == 0) {
    boolean_var()->RemoveValue(0);
  } else if (v == cst_) {
    boolean_var()->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::RemoveInterval(int64_t l, int64_t u) {
  if (l <= 0 && u >= 0) {
    boolean_var()->RemoveValue(0);
  }
  if (l <= cst_ && u >= cst_) {
    boolean_var()->RemoveValue(1);
  }
}

void TimesPosCstBoolVar::WhenBound(Demon* d) { boolean_var()->WhenBound(d); }

void TimesPosCstBoolVar::WhenDomain(Demon* d) { boolean_var()->WhenDomain(d); }

uint64_t TimesPosCstBoolVar::Size() const {
  return (1 +
          (boolean_var()->RawValue() == BooleanVar::kUnboundBooleanVarValue));
}

bool TimesPosCstBoolVar::Contains(int64_t v) const {
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
    TimesNegCstIntVarIterator(const IntVar* v, int64_t c, bool hole,
                              bool reversible)
        : UnaryIterator(v, hole, reversible), cst_(c) {}
    ~TimesNegCstIntVarIterator() override {}

    int64_t Value() const override { return iterator_->Value() * cst_; }

   private:
    const int64_t cst_;
  };

  TimesNegCstIntVar(Solver* s, IntVar* v, int64_t c);
  ~TimesNegCstIntVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  void WhenRange(Demon* d) override;
  void WhenBound(Demon* d) override;
  void WhenDomain(Demon* d) override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new TimesNegCstIntVarIterator(var_, cst_, true, reversible));
  }
  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return CondRevAlloc(
        solver(), reversible,
        new TimesNegCstIntVarIterator(var_, cst_, false, reversible));
  }
  int64_t OldMin() const override { return CapProd(var_->OldMax(), cst_); }
  int64_t OldMax() const override { return CapProd(var_->OldMin(), cst_); }
};

// ----- TimesNegCstIntVar -----

TimesNegCstIntVar::TimesNegCstIntVar(Solver* s, IntVar* v, int64_t c)
    : TimesCstIntVar(s, v, c) {}

TimesNegCstIntVar::~TimesNegCstIntVar() {}

int64_t TimesNegCstIntVar::Min() const { return CapProd(var_->Max(), cst_); }

void TimesNegCstIntVar::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    var_->SetMax(PosIntDivDown(-m, -cst_));
  }
}

int64_t TimesNegCstIntVar::Max() const { return CapProd(var_->Min(), cst_); }

void TimesNegCstIntVar::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    var_->SetMin(PosIntDivUp(-m, -cst_));
  }
}

void TimesNegCstIntVar::SetRange(int64_t l, int64_t u) {
  var_->SetRange(PosIntDivUp(CapOpp(u), CapOpp(cst_)),
                 PosIntDivDown(CapOpp(l), CapOpp(cst_)));
}

void TimesNegCstIntVar::SetValue(int64_t v) {
  if (v % cst_ != 0) {
    solver()->Fail();
  }
  var_->SetValue(v / cst_);
}

bool TimesNegCstIntVar::Bound() const { return var_->Bound(); }

void TimesNegCstIntVar::WhenRange(Demon* d) { var_->WhenRange(d); }

int64_t TimesNegCstIntVar::Value() const {
  return CapProd(var_->Value(), cst_);
}

void TimesNegCstIntVar::RemoveValue(int64_t v) {
  if (v % cst_ == 0) {
    var_->RemoveValue(v / cst_);
  }
}

void TimesNegCstIntVar::RemoveInterval(int64_t l, int64_t u) {
  for (int64_t v = l; v <= u; ++v) {
    RemoveValue(v);
  }
  // TODO(user) : Improve me
}

void TimesNegCstIntVar::WhenBound(Demon* d) { var_->WhenBound(d); }

void TimesNegCstIntVar::WhenDomain(Demon* d) { var_->WhenDomain(d); }

uint64_t TimesNegCstIntVar::Size() const { return var_->Size(); }

bool TimesNegCstIntVar::Contains(int64_t v) const {
  return (v % cst_ == 0 && var_->Contains(v / cst_));
}

// This constraints links an expression and the variable it is casted into
class LinkExprAndVar : public CastConstraint {
 public:
  LinkExprAndVar(Solver* s, IntExpr* expr, IntVar* var)
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
    int64_t l, u;
    expr_->Range(&l, &u);
    target_var_->SetRange(l, u);
  }

  std::string DebugString() const override {
    return absl::StrFormat("cast(%s, %s)", expr_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* visitor) const override {
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
  LinkExprAndDomainIntVar(Solver* s, IntExpr* expr, DomainIntVar* var)
      : CastConstraint(s, var),
        expr_(expr),
        cached_min_(std::numeric_limits<int64_t>::min()),
        cached_max_(std::numeric_limits<int64_t>::max()),
        fail_stamp_(uint64_t{0}) {}

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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLinkExprVar, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kLinkExprVar, this);
  }

 private:
  IntExpr* const expr_;
  int64_t cached_min_;
  int64_t cached_max_;
  uint64_t fail_stamp_;
};

// ----- API -----

void CleanVariableOnFail(IntVar* var) {
  DCHECK_EQ(IntVar::DOMAIN_INT_VAR, var->VarType());
  DomainIntVar* dvar = reinterpret_cast<DomainIntVar*>(var);
  dvar->CleanInProcess();
}

Constraint* SetIsEqual(IntVar* var, absl::Span<const int64_t> values,
                       const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsEqual(values, vars);
}

Constraint* SetIsGreaterOrEqual(IntVar* var, absl::Span<const int64_t> values,
                                const std::vector<IntVar*>& vars) {
  DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(var);
  CHECK(dvar != nullptr);
  return dvar->SetIsGreaterOrEqual(values, vars);
}

// --------- IntVar ---------

void LinkVarExpr(Solver* s, IntExpr* expr, IntVar* var) {
  if (!var->Bound()) {
    if (var->VarType() == IntVar::DOMAIN_INT_VAR) {
      DomainIntVar* dvar = reinterpret_cast<DomainIntVar*>(var);
      s->AddCastConstraint(
          s->RevAlloc(new LinkExprAndDomainIntVar(s, expr, dvar)), dvar, expr);
    } else {
      s->AddCastConstraint(s->RevAlloc(new LinkExprAndVar(s, expr, var)), var,
                           expr);
    }
  }
}

IntVar* NewVarPlusInt(Solver* s, IntVar* var, int64_t value) {
  if (value == 0) return var;
  switch (var->VarType()) {
    case IntVar::DOMAIN_INT_VAR: {
      return RegisterIntVar(s->RevAlloc(new PlusCstDomainIntVar(
          s, reinterpret_cast<DomainIntVar*>(var), value)));
    }
    case IntVar::CONST_VAR: {
      DCHECK(!AddOverflows(var->Min(), value));
      return RegisterIntVar(s->MakeIntConst(var->Min() + value));
    }
    case IntVar::VAR_ADD_CST: {
      PlusCstVar* const var_add_cst = reinterpret_cast<PlusCstVar*>(var);
      IntVar* const sub_var = var_add_cst->SubVar();
      DCHECK(!AddOverflows(value, var_add_cst->Constant()));
      const int64_t new_constant = value + var_add_cst->Constant();
      if (new_constant == 0) {
        return sub_var;
      } else {
        if (sub_var->VarType() == IntVar::DOMAIN_INT_VAR) {
          DomainIntVar* const dvar = reinterpret_cast<DomainIntVar*>(sub_var);
          return RegisterIntVar(
              s->RevAlloc(new PlusCstDomainIntVar(s, dvar, new_constant)));
        } else {
          return RegisterIntVar(
              s->RevAlloc(new PlusCstIntVar(s, sub_var, new_constant)));
        }
      }
    }
    case IntVar::CST_SUB_VAR: {
      SubCstIntVar* const cst_sub_var = reinterpret_cast<SubCstIntVar*>(var);
      IntVar* const sub_var = cst_sub_var->SubVar();
      DCHECK(!AddOverflows(value, cst_sub_var->Constant()));
      const int64_t new_constant = value + cst_sub_var->Constant();
      return NewIntMinusVar(s, new_constant, sub_var);
    }
    case IntVar::OPP_VAR: {
      OppIntVar* const opp_var = reinterpret_cast<OppIntVar*>(var);
      IntVar* const sub_var = opp_var->SubVar();
      return NewIntMinusVar(s, value, sub_var);
    }
    default:
      return RegisterIntVar(s->RevAlloc(new PlusCstIntVar(s, var, value)));
  }
}

IntVar* NewIntMinusVar(Solver* s, int64_t value, IntVar* var) {
  switch (var->VarType()) {
    case IntVar::VAR_ADD_CST: {
      PlusCstVar* const cst_add_var = reinterpret_cast<PlusCstVar*>(var);
      IntVar* const sub_var = cst_add_var->SubVar();
      DCHECK(!SubOverflows(value, cst_add_var->Constant()));
      const int64_t new_constant = value - cst_add_var->Constant();
      if (new_constant == 0) {
        return sub_var;
      } else {
        return RegisterIntVar(
            s->RevAlloc(new SubCstIntVar(s, sub_var, new_constant)));
      }
    }
    case IntVar::CST_SUB_VAR: {
      SubCstIntVar* const cst_sub_var = reinterpret_cast<SubCstIntVar*>(var);
      IntVar* const sub_var = cst_sub_var->SubVar();
      DCHECK(!SubOverflows(value, cst_sub_var->Constant()));
      const int64_t new_constant = value - cst_sub_var->Constant();
      return NewVarPlusInt(s, sub_var, new_constant);
    }
    case IntVar::OPP_VAR: {
      OppIntVar* const add_var = reinterpret_cast<OppIntVar*>(var);
      IntVar* const sub_var = add_var->SubVar();
      return NewVarPlusInt(s, sub_var, value);
    }
    default: {
      if (value == 0) {
        return RegisterIntVar(s->RevAlloc(new OppIntVar(s, var)));
      } else {
        return RegisterIntVar(s->RevAlloc(new SubCstIntVar(s, var, value)));
      }
    }
  }
}

IntVar* NewVarTimesInt(Solver* s, IntVar* var, int64_t value) {
  if (value == 0) {
    return s->MakeIntConst(0)->Var();
  } else if (value > 0) {
    if (var->VarType() == IntVar::BOOLEAN_VAR) {
      return RegisterIntVar(s->RevAlloc(new TimesPosCstBoolVar(
          s, reinterpret_cast<BooleanVar*>(var), value)));
    } else {
      return RegisterIntVar(s->RevAlloc(new TimesPosCstIntVar(s, var, value)));
    }
  } else {
    return RegisterIntVar(s->RevAlloc(new TimesNegCstIntVar(s, var, value)));
  }
}

// Discovery methods

bool IsVarProduct(IntExpr* expr, IntVar** inner_var, int64_t* coefficient) {
  TimesCstIntVar* const prod_var = dynamic_cast<TimesCstIntVar*>(expr);
  if (prod_var != nullptr) {
    *coefficient = prod_var->Constant();
    *inner_var = prod_var->SubVar();
    return true;
  }
  return false;
}

}  // namespace operations_research
