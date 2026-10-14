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

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
class HoleIteratorTest : public testing::Test {
 protected:
  HoleIteratorTest() { solver_ = std::make_unique<Solver>("HoleIteratorTest"); }
  std::unique_ptr<Solver> solver_;
};

class HoleDemon : public Demon {
 public:
  explicit HoleDemon(const IntVar* const var)
      : iterator_(var->MakeHoleIterator(false)),
        min_(var->Min()),
        size_(var->Max() - var->Min() + 1),
        counter_(new int[size_]) {
    for (int i = 0; i < size_; ++i) {
      counter_[i] = 0;
    }
  }

  ~HoleDemon() override { delete iterator_; }

  void Run(Solver* const s) override {
    for (const int64_t value : InitAndGetValues(iterator_)) {
      counter_[value - min_]++;
    }
  }

  int counter(int value) { return counter_[value - min_]; }

 private:
  IntVarIterator* const iterator_;
  int min_;
  int size_;
  std::unique_ptr<int[]> counter_;
};

TEST_IN_PROPAGATION0(HoleIteratorTest, HoleDemon, solver, true) {
  IntVar* x = solver->MakeIntVar(0, 10, "x");
  IntVar* y1 = solver->MakeSum(x, 1)->Var();
  IntVar* y2 = solver->MakeDifference(1, x)->Var();
  IntVar* y3 = solver->MakeOpposite(x)->Var();
  IntVar* y4 = solver->MakeProd(x, 3)->Var();
  HoleDemon d0(x);
  x->WhenDomain(&d0);
  HoleDemon d1(y1);
  y1->WhenDomain(&d1);
  HoleDemon d2(y2);
  y2->WhenDomain(&d2);
  HoleDemon d3(y3);
  y3->WhenDomain(&d3);
  HoleDemon d4(y4);
  y4->WhenDomain(&d4);
  x->RemoveValue(2);
  EXPECT_EQ(1, d0.counter(2));
  EXPECT_EQ(0, d0.counter(1));
  EXPECT_EQ(0, d0.counter(3));
  EXPECT_EQ(1, d1.counter(3));
  EXPECT_EQ(0, d1.counter(2));
  EXPECT_EQ(0, d1.counter(4));
  EXPECT_EQ(1, d2.counter(-1));
  EXPECT_EQ(0, d2.counter(0));
  EXPECT_EQ(0, d2.counter(-2));
  EXPECT_EQ(1, d3.counter(-2));
  EXPECT_EQ(0, d3.counter(-1));
  EXPECT_EQ(0, d3.counter(-3));
  EXPECT_EQ(1, d4.counter(6));
  EXPECT_EQ(0, d4.counter(3));
  EXPECT_EQ(0, d4.counter(9));
  x->SetMin(4);
  EXPECT_EQ(0, d0.counter(0));
  EXPECT_EQ(0, d0.counter(1));
  EXPECT_EQ(1, d0.counter(2));
  EXPECT_EQ(0, d0.counter(3));
  EXPECT_EQ(0, d0.counter(4));
  EXPECT_EQ(0, d1.counter(1));
  EXPECT_EQ(0, d1.counter(2));
  EXPECT_EQ(1, d1.counter(3));
  EXPECT_EQ(0, d1.counter(4));
  EXPECT_EQ(0, d1.counter(5));
  EXPECT_EQ(0, d2.counter(1));
  EXPECT_EQ(0, d2.counter(0));
  EXPECT_EQ(1, d2.counter(-1));
  EXPECT_EQ(0, d2.counter(-2));
  EXPECT_EQ(0, d2.counter(-3));
  EXPECT_EQ(0, d3.counter(0));
  EXPECT_EQ(0, d3.counter(-1));
  EXPECT_EQ(1, d3.counter(-2));
  EXPECT_EQ(0, d3.counter(-3));
  EXPECT_EQ(0, d3.counter(-4));
  EXPECT_EQ(0, d4.counter(0));
  EXPECT_EQ(0, d4.counter(3));
  EXPECT_EQ(1, d4.counter(6));
  EXPECT_EQ(0, d4.counter(9));
  EXPECT_EQ(0, d4.counter(12));
  x->RemoveValue(2);
  EXPECT_EQ(1, d0.counter(2));
  EXPECT_EQ(1, d1.counter(3));
  EXPECT_EQ(1, d2.counter(-1));
  EXPECT_EQ(1, d3.counter(-2));
  EXPECT_EQ(1, d4.counter(6));
  x->SetMax(9);
  EXPECT_EQ(0, d0.counter(9));
  EXPECT_EQ(0, d0.counter(10));
  EXPECT_EQ(0, d1.counter(10));
  EXPECT_EQ(0, d1.counter(11));
  EXPECT_EQ(0, d2.counter(-8));
  EXPECT_EQ(0, d2.counter(-9));
  EXPECT_EQ(0, d3.counter(-9));
  EXPECT_EQ(0, d3.counter(-10));
  EXPECT_EQ(0, d4.counter(27));
  EXPECT_EQ(0, d4.counter(30));
  x->RemoveInterval(5, 6);
  EXPECT_EQ(0, d0.counter(4));
  EXPECT_EQ(1, d0.counter(5));
  EXPECT_EQ(1, d0.counter(6));
  EXPECT_EQ(0, d0.counter(7));
  EXPECT_EQ(0, d1.counter(5));
  EXPECT_EQ(1, d1.counter(6));
  EXPECT_EQ(1, d1.counter(7));
  EXPECT_EQ(0, d1.counter(8));
  EXPECT_EQ(0, d2.counter(-3));
  EXPECT_EQ(1, d2.counter(-4));
  EXPECT_EQ(1, d2.counter(-5));
  EXPECT_EQ(0, d2.counter(-6));
  EXPECT_EQ(0, d3.counter(-4));
  EXPECT_EQ(1, d3.counter(-5));
  EXPECT_EQ(1, d3.counter(-6));
  EXPECT_EQ(0, d3.counter(-7));
  EXPECT_EQ(0, d4.counter(12));
  EXPECT_EQ(1, d4.counter(15));
  EXPECT_EQ(1, d4.counter(18));
  EXPECT_EQ(0, d4.counter(21));
  return nullptr;
};

