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

#include "ortools/sat/continuous_prober.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

#define RETURN_IF_NOT_FEASIBLE(test)       \
  const SatSolver::Status status = (test); \
  if (status != SatSolver::FEASIBLE) return status;

ContinuousProber::ContinuousProber(const CpModelProto& model_proto,
                                   Model* model)
    : model_(model),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      binary_implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      inprocessing_(model->GetOrCreate<Inprocessing>()),
      parameters_(*(model->GetOrCreate<SatParameters>())),
      level_zero_callbacks_(model->GetOrCreate<LevelZeroCallbackHelper>()),
      prober_(model->GetOrCreate<Prober>()),
      shared_response_manager_(model->Mutable<SharedResponseManager>()),
      shared_bounds_manager_(model->Mutable<SharedBoundsManager>()),
      shared_stats_(model->Mutable<SharedStatistics>()),
      random_(*model->GetOrCreate<ModelRandomGenerator>()),
      active_limit_(parameters_.shaving_search_deterministic_time()) {
  auto* mapping = model_->GetOrCreate<CpModelMapping>();
  absl::flat_hash_set<BooleanVariable> visited;

  // Build variable lists.
  // TODO(user): Ideally, we should scan the internal model. But there can be
  // a large blowup of variables during loading, which slows down the probing
  // part. Using model variables is a good heuristic to select 'impactful'
  // Boolean variables.
  for (int v = 0; v < model_proto.variables_size(); ++v) {
    if (mapping->IsBoolean(v)) {
      const BooleanVariable bool_var = mapping->Literal(v).Variable();
      const auto [_, inserted] = visited.insert(bool_var);
      if (inserted) {
        bool_vars_.push_back(bool_var);
      }
    } else {
      IntegerVariable var = mapping->Integer(v);
      if (integer_trail_->IsFixed(var)) continue;
      int_vars_.push_back(var);
    }
  }

  VLOG(2) << "Start continuous probing with " << bool_vars_.size()
          << " Boolean variables,  " << int_vars_.size()
          << " integer variables, deterministic time limit = "
          << time_limit_->GetDeterministicLimit() << " on " << model_->Name();
}

ContinuousProber::~ContinuousProber() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"ContinuousProber/iterations", iteration_});
  for (const auto& method_stats :
       {counters_.one_variable_stats, counters_.at_least_one_stats,
        counters_.at_most_one_stats, counters_.pairs_stats,
        counters_.shaving}) {
    const std::string prefix =
        absl::StrCat("ContinuousProber/", method_stats.name);
    stats.push_back(
        {absl::StrCat(prefix, "/num_calls"), method_stats.num_calls});
    stats.push_back(
        {absl::StrCat(prefix, "/num_new_bounds"), method_stats.num_new_bounds});
    stats.push_back({absl::StrCat(prefix, "/num_new_literals_fixed"),
                     method_stats.num_new_literals_fixed});
    stats.push_back(
        {absl::StrCat(prefix, "/num_new_binary"), method_stats.num_new_binary});
  }
  shared_stats_->AddStats(stats);
}

