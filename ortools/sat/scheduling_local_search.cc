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

#include "ortools/sat/scheduling_local_search.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/graph_base/topologicalsorter.h"
#include "ortools/sat/combine_solutions.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/scheduling_model.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

CompactVectorVector<int> SchedulingLocalSearch::BuildInitialMachineSequences(
    absl::Span<const int64_t> initial_solution,
    absl::Span<const int> current_machines) const {
  CompactVectorVectorBuilder<int> machine_tasks_builder;
  machine_tasks_builder.ReserveNumItems(num_tasks_);
  for (int i = 0; i < num_tasks_; ++i) {
    machine_tasks_builder.Add(current_machines[i], i);
  }
  CompactVectorVector<int> machine_tasks;
  machine_tasks.ResetFromBuilder(machine_tasks_builder, num_machines_);

  for (int machine = 0; machine < num_machines_; ++machine) {
    absl::Span<int> tasks_on_machine = machine_tasks[machine];
    absl::c_sort(tasks_on_machine, [&initial_solution](int a, int b) {
      return initial_solution[a] < initial_solution[b];
    });
  }

  return machine_tasks;
}

std::vector<SchedulingLocalSearch::InsertMove>
SchedulingLocalSearch::GenerateMoves(
    absl::Span<const int> critical_path, const SolverState& state,
    absl::Span<const IntegerValue> start_mins,
    absl::Span<const int> current_machines,
    absl::Span<const int64_t> current_durations,
    const CompactVectorVector<int>& current_machine_tasks) const {
  const absl::Span<const int> prev_on_machine = state.prev_on_machine;
  const absl::Span<const int> next_on_machine = state.next_on_machine;
  const absl::Span<const IntegerValue> tails = state.tails;

  if (critical_path.size() < 2) return {};

  std::vector<InsertMove> moves;
  // Reserve enough space to cover both N8 and NA candidates
  moves.reserve(critical_path.size() * 8);

  GenerateN8Moves(critical_path, prev_on_machine, next_on_machine, start_mins,
                  tails, current_machines, current_durations, &moves);

  GenerateNAMoves(critical_path, start_mins, tails, current_machines,
                  current_durations, current_machine_tasks, state, &moves);

  return moves;
}

void SchedulingLocalSearch::GenerateN8Moves(
    absl::Span<const int> critical_path, absl::Span<const int> prev_on_machine,
    absl::Span<const int> next_on_machine,
    absl::Span<const IntegerValue> start_mins,
    absl::Span<const IntegerValue> tails,
    absl::Span<const int> current_machines,
    absl::Span<const int64_t> current_durations,
    std::vector<InsertMove>* moves) const {
  if (critical_path.size() < 2) return;
  // Heuristics to forbid moves that can potentially create cycles.
  auto is_invalid_forward_move = [&](int x, int target) {
    if (target == -1) return false;

    // This check is not needed for classical JSSP where there is no precedences
    // between operations on the same machine.
    if (job_reachability_[x * num_tasks_ + target]) return true;

    for (const int js : job_successors_[x]) {
      // Another check that is not needed for classical JSSP where there are no
      // precedences between operations on the same machine. If a static
      // successor is on the same machine, we cannot leap over it. If it starts
      // before or at the target, we are leaping over it (or landing on it),
      // which creates a cycle.
      if (current_machines[js] == current_machines[x]) {
        if (start_mins[js] <= start_mins[target]) return true;
      }

      // Xie et al. Prop 1: moving x here provably worsens the makespan or
      // forms an indirect cycle.
      if (current_durations[target] + tails[target] < tails[js]) return true;
    }
    return false;
  };

  auto is_invalid_backward_move = [&](int x, int target) {
    if (target == -1) return false;

    if (job_reachability_[target * num_tasks_ + x]) return true;

    for (const int jp :
         problem_.tasks[x].tasks_that_must_complete_before_this) {
      if (current_machines[jp] == current_machines[x]) {
        if (start_mins[jp] >= start_mins[target]) return true;
      }

      if (start_mins[target] + current_durations[target] <
          start_mins[jp] + current_durations[jp]) {
        return true;
      }
    }
    return false;
  };

  struct Block {
    // Indexes are relative to the position on the critical_path vector.
    int start_idx;
    int end_idx;
  };

  // A block is a sequence of tasks in the critical path that must be executed
  // in sequence on the same machine.
  std::vector<Block> blocks;
  blocks.reserve(critical_path.size());

  // Fast lookup table to prevent external leaps from inserting tasks back into
  // other sections of the critical path.
  std::vector<bool> is_critical(num_tasks_, false);

  int current_start = 0;
  for (int i = 0; i <= critical_path.size(); ++i) {
    if (i != critical_path.size()) {
      is_critical[critical_path[i]] = true;
    }
    if (i == critical_path.size() ||
        current_machines[critical_path[i]] !=
            current_machines[critical_path[current_start]]) {
      if (i - current_start >= 1) {
        blocks.push_back({current_start, i - 1});
      }
      current_start = i;
    }
  }

  const int num_blocks = blocks.size();
  if (num_blocks == 0) return;

  // First op restrictions and leftward leaps.
  // Applies to all blocks except the first one.
  for (int b = 1; b < num_blocks; ++b) {
    const int start = blocks[b].start_idx;
    const int end = blocks[b].end_idx;
    const int first_op = critical_path[start];
    const int current_m = current_machines[first_op];

    // Prop 5: Inner ops moving left before first_op.
    for (int i = start + 1; i <= end; ++i) {
      if (!is_invalid_backward_move(critical_path[i], first_op)) {
        moves->push_back({critical_path[i], first_op, false, current_m,
                          current_durations[critical_path[i]]});
      }
    }

    // Prop 3: first_op moving right after inner ops.
    for (int i = start + 2; i < end; ++i) {
      const int target = critical_path[i];
      if (is_invalid_forward_move(first_op, target)) break;  // Monotonic prune.
      moves->push_back(
          {first_op, target, true, current_m, current_durations[first_op]});
    }

    // N8 external leaps (leftward).
    int target = prev_on_machine[first_op];
    while (target != -1 && !is_critical[target]) {
      bool pushed_any = false;
      for (int i = start; i <= end; ++i) {
        if (!is_invalid_backward_move(critical_path[i], target)) {
          moves->push_back({critical_path[i], target, false, current_m,
                            current_durations[critical_path[i]]});
          pushed_any = true;
        }
      }
      if (!pushed_any) break;  // Monotonic prune.
      target = prev_on_machine[target];
    }
  }

  // Last op restrictions and rightward leaps.
  // Applies to all blocks except the last one.
  for (int b = 0; b < num_blocks - 1; ++b) {
    const int start = blocks[b].start_idx;
    const int end = blocks[b].end_idx;
    const int last_op = critical_path[end];
    const int current_m = current_machines[last_op];

    // Prop 6: Inner ops moving RIGHT after last_op.
    for (int i = start; i < end; ++i) {
      if (!is_invalid_forward_move(critical_path[i], last_op)) {
        moves->push_back({critical_path[i], last_op, true, current_m,
                          current_durations[critical_path[i]]});
      }
    }

    // Prop 4: last_op moving LEFT before inner ops.
    for (int i = end - 2; i > start; --i) {
      const int target = critical_path[i];
      if (is_invalid_backward_move(last_op, target)) break;  // Monotonic prune.
      moves->push_back(
          {last_op, target, false, current_m, current_durations[last_op]});
    }

    // N8 external leaps (rightward).
    int target = next_on_machine[last_op];
    while (target != -1 && !is_critical[target]) {
      bool pushed_any = false;
      // Evaluate ALL operations in the block, including last_op!
      for (int i = start; i <= end; ++i) {
        if (!is_invalid_forward_move(critical_path[i], target)) {
          moves->push_back({critical_path[i], target, true, current_m,
                            current_durations[critical_path[i]]});
          pushed_any = true;
        }
      }
      if (!pushed_any) break;  // Monotonic prune.
      target = next_on_machine[target];
    }
  }
}

