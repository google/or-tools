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

#include "ortools/constraint_solver/constraint_solver.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/gzipstring.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/reversible_engine.h"

namespace operations_research {
namespace {
class ConstraintAddTestOneNestedConstraint;
}
namespace {
class ConstraintAddTestTenNestedConstraints;
}
namespace {
class ConstraintAddTestTwoLevelNesting;
}

namespace {
class KernelTest : public testing::Test {
 protected:
  KernelTest() : solver_(new Solver("KernelTest")) {}
  std::unique_ptr<Solver> solver_;
};

class TestDemon : public Demon {
 public:
  explicit TestDemon(int* val) : val_(val) {}
  ~TestDemon() override {}

  void Run(Solver*) override { (*val_) = 246; }
  std::string DebugString() const override {
    return absl::StrFormat("TestDemon(%i)", *val_);
  }

 private:
  int* val_;
};

TEST_F(KernelTest, Demon) {
  int val = 123;
  TestDemon test1(&val);
  test1.Run(solver_.get());
  EXPECT_EQ(246, val) << "val not set by demon";
}

class MyObject : public BaseObject {
 public:
  explicit MyObject(int* val) : val_(val) { (*val_) += 2; }
  ~MyObject() override { (*val_) += 3; }
  int Value() const { return *val_; }

 private:
  int* val_;
};

TEST_F(KernelTest, RevObject) {
  int val = 0;
  solver_->PushState();
  MyObject* myO = solver_->RevAlloc(new MyObject(&val));
  EXPECT_EQ(2, myO->Value()) << "value not set in ctor";
  solver_->PopState();
  EXPECT_EQ(5, val) << "value not set in dtor";
}

class SecondLevel : public DecisionBuilder {
 public:
  SecondLevel(int* a, int v) : value_(a), new_value_(v) {}
  ~SecondLevel() override {}
  Decision* Next(Solver* const s) override {
    s->SaveAndSetValue(value_, new_value_);
    EXPECT_EQ(new_value_, *value_) << "incorrect value";
    s->SaveAndSetValue(value_, new_value_ + 1);
    EXPECT_EQ(new_value_ + 1, *value_) << "incorrect value";
    return nullptr;
  }
  std::string DebugString() const override {
    return absl::StrFormat("SecondLevel(%i -> %i)", *value_, new_value_);
  }

 private:
  int* value_;
  int new_value_;
};

class FirstLevel : public DecisionBuilder {
 public:
  FirstLevel(int* a, bool fail) : value_(a), fail_(fail) {}
  ~FirstLevel() override {}
  Decision* Next(Solver* const s) override {
    EXPECT_EQ(0, *value_) << "initial value incorrect";
    SecondLevel* s1 = s->RevAlloc(new SecondLevel(value_, 2));
    s->SolveAndCommit(s1);
    EXPECT_EQ(3, *value_) << "incorrect value";
    SecondLevel* s2 = s->RevAlloc(new SecondLevel(value_, 4));
    s->Solve(s2);
    EXPECT_EQ(3, *value_) << "incorrect value";
    if (fail_) {
      s->Fail();
    }
    return nullptr;
  }
  std::string DebugString() const override {
    return absl::StrFormat("FirstLevel(%i)", *value_);
  }

 private:
  int* value_;
  bool fail_;
};

TEST_F(KernelTest, testNestedSolve) {
  int value = 0;
  FirstLevel* f1 = solver_->RevAlloc(new FirstLevel(&value, false));
  FirstLevel* f2 = solver_->RevAlloc(new FirstLevel(&value, true));
  solver_->NewSearch(f1);
  solver_->NextSolution();
  EXPECT_EQ(3, value) << "incorrect value";
  solver_->EndSearch();
  solver_->NewSearch(f2);
  solver_->NextSolution();
  EXPECT_EQ(0, value) << "incorrect value";
  solver_->EndSearch();
}

class RevAction : public DecisionBuilder {
 public:
  RevAction(int* a, bool fail) : value_(a), fail_(fail) {}
  ~RevAction() override {}
  Decision* Next(Solver* const s) override {
    int* const ptr = value_;
    const int old = *value_;
    s->AddBacktrackAction([ptr, old](Solver*) { *ptr = old; }, false);
    *value_ = 10;
    if (fail_) {
      s->Fail();
    }
    return nullptr;
  }
  std::string DebugString() const override {
    return absl::StrFormat("RevAction(%i)", *value_);
  }

