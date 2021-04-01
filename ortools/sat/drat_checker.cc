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

#include "ortools/sat/drat_checker.h"

#include <algorithm>
#include <cstdint>
#include <fstream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "ortools/base/hash.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

DratChecker::Clause::Clause(int first_literal_index, int num_literals)
    : first_literal_index(first_literal_index), num_literals(num_literals) {}

std::size_t DratChecker::ClauseHash::operator()(
    const ClauseIndex clause_index) const {
  size_t hash = 0;
  for (Literal literal : checker->Literals(checker->clauses_[clause_index])) {
    hash = util_hash::Hash(literal.Index().value(), hash);
  }
  return hash;
}

bool DratChecker::ClauseEquiv::operator()(
    const ClauseIndex clause_index1, const ClauseIndex clause_index2) const {
  return checker->Literals(checker->clauses_[clause_index1]) ==
         checker->Literals(checker->clauses_[clause_index2]);
}

DratChecker::DratChecker()
    : first_infered_clause_index_(kNoClauseIndex),
      clause_set_(0, ClauseHash(this), ClauseEquiv(this)),
      num_variables_(0) {}

bool DratChecker::Clause::IsDeleted(ClauseIndex clause_index) const {
  return deleted_index <= clause_index;
}

void DratChecker::AddProblemClause(absl::Span<const Literal> clause) {
  DCHECK_EQ(first_infered_clause_index_, kNoClauseIndex);
  const ClauseIndex clause_index = AddClause(clause);

  const auto it = clause_set_.find(clause_index);
  if (it != clause_set_.end()) {
    clauses_[*it].num_copies += 1;
    RemoveLastClause();
  } else {
    clause_set_.insert(clause_index);
  }
}

void DratChecker::AddInferedClause(absl::Span<const Literal> clause) {
  const ClauseIndex infered_clause_index = AddClause(clause);
  if (first_infered_clause_index_ == kNoClauseIndex) {
    first_infered_clause_index_ = infered_clause_index;
  }

  const auto it = clause_set_.find(infered_clause_index);
  if (it != clause_set_.end()) {
    clauses_[*it].num_copies += 1;
    if (*it >= first_infered_clause_index_ && !clause.empty()) {
      CHECK_EQ(clauses_[*it].rat_literal_index, clause[0].Index());
    }
    RemoveLastClause();
  } else {
    clauses_[infered_clause_index].rat_literal_index =
        clause.empty() ? kNoLiteralIndex : clause[0].Index();
    clause_set_.insert(infered_clause_index);
  }
}

ClauseIndex DratChecker::AddClause(absl::Span<const Literal> clause) {
  const int first_literal_index = literals_.size();
  literals_.insert(literals_.end(), clause.begin(), clause.end());
  // Sort the input clause in strictly increasing order (by sorting and then
  // removing the duplicate literals).
  std::sort(literals_.begin() + first_literal_index, literals_.end());
  literals_.erase(
      std::unique(literals_.begin() + first_literal_index, literals_.end()),
      literals_.end());

  for (int i = first_literal_index + 1; i < literals_.size(); ++i) {
    CHECK(literals_[i] != literals_[i - 1].Negated());
  }
  clauses_.push_back(
      Clause(first_literal_index, literals_.size() - first_literal_index));
  if (!clause.empty()) {
    num_variables_ =
        std::max(num_variables_, literals_.back().Variable().value() + 1);
  }
  return ClauseIndex(clauses_.size() - 1);
}

void DratChecker::DeleteClause(absl::Span<const Literal> clause) {
  // Temporarily add 'clause' to find if it has been previously added.
  const auto it = clause_set_.find(AddClause(clause));
  if (it != clause_set_.end()) {
    Clause& existing_clause = clauses_[*it];
    existing_clause.num_copies -= 1;
    if (existing_clause.num_copies == 0) {
      DCHECK(existing_clause.deleted_index == std::numeric_limits<int>::max());
      existing_clause.deleted_index = clauses_.size() - 1;
      if (clauses_.back().num_literals >= 2) {
        clauses_[ClauseIndex(clauses_.size() - 2)].deleted_clauses.push_back(
            *it);
      }
      clause_set_.erase(it);
    }
  } else {
    LOG(WARNING) << "Couldn't find deleted clause";
  }
  // Delete 'clause' and its literals.
  RemoveLastClause();
}

void DratChecker::RemoveLastClause() {
  literals_.resize(clauses_.back().first_literal_index);
  clauses_.pop_back();
}