void SchedulingLocalSearch::GenerateNAMoves(
    absl::Span<const int> critical_path,
    absl::Span<const IntegerValue> start_mins,
    absl::Span<const IntegerValue> tails,
    absl::Span<const int> current_machines,
    absl::Span<const int64_t> current_durations,
    const CompactVectorVector<int>& current_machine_tasks,
    const SolverState& state, std::vector<InsertMove>* moves) const {
  auto x_reaches_target = [&](int x, int target) {
    if (target == -1 || x == -1) return false;
    if (job_reachability_[x * num_tasks_ + target]) return true;
    for (int js : job_successors_[x]) {
      if (state.topo_index[js] <= state.topo_index[target] &&
          start_mins[target] >= start_mins[js] + current_durations[js] &&
          tails[js] >= current_durations[target] + tails[target]) {
        return true;
      }
    }
    return false;
  };

  auto target_reaches_x = [&](int target, int x) {
    if (target == -1 || x == -1) return false;
    if (job_reachability_[target * num_tasks_ + x]) return true;
    for (int jp : problem_.tasks[x].tasks_that_must_complete_before_this) {
      if (state.topo_index[target] <= state.topo_index[jp] &&
          start_mins[jp] >= start_mins[target] + current_durations[target] &&
          tails[target] >= current_durations[jp] + tails[jp]) {
        return true;
      }
    }
    return false;
  };

  for (const int t : critical_path) {
    if (problem_.tasks[t].compatible_machine.size() <= 1) continue;

    for (int a = 0; a < problem_.tasks[t].compatible_machine.size(); ++a) {
      const int new_m = problem_.tasks[t].compatible_machine[a];
      if (new_m == current_machines[t]) continue;

      const int64_t new_duration = problem_.tasks[t].duration_for_machine[a];

      absl::Span<const int> seq = current_machine_tasks[new_m];

      if (seq.empty()) {
        moves->push_back({t, -1, true, new_m, new_duration});
        continue;
      }

      const int first_task = seq[0];
      // Check if safe to place BEFORE the first element
      if (!target_reaches_x(first_task, t)) {
        moves->push_back({t, first_task, false, new_m, new_duration});
      }

      for (int i = 0; i < seq.size(); ++i) {
        const int t2 = seq[i];
        const int next_t2 = (i + 1 < seq.size()) ? seq[i + 1] : -1;

        // Check if safe to place AFTER t2 (must check both sides of insertion)
        if (x_reaches_target(t, t2)) continue;
        if (next_t2 != -1 && target_reaches_x(next_t2, t)) continue;

        moves->push_back({t, t2, true, new_m, new_duration});
      }
    }
  }
}

IntegerValue SchedulingLocalSearch::EstimateSequenceMove(
    const InsertMove& move, absl::Span<const IntegerValue> start_mins,
    absl::Span<const IntegerValue> tails, absl::Span<const int> prev_on_machine,
    absl::Span<const int> next_on_machine,
    absl::Span<const int64_t> current_durations,
    absl::Span<const IntegerValue> static_job_heads,
    absl::Span<const IntegerValue> static_job_tails,
    IntegerValue prune_threshold, MoveEvaluationScratch* scratch) const {
  const int x = move.task;
  const int target = move.target_task;
  const int64_t x_duration = current_durations[x];

  // Build the contiguous mutated sequence of tasks on this machine.
  //
  // Note that in the original Balas & Vazacopoulos paper, the moved task 'u'
  // and target 'v' (our 'x' and 'target') are picked so all tasks between
  // them on the critical path are on the same machine. The article refers to
  // the 'Q' set of tasks between 'u' and 'v' as a subsequence of the critical
  // path. However, it is also technically correct to say that 'Q' is simply a
  // subsequence of the machine tasks.
  //
  // Because we generalize this for the N8 leaps from Xie et al., our tasks
  // are picked from the same machine, but the sequence between them is not
  // necessarily on the critical path. `mutated_sequence` below represents
  // this generalized displaced machine segment (Q) combined with the moved
  // task (x).

  // O(1) Job-level early exit
  if (static_job_heads[x] + x_duration + static_job_tails[x] >
      prune_threshold) {
    return kMaxIntegerValue;
  }

  std::vector<int>& mutated_sequence = scratch->mutated_sequence;
  mutated_sequence.clear();
  int p_m = -1;  // The machine predecessor to the entire mutated segment.
  int s_m = -1;  // The machine successor to the entire mutated segment.

  if (move.place_after) {
    p_m = prev_on_machine[x];
    s_m = next_on_machine[target] == x ? next_on_machine[x]
                                       : next_on_machine[target];

    int curr = next_on_machine[x];
    while (curr != -1) {
      mutated_sequence.push_back(curr);
      if (curr == target) break;
      curr = next_on_machine[curr];
    }
    mutated_sequence.push_back(x);
  } else {
    p_m = prev_on_machine[target] == x ? prev_on_machine[x]
                                       : prev_on_machine[target];
    s_m = next_on_machine[x];

    mutated_sequence.push_back(x);
    const int stop_node = prev_on_machine[x];
    int curr = target;
    while (curr != -1) {
      mutated_sequence.push_back(curr);
      if (curr == stop_node) break;
      curr = next_on_machine[curr];
    }
  }

  return ComputeMutatedSequenceMakespan(
      x, x_duration, p_m, s_m, start_mins, tails, current_durations,
      static_job_heads, static_job_tails, scratch);
}

