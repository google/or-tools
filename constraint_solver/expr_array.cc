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
//  Array Expression constraints

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

// ---------- Base array classes used for code factorization ----------

// ----- Array Constraint -----

class ArrayConstraint : public Constraint {
 public:
  ArrayConstraint(Solver* const s,
                  const IntVar* const * vars,
                  int size,
                  IntVar* const var)
    : Constraint(s), vars_(new IntVar*[size]), size_(size), var_(var) {
    CHECK_GT(size, 0);
    CHECK_NOTNULL(vars);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }
  virtual ~ArrayConstraint() {}
 protected:
  string DebugStringInternal(const string& name) const {
    string out = name + "(";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += vars_[i]->DebugString();
    }
    out += ", " + var_->DebugString() + ")";
    return out;
  }

  scoped_array<IntVar*> vars_;
  const int size_;
  IntVar* const var_;
};

// ----- ArrayExpr -----

class ArrayExpr : public BaseIntExpr {
 public:
  ArrayExpr(Solver* const s, const IntVar* const* vars, int size)
    : BaseIntExpr(s), vars_(new IntVar*[size]), size_(size) {
    CHECK_GT(size, 0);
    CHECK_NOTNULL(vars);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }

  virtual ~ArrayExpr() {}
 protected:
  string DebugStringInternal(const string& name) const {
    string out = name + "(";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += vars_[i]->DebugString();
    }
    out += ")";
    return out;
  }

  scoped_array<IntVar*> vars_;
  const int size_;
};

// ----- Tree Array Constraint -----

// Helper class
class TreeArrayConstraint : public ArrayConstraint {
 public:
  TreeArrayConstraint(Solver* const solver,
                      IntVar* const* vars,
                      int size,
                      IntVar* const sum_var)
      : ArrayConstraint(solver, vars, size, sum_var),
        block_size_(solver->parameters().array_split_size) {
    std::vector<int> lengths;
    lengths.push_back(size_);
    while (lengths.back() > 1) {
      const int current = lengths.back();
      lengths.push_back((current + block_size_ - 1) / block_size_);
    }
    tree_.resize(lengths.size());
    for (int i = 0; i < lengths.size(); ++i) {
      tree_[i].resize(lengths[lengths.size() - i - 1]);
    }
    DCHECK_GE(tree_.size(), 1);
    DCHECK_EQ(1, tree_[0].size());
    root_node_ = &tree_[0][0];
  }

  // Increases min by delta_min, reduces max by delta_max.
  void ReduceRange(int depth, int position, int64 delta_min, int64 delta_max) {
    const uint64 stamp = solver()->stamp();
    NodeInfo* const info = &tree_[depth][position];
    if (delta_min > 0) {
      if (stamp != info->min_stamp) {
        solver()->SaveValue(&info->node_min);
        info->min_stamp = stamp;
      }
      info->node_min += delta_min;
    }
    if (delta_max > 0) {
      if (stamp != info->max_stamp) {
        solver()->SaveValue(&info->node_max);
        info->max_stamp = stamp;
      }
      info->node_max -= delta_max;
    }
  }

  void InitLeaf(int position, int64 var_min, int64 var_max) {
    InitNode(tree_.size() - 1, position, var_min, var_max);
  }

  void InitNode(int depth, int position, int64 node_min, int64 node_max) {
    NodeInfo* const info = &tree_[depth][position];
    info->node_min = node_min;
    info->min_stamp = 0;
    info->node_max = node_max;
    info->max_stamp= 0;
  }

  int64 Min(int depth, int position) const {
    return tree_[depth][position].node_min;
  }

  int64 Max(int depth, int position) const {
    return tree_[depth][position].node_max;
  }

  int64 RootMin() const {
    return root_node_->node_min;
  }

  int64 RootMax() const {
    return root_node_->node_max;
  }

  int Parent(int position) const {
    return position / block_size_;
  }

  int ChildStart(int position) const {
    return position * block_size_;
  }

  int ChildEnd(int depth, int position) const {
    DCHECK_LT(depth + 1, tree_.size());
    const int max_position = tree_[depth + 1].size() - 1;
    return std::min((position + 1) * block_size_ - 1, max_position);
  }

  bool IsLeaf(int depth) const {
    return depth == tree_.size() - 1;
  }

  int MaxDepth() const {
    return tree_.size();
  }

  int Width(int depth) const {
    return tree_[depth].size();
  }
 private:
  struct NodeInfo {
    int64 node_min;
    int64 node_max;
    uint64 min_stamp;
    uint64 max_stamp;
  };

  std::vector<std::vector<NodeInfo> > tree_;
  const int block_size_;
  NodeInfo* root_node_;
};

// ---------- Sum Array ----------

// Some of these optimizations here are described in:
// "Bounds consistency techniques for long linear constraints".  In
// Workshop on Techniques for Implementing Constraint Programming
// Systems (TRICS), a workshop of CP 2002, N. Beldiceanu, W. Harvey,
// Martin Henz, Francois Laburthe, Eric Monfroy, Tobias Müller,
// Laurent Perron and Christian Schulte editors, pages 39–46, 2002.

// ----- SumConstraint -----

// This constraint implements sum(vars) == sum_var.
class SumConstraint : public TreeArrayConstraint {
 public:
  SumConstraint(Solver* const solver,
                IntVar* const * vars,
                int size,
                IntVar* const sum_var)
    : TreeArrayConstraint(solver, vars, size, sum_var), sum_demon_(NULL) {}

  virtual ~SumConstraint() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      Demon* const demon = MakeConstraintDemon1(solver(),
                                                this,
                                                &SumConstraint::LeafChanged,
                                                "LeafChanged",
                                                i);
      vars_[i]->WhenRange(demon);
    }
    sum_demon_ = MakeDelayedConstraintDemon0(solver(),
                                             this,
                                             &SumConstraint::SumChanged,
                                             "SumChanged");
    var_->WhenRange(sum_demon_);
  }

  virtual void InitialPropagate() {
    // Copy vars to leaf nodes.
    for (int i = 0; i < size_; ++i) {
      InitLeaf(i, vars_[i]->Min(), vars_[i]->Max());
    }
    // Compute up.
    for (int i = MaxDepth() - 2; i >= 0; --i) {
      for (int j = 0; j < Width(i); ++j) {
        int64 sum_min = 0;
        int64 sum_max = 0;
        const int block_start = ChildStart(j);
        const int block_end = ChildEnd(i, j);
        for (int k = block_start; k <= block_end; ++k) {
          sum_min += Min(i + 1, k);
          sum_max += Max(i + 1, k);
        }
        InitNode(i, j, sum_min, sum_max);
      }
    }
    // Propagate to sum_var.
    var_->SetRange(RootMin(), RootMax());

    // Push down.
    SumChanged();
  }

  void SumChanged() {
    if (var_->Max() == RootMin()) {
      // We can fix all terms to min.
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetValue(vars_[i]->Min());
      }
    } else if (var_->Min() == RootMax()) {
      // We can fix all terms to max.
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetValue(vars_[i]->Max());
      }
    } else {
      PushDown(0, 0, var_->Min(), var_->Max());
    }
  }

  void PushDown(int depth, int position, int64 new_min, int64 new_max) {
    // Nothing to do?
    if (new_min <= Min(depth, position) && new_max >= Max(depth, position)) {
      return;
    }

    // Leaf node -> push to leaf var.
    if (IsLeaf(depth)) {
      vars_[position]->SetRange(new_min, new_max);
      return;
    }

    // Standard propagation from the bounds of the sum to the
    // individuals terms.

    // These are maintained automatically in the tree structure.
    const int64 sum_min = Min(depth, position);
    const int64 sum_max = Max(depth, position);

    // Intersect the new bounds with the computed bounds.
    new_max = std::min(sum_max, new_max);
    new_min = std::max(sum_min, new_min);

    // Detect failure early.
    if (new_max < sum_min || new_min > sum_max) {
      solver()->Fail();
    }

    // Push to children nodes.
    const int block_start = ChildStart(position);
    const int block_end = ChildEnd(depth, position);
    for (int i = block_start; i <= block_end; ++i) {
      const int64 var_min = Min(depth + 1, i);
      const int64 var_max = Max(depth + 1, i);
      const int64 residual_min = sum_min - var_min;
      const int64 residual_max = sum_max - var_max;
      PushDown(depth + 1, i, new_min - residual_max, new_max - residual_min);
    }
    // TODO(user) : Is the diameter optimization (see reference
    // above, rule 5) useful?
  }

  void LeafChanged(int term_index) {
    IntVar* const var = vars_[term_index];
    PushUp(term_index, var->Min() - var->OldMin(), var->OldMax() - var->Max());
    Enqueue(sum_demon_);  // TODO(user): Is this needed?
  }

  void PushUp(int position, int64 delta_min, int64 delta_max) {
    DCHECK_GE(delta_max, 0);
    DCHECK_GE(delta_min, 0);
    DCHECK_GT(delta_min + delta_max, 0);
    for (int depth = MaxDepth() - 1; depth >= 0; --depth) {
      ReduceRange(depth, position, delta_min, delta_max);
      position = Parent(position);
    }
    DCHECK_EQ(0, position);
    var_->SetRange(RootMin(), RootMax());
  }

  string DebugString() const {
    return DebugStringInternal("Sum");
  }
 private:
  Demon* sum_demon_;
};

