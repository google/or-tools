// Copyright 2010-2017 Google
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

// This file contains implementations of several scheduling constraints.
// The implemented constraints are:
//
// * Cover constraints: ensure that an interval is the convex hull of
//   a set of interval variables. This includes the performed status
//   (one interval performed implies the cover var performed, all
//   intervals unperformed implies the cover var unperformed, cover
//   var unperformed implies all intervals unperformed, cover var
//   performed implis at least one interval performed).

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"

namespace operations_research {
namespace {
class TreeArrayConstraint : public Constraint {
 public:
  enum PerformedStatus {
    UNPERFORMED,
    PERFORMED,
    UNDECIDED
  };

  TreeArrayConstraint(Solver* const solver,
                      const std::vector<IntervalVar*>& vars,
                      IntervalVar* const target_var)
      : Constraint(solver),
        vars_(vars),
        target_var_(target_var),
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

  std::string DebugStringInternal(const std::string& name) const {
    return StringPrintf("Cover(%s) == %s",
                        JoinDebugStringPtr(vars_, ", ").c_str(),
                        target_var_->DebugString().c_str());
  }

  void AcceptInternal(const std::string& name, ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(name, this);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        vars_);
    visitor->VisitIntervalArgument(ModelVisitor::kTargetArgument, target_var_);
    visitor->EndVisitConstraint(name, this);
  }

  // Reduce the range of a given node (interval, state).
  void ReduceDomain(int depth, int position, int64 new_start_min,
                    int64 new_start_max, int64 new_end_min, int64 new_end_max,
                    PerformedStatus performed) {
    NodeInfo* const info = &tree_[depth][position];
    if (new_start_min > info->start_min.Value()) {
      info->start_min.SetValue(solver(), new_start_min);
    }
    if (new_start_max < info->start_max.Value()) {
      info->start_max.SetValue(solver(), new_start_max);
    }
    if (new_end_min > info->end_min.Value()) {
      info->end_min.SetValue(solver(), new_end_min);
    }
    if (new_end_max < info->end_max.Value()) {
      info->end_max.SetValue(solver(), new_end_max);
    }
    if (performed != UNDECIDED) {
      CHECK(performed == info->performed.Value() ||
            info->performed.Value() == UNDECIDED);
      info->performed.SetValue(solver(), performed);
    }
  }

  void InitLeaf(int position, int64 start_min, int64 start_max, int64 end_min,
                int64 end_max, PerformedStatus performed) {
    InitNode(MaxDepth(), position, start_min, start_max, end_min, end_max,
             performed);
  }

  void InitNode(int depth, int position, int64 start_min, int64 start_max,
                int64 end_min, int64 end_max, PerformedStatus performed) {
    tree_[depth][position].start_min.SetValue(solver(), start_min);
    tree_[depth][position].start_max.SetValue(solver(), start_max);
    tree_[depth][position].end_min.SetValue(solver(), end_min);
    tree_[depth][position].end_max.SetValue(solver(), end_max);
    tree_[depth][position].performed.SetValue(solver(),
                                              static_cast<int>(performed));
  }

  int64 StartMin(int depth, int position) const {
    return tree_[depth][position].start_min.Value();
  }

  int64 StartMax(int depth, int position) const {
    return tree_[depth][position].start_max.Value();
  }

  int64 EndMax(int depth, int position) const {
    return tree_[depth][position].end_max.Value();
  }

  int64 EndMin(int depth, int position) const {
    return tree_[depth][position].end_min.Value();
  }

  PerformedStatus Performed(int depth, int position) const {
    const int p = tree_[depth][position].performed.Value();
    CHECK_GE(p, UNPERFORMED);
    CHECK_LE(p, UNDECIDED);
    return static_cast<PerformedStatus>(p);
  }

  int64 RootStartMin() const { return root_node_->start_min.Value(); }

  int64 RootStartMax() const { return root_node_->start_max.Value(); }

  int64 RootEndMin() const { return root_node_->end_min.Value(); }

  int64 RootEndMax() const { return root_node_->end_max.Value(); }

