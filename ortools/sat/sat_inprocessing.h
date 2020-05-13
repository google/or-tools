// Copyright 2010-2018 Google LLC
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

// This file contains the entry point for our presolve/inprocessing code.
//
// TODO(user): for now it is mainly presolve, but the idea is to call these
// function during the search so they should be as incremental as possible. That
// is avoid doing work that is not useful because nothing changed or exploring
// parts that were not done during the last round.

#ifndef OR_TOOLS_SAT_SAT_INPROCESSING_H_
#define OR_TOOLS_SAT_SAT_INPROCESSING_H_

#include "ortools/sat/clause.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

class StampingSimplifier;

struct SatPresolveOptions {
  // The time budget to spend.
  double deterministic_time_limit = 30.0;

  // We assume this is also true if --v 1 is activated and we actually log
  // even more in --v 1.
  bool log_info = false;

  // See ProbingOptions in probing.h
  bool extract_binary_clauses_in_probing = false;

  // Whether we perform a transitive reduction of the binary implication graph
  // after equivalent literal detection and before each probing pass.
  //
  // TODO(user): Doing that before the current SAT presolve also change the
  // possible reduction. This shouldn't matter if we use the binary implication
  // graph and its reachability instead of just binary clause though.
  bool use_transitive_reduction = false;
};

// We need to keep some information from one call to the next, so we use a
// class. Note that as this becomes big, we can probably split it.
//
// TODO(user): Some algorithms here use the normal SAT propagation engine.
// However we might want to temporarily disable activities/phase saving and so
// on if we want to run them as in-processing steps so that they
// do not "pollute" the normal SAT search.
//
// TODO(user): For the propagation, this depends on the SatSolver class, which
// mean we cannot really use it without some refactoring as an in-processing
// from the SatSolver::Solve() function. So we do need a special
// InprocessingSolve() that lives outside SatSolver. Alternatively, we can
// extract the propagation main loop and conflict analysis from SatSolver.
class Inprocessing {
 public:
  explicit Inprocessing(Model* model)
      : assignment_(model->GetOrCreate<Trail>()->Assignment()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<LiteralWatchers>()),
        trail_(model->GetOrCreate<Trail>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        stamping_simplifier_(model->GetOrCreate<StampingSimplifier>()),
        model_(model) {}

  // Does some simplifications until a fix point is reached or the given
  // deterministic time is passed.
  bool PresolveLoop(SatPresolveOptions options);

  // Simple wrapper that makes sure all the clauses are attached before a
  // propagation is performed.
  bool LevelZeroPropagate();

  // Detects equivalences in the implication graph and propagates any failed
  // literal found during the process.
  bool DetectEquivalencesAndStamp(bool log_info);

  // Removes fixed variables and exploit equivalence relations to cleanup the
  // clauses. Returns false if UNSAT.
  bool RemoveFixedAndEquivalentVariables(bool log_info);

  // Returns true if there is new fixed variables or new equivalence relations
  // since RemoveFixedAndEquivalentVariables() was last called.
  bool MoreFixedVariableToClean() const;
  bool MoreRedundantVariableToClean() const;

  // Processes all clauses and see if there is any subsumption/strenghtening
  // reductions that can be performed. Returns false if UNSAT.
  bool SubsumeAndStrenghtenRound(bool log_info);

 private:
  const VariablesAssignment& assignment_;
  BinaryImplicationGraph* implication_graph_;
  LiteralWatchers* clause_manager_;
  Trail* trail_;
  TimeLimit* time_limit_;
  SatSolver* sat_solver_;
  StampingSimplifier* stamping_simplifier_;

  // TODO(user): This is only used for calling probing. We should probably
  // create a Probing class to wraps its data. This will also be needed to not
  // always probe the same variables in each round if the deterministic time
  // limit is low.
  Model* model_;

  // Last since clause database was cleaned up.
  int64 last_num_redundant_literals_ = 0;
  int64 last_num_fixed_variables_ = 0;
};

// Implements "stamping" as described in "Efficient CNF Simplification based on
// Binary Implication Graphs", Marijn Heule, Matti Jarvisalo and Armin Biere.
//
// This sample the implications graph with a spanning tree, and then simplify
// all clauses (subsumption / strengthening) using the implications encoded in
// this tree. So this allows to consider chain of implications instead of just
// direct ones, but depending on the problem, only a small fraction of the
// implication graph will be captured by the tree.
//
// Note that we randomize the spanning tree at each call. This can benefit by
// having the implication graph be transitively reduced before.
class StampingSimplifier {
 public:
  explicit StampingSimplifier(Model* model)
      : implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<LiteralWatchers>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        time_limit_(model->GetOrCreate<TimeLimit>()) {}

  // This is "fast" (linear scan + sort of all clauses) so we always complete
  // the full round.
  //
  // TODO(user): To save one scan over all the clauses, we could do the fixed
  // and equivalence variable cleaning here too.
  bool DoOneRound(bool log_info);

  // When we compute stamps, we might detect fixed variable (via failed literal
  // probing in the implication graph). So it might make sense to do that until
  // we have dealt with all fixed literals before calling DoOneRound().
  bool ComputeStampsForNextRound(bool log_info);

  // Visible for testing.
  void SampleTreeAndFillParent();

  // Using a DFS visiting order, we can answer reachability query in O(1) on a
  // tree, this is well known. ComputeStamps() also detect failed literal in
  // the tree and fix them. It can return false on UNSAT.
  bool ComputeStamps();
  bool ImplicationIsInTree(Literal a, Literal b) const {
    return first_stamps_[a.Index()] < first_stamps_[b.Index()] &&
           last_stamps_[b.Index()] < last_stamps_[a.Index()];
  }

  bool ProcessClauses();

 private:
  BinaryImplicationGraph* implication_graph_;
  LiteralWatchers* clause_manager_;
  ModelRandomGenerator* random_;
  TimeLimit* time_limit_;

  // For ComputeStampsForNextRound().
  bool stamps_are_already_computed_ = false;

  // Reset at each round.
  double dtime_ = 0.0;
  int64 num_subsumed_clauses_ = 0;
  int64 num_removed_literals_ = 0;
  int64 num_fixed_ = 0;

  // Encode a spanning tree of the implication graph.
  gtl::ITIVector<LiteralIndex, LiteralIndex> parents_;

  // Adjacency list representation of the parents_ tree.
  gtl::ITIVector<LiteralIndex, int> sizes_;
  gtl::ITIVector<LiteralIndex, int> starts_;
  std::vector<LiteralIndex> children_;

  // Temporary data for the DFS.
  gtl::ITIVector<LiteralIndex, bool> marked_;
  std::vector<LiteralIndex> dfs_stack_;

  // First/Last visited index in a DFS of the tree above.
  gtl::ITIVector<LiteralIndex, int> first_stamps_;
  gtl::ITIVector<LiteralIndex, int> last_stamps_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_INPROCESSING_H_