IntegerValue SchedulingLocalSearch::ComputeMutatedSequenceMakespan(
    int x, int64_t x_duration, int p_m, int s_m,
    absl::Span<const IntegerValue> start_mins,
    absl::Span<const IntegerValue> tails,
    absl::Span<const int64_t> current_durations,
    absl::Span<const IntegerValue> static_job_heads,
    absl::Span<const IntegerValue> static_job_tails,
    MoveEvaluationScratch* scratch) const {
  std::vector<int>& mutated_sequence = scratch->mutated_sequence;

  // Forward sweep to calculate new heads.
  std::vector<IntegerValue>& new_heads = scratch->heads;
  new_heads.resize(mutated_sequence.size());
  IntegerValue head = 0;
  if (p_m != -1) {
    head = start_mins[p_m] + current_durations[p_m];
  }

  for (size_t i = 0; i < mutated_sequence.size(); ++i) {
    const int t = mutated_sequence[i];
    const int64_t dur = (t == x) ? x_duration : current_durations[t];
    head = std::max(head, static_job_heads[t]);
    new_heads[i] = head;
    head += dur;
  }

  // Backward sweep to calculate new tails.
  std::vector<IntegerValue>& new_tails = scratch->tails;
  new_tails.resize(mutated_sequence.size());
  IntegerValue tail = 0;
  if (s_m != -1) {
    tail = current_durations[s_m] + tails[s_m];
  }

  for (int i = mutated_sequence.size() - 1; i >= 0; --i) {
    const int t = mutated_sequence[i];
    const int64_t dur = (t == x) ? x_duration : current_durations[t];
    tail = std::max(tail, static_job_tails[t]);
    new_tails[i] = tail;
    tail += dur;
  }

  // Find the maximum path through the mutated segment.
  IntegerValue max_path = 0;
  for (size_t i = 0; i < mutated_sequence.size(); ++i) {
    const int t = mutated_sequence[i];
    const int64_t dur = (t == x) ? x_duration : current_durations[t];
    const IntegerValue path = new_heads[i] + dur + new_tails[i];
    max_path = std::max(max_path, path);
  }

  return max_path;
}

IntegerValue SchedulingLocalSearch::EstimateAssignmentMove(
    const InsertMove& move, absl::Span<const IntegerValue> start_mins,
    absl::Span<const IntegerValue> tails, const SolverState& state,
    absl::Span<const int64_t> current_durations,
    const CompactVectorVector<int>& current_machine_tasks,
    absl::Span<const IntegerValue> static_job_heads,
    absl::Span<const IntegerValue> static_job_tails,
    IntegerValue prune_threshold, MoveEvaluationScratch* scratch) const {
  const int x = move.task;
  const int target = move.target_task;
  const int new_m = move.new_machine;
  const int64_t x_duration = move.new_duration;

  int64_t new_workload = state.machine_workloads[new_m] + x_duration;
  if (new_workload > prune_threshold) {
    return new_workload;
  }

  absl::Span<const int> seq = current_machine_tasks[new_m];

  // Track the immediate neighbors of x specifically for the pruning bounds.
  int p_x = -1;
  int s_x = -1;

  if (!seq.empty()) {
    if (target == -1) {
      p_x = seq.back();
    } else if (!move.place_after && target == seq[0]) {
      s_x = seq[0];
    } else {
      p_x = target;
    }
  }

  IntegerValue lb_head_x = static_job_heads[x];
  if (p_x != -1) {
    lb_head_x = std::max(lb_head_x, start_mins[p_x] + current_durations[p_x]);
  }

  IntegerValue lb_tail_x = static_job_tails[x];
  if (s_x != -1) {
    lb_tail_x = std::max(lb_tail_x, current_durations[s_x] + tails[s_x]);
  }

  if (lb_head_x + x_duration + lb_tail_x > prune_threshold) {
    return kMaxIntegerValue;
  }

  std::vector<int>& mutated_sequence = scratch->mutated_sequence;
  mutated_sequence.clear();
  mutated_sequence.reserve(num_tasks_);

  bool inserted = false;
  if (target == -1) {
    mutated_sequence.push_back(x);
    inserted = true;
  } else if (!move.place_after && !seq.empty() && target == seq[0]) {
    mutated_sequence.push_back(x);
    inserted = true;
  }

  for (int t : seq) {
    mutated_sequence.push_back(t);
    if (move.place_after && t == target) {
      mutated_sequence.push_back(x);
      inserted = true;
    }
  }
  if (!inserted) mutated_sequence.push_back(x);

  return ComputeMutatedSequenceMakespan(
      x, x_duration, -1, -1, start_mins, tails, current_durations,
      static_job_heads, static_job_tails, scratch);
}