  PerformedStatus RootPerformed() const { return Performed(0, 0); }

  // This getters query first if the var can be performed, and will
  // return a default value if not.
  int64 VarStartMin(int position) const {
    return vars_[position]->MayBePerformed() ? vars_[position]->StartMin() : 0;
  }

  int64 VarStartMax(int position) const {
    return vars_[position]->MayBePerformed() ? vars_[position]->StartMax() : 0;
  }

  int64 VarEndMin(int position) const {
    return vars_[position]->MayBePerformed() ? vars_[position]->EndMin() : 0;
  }

  int64 VarEndMax(int position) const {
    return vars_[position]->MayBePerformed() ? vars_[position]->EndMax() : 0;
  }

  int64 TargetVarStartMin() const {
    return target_var_->MayBePerformed() ? target_var_->StartMin() : 0;
  }

  int64 TargetVarStartMax() const {
    return target_var_->MayBePerformed() ? target_var_->StartMax() : 0;
  }

  int64 TargetVarEndMin() const {
    return target_var_->MayBePerformed() ? target_var_->EndMin() : 0;
  }

  int64 TargetVarEndMax() const {
    return target_var_->MayBePerformed() ? target_var_->EndMax() : 0;
  }

  // Returns the the performed status of the 'position' nth interval
  // var of the problem.
  PerformedStatus VarPerformed(int position) const {
    IntervalVar* const var = vars_[position];
    if (var->MustBePerformed()) {
      return PERFORMED;
    } else if (var->MayBePerformed()) {
      return UNDECIDED;
    } else {
      return UNPERFORMED;
    }
  }

  // Returns the the performed status of the target var.
  PerformedStatus TargetVarPerformed() const {
    if (target_var_->MustBePerformed()) {
      return PERFORMED;
    } else if (target_var_->MayBePerformed()) {
      return UNDECIDED;
    } else {
      return UNPERFORMED;
    }
  }

  // Returns the position of the parent of a node with a given position.
  int Parent(int position) const { return position / block_size_; }

  // Returns the index of the first child of a node at a given 'position'.
  int ChildStart(int position) const { return position * block_size_; }

  // Returns the index of the last child of a node at a given
  // 'position'.  The depth is needed to make sure that do not overlap
  // the width of the tree at a given depth.
  int ChildEnd(int depth, int position) const {
    DCHECK_LT(depth + 1, tree_.size());
    return std::min((position + 1) * block_size_ - 1, Width(depth + 1) - 1);
  }

  bool IsLeaf(int depth) const { return depth == MaxDepth(); }

  int MaxDepth() const { return tree_.size() - 1; }

  int Width(int depth) const { return tree_[depth].size(); }

 protected:
  const std::vector<IntervalVar*> vars_;
  IntervalVar* const target_var_;

 private:
  struct NodeInfo {
    NodeInfo()
        : start_min(0),
          start_max(0),
          end_min(0),
          end_max(0),
          performed(UNDECIDED) {}

    Rev<int64> start_min;
    Rev<int64> start_max;
    Rev<int64> end_min;
    Rev<int64> end_max;
    Rev<int> performed;
  };

  std::vector<std::vector<NodeInfo> > tree_;
  const int block_size_;
  NodeInfo* root_node_;
};

// This constraint implements cover(vars) == cover_var.
class CoverConstraint : public TreeArrayConstraint {
 public:
  CoverConstraint(Solver* const solver, const std::vector<IntervalVar*>& vars,
                  IntervalVar* const cover_var)
      : TreeArrayConstraint(solver, vars, cover_var), cover_demon_(nullptr) {}

