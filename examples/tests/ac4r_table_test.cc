// Copyright 2011-2012 Jean Charles Régin
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

DEFINE_int32(arity, 3, "Arity of tuples");
DEFINE_int32(upper, 10, "Upper bound of variables, lower is always 0");
DEFINE_int32(tuples, 1000, "Number of tuples");
DEFINE_int32(bucket, 64, "Size of buckets");
DEFINE_bool(ac4, false, "Use AC4 Table only");

namespace operations_research {
extern Constraint* BuildAc4TableConstraint(Solver* const solver,
                                           const IntTupleSet& tuples,
                                           const std::vector<IntVar*>& vars);

void RandomFillTable(int num_tuples,
                     int64 lower,
                     int64 upper,
                     IntTupleSet* const tuples) {
  ACMRandom rgen(0); // defines a random generator
  std::vector<int64> vals(tuples->Arity());

  for(int t = 0; t < num_tuples; ++t){
    for(int i = 0; i < tuples->Arity(); i++){
      int64 rnumber = rgen.Next64(); // generates a 64bits number
      vals[i] = (rnumber % (upper - lower + 1)) + lower;
    }
    tuples->Insert(vals);
  }
}

void TestTable(int arity, int num_tuples, int upper, bool use_ac4r_table) {
  if (use_ac4r_table) {
    LOG(INFO) <<  "Creation of a AC4-Regin tuple Table with :";
  } else {
    LOG(INFO) << "Creation of a Allowed Assignment Table with :";
  }
  LOG(INFO) << " - " << arity <<  " variables";
  LOG(INFO) << " - " << upper + 1 <<  " values per domain";
  LOG(INFO) << " - " << num_tuples << " tuples";

  Solver solver("SolverInBk");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(arity, 0, upper, &vars);

  IntTupleSet table(arity);
  RandomFillTable(num_tuples, 0, upper, &table);
  LOG(INFO) << "Table is created";

  Constraint* const ct = use_ac4r_table ?
      BuildAc4TableConstraint(&solver, table, vars) :
      solver.MakeAllowedAssignments(vars, table);
  solver.AddConstraint(ct);

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
  CHECK_EQ(counter, table.NumTuples());
}
}  // namespace operations_research


int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (!FLAGS_ac4) {
    operations_research::TestTable(FLAGS_arity,
                                   FLAGS_tuples,
                                   FLAGS_upper,
                                   false);
  }
  operations_research::TestTable(FLAGS_arity,
                                 FLAGS_tuples,
                                 FLAGS_upper,
                                 true);
  return 0;
}