// See Algorithm of Fig. 8 in 'Trimming while Checking Clausal Proofs'.
DratChecker::Status DratChecker::Check(double max_time_in_seconds) {
  // First check that the last infered clause is empty (this implies there
  // should be at least one infered clause), and mark it as needed for the
  // proof.
  if (clauses_.empty() || first_infered_clause_index_ == kNoClauseIndex ||
      clauses_.back().num_literals != 0) {
    return Status::INVALID;
  }
  clauses_.back().is_needed_for_proof = true;

  // Checks the infered clauses in reversed order. The advantage of this order
  // is that when checking a clause, one can mark all the clauses that are used
  // to check it. In turn, only these marked clauses need to be checked (and so
  // on recursively). By contrast, a forward iteration needs to check all the
  // clauses.
  const int64_t start_time_nanos = absl::GetCurrentTimeNanos();
  TimeLimit time_limit(max_time_in_seconds);
  Init();
  for (ClauseIndex i(clauses_.size() - 1); i >= first_infered_clause_index_;
       --i) {
    if (time_limit.LimitReached()) {
      return Status::UNKNOWN;
    }
    const Clause& clause = clauses_[i];
    // Start watching the literals of the clauses that were deleted just after
    // this one, and which are now no longer deleted.
    for (const ClauseIndex j : clause.deleted_clauses) {
      WatchClause(j);
    }
    if (!clause.is_needed_for_proof) {
      continue;
    }
    // 'clause' must have either the Reverse Unit Propagation (RUP) property:
    if (HasRupProperty(i, Literals(clause))) {
      continue;
    }
    // or the Reverse Asymetric Tautology (RAT) property. This property is
    // defined by the fact that all clauses which contain the negation of
    // the RAT literal of 'clause', after resolution with 'clause', must have
    // the RUP property.
    // Note from 'DRAT-trim: Efficient Checking and Trimming Using Expressive
    // Clausal Proofs': "[in order] to access to all clauses containing the
    // negation of the resolution literal, one could build a literal-to-clause
    // lookup table of the original formula and update it after each lemma
    // addition and deletion step. However, these updates can be expensive and
    // the lookup table potentially doubles the memory usage of the tool.
    // Since most lemmas emitted by state-of-the-art SAT solvers can be
    // validated using the RUP check, such a lookup table has been omitted."
    if (clause.rat_literal_index == kNoLiteralIndex) return Status::INVALID;
    ++num_rat_checks_;
    std::vector<Literal> resolvent;
    for (ClauseIndex j(0); j < i; ++j) {
      if (!clauses_[j].IsDeleted(i) &&
          ContainsLiteral(Literals(clauses_[j]),
                          Literal(clause.rat_literal_index).Negated())) {
        // Check that the resolvent has the RUP property.
        if (!Resolve(Literals(clause), Literals(clauses_[j]),
                     Literal(clause.rat_literal_index), &tmp_assignment_,
                     &resolvent) ||
            !HasRupProperty(i, resolvent)) {
          return Status::INVALID;
        }
      }
    }
  }
  LogStatistics(absl::GetCurrentTimeNanos() - start_time_nanos);
  return Status::VALID;
}

std::vector<std::vector<Literal>> DratChecker::GetUnsatSubProblem() const {
  return GetClausesNeededForProof(ClauseIndex(0), first_infered_clause_index_);
}

std::vector<std::vector<Literal>> DratChecker::GetOptimizedProof() const {
  return GetClausesNeededForProof(first_infered_clause_index_,
                                  ClauseIndex(clauses_.size()));
}

std::vector<std::vector<Literal>> DratChecker::GetClausesNeededForProof(
    ClauseIndex begin, ClauseIndex end) const {
  std::vector<std::vector<Literal>> result;
  for (ClauseIndex i = begin; i < end; ++i) {
    const Clause& clause = clauses_[i];
    if (clause.is_needed_for_proof) {
      const absl::Span<const Literal>& literals = Literals(clause);
      result.emplace_back(literals.begin(), literals.end());
      if (clause.rat_literal_index != kNoLiteralIndex) {
        const int rat_literal_clause_index =
            std::find(literals.begin(), literals.end(),
                      Literal(clause.rat_literal_index)) -
            literals.begin();
        std::swap(result.back()[0], result.back()[rat_literal_clause_index]);
      }
    }
  }
  return result;
}

absl::Span<const Literal> DratChecker::Literals(const Clause& clause) const {
  return absl::Span<const Literal>(
      literals_.data() + clause.first_literal_index, clause.num_literals);
}

