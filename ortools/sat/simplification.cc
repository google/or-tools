// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/simplification.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/logging.h"
#include "ortools/base/random.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

SatPostsolver::SatPostsolver(int num_variables)
    : initial_num_variables_(num_variables), num_variables_(num_variables) {
  reverse_mapping_.resize(num_variables);
  for (BooleanVariable var(0); var < num_variables; ++var) {
    reverse_mapping_[var] = var;
  }
  assignment_.Resize(num_variables);
}

void SatPostsolver::Add(Literal x, absl::Span<const Literal> clause) {
  DCHECK(!clause.empty());
  DCHECK(std::find(clause.begin(), clause.end(), x) != clause.end());
  associated_literal_.push_back(ApplyReverseMapping(x));
  clauses_start_.push_back(clauses_literals_.size());
  for (const Literal& l : clause) {
    clauses_literals_.push_back(ApplyReverseMapping(l));
  }
}

void SatPostsolver::FixVariable(Literal x) {
  const Literal l = ApplyReverseMapping(x);
  assignment_.AssignFromTrueLiteral(l);
}

void SatPostsolver::ApplyMapping(
    const absl::StrongVector<BooleanVariable, BooleanVariable>& mapping) {
  absl::StrongVector<BooleanVariable, BooleanVariable> new_mapping;
  if (reverse_mapping_.size() < mapping.size()) {
    // We have new variables.
    while (reverse_mapping_.size() < mapping.size()) {
      reverse_mapping_.push_back(BooleanVariable(num_variables_++));
    }
    assignment_.Resize(num_variables_);
  }
  for (BooleanVariable v(0); v < mapping.size(); ++v) {
    const BooleanVariable image = mapping[v];
    if (image != kNoBooleanVariable) {
      if (image >= new_mapping.size()) {
        new_mapping.resize(image.value() + 1, kNoBooleanVariable);
      }
      new_mapping[image] = reverse_mapping_[v];
      DCHECK_NE(new_mapping[image], kNoBooleanVariable);
    }
  }
  std::swap(new_mapping, reverse_mapping_);
}

Literal SatPostsolver::ApplyReverseMapping(Literal l) {
  if (l.Variable() >= reverse_mapping_.size()) {
    // We have new variables.
    while (l.Variable() >= reverse_mapping_.size()) {
      reverse_mapping_.push_back(BooleanVariable(num_variables_++));
    }
    assignment_.Resize(num_variables_);
  }
  DCHECK_NE(reverse_mapping_[l.Variable()], kNoBooleanVariable);
  const Literal result(reverse_mapping_[l.Variable()], l.IsPositive());
  DCHECK(!assignment_.LiteralIsAssigned(result));
  return result;
}

void SatPostsolver::Postsolve(VariablesAssignment* assignment) const {
  // First, we set all unassigned variable to true.
  // This will be a valid assignment of the presolved problem.
  for (BooleanVariable var(0); var < assignment->NumberOfVariables(); ++var) {
    if (!assignment->VariableIsAssigned(var)) {
      assignment->AssignFromTrueLiteral(Literal(var, true));
    }
  }

  int previous_start = clauses_literals_.size();
  for (int i = static_cast<int>(clauses_start_.size()) - 1; i >= 0; --i) {
    bool set_associated_var = true;
    const int new_start = clauses_start_[i];
    for (int j = new_start; j < previous_start; ++j) {
      if (assignment->LiteralIsTrue(clauses_literals_[j])) {
        set_associated_var = false;
        break;
      }
    }
    previous_start = new_start;
    if (set_associated_var) {
      assignment->UnassignLiteral(associated_literal_[i].Negated());
      assignment->AssignFromTrueLiteral(associated_literal_[i]);
    }
  }

  // Ignore the value of any variables added by the presolve.
  assignment->Resize(initial_num_variables_);
}

std::vector<bool> SatPostsolver::ExtractAndPostsolveSolution(
    const SatSolver& solver) {
  std::vector<bool> solution(solver.NumVariables());
  for (BooleanVariable var(0); var < solver.NumVariables(); ++var) {
    DCHECK(solver.Assignment().VariableIsAssigned(var));
    solution[var.value()] =
        solver.Assignment().LiteralIsTrue(Literal(var, true));
  }
  return PostsolveSolution(solution);
}

std::vector<bool> SatPostsolver::PostsolveSolution(
    const std::vector<bool>& solution) {
  for (BooleanVariable var(0); var < solution.size(); ++var) {
    DCHECK_LT(var, reverse_mapping_.size());
    DCHECK_NE(reverse_mapping_[var], kNoBooleanVariable);
    DCHECK(!assignment_.VariableIsAssigned(reverse_mapping_[var]));
    assignment_.AssignFromTrueLiteral(
        Literal(reverse_mapping_[var], solution[var.value()]));
  }
  Postsolve(&assignment_);
  std::vector<bool> postsolved_solution;
  postsolved_solution.reserve(initial_num_variables_);
  for (int i = 0; i < initial_num_variables_; ++i) {
    postsolved_solution.push_back(
        assignment_.LiteralIsTrue(Literal(BooleanVariable(i), true)));
  }
  return postsolved_solution;
}

void SatPresolver::AddBinaryClause(Literal a, Literal b) { AddClause({a, b}); }

void SatPresolver::AddClause(absl::Span<const Literal> clause) {
  DCHECK_GT(clause.size(), 0) << "Added an empty clause to the presolver";
  const ClauseIndex ci(clauses_.size());
  clauses_.push_back(std::vector<Literal>(clause.begin(), clause.end()));
  in_clause_to_process_.push_back(true);
  clause_to_process_.push_back(ci);

  bool changed = false;
  std::vector<Literal>& clause_ref = clauses_.back();
  if (!equiv_mapping_.empty()) {
    for (int i = 0; i < clause_ref.size(); ++i) {
      const Literal old_literal = clause_ref[i];
      clause_ref[i] = Literal(equiv_mapping_[clause_ref[i].Index()]);
      if (old_literal != clause_ref[i]) changed = true;
    }
  }
  std::sort(clause_ref.begin(), clause_ref.end());
  clause_ref.erase(std::unique(clause_ref.begin(), clause_ref.end()),
                   clause_ref.end());

  // Check for trivial clauses:
  for (int i = 1; i < clause_ref.size(); ++i) {
    if (clause_ref[i] == clause_ref[i - 1].Negated()) {
      // The clause is trivial!
      ++num_trivial_clauses_;
      clause_to_process_.pop_back();
      clauses_.pop_back();
      in_clause_to_process_.pop_back();
      return;
    }
  }

  // This needs to be done after the basic canonicalization above.
  signatures_.push_back(ComputeSignatureOfClauseVariables(ci));
  DCHECK_EQ(signatures_.size(), clauses_.size());

  if (drat_proof_handler_ != nullptr && changed) {
    drat_proof_handler_->AddClause(clause_ref);
    drat_proof_handler_->DeleteClause(clause);
  }

  const Literal max_literal = clause_ref.back();
  const int required_size = std::max(max_literal.Index().value(),
                                     max_literal.NegatedIndex().value()) +
                            1;
  if (required_size > literal_to_clauses_.size()) {
    literal_to_clauses_.resize(required_size);
    literal_to_clause_sizes_.resize(required_size);
  }
  for (Literal e : clause_ref) {
    literal_to_clauses_[e.Index()].push_back(ci);
    literal_to_clause_sizes_[e.Index()]++;
  }
}

