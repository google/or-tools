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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "benchmark/benchmark.h"
#include "google/protobuf/arena.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_copy.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research::sat {
namespace {

enum ModelKind {
  kBooleanHeavy,
  kLinearHeavy,
  kSchedulingHeavy,
  kRoutingHeavy,
  kGeneralCpHeavy,
};

struct PlantedModel {
  CpModelProto proto;
  std::vector<int64_t> planted_sol;
  std::vector<int> bool_vars;  // domain [0, 1]
  std::vector<int> int_vars;   // initial domain width >= 2 (ub - lb >= 2)
};

struct BoundsBatch {
  std::vector<int> vars;
  std::vector<int64_t> lbs;
  std::vector<int64_t> ubs;
};

struct MixedBatch {
  std::vector<int> vars;
  std::vector<int64_t> lbs;
  std::vector<int64_t> ubs;
  std::vector<std::pair<int, int>> binary_clauses;
};

int AddBoolVar(PlantedModel& model, int64_t val) {
  CHECK(val == 0 || val == 1);
  const int var = model.proto.variables_size();
  auto* v = model.proto.add_variables();
  v->add_domain(0);
  v->add_domain(1);
  model.planted_sol.push_back(val);
  model.bool_vars.push_back(var);
  return var;
}

int AddIntVar(PlantedModel& model, int64_t lb, int64_t ub, int64_t val) {
  CHECK_LE(lb, val);
  CHECK_LE(val, ub);
  const int var = model.proto.variables_size();
  auto* v = model.proto.add_variables();
  v->add_domain(lb);
  v->add_domain(ub);
  model.planted_sol.push_back(val);
  if (lb == 0 && ub == 1) {
    model.bool_vars.push_back(var);
  } else if (ub - lb >= 2) {
    model.int_vars.push_back(var);
  }
  return var;
}

void SetAffineExpr(LinearExpressionProto* expr, int var, int64_t coeff = 1,
                   int64_t offset = 0) {
  expr->Clear();
  if (coeff != 0) {
    expr->add_vars(var);
    expr->add_coeffs(coeff);
  }
  if (offset != 0) {
    expr->set_offset(offset);
  }
}

void SetConstExpr(LinearExpressionProto* expr, int64_t offset) {
  expr->Clear();
  if (offset != 0) {
    expr->set_offset(offset);
  }
}

// Samples `count` distinct entries from `source` and appends them to `out`.
void SampleDistinctFrom(const std::vector<int>& source, int count,
                        std::mt19937& rng, std::vector<int>& out) {
  const int n = static_cast<int>(source.size());
  const int target_size = static_cast<int>(out.size()) + std::min(count, n);
  while (static_cast<int>(out.size()) < target_size) {
    const int candidate = source[absl::Uniform(rng, 0, n)];
    if (std::find(out.begin(), out.end(), candidate) == out.end()) {
      out.push_back(candidate);
    }
  }
}

// Samples `count` distinct elements from `pool` using partial Fisher-Yates,
// modifying `pool` order in place.
void SampleDistinctFromPool(std::vector<int>& pool, int count,
                            std::mt19937& rng, std::vector<int>& out) {
  const int n = static_cast<int>(pool.size());
  const int k = std::min(count, n);
  out.clear();
  out.reserve(k);
  for (int i = 0; i < k; ++i) {
    const int j = absl::Uniform(rng, i, n);
    std::swap(pool[i], pool[j]);
    out.push_back(pool[i]);
  }
}

void PopulateObjective(PlantedModel& model, double inclusion_prob,
                       std::mt19937& rng) {
  CpObjectiveProto* obj = model.proto.mutable_objective();
  obj->set_offset(10.0);
  obj->set_integer_before_offset(10);
  obj->set_integer_scaling_factor(1);
  const int num_vars = model.proto.variables_size();
  for (int v = 0; v < num_vars; ++v) {
    if (absl::Bernoulli(rng, inclusion_prob)) {
      const int mag = absl::Uniform(absl::IntervalClosed, rng, 1, 10);
      const int64_t coeff = absl::Bernoulli(rng, 0.5) ? mag : -mag;
      obj->add_vars(v);
      obj->add_coeffs(coeff);
    }
  }
}

int TrueLitFor(const PlantedModel& model, int var) {
  return model.planted_sol[var] == 1 ? var : NegatedRef(var);
}

int FalseLitFor(const PlantedModel& model, int var) {
  return model.planted_sol[var] == 0 ? var : NegatedRef(var);
}

PlantedModel BuildBooleanHeavyModel(int target_num_vars, std::mt19937& rng) {
  PlantedModel model;
  const int num_bools = std::max(10, (target_num_vars * 9) / 10);
  const int num_ints = std::max(2, target_num_vars - num_bools);
  model.proto.mutable_variables()->Reserve(num_bools + num_ints);
  model.planted_sol.reserve(num_bools + num_ints);

  for (int i = 0; i < num_bools; ++i) {
    AddBoolVar(model, absl::Uniform(absl::IntervalClosed, rng, 0, 1));
  }
  for (int i = 0; i < num_ints; ++i) {
    AddIntVar(model, 0, 100, absl::Uniform(absl::IntervalClosed, rng, 10, 90));
  }

  std::vector<int> chosen;
  chosen.reserve(10);

  // 1.5x bool_or clauses (width 3-7, ~25% with 1 enforcement_literal).
  const int num_bool_or = (target_num_vars * 3) / 2;
  for (int c = 0; c < num_bool_or; ++c) {
    const int width =
        std::min(num_bools, absl::Uniform(absl::IntervalClosed, rng, 3, 7));
    const bool has_enf = num_bools > width && absl::Bernoulli(rng, 0.25);
    chosen.clear();
    SampleDistinctFrom(model.bool_vars, width + (has_enf ? 1 : 0), rng, chosen);
    ConstraintProto* ct = model.proto.add_constraints();
    if (has_enf) {
      const int enf_var = chosen.back();
      ct->add_enforcement_literal(
          absl::Bernoulli(rng, 0.5) ? enf_var : NegatedRef(enf_var));
    }
    const int true_pos = absl::Uniform(rng, 0, width);
    auto* lits = ct->mutable_bool_or()->mutable_literals();
    lits->Reserve(width);
    for (int i = 0; i < width; ++i) {
      const int v = chosen[i];
      if (i == true_pos) {
        lits->Add(TrueLitFor(model, v));
      } else {
        lits->Add(absl::Bernoulli(rng, 0.5) ? v : NegatedRef(v));
      }
    }
  }

  // 0.3x reified bool_and constraints (ALWAYS 1 enforcement_literal).
  const int num_bool_and = (target_num_vars * 3) / 10;
  for (int c = 0; c < num_bool_and; ++c) {
    const int width =
        std::min(num_bools - 1, absl::Uniform(absl::IntervalClosed, rng, 2, 4));
    chosen.clear();
    SampleDistinctFrom(model.bool_vars, width + 1, rng, chosen);
    const int enf_var = chosen.back();
    const bool enf_is_true = absl::Bernoulli(rng, 0.5);
    ConstraintProto* ct = model.proto.add_constraints();
    ct->add_enforcement_literal(enf_is_true ? TrueLitFor(model, enf_var)
                                            : FalseLitFor(model, enf_var));
    auto* lits = ct->mutable_bool_and()->mutable_literals();
    lits->Reserve(width);
    for (int i = 0; i < width; ++i) {
      const int v = chosen[i];
      if (enf_is_true) {
        lits->Add(TrueLitFor(model, v));
      } else {
        lits->Add(absl::Bernoulli(rng, 0.5) ? v : NegatedRef(v));
      }
    }
  }

  // 0.25x at_most_one constraints (width 5-8).
  const int num_amo = target_num_vars / 4;
  for (int c = 0; c < num_amo; ++c) {
    const int width =
        std::min(num_bools, absl::Uniform(absl::IntervalClosed, rng, 5, 8));
    chosen.clear();
    SampleDistinctFrom(model.bool_vars, width, rng, chosen);
    const int true_pos =
        absl::Bernoulli(rng, 0.7) ? absl::Uniform(rng, 0, width) : -1;
    auto* lits = model.proto.add_constraints()
                     ->mutable_at_most_one()
                     ->mutable_literals();
    lits->Reserve(width);
    for (int i = 0; i < width; ++i) {
      lits->Add(i == true_pos ? TrueLitFor(model, chosen[i])
                              : FalseLitFor(model, chosen[i]));
    }
  }

  // 0.25x exactly_one constraints (width 4-6).
  const int num_exo = target_num_vars / 4;
  for (int c = 0; c < num_exo; ++c) {
    const int width =
        std::min(num_bools, absl::Uniform(absl::IntervalClosed, rng, 4, 6));
    chosen.clear();
    SampleDistinctFrom(model.bool_vars, width, rng, chosen);
    const int true_pos = absl::Uniform(rng, 0, width);
    auto* lits = model.proto.add_constraints()
                     ->mutable_exactly_one()
                     ->mutable_literals();
    lits->Reserve(width);
    for (int i = 0; i < width; ++i) {
      lits->Add(i == true_pos ? TrueLitFor(model, chosen[i])
                              : FalseLitFor(model, chosen[i]));
    }
  }

  // 0.15x bool_xor constraints (width 3-5, odd true count).
  const int num_xor = (target_num_vars * 15) / 100;
  for (int c = 0; c < num_xor; ++c) {
    const int width =
        std::min(num_bools, absl::Uniform(absl::IntervalClosed, rng, 3, 5));
    chosen.clear();
    SampleDistinctFrom(model.bool_vars, width, rng, chosen);
    auto* lits =
        model.proto.add_constraints()->mutable_bool_xor()->mutable_literals();
    lits->Reserve(width);
    int true_count = 0;
    for (int i = 1; i < width; ++i) {
      const int lit =
          absl::Bernoulli(rng, 0.5) ? chosen[i] : NegatedRef(chosen[i]);
      if (lit == TrueLitFor(model, chosen[i])) {
        ++true_count;
      }
      lits->Add(lit);
    }
    const bool need_true = (true_count % 2 == 0);
    lits->Add(need_true ? TrueLitFor(model, chosen[0])
                        : FalseLitFor(model, chosen[0]));
  }

  // 0.2x linking linear constraints mixing ints and bools.
  const int num_linear = target_num_vars / 5;
  for (int c = 0; c < num_linear; ++c) {
    const int ni = std::min(num_ints, 2);
    const int nb = std::min(num_bools, 4);
    chosen.clear();
    SampleDistinctFrom(model.int_vars, ni, rng, chosen);
    SampleDistinctFrom(model.bool_vars, nb, rng, chosen);
    std::sort(chosen.begin(), chosen.end());
    LinearConstraintProto* lin =
        model.proto.add_constraints()->mutable_linear();
    lin->mutable_vars()->Reserve(chosen.size());
    lin->mutable_coeffs()->Reserve(chosen.size());
    int64_t activity = 0;
    for (const int v : chosen) {
      const int mag = absl::Uniform(absl::IntervalClosed, rng, 1, 10);
      const int64_t coeff = absl::Bernoulli(rng, 0.5) ? mag : -mag;
      lin->add_vars(v);
      lin->add_coeffs(coeff);
      activity += coeff * model.planted_sol[v];
    }
    const int64_t slack = absl::Uniform(absl::IntervalClosed, rng, 0, 30);
    lin->add_domain(activity - slack);
    lin->add_domain(activity + slack);
  }

  PopulateObjective(model, 0.5, rng);
  return model;
}

PlantedModel BuildLinearHeavyModel(int target_num_vars, std::mt19937& rng) {
  PlantedModel model;
  const int num_bools = std::max(4, target_num_vars / 4);
  const int num_ints = std::max(10, target_num_vars - num_bools);
  const int total_vars = num_bools + num_ints;
  model.proto.mutable_variables()->Reserve(total_vars);
  model.planted_sol.reserve(total_vars);

  for (int i = 0; i < num_bools; ++i) {
    AddBoolVar(model, absl::Uniform(absl::IntervalClosed, rng, 0, 1));
  }
  for (int i = 0; i < num_ints; ++i) {
    AddIntVar(model, 0, 10000,
              absl::Uniform(absl::IntervalClosed, rng, 500, 9500));
  }

  std::vector<int> all_vars(total_vars);
  std::iota(all_vars.begin(), all_vars.end(), 0);
  std::vector<int> chosen;
  chosen.reserve(16);

  // 1.5x MIP-style linear constraints of width 8-14.
  const int num_mip_lin = (target_num_vars * 3) / 2;
  for (int c = 0; c < num_mip_lin; ++c) {
    const int width =
        std::min(total_vars, absl::Uniform(absl::IntervalClosed, rng, 8, 14));
    chosen.clear();
    SampleDistinctFrom(all_vars, width, rng, chosen);
    std::sort(chosen.begin(), chosen.end());
    LinearConstraintProto* lin =
        model.proto.add_constraints()->mutable_linear();
    lin->mutable_vars()->Reserve(width);
    lin->mutable_coeffs()->Reserve(width);
    int64_t activity = 0;
    for (const int v : chosen) {
      const int mag = absl::Uniform(absl::IntervalClosed, rng, 1, 100);
      const int64_t coeff = absl::Bernoulli(rng, 0.5) ? mag : -mag;
      lin->add_vars(v);
      lin->add_coeffs(coeff);
      activity += coeff * model.planted_sol[v];
    }
    const int64_t slack = absl::Uniform(absl::IntervalClosed, rng, 0, 1000);
    const int kind = c % 5;
    if (kind == 0) {
      lin->add_domain(activity);
      lin->add_domain(activity);
    } else if (kind == 1 || kind == 2) {
      lin->add_domain(std::numeric_limits<int64_t>::min());
      lin->add_domain(activity + slack);
    } else if (kind == 3) {
      lin->add_domain(activity - slack);
      lin->add_domain(std::numeric_limits<int64_t>::max());
    } else {
      lin->add_domain(activity - slack);
      lin->add_domain(activity + slack);
    }
  }

  // 0.5x indicator linear constraints (1 enforcement + 2-3 var linear).
  const int num_ind = target_num_vars / 2;
  for (int c = 0; c < num_ind; ++c) {
    const int enf_var = model.bool_vars[absl::Uniform(rng, 0, num_bools)];
    const int width =
        std::min(num_ints, absl::Uniform(absl::IntervalClosed, rng, 2, 3));
    chosen.clear();
    SampleDistinctFrom(model.int_vars, width, rng, chosen);
    std::sort(chosen.begin(), chosen.end());
    ConstraintProto* ct = model.proto.add_constraints();
    ct->add_enforcement_literal(
        absl::Bernoulli(rng, 0.5) ? enf_var : NegatedRef(enf_var));
    LinearConstraintProto* lin = ct->mutable_linear();
    lin->mutable_vars()->Reserve(width);
    lin->mutable_coeffs()->Reserve(width);
    int64_t activity = 0;
    for (const int v : chosen) {
      const int mag = absl::Uniform(absl::IntervalClosed, rng, 1, 50);
      const int64_t coeff = absl::Bernoulli(rng, 0.5) ? mag : -mag;
      lin->add_vars(v);
      lin->add_coeffs(coeff);
      activity += coeff * model.planted_sol[v];
    }
    const int64_t slack = absl::Uniform(absl::IntervalClosed, rng, 0, 500);
    lin->add_domain(std::numeric_limits<int64_t>::min());
    lin->add_domain(activity + slack);
  }

  PopulateObjective(model, 0.8, rng);
  return model;
}

PlantedModel BuildSchedulingHeavyModel(int target_num_vars, std::mt19937& rng) {
  PlantedModel model;
  const int num_tasks = std::max(4, target_num_vars / 4);
  const int num_machines = std::max(1, (num_tasks + 11) / 12);
  const int num_cumul = std::max(1, (num_machines + 3) / 4);

  struct TaskInfo {
    int p_var = -1;
    int start_var = -1;
    int size_var = -1;
    int end_var = -1;
    int machine = 0;
    int interval_idx = -1;
  };
  std::vector<TaskInfo> tasks(num_tasks);
  std::vector<std::vector<int>> machine_tasks(num_machines);
  std::vector<int64_t> machine_cur_t(num_machines, 0);
  int64_t max_end_sol = 0;

  for (int t = 0; t < num_tasks; ++t) {
    const int m = std::min(num_machines - 1, t / 12);
    tasks[t].machine = m;
    machine_tasks[m].push_back(t);

    const int64_t dur = absl::Uniform(absl::IntervalClosed, rng, 2, 30);
    const int64_t gap = absl::Uniform(absl::IntervalClosed, rng, 0, 15);
    const int64_t s_val = machine_cur_t[m] + gap;
    const int64_t e_val = s_val + dur;
    machine_cur_t[m] = e_val;
    max_end_sol = std::max(max_end_sol, e_val);

    if (absl::Bernoulli(rng, 0.6)) {
      tasks[t].p_var = AddBoolVar(model, absl::Bernoulli(rng, 0.75) ? 1 : 0);
    }
    tasks[t].start_var = AddIntVar(model, 0, 10000, s_val);
    tasks[t].size_var = AddIntVar(model, 2, 30, dur);
    tasks[t].end_var = AddIntVar(model, 0, 10030, e_val);
  }

  // Capacity vars for cumulative constraints + makespan var.
  std::vector<int> cumul_cap_vars(num_cumul);
  for (int k = 0; k < num_cumul; ++k) {
    cumul_cap_vars[k] = AddIntVar(
        model, 20, 40, absl::Uniform(absl::IntervalClosed, rng, 20, 40));
  }
  const int makespan_var = AddIntVar(model, 0, 10030, max_end_sol);

  // Allocate variable cumulative demands up to a portion of remaining budget,
  // and pad the remainder with auxiliary Boolean variables so total == target.
  const int remaining_vars =
      std::max(0, target_num_vars - model.proto.variables_size());
  const int num_demand_vars = std::min(num_tasks, remaining_vars / 3);
  std::vector<int> demand_vars(num_tasks, -1);
  for (int t = 0; t < num_demand_vars; ++t) {
    demand_vars[t] =
        AddIntVar(model, 1, 5, absl::Uniform(absl::IntervalClosed, rng, 1, 5));
  }
  while (model.proto.variables_size() < target_num_vars) {
    AddBoolVar(model, absl::Bernoulli(rng, 0.5) ? 1 : 0);
  }

  // CRITICAL: ALL kInterval constraints MUST appear before any kNoOverlap or
  // kCumulative constraint when copying with first_copy=false.
  for (int t = 0; t < num_tasks; ++t) {
    tasks[t].interval_idx = model.proto.constraints_size();
    ConstraintProto* ct = model.proto.add_constraints();
    if (tasks[t].p_var != -1) {
      ct->add_enforcement_literal(tasks[t].p_var);
    }
    IntervalConstraintProto* itv = ct->mutable_interval();
    SetAffineExpr(itv->mutable_start(), tasks[t].start_var);
    SetAffineExpr(itv->mutable_size(), tasks[t].size_var);
    SetAffineExpr(itv->mutable_end(), tasks[t].end_var);
  }

  // Linking start_t + size_t - end_t == 0.
  for (int t = 0; t < num_tasks; ++t) {
    ConstraintProto* ct = model.proto.add_constraints();
    if (tasks[t].p_var != -1) {
      ct->add_enforcement_literal(tasks[t].p_var);
    }
    LinearConstraintProto* lin = ct->mutable_linear();
    lin->add_vars(tasks[t].start_var);
    lin->add_coeffs(1);
    lin->add_vars(tasks[t].size_var);
    lin->add_coeffs(1);
    lin->add_vars(tasks[t].end_var);
    lin->add_coeffs(-1);
    lin->add_domain(0);
    lin->add_domain(0);
  }

  // Machine no_overlap constraints.
  for (int m = 0; m < num_machines; ++m) {
    NoOverlapConstraintProto* no_overlap =
        model.proto.add_constraints()->mutable_no_overlap();
    for (const int t : machine_tasks[m]) {
      no_overlap->add_intervals(tasks[t].interval_idx);
    }
  }

  // Cumulative constraints over groups of 4 machines (<= 4 active intervals at
  // any point, each demand <= 5, total peak load <= 20 <= capacity).
  for (int k = 0; k < num_cumul; ++k) {
    CumulativeConstraintProto* cumul =
        model.proto.add_constraints()->mutable_cumulative();
    SetAffineExpr(cumul->mutable_capacity(), cumul_cap_vars[k]);
    const int m_begin = k * 4;
    const int m_end = std::min(num_machines, m_begin + 4);
    for (int m = m_begin; m < m_end; ++m) {
      for (const int t : machine_tasks[m]) {
        cumul->add_intervals(tasks[t].interval_idx);
        if (demand_vars[t] != -1) {
          SetAffineExpr(cumul->add_demands(), demand_vars[t]);
        } else {
          SetConstExpr(cumul->add_demands(),
                       absl::Uniform(absl::IntervalClosed, rng, 1, 5));
        }
      }
    }
  }

  // Precedence linear constraints along machine chains: end_t - start_{t+1} <=
  // 0.
  for (int m = 0; m < num_machines; ++m) {
    for (int i = 0; i + 1 < static_cast<int>(machine_tasks[m].size()); ++i) {
      const int t1 = machine_tasks[m][i];
      const int t2 = machine_tasks[m][i + 1];
      ConstraintProto* ct = model.proto.add_constraints();
      if (tasks[t1].p_var != -1 && absl::Bernoulli(rng, 0.5)) {
        ct->add_enforcement_literal(tasks[t1].p_var);
      }
      LinearConstraintProto* lin = ct->mutable_linear();
      lin->add_vars(tasks[t1].end_var);
      lin->add_coeffs(1);
      lin->add_vars(tasks[t2].start_var);
      lin->add_coeffs(-1);
      lin->add_domain(std::numeric_limits<int64_t>::min());
      lin->add_domain(0);
    }
  }

  // Makespan kLinMax constraint over all task end_vars.
  {
    ConstraintProto* ct = model.proto.add_constraints();
    LinearArgumentProto* lin_max = ct->mutable_lin_max();
    SetAffineExpr(lin_max->mutable_target(), makespan_var);
    lin_max->mutable_exprs()->Reserve(num_tasks);
    for (int t = 0; t < num_tasks; ++t) {
      SetAffineExpr(lin_max->add_exprs(), tasks[t].end_var);
    }
    CanonicalizeLinMax(ct);
  }

  // Tie auxiliary Booleans into simple satisfied bool_or clauses.
  if (static_cast<int>(model.bool_vars.size()) >= 4) {
    const int num_clauses = static_cast<int>(model.bool_vars.size()) / 2;
    std::vector<int> chosen;
    chosen.reserve(4);
    for (int c = 0; c < num_clauses; ++c) {
      chosen.clear();
      SampleDistinctFrom(model.bool_vars, 3, rng, chosen);
      auto* lits =
          model.proto.add_constraints()->mutable_bool_or()->mutable_literals();
      lits->Add(TrueLitFor(model, chosen[0]));
      lits->Add(absl::Bernoulli(rng, 0.5) ? chosen[1] : NegatedRef(chosen[1]));
      lits->Add(absl::Bernoulli(rng, 0.5) ? chosen[2] : NegatedRef(chosen[2]));
    }
  }

  PopulateObjective(model, 0.5, rng);
  return model;
}

void AddRoutingBlock(PlantedModel& model, int num_nodes, int total_arc_vars,
                     bool add_circuit, std::mt19937& rng) {
  CHECK_GE(num_nodes, 4);
  const int num_routes = 3;
  std::vector<std::vector<int>> routes(num_routes);
  for (int customer = 1; customer < num_nodes; ++customer) {
    routes[(customer - 1) * num_routes / (num_nodes - 1)].push_back(customer);
  }

  std::vector<std::vector<int>> arc_var(num_nodes,
                                        std::vector<int>(num_nodes, -1));
  std::vector<std::vector<int>> travel_time(num_nodes,
                                            std::vector<int>(num_nodes, 0));
  for (int u = 0; u < num_nodes; ++u) {
    for (int v = 0; v < num_nodes; ++v) {
      if (u != v) {
        travel_time[u][v] = absl::Uniform(absl::IntervalClosed, rng, 50, 200);
      }
    }
  }
  std::vector<int> demand(num_nodes, 0);
  std::vector<int> time_sol(num_nodes, 0);
  std::vector<int> load_sol(num_nodes, 0);

  // Planted route arcs (all 1) and arrival time/load along each route.
  for (int r = 0; r < num_routes; ++r) {
    CHECK(!routes[r].empty());
    int prev = 0;
    int cur_t = 0;
    int cur_load = 0;
    for (const int cust : routes[r]) {
      arc_var[prev][cust] = AddBoolVar(model, 1);
      demand[cust] = absl::Uniform(absl::IntervalClosed, rng, 10, 30);
      cur_t += travel_time[prev][cust];
      cur_load += demand[cust];
      time_sol[cust] = cur_t;
      load_sol[cust] = cur_load;
      prev = cust;
    }
    arc_var[prev][0] = AddBoolVar(model, 1);
  }

  // Reserve 2 bridge arc vars for Hamiltonian circuit on half the blocks so
  // bridge edges never conflict with kRoutes.
  int bridge_01 = -1;
  int bridge_12 = -1;
  if (add_circuit) {
    bridge_01 = AddBoolVar(model, 1);
    bridge_12 = AddBoolVar(model, 1);
  }
  const int b0_tail = routes[0].back();
  const int b1_head = routes[1].front();
  const int b1_tail = routes[1].back();
  const int b2_head = routes[2].front();

  const int planted_route_arcs = num_routes + (num_nodes - 1);
  const int target_routes_arcs = total_arc_vars - (add_circuit ? 2 : 0);
  int added_route_arcs = planted_route_arcs;

  auto try_add_false_arc = [&](int u, int v) -> bool {
    if (u == v) return false;
    if (arc_var[u][v] != -1) return false;
    if (add_circuit &&
        ((u == b0_tail && v == b1_head) || (u == b1_tail && v == b2_head))) {
      return false;
    }
    arc_var[u][v] = AddBoolVar(model, 0);
    ++added_route_arcs;
    return true;
  };

  // Ensure every customer has >= 1 false incoming and >= 1 false outgoing arc
  // so kExactlyOne constraints remain non-trivial.
  for (int v = 1; v < num_nodes && added_route_arcs < target_routes_arcs; ++v) {
    bool has_false_out = false;
    bool has_false_in = false;
    for (int u = 0; u < num_nodes; ++u) {
      if (arc_var[v][u] != -1 && model.planted_sol[arc_var[v][u]] == 0) {
        has_false_out = true;
      }
      if (arc_var[u][v] != -1 && model.planted_sol[arc_var[u][v]] == 0) {
        has_false_in = true;
      }
    }
    for (int tries = 0; tries < 20 && !has_false_out; ++tries) {
      if (try_add_false_arc(v, absl::Uniform(rng, 0, num_nodes))) {
        has_false_out = true;
      }
    }
    for (int tries = 0;
         tries < 20 && !has_false_in && added_route_arcs < target_routes_arcs;
         ++tries) {
      if (try_add_false_arc(absl::Uniform(rng, 0, num_nodes), v)) {
        has_false_in = true;
      }
    }
  }

  while (added_route_arcs < target_routes_arcs) {
    try_add_false_arc(absl::Uniform(rng, 0, num_nodes),
                      absl::Uniform(rng, 0, num_nodes));
  }

  // Time [0, 5000] and load [0, 500] integer variables per node.
  std::vector<int> time_vars(num_nodes);
  std::vector<int> load_vars(num_nodes);
  for (int v = 0; v < num_nodes; ++v) {
    time_vars[v] = AddIntVar(model, 0, 5000, time_sol[v]);
    load_vars[v] = AddIntVar(model, 0, 500, load_sol[v]);
  }

  // Emit kRoutes with 2 dimensions (time and load).
  {
    RoutesConstraintProto* routes_ct =
        model.proto.add_constraints()->mutable_routes();
    for (int u = 0; u < num_nodes; ++u) {
      for (int v = 0; v < num_nodes; ++v) {
        if (arc_var[u][v] != -1) {
          routes_ct->add_tails(u);
          routes_ct->add_heads(v);
          routes_ct->add_literals(arc_var[u][v]);
        }
      }
    }
    auto* time_dim = routes_ct->add_dimensions();
    auto* load_dim = routes_ct->add_dimensions();
    for (int v = 0; v < num_nodes; ++v) {
      SetAffineExpr(time_dim->add_exprs(), time_vars[v]);
      SetAffineExpr(load_dim->add_exprs(), load_vars[v]);
    }
  }

  // Emit Hamiltonian kCircuit on half the blocks reusing non-bridge arcs.
  if (add_circuit) {
    CircuitConstraintProto* circuit =
        model.proto.add_constraints()->mutable_circuit();
    const int b0_head = routes[0].front();
    const int b2_tail = routes[2].back();
    for (int u = 0; u < num_nodes; ++u) {
      for (int v = 0; v < num_nodes; ++v) {
        const int a = arc_var[u][v];
        if (a == -1) continue;
        // Skip depot/tail arcs from routes that would create branching if left
        // as true in kCircuit; replace with bridge arcs.
        if (u == 0 && (v == b1_head || v == b2_head)) continue;
        if (v == 0 && (u == b0_tail || u == b1_tail)) continue;
        circuit->add_tails(u);
        circuit->add_heads(v);
        circuit->add_literals(a);
      }
    }
    circuit->add_tails(b0_tail);
    circuit->add_heads(b1_head);
    circuit->add_literals(bridge_01);
    circuit->add_tails(b1_tail);
    circuit->add_heads(b2_head);
    circuit->add_literals(bridge_12);
    // Keep compiler/reader confidence that depot enters r0 and leaves r2:
    CHECK_NE(arc_var[0][b0_head], -1);
    CHECK_NE(arc_var[b2_tail][0], -1);
  }

  // Customer in-degree and out-degree kExactlyOne constraints.
  for (int v = 1; v < num_nodes; ++v) {
    ConstraintProto* out_ct = model.proto.add_constraints();
    auto* out_lits = out_ct->mutable_exactly_one()->mutable_literals();
    for (int u = 0; u < num_nodes; ++u) {
      if (arc_var[v][u] != -1) out_lits->Add(arc_var[v][u]);
    }
    ConstraintProto* in_ct = model.proto.add_constraints();
    auto* in_lits = in_ct->mutable_exactly_one()->mutable_literals();
    for (int u = 0; u < num_nodes; ++u) {
      if (arc_var[u][v] != -1) in_lits->Add(arc_var[u][v]);
    }
  }

  // Reified time and load propagation along arcs (u -> v, v != 0):
  // arc_lit => time[v] - time[u] >= travel_time[u][v]
  // arc_lit => load[v] - load[u] >= demand[v]
  for (int u = 0; u < num_nodes; ++u) {
    for (int v = 1; v < num_nodes; ++v) {
      const int a = arc_var[u][v];
      if (a == -1) continue;

      {
        ConstraintProto* ct = model.proto.add_constraints();
        ct->add_enforcement_literal(a);
        LinearConstraintProto* lin = ct->mutable_linear();
        if (time_vars[u] < time_vars[v]) {
          lin->add_vars(time_vars[u]);
          lin->add_coeffs(-1);
          lin->add_vars(time_vars[v]);
          lin->add_coeffs(1);
        } else {
          lin->add_vars(time_vars[v]);
          lin->add_coeffs(1);
          lin->add_vars(time_vars[u]);
          lin->add_coeffs(-1);
        }
        lin->add_domain(travel_time[u][v]);
        lin->add_domain(std::numeric_limits<int64_t>::max());
      }

      {
        ConstraintProto* ct = model.proto.add_constraints();
        ct->add_enforcement_literal(a);
        LinearConstraintProto* lin = ct->mutable_linear();
        if (load_vars[u] < load_vars[v]) {
          lin->add_vars(load_vars[u]);
          lin->add_coeffs(-1);
          lin->add_vars(load_vars[v]);
          lin->add_coeffs(1);
        } else {
          lin->add_vars(load_vars[v]);
          lin->add_coeffs(1);
          lin->add_vars(load_vars[u]);
          lin->add_coeffs(-1);
        }
        lin->add_domain(demand[v]);
        lin->add_domain(std::numeric_limits<int64_t>::max());
      }
    }
  }
}

PlantedModel BuildRoutingHeavyModel(int target_num_vars, std::mt19937& rng) {
  PlantedModel model;
  constexpr int kNodesPerBlock = 16;
  int block_idx = 0;
  while (target_num_vars - model.proto.variables_size() > 265) {
    AddRoutingBlock(model, kNodesPerBlock, 140, (block_idx % 2 == 0), rng);
    ++block_idx;
  }
  const int rem = target_num_vars - model.proto.variables_size();
  if (rem > 0) {
    const int n = std::clamp((rem - 18) / 8, 4, kNodesPerBlock);
    const int arc_vars = std::max(3 + (n - 1) + 2, rem - 2 * n);
    AddRoutingBlock(model, n, arc_vars, (block_idx % 2 == 0), rng);
  }
  PopulateObjective(model, 0.5, rng);
  return model;
}

void AddAllDiffGadget(PlantedModel& model, int width, std::mt19937& rng) {
  std::vector<int> vals(81);
  std::iota(vals.begin(), vals.end(), 10);
  std::vector<int> chosen_vals;
  SampleDistinctFromPool(vals, width, rng, chosen_vals);
  auto* exprs =
      model.proto.add_constraints()->mutable_all_diff()->mutable_exprs();
  for (int i = 0; i < width; ++i) {
    const int v = AddIntVar(model, 0, 100, chosen_vals[i]);
    SetAffineExpr(exprs->Add(), v);
  }
}

void AddElementGadget(PlantedModel& model, std::mt19937& rng) {
  const int idx_val = absl::Uniform(absl::IntervalClosed, rng, 1, 8);
  const int idx_var = AddIntVar(model, 0, 9, idx_val);
  std::vector<int> elem_vars(10);
  for (int i = 0; i < 10; ++i) {
    elem_vars[i] = AddIntVar(model, 0, 200,
                             absl::Uniform(absl::IntervalClosed, rng, 0, 200));
  }
  const int target_var =
      AddIntVar(model, 0, 200, model.planted_sol[elem_vars[idx_val]]);
  ElementConstraintProto* elem =
      model.proto.add_constraints()->mutable_element();
  SetAffineExpr(elem->mutable_linear_index(), idx_var);
  SetAffineExpr(elem->mutable_linear_target(), target_var);
  for (const int ev : elem_vars) {
    SetAffineExpr(elem->add_exprs(), ev);
  }
}

void AddTableGadget(PlantedModel& model, std::mt19937& rng) {
  std::vector<int> vars(4);
  TableConstraintProto* table = model.proto.add_constraints()->mutable_table();
  for (int i = 0; i < 4; ++i) {
    const int64_t val = absl::Uniform(absl::IntervalClosed, rng, 0, 50);
    vars[i] = AddIntVar(model, 0, 50, val);
    SetAffineExpr(table->add_exprs(), vars[i]);
    table->add_values(val);
  }
  for (int t = 1; t < 10; ++t) {
    for (int i = 0; i < 4; ++i) {
      table->add_values(absl::Uniform(absl::IntervalClosed, rng, 0, 50));
    }
  }
}

void AddLinMaxGadget(PlantedModel& model, std::mt19937& rng) {
  int64_t max_val = 0;
  std::vector<int> arg_vars(4);
  for (int i = 0; i < 4; ++i) {
    const int64_t val = absl::Uniform(absl::IntervalClosed, rng, 0, 200);
    arg_vars[i] = AddIntVar(model, 0, 200, val);
    max_val = std::max(max_val, val);
  }
  const int target_var = AddIntVar(model, 0, 200, max_val);
  ConstraintProto* ct = model.proto.add_constraints();
  LinearArgumentProto* lin_max = ct->mutable_lin_max();
  SetAffineExpr(lin_max->mutable_target(), target_var);
  for (const int av : arg_vars) {
    SetAffineExpr(lin_max->add_exprs(), av);
  }
  CanonicalizeLinMax(ct);
}

void AddIntProdGadget(PlantedModel& model, std::mt19937& rng) {
  const int64_t vx = absl::Uniform(absl::IntervalClosed, rng, 0, 15);
  const int64_t vy = absl::Uniform(absl::IntervalClosed, rng, 0, 15);
  const int x = AddIntVar(model, 0, 15, vx);
  const int y = AddIntVar(model, 0, 15, vy);
  const int target = AddIntVar(model, 0, 225, vx * vy);
  LinearArgumentProto* prod = model.proto.add_constraints()->mutable_int_prod();
  SetAffineExpr(prod->mutable_target(), target);
  SetAffineExpr(prod->add_exprs(), x);
  SetAffineExpr(prod->add_exprs(), y);
}

void AddIntDivGadget(PlantedModel& model, std::mt19937& rng) {
  const int64_t vx = absl::Uniform(absl::IntervalClosed, rng, 0, 200);
  const int64_t vy = absl::Uniform(absl::IntervalClosed, rng, 2, 19);
  const int x = AddIntVar(model, 0, 200, vx);
  const int y = AddIntVar(model, 1, 20, vy);
  const int target = AddIntVar(model, 0, 200, vx / vy);
  LinearArgumentProto* div = model.proto.add_constraints()->mutable_int_div();
  SetAffineExpr(div->mutable_target(), target);
  SetAffineExpr(div->add_exprs(), x);
  SetAffineExpr(div->add_exprs(), y);
}

void AddIntModGadget(PlantedModel& model, std::mt19937& rng) {
  const int64_t vx = absl::Uniform(absl::IntervalClosed, rng, 0, 200);
  const int64_t vy = absl::Uniform(absl::IntervalClosed, rng, 2, 19);
  const int x = AddIntVar(model, 0, 200, vx);
  const int y = AddIntVar(model, 1, 20, vy);
  const int target = AddIntVar(model, 0, 20, vx % vy);
  LinearArgumentProto* mod = model.proto.add_constraints()->mutable_int_mod();
  SetAffineExpr(mod->mutable_target(), target);
  SetAffineExpr(mod->add_exprs(), x);
  SetAffineExpr(mod->add_exprs(), y);
}

void AddBoolXorCycle(PlantedModel& model, int num_bool_vars,
                     std::mt19937& rng) {
  CHECK_GE(num_bool_vars, 4);
  std::vector<int> b;
  b.reserve(num_bool_vars);
  for (int i = 0; i < num_bool_vars; ++i) {
    b.push_back(AddBoolVar(model, absl::Bernoulli(rng, 0.5) ? 1 : 0));
  }
  const int num_xors = std::max(1, (num_bool_vars + 2) / 3);
  for (int k = 0; k < num_xors; ++k) {
    auto* lits =
        model.proto.add_constraints()->mutable_bool_xor()->mutable_literals();
    int true_count = 0;
    const int base = (k * 3) % std::max(1, num_bool_vars - 3);
    for (int i = 1; i < 4; ++i) {
      const int v = b[(base + i) % num_bool_vars];
      const int lit = absl::Bernoulli(rng, 0.5) ? v : NegatedRef(v);
      if (lit == TrueLitFor(model, v)) ++true_count;
      lits->Add(lit);
    }
    const int v0 = b[base % num_bool_vars];
    lits->Add(true_count % 2 == 0 ? TrueLitFor(model, v0)
                                  : FalseLitFor(model, v0));
  }
}

PlantedModel BuildGeneralCpHeavyModel(int target_num_vars, std::mt19937& rng) {
  PlantedModel model;
  // Full cycle = 10 bools (3 bool_xor) + 38 ints (8+12+4+5+3+3+3) = 48 vars.
  while (target_num_vars - model.proto.variables_size() >= 48) {
    AddBoolXorCycle(model, 10, rng);
    AddAllDiffGadget(model, 8, rng);
    AddElementGadget(model, rng);
    AddTableGadget(model, rng);
    AddLinMaxGadget(model, rng);
    AddIntProdGadget(model, rng);
    AddIntDivGadget(model, rng);
    AddIntModGadget(model, rng);
  }

  int rem = target_num_vars - model.proto.variables_size();
  const int rem_bools = std::min(rem, std::max(0, rem / 5));
  if (rem_bools >= 4) {
    AddBoolXorCycle(model, rem_bools, rng);
  } else {
    for (int i = 0; i < rem_bools; ++i) {
      AddBoolVar(model, absl::Bernoulli(rng, 0.5) ? 1 : 0);
    }
  }
  rem = target_num_vars - model.proto.variables_size();
  while (rem >= 12) {
    AddElementGadget(model, rng);
    rem -= 12;
  }
  if (rem >= 8) {
    AddAllDiffGadget(model, 8, rng);
    rem -= 8;
  }
  if (rem >= 5) {
    AddLinMaxGadget(model, rng);
    rem -= 5;
  }
  if (rem >= 4) {
    AddTableGadget(model, rng);
    rem -= 4;
  }
  if (rem >= 3) {
    AddIntModGadget(model, rng);
    rem -= 3;
  }
  while (rem > 0) {
    AddIntVar(model, 0, 100, absl::Uniform(absl::IntervalClosed, rng, 10, 90));
    --rem;
  }

  PopulateObjective(model, 0.5, rng);
  return model;
}

PlantedModel BuildRawReferenceModel(ModelKind kind, int target_num_vars,
                                    std::mt19937& rng) {
  switch (kind) {
    case kBooleanHeavy:
      return BuildBooleanHeavyModel(target_num_vars, rng);
    case kLinearHeavy:
      return BuildLinearHeavyModel(target_num_vars, rng);
    case kSchedulingHeavy:
      return BuildSchedulingHeavyModel(target_num_vars, rng);
    case kRoutingHeavy:
      return BuildRoutingHeavyModel(target_num_vars, rng);
    case kGeneralCpHeavy:
      return BuildGeneralCpHeavyModel(target_num_vars, rng);
  }
  LOG(FATAL) << "Unknown ModelKind: " << static_cast<int>(kind);
}

PlantedModel BuildReferenceModel(ModelKind kind, int target_num_vars,
                                 std::mt19937& rng) {
  PlantedModel model = BuildRawReferenceModel(kind, target_num_vars, rng);
  CpModelProto copied_proto;
  Model sat_model;
  CHECK(CopyModel(model.proto, /*clear_internal_fields=*/false, &copied_proto,
                  &sat_model));
  CHECK_EQ(copied_proto.variables_size(), model.proto.variables_size());
  model.proto.Swap(&copied_proto);
  return model;
}

std::string FormatCopyDtime(const ModelStats& s) {
  return absl::StrFormat("dtime=%.3fms",
                         1e3 * EstimateCopyDeterministicTime(s));
}

void BM_EmptyModelCopy(benchmark::State& state) {
  const CpModelProto empty;
  for (auto _ : state) {
    google::protobuf::Arena arena;
    CpModelProto* copied =
        google::protobuf::Arena::Create<CpModelProto>(&arena);
    Model sat_model;
    CHECK(
        CopyModel(empty, /*clear_internal_fields=*/false, copied, &sat_model));
    benchmark::DoNotOptimize(copied);
  }
  state.SetLabel(absl::StrFormat(
      "dtime=%.3fus", 1e6 * EstimateCopyDeterministicTime(ModelStats{})));
}

void BM_EmptyModelRebuild(benchmark::State& state) {
  const CpModelProto empty;
  for (auto _ : state) {
    DenseModelCopy copy("ls", empty, /*shared_bounds=*/nullptr,
                        /*shared_clauses=*/nullptr, [](int) { return true; });
    bool updated = false;
    CHECK(copy.MaybeUpdate(updated));
    CHECK(updated);
    benchmark::DoNotOptimize(&copy.proto());
  }
  state.SetLabel(absl::StrFormat(
      "dtime=%.3fus", 1e6 * EstimateCopyDeterministicTime(ModelStats{})));
}

template <ModelKind kKind>
void BM_InitialCopy(benchmark::State& state) {
  const int num_vars = state.range(0);
  std::mt19937 rng(20261108 + static_cast<int>(kKind) * 10007 + num_vars * 31);
  const PlantedModel raw = BuildRawReferenceModel(kKind, num_vars, rng);

  ModelStats stats;
  bool has_stats = false;
  for (auto _ : state) {
    google::protobuf::Arena arena;
    CpModelProto* copied =
        google::protobuf::Arena::Create<CpModelProto>(&arena);
    Model sat_model;
    CHECK(CopyModel(raw.proto, /*clear_internal_fields=*/false, copied,
                    &sat_model));
    if (!has_stats) {
      state.PauseTiming();
      stats = ComputeModelStats(*copied);
      has_stats = true;
      state.ResumeTiming();
    }
    benchmark::DoNotOptimize(copied);
  }
  state.SetLabel(FormatCopyDtime(stats));
}

template <ModelKind kKind>
void BM_ModelReduction(benchmark::State& state) {
  const int num_vars = state.range(0);
  const int target_fixed_pct = state.range(1);
  std::mt19937 rng(20261112 + static_cast<int>(kKind) * 10007 + num_vars * 31 +
                   target_fixed_pct);
  const PlantedModel model = BuildReferenceModel(kKind, num_vars, rng);
  const int total_vars = model.proto.variables_size();

  const int prev_fixed_pct = target_fixed_pct <= 25   ? 0
                             : target_fixed_pct <= 55 ? 25
                                                      : 50;

  CpModelProto stage_proto = model.proto;
  ModelStats stage_stats;
  std::vector<int64_t> stage_sol = model.planted_sol;

  if (prev_fixed_pct > 0) {
    std::vector<int> perm(total_vars);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng);
    const int num_prev = (total_vars * prev_fixed_pct) / 100;
    std::vector<int> vars(perm.begin(), perm.begin() + num_prev);
    std::vector<int64_t> vals;
    vals.reserve(num_prev);
    for (const int v : vars) vals.push_back(model.planted_sol[v]);

    SharedBoundsManager prep_bounds(model.proto);
    DenseModelCopy prep_copy("prep", model.proto, &prep_bounds,
                             /*shared_clauses=*/nullptr);
    bool updated = false;
    CHECK(prep_copy.MaybeUpdate(updated));
    prep_bounds.ReportPotentialNewBounds("sat", vars, vals, vals);
    prep_bounds.Synchronize();
    CHECK(prep_copy.MaybeUpdate(updated));
    CHECK(updated);
    stage_proto = prep_copy.proto();
    stage_stats = ComputeModelStats(stage_proto);
    stage_sol = prep_copy.MapSolution(model.planted_sol);
  } else {
    stage_stats = ComputeModelStats(stage_proto);
  }

