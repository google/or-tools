// Copyright 2010-2011 Google
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
//  Expression constraints

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/const_int_array.h"

DEFINE_int32(cache_initial_size, 1024, "Initial size of the array of the hash "
             "table of caches for objects of type StatusVar(x == 3)");

namespace operations_research {

//-----------------------------------------------------------------------------
// Equality

class EqualityExprCst : public Constraint {
 public:
  EqualityExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~EqualityExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
 private:
  IntExpr* const expr_;
  int64 value_;
};

EqualityExprCst::EqualityExprCst(Solver* const s, IntExpr* const e, int64 v)
  : Constraint(s), expr_(e), value_(v) {}

void EqualityExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void EqualityExprCst::InitialPropagate() {
  expr_->SetValue(value_);
}

string EqualityExprCst::DebugString() const {
  return StringPrintf("(%s == %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}

Constraint* Solver::MakeEquality(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new EqualityExprCst(this, e, v));
}

Constraint* Solver::MakeEquality(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new EqualityExprCst(this, e, v));
}

//-----------------------------------------------------------------------------
// Greater or equal constraint

class GreaterEqExprCst : public Constraint {
 public:
  GreaterEqExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~GreaterEqExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
 private:
  IntExpr* const expr_;
  int64 value_;
};

GreaterEqExprCst::GreaterEqExprCst(Solver* const s, IntExpr* const e, int64 v)
  : Constraint(s), expr_(e), value_(v) {}

void GreaterEqExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void GreaterEqExprCst::InitialPropagate() {
  expr_->SetMin(value_);
}

string GreaterEqExprCst::DebugString() const {
  return StringPrintf("(%s >= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v));
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v));
}

Constraint* Solver::MakeGreater(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
}

Constraint* Solver::MakeGreater(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
}

//-----------------------------------------------------------------------------
// Less or equal constraint

class LessEqExprCst : public Constraint {
 public:
  LessEqExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~LessEqExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
 private:
  IntExpr* const expr_;
  int64 value_;
};

LessEqExprCst::LessEqExprCst(Solver* const s, IntExpr* const e, int64 v)
  : Constraint(s), expr_(e), value_(v) {}

void LessEqExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void LessEqExprCst::InitialPropagate() {
  expr_->SetMax(value_);
}

string LessEqExprCst::DebugString() const {
  return StringPrintf("(%s <= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v));
}

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v));
}

Constraint* Solver::MakeLess(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v - 1));
}

Constraint* Solver::MakeLess(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v - 1));
}

//-----------------------------------------------------------------------------
// Different constraints

class DiffCst : public Constraint {
 public:
  DiffCst(Solver* const s, IntVar* const var, int64 value);
  virtual ~DiffCst() {}
  virtual void Post() {}
  virtual void InitialPropagate();
  void BoundPropagate();
  virtual string DebugString() const;
 private:
  IntVar* const var_;
  int64 value_;
  Demon* demon_;
};

DiffCst::DiffCst(Solver* const s, IntVar* const var, int64 value)
    : Constraint(s), var_(var), value_(value), demon_(NULL) {}

void DiffCst::InitialPropagate() {
  if (var_->Size() >= 0xFFFFFFFF) {
    demon_ = MakeConstraintDemon0(solver(),
                                  this,
                                  &DiffCst::BoundPropagate,
                                  "BoundPropagate");
    var_->WhenRange(demon_);
  } else {
    var_->RemoveValue(value_);
  }
}

void DiffCst::BoundPropagate() {
  const int64 var_min = var_->Min();
  const int64 var_max = var_->Max();
  if (var_min > value_ || var_max < value_) {
    demon_->inhibit(solver());
  } else if (var_min == value_) {
    var_->SetMin(value_ + 1);
  } else if (var_max == value_) {
    var_->SetMax(value_ - 1);
  } else if (var_->Size() <= 0xFFFFFF) {
    demon_->inhibit(solver());
    var_->RemoveValue(value_);
  }
}