// ---------- Min Array ----------

// ----- Min Bool Array Ct -----

// This constraint implements min(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.
class MinBoolArrayCt : public ArrayConstraint {
 public:
  MinBoolArrayCt(Solver* const s, const IntVar* const * vars, int size,
                 IntVar* var);
  virtual ~MinBoolArrayCt() {}

  virtual void Post();
  virtual void InitialPropagate();

  void Update(int index);
  void UpdateVar();

  virtual string DebugString() const;

 private:
  SmallRevBitSet bits_;
  bool inhibited_;
};

MinBoolArrayCt::MinBoolArrayCt(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               IntVar* var)
    : ArrayConstraint(s, vars, size, var), bits_(size), inhibited_(false) {}

void MinBoolArrayCt::Post() {
  for (int i = 0; i < size_; ++i) {
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &MinBoolArrayCt::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(d);
  }

  Demon* uv = MakeConstraintDemon0(solver(),
                                   this,
                                   &MinBoolArrayCt::UpdateVar,
                                   "UpdateVar");
  var_->WhenRange(uv);
}

void MinBoolArrayCt::InitialPropagate() {
  if (var_->Min() == 1LL) {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->SetMin(1LL);
    }
    solver()->SaveAndSetValue(&inhibited_, true);
  } else {
    for (int i = 0; i < size_; ++i) {
      IntVar* const var = vars_[i];
      if (var->Max() == 0LL) {
        var_->SetMax(0LL);
        solver()->SaveAndSetValue(&inhibited_, true);
        return;
      }
      if (var->Min() == 0LL) {
        bits_.SetToOne(solver(), i);
      }
    }
    if (bits_.IsCardinalityZero()) {
      var_->SetValue(1LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    } else if (var_->Max() == 0LL && bits_.IsCardinalityOne()) {
      vars_[bits_.GetFirstOne()]->SetValue(0LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    }
  }
}

void MinBoolArrayCt::Update(int index) {
  if (!inhibited_) {
    if (vars_[index]->Max() == 0LL) {  // Bound to 0.
      var_->SetValue(0LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    } else {
      bits_.SetToZero(solver(), index);
      if (bits_.IsCardinalityZero()) {
        var_->SetValue(1LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      } else if (var_->Max() == 0LL && bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstOne()]->SetValue(0LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      }
    }
  }
}

void MinBoolArrayCt::UpdateVar() {
  if (!inhibited_) {
    if (var_->Min() == 1LL) {
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetMin(1LL);
      }
      solver()->SaveAndSetValue(&inhibited_, true);
    } else {
      if (bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstOne()]->SetValue(0LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      }
    }
  }
}

string MinBoolArrayCt::DebugString() const {
  return DebugStringInternal("MinBoolArrayCt");
}

// ----- MinBoolArray -----

class MinBoolArray : public ArrayExpr {
 public:
  // This constructor will copy the array. The caller can safely delete the
  // exprs array himself
  MinBoolArray(Solver* const s, const IntVar* const* exprs, int size);
  virtual ~MinBoolArray();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual string DebugString() const;
  virtual void WhenRange(Demon* d);
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = 0LL;
    int64 vmax = 0LL;
    Range(&vmin, &vmax);
    IntVar* var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    Constraint* const ct =
        s->RevAlloc(new MinBoolArrayCt(s, vars_.get(), size_, var));
    s->AddConstraint(ct);
    return var;
  }
};

MinBoolArray::~MinBoolArray() {}

MinBoolArray::MinBoolArray(Solver* const s, const IntVar* const* vars, int size)
    : ArrayExpr(s, vars, size) {}

int64 MinBoolArray::Min() const {
  for (int i = 0; i < size_; ++i) {
    const int64 vmin = vars_[i]->Min();
    if (vmin == 0LL) {
      return 0LL;
    }
  }
  return 1LL;
}

void MinBoolArray::SetMin(int64 m) {
  if (m <= 0) {
    return;
  }
  if (m > 1) {
    solver()->Fail();
  }
  for (int i = 0; i < size_; ++i) {
    vars_[i]->SetMin(1LL);
  }
}

int64 MinBoolArray::Max() const {
  for (int i = 0; i < size_; ++i) {
    const int64 vmax = vars_[i]->Max();
    if (vmax == 0LL) {
      return 0LL;
    }
  }
  return 1LL;
}

void MinBoolArray::SetMax(int64 m) {
  if (m < 0) {
    solver()->Fail();
  } else if (m >= 1) {
    return;
  }
  DCHECK_EQ(m, 0LL);
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Min() == 0LL) {
      active++;
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMax(0LL);
  }
}

string MinBoolArray::DebugString() const {
  return DebugStringInternal("MinBoolArray");
}

void MinBoolArray::WhenRange(Demon* d) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->WhenRange(d);
  }
}

// ----- Min Array Ct -----

// This constraint implements min(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.
class MinArrayCt : public ArrayConstraint {
 public:
  MinArrayCt(Solver* const s, const IntVar* const * vars, int size,
             IntVar* var);
  virtual ~MinArrayCt() {}

  virtual void Post();
  virtual void InitialPropagate();

  void Update(int index);
  void UpdateVar();

  virtual string DebugString() const;

 private:
  Rev<int> min_support_;
};

MinArrayCt::MinArrayCt(Solver* const s,
                       const IntVar* const * vars,
                       int size,
                       IntVar* var)
    : ArrayConstraint(s, vars, size, var), min_support_(0) {}

void MinArrayCt::Post() {
  for (int i = 0; i < size_; ++i) {
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &MinArrayCt::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(d);
  }
  Demon* uv = MakeConstraintDemon0(solver(),
                                   this,
                                   &MinArrayCt::UpdateVar,
                                   "UpdateVar");
  var_->WhenRange(uv);
}

void MinArrayCt::InitialPropagate() {
  int64 vmin = var_->Min();
  int64 vmax = var_->Max();
  int64 cmin = kint64max;
  int64 cmax = kint64max;
  int min_support = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    var->SetMin(vmin);
    const int64 tmin = var->Min();
    const int64 tmax = var->Max();
    if (tmin < cmin) {
      cmin = tmin;
      min_support = i;
    }
    if (tmax < cmax) {
      cmax = tmax;
    }
  }
  min_support_.SetValue(solver(), min_support);
  var_->SetRange(cmin, cmax);
  vmin = var_->Min();
  vmax = var_->Max();
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Min() <= vmax) {
      if (active++ >= 1) {
        return;
      }
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMax(vmax);
  }
}

void MinArrayCt::Update(int index) {
  IntVar* const modified = vars_[index];
  if (modified->OldMax() != modified->Max()) {
    var_->SetMax(modified->Max());
  }
  if (index == min_support_.Value() && modified->OldMin() != modified->Min()) {
    // TODO(user) : can we merge this code with above into
    // ComputeMinSupport?
    int64 cmin = kint64max;
    int min_support = -1;
    for (int i = 0; i < size_; ++i) {
      const int64 tmin = vars_[i]->Min();
      if (tmin < cmin) {
        cmin = tmin;
        min_support = i;
      }
    }
    min_support_.SetValue(solver(), min_support);
    var_->SetMin(cmin);
  }
}

void MinArrayCt::UpdateVar() {
  const int64 vmin = var_->Min();
  if (vmin != var_->OldMin()) {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->SetMin(vmin);
    }
  }
  const int64 vmax = var_->Max();
  if (vmax != var_->OldMax()) {
    int active = 0;
    int curr = -1;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min() <= vmax) {
        if (active++ >= 1) {
          return;
        }
        curr = i;
      }
    }
    if (active == 0) {
      solver()->Fail();
    }
    if (active == 1) {
      vars_[curr]->SetMax(vmax);
    }
  }
}

string MinArrayCt::DebugString() const {
  return DebugStringInternal("MinArrayCt");
}

// Array Min: the min of all the elements. More efficient that using just
// binary MinIntExpr operators when the array grows
class MinArray : public ArrayExpr {
 public:
  // this constructor will copy the array. The caller can safely delete the
  // exprs array himself
  MinArray(Solver* const s, const IntVar* const* exprs, int size);
  virtual ~MinArray();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual string DebugString() const;
  virtual void WhenRange(Demon* d);
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = 0LL;
    int64 vmax = 0LL;
    Range(&vmin, &vmax);
    IntVar* var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    Constraint* const ct =
        s->RevAlloc(new MinArrayCt(s, vars_.get(), size_, var));
    s->AddConstraint(ct);
    return var;
  }
};

