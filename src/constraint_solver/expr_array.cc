// Copyright 2010-2012 Google
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

#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

namespace operations_research {
namespace {
// ----- Tree Array Constraint -----

class TreeArrayConstraint : public CastConstraint {
 public:
  TreeArrayConstraint(Solver* const solver,
                      const std::vector<IntVar*>& vars,
                      IntVar* const sum_var)
      : CastConstraint(solver, sum_var),
        vars_(vars),
        size_(vars.size()),
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

  string DebugStringInternal(const string& name) const {
    return StringPrintf("%s(%s) == %s",
                        name.c_str(),
                        DebugStringVector(vars_, ", ").c_str(),
                        target_var_->DebugString().c_str());
  }

  void AcceptInternal(const string& name, ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(name, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.data(), vars_.size());
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(name, this);
  }

  // Increases min by delta_min, reduces max by delta_max.
  void ReduceRange(int depth, int position, int64 delta_min, int64 delta_max) {
    NodeInfo* const info = &tree_[depth][position];
    if (delta_min > 0) {
      info->node_min.SetValue(solver(), info->node_min.Value() + delta_min);
    }
    if (delta_max > 0) {
      info->node_max.SetValue(solver(), info->node_max.Value() - delta_max);
    }
  }

  // Sets the range on the given node.
  void SetRange(int depth, int position, int64 new_min, int64 new_max) {
    NodeInfo* const info = &tree_[depth][position];
    if (new_min > info->node_min.Value()) {
      info->node_min.SetValue(solver(), new_min);
    }
    if (new_max < info->node_max.Value()) {
      info->node_max.SetValue(solver(), new_max);
    }
  }

  void InitLeaf(Solver* const solver,
                int position,
                int64 var_min,
                int64 var_max) {
    InitNode(solver, MaxDepth(), position, var_min, var_max);
  }

  void InitNode(Solver* const solver,
                int depth,
                int position,
                int64 node_min,
                int64 node_max) {
    tree_[depth][position].node_min.SetValue(solver, node_min);
    tree_[depth][position].node_max.SetValue(solver, node_max);
  }

  int64 Min(int depth, int position) const {
    return tree_[depth][position].node_min.Value();
  }

  int64 Max(int depth, int position) const {
    return tree_[depth][position].node_max.Value();
  }

  int64 RootMin() const {
    return root_node_->node_min.Value();
  }

  int64 RootMax() const {
    return root_node_->node_max.Value();
  }

  int Parent(int position) const {
    return position / block_size_;
  }

  int ChildStart(int position) const {
    return position * block_size_;
  }

  int ChildEnd(int depth, int position) const {
    DCHECK_LT(depth + 1, tree_.size());
    return std::min((position + 1) * block_size_ - 1, Width(depth + 1) - 1);
  }

  bool IsLeaf(int depth) const {
    return depth == MaxDepth();
  }

  int MaxDepth() const {
    return tree_.size() - 1;
  }

  int Width(int depth) const {
    return tree_[depth].size();
  }

 protected:
  std::vector<IntVar*> vars_;
  const int size_;

 private:
  struct NodeInfo {
    NodeInfo() : node_min(0), node_max(0) {}
    Rev<int64> node_min;
    Rev<int64> node_max;
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
                const std::vector<IntVar*>& vars,
                IntVar* const sum_var)
      : TreeArrayConstraint(solver, vars, sum_var), sum_demon_(NULL) {}

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
    sum_demon_ = solver()->RegisterDemon(
        MakeDelayedConstraintDemon0(solver(),
                                    this,
                                    &SumConstraint::SumChanged,
                                    "SumChanged"));
    target_var_->WhenRange(sum_demon_);
  }

  virtual void InitialPropagate() {
    // Copy vars to leaf nodes.
    for (int i = 0; i < size_; ++i) {
      InitLeaf(solver(), i, vars_[i]->Min(), vars_[i]->Max());
    }
    // Compute up.
    for (int i = MaxDepth() - 1; i >= 0; --i) {
      for (int j = 0; j < Width(i); ++j) {
        int64 sum_min = 0;
        int64 sum_max = 0;
        const int block_start = ChildStart(j);
        const int block_end = ChildEnd(i, j);
        for (int k = block_start; k <= block_end; ++k) {
          sum_min += Min(i + 1, k);
          sum_max += Max(i + 1, k);
        }
        InitNode(solver(), i, j, sum_min, sum_max);
      }
    }
    // Propagate to sum_var.
    target_var_->SetRange(RootMin(), RootMax());

    // Push down.
    SumChanged();
  }