void SatPresolver::SetNumVariables(int num_variables) {
  const int required_size = 2 * num_variables;
  if (required_size > literal_to_clauses_.size()) {
    literal_to_clauses_.resize(required_size);
    literal_to_clause_sizes_.resize(required_size);
  }
}

void SatPresolver::AddClauseInternal(std::vector<Literal>* clause) {
  if (drat_proof_handler_ != nullptr) drat_proof_handler_->AddClause(*clause);

  DCHECK(std::is_sorted(clause->begin(), clause->end()));
  DCHECK_GT(clause->size(), 0) << "TODO(user): Unsat during presolve?";
  const ClauseIndex ci(clauses_.size());
  clauses_.push_back(std::vector<Literal>());
  clauses_.back().swap(*clause);
  in_clause_to_process_.push_back(true);
  clause_to_process_.push_back(ci);
  for (const Literal e : clauses_.back()) {
    literal_to_clauses_[e.Index()].push_back(ci);
    literal_to_clause_sizes_[e.Index()]++;
    UpdatePriorityQueue(e.Variable());
    UpdateBvaPriorityQueue(e.Index());
  }

  signatures_.push_back(ComputeSignatureOfClauseVariables(ci));
  DCHECK_EQ(signatures_.size(), clauses_.size());
}

absl::StrongVector<BooleanVariable, BooleanVariable>
SatPresolver::VariableMapping() const {
  absl::StrongVector<BooleanVariable, BooleanVariable> result;
  BooleanVariable new_var(0);
  for (BooleanVariable var(0); var < NumVariables(); ++var) {
    if (literal_to_clause_sizes_[Literal(var, true).Index()] > 0 ||
        literal_to_clause_sizes_[Literal(var, false).Index()] > 0) {
      result.push_back(new_var);
      ++new_var;
    } else {
      result.push_back(kNoBooleanVariable);
    }
  }
  return result;
}

void SatPresolver::LoadProblemIntoSatSolver(SatSolver* solver) {
  // Cleanup some memory that is not needed anymore. Note that we do need
  // literal_to_clause_sizes_ for VariableMapping() to work.
  var_pq_.Clear();
  var_pq_elements_.clear();
  in_clause_to_process_.clear();
  clause_to_process_.clear();
  literal_to_clauses_.clear();
  signatures_.clear();

  const absl::StrongVector<BooleanVariable, BooleanVariable> mapping =
      VariableMapping();
  int new_size = 0;
  for (BooleanVariable index : mapping) {
    if (index != kNoBooleanVariable) ++new_size;
  }

  std::vector<Literal> temp;
  solver->SetNumVariables(new_size);
  for (std::vector<Literal>& clause_ref : clauses_) {
    temp.clear();
    for (Literal l : clause_ref) {
      DCHECK_NE(mapping[l.Variable()], kNoBooleanVariable);
      temp.push_back(Literal(mapping[l.Variable()], l.IsPositive()));
    }
    if (!temp.empty()) solver->AddProblemClause(temp);
    gtl::STLClearObject(&clause_ref);
  }
}

bool SatPresolver::ProcessAllClauses() {
  int num_skipped_checks = 0;
  const int kCheckFrequency = 1000;

  // Because on large problem we don't have a budget to process all clauses,
  // lets start by the smallest ones first.
  std::sort(clause_to_process_.begin(), clause_to_process_.end(),
            [this](ClauseIndex c1, ClauseIndex c2) {
              return clauses_[c1].size() < clauses_[c2].size();
            });
  while (!clause_to_process_.empty()) {
    const ClauseIndex ci = clause_to_process_.front();
    in_clause_to_process_[ci] = false;
    clause_to_process_.pop_front();
    if (!ProcessClauseToSimplifyOthers(ci)) return false;
    if (++num_skipped_checks >= kCheckFrequency) {
      if (num_inspected_signatures_ + num_inspected_literals_ > 1e9) {
        VLOG(1) << "Aborting ProcessAllClauses() because work limit has been "
                   "reached";
        return true;
      }
      if (time_limit_ != nullptr && time_limit_->LimitReached()) return true;
      num_skipped_checks = 0;
    }
  }
  return true;
}

bool SatPresolver::Presolve() {
  // This is slighlty inefficient, but the presolve algorithm is
  // a lot more costly anyway.
  std::vector<bool> can_be_removed(NumVariables(), true);
  return Presolve(can_be_removed);
}

bool SatPresolver::Presolve(const std::vector<bool>& can_be_removed) {
  WallTimer timer;
  timer.Start();

  if (logger_->LoggingIsEnabled()) {
    int64_t num_removable = 0;
    for (const bool b : can_be_removed) {
      if (b) ++num_removable;
    }
    SOLVER_LOG(logger_,
               "[SAT presolve] num removable Booleans: ", num_removable, " / ",
               can_be_removed.size());
    SOLVER_LOG(logger_,
               "[SAT presolve] num trivial clauses: ", num_trivial_clauses_);
    DisplayStats(0);
  }

  // TODO(user): When a clause is strengthened, add it to a queue so it can
  // be processed again?
  if (!ProcessAllClauses()) return false;
  if (logger_->LoggingIsEnabled()) DisplayStats(timer.Get());

  if (time_limit_ != nullptr && time_limit_->LimitReached()) return true;
  if (num_inspected_signatures_ + num_inspected_literals_ > 1e9) return true;

  InitializePriorityQueue();
  while (var_pq_.Size() > 0) {
    const BooleanVariable var = var_pq_.Top()->variable;
    var_pq_.Pop();
    if (!can_be_removed[var.value()]) continue;
    if (CrossProduct(Literal(var, true))) {
      if (!ProcessAllClauses()) return false;
    }
    if (time_limit_ != nullptr && time_limit_->LimitReached()) return true;
    if (num_inspected_signatures_ + num_inspected_literals_ > 1e9) return true;
  }
  if (logger_->LoggingIsEnabled()) DisplayStats(timer.Get());

  // We apply BVA after a pass of the other algorithms.
  if (parameters_.presolve_use_bva()) {
    PresolveWithBva();
    if (logger_->LoggingIsEnabled()) DisplayStats(timer.Get());
  }

  return true;
}