class DelayedRemovalDemon : public Demon {
 public:
  explicit DelayedRemovalDemon(IntVar* const var) : var_(var) {}
  ~DelayedRemovalDemon() override {}

  void Run(Solver* const s) override {
    if (!var_->Contains(2)) {
      var_->RemoveValue(4);
    }
  }

 private:
  IntVar* const var_;
};

TEST_IN_PROPAGATION0(HoleIteratorTest, DelayedHoleDemon, solver, true) {
  IntVar* const x = solver->MakeIntVar(0, 10, "x");
  HoleDemon d0(x);
  x->WhenDomain(&d0);
  DelayedRemovalDemon d1(x);
  x->WhenDomain(&d1);
  x->RemoveValue(2);
  EXPECT_EQ(0, d0.counter(0));
  EXPECT_EQ(0, d0.counter(1));
  EXPECT_EQ(1, d0.counter(2));
  EXPECT_EQ(0, d0.counter(3));
  EXPECT_EQ(1, d0.counter(4));
  EXPECT_EQ(0, d0.counter(5));
  return nullptr;
};

class CleanHoleDemon : public Demon {
 public:
  explicit CleanHoleDemon(IntVar* const var1, IntVar* const var2)
      : var1_(var1), var2_(var2) {}
  ~CleanHoleDemon() override {}

  void Run(Solver* const s) override {
    if (var1_->Bound()) {
      const int64_t val = var1_->Value();
      var2_->RemoveValue(val + 2);
      if (val == 0) {
        s->Fail();
      }
    }
  }

 private:
  IntVar* const var1_;
  IntVar* const var2_;
};