// Continuous probing procedure.
// TODO(user):
//   - sort variables before the iteration (statically or dynamically)
//   - compress clause databases regularly (especially the implication graph)
//   - better interleaving of the probing and shaving phases
//   - move the shaving code directly in the probing class
//   - probe all variables and not just the model ones
SatSolver::Status ContinuousProber::Probe() {
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  while (!time_limit_->LimitReached()) {
    // Store current statistics to detect an iteration without any improvement.
    const int64_t initial_num_literals_fixed =
        prober_->num_new_literals_fixed();
    const int64_t initial_num_bounds_shaved = counters_.shaving.num_new_bounds;
    const auto& assignment = sat_solver_->Assignment();

    // Probe variable bounds.
    // TODO(user): Probe optional variables.
    for (; current_int_var_ < int_vars_.size(); ++current_int_var_) {
      const IntegerVariable int_var = int_vars_[current_int_var_];
      if (integer_trail_->IsFixed(int_var)) continue;

      const Literal shave_lb_literal =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(
              int_var, integer_trail_->LowerBound(int_var)));
      const BooleanVariable shave_lb_var = shave_lb_literal.Variable();
      const auto [_lb, lb_inserted] = probed_bool_vars_.insert(shave_lb_var);
      if (lb_inserted) {
        const MethodStats start_stats = GetStats(prober_);
        if (!prober_->ProbeOneVariable(shave_lb_var)) {
          return SatSolver::INFEASIBLE;
        }
        AddStats(counters_.one_variable_stats, start_stats, GetStats(prober_));
      }

      const Literal shave_ub_literal =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              int_var, integer_trail_->UpperBound(int_var)));
      const BooleanVariable shave_ub_var = shave_ub_literal.Variable();
      const auto [_ub, ub_inserted] = probed_bool_vars_.insert(shave_ub_var);
      if (ub_inserted) {
        const MethodStats start_stats = GetStats(prober_);
        if (!prober_->ProbeOneVariable(shave_ub_var)) {
          return SatSolver::INFEASIBLE;
        }
        AddStats(counters_.one_variable_stats, start_stats, GetStats(prober_));
      }

      if (use_shaving()) {
        const SatSolver::Status lb_status = ShaveLiteral(
            shave_lb_literal, /*literal_is_an_integer_bound=*/true);
        if (ReportStatus(lb_status)) return lb_status;

        const SatSolver::Status ub_status = ShaveLiteral(
            shave_ub_literal, /*literal_is_an_integer_bound=*/true);
        if (ReportStatus(ub_status)) return ub_status;
      }

      RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
    }

    // Probe Boolean variables from the model.
    for (; current_bool_var_ < bool_vars_.size(); ++current_bool_var_) {
      const BooleanVariable& bool_var = bool_vars_[current_bool_var_];

      if (assignment.VariableIsAssigned(bool_var)) continue;
      if (binary_implication_graph_->IsRedundant(Literal(bool_var, true))) {
        continue;
      }

      const auto [_, inserted] = probed_bool_vars_.insert(bool_var);
      if (!inserted) continue;

      const MethodStats start_stats = GetStats(prober_);
      if (!prober_->ProbeOneVariable(bool_var)) {
        return SatSolver::INFEASIBLE;
      }
      AddStats(counters_.one_variable_stats, start_stats, GetStats(prober_));

      const Literal literal(bool_var, true);
      if (use_shaving() && !assignment.LiteralIsAssigned(literal)) {
        const SatSolver::Status true_status =
            ShaveLiteral(literal, /*literal_is_an_integer_bound=*/false);
        if (ReportStatus(true_status)) return true_status;

        if (true_status == SatSolver::ASSUMPTIONS_UNSAT) continue;

        const SatSolver::Status false_status = ShaveLiteral(
            literal.Negated(), /*literal_is_an_integer_bound=*/false);
        if (ReportStatus(false_status)) return false_status;
      }

      RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
    }

    if (parameters_.use_extended_probing()) {
      const auto at_least_one_literal_is_true =
          [&assignment](absl::Span<const Literal> literals) {
            for (const Literal literal : literals) {
              if (assignment.LiteralIsTrue(literal)) {
                return true;
              }
            }
            return false;
          };

      // Probe clauses of the SAT model.
      for (;;) {
        const SatClause* clause = clause_manager_->NextClauseToProbe();
        if (clause == nullptr) break;
        if (at_least_one_literal_is_true(clause->AsSpan())) continue;

        tmp_dnf_.clear();
        for (const Literal literal : clause->AsSpan()) {
          if (assignment.LiteralIsAssigned(literal)) continue;
          tmp_dnf_.push_back({literal});
        }
        ++counters_.at_least_one_stats.num_calls;
        const MethodStats start_stats = GetStats(prober_);
        if (!prober_->ProbeDnf("at_least_one", tmp_dnf_,
                               Prober::DnfType::kAtLeastOne, clause)) {
          return SatSolver::INFEASIBLE;
        }
        AddStats(counters_.at_least_one_stats, start_stats, GetStats(prober_));

        RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
      }

      // Probe at_most_ones of the SAT model.
      for (;;) {
        const absl::Span<const Literal> at_most_one =
            binary_implication_graph_->NextAtMostOne();
        if (at_most_one.empty()) break;
        if (at_least_one_literal_is_true(at_most_one)) continue;

        tmp_dnf_.clear();
        tmp_literals_.clear();
        for (const Literal literal : at_most_one) {
          if (assignment.LiteralIsAssigned(literal)) continue;
          tmp_dnf_.push_back({literal});
          tmp_literals_.push_back(literal.Negated());
        }
        tmp_dnf_.push_back(tmp_literals_);
        ++counters_.at_most_one_stats.num_calls;
        const MethodStats start_stats = GetStats(prober_);
        if (!prober_->ProbeDnf("at_most_one", tmp_dnf_,
                               Prober::DnfType::kAtLeastOneOrZero)) {
          return SatSolver::INFEASIBLE;
        }
        AddStats(counters_.at_most_one_stats, start_stats, GetStats(prober_));

        RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
      }

      // Probe combinations of Booleans variables.
      const int limit = parameters_.probing_num_combinations_limit();
      const int max_num_bool_vars_for_pairs_probing =
          static_cast<int>(std::sqrt(2 * limit));
      const int num_bool_vars = bool_vars_.size();

      if (num_bool_vars < max_num_bool_vars_for_pairs_probing) {
        for (; current_bv1_ + 1 < bool_vars_.size(); ++current_bv1_) {
          const BooleanVariable bv1 = bool_vars_[current_bv1_];
          if (assignment.VariableIsAssigned(bv1)) continue;
          current_bv2_ = std::max(current_bv1_ + 1, current_bv2_);
          for (; current_bv2_ < bool_vars_.size(); ++current_bv2_) {
            const BooleanVariable& bv2 = bool_vars_[current_bv2_];
            if (assignment.VariableIsAssigned(bv2)) continue;
            ++counters_.pairs_stats.num_calls;
            const MethodStats start_stats = GetStats(prober_);
            if (!prober_->ProbeDnf("pair_of_bool_vars",
                                   {{Literal(bv1, false), Literal(bv2, false)},
                                    {Literal(bv1, false), Literal(bv2, true)},
                                    {Literal(bv1, true), Literal(bv2, false)},
                                    {Literal(bv1, true), Literal(bv2, true)}},
                                   Prober::DnfType::kAtLeastOneCombination)) {
              return SatSolver::INFEASIBLE;
            }
            AddStats(counters_.pairs_stats, start_stats, GetStats(prober_));
            RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
          }
          current_bv2_ = 0;
        }
      } else {
        for (; random_pair_of_bool_vars_probed_ < 10000;
             ++random_pair_of_bool_vars_probed_) {
          const BooleanVariable bv1 =
              bool_vars_[absl::Uniform<int>(random_, 0, bool_vars_.size())];
          if (assignment.VariableIsAssigned(bv1)) continue;
          const BooleanVariable bv2 =
              bool_vars_[absl::Uniform<int>(random_, 0, bool_vars_.size())];
          if (assignment.VariableIsAssigned(bv2) || bv1 == bv2) {
            continue;
          }
          ++counters_.pairs_stats.num_calls;
          const MethodStats start_stats = GetStats(prober_);
          if (!prober_->ProbeDnf("rnd_pair_of_bool_vars",
                                 {{Literal(bv1, false), Literal(bv2, false)},
                                  {Literal(bv1, false), Literal(bv2, true)},
                                  {Literal(bv1, true), Literal(bv2, false)},
                                  {Literal(bv1, true), Literal(bv2, true)}},
                                 Prober::DnfType::kAtLeastOneCombination)) {
            return SatSolver::INFEASIBLE;
          }
          AddStats(counters_.pairs_stats, start_stats, GetStats(prober_));

          RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
        }
      }
    }

    // Adjust the active_limit.
    if (use_shaving()) {
      const double dtime = parameters_.shaving_search_deterministic_time();
      const bool something_has_been_detected =
          counters_.shaving.num_new_bounds != initial_num_bounds_shaved ||
          prober_->num_new_literals_fixed() != initial_num_literals_fixed;
      if (something_has_been_detected) {  // Reset the limit.
        active_limit_ = dtime;
      } else if (active_limit_ <= 128 * dtime) {  // Bump the limit.
        active_limit_ *= 2;
      }
    }

    // Reset all counters.
    ++iteration_;
    current_bool_var_ = 0;
    current_int_var_ = 0;
    current_bv1_ = 0;
    current_bv2_ = 1;
    random_pair_of_bool_vars_probed_ = 0;
    binary_implication_graph_->ResetAtMostOneIterator();
    clause_manager_->ResetToProbeIndex();
    probed_bool_vars_.clear();
    shaved_literals_.clear();

    // Remove fixed Boolean variables.
    int new_size = 0;
    for (int i = 0; i < bool_vars_.size(); ++i) {
      if (!sat_solver_->Assignment().VariableIsAssigned(bool_vars_[i])) {
        bool_vars_[new_size++] = bool_vars_[i];
      }
    }
    bool_vars_.resize(new_size);

    // Remove fixed integer variables.
    new_size = 0;
    for (int i = 0; i < int_vars_.size(); ++i) {
      if (!integer_trail_->IsFixed(int_vars_[i])) {
        int_vars_[new_size++] = int_vars_[i];
      }
    }
    int_vars_.resize(new_size);
  }
  return SatSolver::LIMIT_REACHED;
}

