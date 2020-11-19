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

#ifndef OR_TOOLS_SAT_PROBING_H_
#define OR_TOOLS_SAT_PROBING_H_

#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

class Prober {
 public:
  explicit Prober(Model* model);

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
  bool ProbeBooleanVariables(double deterministic_time_limit,
                             bool log_info = false);

  // Same as above method except it probes only on the variables given in
  // 'bool_vars'.
  bool ProbeBooleanVariables(double deterministic_time_limit,
                             absl::Span<const BooleanVariable> bool_vars,
                             bool log_info = false);

  bool ProbeOneVariable(BooleanVariable b);

 private:
  bool ProbeOneVariableInternal(BooleanVariable b);

  // Model owned classes.
  const Trail& trail_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  ImpliedBounds* implied_bounds_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  BinaryImplicationGraph* implication_graph_;

  // To detect literal x that must be true because b => x and not(b) => x.
  // When probing b, we add all propagated literal to propagated, and when
  // probing not(b) we check if any are already there.
  SparseBitset<LiteralIndex> propagated_;

  // Modifications found during probing.
  std::vector<Literal> to_fix_at_true_;
  std::vector<IntegerLiteral> new_integer_bounds_;
  std::vector<std::pair<Literal, Literal>> new_binary_clauses_;

  // Probing statistics.
  int num_new_holes_ = 0;
  int num_new_binary_ = 0;
  int num_new_integer_bounds_ = 0;
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
                               bool log_info = false);

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

  // There is two sligthly different implementation of the tree-look algo.
  //
  // TODO(user): Decide which one is better, currently the difference seems
  // small but the queue seems slightly faster.
  bool use_queue = true;

  // If we detect as we probe that a new binary clause subsumes one of the
  // non-binary clause, we will replace the long clause by the binary one. This
  // is orthogonal to the extract_binary_clauses parameters which will add all
  // binary clauses but not neceassirly check for subsumption.
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
bool FailedLiteralProbingRound(ProbingOptions options, Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PROBING_H_
