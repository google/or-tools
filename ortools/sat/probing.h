// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_PROBING_H_
#define ORTOOLS_SAT_PROBING_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

class TrailCopy;

class Prober {
 public:
  explicit Prober(Model* model);
  ~Prober();

  // Fixes Booleans variables to true/false and see what is propagated. This
  // can:
  //
  // - Fix some Boolean variables (if we reach a conflict while probing).
  //
  // - Infer new direct implications. We add them directly to the
  //   BinaryImplicationGraph and they can later be used to detect equivalent
  //   literals, expand at most ones clique, etc...
  //
  // - Tighten the bounds of integer variables. If we probe the two possible
  //   values of a Boolean (b=0 and b=1), we get for each integer variables two
  //   propagated domain D_0 and D_1. The level zero domain can then be
  //   intersected with D_0 U D_1. This can restrict the lower/upper bounds of a
  //   variable, but it can also create holes in the domain! This will detect
  //   common cases like an integer variable in [0, 10] that actually only take
  //   two values [0] or [10] depending on one Boolean.
  //
  // Returns false if the problem was proved INFEASIBLE during probing.
  //
  // TODO(user): For now we process the Boolean in their natural order, this is
  // not the most efficient.
  //
  // TODO(user): This might generate a lot of new direct implications. We might
  // not want to add them directly to the BinaryImplicationGraph and could
  // instead use them directly to detect equivalent literal like in
  // ProbeAndFindEquivalentLiteral(). The situation is not clear.
  //
  // TODO(user): More generally, we might want to register any literal => bound
  // in the IntegerEncoder. This would allow to remember them and use them in
  // other part of the solver (cuts, lifting, ...).
  //
  // TODO(user): Rename to include Integer in the name and distinguish better
  // from FailedLiteralProbing() below.
  bool ProbeBooleanVariables(double deterministic_time_limit);

  // Same as above method except it probes only on the variables given in
  // 'bool_vars'.
  bool ProbeBooleanVariables(double deterministic_time_limit,
                             absl::Span<const BooleanVariable> bool_vars);

  bool ProbeOneVariable(BooleanVariable b);

  // Probes the given problem DNF (disjunction of conjunctions). Since one of
  // the conjunction must be true, we might be able to fix literal or improve
  // integer bounds if all conjunction propagate the same thing.
  enum DnfType {
    // DNF is an existing clause 'dnf_clause' = (l1) OR ... (ln), minus its
    // literals which are already assigned.
    kAtLeastOne,
    // DNF is the tautology "either at least one of n literals is true, or all
    // of them are false": (l1) OR ... (ln) OR (not(l1) AND ... not(ln)). The
    // single literal conjunctions must be listed first.
    kAtLeastOneOrZero,
    // DNF is the tautology "one of the 2^n possible assignments of n Boolean
    // variables is true". The n variables must be in the same order in each
    // conjunction, and their assignment in the i-th conjunction must be the
    // binary representation of i. For instance, if the variables are b0 and b1,
    // the conjunctions must be (not(b0) AND not(b1)), (not(b0) AND b1),
    // (b0 AND not(b1)), and (b0 AND b1), in this order.
    kAtLeastOneCombination,
  };
  bool ProbeDnf(absl::string_view name,
                absl::Span<const std::vector<Literal>> dnf, DnfType type,
                const SatClause* dnf_clause = nullptr);

  // Statistics.
  // They are reset each time ProbleBooleanVariables() is called.
  // Note however that we do not reset them on a call to ProbeOneVariable().
  int num_decisions() const { return num_decisions_; }
  int num_new_literals_fixed() const { return num_new_literals_fixed_; }
  int num_new_binary_clauses() const { return num_new_binary_; }

  // Register a callback that will be called on each "propagation".
  // One can inspect the VariablesAssignment to see what are the inferred
  // literals.
  void SetPropagationCallback(std::function<void(Literal decision)> f) {
    callback_ = f;
  }

 private:
  bool ProbeOneVariableInternal(BooleanVariable b);

  // Computes the LRAT proofs that all the `propagated_literals` can be fixed to
  // true, and fixes them.
  bool FixProbedDnfLiterals(
      absl::Span<const std::vector<Literal>> dnf,
      const absl::btree_set<LiteralIndex>& propagated_literals, DnfType type,
      const SatClause* dnf_clause);