  ~CoverConstraint() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      Demon* const demon = MakeConstraintDemon1(
          solver(), this, &CoverConstraint::LeafChanged, "LeafChanged", i);
      vars_[i]->WhenStartRange(demon);
      vars_[i]->WhenEndRange(demon);
      vars_[i]->WhenPerformedBound(demon);
    }
    cover_demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
        solver(), this, &CoverConstraint::CoverVarChanged, "CoverVarChanged"));
    target_var_->WhenStartRange(cover_demon_);
    target_var_->WhenEndRange(cover_demon_);
    target_var_->WhenPerformedBound(cover_demon_);
  }

  void InitialPropagate() override {
    // Copy vars to leaf nodes.
    for (int i = 0; i < vars_.size(); ++i) {
      InitLeaf(i, VarStartMin(i), VarStartMax(i), VarEndMin(i), VarEndMax(i),
               VarPerformed(i));
    }

    // Compute up.
    for (int i = MaxDepth() - 1; i >= 0; --i) {
      for (int j = 0; j < Width(i); ++j) {
        int64 bucket_start_min = kint64max;
        int64 bucket_start_max = kint64max;
        int64 bucket_end_min = kint64min;
        int64 bucket_end_max = kint64min;
        bool one_undecided = false;
        const PerformedStatus up_performed = ComputePropagationUp(
            i, j, &bucket_start_min, &bucket_start_max, &bucket_end_min,
            &bucket_end_max, &one_undecided);
        InitNode(i, j, bucket_start_min, bucket_start_max, bucket_end_min,
                 bucket_end_max, up_performed);
      }
    }
    // Compute down.
    PropagateRoot();
  }

  void PropagateRoot() {
    // Propagate from the root of the tree to the target var.
    switch (RootPerformed()) {
      case UNPERFORMED:
        target_var_->SetPerformed(false);
        break;
      case PERFORMED:
        target_var_->SetPerformed(true);
        FALLTHROUGH_INTENDED;
      case UNDECIDED:
        target_var_->SetStartRange(RootStartMin(), RootStartMax());
        target_var_->SetEndRange(RootEndMin(), RootEndMax());
        break;
    }
    // Check if we need to propagate back. This is useful in case the
    // target var is performed and only one last interval var may be
    // performed, and thus needs to change is status to performed.
    CoverVarChanged();
  }

  // Propagates from top to bottom.
  void CoverVarChanged() {
    PushDown(0, 0, TargetVarStartMin(), TargetVarStartMax(), TargetVarEndMin(),
             TargetVarEndMax(), TargetVarPerformed());
  }

  void PushDown(int depth, int position, int64 new_start_min,
                int64 new_start_max, int64 new_end_min, int64 new_end_max,
                PerformedStatus performed) {
    // TODO(user): Propagate start_max and end_min going down.
    // Nothing to do?
    if (new_start_min <= StartMin(depth, position) &&
        new_start_max >= StartMax(depth, position) &&
        new_end_min <= EndMin(depth, position) &&
        new_end_max >= EndMax(depth, position) &&
        (performed == UNDECIDED || performed == Performed(depth, position))) {
      return;
    }
    // Leaf node -> push to leaf var.
    if (IsLeaf(depth)) {
      switch (performed) {
        case UNPERFORMED:
          vars_[position]->SetPerformed(false);
          break;
        case PERFORMED:
          vars_[position]->SetPerformed(true);
          FALLTHROUGH_INTENDED;
        case UNDECIDED:
          vars_[position]->SetStartRange(new_start_min, new_start_max);
          vars_[position]->SetEndRange(new_end_min, new_end_max);
      }
      return;
    }

    const int block_start = ChildStart(position);
    const int block_end = ChildEnd(depth, position);

    switch (performed) {
      case UNPERFORMED: {  // Mark all node unperformed.
        for (int i = block_start; i <= block_end; ++i) {
          PushDown(depth + 1, i, new_start_min, new_start_max, new_end_min,
                   new_end_max, UNPERFORMED);
        }
        break;
      }
      case PERFORMED: {  // Count number of undecided or performed;
        int candidate = -1;
        int may_be_performed_count = 0;
        int must_be_performed_count = 0;
        for (int i = block_start; i <= block_end; ++i) {
          switch (Performed(depth + 1, i)) {
            case UNPERFORMED:
              break;
            case PERFORMED:
              must_be_performed_count++;
              FALLTHROUGH_INTENDED;
            case UNDECIDED:
              may_be_performed_count++;
              candidate = i;
          }
        }
        if (may_be_performed_count == 0) {
          solver()->Fail();
        } else if (may_be_performed_count == 1) {
          PushDown(depth + 1, candidate, new_start_min, new_start_max,
                   new_end_min, new_end_max, PERFORMED);
        } else {
          for (int i = block_start; i <= block_end; ++i) {
            // Since there are more than 1 active child node, we
            // cannot propagate on new_start_max and new_end_min. Thus
            // we substitute them with safe bounds e.g. new_end_max
            // and new_start_min.
            PushDown(depth + 1, i, new_start_min, new_end_max, new_start_min,
                     new_end_max, UNDECIDED);
          }
        }
        break;
      }
      case UNDECIDED: {
        for (int i = block_start; i <= block_end; ++i) {
          // Since there are more than 1 active child node, we
          // cannot propagate on new_start_max and new_end_min. Thus
          // we substitute them with safe bounds e.g. new_end_max
          // and new_start_min.
          PushDown(depth + 1, i, new_start_min, new_end_max, new_start_min,
                   new_end_max, UNDECIDED);
        }
      }
    }
  }

  void LeafChanged(int term_index) {
    ReduceDomain(MaxDepth(), term_index, VarStartMin(term_index),
                 VarStartMax(term_index), VarEndMin(term_index),
                 VarEndMax(term_index), VarPerformed(term_index));
    // Do we need to propagate up?
    const int parent = Parent(term_index);
    const int parent_depth = MaxDepth() - 1;
    const int64 parent_start_min = StartMin(parent_depth, parent);
    const int64 parent_start_max = StartMax(parent_depth, parent);
    const int64 parent_end_min = EndMin(parent_depth, parent);
    const int64 parent_end_max = EndMax(parent_depth, parent);
    IntervalVar* const var = vars_[term_index];
    const bool performed_bound = var->IsPerformedBound();
    const bool was_performed_bound = var->WasPerformedBound();
    if (performed_bound == was_performed_bound && var->MayBePerformed() &&
        var->OldStartMin() != parent_start_min &&
        var->OldStartMax() != parent_start_max &&
        var->OldEndMin() != parent_end_min &&
        var->OldEndMax() != parent_end_max) {
      // We were not a support of the parent bounds, and the performed
      // status has not changed. There is no need to propagate up.
      return;
    }
    PushUp(term_index);
  }

  void PushUp(int position) {
    int depth = MaxDepth();
    while (depth > 0) {
      const int parent = Parent(position);
      const int parent_depth = depth - 1;
      int64 bucket_start_min = kint64max;
      int64 bucket_start_max = kint64max;
      int64 bucket_end_min = kint64min;
      int64 bucket_end_max = kint64min;
      bool one_undecided = false;
      const PerformedStatus status_up = ComputePropagationUp(
          parent_depth, parent, &bucket_start_min, &bucket_start_max,
          &bucket_end_min, &bucket_end_max, &one_undecided);
      if (bucket_start_min > StartMin(parent_depth, parent) ||
          bucket_start_max < StartMax(parent_depth, parent_depth) ||
          bucket_end_min > EndMin(parent_depth, parent) ||
          bucket_end_max < EndMax(parent_depth, parent) ||
          status_up != Performed(parent_depth, parent)) {
        ReduceDomain(parent_depth, parent, bucket_start_min, bucket_start_max,
                     bucket_end_min, bucket_end_max, status_up);
      } else {
        if (one_undecided && TargetVarPerformed() == PERFORMED) {
          // This may be the last possible interval that can and
          // should be performed.
          PropagateRoot();
        }
        // There is nothing more to propagate up. We can stop now.
        return;
      }
      depth = parent_depth;
      position = parent;
    }
    DCHECK_EQ(0, depth);
    PropagateRoot();
  }

  std::string DebugString() const override {
    return DebugStringInternal(ModelVisitor::kCover);
  }

  void Accept(ModelVisitor* const visitor) const override {
    AcceptInternal(ModelVisitor::kCover, visitor);
  }

 private:
  PerformedStatus ComputePropagationUp(int parent_depth, int parent_position,
                                       int64* const bucket_start_min,
                                       int64* const bucket_start_max,
                                       int64* const bucket_end_min,
                                       int64* const bucket_end_max,
                                       bool* one_undecided) {
    *bucket_start_min = kint64max;
    *bucket_start_max = kint64max;
    *bucket_end_min = kint64min;
    *bucket_end_max = kint64min;

    int may_be_performed_count = 0;
    int must_be_performed_count = 0;
    const int block_start = ChildStart(parent_position);
    const int block_end = ChildEnd(parent_depth, parent_position);
    for (int k = block_start; k <= block_end; ++k) {
      const PerformedStatus performed = Performed(parent_depth + 1, k);
      if (performed != UNPERFORMED) {
        *bucket_start_min =
            std::min(*bucket_start_min, StartMin(parent_depth + 1, k));
        *bucket_end_max =
            std::max(*bucket_end_max, EndMax(parent_depth + 1, k));
        may_be_performed_count++;
        if (performed == PERFORMED) {
          *bucket_start_max =
              std::min(*bucket_start_max, StartMax(parent_depth + 1, k));
          *bucket_end_min =
              std::max(*bucket_end_min, EndMin(parent_depth + 1, k));
          must_be_performed_count++;
        }
      }
    }
    const PerformedStatus up_performed =
        must_be_performed_count > 0
            ? PERFORMED
            : (may_be_performed_count > 0 ? UNDECIDED : UNPERFORMED);
    *one_undecided =
        (may_be_performed_count == 1) && (must_be_performed_count == 0);
    return up_performed;
  }

  Demon* cover_demon_;
};

