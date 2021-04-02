// Copyright 2011-2012 Google
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

namespace operations_research {

class ForbiddenIntervalTestSimpleReductionOnBothSide : public DecisionBuilder {
 public:
  ForbiddenIntervalTestSimpleReductionOnBothSide(IntVar* const var)
      : var_(var) {}
  ~ForbiddenIntervalTestSimpleReductionOnBothSide() override {}

  Decision* Next(Solver* const s) override {
    CHECK_EQ(101, var_->Min());
    CHECK_EQ(899, var_->Max());
    return NULL;
  }

 private:
  IntVar* const var_;
};

class ForbiddenIntervalTestMultipleReductionsOnMin : public DecisionBuilder {
 public:
  ForbiddenIntervalTestMultipleReductionsOnMin(IntVar* const var) : var_(var) {}
  ~ForbiddenIntervalTestMultipleReductionsOnMin() override {}

  Decision* Next(Solver* const s) override {
    CHECK_EQ(0, var_->Min());
    CHECK_EQ(1000, var_->Max());
    var_->SetMin(5);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(1000, var_->Max());
    var_->SetMax(995);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMin(10);
    CHECK_EQ(21, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMin(30);
    CHECK_EQ(30, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMin(505);
    CHECK_EQ(511, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMin(600);
    CHECK_EQ(600, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMin(900);
    CHECK_EQ(901, var_->Min());
    CHECK_EQ(995, var_->Max());
    return NULL;
  }

 private:
  IntVar* const var_;
};

class ForbiddenIntervalTestMultipleReductionsOnMax : public DecisionBuilder {
 public:
  ForbiddenIntervalTestMultipleReductionsOnMax(IntVar* const var) : var_(var) {}
  ~ForbiddenIntervalTestMultipleReductionsOnMax() override {}

  Decision* Next(Solver* const s) override {
    CHECK_EQ(0, var_->Min());
    CHECK_EQ(1000, var_->Max());
    var_->SetMin(5);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(1000, var_->Max());
    var_->SetMax(995);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(995, var_->Max());
    var_->SetMax(900);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(799, var_->Max());
    var_->SetMax(750);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(750, var_->Max());
    var_->SetMax(505);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(499, var_->Max());
    var_->SetMax(300);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(300, var_->Max());
    var_->SetMax(20);
    CHECK_EQ(5, var_->Min());
    CHECK_EQ(9, var_->Max());
    return NULL;
  }

 private:
  IntVar* const var_;
};

class ForbiddenIntervalTest {
 public:
  void SetUp(std::vector<int64_t>& starts, std::vector<int64_t>& ends) {
    solver_.reset(new Solver("ForbiddenIntervalTest"));
    var_ = solver_->MakeIntVar(0, 1000, "var");
    CHECK_EQ(starts.size(), ends.size());
    for (std::size_t i = 0; i < starts.size(); ++i) {
      var_->RemoveInterval(starts[i], ends[i]);
    }
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;

  void TestSimpleReductionOnBothSide() {
    std::cout << "TestSimpleReductionOnBothSide" << std::endl;
    std::vector<int64_t> starts = {0, 900};
    std::vector<int64_t> ends = {100, 1000};
    SetUp(starts, ends);
    CHECK(solver_->Solve(solver_->RevAlloc(
        new ForbiddenIntervalTestSimpleReductionOnBothSide(var_))));
    std::cout << "  .. done" << std::endl;
  }

  void TestMultipleReductionsOnMin() {
    std::cout << "TestMultipleReductionsOnMin" << std::endl;
    std::vector<int64_t> starts = {10, 500, 800};
    std::vector<int64_t> ends = {20, 510, 900};
    SetUp(starts, ends);
    CHECK(solver_->Solve(solver_->RevAlloc(
        new ForbiddenIntervalTestMultipleReductionsOnMin(var_))));
    std::cout << "  .. done" << std::endl;
  }

  void TestMultipleReductionsOnMax() {
    std::cout << "TestMultipleReductionsOnMax" << std::endl;
    std::vector<int64_t> starts = {10, 500, 800};
    std::vector<int64_t> ends = {20, 510, 900};
    SetUp(starts, ends);
    CHECK(solver_->Solve(solver_->RevAlloc(
        new ForbiddenIntervalTestMultipleReductionsOnMax(var_))));
    std::cout << "  .. done" << std::endl;
  }
};
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  operations_research::ForbiddenIntervalTest forbidden_intervals_test;
  forbidden_intervals_test.TestSimpleReductionOnBothSide();
  forbidden_intervals_test.TestMultipleReductionsOnMin();
  forbidden_intervals_test.TestMultipleReductionsOnMax();
  return 0;
}
