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

#include "ortools/sat/sat_inprocessing.h"

#include "absl/container/inlined_vector.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/sat/probing.h"

namespace operations_research {
namespace sat {

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

bool Inprocessing::PresolveLoop(SatPresolveOptions options) {
  WallTimer wall_timer;
  wall_timer.Start();

  const bool log_info = options.log_info || VLOG_IS_ON(1);
  const bool log_round_info = VLOG_IS_ON(1);

  // Mainly useful for development.
  double probing_time = 0.0;

  // We currently do the transformations in a given order and restart each time
  // we did something to make sure that the earlier step cannot srengthen more.
  // This might not be the best, but it is really good during development phase
  // to make sure each individual functions is as incremental and as fast as
  // possible.
  const double start_dtime = time_limit_->GetElapsedDeterministicTime();
  const double stop_dtime = start_dtime + options.deterministic_time_limit;
  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= stop_dtime) {
    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    if (!LevelZeroPropagate()) return false;

    // This one is fast since only newly fixed variables are considered.
    implication_graph_->RemoveFixedVariables();

    // This also prepare the stamping below so that we do that on a DAG and do
    // not consider potential new implications added by
    // RemoveFixedAndEquivalentVariables().
    RETURN_IF_FALSE(DetectEquivalencesAndStamp(log_round_info));

    // TODO(user): This should/could be integrated with the stamping since it
    // seems better to do just one loop instead of two over all clauses. Because
    // of memory access. it isn't that clear though.
    RETURN_IF_FALSE(RemoveFixedAndEquivalentVariables(log_round_info));
    RETURN_IF_FALSE(stamping_simplifier_->DoOneRound(log_round_info));

    // We wait for the fix-point to be reached before doing the other
    // simplifications below.
    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    RETURN_IF_FALSE(SubsumeAndStrenghtenRound(log_round_info));
    if (MoreFixedVariableToClean() || MoreRedundantVariableToClean() ||
        !implication_graph_->IsDag()) {
      continue;
    }

    // Probing.
    const double saved_wtime = wall_timer.Get();
    const double time_left =
        stop_dtime - time_limit_->GetElapsedDeterministicTime();
    if (time_left <= 0) break;
    ProbingOptions probing_options;
    probing_options.log_info = log_round_info;
    probing_options.deterministic_limit = time_left;
    probing_options.extract_binary_clauses =
        options.extract_binary_clauses_in_probing;
    RETURN_IF_FALSE(FailedLiteralProbingRound(probing_options, model_));
    probing_time += wall_timer.Get() - saved_wtime;

    // TODO(user): not sure when to run this one.
    if (options.use_transitive_reduction) {
      RETURN_IF_FALSE(
          implication_graph_->ComputeTransitiveReduction(log_round_info));
      RETURN_IF_FALSE(LevelZeroPropagate());
    }

    // TODO(user): Maintain the total number of literals in the watched clauses.
    LOG_IF(INFO, log_info)
        << "Presolve."
        << " num_fixed: " << trail_->Index() << " num_redundant: "
        << implication_graph_->num_redundant_literals() / 2 << "/"
        << sat_solver_->NumVariables()
        << " num_implications: " << implication_graph_->num_implications()
        << " num_watched_clauses: " << clause_manager_->num_watched_clauses()
        << " dtime: "
        << time_limit_->GetElapsedDeterministicTime() - start_dtime << "/"
        << options.deterministic_time_limit << " wtime: " << wall_timer.Get();

    if (!MoreFixedVariableToClean() && !MoreRedundantVariableToClean() &&
        implication_graph_->IsDag()) {
      break;
    }
  }

  // TODO(user): consider doing transitive reduction here before bounded
  // variable elimination.