  const int stage_num_vars = stage_proto.variables_size();
  std::vector<int> unfixed_stage_vars;
  unfixed_stage_vars.reserve(stage_num_vars);
  for (int v = 0; v < stage_num_vars; ++v) {
    const auto& dom = stage_proto.variables(v).domain();
    if (dom[0] < dom[dom.size() - 1]) {
      unfixed_stage_vars.push_back(v);
    }
  }
  std::shuffle(unfixed_stage_vars.begin(), unfixed_stage_vars.end(), rng);
  const int step_pct =
      ((target_fixed_pct - prev_fixed_pct) * 100) / (100 - prev_fixed_pct);
  const int num_to_fix = std::max(
      1, static_cast<int>((unfixed_stage_vars.size() * step_pct) / 100));
  BoundsBatch batch;
  batch.vars.assign(unfixed_stage_vars.begin(),
                    unfixed_stage_vars.begin() + num_to_fix);
  batch.lbs.reserve(num_to_fix);
  batch.ubs.reserve(num_to_fix);
  for (const int v : batch.vars) {
    batch.lbs.push_back(stage_sol[v]);
    batch.ubs.push_back(stage_sol[v]);
  }

  for (auto _ : state) {
    state.PauseTiming();
    std::optional<SharedBoundsManager> shared_bounds(stage_proto);
    std::optional<DenseModelCopy> copy(std::in_place, "ls", stage_proto,
                                       &*shared_bounds,
                                       /*shared_clauses=*/nullptr);
    shared_bounds->ReportPotentialNewBounds("sat", batch.vars, batch.lbs,
                                            batch.ubs);
    shared_bounds->Synchronize();
    bool updated = false;
    state.ResumeTiming();
    CHECK(copy->MaybeUpdate(updated));
    CHECK(updated);
    benchmark::DoNotOptimize(copy->var_domains().size());
    state.PauseTiming();
    copy.reset();
    shared_bounds.reset();
    state.ResumeTiming();
  }
  state.SetLabel(FormatCopyDtime(stage_stats));
}