  // Computes the LRAT proof that `propagated_lit` can be fixed to true, and
  // fixes it. `conjunctions` must have the property described for
  // DnfType::kAtLeastOneCombination. `clauses` must contain the LRAT clauses
  // "conjunctions[i] => propagated_lit" (some clauses can be kNullClausePtr, if
  // a conjunction contains `propagated_lit`). Deletes all `clauses` and
  // replaces these with kNullClausePtr values.
  bool FixLiteralImpliedByAnAtLeastOneCombinationDnf(
      absl::Span<const std::vector<Literal>> conjunctions,
      absl::Span<ClausePtr> clauses, Literal propagated_lit);

  // Model owned classes.
  const Trail& trail_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  ImpliedBounds* implied_bounds_;
  ProductDetector* product_detector_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  BinaryImplicationGraph* implication_graph_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  TrailCopy* trail_copy_;

  // To detect literal x that must be true because b => x and not(b) => x.
  // When probing b, we add all propagated literal to propagated, and when
  // probing not(b) we check if any are already there.
  SparseBitset<LiteralIndex> propagated_;

  // Modifications found during probing.
  std::vector<Literal> to_fix_at_true_;
  std::vector<IntegerLiteral> new_integer_bounds_;
  std::vector<Literal> new_literals_implied_by_decision_;
  std::vector<Literal> new_implied_or_fixed_literals_;
  absl::btree_set<LiteralIndex> new_propagated_literals_;
  absl::btree_set<LiteralIndex> always_propagated_literals_;
  absl::btree_map<IntegerVariable, IntegerValue> new_propagated_bounds_;
  absl::btree_map<IntegerVariable, IntegerValue> always_propagated_bounds_;

  std::vector<ClausePtr> tmp_proof_;
  std::vector<Literal> tmp_literals_;
  CompactVectorVector<int, ClausePtr> tmp_dnf_clauses_;

  // Probing statistics.
  int num_decisions_ = 0;
  int num_new_holes_ = 0;
  int num_new_binary_ = 0;
  int num_new_integer_bounds_ = 0;
  int num_new_literals_fixed_ = 0;
  int num_lrat_clauses_ = 0;
  int num_lrat_proof_clauses_ = 0;
  int num_unneeded_lrat_clauses_ = 0;

  std::function<void(Literal decision)> callback_ = nullptr;

  // Logger.
  SolverLogger* logger_;
};

// Try to randomly tweak the search and stop at the first conflict each time.
// This can sometimes find feasible solution, but more importantly, it is a form
// of probing that can sometimes find small and interesting conflicts or fix
// variables. This seems to work well on the SAT14/app/rook-* problems and
// do fix more variables if run before probing.
//
// If a feasible SAT solution is found (i.e. all Boolean assigned), then this
// abort and leave the solver with the full solution assigned.
//
// Returns false iff the problem is UNSAT.
bool LookForTrivialSatSolution(double deterministic_time_limit, Model* model,
                               SolverLogger* logger);

// Options for the FailedLiteralProbing() code below.
//
// A good reference for the algorithms involved here is the paper "Revisiting
// Hyper Binary Resolution" Marijn J. H. Heule, Matti Jarvisalo, Armin Biere,
// http://www.cs.utexas.edu/~marijn/cpaior2013.pdf
struct ProbingOptions {
  // The probing will consume all this deterministic time or stop if nothing
  // else can be deduced and everything has been probed until fix-point. The
  // fix point depend on the extract_binay_clauses option:
  // - If false, we will just stop when no more failed literal can be found.
  // - If true, we will do more work and stop when all failed literal have been
  //   found and all hyper binary resolution have been performed.
  //
  // TODO(user): We can also provide a middle ground and probe all failed
  // literal but do not extract all binary clauses.
  //
  // Note that the fix-point is unique, modulo the equivalent literal detection
  // we do. And if we add binary clauses, modulo the transitive reduction of the
  // binary implication graph.
  //
  // To be fast, we only use the binary clauses in the binary implication graph
  // for the equivalence detection. So the power of the equivalence detection
  // changes if the extract_binay_clauses option is true or not.
  //
  // TODO(user): The fix point is not yet reached since we don't currently
  // simplify non-binary clauses with these equivalence, but we will.
  double deterministic_limit = 1.0;

