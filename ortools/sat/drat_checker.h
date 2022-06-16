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

#ifndef OR_TOOLS_SAT_DRAT_CHECKER_H_
#define OR_TOOLS_SAT_DRAT_CHECKER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Index of a clause (>= 0).
DEFINE_STRONG_INDEX_TYPE(ClauseIndex);
const ClauseIndex kNoClauseIndex(-1);

// DRAT is a SAT proof format that allows a simple program to check that a
// problem is really UNSAT. The description of the format and a checker are
// available at http://www.cs.utexas.edu/~marijn/drat-trim/. This class checks
// that a DRAT proof is valid.
//
// Note that DRAT proofs are often huge (can be GB), and can take about as much
// time to check as it takes to find the proof in the first place!
class DratChecker {
 public:
  DratChecker();
  ~DratChecker() {}

  // Returns the number of Boolean variables used in the problem and infered
  // clauses.
  int num_variables() const { return num_variables_; }

  // Adds a clause of the problem that must be checked. The problem clauses must
  // be added first, before any infered clause. The given clause must not
  // contain a literal and its negation. Must not be called after Check().
  void AddProblemClause(absl::Span<const Literal> clause);

  // Adds a clause which is infered from the problem clauses and the previously
  // infered clauses (that are have not been deleted). Infered clauses must be
  // added after the problem clauses. Clauses with the Reverse Asymmetric
  // Tautology (RAT) property for literal l must start with this literal. The
  // given clause must not contain a literal and its negation. Must not be
  // called after Check().
  void AddInferedClause(absl::Span<const Literal> clause);

  // Deletes a problem or infered clause. The order of the literals does not
  // matter. In particular, it can be different from the order that was used
  // when the clause was added. Must not be called after Check().
  void DeleteClause(absl::Span<const Literal> clause);

  // Checks that the infered clauses form a DRAT proof that the problem clauses
  // are UNSAT. For this the last added infered clause must be the empty clause
  // and each infered clause must have either the Reverse Unit Propagation (RUP)
  // or the Reverse Asymmetric Tautology (RAT) property with respect to the
  // problem clauses and the previously infered clauses which are not deleted.
  // Returns VALID if the proof is valid, INVALID if it is not, and UNKNOWN if
  // the check timed out.
  // WARNING: no new clause must be added or deleted after this method has been
  // called.
  enum Status {
    UNKNOWN,
    VALID,
    INVALID,
  };
  Status Check(double max_time_in_seconds);

  // Returns a subproblem of the original problem that is already UNSAT. The
  // result is undefined if Check() was not previously called, or did not return
  // true.
  std::vector<std::vector<Literal>> GetUnsatSubProblem() const;

  // Returns a DRAT proof that GetUnsatSubProblem() is UNSAT. The result is
  // undefined if Check() was not previously called, or did not return true.
  std::vector<std::vector<Literal>> GetOptimizedProof() const;

 private:
  // A problem or infered clause. The literals are specified as a subrange of
  // 'literals_' (namely the subrange from 'first_literal_index' to
  // 'first_literal_index' + 'num_literals' - 1), and are sorted in increasing
  // order *before Check() is called*.
  struct Clause {
    // The index of the first literal of this clause in 'literals_'.
    int first_literal_index;
    // The number of literals of this clause.
    int num_literals;

    // The clause literal to use to check the RAT property, or kNoLiteralIndex
    // for problem clauses and empty infered clauses.
    LiteralIndex rat_literal_index = kNoLiteralIndex;

    // The *current* number of copies of this clause. This number is incremented
    // each time a copy of the clause is added, and decremented each time a copy
    // is deleted. When this number reaches 0, the clause is actually marked as
    // deleted (see 'deleted_index'). If other copies are added after this
    // number reached 0, a new clause is added (because a Clause lifetime is a
    // single interval of ClauseIndex values; therefore, in order to represent a
    // lifetime made of several intervals, several Clause are used).
    int num_copies = 1;

    // The index in 'clauses_' from which this clause is deleted (inclusive).
    // For instance, with AddProblemClause(c0), AddProblemClause(c1),
    // DeleteClause(c0), AddProblemClause(c2), ... if c0's index is 0, then its
    // deleted_index is 2. Meaning that when checking a clause whose index is
    // larger than or equal to 2 (e.g. c2), c0 can be ignored.
    ClauseIndex deleted_index = ClauseIndex(std::numeric_limits<int>::max());

    // The indices of the clauses (with at least two literals) which are deleted
    // just after this clause.
    std::vector<ClauseIndex> deleted_clauses;