// Shrinks `[cur_lb, cur_ub]` around `sol` while maintaining
// `cur_lb <= new_lb <= sol <= new_ub <= cur_ub` and `new_lb < new_ub`.
std::pair<int64_t, int64_t> NarrowAroundSolution(int64_t cur_lb, int64_t cur_ub,
                                                 int64_t sol) {
  int64_t new_lb = cur_lb;
  int64_t new_ub = cur_ub;
  const int64_t left = sol - cur_lb;
  const int64_t right = cur_ub - sol;
  if (left > 0 && new_ub - new_lb >= 2) {
    const int64_t step = std::max<int64_t>(1, left / 4);
    new_lb = std::min({cur_lb + step, sol, new_ub - 1});
  }
  if (right > 0 && new_ub - new_lb >= 2) {
    const int64_t step = std::max<int64_t>(1, right / 4);
    new_ub = std::max({cur_ub - step, sol, new_lb + 1});
  }
  return {new_lb, new_ub};
}

template <ModelKind kKind>
void BM_DomainReductions(benchmark::State& state) {
  const int num_vars = state.range(0);
  const int num_rounds = state.range(1);
  std::mt19937 rng(20261109 + static_cast<int>(kKind) * 10007 + num_vars * 31 +
                   num_rounds);
  const PlantedModel model = BuildReferenceModel(kKind, num_vars, rng);
  const int total_vars = model.proto.variables_size();

  std::vector<int64_t> lb(total_vars);
  std::vector<int64_t> ub(total_vars);
  for (int v = 0; v < total_vars; ++v) {
    const auto& dom = model.proto.variables(v).domain();
    lb[v] = dom[0];
    ub[v] = dom[dom.size() - 1];
  }

  std::vector<int> int_pool = model.int_vars;
  std::vector<BoundsBatch> rounds(num_rounds);
  std::vector<int> picked;
  const int num_int_vars = static_cast<int>(int_pool.size());
  const int batch_size =
      std::min(num_int_vars, std::max(1, (num_int_vars * 15) / 100));

  for (int r = 0; r < num_rounds; ++r) {
    SampleDistinctFromPool(int_pool, batch_size, rng, picked);
    BoundsBatch& batch = rounds[r];
    batch.vars.reserve(picked.size());
    batch.lbs.reserve(picked.size());
    batch.ubs.reserve(picked.size());
    for (const int v : picked) {
      const auto [new_lb, new_ub] =
          NarrowAroundSolution(lb[v], ub[v], model.planted_sol[v]);
      CHECK_LT(new_lb, new_ub);
      lb[v] = new_lb;
      ub[v] = new_ub;
      batch.vars.push_back(v);
      batch.lbs.push_back(new_lb);
      batch.ubs.push_back(new_ub);
    }
  }

  int64_t total_rebuilds = 0;
  int64_t last_rebuilds = 0;
  for (auto _ : state) {
    SharedBoundsManager shared_bounds(model.proto);
    DenseModelCopy copy("ls", model.proto, &shared_bounds,
                        /*shared_clauses=*/nullptr);
    bool updated = false;
    CHECK(copy.MaybeUpdate(updated, std::numeric_limits<double>::infinity()));
    ModelStats stats = ComputeModelStats(copy.proto());
    double search_dtime = 0.0;
    int64_t iter_rebuilds = updated ? 1 : 0;
    for (const BoundsBatch& batch : rounds) {
      shared_bounds.ReportPotentialNewBounds("sat", batch.vars, batch.lbs,
                                             batch.ubs);
      shared_bounds.Synchronize();
      const double pre_dtime = EstimateCopyDeterministicTime(stats);
      search_dtime += 3.0 * pre_dtime;
      updated = false;
      CHECK(copy.MaybeUpdate(updated, pre_dtime / search_dtime));
      if (updated) {
        stats = ComputeModelStats(copy.proto());
        search_dtime = 0.0;
        ++iter_rebuilds;
      }
    }
    benchmark::DoNotOptimize(copy.var_domains().size());
    last_rebuilds = iter_rebuilds;
    total_rebuilds += iter_rebuilds;
  }
  benchmark::DoNotOptimize(total_rebuilds);
  state.SetLabel(absl::StrCat("rebuilds=", last_rebuilds));
}