#undef RETURN_IF_NOT_FEASIBLE

SatSolver::Status ContinuousProber::PeriodicSyncAndCheck() {
  // Check limit.
  if (--num_test_limit_remaining_ <= 0) {
    num_test_limit_remaining_ = kTestLimitPeriod;
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
  }

  // Log the state of the prober.
  if (--num_logs_remaining_ <= 0) {
    num_logs_remaining_ = kLogPeriod;
    LogStatistics();
  }

  // Sync with shared storage.
  if (--num_syncs_remaining_ <= 0) {
    num_syncs_remaining_ = kSyncPeriod;
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
    for (const auto& cb : level_zero_callbacks_->callbacks) {
      if (!cb()) {
        sat_solver_->NotifyThatModelIsUnsat();
        return SatSolver::INFEASIBLE;
      }
    }

    // Run inprocessing. Note that this does nothing if not enough dtime was
    // spent since the last call, so it is okay to call it relatively often.
    if (parameters_.use_sat_inprocessing() &&
        !inprocessing_->InprocessingRound()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return sat_solver_->UnsatStatus();
    }
  }

  return SatSolver::FEASIBLE;
}

SatSolver::Status ContinuousProber::ShaveLiteral(
    Literal literal, bool literal_is_an_integer_bound) {
  if (trail_->Assignment().LiteralIsAssigned(literal)) {
    return SatSolver::LIMIT_REACHED;
  }
  const auto [_, inserted] = shaved_literals_.insert(literal.Index());
  if (!inserted) return SatSolver::LIMIT_REACHED;
  counters_.shaving.num_calls++;

  const double original_dtime_limit = time_limit_->GetDeterministicLimit();
  time_limit_->ChangeDeterministicLimit(
      std::min(original_dtime_limit,
               time_limit_->GetElapsedDeterministicTime() + active_limit_));
  const SatSolver::Status status =
      ResetAndSolveIntegerProblem({literal}, model_);
  time_limit_->ChangeDeterministicLimit(original_dtime_limit);
  if (ReportStatus(status)) return status;

  // Important: we want to reset the solver right away, as we check for
  // fixed variable in the main loop!
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  if (status == SatSolver::ASSUMPTIONS_UNSAT) {
    if (literal_is_an_integer_bound) {
      counters_.shaving.num_new_bounds++;
    } else {
      counters_.shaving.num_new_literals_fixed++;
    }
    CHECK(trail_->Assignment().LiteralIsFalse(literal));
  }

  return status;
}