MinArray::~MinArray() {}

MinArray::MinArray(Solver* const s, const IntVar* const* vars, int size)
    : ArrayExpr(s, vars, size) {}

int64 MinArray::Min() const {
  int64 min = kint64max;
  for (int i = 0; i < size_; ++i) {
    const int64 vmin = vars_[i]->Min();
    if (min > vmin) {
      min = vmin;
    }
  }
  return min;
}

void MinArray::SetMin(int64 m) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->SetMin(m);
  }
}

int64 MinArray::Max() const {
  int64 max = kint64max;
  for (int i = 0; i < size_; ++i) {
    const int64 vmax = vars_[i]->Max();
    if (max > vmax) {
      max = vmax;
    }
  }
  return max;
}

void MinArray::SetMax(int64 m) {
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Min() <= m) {
      if (active++ >= 1) {
        return;
      }
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMax(m);
  }
}

string MinArray::DebugString() const {
  return DebugStringInternal("MinArray");
}

void MinArray::WhenRange(Demon* d) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->WhenRange(d);
  }
}

// ---------- Max Array ----------

// ----- Max Array Ct -----

// This constraint implements max(vars) == var.
class MaxArrayCt : public ArrayConstraint {
 public:
  MaxArrayCt(Solver* const s, const IntVar* const * vars, int size,
             IntVar* var);
  virtual ~MaxArrayCt() {}

  virtual void Post();
  virtual void InitialPropagate();

  void Update(int index);
  void UpdateVar();

  virtual string DebugString() const;

 private:
  Rev<int> max_support_;
};

MaxArrayCt::MaxArrayCt(Solver* const s,
                       const IntVar* const * vars,
                       int size,
                       IntVar* var)
    : ArrayConstraint(s, vars, size, var), max_support_(0) {}

void MaxArrayCt::Post() {
  for (int i = 0; i < size_; ++i) {
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &MaxArrayCt::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(d);
  }
  Demon* uv = MakeConstraintDemon0(solver(),
                                   this,
                                   &MaxArrayCt::UpdateVar,
                                   "UpdateVar");
  var_->WhenRange(uv);
}

void MaxArrayCt::InitialPropagate() {
  int64 vmin = var_->Min();
  int64 vmax = var_->Max();
  int64 cmin = kint64min;
  int64 cmax = kint64min;
  int max_support = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    var->SetMax(vmax);
    const int64 tmin = var->Min();
    const int64 tmax = var->Max();
    if (tmin > cmin) {
      cmin = tmin;
    }
    if (tmax > cmax) {
      cmax = tmax;
      max_support = i;
    }
  }
  max_support_.SetValue(solver(), max_support);
  var_->SetRange(cmin, cmax);
  vmin = var_->Min();
  vmax = var_->Max();
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Max() >= vmin) {
      if (active++ >= 1) {
        return;
      }
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMin(vmin);
  }
}

void MaxArrayCt::Update(int index) {
  IntVar* const modified = vars_[index];
  if (modified->OldMin() != modified->Min()) {
    var_->SetMin(modified->Min());
  }
  const int64 oldmax = modified->OldMax();
  if (index == max_support_.Value() && oldmax != modified->Max()) {
    // TODO(user) : can we merge this code with above into
    // ComputeMaxSupport?
    int64 cmax = kint64min;
    int max_support = -1;
    for (int i = 0; i < size_; ++i) {
      const int64 tmax = vars_[i]->Max();
      if (tmax > cmax) {
        cmax = tmax;
        max_support = i;
      }
    }
    max_support_.SetValue(solver(), max_support);
    var_->SetMax(cmax);
  }
}

void MaxArrayCt::UpdateVar() {
  const int64 vmax = var_->Max();
  if (vmax != var_->OldMax()) {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->SetMax(vmax);
    }
  }
  const int64 vmin = var_->Min();
  if (vmin != var_->OldMin()) {
    int active = 0;
    int curr = -1;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Max() >= vmin) {
        if (active++ >= 1) {
          return;
        }
        curr = i;
      }
    }
    if (active == 0) {
      solver()->Fail();
    }
    if (active == 1) {
      vars_[curr]->SetMin(vmin);
    }
  }
}

string MaxArrayCt::DebugString() const {
  return DebugStringInternal("MaxArrayCt");
}

// Array Max: the max of all the elements. More efficient that using just
// binary MaxIntExpr operators when the array grows
class MaxArray : public ArrayExpr {
 public:
  // this constructor will copy the array. The caller can safely delete the
  // exprs array himself
  MaxArray(Solver* const s, const IntVar* const* exprs, int size);
  virtual ~MaxArray();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual string DebugString() const;
  virtual void WhenRange(Demon* d);
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = Min();
    int64 vmax = Max();
    IntVar* var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    Constraint* const ct =
        s->RevAlloc(new MaxArrayCt(s, vars_.get(), size_, var));
    s->AddConstraint(ct);
    return var;
  }
};

MaxArray::~MaxArray() {}

MaxArray::MaxArray(Solver* const s, const IntVar* const* vars, int size)
    : ArrayExpr(s, vars, size) {}

int64 MaxArray::Min() const {
  int64 min = kint64min;
  for (int i = 0; i < size_; ++i) {
    const int64 vmin = vars_[i]->Min();
    if (min < vmin) {
      min = vmin;
    }
  }
  return min;
}

void MaxArray::SetMin(int64 m) {
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Max() >= m) {
      active++;
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMin(m);
  }
}

int64 MaxArray::Max() const {
  int64 max = kint64min;
  for (int i = 0; i < size_; ++i) {
    const int64 vmax = vars_[i]->Max();
    if (max < vmax) {
      max = vmax;
    }
  }
  return max;
}

void MaxArray::SetMax(int64 m) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->SetMax(m);
  }
}

string MaxArray::DebugString() const {
  return DebugStringInternal("MaxArray");
}

void MaxArray::WhenRange(Demon* d) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->WhenRange(d);
  }
}

// ----- Max Bool Array Ct -----

// This constraint implements max(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.
class MaxBoolArrayCt : public ArrayConstraint {
 public:
  MaxBoolArrayCt(Solver* const s, const IntVar* const * vars, int size,
                 IntVar* var);
  virtual ~MaxBoolArrayCt() {}

  virtual void Post();
  virtual void InitialPropagate();

  void Update(int index);
  void UpdateVar();

  virtual string DebugString() const;

 private:
  SmallRevBitSet bits_;
  bool inhibited_;
};

MaxBoolArrayCt::MaxBoolArrayCt(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               IntVar* var)
    : ArrayConstraint(s, vars, size, var), bits_(size), inhibited_(false) {}

void MaxBoolArrayCt::Post() {
  for (int i = 0; i < size_; ++i) {
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &MaxBoolArrayCt::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(d);
  }

  Demon* uv = MakeConstraintDemon0(solver(),
                                   this,
                                   &MaxBoolArrayCt::UpdateVar,
                                   "UpdateVar");
  var_->WhenRange(uv);
}

void MaxBoolArrayCt::InitialPropagate() {
  if (var_->Max() == 0) {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->SetMax(0LL);
    }
    solver()->SaveAndSetValue(&inhibited_, true);
  } else {
    for (int i = 0; i < size_; ++i) {
      IntVar* const var = vars_[i];
      if (var->Min() == 1LL) {
        var_->SetMin(1LL);
        solver()->SaveAndSetValue(&inhibited_, true);
        return;
      }
      if (var->Max() == 1LL) {
        bits_.SetToOne(solver(), i);
      }
    }
    if (bits_.IsCardinalityZero()) {
      var_->SetValue(0LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    } else if (var_->Min() == 1LL && bits_.IsCardinalityOne()) {
      vars_[bits_.GetFirstOne()]->SetValue(1LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    }
  }
}

void MaxBoolArrayCt::Update(int index) {
  if (!inhibited_) {
    if (vars_[index]->Min() == 1LL) {  // Bound to 1.
      var_->SetValue(1LL);
      solver()->SaveAndSetValue(&inhibited_, true);
    } else {
      bits_.SetToZero(solver(), index);
      if (bits_.IsCardinalityZero()) {
        var_->SetValue(0LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      } else if (var_->Min() == 1LL && bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstOne()]->SetValue(1LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      }
    }
  }
}

void MaxBoolArrayCt::UpdateVar() {
  if (!inhibited_) {
    if (var_->Max() == 0) {
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetMax(0LL);
      }
      solver()->SaveAndSetValue(&inhibited_, true);
    } else {
      if (bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstOne()]->SetValue(1LL);
        solver()->SaveAndSetValue(&inhibited_, true);
      }
    }
  }
}

string MaxBoolArrayCt::DebugString() const {
  return DebugStringInternal("MaxBoolArrayCt");
}

// ----- MaxBoolArray -----