template <ModelKind kKind>
void BM_FixedVariables(benchmark::State& state) {
  const int num_vars = state.range(0);
  const int num_rounds = state.range(1);
  std::mt19937 rng(20261110 + static_cast<int>(kKind) * 10007 + num_vars * 31 +
                   num_rounds);
  const PlantedModel model = BuildReferenceModel(kKind, num_vars, rng);
  const int total_vars = model.proto.variables_size();

  std::vector<int> all_vars(total_vars);
  std::iota(all_vars.begin(), all_vars.end(), 0);
  std::shuffle(all_vars.begin(), all_vars.end(), rng);

  const int total_to_fix = (total_vars * 80) / 100;
  std::vector<BoundsBatch> rounds(num_rounds);
  for (int r = 0; r < num_rounds; ++r) {
    const int begin = (r * total_to_fix) / num_rounds;
    const int end = ((r + 1) * total_to_fix) / num_rounds;
    BoundsBatch& batch = rounds[r];
    batch.vars.reserve(end - begin);
    batch.lbs.reserve(end - begin);
    batch.ubs.reserve(end - begin);
    for (int i = begin; i < end; ++i) {
      const int v = all_vars[i];
      batch.vars.push_back(v);
      batch.lbs.push_back(model.planted_sol[v]);
      batch.ubs.push_back(model.planted_sol[v]);
    }
  }

  int64_t total_rebuilds = 0;
  int64_t last_rebuilds = 0;
  double last_copy_dtime = 0.0;
  for (auto _ : state) {
    SharedBoundsManager shared_bounds(model.proto);
    DenseModelCopy copy("ls", model.proto, &shared_bounds,
                        /*shared_clauses=*/nullptr);
    bool updated = false;
    CHECK(copy.MaybeUpdate(updated, std::numeric_limits<double>::infinity()));
    ModelStats stats = ComputeModelStats(copy.proto());
    double pre_dtime = EstimateCopyDeterministicTime(stats);
    double search_dtime = 0.0;
    int64_t iter_rebuilds = updated ? 1 : 0;
    double iter_copy_dtime = updated ? pre_dtime : 0.0;
    for (const BoundsBatch& batch : rounds) {
      shared_bounds.ReportPotentialNewBounds("sat", batch.vars, batch.lbs,
                                             batch.ubs);
      shared_bounds.Synchronize();
      pre_dtime = EstimateCopyDeterministicTime(stats);
      search_dtime += 10.0 * pre_dtime;
      updated = false;
      CHECK(copy.MaybeUpdate(updated, pre_dtime / search_dtime));
      if (updated) {
        stats = ComputeModelStats(copy.proto());
        search_dtime = 0.0;
        ++iter_rebuilds;
        iter_copy_dtime += pre_dtime;
      }
    }
    benchmark::DoNotOptimize(copy.var_domains().size());
    last_rebuilds = iter_rebuilds;
    last_copy_dtime = iter_copy_dtime;
    total_rebuilds += iter_rebuilds;
  }
  benchmark::DoNotOptimize(total_rebuilds);
  state.SetLabel(absl::StrFormat("rebuilds=%d dtime=%.3fms", last_rebuilds,
                                 1e3 * last_copy_dtime));
}

