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
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
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
#include "ortools/util/running_stat.h"
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
      base_dtime_(parameters_.shaving_deterministic_time_in_probing_search()),
      use_shaving_(base_dtime_ > 0),
      at_least_one_stats_("at_least_one"),
      at_most_one_stats_("at_most_one"),
      pairs_stats_("pairs"),
      bool_probing_("bool_probing"),
      int_probing_("int_probing"),
      bool_shaving_(base_dtime_, "bool_shaving"),
      int_shaving_(base_dtime_, "int_shaving") {
  auto* mapping = model_->GetOrCreate<CpModelMapping>();

  // Build variable lists.
  //
  // Note: we use the model variables since most Boolean variables created
  // during search are bounds on integer variables.
  for (int v = 0; v < model_proto.variables_size(); ++v) {
    if (mapping->IsBoolean(v)) {
      const BooleanVariable bool_var = mapping->Literal(v).Variable();
      bool_vars_to_probe_.push_back(bool_var);
    } else {
      IntegerVariable var = mapping->Integer(v);
      if (integer_trail_->IsFixed(var)) continue;
      int_vars_to_probe_.push_back(var);
    }
  }
  bool_vars_to_shave_ = bool_vars_to_probe_;
  int_vars_to_shave_ = int_vars_to_probe_;

  bool_shaving_.success_tracker.Reset(
      std::min(static_cast<int>(bool_vars_to_shave_.size()),
               ShavingStats::kRateWindowSizeMax));
  bool_shaving_.limit_update_frequency = ShavingStats::kUpdateFrequency;
  int_shaving_.success_tracker.Reset(
      std::min(static_cast<int>(int_vars_to_shave_.size()),
               ShavingStats::kRateWindowSizeMax));
  int_shaving_.limit_update_frequency = ShavingStats::kUpdateFrequency;

  std::shuffle(bool_vars_to_probe_.begin(), bool_vars_to_probe_.end(), random_);
  std::shuffle(int_vars_to_probe_.begin(), int_vars_to_probe_.end(), random_);
  std::shuffle(bool_vars_to_shave_.begin(), bool_vars_to_shave_.end(), random_);
  std::shuffle(int_vars_to_shave_.begin(), int_vars_to_shave_.end(), random_);

  CompactVectorVector<int, int> orbits;
  std::vector<int> var_to_orbit_index;
  GetOrbitsAndRepresentatives(model_proto, orbits, var_to_orbit_index,
                              var_to_representative_);

  VLOG(2) << "Start continuous probing with " << bool_vars_to_probe_.size()
          << " Boolean variables,  " << int_vars_to_probe_.size()
          << " integer variables, deterministic time limit = "
          << time_limit_->GetDeterministicLimit() << " on " << model_->Name();
}

ContinuousProber::~ContinuousProber() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  const MethodStats* all_stats[] = {
      &bool_probing_,       &int_probing_,       &bool_shaving_, &int_shaving_,
      &at_least_one_stats_, &at_most_one_stats_, &pairs_stats_,
  };
  for (const MethodStats* method_stats : all_stats) {
    method_stats->AddToStats(stats);
  }
  shared_stats_->AddStats(stats);
}

void ContinuousProber::CompactAndShuffleBooleanVariables(
    std::vector<BooleanVariable>& bool_vars) {
  int new_size = 0;
  for (int i = 0; i < bool_vars.size(); ++i) {
    if (ShouldProbe(bool_vars[i])) {
      bool_vars[new_size++] = bool_vars[i];
    }
  }
  bool_vars.resize(new_size);
  std::shuffle(bool_vars.begin(), bool_vars.end(), random_);
}

void ContinuousProber::CompactAndShuffleIntegerVariables(
    std::vector<IntegerVariable>& int_vars) {
  int new_size = 0;
  for (int i = 0; i < int_vars.size(); ++i) {
    if (!integer_trail_->IsFixed(int_vars[i])) {
      int_vars[new_size++] = int_vars[i];
    }
  }
  int_vars.resize(new_size);
  std::shuffle(int_vars.begin(), int_vars.end(), random_);
}

#define RETURN_IF_VALUE(fun)                                           \
  {                                                                    \
    const std::optional<SatSolver::Status> status = fun(time_chunk()); \
    if (status.has_value()) {                                          \
      return status.value();                                           \
    }                                                                  \
  }