TEST_F(HoleIteratorTest, CleanHoles) {
  IntVar* const x = solver_->MakeIntVar(0, 10, "x");
  IntVar* const y = solver_->MakeIntVar(0, 10, "y");
  HoleDemon d0(y);
  y->WhenDomain(&d0);
  CleanHoleDemon d1(x, y);
  x->WhenDomain(&d1);
  DecisionBuilder* const db = solver_->MakePhase(
      x, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(1, x->Value());
  EXPECT_TRUE(y->Contains(0));
  EXPECT_TRUE(y->Contains(1));
  EXPECT_TRUE(y->Contains(2));
  EXPECT_FALSE(y->Contains(3));
  EXPECT_EQ(0, d0.counter(0));
  EXPECT_EQ(0, d0.counter(1));
  EXPECT_EQ(0, d0.counter(2));
  EXPECT_EQ(1, d0.counter(3));
  EXPECT_EQ(0, d0.counter(4));
  solver_->EndSearch();
}

class OldBoundsTest : public testing::Test {
 protected:
  OldBoundsTest() { solver_ = std::make_unique<Solver>("OldBoundsTest"); }
  std::unique_ptr<Solver> solver_;
};

class OldBoundsDemon : public Demon {
 public:
  explicit OldBoundsDemon(IntVar* const var) : var_(var) {}
  ~OldBoundsDemon() override {}

  void Run(Solver* const s) override {
    if (var_->Min() == 2) {
      var_->SetRange(4, 6);
    } else {
      EXPECT_EQ(2, var_->OldMin());
      EXPECT_EQ(8, var_->OldMax());
    }
  }

 private:
  IntVar* const var_;
};

TEST_IN_PROPAGATION0(OldBoundsTest, OldBounds, solver, true) {
  IntVar* x = solver->MakeIntVar(0, 10, "x");
  OldBoundsDemon d(x);
  x->WhenRange(&d);
  x->SetRange(2, 8);
  EXPECT_EQ(4, x->Min());
  EXPECT_EQ(6, x->Max());
  return nullptr;
};

class DelayedDemonTest : public testing::Test {
 protected:
  DelayedDemonTest() { solver_ = std::make_unique<Solver>("DelayedDemonTest"); }
  std::unique_ptr<Solver> solver_;
};

class DelayedDemon : public Demon {
 public:
  DelayedDemon(const IntVar* const* vars, int size)
      : vars_(vars), size_(size), count_(0) {}

  ~DelayedDemon() override {}

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  void Run(Solver* const s) override {
    count_++;
    EXPECT_EQ(1, count_);
    for (int i = 0; i < size_; ++i) {
      ASSERT_TRUE(vars_[i]->Bound());
      EXPECT_EQ(0, vars_[i]->Value());
    }
  }

 private:
  const IntVar* const* vars_;
  int size_;
  int count_;
};

TEST_IN_PROPAGATION0(DelayedDemonTest, DelayedDemon, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(10, 0, 4, "x", &vars);
  IntExpr* sum = solver->MakeMax(vars);
  Demon* d = solver->RevAlloc(new DelayedDemon(vars.data(), vars.size()));
  for (int i = 0; i < 10; ++i) {
    vars[i]->WhenDomain(d);
  }
  solver->AddConstraint(solver->MakeEquality(sum, Zero()));
  return nullptr;
};

// ----- Domain Iterator -----

class DomainIteratorTest : public testing::Test {
 protected:
  DomainIteratorTest() {
    solver_ = std::make_unique<Solver>("DomainIteratorTest");
  }
  std::unique_ptr<Solver> solver_;
};

void CollectAll(IntVarIterator* iterator, std::set<int64_t>* collector) {
  collector->clear();
  for (const int64_t value : InitAndGetValues(iterator)) {
    collector->insert(value);
  }
}

TEST_F(DomainIteratorTest, DomainIterator) {
  std::set<int64_t> collector;
  IntVar* x = solver_->MakeIntVar(0, 10, "x");
  IntVar* px = solver_->MakeSum(x, 3)->Var();
  IntVar* dx = solver_->MakeDifference(3, x)->Var();
  IntVar* ox = solver_->MakeOpposite(x)->Var();
  IntVar* tx = solver_->MakeProd(x, 3)->Var();
  std::unique_ptr<IntVarIterator> it(x->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);

  EXPECT_EQ(11, collector.size());
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_TRUE(collector.contains(0));
  EXPECT_TRUE(collector.contains(1));
  EXPECT_TRUE(collector.contains(2));
  EXPECT_TRUE(collector.contains(3));
  EXPECT_TRUE(collector.contains(4));
  EXPECT_TRUE(collector.contains(5));
  EXPECT_TRUE(collector.contains(6));
  EXPECT_TRUE(collector.contains(7));
  EXPECT_TRUE(collector.contains(8));
  EXPECT_TRUE(collector.contains(9));
  EXPECT_TRUE(collector.contains(10));
  EXPECT_FALSE(collector.contains(11));

  x->RemoveInterval(3, 6);
  CollectAll(it.get(), &collector);
  EXPECT_EQ(7, collector.size());
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_TRUE(collector.contains(0));
  EXPECT_TRUE(collector.contains(1));
  EXPECT_TRUE(collector.contains(2));
  EXPECT_FALSE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));
  EXPECT_FALSE(collector.contains(5));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_TRUE(collector.contains(7));
  EXPECT_TRUE(collector.contains(8));
  EXPECT_TRUE(collector.contains(9));
  EXPECT_TRUE(collector.contains(10));
  EXPECT_FALSE(collector.contains(11));

  x->SetMin(4);
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_FALSE(collector.contains(1));
  EXPECT_FALSE(collector.contains(2));
  EXPECT_FALSE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));
  EXPECT_FALSE(collector.contains(5));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_TRUE(collector.contains(7));
  EXPECT_TRUE(collector.contains(8));
  EXPECT_TRUE(collector.contains(9));
  EXPECT_TRUE(collector.contains(10));
  EXPECT_FALSE(collector.contains(11));

  it.reset(px->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(2));
  EXPECT_FALSE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));
  EXPECT_FALSE(collector.contains(5));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_FALSE(collector.contains(7));
  EXPECT_FALSE(collector.contains(8));
  EXPECT_FALSE(collector.contains(9));
  EXPECT_TRUE(collector.contains(10));
  EXPECT_TRUE(collector.contains(11));
  EXPECT_TRUE(collector.contains(12));
  EXPECT_TRUE(collector.contains(13));
  EXPECT_FALSE(collector.contains(14));

  it.reset(dx->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(-8));
  EXPECT_TRUE(collector.contains(-7));
  EXPECT_TRUE(collector.contains(-6));
  EXPECT_TRUE(collector.contains(-5));
  EXPECT_TRUE(collector.contains(-4));
  EXPECT_FALSE(collector.contains(-3));
  EXPECT_FALSE(collector.contains(-2));
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_FALSE(collector.contains(1));
  EXPECT_FALSE(collector.contains(2));
  EXPECT_FALSE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));

  it.reset(ox->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(1));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_FALSE(collector.contains(-2));
  EXPECT_FALSE(collector.contains(-3));
  EXPECT_FALSE(collector.contains(-4));
  EXPECT_FALSE(collector.contains(-5));
  EXPECT_FALSE(collector.contains(-6));
  EXPECT_TRUE(collector.contains(-7));
  EXPECT_TRUE(collector.contains(-8));
  EXPECT_TRUE(collector.contains(-9));
  EXPECT_TRUE(collector.contains(-10));
  EXPECT_FALSE(collector.contains(-11));

  it.reset(tx->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(-3));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_FALSE(collector.contains(3));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_FALSE(collector.contains(9));
  EXPECT_FALSE(collector.contains(12));
  EXPECT_FALSE(collector.contains(15));
  EXPECT_FALSE(collector.contains(18));
  EXPECT_TRUE(collector.contains(21));
  EXPECT_TRUE(collector.contains(24));
  EXPECT_TRUE(collector.contains(27));
  EXPECT_TRUE(collector.contains(30));
  EXPECT_FALSE(collector.contains(33));

  std::vector<int64_t> vect;
  vect.push_back(1);
  vect.push_back(3);
  vect.push_back(5);
  vect.push_back(7);
  x = solver_->MakeIntVar(vect, "x");
  it.reset(x->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(4, collector.size());
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_TRUE(collector.contains(1));
  EXPECT_FALSE(collector.contains(2));
  EXPECT_TRUE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));
  EXPECT_TRUE(collector.contains(5));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_TRUE(collector.contains(7));
  EXPECT_FALSE(collector.contains(8));
  EXPECT_FALSE(collector.contains(9));
  EXPECT_FALSE(collector.contains(10));
  EXPECT_FALSE(collector.contains(11));

  vect.push_back(101);
  x = solver_->MakeIntVar(vect, "x");
  it.reset(x->MakeDomainIterator(false));
  CollectAll(it.get(), &collector);
  EXPECT_EQ(5, collector.size());
  EXPECT_FALSE(collector.contains(-1));
  EXPECT_FALSE(collector.contains(0));
  EXPECT_TRUE(collector.contains(1));
  EXPECT_FALSE(collector.contains(2));
  EXPECT_TRUE(collector.contains(3));
  EXPECT_FALSE(collector.contains(4));
  EXPECT_TRUE(collector.contains(5));
  EXPECT_FALSE(collector.contains(6));
  EXPECT_TRUE(collector.contains(7));
  EXPECT_FALSE(collector.contains(8));
  EXPECT_FALSE(collector.contains(9));
  EXPECT_FALSE(collector.contains(10));
  EXPECT_FALSE(collector.contains(11));
  EXPECT_TRUE(collector.contains(101));
}

