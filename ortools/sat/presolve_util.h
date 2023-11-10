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

#ifndef OR_TOOLS_SAT_PRESOLVE_UTIL_H_
#define OR_TOOLS_SAT_PRESOLVE_UTIL_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Simple helper class to:
// - log in an uniform way a "time-consuming" presolve operation.
// - track a deterministic work limit.
// - update the deterministic time on finish.
class PresolveTimer {
 public:
  PresolveTimer(std::string name, SolverLogger* logger, TimeLimit* time_limit)
      : name_(std::move(name)), logger_(logger), time_limit_(time_limit) {
    timer_.Start();
  }

  // Track the work done (which is also the deterministic time).
  // By default we want a limit of around 1 deterministic seconds.
  void AddToWork(double dtime) { work_ += dtime; }
  void TrackSimpleLoop(int size) { work_ += 5e-9 * size; }
  bool WorkLimitIsReached() const { return work_ >= 1.0; }

  // Extra stats=value to display at the end.
  // We filter value of zero to have less clutter.
  void AddCounter(std::string name, int64_t count) {
    if (count == 0) return;
    counters_.emplace_back(std::move(name), count);
  }

  // Extra info at the end of the log line.
  void AddMessage(std::string name) { extra_infos_.push_back(std::move(name)); }

  // Update dtime and log operation summary.
  ~PresolveTimer();

 private:
  const std::string name_;

  WallTimer timer_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  double work_ = 0;
  std::vector<std::pair<std::string, int64_t>> counters_;
  std::vector<std::string> extra_infos_;
};

// If for each literal of a clause, we can infer a domain on an integer
// variable, then we know that this variable domain is included in the union of
// such infered domains.
//
// This allows to propagate "element" like constraints encoded as enforced
// linear relations, and other more general reasoning.
//
// TODO(user): Also use these "deductions" in the solver directly. This is done
// in good MIP solvers, and we should exploit them more.
//
// TODO(user): Also propagate implicit clauses (lit, not(lit)). Maybe merge
// that with probing code? it might be costly to store all deduction done by
// probing though, but I think this is what MIP solver do.
class DomainDeductions {
 public:
  // Adds the fact that enforcement => var \in domain.
  //
  // Important: No need to store any deductions where the domain is a superset
  // of the current variable domain.
  void AddDeduction(int literal_ref, int var, Domain domain);

  // Returns the domain of var when literal_ref is true.
  // If there is no information, returns Domain::AllValues().
  Domain ImpliedDomain(int literal_ref, int var) const;

  // Returns list of (var, domain) that were deduced because:
  //   1/ We have a domain deduction for var and all literal from the clause
  //   2/ So we can take the union of all the deduced domains.
  //
  // TODO(user): We could probably be even more efficient. We could also
  // compute exactly what clauses need to be "waked up" as new deductions are
  // added.
  std::vector<std::pair<int, Domain>> ProcessClause(
      absl::Span<const int> clause);

  // Optimization. Any following ProcessClause() will be fast if no more
  // deduction touching that clause are added.
  void MarkProcessingAsDoneForNow() {
    something_changed_.ClearAndResize(something_changed_.size());
  }

  // Returns the total number of "deductions" stored by this class.
  int NumDeductions() const { return deductions_.size(); }

 private:
  DEFINE_STRONG_INDEX_TYPE(Index);
  Index IndexFromLiteral(int ref) const {
    return Index(ref >= 0 ? 2 * ref : -2 * ref - 1);
  }

  std::vector<int> tmp_num_occurrences_;

  SparseBitset<Index> something_changed_;
  absl::StrongVector<Index, std::vector<int>> enforcement_to_vars_;
  absl::flat_hash_map<std::pair<Index, int>, Domain> deductions_;
};

// Does "to_modify += factor * to_add". Both constraint must be linear.
// Returns false and does not change anything in case of overflow.
//
// Note that the enforcement literals (if any) are ignored and left untouched.
bool AddLinearConstraintMultiple(int64_t factor, const ConstraintProto& to_add,
                                 ConstraintProto* to_modify);

// Replaces the variable var in ct using the definition constraint.
// Currently the coefficient in the definition must be 1 or -1.
//
// This might return false and NOT modify ConstraintProto in case of overflow
// or other issue with the substitution.
bool SubstituteVariable(int var, int64_t var_coeff_in_definition,
                        const ConstraintProto& definition, ConstraintProto* ct);

// Try to get more precise min/max activity of a linear constraints using
// at most ones from the model. This is heuristic based but should be relatively
// fast.
//
// TODO(user): Use better algorithm. The problem is the same as finding upper
// bound to the classic problem: maximum-independent set in a graph. We also
// only use at most ones, but we could use the more general binary implication
// graph.
class ActivityBoundHelper {
 public:
  ActivityBoundHelper() = default;

  // The at most one constraint must be added before linear constraint are
  // processed. The functions below will still works, but do nothing more than
  // compute trivial bounds.
  void ClearAtMostOnes();
  void AddAtMostOne(absl::Span<const int> amo);
  void AddAllAtMostOnes(const CpModelProto& proto);