 private:
  int* value_;
  bool fail_;
};

TEST_F(KernelTest, testRevActions) {
  int value = 0;
  RevAction* const r1 = solver_->RevAlloc(new RevAction(&value, false));
  RevAction* const r2 = solver_->RevAlloc(new RevAction(&value, true));
  solver_->NewSearch(r1);
  solver_->NextSolution();
  EXPECT_EQ(10, value);
  solver_->EndSearch();
  solver_->NewSearch(r2);
  solver_->NextSolution();
  EXPECT_EQ(0, value);
  solver_->EndSearch();
}

TEST_F(KernelTest, testRev) {
  solver_->PushState();
  std::unique_ptr<operations_research::Rev<int>> ri(new Rev<int>(5));

  EXPECT_EQ(5, ri->Value()) << "initial revInt value check";
  solver_->PushState();
  ri->SetValue(solver_.get(), 7);  // overwritten assignments
  ri->SetValue(solver_.get(), 8);
  ri->SetValue(solver_.get(), 12);
  EXPECT_EQ(12, ri->Value()) << "assigned revInt value check";
  solver_->PushState();
  ri->SetValue(solver_.get(), 23);
  EXPECT_EQ(23, ri->Value()) << "assigned revInt value check";
  solver_->PopState();
  EXPECT_EQ(12, ri->Value()) << "backtracked revInt value check";
  solver_->PopState();
  EXPECT_EQ(5, ri->Value()) << "backtracked revInt value check";
  solver_->PopState();
}

// Test profiled DecisionBuilders

class NoOpDecision : public Decision {
 public:
  NoOpDecision() {}
  void Apply(Solver*) override {}
  void Refute(Solver*) override {}
};

class SleepDecisionBuilder : public DecisionBuilder {
 public:
  SleepDecisionBuilder(double seconds, bool fail)
      : seconds_(seconds), fail_(fail) {}
  Decision* Next(Solver* const s) override {
    if (seconds_ > 0) {
      seconds_ -= 1.0;
      absl::SleepFor(absl::Seconds(1));
      return s->RevAlloc(new NoOpDecision);
    }
    if (fail_) s->Fail();
    return nullptr;
  }
  std::string DebugString() const override { return "Sleep"; }

 private:
  double seconds_;
  const bool fail_;
};

// test failure

void FailAction(Solver* s, int dir) {
  if (dir == 1) {  // right
    LOG(INFO) << "Failure on Right Move";
    s->Fail();
  } else {
    LOG(INFO) << "Failure on Left Move";
    s->Fail();
  }
}

void DisplaySuccess(int value) { LOG(INFO) << "Success(" << value << ") "; }

void DisplayFailure(int value) { LOG(INFO) << "Failure(" << value << ") "; }

class FullTree : public DecisionBuilder {
 public:
  explicit FullTree(int max_val) : max_val_(max_val), current_val_(0) {}
  ~FullTree() override {}
  Decision* Next(Solver* const s) override {
    const int val = current_val_.Value();
    if (val == max_val_) {
      return s->MakeDecision([](Solver* s) { FailAction(s, 0); },
                             [](Solver* s) { FailAction(s, 1); });
    } else {
      current_val_.SetValue(s, val + 1);
      return s->MakeDecision([val](Solver*) { DisplaySuccess(val + 1); },
                             [val](Solver*) { DisplayFailure(val + 1); });
    }
  }

 private:
  int max_val_;
  operations_research::Rev<int> current_val_;
};

TEST_F(KernelTest, testFail) {
  FullTree* ft = solver_->RevAlloc(new FullTree(3));
  solver_->Solve(ft);
}

class MyTrace : public SearchMonitor {
 public:
  explicit MyTrace(Solver* const s) : SearchMonitor(s) {}
  ~MyTrace() override {}

