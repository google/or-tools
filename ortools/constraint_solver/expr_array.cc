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

#include "ortools/constraint_solver/expr_array.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/constraint_solver/visitor.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/string_array.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {
namespace {

struct Container {
  IntVar* var;
  int64_t coef;
  Container(IntVar* v, int64_t c) : var(v), coef(c) {}
  bool operator<(const Container& c) const { return (coef < c.coef); }
};

// This method will sort both vars and coefficients in increasing
// coefficient order. Vars with null coefficients will be
// removed. Bound vars will be collected and the sum of the
// corresponding products (when the var is bound to 1) is returned by
// this method.
// If keep_inside is true, the constant will be added back into the
// scalprod as IntConst(1) * constant.
int64_t SortBothChangeConstant(std::vector<IntVar*>* vars,
                               std::vector<int64_t>* coefs, bool keep_inside) {
  CHECK(vars != nullptr);
  CHECK(coefs != nullptr);
  if (vars->empty()) {
    return 0;
  }
  int64_t cst = 0;
  std::vector<Container> to_sort;
  for (int index = 0; index < vars->size(); ++index) {
    if ((*vars)[index]->Bound()) {
      cst = CapAdd(cst, CapProd((*coefs)[index], (*vars)[index]->Min()));
    } else if ((*coefs)[index] != 0) {
      to_sort.push_back(Container((*vars)[index], (*coefs)[index]));
    }
  }
  if (keep_inside && cst != 0) {
    CHECK_LT(to_sort.size(), vars->size());
    Solver* const solver = (*vars)[0]->solver();
    to_sort.push_back(Container(solver->MakeIntConst(1), cst));
    cst = 0;
  }
  std::sort(to_sort.begin(), to_sort.end());
  for (int index = 0; index < to_sort.size(); ++index) {
    (*vars)[index] = to_sort[index].var;
    (*coefs)[index] = to_sort[index].coef;
  }
  vars->resize(to_sort.size());
  coefs->resize(to_sort.size());
  return cst;
}
}  // namespace

// ----- Tree Array Constraint -----

TreeArrayConstraint::TreeArrayConstraint(Solver* solver,
                                         const std::vector<IntVar*>& vars,
                                         IntVar* sum_var)
    : CastConstraint(solver, sum_var),
      vars_(vars),
      block_size_(solver->parameters().array_split_size()) {
  std::vector<int> lengths;
  lengths.push_back(vars_.size());
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

std::string TreeArrayConstraint::DebugStringInternal(
    absl::string_view name) const {
  return absl::StrFormat("%s(%s) == %s", name, JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void TreeArrayConstraint::AcceptInternal(const std::string& name,
                                         ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(name, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(name, this);
}

// Increases min by delta_min, reduces max by delta_max.
void TreeArrayConstraint::ReduceRange(int depth, int position,
                                      int64_t delta_min, int64_t delta_max) {
  NodeInfo* const info = &tree_[depth][position];
  if (delta_min > 0) {
    info->node_min.SetValue(solver(),
                            CapAdd(info->node_min.Value(), delta_min));
  }
  if (delta_max > 0) {
    info->node_max.SetValue(solver(),
                            CapSub(info->node_max.Value(), delta_max));
  }
}

// Sets the range on the given node.
void TreeArrayConstraint::SetRange(int depth, int position, int64_t new_min,
                                   int64_t new_max) {
  NodeInfo* const info = &tree_[depth][position];
  if (new_min > info->node_min.Value()) {
    info->node_min.SetValue(solver(), new_min);
  }
  if (new_max < info->node_max.Value()) {
    info->node_max.SetValue(solver(), new_max);
  }
}

void TreeArrayConstraint::InitLeaf(int position, int64_t var_min,
                                   int64_t var_max) {
  InitNode(MaxDepth(), position, var_min, var_max);
}

void TreeArrayConstraint::InitNode(int depth, int position, int64_t node_min,
                                   int64_t node_max) {
  tree_[depth][position].node_min.SetValue(solver(), node_min);
  tree_[depth][position].node_max.SetValue(solver(), node_max);
}

int64_t TreeArrayConstraint::Min(int depth, int position) const {
  return tree_[depth][position].node_min.Value();
}

int64_t TreeArrayConstraint::Max(int depth, int position) const {
  return tree_[depth][position].node_max.Value();
}

int64_t TreeArrayConstraint::RootMin() const {
  return root_node_->node_min.Value();
}

int64_t TreeArrayConstraint::RootMax() const {
  return root_node_->node_max.Value();
}

int TreeArrayConstraint::Parent(int position) const {
  return position / block_size_;
}

int TreeArrayConstraint::ChildStart(int position) const {
  return position * block_size_;
}

int TreeArrayConstraint::ChildEnd(int depth, int position) const {
  DCHECK_LT(depth + 1, tree_.size());
  return std::min((position + 1) * block_size_ - 1, Width(depth + 1) - 1);
}

bool TreeArrayConstraint::IsLeaf(int depth) const {
  return depth == MaxDepth();
}

int TreeArrayConstraint::MaxDepth() const { return tree_.size() - 1; }

int TreeArrayConstraint::Width(int depth) const { return tree_[depth].size(); }

// ---------- Sum Array ----------

// ----- SumConstraint -----

TreeArrayConstraint::~TreeArrayConstraint() {}
std::string TreeArrayConstraint::DebugString() const {
  return "TreeArrayConstraint";
}
void TreeArrayConstraint::Accept(ModelVisitor* visitor) const {}

SumConstraint::SumConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                             IntVar* sum_var)
    : TreeArrayConstraint(solver, vars, sum_var), sum_demon_(nullptr) {}

SumConstraint::~SumConstraint() {}

void SumConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* const demon = MakeConstraintDemon1(
        solver(), this, &SumConstraint::LeafChanged, "LeafChanged", i);
    vars_[i]->WhenRange(demon);
  }
  sum_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &SumConstraint::SumChanged, "SumChanged"));
  target_var_->WhenRange(sum_demon_);
}

void SumConstraint::InitialPropagate() {
  // Copy vars to leaf nodes.
  for (int i = 0; i < vars_.size(); ++i) {
    InitLeaf(i, vars_[i]->Min(), vars_[i]->Max());
  }
  // Compute up.
  for (int i = MaxDepth() - 1; i >= 0; --i) {
    for (int j = 0; j < Width(i); ++j) {
      int64_t sum_min = 0;
      int64_t sum_max = 0;
      const int block_start = ChildStart(j);
      const int block_end = ChildEnd(i, j);
      for (int k = block_start; k <= block_end; ++k) {
        sum_min = CapAdd(sum_min, Min(i + 1, k));
        sum_max = CapAdd(sum_max, Max(i + 1, k));
      }
      InitNode(i, j, sum_min, sum_max);
    }
  }
  // Propagate to sum_var.
  target_var_->SetRange(RootMin(), RootMax());

  // Push down.
  SumChanged();
}

void SumConstraint::SumChanged() {
  if (target_var_->Max() == RootMin() &&
      target_var_->Max() != std::numeric_limits<int64_t>::max()) {
    // We can fix all terms to min.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Min());
    }
  } else if (target_var_->Min() == RootMax() &&
             target_var_->Min() != std::numeric_limits<int64_t>::min()) {
    // We can fix all terms to max.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Max());
    }
  } else {
    PushDown(0, 0, target_var_->Min(), target_var_->Max());
  }
}

void SumConstraint::PushDown(int depth, int position, int64_t new_min,
                             int64_t new_max) {
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
  const int64_t sum_min = Min(depth, position);
  const int64_t sum_max = Max(depth, position);

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
    const int64_t target_var_min = Min(depth + 1, i);
    const int64_t target_var_max = Max(depth + 1, i);
    const int64_t residual_min = CapSub(sum_min, target_var_min);
    const int64_t residual_max = CapSub(sum_max, target_var_max);
    PushDown(depth + 1, i, CapSub(new_min, residual_max),
             CapSub(new_max, residual_min));
  }
  // TODO(user) : Is the diameter optimization (see reference
  // above, rule 5) useful?
}

void SumConstraint::LeafChanged(int term_index) {
  IntVar* const var = vars_[term_index];
  PushUp(term_index, CapSub(var->Min(), var->OldMin()),
         CapSub(var->OldMax(), var->Max()));
  EnqueueDelayedDemon(sum_demon_);  // TODO(user): Is this needed?
}

void SumConstraint::PushUp(int position, int64_t delta_min, int64_t delta_max) {
  DCHECK_GE(delta_max, 0);
  DCHECK_GE(delta_min, 0);
  DCHECK_GT(CapAdd(delta_min, delta_max), 0);
  for (int depth = MaxDepth(); depth >= 0; --depth) {
    ReduceRange(depth, position, delta_min, delta_max);
    position = Parent(position);
  }
  DCHECK_EQ(0, position);
  target_var_->SetRange(RootMin(), RootMax());
}