  void SumChanged() {
    if (target_var_->Max() == RootMin()) {
      // We can fix all terms to min.
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetValue(vars_[i]->Min());
      }
    } else if (target_var_->Min() == RootMax()) {
      // We can fix all terms to max.
      for (int i = 0; i < size_; ++i) {
        vars_[i]->SetValue(vars_[i]->Max());
      }
    } else {
      PushDown(0, 0, target_var_->Min(), target_var_->Max());
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
      const int64 target_var_min = Min(depth + 1, i);
      const int64 target_var_max = Max(depth + 1, i);
      const int64 residual_min = sum_min - target_var_min;
      const int64 residual_max = sum_max - target_var_max;
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
    for (int depth = MaxDepth(); depth >= 0; --depth) {
      ReduceRange(depth, position, delta_min, delta_max);
      position = Parent(position);
    }
    DCHECK_EQ(0, position);
    target_var_->SetRange(RootMin(), RootMax());
  }

  string DebugString() const {
    return DebugStringInternal("Sum");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    AcceptInternal(ModelVisitor::kSumEqual, visitor);
  }

 private:
  Demon* sum_demon_;
};

// ---------- Min Array ----------

// This constraint implements min(vars) == min_var.
class MinConstraint : public TreeArrayConstraint {
 public:
  MinConstraint(Solver* const solver,
                const std::vector<IntVar*>& vars,
                IntVar* const min_var)
      : TreeArrayConstraint(solver, vars, min_var), min_demon_(NULL) {}

  virtual ~MinConstraint() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      Demon* const demon = MakeConstraintDemon1(solver(),
                                                this,
                                                &MinConstraint::LeafChanged,
                                                "LeafChanged",
                                                i);
      vars_[i]->WhenRange(demon);
    }
    min_demon_ = solver()->RegisterDemon(
        MakeDelayedConstraintDemon0(solver(),
                                    this,
                                    &MinConstraint::MinVarChanged,
                                    "MinVarChanged"));
    target_var_->WhenRange(min_demon_);
  }

  virtual void InitialPropagate() {
    // Copy vars to leaf nodes.
    for (int i = 0; i < size_; ++i) {
      InitLeaf(solver(), i, vars_[i]->Min(), vars_[i]->Max());
    }

    // Compute up.
    for (int i = MaxDepth() - 1; i >= 0; --i) {
      for (int j = 0; j < Width(i); ++j) {
        int64 min_min = kint64max;
        int64 min_max = kint64max;
        const int block_start = ChildStart(j);
        const int block_end = ChildEnd(i, j);
        for (int k = block_start; k <= block_end; ++k) {
          min_min = std::min(min_min, Min(i + 1, k));
          min_max = std::min(min_max, Max(i + 1, k));
        }
        InitNode(solver(), i, j, min_min, min_max);
      }
    }
    // Propagate to min_var.
    target_var_->SetRange(RootMin(), RootMax());

    // Push down.
    MinVarChanged();
  }

  void MinVarChanged() {
    PushDown(0, 0, target_var_->Min(), target_var_->Max());
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

    const int64 node_min = Min(depth, position);
    const int64 node_max = Max(depth, position);

    int candidate = -1;
    int active = 0;
    const int block_start = ChildStart(position);
    const int block_end = ChildEnd(depth, position);

    if (new_max < node_max) {
      // Look if only one candidat to push the max down.
      for (int i = block_start; i <= block_end; ++i) {
        if (Min(depth + 1, i) <= new_max) {
          if (active++ >= 1) {
            break;
          }
          candidate = i;
        }
      }
      if (active == 0) {
        solver()->Fail();
      }
    }

    if (node_min < new_min) {
      for (int i = block_start; i <= block_end; ++i) {
        if (i == candidate && active == 1) {
          PushDown(depth + 1, i, new_min, new_max);
        } else {
          PushDown(depth + 1, i, new_min, Max(depth + 1, i));
        }
      }
    } else if (active == 1) {
      PushDown(depth + 1, candidate, Min(depth + 1, candidate), new_max);
    }
  }

  void LeafChanged(int term_index) {
    IntVar* const var = vars_[term_index];
    SetRange(MaxDepth(), term_index, var->Min(), var->Max());
    PushUp(term_index);
  }

  void PushUp(int position) {
    int depth = MaxDepth();
    while (depth > 0) {
      const int parent = Parent(position);
      const int parent_depth = depth - 1;
      int64 min_min = kint64max;
      int64 min_max = kint64max;
      const int block_start = ChildStart(parent);
      const int block_end = ChildEnd(parent_depth, parent);
      for (int k = block_start; k <= block_end; ++k) {
        min_min = std::min(min_min, Min(depth, k));
        min_max = std::min(min_max, Max(depth, k));
      }
      if (min_min > Min(parent_depth, parent) ||
          min_max < Max(parent_depth, parent)) {
        SetRange(parent_depth, parent, min_min, min_max);
      } else {
        break;
      }
      depth = parent_depth;
      position = parent;
    }
    if (depth == 0) {  // We have pushed all the way up.
      target_var_->SetRange(RootMin(), RootMax());
    }
    MinVarChanged();
  }

  string DebugString() const {
    return DebugStringInternal("Min");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    AcceptInternal(ModelVisitor::kMinEqual, visitor);
  }

 private:
  Demon* min_demon_;
};