template <ModelKind kKind>
void BM_MixedUpdates(benchmark::State& state) {
  const int num_vars = state.range(0);
  const int num_rounds = state.range(1);
  std::mt19937 rng(20261111 + static_cast<int>(kKind) * 10007 + num_vars * 31 +
                   num_rounds);
  const PlantedModel model = BuildReferenceModel(kKind, num_vars, rng);
  const int total_vars = model.proto.variables_size();
  const int num_bools = static_cast<int>(model.bool_vars.size());
  const int num_ints = static_cast<int>(model.int_vars.size());

  std::vector<int> unfixed_vars(total_vars);
  std::iota(unfixed_vars.begin(), unfixed_vars.end(), 0);
  std::shuffle(unfixed_vars.begin(), unfixed_vars.end(), rng);
  std::vector<bool> is_fixed(total_vars, false);

  std::vector<int> equiv_bool_pool = model.bool_vars;
  std::shuffle(equiv_bool_pool.begin(), equiv_bool_pool.end(), rng);

  std::vector<int64_t> lb(total_vars);
  std::vector<int64_t> ub(total_vars);
  for (int v = 0; v < total_vars; ++v) {
    const auto& dom = model.proto.variables(v).domain();
    lb[v] = dom[0];
    ub[v] = dom[dom.size() - 1];
  }

  std::vector<int> int_pool;
  int_pool.reserve(num_ints);
  std::vector<int> picked;
  std::vector<MixedBatch> rounds(num_rounds);
  int fix_cursor = 0;
  int equiv_cursor = 0;

  for (int r = 0; r < num_rounds; ++r) {
    MixedBatch& batch = rounds[r];

    // Fix ~4% of remaining unfixed variables per round to `planted_sol[v]`.
    const int remaining_unfixed = total_vars - fix_cursor;
    const int num_fix =
        std::min(remaining_unfixed, std::max(1, (remaining_unfixed * 4) / 100));
    for (int i = 0; i < num_fix; ++i) {
      const int v = unfixed_vars[fix_cursor++];
      is_fixed[v] = true;
      lb[v] = model.planted_sol[v];
      ub[v] = model.planted_sol[v];
      batch.vars.push_back(v);
      batch.lbs.push_back(model.planted_sol[v]);
      batch.ubs.push_back(model.planted_sol[v]);
    }

    // Non-fixing integer domain reductions (~10% of unfixed int vars/round),
    // skipping any whose current width < 2.
    int_pool.clear();
    for (const int v : model.int_vars) {
      if (!is_fixed[v] && ub[v] - lb[v] >= 2) {
        int_pool.push_back(v);
      }
    }
    const int num_reduce =
        std::min<int>(int_pool.size(), std::max(1, (num_ints * 10) / 100));
    SampleDistinctFromPool(int_pool, num_reduce, rng, picked);
    for (const int v : picked) {
      const auto [new_lb, new_ub] =
          NarrowAroundSolution(lb[v], ub[v], model.planted_sol[v]);
      CHECK_LT(new_lb, new_ub);
      lb[v] = new_lb;
      ub[v] = new_ub;
      batch.vars.push_back(v);
      batch.lbs.push_back(new_lb);
      batch.ubs.push_back(new_ub);
    }

    // Acyclic Boolean equivalences (~2% of bool_vars/round) consistent with
    // `planted_sol`.
    const int target_equivs = std::max(1, (num_bools * 2) / 100);
    for (int e = 0; e < target_equivs && equiv_cursor + 1 < num_bools; ++e) {
      const int u = equiv_bool_pool[equiv_cursor++];
      const int v =
          equiv_bool_pool[absl::Uniform(rng, equiv_cursor, num_bools)];
      if (model.planted_sol[u] == model.planted_sol[v]) {
        batch.binary_clauses.push_back({NegatedRef(u), v});
        batch.binary_clauses.push_back({u, NegatedRef(v)});
      } else {
        batch.binary_clauses.push_back({u, v});
        batch.binary_clauses.push_back({NegatedRef(u), NegatedRef(v)});
      }
    }
  }

  int64_t total_rebuilds = 0;
  int64_t last_rebuilds = 0;
  double last_copy_dtime = 0.0;
  for (auto _ : state) {
    SharedBoundsManager shared_bounds(model.proto);
    SharedClausesManager shared_clauses(/*always_synchronize=*/true);
    const int sat_id =
        shared_clauses.RegisterNewId("sat", /*may_terminate_early=*/true);
    DenseModelCopy copy("ls", model.proto, &shared_bounds, &shared_clauses);
    bool updated = false;
    CHECK(copy.MaybeUpdate(updated, std::numeric_limits<double>::infinity()));
    ModelStats stats = ComputeModelStats(copy.proto());
    double pre_dtime = EstimateCopyDeterministicTime(stats);
    double search_dtime = 0.0;
    int64_t iter_rebuilds = updated ? 1 : 0;
    double iter_copy_dtime = updated ? pre_dtime : 0.0;
    for (const MixedBatch& batch : rounds) {
      if (!batch.vars.empty()) {
        shared_bounds.ReportPotentialNewBounds("sat", batch.vars, batch.lbs,
                                               batch.ubs);
        shared_bounds.Synchronize();
      }
      for (const auto [lit1, lit2] : batch.binary_clauses) {
        shared_clauses.AddBinaryClause(sat_id, lit1, lit2);
      }
      pre_dtime = EstimateCopyDeterministicTime(stats);
      search_dtime += 3.0 * pre_dtime;
      updated = false;
      CHECK(copy.MaybeUpdate(updated, pre_dtime / search_dtime));
      if (updated) {
        stats = ComputeModelStats(copy.proto());
        search_dtime = 0.0;
        ++iter_rebuilds;
        iter_copy_dtime += pre_dtime;
        const std::vector<int64_t> dense_sol =
            copy.MapSolution(model.planted_sol);
        CHECK_EQ(dense_sol.size(), copy.var_domains().size());
        std::vector<int64_t> rev_sol = copy.ReverseMapSolution(dense_sol);
        CHECK_EQ(rev_sol.size(), model.planted_sol.size());
        benchmark::DoNotOptimize(rev_sol);
      }
    }
    benchmark::DoNotOptimize(copy.var_domains().size());
    last_rebuilds = iter_rebuilds;
    last_copy_dtime = iter_copy_dtime;
    total_rebuilds += iter_rebuilds;
  }
  benchmark::DoNotOptimize(total_rebuilds);
  state.SetLabel(absl::StrFormat("rebuilds=%d dtime=%.3fms", last_rebuilds,
                                 1e3 * last_copy_dtime));
}