class MaxBoolArray : public ArrayExpr {
 public:
  // this constructor will copy the array. The caller can safely delete the
  // exprs array himself
  MaxBoolArray(Solver* const s, const IntVar* const* exprs, int size);
  virtual ~MaxBoolArray();

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual string DebugString() const;
  virtual void WhenRange(Demon* d);
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = Min();
    int64 vmax = Max();
    IntVar* var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    Constraint* const ct =
        s->RevAlloc(new MaxBoolArrayCt(s, vars_.get(), size_, var));
    s->AddConstraint(ct);
    return var;
  }
};

MaxBoolArray::~MaxBoolArray() {}

MaxBoolArray::MaxBoolArray(Solver* const s, const IntVar* const* vars, int size)
    : ArrayExpr(s, vars, size) {}

int64 MaxBoolArray::Min() const {
  for (int i = 0; i < size_; ++i) {
    const int64 vmin = vars_[i]->Min();
    if (vmin == 1LL) {
      return 1LL;
    }
  }
  return 0LL;
}

void MaxBoolArray::SetMin(int64 m) {
  if (m > 1) {
    solver()->Fail();
  } else if (m <= 0) {
    return;
  }
  DCHECK_EQ(m, 1LL);
  int active = 0;
  int curr = -1;
  for (int i = 0; i < size_; ++i) {
    if (vars_[i]->Max() == 1LL) {
      active++;
      curr = i;
    }
  }
  if (active == 0) {
    solver()->Fail();
  }
  if (active == 1) {
    vars_[curr]->SetMin(1LL);
  }
}

int64 MaxBoolArray::Max() const {
  for (int i = 0; i < size_; ++i) {
    const int64 vmax = vars_[i]->Max();
    if (vmax == 1LL) {
      return 1LL;
    }
  }
  return 0LL;
}

void MaxBoolArray::SetMax(int64 m) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->SetMax(m);
  }
}

string MaxBoolArray::DebugString() const {
  return DebugStringInternal("MaxBoolArray");
}

void MaxBoolArray::WhenRange(Demon* d) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->WhenRange(d);
  }
}

// ----- Builders -----

namespace {

void ScanArray(IntVar* const* vars, int size, int* bound,
               int64* amin, int64* amax, int64* min_max, int64* max_min) {
  *amin = kint64max;  // Max of the array.
  *min_max = kint64max;  // Smallest max in the array.
  *max_min = kint64min;  // Biggest min in the array.
  *amax = kint64min;  // Min of the array.
  *bound = 0;
  for (int i = 0; i < size; ++i) {
    const int64 vmin = vars[i]->Min();
    const int64 vmax = vars[i]->Max();
    if (vmin < *amin) {
      *amin = vmin;
    }
    if (vmax > *amax) {
      *amax = vmax;
    }
    if (vmax < *min_max) {
      *min_max = vmax;
    }
    if (vmin > *max_min) {
      *max_min = vmin;
    }
    if (vmin == vmax) {
      (*bound)++;
    }
  }
}

IntExpr* BuildMinArray(Solver* const s, IntVar* const* vars, int size) {
  int64 amin = 0, amax = 0, min_max = 0, max_min = 0;
  int bound = 0;
  ScanArray(vars, size, &bound, &amin, &amax, &min_max, &max_min);
  if (bound == size || amin == min_max) {  // Bound min(array)
    return s->MakeIntConst(amin);
  }
  if (amin == 0 && amax == 1) {
    return s->RevAlloc(new MinBoolArray(s, vars, size));
  }
  return s->RevAlloc(new MinArray(s, vars, size));
}

IntExpr* BuildMaxArray(Solver* const s, IntVar* const* vars, int size) {
  int64 amin = 0, amax = 0, min_max = 0, max_min = 0;
  int bound = 0;
  ScanArray(vars, size, &bound, &amin, &amax, &min_max, &max_min);
  if (bound == size || amax == max_min) {  // Bound max(array)
    return s->MakeIntConst(amax);
  }
  if (amin == 0 && amax == 1) {
    return s->RevAlloc(new MaxBoolArray(s, vars, size));
  }
  return s->RevAlloc(new MaxArray(s, vars, size));
}

enum BuildOp { MIN_OP, MAX_OP };

IntExpr* BuildLogSplitArray(Solver* const s,
                            IntVar* const* vars,
                            int size,
                            BuildOp op) {
  const int split_size = s->parameters().array_split_size;
  if (size == 0) {
    return s->MakeIntConst(0LL);
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    switch (op) {
      case MIN_OP:
        return s->MakeMin(vars[0], vars[1]);
      case MAX_OP:
        return s->MakeMax(vars[0], vars[1]);
    };
  } else if (size > split_size) {
    const int nb_blocks = (size - 1) / split_size + 1;
    const int block_size = (size + nb_blocks - 1) / nb_blocks;
    std::vector<IntVar*> top_vector;
    int start = 0;
    while (start < size) {
      int real_size = (start + block_size > size ? size - start : block_size);
      IntVar* intermediate = NULL;
      switch (op) {
        case MIN_OP:
          intermediate = s->MakeMin(vars + start, real_size)->Var();
          break;
        case MAX_OP:
          intermediate = s->MakeMax(vars + start, real_size)->Var();
          break;
      }
      top_vector.push_back(intermediate);
      start += real_size;
    }
    switch (op) {
      case MIN_OP:
        return s->MakeMin(top_vector);
      case MAX_OP:
        return s->MakeMax(top_vector);
    };
  } else {
    for (int i = 0; i < size; ++i) {
      CHECK_EQ(s, vars[i]->solver());
    }
    switch (op) {
      case MIN_OP:
        return BuildMinArray(s, vars, size);
      case MAX_OP:
        return BuildMaxArray(s, vars, size);
    };
  }
  LOG(FATAL) << "Unknown operator";
  return NULL;
}

IntExpr* BuildLogSplitArray(Solver* const s,
                            const std::vector<IntVar*>& vars,
                            BuildOp op) {
  return BuildLogSplitArray(s, vars.data(), vars.size(), op);
}

}  // namespace

IntExpr* Solver::MakeSum(const std::vector<IntVar*>& vars) {
  return MakeSum(vars.data(), vars.size());
}

IntExpr* Solver::MakeSum(IntVar* const* vars, int size) {
  if (size == 0) {
    return MakeIntConst(0LL);
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeSum(vars[0], vars[1]);
  } else {
    int64 sum_min = 0;
    int64 sum_max = 0;
    for (int i = 0; i < size; ++i) {
      sum_min += vars[i]->Min();
      sum_max += vars[i]->Max();
    }
    IntVar* const sum_var = MakeIntVar(sum_min, sum_max);
    AddConstraint(RevAlloc(new SumConstraint(this, vars, size, sum_var)));
    return sum_var;
  }
}

IntExpr* Solver::MakeMin(const std::vector<IntVar*>& vars) {
  return BuildLogSplitArray(this, vars, MIN_OP);
}

IntExpr* Solver::MakeMin(IntVar* const* vars, int size) {
  return BuildLogSplitArray(this, vars, size, MIN_OP);
}

IntExpr* Solver::MakeMax(const std::vector<IntVar*>& vars) {
  return BuildLogSplitArray(this, vars, MAX_OP);
}

IntExpr* Solver::MakeMax(IntVar* const* vars, int size) {
  return BuildLogSplitArray(this, vars, size, MAX_OP);
}

// ---------- Specialized cases ----------

namespace {
bool AreAllBooleans(const IntVar* const* vars, int size) {
  for (int i = 0; i < size; ++i) {
    const IntVar* var = vars[i];
    if (var->Min() < 0 || var->Max() > 1) {
      return false;
    }
  }
  return true;
}

template<class T> bool AreAllPositive(const T* const values, int size) {
  for (int i = 0; i < size; ++i) {
    if (values[i] < 0) {
      return false;
    }
  }
  return true;
}

template<class T> bool AreAllNull(const T* const values, int size) {
  for (int i = 0; i < size; ++i) {
    if (values[i] != 0) {
      return false;
    }
  }
  return true;
}

template <class T> bool AreAllBoundOrNull(const IntVar* const * vars,
                                          const T* const values,
                                          int size) {
  for (int i = 0; i < size; ++i) {
    if (values[i] != 0 && !vars[i]->Bound()) {
      return false;
    }
  }
  return true;
}

}  // namespace

class BaseSumBooleanConstraint : public Constraint {
 public:
  BaseSumBooleanConstraint(Solver* const s,
                           const IntVar* const* vars,
                           int size)
      : Constraint(s), vars_(new IntVar*[size]), size_(size), inactive_(false) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }
  virtual ~BaseSumBooleanConstraint() {}
 protected:
  string DebugStringInternal(const string& name) const;

  scoped_array<IntVar*> vars_;
  int size_;
  int inactive_;
};