std::string SumConstraint::DebugString() const {
  return DebugStringInternal("Sum");
}

void SumConstraint::Accept(ModelVisitor* visitor) const {
  AcceptInternal(ModelVisitor::kSumEqual, visitor);
}

SmallSumConstraint::SmallSumConstraint(Solver* solver,
                                       const std::vector<IntVar*>& vars,
                                       IntVar* target_var)
    : Constraint(solver),
      vars_(vars),
      target_var_(target_var),
      computed_min_(0),
      computed_max_(0),
      sum_demon_(nullptr) {}

SmallSumConstraint::~SmallSumConstraint() {}

void SmallSumConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      Demon* const demon =
          MakeConstraintDemon1(solver(), this, &SmallSumConstraint::VarChanged,
                               "VarChanged", vars_[i]);
      vars_[i]->WhenRange(demon);
    }
  }
  sum_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &SmallSumConstraint::SumChanged, "SumChanged"));
  target_var_->WhenRange(sum_demon_);
}

void SmallSumConstraint::InitialPropagate() {
  // Compute up.
  int64_t sum_min = 0;
  int64_t sum_max = 0;
  for (IntVar* const var : vars_) {
    sum_min = CapAdd(sum_min, var->Min());
    sum_max = CapAdd(sum_max, var->Max());
  }

  // Propagate to sum_var.
  computed_min_.SetValue(solver(), sum_min);
  computed_max_.SetValue(solver(), sum_max);
  target_var_->SetRange(sum_min, sum_max);

  // Push down.
  SumChanged();
}

void SmallSumConstraint::SumChanged() {
  int64_t new_min = target_var_->Min();
  int64_t new_max = target_var_->Max();
  const int64_t sum_min = computed_min_.Value();
  const int64_t sum_max = computed_max_.Value();
  if (new_max == sum_min && new_max != std::numeric_limits<int64_t>::max()) {
    // We can fix all terms to min.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Min());
    }
  } else if (new_min == sum_max &&
             new_min != std::numeric_limits<int64_t>::min()) {
    // We can fix all terms to max.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Max());
    }
  } else {
    if (new_min > sum_min || new_max < sum_max) {  // something to do.
      // Intersect the new bounds with the computed bounds.
      new_max = std::min(sum_max, new_max);
      new_min = std::max(sum_min, new_min);

      // Detect failure early.
      if (new_max < sum_min || new_min > sum_max) {
        solver()->Fail();
      }

      // Push to variables.
      for (IntVar* const var : vars_) {
        const int64_t var_min = var->Min();
        const int64_t var_max = var->Max();
        const int64_t residual_min = CapSub(sum_min, var_min);
        const int64_t residual_max = CapSub(sum_max, var_max);
        var->SetRange(CapSub(new_min, residual_max),
                      CapSub(new_max, residual_min));
      }
    }
  }
}

void SmallSumConstraint::VarChanged(IntVar* var) {
  const int64_t delta_min = CapSub(var->Min(), var->OldMin());
  const int64_t delta_max = CapSub(var->OldMax(), var->Max());
  computed_min_.Add(solver(), delta_min);
  computed_max_.Add(solver(), -delta_max);
  if (computed_max_.Value() < target_var_->Max() ||
      computed_min_.Value() > target_var_->Min()) {
    target_var_->SetRange(computed_min_.Value(), computed_max_.Value());
  } else {
    EnqueueDelayedDemon(sum_demon_);
  }
}

std::string SmallSumConstraint::DebugString() const {
  return absl::StrFormat("SmallSum(%s) == %s", JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void SmallSumConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
}

// ----- SafeSumConstraint -----

bool DetectSumOverflow(const std::vector<IntVar*>& vars) {
  int64_t sum_min = 0;
  int64_t sum_max = 0;
  for (int i = 0; i < vars.size(); ++i) {
    sum_min = CapAdd(sum_min, vars[i]->Min());
    sum_max = CapAdd(sum_max, vars[i]->Max());
    if (sum_min == std::numeric_limits<int64_t>::min() ||
        sum_max == std::numeric_limits<int64_t>::max()) {
      return true;
    }
  }
  return false;
}

SafeSumConstraint::SafeSumConstraint(Solver* solver,
                                     const std::vector<IntVar*>& vars,
                                     IntVar* sum_var)
    : TreeArrayConstraint(solver, vars, sum_var), sum_demon_(nullptr) {}

SafeSumConstraint::~SafeSumConstraint() {}

void SafeSumConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* const demon = MakeConstraintDemon1(
        solver(), this, &SafeSumConstraint::LeafChanged, "LeafChanged", i);
    vars_[i]->WhenRange(demon);
  }
  sum_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &SafeSumConstraint::SumChanged, "SumChanged"));
  target_var_->WhenRange(sum_demon_);
}

void SafeSumConstraint::SafeComputeNode(int depth, int position,
                                        int64_t* sum_min, int64_t* sum_max) {
  DCHECK_LT(depth, MaxDepth());
  const int block_start = ChildStart(position);
  const int block_end = ChildEnd(depth, position);
  for (int k = block_start; k <= block_end; ++k) {
    if (*sum_min != std::numeric_limits<int64_t>::min()) {
      *sum_min = CapAdd(*sum_min, Min(depth + 1, k));
    }
    if (*sum_max != std::numeric_limits<int64_t>::max()) {
      *sum_max = CapAdd(*sum_max, Max(depth + 1, k));
    }
    if (*sum_min == std::numeric_limits<int64_t>::min() &&
        *sum_max == std::numeric_limits<int64_t>::max()) {
      break;
    }
  }
}

void SafeSumConstraint::InitialPropagate() {
  // Copy vars to leaf nodes.
  for (int i = 0; i < vars_.size(); ++i) {
    InitLeaf(i, vars_[i]->Min(), vars_[i]->Max());
  }
  // Compute up.
  for (int i = MaxDepth() - 1; i >= 0; --i) {
    for (int j = 0; j < Width(i); ++j) {
      int64_t sum_min = 0;
      int64_t sum_max = 0;
      SafeComputeNode(i, j, &sum_min, &sum_max);
      InitNode(i, j, sum_min, sum_max);
    }
  }
  // Propagate to sum_var.
  target_var_->SetRange(RootMin(), RootMax());

  // Push down.
  SumChanged();
}

void SafeSumConstraint::SumChanged() {
  DCHECK(CheckInternalState());
  if (target_var_->Max() == RootMin()) {
    // We can fix all terms to min.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Min());
    }
  } else if (target_var_->Min() == RootMax()) {
    // We can fix all terms to max.
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(vars_[i]->Max());
    }
  } else {
    PushDown(0, 0, target_var_->Min(), target_var_->Max());
  }
}

void SafeSumConstraint::PushDown(int depth, int position, int64_t new_min,
                                 int64_t new_max) {
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
  const int64_t sum_min = Min(depth, position);
  const int64_t sum_max = Max(depth, position);

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
  for (int pos = block_start; pos <= block_end; ++pos) {
    const int64_t target_var_min = Min(depth + 1, pos);
    const int64_t residual_min = sum_min != std::numeric_limits<int64_t>::min()
                                     ? CapSub(sum_min, target_var_min)
                                     : std::numeric_limits<int64_t>::min();
    const int64_t target_var_max = Max(depth + 1, pos);
    const int64_t residual_max = sum_max != std::numeric_limits<int64_t>::max()
                                     ? CapSub(sum_max, target_var_max)
                                     : std::numeric_limits<int64_t>::max();
    PushDown(depth + 1, pos,
             (residual_max == std::numeric_limits<int64_t>::min()
                  ? std::numeric_limits<int64_t>::min()
                  : CapSub(new_min, residual_max)),
             (residual_min == std::numeric_limits<int64_t>::max()
                  ? std::numeric_limits<int64_t>::min()
                  : CapSub(new_max, residual_min)));
  }
  // TODO(user) : Is the diameter optimization (see reference
  // above, rule 5) useful?
}

void SafeSumConstraint::LeafChanged(int term_index) {
  IntVar* const var = vars_[term_index];
  PushUp(term_index, CapSub(var->Min(), var->OldMin()),
         CapSub(var->OldMax(), var->Max()));
  EnqueueDelayedDemon(sum_demon_);  // TODO(user): Is this needed?
}