// Test value watchers.
TEST_IN_PROPAGATION(ValueWatcherTest, Dense, solver, true) {
  IntVar* const var = solver->MakeIntVar(0, 20);
  IntVar* const w1 = solver->MakeIsEqualCstVar(var, 1);
  IntVar* const w3 = solver->MakeIsEqualCstVar(var, 3);
  IntVar* const w14 = solver->MakeIsEqualCstVar(var, 14);
  IntVar* const w20 = solver->MakeIsEqualCstVar(var, 20);
  IntVar* const w21 = solver->MakeIsEqualCstVar(var, 21);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_EQ_RANGE(w20, 0, 1);
  EXPECT_BOUND_TO(w21, 0);

  var->SetRange(1, 15);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_BOUND_TO(w20, 0);

  var->RemoveValue(3);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_BOUND_TO(w3, 0);
  EXPECT_EQ_RANGE(w14, 0, 1);

  w14->SetValue(0);
  EXPECT_FALSE(var->Contains(14));

  w1->SetValue(1);
  EXPECT_BOUND_TO(var, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(ValueWatcherTest, Sparse, solver, true) {
  IntVar* const var = solver->MakeIntVar(0, 2000);
  IntVar* const w1 = solver->MakeIsEqualCstVar(var, 1);
  IntVar* const w3 = solver->MakeIsEqualCstVar(var, 3);
  IntVar* const w14 = solver->MakeIsEqualCstVar(var, 14);
  IntVar* const w2000 = solver->MakeIsEqualCstVar(var, 2000);
  IntVar* const w2001 = solver->MakeIsEqualCstVar(var, 2001);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_EQ_RANGE(w2000, 0, 1);
  EXPECT_BOUND_TO(w2001, 0);

  var->SetRange(1, 15);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_BOUND_TO(w2000, 0);

  var->RemoveValue(3);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_BOUND_TO(w3, 0);
  EXPECT_EQ_RANGE(w14, 0, 1);

  w14->SetValue(0);
  EXPECT_FALSE(var->Contains(14));

  w1->SetValue(1);
  EXPECT_BOUND_TO(var, 1);
  return nullptr;
  return nullptr;
}

// Test upper bound watchers.
TEST_IN_PROPAGATION(UpperBoundWatcherTest, Dense, solver, true) {
  IntVar* const var = solver->MakeIntVar(0, 20);
  IntVar* const wm1 = solver->MakeIsGreaterOrEqualCstVar(var, -1);
  IntVar* const w0 = solver->MakeIsGreaterOrEqualCstVar(var, 0);
  IntVar* const w1 = solver->MakeIsGreaterOrEqualCstVar(var, 1);
  IntVar* const w3 = solver->MakeIsGreaterOrEqualCstVar(var, 3);
  IntVar* const w14 = solver->MakeIsGreaterOrEqualCstVar(var, 14);
  IntVar* const w20 = solver->MakeIsGreaterOrEqualCstVar(var, 20);
  IntVar* const w21 = solver->MakeIsGreaterOrEqualCstVar(var, 21);
  EXPECT_BOUND_TO(wm1, 1);
  EXPECT_BOUND_TO(w0, 1);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_EQ_RANGE(w20, 0, 1);
  EXPECT_BOUND_TO(w21, 0);

  var->SetRange(1, 15);
  EXPECT_BOUND_TO(w0, 1);
  EXPECT_BOUND_TO(w1, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_BOUND_TO(w20, 0);

  w14->SetValue(0);
  EXPECT_EQ_RANGE(var, 1, 13);

  w3->SetValue(1);
  EXPECT_EQ_RANGE(var, 3, 13);
  return nullptr;
}

TEST_IN_PROPAGATION(UpperBoundWatcherTest, Sparse, solver, true) {
  IntVar* const var = solver->MakeIntVar(0, 2000);
  IntVar* const wm1 = solver->MakeIsGreaterOrEqualCstVar(var, -1);
  IntVar* const w0 = solver->MakeIsGreaterOrEqualCstVar(var, 0);
  IntVar* const w1 = solver->MakeIsGreaterOrEqualCstVar(var, 1);
  IntVar* const w3 = solver->MakeIsGreaterOrEqualCstVar(var, 3);
  IntVar* const w14 = solver->MakeIsGreaterOrEqualCstVar(var, 14);
  IntVar* const w2000 = solver->MakeIsGreaterOrEqualCstVar(var, 2000);
  IntVar* const w2001 = solver->MakeIsGreaterOrEqualCstVar(var, 2001);
  EXPECT_BOUND_TO(wm1, 1);
  EXPECT_BOUND_TO(w0, 1);
  EXPECT_EQ_RANGE(w1, 0, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_EQ_RANGE(w2000, 0, 1);
  EXPECT_BOUND_TO(w2001, 0);

  var->SetRange(1, 15);
  EXPECT_BOUND_TO(w0, 1);
  EXPECT_BOUND_TO(w1, 1);
  EXPECT_EQ_RANGE(w3, 0, 1);
  EXPECT_EQ_RANGE(w14, 0, 1);
  EXPECT_BOUND_TO(w2000, 0);

  w14->SetValue(0);
  EXPECT_EQ_RANGE(var, 1, 13);

  w3->SetValue(1);
  EXPECT_EQ_RANGE(var, 3, 13);
  return nullptr;
}
}  // namespace
}  // namespace operations_research