  void EnterSearch() override { LOG(INFO) << "EnterSearch()"; }
  void RestartSearch() override { LOG(INFO) << "RestartSearch()"; }
  void ExitSearch() override { LOG(INFO) << "ExitSearch()"; }
  void BeginNextDecision(DecisionBuilder* const b) override {
    LOG(INFO) << "BeginNextDecision(" << b->DebugString() << ") ";
  }
  void EndNextDecision(DecisionBuilder* const b, Decision* const d) override {
    LOG(INFO) << "EndNextDecision(" << b->DebugString() << ", "
              << d->DebugString() << ") ";
  }
  void ApplyDecision(Decision* const d) override {
    LOG(INFO) << "ApplyDecision(" << d->DebugString() << ") ";
  }
  void RefuteDecision(Decision* const d) override {
    LOG(INFO) << "RefuteDecision(" << d->DebugString() << ") ";
  }
  void AfterDecision(Decision* const d, bool apply) override {
    LOG(INFO) << "AfterDecision(" << d->DebugString() << ", " << apply << ") ";
  }
  void BeginFail() override { LOG(INFO) << "BeginFail()"; }
  void EndFail() override { LOG(INFO) << "EndFail()"; }
  void BeginInitialPropagation() override {
    LOG(INFO) << "BeginInitialPropagation()";
  }
  void EndInitialPropagation() override {
    LOG(INFO) << "EndInitialPropagation()";
  }
  bool AtSolution() override {
    LOG(INFO) << "AtSolution()";
    return false;
  }
  bool AcceptSolution() override {
    LOG(INFO) << "AcceptSolution()";
    return true;
  }
  void NoMoreSolutions() override { LOG(INFO) << "NoMoreSolutions()"; }
};

TEST_F(KernelTest, testSearchMonitors) {
  FullTree* ft = solver_->RevAlloc(new FullTree(2));
  MyTrace* tr = solver_->RevAlloc(new MyTrace(solver_.get()));
  solver_->Solve(ft, tr);
}

class SearchKiller : public SearchMonitor {
 public:
  SearchKiller(Solver* const s, int nb_nodes)
      : SearchMonitor(s), nb_nodes_(nb_nodes), current_(0) {}
  ~SearchKiller() override {}

  void BeginNextDecision(DecisionBuilder*) override {
    if (++current_ == nb_nodes_) {
      solver()->FinishCurrentSearch();
    }
  }
  void ExitSearch() override { EXPECT_EQ(nb_nodes_, current_); }
  bool AtSolution() override { return true; }

 private:
  const int nb_nodes_;
  int current_;
};

TEST_F(KernelTest, testFinishSearch) {
  IntVar* v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* db = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* sk = solver_->RevAlloc(new SearchKiller(solver_.get(), 5));
  EXPECT_TRUE(solver_->Solve(db, sk));
}

class SearchRestarter : public SearchMonitor {
 public:
  SearchRestarter(Solver* const s, int nb_nodes, IntVar* const v1,
                  IntVar* const v2, IntVar* const v3)
      : SearchMonitor(s),
        nb_nodes_(nb_nodes),
        current_(0),
        loops_(5),
        v1_(v1),
        v2_(v2),
        v3_(v3),
        passed_(false) {}
  ~SearchRestarter() override {}

  void BeginNextDecision(DecisionBuilder*) override {
    if (current_ == 0) {
      EXPECT_EQ(0, v1_->Min());
      EXPECT_EQ(10, v1_->Max());
      EXPECT_EQ(0, v2_->Min());
      EXPECT_EQ(10, v2_->Max());
      EXPECT_EQ(0, v3_->Min());
      EXPECT_EQ(10, v3_->Max());
    }
    if (++current_ == nb_nodes_) {
      if (!--loops_) {
        solver()->FinishCurrentSearch();
      } else {
        solver()->RestartCurrentSearch();
        current_ = 0;
      }
    }
  }
  void RestartSearch() override { passed_ = true; }
  void ExitSearch() override {
    EXPECT_EQ(0, loops_);
    EXPECT_TRUE(passed_);
  }
  bool AtSolution() override {
    EXPECT_EQ(0, v1_->Value());
    EXPECT_EQ(0, v2_->Value());
    EXPECT_EQ(0, v3_->Value());
    return true;
  }

 private:
  const int nb_nodes_;
  int current_;
  int loops_;
  IntVar* const v1_;
  IntVar* const v2_;
  IntVar* const v3_;
  bool passed_;
};

TEST_F(KernelTest, testRestartSearch) {
  IntVar* v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* db = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* sk =
      solver_->RevAlloc(new SearchRestarter(solver_.get(), 5, v1, v2, v3));
  EXPECT_TRUE(solver_->Solve(db, sk));
}

class PropagationCounter : public Constraint {
 public:
  explicit PropagationCounter(Solver* const s) : Constraint(s), counter_(0) {}

  ~PropagationCounter() override {}

  void Post() override { counter_++; }

  void InitialPropagate() override {}