  LOG_IF(INFO, log_info) << "Presolve done. non-probing time: "
                         << (wall_timer.Get() - probing_time);
  return LevelZeroPropagate();
}

#undef RETURN_IF_FALSE

bool Inprocessing::MoreFixedVariableToClean() const {
  const int64 new_num_fixed_variables = trail_->Index();
  return last_num_fixed_variables_ < new_num_fixed_variables;
}

bool Inprocessing::MoreRedundantVariableToClean() const {
  const int64 new_num_redundant_literals =
      implication_graph_->num_redundant_literals();
  return last_num_redundant_literals_ < new_num_redundant_literals;
}

bool Inprocessing::LevelZeroPropagate() {
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  clause_manager_->AttachAllClauses();
  return sat_solver_->Propagate();
}

// It make sense to do the pre-stamping right after the equivalence detection
// since it needs a DAG and can detect extra failed literal.
bool Inprocessing::DetectEquivalencesAndStamp(bool log_info) {
  if (!LevelZeroPropagate()) return false;
  implication_graph_->RemoveFixedVariables();
  if (!implication_graph_->DetectEquivalences(log_info)) return false;
  if (!LevelZeroPropagate()) return false;
  if (!stamping_simplifier_->ComputeStampsForNextRound(log_info)) return false;
  if (!LevelZeroPropagate()) return false;
  return true;
}

bool Inprocessing::RemoveFixedAndEquivalentVariables(bool log_info) {
  // Preconditions.
  //
  // TODO(user): The level zero is required because we remove fixed variables
  // but if we split this into two functions, we could rewrite clause at any
  // level.
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);

  // Test if some work is needed.
  //
  // TODO(user): If only new fixed variables are there, we can use a faster
  // function. We should also merge the code with the deletion code in
  // sat_solver_.cc, but that require some refactoring of the dependence between
  // files.
  const int64 new_num_redundant_literals =
      implication_graph_->num_redundant_literals();
  const int64 new_num_fixed_variables = trail_->Index();
  if (last_num_redundant_literals_ == new_num_redundant_literals &&
      last_num_fixed_variables_ == new_num_fixed_variables) {
    return true;
  }
  last_num_fixed_variables_ = new_num_fixed_variables;
  last_num_redundant_literals_ = new_num_redundant_literals;

  // Start the round.
  WallTimer wall_timer;
  wall_timer.Start();

  int64 num_removed_literals = 0;
  int64 num_inspected_literals = 0;

  // We need this temporary vector for the DRAT proof settings, otherwise
  // we could just have done an in-place transformation.
  std::vector<Literal> new_clause;

  // Used to mark clause literals.
  const int num_literals(sat_solver_->NumVariables() * 2);
  gtl::ITIVector<LiteralIndex, bool> marked(num_literals, false);

  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    bool removed = false;
    bool need_rewrite = false;

    // We first do a loop to see if there is anything to do.
    for (const Literal l : clause->AsSpan()) {
      if (assignment_.LiteralIsTrue(l)) {
        // TODO(user): we should output literal to the proof right away,
        // currently if we remove clauses before fixing literal the proof is
        // wrong.
        clause_manager_->InprocessingFixLiteral(l);
        clause_manager_->InprocessingRemoveClause(clause);
        num_removed_literals += clause->Size();
        removed = true;
        break;
      }
      if (assignment_.LiteralIsFalse(l) || implication_graph_->IsRedundant(l)) {
        need_rewrite = true;
        break;
      }
    }

    num_inspected_literals += clause->Size();
    if (removed || !need_rewrite) continue;
    num_inspected_literals += clause->Size();

    // Rewrite the clause.
    new_clause.clear();
    for (const Literal l : clause->AsSpan()) {
      const Literal r = implication_graph_->RepresentativeOf(l);
      if (marked[r.Index()] || assignment_.LiteralIsFalse(r)) {
        continue;
      }
      if (marked[r.NegatedIndex()] || assignment_.LiteralIsTrue(r)) {
        clause_manager_->InprocessingRemoveClause(clause);
        num_removed_literals += clause->Size();
        removed = true;
        break;
      }
      marked[r.Index()] = true;
      new_clause.push_back(r);
    }

    // Restore marked.
    for (const Literal l : new_clause) marked[l.Index()] = false;
    if (removed) continue;

    num_removed_literals += clause->Size() - new_clause.size();
    if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
      return false;
    }
  }

  // TODO(user): find a way to auto-tune that after a run on borg...
  const double dtime = static_cast<double>(num_inspected_literals) * 1e-8;
  time_limit_->AdvanceDeterministicTime(dtime);
  LOG_IF(INFO, log_info) << "Cleanup. num_removed_literals: "
                         << num_removed_literals << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get();
  return true;
}

