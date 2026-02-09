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

#include "ortools/sat/sat_sweeping.h"

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

void EquivalenceSatSweeping::LoadClausesInModel(
    absl::Span<const SatClause* const> clauses, Model* m) {
  const int num_booleans = big_model_to_small_model_.size();
  auto* sat_solver = m->GetOrCreate<SatSolver>();
  CHECK_EQ(sat_solver->NumVariables(), 0);

  sat_solver->SetNumVariables(num_booleans);

  std::vector<Literal> literals;
  for (const SatClause* clause : clauses) {
    literals.clear();
    for (const Literal l : clause->AsSpan()) {
      literals.push_back(
          Literal(big_model_to_small_model_[l.Variable()], l.IsPositive()));
    }
    sat_solver->AddProblemClause(literals);
  }
}

// We define the neighborhood of clauses and variables with respect to `v` by
// defining a distance between variables and getting the closest variables to it
// and the clauses linking the neighborhood variables. The distance is defined
// as a non-negative integer satisfying:
// - d(v1, v2) == 0 iff v1 == v2.
// - d(v1, v2) == 1 iff v1 and v2 appears in the same clause.
// - d(v1, v2) satisfies triangle inequality.
//
// Note that the distance above is equivalent to the distance on the graph of
// clauses.
std::vector<absl::Span<const Literal>> EquivalenceSatSweeping::GetNeighborhood(
    BooleanVariable var) {
  std::vector<absl::Span<const Literal>> neighborhood;
  absl::flat_hash_set<BooleanVariable> seen_bools = {var};
  const int binary_clause_slack = max_num_clauses_ / 10;
  std::deque<BooleanVariable> bools_to_add;
  bools_to_add.push_back(var);
  auto rep = [&](Literal l) {
    auto it = lit_representative_.find(l);
    return it == lit_representative_.end() ? l : it->second;
  };
  auto rep_var = [&](BooleanVariable v) {
    return rep(Literal(v, true)).Variable();
  };
  while (!bools_to_add.empty()) {
    // TODO(user): when all the variables and clauses of a given distance
    // don't fit in the limit we are picking which ones we put in the
    // neighborhood quite arbitrarily. We should try doing it using a priority
    // queue of how many clauses they using the variable or do it randomly.
    const BooleanVariable cur_var = rep_var(bools_to_add.front());
    bools_to_add.pop_front();
    for (const ClauseIndex clause_index : var_to_clauses_[cur_var]) {
      const absl::Span<const Literal> cur_clause = clauses_[clause_index];
      const int num_unseen_bools = absl::c_count_if(cur_clause, [&](Literal l) {
        return !seen_bools.contains(rep(l).Variable());
      });
      if (seen_bools.size() + num_unseen_bools >= max_num_boolean_variables_) {
        continue;
      }
      if (cur_clause.size() == 2) {
        const Literal l1 = rep(cur_clause[0]);
        const Literal l2 = rep(cur_clause[1]);
        const Literal other_lit = l1.Variable() == cur_var ? l2 : l1;
        if (l1.Variable() == l2.Variable() ||
            implication_graph_->RepresentativeOf(other_lit).Variable() ==
                cur_var) {
          // Do not waste our variable budget with non-representative literals
          // and the clauses mapping them to their representative. We might end
          // up with a neighborhood that is too small if the inprocessing did
          // not yet replaced the literals with their representative, but it's
          // better than wasting effort.
          continue;
        }
      }
      if (neighborhood.size() >= max_num_clauses_ - binary_clause_slack &&
          cur_clause.size() > 2) {
        // Reserve a bit of out clauses budget for binary clauses. We do not
        // want to waste resources rediscovering them.
        continue;
      }
      for (const Literal non_rep_l : cur_clause) {
        const Literal l = rep(non_rep_l);
        if (seen_bools.contains(l.Variable())) continue;
        bools_to_add.push_back(l.Variable());
        seen_bools.insert(l.Variable());
      }
      neighborhood.push_back(cur_clause);
      if (neighborhood.size() >= max_num_clauses_) return neighborhood;
    }
  }
  return neighborhood;
}