// ---------- Max Array ----------

// This constraint implements max(vars) == max_var.
class MaxConstraint : public TreeArrayConstraint {
 public:
  MaxConstraint(Solver* const solver,
                const std::vector<IntVar*>& vars,
                IntVar* const max_var)
      : TreeArrayConstraint(solver, vars, max_var), max_demon_(NULL) {}

  virtual ~MaxConstraint() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      Demon* const demon = MakeConstraintDemon1(solver(),
                                                this,
                                                &MaxConstraint::LeafChanged,
                                                "LeafChanged",
                                                i);
      vars_[i]->WhenRange(demon);
    }
    max_demon_ = solver()->RegisterDemon(
        MakeDelayedConstraintDemon0(solver(),
                                    this,
                                    &MaxConstraint::MaxVarChanged,
                                    "MaxVarChanged"));
    target_var_->WhenRange(max_demon_);
  }

  virtual void InitialPropagate() {
    // Copy vars to leaf nodes.
    for (int i = 0; i < size_; ++i) {
      InitLeaf(solver(), i, vars_[i]->Min(), vars_[i]->Max());
    }

    // Compute up.
    for (int i = MaxDepth() - 1; i >= 0; --i) {
      for (int j = 0; j < Width(i); ++j) {
        int64 max_min = kint64min;
        int64 max_max = kint64min;
        const int block_start = ChildStart(j);
        const int block_end = ChildEnd(i, j);
        for (int k = block_start; k <= block_end; ++k) {
          max_min = std::max(max_min, Min(i + 1, k));
          max_max = std::max(max_max, Max(i + 1, k));
        }
        InitNode(solver(), i, j, max_min, max_max);
      }
    }
    // Propagate to min_var.
    target_var_->SetRange(RootMin(), RootMax());

    // Push down.
    MaxVarChanged();
  }

  void MaxVarChanged() {
    PushDown(0, 0, target_var_->Min(), target_var_->Max());
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

    const int64 node_min = Min(depth, position);
    const int64 node_max = Max(depth, position);

    int candidate = -1;
    int active = 0;
    const int block_start = ChildStart(position);
    const int block_end = ChildEnd(depth, position);

    if (node_min < new_min) {
      // Look if only one candidat to push the max down.
      for (int i = block_start; i <= block_end; ++i) {
        if (Max(depth + 1, i) >= new_min) {
          if (active++ >= 1) {
            break;
          }
          candidate = i;
        }
      }
      if (active == 0) {
        solver()->Fail();
      }
    }

    if (node_max > new_max) {
      for (int i = block_start; i <= block_end; ++i) {
        if (i == candidate && active == 1) {
          PushDown(depth + 1, i, new_min, new_max);
        } else {
          PushDown(depth + 1, i, Min(depth + 1, i), new_max);
        }
      }
    } else if (active == 1) {
      PushDown(depth + 1, candidate, new_min, Max(depth + 1, candidate));
    }
  }

  void LeafChanged(int term_index) {
    IntVar* const var = vars_[term_index];
    SetRange(MaxDepth(), term_index, var->Min(), var->Max());
    PushUp(term_index);
  }

  void PushUp(int position) {
    int depth = MaxDepth();
    while (depth > 0) {
      const int parent = Parent(position);
      const int parent_depth = depth - 1;
      int64 max_min = kint64min;
      int64 max_max = kint64min;
      const int block_start = ChildStart(parent);
      const int block_end = ChildEnd(parent_depth, parent);
      for (int k = block_start; k <= block_end; ++k) {
        max_min = std::max(max_min, Min(depth, k));
        max_max = std::max(max_max, Max(depth, k));
      }
      if (max_min > Min(parent_depth, parent) ||
          max_max < Max(parent_depth, parent)) {
        SetRange(parent_depth, parent, max_min, max_max);
      } else {
        break;
      }
      depth = parent_depth;
      position = parent;
    }
    if (depth == 0) {  // We have pushed all the way up.
      target_var_->SetRange(RootMin(), RootMax());
    }
    MaxVarChanged();
  }

  string DebugString() const {
    return DebugStringInternal("Max");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    AcceptInternal(ModelVisitor::kMaxEqual, visitor);
  }

 private:
  Demon* max_demon_;
};
}  // namespace