string DiffCst::DebugString() const {
  return StringPrintf("(%s != %" GG_LL_FORMAT "d)",
                      var_->DebugString().c_str(), value_);
}

Constraint* Solver::MakeNonEquality(IntVar* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new DiffCst(this, e, v));
}

Constraint* Solver::MakeNonEquality(IntVar* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new DiffCst(this, e, v));
}
// ----- is_equal_cst Constraint -----

class IsEqualCstCt : public Constraint {
 public:
  IsEqualCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : Constraint(s), var_(v), cst_(c), boolvar_(b), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    boolvar_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = var_->Bound();
    int64 u = var_->Contains(cst_);
    int64 l = inhibit ? u : 0;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        var_->RemoveValue(cst_);
      } else {
        var_->SetValue(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  string DebugString() const {
    return StringPrintf("IsEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  int64 cst_;
  IntVar* const boolvar_;
  Demon* demon_;
};

// ---------- VarCstCache ----------

class VarCstCache : public BaseObject {
 public:
  explicit VarCstCache(Solver* const s)
      : solver_(s), size_(FLAGS_cache_initial_size), counter_(0) {
    array_ = s->UnsafeRevAllocArray(new Cell*[size_]);
    for (int i = 0; i < size_; ++i) {
      array_[i] = NULL;
    }
  }
  virtual ~VarCstCache() {}

  void Insert(IntVar* const var,
              int64 value,
              IntVar* const boolvar) {
    int code = HashCode(var, value) % size_;
    Cell* tmp = array_[code];
    while (tmp != NULL) {
      if (tmp->value == value && tmp->var == var) {
        return;
      }
      tmp = tmp->next;
    }
    UnsafeInsert(var, value, boolvar);
  }

  Solver* const solver() const { return solver_; }

 protected:
  IntVar* Find(IntVar* const var, int64 value) const {
    int code = HashCode(var, value) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      if (tmp->value == value && tmp->var == var) {
        return tmp->boolvar;
      }
      tmp = tmp->next;
    }
    return NULL;
    // TODO(user): bench optim that moves the found cell first in the list.
  }

  void UnsafeInsert(IntVar* const var, int64 value, IntVar* const boolvar) {
    int code = HashCode(var, value) % size_;
    Cell* tmp = array_[code];
    Cell* cell = solver_->UnsafeRevAlloc(new Cell(var, value, boolvar, tmp));
    solver_->SaveValue(reinterpret_cast<void**>(&array_[code]));
    array_[code] = cell;
    solver_->SaveAndAdd(&counter_, 1);
    if (counter_ > 2 * size_) {
      Double();
    }
  }
 private:
  struct Cell {
   public:
    Cell(IntVar* const v, int64 c, IntVar* b, Cell* n)
        : var(v), value(c), boolvar(b), next(n) {}
    IntVar* const var;
    const int64 value;
    IntVar* const boolvar;
    Cell* next;
  };

  static uint64 HashCode(IntVar* const var, int64 value) {
    return ((reinterpret_cast<uint64>(var) >> 4) * 3 + value * 5);
  }

  void Double() {
    Cell** old_cell_array = array_;
    const int old_size = size_;
    solver_->SaveValue(&size_);
    size_ *= 2;
    solver_->SaveValue(reinterpret_cast<void**>(&array_));
    array_ = solver_->UnsafeRevAllocArray(new Cell*[size_]);
    for (int i = 0; i < size_; ++i) {
      array_[i] = NULL;
    }
    for (int i = 0; i < old_size; ++i) {
      Cell* tmp = old_cell_array[i];
      while (tmp != NULL) {
        Cell* to_reinsert = tmp;
        tmp = tmp->next;
        int code = HashCode(to_reinsert->var, to_reinsert->value) % size_;
        Cell* new_next = array_[code];
        solver_->SaveValue(reinterpret_cast<void**>(&to_reinsert->next));
        to_reinsert->next = new_next;
        array_[code] = to_reinsert;
      }
    }
  }

  Solver* const solver_;
  Cell** array_;
  int size_;
  int counter_;
};

// ---------- EqualityVarCstCache ----------

class EqualityVarCstCache : public VarCstCache {
 public:
  explicit EqualityVarCstCache(Solver* const s) : VarCstCache(s) {}
  virtual ~EqualityVarCstCache() {}

  IntVar* VarEqCstStatus(IntVar* const var, int64 value) {
    IntVar* boolvar = Find(var, value);
    if (!boolvar) {
      boolvar = solver()->MakeBoolVar(
          StringPrintf("StatusVar<%s == %" GG_LL_FORMAT "d>",
                       var->name().c_str(), value));
      Constraint* const maintain =
          solver()->RevAlloc(new IsEqualCstCt(solver(), var, value, boolvar));
      solver()->AddConstraint(maintain);
      UnsafeInsert(var, value, boolvar);
    }
    return boolvar;
  }
};

IntVar* Solver::MakeIsEqualCstVar(IntVar* const var, int64 value) {
  if (value == var->Min()) {
    return MakeIsLessOrEqualCstVar(var, value);
  }
  if (value == var->Max()) {
    return MakeIsGreaterOrEqualCstVar(var, value);
  }
  if (!var->Contains(value)) {
    return MakeIntConst(0LL);
  }
  if (var->Bound() && var->Value() == value) {
    return MakeIntConst(1LL);
  }
  return equality_var_cst_cache_->VarEqCstStatus(var, value);
}

Constraint* Solver::MakeIsEqualCstCt(IntVar* const v, int64 c,
                                     IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  if (c == v->Min()) {
    return MakeIsLessOrEqualCstCt(v, c, b);
  }
  if (c == v->Max()) {
    return MakeIsGreaterOrEqualCstCt(v, c, b);
  }
  // TODO(user) : what happens if the constraint is not posted?
  // The cache becomes tainted.
  equality_var_cst_cache_->Insert(v, c, b);
  return RevAlloc(new IsEqualCstCt(this, v, c, b));
}

// ----- is_diff_cst Constraint -----

class IsDiffCstCt : public Constraint {
 public:
  IsDiffCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : Constraint(s), var_(v), cst_(c), boolvar_(b), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    boolvar_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = var_->Bound();
    int64 l = 1 - var_->Contains(cst_);
    int64 u = inhibit ? l : 1;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 1) {
        var_->RemoveValue(cst_);
      } else {
        var_->SetValue(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  virtual string DebugString() const {
    return StringPrintf("IsDiffCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  int64 cst_;
  IntVar* const boolvar_;
  Demon* demon_;
};

// ---------- UnequalityVarCstCache ----------

class UnequalityVarCstCache : public VarCstCache {
 public:
  explicit UnequalityVarCstCache(Solver* const s) : VarCstCache(s) {}
  virtual ~UnequalityVarCstCache() {}

  IntVar* VarNonEqCstStatus(IntVar* const var, int64 value) {
    IntVar* boolvar = Find(var, value);
    if (!boolvar) {
      boolvar = solver()->MakeBoolVar(StringPrintf("StatusVar<%s == %"
                                                   GG_LL_FORMAT "d>",
                                                   var->name().c_str(),
                                                   value));
      Constraint* const maintain =
          solver()->RevAlloc(new IsDiffCstCt(solver(), var, value, boolvar));
      solver()->AddConstraint(maintain);
      UnsafeInsert(var, value, boolvar);
    }
    return boolvar;
  }
};

IntVar* Solver::MakeIsDifferentCstVar(IntVar* const var, int64 value) {
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstVar(var, value + 1);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstVar(var, value - 1);
  }
  if (!var->Contains(value)) {
    return MakeIntConst(1LL);
  }
  if (var->Bound() && var->Value() == value) {
    return MakeIntConst(0LL);
  }
  return unequality_var_cst_cache_->VarNonEqCstStatus(var, value);
}

Constraint* Solver::MakeIsDifferentCstCt(IntVar* const v, int64 c,
                                         IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  if (c == v->Min()) {
    return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
  }
  if (c == v->Max()) {
    return MakeIsLessOrEqualCstCt(v, c - 1, b);
  }
  unequality_var_cst_cache_->Insert(v, c, b);
  return RevAlloc(new IsDiffCstCt(this, v, c, b));
}

// ----- is_greater_equal_cst Constraint -----

class IsGreaterEqualCstCt : public Constraint {
 public:
  IsGreaterEqualCstCt(Solver* const s, IntVar* const v, int64 c,
                      IntVar* const b)
      : Constraint(s), var_(v), cst_(c), boolvar_(b), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = var_->Max() >= cst_;
    int64 l = var_->Min() >= cst_;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        var_->SetMax(cst_ - 1);
      } else {
        var_->SetMin(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  virtual string DebugString() const {
    return StringPrintf("IsGreaterEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  int64 cst_;
  IntVar* const boolvar_;
  Demon* demon_;
};

// ---------- GreaterEqualCstCache ----------

class GreaterEqualCstCache : public VarCstCache {
 public:
  explicit GreaterEqualCstCache(Solver* const s) : VarCstCache(s) {}
  virtual ~GreaterEqualCstCache() {}

  IntVar* VarGreaterEqCstStatus(IntVar* const var, int64 value) {
    IntVar* boolvar = Find(var, value);
    if (!boolvar) {
      boolvar = solver()->MakeBoolVar(
          StringPrintf("StatusVar<%s >= %" GG_LL_FORMAT "d>",
                       var->name().c_str(), value));
      Constraint* const maintain =
          solver()->RevAlloc(new IsGreaterEqualCstCt(solver(),
                                                     var,
                                                     value,
                                                     boolvar));
      solver()->AddConstraint(maintain);
      UnsafeInsert(var, value, boolvar);
    }
    return boolvar;
  }
};

IntVar* Solver::MakeIsGreaterOrEqualCstVar(IntVar* const var, int64 value) {
  if (var->Min() >= value) {
    return MakeIntConst(1LL);
  }
  if (var->Max() < value) {
    return MakeIntConst(0LL);
  }
  return greater_equal_var_cst_cache_->VarGreaterEqCstStatus(var, value);
}

IntVar* Solver::MakeIsGreaterCstVar(IntVar* const var, int64 value) {
  return greater_equal_var_cst_cache_->VarGreaterEqCstStatus(var, value + 1);
}

Constraint* Solver::MakeIsGreaterOrEqualCstCt(IntVar* const v, int64 c,
                                              IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  greater_equal_var_cst_cache_->Insert(v, c, b);
  return RevAlloc(new IsGreaterEqualCstCt(this, v, c, b));
}

Constraint* Solver::MakeIsGreaterCstCt(IntVar* const v, int64 c,
                                       IntVar* const b) {
  return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
}

// ----- is_lesser_equal_cst Constraint -----

class IsLessEqualCstCt : public Constraint {
 public:
  IsLessEqualCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : Constraint(s), var_(v), cst_(c), boolvar_(b), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = var_->Min() <= cst_;
    int64 l = var_->Max() <= cst_;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        var_->SetMin(cst_ + 1);
      } else {
        var_->SetMax(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  virtual string DebugString() const {
    return StringPrintf("IsLessEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  int64 cst_;
  IntVar* const boolvar_;
  Demon* demon_;
};

// ---------- LessEqualCstCache ----------

class LessEqualCstCache : public VarCstCache {
 public:
  explicit LessEqualCstCache(Solver* const s) : VarCstCache(s) {}
  virtual ~LessEqualCstCache() {}

  IntVar* VarLessEqCstStatus(IntVar* const var, int64 value) {
    IntVar* boolvar = Find(var, value);
    if (!boolvar) {
      boolvar = solver()->MakeBoolVar(
          StringPrintf("StatusVar<%s <= %" GG_LL_FORMAT "d>",
                       var->name().c_str(), value));
      Constraint* const maintain =
          solver()->RevAlloc(new IsLessEqualCstCt(solver(),
                                                  var,
                                                  value,
                                                  boolvar));
      solver()->AddConstraint(maintain);
      UnsafeInsert(var, value, boolvar);
    }
    return boolvar;
  }
};

IntVar* Solver::MakeIsLessOrEqualCstVar(IntVar* const var, int64 value) {
  if (var->Max() <= value) {
    return MakeIntConst(1LL);
  }
  if (var->Min() > value) {
    return MakeIntConst(0LL);
  }
  return less_equal_var_cst_cache_->VarLessEqCstStatus(var, value);
}

IntVar* Solver::MakeIsLessCstVar(IntVar* const var, int64 value) {
  return MakeIsLessOrEqualCstVar(var, value - 1);
}

Constraint* Solver::MakeIsLessOrEqualCstCt(IntVar* const v, int64 c,
                                           IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  less_equal_var_cst_cache_->Insert(v, c, b);
  return RevAlloc(new IsLessEqualCstCt(this, v, c, b));
}

Constraint* Solver::MakeIsLessCstCt(IntVar* const v, int64 c,
                                    IntVar* const b) {
  return MakeIsLessOrEqualCstCt(v, c - 1, b);
}

// ----- BetweenCt -----

class BetweenCt : public Constraint {
 public:
  BetweenCt(Solver* const s, IntVar* const v, int64 l, int64 u)
      : Constraint(s), var_(v), min_(l), max_(u) {}

  virtual void Post() {}

  virtual void InitialPropagate() {
    var_->SetRange(min_, max_);
  }

  virtual string DebugString() const {
    return StringPrintf("BetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), min_, max_);
  }
 private:
  IntVar* const var_;
  int64 min_;
  int64 max_;
};

Constraint* Solver::MakeBetweenCt(IntVar* const v, int64 l, int64 u) {
  CHECK_EQ(this, v->solver());
  return RevAlloc(new BetweenCt(this, v, l, u));
}

// ----- is_between_cst Constraint -----

class IsBetweenCt : public Constraint {
 public:
  IsBetweenCt(Solver* const s, IntVar* const v, int64 l, int64 u,
              IntVar* const b)
      : Constraint(s), var_(v), min_(l), max_(u), boolvar_(b), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = 1 - (var_->Min() > max_ || var_->Max() < min_);
    int64 l = var_->Max() <= max_ && var_->Min() >= min_;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        var_->RemoveInterval(min_, max_);
      } else {
        var_->SetRange(min_, max_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  virtual string DebugString() const {
    return StringPrintf(
        "IsBetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d, %s)",
        var_->DebugString().c_str(), min_, max_,
        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  int64 min_;
  int64 max_;
  IntVar* const boolvar_;
  Demon* demon_;
};

Constraint* Solver::MakeIsBetweenCt(IntVar* const v,
                                    int64 l,
                                    int64 u,
                                    IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  return RevAlloc(new IsBetweenCt(this, v, l, u, b));
}

// ---------- Member ----------

// ----- Member(IntVar, IntSet) -----

class MemberCt : public Constraint {
 public:
  MemberCt(Solver* const s,
           IntVar* const v,
           std::vector<int64>* const sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != NULL);
    DCHECK(s != NULL);
    DCHECK(sorted_values != NULL);
  }

  virtual void Post() {}

  virtual void InitialPropagate() {
    var_->SetValues(values_.RawData(), values_.size());
  }

  virtual string DebugString() const {
    return StringPrintf("Member(%s, %s)",
                        var_->DebugString().c_str(),
                        values_.DebugString().c_str());
  }
 private:
  IntVar* const var_;
  ConstIntArray values_;
};

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const int64* const values,
                                 int size) {
  ConstIntArray local_values(values, size);
  return RevAlloc(
      new MemberCt(this, var, local_values.SortedCopyWithoutDuplicates(true)));
}

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const std::vector<int64>& values) {
  return MakeMemberCt(var, values.data(), values.size());
}

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const int* const values,
                                 int size) {
  ConstIntArray local_values(values, size);
  return RevAlloc(
      new MemberCt(this, var, local_values.SortedCopyWithoutDuplicates(true)));
}

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const std::vector<int>& values) {
  return MakeMemberCt(var, values.data(), values.size());
}

// ----- IsMemberCt -----

class IsMemberCt : public Constraint {
 public:
  IsMemberCt(Solver* const s,
             IntVar* const v,
             std::vector<int64>* const sorted_values,
             IntVar* const b)
      : Constraint(s),
        var_(v),
        values_(sorted_values),
        boolvar_(b),
        support_pos_(0),
        demon_(NULL) {
    DCHECK(v != NULL);
    DCHECK(s != NULL);
    DCHECK(b != NULL);
    DCHECK(sorted_values != NULL);
  }

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    if (!var_->Bound()) {
      var_->WhenDomain(demon_);
    }
    if (!boolvar_->Bound()) {
      boolvar_->WhenBound(demon_);
    }
  }

  virtual void InitialPropagate() {
    if (boolvar_->Min() == 1LL) {
      demon_->inhibit(solver());
      var_->SetValues(values_.RawData(), values_.size());
    } else if (boolvar_->Max() == 1LL) {
      int support = support_pos_.Value();
      const int64 vmin = var_->Min();
      const int64 vmax = var_->Max();
      while (support < values_.size() &&
             (values_[support] < vmin ||
              !var_->Contains(values_[support]))) {
        if (values_[support] <= vmax) {
          support++;
        } else {
          support = values_.size();
        }
      }
      support_pos_.SetValue(solver(), support);
      if (support >= values_.size()) {
        demon_->inhibit(solver());
        boolvar_->SetValue(0LL);
      } else if (var_->Bound()) {
        demon_->inhibit(solver());
        boolvar_->SetValue(1LL);
      }
    } else {  // boolvar_ set to 0.
      demon_->inhibit(solver());
      var_->RemoveValues(values_.RawData(), values_.size());
    }
  }

  virtual string DebugString() const {
    return StringPrintf("IsMemberCt(%s, %s, %s)",
                        var_->DebugString().c_str(),
                        values_.DebugString().c_str(),
                        boolvar_->DebugString().c_str());
  }
 private:
  IntVar* const var_;
  ConstIntArray values_;
  IntVar* const boolvar_;
  Rev<int> support_pos_;
  Demon* demon_;
};

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const int64* const values,
                                   int size,
                                   IntVar* const boolvar) {
  ConstIntArray local_values(values, size);
  return RevAlloc(
      new IsMemberCt(this,
                     var,
                     local_values.SortedCopyWithoutDuplicates(true),
                     boolvar));
}

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const std::vector<int64>& values,
                                   IntVar* const boolvar) {
  return MakeIsMemberCt(var, values.data(), values.size(), boolvar);
}

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const int* const values,
                                   int size,
                                   IntVar* const boolvar) {
  ConstIntArray local_values(values, size);
  return RevAlloc(
      new IsMemberCt(this,
                     var,
                     local_values.SortedCopyWithoutDuplicates(true),
                     boolvar));
}

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const std::vector<int>& values,
                                   IntVar* const boolvar) {
  return MakeIsMemberCt(var, values.data(), values.size(), boolvar);
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var,
                                const int64* const values,
                                int size) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(var, values, size, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var,
                                const std::vector<int64>& values) {
  return MakeIsMemberVar(var, values.data(), values.size());
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var,
                                const int* const values,
                                int size) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(var, values, size, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var, const std::vector<int>& values) {
  return MakeIsMemberVar(var, values.data(), values.size());
}

// ---------- Init Caches ----------

void Solver::InitBoolVarCaches() {
  equality_var_cst_cache_ = RevAlloc(new EqualityVarCstCache(this));
  unequality_var_cst_cache_ = RevAlloc(new UnequalityVarCstCache(this));
  greater_equal_var_cst_cache_ = RevAlloc(new GreaterEqualCstCache(this));
  less_equal_var_cst_cache_ = RevAlloc(new LessEqualCstCache(this));
}

}  // namespace operations_research
