// Copyright 2010-2022 Google LLC
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

#include <cstdint>
#include <deque>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/integer_pq.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// The order is important and each clauses has a "special" literal that is
// put first.
//
// TODO(user): Use a flat memory structure instead.
struct PostsolveClauses {
  // Utility function that push back clause but also make sure the given literal
  // from clause appear first.
  void AddClauseWithSpecialLiteral(Literal literal,
                                   absl::Span<const Literal> clause);

  std::deque<std::vector<Literal>> clauses;
};

class BlockedClauseSimplifier;
class BoundedVariableElimination;
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
        decision_policy_(model->GetOrCreate<SatDecisionPolicy>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        stamping_simplifier_(model->GetOrCreate<StampingSimplifier>()),
        blocked_clause_simplifier_(
            model->GetOrCreate<BlockedClauseSimplifier>()),
        bounded_variable_elimination_(
            model->GetOrCreate<BoundedVariableElimination>()),
        model_(model) {}

  // Does some simplifications until a fix point is reached or the given
  // deterministic time is passed.
  bool PresolveLoop(SatPresolveOptions options);

  // When the option use_sat_inprocessing is true, then this is called at each
  // restart. It is up to the heuristics here to decide what inprocessing we
  // do or if we wait for the next call before doing anything.
  bool InprocessingRound();

  // Simple wrapper that makes sure all the clauses are attached before a
  // propagation is performed.
  bool LevelZeroPropagate();

  // Detects equivalences in the implication graph and propagates any failed
  // literal found during the process.
  bool DetectEquivalencesAndStamp(bool use_transitive_reduction, bool log_info);

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
  SatDecisionPolicy* decision_policy_;
  TimeLimit* time_limit_;
  SatSolver* sat_solver_;
  StampingSimplifier* stamping_simplifier_;
  BlockedClauseSimplifier* blocked_clause_simplifier_;
  BoundedVariableElimination* bounded_variable_elimination_;

  double total_dtime_ = 0.0;

  // TODO(user): This is only used for calling probing. We should probably
  // create a Probing class to wraps its data. This will also be needed to not
  // always probe the same variables in each round if the deterministic time
  // limit is low.
  Model* model_;

  // Last since clause database was cleaned up.
  int64_t last_num_redundant_literals_ = 0;
  int64_t last_num_fixed_variables_ = 0;
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
      : assignment_(model->GetOrCreate<Trail>()->Assignment()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
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
  const VariablesAssignment& assignment_;
  BinaryImplicationGraph* implication_graph_;
  LiteralWatchers* clause_manager_;
  ModelRandomGenerator* random_;
  TimeLimit* time_limit_;

  // For ComputeStampsForNextRound().
  bool stamps_are_already_computed_ = false;

  // Reset at each round.
  double dtime_ = 0.0;
  int64_t num_subsumed_clauses_ = 0;
  int64_t num_removed_literals_ = 0;
  int64_t num_fixed_ = 0;

  // Encode a spanning tree of the implication graph.
  absl::StrongVector<LiteralIndex, LiteralIndex> parents_;

  // Adjacency list representation of the parents_ tree.
  absl::StrongVector<LiteralIndex, int> sizes_;
  absl::StrongVector<LiteralIndex, int> starts_;
  std::vector<LiteralIndex> children_;

  // Temporary data for the DFS.
  absl::StrongVector<LiteralIndex, bool> marked_;
  std::vector<LiteralIndex> dfs_stack_;

  // First/Last visited index in a DFS of the tree above.
  absl::StrongVector<LiteralIndex, int> first_stamps_;
  absl::StrongVector<LiteralIndex, int> last_stamps_;
};

// A clause c is "blocked" by a literal l if all clauses containing the
// negation of l resolve to trivial clause with c. Blocked clause can be
// simply removed from the problem. At postsolve, if a blocked clause is not
// satisfied, then l can simply be set to true without breaking any of the
// clause containing not(l).
//
// See the paper "Blocked Clause Elimination", Matti Jarvisalo, Armin Biere,
// and Marijn Heule.
//
// TODO(user): This requires that l only appear in clauses and not in the
// integer part of CP-SAT.
class BlockedClauseSimplifier {
 public:
  explicit BlockedClauseSimplifier(Model* model)
      : assignment_(model->GetOrCreate<Trail>()->Assignment()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<LiteralWatchers>()),
        postsolve_(model->GetOrCreate<PostsolveClauses>()),
        time_limit_(model->GetOrCreate<TimeLimit>()) {}

  void DoOneRound(bool log_info);

 private:
  void InitializeForNewRound();
  void ProcessLiteral(Literal current_literal);
  bool ClauseIsBlocked(Literal current_literal,
                       absl::Span<const Literal> clause);

  const VariablesAssignment& assignment_;
  BinaryImplicationGraph* implication_graph_;
  LiteralWatchers* clause_manager_;
  PostsolveClauses* postsolve_;
  TimeLimit* time_limit_;

  double dtime_ = 0.0;
  int32_t num_blocked_clauses_ = 0;
  int64_t num_inspected_literals_ = 0;

  // Temporary vector to mark literal of a clause.
  absl::StrongVector<LiteralIndex, bool> marked_;

  // List of literal to process.
  // TODO(user): use priority queue?
  absl::StrongVector<LiteralIndex, bool> in_queue_;
  std::deque<Literal> queue_;

  // We compute the occurrence graph just once at the beginning of each round
  // and we do not shrink it as we remove blocked clauses.
  DEFINE_STRONG_INDEX_TYPE(rat_literal_clause_index);
  absl::StrongVector<ClauseIndex, SatClause*> clauses_;
  absl::StrongVector<LiteralIndex, std::vector<ClauseIndex>>
      literal_to_clauses_;
};

class BoundedVariableElimination {
 public:
  explicit BoundedVariableElimination(Model* model)
      : parameters_(*model->GetOrCreate<SatParameters>()),
        assignment_(model->GetOrCreate<Trail>()->Assignment()),
        implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
        clause_manager_(model->GetOrCreate<LiteralWatchers>()),
        postsolve_(model->GetOrCreate<PostsolveClauses>()),
        trail_(model->GetOrCreate<Trail>()),
        time_limit_(model->GetOrCreate<TimeLimit>()) {}

  bool DoOneRound(bool log_info);

 private:
  int NumClausesContaining(Literal l);
  void UpdatePriorityQueue(BooleanVariable var);
  bool CrossProduct(BooleanVariable var);
  void DeleteClause(SatClause* sat_clause);
  void DeleteAllClausesContaining(Literal literal);
  void AddClause(absl::Span<const Literal> clause);
  bool RemoveLiteralFromClause(Literal lit, SatClause* sat_clause);
  bool Propagate();

  // The actual clause elimination algo. We have two versions, one just compute
  // the "score" of what will be the final state. The other perform the
  // resolution, remove old clauses and add the new ones.
  //
  // Returns false on UNSAT.
  template <bool score_only, bool with_binary_only>
  bool ResolveAllClauseContaining(Literal lit);

  const SatParameters& parameters_;
  const VariablesAssignment& assignment_;
  BinaryImplicationGraph* implication_graph_;
  LiteralWatchers* clause_manager_;
  PostsolveClauses* postsolve_;
  Trail* trail_;
  TimeLimit* time_limit_;

  int propagation_index_;

  double dtime_ = 0.0;
  int64_t num_inspected_literals_ = 0;
  int64_t num_simplifications_ = 0;
  int64_t num_blocked_clauses_ = 0;
  int64_t num_eliminated_variables_ = 0;
  int64_t num_literals_diff_ = 0;
  int64_t num_clauses_diff_ = 0;

  int64_t new_score_;
  int64_t score_threshold_;

  // Temporary vector to mark literal of a clause and compute its resolvant.
  absl::StrongVector<LiteralIndex, bool> marked_;
  std::vector<Literal> resolvant_;

  // Priority queue of variable to process.
  // We will process highest priority first.
  struct VariableWithPriority {
    BooleanVariable var;
    int32_t priority;

    // Interface for the IntegerPriorityQueue.
    int Index() const { return var.value(); }
    bool operator<(const VariableWithPriority& o) const {
      return priority < o.priority;
    }
  };
  IntegerPriorityQueue<VariableWithPriority> queue_;

  // We update the queue_ in batch.
  absl::StrongVector<BooleanVariable, bool> in_need_to_be_updated_;
  std::vector<BooleanVariable> need_to_be_updated_;

  // We compute the occurrence graph just once at the beginning of each round.
  // We maintains the sizes at all time and lazily shrink the graph with deleted
  // clauses.
  DEFINE_STRONG_INDEX_TYPE(ClauseIndex);
  absl::StrongVector<ClauseIndex, SatClause*> clauses_;
  absl::StrongVector<LiteralIndex, std::vector<ClauseIndex>>
      literal_to_clauses_;
  absl::StrongVector<LiteralIndex, int> literal_to_num_clauses_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_INPROCESSING_H_