IntExpr* Solver::MakeSum(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    return MakeIntConst(0LL);
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeSum(vars[0], vars[1]);
  } else {
    int64 new_min = 0;
    int64 new_max = 0;
    for (int i = 0; i < size; ++i) {
      new_min += vars[i]->Min();
      new_max += vars[i]->Max();
    }
    IntVar* const sum_var = MakeIntVar(new_min, new_max);
    AddConstraint(RevAlloc(new SumConstraint(this, vars, sum_var)));
    return sum_var;
  }
}

IntExpr* Solver::MakeMin(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    return MakeIntConst(0LL);
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMin(vars[0], vars[1]);
  } else {
    int64 new_min = kint64max;
    int64 new_max = kint64max;
    for (int i = 0; i < size; ++i) {
      new_min = std::min(new_min, vars[i]->Min());
      new_max = std::min(new_max, vars[i]->Max());
    }
    IntVar* const new_var = MakeIntVar(new_min, new_max);
    AddConstraint(RevAlloc(new MinConstraint(this, vars, new_var)));
    return new_var;
  }
}

Constraint* Solver::MakeMinEquality(const std::vector<IntVar*>& vars,
                                    IntVar* const min_var) {
  return RevAlloc(new MinConstraint(this, vars, min_var));
}

IntExpr* Solver::MakeMax(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    return MakeIntConst(0LL);
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMax(vars[0], vars[1]);
  } else {
    int64 new_min = kint64min;
    int64 new_max = kint64min;
    for (int i = 0; i < size; ++i) {
      new_min = std::max(new_min, vars[i]->Min());
      new_max = std::max(new_max, vars[i]->Max());
    }
    IntVar* const new_var = MakeIntVar(new_min, new_max);
    AddConstraint(RevAlloc(new MaxConstraint(this, vars, new_var)));
    return new_var;
  }
}