// Continuous probing procedure.
SatSolver::Status ContinuousProber::Probe() {
  while (!time_limit_->LimitReached()) {
    const double kMaxProbingTimePerMethod = 0.1;
    const auto time_chunk = [this, kMaxProbingTimePerMethod]() {
      return time_limit_->GetElapsedDeterministicTime() +
             kMaxProbingTimePerMethod;
    };

    RETURN_IF_VALUE(ProbeIntegerVariables);
    RETURN_IF_VALUE(ShaveIntegerVariables);
    RETURN_IF_VALUE(ProbeBooleanVariables);
    RETURN_IF_VALUE(ShaveBooleanVariables);
    RETURN_IF_VALUE(ProbeAtLeastOnes);
    RETURN_IF_VALUE(ProbeAtMostOnes);
    RETURN_IF_VALUE(ProbePairsOfBoolVars);

    // Probing loop finishes checks & compaction.
    if (bool_probing_.current_var >= bool_vars_to_probe_.size() &&
        !bool_vars_to_probe_.empty()) {
      CompactAndShuffleBooleanVariables(bool_vars_to_probe_);
      bool_probing_.current_var = 0;
      ++bool_probing_.iteration;
    }

    if (int_probing_.current_var >= int_vars_to_probe_.size() &&
        !int_vars_to_probe_.empty()) {
      CompactAndShuffleIntegerVariables(int_vars_to_probe_);
      int_probing_.current_var = 0;
      ++int_probing_.iteration;
    }

    if (bool_shaving_.current_var >= bool_vars_to_shave_.size() &&
        !bool_vars_to_shave_.empty()) {
      CompactAndShuffleBooleanVariables(bool_vars_to_shave_);
      bool_shaving_.current_var = 0;
      ++bool_shaving_.iteration;
    }

    if (int_shaving_.current_var >= int_vars_to_shave_.size() &&
        !int_vars_to_shave_.empty()) {
      CompactAndShuffleIntegerVariables(int_vars_to_shave_);
      int_shaving_.current_var = 0;
      ++int_shaving_.iteration;
    }
  }

  return SatSolver::LIMIT_REACHED;
}

#undef RETURN_IF_VALUE