void SafeSumConstraint::PushUp(int position, int64_t delta_min,
                               int64_t delta_max) {
  DCHECK_GE(delta_max, 0);
  DCHECK_GE(delta_min, 0);
  if (CapAdd(delta_min, delta_max) == 0) {
    // This may happen if the computation of old min/max has under/overflowed
    // resulting in no actual change in min and max.
    return;
  }
  bool delta_corrupted = false;
  for (int depth = MaxDepth(); depth >= 0; --depth) {
    if (Min(depth, position) != std::numeric_limits<int64_t>::min() &&
        Max(depth, position) != std::numeric_limits<int64_t>::max() &&
        delta_min != std::numeric_limits<int64_t>::max() &&
        delta_max != std::numeric_limits<int64_t>::max() &&
        !delta_corrupted) {  // No overflow.
      ReduceRange(depth, position, delta_min, delta_max);
    } else if (depth == MaxDepth()) {  // Leaf.
      SetRange(depth, position, vars_[position]->Min(), vars_[position]->Max());
      delta_corrupted = true;
    } else {  // Recompute.
      int64_t sum_min = 0;
      int64_t sum_max = 0;
      SafeComputeNode(depth, position, &sum_min, &sum_max);
      if (sum_min == std::numeric_limits<int64_t>::min() &&
          sum_max == std::numeric_limits<int64_t>::max()) {
        return;  // Nothing to do upward.
      }
      SetRange(depth, position, sum_min, sum_max);
      delta_corrupted = true;
    }
    position = Parent(position);
  }
  DCHECK_EQ(0, position);
  target_var_->SetRange(RootMin(), RootMax());
}

std::string SafeSumConstraint::DebugString() const {
  return DebugStringInternal("Sum");
}

void SafeSumConstraint::Accept(ModelVisitor* visitor) const {
  AcceptInternal(ModelVisitor::kSumEqual, visitor);
}

bool SafeSumConstraint::CheckInternalState() {
  for (int i = 0; i < vars_.size(); ++i) {
    CheckLeaf(i, vars_[i]->Min(), vars_[i]->Max());
  }
  // Check up.
  for (int i = MaxDepth() - 1; i >= 0; --i) {
    for (int j = 0; j < Width(i); ++j) {
      int64_t sum_min = 0;
      int64_t sum_max = 0;
      SafeComputeNode(i, j, &sum_min, &sum_max);
      CheckNode(i, j, sum_min, sum_max);
    }
  }
  return true;
}

void SafeSumConstraint::CheckLeaf(int position, int64_t var_min,
                                  int64_t var_max) {
  CheckNode(MaxDepth(), position, var_min, var_max);
}

void SafeSumConstraint::CheckNode(int depth, int position, int64_t node_min,
                                  int64_t node_max) {
  DCHECK_EQ(Min(depth, position), node_min);
  DCHECK_EQ(Max(depth, position), node_max);
}

// ---------- Min Array ----------

MinConstraint::MinConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                             IntVar* min_var)
    : TreeArrayConstraint(solver, vars, min_var), min_demon_(nullptr) {}

MinConstraint::~MinConstraint() {}

void MinConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* const demon = MakeConstraintDemon1(
        solver(), this, &MinConstraint::LeafChanged, "LeafChanged", i);
    vars_[i]->WhenRange(demon);
  }
  min_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &MinConstraint::MinVarChanged, "MinVarChanged"));
  target_var_->WhenRange(min_demon_);
}

void MinConstraint::InitialPropagate() {
  // Copy vars to leaf nodes.
  for (int i = 0; i < vars_.size(); ++i) {
    InitLeaf(i, vars_[i]->Min(), vars_[i]->Max());
  }

  // Compute up.
  for (int i = MaxDepth() - 1; i >= 0; --i) {
    for (int j = 0; j < Width(i); ++j) {
      int64_t min_min = std::numeric_limits<int64_t>::max();
      int64_t min_max = std::numeric_limits<int64_t>::max();
      const int block_start = ChildStart(j);
      const int block_end = ChildEnd(i, j);
      for (int k = block_start; k <= block_end; ++k) {
        min_min = std::min(min_min, Min(i + 1, k));
        min_max = std::min(min_max, Max(i + 1, k));
      }
      InitNode(i, j, min_min, min_max);
    }
  }
  // Propagate to min_var.
  target_var_->SetRange(RootMin(), RootMax());

  // Push down.
  MinVarChanged();
}

void MinConstraint::MinVarChanged() {
  PushDown(0, 0, target_var_->Min(), target_var_->Max());
}

void MinConstraint::PushDown(int depth, int position, int64_t new_min,
                             int64_t new_max) {
  // Nothing to do?
  if (new_min <= Min(depth, position) && new_max >= Max(depth, position)) {
    return;
  }

  // Leaf node -> push to leaf var.
  if (IsLeaf(depth)) {
    vars_[position]->SetRange(new_min, new_max);
    return;
  }

  const int64_t node_min = Min(depth, position);
  const int64_t node_max = Max(depth, position);

  int candidate = -1;
  int active = 0;
  const int block_start = ChildStart(position);
  const int block_end = ChildEnd(depth, position);

  if (new_max < node_max) {
    // Look if only one candidate to push the max down.
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

// TODO(user): Regroup code between Min and Max constraints.
void MinConstraint::LeafChanged(int term_index) {
  IntVar* const var = vars_[term_index];
  SetRange(MaxDepth(), term_index, var->Min(), var->Max());
  const int parent_depth = MaxDepth() - 1;
  const int parent = Parent(term_index);
  const int64_t old_min = var->OldMin();
  const int64_t var_min = var->Min();
  const int64_t var_max = var->Max();
  if ((old_min == Min(parent_depth, parent) && old_min != var_min) ||
      var_max < Max(parent_depth, parent)) {
    // Can influence the parent bounds.
    PushUp(term_index);
  }
}

void MinConstraint::PushUp(int position) {
  int depth = MaxDepth();
  while (depth > 0) {
    const int parent = Parent(position);
    const int parent_depth = depth - 1;
    int64_t min_min = std::numeric_limits<int64_t>::max();
    int64_t min_max = std::numeric_limits<int64_t>::max();
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

std::string MinConstraint::DebugString() const {
  return DebugStringInternal("Min");
}

void MinConstraint::Accept(ModelVisitor* visitor) const {
  AcceptInternal(ModelVisitor::kMinEqual, visitor);
}

SmallMinConstraint::SmallMinConstraint(Solver* solver,
                                       const std::vector<IntVar*>& vars,
                                       IntVar* target_var)
    : Constraint(solver),
      vars_(vars),
      target_var_(target_var),
      computed_min_(0),
      computed_max_(0) {}

SmallMinConstraint::~SmallMinConstraint() {}

void SmallMinConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      Demon* const demon =
          MakeConstraintDemon1(solver(), this, &SmallMinConstraint::VarChanged,
                               "VarChanged", vars_[i]);
      vars_[i]->WhenRange(demon);
    }
  }
  Demon* const mdemon = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &SmallMinConstraint::MinVarChanged, "MinVarChanged"));
  target_var_->WhenRange(mdemon);
}

void SmallMinConstraint::InitialPropagate() {
  int64_t min_min = std::numeric_limits<int64_t>::max();
  int64_t min_max = std::numeric_limits<int64_t>::max();
  for (IntVar* const var : vars_) {
    min_min = std::min(min_min, var->Min());
    min_max = std::min(min_max, var->Max());
  }
  computed_min_.SetValue(solver(), min_min);
  computed_max_.SetValue(solver(), min_max);
  // Propagate to min_var.
  target_var_->SetRange(min_min, min_max);

  // Push down.
  MinVarChanged();
}

std::string SmallMinConstraint::DebugString() const {
  return absl::StrFormat("SmallMin(%s) == %s", JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void SmallMinConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMinEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kMinEqual, this);
}

void SmallMinConstraint::VarChanged(IntVar* var) {
  const int64_t old_min = var->OldMin();
  const int64_t var_min = var->Min();
  const int64_t var_max = var->Max();
  if ((old_min == computed_min_.Value() && old_min != var_min) ||
      var_max < computed_max_.Value()) {
    // Can influence the min var bounds.
    int64_t min_min = std::numeric_limits<int64_t>::max();
    int64_t min_max = std::numeric_limits<int64_t>::max();
    for (IntVar* const var : vars_) {
      min_min = std::min(min_min, var->Min());
      min_max = std::min(min_max, var->Max());
    }
    if (min_min > computed_min_.Value() || min_max < computed_max_.Value()) {
      computed_min_.SetValue(solver(), min_min);
      computed_max_.SetValue(solver(), min_max);
      target_var_->SetRange(computed_min_.Value(), computed_max_.Value());
    }
  }
  MinVarChanged();
}

void SmallMinConstraint::MinVarChanged() {
  const int64_t new_min = target_var_->Min();
  const int64_t new_max = target_var_->Max();
  // Nothing to do?
  if (new_min <= computed_min_.Value() && new_max >= computed_max_.Value()) {
    return;
  }

  IntVar* candidate = nullptr;
  int active = 0;

  if (new_max < computed_max_.Value()) {
    // Look if only one candidate to push the max down.
    for (IntVar* const var : vars_) {
      if (var->Min() <= new_max) {
        if (++active > 1) {
          break;
        }
        candidate = var;
      }
    }
    if (active == 0) {
      solver()->Fail();
    }
  }
  if (computed_min_.Value() < new_min) {
    if (active == 1) {
      DCHECK(candidate != nullptr);
      candidate->SetRange(new_min, new_max);
    } else {
      for (IntVar* const var : vars_) {
        var->SetMin(new_min);
      }
    }
  } else if (active == 1) {
    DCHECK(candidate != nullptr);
    candidate->SetMax(new_max);
  }
}