class IntervalEquality : public Constraint {
 public:
  IntervalEquality(Solver* const solver, IntervalVar* const var1,
                   IntervalVar* const var2)
      : Constraint(solver), var1_(var1), var2_(var2) {}

  ~IntervalEquality() override {}

  void Post() override {
    Demon* const demon = solver()->MakeConstraintInitialPropagateCallback(this);
    var1_->WhenAnything(demon);
    var2_->WhenAnything(demon);
  }

  void InitialPropagate() override {
    // Naive code. Can be split by property (performed, start...).
    if (!var1_->MayBePerformed()) {
      var2_->SetPerformed(false);
    } else {
      if (var1_->MustBePerformed()) {
        var2_->SetPerformed(true);
      }
      var2_->SetStartRange(var1_->StartMin(), var1_->StartMax());
      var2_->SetDurationRange(var1_->DurationMin(), var1_->DurationMax());
      var2_->SetEndRange(var1_->EndMin(), var1_->EndMax());
    }
    if (!var2_->MayBePerformed()) {
      var1_->SetPerformed(false);
    } else {
      if (var2_->MustBePerformed()) {
        var1_->SetPerformed(true);
      }
      var1_->SetStartRange(var2_->StartMin(), var2_->StartMax());
      var1_->SetDurationRange(var2_->DurationMin(), var2_->DurationMax());
      var1_->SetEndRange(var2_->EndMin(), var2_->EndMax());
    }
  }

  std::string DebugString() const override {
    return StringPrintf("Equality(%s, %s)", var1_->DebugString().c_str(),
                        var2_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
    visitor->VisitIntervalArgument(ModelVisitor::kLeftArgument, var1_);
    visitor->VisitIntervalArgument(ModelVisitor::kRightArgument, var2_);
    visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
  }

 private:
  IntervalVar* const var1_;
  IntervalVar* const var2_;
};
}  // namespace

Constraint* Solver::MakeCover(const std::vector<IntervalVar*>& vars,
                              IntervalVar* const target_var) {
  CHECK(!vars.empty());
  if (vars.size() == 1) {
    return MakeEquality(vars[0], target_var);
  } else {
    return RevAlloc(new CoverConstraint(this, vars, target_var));
  }
}

Constraint* Solver::MakeEquality(IntervalVar* const var1,
                                 IntervalVar* const var2) {
  return RevAlloc(new IntervalEquality(this, var1, var2));
}
}  // namespace operations_research
