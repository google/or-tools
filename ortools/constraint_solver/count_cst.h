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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_COUNT_CST_H_
#define ORTOOLS_CONSTRAINT_SOLVER_COUNT_CST_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/utilities.h"

namespace operations_research {

class AtMost : public Constraint {
 public:
  AtMost(Solver* s, std::vector<IntVar*> vars, int64_t value, int64_t max_count)
      : Constraint(s),
        vars_(std::move(vars)),
        value_(value),
        max_count_(max_count),
        current_count_(0) {}

  ~AtMost() override {}

  void Post() override;

  void InitialPropagate() override;

  void OneBound(IntVar* var);

  void CheckCount();

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
  const int64_t value_;
  const int64_t max_count_;
  NumericalRev<int> current_count_;
};

class Distribute : public Constraint {
 public:
  Distribute(Solver* s, const std::vector<IntVar*>& vars,
             const std::vector<int64_t>& values,
             const std::vector<IntVar*>& cards)
      : Constraint(s),
        vars_(vars),
        values_(values),
        cards_(cards),
        undecided_(vars.size(), cards.size()),
        min_(cards.size(), 0),
        max_(cards.size(), 0) {}

  ~Distribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int index);
  void OneDomain(int index);
  void CountVar(int cindex);
  void CardMin(int cindex);
  void CardMax(int cindex);
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t var_size() const { return vars_.size(); }
  int64_t card_size() const { return cards_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64_t> values_;
  const std::vector<IntVar*> cards_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
};

class FastDistribute : public Constraint {
 public:
  FastDistribute(Solver* s, const std::vector<IntVar*>& vars,
                 const std::vector<IntVar*>& cards);
  ~FastDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int index);
  void OneDomain(int index);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64_t var_index, int64_t card_index);
  void SetRevDoContribute(int64_t var_index, int64_t card_index);

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t var_size() const { return vars_.size(); }
  int64_t card_size() const { return cards_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<IntVar*> cards_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

class BoundedDistribute : public Constraint {
 public:
  BoundedDistribute(Solver* s, const std::vector<IntVar*>& vars,
                    const std::vector<int64_t>& values,
                    const std::vector<int64_t>& card_min,
                    const std::vector<int64_t>& card_max);
  ~BoundedDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int index);
  void OneDomain(int index);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64_t var_index, int64_t card_index);
  void SetRevDoContribute(int64_t var_index, int64_t card_index);

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t var_size() const { return vars_.size(); }
  int64_t card_size() const { return values_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64_t> values_;
  const std::vector<int64_t> card_min_;
  const std::vector<int64_t> card_max_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

class BoundedFastDistribute : public Constraint {
 public:
  BoundedFastDistribute(Solver* s, const std::vector<IntVar*>& vars,
                        const std::vector<int64_t>& card_min,
                        const std::vector<int64_t>& card_max);
  ~BoundedFastDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int index);
  void OneDomain(int index);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64_t var_index, int64_t card_index);
  void SetRevDoContribute(int64_t var_index, int64_t card_index);

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t var_size() const { return vars_.size(); }
  int64_t card_size() const { return card_min_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64_t> card_min_;
  const std::vector<int64_t> card_max_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

class SetAllToZero : public Constraint {
 public:
  SetAllToZero(Solver* s, const std::vector<IntVar*>& vars)
      : Constraint(s), vars_(vars) {}

  ~SetAllToZero() override {}

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_COUNT_CST_H_