// ---------- Max Array ----------

MaxConstraint::MaxConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                             IntVar* max_var)
    : TreeArrayConstraint(solver, vars, max_var), max_demon_(nullptr) {}

MaxConstraint::~MaxConstraint() {}

void MaxConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* const demon = MakeConstraintDemon1(
        solver(), this, &MaxConstraint::LeafChanged, "LeafChanged", i);
    vars_[i]->WhenRange(demon);
  }
  max_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &MaxConstraint::MaxVarChanged, "MaxVarChanged"));
  target_var_->WhenRange(max_demon_);
}

void MaxConstraint::InitialPropagate() {
  // Copy vars to leaf nodes.
  for (int i = 0; i < vars_.size(); ++i) {
    InitLeaf(i, vars_[i]->Min(), vars_[i]->Max());
  }

  // Compute up.
  for (int i = MaxDepth() - 1; i >= 0; --i) {
    for (int j = 0; j < Width(i); ++j) {
      int64_t max_min = std::numeric_limits<int64_t>::min();
      int64_t max_max = std::numeric_limits<int64_t>::min();
      const int block_start = ChildStart(j);
      const int block_end = ChildEnd(i, j);
      for (int k = block_start; k <= block_end; ++k) {
        max_min = std::max(max_min, Min(i + 1, k));
        max_max = std::max(max_max, Max(i + 1, k));
      }
      InitNode(i, j, max_min, max_max);
    }
  }
  // Propagate to min_var.
  target_var_->SetRange(RootMin(), RootMax());

  // Push down.
  MaxVarChanged();
}

void MaxConstraint::MaxVarChanged() {
  PushDown(0, 0, target_var_->Min(), target_var_->Max());
}

void MaxConstraint::PushDown(int depth, int position, int64_t new_min,
                             int64_t new_max) {
  // Nothing to do?
  if (new_min <= Min(depth, position) && new_max >= Max(depth, position)) {
    return;
  }

  // Leaf node -> push to leaf var.
  if (IsLeaf(depth)) {
    vars_[position]->SetRange(new_min, new_max);
    return;
  }

  const int64_t node_min = Min(depth, position);
  const int64_t node_max = Max(depth, position);

  int candidate = -1;
  int active = 0;
  const int block_start = ChildStart(position);
  const int block_end = ChildEnd(depth, position);

  if (node_min < new_min) {
    // Look if only one candidate to push the max down.
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

void MaxConstraint::LeafChanged(int term_index) {
  IntVar* const var = vars_[term_index];
  SetRange(MaxDepth(), term_index, var->Min(), var->Max());
  const int parent_depth = MaxDepth() - 1;
  const int parent = Parent(term_index);
  const int64_t old_max = var->OldMax();
  const int64_t var_min = var->Min();
  const int64_t var_max = var->Max();
  if ((old_max == Max(parent_depth, parent) && old_max != var_max) ||
      var_min > Min(parent_depth, parent)) {
    // Can influence the parent bounds.
    PushUp(term_index);
  }
}

void MaxConstraint::PushUp(int position) {
  int depth = MaxDepth();
  while (depth > 0) {
    const int parent = Parent(position);
    const int parent_depth = depth - 1;
    int64_t max_min = std::numeric_limits<int64_t>::min();
    int64_t max_max = std::numeric_limits<int64_t>::min();
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

std::string MaxConstraint::DebugString() const {
  return DebugStringInternal("Max");
}

void MaxConstraint::Accept(ModelVisitor* visitor) const {
  AcceptInternal(ModelVisitor::kMaxEqual, visitor);
}

SmallMaxConstraint::SmallMaxConstraint(Solver* solver,
                                       const std::vector<IntVar*>& vars,
                                       IntVar* target_var)
    : Constraint(solver),
      vars_(vars),
      target_var_(target_var),
      computed_min_(0),
      computed_max_(0) {}

SmallMaxConstraint::~SmallMaxConstraint() {}

void SmallMaxConstraint::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      Demon* const demon =
          MakeConstraintDemon1(solver(), this, &SmallMaxConstraint::VarChanged,
                               "VarChanged", vars_[i]);
      vars_[i]->WhenRange(demon);
    }
  }
  Demon* const mdemon = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &SmallMaxConstraint::MaxVarChanged, "MinVarChanged"));
  target_var_->WhenRange(mdemon);
}

void SmallMaxConstraint::InitialPropagate() {
  int64_t max_min = std::numeric_limits<int64_t>::min();
  int64_t max_max = std::numeric_limits<int64_t>::min();
  for (IntVar* const var : vars_) {
    max_min = std::max(max_min, var->Min());
    max_max = std::max(max_max, var->Max());
  }
  computed_min_.SetValue(solver(), max_min);
  computed_max_.SetValue(solver(), max_max);
  // Propagate to min_var.
  target_var_->SetRange(max_min, max_max);

  // Push down.
  MaxVarChanged();
}

std::string SmallMaxConstraint::DebugString() const {
  return absl::StrFormat("SmallMax(%s) == %s", JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void SmallMaxConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMaxEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kMaxEqual, this);
}

void SmallMaxConstraint::VarChanged(IntVar* var) {
  const int64_t old_max = var->OldMax();
  const int64_t var_min = var->Min();
  const int64_t var_max = var->Max();
  if ((old_max == computed_max_.Value() && old_max != var_max) ||
      var_min > computed_min_.Value()) {  // REWRITE
    // Can influence the min var bounds.
    int64_t max_min = std::numeric_limits<int64_t>::min();
    int64_t max_max = std::numeric_limits<int64_t>::min();
    for (IntVar* const var : vars_) {
      max_min = std::max(max_min, var->Min());
      max_max = std::max(max_max, var->Max());
    }
    if (max_min > computed_min_.Value() || max_max < computed_max_.Value()) {
      computed_min_.SetValue(solver(), max_min);
      computed_max_.SetValue(solver(), max_max);
      target_var_->SetRange(computed_min_.Value(), computed_max_.Value());
    }
  }
  MaxVarChanged();
}

void SmallMaxConstraint::MaxVarChanged() {
  const int64_t new_min = target_var_->Min();
  const int64_t new_max = target_var_->Max();
  // Nothing to do?
  if (new_min <= computed_min_.Value() && new_max >= computed_max_.Value()) {
    return;
  }

  IntVar* candidate = nullptr;
  int active = 0;

  if (new_min > computed_min_.Value()) {
    // Look if only one candidate to push the max down.
    for (IntVar* const var : vars_) {
      if (var->Max() >= new_min) {
        if (active++ >= 1) {
          break;
        }
        candidate = var;
      }
    }
    if (active == 0) {
      solver()->Fail();
    }
  }
  if (computed_max_.Value() > new_max) {
    if (active == 1) {
      DCHECK(candidate != nullptr);
      candidate->SetRange(new_min, new_max);
    } else {
      for (IntVar* const var : vars_) {
        var->SetMax(new_max);
      }
    }
  } else if (active == 1) {
    DCHECK(candidate != nullptr);
    candidate->SetMin(new_min);
  }
}

// Boolean And and Ors

ArrayBoolAndEq::ArrayBoolAndEq(Solver* s, const std::vector<IntVar*>& vars,
                               IntVar* target)
    : CastConstraint(s, target),
      vars_(vars),
      demons_(vars.size()),
      unbounded_(0) {}

ArrayBoolAndEq::~ArrayBoolAndEq() {}

void ArrayBoolAndEq::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      demons_[i] =
          MakeConstraintDemon1(solver(), this, &ArrayBoolAndEq::PropagateVar,
                               "PropagateVar", vars_[i]);
      vars_[i]->WhenBound(demons_[i]);
    }
  }
  if (!target_var_->Bound()) {
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &ArrayBoolAndEq::PropagateTarget, "PropagateTarget");
    target_var_->WhenBound(target_demon);
  }
}

void ArrayBoolAndEq::InitialPropagate() {
  target_var_->SetRange(0, 1);
  if (target_var_->Min() == 1) {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetMin(1);
    }
  } else {
    int possible_zero = -1;
    int ones = 0;
    int unbounded = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        unbounded++;
        possible_zero = i;
      } else if (vars_[i]->Max() == 0) {
        InhibitAll();
        target_var_->SetMax(0);
        return;
      } else {
        DCHECK_EQ(1, vars_[i]->Min());
        ones++;
      }
    }
    if (unbounded == 0) {
      target_var_->SetMin(1);
    } else if (target_var_->Max() == 0 && unbounded == 1) {
      CHECK_NE(-1, possible_zero);
      vars_[possible_zero]->SetMax(0);
    } else {
      unbounded_.SetValue(solver(), unbounded);
    }
  }
}