    // Whether this clause is actually needed to check the DRAT proof.
    bool is_needed_for_proof = false;
    // Whether this clause is actually needed to check the current step (i.e. an
    // infered clause) of the DRAT proof. This bool is always false, except in
    // MarkAsNeededForProof() that uses it temporarily.
    bool tmp_is_needed_for_proof_step = false;

    Clause(int first_literal_index, int num_literals);

    // Returns true if this clause is deleted before the given clause.
    bool IsDeleted(ClauseIndex clause_index) const;
  };

  // A literal to assign to true during boolean constraint propagation. When a
  // literal is assigned, new literals can be found that also need to be
  // assigned to true (via unit clauses). In this case they are pushed on a
  // stack of LiteralToAssign values, to be processed later on (the use of this
  // stack avoids recursive calls to the boolean constraint propagation method
  // AssignAndPropagate()).
  struct LiteralToAssign {
    // The literal that must be assigned to true.
    Literal literal;
    // The index of the clause from which this assignment was deduced. This is
    // kNoClauseIndex for the clause we are currently checking (whose literals
    // are all falsified to check if a conflict can be derived). Otherwise this
    // is the index of a unit clause with unit literal 'literal' that was found
    // during boolean constraint propagation.
    ClauseIndex source_clause_index;
  };

  // Hash function for clauses.
  struct ClauseHash {
    DratChecker* checker;
    explicit ClauseHash(DratChecker* checker) : checker(checker) {}
    std::size_t operator()(const ClauseIndex clause_index) const;
  };

  // Equality function for clauses.
  struct ClauseEquiv {
    DratChecker* checker;
    explicit ClauseEquiv(DratChecker* checker) : checker(checker) {}
    bool operator()(const ClauseIndex clause_index1,
                    const ClauseIndex clause_index2) const;
  };

  // Adds a clause and returns its index.
  ClauseIndex AddClause(absl::Span<const Literal> clause);

  // Removes the last clause added to 'clauses_'.
  void RemoveLastClause();

  // Returns the literals of the given clause in increasing order.
  absl::Span<const Literal> Literals(const Clause& clause) const;

  // Initializes the data structures used to check the DRAT proof.
  void Init();

  // Adds 2 watch literals for the given clause.
  void WatchClause(ClauseIndex clause_index);

  // Returns true if, by assigning all the literals of 'clause' to false, a
  // conflict can be found with boolean constraint propagation, using the non
  // deleted clauses whose index is strictly less than 'num_clauses'. If so,
  // marks the clauses actually used in this process as needed to check to DRAT
  // proof.
  bool HasRupProperty(ClauseIndex num_clauses,
                      absl::Span<const Literal> clause);

  // Assigns 'literal' to true in 'assignment_' (and pushes it to 'assigned_'),
  // records its source clause 'source_clause_index' in 'assignment_source_',
  // and uses the watched literals to find all the clauses (whose index is less
  // than 'num_clauses') that become unit due to this assignment. For each unit
  // clause found, pushes its unit literal on top of
  // 'high_priority_literals_to_assign_' or 'low_priority_literals_to_assign_'.
  // Returns kNoClauseIndex if no falsified clause is found, or the index of the
  // first found falsified clause otherwise.
  ClauseIndex AssignAndPropagate(ClauseIndex num_clauses, Literal literal,
                                 ClauseIndex source_clause_index);

  // Marks the given clause as needed to check the DRAT proof, as well as the
  // other clauses which were used to check this clause (these are found from
  // 'unit_stack_', containing the clauses that became unit in
  // AssignAndPropagate, and from 'assignment_source_', containing for each
  // variable the clause that caused its assignment).
  void MarkAsNeededForProof(Clause* clause);

  // Returns the clauses whose index is in [begin,end) which are needed for the
  // proof. The result is undefined if Check() was not previously called, or did
  // not return true.
  std::vector<std::vector<Literal>> GetClausesNeededForProof(
      ClauseIndex begin, ClauseIndex end) const;

  void LogStatistics(int64_t duration_nanos) const;

  // The index of the first infered clause in 'clauses_', or kNoClauseIndex if
  // there is no infered clause.
  ClauseIndex first_infered_clause_index_;

  // The problem clauses, followed by the infered clauses.
  absl::StrongVector<ClauseIndex, Clause> clauses_;