std::optional<SchedulingLocalSearch::InsertMove>
SchedulingLocalSearch::SelectBestMove(
    absl::Span<const InsertMove> candidates,
    absl::Span<const IntegerValue> start_mins, const SolverState& state,
    absl::Span<const int> tabu_matrix,
    absl::Span<const int> assignment_tabu_matrix,
    absl::Span<const int> current_machines,
    absl::Span<const int64_t> current_durations,
    const CompactVectorVector<int>& current_machine_tasks,
    int current_iteration, IntegerValue global_best_makespan,
    absl::BitGenRef random) const {
  absl::Span<const IntegerValue> tails = state.tails;
  absl::Span<const int> prev_on_machine = state.prev_on_machine;
  absl::Span<const int> next_on_machine = state.next_on_machine;
  DCHECK_EQ(start_mins.size(), num_tasks_);
  DCHECK_EQ(tails.size(), num_tasks_);
  DCHECK_EQ(prev_on_machine.size(), num_tasks_);
  DCHECK_EQ(next_on_machine.size(), num_tasks_);

  std::vector<IntegerValue> static_job_heads(num_tasks_, 0);
  std::vector<IntegerValue> static_job_tails(num_tasks_, 0);
  for (int t = 0; t < num_tasks_; ++t) {
    IntegerValue j_head = problem_.tasks[t].min_start;
    for (int p : problem_.tasks[t].tasks_that_must_complete_before_this) {
      j_head = std::max(j_head, start_mins[p] + current_durations[p]);
    }
    static_job_heads[t] = j_head;

    IntegerValue j_tail = 0;
    for (int s : job_successors_[t]) {
      j_tail = std::max(j_tail, current_durations[s] + tails[s]);
    }
    static_job_tails[t] = j_tail;
  }

  std::optional<InsertMove> best_move = std::nullopt;
  IntegerValue best_estimate = kMaxIntegerValue;
  int tie_count = 0;

  std::optional<InsertMove> best_tabu_move = std::nullopt;
  IntegerValue best_tabu_estimate = kMaxIntegerValue;
  int tabu_tie_count = 0;

  MoveEvaluationScratch scratch;
  for (const InsertMove& move : candidates) {
    const int x = move.task;
    const int target = move.target_task;

    bool is_tabu = false;
    // Tabu check
    if (move.new_machine != current_machines[x]) {
      if (assignment_tabu_matrix[x * num_machines_ + move.new_machine] >
          current_iteration) {
        is_tabu = true;
      }
    } else {
      int new_prev = move.place_after ? target : prev_on_machine[target];
      if (new_prev == x) new_prev = prev_on_machine[x];

      int new_next = move.place_after ? next_on_machine[target] : target;
      if (new_next == x) new_next = next_on_machine[x];

      if (new_prev != -1) {
        int u = x;
        int v = new_prev;
        if (u > v) std::swap(u, v);
        DCHECK_LT(u * num_tasks_ + v, tabu_matrix.size());
        if (tabu_matrix[u * num_tasks_ + v] > current_iteration) is_tabu = true;
      }
      if (!is_tabu && new_next != -1) {
        int u = x;
        int v = new_next;
        if (u > v) std::swap(u, v);
        DCHECK_LT(u * num_tasks_ + v, tabu_matrix.size());
        if (tabu_matrix[u * num_tasks_ + v] > current_iteration) is_tabu = true;
      }
    }
    // A non-tabu move only matters if it ties or beats the current best.
    // A tabu move matters if it beats the global best or is the best fallback
    // tabu move.
    const IntegerValue prune_threshold =
        is_tabu ? std::max(global_best_makespan - 1, best_tabu_estimate)
                : best_estimate;

    // Evaluate an approximation of the makespan if we make this move.
    IntegerValue estimate;
    if (move.new_machine == current_machines[x]) {
      estimate = EstimateSequenceMove(move, start_mins, tails, prev_on_machine,
                                      next_on_machine, current_durations,
                                      static_job_heads, static_job_tails,
                                      prune_threshold, &scratch);
    } else {
      estimate = EstimateAssignmentMove(
          move, start_mins, tails, state, current_durations,
          current_machine_tasks, static_job_heads, static_job_tails,
          prune_threshold, &scratch);
    }

    // 3. Tabu Search Logic + Aspiration Criterion
    // If it's NOT tabu, it's a valid candidate.
    // If it IS tabu, we only allow it if the estimate beats our all-time global
    // best schedule.
    if (!is_tabu || estimate < global_best_makespan) {
      if (estimate < best_estimate) {
        // We found a strictly better move!
        best_estimate = estimate;
        best_move = move;
        tie_count = 1;
      } else if (estimate == best_estimate) {
        // Use Reservoir Sampling for tie-breaking.
        tie_count++;
        // There is exactly a 1-in-N chance of replacing the current best.
        if (absl::Uniform<int>(random, 0, tie_count) == 0) {
          best_move = move;
        }
      }
    } else {
      // Track the best forbidden move just in case we get trapped.
      if (estimate < best_tabu_estimate) {
        best_tabu_estimate = estimate;
        best_tabu_move = move;
        tabu_tie_count = 1;
      } else if (estimate == best_tabu_estimate) {
        tabu_tie_count++;
        if (absl::Uniform<int>(random, 0, tabu_tie_count) == 0) {
          best_tabu_move = move;
        }
      }
    }
  }

  // If we are completely boxed in by the Tabu list, use the best Tabu move.
  if (!best_move.has_value() && best_tabu_move.has_value()) {
    return best_tabu_move;
  }

  return best_move;
}

CompactVectorVector<int> SchedulingLocalSearch::ComputeJobSuccessors(
    const SchedulingProblem& problem) {
  const int num_tasks = problem.tasks.size();
  CompactVectorVectorBuilder<int> job_successors_builder;
  job_successors_builder.ReserveNumItems(num_tasks);

  for (int v = 0; v < num_tasks; ++v) {
    for (int u : problem.tasks[v].tasks_that_must_complete_before_this) {
      job_successors_builder.Add(u, v);
    }
  }
  return CompactVectorVector<int>(job_successors_builder, num_tasks);
}

SchedulingLocalSearch::SolverState SchedulingLocalSearch::ComputeDynamicState(
    const CompactVectorVector<int>& machine_tasks,
    absl::Span<const int> topo_order,
    absl::Span<const int64_t> current_durations) const {
  SolverState state;
  state.prev_on_machine.assign(num_tasks_, -1);
  state.next_on_machine.assign(num_tasks_, -1);
  state.position_in_machine.assign(num_tasks_, -1);
  state.tails.assign(num_tasks_, 0);
  state.machine_workloads.assign(num_machines_, 0);
  state.topo_index.assign(num_tasks_, -1);

  for (int i = 0; i < topo_order.size(); ++i) {
    state.topo_index[topo_order[i]] = i;
  }

  // 1. Build O(1) Machine Successor/Predecessor lookups
  for (int m = 0; m < machine_tasks.size(); ++m) {
    const absl::Span<const int> tasks_on_machine = machine_tasks[m];
    for (int i = 0; i < tasks_on_machine.size(); ++i) {
      const int u = tasks_on_machine[i];
      state.machine_workloads[m] += current_durations[u];
      state.position_in_machine[u] = i;
      if (i > 0) {
        state.prev_on_machine[u] = tasks_on_machine[i - 1];
      }
      if (i < tasks_on_machine.size() - 1) {
        state.next_on_machine[u] = tasks_on_machine[i + 1];
      }
    }
  }

  // 2. Calculate Tails (Reverse Topological Traversal)
  // We iterate backwards through the topological order. By the time we visit
  // a node, all of its successors have already finalized their tails.
  for (int i = num_tasks_ - 1; i >= 0; --i) {
    const int u = topo_order[i];
    IntegerValue max_tail = 0;

    // A. Check Job Successors
    for (const int s : job_successors_[u]) {
      const IntegerValue path_len = current_durations[s] + state.tails[s];
      if (path_len > max_tail) {
        max_tail = path_len;
      }
    }

    // B. Check Machine Successor
    const int m_succ = state.next_on_machine[u];
    if (m_succ != -1) {
      const IntegerValue path_len =
          current_durations[m_succ] + state.tails[m_succ];
      if (path_len > max_tail) {
        max_tail = path_len;
      }
    }

    state.tails[u] = max_tail;
  }

  return state;
}