bool ContinuousProber::ReportStatus(const SatSolver::Status status) {
  return status == SatSolver::INFEASIBLE || status == SatSolver::FEASIBLE;
}

bool ContinuousProber::use_shaving() const {
  return parameters_.shaving_deterministic_time_in_probing_search() > 0.0 &&
         iteration_ % 2 == 0;
}

void ContinuousProber::LogStatistics() {
  if (shared_response_manager_ == nullptr ||
      shared_bounds_manager_ == nullptr) {
    return;
  }
  if (VLOG_IS_ON(1)) {
    shared_response_manager_->LogMessageWithThrottling(
        "Probe",
        absl::StrCat(
            " (iterations=", iteration_, " linearization_level=",
            parameters_.linearization_level(), " active_limit=", active_limit_,
            " shaving=", use_shaving(), " active_bool_vars=", bool_vars_.size(),
            " active_int_vars=", integer_trail_->NumIntegerVariables(),
            " literals fixed/probed=", prober_->num_new_literals_fixed(), "/",
            counters_.one_variable_stats.num_calls,
            " shaving calls/literals/bounds=", counters_.shaving.num_calls, "/",
            counters_.shaving.num_new_literals_fixed, "/",
            counters_.shaving.num_new_bounds, " new_integer_bounds=",
            shared_bounds_manager_->NumBoundsExported("probing"),
            " new_binary_clause=", prober_->num_new_binary_clauses(),
            " num_at_least_one_probed=", counters_.at_least_one_stats.num_calls,
            " num_at_most_one_probed=", counters_.at_most_one_stats.num_calls,
            ")"));
  }
}

ContinuousProber::MethodStats ContinuousProber::GetStats(Prober* prober) const {
  MethodStats stats;
  stats.num_new_bounds = prober->num_total_new_integer_bounds(),
  stats.num_new_literals_fixed = prober->num_total_new_literals_fixed(),
  stats.num_new_binary = prober->num_total_new_binary();
  return stats;
}

void ContinuousProber::AddStats(MethodStats& total_stats,
                                const MethodStats& start_stats,
                                const MethodStats& end_stats) {
  total_stats.num_calls += 1;
  total_stats.num_new_bounds +=
      end_stats.num_new_bounds - start_stats.num_new_bounds;
  total_stats.num_new_literals_fixed +=
      end_stats.num_new_literals_fixed - start_stats.num_new_literals_fixed;
  total_stats.num_new_binary +=
      end_stats.num_new_binary - start_stats.num_new_binary;
}

}  // namespace sat
}  // namespace operations_research