void SatPresolver::PresolveWithBva() {
  var_pq_elements_.clear();  // so we don't update it.
  InitializeBvaPriorityQueue();
  while (bva_pq_.Size() > 0) {
    const LiteralIndex lit = bva_pq_.Top()->literal;
    bva_pq_.Pop();
    SimpleBva(lit);
  }
}

// We use the same notation as in the article mentioned in the .h
void SatPresolver::SimpleBva(LiteralIndex l) {
  literal_to_p_size_.resize(literal_to_clauses_.size(), 0);
  DCHECK(std::all_of(literal_to_p_size_.begin(), literal_to_p_size_.end(),
                     [](int v) { return v == 0; }));

  // We will try to add a literal to m_lit_ and take a subset of m_cls_ such
  // that |m_lit_| * |m_cls_| - |m_lit_| - |m_cls_| is maximized.
  m_lit_ = {l};
  m_cls_ = literal_to_clauses_[l];

  int reduction = 0;
  while (true) {
    LiteralIndex lmax = kNoLiteralIndex;
    int max_size = 0;

    flattened_p_.clear();
    for (const ClauseIndex c : m_cls_) {
      const std::vector<Literal>& clause = clauses_[c];
      if (clause.empty()) continue;  // It has been deleted.

      // Find a literal different from l that occur in the less number of
      // clauses.
      const LiteralIndex l_min =
          FindLiteralWithShortestOccurrenceListExcluding(clause, Literal(l));
      if (l_min == kNoLiteralIndex) continue;

      // Find all the clauses of the form "clause \ {l} + {l'}", for a literal
      // l' that is not in the clause.
      for (const ClauseIndex d : literal_to_clauses_[l_min]) {
        if (clause.size() != clauses_[d].size()) continue;
        const LiteralIndex l_diff =
            DifferAtGivenLiteral(clause, clauses_[d], Literal(l));
        if (l_diff == kNoLiteralIndex || m_lit_.count(l_diff) > 0) continue;
        if (l_diff == Literal(l).NegatedIndex()) {
          // Self-subsumbtion!
          //
          // TODO(user): Not sure this can happen after the presolve we did
          // before calling SimpleBva().
          VLOG(1) << "self-subsumbtion";
        }

        flattened_p_.push_back({l_diff, c});
        const int new_size = ++literal_to_p_size_[l_diff];
        if (new_size > max_size) {
          lmax = l_diff;
          max_size = new_size;
        }
      }
    }

    if (lmax == kNoLiteralIndex) break;
    const int new_m_lit_size = m_lit_.size() + 1;
    const int new_m_cls_size = max_size;
    const int new_reduction =
        new_m_lit_size * new_m_cls_size - new_m_cls_size - new_m_lit_size;

    if (new_reduction <= reduction) break;
    DCHECK_NE(1, new_m_lit_size);
    DCHECK_NE(1, new_m_cls_size);

    // TODO(user): Instead of looping and recomputing p_ again, we can instead
    // simply intersect the clause indices in p_. This should be a lot faster.
    // That said, we loop again only when we have a reduction, so this happens
    // not that often compared to the initial computation of p.
    reduction = new_reduction;
    m_lit_.insert(lmax);

    // Set m_cls_ to p_[lmax].
    m_cls_.clear();
    for (const auto entry : flattened_p_) {
      literal_to_p_size_[entry.first] = 0;
      if (entry.first == lmax) m_cls_.push_back(entry.second);
    }
    flattened_p_.clear();
  }

  // Make sure literal_to_p_size_ is all zero.
  for (const auto entry : flattened_p_) literal_to_p_size_[entry.first] = 0;
  flattened_p_.clear();

  // A strictly positive reduction means that applying the BVA transform will
  // reduce the overall number of clauses by that much. Here we can control
  // what kind of reduction we want to apply.
  if (reduction <= parameters_.presolve_bva_threshold()) return;
  DCHECK_GT(m_lit_.size(), 1);

  // Create a new variable.
  const int old_size = literal_to_clauses_.size();
  const LiteralIndex x_true = LiteralIndex(old_size);
  const LiteralIndex x_false = LiteralIndex(old_size + 1);
  literal_to_clauses_.resize(old_size + 2);
  literal_to_clause_sizes_.resize(old_size + 2);
  bva_pq_elements_.resize(old_size + 2);
  bva_pq_elements_[x_true.value()].literal = x_true;
  bva_pq_elements_[x_false.value()].literal = x_false;

  // Add the new clauses.
  if (drat_proof_handler_ != nullptr) drat_proof_handler_->AddOneVariable();
  for (const LiteralIndex lit : m_lit_) {
    tmp_new_clause_ = {Literal(lit), Literal(x_true)};
    AddClauseInternal(&tmp_new_clause_);
  }
  for (const ClauseIndex ci : m_cls_) {
    tmp_new_clause_ = clauses_[ci];
    DCHECK(!tmp_new_clause_.empty());
    for (Literal& ref : tmp_new_clause_) {
      if (ref.Index() == l) {
        ref = Literal(x_false);
        break;
      }
    }

    // TODO(user): we can be more efficient here since the clause used to
    // derive this one is already sorted. We just need to insert x_false in
    // the correct place and remove l.
    std::sort(tmp_new_clause_.begin(), tmp_new_clause_.end());
    AddClauseInternal(&tmp_new_clause_);
  }

  // Delete the old clauses.
  //
  // TODO(user): do that more efficiently? we can simply store the clause d
  // instead of finding it again. That said, this is executed only when a
  // reduction occur, whereas the start of this function occur all the time, so
  // we want it to be as fast as possible.
  for (const ClauseIndex c : m_cls_) {
    const std::vector<Literal>& clause = clauses_[c];
    DCHECK(!clause.empty());
    const LiteralIndex l_min =
        FindLiteralWithShortestOccurrenceListExcluding(clause, Literal(l));
    for (const LiteralIndex lit : m_lit_) {
      if (lit == l) continue;
      for (const ClauseIndex d : literal_to_clauses_[l_min]) {
        if (clause.size() != clauses_[d].size()) continue;
        const LiteralIndex l_diff =
            DifferAtGivenLiteral(clause, clauses_[d], Literal(l));
        if (l_diff == lit) {
          Remove(d);
          break;
        }
      }
    }
    Remove(c);
  }

  // Add these elements to the priority queue.
  //
  // TODO(user): It seems some of the element already processed could benefit
  // from being processed again by SimpleBva(). It is unclear if it is worth the
  // extra time though.
  AddToBvaPriorityQueue(x_true);
  AddToBvaPriorityQueue(x_false);
  AddToBvaPriorityQueue(l);
}

