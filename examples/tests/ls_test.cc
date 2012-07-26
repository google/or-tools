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


#include "base/hash.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/model.pb.h"

namespace operations_research {
class OneVarLns : public BaseLNS {
 public:
  OneVarLns(const IntVar* const* vars, int size)
      : BaseLNS(vars, size),
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

void Basic_LS() {
  Solver s("Sample");
  vector<IntVar*> vars;
  s.MakeIntVarArray(10, 0, 10, &vars);
  IntVar* const sum_var = s.MakeSum(vars)->Var();
  OptimizeVar* const obj = s.MakeMinimize(sum_var, 1);
  DecisionBuilder* const db =
      s.MakePhase(vars,
                  Solver::CHOOSE_FIRST_UNBOUND,
                  Solver::ASSIGN_MAX_VALUE);
  OneVarLns one_var_lns(vars.data(), vars.size());
  LocalSearchPhaseParameters* const ls_params =
      s.MakeLocalSearchPhaseParameters(&one_var_lns, db);
  DecisionBuilder* const ls = s.MakeLocalSearchPhase(vars, db, ls_params);
  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->Add(vars);
  //  collector->AddObjective(sum_var);
  SearchMonitor* const log = s.MakeSearchLog(100, obj);
  s.Solve(ls, collector, obj, log);
  LOG(INFO) << "Objective value = " << collector->objective_value(0);
}
}  //namespace operations_research

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::Basic_LS();
  return 0;
}