  // This is also called hyper binary resolution. Basically, we make sure that
  // the binary implication graph is augmented with all the implication of the
  // form a => b that can be derived by fixing 'a' at level zero and doing a
  // propagation using all constraints. Note that we only add clauses that
  // cannot be derived by the current implication graph.
  //
  // With these extra clause the power of the equivalence literal detection
  // using only the binary implication graph with increase. Note that it is
  // possible to do exactly the same thing without adding these binary clause
  // first. This is what is done by yet another probing algorithm (currently in
  // simplification.cc).
  //
  // TODO(user): Note that adding binary clause before/during the SAT presolve
  // is currently not always a good idea. This is because we don't simplify the
  // other clause as much as we could. Also, there can be up to a quadratic
  // number of clauses added this way, which might slow down things a lot. But
  // then because of the deterministic limit, we usually cannot add too much
  // clauses, even for huge problems, since we will reach the limit before that.
  bool extract_binary_clauses = false;

  // Use a version of the "Tree look" algorithm as explained in the paper above.
  // This is usually faster and more efficient. Note that when extracting binary
  // clauses it might currently produce more "redundant" one in the sense that a
  // transitive reduction of the binary implication graph after all hyper binary
  // resolution have been performed may need to do more work.
  bool use_tree_look = true;

  // There is two slightly different implementation of the tree-look algo.
  //
  // TODO(user): Decide which one is better, currently the difference seems
  // small but the queue seems slightly faster.
  bool use_queue = true;

  // If we detect as we probe that a new binary clause subsumes one of the
  // non-binary clause, we will replace the long clause by the binary one. This
  // is orthogonal to the extract_binary_clauses parameters which will add all
  // binary clauses but not necessarily check for subsumption.
  bool subsume_with_binary_clause = true;

  // We assume this is also true if --v 1 is activated.
  bool log_info = false;

  std::string ToString() const {
    return absl::StrCat("deterministic_limit: ", deterministic_limit,
                        " extract_binary_clauses: ", extract_binary_clauses,
                        " use_tree_look: ", use_tree_look,
                        " use_queue: ", use_queue);
  }
};

// Similar to ProbeBooleanVariables() but different :-)
//
// First, this do not consider integer variable. It doesn't do any disjunctive
// reasoning (i.e. changing the domain of an integer variable by intersecting
// it with the union of what happen when x is fixed and not(x) is fixed).
//
// However this should be more efficient and just work better for pure Boolean
// problems. On integer problems, we might also want to run this one first,
// and then do just one quick pass of ProbeBooleanVariables().
//
// Note that this by itself just do one "round", look at the code in the
// Inprocessing class that call this interleaved with other reductions until a
// fix point is reached.
//
// This can fix a lot of literals via failed literal detection, that is when
// we detect that x => not(x) via propagation after taking x as a decision. It
// also use the strongly connected component algorithm to detect equivalent
// literals.
//
// It will add any detected binary clause (via hyper binary resolution) to
// the implication graph. See the option comments for more details.
class FailedLiteralProbing {
 public:
  explicit FailedLiteralProbing(Model* model);

  bool DoOneRound(ProbingOptions options);

 private:
  struct SavedNextLiteral {
    LiteralIndex literal_index;  // kNoLiteralIndex if we need to backtrack.
    int rank;  // Cached position_in_order, we prefer lower positions.

    bool operator<(const SavedNextLiteral& o) const { return rank < o.rank; }
  };

  // Returns true if we can skip this candidate decision.
  // This factor out some code used by the functions below.
  bool SkipCandidate(Literal last_decision, Literal candidate);

  // Sets `next_decision` to the unassigned literal which implies the last
  // decision and which comes first in the probing order (which itself can be
  // the topological order of the implication graph, or the reverse).
  bool ComputeNextDecisionInOrder(LiteralIndex& next_decision);

  // Sets `next_decision` to the first unassigned literal we find which implies
  // the last decision, in no particular order.
  bool GetNextDecisionInNoParticularOrder(LiteralIndex& next_decision);

  // Sets `next_decision` to the first unassigned literal in probing_order (if
  // there is no last decision we can use any literal as first decision).
  bool GetFirstDecision(LiteralIndex& next_decision);

  // Enqueues `next_decision`. Backjumps and sets `next_decision` to false in
  // case of conflict. Returns false if the problem was proved UNSAT.
  bool EnqueueDecisionAndBackjumpOnConflict(LiteralIndex next_decision,
                                            bool use_queue,
                                            int& first_new_trail_index);