uint64_t SatPresolver::ComputeSignatureOfClauseVariables(ClauseIndex ci) {
  uint64_t signature = 0;
  for (const Literal l : clauses_[ci]) {
    signature |= (uint64_t{1} << (l.Variable().value() % 64));
  }
  DCHECK_EQ(signature == 0, clauses_[ci].empty());
  return signature;
}

// We are looking for clause that contains lit and contains a superset of the
// literals in clauses_[clauses_index] or a superset with just one literal of
// clauses_[clause_index] negated.
bool SatPresolver::ProcessClauseToSimplifyOthersUsingLiteral(
    ClauseIndex clause_index, Literal lit) {
  const std::vector<Literal>& clause = clauses_[clause_index];
  const uint64_t clause_signature = signatures_[clause_index];
  LiteralIndex opposite_literal;

  // Try to simplify the clauses containing 'lit'. We take advantage of this
  // loop to also detect if there is any empty clause, in which case we will
  // trigger a "cleaning" below.
  bool need_cleaning = false;
  num_inspected_signatures_ += literal_to_clauses_[lit.Index()].size();
  for (const ClauseIndex ci : literal_to_clauses_[lit.Index()]) {
    const uint64_t ci_signature = signatures_[ci];

    // This allows to check for empty clause without fetching the memory at
    // clause_[ci]. It can have a huge time impact on large problems.
    DCHECK_EQ(ci_signature, ComputeSignatureOfClauseVariables(ci));
    if (ci_signature == 0) {
      need_cleaning = true;
      continue;
    }

    // Note that SimplifyClause() can return true only if the variables in
    // 'a' are a subset of the one in 'b'. We use the signatures to abort
    // early as a speed optimization.
    if (ci != clause_index && (clause_signature & ~ci_signature) == 0 &&
        SimplifyClause(clause, &clauses_[ci], &opposite_literal,
                       &num_inspected_literals_)) {
      if (opposite_literal == kNoLiteralIndex) {
        need_cleaning = true;
        Remove(ci);
        continue;
      } else {
        DCHECK_NE(opposite_literal, lit.Index());
        if (clauses_[ci].empty()) return false;  // UNSAT.
        if (drat_proof_handler_ != nullptr) {
          // TODO(user): remove the old clauses_[ci] afterwards.
          drat_proof_handler_->AddClause(clauses_[ci]);
        }

        // Recompute signature.
        signatures_[ci] = ComputeSignatureOfClauseVariables(ci);

        // Remove ci from the occurrence list. Note that opposite_literal
        // cannot be literal or literal.Negated().
        gtl::STLEraseAllFromSequence(&literal_to_clauses_[opposite_literal],
                                     ci);
        --literal_to_clause_sizes_[opposite_literal];
        UpdatePriorityQueue(Literal(opposite_literal).Variable());

        if (!in_clause_to_process_[ci]) {
          in_clause_to_process_[ci] = true;
          clause_to_process_.push_back(ci);
        }
      }
    }
  }

  if (need_cleaning) {
    int new_index = 0;
    auto& occurrence_list_ref = literal_to_clauses_[lit.Index()];
    for (const ClauseIndex ci : occurrence_list_ref) {
      if (signatures_[ci] != 0) occurrence_list_ref[new_index++] = ci;
    }
    occurrence_list_ref.resize(new_index);
    DCHECK_EQ(literal_to_clause_sizes_[lit.Index()], new_index);
  }

  return true;
}

// TODO(user): Binary clauses are really common, and we can probably do this
// more efficiently for them. For instance, we could just take the intersection
// of two sorted lists to get the simplified clauses.
bool SatPresolver::ProcessClauseToSimplifyOthers(ClauseIndex clause_index) {
  const std::vector<Literal>& clause = clauses_[clause_index];
  if (clause.empty()) return true;
  DCHECK(std::is_sorted(clause.begin(), clause.end()));

  LiteralIndex opposite_literal;
  const Literal lit = FindLiteralWithShortestOccurrenceList(clause);

  if (!ProcessClauseToSimplifyOthersUsingLiteral(clause_index, lit)) {
    return false;
  }

  // If there is another "short" occurrence list, use it. Otherwise use the
  // one corresponding to the negation of lit.
  const LiteralIndex other_lit_index =
      FindLiteralWithShortestOccurrenceListExcluding(clause, lit);
  if (other_lit_index != kNoLiteralIndex &&
      literal_to_clause_sizes_[other_lit_index] <
          literal_to_clause_sizes_[lit.NegatedIndex()]) {
    return ProcessClauseToSimplifyOthersUsingLiteral(clause_index,
                                                     Literal(other_lit_index));
  } else {
    // Treat the clauses containing lit.Negated().
    int new_index = 0;
    bool something_removed = false;
    auto& occurrence_list_ref = literal_to_clauses_[lit.NegatedIndex()];
    const uint64_t clause_signature = signatures_[clause_index];
    for (const ClauseIndex ci : occurrence_list_ref) {
      const uint64_t ci_signature = signatures_[ci];
      DCHECK_EQ(ci_signature, ComputeSignatureOfClauseVariables(ci));
      if (ci_signature == 0) continue;

      // TODO(user): not super optimal since we could abort earlier if
      // opposite_literal is not the negation of shortest_list. Note that this
      // applies to the second call to
      // ProcessClauseToSimplifyOthersUsingLiteral() above too.
      if ((clause_signature & ~ci_signature) == 0 &&
          SimplifyClause(clause, &clauses_[ci], &opposite_literal,
                         &num_inspected_literals_)) {
        DCHECK_EQ(opposite_literal, lit.NegatedIndex());
        if (clauses_[ci].empty()) return false;  // UNSAT.
        if (drat_proof_handler_ != nullptr) {
          // TODO(user): remove the old clauses_[ci] afterwards.
          drat_proof_handler_->AddClause(clauses_[ci]);
        }

        // Recompute signature.
        signatures_[ci] = ComputeSignatureOfClauseVariables(ci);

        if (!in_clause_to_process_[ci]) {
          in_clause_to_process_[ci] = true;
          clause_to_process_.push_back(ci);
        }
        something_removed = true;
        continue;
      }
      occurrence_list_ref[new_index] = ci;
      ++new_index;
    }
    occurrence_list_ref.resize(new_index);
    literal_to_clause_sizes_[lit.NegatedIndex()] = new_index;
    if (something_removed) {
      UpdatePriorityQueue(Literal(lit.NegatedIndex()).Variable());
    }
  }
  return true;
}

void SatPresolver::RemoveAndRegisterForPostsolveAllClauseContaining(Literal x) {
  for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
    if (!clauses_[i].empty()) RemoveAndRegisterForPostsolve(i, x);
  }
  gtl::STLClearObject(&literal_to_clauses_[x.Index()]);
  literal_to_clause_sizes_[x.Index()] = 0;
}