void DratChecker::Init() {
  assigned_.clear();
  assignment_.Resize(num_variables_);
  assignment_source_.resize(num_variables_, kNoClauseIndex);
  high_priority_literals_to_assign_.clear();
  low_priority_literals_to_assign_.clear();
  watched_literals_.clear();
  watched_literals_.resize(2 * num_variables_);
  single_literal_clauses_.clear();
  unit_stack_.clear();
  tmp_assignment_.Resize(num_variables_);
  num_rat_checks_ = 0;

  for (ClauseIndex clause_index(0); clause_index < clauses_.size();
       ++clause_index) {
    Clause& clause = clauses_[clause_index];
    if (clause.num_literals >= 2) {
      // Don't watch the literals of the deleted clauses right away, instead
      // watch them when these clauses become 'undeleted' in backward checking.
      if (clause.deleted_index == std::numeric_limits<int>::max()) {
        WatchClause(clause_index);
      }
    } else if (clause.num_literals == 1) {
      single_literal_clauses_.push_back(clause_index);
    }
  }
}

void DratChecker::WatchClause(ClauseIndex clause_index) {
  const Literal* clause_literals =
      literals_.data() + clauses_[clause_index].first_literal_index;
  watched_literals_[clause_literals[0].Index()].push_back(clause_index);
  watched_literals_[clause_literals[1].Index()].push_back(clause_index);
}

bool DratChecker::HasRupProperty(ClauseIndex num_clauses,
                                 absl::Span<const Literal> clause) {
  ClauseIndex conflict = kNoClauseIndex;
  for (const Literal literal : clause) {
    conflict =
        AssignAndPropagate(num_clauses, literal.Negated(), kNoClauseIndex);
    if (conflict != kNoClauseIndex) {
      break;
    }
  }

  for (const ClauseIndex clause_index : single_literal_clauses_) {
    const Clause& clause = clauses_[clause_index];
    // TODO(user): consider ignoring the deletion of single literal clauses
    // as done in drat-trim.
    if (clause_index < num_clauses && !clause.IsDeleted(num_clauses)) {
      if (clause.is_needed_for_proof) {
        high_priority_literals_to_assign_.push_back(
            {literals_[clause.first_literal_index], clause_index});
      } else {
        low_priority_literals_to_assign_.push_back(
            {literals_[clause.first_literal_index], clause_index});
      }
    }
  }

  while (!(high_priority_literals_to_assign_.empty() &&
           low_priority_literals_to_assign_.empty()) &&
         conflict == kNoClauseIndex) {
    std::vector<LiteralToAssign>& stack =
        high_priority_literals_to_assign_.empty()
            ? low_priority_literals_to_assign_
            : high_priority_literals_to_assign_;
    const LiteralToAssign literal_to_assign = stack.back();
    stack.pop_back();
    if (assignment_.LiteralIsAssigned(literal_to_assign.literal)) {
      // If the literal to assign to true is already assigned to false, we found
      // a conflict, with the source clause of this previous assignment.
      if (assignment_.LiteralIsFalse(literal_to_assign.literal)) {
        conflict = literal_to_assign.source_clause_index;
        break;
      } else {
        continue;
      }
    }
    DCHECK(literal_to_assign.source_clause_index != kNoClauseIndex);
    unit_stack_.push_back(literal_to_assign.source_clause_index);
    conflict = AssignAndPropagate(num_clauses, literal_to_assign.literal,
                                  literal_to_assign.source_clause_index);
  }
  if (conflict != kNoClauseIndex) {
    MarkAsNeededForProof(&clauses_[conflict]);
  }

  for (const Literal literal : assigned_) {
    assignment_.UnassignLiteral(literal);
  }
  assigned_.clear();
  high_priority_literals_to_assign_.clear();
  low_priority_literals_to_assign_.clear();
  unit_stack_.clear();

  return conflict != kNoClauseIndex;
}