void ArrayBoolAndEq::PropagateVar(IntVar* var) {
  if (var->Min() == 1) {
    unbounded_.Decr(solver());
    if (unbounded_.Value() == 0 && !decided_.Switched()) {
      target_var_->SetMin(1);
      decided_.Switch(solver());
    } else if (target_var_->Max() == 0 && unbounded_.Value() == 1 &&
               !decided_.Switched()) {
      ForceToZero();
    }
  } else {
    InhibitAll();
    target_var_->SetMax(0);
  }
}

void ArrayBoolAndEq::PropagateTarget() {
  if (target_var_->Min() == 1) {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetMin(1);
    }
  } else {
    if (unbounded_.Value() == 1 && !decided_.Switched()) {
      ForceToZero();
    }
  }
}

std::string ArrayBoolAndEq::DebugString() const {
  return absl::StrFormat("And(%s) == %s", JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void ArrayBoolAndEq::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMinEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kMinEqual, this);
}

void ArrayBoolAndEq::InhibitAll() {
  for (int i = 0; i < demons_.size(); ++i) {
    if (demons_[i] != nullptr) {
      demons_[i]->inhibit(solver());
    }
  }
}

void ArrayBoolAndEq::ForceToZero() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Min() == 0) {
      vars_[i]->SetValue(0);
      decided_.Switch(solver());
      return;
    }
  }
  solver()->Fail();
}

ArrayBoolOrEq::ArrayBoolOrEq(Solver* s, const std::vector<IntVar*>& vars,
                             IntVar* target)
    : CastConstraint(s, target),
      vars_(vars),
      demons_(vars.size()),
      unbounded_(0) {}

ArrayBoolOrEq::~ArrayBoolOrEq() {}

void ArrayBoolOrEq::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      demons_[i] =
          MakeConstraintDemon1(solver(), this, &ArrayBoolOrEq::PropagateVar,
                               "PropagateVar", vars_[i]);
      vars_[i]->WhenBound(demons_[i]);
    }
  }
  if (!target_var_->Bound()) {
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &ArrayBoolOrEq::PropagateTarget, "PropagateTarget");
    target_var_->WhenBound(target_demon);
  }
}

void ArrayBoolOrEq::InitialPropagate() {
  target_var_->SetRange(0, 1);
  if (target_var_->Max() == 0) {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetMax(0);
    }
  } else {
    int zeros = 0;
    int possible_one = -1;
    int unbounded = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        unbounded++;
        possible_one = i;
      } else if (vars_[i]->Min() == 1) {
        InhibitAll();
        target_var_->SetMin(1);
        return;
      } else {
        DCHECK_EQ(0, vars_[i]->Max());
        zeros++;
      }
    }
    if (unbounded == 0) {
      target_var_->SetMax(0);
    } else if (target_var_->Min() == 1 && unbounded == 1) {
      CHECK_NE(-1, possible_one);
      vars_[possible_one]->SetMin(1);
    } else {
      unbounded_.SetValue(solver(), unbounded);
    }
  }
}

void ArrayBoolOrEq::PropagateVar(IntVar* var) {
  if (var->Min() == 0) {
    unbounded_.Decr(solver());
    if (unbounded_.Value() == 0 && !decided_.Switched()) {
      target_var_->SetMax(0);
      decided_.Switch(solver());
    }
    if (target_var_->Min() == 1 && unbounded_.Value() == 1 &&
        !decided_.Switched()) {
      ForceToOne();
    }
  } else {
    InhibitAll();
    target_var_->SetMin(1);
  }
}

void ArrayBoolOrEq::PropagateTarget() {
  if (target_var_->Max() == 0) {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetMax(0);
    }
  } else {
    if (unbounded_.Value() == 1 && !decided_.Switched()) {
      ForceToOne();
    }
  }
}

std::string ArrayBoolOrEq::DebugString() const {
  return absl::StrFormat("Or(%s) == %s", JoinDebugStringPtr(vars_),
                         target_var_->DebugString());
}

void ArrayBoolOrEq::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMaxEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kMaxEqual, this);
}

void ArrayBoolOrEq::InhibitAll() {
  for (int i = 0; i < demons_.size(); ++i) {
    if (demons_[i] != nullptr) {
      demons_[i]->inhibit(solver());
    }
  }
}

void ArrayBoolOrEq::ForceToOne() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Max() == 1) {
      vars_[i]->SetValue(1);
      decided_.Switch(solver());
      return;
    }
  }
  solver()->Fail();
}

// ---------- Specialized cases ----------

BaseSumBooleanConstraint::BaseSumBooleanConstraint(
    Solver* s, const std::vector<IntVar*>& vars)
    : Constraint(s), vars_(vars) {}

BaseSumBooleanConstraint::~BaseSumBooleanConstraint() {}

std::string BaseSumBooleanConstraint::DebugStringInternal(
    absl::string_view name) const {
  return absl::StrFormat("%s(%s)", name, JoinDebugStringPtr(vars_));
}

// ----- Sum of Boolean <= 1 -----

SumBooleanLessOrEqualToOne::SumBooleanLessOrEqualToOne(
    Solver* const s, const std::vector<IntVar*>& vars)
    : BaseSumBooleanConstraint(s, vars) {}

SumBooleanLessOrEqualToOne::~SumBooleanLessOrEqualToOne() {}

void SumBooleanLessOrEqualToOne::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!vars_[i]->Bound()) {
      Demon* u = MakeConstraintDemon1(solver(), this,
                                      &SumBooleanLessOrEqualToOne::Update,
                                      "Update", vars_[i]);
      vars_[i]->WhenBound(u);
    }
  }
}

void SumBooleanLessOrEqualToOne::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Min() == 1) {
      PushAllToZeroExcept(vars_[i]);
      return;
    }
  }
}

void SumBooleanLessOrEqualToOne::Update(IntVar* var) {
  if (!inactive_.Switched()) {
    DCHECK(var->Bound());
    if (var->Min() == 1) {
      PushAllToZeroExcept(var);
    }
  }
}

void SumBooleanLessOrEqualToOne::PushAllToZeroExcept(IntVar* var) {
  inactive_.Switch(solver());
  for (int i = 0; i < vars_.size(); ++i) {
    IntVar* const other = vars_[i];
    if (other != var && other->Max() != 0) {
      other->SetMax(0);
    }
  }
}

std::string SumBooleanLessOrEqualToOne::DebugString() const {
  return DebugStringInternal("SumBooleanLessOrEqualToOne");
}

void SumBooleanLessOrEqualToOne::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSumLessOrEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
  visitor->EndVisitConstraint(ModelVisitor::kSumLessOrEqual, this);
}

// ----- Sum of Boolean >= 1 -----

SumBooleanGreaterOrEqualToOne::SumBooleanGreaterOrEqualToOne(
    Solver* s, const std::vector<IntVar*>& vars)
    : BaseSumBooleanConstraint(s, vars), bits_(vars.size()) {}

SumBooleanGreaterOrEqualToOne::~SumBooleanGreaterOrEqualToOne() {}

void SumBooleanGreaterOrEqualToOne::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* d = MakeConstraintDemon1(
        solver(), this, &SumBooleanGreaterOrEqualToOne::Update, "Update", i);
    vars_[i]->WhenRange(d);
  }
}

void SumBooleanGreaterOrEqualToOne::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
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
    vars_[bits_.GetFirstBit(0)]->SetValue(int64_t{1});
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
        vars_[bits_.GetFirstBit(0)]->SetValue(int64_t{1});
        inactive_.Switch(solver());
      }
    }
  }
}

std::string SumBooleanGreaterOrEqualToOne::DebugString() const {
  return DebugStringInternal("SumBooleanGreaterOrEqualToOne");
}

void SumBooleanGreaterOrEqualToOne::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSumGreaterOrEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
  visitor->EndVisitConstraint(ModelVisitor::kSumGreaterOrEqual, this);
}

// ----- Sum of Boolean == 1 -----

SumBooleanEqualToOne::SumBooleanEqualToOne(Solver* s,
                                           const std::vector<IntVar*>& vars)
    : BaseSumBooleanConstraint(s, vars), active_vars_(0) {}

SumBooleanEqualToOne::~SumBooleanEqualToOne() {}

void SumBooleanEqualToOne::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* u = MakeConstraintDemon1(solver(), this,
                                    &SumBooleanEqualToOne::Update, "Update", i);
    vars_[i]->WhenBound(u);
  }
}