std::optional<SatSolver::Status> ContinuousProber::ProbeIntegerVariables(
    double deadline) {
  if (int_vars_to_shave_.empty()) return std::nullopt;
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  for (; int_probing_.current_var < int_vars_to_probe_.size();
       ++int_probing_.current_var) {
    const IntegerVariable int_var =
        int_vars_to_probe_[int_probing_.current_var];
    if (integer_trail_->IsFixed(int_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromIntegerVariable(int_var))) {
      continue;
    }
    if (time_limit_->GetElapsedDeterministicTime() > deadline) {
      break;
    }

    const bool var_is_size_too = integer_trail_->LowerBound(int_var) + 1 ==
                                 integer_trail_->UpperBound(int_var);

    const Literal probe_lb_literal =
        encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(
            int_var, integer_trail_->LowerBound(int_var)));
    const BooleanVariable probe_lb_var = probe_lb_literal.Variable();
    const MethodStats start_lb_stats = GetStats(prober_);
    if (!prober_->ProbeOneVariable(probe_lb_var)) {
      return SatSolver::INFEASIBLE;
    }
    AddStats(int_probing_, start_lb_stats, GetStats(prober_));

    if (!var_is_size_too) {
      const Literal probe_ub_literal =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              int_var, integer_trail_->UpperBound(int_var)));
      const BooleanVariable probe_ub_var = probe_ub_literal.Variable();
      const MethodStats start_ub_stats = GetStats(prober_);
      if (!prober_->ProbeOneVariable(probe_ub_var)) {
        return SatSolver::INFEASIBLE;
      }
      AddStats(int_probing_, start_ub_stats, GetStats(prober_));
    }

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ShaveIntegerVariables(
    double deadline) {
  if (int_vars_to_shave_.empty() || !use_shaving_) return std::nullopt;
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  for (; int_shaving_.current_var < int_vars_to_shave_.size();
       ++int_shaving_.current_var) {
    const IntegerVariable int_var =
        int_vars_to_shave_[int_shaving_.current_var];
    if (integer_trail_->IsFixed(int_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromIntegerVariable(int_var))) {
      continue;
    }
    if (time_limit_->GetElapsedDeterministicTime() > deadline) {
      break;
    }

    const Literal shave_lb_literal =
        encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(
            int_var, integer_trail_->LowerBound(int_var)));
    const SatSolver::Status lb_status =
        ShaveLiteral(shave_lb_literal, /*literal_is_an_integer_bound=*/true);
    if (ReportStatus(lb_status)) return lb_status;

    const Literal shave_ub_literal =
        encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
            int_var, integer_trail_->UpperBound(int_var)));
    const SatSolver::Status ub_status =
        ShaveLiteral(shave_ub_literal, /*literal_is_an_integer_bound=*/true);
    if (ReportStatus(ub_status)) return ub_status;

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ProbeBooleanVariables(
    double deadline) {
  if (bool_vars_to_probe_.empty()) return std::nullopt;
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  for (; bool_probing_.current_var < bool_vars_to_probe_.size();
       ++bool_probing_.current_var) {
    const BooleanVariable bool_var =
        bool_vars_to_probe_[bool_probing_.current_var];
    if (time_limit_->GetElapsedDeterministicTime() > deadline) {
      break;
    }

    if (!ShouldProbe(bool_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromBooleanVariable(bool_var))) {
      continue;
    }

    const MethodStats start_stats = GetStats(prober_);
    if (!prober_->ProbeOneVariable(bool_var)) {
      return SatSolver::INFEASIBLE;
    }
    AddStats(bool_probing_, start_stats, GetStats(prober_));

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ShaveBooleanVariables(
    double deadline) {
  if (bool_vars_to_shave_.empty() || !use_shaving_) return std::nullopt;
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  for (; bool_shaving_.current_var < bool_vars_to_shave_.size();
       ++bool_shaving_.current_var) {
    const BooleanVariable bool_var =
        bool_vars_to_shave_[bool_shaving_.current_var];
    if (time_limit_->GetElapsedDeterministicTime() > deadline) {
      break;
    }

    if (!ShouldProbe(bool_var)) continue;
    if (!IsOrbitRepresentative(
            cp_model_mapping_->GetProtoVariableFromBooleanVariable(bool_var))) {
      continue;
    }

    const Literal literal(bool_var, true);
    const SatSolver::Status true_status =
        ShaveLiteral(literal, /*literal_is_an_integer_bound=*/false);
    if (ReportStatus(true_status)) return true_status;

    if (true_status == SatSolver::ASSUMPTIONS_UNSAT) continue;

    const SatSolver::Status false_status =
        ShaveLiteral(literal.Negated(), /*literal_is_an_integer_bound=*/false);
    if (ReportStatus(false_status)) return false_status;

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
    double deadline) {
  if (!parameters_.use_extended_probing()) return std::nullopt;

  const auto& assignment = sat_solver_->Assignment();
  while (time_limit_->GetElapsedDeterministicTime() <= deadline) {
    // Backtrack to level 0 in case we are not there.
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

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
    ++at_least_one_stats_.num_calls;
    const MethodStats start_stats = GetStats(prober_);
    if (!prober_->ProbeDnf("at_least_one", tmp_dnf_,
                           Prober::DnfType::kAtLeastOne, clause)) {
      return SatSolver::INFEASIBLE;
    }
    AddStats(at_least_one_stats_, start_stats, GetStats(prober_));

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

std::optional<SatSolver::Status> ContinuousProber::ProbeAtMostOnes(
    double deadline) {
  if (!parameters_.use_extended_probing()) return std::nullopt;

  const auto& assignment = sat_solver_->Assignment();
  while (time_limit_->GetElapsedDeterministicTime() <= deadline) {
    // Backtrack to level 0 in case we are not there.
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

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
    ++at_most_one_stats_.num_calls;
    const MethodStats start_stats = GetStats(prober_);
    if (!prober_->ProbeDnf("at_most_one", tmp_dnf_,
                           Prober::DnfType::kAtLeastOneOrZero)) {
      return SatSolver::INFEASIBLE;
    }
    AddStats(at_most_one_stats_, start_stats, GetStats(prober_));

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
  }
  return std::nullopt;
}

// TODO(user): Maintain a lazy unsorted list of all unassigned Boolean
// variables.
std::optional<SatSolver::Status> ContinuousProber::ProbePairsOfBoolVars(
    double deadline) {
  if (!parameters_.use_extended_probing()) return std::nullopt;

  const int num_bool_vars = sat_solver_->NumVariables();
  if (num_bool_vars < 2) return std::nullopt;

  for (int i = 0; i < kNumPairsOfBooleanVarsToProbe; ++i) {
    // Backtrack to level 0 in case we are not there.
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

    const BooleanVariable bv1 =
        BooleanVariable(absl::Uniform<int>(random_, 0, num_bool_vars));
    if (!ShouldProbe(bv1)) continue;

    const BooleanVariable bv2 =
        BooleanVariable(absl::Uniform<int>(random_, 0, num_bool_vars));
    if (bv1 == bv2 || !ShouldProbe(bv2)) continue;
    if (time_limit_->GetElapsedDeterministicTime() > deadline) break;

    const MethodStats start_stats = GetStats(prober_);
    ++pairs_stats_.num_calls;
    if (!prober_->ProbeDnf("rnd_pair_of_bool_vars",
                           {{Literal(bv1, false), Literal(bv2, false)},
                            {Literal(bv1, false), Literal(bv2, true)},
                            {Literal(bv1, true), Literal(bv2, false)},
                            {Literal(bv1, true), Literal(bv2, true)}},
                           Prober::DnfType::kAtLeastOneCombination)) {
      return SatSolver::INFEASIBLE;
    }
    AddStats(pairs_stats_, start_stats, GetStats(prober_));

    RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
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
  ShavingStats& state =
      literal_is_an_integer_bound ? int_shaving_ : bool_shaving_;
  state.num_calls++;

  const double original_dtime_limit = time_limit_->GetDeterministicLimit();
  time_limit_->ChangeDeterministicLimit(std::min(
      original_dtime_limit, time_limit_->GetElapsedDeterministicTime() +
                                base_dtime_ * state.limit_multiplier));
  const SatSolver::Status status =
      ResetAndSolveIntegerProblem({literal}, model_);
  state.AdaptMultiplier(
      status == SatSolver::ASSUMPTIONS_UNSAT || status == SatSolver::FEASIBLE,
      literal_is_an_integer_bound ? int_vars_to_shave_.size()
                                  : bool_vars_to_shave_.size());
  time_limit_->ChangeDeterministicLimit(original_dtime_limit);
  if (ReportStatus(status)) return status;

  // Important: we want to reset the solver right away, as we check for
  // fixed variable in the main loop!
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  if (status == SatSolver::ASSUMPTIONS_UNSAT) {
    if (literal_is_an_integer_bound) {
      state.num_new_bounds++;
    } else {
      state.num_new_literals_fixed++;
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
    const std::string bool_probing_str =
        bool_probing_.num_calls > 0
            ? absl::StrCat(" bool_probing{", bool_probing_.StatsStr(), "}")
            : "";
    const std::string int_probing_str =
        int_probing_.num_calls > 0
            ? absl::StrCat(" int_probing{", int_probing_.StatsStr(), "}")
            : "";
    const std::string bool_shaving_str =
        bool_shaving_.num_calls > 0 && use_shaving_
            ? absl::StrCat(" bool_shaving{", bool_shaving_.StatsStr(), "}")
            : "";
    const std::string int_shaving_str =
        int_shaving_.num_calls > 0 && use_shaving_
            ? absl::StrCat(" int_shaving{", int_shaving_.StatsStr(), "}")
            : "";
    const std::string pairs_str =
        pairs_stats_.num_calls > 0
            ? absl::StrCat(" pairs{", pairs_stats_.StatsStr(), "}")
            : "";
    const std::string at_least_one_str =
        at_least_one_stats_.num_calls > 0
            ? absl::StrCat(" at_least_one{", at_least_one_stats_.StatsStr(),
                           "}")
            : "";
    const std::string at_most_one_str =
        at_most_one_stats_.num_calls > 0
            ? absl::StrCat(" at_most_one{", at_most_one_stats_.StatsStr(), "}")
            : "";

    shared_response_manager_->LogMessageWithThrottling(
        "Stats",
        absl::StrCat(name, "(bool_vars=", bool_vars_to_probe_.size(),
                     " int_vars=", int_vars_to_probe_.size(),
                     " new_integer_bounds=",
                     shared_bounds_manager_->NumBoundsExported(name),
                     " new_binary_clause=", prober_->num_total_new_binary(),
                     bool_probing_str, bool_shaving_str, int_probing_str,
                     int_shaving_str, at_least_one_str, at_most_one_str,
                     pairs_str, ")"));
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

void ContinuousProber::MethodStats::AddToStats(
    std::vector<std::pair<std::string, int64_t>>& stats) const {
  if (num_calls == 0) return;
  const std::string prefix = absl::StrCat("ContinuousProber/", name);
  stats.push_back({absl::StrCat(prefix, "/num_calls"), num_calls});
  stats.push_back({absl::StrCat(prefix, "/num_new_bounds"), num_new_bounds});
  stats.push_back({absl::StrCat(prefix, "/num_new_literals_fixed"),
                   num_new_literals_fixed});
  stats.push_back({absl::StrCat(prefix, "/num_new_binary"), num_new_binary});
}

std::string ContinuousProber::MethodStats::StatsStr() const {
  std::string result;
  if (num_calls != 0) {
    absl::StrAppend(&result, "calls=", num_calls);
  }
  if (num_new_bounds != 0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "bounds=", num_new_bounds);
  }
  if (num_new_literals_fixed != 0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "literals=", num_new_literals_fixed);
  }
  if (num_new_binary != 0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "binary=", num_new_binary);
  }
  return result;
}

void ContinuousProber::ProbingStats::AddToStats(
    std::vector<std::pair<std::string, int64_t>>& stats) const {
  MethodStats::AddToStats(stats);
  const std::string prefix = absl::StrCat("ContinuousProber/", name);
  stats.push_back({absl::StrCat(prefix, "/iterations"), iteration});
}

std::string ContinuousProber::ProbingStats::StatsStr() const {
  std::string result;
  if (iteration != 0) {
    absl::StrAppend(&result, "iterations=", iteration);
  }
  const std::string method_stats = MethodStats::StatsStr();
  if (!method_stats.empty()) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, method_stats);
  }
  return result;
}

void ContinuousProber::ShavingStats::AdaptMultiplier(bool success,
                                                     int num_vars) {
  if (success) {
    success_tracker.Add(1);
    ineffective_streak = 0;
  } else {
    success_tracker.Add(0);
    // If we are at the max limit multiplier and the success rate is 0, we
    // increase the ineffective streak.
    if (limit_multiplier == kLimitMultiplierMax) ++ineffective_streak;
  }

  if (--limit_update_frequency <= 0) {
    limit_update_frequency = kUpdateFrequency;
    const double rate = success_tracker.WindowAverage();
    if (rate == 0.0 && limit_multiplier == kLimitMultiplierMax) {
      // If the streak is long enough, we reset the multiplier to 1.0.
      if (ineffective_streak > 4 * num_vars) {  // 2 full rounds.
        ++limit_reset_count;
        limit_multiplier = 1.0;
        ineffective_streak = 0;
      }
    } else if (rate < 0.05 && limit_multiplier < kLimitMultiplierMax) {
      limit_multiplier =
          std::min(kLimitMultiplierMax, limit_multiplier * kLimitIncrease);
    }
  }
}

void ContinuousProber::ShavingStats::AddToStats(
    std::vector<std::pair<std::string, int64_t>>& stats) const {
  ProbingStats::AddToStats(stats);
  stats.push_back({absl::StrCat("ContinuousProber/", name, "/limit_reset"),
                   limit_reset_count});
}

std::string ContinuousProber::ShavingStats::StatsStr() const {
  std::string result = ProbingStats::StatsStr();
  const double limit = base_dtime * limit_multiplier;
  if (limit != 0.0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "limit=", limit);
  }
  if (ineffective_streak != 0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "bad_streak=", ineffective_streak);
  }
  if (limit_reset_count != 0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "resets=", limit_reset_count);
  }
  const double success_rate = success_tracker.WindowAverage();
  if (success_rate != 0.0) {
    if (!result.empty()) absl::StrAppend(&result, " ");
    absl::StrAppend(&result, "rate=", success_rate);
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