  std::string DebugString() const override { return "PropagationCounter"; }

  void Accept(ModelVisitor* const visitor) const override {
    const char kId[] = "Dummy";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }

  int counter() const { return counter_; }

 private:
  int counter_;
};

class EmptyTree : public DecisionBuilder {
 public:
  explicit EmptyTree(bool do_fail) : do_fail_(do_fail), counter_(0) {}
  ~EmptyTree() override {}

  Decision* Next(Solver* const s) override {
    counter_++;
    if (do_fail_) {
      s->Fail();
    }
    return nullptr;
  }
  int counter() const { return counter_; }

 private:
  bool do_fail_;
  int counter_;
};

class RestartChecker : public SearchMonitor {
 public:
  explicit RestartChecker(Solver* const s) : SearchMonitor(s), passed_(false) {}
  ~RestartChecker() override {}

  void RestartSearch() override { passed_ = true; }
  void ExitSearch() override { EXPECT_TRUE(passed_); }

 private:
  bool passed_;
};

class MockConstraint : public Constraint {
 public:
  explicit MockConstraint(Solver* solver) : Constraint(solver) {}
  void Post() override {}
  void InitialPropagate() override {}
  void Accept(ModelVisitor* const visitor) const override {
    const char kId[] = "Mock";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }
};

// A constraint that posts more constraints when it is itself posted
class ConstraintWithDelagate : public Constraint {
 public:
  explicit ConstraintWithDelagate(Solver* solver) : Constraint(solver) {}
  void Post() override {
    Constraint* c = solver()->RevAlloc(new MockConstraint(solver()));
    solver()->AddConstraint(c);
  }
  void InitialPropagate() override {}
  void Accept(ModelVisitor* const visitor) const override {
    const char kId[] = "Delegate";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }
};

// A constraint should be able to be added during propagation, not just at
// the top level. This tests verifies that this is true even for constraints
// that create delegate constraints.
TEST_IN_PROPAGATION0(KernelTest, ConstraintWithDelegate, solver_, true) {
  Constraint* const constraint = new ConstraintWithDelagate(solver_);
  Constraint* const rev_constraint = solver_->RevAlloc(constraint);
  EXPECT_NO_FATAL_FAILURE(solver_->AddConstraint(rev_constraint));
  return nullptr;
}

TEST_F(KernelTest, CountNestedConstraints) {
  Constraint* const constraint = new ConstraintWithDelagate(solver_.get());
  Constraint* const rev_constraint = solver_->RevAlloc(constraint);
  Assignment* const empty_assignent = solver_->MakeAssignment();
  solver_->AddConstraint(rev_constraint);
  EXPECT_EQ(1, solver_->constraints());
  EXPECT_EQ(true, solver_->CheckAssignment(empty_assignent));
  EXPECT_EQ(1, solver_->constraints());
}

TEST_F(KernelTest, testBacktrackAPI) {
  PropagationCounter* pc =
      solver_->RevAlloc(new PropagationCounter(solver_.get()));
  solver_->AddConstraint(pc);
  EmptyTree* fail_tree = solver_->RevAlloc(new EmptyTree(true));
  EmptyTree* succeed_tree = solver_->RevAlloc(new EmptyTree(false));
  SearchMonitor* restart_checker =
      solver_->RevAlloc(new RestartChecker(solver_.get()));
  EXPECT_TRUE(solver_->Solve(succeed_tree));
  EXPECT_EQ(1, pc->counter());
  EXPECT_EQ(1, succeed_tree->counter());

  solver_->NewSearch(fail_tree);
  EXPECT_FALSE(solver_->NextSolution());
  EXPECT_EQ(2, pc->counter());
  EXPECT_EQ(1, fail_tree->counter());
  EXPECT_FALSE(solver_->NextSolution());
  EXPECT_EQ(2, pc->counter());
  EXPECT_EQ(1, fail_tree->counter());
  solver_->RestartSearch();
  EXPECT_EQ(2, pc->counter());
  EXPECT_FALSE(solver_->NextSolution());
  EXPECT_EQ(2, pc->counter());
  EXPECT_EQ(2, fail_tree->counter());
  solver_->EndSearch();
  EXPECT_EQ(2, pc->counter());
  EXPECT_EQ(2, fail_tree->counter());

  solver_->NewSearch(succeed_tree);
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(3, pc->counter());
  EXPECT_EQ(2, succeed_tree->counter());
  EXPECT_FALSE(solver_->NextSolution());
  EXPECT_EQ(3, pc->counter());
  EXPECT_EQ(2, succeed_tree->counter());
  solver_->RestartSearch();
  EXPECT_EQ(3, pc->counter());
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(3, pc->counter());
  EXPECT_EQ(3, succeed_tree->counter());
  solver_->EndSearch();
  EXPECT_EQ(3, pc->counter());
  EXPECT_EQ(3, succeed_tree->counter());

  solver_->NewSearch(succeed_tree, restart_checker);
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(4, pc->counter());
  EXPECT_EQ(4, succeed_tree->counter());
  solver_->RestartSearch();
  EXPECT_EQ(4, pc->counter());
  EXPECT_EQ(4, succeed_tree->counter());
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(4, pc->counter());
  EXPECT_EQ(5, succeed_tree->counter());
  solver_->EndSearch();
  EXPECT_EQ(4, pc->counter());
  EXPECT_EQ(5, succeed_tree->counter());
}

// PropagationBaseObject tests

TEST(PropagationBaseObjectTest, NameOn) {
  ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
  parameters.set_store_names(true);
  Solver solver("PropagationBaseObjectTest::TestNameOn", parameters);
  EXPECT_EQ(true, solver.parameters().store_names());
  PropagationBaseObject* object =
      solver.RevAlloc(new PropagationBaseObject(&solver));
  EXPECT_FALSE(object->DebugString().empty());
  EXPECT_TRUE(object->name().empty());
  const std::string name = "Name";
  object->set_name(name);
  EXPECT_EQ(object->name(), name);
  object->set_name("");
  EXPECT_TRUE(object->name().empty());
}

TEST(PropagationBaseObjectTest, NameOff) {
  ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
  parameters.set_store_names(false);
  Solver solver("PropagationBaseObjectTest::TestNameOff", parameters);
  EXPECT_EQ(false, solver.parameters().store_names());
  PropagationBaseObject* object =
      solver.RevAlloc(new PropagationBaseObject(&solver));
  EXPECT_FALSE(object->DebugString().empty());
  EXPECT_TRUE(object->name().empty());
  const std::string name = "Name";
  object->set_name(name);
  EXPECT_TRUE(object->name().empty());
}

// Check Assignment

TEST(TestCheckAssignment, EmptyModel) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  EXPECT_TRUE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, EmptyAssignment) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  solver.AddConstraint(solver.MakeGreaterOrEqual(x, 4));
  EXPECT_TRUE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, PositiveCheck) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  solver.AddConstraint(solver.MakeLessOrEqual(x, 4));
  EXPECT_TRUE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, PositiveCheckBis) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  solver.AddConstraint(solver.MakeLessOrEqual(x, 4));
  solver.AddConstraint(solver.MakeNonEquality(x, 2));
  EXPECT_TRUE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, NegativeCheck) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  solver.AddConstraint(solver.MakeGreaterOrEqual(x, 4));
  EXPECT_FALSE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, NegativeCheckWithName) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  Constraint* const ct = solver.MakeGreaterOrEqual(x, 4);
  ct->set_name("ct");
  solver.AddConstraint(ct);
  EXPECT_FALSE(solver.CheckAssignment(assignment));
}