void SumBooleanEqualToOne::InitialPropagate() {
  int min1 = 0;
  int max1 = 0;
  int index_min = -1;
  int index_max = -1;
  for (int i = 0; i < vars_.size(); ++i) {
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

void SumBooleanEqualToOne::Update(int index) {
  if (!inactive_.Switched()) {
    DCHECK(vars_[index]->Bound());
    const int64_t value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      active_vars_.Decr(solver());
      DCHECK_GE(active_vars_.Value(), 0);
      if (active_vars_.Value() == 0) {
        solver()->Fail();
      } else if (active_vars_.Value() == 1) {
        bool found = false;
        for (int i = 0; i < vars_.size(); ++i) {
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

void SumBooleanEqualToOne::PushAllToZeroExcept(int index) {
  inactive_.Switch(solver());
  for (int i = 0; i < vars_.size(); ++i) {
    if (i != index && vars_[i]->Max() != 0) {
      vars_[i]->SetMax(0);
    }
  }
}

std::string SumBooleanEqualToOne::DebugString() const {
  return DebugStringInternal("SumBooleanEqualToOne");
}

void SumBooleanEqualToOne::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, 1);
  visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
}

// ----- Sum of Boolean Equal To Var -----

SumBooleanEqualToVar::SumBooleanEqualToVar(
    Solver* s, const std::vector<IntVar*>& bool_vars, IntVar* sum_var)
    : BaseSumBooleanConstraint(s, bool_vars),
      num_possible_true_vars_(0),
      num_always_true_vars_(0),
      sum_var_(sum_var) {}

SumBooleanEqualToVar::~SumBooleanEqualToVar() {}

void SumBooleanEqualToVar::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    Demon* const u = MakeConstraintDemon1(
        solver(), this, &SumBooleanEqualToVar::Update, "Update", i);
    vars_[i]->WhenBound(u);
  }
  if (!sum_var_->Bound()) {
    Demon* const u = MakeConstraintDemon0(
        solver(), this, &SumBooleanEqualToVar::UpdateVar, "UpdateVar");
    sum_var_->WhenRange(u);
  }
}

void SumBooleanEqualToVar::InitialPropagate() {
  int num_always_true_vars = 0;
  int possible_true = 0;
  for (int i = 0; i < vars_.size(); ++i) {
    const IntVar* const var = vars_[i];
    if (var->Min() == 1) {
      num_always_true_vars++;
    }
    if (var->Max() == 1) {
      possible_true++;
    }
  }
  sum_var_->SetRange(num_always_true_vars, possible_true);
  const int64_t var_min = sum_var_->Min();
  const int64_t var_max = sum_var_->Max();
  if (num_always_true_vars == var_max && possible_true > var_max) {
    PushAllUnboundToZero();
  } else if (possible_true == var_min && num_always_true_vars < var_min) {
    PushAllUnboundToOne();
  } else {
    num_possible_true_vars_.SetValue(solver(), possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true_vars);
  }
}

void SumBooleanEqualToVar::UpdateVar() {
  if (!inactive_.Switched()) {
    if (num_possible_true_vars_.Value() == sum_var_->Min()) {
      PushAllUnboundToOne();
      sum_var_->SetValue(num_possible_true_vars_.Value());
    } else if (num_always_true_vars_.Value() == sum_var_->Max()) {
      PushAllUnboundToZero();
      sum_var_->SetValue(num_always_true_vars_.Value());
    }
  }
}

void SumBooleanEqualToVar::Update(int index) {
  if (!inactive_.Switched()) {
    DCHECK(vars_[index]->Bound());
    const int64_t value = vars_[index]->Min();  // Faster than Value().
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

void SumBooleanEqualToVar::PushAllUnboundToZero() {
  int64_t counter = 0;
  inactive_.Switch(solver());
  for (int i = 0; i < vars_.size(); ++i) {
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

void SumBooleanEqualToVar::PushAllUnboundToOne() {
  int64_t counter = 0;
  inactive_.Switch(solver());
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Max() == 1) {
      vars_[i]->SetValue(1);
      counter++;
    }
  }
  if (counter < sum_var_->Min() || counter > sum_var_->Max()) {
    solver()->Fail();
  }
}

std::string SumBooleanEqualToVar::DebugString() const {
  return absl::StrFormat("%s == %s", DebugStringInternal("SumBoolean"),
                         sum_var_->DebugString());
}

void SumBooleanEqualToVar::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          sum_var_);
  visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
}

// ---------- ScalProd ----------

// ----- Boolean Scal Prod -----

// This constraint implements sum(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.

BooleanScalProdLessConstant::BooleanScalProdLessConstant(
    Solver* s, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& coefs, int64_t upper_bound)
    : Constraint(s),
      vars_(vars),
      coefs_(coefs),
      upper_bound_(upper_bound),
      first_unbound_backward_(vars.size() - 1),
      sum_of_bound_variables_(0LL),
      max_coefficient_(0) {
  CHECK(!vars.empty());
  for (int i = 0; i < vars_.size(); ++i) {
    DCHECK_GE(coefs_[i], 0);
  }
  upper_bound_ =
      CapSub(upper_bound, SortBothChangeConstant(&vars_, &coefs_, false));
  max_coefficient_.SetValue(s, coefs_[vars_.size() - 1]);
}

BooleanScalProdLessConstant::~BooleanScalProdLessConstant() {}

void BooleanScalProdLessConstant::Post() {
  for (int var_index = 0; var_index < vars_.size(); ++var_index) {
    if (vars_[var_index]->Bound()) {
      continue;
    }
    Demon* d = MakeConstraintDemon1(solver(), this,
                                    &BooleanScalProdLessConstant::Update,
                                    "InitialPropagate", var_index);
    vars_[var_index]->WhenRange(d);
  }
}

void BooleanScalProdLessConstant::PushFromTop() {
  const int64_t slack = CapSub(upper_bound_, sum_of_bound_variables_.Value());
  if (slack < 0) {
    solver()->Fail();
  }
  if (slack < max_coefficient_.Value()) {
    int64_t last_unbound = first_unbound_backward_.Value();
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

void BooleanScalProdLessConstant::InitialPropagate() {
  Solver* const s = solver();
  int last_unbound = -1;
  int64_t sum = 0LL;
  for (int index = 0; index < vars_.size(); ++index) {
    if (vars_[index]->Bound()) {
      const int64_t value = vars_[index]->Min();
      sum = CapAdd(sum, CapProd(value, coefs_[index]));
    } else {
      last_unbound = index;
    }
  }
  sum_of_bound_variables_.SetValue(s, sum);
  first_unbound_backward_.SetValue(s, last_unbound);
  PushFromTop();
}

void BooleanScalProdLessConstant::Update(int var_index) {
  if (vars_[var_index]->Min() == 1) {
    sum_of_bound_variables_.SetValue(
        solver(), CapAdd(sum_of_bound_variables_.Value(), coefs_[var_index]));
    PushFromTop();
  }
}

std::string BooleanScalProdLessConstant::DebugString() const {
  return absl::StrFormat("BooleanScalProd([%s], [%s]) <= %d)",
                         JoinDebugStringPtr(vars_), absl::StrJoin(coefs_, ", "),
                         upper_bound_);
}

void BooleanScalProdLessConstant::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kScalProdLessOrEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                     coefs_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, upper_bound_);
  visitor->EndVisitConstraint(ModelVisitor::kScalProdLessOrEqual, this);
}

// ----- PositiveBooleanScalProdEqVar -----

PositiveBooleanScalProdEqVar::PositiveBooleanScalProdEqVar(
    Solver* s, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& coefs, IntVar* var)
    : CastConstraint(s, var),
      vars_(vars),
      coefs_(coefs),
      first_unbound_backward_(vars.size() - 1),
      sum_of_bound_variables_(0LL),
      sum_of_all_variables_(0LL),
      max_coefficient_(0) {
  SortBothChangeConstant(&vars_, &coefs_, true);
  max_coefficient_.SetValue(s, coefs_[vars_.size() - 1]);
}

PositiveBooleanScalProdEqVar::~PositiveBooleanScalProdEqVar() {}

void PositiveBooleanScalProdEqVar::Post() {
  for (int var_index = 0; var_index < vars_.size(); ++var_index) {
    if (vars_[var_index]->Bound()) {
      continue;
    }
    Demon* const d = MakeConstraintDemon1(solver(), this,
                                          &PositiveBooleanScalProdEqVar::Update,
                                          "Update", var_index);
    vars_[var_index]->WhenRange(d);
  }
  if (!target_var_->Bound()) {
    Demon* const uv = MakeConstraintDemon0(
        solver(), this, &PositiveBooleanScalProdEqVar::Propagate, "Propagate");
    target_var_->WhenRange(uv);
  }
}

void PositiveBooleanScalProdEqVar::Propagate() {
  target_var_->SetRange(sum_of_bound_variables_.Value(),
                        sum_of_all_variables_.Value());
  const int64_t slack_up =
      CapSub(target_var_->Max(), sum_of_bound_variables_.Value());
  const int64_t slack_down =
      CapSub(sum_of_all_variables_.Value(), target_var_->Min());
  const int64_t max_coeff = max_coefficient_.Value();
  if (slack_down < max_coeff || slack_up < max_coeff) {
    int64_t last_unbound = first_unbound_backward_.Value();
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

void PositiveBooleanScalProdEqVar::InitialPropagate() {
  Solver* const s = solver();
  int last_unbound = -1;
  int64_t sum_bound = 0;
  int64_t sum_all = 0;
  for (int index = 0; index < vars_.size(); ++index) {
    const int64_t value = CapProd(vars_[index]->Max(), coefs_[index]);
    sum_all = CapAdd(sum_all, value);
    if (vars_[index]->Bound()) {
      sum_bound = CapAdd(sum_bound, value);
    } else {
      last_unbound = index;
    }
  }
  sum_of_bound_variables_.SetValue(s, sum_bound);
  sum_of_all_variables_.SetValue(s, sum_all);
  first_unbound_backward_.SetValue(s, last_unbound);
  Propagate();
}

void PositiveBooleanScalProdEqVar::Update(int var_index) {
  if (vars_[var_index]->Min() == 1) {
    sum_of_bound_variables_.SetValue(
        solver(), CapAdd(sum_of_bound_variables_.Value(), coefs_[var_index]));
  } else {
    sum_of_all_variables_.SetValue(
        solver(), CapSub(sum_of_all_variables_.Value(), coefs_[var_index]));
  }
  Propagate();
}

std::string PositiveBooleanScalProdEqVar::DebugString() const {
  return absl::StrFormat("PositiveBooleanScal([%s], [%s]) == %s",
                         JoinDebugStringPtr(vars_), absl::StrJoin(coefs_, ", "),
                         target_var_->DebugString());
}

void PositiveBooleanScalProdEqVar::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kScalProdEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                     coefs_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kScalProdEqual, this);
}

// ----- PositiveBooleanScalProd -----

PositiveBooleanScalProd::PositiveBooleanScalProd(
    Solver* s, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& coefs)
    : BaseIntExpr(s), vars_(vars), coefs_(coefs) {
  CHECK(!vars.empty());
  SortBothChangeConstant(&vars_, &coefs_, true);
  for (int i = 0; i < vars_.size(); ++i) {
    DCHECK_GE(coefs_[i], 0);
  }
}

PositiveBooleanScalProd::~PositiveBooleanScalProd() {}

int64_t PositiveBooleanScalProd::Min() const {
  int64_t min = 0;
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Min()) {
      min = CapAdd(min, coefs_[i]);
    }
  }
  return min;
}