// TODO(user): Use better work limits, see SAT09.CRAFTED.ramseycube.Q3inK12
//
// TODO(user): Be more incremental, each time a clause is added/reduced track
// which literal are impacted? Also try to do orthogonal reductions from one
// round to the next.
bool Inprocessing::SubsumeAndStrenghtenRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  int64 num_subsumed_clauses = 0;
  int64 num_removed_literals = 0;
  int64 num_inspected_signatures = 0;
  int64 num_inspected_literals = 0;

  // We need this temporary vector for the DRAT proof settings, otherwise
  // we could just have done an in-place transformation.
  std::vector<Literal> new_clause;

  // This function needs the watcher to be detached as we might remove some
  // of the watched literals.
  //
  // TODO(user): We could do that only if we do some reduction, but this is
  // quite fast though.
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();

  // Process clause by increasing sizes.
  // TODO(user): probably faster without the size indirection.
  std::vector<SatClause*> clauses =
      clause_manager_->AllClausesInCreationOrder();
  std::sort(clauses.begin(), clauses.end(),
            [](SatClause* a, SatClause* b) { return a->Size() < b->Size(); });

  // Used to mark clause literals.
  const LiteralIndex num_literals(sat_solver_->NumVariables() * 2);
  SparseBitset<LiteralIndex> marked(num_literals);

  // Clause index in clauses.
  // TODO(user): Storing signatures here might be faster?
  gtl::ITIVector<LiteralIndex, absl::InlinedVector<int, 6>> one_watcher(
      num_literals.value());

  // Clause signatures in the same order as clauses.
  std::vector<uint64> signatures(clauses.size());

  std::vector<Literal> candidates_for_removal;
  for (int clause_index = 0; clause_index < clauses.size(); ++clause_index) {
    SatClause* clause = clauses[clause_index];

    // TODO(user): Better abort limit. We could also limit the watcher sizes and
    // never look at really long clauses. Note that for an easier
    // incrementality, it is better to reach some kind of completion so we know
    // what new stuff need to be done.
    if (num_inspected_literals + num_inspected_signatures > 1e9) {
      break;
    }

    // Check for subsumption, note that this currently ignore all clauses in the
    // binary implication graphs. Stamping is doing some of that (and some also
    // happen during probing), but we could consider only direct implications
    // here and be a bit more exhaustive than what stamping do with them (at
    // least for node with many incoming and outgoing implications).
    //
    // TODO(user): Do some reduction using binary clauses. Note that only clause
    // that never propagated since last round need to be checked for binary
    // subsumption.

    // Compute hash and mark literals.
    uint64 signature = 0;
    marked.SparseClearAll();
    for (const Literal l : clause->AsSpan()) {
      marked.Set(l.Index());
      signature |= (uint64{1} << (l.Variable().value() % 64));
    }

    // Look for clause that subsumes this one. Note that because we inspect
    // all one watcher lists for the literals of this clause, if a clause is
    // included inside this one, it must appear in one of these lists.
    bool removed = false;
    candidates_for_removal.clear();
    const uint64 mask = ~signature;
    for (const Literal l : clause->AsSpan()) {
      num_inspected_signatures += one_watcher[l.Index()].size();
      for (const int i : one_watcher[l.Index()]) {
        if ((mask & signatures[i]) != 0) continue;

        bool subsumed = true;
        bool stengthen = true;
        LiteralIndex to_remove = kNoLiteralIndex;
        num_inspected_literals += clauses[i]->Size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (!marked[o.Index()]) {
            subsumed = false;
            if (to_remove == kNoLiteralIndex && marked[o.NegatedIndex()]) {
              to_remove = o.NegatedIndex();
            } else {
              stengthen = false;
              break;
            }
          }
        }
        if (subsumed) {
          ++num_subsumed_clauses;
          num_removed_literals += clause->Size();
          clause_manager_->InprocessingRemoveClause(clause);
          removed = true;
          break;
        }
        if (stengthen) {
          CHECK_NE(kNoLiteralIndex, to_remove);
          candidates_for_removal.push_back(Literal(to_remove));
        }
      }
      if (removed) break;
    }
    if (removed) continue;

    // For strengthenning we also need to check the negative watcher lists.
    for (const Literal l : clause->AsSpan()) {
      num_inspected_signatures += one_watcher[l.NegatedIndex()].size();
      for (const int i : one_watcher[l.NegatedIndex()]) {
        if ((mask & signatures[i]) != 0) continue;

        bool stengthen = true;
        num_inspected_literals += clauses[i]->Size();
        for (const Literal o : clauses[i]->AsSpan()) {
          if (o == l.Negated()) continue;
          if (!marked[o.Index()]) {
            stengthen = false;
            break;
          }
        }
        if (stengthen) {
          candidates_for_removal.push_back(l);
        }
      }
    }

    // Any literal here can be removed, but afterwards the other might not. For
    // now we just remove the first one.
    //
    // TODO(user): remove first and see if other still removable. Alternatively
    // use a "removed" marker and redo a check for each clause that simplifies
    // this one? Or just remove the first one, and wait for next round.
    if (!candidates_for_removal.empty()) {
      new_clause.clear();
      for (const Literal l : clause->AsSpan()) {
        new_clause.push_back(l);
      }

      int new_size = 0;
      for (const Literal l : new_clause) {
        if (l == candidates_for_removal[0]) continue;
        new_clause[new_size++] = l;
      }
      CHECK_EQ(new_size + 1, new_clause.size());
      new_clause.resize(new_size);

      num_removed_literals += clause->Size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
        return false;
      }
      if (clause->Size() == 0) continue;

      // Recompute signature.
      signature = 0;
      for (const Literal l : clause->AsSpan()) {
        signature |= (uint64{1} << (l.Variable().value() % 64));
      }
    }

    // Register one literal to watch. Any literal works, but we choose the
    // smallest list.
    //
    // TODO(user): No need to add this clause if we know it cannot subsume
    // any new clause since last round. i.e. unchanged clause that do not
    // contains any literals of newly added clause do not need to be added
    // here. We can track two bitset in LiteralWatchers via a register
    // mechanism:
    // - literal of newly watched clauses since last clear.
    // - literal of reduced clauses since last clear.
    //
    // Important: we can only use this clause to subsume/strenghten others if
    // it cannot be deleted later.
    if (!clause_manager_->IsRemovable(clause)) {
      int min_size = kint32max;
      LiteralIndex min_literal = kNoLiteralIndex;
      for (const Literal l : clause->AsSpan()) {
        if (one_watcher[l.Index()].size() < min_size) {
          min_size = one_watcher[l.Index()].size();
          min_literal = l.Index();
        }
      }

      // TODO(user): We could/should sort the literal in this clause by
      // using literals that appear in a small number of clauses first so that
      // we maximize the chance of early abort in the critical loops above.
      //
      // TODO(user): We could also move the watched literal first so we always
      // skip it.
      signatures[clause_index] = signature;
      one_watcher[min_literal].push_back(clause_index);
    }
  }

  // We might have fixed variables, finish the propagation.
  if (!LevelZeroPropagate()) return false;

  // TODO(user): tune the deterministic time.
  const double dtime = static_cast<double>(num_inspected_signatures) * 1e-8 +
                       static_cast<double>(num_inspected_literals) * 5e-9;
  time_limit_->AdvanceDeterministicTime(dtime);
  LOG_IF(INFO, log_info) << "Subsume. num_removed_literals: "
                         << num_removed_literals
                         << " num_subsumed: " << num_subsumed_clauses
                         << " dtime: " << dtime
                         << " wtime: " << wall_timer.Get();
  return true;
}