string BaseSumBooleanConstraint::DebugStringInternal(const string& name) const {
  string out = name + "(";
  for (int i = 0; i < size_; ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += vars_[i]->DebugString();
  }
  out += ")";
  return out;
}

// ----- Sum of Boolean <= 1 -----

class SumBooleanLessOrEqualToOne : public  BaseSumBooleanConstraint {
 public:
  SumBooleanLessOrEqualToOne(Solver* const s,
                             const IntVar* const* vars,
                             int size)
      :  BaseSumBooleanConstraint(s, vars, size) {}

  virtual ~SumBooleanLessOrEqualToOne() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        Demon* u = MakeConstraintDemon1(solver(),
                                        this,
                                        &SumBooleanLessOrEqualToOne::Update,
                                        "Update",
                                        i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min() == 1) {
        PushAllToZeroExcept(i);
        return;
      }
    }
  }

  void Update(int index) {
    if (!inactive_) {
      DCHECK(vars_[index]->Bound());
      if (vars_[index]->Min() == 1) {
        PushAllToZeroExcept(index);
      }
    }
  }

  void PushAllToZeroExcept(int index) {
    solver()->SaveAndSetValue(&inactive_, 1);
    for (int i = 0; i < size_; ++i) {
      if (i != index && vars_[i]->Max() != 0) {
        vars_[i]->SetMax(0);
      }
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("SumBooleanLessOrEqualToOne");
  }
};

// ----- Sum of Boolean >= 1 -----

// We implement this one as a Max(array) == 1.

class SumBooleanGreaterOrEqualToOne : public BaseSumBooleanConstraint {
 public:
  SumBooleanGreaterOrEqualToOne(Solver* const s, const IntVar* const * vars,
                                int size);
  virtual ~SumBooleanGreaterOrEqualToOne() {}

  virtual void Post();
  virtual void InitialPropagate();

  void Update(int index);
  void UpdateVar();

  virtual string DebugString() const;

 private:
  RevBitSet bits_;
};

SumBooleanGreaterOrEqualToOne::SumBooleanGreaterOrEqualToOne(
    Solver* const s,
    const IntVar* const * vars,
    int size)
    : BaseSumBooleanConstraint(s, vars, size), bits_(size) {}

void SumBooleanGreaterOrEqualToOne::Post() {
  for (int i = 0; i < size_; ++i) {
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &SumBooleanGreaterOrEqualToOne::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(d);
  }
}

void SumBooleanGreaterOrEqualToOne::InitialPropagate() {
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (var->Min() == 1LL) {
      solver()->SaveAndSetValue(&inactive_, 1);
      return;
    }
    if (var->Max() == 1LL) {
      bits_.SetToOne(solver(), i);
    }
  }
  if (bits_.IsCardinalityZero()) {
    solver()->Fail();
  } else if (bits_.IsCardinalityOne()) {
    vars_[bits_.GetFirstBit(0)]->SetValue(1LL);
    solver()->SaveAndSetValue(&inactive_, 1);
  }
}

void SumBooleanGreaterOrEqualToOne::Update(int index) {
  if (!inactive_) {
    if (vars_[index]->Min() == 1LL) {  // Bound to 1.
      solver()->SaveAndSetValue(&inactive_, 1);
    } else {
      bits_.SetToZero(solver(), index);
      if (bits_.IsCardinalityZero()) {
        solver()->Fail();
      } else if (bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstBit(0)]->SetValue(1LL);
        solver()->SaveAndSetValue(&inactive_, 1);
      }
    }
  }
}

string SumBooleanGreaterOrEqualToOne::DebugString() const {
  return DebugStringInternal("SumBooleanGreaterOrEqualToOne");
}

// ----- Sum of Boolean == 1 -----

class SumBooleanEqualToOne : public BaseSumBooleanConstraint {
 public:
  SumBooleanEqualToOne(Solver* const s,
                       IntVar* const* vars,
                       int size)
      : BaseSumBooleanConstraint(s, vars, size), active_vars_(0) {}

  virtual ~SumBooleanEqualToOne() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      Demon* u = MakeConstraintDemon1(solver(),
                                      this,
                                      &SumBooleanEqualToOne::Update,
                                      "Update",
                                      i);
      vars_[i]->WhenBound(u);
    }
  }

  virtual void InitialPropagate() {
    int min1 = 0;
    int max1 = 0;
    int index_min = -1;
    int index_max = -1;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        min1++;
        index_min = i;
      }
      if (var->Max() == 1) {
        max1++;
        index_max = i;
      }
    }
    if (min1 > 1 || max1 == 0) {
      solver()->Fail();
    } else if (min1 == 1) {
      DCHECK_NE(-1, index_min);
      PushAllToZeroExcept(index_min);
    } else if (max1 == 1) {
      DCHECK_NE(-1, index_max);
      vars_[index_max]->SetValue(1);
      solver()->SaveAndSetValue(&inactive_, 1);
    } else {
      solver()->SaveAndSetValue(&active_vars_, max1);
    }
  }

  void Update(int index) {
    if (!inactive_) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        solver()->SaveAndAdd(&active_vars_, -1);
        DCHECK_GE(active_vars_, 0);
        if (active_vars_ == 0) {
          solver()->Fail();
        } else if (active_vars_ == 1) {
          bool found = false;
          for (int i = 0; i < size_; ++i) {
            IntVar* const var = vars_[i];
            if (var->Max() == 1) {
              var->SetValue(1);
              PushAllToZeroExcept(i);
              found = true;
              break;
            }
          }
          if (!found) {
            solver()->Fail();
          }
        }
      } else {
        PushAllToZeroExcept(index);
      }
    }
  }

  void PushAllToZeroExcept(int index) {
    solver()->SaveAndSetValue(&inactive_, 1);
    for (int i = 0; i < size_; ++i) {
      if (i != index && vars_[i]->Max() != 0) {
        vars_[i]->SetMax(0);
      }
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("SumBooleanEqualToOne");
  }

 private:
  int active_vars_;
};

// ----- Sum of Boolean Equal To Var -----

class SumBooleanEqualToVar : public BaseSumBooleanConstraint {
 public:
  SumBooleanEqualToVar(Solver* const s,
                       IntVar* const* bool_vars,
                       int size,
                       IntVar* const sum_var)
      : BaseSumBooleanConstraint(s, bool_vars, size),
        num_possible_true_vars_(0),
        num_always_true_vars_(0),
        sum_var_(sum_var) {}

  virtual ~SumBooleanEqualToVar() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      Demon* const u = MakeConstraintDemon1(solver(),
                                            this,
                                            &SumBooleanEqualToVar::Update,
                                            "Update",
                                            i);
      vars_[i]->WhenBound(u);
    }
    if (!sum_var_->Bound()) {
      Demon* const u = MakeConstraintDemon0(solver(),
                                            this,
                                            &SumBooleanEqualToVar::UpdateVar,
                                            "UpdateVar");
      sum_var_->WhenRange(u);
    }
  }

  virtual void InitialPropagate() {
    int num_always_true_vars = 0;
    int possible_true = 0;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true_vars++;
      }
      if (var->Max() == 1) {
        possible_true++;
      }
    }
    sum_var_->SetRange(num_always_true_vars, possible_true);
    const int64 var_min = sum_var_->Min();
    const int64 var_max = sum_var_->Max();
    if (num_always_true_vars == var_max && possible_true > var_max) {
      PushAllUnboundToZero();
    } else if (possible_true == var_min && num_always_true_vars < var_min) {
      PushAllUnboundToOne();
    } else {
      solver()->SaveAndSetValue(&num_possible_true_vars_, possible_true);
      solver()->SaveAndSetValue(&num_always_true_vars_, num_always_true_vars);
    }
  }

  void UpdateVar() {
    if (num_possible_true_vars_ == sum_var_->Min()) {
      PushAllUnboundToOne();
    } else if (num_always_true_vars_ == sum_var_->Max()) {
      PushAllUnboundToZero();
    }
  }

  void Update(int index) {
    if (!inactive_) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        solver()->SaveAndAdd(&num_possible_true_vars_, -1);
        if (num_possible_true_vars_ == sum_var_->Min()) {
          PushAllUnboundToOne();
        }
      } else {
        DCHECK_EQ(1, value);
        solver()->SaveAndAdd(&num_always_true_vars_, 1);
        if (num_always_true_vars_ == sum_var_->Max()) {
          PushAllUnboundToZero();
        }
      }
    }
  }

  void PushAllUnboundToZero() {
    int64 counter = 0;
    solver()->SaveAndSetValue(&inactive_, 1);
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min() == 0) {
        vars_[i]->SetValue(0);
      } else {
        counter++;
      }
    }
    if (counter < sum_var_->Min() || counter > sum_var_->Max()) {
      solver()->Fail();
    }
  }

  void PushAllUnboundToOne() {
    int64 counter = 0;
    solver()->SaveAndSetValue(&inactive_, 1);
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Max() == 1) {
        vars_[i]->SetValue(1);
        counter++;
      }
    }
    if (counter < sum_var_->Min() || counter > sum_var_->Max()) {
      solver()->Fail();
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("SumBooleanEqualToVar");
  }

 private:
  int num_possible_true_vars_;
  int num_always_true_vars_;
  IntVar* const sum_var_;
};