void PositiveBooleanScalProd::SetMin(int64_t m) {
  SetRange(m, std::numeric_limits<int64_t>::max());
}

int64_t PositiveBooleanScalProd::Max() const {
  int64_t max = 0;
  for (int i = 0; i < vars_.size(); ++i) {
    if (vars_[i]->Max()) {
      max = CapAdd(max, coefs_[i]);
    }
  }
  return max;
}

void PositiveBooleanScalProd::SetMax(int64_t m) {
  SetRange(std::numeric_limits<int64_t>::min(), m);
}

void PositiveBooleanScalProd::SetRange(int64_t l, int64_t u) {
  int64_t current_min = 0;
  int64_t current_max = 0;
  int64_t diameter = -1;
  for (int i = 0; i < vars_.size(); ++i) {
    const int64_t coefficient = coefs_[i];
    const int64_t var_min = CapProd(vars_[i]->Min(), coefficient);
    const int64_t var_max = CapProd(vars_[i]->Max(), coefficient);
    current_min = CapAdd(current_min, var_min);
    current_max = CapAdd(current_max, var_max);
    if (var_min != var_max) {  // Coefficients are increasing.
      diameter = CapSub(var_max, var_min);
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

  if (CapSub(u, l) > diameter) {
    return;
  }

  for (int i = 0; i < vars_.size(); ++i) {
    const int64_t coefficient = coefs_[i];
    IntVar* const var = vars_[i];
    const int64_t new_min =
        CapAdd(CapSub(l, current_max), CapProd(var->Max(), coefficient));
    const int64_t new_max =
        CapAdd(CapSub(u, current_min), CapProd(var->Min(), coefficient));
    if (new_max < 0 || new_min > coefficient || new_min > new_max) {
      solver()->Fail();
    }
    if (new_min > 0LL) {
      var->SetMin(int64_t{1});
    } else if (new_max < coefficient) {
      var->SetMax(int64_t{0});
    }
  }
}

std::string PositiveBooleanScalProd::DebugString() const {
  return absl::StrFormat("PositiveBooleanScalProd([%s], [%s])",
                         JoinDebugStringPtr(vars_),
                         absl::StrJoin(coefs_, ", "));
}

void PositiveBooleanScalProd::WhenRange(Demon* d) {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->WhenRange(d);
  }
}
IntVar* PositiveBooleanScalProd::CastToVar() {
  Solver* const s = solver();
  int64_t vmin = 0LL;
  int64_t vmax = 0LL;
  Range(&vmin, &vmax);
  IntVar* const var = solver()->MakeIntVar(vmin, vmax);
  if (!vars_.empty()) {
    CastConstraint* const ct =
        s->RevAlloc(new PositiveBooleanScalProdEqVar(s, vars_, coefs_, var));
    s->AddCastConstraint(ct, var, this);
  }
  return var;
}

void PositiveBooleanScalProd::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kScalProd, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                     coefs_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kScalProd, this);
}

// ----- PositiveBooleanScalProdEqCst ----- (all constants >= 0)

PositiveBooleanScalProdEqCst::PositiveBooleanScalProdEqCst(
    Solver* s, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& coefs, int64_t constant)
    : Constraint(s),
      vars_(vars),
      coefs_(coefs),
      first_unbound_backward_(vars.size() - 1),
      sum_of_bound_variables_(0LL),
      sum_of_all_variables_(0LL),
      constant_(constant),
      max_coefficient_(0) {
  CHECK(!vars.empty());
  constant_ = CapSub(constant_, SortBothChangeConstant(&vars_, &coefs_, false));
  max_coefficient_.SetValue(s, coefs_[vars_.size() - 1]);
}

PositiveBooleanScalProdEqCst::~PositiveBooleanScalProdEqCst() {}

void PositiveBooleanScalProdEqCst::Post() {
  for (int var_index = 0; var_index < vars_.size(); ++var_index) {
    if (!vars_[var_index]->Bound()) {
      Demon* const d = MakeConstraintDemon1(
          solver(), this, &PositiveBooleanScalProdEqCst::Update, "Update",
          var_index);
      vars_[var_index]->WhenRange(d);
    }
  }
}

void PositiveBooleanScalProdEqCst::Propagate() {
  if (sum_of_bound_variables_.Value() > constant_ ||
      sum_of_all_variables_.Value() < constant_) {
    solver()->Fail();
  }
  const int64_t slack_up = CapSub(constant_, sum_of_bound_variables_.Value());
  const int64_t slack_down = CapSub(sum_of_all_variables_.Value(), constant_);
  const int64_t max_coeff = max_coefficient_.Value();
  if (slack_down < max_coeff || slack_up < max_coeff) {
    int64_t last_unbound = first_unbound_backward_.Value();
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

void PositiveBooleanScalProdEqCst::InitialPropagate() {
  Solver* const s = solver();
  int last_unbound = -1;
  int64_t sum_bound = 0LL;
  int64_t sum_all = 0LL;
  for (int index = 0; index < vars_.size(); ++index) {
    const int64_t value = CapProd(vars_[index]->Max(), coefs_[index]);
    sum_all = CapAdd(sum_all, value);
    if (vars_[index]->Bound()) {
      sum_bound = CapAdd(sum_bound, value);
    } else {
      last_unbound = index;
    }
  }
  sum_of_bound_variables_.SetValue(s, sum_bound);
  sum_of_all_variables_.SetValue(s, sum_all);
  first_unbound_backward_.SetValue(s, last_unbound);
  Propagate();
}

void PositiveBooleanScalProdEqCst::Update(int var_index) {
  if (vars_[var_index]->Min() == 1) {
    sum_of_bound_variables_.SetValue(
        solver(), CapAdd(sum_of_bound_variables_.Value(), coefs_[var_index]));
  } else {
    sum_of_all_variables_.SetValue(
        solver(), CapSub(sum_of_all_variables_.Value(), coefs_[var_index]));
  }
  Propagate();
}

std::string PositiveBooleanScalProdEqCst::DebugString() const {
  return absl::StrFormat("PositiveBooleanScalProd([%s], [%s]) == %d",
                         JoinDebugStringPtr(vars_), absl::StrJoin(coefs_, ", "),
                         constant_);
}

void PositiveBooleanScalProdEqCst::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kScalProdEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                     coefs_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, constant_);
  visitor->EndVisitConstraint(ModelVisitor::kScalProdEqual, this);
}

// ----- Linearizer -----

#define IS_TYPE(type, tag) type == ModelVisitor::tag