bool SatPresolver::CrossProduct(Literal x) {
  const int s1 = literal_to_clause_sizes_[x.Index()];
  const int s2 = literal_to_clause_sizes_[x.NegatedIndex()];

  // Note that if s1 or s2 is equal to 0, this function will implicitly just
  // fix the variable x.
  if (s1 == 0 && s2 == 0) return false;

  // Heuristic. Abort if the work required to decide if x should be removed
  // seems to big.
  if (s1 > 1 && s2 > 1 && s1 * s2 > parameters_.presolve_bve_threshold()) {
    return false;
  }

  // Compute the threshold under which we don't remove x.Variable().
  int threshold = 0;
  const int clause_weight = parameters_.presolve_bve_clause_weight();
  for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
    if (!clauses_[i].empty()) {
      threshold += clause_weight + clauses_[i].size();
    }
  }
  for (ClauseIndex i : literal_to_clauses_[x.NegatedIndex()]) {
    if (!clauses_[i].empty()) {
      threshold += clause_weight + clauses_[i].size();
    }
  }

  // For the BCE, we prefer s2 to be small.
  if (s1 < s2) x = x.Negated();

  // Test whether we should remove the x.Variable().
  int size = 0;
  for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
    if (clauses_[i].empty()) continue;
    bool no_resolvant = true;
    for (ClauseIndex j : literal_to_clauses_[x.NegatedIndex()]) {
      if (clauses_[j].empty()) continue;
      const int rs = ComputeResolvantSize(x, clauses_[i], clauses_[j]);
      if (rs >= 0) {
        no_resolvant = false;
        size += clause_weight + rs;

        // Abort early if the "size" become too big.
        if (size > threshold) return false;
      }
    }
    if (no_resolvant && parameters_.presolve_blocked_clause()) {
      // This is an incomplete heuristic for blocked clause detection. Here,
      // the clause i is "blocked", so we can remove it. Note that the code
      // below already do that if we decide to eliminate x.
      //
      // For more details, see the paper "Blocked clause elimination", Matti
      // Jarvisalo, Armin Biere, Marijn Heule. TACAS, volume 6015 of Lecture
      // Notes in Computer Science, pages 129–144. Springer, 2010.
      //
      // TODO(user): Choose if we use x or x.Negated() depending on the list
      // sizes? The function achieve the same if x = x.Negated(), however the
      // loops are not done in the same order which may change this incomplete
      // "blocked" clause detection.
      RemoveAndRegisterForPostsolve(i, x);
    }
  }

  // Add all the resolvant clauses.
  // Note that the variable priority queue will only be updated during the
  // deletion.
  std::vector<Literal> temp;
  for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
    if (clauses_[i].empty()) continue;
    for (ClauseIndex j : literal_to_clauses_[x.NegatedIndex()]) {
      if (clauses_[j].empty()) continue;
      if (ComputeResolvant(x, clauses_[i], clauses_[j], &temp)) {
        AddClauseInternal(&temp);
      }
    }
  }

  // Deletes the old clauses.
  //
  // TODO(user): We could only update the priority queue once for each variable
  // instead of doing it many times.
  RemoveAndRegisterForPostsolveAllClauseContaining(x);
  RemoveAndRegisterForPostsolveAllClauseContaining(x.Negated());

  // TODO(user): At this point x.Variable() is added back to the priority queue.
  // Avoid doing that.
  return true;
}

void SatPresolver::Remove(ClauseIndex ci) {
  signatures_[ci] = 0;
  for (Literal e : clauses_[ci]) {
    literal_to_clause_sizes_[e.Index()]--;
    UpdatePriorityQueue(e.Variable());
    UpdateBvaPriorityQueue(Literal(e.Variable(), true).Index());
    UpdateBvaPriorityQueue(Literal(e.Variable(), false).Index());
  }
  if (drat_proof_handler_ != nullptr) {
    drat_proof_handler_->DeleteClause(clauses_[ci]);
  }
  gtl::STLClearObject(&clauses_[ci]);
}

void SatPresolver::RemoveAndRegisterForPostsolve(ClauseIndex ci, Literal x) {
  postsolver_->Add(x, clauses_[ci]);
  Remove(ci);
}

Literal SatPresolver::FindLiteralWithShortestOccurrenceList(
    const std::vector<Literal>& clause) {
  DCHECK(!clause.empty());
  Literal result = clause.front();
  int best_size = literal_to_clause_sizes_[result.Index()];
  for (const Literal l : clause) {
    const int size = literal_to_clause_sizes_[l.Index()];
    if (size < best_size) {
      result = l;
      best_size = size;
    }
  }
  return result;
}

LiteralIndex SatPresolver::FindLiteralWithShortestOccurrenceListExcluding(
    const std::vector<Literal>& clause, Literal to_exclude) {
  DCHECK(!clause.empty());
  LiteralIndex result = kNoLiteralIndex;
  int num_occurrences = std::numeric_limits<int>::max();
  for (const Literal l : clause) {
    if (l == to_exclude) continue;
    if (literal_to_clause_sizes_[l.Index()] < num_occurrences) {
      result = l.Index();
      num_occurrences = literal_to_clause_sizes_[l.Index()];
    }
  }
  return result;
}

void SatPresolver::UpdatePriorityQueue(BooleanVariable var) {
  if (var_pq_elements_.empty()) return;  // not initialized.
  PQElement* element = &var_pq_elements_[var];
  element->weight = literal_to_clause_sizes_[Literal(var, true).Index()] +
                    literal_to_clause_sizes_[Literal(var, false).Index()];
  if (var_pq_.Contains(element)) {
    var_pq_.NoteChangedPriority(element);
  } else {
    var_pq_.Add(element);
  }
}

void SatPresolver::InitializePriorityQueue() {
  const int num_vars = NumVariables();
  var_pq_elements_.resize(num_vars);
  for (BooleanVariable var(0); var < num_vars; ++var) {
    PQElement* element = &var_pq_elements_[var];
    element->variable = var;
    element->weight = literal_to_clause_sizes_[Literal(var, true).Index()] +
                      literal_to_clause_sizes_[Literal(var, false).Index()];
    var_pq_.Add(element);
  }
}

void SatPresolver::UpdateBvaPriorityQueue(LiteralIndex lit) {
  if (bva_pq_elements_.empty()) return;  // not initialized.
  DCHECK_LT(lit, bva_pq_elements_.size());
  BvaPqElement* element = &bva_pq_elements_[lit.value()];
  element->weight = literal_to_clause_sizes_[lit];
  if (bva_pq_.Contains(element)) {
    bva_pq_.NoteChangedPriority(element);
  }
}

void SatPresolver::AddToBvaPriorityQueue(LiteralIndex lit) {
  if (bva_pq_elements_.empty()) return;  // not initialized.
  DCHECK_LT(lit, bva_pq_elements_.size());
  BvaPqElement* element = &bva_pq_elements_[lit.value()];
  element->weight = literal_to_clause_sizes_[lit];
  DCHECK(!bva_pq_.Contains(element));
  if (element->weight > 2) bva_pq_.Add(element);
}