TEST(TestCheckAssignment, NegativeCheckBis) {
  Solver solver("TestCheckAssignment");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(x);
  assignment->SetValue(x, 3);
  solver.AddConstraint(solver.MakeLessOrEqual(x, 4));
  solver.AddConstraint(solver.MakeNonEquality(x, 3));
  EXPECT_FALSE(solver.CheckAssignment(assignment));
}

// We unit test the compression schema independently from the solver context.
// This code is cloned from the internal kernel code because we do not wish to
// expose this low level API and we have a strong wish to limit the headers
// used in the project.
// A second test using the public API and mimicking this first test is done
// after. This first test is used to communicate with either the gzip/zippy
// code owners, or with the valgrind owners.

template <class T>
using local_addrval = internal::addrval<T>;

// ----- Compressed trail -----

template <class T>
class LocalCompressedTrail {
 public:
  LocalCompressedTrail(int block_size, bool use_gzip)
      : use_gzip_(use_gzip),
        block_size_(block_size),
        blocks_(nullptr),
        free_blocks_(nullptr),
        data_(new local_addrval<T>[block_size]),
        buffer_(new local_addrval<T>[block_size]),
        buffer_used_(false),
        current_(0),
        size_(0) {}
  ~LocalCompressedTrail() {
    FreeBlocks(blocks_);
    FreeBlocks(free_blocks_);
  }
  const local_addrval<T>& Back() const {
    DCHECK_GT(current_, 0) << "Back of empty trail";
    return data_[current_ - 1];
  }
  void PopBack() {
    if (size_ > 0) {
      --current_;
      if (current_ <= 0) {
        if (buffer_used_) {
          data_.swap(buffer_);
          current_ = block_size_;
          buffer_used_ = false;
        } else if (blocks_ != nullptr) {
          if (use_gzip_) {
            std::string uncompressed_block;
            const bool ok =
                GunzipString(blocks_->compressed, &uncompressed_block);
            memcpy(data_.get(), uncompressed_block.c_str(),
                   uncompressed_block.size());
            DCHECK(ok);
          }
          FreeTopBlock();
          current_ = block_size_;
        }
      }
      --size_;
    }
  }
  void PushBack(const local_addrval<T>& addr_val) {
    if (current_ >= block_size_) {
      if (buffer_used_) {  // Buffer is used.
        absl::string_view input(reinterpret_cast<const char*>(buffer_.get()),
                                block_size_ * sizeof(local_addrval<T>));
        NewTopBlock();
        if (use_gzip_) {
          GzipString(input, &blocks_->compressed);
        }
        // O(1) operation.
        data_.swap(buffer_);
      } else {
        data_.swap(buffer_);
        buffer_used_ = true;
      }
      current_ = 0;
    }
    data_[current_] = addr_val;
    ++current_;
    ++size_;
  }
  int size() const { return size_; }