ExprLinearizer::ExprLinearizer(
    absl::flat_hash_map<IntVar*, int64_t>* variables_to_coefficients)
    : variables_to_coefficients_(variables_to_coefficients), constant_(0) {}

ExprLinearizer::~ExprLinearizer() {}

// Begin/End visit element.
void ExprLinearizer::BeginVisitModel(const std::string& solver_name) {
  LOG(FATAL) << "Should not be here";
}

void ExprLinearizer::EndVisitModel(const std::string& solver_name) {
  LOG(FATAL) << "Should not be here";
}

void ExprLinearizer::BeginVisitConstraint(const std::string& type_name,
                                          const Constraint* constraint) {
  LOG(FATAL) << "Should not be here";
}

void ExprLinearizer::EndVisitConstraint(const std::string& type_name,
                                        const Constraint* constraint) {
  LOG(FATAL) << "Should not be here";
}

void ExprLinearizer::BeginVisitExtension(const std::string& type) {}

void ExprLinearizer::EndVisitExtension(const std::string& type) {}
void ExprLinearizer::BeginVisitIntegerExpression(const std::string& type_name,
                                                 const IntExpr* expr) {
  BeginVisit(true);
}

void ExprLinearizer::EndVisitIntegerExpression(const std::string& type_name,
                                               const IntExpr* expr) {
  if (IS_TYPE(type_name, kSum)) {
    VisitSum(expr);
  } else if (IS_TYPE(type_name, kScalProd)) {
    VisitScalProd(expr);
  } else if (IS_TYPE(type_name, kDifference)) {
    VisitDifference(expr);
  } else if (IS_TYPE(type_name, kOpposite)) {
    VisitOpposite(expr);
  } else if (IS_TYPE(type_name, kProduct)) {
    VisitProduct(expr);
  } else if (IS_TYPE(type_name, kTrace)) {
    VisitTrace(expr);
  } else {
    VisitIntegerExpression(expr);
  }
  EndVisit();
}

void ExprLinearizer::VisitIntegerVariable(const IntVar* variable,
                                          const std::string& operation,
                                          int64_t value, IntVar* delegate) {
  if (operation == ModelVisitor::kSumOperation) {
    AddConstant(value);
    VisitSubExpression(delegate);
  } else if (operation == ModelVisitor::kDifferenceOperation) {
    AddConstant(value);
    PushMultiplier(-1);
    VisitSubExpression(delegate);
    PopMultiplier();
  } else if (operation == ModelVisitor::kProductOperation) {
    PushMultiplier(value);
    VisitSubExpression(delegate);
    PopMultiplier();
  } else if (operation == ModelVisitor::kTraceOperation) {
    VisitSubExpression(delegate);
  }
}

void ExprLinearizer::VisitIntegerVariable(const IntVar* variable,
                                          IntExpr* delegate) {
  if (delegate != nullptr) {
    VisitSubExpression(delegate);
  } else {
    if (variable->Bound()) {
      AddConstant(variable->Min());
    } else {
      RegisterExpression(variable, 1);
    }
  }
}

// Visit integer arguments.
void ExprLinearizer::VisitIntegerArgument(const std::string& arg_name,
                                          int64_t value) {
  Top()->SetIntegerArgument(arg_name, value);
}

void ExprLinearizer::VisitIntegerArrayArgument(
    const std::string& arg_name, const std::vector<int64_t>& values) {
  Top()->SetIntegerArrayArgument(arg_name, values);
}

void ExprLinearizer::VisitIntegerMatrixArgument(const std::string& arg_name,
                                                const IntTupleSet& values) {
  Top()->SetIntegerMatrixArgument(arg_name, values);
}

// Visit integer expression argument.
void ExprLinearizer::VisitIntegerExpressionArgument(const std::string& arg_name,
                                                    IntExpr* argument) {
  Top()->SetIntegerExpressionArgument(arg_name, argument);
}

void ExprLinearizer::VisitIntegerVariableArrayArgument(
    const std::string& arg_name, const std::vector<IntVar*>& arguments) {
  Top()->SetIntegerVariableArrayArgument(arg_name, arguments);
}

// Visit interval argument.
void ExprLinearizer::VisitIntervalArgument(const std::string& arg_name,
                                           IntervalVar* argument) {}

void ExprLinearizer::VisitIntervalArrayArgument(
    const std::string& arg_name, const std::vector<IntervalVar*>& argument) {}

void ExprLinearizer::Visit(const IntExpr* expr, int64_t multiplier) {
  if (expr->Min() == expr->Max()) {
    constant_ = CapAdd(constant_, CapProd(expr->Min(), multiplier));
  } else {
    PushMultiplier(multiplier);
    expr->Accept(this);
    PopMultiplier();
  }
}

int64_t ExprLinearizer::Constant() const { return constant_; }

std::string ExprLinearizer::DebugString() const { return "ExprLinearizer"; }

void ExprLinearizer::BeginVisit(bool active) { PushArgumentHolder(); }

void ExprLinearizer::EndVisit() { PopArgumentHolder(); }

void ExprLinearizer::VisitSubExpression(const IntExpr* cp_expr) {
  cp_expr->Accept(this);
}

void ExprLinearizer::VisitSum(const IntExpr* cp_expr) {
  if (Top()->HasIntegerVariableArrayArgument(ModelVisitor::kVarsArgument)) {
    const std::vector<IntVar*>& cp_vars =
        Top()->FindIntegerVariableArrayArgumentOrDie(
            ModelVisitor::kVarsArgument);
    for (int i = 0; i < cp_vars.size(); ++i) {
      VisitSubExpression(cp_vars[i]);
    }
  } else if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
    const IntExpr* const left =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kLeftArgument);
    const IntExpr* const right =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kRightArgument);
    VisitSubExpression(left);
    VisitSubExpression(right);
  } else {
    const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
        ModelVisitor::kExpressionArgument);
    const int64_t value =
        Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
    VisitSubExpression(expr);
    AddConstant(value);
  }
}

void ExprLinearizer::VisitScalProd(const IntExpr* cp_expr) {
  const std::vector<IntVar*>& cp_vars =
      Top()->FindIntegerVariableArrayArgumentOrDie(ModelVisitor::kVarsArgument);
  const std::vector<int64_t>& cp_coefficients =
      Top()->FindIntegerArrayArgumentOrDie(ModelVisitor::kCoefficientsArgument);
  CHECK_EQ(cp_vars.size(), cp_coefficients.size());
  for (int i = 0; i < cp_vars.size(); ++i) {
    const int64_t coefficient = cp_coefficients[i];
    PushMultiplier(coefficient);
    VisitSubExpression(cp_vars[i]);
    PopMultiplier();
  }
}

void ExprLinearizer::VisitDifference(const IntExpr* cp_expr) {
  if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
    const IntExpr* const left =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kLeftArgument);
    const IntExpr* const right =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kRightArgument);
    VisitSubExpression(left);
    PushMultiplier(-1);
    VisitSubExpression(right);
    PopMultiplier();
  } else {
    const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
        ModelVisitor::kExpressionArgument);
    const int64_t value =
        Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
    AddConstant(value);
    PushMultiplier(-1);
    VisitSubExpression(expr);
    PopMultiplier();
  }
}

void ExprLinearizer::VisitOpposite(const IntExpr* cp_expr) {
  const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
      ModelVisitor::kExpressionArgument);
  PushMultiplier(-1);
  VisitSubExpression(expr);
  PopMultiplier();
}

void ExprLinearizer::VisitProduct(const IntExpr* cp_expr) {
  if (Top()->HasIntegerExpressionArgument(ModelVisitor::kExpressionArgument)) {
    const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
        ModelVisitor::kExpressionArgument);
    const int64_t value =
        Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
    PushMultiplier(value);
    VisitSubExpression(expr);
    PopMultiplier();
  } else {
    RegisterExpression(cp_expr, 1);
  }
}

void ExprLinearizer::VisitTrace(const IntExpr* cp_expr) {
  const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
      ModelVisitor::kExpressionArgument);
  VisitSubExpression(expr);
}

void ExprLinearizer::VisitIntegerExpression(const IntExpr* cp_expr) {
  RegisterExpression(cp_expr, 1);
}

void ExprLinearizer::RegisterExpression(const IntExpr* expr, int64_t coef) {
  int64_t& value =
      (*variables_to_coefficients_)[const_cast<IntExpr*>(expr)->Var()];
  value = CapAdd(value, CapProd(coef, multipliers_.back()));
}

void ExprLinearizer::AddConstant(int64_t constant) {
  constant_ = CapAdd(constant_, CapProd(constant, multipliers_.back()));
}

void ExprLinearizer::PushMultiplier(int64_t multiplier) {
  if (multipliers_.empty()) {
    multipliers_.push_back(multiplier);
  } else {
    multipliers_.push_back(CapProd(multiplier, multipliers_.back()));
  }
}

void ExprLinearizer::PopMultiplier() { multipliers_.pop_back(); }
#undef IS_TYPE

}  // namespace operations_research
