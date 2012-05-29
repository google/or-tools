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
#include "util/string_array.h"

DEFINE_int32(vars, 2, "Number of variables");
DEFINE_int32(values, 3, "Number of values");
DEFINE_int32(slack, 1, "Slack in cardinalities");
DEFINE_int32(seed, 1, "Random seed");

namespace operations_research {
extern Constraint* MakeGcc(Solver* const solver,
                           const std::vector<IntVar*>& vars,
                           int64 first_domain_value,
                           const std::vector<int>& min_occurrences,
                           const std::vector<int>& max_occurrences);

extern Constraint* MakeSoftGcc(Solver* const solver,
                               const std::vector<IntVar*>& vars,
                               int64 min_value,
                               const std::vector<int>& card_mins,
                               const std::vector<int>& card_max,
                               IntVar* const violation_var);


static const char* kConstraintName[] = { "Distribute", "Gcc", "SoftGcc" };

int64 TestGcc(int num_vars, int num_values, int slack, int seed, int type) {
  ACMRandom rgen(seed); // defines a random generator

  std::vector<int> card_min(num_values, 0);
  std::vector<int> card_max(num_values, 0);
  for (int i = 0; i< num_vars - slack; ++i) {
    const int index = rgen.Uniform(num_values);
    card_min[index]++;
    card_max[index]++;
  }
  for (int i = 0; i < 2 * slack; ++i) {
    card_max[rgen.Uniform(num_values)]++;
  }

  LOG(INFO) << kConstraintName[type] << " constraint";
  LOG(INFO) << "  - num variables = " << num_vars;
  LOG(INFO) << "  - num values = " << num_values;
  LOG(INFO) << "  - slack = " << slack;
  LOG(INFO) << "  - seed = " << seed;
  LOG(INFO) << "  - min_cards = [" << IntVectorToString(card_min, " ") << "]";
  LOG(INFO) << "  - max_cards = [" << IntVectorToString(card_max, " ") << "]";

  Solver solver("TestGcc");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(num_vars, 0, num_values - 1, &vars);
  switch (type) {
    case 0:
      solver.AddConstraint(solver.MakeDistribute(vars, card_min, card_max));
      break;
    case 1:
      solver.AddConstraint(MakeGcc(&solver, vars, 0, card_min, card_max));
      break;
    case 2:
      solver.AddConstraint(MakeSoftGcc(&solver,
                                       vars,
                                       0,
                                       card_min,
                                       card_max,
                                       solver.MakeIntConst(0)));
      break;
    default:
      LOG(FATAL) << "Constraint type not recognized";
  }
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
  operations_research::TestGcc(FLAGS_vars,
                               FLAGS_values,
                               FLAGS_slack,
                               FLAGS_seed,
                               0);
  operations_research::TestGcc(FLAGS_vars,
                               FLAGS_values,
                               FLAGS_slack,
                               FLAGS_seed,
                               1);
  operations_research::TestGcc(FLAGS_vars,
                               FLAGS_values,
                               FLAGS_slack,
                               FLAGS_seed,
                               2);
  return 0;
}