 private:
  struct Block {
    Block() : next(nullptr) {}
    std::string compressed;
    Block* next;
  };

  void FreeTopBlock() {
    Block* block = blocks_;
    blocks_ = block->next;
    block->compressed.clear();
    block->next = free_blocks_;
    free_blocks_ = block;
  }
  void NewTopBlock() {
    Block* block = nullptr;
    if (free_blocks_ != nullptr) {
      block = free_blocks_;
      free_blocks_ = block->next;
    } else {
      block = new Block;
    }
    block->next = blocks_;
    blocks_ = block;
  }
  void FreeBlocks(Block* blocks) {
    while (nullptr != blocks) {
      Block* next = blocks->next;
      delete blocks;
      blocks = next;
    }
  }

  const bool use_gzip_;
  const int block_size_;
  Block* blocks_;
  Block* free_blocks_;
  std::unique_ptr<local_addrval<T>[]> data_;
  std::unique_ptr<local_addrval<T>[]> buffer_;
  bool buffer_used_;
  int current_;
  int size_;
};

// Use a data type that is always different from a pointer size
class LocalCompressedTrailShortTest : public testing::Test {
 protected:
  LocalCompressedTrailShortTest() {}
  std::unique_ptr<LocalCompressedTrail<int16_t>> trail_;
  static constexpr size_t kNumValues = 256;
  void TestAdd();
};

void LocalCompressedTrailShortTest::TestAdd() {
  int16_t values[kNumValues];
  // initialize the values
  for (int16_t i = 0; i < kNumValues; ++i) {
    values[i] = i;
  }
  // save the values
  for (int16_t i = 0; i < kNumValues; ++i) {
    local_addrval<int16_t> address(&values[i]);
    trail_->PushBack(address);
  }
  // clear the values
  for (int16_t i = 0; i < kNumValues; ++i) {
    values[i] = 0;
  }
  // restore the values
  for (int16_t i = 0; i < kNumValues; ++i) {
    const local_addrval<int16_t> val = trail_->Back();
    val.restore();
    trail_->PopBack();
  }
  // Check the values
  for (int16_t i = 0; i < kNumValues; ++i) {
    EXPECT_EQ(i, values[i]);
  }
}

TEST_F(LocalCompressedTrailShortTest, AddGzip) {
  for (int block_size = 1; block_size < 8; ++block_size) {
    trail_ = std::make_unique<LocalCompressedTrail<int16_t>>(block_size, true);
    TestAdd();
  }
}

// This is the same test using the public API of the constraint solver.
class CompressedTrailTest : public testing::Test {
 protected:
  CompressedTrailTest() {}
  std::unique_ptr<Solver> solver_;
  static constexpr size_t kNumValues = 256;
  void TestAdd();
};