bool StampingSimplifier::DoOneRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();

  dtime_ = 0.0;
  num_subsumed_clauses_ = 0;
  num_removed_literals_ = 0;
  num_fixed_ = 0;

  if (implication_graph_->literal_size() == 0) return true;
  if (implication_graph_->num_implications() == 0) return true;

  if (!stamps_are_already_computed_) {
    // We need a DAG so that we don't have cycle while we sample the tree.
    // TODO(user): We could probably deal with it if needed so that we don't
    // need to do equivalence detetion each time we want to run this.
    implication_graph_->RemoveFixedVariables();
    if (!implication_graph_->DetectEquivalences(log_info)) return true;
    SampleTreeAndFillParent();
    if (!ComputeStamps()) return false;
  }
  stamps_are_already_computed_ = false;
  if (!ProcessClauses()) return false;

  // Note that num_removed_literals_ do not count the literals of the subsumed
  // clauses.
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Stamping. num_removed_literals: "
                         << num_removed_literals_
                         << " num_subsumed: " << num_subsumed_clauses_
                         << " num_fixed: " << num_fixed_ << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
  return true;
}

bool StampingSimplifier::ComputeStampsForNextRound(bool log_info) {
  WallTimer wall_timer;
  wall_timer.Start();
  dtime_ = 0.0;
  num_fixed_ = 0;

  if (implication_graph_->literal_size() == 0) return true;
  if (implication_graph_->num_implications() == 0) return true;

  implication_graph_->RemoveFixedVariables();
  if (!implication_graph_->DetectEquivalences(log_info)) return true;
  SampleTreeAndFillParent();
  if (!ComputeStamps()) return false;
  stamps_are_already_computed_ = true;

  // TODO(user): compute some dtime, it is always zero currently.
  time_limit_->AdvanceDeterministicTime(dtime_);
  log_info |= VLOG_IS_ON(1);
  LOG_IF(INFO, log_info) << "Prestamping."
                         << " num_fixed: " << num_fixed_ << " dtime: " << dtime_
                         << " wtime: " << wall_timer.Get();
  return true;
}