void SatPresolver::InitializeBvaPriorityQueue() {
  const int num_literals = 2 * NumVariables();
  bva_pq_.Clear();
  bva_pq_elements_.assign(num_literals, BvaPqElement());
  for (LiteralIndex lit(0); lit < num_literals; ++lit) {
    BvaPqElement* element = &bva_pq_elements_[lit.value()];
    element->literal = lit;
    element->weight = literal_to_clause_sizes_[lit];

    // If a literal occur only in two clauses, then there is no point calling
    // SimpleBva() on it.
    if (element->weight > 2) bva_pq_.Add(element);
  }
}

void SatPresolver::DisplayStats(double elapsed_seconds) {
  int num_literals = 0;
  int num_clauses = 0;
  int num_singleton_clauses = 0;
  for (const std::vector<Literal>& c : clauses_) {
    if (!c.empty()) {
      if (c.size() == 1) ++num_singleton_clauses;
      ++num_clauses;
      num_literals += c.size();
    }
  }
  int num_one_side = 0;
  int num_simple_definition = 0;
  int num_vars = 0;
  for (BooleanVariable var(0); var < NumVariables(); ++var) {
    const int s1 = literal_to_clause_sizes_[Literal(var, true).Index()];
    const int s2 = literal_to_clause_sizes_[Literal(var, false).Index()];
    if (s1 == 0 && s2 == 0) continue;

    ++num_vars;
    if (s1 == 0 || s2 == 0) {
      num_one_side++;
    } else if (s1 == 1 || s2 == 1) {
      num_simple_definition++;
    }
  }
  SOLVER_LOG(logger_, "[SAT presolve] [", elapsed_seconds, "s]",
             " clauses:", num_clauses, " literals:", num_literals,
             " vars:", num_vars, " one_side_vars:", num_one_side,
             " simple_definition:", num_simple_definition,
             " singleton_clauses:", num_singleton_clauses);
}

bool SimplifyClause(const std::vector<Literal>& a, std::vector<Literal>* b,
                    LiteralIndex* opposite_literal,
                    int64_t* num_inspected_literals) {
  if (b->size() < a.size()) return false;
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b->begin(), b->end()));
  if (num_inspected_literals != nullptr) {
    *num_inspected_literals += a.size();
    *num_inspected_literals += b->size();
  }

  *opposite_literal = LiteralIndex(-1);

  int num_diff = 0;
  std::vector<Literal>::const_iterator ia = a.begin();
  std::vector<Literal>::const_iterator ib = b->begin();
  std::vector<Literal>::const_iterator to_remove;

  // Because we abort early when size_diff becomes negative, the second test
  // in the while loop is not needed.
  int size_diff = b->size() - a.size();
  while (ia != a.end() /* && ib != b->end() */) {
    if (*ia == *ib) {  // Same literal.
      ++ia;
      ++ib;
    } else if (*ia == ib->Negated()) {  // Opposite literal.
      ++num_diff;
      if (num_diff > 1) return false;  // Too much difference.
      to_remove = ib;
      ++ia;
      ++ib;
    } else if (*ia < *ib) {
      return false;  // A literal of a is not in b.
    } else {         // *ia > *ib
      ++ib;

      // A literal of b is not in a, we can abort early by comparing the sizes
      // left.
      if (--size_diff < 0) return false;
    }
  }
  if (num_diff == 1) {
    *opposite_literal = to_remove->Index();
    b->erase(to_remove);
  }
  return true;
}

LiteralIndex DifferAtGivenLiteral(const std::vector<Literal>& a,
                                  const std::vector<Literal>& b, Literal l) {
  DCHECK_EQ(b.size(), a.size());
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b.begin(), b.end()));
  LiteralIndex result = kNoLiteralIndex;
  std::vector<Literal>::const_iterator ia = a.begin();
  std::vector<Literal>::const_iterator ib = b.begin();
  while (ia != a.end() && ib != b.end()) {
    if (*ia == *ib) {  // Same literal.
      ++ia;
      ++ib;
    } else if (*ia < *ib) {
      // A literal of a is not in b, it must be l.
      // Note that this can only happen once.
      if (*ia != l) return kNoLiteralIndex;
      ++ia;
    } else {
      // A literal of b is not in a, save it.
      // We abort if this happen twice.
      if (result != kNoLiteralIndex) return kNoLiteralIndex;
      result = (*ib).Index();
      ++ib;
    }
  }
  // Check the corner case of the difference at the end.
  if (ia != a.end() && *ia != l) return kNoLiteralIndex;
  if (ib != b.end()) {
    if (result != kNoLiteralIndex) return kNoLiteralIndex;
    result = (*ib).Index();
  }
  return result;
}

bool ComputeResolvant(Literal x, const std::vector<Literal>& a,
                      const std::vector<Literal>& b,
                      std::vector<Literal>* out) {
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b.begin(), b.end()));

  out->clear();
  std::vector<Literal>::const_iterator ia = a.begin();
  std::vector<Literal>::const_iterator ib = b.begin();
  while ((ia != a.end()) && (ib != b.end())) {
    if (*ia == *ib) {
      out->push_back(*ia);
      ++ia;
      ++ib;
    } else if (*ia == ib->Negated()) {
      if (*ia != x) return false;  // Trivially true.
      DCHECK_EQ(*ib, x.Negated());
      ++ia;
      ++ib;
    } else if (*ia < *ib) {
      out->push_back(*ia);
      ++ia;
    } else {  // *ia > *ib
      out->push_back(*ib);
      ++ib;
    }
  }

  // Copy remaining literals.
  out->insert(out->end(), ia, a.end());
  out->insert(out->end(), ib, b.end());
  return true;
}

// Note that this function takes a big chunk of the presolve running time.
int ComputeResolvantSize(Literal x, const std::vector<Literal>& a,
                         const std::vector<Literal>& b) {
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b.begin(), b.end()));

  int size = static_cast<int>(a.size() + b.size()) - 2;
  std::vector<Literal>::const_iterator ia = a.begin();
  std::vector<Literal>::const_iterator ib = b.begin();
  while ((ia != a.end()) && (ib != b.end())) {
    if (*ia == *ib) {
      --size;
      ++ia;
      ++ib;
    } else if (*ia == ib->Negated()) {
      if (*ia != x) return -1;  // Trivially true.
      DCHECK_EQ(*ib, x.Negated());
      ++ia;
      ++ib;
    } else if (*ia < *ib) {
      ++ia;
    } else {  // *ia > *ib
      ++ib;
    }
  }
  DCHECK_GE(size, 0);
  return size;
}