// ---------- ScalProd ----------

// ----- Boolean Scal Prod -----

namespace {
struct Container {
  IntVar* var;
  int64 coef;
  Container(IntVar* v, int64 c) : var(v), coef(c) {}
  bool operator<(const Container& c) const { return (coef < c.coef); }
};

// This method will sort both vars and coefficients in increasing
// coefficient order. Vars with null coefficients will be
// removed. Bound vars will be collected and the sum of the
// corresponding products (when the var is bound to 1) is returned by
// this method.
int64 SortBothChangeConstant(IntVar** const vars,
                             int64* const coefs,
                             int* const size) {
  CHECK_NOTNULL(vars);
  CHECK_NOTNULL(coefs);
  CHECK_NOTNULL(size);
  int64 cst = 0;
  std::vector<Container> to_sort;
  for (int index = 0; index < *size; ++index) {
    if (vars[index]->Bound()) {
      cst += coefs[index] * vars[index]->Min();
    } else if (coefs[index] != 0) {
      to_sort.push_back(Container(vars[index], coefs[index]));
    }
  }
  std::sort(to_sort.begin(), to_sort.end());
  *size = to_sort.size();
  for (int index = 0; index < *size; ++index) {
    vars[index] = to_sort[index].var;
    coefs[index] = to_sort[index].coef;
  }
  return cst;
}
}  // namespace

// This constraint implements sum(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.
class BooleanScalProdLessConstant : public Constraint {
 public:
  BooleanScalProdLessConstant(Solver* const s,
                              const IntVar* const * vars,
                              int size,
                              const int64* const coefs,
                              int64 upper_bound)
      : Constraint(s),
        vars_(new IntVar*[size]),
        size_(size),
        coefs_(new int64[size]),
        upper_bound_(upper_bound),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    for (int i = 0; i < size_; ++i) {
      DCHECK_GE(coefs_[i], 0);
    }
    upper_bound_ -= SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    max_coefficient_.SetValue(s, coefs_[size_ - 1]);
  }

  BooleanScalProdLessConstant(Solver* const s,
                              const IntVar* const * vars,
                              int size,
                              const int* const coefs,
                              int64 upper_bound)
      : Constraint(s),
        vars_(new IntVar*[size]),
        size_(size),
        coefs_(new int64[size]),
        upper_bound_(upper_bound),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    for (int i = 0; i < size_; ++i) {
      DCHECK_GE(coefs[i], 0);
      coefs_[i] = coefs[i];
    }
    upper_bound_ -= SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    max_coefficient_.SetValue(s, coefs_[size_ - 1]);
  }

  virtual ~BooleanScalProdLessConstant() {}

  virtual void Post() {
    for (int var_index = 0; var_index < size_; ++var_index) {
      if (vars_[var_index]->Bound()) {
        continue;
      }
      Demon* d = MakeConstraintDemon1(
          solver(),
          this,
          &BooleanScalProdLessConstant::Update,
          "InitialPropagate",
          var_index);
      vars_[var_index]->WhenRange(d);
    }
  }

  void PushFromTop() {
    const int64 slack = upper_bound_ - sum_of_bound_variables_.Value();
    if (slack < 0) {
      solver()->Fail();
    }
    if (slack < max_coefficient_.Value()) {
      int64 last_unbound = first_unbound_backward_.Value();
      for (;last_unbound >= 0; --last_unbound) {
        if (!vars_[last_unbound]->Bound()) {
          if (coefs_[last_unbound] <= slack) {
            max_coefficient_.SetValue(solver(), coefs_[last_unbound]);
            break;
          } else {
            vars_[last_unbound]->SetValue(0);
          }
        }
      }
      first_unbound_backward_.SetValue(solver(), last_unbound);
    }
  }

  virtual void InitialPropagate() {
    Solver* const s = solver();
    int last_unbound = -1;
    int64 sum = 0LL;
    for (int index = 0; index < size_; ++index) {
      if (vars_[index]->Bound()) {
        const int64 value = vars_[index]->Min();
        sum += value * coefs_[index];
      } else {
        last_unbound = index;
      }
    }
    sum_of_bound_variables_.SetValue(s, sum);
    first_unbound_backward_.SetValue(s, last_unbound);
    PushFromTop();
  }

  void Update(int var_index) {
    if (vars_[var_index]->Min() == 1) {
      sum_of_bound_variables_.SetValue(
          solver(), sum_of_bound_variables_.Value() + coefs_[var_index]);
      PushFromTop();
    }
  }

  virtual string DebugString() const {
    string out =  "BooleanScalProdLessConstant([";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%s", vars_[i]->DebugString().c_str());
    }
    StringAppendF(&out, "], [");
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%" GG_LL_FORMAT "d", coefs_[i]);
    }
    StringAppendF(&out, "], %" GG_LL_FORMAT "d)", upper_bound_);
    return out;
  }
 private:
  scoped_array<IntVar*> vars_;
  int size_;
  scoped_array<int64> coefs_;
  int64 upper_bound_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_bound_variables_;
  Rev<int64> max_coefficient_;
};

// ----- PositiveBooleanScalProdEqVar -----

class PositiveBooleanScalProdEqVar : public Constraint {
 public:
  PositiveBooleanScalProdEqVar(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               const int64* const coefs,
                               IntVar* const var,
                               int64 constant)
      : Constraint(s),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        var_(var),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        sum_of_all_variables_(0LL),
        constant_(constant),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    constant_ += SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    max_coefficient_.SetValue(s, coefs_[size_ - 1]);
  }

  virtual ~PositiveBooleanScalProdEqVar() {}

  virtual void Post() {
    for (int var_index = 0; var_index < size_; ++var_index) {
      if (vars_[var_index]->Bound()) {
        continue;
      }
      Demon* const d =
          MakeConstraintDemon1(solver(),
                               this,
                               &PositiveBooleanScalProdEqVar::Update,
                               "Update",
                               var_index);
      vars_[var_index]->WhenRange(d);
    }
    if (!var_->Bound()) {
      Demon* const uv =
          MakeConstraintDemon0(solver(),
                               this,
                               &PositiveBooleanScalProdEqVar::Propagate,
                               "Propagate");
      var_->WhenRange(uv);
    }
  }

  void Propagate() {
    var_->SetRange(sum_of_bound_variables_.Value(),
                   sum_of_all_variables_.Value());
    const int64 slack_up = var_->Max() - sum_of_bound_variables_.Value();
    const int64 slack_down = sum_of_all_variables_.Value() - var_->Min();
    const int64 max_coeff = max_coefficient_.Value();
    if (slack_down < max_coeff || slack_up < max_coeff) {
      int64 last_unbound = first_unbound_backward_.Value();
      for (; last_unbound >= 0; --last_unbound) {
        if (!vars_[last_unbound]->Bound()) {
          if (coefs_[last_unbound] > slack_up) {
            vars_[last_unbound]->SetValue(0);
          } else if (coefs_[last_unbound] > slack_down) {
            vars_[last_unbound]->SetValue(1);
          } else {
            max_coefficient_.SetValue(solver(), coefs_[last_unbound]);
            break;
          }
        }
      }
      first_unbound_backward_.SetValue(solver(), last_unbound);
    }
  }

  virtual void InitialPropagate() {
    Solver* const s = solver();
    int last_unbound = -1;
    int64 sum_bound = constant_;
    int64 sum_all = constant_;
    for (int index = 0; index < size_; ++index) {
      const int64 value = vars_[index]->Max() * coefs_[index];
      sum_all += value;
      if (vars_[index]->Bound()) {
        sum_bound += value;
      } else {
        last_unbound = index;
      }
    }
    sum_of_bound_variables_.SetValue(s, sum_bound);
    sum_of_all_variables_.SetValue(s, sum_all);
    first_unbound_backward_.SetValue(s, last_unbound);
    Propagate();
  }

  void Update(int var_index) {
    if (vars_[var_index]->Min() == 1) {
      sum_of_bound_variables_.SetValue(
          solver(), sum_of_bound_variables_.Value() + coefs_[var_index]);
    } else {
      sum_of_all_variables_.SetValue(
          solver(), sum_of_all_variables_.Value() - coefs_[var_index]);
    }
    Propagate();
  }

  virtual string DebugString() const {
    string out =  "PositiveBooleanScalProdEqVar([";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%s", vars_[i]->DebugString().c_str());
    }
    StringAppendF(&out, "], [");
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%" GG_LL_FORMAT "d", coefs_[i]);
    }
    StringAppendF(&out, "], constant = %" GG_LL_FORMAT "d, %s)",
                  constant_, var_->DebugString().c_str());
    return out;
  }
 private:
  int size_;
  scoped_array<IntVar*> vars_;
  scoped_array<int64> coefs_;
  IntVar* const var_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_bound_variables_;
  Rev<int64> sum_of_all_variables_;
  int64 constant_;
  Rev<int64> max_coefficient_;
};

