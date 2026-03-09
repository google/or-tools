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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/reversible_data.h"

namespace operations_research {

class TrueConstraint : public Constraint {
 public:
  explicit TrueConstraint(Solver* s) : Constraint(s) {}
  ~TrueConstraint() override = default;

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;
  IntVar* Var() override;
};

class FalseConstraint : public Constraint {
 public:
  explicit FalseConstraint(Solver* s) : Constraint(s) {}
  FalseConstraint(Solver* s, const std::string& explanation)
      : Constraint(s), explanation_(explanation) {}
  ~FalseConstraint() override = default;

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;
  IntVar* Var() override;

 private:
  const std::string explanation_;
};

class MapDomain : public Constraint {
 public:
  MapDomain(Solver* s, IntVar* var, const std::vector<IntVar*>& actives)
      : Constraint(s), var_(var), actives_(actives) {
    holes_ = var->MakeHoleIterator(true);
  }

  ~MapDomain() override = default;

  void Post() override;

  void InitialPropagate() override;

  void UpdateActive(int64_t index);

  void VarDomain();

  void VarBound();
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* var_;
  std::vector<IntVar*> actives_;
  IntVarIterator* holes_;
};

class LexicalLessOrEqual : public Constraint {
 public:
  LexicalLessOrEqual(Solver* s, std::vector<IntVar*> left,
                     std::vector<IntVar*> right, std::vector<int64_t> offsets)
      : Constraint(s),
        left_(std::move(left)),
        right_(std::move(right)),
        active_var_(0),
        offsets_(std::move(offsets)),
        demon_added_(offsets_.size(), false),
        demon_(nullptr) {
    CHECK_EQ(left_.size(), right_.size());
    CHECK_EQ(offsets_.size(), right_.size());
    CHECK(std::all_of(offsets_.begin(), offsets_.end(),
                      [](int step) { return step > 0; }));
  }

  ~LexicalLessOrEqual() override = default;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  int JumpEqualVariables(int start_position) const;
  void AddDemon(int position);

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  NumericalRev<int> active_var_;
  std::vector<int64_t> offsets_;
  RevArray<bool> demon_added_;
  Demon* demon_;
};

class InversePermutationConstraint : public Constraint {
 public:
  InversePermutationConstraint(Solver* s, const std::vector<IntVar*>& left,
                               const std::vector<IntVar*>& right)
      : Constraint(s),
        left_(left),
        right_(right),
        left_hole_iterators_(left.size()),
        left_domain_iterators_(left_.size()),
        right_hole_iterators_(right_.size()),
        right_domain_iterators_(right_.size()) {
    CHECK_EQ(left_.size(), right_.size());
    for (int i = 0; i < left_.size(); ++i) {
      left_hole_iterators_[i] = left_[i]->MakeHoleIterator(true);
      left_domain_iterators_[i] = left_[i]->MakeDomainIterator(true);
      right_hole_iterators_[i] = right_[i]->MakeHoleIterator(true);
      right_domain_iterators_[i] = right_[i]->MakeDomainIterator(true);
    }
  }

  ~InversePermutationConstraint() override = default;

  void Post() override;

  void InitialPropagate() override;

  void PropagateHolesOfLeftVarToRight(int index);

  void PropagateHolesOfRightVarToLeft(int index);

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  // See PropagateHolesOfLeftVarToRight() and PropagateHolesOfRightVarToLeft().
  void PropagateHoles(int index, IntVar* var, IntVarIterator* holes,
                      const std::vector<IntVar*>& inverse);

  void PropagateDomain(int index, IntVar* var, IntVarIterator* domain,
                       const std::vector<IntVar*>& inverse);

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  std::vector<IntVarIterator*> left_hole_iterators_;
  std::vector<IntVarIterator*> left_domain_iterators_;
  std::vector<IntVarIterator*> right_hole_iterators_;
  std::vector<IntVarIterator*> right_domain_iterators_;

  // used only in PropagateDomain().
  std::vector<int64_t> tmp_removed_values_;
};

class IndexOfFirstMaxValue : public Constraint {
 public:
  IndexOfFirstMaxValue(Solver* solver, IntVar* index,
                       const std::vector<IntVar*>& vars)
      : Constraint(solver), index_(index), vars_(vars) {}

  ~IndexOfFirstMaxValue() override = default;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* index_;
  const std::vector<IntVar*> vars_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_