ClauseIndex DratChecker::AssignAndPropagate(ClauseIndex num_clauses,
                                            Literal literal,
                                            ClauseIndex source_clause_index) {
  assigned_.push_back(literal);
  assignment_.AssignFromTrueLiteral(literal);
  assignment_source_[literal.Variable()] = source_clause_index;

  const Literal false_literal = literal.Negated();
  std::vector<ClauseIndex>& watched = watched_literals_[false_literal.Index()];
  int new_watched_size = 0;
  ClauseIndex conflict_index = kNoClauseIndex;
  for (const ClauseIndex clause_index : watched) {
    if (clause_index >= num_clauses) {
      // Stop watching the literals of clauses which cannot possibly be
      // necessary to check the rest of the proof.
      continue;
    }
    Clause& clause = clauses_[clause_index];
    DCHECK(!clause.IsDeleted(num_clauses));
    if (conflict_index != kNoClauseIndex) {
      watched[new_watched_size++] = clause_index;
      continue;
    }

    Literal* clause_literals = literals_.data() + clause.first_literal_index;
    const Literal other_watched_literal(LiteralIndex(
        clause_literals[0].Index().value() ^
        clause_literals[1].Index().value() ^ false_literal.Index().value()));
    if (assignment_.LiteralIsTrue(other_watched_literal)) {
      watched[new_watched_size++] = clause_index;
      continue;
    }

    bool new_watched_literal_found = false;
    for (int i = 2; i < clause.num_literals; ++i) {
      if (!assignment_.LiteralIsFalse(clause_literals[i])) {
        clause_literals[0] = other_watched_literal;
        clause_literals[1] = clause_literals[i];
        clause_literals[i] = false_literal;
        watched_literals_[clause_literals[1].Index()].push_back(clause_index);
        new_watched_literal_found = true;
        break;
      }
    }

    if (!new_watched_literal_found) {
      if (assignment_.LiteralIsFalse(other_watched_literal)) {
        // 'clause' is falsified with 'assignment_', we found a conflict.
        // TODO(user): test moving the rest of the vector here and
        // returning right away.
        conflict_index = clause_index;
      } else {
        DCHECK(!assignment_.LiteralIsAssigned(other_watched_literal));
        // 'clause' is unit, push its unit literal on
        // 'literals_to_assign_high_priority' or
        // 'literals_to_assign_low_priority' to assign it to true and propagate
        // it in a later call to AssignAndPropagate().
        if (clause.is_needed_for_proof) {
          high_priority_literals_to_assign_.push_back(
              {other_watched_literal, clause_index});
        } else {
          low_priority_literals_to_assign_.push_back(
              {other_watched_literal, clause_index});
        }
      }
      watched[new_watched_size++] = clause_index;
    }
  }
  watched.resize(new_watched_size);
  return conflict_index;
}

void DratChecker::MarkAsNeededForProof(Clause* clause) {
  const auto mark_clause_and_sources = [&](Clause* clause) {
    clause->is_needed_for_proof = true;
    for (const Literal literal : Literals(*clause)) {
      const ClauseIndex source_clause_index =
          assignment_source_[literal.Variable()];
      if (source_clause_index != kNoClauseIndex) {
        clauses_[source_clause_index].tmp_is_needed_for_proof_step = true;
      }
    }
  };
  mark_clause_and_sources(clause);
  for (int i = unit_stack_.size() - 1; i >= 0; --i) {
    Clause& unit_clause = clauses_[unit_stack_[i]];
    if (unit_clause.tmp_is_needed_for_proof_step) {
      mark_clause_and_sources(&unit_clause);
      // We can clean this flag here without risking missing clauses needed for
      // the proof, because the clauses needed for a clause C are always lower
      // than C in the stack.
      unit_clause.tmp_is_needed_for_proof_step = false;
    }
  }
}

void DratChecker::LogStatistics(int64_t duration_nanos) const {
  int problem_clauses_needed_for_proof = 0;
  int infered_clauses_needed_for_proof = 0;
  for (ClauseIndex i(0); i < clauses_.size(); ++i) {
    if (clauses_[i].is_needed_for_proof) {
      if (i < first_infered_clause_index_) {
        ++problem_clauses_needed_for_proof;
      } else {
        ++infered_clauses_needed_for_proof;
      }
    }
  }
  LOG(INFO) << problem_clauses_needed_for_proof
            << " problem clauses needed for proof, out of "
            << first_infered_clause_index_;
  LOG(INFO) << infered_clauses_needed_for_proof
            << " infered clauses needed for proof, out of "
            << clauses_.size() - first_infered_clause_index_;
  LOG(INFO) << num_rat_checks_ << " RAT infered clauses";
  LOG(INFO) << "verification time: " << 1e-9 * duration_nanos << " s";
}

bool ContainsLiteral(absl::Span<const Literal> clause, Literal literal) {
  return std::find(clause.begin(), clause.end(), literal) != clause.end();
}

