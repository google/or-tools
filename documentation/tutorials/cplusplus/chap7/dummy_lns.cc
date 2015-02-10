// Copyright 2011-2014 Google
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
//
// Dummy Local Search to understand the behavior of LSN Operators.

#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"


DEFINE_int64(n, 4, "Size of the problem");

DEFINE_int64(ls_time_limit, 10000, "LS time limit (in ms)");
DEFINE_int64(ls_branches_limit, 10000, "LS branches limit");
DEFINE_int64(ls_failures_limit, 10000, "LS failures limit");
DEFINE_int64(ls_solutions_limit, 1, "LS solutions limit");
DEFINE_bool(print_intermediate_solutions, true,
            "Add a search log for the objective?");

namespace operations_research {

class OneVarLns : public BaseLNS {
 public:
  OneVarLns(const std::vector<IntVar*>& vars)
      : BaseLNS(vars),
        index_(0) {}

  ~OneVarLns() {}

  virtual void InitFragments() { index_ = 0; }

  virtual bool NextFragment(std::vector<int>* fragment) {
    const int size = Size();
    if (index_ < size) {
      fragment->push_back(index_);
      ++index_;
      return true;
    } else {
      return false;
    }
  }

 private:
  int index_;
};

void DummyLNS(const int64 n) {
    CHECK_GE(n, 2) << "size of problem (n) must be greater or equal than 2";
    LOG(INFO) << "Simple Large Neighborhood Search with initial solution";

    Solver s("Dummy LNS");

    //  Model
    vector<IntVar*> vars;
    s.MakeIntVarArray(n, 0, n-1, &vars);
    IntVar* const sum_var = s.MakeSum(vars)->Var();
    OptimizeVar* const obj = s.MakeMinimize(sum_var, 1);

    // unique constraint x_0 >= 1
    s.AddConstraint(s.MakeGreaterOrEqual(vars[0], 1));

    // initial solution
    Assignment * const initial_solution = s.MakeAssignment();
    initial_solution->Add(vars);
    for (int i = 0; i < n; ++i) {
      if (i % 2 == 0) {
        initial_solution->SetValue(vars[i], n - 1);
      } else {
        initial_solution->SetValue(vars[i], n - 2);
      }
    }

    // complementary phase builder
    Assignment * const optimal_candidate_solution = s.MakeAssignment();
    optimal_candidate_solution->Add(vars);
    optimal_candidate_solution->AddObjective(sum_var);
    DecisionBuilder * optimal_complementary_db = s.MakeNestedOptimize(
            s.MakePhase(vars,
                        Solver::CHOOSE_FIRST_UNBOUND,
                        Solver::ASSIGN_MAX_VALUE),
            optimal_candidate_solution,
            false,
            1);

    //  BaseLNS
    OneVarLns one_var_lns(vars);

    SearchLimit * const limit = s.MakeLimit(FLAGS_ls_time_limit,
                                            FLAGS_ls_branches_limit,
                                            FLAGS_ls_failures_limit,
                                            FLAGS_ls_solutions_limit);

    LocalSearchPhaseParameters* ls_params
     = s.MakeLocalSearchPhaseParameters(&one_var_lns, optimal_complementary_db, limit);

    DecisionBuilder* ls = s.MakeLocalSearchPhase(initial_solution, ls_params);

    SolutionCollector* const collector = s.MakeLastSolutionCollector();
    collector->Add(vars);
    collector->AddObjective(sum_var);

    SearchMonitor* log = NULL;
    if (FLAGS_print_intermediate_solutions) {
      log = s.MakeSearchLog(1000, obj);
    }

    if (s.Solve(ls,  collector, obj, log)) {
    LOG(INFO) << "Objective value = " << collector->objective_value(0);
    } else {
      LG << "No solution...";
    }
}

}  //  namespace operations_research

int main(int argc, char** argv) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    operations_research::DummyLNS(FLAGS_n);
    return 0;
}