// A simple graph where the nodes are the literals and the nodes adjacent to a
// literal l are the propagated literal when l is assigned in the underlying
// sat solver.
//
// This can be used to do a strong component analysis while probing all the
// literals of a solver. Note that this can be expensive, hence the support
// for a deterministic time limit.
class PropagationGraph {
 public:
  PropagationGraph(double deterministic_time_limit, SatSolver* solver)
      : solver_(solver),
        deterministic_time_limit(solver->deterministic_time() +
                                 deterministic_time_limit) {}

  // Returns the set of node adjacent to the given one.
  // Interface needed by FindStronglyConnectedComponents(), note that it needs
  // to be const.
  const std::vector<int32_t>& operator[](int32_t index) const {
    scratchpad_.clear();
    solver_->Backtrack(0);

    // Note that when the time limit is reached, we just keep returning empty
    // adjacency list. This way, the SCC algorithm will terminate quickly and
    // the equivalent literals detection will be incomplete but correct. Note
    // also that thanks to the SCC algorithm, we will explore the connected
    // components first.
    if (solver_->deterministic_time() > deterministic_time_limit) {
      return scratchpad_;
    }

    const Literal l = Literal(LiteralIndex(index));
    if (!solver_->Assignment().LiteralIsAssigned(l)) {
      const int trail_index = solver_->LiteralTrail().Index();
      solver_->EnqueueDecisionAndBackjumpOnConflict(l);
      if (solver_->CurrentDecisionLevel() > 0) {
        // Note that the +1 is to avoid adding a => a.
        for (int i = trail_index + 1; i < solver_->LiteralTrail().Index();
             ++i) {
          scratchpad_.push_back(solver_->LiteralTrail()[i].Index().value());
        }
      }
    }
    return scratchpad_;
  }

 private:
  mutable std::vector<int32_t> scratchpad_;
  SatSolver* const solver_;
  const double deterministic_time_limit;

  DISALLOW_COPY_AND_ASSIGN(PropagationGraph);
};

void ProbeAndFindEquivalentLiteral(
    SatSolver* solver, SatPostsolver* postsolver,
    DratProofHandler* drat_proof_handler,
    absl::StrongVector<LiteralIndex, LiteralIndex>* mapping) {
  WallTimer timer;
  timer.Start();

  solver->Backtrack(0);
  mapping->clear();
  const int num_already_fixed_vars = solver->LiteralTrail().Index();

  PropagationGraph graph(
      solver->parameters().presolve_probing_deterministic_time_limit(), solver);
  const int32_t size = solver->NumVariables() * 2;
  std::vector<std::vector<int32_t>> scc;
  FindStronglyConnectedComponents(size, graph, &scc);

  // We have no guarantee that the cycle of x and not(x) touch the same
  // variables. This is because we may have more info for the literal probed
  // later or the propagation may go only in one direction. For instance if we
  // have two clauses (not(x1) v x2) and (not(x1) v not(x2) v x3) then x1
  // implies x2 and x3 but not(x3) doesn't imply anything by unit propagation.
  //
  // TODO(user): Add some constraint so that it does?
  //
  // Because of this, we "merge" the cycles.
  MergingPartition partition(size);
  for (const std::vector<int32_t>& component : scc) {
    if (component.size() > 1) {
      if (mapping->empty()) mapping->resize(size, LiteralIndex(-1));
      const Literal representative((LiteralIndex(component[0])));
      for (int i = 1; i < component.size(); ++i) {
        const Literal l((LiteralIndex(component[i])));
        // TODO(user): check compatibility? if x ~ not(x) => unsat.
        // but probably, the solver would have found this too? not sure...
        partition.MergePartsOf(representative.Index().value(),
                               l.Index().value());
        partition.MergePartsOf(representative.NegatedIndex().value(),
                               l.NegatedIndex().value());
      }

      // We rely on the fact that the representative of a literal x and the one
      // of its negation are the same variable.
      DCHECK_EQ(Literal(LiteralIndex(partition.GetRootAndCompressPath(
                    representative.Index().value()))),
                Literal(LiteralIndex(partition.GetRootAndCompressPath(
                            representative.NegatedIndex().value())))
                    .Negated());
    }
  }

  solver->Backtrack(0);
  int num_equiv = 0;
  if (!mapping->empty()) {
    // If a variable in a cycle is fixed. We want to fix all of them.
    //
    // We first fix all representative if one variable of the cycle is fixed. In
    // a second pass we fix all the variable of a cycle whose representative is
    // fixed.
    //
    // TODO(user): Fixing a variable might fix more of them by propagation, so
    // we might not fix everything possible with these loops.
    const VariablesAssignment& assignment = solver->Assignment();
    for (LiteralIndex i(0); i < size; ++i) {
      const LiteralIndex rep(partition.GetRootAndCompressPath(i.value()));
      if (assignment.LiteralIsAssigned(Literal(i)) &&
          !assignment.LiteralIsAssigned(Literal(rep))) {
        const Literal true_lit = assignment.LiteralIsTrue(Literal(i))
                                     ? Literal(rep)
                                     : Literal(rep).Negated();
        solver->AddUnitClause(true_lit);
        if (drat_proof_handler != nullptr) {
          drat_proof_handler->AddClause({true_lit});
        }
      }
    }
    for (LiteralIndex i(0); i < size; ++i) {
      const LiteralIndex rep(partition.GetRootAndCompressPath(i.value()));
      (*mapping)[i] = rep;
      if (assignment.LiteralIsAssigned(Literal(rep))) {
        if (!assignment.LiteralIsAssigned(Literal(i))) {
          const Literal true_lit = assignment.LiteralIsTrue(Literal(rep))
                                       ? Literal(i)
                                       : Literal(i).Negated();
          solver->AddUnitClause(true_lit);
          if (drat_proof_handler != nullptr) {
            drat_proof_handler->AddClause({true_lit});
          }
        }
      } else if (assignment.LiteralIsAssigned(Literal(i))) {
        if (!assignment.LiteralIsAssigned(Literal(rep))) {
          const Literal true_lit = assignment.LiteralIsTrue(Literal(i))
                                       ? Literal(rep)
                                       : Literal(rep).Negated();
          solver->AddUnitClause(true_lit);
          if (drat_proof_handler != nullptr) {
            drat_proof_handler->AddClause({true_lit});
          }
        }
      } else if (rep != i) {
        ++num_equiv;
        postsolver->Add(Literal(i), {Literal(i), Literal(rep).Negated()});
        if (drat_proof_handler != nullptr) {
          drat_proof_handler->AddClause({Literal(i), Literal(rep).Negated()});
        }
      }
    }
  }

  const bool log_info =
      solver->parameters().log_search_progress() || VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Probing. fixed " << num_already_fixed_vars << " + "
                         << solver->LiteralTrail().Index() -
                                num_already_fixed_vars
                         << " equiv " << num_equiv / 2 << " total "
                         << solver->NumVariables() << " wtime: " << timer.Get();
}