void CompressedTrailTest::TestAdd() {
  int32_t values[kNumValues];
  int64_t big_values[kNumValues];
  // initialize the values
  for (int32_t i = 0; i < kNumValues; ++i) {
    values[i] = i;
    big_values[i] = i;
  }
  solver_->PushState();
  // save the values
  for (int32_t i = 0; i < kNumValues; ++i) {
    solver_->SaveValue(&values[i]);
    solver_->SaveValue(&big_values[i]);
  }
  // clear the values
  for (int32_t i = 0; i < kNumValues; ++i) {
    values[i] = 0;
    big_values[i] = 0;
  }
  // restore the values
  solver_->PopState();
  // Check the values
  for (int32_t i = 0; i < kNumValues; ++i) {
    EXPECT_EQ(i, values[i]);
    EXPECT_EQ(i, big_values[i]);
  }
}

TEST_F(CompressedTrailTest, AddGzip) {
  for (int block_size = 1; block_size < 8; ++block_size) {
    ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
    parameters.set_trail_block_size(block_size);
    parameters.set_compress_trail(
        ConstraintSolverParameters::COMPRESS_WITH_GZIP);
    solver_ = std::make_unique<Solver>("CompressedTrailTest::TestAddGzip",
                                       parameters);
    TestAdd();
  }
}

TEST_F(CompressedTrailTest, AddNoCompression) {
  for (int block_size = 1; block_size < 8; ++block_size) {
    ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
    parameters.set_trail_block_size(block_size);
    parameters.set_compress_trail(ConstraintSolverParameters::NO_COMPRESSION);
    solver_ = std::make_unique<Solver>(
        "CompressedTrailTest::TestAddNoCompression", parameters);
    TestAdd();
  }
}

TEST_F(CompressedTrailTest, AddZlib) {
  for (int block_size = 1; block_size < 8; ++block_size) {
    ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
    parameters.set_trail_block_size(block_size);
    parameters.set_compress_trail(
        ConstraintSolverParameters::COMPRESS_WITH_ZLIB);
    solver_ = std::make_unique<Solver>("CompressedTrailTest::TestAddZlib",
                                       parameters);
    TestAdd();
  }
}

class NestedConstraint : public Constraint {
 public:
  NestedConstraint(Solver* const s, int expected, int new_value, int* ptr)
      : Constraint(s), expected_(expected), new_value_(new_value), ptr_(ptr) {}
  ~NestedConstraint() override {}

  void Post() override { EXPECT_EQ(expected_, *ptr_); }

  void InitialPropagate() override {
    EXPECT_EQ(expected_, *ptr_);
    *ptr_ = new_value_;
  }
  void Accept(ModelVisitor* const visitor) const override {
    const char kId[] = "Nested";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }

 private:
  const int expected_;
  const int new_value_;
  int* const ptr_;
};

static const int kBeforeTwoLevelNested = -2;
static const int kBeforeNesting = -1;

class WrappingConstraint : public Constraint {
 public:
  WrappingConstraint(Solver* const s, int nested_count, int* ptr)
      : Constraint(s), nested_count_(nested_count), ptr_(ptr) {}
  ~WrappingConstraint() override {}

  void Post() override {
    Solver* const s = solver();
    if (nested_count_ > 0) {
      EXPECT_EQ(kBeforeNesting, *ptr_);
      *ptr_ = 0;
      for (int i = 0; i < nested_count_; ++i) {
        s->AddConstraint(
            s->RevAlloc(new NestedConstraint(s, i + 1, i + 2, ptr_)));
      }
    } else {
      EXPECT_EQ(kBeforeTwoLevelNested, *ptr_);
      s->AddConstraint(s->RevAlloc(new WrappingConstraint(s, 1, ptr_)));
    }
  }

  void InitialPropagate() override {
    *ptr_ = nested_count_ == kBeforeNesting ? kBeforeNesting : 1;
  }

  void Accept(ModelVisitor* const visitor) const override {
    const char kId[] = "Wrapping";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }

 private:
  const int nested_count_;
  int* const ptr_;
};

TEST_IN_PROPAGATION(ConstraintAddTest, OneNestedConstraint, solver, true) {
  int shared_int = kBeforeNesting;
  solver->AddConstraint(
      solver->RevAlloc(new WrappingConstraint(solver, 1, &shared_int)));
  return nullptr;
}