namespace {
void RefinePartitions(std::vector<std::vector<Literal>>& partitions,
                      const VariablesAssignment& solution) {
  // TODO(user): check whether we can use
  // google3/ortools/algorithms/dynamic_partition.h
  const int original_num_partitions = partitions.size();
  for (int i = 0; i < original_num_partitions; i++) {
    std::vector<Literal>& partition_for_negated = partitions.emplace_back();
    std::vector<Literal>& partition_for_true = partitions[i];
    // Split the partition in two, according to the value of the literals in the
    // solution.
    int new_partition_for_true_size = 0;
    for (int j = 0; j < partition_for_true.size(); j++) {
      const Literal lit = partition_for_true[j];
      if (!solution.LiteralIsTrue(lit)) {
        partition_for_negated.push_back(lit);
        continue;
      }
      partition_for_true[new_partition_for_true_size++] = lit;
    }
    partition_for_true.resize(new_partition_for_true_size);
    // Partitions of size 1 are useless.
    if (partition_for_negated.size() <= 1) {
      partitions.pop_back();
    }
  }
  int new_num_partitions = 0;
  for (int i = 0; i < partitions.size(); i++) {
    if (partitions[i].size() > 1) {
      if (new_num_partitions != i) {
        partitions[new_num_partitions] = std::move(partitions[i]);
      }
      new_num_partitions++;
    }
  }
  partitions.resize(new_num_partitions);
}
}  // namespace