void StampingSimplifier::SampleTreeAndFillParent() {
  const int size = implication_graph_->literal_size();
  CHECK(implication_graph_->IsDag());  // so we don't have cycle.
  parents_.resize(size);
  for (LiteralIndex i(0); i < size; ++i) {
    parents_[i] = i;  // default.
    if (implication_graph_->IsRedundant(Literal(i))) continue;

    // TODO(user): Better algo to not select redundant parent.
    //
    // TODO(user): if parents_[x] = y, try not to have parents_[not(y)] = not(x)
    // because this is not as useful for the simplification power.
    //
    // TODO(user): Consider at most ones too.
    // TODO(user): More generally, we could sample a parent while probing so
    // that we consider all hyper binary implications (in the case we don't add
    // them to the implication graph already).
    const auto& children_of_not_l =
        implication_graph_->Implications(Literal(i).Negated());
    if (children_of_not_l.empty()) continue;
    for (int num_tries = 0; num_tries < 10; ++num_tries) {
      const Literal candidate =
          children_of_not_l[absl::Uniform<int>(*random_, 0,
                                               children_of_not_l.size())]
              .Negated();
      if (implication_graph_->IsRedundant(candidate)) continue;
      if (i == candidate.Index()) continue;

      // We found an interesting parent.
      parents_[i] = candidate.Index();
      break;
    }
  }
}

bool StampingSimplifier::ComputeStamps() {
  const int size = implication_graph_->literal_size();

  // Compute sizes.
  sizes_.assign(size, 0);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    sizes_[parents_[i]]++;
  }

  // Compute starts in the children_ vector for each node.
  starts_.resize(size + 1);  // We use a sentinel.
  starts_[LiteralIndex(0)] = 0;
  for (LiteralIndex i(1); i <= size; ++i) {
    starts_[i] = starts_[i - 1] + sizes_[i - 1];
  }

  // Fill children. This messes up starts_.
  children_.resize(size);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] == i) continue;  // leaf.
    children_[starts_[parents_[i]]++] = i;
  }

  // Reset starts to correct value.
  for (LiteralIndex i(0); i < size; ++i) {
    starts_[i] -= sizes_[i];
  }

  if (DEBUG_MODE) {
    CHECK_EQ(starts_[LiteralIndex(0)], 0);
    for (LiteralIndex i(1); i <= size; ++i) {
      CHECK_EQ(starts_[i], starts_[i - 1] + sizes_[i - 1]);
    }
  }

  // Perform a DFS from each root to compute the stamps.
  int64 stamp = 0;
  first_stamps_.resize(size);
  last_stamps_.resize(size);
  marked_.assign(size, false);
  for (LiteralIndex i(0); i < size; ++i) {
    if (parents_[i] != i) continue;  // Not a root.
    DCHECK(!marked_[i]);
    const LiteralIndex tree_root = i;
    dfs_stack_.push_back(i);
    while (!dfs_stack_.empty()) {
      const LiteralIndex top = dfs_stack_.back();
      if (marked_[top]) {
        dfs_stack_.pop_back();
        last_stamps_[top] = stamp++;
        continue;
      }
      first_stamps_[top] = stamp++;
      marked_[top] = true;

      // Failed literal detection. If the negation of top is in the same
      // tree, we can fix the LCA of top and its negation to false.
      if (marked_[Literal(top).NegatedIndex()] &&
          first_stamps_[Literal(top).NegatedIndex()] >=
              first_stamps_[tree_root]) {
        // Find the LCA.
        const int first_stamp = first_stamps_[Literal(top).NegatedIndex()];
        LiteralIndex lca = top;
        while (first_stamps_[lca] > first_stamp) {
          lca = parents_[lca];
        }
        ++num_fixed_;
        clause_manager_->InprocessingFixLiteral(Literal(lca).Negated());
      }

      const int end = starts_[top + 1];  // Ok with sentinel.
      for (int j = starts_[top]; j < end; ++j) {
        DCHECK_NE(top, children_[j]);    // We removed leaf self-loop.
        DCHECK(!marked_[children_[j]]);  // This is a tree.
        dfs_stack_.push_back(children_[j]);
      }
    }
  }
  DCHECK_EQ(stamp, 2 * size);
  return true;
}