Constraint* Solver::MakeMaxEquality(const std::vector<IntVar*>& vars,
                                    IntVar* const max_var) {
  return RevAlloc(new MaxConstraint(this, vars, max_var));
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

class BaseSumBooleanConstraint : public Constraint {
 public:
  BaseSumBooleanConstraint(Solver* const s,
                           const IntVar* const* vars,
                           int size)
      : Constraint(s), vars_(new IntVar*[size]), size_(size) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }
  virtual ~BaseSumBooleanConstraint() {}

 protected:
  string DebugStringInternal(const string& name) const;

  const scoped_array<IntVar*> vars_;
  const int size_;
  RevSwitch inactive_;
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
    if (!inactive_.Switched()) {
      DCHECK(vars_[index]->Bound());
      if (vars_[index]->Min() == 1) {
        PushAllToZeroExcept(index);
      }
    }
  }

  void PushAllToZeroExcept(int index) {
    inactive_.Switch(solver());
    for (int i = 0; i < size_; ++i) {
      if (i != index && vars_[i]->Max() != 0) {
        vars_[i]->SetMax(0);
      }
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("SumBooleanLessOrEqualToOne");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumLessOrEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
    visitor->EndVisitConstraint(ModelVisitor::kSumLessOrEqual, this);
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumGreaterOrEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
    visitor->EndVisitConstraint(ModelVisitor::kSumGreaterOrEqual, this);
  }

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
      inactive_.Switch(solver());
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
    inactive_.Switch(solver());
  }
}