// ----- PositiveBooleanScalProd -----

class PositiveBooleanScalProd : public BaseIntExpr {
 public:
  // this constructor will copy the array. The caller can safely delete the
  // exprs array himself
  PositiveBooleanScalProd(Solver* const s,
                          const IntVar* const* vars,
                          int size,
                          const int64* const coefs)
      : BaseIntExpr(s),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        constant_(0LL) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    constant_ += SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    for (int i = 0; i < size_; ++i) {
      DCHECK_GE(coefs_[i], 0);
    }
  }

  PositiveBooleanScalProd(Solver* const s,
                          const IntVar* const* vars,
                          int size,
                          const int* const coefs)
      : BaseIntExpr(s),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        constant_(0LL) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    for (int i = 0; i < size_; ++i) {
      coefs_[i] = coefs[i];
      DCHECK_GE(coefs_[i], 0);
    }
    constant_ += SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
  }

  virtual ~PositiveBooleanScalProd() {}

  virtual int64 Min() const {
    int64 min = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min()) {
        min += coefs_[i];
      }
    }
    return min + constant_;
  }

  virtual void SetMin(int64 m) {
    SetRange(m, kint64max);
  }

  virtual int64 Max() const {
    int64 max = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Max()) {
        max += coefs_[i];
      }
    }
    return max + constant_;
  }

  virtual void SetMax(int64 m) {
    SetRange(kint64min, m);
  }

  virtual void SetRange(int64 l, int64 u) {
    int64 current_min = constant_;
    int64 current_max = constant_;
    int64 diameter = -1;
    for (int i = 0; i < size_; ++i) {
      const int64 coefficient = coefs_[i];
      const int64 var_min = vars_[i]->Min() * coefficient;
      const int64 var_max = vars_[i]->Max() * coefficient;
      current_min += var_min;
      current_max += var_max;
      if (var_min != var_max) {  // Coefficients are increasing.
        diameter = var_max - var_min;
      }
    }
    if (u >= current_max && l <= current_min) {
      return;
    }
    if (u < current_min || l > current_max) {
      solver()->Fail();
    }

    u = std::min(current_max, u);
    l = std::max(l, current_min);

    if (u - l > diameter) {
      return;
    }

    for (int i = 0; i < size_; ++i) {
      const int64 coefficient = coefs_[i];
      IntVar* const var = vars_[i];
      const int64 new_min = l - current_max + var->Max() * coefficient;
      const int64 new_max = u - current_min + var->Min() * coefficient;
      if (new_max < 0 || new_min > coefficient || new_min > new_max) {
        solver()->Fail();
      }
      if (new_min > 0LL) {
        var->SetMin(1LL);
      } else if (new_max < coefficient) {
        var->SetMax(0LL);
      }
    }
  }

  virtual string DebugString() const {
    string out =  "PositiveBooleanScalProd([";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%s", vars_[i]->DebugString().c_str());
    }
    StringAppendF(&out, "], [");
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%" GG_LL_FORMAT "d", coefs_[i]);
    }
    if (constant_) {
      StringAppendF(&out, "], constant = %" GG_LL_FORMAT "d)", constant_);
    } else {
      StringAppendF(&out, "])");
    }
    return out;
  }

  virtual void WhenRange(Demon* d) {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->WhenRange(d);
    }
  }
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = 0LL;
    int64 vmax = 0LL;
    Range(&vmin, &vmax);
    IntVar* const var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    if (size_ > 0) {
      Constraint* const ct = s->RevAlloc(
          new PositiveBooleanScalProdEqVar(s,
                                           vars_.get(),
                                           size_,
                                           coefs_.get(),
                                           var,
                                           constant_));
      s->AddConstraint(ct);
    }
    return var;
  }
 private:
  int size_;
  scoped_array<IntVar*> vars_;
  scoped_array<int64> coefs_;
  int64 constant_;
};

// ----- PositiveBooleanScalProdEqCst ----- (all constants >= 0)

class PositiveBooleanScalProdEqCst : public Constraint {
 public:
  PositiveBooleanScalProdEqCst(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               const int64* const coefs,
                               int64 constant)
      : Constraint(s),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        sum_of_all_variables_(0LL),
        constant_(constant),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    constant_ -= SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    max_coefficient_.SetValue(s, coefs_[size_ - 1]);
  }

  PositiveBooleanScalProdEqCst(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               const int* const coefs,
                               int64 constant)
      : Constraint(s),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        sum_of_all_variables_(0LL),
        constant_(constant),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    for (int i = 0; i < size; ++i) {
      coefs_[i] = coefs[i];
    }
    constant_ -= SortBothChangeConstant(vars_.get(), coefs_.get(), &size_);
    max_coefficient_.SetValue(s, coefs_[size_ - 1]);
  }

  virtual ~PositiveBooleanScalProdEqCst() {}

  virtual void Post() {
    for (int var_index = 0; var_index < size_; ++var_index) {
      if (!vars_[var_index]->Bound()) {
        Demon* const d =
            MakeConstraintDemon1(solver(),
                                 this,
                                 &PositiveBooleanScalProdEqCst::Update,
                                 "Update",
                                 var_index);
        vars_[var_index]->WhenRange(d);
      }
    }
  }

  void Propagate() {
    if (sum_of_bound_variables_.Value() > constant_ ||
        sum_of_all_variables_.Value() < constant_) {
      solver()->Fail();
    }
    const int64 slack_up = constant_ - sum_of_bound_variables_.Value();
    const int64 slack_down = sum_of_all_variables_.Value() - constant_;
    const int64 max_coeff = max_coefficient_.Value();
    if (slack_down < max_coeff || slack_up < max_coeff) {
      int64 last_unbound = first_unbound_backward_.Value();
      for (; last_unbound >= 0; --last_unbound) {
        if (!vars_[last_unbound]->Bound()) {
          if (coefs_[last_unbound] > slack_up) {
            vars_[last_unbound]->SetValue(0);
          } else if (coefs_[last_unbound] > slack_down) {
            vars_[last_unbound]->SetValue(1);
          } else {
            max_coefficient_.SetValue(solver(), coefs_[last_unbound]);
            break;
          }
        }
      }
      first_unbound_backward_.SetValue(solver(), last_unbound);
    }
  }

  virtual void InitialPropagate() {
    Solver* const s = solver();
    int last_unbound = -1;
    int64 sum_bound = 0LL;
    int64 sum_all = 0LL;
    for (int index = 0; index < size_; ++index) {
      const int64 value = vars_[index]->Max() * coefs_[index];
      sum_all += value;
      if (vars_[index]->Bound()) {
        sum_bound += value;
      } else {
        last_unbound = index;
      }
    }
    sum_of_bound_variables_.SetValue(s, sum_bound);
    sum_of_all_variables_.SetValue(s, sum_all);
    first_unbound_backward_.SetValue(s, last_unbound);
    Propagate();
  }

  void Update(int var_index) {
    if (vars_[var_index]->Min() == 1) {
      sum_of_bound_variables_.SetValue(
          solver(), sum_of_bound_variables_.Value() + coefs_[var_index]);
    } else {
      sum_of_all_variables_.SetValue(
          solver(), sum_of_all_variables_.Value() - coefs_[var_index]);
    }
    Propagate();
  }

  virtual string DebugString() const {
    string out =  "PositiveBooleanScalProdEqCst([";
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%s", vars_[i]->DebugString().c_str());
    }
    StringAppendF(&out, "], [");
    for (int i = 0; i < size_; ++i) {
      if (i > 0) {
        StringAppendF(&out, ", ");
      }
      StringAppendF(&out, "%" GG_LL_FORMAT "d", coefs_[i]);
    }
    StringAppendF(&out, "], constant = %" GG_LL_FORMAT "d)", constant_);
    return out;
  }
 private:
  int size_;
  scoped_array<IntVar*> vars_;
  scoped_array<int64> coefs_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_bound_variables_;
  Rev<int64> sum_of_all_variables_;
  int64 constant_;
  Rev<int64> max_coefficient_;
};

// ----- API -----

Constraint* Solver::MakeSumLessOrEqual(const std::vector<IntVar*>& vars, int64 cst) {
  return MakeSumLessOrEqual(vars.data(), vars.size(), cst);
}

Constraint* Solver::MakeSumLessOrEqual(IntVar* const* vars,
                                       int size,
                                       int64 cst) {
  if (cst == 1LL && AreAllBooleans(vars, size) && size > 2) {
    return RevAlloc(new SumBooleanLessOrEqualToOne(this, vars, size));
  } else {
    return MakeLessOrEqual(MakeSum(vars, size), cst);
  }
}

