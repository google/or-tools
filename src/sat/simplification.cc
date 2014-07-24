// Copyright 2010-2014 Google
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

#include "sat/simplification.h"

#include "base/timer.h"

namespace operations_research {
namespace sat {

void SatPostsolver::Postsolve(VariablesAssignment* assignment) const {
  // First, we set all unassigned variable to true.
  // This will be a valid assignment of the presolved problem.
  for (VariableIndex var(0); var < assignment->NumberOfVariables(); ++var) {
    if (!assignment->IsVariableAssigned(var)) {
      assignment->AssignFromTrueLiteral(Literal(var, true));
    }
  }

  for (int i = clauses_.size() - 1; i >= 0; --i) {
    bool set_associated_var = true;
    for (const Literal l : clauses_[i]) {
      if (assignment->IsLiteralFalse(l)) continue;
      set_associated_var = false;
      break;
    }
    if (set_associated_var) {
      // Note(user): The VariablesAssignment interface is a bit weird in this
      // context, because we can only assign an unassigned literal.
      assignment->UnassignLiteral(associated_literal_[i]);
      assignment->AssignFromTrueLiteral(associated_literal_[i]);
    }
  }
}

void SatPresolver::AddBinaryClause(Literal a, Literal b) {
  Literal c[2];
  c[0] = a;
  c[1] = b;
  AddClause(ClauseRef(c, c + 2));
}

void SatPresolver::AddClause(ClauseRef clause) {
  CHECK_GT(clause.size(), 0) << "TODO(fdid): Unsat during presolve?";
  const ClauseIndex ci(clauses_.size());
  clauses_.push_back(std::vector<Literal>(clause.begin(), clause.end()));
  in_clause_to_process_.push_back(true);
  clause_to_process_.push_back(ci);
  std::sort(clauses_.back().begin(), clauses_.back().end());
  const Literal max_literal = clauses_.back().back();
  const int required_size =
      std::max(max_literal.Index().value(), max_literal.NegatedIndex().value()) + 1;
  if (required_size > literal_to_clauses_.size()) {
    literal_to_clauses_.resize(required_size);
    literal_to_clauses_sizes_.resize(required_size);
  }
  for (Literal e : clauses_.back()) {
    literal_to_clauses_[e.Index()].push_back(ci);
    literal_to_clauses_sizes_[e.Index()]++;
    UpdatePriorityQueue(e.Variable());
  }
}

ITIVector<VariableIndex, VariableIndex> SatPresolver::GetMapping(
    int* new_size) const {
  ITIVector<VariableIndex, VariableIndex> result;
  VariableIndex new_var(0);
  for (VariableIndex var(0); var < NumVariables(); ++var) {
    if (literal_to_clauses_sizes_[Literal(var, true).Index()] > 0 ||
        literal_to_clauses_sizes_[Literal(var, false).Index()] > 0) {
      result.push_back(new_var);
      ++new_var;
    } else {
      result.push_back(VariableIndex(-1));
    }
  }
  *new_size = new_var.value();
  return result;
}

bool SatPresolver::ProcessAllClauses() {
  while (!clause_to_process_.empty()) {
    const ClauseIndex ci = clause_to_process_.front();
    in_clause_to_process_[ci] = false;
    clause_to_process_.pop_front();
    if (!ProcessClauseToSimplifyOthers(ci)) return false;
  }
  return true;
}

bool SatPresolver::Presolve() {
  WallTimer timer;
  timer.Start();
  DisplayStats(0);

  // TODO(user): When a clause is strengthened, add it to a queue so it can
  // be processed again?
  if (!ProcessAllClauses()) return false;
  DisplayStats(timer.Get());

  InitializePriorityQueue();
  while (var_pq_.Size() > 0) {
    VariableIndex var = var_pq_.Top()->variable;
    var_pq_.Pop();
    if (CrossProduct(Literal(var, true))) {
      if (!ProcessAllClauses()) return false;
    }
  }

  DisplayStats(timer.Get());
  return true;
}

// TODO(user): Binary clauses are really common, and we can probably do this
// more efficiently for them. For instance, we could just take the intersection
// of two sorted lists to get the simplified clauses.
//
// TODO(user): SimplifyClause can returns true only if the variables in 'a' are
// a subset of the one in 'b'. Use an int64 signature for speeding up the test.
bool SatPresolver::ProcessClauseToSimplifyOthers(ClauseIndex clause_index) {
  const std::vector<Literal>& clause = clauses_[clause_index];
  if (clause.empty()) return true;
  DCHECK(std::is_sorted(clause.begin(), clause.end()));

  LiteralIndex opposite_literal;
  const Literal lit = FindLiteralWithShortestOccurenceList(clause);

  // Try to simplify the clauses containing 'lit'. We take advantage of this
  // loop to also remove the empty sets from the list.
  {
    int new_index = 0;
    std::vector<ClauseIndex>& occurence_list_ref = literal_to_clauses_[lit.Index()];
    for (ClauseIndex ci : occurence_list_ref) {
      if (clauses_[ci].empty()) continue;
      if (ci != clause_index &&
          SimplifyClause(clause, &clauses_[ci], &opposite_literal)) {
        if (opposite_literal == LiteralIndex(-1)) {
          Remove(ci);
          continue;
        } else {
          CHECK_NE(opposite_literal, lit.Index());
          if (clauses_[ci].empty()) return false;  // UNSAT.
          // Remove ci from the occurence list. Note that the occurence list
          // can't be shortest_list or its negation.
          auto iter =
              std::find(literal_to_clauses_[opposite_literal].begin(),
                        literal_to_clauses_[opposite_literal].end(), ci);
          DCHECK(iter != literal_to_clauses_[opposite_literal].end());
          literal_to_clauses_[opposite_literal].erase(iter);

          --literal_to_clauses_sizes_[opposite_literal];
          UpdatePriorityQueue(Literal(opposite_literal).Variable());

          if (!in_clause_to_process_[ci]) {
            in_clause_to_process_[ci] = true;
            clause_to_process_.push_back(ci);
          }
        }
      }
      occurence_list_ref[new_index] = ci;
      ++new_index;
    }
    occurence_list_ref.resize(new_index);
    CHECK_EQ(literal_to_clauses_sizes_[lit.Index()], new_index);
    literal_to_clauses_sizes_[lit.Index()] = new_index;
  }

  // Now treat clause containing lit.Negated().
  // TODO(user): choose a potentially smaller list.
  {
    int new_index = 0;
    bool something_removed = false;
    std::vector<ClauseIndex>& occurence_list_ref =
        literal_to_clauses_[lit.NegatedIndex()];
    for (ClauseIndex ci : occurence_list_ref) {
      if (clauses_[ci].empty()) continue;

      // TODO(user): not super optimal since we could abort earlier if
      // opposite_literal is not the negation of shortest_list.
      if (SimplifyClause(clause, &clauses_[ci], &opposite_literal)) {
        CHECK_EQ(opposite_literal, lit.NegatedIndex());
        if (clauses_[ci].empty()) return false;  // UNSAT.
        if (!in_clause_to_process_[ci]) {
          in_clause_to_process_[ci] = true;
          clause_to_process_.push_back(ci);
        }
        something_removed = true;
        continue;
      }
      occurence_list_ref[new_index] = ci;
      ++new_index;
    }
    occurence_list_ref.resize(new_index);
    literal_to_clauses_sizes_[lit.NegatedIndex()] = new_index;
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
  literal_to_clauses_[x.Index()].clear();
  literal_to_clauses_sizes_[x.Index()] = 0;
}

bool SatPresolver::CrossProduct(Literal x) {
  const int s1 = literal_to_clauses_sizes_[x.Index()];
  const int s2 = literal_to_clauses_sizes_[x.NegatedIndex()];

  // Note that if s1 or s2 is equal to 0, this function will implicitely just
  // fix the variable x.
  if (s1 == 0 && s2 == 0) return false;

  std::vector<Literal> temp;
  if (s1 > 1 && s2 > 1) {
    // Heuristic. Abort if the work required to decide if x should be removed
    // seems to big.
    if (s1 * s2 > parameters_.presolve_bce_threshold()) return false;

    // Test whether we should remove the variable x.Variable().
    int size = 0;
    for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
      if (clauses_[i].empty()) continue;
      const int old_size = size;
      for (ClauseIndex j : literal_to_clauses_[x.NegatedIndex()]) {
        if (clauses_[j].empty()) continue;

        // TODO(user): The output temp is not needed here. Optimize.
        if (ComputeResolvant(x, clauses_[i], clauses_[j], &temp)) {
          ++size;

          // Abort early if the resolution is producing more clauses than the
          // ones it will eliminate.
          if (size > s1 + s2) return false;
        }
      }
      if (size == old_size) {
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
  }

  // Add all the resolvant clauses.
  for (ClauseIndex i : literal_to_clauses_[x.Index()]) {
    if (clauses_[i].empty()) continue;
    for (ClauseIndex j : literal_to_clauses_[x.NegatedIndex()]) {
      if (clauses_[j].empty()) continue;
      if (ComputeResolvant(x, clauses_[i], clauses_[j], &temp)) {
        AddClause(ClauseRef(temp));
      }
    }
  }

  // Deletes the old clauses.
  RemoveAndRegisterForPostsolveAllClauseContaining(x);
  RemoveAndRegisterForPostsolveAllClauseContaining(x.Negated());

  // TODO(user): At this point x.Variable() is added back to the priority queue.
  // Avoid doing that.
  return true;
}

void SatPresolver::Remove(ClauseIndex ci) {
  for (Literal e : clauses_[ci]) {
    literal_to_clauses_sizes_[e.Index()]--;
    UpdatePriorityQueue(e.Variable());
  }
  clauses_[ci].clear();
}

void SatPresolver::RemoveAndRegisterForPostsolve(ClauseIndex ci, Literal x) {
  for (Literal e : clauses_[ci]) {
    literal_to_clauses_sizes_[e.Index()]--;
    UpdatePriorityQueue(e.Variable());
  }
  // This will copy and clear the clause.
  postsolver_.Add(x, &clauses_[ci]);
}

Literal SatPresolver::FindLiteralWithShortestOccurenceList(
    const std::vector<Literal>& clause) {
  CHECK(!clause.empty());
  Literal result = clause.front();
  for (Literal l : clause) {
    if (literal_to_clauses_sizes_[l.Index()] <
        literal_to_clauses_sizes_[result.Index()]) {
      result = l;
    }
  }
  return result;
}

void SatPresolver::UpdatePriorityQueue(VariableIndex var) {
  if (var_pq_elements_.empty()) return;  // not initialized.
  PQElement* element = &var_pq_elements_[var];
  element->weight = literal_to_clauses_sizes_[Literal(var, true).Index()] +
                    literal_to_clauses_sizes_[Literal(var, false).Index()];
  if (var_pq_.Contains(element)) {
    var_pq_.NoteChangedPriority(element);
  } else {
    var_pq_.Add(element);
  }
}

void SatPresolver::InitializePriorityQueue() {
  const int num_vars = NumVariables();
  var_pq_elements_.resize(num_vars);
  for (VariableIndex var(0); var < num_vars; ++var) {
    PQElement* element = &var_pq_elements_[var];
    element->variable = var;
    element->weight = literal_to_clauses_sizes_[Literal(var, true).Index()] +
                      literal_to_clauses_sizes_[Literal(var, false).Index()];
    var_pq_.Add(element);
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
  for (VariableIndex var(0); var < NumVariables(); ++var) {
    const int s1 = literal_to_clauses_sizes_[Literal(var, true).Index()];
    const int s2 = literal_to_clauses_sizes_[Literal(var, false).Index()];
    if (s1 == 0 && s2 == 0) continue;

    ++num_vars;
    if (s1 == 0 || s2 == 0) {
      num_one_side++;
    } else if (s1 == 1 || s2 == 1) {
      num_simple_definition++;
    }
  }
  LOG(INFO) << " [" << elapsed_seconds << "s]"
            << " clauses:" << num_clauses << " literals:" << num_literals
            << " vars:" << num_vars << " one_side_vars:" << num_one_side
            << " simple_definition:" << num_simple_definition
            << " singleton_clauses:" << num_singleton_clauses;
}

bool SimplifyClause(const std::vector<Literal>& a, std::vector<Literal>* b,
                    LiteralIndex* opposite_literal) {
  if (b->size() < a.size()) return false;
  DCHECK(std::is_sorted(a.begin(), a.end()));
  DCHECK(std::is_sorted(b->begin(), b->end()));

  *opposite_literal = LiteralIndex(-1);

  int num_diff = 0;
  std::vector<Literal>::const_iterator ia = a.begin();
  std::vector<Literal>::iterator ib = b->begin();
  std::vector<Literal>::iterator to_remove = b->begin();

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

bool ComputeResolvant(Literal x, const std::vector<Literal>& a,
                      const std::vector<Literal>& b, std::vector<Literal>* out) {
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

}  // namespace sat
}  // namespace operations_research