void SumBooleanGreaterOrEqualToOne::Update(int index) {
  if (!inactive_.Switched()) {
    if (vars_[index]->Min() == 1LL) {  // Bound to 1.
      inactive_.Switch(solver());
    } else {
      bits_.SetToZero(solver(), index);
      if (bits_.IsCardinalityZero()) {
        solver()->Fail();
      } else if (bits_.IsCardinalityOne()) {
        vars_[bits_.GetFirstBit(0)]->SetValue(1LL);
        inactive_.Switch(solver());
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
      inactive_.Switch(solver());
    } else {
      active_vars_.SetValue(solver(), max1);
    }
  }

  void Update(int index) {
    if (!inactive_.Switched()) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        active_vars_.Decr(solver());
        DCHECK_GE(active_vars_.Value(), 0);
        if (active_vars_.Value() == 0) {
          solver()->Fail();
        } else if (active_vars_.Value() == 1) {
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
    inactive_.Switch(solver());
    for (int i = 0; i < size_; ++i) {
      if (i != index && vars_[i]->Max() != 0) {
        vars_[i]->SetMax(0);
      }
    }
  }

  virtual string DebugString() const {
    return DebugStringInternal("SumBooleanEqualToOne");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  NumericalRev<int> active_vars_;
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
      num_possible_true_vars_.SetValue(solver(), possible_true);
      num_always_true_vars_.SetValue(solver(), num_always_true_vars);
    }
  }

  void UpdateVar() {
    if (num_possible_true_vars_.Value() == sum_var_->Min()) {
      PushAllUnboundToOne();
    } else if (num_always_true_vars_.Value() == sum_var_->Max()) {
      PushAllUnboundToZero();
    }
  }

  void Update(int index) {
    if (!inactive_.Switched()) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        num_possible_true_vars_.Decr(solver());
        sum_var_->SetRange(num_always_true_vars_.Value(),
                           num_possible_true_vars_.Value());
        if (num_possible_true_vars_.Value() == sum_var_->Min()) {
          PushAllUnboundToOne();
        }
      } else {
        DCHECK_EQ(1, value);
        num_always_true_vars_.Incr(solver());
        sum_var_->SetRange(num_always_true_vars_.Value(),
                           num_possible_true_vars_.Value());
        if (num_always_true_vars_.Value() == sum_var_->Max()) {
          PushAllUnboundToZero();
        }
      }
    }
  }

  void PushAllUnboundToZero() {
    int64 counter = 0;
    inactive_.Switch(solver());
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
    inactive_.Switch(solver());
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
    return StringPrintf("%s == %s",
                        DebugStringInternal("SumBoolean").c_str(),
                        sum_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            sum_var_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
  IntVar* const sum_var_;
};

// ---------- ScalProd ----------

// ----- Boolean Scal Prod -----

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
// If keep_inside is true, the constant will be added back into the
// scalprod as IntConst(1) * constant.
int64 SortBothChangeConstant(IntVar** const vars,
                             int64* const coefs,
                             int* const size,
                             bool keep_inside) {
  CHECK_NOTNULL(vars);
  CHECK_NOTNULL(coefs);
  CHECK_NOTNULL(size);
  if (*size == 0) {
    return 0;
  }
  int64 cst = 0;
  std::vector<Container> to_sort;
  for (int index = 0; index < *size; ++index) {
    if (vars[index]->Bound()) {
      cst += coefs[index] * vars[index]->Min();
    } else if (coefs[index] != 0) {
      to_sort.push_back(Container(vars[index], coefs[index]));
    }
  }
  if (keep_inside && cst != 0) {
    CHECK_LT(to_sort.size(), *size);
    Solver* const solver = vars[0]->solver();
    to_sort.push_back(Container(solver->MakeIntConst(1), cst));
    cst = 0;
  }
  std::sort(to_sort.begin(), to_sort.end());
  *size = to_sort.size();
  for (int index = 0; index < *size; ++index) {
    vars[index] = to_sort[index].var;
    coefs[index] = to_sort[index].coef;
  }
  return cst;
}

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
    upper_bound_ -= SortBothChangeConstant(vars_.get(),
                                           coefs_.get(),
                                           &size_,
                                           false);
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
    upper_bound_ -= SortBothChangeConstant(vars_.get(),
                                           coefs_.get(),
                                           &size_,
                                           false);
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
      for (; last_unbound >= 0; --last_unbound) {
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
    return StringPrintf("BooleanScalProd([%s], [%s]) <= %" GG_LL_FORMAT "d)",
                        DebugStringArray(vars_.get(), size_, ", ").c_str(),
                        Int64ArrayToString(coefs_.get(), size_, ", ").c_str(),
                        upper_bound_);
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kScalProdLessOrEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       coefs_.get(),
                                       size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  upper_bound_);
    visitor->EndVisitConstraint(ModelVisitor::kScalProdLessOrEqual, this);
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

class PositiveBooleanScalProdEqVar : public CastConstraint {
 public:
  PositiveBooleanScalProdEqVar(Solver* const s,
                               const IntVar* const * vars,
                               int size,
                               const int64* const coefs,
                               IntVar* const var)
      : CastConstraint(s, var),
        size_(size),
        vars_(new IntVar*[size_]),
        coefs_(new int64[size_]),
        first_unbound_backward_(size_ - 1),
        sum_of_bound_variables_(0LL),
        sum_of_all_variables_(0LL),
        max_coefficient_(0) {
    CHECK_GT(size, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    SortBothChangeConstant(vars_.get(), coefs_.get(), &size_, true);
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
    if (!target_var_->Bound()) {
      Demon* const uv =
          MakeConstraintDemon0(solver(),
                               this,
                               &PositiveBooleanScalProdEqVar::Propagate,
                               "Propagate");
      target_var_->WhenRange(uv);
    }
  }

  void Propagate() {
    target_var_->SetRange(sum_of_bound_variables_.Value(),
                          sum_of_all_variables_.Value());
    const int64 slack_up = target_var_->Max() - sum_of_bound_variables_.Value();
    const int64 slack_down = sum_of_all_variables_.Value() - target_var_->Min();
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
    int64 sum_bound = 0;
    int64 sum_all = 0;
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
    return StringPrintf(
        "PositiveBooleanScal([%s], [%s]) == %s",
        DebugStringArray(vars_.get(), size_, ", ").c_str(),
        Int64ArrayToString(coefs_.get(), size_, ", ").c_str(),
        target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kScalProdEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       coefs_.get(),
                                       size_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kScalProdEqual, this);
  }

 private:
  int size_;
  scoped_array<IntVar*> vars_;
  scoped_array<int64> coefs_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_bound_variables_;
  Rev<int64> sum_of_all_variables_;
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
        coefs_(new int64[size_]) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    memcpy(coefs_.get(), coefs, size_ * sizeof(*coefs));
    SortBothChangeConstant(vars_.get(), coefs_.get(), &size_, true);
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
        coefs_(new int64[size_]) {
    CHECK_GT(size_, 0);
    CHECK(vars != NULL);
    CHECK(coefs != NULL);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    for (int i = 0; i < size_; ++i) {
      coefs_[i] = coefs[i];
      DCHECK_GE(coefs_[i], 0);
    }
    SortBothChangeConstant(vars_.get(), coefs_.get(), &size_, true);
  }

  virtual ~PositiveBooleanScalProd() {}

  virtual int64 Min() const {
    int64 min = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min()) {
        min += coefs_[i];
      }
    }
    return min;
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
    return max;
  }

  virtual void SetMax(int64 m) {
    SetRange(kint64min, m);
  }

  virtual void SetRange(int64 l, int64 u) {
    int64 current_min = 0;
    int64 current_max = 0;
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
    return StringPrintf(
        "PositiveBooleanScalProd([%s], [%s])",
        DebugStringArray(vars_.get(), size_, ", ").c_str(),
        Int64ArrayToString(coefs_.get(), size_, ", ").c_str());
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
    if (size_ > 0) {
      CastConstraint* const ct = s->RevAlloc(
          new PositiveBooleanScalProdEqVar(s,
                                           vars_.get(),
                                           size_,
                                           coefs_.get(),
                                           var));
      s->AddCastConstraint(ct, var, this);
    }
    return var;
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kScalProd, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       coefs_.get(),
                                       size_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kScalProd, this);
  }

 private:
  int size_;
  scoped_array<IntVar*> vars_;
  scoped_array<int64> coefs_;
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
    constant_ -= SortBothChangeConstant(vars_.get(),
                                        coefs_.get(),
                                        &size_,
                                        false);
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
    constant_ -= SortBothChangeConstant(vars_.get(),
                                        coefs_.get(),
                                        &size_,
                                        false);
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
    return StringPrintf(
        "PositiveBooleanScalProd([%s], [%s]) == %" GG_LL_FORMAT "d",
        DebugStringArray(vars_.get(), size_, ", ").c_str(),
        Int64ArrayToString(coefs_.get(), size_, ", ").c_str(),
        constant_);
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kScalProdEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       coefs_.get(),
                                       size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  constant_);
    visitor->EndVisitConstraint(ModelVisitor::kScalProdEqual, this);
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

}  // namespace
Constraint* Solver::MakeSumLessOrEqual(const std::vector<IntVar*>& vars, int64 cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars.data(), size) && size > 2) {
    return RevAlloc(new SumBooleanLessOrEqualToOne(this, vars.data(), size));
  } else {
    return MakeLessOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumGreaterOrEqual(const std::vector<IntVar*>& vars,
                                          int64 cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars.data(), size) && size > 2) {
    return RevAlloc(new SumBooleanGreaterOrEqualToOne(this, vars.data(), size));
  } else {
    return MakeGreaterOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars, int64 cst) {
  const int size = vars.size();
  if (AreAllBooleans(vars.data(), size) && size > 2) {
    if (cst == 1) {
      return RevAlloc(new SumBooleanEqualToOne(this, vars.data(), size));
    } else if (cst < 0 || cst > size) {
      return MakeFalseConstraint();
    } else {
      return RevAlloc(new SumBooleanEqualToVar(this,
                                               vars.data(),
                                               size,
                                               MakeIntConst(cst)));
    }
  } else {
    if (vars.size() == 1) {
      return MakeEquality(vars[0], cst);
    } else if (vars.size() == 2) {
      return MakeEquality(vars[0], MakeDifference(cst, vars[1])->Var());
    }
    return RevAlloc(new SumConstraint(this, vars, MakeIntConst(cst)));
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars,
                                    IntVar* const var) {
  const int size = vars.size();
  if (AreAllBooleans(vars.data(), size) && size > 2) {
    return RevAlloc(new SumBooleanEqualToVar(this, vars.data(), size, var));
  } else {
    return RevAlloc(new SumConstraint(this, vars, var));
  }
}

namespace {
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
  // Some simplications
  int constants = 0;
  int positives = 0;
  int negatives = 0;
  for (int i = 0; i < size; ++i) {
    if (coefficients[i] == 0 || vars[i]->Bound()) {
      constants++;
    } else if (coefficients[i] > 0) {
      positives++;
    } else {
      negatives++;
    }
  }
  if (positives > 0 && negatives > 0) {
    std::vector<IntVar*> pos_terms;
    std::vector<IntVar*> neg_terms;
    int64 rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefficients[i] == 0 || vars[i]->Bound()) {
        rhs -= coefficients[i] * vars[i]->Min();
      } else if (coefficients[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefficients[i])->Var());
      }
    }
    if (negatives == 1) {
      if (rhs != 0) {
        pos_terms.push_back(solver->MakeIntConst(-rhs));
      }
      return solver->MakeSumEquality(pos_terms, neg_terms[0]);
    } else if (positives == 1) {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeSumEquality(neg_terms, pos_terms[0]);
    } else {
      return solver->MakeEquality(
          solver->MakeSum(pos_terms)->Var(),
          solver->MakeSum(solver->MakeSum(neg_terms)->Var(), rhs)->Var());
    }
  } else if (positives == 1) {
    IntVar* pos_term = NULL;
    int64 rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefficients[i] == 0 || vars[i]->Bound()) {
        rhs -= coefficients[i] * vars[i]->Min();
      } else if (coefficients[i] > 0) {
        pos_term = solver->MakeProd(vars[i], coefficients[i])->Var();
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeEquality(pos_term, rhs);
  } else if (negatives == 1) {
    IntVar* neg_term = NULL;
    int64 rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefficients[i] == 0 || vars[i]->Bound()) {
        rhs -= coefficients[i] * vars[i]->Min();
      } else if (coefficients[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_term = solver->MakeProd(vars[i], -coefficients[i])->Var();
      }
    }
    return solver->MakeEquality(neg_term, -rhs);
  } else if (positives > 1) {
    std::vector<IntVar*> pos_terms;
    int64 rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefficients[i] == 0 || vars[i]->Bound()) {
        rhs -= coefficients[i] * vars[i]->Min();
      } else if (coefficients[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeSumEquality(pos_terms, rhs);
  } else if (negatives > 1) {
    std::vector<IntVar*> neg_terms;
    int64 rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefficients[i] == 0 || vars[i]->Bound()) {
        rhs -= coefficients[i] * vars[i]->Min();
      } else if (coefficients[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefficients[i])->Var());
      }
    }
    return solver->MakeSumEquality(neg_terms, -rhs);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefficients[i])->Var());
  }
  return solver->MakeSumEquality(terms, solver->MakeIntConst(cst));
}
}  // namespace

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int64>& coefficients,
                                         int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct<int64>(this,
                                        vars.data(),
                                        vars.size(),
                                        coefficients.data(),
                                        cst);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct<int>(this,
                                      vars.data(),
                                      vars.size(),
                                      coefficients.data(),
                                      cst);
}