SatSolver::Status SolveWithPresolve(std::unique_ptr<SatSolver>* solver,
                                    TimeLimit* time_limit,
                                    std::vector<bool>* solution,
                                    DratProofHandler* drat_proof_handler,
                                    SolverLogger* logger) {
  // We save the initial parameters.
  const SatParameters parameters = (*solver)->parameters();
  SatPostsolver postsolver((*solver)->NumVariables());

  const bool log_info = parameters.log_search_progress() || VLOG_IS_ON(1);

  // Some problems are formulated in such a way that our SAT heuristics
  // simply works without conflict. Get them out of the way first because it
  // is possible that the presolve lose this "lucky" ordering. This is in
  // particular the case on the SAT14.crafted.complete-xxx-... problems.
  {
    Model* model = (*solver)->model();
    const double dtime = std::min(1.0, time_limit->GetDeterministicTimeLeft());
    if (!LookForTrivialSatSolution(dtime, model)) {
      VLOG(1) << "UNSAT during probing.";
      return SatSolver::INFEASIBLE;
    }
    const int num_variables = (*solver)->NumVariables();
    if ((*solver)->LiteralTrail().Index() == num_variables) {
      VLOG(1) << "Problem solved by trivial heuristic!";
      solution->clear();
      for (int i = 0; i < (*solver)->NumVariables(); ++i) {
        solution->push_back((*solver)->Assignment().LiteralIsTrue(
            Literal(BooleanVariable(i), true)));
      }
      return SatSolver::FEASIBLE;
    }
  }

  // We use a new block so the memory used by the presolver can be
  // reclaimed as soon as it is no longer needed.
  const int max_num_passes = 4;
  for (int i = 0; i < max_num_passes && !time_limit->LimitReached(); ++i) {
    const int saved_num_variables = (*solver)->NumVariables();

    // Run the new preprocessing code. Note that the probing that it does is
    // faster than the ProbeAndFindEquivalentLiteral() call below, but does not
    // do equivalence detection as completely, so we still apply the other
    // "probing" code afterwards even if it will not fix more literals, but it
    // will do one pass of proper equivalence detection.
    {
      Model* model = (*solver)->model();
      model->GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit);
      SatPresolveOptions options;
      options.log_info = log_info;
      options.extract_binary_clauses_in_probing = false;
      options.use_transitive_reduction = false;
      options.deterministic_time_limit =
          parameters.presolve_probing_deterministic_time_limit();

      if (!model->GetOrCreate<Inprocessing>()->PresolveLoop(options)) {
        VLOG(1) << "UNSAT during probing.";
        return SatSolver::INFEASIBLE;
      }
      for (const auto& c : model->GetOrCreate<PostsolveClauses>()->clauses) {
        postsolver.Add(c[0], c);
      }
    }

    // Probe + find equivalent literals.
    // TODO(user): Use a derived time limit in the probing phase.
    absl::StrongVector<LiteralIndex, LiteralIndex> equiv_map;
    ProbeAndFindEquivalentLiteral((*solver).get(), &postsolver,
                                  drat_proof_handler, &equiv_map);
    if ((*solver)->IsModelUnsat()) {
      VLOG(1) << "UNSAT during probing.";
      return SatSolver::INFEASIBLE;
    }

    // The rest of the presolve only work on pure SAT problem.
    if (!(*solver)->ProblemIsPureSat()) {
      VLOG(1) << "The problem is not a pure SAT problem, skipping the SAT "
                 "specific presolve.";
      break;
    }

    // Register the fixed variables with the postsolver.
    // TODO(user): Find a better place for this?
    (*solver)->Backtrack(0);
    for (int i = 0; i < (*solver)->LiteralTrail().Index(); ++i) {
      postsolver.FixVariable((*solver)->LiteralTrail()[i]);
    }

    // TODO(user): Pass the time_limit to the presolver.
    SatPresolver presolver(&postsolver, logger);
    presolver.SetParameters(parameters);
    presolver.SetDratProofHandler(drat_proof_handler);
    presolver.SetEquivalentLiteralMapping(equiv_map);
    (*solver)->ExtractClauses(&presolver);
    (*solver)->AdvanceDeterministicTime(time_limit);

    // Tricky: the model local time limit is updated by the new functions, but
    // the old ones update time_limit directly.
    time_limit->AdvanceDeterministicTime((*solver)
                                             ->model()
                                             ->GetOrCreate<TimeLimit>()
                                             ->GetElapsedDeterministicTime());

    (*solver).reset(nullptr);
    std::vector<bool> can_be_removed(presolver.NumVariables(), true);
    if (!presolver.Presolve(can_be_removed)) {
      VLOG(1) << "UNSAT during presolve.";

      // This is just here to reset the SatSolver::Solve() statistics.
      (*solver) = absl::make_unique<SatSolver>();
      return SatSolver::INFEASIBLE;
    }

    postsolver.ApplyMapping(presolver.VariableMapping());
    if (drat_proof_handler != nullptr) {
      drat_proof_handler->ApplyMapping(presolver.VariableMapping());
    }

    // Load the presolved problem in a new solver.
    (*solver) = absl::make_unique<SatSolver>();
    (*solver)->SetDratProofHandler(drat_proof_handler);
    (*solver)->SetParameters(parameters);
    presolver.LoadProblemIntoSatSolver((*solver).get());

    // Stop if a fixed point has been reached.
    if ((*solver)->NumVariables() == saved_num_variables) break;
  }

  // Before solving, we use the new probing code that adds all new binary
  // implication it can find to the binary implication graph. This gives good
  // benefits. Note that we currently do not do it before presolve because then
  // the current presolve code does not work too well with the potential huge
  // number of binary clauses added.
  //
  // TODO(user): Revisit the situation when we simplify better all the clauses
  // using binary ones. Or if/when we support at most one better in pure SAT
  // solving and presolve.
  {
    Model* model = (*solver)->model();
    model->GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit);
    SatPresolveOptions options;
    options.log_info = log_info;
    options.use_transitive_reduction = true;
    options.extract_binary_clauses_in_probing = true;
    options.deterministic_time_limit =
        model->GetOrCreate<SatParameters>()
            ->presolve_probing_deterministic_time_limit();
    if (!model->GetOrCreate<Inprocessing>()->PresolveLoop(options)) {
      return SatSolver::INFEASIBLE;
    }
    for (const auto& c : model->GetOrCreate<PostsolveClauses>()->clauses) {
      postsolver.Add(c[0], c);
    }
  }

  // Solve.
  const SatSolver::Status result = (*solver)->SolveWithTimeLimit(time_limit);
  if (result == SatSolver::FEASIBLE) {
    *solution = postsolver.ExtractAndPostsolveSolution(**solver);
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