bool StampingSimplifier::ProcessClauses() {
  struct Entry {
    int i;            // Index in the clause.
    bool is_negated;  // Correspond to clause[i] or clause[i].Negated();
    int start;        // Note that all start stamps are different.
    int end;
    bool operator<(const Entry& o) const { return start < o.start; }
  };
  std::vector<int> to_remove;
  std::vector<Literal> new_clause;
  std::vector<Entry> entries;
  clause_manager_->DeleteRemovedClauses();
  clause_manager_->DetachAllClauses();
  for (SatClause* clause : clause_manager_->AllClausesInCreationOrder()) {
    const auto span = clause->AsSpan();
    if (span.empty()) continue;

    // For a and b in the clause, if not(a) => b is present, then the clause is
    // subsumed. If a => b, then a can be removed, and if not(a) => not(b) then
    // b can be removed. Nothing can be done if a => not(b).
    entries.clear();
    for (int i = 0; i < span.size(); ++i) {
      entries.push_back({i, false, first_stamps_[span[i].Index()],
                         last_stamps_[span[i].Index()]});
      entries.push_back({i, true, first_stamps_[span[i].NegatedIndex()],
                         last_stamps_[span[i].NegatedIndex()]});
    }
    std::sort(entries.begin(), entries.end());

    Entry top_entry;
    top_entry.end = -1;  // Sentinel.
    to_remove.clear();
    for (const Entry& e : entries) {
      if (e.end < top_entry.end) {
        // We found an implication: top_entry => this entry.
        const Literal lhs = top_entry.is_negated ? span[top_entry.i].Negated()
                                                 : span[top_entry.i];
        const Literal rhs = e.is_negated ? span[e.i].Negated() : span[e.i];
        DCHECK(ImplicationIsInTree(lhs, rhs));

        if (top_entry.is_negated != e.is_negated) {
          // Failed literal?
          if (top_entry.i == e.i) {
            ++num_fixed_;
            if (top_entry.is_negated) {
              // not(span[i]) => span[i] so span[i] true.
              // And the clause is satisfied (so we count as as subsumed).
              clause_manager_->InprocessingFixLiteral(span[top_entry.i]);
            } else {
              // span[i] => not(span[i]) so span[i] false.
              clause_manager_->InprocessingFixLiteral(
                  span[top_entry.i].Negated());
              to_remove.push_back(top_entry.i);
              continue;
            }
          }

          // not(a) => b : subsumption.
          // a => not(b), we cannot deduce anything, but it might make sense
          // to see if not(b) implies anything instead of just keeping
          // top_entry. See TODO below.
          if (top_entry.is_negated) {
            num_subsumed_clauses_++;
            clause_manager_->InprocessingRemoveClause(clause);
            break;
          }
        } else {
          CHECK_NE(top_entry.i, e.i);
          if (top_entry.is_negated) {
            // not(a) => not(b), we can remove b.
            to_remove.push_back(e.i);
          } else {
            // a => b, we can remove a.
            //
            // TODO(user): Note that it is okay to still use top_entry, but we
            // might miss the removal of b if b => c. Also the paper do things
            // differently. Make sure we don't miss any simplification
            // opportunites by not changing top_entry. Same in the other
            // branches.
            to_remove.push_back(top_entry.i);
          }
        }
      } else {
        top_entry = e;
      }
    }

    if (clause->empty()) continue;

    // Strengthen the clause.
    if (!to_remove.empty()) {
      new_clause.clear();
      gtl::STLSortAndRemoveDuplicates(&to_remove);
      int to_remove_index = 0;
      for (int i = 0; i < span.size(); ++i) {
        if (to_remove_index < to_remove.size() &&
            i == to_remove[to_remove_index]) {
          ++to_remove_index;
          continue;
        }
        new_clause.push_back(span[i]);
      }
      num_removed_literals_ += span.size() - new_clause.size();
      if (!clause_manager_->InprocessingRewriteClause(clause, new_clause)) {
        return false;
      }
    }

    // The sort should be dominant.
    const double n = static_cast<double>(entries.size());
    dtime_ += 1.5e-8 * n * std::log(n);
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