void SchedulingLocalSearch::AnalyzeSchedule(
    const CompactVectorVector<int>& machine_tasks,
    absl::Span<const int64_t> current_durations,
    SchedulingLocalSearch::ScheduleAnalysis* analysis) const {
  // Build the precedences graph, taking into account the order of the tasks on
  // the machines and the task precedences.
  std::vector<int> prev_on_machine(num_tasks_, -1);
  CompactVectorVector<int>& adj = analysis->scratch_adjacency_list;

  for (int machine = 0; machine < num_machines_; ++machine) {
    const auto& tasks_on_machine = machine_tasks[machine];
    for (int i = 1; i < tasks_on_machine.size(); ++i) {
      int u = tasks_on_machine[i - 1];
      int v = tasks_on_machine[i];
      prev_on_machine[v] = u;
      adj[u][adj[u].size() - 1] = v;
    }
    if (!tasks_on_machine.empty()) {
      // Make sure we overwrite the fake next task for every task.
      absl::Span<int> adj_last = adj[tasks_on_machine.back()];
      adj_last[adj_last.size() - 1] = num_tasks_;
    }
  }

  // Now run a topological sort and compute the earliest start time of each
  // task by forcing it to be equal the maximal completion time of all its
  // predecessors.
  absl::StatusOr<std::vector<int>> maybe_topo_order =
      util::graph::FastTopologicalSort(adj);
  CHECK_OK(maybe_topo_order);
  analysis->topo_order = std::move(*maybe_topo_order);
  CHECK_EQ(analysis->topo_order.size(), num_tasks_ + 1);
  CHECK_EQ(analysis->topo_order.back(), num_tasks_);
  analysis->topo_order.pop_back();

  // Find the start_min
  analysis->start_mins.assign(num_tasks_, 0);
  for (int i = 0; i < num_tasks_; ++i) {
    analysis->start_mins[i] = problem_.tasks[i].min_start;
  }

  for (int u : analysis->topo_order) {
    if (u == num_tasks_) continue;
    const IntegerValue completion_time_u =
        analysis->start_mins[u] + current_durations[u];
    for (int v : adj[u]) {
      if (v == num_tasks_) continue;
      if (completion_time_u > analysis->start_mins[v]) {
        analysis->start_mins[v] = completion_time_u;
      }
    }
  }

  // Use our calculated EST to find the makespan and the last task.
  int last_task = -1;
  analysis->makespan = -1;
  for (int i = 0; i < num_tasks_; ++i) {
    const IntegerValue comp_time =
        analysis->start_mins[i] + current_durations[i];
    if (comp_time > analysis->makespan) {
      analysis->makespan = comp_time;
      last_task = i;
    }
  }

  // Start from the last task and look for the reason it starts at the current
  // EST, which must be a predecessor that ends at the current task EST.
  int curr = last_task;
  analysis->critical_path.clear();
  analysis->critical_path.reserve(num_tasks_ / num_machines_);

  while (curr != -1) {
    analysis->critical_path.push_back(curr);
    if (analysis->start_mins[curr] == problem_.tasks[curr].min_start) break;

    int tightest_pred = -1;
    for (const int pred :
         problem_.tasks[curr].tasks_that_must_complete_before_this) {
      if (analysis->start_mins[pred] + current_durations[pred] ==
          analysis->start_mins[curr]) {
        tightest_pred = pred;
        break;
      }
    }

    // If no job predecessor is the bottleneck, check the machine predecessor
    if (tightest_pred == -1) {
      int m_pred = prev_on_machine[curr];
      if (m_pred != -1 &&
          analysis->start_mins[m_pred] + current_durations[m_pred] ==
              analysis->start_mins[curr]) {
        tightest_pred = m_pred;
      }
    }
    curr = tightest_pred;
  }

  absl::c_reverse(analysis->critical_path);
}

std::vector<bool> SchedulingLocalSearch::ComputeJobReachability(
    const SchedulingProblem& problem,
    const CompactVectorVector<int>& job_successors) {
  const int num_tasks = problem.tasks.size();
  // Compute static job reachability to prevent cyclic swaps
  std::vector<bool> job_reachability(num_tasks * num_tasks, false);
  std::vector<int> stack;
  stack.reserve(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    stack.assign({i});
    while (!stack.empty()) {
      int curr = stack.back();
      stack.pop_back();
      for (int succ : job_successors[curr]) {
        if (!job_reachability[i * num_tasks + succ]) {
          job_reachability[i * num_tasks + succ] = true;
          stack.push_back(succ);
        }
      }
    }
  }
  return job_reachability;
}

SchedulingLocalSearch::SchedulingLocalSearch(const SchedulingProblem& problem)
    : problem_(problem),
      precedences_(ComputePrecedences(problem)),
      job_successors_(ComputeJobSuccessors(problem)),
      job_reachability_(ComputeJobReachability(problem, job_successors_)),
      num_tasks_(problem_.tasks.size()),
      num_machines_(([](const SchedulingProblem& p) {
        int max_m = -1;
        for (const auto& t : p.tasks) {
          for (int m : t.compatible_machine) {
            max_m = std::max(max_m, m);
          }
        }
        return max_m + 1;
      })(problem_)) {
  CHECK(!problem_.tasks.empty());
  CHECK_GT(num_machines_, 0);
}