bool EquivalenceSatSweeping::DoOneRound(
    std::function<void(Model*)> run_inprocessing) {
  // For now we compute a single neighborhood and do a full SAT sweeping on it.
  // TODO(user): consider doing several neighborhoods to amortize the cost of
  // building the variable->clause graph.
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  if (sat_solver_->AssumptionLevel() != 0) {
    VLOG(2)
        << "Assumption level is not 0 (should not happen), skipping sweeping.";
    return true;
  }
  clauses_.clear();

  struct ExtractedClausesHelper {
    explicit ExtractedClausesHelper(
        util_intops::StrongVector<ClauseIndex, absl::Span<const Literal>>&
            clauses_vec,
        int clause_size_limit_var)
        : clause_size_limit(clause_size_limit_var), clauses(clauses_vec) {}
    void AddBinaryClause(Literal a, Literal b) {
      binary_clauses.push_back({a, b});
      clauses.push_back(absl::MakeConstSpan(binary_clauses.back()));
    }
    void AddClause(absl::Span<const Literal> clause) {
      if (clause.size() > clause_size_limit) return;
      clauses.push_back(clause);
    }
    void SetNumVariables(int /* unused */) {}

    int clause_size_limit;
    // Use a deque for pointer stability.
    std::deque<std::array<Literal, 2>> binary_clauses;
    util_intops::StrongVector<ClauseIndex, absl::Span<const Literal>>& clauses;
  };

  ExtractedClausesHelper helper(clauses_, max_num_boolean_variables_);
  if (!sat_solver_->ExtractClauses(&helper)) return false;

  if (clauses_.empty()) {
    VLOG(2) << "No clauses extracted, skipping sweeping.";
    return true;
  }

  const int num_vars = sat_solver_->NumVariables();

  struct GetVarMapper {
    BooleanVariable operator()(Literal l) const { return l.Variable(); }
  };
  var_to_clauses_.ResetFromTransposeMap<GetVarMapper>(clauses_, num_vars);
  lit_representative_.clear();

  global_time_limit_->AdvanceDeterministicTime(clause_manager_->num_clauses() *
                                               1.0e-7);
  TimeLimit sweep_time_limit;
  sweep_time_limit.ChangeDeterministicLimit(1.0);
  sweep_time_limit.MergeWithGlobalTimeLimit(global_time_limit_);
  std::vector<std::pair<Literal, Literal>> binary_clauses;
  std::vector<Literal> unary_clauses;
  BooleanVariable next_candidate_var = kNoBooleanVariable;
  for (int i = 0; i < 50; ++i) {
    if (next_candidate_var == kNoBooleanVariable) {
      int tries = 0;
      constexpr int kMaxTries = 10;
      for (tries = 0; tries < kMaxTries; ++tries) {
        next_candidate_var = RepresentativeVar(
            BooleanVariable(absl::Uniform<int>(*random_, 0, num_vars)));
        if (var_to_clauses_[next_candidate_var].size() < 2) continue;
        const Literal positive_lit(next_candidate_var, true);
        if (implication_graph_->RepresentativeOf(positive_lit) !=
            positive_lit) {
          continue;
        }
        break;
      }
      if (tries == kMaxTries) continue;
    }

    const std::vector<absl::Span<const Literal>> neighborhood =
        GetNeighborhood(next_candidate_var);

    if (neighborhood.empty()) {
      VLOG(2) << "Neighborhood is empty for " << next_candidate_var;
      next_candidate_var = kNoBooleanVariable;
      continue;
    }

    CompactVectorVector<int, Literal> neighborhood_clauses;
    big_model_to_small_model_.clear();
    small_model_to_big_model_.clear();
    for (const absl::Span<const Literal> clause : neighborhood) {
      neighborhood_clauses.Add({});
      for (const Literal non_rep_l : clause) {
        const Literal l = Representative(non_rep_l);
        const BooleanVariable new_var(big_model_to_small_model_.size());
        auto [it, inserted] =
            big_model_to_small_model_.insert({l.Variable(), new_var});
        if (inserted) {
          small_model_to_big_model_.push_back(l.Variable());
        }
        neighborhood_clauses.AppendToLastVector(
            Literal(it->second, l.IsPositive()));
      }
    }

    const SatSweepingResult result = DoFullSatSweeping(
        neighborhood_clauses, &sweep_time_limit, run_inprocessing);

    if (result.status == SatSolver::INFEASIBLE) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
    for (const auto& [l1, l2] : result.binary_clauses) {
      const Literal mapped_l1 =
          Literal(small_model_to_big_model_[l1.Variable()], l1.IsPositive());
      const Literal mapped_l2 =
          Literal(small_model_to_big_model_[l2.Variable()], l2.IsPositive());
      if (implication_graph_->IsRemoved(mapped_l1) ||
          implication_graph_->IsRemoved(mapped_l2)) {
        continue;
      }
      binary_clauses.push_back({mapped_l1, mapped_l2});
    }
    for (const Literal l : result.unary_clauses) {
      const Literal mapped_l =
          Literal(small_model_to_big_model_[l.Variable()], l.IsPositive());
      if (implication_graph_->IsRemoved(mapped_l)) continue;
      unary_clauses.push_back(mapped_l);
    }
    if (result.status == SatSolver::LIMIT_REACHED) {
      break;
    }

    // Now update var_to_clauses_ and lit_representative_ with the new
    // equivalences. The number of new equivalences is small, so we want to be
    // linear on its size.
    DenseConnectedComponentsFinder union_find;
    union_find.SetNumberOfNodes(2 * small_model_to_big_model_.size() + 1);
    absl::flat_hash_set<BooleanVariable> seen_bools;
    std::vector<BooleanVariable> bools_with_new_equivalences;
    for (const auto& [l1, l2] : result.new_equivalences) {
      CHECK_NE(l1.Variable(), l2.Variable());
      union_find.AddEdge(l1.Index().value(), l2.Index().value());
      union_find.AddEdge(l1.NegatedIndex().value(), l2.NegatedIndex().value());
      DCHECK_EQ(Representative(l1), Representative(l1.Negated()).Negated());
      if (seen_bools.insert(l1.Variable()).second) {
        bools_with_new_equivalences.push_back(l1.Variable());
      }
      if (seen_bools.insert(l2.Variable()).second) {
        bools_with_new_equivalences.push_back(l2.Variable());
      }
    }
    for (const BooleanVariable current_var : bools_with_new_equivalences) {
      const Literal current_lit(current_var, true);
      const Literal representative_lit = Literal(
          LiteralIndex(union_find.FindRoot(current_lit.Index().value())));
      if (current_lit == representative_lit) continue;
      const Literal mapped_representative =
          Literal(small_model_to_big_model_[representative_lit.Variable()],
                  representative_lit.IsPositive());
      const Literal mapped_current =
          Literal(small_model_to_big_model_[current_lit.Variable()],
                  current_lit.IsPositive());
      lit_representative_[mapped_current] = mapped_representative;
      lit_representative_[mapped_current.Negated()] =
          mapped_representative.Negated();
      var_to_clauses_.MergeInto(mapped_current.Variable(),
                                mapped_representative.Variable());
    }
    for (std::pair<const Literal, Literal>& pair : lit_representative_) {
      auto it = lit_representative_.find(pair.second);
      if (it != lit_representative_.end()) {
        pair.second = it->second;
      }
    }
    if (result.new_equivalences.size() > 10) {
      // Retry same variable
      next_candidate_var = RepresentativeVar(next_candidate_var);
    } else if (!result.new_equivalences.empty()) {
      // Try a different variable from the same neighborhood.
      const int var_index =
          absl::Uniform<int>(*random_, 0, bools_with_new_equivalences.size());
      const BooleanVariable unmapped_next_candidate_var =
          bools_with_new_equivalences[var_index];
      next_candidate_var = RepresentativeVar(
          small_model_to_big_model_[unmapped_next_candidate_var]);
    } else {
      next_candidate_var = kNoBooleanVariable;
    }
  }
  global_time_limit_->AdvanceDeterministicTime(
      sweep_time_limit.GetElapsedDeterministicTime());
  if (binary_clauses.empty() && unary_clauses.empty()) {
    return true;
  }
  // TODO(user): find out why this is necessary.
  clause_manager_->DetachAllClauses();
  for (const auto& [l1, l2] : binary_clauses) {
    if (!implication_graph_->AddBinaryClause(l1, l2)) return false;
  }
  for (const Literal l : unary_clauses) {
    if (!implication_graph_->FixLiteral(l, {})) return false;
  }
  return true;
}

