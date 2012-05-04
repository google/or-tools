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


#include "base/hash.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {
// Enums.
#define TABLECT_RESTART 0
#define TABLECT_CONTINUE 1
#define TABLECT_INVERSE 2
#define TABLECT_ORIGINAL 3
// External build functions.
extern Constraint* BuildTableCt(Solver* const solver,
                                const IntTupleSet& tuples,
                                const std::vector<IntVar*>& vars,
                                int order,
                                int type,
                                int size_bucket);

void RandomFillTable(int num_tuples, int64 lower, int64 upper, IntTupleSet* const tuples) {
  ACMRandom rgen(0); // defines a random generator
  std::vector<int64> vals(tuples->Arity());

  for(int t = 0; t < num_tuples; ++t){
    for(int i = 0; i < tuples->Arity(); i++){
      int64 rnumber=rgen.Next64(); // generates a 64bits number
      vals[i] = (rnumber % (upper - lower+1)) + lower;
    }
    tuples->Insert(vals);
  }
}

void FillMiddleTuple(const IntTupleSet& tuples, std::vector<int64>* const tuple) {
  tuple->clear();
  const int index = tuples.NumTuples() / 2;
  for (int i = 0; i < tuples.Arity(); ++i) {
    tuple->push_back(tuples.Value(index, i));
  }
}

void test_table(Solver* solver,
                int n,
                const unsigned numTuples,
                const unsigned upper,
                const unsigned sizeBucket,
                const unsigned type,
                const unsigned ord){
  // var array creation
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(n,0,upper,&vars);

  IntTupleSet table(n);

  LOG(INFO) << "Creation of a Bucketted tuple Table with";
  LOG(INFO) << "\t" << n <<  " variables";
  LOG(INFO) << "\t" << upper+1 <<  " values per domain";
  LOG(INFO) << "\t" << numTuples << " tuples";

  RandomFillTable(numTuples, 0, upper, &table);
  LOG(INFO) << "Table is created";

  Constraint* const ct = BuildTableCt(solver, table, vars, ord, type, sizeBucket);
  solver->AddConstraint(ct);
  LOG(INFO) << "The constraint has been added";

  LOG(INFO) << " Type: ";
  switch(type){
    case TABLECT_RESTART : LOG(INFO) << " RESTART"; break;
    case TABLECT_CONTINUE : LOG(INFO) << " CONTINUE"; break;
    case TABLECT_INVERSE : LOG(INFO) << " INVERSE";  break;
    case TABLECT_ORIGINAL : LOG(INFO) << " ORIGINAL";  break;
    default : break;
  };

  if (ord ==0) LOG(INFO) << " Order: none";
  if (ord ==1) LOG(INFO) << " Order: domain min";
  if (ord ==2) LOG(INFO) << " Order: conflicts max";

  std::vector<int64> vals(n);
  FillMiddleTuple(table, &vals);

  CycleTimer t;
  t.Start();
  for(int x=n-1;x >=0;x--){
    IntVarIterator* it=vars[x]->MakeDomainIterator(false);
    for(it->Init();it->Ok();it->Next()){ // We traverse the domain of x
      int64 val=it->Value();
      if (vals[x] != val){
        vars[x]->RemoveValue(val);
      }
    }
  }
  t.Stop();

  LOG(INFO) << "a solution is found";
  LOG(INFO) << "test time : " << t.GetInUsec() << " micro seconds";
}

class MyTestDb : public DecisionBuilder {
 public:
  MyTestDb(int n,
           int numTuples,
           int upper,
           int sizeBucket,
           int type,
           int ord)
      : n_(n),
        numTuples_(numTuples),
        upper_(upper),
        sizeBucket_(sizeBucket),
        type_(type),
        ord_(ord) {}

  virtual ~MyTestDb() {}

  virtual Decision* Next(Solver* const s) {
    test_table(s,
               n_,
               numTuples_,
               upper_,
               sizeBucket_,
               type_,
               ord_);
    return NULL;
  }

 private:
  const int n_;
  const int numTuples_;
  const int upper_;
  const int sizeBucket_;
  const int type_;
  const int ord_;
};

void test_table_in_bk(int n,
                      const unsigned numTuples,
                      const unsigned upper,
                      const unsigned sizeBucket,
                      const unsigned type,
                      const unsigned ord){
  // var array creation
  Solver solver("SolverInBk");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(n,0,upper,&vars);

  IntTupleSet table(n);

  LOG(INFO) << "Creation of a Bucketted tuple Table with";
  LOG(INFO) << "\t" << n <<  " variables";
  LOG(INFO) << "\t" << upper+1 <<  " values per domain";
  LOG(INFO) << "\t" << numTuples << " tuples";

  RandomFillTable(numTuples, 0, upper, &table);

  LOG(INFO) << "Table is created";

  Constraint* const ct = BuildTableCt(&solver, table, vars, ord, type, sizeBucket);
  solver.AddConstraint(ct);

  LOG(INFO) << "The constraint has been added";

  LOG(INFO) << " Type: ";
  switch(type){
    case TABLECT_RESTART : LOG(INFO) << " RESTART"; break;
    case TABLECT_CONTINUE : LOG(INFO) << " CONTINUE"; break;
    case TABLECT_INVERSE : LOG(INFO) << " INVERSE";  break;
    case TABLECT_ORIGINAL : LOG(INFO) << " ORIGINAL";  break;
    default : break;
  };

  if (ord ==0) LOG(INFO) << " Order: none";
  if (ord ==1) LOG(INFO) << " Order: domain min";
  if (ord ==2) LOG(INFO) << " Order: conflicts max";

  DecisionBuilder* const db =
      solver.MakePhase(vars,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);

  CycleTimer t;
  t.Start();
  while(solver.NextSolution()){
    LOG(INFO) << "Solution: ";
    for(int i =0;i<n;i++){
      LOG(INFO) << "(" << i << "," << vars[i]->Value() << ") ";
    }
    LOG(INFO);
  }
  t.Stop();
  solver.EndSearch();
  LOG(INFO) << "test time : " << t.GetInUsec() << " micro seconds";
}
}  // namespace operations_research


int main(int argc, char** argv) {
  int n = (1 < argc) ? atoi(argv[1]) : 3; //10;
  int upper = (2 < argc) ? atoi(argv[2]) : 3;//10;
  int numTuples = (3 < argc) ? atoi(argv[3]) : 15;//500;
  int sizeBucket=(4 < argc) ? atoi(argv[4]) : (upper+1)/2;
  int type=(5 < argc) ? atoi(argv[5]) : 0;
  int ord=(6 < argc) ? atoi(argv[6]) : 0;

  LOG(INFO) << "n: " << n << " numTuples: " << numTuples << " values from 0 to "
            << upper << " bucketSize: " << sizeBucket << " type: " << type
            << " ord: " << ord;

  /*	operations_research::Solver solver("test");
	operations_research::DecisionBuilder* const db = solver.RevAlloc(new operations_research::MyTestDb(n,numTuples,upper,sizeBucket,type,ord));
	solver.Solve(db);
  */
  operations_research::test_table_in_bk(n, numTuples, upper, sizeBucket, type, ord);
  return 0;
}