namespace {
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
}  // namespace

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int64>& coeffs,
                                               int64 cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct<int64>(this,
                                              vars.data(),
                                              vars.size(),
                                              coeffs.data(),
                                              cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int>& coeffs,
                                               int64 cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct<int>(this,
                                            vars.data(),
                                            vars.size(),
                                            coeffs.data(),
                                            cst);
}

namespace {
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
}  // namespace

Constraint* Solver::MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                            const std::vector<int64>& coefficients,
                                            int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct<int64>(this,
                                           vars.data(),
                                           vars.size(),
                                           coefficients.data(),
                                           cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                            const std::vector<int>& coefficients,
                                            int64 cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct<int>(this,
                                         vars.data(),
                                         vars.size(),
                                         coefficients.data(),
                                         cst);
}

namespace {
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
      return solver->RegisterIntExpr(solver->RevAlloc(
          new PositiveBooleanScalProd(solver, vars, size, coefs)));
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
      IntExpr* const negatives =
          solver->RegisterIntExpr(solver->RevAlloc(
              new PositiveBooleanScalProd(solver,
                                          negative_coef_vars.data(),
                                          negative_coef_vars.size(),
                                          negative_coefs.data())));
      if (!positive_coefs.empty()) {
        IntExpr* const positives =
            solver->RegisterIntExpr(solver->RevAlloc(
                new PositiveBooleanScalProd(solver,
                                            positive_coef_vars.data(),
                                            positive_coef_vars.size(),
                                            positive_coefs.data())));
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
}  // namespace

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int64>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct<int64>(this, vars.data(), coefs.data(), vars.size());
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct<int>(this, vars.data(), coefs.data(), vars.size());
}



}  // namespace operations_research