SchedulingLocalSearch::ScheduleAnalysis::ScheduleAnalysis(
    const SchedulingProblem& problem) {
  // Build the scratch adjacency list for topological sort.
  std::vector<std::pair<int, int>> adj_edges;
  for (int v = 0; v < problem.tasks.size(); ++v) {
    for (int u : problem.tasks[v].tasks_that_must_complete_before_this) {
      adj_edges.push_back({u, v});
    }
  }
  for (int i = 0; i < problem.tasks.size(); ++i) {
    // Overwritable fake task with index num_tasks_.
    adj_edges.push_back({i, problem.tasks.size()});
  }
  scratch_adjacency_list.ResetFromPairs(adj_edges, problem.tasks.size() + 1);
}

// static
std::vector<std::pair<int, int>> SchedulingLocalSearch::ComputePrecedences(
    const SchedulingProblem& problem) {
  std::vector<std::pair<int, int>> precedences;
  precedences.reserve(problem.tasks.size());
  for (int v = 0; v < problem.tasks.size(); ++v) {
    for (int u : problem.tasks[v].tasks_that_must_complete_before_this) {
      precedences.push_back({u, v});
    }
  }
  return precedences;
}

void SchedulingLocalSearch::ApplySequenceMove(
    const InsertMove& move, const SolverState& state, int current_iteration,
    int tenure, absl::Span<int> tabu_matrix,
    absl::Span<int> machine_seq) const {
  const int x = move.task;
  const int target = move.target_task;

  const int idx_x = state.position_in_machine[x];
  const int idx_target = state.position_in_machine[target];

  // Determine the raw destination index based on placement.
  int insert_idx = idx_target + (move.place_after ? 1 : 0);

  // If the task is moving right, its own removal shifts the target index left.
  if (idx_x < insert_idx) {
    insert_idx--;
  }

  // Execute the insertion in-place via std::rotate.
  if (idx_x < insert_idx) {
    // Moving task to the right.
    // Everything between idx_x+1 and insert_idx shifts left by 1.
    std::rotate(machine_seq.begin() + idx_x, machine_seq.begin() + idx_x + 1,
                machine_seq.begin() + insert_idx + 1);
  } else if (idx_x > insert_idx) {
    // Moving task to the left.
    // Everything between insert_idx and idx_x-1 shifts right by 1.
    std::rotate(machine_seq.begin() + insert_idx, machine_seq.begin() + idx_x,
                machine_seq.begin() + idx_x + 1);
  }

  // Penalize the task jumping back next to its old neighbors.
  const int old_prev = state.prev_on_machine[x];
  const int old_next = state.next_on_machine[x];

  if (old_prev != -1) {
    tabu_matrix[x * num_tasks_ + old_prev] = current_iteration + tenure;
    tabu_matrix[old_prev * num_tasks_ + x] = current_iteration + tenure;
  }
  if (old_next != -1) {
    tabu_matrix[x * num_tasks_ + old_next] = current_iteration + tenure;
    tabu_matrix[old_next * num_tasks_ + x] = current_iteration + tenure;
  }
}

void SchedulingLocalSearch::ApplyAssignmentMove(
    const InsertMove& move, int old_m, int current_iteration, int tenure,
    absl::Span<int> current_machines,
    absl::Span<int> current_active_machine_indices,
    absl::Span<int64_t> current_durations,
    absl::Span<int> assignment_tabu_matrix,
    CompactVectorVector<int>* current_machine_tasks) const {
  const int x = move.task;
  const int target = move.target_task;
  const int new_m = move.new_machine;

  CompactVectorVector<int> next_machine_tasks;
  next_machine_tasks.reserve(num_machines_, num_tasks_);

  for (int m = 0; m < num_machines_; ++m) {
    absl::Span<const int> old_seq = (*current_machine_tasks)[m];

    if (m == old_m) {
      next_machine_tasks.Add({});
      for (const int t : old_seq) {
        if (t != x) next_machine_tasks.AppendToLastVector(t);
      }
    } else if (m == new_m) {
      if (old_seq.empty()) {
        next_machine_tasks.Add({x});
        continue;
      }
      next_machine_tasks.Add({});
      bool inserted = false;
      if (!move.place_after && target == old_seq[0]) {
        next_machine_tasks.AppendToLastVector(x);
        inserted = true;
      }
      for (const int t : old_seq) {
        next_machine_tasks.AppendToLastVector(t);
        if (move.place_after && t == target) {
          next_machine_tasks.AppendToLastVector(x);
          inserted = true;
        }
      }
      if (!inserted) next_machine_tasks.AppendToLastVector(x);
    } else {
      next_machine_tasks.Add(old_seq);
    }
  }

  *current_machine_tasks = std::move(next_machine_tasks);

  current_machines[x] = new_m;
  for (int a = 0; a < problem_.tasks[x].compatible_machine.size(); ++a) {
    if (problem_.tasks[x].compatible_machine[a] == new_m) {
      current_active_machine_indices[x] = a;
      current_durations[x] = problem_.tasks[x].duration_for_machine[a];
      break;
    }
  }

  assignment_tabu_matrix[x * num_machines_ + old_m] =
      current_iteration + tenure;
}