bool Resolve(absl::Span<const Literal> clause,
             absl::Span<const Literal> other_clause,
             Literal complementary_literal, VariablesAssignment* assignment,
             std::vector<Literal>* resolvent) {
  DCHECK(ContainsLiteral(clause, complementary_literal));
  DCHECK(ContainsLiteral(other_clause, complementary_literal.Negated()));
  resolvent->clear();

  for (const Literal literal : clause) {
    if (literal != complementary_literal) {
      // Temporary assignment used to do the checks below in linear time.
      assignment->AssignFromTrueLiteral(literal);
      resolvent->push_back(literal);
    }
  }

  bool result = true;
  for (const Literal other_literal : other_clause) {
    if (other_literal != complementary_literal.Negated()) {
      if (assignment->LiteralIsFalse(other_literal)) {
        result = false;
        break;
      } else if (!assignment->LiteralIsAssigned(other_literal)) {
        resolvent->push_back(other_literal);
      }
    }
  }

  // Revert the temporary assignment done above.
  for (const Literal literal : clause) {
    if (literal != complementary_literal) {
      assignment->UnassignLiteral(literal);
    }
  }
  return result;
}

bool AddProblemClauses(const std::string& file_path,
                       DratChecker* drat_checker) {
  int line_number = 0;
  int num_variables = 0;
  int num_clauses = 0;
  std::vector<Literal> literals;
  std::ifstream file(file_path);
  std::string line;
  bool result = true;
  while (std::getline(file, line)) {
    line_number++;
    std::vector<absl::string_view> words =
        absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipWhitespace());
    if (words.empty() || words[0] == "c") {
      // Ignore empty and comment lines.
      continue;
    }
    if (words[0] == "p") {
      if (num_clauses > 0 || words.size() != 4 || words[1] != "cnf" ||
          !absl::SimpleAtoi(words[2], &num_variables) || num_variables <= 0 ||
          !absl::SimpleAtoi(words[3], &num_clauses) || num_clauses <= 0) {
        LOG(ERROR) << "Invalid content '" << line << "' at line " << line_number
                   << " of " << file_path;
        result = false;
        break;
      }
      continue;
    }
    literals.clear();
    for (int i = 0; i < words.size(); ++i) {
      int signed_value;
      if (!absl::SimpleAtoi(words[i], &signed_value) ||
          std::abs(signed_value) > num_variables ||
          (signed_value == 0 && i != words.size() - 1)) {
        LOG(ERROR) << "Invalid content '" << line << "' at line " << line_number
                   << " of " << file_path;
        result = false;
        break;
      }
      if (signed_value != 0) {
        literals.push_back(Literal(signed_value));
      }
    }
    drat_checker->AddProblemClause(literals);
  }
  file.close();
  return result;
}

bool AddInferedAndDeletedClauses(const std::string& file_path,
                                 DratChecker* drat_checker) {
  int line_number = 0;
  bool ends_with_empty_clause = false;
  std::vector<Literal> literals;
  std::ifstream file(file_path);
  std::string line;
  bool result = true;
  while (std::getline(file, line)) {
    line_number++;
    std::vector<absl::string_view> words =
        absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipWhitespace());
    bool delete_clause = !words.empty() && words[0] == "d";
    literals.clear();
    for (int i = (delete_clause ? 1 : 0); i < words.size(); ++i) {
      int signed_value;
      if (!absl::SimpleAtoi(words[i], &signed_value) ||
          (signed_value == 0 && i != words.size() - 1)) {
        LOG(ERROR) << "Invalid content '" << line << "' at line " << line_number
                   << " of " << file_path;
        result = false;
        break;
      }
      if (signed_value != 0) {
        literals.push_back(Literal(signed_value));
      }
    }
    if (delete_clause) {
      drat_checker->DeleteClause(literals);
      ends_with_empty_clause = false;
    } else {
      drat_checker->AddInferedClause(literals);
      ends_with_empty_clause = literals.empty();
    }
  }
  if (!ends_with_empty_clause) {
    drat_checker->AddInferedClause({});
  }
  file.close();
  return result;
}

bool PrintClauses(const std::string& file_path, SatFormat format,
                  const std::vector<std::vector<Literal>>& clauses,
                  int num_variables) {
  std::ofstream output_stream(file_path, std::ofstream::out);
  if (format == DIMACS) {
    output_stream << "p cnf " << num_variables << " " << clauses.size() << "\n";
  }
  for (const auto& clause : clauses) {
    for (Literal literal : clause) {
      output_stream << literal.SignedValue() << " ";
    }
    output_stream << "0\n";
  }
  output_stream.close();
  return output_stream.good();
}

}  // namespace sat
}  // namespace operations_research