SatSweepingResult DoFullSatSweeping(
    const CompactVectorVector<int, Literal>& clauses, TimeLimit* time_limit,
    std::function<void(Model*)> configure_model_before_first_solve) {
  WallTimer wall_timer;
  wall_timer.Start();
  Model neighborhood_model;

  TimeLimit* model_time_limit = neighborhood_model.GetOrCreate<TimeLimit>();
  absl::Cleanup update_elapsed_dtime =
      [model_time_limit, time_limit,
       time_limit_dtime_start =
           model_time_limit->GetElapsedDeterministicTime()] {
        time_limit->AdvanceDeterministicTime(
            model_time_limit->GetElapsedDeterministicTime() -
            time_limit_dtime_start);
      };

  model_time_limit->MergeWithGlobalTimeLimit(time_limit);

  // This algorithm splits the partitions much faster if it sees a more
  // diversified set of solutions. So we tweak the SAT solver to do assignments
  // more randomly.
  SatParameters* params = neighborhood_model.GetOrCreate<SatParameters>();
  params->set_initial_polarity(SatParameters::POLARITY_RANDOM);
  params->set_preferred_variable_order(SatParameters::IN_RANDOM_ORDER);
  params->set_random_polarity_ratio(0.3);
  params->set_random_branches_ratio(0.3);

  SatDecisionPolicy* decision_policy =
      neighborhood_model.GetOrCreate<SatDecisionPolicy>();

  auto* sat_solver = neighborhood_model.GetOrCreate<SatSolver>();
  BooleanVariable max_boolean = BooleanVariable(0);
  for (int i = 0; i < clauses.size(); ++i) {
    for (const Literal l : clauses[i]) {
      max_boolean = std::max(max_boolean, l.Variable());
    }
  }
  CHECK_EQ(sat_solver->NumVariables(), 0);
  sat_solver->SetNumVariables(max_boolean.value() + 1);

  absl::flat_hash_set<std::pair<Literal, Literal>> input_binary_clauses;
  for (int i = 0; i < clauses.size(); ++i) {
    sat_solver->AddProblemClause(clauses[i]);
    if (clauses[i].size() == 2) {
      Literal l1 = clauses[i][0];
      Literal l2 = clauses[i][1];
      if (l1.Variable() > l2.Variable()) {
        std::swap(l1, l2);
      }
      input_binary_clauses.insert({l1, l2});
    }
  }
  configure_model_before_first_solve(&neighborhood_model);

  SatSweepingResult result;
  SatSolver* nh_solver = neighborhood_model.GetOrCreate<SatSolver>();
  if (!nh_solver->FinishPropagation()) {
    result.status = SatSolver::INFEASIBLE;
    return result;
  }

  // We start by finding a first solution to our problem. This will be used for
  // initializing the set of potential backbone (ie., fixable) literals and
  // the partitions of potentially equivalent literals.
  result.status = nh_solver->Solve();
  if (result.status == SatSolver::INFEASIBLE) {
    VLOG(2) << "Neighborhood is infeasible, problem closed?";
    return result;
  }

  if (result.status == SatSolver::LIMIT_REACHED) {
    VLOG(2)
        << "Limit reached in first solve of the neighborhood, elapsed_dtime="
        << model_time_limit->GetElapsedDeterministicTime();
    return result;
  }
  CHECK_EQ(result.status, SatSolver::FEASIBLE);

  ModelRandomGenerator* random =
      neighborhood_model.GetOrCreate<ModelRandomGenerator>();
  int num_sat_calls = 1;
  std::vector<Literal> possible_backbone;
  const int num_variables = nh_solver->NumVariables();
  possible_backbone.reserve(num_variables);
  for (BooleanVariable var{0}; var < num_variables; ++var) {
    possible_backbone.push_back(
        nh_solver->Assignment().GetTrueLiteralForAssignedVariable(var));
  }
  std::vector<std::vector<Literal>> partitions = {possible_backbone};
  while (!possible_backbone.empty()) {
    // Pick a random literal from the possible backbone and try to prove it is
    // indeed in the backbone. As a side-effect, if it is not, we get a new,
    // different solution.
    const int index = absl::Uniform<int>(*random, 0, possible_backbone.size());
    const Literal l = possible_backbone[index];
    std::swap(possible_backbone[index], possible_backbone.back());
    possible_backbone.pop_back();
    decision_policy->ResetDecisionHeuristic();
    const SatSolver::Status status =
        nh_solver->ResetAndSolveWithGivenAssumptions({l.Negated()});
    ++num_sat_calls;
    if (status == SatSolver::LIMIT_REACHED) {
      VLOG(2) << "Limit reached in neighborhood, elapsed_dtime="
              << model_time_limit->GetElapsedDeterministicTime();
      result.status = status;
      break;
    }
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      // Our subproblem is unsat with ~l!
      result.unary_clauses.push_back(l);
      // TODO(user): make sure that adding the assumption back to the
      // model is redundant, since it could be a side-effect of returning
      // ASSUMPTIONS_UNSAT.
      CHECK(nh_solver->ResetToLevelZero());
      CHECK(nh_solver->AddUnitClause(l));

      // Remove from the partitions.
      for (std::vector<Literal>& partition : partitions) {
        int new_partition_size = 0;
        for (int i = 0; i < partition.size(); i++) {
          const Literal literal = partition[i];
          if (literal == l || literal == l.Negated()) continue;
          partition[new_partition_size++] = literal;
        }
        partition.resize(new_partition_size);
      }
    } else {
      // This is the most common case, where the literal is not in the backbone.
      // So we use the solution we got to refine the partitions and update the
      // backbone.
      CHECK(status == SatSolver::FEASIBLE);
      // Update the backbone
      int new_possible_backbone_size = 0;
      for (int i = 0; i < possible_backbone.size(); ++i) {
        if (!nh_solver->Assignment().LiteralIsTrue(possible_backbone[i])) {
          continue;
        }
        // If a literal has a different polarity in this solution than it had in
        // the previous ones, we know it's not part of the backbone.
        possible_backbone[new_possible_backbone_size++] = possible_backbone[i];
      }
      possible_backbone.resize(new_possible_backbone_size);

      // Use the new solution to update the partitions.
      RefinePartitions(partitions, nh_solver->Assignment());
    }
  }
  const int num_partitions = partitions.size();
  int num_equivalences = 0;

  while (result.status != SatSolver::LIMIT_REACHED && !partitions.empty()) {
    std::vector<Literal>& partition = partitions.back();
    if (partition.size() <= 1) {
      partitions.pop_back();
      continue;
    }
    const Literal l1 = partition[0];
    const Literal l2 = partition.back();
    SatSolver::Status status =
        nh_solver->ResetAndSolveWithGivenAssumptions({l1, l2.Negated()});
    ++num_sat_calls;
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      // We found a binary clause! Add the clause (~l1, l2) to the main problem.
      CHECK(nh_solver->ResetToLevelZero());
      CHECK(nh_solver->AddBinaryClause(l1.Negated(), l2));

      ++num_sat_calls;
      // Now check if we have an equivalence with l1 and l2.
      status = nh_solver->ResetAndSolveWithGivenAssumptions({l1.Negated(), l2});
    }
    if (status == SatSolver::LIMIT_REACHED) {
      VLOG(2) << "Limit reached in neighborhood, elapsed_dtime="
              << model_time_limit->GetElapsedDeterministicTime();
      result.status = status;
      break;
    }
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      // We have an equivalence! Add it to the main problem.
      ++num_equivalences;
      Literal l1_canonical = l1;
      Literal l2_canonical = l2;
      if (l1_canonical.Variable() > l2_canonical.Variable()) {
        std::swap(l1_canonical, l2_canonical);
      }
      if (!input_binary_clauses.contains(
              {l1_canonical, l2_canonical.Negated()}) ||
          !input_binary_clauses.contains(
              {l1_canonical.Negated(), l2_canonical})) {
        result.new_equivalences.push_back({l1_canonical, l2_canonical});
      }
      partition.pop_back();  // Remove l2 from the partition. It's equivalent to
                             // l1, so it's not useful for finding more
                             // equivalences.
      CHECK(nh_solver->ResetToLevelZero());
      CHECK(nh_solver->AddBinaryClause(l1, l2.Negated()));
    } else {
      CHECK_EQ(status, SatSolver::FEASIBLE);
      // Use the new solution to update the partitions. Note that this will
      // at least break the current partition in two, since we now have a
      // solution where l1 and l2 have different polarities. This guarantees
      // that this loop will run at most num_variables times.
      RefinePartitions(partitions, nh_solver->Assignment());
    }
  }

  CHECK(nh_solver->ResetToLevelZero());
  BinaryImplicationGraph* implication_graph =
      neighborhood_model.GetOrCreate<BinaryImplicationGraph>();
  CHECK(implication_graph->DetectEquivalences());

  struct GetBinaryClause {
    explicit GetBinaryClause(std::vector<std::pair<Literal, Literal>>& clauses)
        : binary_clauses(clauses) {}
    void AddBinaryClause(Literal a, Literal b) {
      binary_clauses.push_back({a, b});
    }
    std::vector<std::pair<Literal, Literal>>& binary_clauses;
  };

  GetBinaryClause helper(result.binary_clauses);
  implication_graph->ExtractAllBinaryClauses(&helper);

  if (result.status != SatSolver::LIMIT_REACHED && DEBUG_MODE) {
    // Since we kept the set of all possible partitions and ran the algorithm
    // until they were all unitary, we must have seen all possible equivalences
    // that are valid. Check that the solver didn't found more equivalences than
    // we did.
    int num_equivalences_in_model = 0;
    for (BooleanVariable var{0}; var < num_variables; ++var) {
      const Literal l = Literal(var, true);
      num_equivalences_in_model += implication_graph->RepresentativeOf(l) != l;
    }
    DCHECK_EQ(num_equivalences_in_model, num_equivalences);
  }

  // Remove binary clauses that were already in the input
  absl::flat_hash_set<std::pair<Literal, Literal>> input_clauses;
  for (int i = 0; i < clauses.size(); i++) {
    const absl::Span<const Literal> clause = clauses[i];
    if (clause.size() != 2) continue;
    Literal l1 = clause[0];
    Literal l2 = clause[1];
    if (l1 < l2) std::swap(l1, l2);
    input_clauses.insert({l1, l2});
  }
  int new_binary_clauses_size = 0;
  for (int i = 0; i < result.binary_clauses.size(); ++i) {
    std::pair<Literal, Literal>& clause = result.binary_clauses[i];
    if (clause.first < clause.second) {
      std::swap(clause.first, clause.second);
    }
    if (input_clauses.contains(clause)) continue;

    result.binary_clauses[new_binary_clauses_size++] = clause;
  }
  result.binary_clauses.resize(new_binary_clauses_size);

  VLOG(2) << "num_booleans: " << num_variables
          << " num_clauses: " << clauses.size()
          << " num_partitions: " << num_partitions
          << " num_unary_clauses: " << result.unary_clauses.size()
          << " num_binary_clauses: " << result.binary_clauses.size()
          << " num_equivalences: " << result.new_equivalences.size()
          << " num_sat_calls: " << num_sat_calls
          << " dtime: " << model_time_limit->GetElapsedDeterministicTime()
          << " wtime: " << wall_timer.Get();

  return result;
}

}  // namespace sat
}  // namespace operations_research