SchedulingLocalSearch::LocalSearchResult SchedulingLocalSearch::Solve(
    absl::Span<const int64_t> initial_hint,
    absl::Span<const int> active_machine_indices, int64_t makespan_to_beat,
    absl::BitGenRef random, TimeLimit* time_limit) const {
  if (num_tasks_ > 40000) {
    // TODO(user): Support huge problems.
    return {};
  }

  std::vector<int> current_machines(num_tasks_);
  std::vector<int64_t> current_durations(num_tasks_);
  std::vector<int> current_active_machine_indices(
      active_machine_indices.begin(), active_machine_indices.end());
  for (int i = 0; i < num_tasks_; ++i) {
    const int a_idx = active_machine_indices[i];
    current_machines[i] = problem_.tasks[i].compatible_machine[a_idx];
    current_durations[i] = problem_.tasks[i].duration_for_machine[a_idx];
  }

  std::vector<int> tabu_matrix(num_tasks_ * num_tasks_, 0);
  std::vector<int> assignment_tabu_matrix(num_tasks_ * num_machines_, 0);

  // Build the initial sequence state from the external hint
  CompactVectorVector<int> current_machine_tasks =
      BuildInitialMachineSequences(initial_hint, current_machines);

  IntegerValue initial_makespan = -1;

  CompactVectorVector<int> best_machine_tasks;
  std::vector<int64_t> best_solution_start_mins(num_tasks_);
  std::vector<int> best_active_machine_indices(active_machine_indices.begin(),
                                               active_machine_indices.end());
  IntegerValue global_best_makespan = kMaxIntegerValue;

  TimeLimitCheckEveryNCalls time_limit_check(100, time_limit);
  int current_iteration = 0;
  const int min_iterations = 10000;
  const int max_iterations = 1000000;
  const int num_jobs = num_tasks_ / num_machines_;
  const int baseline_tenure = 10 + num_jobs / num_machines_;
  const int randomized_tenure = absl::Uniform<int>(
      random, std::max(5, static_cast<int>(baseline_tenure * 0.5)),
      static_cast<int>(baseline_tenure * 1.5) + 1);
  const int tabu_tenure_min = randomized_tenure;
  const int tabu_tenure_max = randomized_tenure + randomized_tenure / 2;
  const int stagnation_limit = absl::Uniform<int>(random, 100, 400);

  ScheduleAnalysis analysis(problem_);

  int iterations_without_improvement = 0;
  for (current_iteration = 0; current_iteration < max_iterations;
       ++current_iteration) {
    if (time_limit_check.LimitReached()) break;

    // Analyze the current sequence.
    AnalyzeSchedule(current_machine_tasks, current_durations, &analysis);

    // Track our absolute best schedule
    if (analysis.makespan < global_best_makespan) {
      if (current_iteration == 0) {
        // Keep the initial makespan for debugging and logging.
        initial_makespan = analysis.makespan;
      }
      global_best_makespan = analysis.makespan;
      best_machine_tasks = current_machine_tasks;
      for (int i = 0; i < num_tasks_; ++i) {
        best_solution_start_mins[i] = analysis.start_mins[i].value();
      }
      best_active_machine_indices = current_active_machine_indices;
      iterations_without_improvement = 0;
    }
    ++iterations_without_improvement;

    if (analysis.makespan < makespan_to_beat &&
        current_iteration > min_iterations) {
      break;
    }

    // Compute dynamic state.
    const SolverState state = ComputeDynamicState(
        current_machine_tasks, analysis.topo_order, current_durations);

    const std::vector<InsertMove> candidates = GenerateMoves(
        analysis.critical_path, state, analysis.start_mins, current_machines,
        current_durations, current_machine_tasks);

    // Select the best move.
    std::optional<InsertMove> best_move = SelectBestMove(
        candidates, analysis.start_mins, state, tabu_matrix,
        assignment_tabu_matrix, current_machines, current_durations,
        current_machine_tasks, current_iteration, global_best_makespan, random);

    if (iterations_without_improvement > stagnation_limit) {
      // To diversify the our tabu search, when the search stagnates, we
      // select a random move from the candidates. The idea is older, but the
      // specific implementation is based on "Xie, J., et al., A new
      // neighborhood structure for job shop scheduling problems. International
      // Journal of Production Research, 1–15 (2022)".
      if (!candidates.empty()) {
        iterations_without_improvement = 0;
        best_move =
            candidates[absl::Uniform<int>(random, 0, candidates.size())];
      }
    }

    if (!best_move.has_value()) break;

    // Update Tabu matrix using a random tenure length
    const int tenure =
        absl::Uniform<int>(random, tabu_tenure_min, tabu_tenure_max + 1);

    // Apply the move directly to the schedule sequence.
    const int x = best_move->task;
    const int old_m = current_machines[x];
    const int new_m = best_move->new_machine;

    if (old_m == new_m) {
      ApplySequenceMove(*best_move, state, current_iteration, tenure,
                        absl::MakeSpan(tabu_matrix),
                        current_machine_tasks[old_m]);
    } else {
      ApplyAssignmentMove(*best_move, old_m, current_iteration, tenure,
                          absl::MakeSpan(current_machines),
                          absl::MakeSpan(current_active_machine_indices),
                          absl::MakeSpan(current_durations),
                          absl::MakeSpan(assignment_tabu_matrix),
                          &current_machine_tasks);
    }
  }
  if (VLOG_IS_ON(2)) {
    AnalyzeSchedule(best_machine_tasks, current_durations, &analysis);
  }
  VLOG(2) << "Initial makespan: " << initial_makespan
          << " best makespan: " << analysis.makespan
          << " num_iterations: " << current_iteration;

  // Return the start times of the best schedule we found.
  return {best_solution_start_mins, best_active_machine_indices};
}

namespace {

std::vector<int64_t> ComputeCpSatSolutionFromSchedulingSolution(
    const SchedulingProblemAndMapping& problem_and_mapping,
    const CpModelProto& input_model_proto,
    absl::Span<const int64_t> scheduling_solution,
    absl::Span<const int> active_machine_indices,
    absl::Span<const int64_t> original_solution) {
  // 1. Copy the entire original solution to inherit non-scheduling variables
  std::vector<int64_t> new_solution(original_solution.begin(),
                                    original_solution.end());

  int64_t new_makespan = 0;

  auto assign_expr = [&](const LinearExpressionProto& expr,
                         int64_t target_val) {
    // The division might not be exact: our definition of SchedulingRelaxation
    // cannot enforce the start times to be a multiple of some coefficient. We
    // might end up with a infeasible solution.
    if (expr.vars().size() == 1) {
      const int var = expr.vars(0);
      const int64_t coeff = expr.coeffs(0);
      const int64_t offset = expr.offset();

      DCHECK_GE(var, 0);
      new_solution[var] = (target_val - offset) / coeff;
    }
  };

  for (int i = 0; i < problem_and_mapping.problem.tasks.size(); ++i) {
    const int64_t new_start = scheduling_solution[i];
    const int active_machine_idx = active_machine_indices[i];

    // 2. Explicitly set all intervals' optional presence booleans
    // based on the dynamically provided machine choices.
    const SchedulingProblem::Task& task = problem_and_mapping.problem.tasks[i];
    for (int a = 0; a < task.compatible_machine.size(); ++a) {
      const int lit = problem_and_mapping.task_to_presence_literals[i][a];
      if (lit == kint32max) continue;

      const bool should_be_true = (a == active_machine_idx);
      if (RefIsPositive(lit)) {
        new_solution[lit] = should_be_true ? 1 : 0;
      } else {
        new_solution[NegatedRef(lit)] = should_be_true ? 0 : 1;
      }
    }

    const int64_t duration = problem_and_mapping.problem.tasks[i]
                                 .duration_for_machine[active_machine_idx];
    const int64_t new_end = new_start + duration;
    new_makespan = std::max(new_makespan, new_end);

    // 3. Set interval start, end, and size variables
    const auto& task_intervals = problem_and_mapping.task_to_intervals[i];

    // 3a. Repair the specific active alternative interval
    if (active_machine_idx < task_intervals.alternative_intervals.size()) {
      const int active_interval_idx =
          task_intervals.alternative_intervals[active_machine_idx];
      const auto& interval =
          input_model_proto.constraints(active_interval_idx).interval();

      assign_expr(interval.start(), new_start);
      assign_expr(interval.end(), new_end);
      assign_expr(interval.size(), duration);
    }

    // 3b. Repair all main/shared intervals
    for (const int main_idx : task_intervals.unconditional_intervals) {
      const auto& interval = input_model_proto.constraints(main_idx).interval();

      assign_expr(interval.start(), new_start);
      assign_expr(interval.end(), new_end);
      assign_expr(interval.size(), duration);
    }
  }

  // 4. Update Makespan if present
  if (problem_and_mapping.makespan_expr.has_value()) {
    const auto& [makespan_var, coeff, offset] =
        problem_and_mapping.makespan_expr.value();

    DCHECK_GE(makespan_var, 0);
    new_solution[makespan_var] = (new_makespan - offset) / coeff;
  }

  return new_solution;
}

}  // namespace