#define REGISTER_MODEL_BENCHMARKS(KIND)                                       \
  BENCHMARK(BM_InitialCopy<KIND>)->Args({1000})->Args({8000})->Args({32000}); \
  BENCHMARK(BM_ModelReduction<KIND>)                                          \
      ->Args({1000, 20})                                                      \
      ->Args({1000, 60})                                                      \
      ->Args({8000, 20})                                                      \
      ->Args({8000, 60})                                                      \
      ->Args({32000, 20})                                                     \
      ->Args({32000, 50})                                                     \
      ->Args({32000, 80});                                                    \
  BENCHMARK(BM_DomainReductions<KIND>)                                        \
      ->Args({1000, 10})                                                      \
      ->Args({8000, 10})                                                      \
      ->Args({32000, 10});                                                    \
  BENCHMARK(BM_FixedVariables<KIND>)                                          \
      ->Args({1000, 5})                                                       \
      ->Args({1000, 10})                                                      \
      ->Args({8000, 5})                                                       \
      ->Args({8000, 10})                                                      \
      ->Args({32000, 5})                                                      \
      ->Args({32000, 10});                                                    \
  BENCHMARK(BM_MixedUpdates<KIND>)                                            \
      ->Args({1000, 15})                                                      \
      ->Args({8000, 15})                                                      \
      ->Args({32000, 15})

BENCHMARK(BM_EmptyModelCopy);
BENCHMARK(BM_EmptyModelRebuild);

REGISTER_MODEL_BENCHMARKS(kBooleanHeavy);
REGISTER_MODEL_BENCHMARKS(kLinearHeavy);
REGISTER_MODEL_BENCHMARKS(kSchedulingHeavy);
REGISTER_MODEL_BENCHMARKS(kRoutingHeavy);
REGISTER_MODEL_BENCHMARKS(kGeneralCpHeavy);

#undef REGISTER_MODEL_BENCHMARKS

}  // namespace
}  // namespace operations_research::sat