  // Computes the max/min activity of a linear expression involving only
  // Booleans.
  //
  // Accepts a list of (literal, coefficient). Note that all literal will be
  // interpreted as referring to [0, 1] variables. We use the CpModelProto
  // convention for negated literal index.
  //
  // If conditional is not nullptr, then conditional[i][0/1] will give the
  // max activity if the literal at position i is false/true. This can be used
  // to fix variables or extract enforcement literal.
  //
  // Important: We shouldn't have duplicates or a lit and NegatedRef(lit)
  // appearing both.
  //
  // TODO(user): Indicate when the bounds are trivial (i.e. not intersection
  // with any amo) so that we don't waste more time processing the result?
  int64_t ComputeMaxActivity(
      absl::Span<const std::pair<int, int64_t>> terms,
      std::vector<std::array<int64_t, 2>>* conditional = nullptr) {
    return ComputeActivity(/*compute_min=*/false, terms, conditional);
  }
  int64_t ComputeMinActivity(
      absl::Span<const std::pair<int, int64_t>> terms,
      std::vector<std::array<int64_t, 2>>* conditional = nullptr) {
    return ComputeActivity(/*compute_min=*/true, terms, conditional);
  }

  // Computes relevant info to presolve a constraint with enforcement using
  // at most one information.
  //
  // This returns false iff the enforcement list cannot be satisfied.
  // It filters the enforcement list if some are consequences of other.
  // It fills the given set with the literal that must be true due to the
  // enforcement. Note that only literals or negated literal appearing in ref
  // are filled.
  bool PresolveEnforcement(absl::Span<const int> refs, ConstraintProto* ct,
                           absl::flat_hash_set<int>* literals_at_true);

  // Partition the list of literals into disjoint at most ones. The returned
  // spans are only valid until another function from this class is used.
  std::vector<absl::Span<const int>> PartitionLiteralsIntoAmo(
      absl::Span<const int> literals);

  // Returns true iff the given literal are in at most one relationship.
  bool IsAmo(absl::Span<const int> literals);

 private:
  DEFINE_STRONG_INDEX_TYPE(Index);
  Index IndexFromLiteral(int ref) const {
    return Index(ref >= 0 ? 2 * ref : -2 * ref - 1);
  }

  int64_t ComputeActivity(
      bool compute_min, absl::Span<const std::pair<int, int64_t>> terms,
      std::vector<std::array<int64_t, 2>>* conditional = nullptr);

  void PartitionIntoAmo(absl::Span<const std::pair<int, int64_t>> terms);

  // All coeff must be >= 0 here. Note that in practice, we shouldn't have
  // zero coeff, but we still support it.
  int64_t ComputeMaxActivityInternal(
      absl::Span<const std::pair<int, int64_t>> terms,
      std::vector<std::array<int64_t, 2>>* conditional = nullptr);

  // We use an unique index by at most one, and just stores for each literal
  // the at most one to which it belong.
  int num_at_most_ones_ = 0;
  absl::StrongVector<Index, std::vector<int>> amo_indices_;

  std::vector<std::pair<int, int64_t>> tmp_terms_;
  std::vector<std::pair<int64_t, int>> to_sort_;

  // We partition the set of term into disjoint at most one.
  absl::flat_hash_map<int, int> used_amo_to_dense_index_;
  absl::flat_hash_map<int, int64_t> amo_sums_;
  std::vector<int> partition_;
  std::vector<int64_t> max_by_partition_;
  std::vector<int64_t> second_max_by_partition_;

  // Used by PartitionLiteralsIntoAmo().
  CompactVectorVector<int, int> part_to_literals_;

  absl::flat_hash_set<int> triggered_amo_;
};

// Class to help detects clauses that differ on a single literal.
class ClauseWithOneMissingHasher {
 public:
  explicit ClauseWithOneMissingHasher(absl::BitGenRef random)
      : random_(random) {}

  // We use the proto encoding of literals here.
  void RegisterClause(int c, absl::Span<const int> clause);

  // Returns a hash of the clause with index c and literal ref removed.
  // This assumes that ref was part of the clause. Work in O(1).
  uint64_t HashWithout(int c, int ref) const {
    return clause_to_hash_[c] ^ literal_to_hash_[IndexFromLiteral(ref)];
  }

  // Returns a hash of the negation of all the given literals.
  uint64_t HashOfNegatedLiterals(absl::Span<const int> literals);

 private:
  DEFINE_STRONG_INDEX_TYPE(Index);
  Index IndexFromLiteral(int ref) const {
    return Index(ref >= 0 ? 2 * ref : -2 * ref - 1);
  }

  absl::BitGenRef random_;
  absl::StrongVector<Index, uint64_t> literal_to_hash_;
  std::vector<uint64_t> clause_to_hash_;
};

// Specific function. Returns true if the negation of all literals in clause
// except literal is exactly equal to the literal of enforcement.
//
// We assumes that enforcement and negated(clause) are sorted lexicographically
// Or negated(enforcement) and clause. Both option works. If not, we will only
// return false more often. When we return true, the property is enforced.
//
// TODO(user): For the same complexity, we do not need to specify literal and
// can recover it.
inline bool ClauseIsEnforcementImpliesLiteral(absl::Span<const int> clause,
                                              absl::Span<const int> enforcement,
                                              int literal) {
  if (clause.size() != enforcement.size() + 1) return false;
  int j = 0;
  for (int i = 0; i < clause.size(); ++i) {
    if (clause[i] == literal) continue;
    if (clause[i] != NegatedRef(enforcement[j])) return false;
    ++j;
  }
  return true;
}

// Same as LinearsDifferAtOneTerm() below but also fills the differing terms.
bool FindSingleLinearDifference(const LinearConstraintProto& lin1,
                                const LinearConstraintProto& lin2, int* var1,
                                int64_t* coeff1, int* var2, int64_t* coeff2);

// Returns true iff the two linear constraint only differ at a single term.
//
// Preconditions: Constraint should be sorted by variable and of same size.
inline bool LinearsDifferAtOneTerm(const LinearConstraintProto& lin1,
                                   const LinearConstraintProto& lin2) {
  int var1, var2;
  int64_t coeff1, coeff2;
  return FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2);
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_UTIL_H_