  // If we can extract a binary clause that subsume the reason clause, we do add
  // the binary and remove the subsumed clause.
  //
  // TODO(user): We could be slightly more generic and subsume some clauses that
  // do not contain last_decision.Negated().
  void MaybeSubsumeWithBinaryClause(Literal last_decision, Literal l);

  // Functions to add "last_decision => l" to the repository if not already
  // done. The Maybe() version just calls Extract() if ShouldExtract() is true.
  bool ShouldExtractImplication(Literal l);
  void ExtractImplication(Literal last_decision, Literal l,
                          bool lrat_only = false);
  void MaybeExtractImplication(Literal last_decision, Literal l);

  // Extracts an implication "`last_decision` => l" for each literal l in
  // `literals`. This is more efficient than calling ExtractImplication() for
  // each literal when LRAT is enabled.
  void ExtractImplications(Literal last_decision,
                           absl::Span<const Literal> literals);

  // Inspect the watcher list for last_decision, If we have a blocking
  // literal at true (implied by last decision), then we have subsumptions.
  //
  // The intuition behind this is that if a binary clause (a,b) subsume a
  // clause, and we watch a.Negated() for this clause with a blocking
  // literal b, then this watch entry will never change because we always
  // propagate binary clauses first and the blocking literal will always be
  // true. So after many propagations, we hope to have such configuration
  // which is quite cheap to test here.
  void SubsumeWithBinaryClauseUsingBlockingLiteral(Literal last_decision);

  // Adds 'not(literal)' to `to_fix_`, assuming that 'literal' directly implies
  // the current decision, which itself implies all the previous decisions, with
  // some of them propagating 'not(literal)'.
  void AddFailedLiteralToFix(Literal literal);

  // Fixes all the literals in to_fix_, and finish propagation.
  bool ProcessLiteralsToFix();

  // Deletes the temporary LRAT clauses in trail_implication_clauses_ for all
  // trail indices greater than the current trail index.
  void DeleteTemporaryLratImplicationsAfterBacktrack();
  void DeleteTemporaryLratImplicationsStartingFrom(int trail_index);

  SatSolver* sat_solver_;
  BinaryImplicationGraph* implication_graph_;
  TimeLimit* time_limit_;
  const Trail& trail_;
  const VariablesAssignment& assignment_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  int binary_propagator_id_;
  int clause_propagator_id_;

  int num_variables_;
  std::vector<LiteralIndex> probing_order_;
  int order_index_ = 0;
  SparseBitset<LiteralIndex> processed_;

  // This is only needed when options.use_queue is true.
  std::vector<SavedNextLiteral> queue_;
  util_intops::StrongVector<LiteralIndex, int> position_in_order_;

  // This is only needed when options use_queue is false;
  util_intops::StrongVector<LiteralIndex, int> starts_;

  // We delay fixing of already assigned literals once we go back to level 0.
  std::vector<Literal> to_fix_;
  // The literals for which we want to extract "last_decision => l" clauses.
  std::vector<Literal> binary_clauses_to_extract_;

  // For each literal 'l' in the trail, whether a binary clause "d => l" has
  // been extracted, with 'd' the decision at the same level as 'l'.
  std::vector<bool> binary_clause_extracted_;

  // For each literal on the trail, the LRAT clause stating that this literal is
  // implied by the previous decisions on the trail (or kNullClausePtr if there
  // is no such clause). Clause pointers corresponding to SatClause* denote
  // temporary LRAT clauses (i.e., which are not extracted binary clauses).
  std::vector<ClausePtr> trail_implication_clauses_;

  // Temporary data structures used for LRAT proofs.
  std::vector<ClausePtr> tmp_proof_;
  SparseBitset<BooleanVariable> tmp_mark_;
  std::vector<int> tmp_heap_;
  std::vector<Literal> tmp_marked_literals_;

  // Stats.
  int64_t num_probed_ = 0;
  int64_t num_explicit_fix_ = 0;
  int64_t num_conflicts_ = 0;
  int64_t num_new_binary_ = 0;
  int64_t num_subsumed_ = 0;
  int64_t num_lrat_clauses_ = 0;
  int64_t num_lrat_proof_clauses_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_PROBING_H_