  // A content addressable set of the non-deleted clauses in clauses_. After
  // adding a clause to clauses_, this set can be used to find if the same
  // clause was previously added (i.e if a find using the new clause index
  // returns a previous index) and not yet deleted.
  absl::flat_hash_set<ClauseIndex, ClauseHash, ClauseEquiv> clause_set_;

  // All the literals used in 'clauses_'.
  std::vector<Literal> literals_;

  // The number of Boolean variables used in the clauses.
  int num_variables_;

  // ---------------------------------------------------------------------------
  // Data initialized in Init() and used in Check() to check the DRAT proof.

  // The literals that have been assigned so far (this is used to unassign them
  // after a clause has been checked, before checking the next one).
  std::vector<Literal> assigned_;

  // The current assignment values of literals_.
  VariablesAssignment assignment_;

  // For each variable, the index of the unit clause that caused its assignment,
  // or kNoClauseIndex if the variable is not assigned, or was assigned to
  // falsify the clause that is currently being checked.
  absl::StrongVector<BooleanVariable, ClauseIndex> assignment_source_;

  // The stack of literals that remain to be assigned to true during boolean
  // constraint propagation, with high priority (unit clauses which are already
  // marked as needed for the proof are given higher priority than the others
  // during boolean constraint propagation. According to 'Trimming while
  // Checking Clausal Proofs', this heuristics reduces the final number of
  // clauses that are marked as needed for the proof, and therefore the
  // verification time, in a majority of cases -- but not all).
  std::vector<LiteralToAssign> high_priority_literals_to_assign_;

  // The stack of literals that remain to be assigned to true during boolean
  // constraint propagation, with low priority (see above).
  std::vector<LiteralToAssign> low_priority_literals_to_assign_;

  // For each literal, the list of clauses in which this literal is watched.
  // Invariant 1: the literals with indices first_watched_literal_index and
  // second_watched_literal_index of each clause with at least two literals are
  // watched.
  // Invariant 2: watched literals are non-falsified if the clause is not
  // satisfied (in more details: if a clause c is contained in
  // 'watched_literals_[l]' for literal l, then either c is satisfied with
  // 'assignment_', or l is unassigned or assigned to true).
  absl::StrongVector<LiteralIndex, std::vector<ClauseIndex>> watched_literals_;

  // The list of clauses with only one literal. This is needed for boolean
  // constraint propagation, in addition to watched literals, because watched
  // literals can only be used with clauses having at least two literals.
  std::vector<ClauseIndex> single_literal_clauses_;

  // The stack of clauses that have become unit during boolean constraint
  // propagation, in HasRupProperty().
  std::vector<ClauseIndex> unit_stack_;

  // A temporary assignment, always fully unassigned except in Resolve().
  VariablesAssignment tmp_assignment_;

  // ---------------------------------------------------------------------------
  // Statistics

  // The number of infered clauses having the RAT property (but not the RUP
  // property).
  int num_rat_checks_;
};

// Returns true if the given clause contains the given literal. This works in
// O(clause.size()).
bool ContainsLiteral(absl::Span<const Literal> clause, Literal literal);

// Returns true if 'complementary_literal' is the unique complementary literal
// in the two given clauses. If so the resolvent of these clauses (i.e. their
// union with 'complementary_literal' and its negation removed) is set in
// 'resolvent'. 'clause' must contain 'complementary_literal', while
// 'other_clause' must contain its negation. 'assignment' must have at least as
// many variables as each clause, and they must all be unassigned. They are
// still unassigned upon return.
bool Resolve(absl::Span<const Literal> clause,
             absl::Span<const Literal> other_clause,
             Literal complementary_literal, VariablesAssignment* assignment,
             std::vector<Literal>* resolvent);

// Adds to the given drat checker the problem clauses from the file at the given
// path, which must be in DIMACS format. Returns true iff the file was
// successfully parsed.
bool AddProblemClauses(const std::string& file_path, DratChecker* drat_checker);

// Adds to the given drat checker the infered and deleted clauses from the file
// at the given path, which must be in DRAT format. Returns true iff the file
// was successfully parsed.
bool AddInferedAndDeletedClauses(const std::string& file_path,
                                 DratChecker* drat_checker);

// The file formats that can be used to save a list of clauses.
enum SatFormat {
  DIMACS,
  DRAT,
};

// Prints the given clauses in the file at the given path, using the given file
// format. Returns true iff the file was successfully written.
bool PrintClauses(const std::string& file_path, SatFormat format,
                  const std::vector<std::vector<Literal>>& clauses,
                  int num_variables);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DRAT_CHECKER_H_