Constraint* Solver::MakeSumGreaterOrEqual(const std::vector<IntVar*>& vars,
                                          int64 cst) {
  return MakeSumGreaterOrEqual(vars.data(), vars.size(), cst);
}

Constraint* Solver::MakeSumGreaterOrEqual(IntVar* const* vars,
                                          int size,
                                          int64 cst) {
  if (cst == 1LL && AreAllBooleans(vars, size) && size > 2) {
    return RevAlloc(new SumBooleanGreaterOrEqualToOne(this, vars, size));
  } else {
    return MakeGreaterOrEqual(MakeSum(vars, size), cst);
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars, int64 cst) {
  return MakeSumEquality(vars.data(), vars.size(), cst);
}

Constraint* Solver::MakeSumEquality(IntVar* const* vars,
                                    int size,
                                    int64 cst) {
  if (AreAllBooleans(vars, size) && size > 2) {
    if (cst == 1) {
      return RevAlloc(new SumBooleanEqualToOne(this, vars, size));
    } else if (cst < 0 || cst > size) {
      return MakeFalseConstraint();
    } else {
      return RevAlloc(new SumBooleanEqualToVar(this,
                                               vars,
                                               size,
                                               MakeIntConst(cst)));
    }
  } else {
    return MakeEquality(MakeSum(vars, size), cst);
  }
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int64>& coefficients,
                                         int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEquality(vars.data(),
                              vars.size(),
                              coefficients.data(),
                              cst);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEquality(vars.data(),
                              vars.size(),
                              coefficients.data(),
                              cst);
}

template<class T> Constraint* MakeScalProdEqualityFct(Solver* const solver,
                                                      IntVar* const * vars,
                                                      int size,
                                                      T const * coefficients,
                                                      int64 cst) {
  if (size == 0 || AreAllNull<T>(coefficients, size)) {
    return cst == 0 ? solver->MakeTrueConstraint()
        : solver->MakeFalseConstraint();
  }
  if (AreAllBooleans(vars, size) && AreAllPositive<T>(coefficients, size)) {
    // TODO(user) : bench BooleanScalProdEqVar with IntConst.
    return solver->RevAlloc(new PositiveBooleanScalProdEqCst(solver,
                                                             vars,
                                                             size,
                                                             coefficients,
                                                             cst));
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
  }
  return solver->MakeEquality(solver->MakeSum(terms), cst);
}


Constraint* Solver::MakeScalProdEquality(IntVar* const * vars,
                                         int size,
                                         int64 const * coefficients,
                                         int64 cst) {
  return MakeScalProdEqualityFct<int64>(this, vars, size, coefficients, cst);
}

Constraint* Solver::MakeScalProdEquality(IntVar* const * vars,
                                         int size,
                                         int const * coefficients,
                                         int64 cst) {
  return MakeScalProdEqualityFct<int>(this, vars, size, coefficients, cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int64>& coeffs,
                                               int64 cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqual(vars.data(),
                                    vars.size(),
                                    coeffs.data(),
                                    cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int>& coeffs,
                                               int64 cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqual(vars.data(),
                                    vars.size(),
                                    coeffs.data(),
                                    cst);
}

template<class T>
Constraint* MakeScalProdGreaterOrEqualFct(Solver* solver,
                                          IntVar* const * vars,
                                          int size,
                                          T const * coefficients,
                                          int64 cst) {
  if (size == 0 || AreAllNull<T>(coefficients, size)) {
    return cst <= 0 ? solver->MakeTrueConstraint()
        : solver->MakeFalseConstraint();
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
  }
  return solver->MakeGreaterOrEqual(solver->MakeSum(terms), cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(IntVar* const * vars,
                                               int size,
                                               int64 const * coefficients,
                                               int64 cst) {
  return MakeScalProdGreaterOrEqualFct<int64>(this,
                                              vars, size, coefficients, cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(IntVar* const * vars,
                                               int size,
                                               int const * coefficients,
                                               int64 cst) {
  return MakeScalProdGreaterOrEqualFct<int>(this,
                                            vars, size, coefficients, cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                            const std::vector<int64>& coefficients,
                                            int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqual(vars.data(),
                                 vars.size(),
                                 coefficients.data(),
                                 cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                            const std::vector<int>& coefficients,
                                            int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqual(vars.data(),
                                 vars.size(),
                                 coefficients.data(),
                                 cst);
}

template<class T> Constraint* MakeScalProdLessOrEqualFct(Solver* solver,
                                                         IntVar* const * vars,
                                                         int size,
                                                         T const * coefficients,
                                                         int64 upper_bound) {
  if (size == 0 || AreAllNull<T>(coefficients, size)) {
    return upper_bound >= 0 ? solver->MakeTrueConstraint()
        : solver->MakeFalseConstraint();
  }
  // TODO(user) : compute constant on the fly.
  if (AreAllBoundOrNull(vars, coefficients, size)) {
    int64 cst = 0;
    for (int i = 0; i < size; ++i) {
      cst += vars[i]->Min() * coefficients[i];
    }
    return cst <= upper_bound ?
        solver->MakeTrueConstraint() :
        solver->MakeFalseConstraint();
  }
  if (AreAllBooleans(vars, size) && AreAllPositive<T>(coefficients, size)) {
    return solver->RevAlloc(new BooleanScalProdLessConstant(solver,
                                                            vars,
                                                            size,
                                                            coefficients,
                                                            upper_bound));
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
  }
  return solver->MakeLessOrEqual(solver->MakeSum(terms), upper_bound);
}

Constraint* Solver::MakeScalProdLessOrEqual(IntVar* const * vars,
                                            int size,
                                            int64 const * coefficients,
                                            int64 cst) {
  return MakeScalProdLessOrEqualFct<int64>(this, vars, size, coefficients, cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(IntVar* const * vars,
                                            int size,
                                            int const * coefficients,
                                            int64 cst) {
  return MakeScalProdLessOrEqualFct<int>(this, vars, size, coefficients, cst);
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int64>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProd(vars.data(), coefs.data(), vars.size());
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProd(vars.data(), coefs.data(), vars.size());
}

template<class T> IntExpr* MakeScalProdFct(Solver* solver,
                                           IntVar* const * vars,
                                           const T* const coefs,
                                           int size) {
  if (size == 0 || AreAllNull<T>(coefs, size)) {
    return solver->MakeIntConst(0LL);
  }
  if (AreAllBoundOrNull(vars, coefs, size)) {
    int64 cst = 0;
    for (int i = 0; i < size; ++i) {
      cst += vars[i]->Min() * coefs[i];
    }
    return solver->MakeIntConst(cst);
  }
  if (AreAllBooleans(vars, size)) {
    if (AreAllPositive<T>(coefs, size)) {
      return solver->RevAlloc(
          new PositiveBooleanScalProd(solver, vars, size, coefs));
    } else {
      // If some coefficients are non-positive, partition coefficients in two
      // sets, one for the positive coefficients P and one for the negative
      // ones N.
      // Create two PositiveBooleanScalProd expressions, one on P (s1), the
      // other on Opposite(N) (s2).
      // The final expression is then s1 - s2.
      // If P is empty, the expression is Opposite(s2).
      std::vector<T> positive_coefs;
      std::vector<T> negative_coefs;
      std::vector<IntVar*> positive_coef_vars;
      std::vector<IntVar*> negative_coef_vars;
      for (int i = 0; i < size; ++i) {
        const T coef = coefs[i];
        if (coef > 0) {
          positive_coefs.push_back(coef);
          positive_coef_vars.push_back(vars[i]);
        } else if (coef < 0) {
          negative_coefs.push_back(-coef);
          negative_coef_vars.push_back(vars[i]);
        }
      }
      CHECK_GT(negative_coef_vars.size(), 0);
      IntExpr* negatives =
          solver->RevAlloc(
              new PositiveBooleanScalProd(solver,
                                          negative_coef_vars.data(),
                                          negative_coef_vars.size(),
                                          negative_coefs.data()));
      if (!positive_coefs.empty()) {
        IntExpr* positives =
            solver->RevAlloc(
                new PositiveBooleanScalProd(solver,
                                            positive_coef_vars.data(),
                                            positive_coef_vars.size(),
                                            positive_coefs.data()));
        // Cast to var to avoid slow propagation; all operations on the expr are
        // O(n)!
        return solver->MakeDifference(positives->Var(), negatives->Var());
      } else {
        return solver->MakeOpposite(negatives);
      }
    }
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSum(terms);
}

IntExpr* Solver::MakeScalProd(IntVar* const * vars,
                              const int64* const coefs,
                              int size) {
  return MakeScalProdFct<int64>(this, vars, coefs, size);
}

IntExpr* Solver::MakeScalProd(IntVar* const * vars,
                              const int* const coefs,
                              int size) {
  return MakeScalProdFct<int>(this, vars, coefs, size);
}


}  // namespace operations_research