TEST_IN_PROPAGATION(ConstraintAddTest, TenNestedConstraints, solver, true) {
  int shared_int = kBeforeNesting;
  solver->AddConstraint(
      solver->RevAlloc(new WrappingConstraint(solver, 10, &shared_int)));
  return nullptr;
}

TEST_IN_PROPAGATION(ConstraintAddTest, TwoLevelNesting, solver, true) {
  int shared_int = kBeforeTwoLevelNested;
  solver->AddConstraint(solver->RevAlloc(
      new WrappingConstraint(solver, kBeforeNesting, &shared_int)));
  return nullptr;
}

// Test Solution Counter

TEST(TestSolutionCounter, EmptySearch) {
  Solver solver("TestSolutionCounter::EmptySearch");
  EXPECT_EQ(0, solver.solutions());
}

TEST(TestSolutionCounter, SequenceOfSearch) {
  Solver solver("TestSolutionCounter::SequenceOfSearch");
  const int kIntVarMax = 10;
  const int kLoops = 3;
  IntVar* const var = solver.MakeIntVar(0, kIntVarMax);
  DecisionBuilder* const db = solver.MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  for (int loop = 0; loop < kLoops; ++loop) {
    int counter = 0;
    solver.NewSearch(db);
    while (solver.NextSolution()) {
      counter++;
      EXPECT_EQ(counter, solver.solutions());
    }
    solver.EndSearch();
    EXPECT_EQ(kIntVarMax + 1, counter);
    EXPECT_EQ(kIntVarMax + 1, solver.solutions());
  }
}

// CheckConstraint

TEST(TestCheckConstraint, ValidConstraint) {
  Solver solver("TestCheckConstraint::ValidConstraint");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Constraint* const ct = solver.MakeGreaterOrEqual(x, 5);
  EXPECT_TRUE(solver.CheckConstraint(ct));
}

TEST(TestCheckConstraint, InvalidConstraint) {
  Solver solver("TestCheckConstraint::InvalidConstraint");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  Constraint* const ct = solver.MakeGreaterOrEqual(x, 15);
  EXPECT_FALSE(solver.CheckConstraint(ct));
}

class InfeasibleModel : public KernelTest {
  // Empty class: its sole purpose is to be named.
};

TEST_F(InfeasibleModel, testFalseConstraint) {
  IntVar* var = solver_->MakeIntConst(3);
  solver_->AddConstraint(solver_->MakeFalseConstraint());
  DecisionBuilder* decision_builder =
      solver_->MakePhase(var, Solver::INT_VAR_SIMPLE, Solver::INT_VALUE_SIMPLE);
  solver_->NewSearch(decision_builder);
  EXPECT_FALSE(solver_->NextSolution());
}

TEST_F(InfeasibleModel, testInfeasibleModel) {
  IntVar* var = solver_->MakeIntVar(0, 100);
  solver_->AddConstraint(solver_->MakeGreater(var, 34));
  solver_->AddConstraint(solver_->MakeLess(var, 18));
  DecisionBuilder* decision_builder =
      solver_->MakePhase(var, Solver::INT_VAR_SIMPLE, Solver::INT_VALUE_SIMPLE);
  solver_->NewSearch(decision_builder);
  EXPECT_FALSE(solver_->NextSolution());
}

TEST_F(InfeasibleModel, testSetMin) {
  std::vector<IntVar*> variables = {solver_->MakeIntConst(3),
                                    solver_->MakeIntVar(-4, -2),
                                    solver_->MakeBoolVar()};
  for (IntVar* const var : variables) {
    var->SetMin(51);
  }
  // print
  ModelVisitor* visitor = solver_->MakePrintModelVisitor();
  solver_->Accept(visitor);
  LOG(INFO) << solver_->DebugString();
  DecisionBuilder* decision_builder = solver_->MakePhase(
      variables, Solver::INT_VAR_SIMPLE, Solver::INT_VALUE_SIMPLE);
  solver_->NewSearch(decision_builder);
  EXPECT_FALSE(solver_->NextSolution());
}

TEST(SolverParametersDeathTest, InvalidParameters) {
  const char error[] =
      "Were parameters built using Solver::DefaultSolverParameters() ?";
  const ConstraintSolverParameters parameters1;
  EXPECT_DEATH(new Solver("solver", parameters1), error);
  ConstraintSolverParameters parameters2;
  parameters2.set_array_split_size(-1);
  EXPECT_DEATH(new Solver("solver", parameters2), error);
}

}  // namespace
}  // namespace operations_research