SchedulingLocalSearchSolver::SchedulingLocalSearchSolver(
    const absl::string_view name, SubSolver::SubsolverType type,
    const CpModelProto& input_model_proto, const SatParameters& params,
    ModelSharedTimeLimit* shared_time_limit,
    SharedResponseManager* shared_response, SharedStatTables* stat_tables)
    : SubSolver(name, type),
      input_model_proto_(input_model_proto),
      params_(params),
      shared_time_limit_(shared_time_limit),
      shared_response_(shared_response),
      stat_tables_(stat_tables) {
  relaxation_ = DetectSchedulingProblems(input_model_proto_);
  for (const auto& problem_and_mapping : relaxation_.problems_and_mappings) {
    local_search_solvers_.emplace_back(
        std::make_unique<SchedulingLocalSearch>(problem_and_mapping.problem));
  }
}

std::function<void()> SchedulingLocalSearchSolver::GenerateTask(
    int64_t task_id) {
  return [this, task_id]() {
    if (relaxation_.problems_and_mappings.empty()) return;
    TimeLimit task_time_limit;
    shared_time_limit_->UpdateLocalLimit(&task_time_limit);
    // Create a random number generator whose seed depends both on the task_id
    // and on the params_.random_seed() so that changing the later will
    // change the LNS behavior.
    const int32_t low = static_cast<int32_t>(task_id);
    const int32_t high = static_cast<int32_t>(task_id >> 32);
    std::seed_seq seed{low, high, params_.random_seed()};
    random_engine_t random(seed);
    const int problem_index =
        absl::Uniform<int>(random, 0, relaxation_.problems_and_mappings.size());
    if (relaxation_.problems_and_mappings[problem_index].problem.tasks.size() <
        3)
      return;
    const SchedulingProblemAndMapping& problem_and_mapping =
        relaxation_.problems_and_mappings[problem_index];
    const auto base_solution =
        shared_response_->SolutionPool().GetSolutionToImprove(random);
    if (base_solution == nullptr) return;

    std::vector<int64_t> scheduling_solution;
    scheduling_solution.reserve(
        problem_and_mapping.task_to_start_time_model_var.size());
    for (int i = 0; i < problem_and_mapping.task_to_start_time_model_var.size();
         ++i) {
      const auto& [var, coeff, offset] =
          problem_and_mapping.task_to_start_time_model_var[i];
      scheduling_solution.push_back(
          base_solution->variable_values[var] * coeff + offset);
    }

    std::vector<int> active_machine_indices(
        problem_and_mapping.problem.tasks.size(), 0);
    for (int i = 0; i < problem_and_mapping.problem.tasks.size(); ++i) {
      const SchedulingProblem::Task& task =
          problem_and_mapping.problem.tasks[i];
      int active_idx = 0;
      for (int a = 0; a < task.compatible_machine.size(); ++a) {
        int lit = problem_and_mapping.task_to_presence_literals[i][a];
        if (lit == kint32max) {
          // A single machine choice, no enforcement needed.
          active_idx = a;
          break;
        }
        const bool is_true =
            (RefIsPositive(lit))
                ? (base_solution->variable_values[lit] == 1)
                : (base_solution->variable_values[NegatedRef(lit)] == 0);
        if (is_true) {
          active_idx = a;
          break;
        }
      }
      active_machine_indices[i] = active_idx;
    }

    int64_t relaxed_objective_value = 0;

    DCHECK(VerifySchedulingRelaxation(
        relaxation_, base_solution->variable_values, &relaxed_objective_value));
    DCHECK_LE(relaxed_objective_value,
              ComputeInnerObjective(input_model_proto_.objective(),
                                    base_solution->variable_values));
    SchedulingLocalSearch& local_search_solver =
        *local_search_solvers_[problem_index];

    const SchedulingLocalSearch::LocalSearchResult result =
        local_search_solver.Solve(
            absl::MakeConstSpan(scheduling_solution),
            absl::MakeConstSpan(active_machine_indices),
            shared_response_->BestSolutionInnerObjectiveValue().value(), random,
            &task_time_limit);
    if (result.start_mins.empty()) return;

    std::vector<int64_t> new_solution =
        ComputeCpSatSolutionFromSchedulingSolution(
            problem_and_mapping, input_model_proto_, result.start_mins,
            result.active_machine_indices, base_solution->variable_values);

    if (SolutionIsFeasible(input_model_proto_, new_solution)) {
      const int64_t new_objective_value =
          ComputeInnerObjective(input_model_proto_.objective(), new_solution);
      VLOG(2) << "New solution is feasible, objective value: "
              << new_objective_value << " best: "
              << shared_response_->BestSolutionInnerObjectiveValue();
      PushAndMaybeCombineSolution(shared_response_, input_model_proto_,
                                  new_solution, this->name(), base_solution);
    } else {
      // TODO(user): try to fix it with ViolationLS.
      VLOG(2) << "New solution is infeasible";
    }
  };
}

}  // namespace sat
}  // namespace operations_research
