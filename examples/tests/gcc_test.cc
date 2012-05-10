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

DEFINE_int32(vsize, 5, "Number of variables");
DEFINE_int32(csize, 5, "Number of variables");
DEFINE_int32(slack, 4, "Slack in cardinalities");
DEFINE_int32(seed, 1, "Random seed");

namespace operations_research {
extern Constraint* Gcc(Solver* const solver,
                       const std::vector<IntVar*>& vars,
                       int64 first_domain_value,
                       const std::vector<int>& min_occurrences,
                       const std::vector<int>& max_occurrences);

int64 TestGcc(int vsize, int csize, int slack, int seed, bool use_gcc) {
  if (vsize > csize + slack) {
    LOG(INFO) << "Cannot create problem";
    return 0;
  }
  ACMRandom rgen(seed); // defines a random generator

  std::vector<int> card_min(csize, 0);
  std::vector<int> card_max(csize, 0);
  for (int i = 0; i< vsize - slack; ++i) {
    const int index = rgen.Uniform(csize);
    card_min[index]++;
    card_max[index]++;
  }
  for (int i = 0; i < slack; ++i) {
    const int index = rgen.Uniform(csize);
    card_max[index]++;
  }

  LOG(INFO) << (use_gcc ? "Gcc constraint:" : "Distribute constraint:");
  LOG(INFO) << "  - num variables = " << vsize;
  LOG(INFO) << "  - num values = " << csize;
  LOG(INFO) << "  - slack = " << slack;
  LOG(INFO) << "  - seed = " << seed;
  Solver solver("TestGcc");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(vsize, 0, csize - 1, &vars);
  Constraint* const gcc = use_gcc ?
                          Gcc(&solver, vars, 0, card_min, card_max) :
                          solver.MakeDistribute(vars, card_min, card_max);
  solver.AddConstraint(gcc);
  DecisionBuilder* const db = solver.MakePhase(vars,
                                               Solver::CHOOSE_FIRST_UNBOUND,
                                               Solver::ASSIGN_MIN_VALUE);

  LOG(INFO) << "Start search";
  CycleTimer t;
  t.Start();
  solver.NewSearch(db);
  int counter = 0;
  while(solver.NextSolution()) {
    counter++;
  }
  solver.EndSearch();
  t.Stop();

  LOG(INFO) << "test time : " << t.GetInUsec() << " micro seconds";
  LOG(INFO) << "Found " << counter << " solutions";
  return counter;
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(operations_research::TestGcc(FLAGS_vsize,
                                        FLAGS_csize,
                                        FLAGS_slack,
                                        FLAGS_seed,
                                        false),
           operations_research::TestGcc(FLAGS_vsize,
                                        FLAGS_csize,
                                        FLAGS_slack,
                                        FLAGS_seed,
                                        true));
  return 0;
}
