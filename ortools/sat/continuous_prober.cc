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
#include <optional>
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
#include "ortools/sat/symmetry_util.h"
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
      cp_model_mapping_(model->GetOrCreate<CpModelMapping>()),
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
      base_dtime_(parameters_.shaving_deterministic_time_in_probing_search()) {
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

  CompactVectorVector<int, int> orbits;
  std::vector<int> var_to_orbit_index;
  GetOrbitsAndRepresentatives(model_proto, orbits, var_to_orbit_index,
                              var_to_representative_);

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
    if (method_stats.num_calls == 0) continue;
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
//   - sort variables before the iteration (statically or dynamically) ?
//   - move the shaving code directly in the probing class ?
//   - probe all variables and not just the model ones ?
SatSolver::Status ContinuousProber::Probe() {
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  while (!time_limit_->LimitReached()) {
    const double kMaxProbingTimePerMethod = 0.1;
    // Store current statistics to detect shaving without any improvement.
    int64_t starting_shaving_literals =
        counters_.shaving.num_new_literals_fixed;
    int64_t starting_shaving_bounds = counters_.shaving.num_new_bounds;

    std::optional<SatSolver::Status> status = ProbeIntegerVariables(
        time_limit_->GetElapsedDeterministicTime() + kMaxProbingTimePerMethod);
    if (status.has_value()) return status.value();

    status = ProbeBooleanVariables(time_limit_->GetElapsedDeterministicTime() +
                                   kMaxProbingTimePerMethod);
    if (status.has_value()) return status.value();

    if (parameters_.use_extended_probing()) {
      status = ProbeAtLeastOnes(time_limit_->GetElapsedDeterministicTime() +
                                kMaxProbingTimePerMethod);
      if (status.has_value()) return status.value();

      status = ProbeAtMostOnes(time_limit_->GetElapsedDeterministicTime() +
                               kMaxProbingTimePerMethod);
      if (status.has_value()) return status.value();

      status = ProbePairsOfBoolVars(time_limit_->GetElapsedDeterministicTime() +
                                    kMaxProbingTimePerMethod);
      if (status.has_value()) return status.value();
    }

    // An iteration is finished when all Boolean and integer variables have been
    // probed.
    if (current_bool_var_ >= bool_vars_.size() &&
        current_int_var_ >= int_vars_.size()) {
      ++iteration_;

      // Reset counters.
      current_bool_var_ = 0;
      current_int_var_ = 0;
      probed_bool_vars_.clear();
      shaved_literals_.clear();

      // Alternate between shaving and non-shaving phases.
      if (use_shaving_) {
        if (counters_.shaving.num_new_literals_fixed >
                starting_shaving_literals ||
            counters_.shaving.num_new_bounds > starting_shaving_bounds) {
          // We improved something. We reduce the multiplier by a factor of 2.
          // TODO(user): We could not go below 1.0 and increase the base
          // dtime.
          limit_multiplier_ = std::max(1.0, limit_multiplier_ / 2);

          // Update reference counters.
          starting_shaving_literals = counters_.shaving.num_new_literals_fixed;
          starting_shaving_bounds = counters_.shaving.num_new_bounds;
        } else if (limit_multiplier_ < 16.0) {
          limit_multiplier_ *= 1.5;  // Cap the maximum limit to 16 * dtime.
        }

        // Alternating shaving and non-shaving iterations.
        use_shaving_ = false;
      } else {
        use_shaving_ = base_dtime_ > 0.0;
      }

      // Remove fixed Boolean variables.
      int new_size = 0;
      for (int i = 0; i < bool_vars_.size(); ++i) {
        // Remap the indices of pairs of Boolean variables to probe.
        if (current_bv1_ == i) current_bv1_ = new_size;
        if (current_bv2_ == i) current_bv2_ = new_size;
        // Remap the index of the current Boolean variable to probe. This is
        // more robust if we compact variables outside the end of the iteration.
        if (current_bool_var_ == i) current_bool_var_ = new_size;

        if (ShouldProbe(bool_vars_[i])) {
          bool_vars_[new_size++] = bool_vars_[i];
        }
      }
      bool_vars_.resize(new_size);

      // Remove fixed integer variables.
      new_size = 0;
      for (int i = 0; i < int_vars_.size(); ++i) {
        // Remap the index of the current integer variable to probe. This is
        // more robust if we compact variables outside the end of the iteration.
        if (current_int_var_ == i) current_int_var_ = new_size;

        if (!integer_trail_->IsFixed(int_vars_[i])) {
          int_vars_[new_size++] = int_vars_[i];
        }
      }
      int_vars_.resize(new_size);
    }

    // Update the pairs of variables to probe asynchronously w.r.t. the
    // iterations on variables.
    if (current_bv1_ >= bool_vars_.size() &&
        current_bv2_ >= bool_vars_.size()) {
      current_bv1_ = 0;
      current_bv2_ = 1;
    }
    random_pair_of_bool_vars_probed_ = 0;
  }

  return SatSolver::LIMIT_REACHED;
}

std::optional<SatSolver::Status> ContinuousProber::ProbeIntegerVariables(
    double time_limit) {
  for (; current_int_var_ < int_vars_.size(); ++current_int_var_) {
    const IntegerVariable int_var = int_vars_[current_int_var_];
    if (integer_trail_->IsFixed(int_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromIntegerVariable(int_var))) {
      continue;
    }
    if (time_limit_->GetElapsedDeterministicTime() > time_limit) {
      break;
    }

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

    if (use_shaving_) {
      const SatSolver::Status lb_status =
          ShaveLiteral(shave_lb_literal, /*literal_is_an_integer_bound=*/true);
      if (ReportStatus(lb_status)) return lb_status;

      const SatSolver::Status ub_status =
          ShaveLiteral(shave_ub_literal, /*literal_is_an_integer_bound=*/true);
      if (ReportStatus(ub_status)) return ub_status;
    }

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ProbeBooleanVariables(
    double time_limit) {
  for (; current_bool_var_ < bool_vars_.size(); ++current_bool_var_) {
    const BooleanVariable& bool_var = bool_vars_[current_bool_var_];
    if (time_limit_->GetElapsedDeterministicTime() > time_limit) {
      break;
    }

    if (!ShouldProbe(bool_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromBooleanVariable(bool_var))) {
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
    if (use_shaving_) {
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
  return std::nullopt;
}

namespace {
bool AtLeastOneLiteralIsTrue(absl::Span<const Literal> literals,
                             const VariablesAssignment& assignment) {
  for (const Literal literal : literals) {
    if (assignment.LiteralIsTrue(literal)) {
      return true;
    }
  }
  return false;
};
}  // namespace

std::optional<SatSolver::Status> ContinuousProber::ProbeAtLeastOnes(
    double time_limit) {
  const auto& assignment = sat_solver_->Assignment();
  while (time_limit_->GetElapsedDeterministicTime() <= time_limit) {
    const SatClause* clause = clause_manager_->NextClauseToProbe();
    if (clause == nullptr) {
      clause_manager_->ResetToProbeIndex();
      break;
    }
    if (AtLeastOneLiteralIsTrue(clause->AsSpan(), assignment)) continue;

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
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ProbeAtMostOnes(
    double time_limit) {
  const auto& assignment = sat_solver_->Assignment();
  while (time_limit_->GetElapsedDeterministicTime() <= time_limit) {
    const absl::Span<const Literal> at_most_one =
        binary_implication_graph_->NextAtMostOne();
    if (at_most_one.empty()) {
      binary_implication_graph_->ResetAtMostOneIterator();
      break;
    }
    if (AtLeastOneLiteralIsTrue(at_most_one, assignment)) continue;

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
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ProbePairsOfBoolVars(
    double time_limit) {
  const int limit = parameters_.probing_num_combinations_limit();
  const int max_num_bool_vars_for_pairs_probing =
      static_cast<int>(std::sqrt(2 * limit));
  const int num_bool_vars = bool_vars_.size();

  if (num_bool_vars < max_num_bool_vars_for_pairs_probing) {
    for (; current_bv1_ + 1 < bool_vars_.size(); ++current_bv1_) {
      const BooleanVariable bv1 = bool_vars_[current_bv1_];
      // We update bv2_ unconditionally to support the case where no Boolean
      // variables are free after this point.
      current_bv2_ = std::max(current_bv1_ + 1, current_bv2_);
      if (!ShouldProbe(bv1)) continue;
      if (!IsOrbitRepresentative(
              cp_model_mapping_->GetProtoVariableFromBooleanVariable(bv1))) {
        continue;
      }

      for (; current_bv2_ < bool_vars_.size(); ++current_bv2_) {
        const BooleanVariable& bv2 = bool_vars_[current_bv2_];
        if (time_limit_->GetElapsedDeterministicTime() > time_limit) {
          break;
        }
        if (!ShouldProbe(bv2)) continue;
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
      if (!ShouldProbe(bv1)) continue;
      const BooleanVariable bv2 =
          bool_vars_[absl::Uniform<int>(random_, 0, bool_vars_.size())];
      if (bv1 == bv2 || !ShouldProbe(bv2)) continue;
      if (time_limit_->GetElapsedDeterministicTime() > time_limit) {
        break;
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
  return std::nullopt;
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

    // Run inprocessing. Note that this does nothing if not enough
    // deterministic time was spent since the last call, so it is okay to call
    // it relatively often.
    if (parameters_.use_sat_inprocessing() &&
        !inprocessing_->InprocessingRound()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return sat_solver_->UnsatStatus();
    }
  }

  return SatSolver::FEASIBLE;
}

bool ContinuousProber::ShouldProbe(BooleanVariable bool_var) const {
  return !sat_solver_->Assignment().VariableIsAssigned(bool_var) &&
         !binary_implication_graph_->IsRedundant(Literal(bool_var, true));
}

bool ContinuousProber::IsOrbitRepresentative(int var) const {
  if (var_to_representative_.empty()) return true;
  return var_to_representative_[var] == var;
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
  time_limit_->ChangeDeterministicLimit(std::min(
      original_dtime_limit, time_limit_->GetElapsedDeterministicTime() +
                                base_dtime_ * limit_multiplier_));
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

void ContinuousProber::LogStatistics() {
  if (shared_response_manager_ == nullptr ||
      shared_bounds_manager_ == nullptr) {
    return;
  }
  if (VLOG_IS_ON(1)) {
    const std::string& name = model_->Name();
    const int64_t num_probing_calls = counters_.one_variable_stats.num_calls +
                                      counters_.at_least_one_stats.num_calls +
                                      counters_.at_most_one_stats.num_calls +
                                      counters_.pairs_stats.num_calls;
    shared_response_manager_->LogMessageWithThrottling(
        "Stats",
        absl::StrCat(
            name, "(iterations=", iteration_,
            " linearization_level=", parameters_.linearization_level(),
            " active_bool_vars=", bool_vars_.size(),
            " active_int_vars=", int_vars_.size(), " shaving=", use_shaving_,
            " active_limit=", base_dtime_ * limit_multiplier_,
            " exported_integer_bounds=",
            shared_bounds_manager_->NumBoundsExported(name),
            " new_binary_clause=", prober_->num_total_new_binary(),
            " probing: literals_fixed=",
            prober_->num_total_new_literals_fixed(),
            ",bounds_reduced=", prober_->num_total_new_integer_bounds(),
            ",calls=", num_probing_calls, " shaving: literals_fixed=",
            counters_.shaving.num_new_literals_fixed,
            ",bounds_reduced=", counters_.shaving.num_new_bounds,
            ",calls=", counters_.shaving.num_calls, ")"));
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
