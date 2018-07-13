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

#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/random.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"

DEFINE_int32(vars, 3, "Number of variables");
DEFINE_int32(values, 5, "Number of values");
DEFINE_int32(slack, 1, "Slack in cardinalities");
DEFINE_int32(seed, 1, "Random seed");
DEFINE_int32(offset, 0, "Min value of variables");

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

int64 TestGcc(int num_vars,
              int num_values,
              int slack,
              int seed,
              int type,
              int offset) {
  ACMRandom rgen(seed); // defines a random generator

  std::vector<int> card_min(num_values, 0);
  std::vector<int> card_max(num_values, 0);
  std::vector<int> values(num_values);
  for (int i = 0; i< num_vars - slack; ++i) {
    const int index = rgen.Uniform(num_values);
    card_min[index]++;
    card_max[index]++;
  }
  for (int i = 0; i < num_values; ++i) {
    values[i] = offset + i;
  }
  for (int i = 0; i < 2 * slack; ++i) {
    card_max[rgen.Uniform(num_values)]++;
  }

  LOG(INFO) << kConstraintName[type] << " constraint";
  LOG(INFO) << "  - num variables = " << num_vars;
  LOG(INFO) << "  - num values = " << num_values;
  LOG(INFO) << "  - slack = " << slack;
  LOG(INFO) << "  - seed = " << seed;
  {
    std::string tmp;
    for (const auto& it : card_min)
      tmp += ' ' + it;
    LOG(INFO) << "  - card_min = [" << tmp << "]";
  }
  {
    std::string tmp;
    for (const auto& it : card_max)
      tmp += ' ' + it;
    LOG(INFO) << "  - card_max = [" << tmp << "]";
  }

  Solver solver("TestGcc");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(num_vars, offset, offset + num_values - 1, "v", &vars);
  switch (type) {
    case 0:
      solver.AddConstraint(
          solver.MakeDistribute(vars, values, card_min, card_max));
      break;
    case 1:
      solver.AddConstraint(MakeGcc(&solver, vars, offset, card_min, card_max));
      break;
    case 2:
      solver.AddConstraint(MakeSoftGcc(&solver,
                                       vars,
                                       offset,
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
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  const int dis = operations_research::TestGcc(FLAGS_vars,
                                               FLAGS_values,
                                               FLAGS_slack,
                                               FLAGS_seed,
                                               0,
                                               FLAGS_offset);
  const int gcc = operations_research::TestGcc(FLAGS_vars,
                                               FLAGS_values,
                                               FLAGS_slack,
                                               FLAGS_seed,
                                               1,
                                               FLAGS_offset);
  const int soft = operations_research::TestGcc(FLAGS_vars,
                                                FLAGS_values,
                                                FLAGS_slack,
                                                FLAGS_seed,
                                                2, FLAGS_offset);
  if (gcc != dis && gcc != soft && dis == soft) {
    LOG(INFO) << "Problem with vars = " << FLAGS_vars
              << ", and values = " << FLAGS_values
              << ", seed = " << FLAGS_seed
              << ", slack = " << FLAGS_slack;
  }
  return 0;
}
