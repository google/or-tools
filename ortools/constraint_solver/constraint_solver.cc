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

// This file implements the core objects of the constraint solver:
// Solver, Search, Queue, ... along with the main resolution loop.

#include "ortools/constraint_solver/constraint_solver.h"

#include <algorithm>
#include <csetjmp>
#include <cstdint>
#include <deque>
#include <iosfwd>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/local_search.h"
#include "ortools/constraint_solver/model_cache.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/constraint_solver/sequence_var.h"
#include "ortools/constraint_solver/trace.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/port/sysinfo.h"

// These flags are used to set the fields in the DefaultSolverParameters proto.
ABSL_FLAG(bool, cp_trace_propagation, false,
          "Trace propagation events (constraint and demon executions,"
          " variable modifications).");
ABSL_FLAG(bool, cp_trace_search, false, "Trace search events");
ABSL_FLAG(bool, cp_print_added_constraints, false,
          "show all constraints added to the solver.");
ABSL_FLAG(bool, cp_print_model, false,
          "use PrintModelVisitor on model before solving.");
ABSL_FLAG(bool, cp_model_stats, false,
          "use StatisticsModelVisitor on model before solving.");
ABSL_FLAG(bool, cp_disable_solve, false,
          "Force failure at the beginning of a search.");
ABSL_FLAG(bool, cp_print_local_search_profile, false,
          "Print local search profiling data after solving.");
ABSL_FLAG(bool, cp_name_variables, false, "Force all variables to have names.");
ABSL_FLAG(bool, cp_name_cast_variables, false,
          "Name variables casted from expressions");
ABSL_FLAG(bool, cp_use_small_table, true,
          "Use small compact table constraint when possible.");
ABSL_FLAG(bool, cp_use_cumulative_edge_finder, true,
          "Use the O(n log n) cumulative edge finding algorithm described "
          "in 'Edge Finding Filtering Algorithm for Discrete  Cumulative "
          "Resources in O(kn log n)' by Petr Vilim, CP 2009.");
ABSL_FLAG(bool, cp_use_cumulative_time_table, true,
          "Use a O(n^2) cumulative time table propagation algorithm.");
ABSL_FLAG(bool, cp_use_cumulative_time_table_sync, false,
          "Use a synchronized O(n^2 log n) cumulative time table propagation "
          "algorithm.");
ABSL_FLAG(bool, cp_use_sequence_high_demand_tasks, true,
          "Use a sequence constraints for cumulative tasks that have a "
          "demand greater than half of the capacity of the resource.");
ABSL_FLAG(bool, cp_use_all_possible_disjunctions, true,
          "Post temporal disjunctions for all pairs of tasks sharing a "
          "cumulative resource and that cannot overlap because the sum of "
          "their demand exceeds the capacity.");
ABSL_FLAG(int, cp_max_edge_finder_size, 50,
          "Do not post the edge finder in the cumulative constraints if "
          "it contains more than this number of tasks");
ABSL_FLAG(bool, cp_diffn_use_cumulative, true,
          "Diffn constraint adds redundant cumulative constraint");
ABSL_FLAG(bool, cp_use_element_rmq, true,
          "If true, rmq's will be used in element expressions.");
ABSL_FLAG(int, cp_check_solution_period, 1,
          "Number of solutions explored between two solution checks during "
          "local search.");
ABSL_FLAG(int64_t, cp_random_seed, 12345,
          "Random seed used in several (but not all) random number "
          "generators used by the CP solver. Use -1 to auto-generate an"
          "undeterministic random seed.");
ABSL_FLAG(bool, cp_disable_element_cache, true,
          "If true, caching for IntElement is disabled.");
ABSL_FLAG(bool, cp_disable_expression_optimization, false,
          "Disable special optimization when creating expressions.");
ABSL_FLAG(bool, cp_share_int_consts, true,
          "Share IntConst's with the same value.");

void ConstraintSolverFailsHere() { VLOG(3) << "Fail"; }

#if defined(_MSC_VER)  // WINDOWS
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

namespace {
// Converts a scoped enum to its underlying type.
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) {
  return static_cast<typename std::underlying_type<E>::type>(e);
}

}  // namespace

// ----- ConstraintSolverParameters -----

ConstraintSolverParameters Solver::DefaultSolverParameters() {
  ConstraintSolverParameters params;
  params.set_compress_trail(ConstraintSolverParameters::NO_COMPRESSION);
  params.set_trail_block_size(8000);
  params.set_array_split_size(16);
  params.set_store_names(true);
  params.set_trace_propagation(absl::GetFlag(FLAGS_cp_trace_propagation));
  params.set_trace_search(absl::GetFlag(FLAGS_cp_trace_search));
  params.set_name_all_variables(absl::GetFlag(FLAGS_cp_name_variables));
  params.set_profile_local_search(
      absl::GetFlag(FLAGS_cp_print_local_search_profile));
  params.set_print_local_search_profile(
      absl::GetFlag(FLAGS_cp_print_local_search_profile));
  params.set_print_model(absl::GetFlag(FLAGS_cp_print_model));
  params.set_print_model_stats(absl::GetFlag(FLAGS_cp_model_stats));
  params.set_disable_solve(absl::GetFlag(FLAGS_cp_disable_solve));
  params.set_name_cast_variables(absl::GetFlag(FLAGS_cp_name_cast_variables));
  params.set_print_added_constraints(
      absl::GetFlag(FLAGS_cp_print_added_constraints));
  params.set_use_small_table(absl::GetFlag(FLAGS_cp_use_small_table));
  params.set_use_cumulative_edge_finder(
      absl::GetFlag(FLAGS_cp_use_cumulative_edge_finder));
  params.set_use_cumulative_time_table(
      absl::GetFlag(FLAGS_cp_use_cumulative_time_table));
  params.set_use_cumulative_time_table_sync(
      absl::GetFlag(FLAGS_cp_use_cumulative_time_table_sync));
  params.set_use_sequence_high_demand_tasks(
      absl::GetFlag(FLAGS_cp_use_sequence_high_demand_tasks));
  params.set_use_all_possible_disjunctions(
      absl::GetFlag(FLAGS_cp_use_all_possible_disjunctions));
  params.set_max_edge_finder_size(absl::GetFlag(FLAGS_cp_max_edge_finder_size));
  params.set_diffn_use_cumulative(absl::GetFlag(FLAGS_cp_diffn_use_cumulative));
  params.set_use_element_rmq(absl::GetFlag(FLAGS_cp_use_element_rmq));
  params.set_check_solution_period(
      absl::GetFlag(FLAGS_cp_check_solution_period));
  return params;
}

ModelCache* Solver::Cache() const { return model_cache_.get(); }

// ----- Forward Declarations and Profiling Support -----
extern void InstallLocalSearchProfiler(LocalSearchProfiler* monitor);
extern LocalSearchProfiler* BuildLocalSearchProfiler(Solver* solver);
extern void DeleteLocalSearchProfiler(LocalSearchProfiler* monitor);

// TODO(user): remove this complex logic.
// We need the double test because parameters are set too late when using
// python in the open source. This is the cheapest work-around.
bool Solver::InstrumentsDemons() const { return InstrumentsVariables(); }

bool Solver::IsLocalSearchProfilingEnabled() const {
  return parameters_.profile_local_search() ||
         parameters_.print_local_search_profile();
}

bool Solver::InstrumentsVariables() const {
  return parameters_.trace_propagation();
}

bool Solver::NameAllVariables() const {
  return parameters_.name_all_variables();
}

// ------------------ Demon class ----------------

Solver::DemonPriority Demon::priority() const {
  return Solver::NORMAL_PRIORITY;
}

std::string Demon::DebugString() const { return "Demon"; }

void Demon::inhibit(Solver* s) {
  if (stamp_ < std::numeric_limits<uint64_t>::max()) {
    s->SaveAndSetValue(&stamp_, std::numeric_limits<uint64_t>::max());
  }
}

void Demon::desinhibit(Solver* s) {
  if (stamp_ == std::numeric_limits<uint64_t>::max()) {
    s->SaveAndSetValue(&stamp_, s->stamp() - 1);
  }
}

// ------------------ Queue class ------------------

extern void CleanVariableOnFail(IntVar* var);

class Queue {
 public:
  static constexpr int64_t kTestPeriod = 10000;

  explicit Queue(Solver* s)
      : solver_(s),
        stamp_(s->stamp_ref()),
        freeze_level_(0),
        in_process_(false),
        clean_action_(nullptr),
        clean_variable_(nullptr),
        in_add_(false),
        instruments_demons_(s->InstrumentsDemons()) {}

  ~Queue() {}

  void Freeze() {
    freeze_level_++;
    stamp_++;
  }

  void Unfreeze() {
    if (--freeze_level_ == 0) {
      Process();
    }
  }

  void ProcessOneDemon(Demon* demon) {
    demon->set_stamp(stamp_ - 1);
    if (!instruments_demons_) {
      if (++solver_->demon_runs_[demon->priority()] % kTestPeriod == 0) {
        solver_->TopPeriodicCheck();
      }
      demon->Run(solver_);
      solver_->CheckFail();
    } else {
      solver_->GetPropagationMonitor()->BeginDemonRun(demon);
      if (++solver_->demon_runs_[demon->priority()] % kTestPeriod == 0) {
        solver_->TopPeriodicCheck();
      }
      demon->Run(solver_);
      solver_->CheckFail();
      solver_->GetPropagationMonitor()->EndDemonRun(demon);
    }
  }

  void Process() {
    if (!in_process_) {
      in_process_ = true;
      while (!var_queue_.empty() || !delayed_queue_.empty()) {
        if (!var_queue_.empty()) {
          Demon* const demon = var_queue_.front();
          var_queue_.pop_front();
          ProcessOneDemon(demon);
        } else {
          DCHECK(!delayed_queue_.empty());
          Demon* const demon = delayed_queue_.front();
          delayed_queue_.pop_front();
          ProcessOneDemon(demon);
        }
      }
      in_process_ = false;
    }
  }

  void ExecuteAll(const SimpleRevFIFO<Demon*>& demons) {
    if (!instruments_demons_) {
      for (SimpleRevFIFO<Demon*>::Iterator it(&demons); it.ok(); ++it) {
        Demon* const demon = *it;
        if (demon->stamp() < stamp_) {
          DCHECK_EQ(demon->priority(), Solver::NORMAL_PRIORITY);
          if (++solver_->demon_runs_[Solver::NORMAL_PRIORITY] % kTestPeriod ==
              0) {
            solver_->TopPeriodicCheck();
          }
          demon->Run(solver_);
          solver_->CheckFail();
        }
      }
    } else {
      for (SimpleRevFIFO<Demon*>::Iterator it(&demons); it.ok(); ++it) {
        Demon* const demon = *it;
        if (demon->stamp() < stamp_) {
          DCHECK_EQ(demon->priority(), Solver::NORMAL_PRIORITY);
          solver_->GetPropagationMonitor()->BeginDemonRun(demon);
          if (++solver_->demon_runs_[Solver::NORMAL_PRIORITY] % kTestPeriod ==
              0) {
            solver_->TopPeriodicCheck();
          }
          demon->Run(solver_);
          solver_->CheckFail();
          solver_->GetPropagationMonitor()->EndDemonRun(demon);
        }
      }
    }
  }

  void EnqueueAll(const SimpleRevFIFO<Demon*>& demons) {
    for (SimpleRevFIFO<Demon*>::Iterator it(&demons); it.ok(); ++it) {
      EnqueueDelayedDemon(*it);
    }
  }

  void EnqueueVar(Demon* demon) {
    DCHECK(demon->priority() == Solver::VAR_PRIORITY);
    if (demon->stamp() < stamp_) {
      demon->set_stamp(stamp_);
      var_queue_.push_back(demon);
      if (freeze_level_ == 0) {
        Process();
      }
    }
  }

  void EnqueueDelayedDemon(Demon* demon) {
    DCHECK(demon->priority() == Solver::DELAYED_PRIORITY);
    if (demon->stamp() < stamp_) {
      demon->set_stamp(stamp_);
      delayed_queue_.push_back(demon);
    }
  }

  void AfterFailure() {
    // Clean queue.
    var_queue_.clear();
    delayed_queue_.clear();

    // Call cleaning actions on variables.
    if (clean_action_ != nullptr) {
      clean_action_(solver_);
      clean_action_ = nullptr;
    } else if (clean_variable_ != nullptr) {
      CleanVariableOnFail(clean_variable_);
      clean_variable_ = nullptr;
    }

    freeze_level_ = 0;
    in_process_ = false;
    in_add_ = false;
    to_add_.clear();
  }

  void increase_stamp() { stamp_++; }

  uint64_t stamp() const { return stamp_; }

  void set_action_on_fail(Solver::Action a) {
    DCHECK(clean_variable_ == nullptr);
    clean_action_ = std::move(a);
  }

  void set_variable_to_clean_on_fail(IntVar* var) {
    DCHECK(clean_action_ == nullptr);
    clean_variable_ = var;
  }

  void reset_action_on_fail() {
    DCHECK(clean_variable_ == nullptr);
    clean_action_ = nullptr;
  }

  void AddConstraint(Constraint* c) {
    to_add_.push_back(c);
    ProcessConstraints();
  }

  void ProcessConstraints() {
    if (!in_add_) {
      in_add_ = true;
      // We cannot store to_add_.size() as constraints can add other
      // constraints. For the same reason a range-based for loop cannot be used.
      // TODO(user): Make to_add_ a queue to make the behavior more obvious.
      for (int counter = 0; counter < to_add_.size(); ++counter) {
        Constraint* const constraint = to_add_[counter];
        // TODO(user): Add profiling to initial propagation
        constraint->PostAndPropagate();
      }
      in_add_ = false;
      to_add_.clear();
    }
  }

 private:
  Solver* const solver_;
  std::deque<Demon*> var_queue_;
  std::deque<Demon*> delayed_queue_;
  uint64_t& stamp_;
  // The number of nested freeze levels. The queue is frozen if and only if
  // freeze_level_ > 0.
  uint32_t freeze_level_;
  bool in_process_;
  Solver::Action clean_action_;
  IntVar* clean_variable_;
  std::vector<Constraint*> to_add_;
  bool in_add_;
  const bool instruments_demons_;
};

// ------------------ Search class -----------------

class Search {
 public:
  explicit Search(Solver* s)
      : solver_(s),
        marker_stack_(),
        monitor_event_listeners_(to_underlying(Solver::MonitorEvent::kLast)),
        fail_buffer_(),
        solution_counter_(0),
        unchecked_solution_counter_(0),
        decision_builder_(nullptr),
        created_by_solve_(false),
        search_depth_(0),
        left_search_depth_(0),
        should_restart_(false),
        should_finish_(false),
        sentinel_pushed_(0),
        jmpbuf_filled_(false),
        backtrack_at_the_end_of_the_search_(true) {}

  // Constructor for a dummy search. The only difference between a dummy search
  // and a regular one is that the search depth and left search depth is
  // initialized to -1 instead of zero.
  Search(Solver* s, int /* dummy_argument */)
      : solver_(s),
        marker_stack_(),
        monitor_event_listeners_(to_underlying(Solver::MonitorEvent::kLast)),
        fail_buffer_(),
        solution_counter_(0),
        unchecked_solution_counter_(0),
        decision_builder_(nullptr),
        created_by_solve_(false),
        search_depth_(-1),
        left_search_depth_(-1),
        should_restart_(false),
        should_finish_(false),
        sentinel_pushed_(0),
        jmpbuf_filled_(false),
        backtrack_at_the_end_of_the_search_(true) {}

  ~Search() { gtl::STLDeleteElements(&marker_stack_); }

  void EnterSearch();
  void RestartSearch();
  void ExitSearch();
  void BeginNextDecision(DecisionBuilder* db);
  void EndNextDecision(DecisionBuilder* db, Decision* d);
  void ApplyDecision(Decision* d);
  void AfterDecision(Decision* d, bool apply);
  void RefuteDecision(Decision* d);
  void BeginFail();
  void EndFail();
  void BeginInitialPropagation();
  void EndInitialPropagation();
  bool AtSolution();
  bool AcceptSolution();
  void NoMoreSolutions();
  bool ContinueAtLocalOptimum();
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta);
  void AcceptNeighbor();
  void AcceptUncheckedNeighbor();
  bool IsUncheckedSolutionLimitReached();
  void PeriodicCheck();
  int ProgressPercent();
  void Accept(ModelVisitor* visitor) const;
  void AddEventListener(Solver::MonitorEvent event, SearchMonitor* monitor) {
    if (monitor != nullptr) {
      monitor_event_listeners_[to_underlying(event)].push_back(monitor);
    }
  }
  const std::vector<SearchMonitor*>& GetEventListeners(
      Solver::MonitorEvent event) const {
    return monitor_event_listeners_[to_underlying(event)];
  }
  void Clear();
  void IncrementSolutionCounter() { ++solution_counter_; }
  int64_t solution_counter() const { return solution_counter_; }
  void IncrementUncheckedSolutionCounter() { ++unchecked_solution_counter_; }
  int64_t unchecked_solution_counter() const {
    return unchecked_solution_counter_;
  }
  void set_decision_builder(DecisionBuilder* db) { decision_builder_ = db; }
  DecisionBuilder* decision_builder() const { return decision_builder_; }
  void set_created_by_solve(bool c) { created_by_solve_ = c; }
  bool created_by_solve() const { return created_by_solve_; }
  Solver::DecisionModification ModifyDecision();
  void SetBranchSelector(Solver::BranchSelector bs);
  void LeftMove() {
    search_depth_++;
    left_search_depth_++;
  }
  void RightMove() { search_depth_++; }
  bool backtrack_at_the_end_of_the_search() const {
    return backtrack_at_the_end_of_the_search_;
  }
  void set_backtrack_at_the_end_of_the_search(bool restore) {
    backtrack_at_the_end_of_the_search_ = restore;
  }
  int search_depth() const { return search_depth_; }
  void set_search_depth(int d) { search_depth_ = d; }
  int left_search_depth() const { return left_search_depth_; }
  void set_search_left_depth(int d) { left_search_depth_ = d; }
  void set_should_restart(bool s) { should_restart_ = s; }
  bool should_restart() const { return should_restart_; }
  void set_should_finish(bool s) { should_finish_ = s; }
  bool should_finish() const { return should_finish_; }
  void CheckFail() {
    if (should_finish_ || should_restart_) {
      solver_->Fail();
    }
  }
  void set_search_context(absl::string_view search_context) {
    search_context_ = search_context;
  }
  std::string search_context() const { return search_context_; }
  friend class Solver;

 private:
  // Jumps back to the previous choice point, Checks if it was correctly set.
  void JumpBack();
  void ClearBuffer() {
    CHECK(jmpbuf_filled_) << "Internal error in backtracking";
    jmpbuf_filled_ = false;
  }

  Solver* const solver_;
  std::vector<StateMarker*> marker_stack_;
  std::vector<std::vector<SearchMonitor*>> monitor_event_listeners_;
  jmp_buf fail_buffer_;
  int64_t solution_counter_;
  int64_t unchecked_solution_counter_;
  DecisionBuilder* decision_builder_;
  bool created_by_solve_;
  Solver::BranchSelector selector_;
  int search_depth_;
  int left_search_depth_;
  bool should_restart_;
  bool should_finish_;
  int sentinel_pushed_;
  bool jmpbuf_filled_;
  bool backtrack_at_the_end_of_the_search_;
  std::string search_context_;
};

// Backtrack is implemented using 3 primitives:
// CP_TRY to start searching
// CP_DO_FAIL to signal a failure. The program will continue on the CP_ON_FAIL
// primitive.
// Implementation of backtrack using setjmp/longjmp.
// The clean portable way is to use exceptions, unfortunately, it can be much
// slower.  Thus we use ideas from Prolog, CP/CLP implementations,
// continuations in C and implement the default failing and backtracking
// using setjmp/longjmp. You can still use exceptions by defining
// CP_USE_EXCEPTIONS_FOR_BACKTRACK
#ifndef CP_USE_EXCEPTIONS_FOR_BACKTRACK
// We cannot use a method/function for this as we would lose the
// context in the setjmp implementation.
#define CP_TRY(search)                                              \
  CHECK(!search->jmpbuf_filled_) << "Fail() called outside search"; \
  search->jmpbuf_filled_ = true;                                    \
  if (setjmp(search->fail_buffer_) == 0)
#define CP_ON_FAIL else
#define CP_DO_FAIL(search) longjmp(search->fail_buffer_, 1)
#else  // CP_USE_EXCEPTIONS_FOR_BACKTRACK
class FailException {};
#define CP_TRY(search)                                              \
  CHECK(!search->jmpbuf_filled_) << "Fail() called outside search"; \
  search->jmpbuf_filled_ = true;                                    \
  try
#define CP_ON_FAIL catch (FailException&)
#define CP_DO_FAIL(search) throw FailException()
#endif  // CP_USE_EXCEPTIONS_FOR_BACKTRACK

void Search::JumpBack() {
  if (jmpbuf_filled_) {
    jmpbuf_filled_ = false;
    CP_DO_FAIL(this);
  } else {
    std::string explanation = "Failure outside of search";
    solver_->AddConstraint(solver_->MakeFalseConstraint(explanation));
  }
}

Search* Solver::ActiveSearch() const { return searches_.back(); }

namespace {
class ApplyBranchSelector : public DecisionBuilder {
 public:
  explicit ApplyBranchSelector(Solver::BranchSelector bs)
      : selector_(std::move(bs)) {}
  ~ApplyBranchSelector() override {}

  Decision* Next(Solver* s) override {
    s->SetBranchSelector(selector_);
    return nullptr;
  }

  std::string DebugString() const override { return "Apply(BranchSelector)"; }

 private:
  Solver::BranchSelector const selector_;
};
}  // namespace

void Search::SetBranchSelector(Solver::BranchSelector bs) {
  selector_ = std::move(bs);
}

void Solver::SetBranchSelector(BranchSelector bs) {
  // We cannot use the trail as the search can be nested and thus
  // deleted upon backtrack. Thus we guard the undo action by a
  // check on the number of nesting of solve().
  const int solve_depth = SolveDepth();
  AddBacktrackAction(
      [solve_depth](Solver* s) {
        if (s->SolveDepth() == solve_depth) {
          s->ActiveSearch()->SetBranchSelector(nullptr);
        }
      },
      false);
  searches_.back()->SetBranchSelector(std::move(bs));
}

DecisionBuilder* Solver::MakeApplyBranchSelector(BranchSelector bs) {
  return RevAlloc(new ApplyBranchSelector(std::move(bs)));
}

int Solver::SolveDepth() const {
  return state_ == OUTSIDE_SEARCH ? 0 : searches_.size() - 1;
}

int Solver::SearchDepth() const { return searches_.back()->search_depth(); }

int Solver::SearchLeftDepth() const {
  return searches_.back()->left_search_depth();
}

Solver::DecisionModification Search::ModifyDecision() {
  if (selector_ != nullptr) {
    return selector_();
  }
  return Solver::NO_CHANGE;
}

void Search::Clear() {
  for (auto& listeners : monitor_event_listeners_) listeners.clear();
  search_depth_ = 0;
  left_search_depth_ = 0;
  selector_ = nullptr;
  backtrack_at_the_end_of_the_search_ = true;
}

#define CALL_EVENT_LISTENERS(Event)                           \
  do {                                                        \
    ForAll(GetEventListeners(Solver::MonitorEvent::k##Event), \
           &SearchMonitor::Event);                            \
  } while (false)

void Search::EnterSearch() {
  // The solution counter is reset when entering search and not when
  // leaving search. This enables the information to persist outside of
  // top-level search.
  solution_counter_ = 0;
  unchecked_solution_counter_ = 0;

  CALL_EVENT_LISTENERS(EnterSearch);
}

void Search::ExitSearch() {
  // Backtrack to the correct state.
  CALL_EVENT_LISTENERS(ExitSearch);
}

void Search::RestartSearch() { CALL_EVENT_LISTENERS(RestartSearch); }

void Search::BeginNextDecision(DecisionBuilder* db) {
  ForAll(GetEventListeners(Solver::MonitorEvent::kBeginNextDecision),
         &SearchMonitor::BeginNextDecision, db);
  CheckFail();
}

void Search::EndNextDecision(DecisionBuilder* db, Decision* d) {
  ForAll(GetEventListeners(Solver::MonitorEvent::kEndNextDecision),
         &SearchMonitor::EndNextDecision, db, d);
  CheckFail();
}

void Search::ApplyDecision(Decision* d) {
  ForAll(GetEventListeners(Solver::MonitorEvent::kApplyDecision),
         &SearchMonitor::ApplyDecision, d);
  CheckFail();
}

void Search::AfterDecision(Decision* d, bool apply) {
  ForAll(GetEventListeners(Solver::MonitorEvent::kAfterDecision),
         &SearchMonitor::AfterDecision, d, apply);
  CheckFail();
}

void Search::RefuteDecision(Decision* d) {
  ForAll(GetEventListeners(Solver::MonitorEvent::kRefuteDecision),
         &SearchMonitor::RefuteDecision, d);
  CheckFail();
}

void Search::BeginFail() { CALL_EVENT_LISTENERS(BeginFail); }

void Search::EndFail() { CALL_EVENT_LISTENERS(EndFail); }

void Search::BeginInitialPropagation() {
  CALL_EVENT_LISTENERS(BeginInitialPropagation);
}

void Search::EndInitialPropagation() {
  CALL_EVENT_LISTENERS(EndInitialPropagation);
}

bool Search::AcceptSolution() {
  bool valid = true;
  for (SearchMonitor* const monitor :
       GetEventListeners(Solver::MonitorEvent::kAcceptSolution)) {
    if (!monitor->AcceptSolution()) {
      // Even though we know the return value, we cannot return yet: this would
      // break the contract we have with solution monitors. They all deserve
      // a chance to look at the solution.
      valid = false;
    }
  }
  return valid;
}

bool Search::AtSolution() {
  bool should_continue = false;
  for (SearchMonitor* const monitor :
       GetEventListeners(Solver::MonitorEvent::kAtSolution)) {
    if (monitor->AtSolution()) {
      // Even though we know the return value, we cannot return yet: this would
      // break the contract we have with solution monitors. They all deserve
      // a chance to look at the solution.
      should_continue = true;
    }
  }
  return should_continue;
}

void Search::NoMoreSolutions() { CALL_EVENT_LISTENERS(NoMoreSolutions); }

bool Search::ContinueAtLocalOptimum() {
  bool continue_at_local_optimum = false;
  for (SearchMonitor* const monitor :
       GetEventListeners(Solver::MonitorEvent::kLocalOptimum)) {
    if (monitor->AtLocalOptimum()) {
      continue_at_local_optimum = true;
    }
  }
  return continue_at_local_optimum;
}

bool Search::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  bool accept = true;
  for (SearchMonitor* const monitor :
       GetEventListeners(Solver::MonitorEvent::kAcceptDelta)) {
    if (!monitor->AcceptDelta(delta, deltadelta)) {
      accept = false;
    }
  }
  return accept;
}

void Search::AcceptNeighbor() { CALL_EVENT_LISTENERS(AcceptNeighbor); }

void Search::AcceptUncheckedNeighbor() {
  CALL_EVENT_LISTENERS(AcceptUncheckedNeighbor);
}

bool Search::IsUncheckedSolutionLimitReached() {
  for (SearchMonitor* const monitor : GetEventListeners(
           Solver::MonitorEvent::kIsUncheckedSolutionLimitReached)) {
    if (monitor->IsUncheckedSolutionLimitReached()) {
      return true;
    }
  }
  return false;
}

void Search::PeriodicCheck() { CALL_EVENT_LISTENERS(PeriodicCheck); }

int Search::ProgressPercent() {
  int progress = SearchMonitor::kNoProgress;
  for (SearchMonitor* const monitor :
       GetEventListeners(Solver::MonitorEvent::kProgressPercent)) {
    progress = std::max(progress, monitor->ProgressPercent());
  }
  return progress;
}

void Search::Accept(ModelVisitor* const visitor) const {
  ForAll(GetEventListeners(Solver::MonitorEvent::kAccept),
         &SearchMonitor::Accept, visitor);
  if (decision_builder_ != nullptr) {
    decision_builder_->Accept(visitor);
  }
}

#undef CALL_EVENT_LISTENERS

bool ContinueAtLocalOptimum(Search* search) {
  return search->ContinueAtLocalOptimum();
}

bool AcceptDelta(Search* search, Assignment* delta, Assignment* deltadelta) {
  return search->AcceptDelta(delta, deltadelta);
}

void AcceptNeighbor(Search* search) { search->AcceptNeighbor(); }

void AcceptUncheckedNeighbor(Search* search) {
  search->AcceptUncheckedNeighbor();
}

namespace {

// ---------- Fail Decision ----------

class FailDecision : public Decision {
 public:
  void Apply(Solver* s) override { s->Fail(); }
  void Refute(Solver* s) override { s->Fail(); }
};

// Balancing decision

class BalancingDecision : public Decision {
 public:
  ~BalancingDecision() override {}
  void Apply(Solver* /*s*/) override {}
  void Refute(Solver* /*s*/) override {}
};
}  // namespace

Decision* Solver::MakeFailDecision() { return fail_decision_.get(); }

// ------------------ Solver class -----------------

extern PropagationMonitor* BuildTrace(Solver* s);
extern LocalSearchMonitor* BuildLocalSearchMonitorPrimary(Solver* s);

std::string Solver::model_name() const { return name_; }

namespace {
void CheckSolverParameters(const ConstraintSolverParameters& parameters) {
  CHECK_GT(parameters.array_split_size(), 0)
      << "Were parameters built using Solver::DefaultSolverParameters() ?";
}

// These magic numbers are there to make sure we pop the correct
// sentinels throughout the search.
enum SentinelMarker {
  INITIAL_SEARCH_SENTINEL = 10000000,
  ROOT_NODE_SENTINEL = 20000000,
  SOLVER_CTOR_SENTINEL = 40000000
};

}  // namespace

Solver::Solver(const std::string& name,
               const ConstraintSolverParameters& parameters)
    : name_(name),
      parameters_(parameters),
      random_(CpRandomSeed()),

      use_fast_local_search_(true),
      local_search_profiler_(BuildLocalSearchProfiler(this)) {
  Init();
}

Solver::Solver(const std::string& name)
    : name_(name),
      parameters_(DefaultSolverParameters()),
      random_(CpRandomSeed()),

      use_fast_local_search_(true),
      local_search_profiler_(BuildLocalSearchProfiler(this)) {
  Init();
}

void Solver::Init() {
  CheckSolverParameters(parameters_);
  ReversibleEngine::Init(parameters_.trail_block_size(),
                         parameters_.compress_trail());
  queue_ = std::make_unique<Queue>(this);
  branches_ = 0;
  fails_ = 0;
  decisions_ = 0;
  neighbors_ = 0;
  filtered_neighbors_ = 0;
  accepted_neighbors_ = 0;
  optimization_direction_ = NOT_SET;
  timer_ = std::make_unique<ClockTimer>();
  searches_.assign(1, new Search(this, 0));
  fail_stamp_ = uint64_t{1};
  balancing_decision_ = std::make_unique<BalancingDecision>();
  fail_intercept_ = nullptr;
  true_constraint_ = nullptr;
  false_constraint_ = nullptr;
  fail_decision_ = std::make_unique<FailDecision>();
  constraint_index_ = 0;
  additional_constraint_index_ = 0;
  num_int_vars_ = 0;
  propagation_monitor_.reset(BuildTrace(this));
  local_search_monitor_.reset(BuildLocalSearchMonitorPrimary(this));
  print_trace_ = nullptr;
  anonymous_variable_index_ = 0;
  should_fail_ = false;

  for (int i = 0; i < kNumPriorities; ++i) {
    demon_runs_[i] = 0;
  }
  searches_.push_back(new Search(this));
  PushSentinel(SOLVER_CTOR_SENTINEL);
  InitCachedIntConstants();  // to be called after the SENTINEL is set.
  InitCachedConstraint();    // Cache the true constraint.
  timer_->Restart();
  model_cache_ = std::make_unique<ModelCache>(
      [this]() { return state_ == OUTSIDE_SEARCH; });

  AddLocalSearchMonitor(
      reinterpret_cast<LocalSearchMonitor*>(local_search_profiler_));
  SetRestoreBooleanVarValue([](IntVar* var) { var->RestoreBooleanValue(); });
}

Solver::~Solver() {
  // solver destructor called with searches open.
  CHECK_EQ(2, searches_.size());
  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);

  StateInfo info;
  TrailMarkerType finalType = PopState(&info);
  // Not popping a SENTINEL in Solver destructor.
  DCHECK_EQ(finalType, SENTINEL);
  // Not popping initial SENTINEL in Solver destructor.
  DCHECK_EQ(info.int_info, SOLVER_CTOR_SENTINEL);
  gtl::STLDeleteElements(&searches_);

  DeleteLocalSearchProfiler(local_search_profiler_);
}

std::string Solver::DebugString() const {
  std::string out = "Solver(name = \"" + name_ + "\", state = ";
  switch (state_) {
    case OUTSIDE_SEARCH:
      out += "OUTSIDE_SEARCH";
      break;
    case IN_ROOT_NODE:
      out += "IN_ROOT_NODE";
      break;
    case IN_SEARCH:
      out += "IN_SEARCH";
      break;
    case AT_SOLUTION:
      out += "AT_SOLUTION";
      break;
    case NO_MORE_SOLUTIONS:
      out += "NO_MORE_SOLUTIONS";
      break;
    case PROBLEM_INFEASIBLE:
      out += "PROBLEM_INFEASIBLE";
      break;
  }
  absl::StrAppendFormat(
      &out,
      ", branches = %d, fails = %d, decisions = %d, delayed demon runs = %d, "
      "var demon runs = %d, normal demon runs = %d, Run time = %d ms)",
      branches_, fails_, decisions_, demon_runs_[DELAYED_PRIORITY],
      demon_runs_[VAR_PRIORITY], demon_runs_[NORMAL_PRIORITY], wall_time());
  return out;
}

int64_t Solver::MemoryUsage() {
  return operations_research::sysinfo::MemoryUsageProcess().value_or(-1);
}

int64_t Solver::wall_time() const {
  return absl::ToInt64Milliseconds(timer_->GetDuration());
}

absl::Time Solver::Now() const {
  return absl::FromUnixSeconds(0) + timer_->GetDuration();
}

int64_t Solver::solutions() const {
  return TopLevelSearch()->solution_counter();
}

int64_t Solver::unchecked_solutions() const {
  return TopLevelSearch()->unchecked_solution_counter();
}

void Solver::IncrementUncheckedSolutionCounter() {
  TopLevelSearch()->IncrementUncheckedSolutionCounter();
}

bool Solver::IsUncheckedSolutionLimitReached() {
  return TopLevelSearch()->IsUncheckedSolutionLimitReached();
}

void Solver::TopPeriodicCheck() { TopLevelSearch()->PeriodicCheck(); }

int Solver::TopProgressPercent() { return TopLevelSearch()->ProgressPercent(); }

ConstraintSolverStatistics Solver::GetConstraintSolverStatistics() const {
  ConstraintSolverStatistics stats;
  stats.set_num_branches(branches());
  stats.set_num_failures(failures());
  stats.set_num_solutions(solutions());
  stats.set_bytes_used(MemoryUsage());
  stats.set_duration_seconds(absl::ToDoubleSeconds(timer_->GetDuration()));
  return stats;
}

void Solver::PushState() {
  StateInfo info;
  PushState(SIMPLE_MARKER, info);
}

void Solver::PopState() {
  StateInfo info;
  TrailMarkerType t = PopState(&info);
  CHECK_EQ(SIMPLE_MARKER, t);
}

void Solver::PushState(TrailMarkerType t, const StateInfo& info) {
  StateMarker* m = new StateMarker(t, info);
  if (t != REVERSIBLE_ACTION || info.int_info == 0) {
    FillStateMarker(m);
  }
  searches_.back()->marker_stack_.push_back(m);
  queue_->increase_stamp();
}

void Solver::AddBacktrackAction(Action a, bool fast) {
  StateInfo info(std::move(a), fast);
  PushState(REVERSIBLE_ACTION, info);
}

TrailMarkerType Solver::PopState(StateInfo* info) {
  CHECK(!searches_.back()->marker_stack_.empty())
      << "PopState() on an empty stack";
  CHECK(info != nullptr);
  StateMarker* const m = searches_.back()->marker_stack_.back();
  if (m->type != REVERSIBLE_ACTION || m->info.int_info == 0) {
    BacktrackTo(m);
  }
  TrailMarkerType t = m->type;
  (*info) = m->info;
  searches_.back()->marker_stack_.pop_back();
  delete m;
  queue_->increase_stamp();
  return t;
}
void Solver::FreezeQueue() { queue_->Freeze(); }

void Solver::UnfreezeQueue() { queue_->Unfreeze(); }

void Solver::EnqueueVar(Demon* d) { queue_->EnqueueVar(d); }

void Solver::EnqueueDelayedDemon(Demon* d) { queue_->EnqueueDelayedDemon(d); }

void Solver::ExecuteAll(const SimpleRevFIFO<Demon*>& demons) {
  queue_->ExecuteAll(demons);
}

void Solver::EnqueueAll(const SimpleRevFIFO<Demon*>& demons) {
  queue_->EnqueueAll(demons);
}

uint64_t Solver::fail_stamp() const { return fail_stamp_; }

void Solver::set_action_on_fail(Action a) {
  queue_->set_action_on_fail(std::move(a));
}

void Solver::set_variable_to_clean_on_fail(IntVar* v) {
  queue_->set_variable_to_clean_on_fail(v);
}

void Solver::reset_action_on_fail() { queue_->reset_action_on_fail(); }

void Solver::AddConstraint(Constraint* c) {
  DCHECK(c != nullptr);
  if (c == true_constraint_) {
    return;
  }
  if (state_ == IN_SEARCH) {
    queue_->AddConstraint(c);
  } else if (state_ == IN_ROOT_NODE) {
    DCHECK_GE(constraint_index_, 0);
    DCHECK_LE(constraint_index_, constraints_list_.size());
    const int constraint_parent =
        constraint_index_ == constraints_list_.size()
            ? additional_constraints_parent_list_[additional_constraint_index_]
            : constraint_index_;
    additional_constraints_list_.push_back(c);
    additional_constraints_parent_list_.push_back(constraint_parent);
  } else {
    if (parameters_.print_added_constraints()) {
      LOG(INFO) << c->DebugString();
    }
    constraints_list_.push_back(c);
  }
}

void Solver::AddCastConstraint(CastConstraint* constraint, IntVar* target_var,
                               IntExpr* expr) {
  if (constraint != nullptr) {
    if (state_ != IN_SEARCH) {
      cast_constraints_.insert(constraint);
      cast_information_[target_var] =
          Solver::IntegerCastInfo(target_var, expr, constraint);
    }
    AddConstraint(constraint);
  }
}

void Solver::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitModel(name_);
  ForAll(constraints_list_, &Constraint::Accept, visitor);
  visitor->EndVisitModel(name_);
}

void Solver::ProcessConstraints() {
  // Both constraints_list_ and additional_constraints_list_ are used in
  // a FIFO way.
  if (parameters_.print_model()) {
    ModelVisitor* const visitor = MakePrintModelVisitor();
    Accept(visitor);
  }
  if (parameters_.print_model_stats()) {
    ModelVisitor* const visitor = MakeStatisticsModelVisitor();
    Accept(visitor);
  }

  if (parameters_.disable_solve()) {
    LOG(INFO) << "Forcing early failure";
    Fail();
  }

  // Clear state before processing constraints.
  const int constraints_size = constraints_list_.size();
  additional_constraints_list_.clear();
  additional_constraints_parent_list_.clear();

  for (constraint_index_ = 0; constraint_index_ < constraints_size;
       ++constraint_index_) {
    Constraint* const constraint = constraints_list_[constraint_index_];
    propagation_monitor_->BeginConstraintInitialPropagation(constraint);
    constraint->PostAndPropagate();
    propagation_monitor_->EndConstraintInitialPropagation(constraint);
  }
  CHECK_EQ(constraints_list_.size(), constraints_size);

  // Process nested constraints added during the previous step.
  for (int additional_constraint_index_ = 0;
       additional_constraint_index_ < additional_constraints_list_.size();
       ++additional_constraint_index_) {
    Constraint* const nested =
        additional_constraints_list_[additional_constraint_index_];
    const int parent_index =
        additional_constraints_parent_list_[additional_constraint_index_];
    Constraint* const parent = constraints_list_[parent_index];
    propagation_monitor_->BeginNestedConstraintInitialPropagation(parent,
                                                                  nested);
    nested->PostAndPropagate();
    propagation_monitor_->EndNestedConstraintInitialPropagation(parent, nested);
  }
}

bool Solver::CurrentlyInSolve() const {
  DCHECK_GT(SolveDepth(), 0);
  DCHECK(searches_.back() != nullptr);
  return searches_.back()->created_by_solve();
}

bool Solver::Solve(DecisionBuilder* db, SearchMonitor* m1) {
  return Solve(db, std::vector<SearchMonitor*>{m1});
}

bool Solver::Solve(DecisionBuilder* db) { return Solve(db, {}); }

bool Solver::Solve(DecisionBuilder* db, SearchMonitor* m1, SearchMonitor* m2) {
  return Solve(db, {m1, m2});
}

bool Solver::Solve(DecisionBuilder* db, SearchMonitor* m1, SearchMonitor* m2,
                   SearchMonitor* m3) {
  return Solve(db, {m1, m2, m3});
}

bool Solver::Solve(DecisionBuilder* db, SearchMonitor* m1, SearchMonitor* m2,
                   SearchMonitor* m3, SearchMonitor* m4) {
  return Solve(db, {m1, m2, m3, m4});
}

bool Solver::Solve(DecisionBuilder* db,
                   const std::vector<SearchMonitor*>& monitors) {
  NewSearch(db, monitors);
  searches_.back()->set_created_by_solve(true);  // Overwrites default.
  NextSolution();
  const bool solution_found = searches_.back()->solution_counter() > 0;
  EndSearch();
  return solution_found;
}

void Solver::NewSearch(DecisionBuilder* db, SearchMonitor* m1) {
  return NewSearch(db, std::vector<SearchMonitor*>{m1});
}

void Solver::NewSearch(DecisionBuilder* db) { return NewSearch(db, {}); }

void Solver::NewSearch(DecisionBuilder* db, SearchMonitor* m1,
                       SearchMonitor* m2) {
  return NewSearch(db, {m1, m2});
}

void Solver::NewSearch(DecisionBuilder* db, SearchMonitor* m1,
                       SearchMonitor* m2, SearchMonitor* m3) {
  return NewSearch(db, {m1, m2, m3});
}

void Solver::NewSearch(DecisionBuilder* db, SearchMonitor* m1,
                       SearchMonitor* m2, SearchMonitor* m3,
                       SearchMonitor* m4) {
  return NewSearch(db, {m1, m2, m3, m4});
}

extern PropagationMonitor* BuildPrintTrace(Solver* s);

// Opens a new top level search.
void Solver::NewSearch(DecisionBuilder* db,
                       const std::vector<SearchMonitor*>& monitors) {
  // TODO(user) : reset statistics

  CHECK(db != nullptr);
  const bool nested = state_ == IN_SEARCH;

  if (state_ == IN_ROOT_NODE) {
    LOG(FATAL) << "Cannot start new searches here.";
  }

  Search* const search = nested ? new Search(this) : searches_.back();
  search->set_created_by_solve(false);  // default behavior.

  // ----- jumps to correct state -----

  if (nested) {
    // Nested searches are created on demand, and deleted afterwards.
    DCHECK_GE(searches_.size(), 2);
    searches_.push_back(search);
  } else {
    // Top level search is persistent.
    // TODO(user): delete top level search after EndSearch().
    DCHECK_EQ(2, searches_.size());
    // TODO(user): Check if these two lines are still necessary.
    BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    state_ = OUTSIDE_SEARCH;
  }

  // ----- manages all monitors -----

  // Always install the main propagation and local search monitors.
  propagation_monitor_->Install();

  local_search_monitor_->Install();
  if (local_search_profiler_ != nullptr) {
    InstallLocalSearchProfiler(local_search_profiler_);
  }

  // Push monitors and enter search.
  for (SearchMonitor* const monitor : monitors) {
    if (monitor != nullptr) {
      monitor->Install();
    }
  }
  std::vector<SearchMonitor*> extras;
  db->AppendMonitors(this, &extras);
  for (SearchMonitor* const monitor : extras) {
    if (monitor != nullptr) {
      monitor->Install();
    }
  }
  // Install the print trace if needed.
  // The print_trace needs to be last to detect propagation from the objective.
  if (nested) {
    if (print_trace_ != nullptr) {  // Was installed at the top level?
      print_trace_->Install();      // Propagates to nested search.
    }
  } else {                   // Top level search
    print_trace_ = nullptr;  // Clears it first.
    if (parameters_.trace_propagation()) {
      print_trace_ = BuildPrintTrace(this);
      print_trace_->Install();
    } else if (parameters_.trace_search()) {
      // This is useful to trace the exact behavior of the search.
      // The '######## ' prefix is the same as the propagation trace.
      // Search trace is subsumed by propagation trace, thus only one
      // is necessary.
      SearchMonitor* const trace = MakeSearchTrace("######## ");
      trace->Install();
    }
  }

  // ----- enters search -----

  search->EnterSearch();

  // Push sentinel and set decision builder.
  PushSentinel(INITIAL_SEARCH_SENTINEL);
  search->set_decision_builder(db);
}

// Backtrack to the last open right branch in the search tree.
// It returns true in case the search tree has been completely explored.
bool Solver::BacktrackOneLevel(Decision** fail_decision) {
  bool no_more_solutions = false;
  bool end_loop = false;
  while (!end_loop) {
    StateInfo info;
    TrailMarkerType t = PopState(&info);
    switch (t) {
      case SENTINEL:
        CHECK_EQ(info.ptr_info, this) << "Wrong sentinel found";
        CHECK((info.int_info == ROOT_NODE_SENTINEL && SolveDepth() == 1) ||
              (info.int_info == INITIAL_SEARCH_SENTINEL && SolveDepth() > 1));
        searches_.back()->sentinel_pushed_--;
        no_more_solutions = true;
        end_loop = true;
        break;
      case SIMPLE_MARKER:
        LOG(ERROR) << "Simple markers should not be encountered during search";
        break;
      case CHOICE_POINT:
        if (info.int_info == 0) {  // was left branch
          (*fail_decision) = reinterpret_cast<Decision*>(info.ptr_info);
          end_loop = true;
          searches_.back()->set_search_depth(info.depth);
          searches_.back()->set_search_left_depth(info.left_depth);
        }
        break;
      case REVERSIBLE_ACTION: {
        if (info.reversible_action != nullptr) {
          info.reversible_action(this);
        }
        break;
      }
    }
  }
  Search* const search = searches_.back();
  search->EndFail();
  fail_stamp_++;
  if (no_more_solutions) {
    search->NoMoreSolutions();
  }
  return no_more_solutions;
}

void Solver::PushSentinel(int magic_code) {
  StateInfo info(this, magic_code);
  PushState(SENTINEL, info);
  // We do not count the sentinel pushed in the ctor.
  if (magic_code != SOLVER_CTOR_SENTINEL) {
    searches_.back()->sentinel_pushed_++;
  }
  const int pushed = searches_.back()->sentinel_pushed_;
  DCHECK((magic_code == SOLVER_CTOR_SENTINEL) ||
         (magic_code == INITIAL_SEARCH_SENTINEL && pushed == 1) ||
         (magic_code == ROOT_NODE_SENTINEL && pushed == 2));
}

void Solver::RestartSearch() {
  Search* const search = searches_.back();
  CHECK_NE(0, search->sentinel_pushed_);
  if (SolveDepth() == 1) {  // top level.
    if (search->sentinel_pushed_ > 1) {
      BacktrackToSentinel(ROOT_NODE_SENTINEL);
    }
    CHECK_EQ(1, search->sentinel_pushed_);
    PushSentinel(ROOT_NODE_SENTINEL);
    state_ = IN_SEARCH;
  } else {
    CHECK_EQ(IN_SEARCH, state_);
    if (search->sentinel_pushed_ > 0) {
      BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    }
    CHECK_EQ(0, search->sentinel_pushed_);
    PushSentinel(INITIAL_SEARCH_SENTINEL);
  }

  search->RestartSearch();
}

// Backtrack to the initial search sentinel.
// Does not change the state, this should be done by the caller.
void Solver::BacktrackToSentinel(int magic_code) {
  Search* const search = searches_.back();
  bool end_loop = search->sentinel_pushed_ == 0;
  while (!end_loop) {
    StateInfo info;
    TrailMarkerType t = PopState(&info);
    switch (t) {
      case SENTINEL: {
        CHECK_EQ(info.ptr_info, this) << "Wrong sentinel found";
        CHECK_GE(--search->sentinel_pushed_, 0);
        search->set_search_depth(0);
        search->set_search_left_depth(0);

        if (info.int_info == magic_code) {
          end_loop = true;
        }
        break;
      }
      case SIMPLE_MARKER:
        break;
      case CHOICE_POINT:
        break;
      case REVERSIBLE_ACTION: {
        info.reversible_action(this);
        break;
      }
    }
  }
  fail_stamp_++;
}

// Closes the current search without backtrack.
void Solver::JumpToSentinelWhenNested() {
  CHECK_GT(SolveDepth(), 1) << "calling JumpToSentinel from top level";
  Search* c = searches_.back();
  Search* p = ParentSearch();
  bool found = false;
  while (!c->marker_stack_.empty()) {
    StateMarker* const m = c->marker_stack_.back();
    if (m->type == REVERSIBLE_ACTION) {
      p->marker_stack_.push_back(m);
    } else {
      if (m->type == SENTINEL) {
        CHECK_EQ(c->marker_stack_.size(), 1) << "Sentinel found too early";
        found = true;
      }
      delete m;
    }
    c->marker_stack_.pop_back();
  }
  c->set_search_depth(0);
  c->set_search_left_depth(0);
  CHECK_EQ(found, true) << "Sentinel not found";
}

namespace {
class ReverseDecision : public Decision {
 public:
  explicit ReverseDecision(Decision* const d) : decision_(d) {
    CHECK(d != nullptr);
  }
  ~ReverseDecision() override {}

  void Apply(Solver* s) override { decision_->Refute(s); }

  void Refute(Solver* s) override { decision_->Apply(s); }

  void Accept(DecisionVisitor* visitor) const override {
    decision_->Accept(visitor);
  }

  std::string DebugString() const override {
    std::string str = "Reverse(";
    str += decision_->DebugString();
    str += ")";
    return str;
  }

 private:
  Decision* const decision_;
};
}  // namespace

// Search for the next solution in the search tree.
bool Solver::NextSolution() {
  Search* const search = searches_.back();
  Decision* fd = nullptr;
  const int solve_depth = SolveDepth();
  const bool top_level = solve_depth <= 1;

  if (solve_depth == 0 && !search->decision_builder()) {
    LOG(WARNING) << "NextSolution() called without a NewSearch before";
    return false;
  }

  if (top_level) {  // Manage top level state.
    switch (state_) {
      case PROBLEM_INFEASIBLE:
        return false;
      case NO_MORE_SOLUTIONS:
        return false;
      case AT_SOLUTION: {
        if (BacktrackOneLevel(&fd)) {  // No more solutions.
          state_ = NO_MORE_SOLUTIONS;
          return false;
        }
        state_ = IN_SEARCH;
        break;
      }
      case OUTSIDE_SEARCH: {
        state_ = IN_ROOT_NODE;
        search->BeginInitialPropagation();
        CP_TRY(search) {
          ProcessConstraints();
          search->EndInitialPropagation();
          PushSentinel(ROOT_NODE_SENTINEL);
          state_ = IN_SEARCH;
          search->ClearBuffer();
        }
        CP_ON_FAIL {
          queue_->AfterFailure();
          BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
          state_ = PROBLEM_INFEASIBLE;
          return false;
        }
        break;
      }
      case IN_SEARCH:  // Usually after a RestartSearch
        break;
      case IN_ROOT_NODE:
        LOG(FATAL) << "Should not happen";
        break;
    }
  }

  volatile bool finish = false;
  volatile bool result = false;
  DecisionBuilder* const db = search->decision_builder();

  while (!finish) {
    CP_TRY(search) {
      if (fd != nullptr) {
        StateInfo i1(fd, 1, search->search_depth(),
                     search->left_search_depth());  // 1 for right branch
        PushState(CHOICE_POINT, i1);
        search->RefuteDecision(fd);
        branches_++;
        fd->Refute(this);
        // Check the fail state that could have been set in the python/java/C#
        // layer.
        CheckFail();
        search->AfterDecision(fd, false);
        search->RightMove();
        fd = nullptr;
      }
      Decision* d = nullptr;
      for (;;) {
        search->BeginNextDecision(db);
        d = db->Next(this);
        search->EndNextDecision(db, d);
        if (d == fail_decision_.get()) {
          Fail();  // fail now instead of after 2 branches.
        }
        if (d != nullptr) {
          DecisionModification modification = search->ModifyDecision();
          switch (modification) {
            case SWITCH_BRANCHES: {
              d = RevAlloc(new ReverseDecision(d));
              // We reverse the decision and fall through the normal code.
              ABSL_FALLTHROUGH_INTENDED;
            }
            case NO_CHANGE: {
              decisions_++;
              StateInfo i2(d, 0, search->search_depth(),
                           search->left_search_depth());  // 0 for left branch
              PushState(CHOICE_POINT, i2);
              search->ApplyDecision(d);
              branches_++;
              d->Apply(this);
              CheckFail();
              search->AfterDecision(d, true);
              search->LeftMove();
              break;
            }
            case KEEP_LEFT: {
              search->ApplyDecision(d);
              d->Apply(this);
              CheckFail();
              search->AfterDecision(d, true);
              break;
            }
            case KEEP_RIGHT: {
              search->RefuteDecision(d);
              d->Refute(this);
              CheckFail();
              search->AfterDecision(d, false);
              break;
            }
            case KILL_BOTH: {
              Fail();
            }
          }
        } else {
          break;
        }
      }
      if (search->AcceptSolution()) {
        search->IncrementSolutionCounter();
        if (!search->AtSolution() || !CurrentlyInSolve()) {
          result = true;
          finish = true;
        } else {
          Fail();
        }
      } else {
        Fail();
      }
    }
    CP_ON_FAIL {
      queue_->AfterFailure();
      if (search->should_finish()) {
        fd = nullptr;
        BacktrackToSentinel(top_level ? ROOT_NODE_SENTINEL
                                      : INITIAL_SEARCH_SENTINEL);
        result = false;
        finish = true;
        search->set_should_finish(false);
        search->set_should_restart(false);
        // We do not need to push back the sentinel as we are exiting anyway.
      } else if (search->should_restart()) {
        fd = nullptr;
        BacktrackToSentinel(top_level ? ROOT_NODE_SENTINEL
                                      : INITIAL_SEARCH_SENTINEL);
        search->set_should_finish(false);
        search->set_should_restart(false);
        PushSentinel(top_level ? ROOT_NODE_SENTINEL : INITIAL_SEARCH_SENTINEL);
        search->RestartSearch();
      } else {
        if (BacktrackOneLevel(&fd)) {  // no more solutions.
          result = false;
          finish = true;
        }
      }
    }
  }
  if (result) {
    search->ClearBuffer();
  }
  if (top_level) {  // Manage state after NextSolution().
    state_ = (result ? AT_SOLUTION : NO_MORE_SOLUTIONS);
  }
  return result;
}

void Solver::EndSearch() {
  Search* const search = searches_.back();
  if (search->backtrack_at_the_end_of_the_search()) {
    BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
  } else {
    CHECK_GT(searches_.size(), 2);
    if (search->sentinel_pushed_ > 0) {
      JumpToSentinelWhenNested();
    }
  }
  search->ExitSearch();
  search->Clear();
  if (2 == searches_.size()) {  // Ending top level search.
    // Restores the state.
    state_ = OUTSIDE_SEARCH;

    if (parameters_.print_local_search_profile()) {
      const std::string profile = LocalSearchProfile();
      if (!profile.empty()) LOG(INFO) << profile;
    }
  } else {  // We clean the nested Search.
    delete search;
    searches_.pop_back();
  }
}

bool Solver::CheckAssignment(Assignment* solution) {
  CHECK(solution);
  if (state_ == IN_SEARCH || state_ == IN_ROOT_NODE) {
    LOG(FATAL) << "CheckAssignment is only available at the top level.";
  }
  // Check state and go to OUTSIDE_SEARCH.
  Search* const search = searches_.back();
  search->set_created_by_solve(false);  // default behavior.

  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
  state_ = OUTSIDE_SEARCH;

  // Push monitors and enter search.
  search->EnterSearch();

  // Push sentinel and set decision builder.
  DCHECK_EQ(0, SolveDepth());
  DCHECK_EQ(2, searches_.size());
  PushSentinel(INITIAL_SEARCH_SENTINEL);
  search->BeginInitialPropagation();
  CP_TRY(search) {
    state_ = IN_ROOT_NODE;
    DecisionBuilder* const restore = MakeRestoreAssignment(solution);
    restore->Next(this);
    ProcessConstraints();
    search->EndInitialPropagation();
    BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    search->ClearBuffer();
    state_ = OUTSIDE_SEARCH;
    return true;
  }
  CP_ON_FAIL {
    const int index =
        constraint_index_ < constraints_list_.size()
            ? constraint_index_
            : additional_constraints_parent_list_[additional_constraint_index_];
    Constraint* const ct = constraints_list_[index];
    if (ct->name().empty()) {
      LOG(INFO) << "Failing constraint = " << ct->DebugString();
    } else {
      LOG(INFO) << "Failing constraint = " << ct->name() << ":"
                << ct->DebugString();
    }
    queue_->AfterFailure();
    BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    state_ = PROBLEM_INFEASIBLE;
    return false;
  }
}

namespace {
class AddConstraintDecisionBuilder : public DecisionBuilder {
 public:
  explicit AddConstraintDecisionBuilder(Constraint* ct) : constraint_(ct) {
    CHECK(ct != nullptr);
  }

  ~AddConstraintDecisionBuilder() override {}

  Decision* Next(Solver* solver) override {
    solver->AddConstraint(constraint_);
    return nullptr;
  }

  std::string DebugString() const override {
    return absl::StrFormat("AddConstraintDecisionBuilder(%s)",
                           constraint_->DebugString());
  }

 private:
  Constraint* const constraint_;
};
}  // namespace

DecisionBuilder* Solver::MakeConstraintAdder(Constraint* ct) {
  return RevAlloc(new AddConstraintDecisionBuilder(ct));
}

bool Solver::CheckConstraint(Constraint* ct) {
  return Solve(MakeConstraintAdder(ct));
}

bool Solver::SolveAndCommit(DecisionBuilder* db, SearchMonitor* m1) {
  return SolveAndCommit(db, std::vector<SearchMonitor*>{m1});
}

bool Solver::SolveAndCommit(DecisionBuilder* db) {
  return SolveAndCommit(db, {});
}

bool Solver::SolveAndCommit(DecisionBuilder* db, SearchMonitor* m1,
                            SearchMonitor* m2) {
  return SolveAndCommit(db, {m1, m2});
}

bool Solver::SolveAndCommit(DecisionBuilder* db, SearchMonitor* m1,
                            SearchMonitor* m2, SearchMonitor* m3) {
  return SolveAndCommit(db, {m1, m2, m3});
}

bool Solver::SolveAndCommit(DecisionBuilder* db,
                            const std::vector<SearchMonitor*>& monitors) {
  NewSearch(db, monitors);
  searches_.back()->set_created_by_solve(true);  // Overwrites default.
  searches_.back()->set_backtrack_at_the_end_of_the_search(false);
  NextSolution();
  const bool solution_found = searches_.back()->solution_counter() > 0;
  EndSearch();
  return solution_found;
}

void Solver::Fail() {
  if (fail_intercept_) {
    fail_intercept_();
    return;
  }
  ConstraintSolverFailsHere();
  fails_++;
  searches_.back()->BeginFail();
  searches_.back()->JumpBack();
}

void Solver::FinishCurrentSearch() {
  searches_.back()->set_should_finish(true);
}

void Solver::RestartCurrentSearch() {
  searches_.back()->set_should_restart(true);
}

// ----- Cast Expression -----

IntExpr* Solver::CastExpression(const IntVar* var) const {
  const IntegerCastInfo* const cast_info =
      gtl::FindOrNull(cast_information_, var);
  if (cast_info != nullptr) {
    return cast_info->expression;
  }
  return nullptr;
}

// --- Propagation object names ---

std::string Solver::GetName(const PropagationBaseObject* object) {
  const std::string* name = gtl::FindOrNull(propagation_object_names_, object);
  if (name != nullptr) {
    return *name;
  }
  const IntegerCastInfo* const cast_info =
      gtl::FindOrNull(cast_information_, object);
  if (cast_info != nullptr && cast_info->expression != nullptr) {
    if (cast_info->expression->HasName()) {
      return absl::StrFormat("Var<%s>", cast_info->expression->name());
    } else if (parameters_.name_cast_variables()) {
      return absl::StrFormat("Var<%s>", cast_info->expression->DebugString());
    } else {
      const std::string new_name =
          absl::StrFormat("CastVar<%d>", anonymous_variable_index_++);
      propagation_object_names_[object] = new_name;
      return new_name;
    }
  }
  const std::string base_name = object->BaseName();
  if (parameters_.name_all_variables() && !base_name.empty()) {
    const std::string new_name =
        absl::StrFormat("%s_%d", base_name, anonymous_variable_index_++);
    propagation_object_names_[object] = new_name;
    return new_name;
  }
  return empty_name_;
}

void Solver::SetName(const PropagationBaseObject* object,
                     absl::string_view name) {
  if (parameters_.store_names() &&
      GetName(object) != name) {  // in particular if name.empty()
    propagation_object_names_[object] = name;
  }
}

bool Solver::HasName(const PropagationBaseObject* object) const {
  return propagation_object_names_.contains(
             const_cast<PropagationBaseObject*>(object)) ||
         (!object->BaseName().empty() && parameters_.name_all_variables());
}

// ------------------ Useful Operators ------------------

std::ostream& operator<<(std::ostream& out, const Solver* s) {
  out << s->DebugString();
  return out;
}

std::ostream& operator<<(std::ostream& out, const BaseObject* o) {
  out << o->DebugString();
  return out;
}

// ---------- PropagationBaseObject ---------

std::string PropagationBaseObject::name() const {
  return solver_->GetName(this);
}

void PropagationBaseObject::set_name(absl::string_view name) {
  solver_->SetName(this, name);
}

bool PropagationBaseObject::HasName() const { return solver_->HasName(this); }

std::string PropagationBaseObject::BaseName() const { return ""; }

void PropagationBaseObject::ExecuteAll(const SimpleRevFIFO<Demon*>& demons) {
  solver_->ExecuteAll(demons);
}

void PropagationBaseObject::EnqueueAll(const SimpleRevFIFO<Demon*>& demons) {
  solver_->EnqueueAll(demons);
}

// ---------- Decision Builder ----------

std::string DecisionBuilder::DebugString() const { return "DecisionBuilder"; }

std::string DecisionBuilder::GetName() const {
  return name_.empty() ? DebugString() : name_;
}

void DecisionBuilder::AppendMonitors(Solver* /*solver*/,
                                     std::vector<SearchMonitor*>* /*extras*/) {}

void DecisionBuilder::Accept(ModelVisitor* /*visitor*/) const {}

// ---------- Decision and DecisionVisitor ----------

void Decision::Accept(DecisionVisitor* visitor) const {
  visitor->VisitUnknownDecision();
}

void DecisionVisitor::VisitSetVariableValue([[maybe_unused]] IntVar* var,
                                            [[maybe_unused]] int64_t value) {}
void DecisionVisitor::VisitSplitVariableDomain(
    [[maybe_unused]] IntVar* var, [[maybe_unused]] int64_t value,
    [[maybe_unused]] bool start_with_lower_half) {}
void DecisionVisitor::VisitUnknownDecision() {}
void DecisionVisitor::VisitScheduleOrPostpone([[maybe_unused]] IntervalVar* var,
                                              [[maybe_unused]] int64_t est) {}
void DecisionVisitor::VisitScheduleOrExpedite([[maybe_unused]] IntervalVar* var,
                                              [[maybe_unused]] int64_t est) {}
void DecisionVisitor::VisitRankFirstInterval(
    [[maybe_unused]] SequenceVar* sequence, [[maybe_unused]] int index) {}

void DecisionVisitor::VisitRankLastInterval(
    [[maybe_unused]] SequenceVar* sequence, [[maybe_unused]] int index) {}

// ---------- Search Monitor ----------

void SearchMonitor::EnterSearch() {}
void SearchMonitor::RestartSearch() {}
void SearchMonitor::ExitSearch() {}
void SearchMonitor::BeginNextDecision(DecisionBuilder* const) {}
void SearchMonitor::EndNextDecision(DecisionBuilder* const, Decision* const) {}
void SearchMonitor::ApplyDecision(Decision* const) {}
void SearchMonitor::RefuteDecision(Decision* const) {}
void SearchMonitor::AfterDecision(Decision* const,
                                  [[maybe_unused]] bool apply) {}
void SearchMonitor::BeginFail() {}
void SearchMonitor::EndFail() {}
void SearchMonitor::BeginInitialPropagation() {}
void SearchMonitor::EndInitialPropagation() {}
bool SearchMonitor::AcceptSolution() { return true; }
bool SearchMonitor::AtSolution() { return false; }
void SearchMonitor::NoMoreSolutions() {}
bool SearchMonitor::AtLocalOptimum() { return false; }
bool SearchMonitor::AcceptDelta([[maybe_unused]] Assignment* delta,
                                [[maybe_unused]] Assignment* deltadelta) {
  return true;
}
void SearchMonitor::AcceptNeighbor() {}
void SearchMonitor::AcceptUncheckedNeighbor() {}
void SearchMonitor::PeriodicCheck() {}
void SearchMonitor::Accept([[maybe_unused]] ModelVisitor* visitor) const {}
// A search monitors adds itself on the active search.
void SearchMonitor::Install() {
  for (std::underlying_type<Solver::MonitorEvent>::type event = 0;
       event != to_underlying(Solver::MonitorEvent::kLast); ++event) {
    ListenToEvent(static_cast<Solver::MonitorEvent>(event));
  }
}

void SearchMonitor::ListenToEvent(Solver::MonitorEvent event) {
  solver()->searches_.back()->AddEventListener(event, this);
}

// ---------- Propagation Monitor -----------
PropagationMonitor::PropagationMonitor(Solver* solver)
    : SearchMonitor(solver) {}

PropagationMonitor::~PropagationMonitor() {}

// A propagation monitor listens to search events as well as propagation events.
void PropagationMonitor::Install() {
  SearchMonitor::Install();
  solver()->AddPropagationMonitor(this);
}

void Solver::AddPropagationMonitor(PropagationMonitor* monitor) {
  AppendPropagationMonitor(this, monitor);
}

PropagationMonitor* Solver::GetPropagationMonitor() const {
  return propagation_monitor_.get();
}

// ---------- Local Search Monitor Primary ----------

class LocalSearchMonitorPrimary : public LocalSearchMonitor {
 public:
  explicit LocalSearchMonitorPrimary(Solver* solver)
      : LocalSearchMonitor(solver) {}

  void BeginOperatorStart() override {
    ForAll(monitors_, &LocalSearchMonitor::BeginOperatorStart);
  }
  void EndOperatorStart() override {
    ForAll(monitors_, &LocalSearchMonitor::EndOperatorStart);
  }
  void BeginMakeNextNeighbor(const LocalSearchOperator* op) override {
    ForAll(monitors_, &LocalSearchMonitor::BeginMakeNextNeighbor, op);
  }
  void EndMakeNextNeighbor(const LocalSearchOperator* op, bool neighbor_found,
                           const Assignment* delta,
                           const Assignment* deltadelta) override {
    ForAll(monitors_, &LocalSearchMonitor::EndMakeNextNeighbor, op,
           neighbor_found, delta, deltadelta);
  }
  void BeginFilterNeighbor(const LocalSearchOperator* op) override {
    ForAll(monitors_, &LocalSearchMonitor::BeginFilterNeighbor, op);
  }
  void EndFilterNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    ForAll(monitors_, &LocalSearchMonitor::EndFilterNeighbor, op,
           neighbor_found);
  }
  void BeginAcceptNeighbor(const LocalSearchOperator* op) override {
    ForAll(monitors_, &LocalSearchMonitor::BeginAcceptNeighbor, op);
  }
  void EndAcceptNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    ForAll(monitors_, &LocalSearchMonitor::EndAcceptNeighbor, op,
           neighbor_found);
  }
  void BeginFiltering(const LocalSearchFilter* filter) override {
    ForAll(monitors_, &LocalSearchMonitor::BeginFiltering, filter);
  }
  void EndFiltering(const LocalSearchFilter* filter, bool reject) override {
    ForAll(monitors_, &LocalSearchMonitor::EndFiltering, filter, reject);
  }
  bool IsActive() const override { return !monitors_.empty(); }

  // Does not take ownership of monitor.
  void Add(LocalSearchMonitor* monitor) {
    if (monitor != nullptr) {
      monitors_.push_back(monitor);
    }
  }

  // The trace will dispatch propagation events. It needs to listen to search
  // events.
  void Install() override { SearchMonitor::Install(); }

  std::string DebugString() const override {
    return "LocalSearchMonitorPrimary";
  }

 private:
  std::vector<LocalSearchMonitor*> monitors_;
};

LocalSearchMonitor* BuildLocalSearchMonitorPrimary(Solver* s) {
  return new LocalSearchMonitorPrimary(s);
}

void Solver::AddLocalSearchMonitor(LocalSearchMonitor* monitor) {
  reinterpret_cast<class LocalSearchMonitorPrimary*>(
      local_search_monitor_.get())
      ->Add(monitor);
}

LocalSearchMonitor* Solver::GetLocalSearchMonitor() const {
  return local_search_monitor_.get();
}

void Solver::SetSearchContext(Search* search,
                              absl::string_view search_context) {
  search->set_search_context(search_context);
}

std::string Solver::SearchContext() const {
  return ActiveSearch()->search_context();
}

std::string Solver::SearchContext(const Search* search) const {
  return search->search_context();
}

bool Solver::AcceptSolution(Search* search) const {
  return search->AcceptSolution();
}

Assignment* Solver::GetOrCreateLocalSearchState() {
  if (local_search_state_ == nullptr) {
    local_search_state_ = std::make_unique<Assignment>(this);
  }
  return local_search_state_.get();
}

void Solver::ClearLocalSearchState() { local_search_state_.reset(nullptr); }

// ----------------- ProfiledDecisionBuilder ------------

ProfiledDecisionBuilder::ProfiledDecisionBuilder(DecisionBuilder* db)
    : db_(db), name_(db_->GetName()), seconds_(0) {}

Decision* ProfiledDecisionBuilder::Next(Solver* solver) {
  timer_.Start();
  solver->set_context(name());
  // In case db_->Next() fails, gathering the running time on backtrack.
  solver->AddBacktrackAction(
      [this](Solver* solver) {
        if (timer_.IsRunning()) {
          timer_.Stop();
          seconds_ += timer_.Get();
        }
        solver->set_context("");
      },
      true);
  Decision* const decision = db_->Next(solver);
  timer_.Stop();
  seconds_ += timer_.Get();
  solver->set_context("");
  return decision;
}

std::string ProfiledDecisionBuilder::DebugString() const {
  return db_->DebugString();
}

void ProfiledDecisionBuilder::AppendMonitors(
    Solver* solver, std::vector<SearchMonitor*>* extras) {
  db_->AppendMonitors(solver, extras);
}

void ProfiledDecisionBuilder::Accept(ModelVisitor* visitor) const {
  db_->Accept(visitor);
}

// ----------------- Constraint class -------------------

std::string Constraint::DebugString() const { return "Constraint"; }

void Constraint::PostAndPropagate() {
  FreezeQueue();
  Post();
  InitialPropagate();
  solver()->CheckFail();
  UnfreezeQueue();
}

void Constraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint("unknown", this);
  VLOG(3) << "Unknown constraint " << DebugString();
  visitor->EndVisitConstraint("unknown", this);
}

bool Constraint::IsCastConstraint() const {
  return solver()->cast_constraints_.contains(this);
}

IntVar* Constraint::Var() { return nullptr; }

// ---------- IntExpr ----------

void IntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression("unknown", this);
  VLOG(3) << "Unknown expression " << DebugString();
  visitor->EndVisitIntegerExpression("unknown", this);
}

IntVar* IntExpr::VarWithName(const std::string& name) {
  IntVar* var = Var();
  var->set_name(name);
  return var;
}

// ---------- IntVar ----------

IntVar::IntVar(Solver* s) : IntExpr(s), index_(s->GetNewIntVarIndex()) {}

IntVar::IntVar(Solver* s, const std::string& name)
    : IntExpr(s), index_(s->GetNewIntVarIndex()) {
  set_name(name);
}

void IntVar::Accept(ModelVisitor* visitor) const {
  IntExpr* const casted = solver()->CastExpression(this);
  visitor->VisitIntegerVariable(this, casted);
}

int IntVar::VarType() const { return UNSPECIFIED; }

void IntVar::SetValues(const std::vector<int64_t>& values) {
  switch (values.size()) {
    case 0: {
      solver()->Fail();
      break;
    }
    case 1: {
      SetValue(values.back());
      break;
    }
    case 2: {
      if (Contains(values[0])) {
        if (Contains(values[1])) {
          const int64_t l = std::min(values[0], values[1]);
          const int64_t u = std::max(values[0], values[1]);
          SetRange(l, u);
          if (u > l + 1) {
            RemoveInterval(l + 1, u - 1);
          }
        } else {
          SetValue(values[0]);
        }
      } else {
        SetValue(values[1]);
      }
      break;
    }
    default: {
      // TODO(user): use a clean and safe SortedUniqueCopy() class
      // that uses a global, static shared (and locked) storage.
      // TODO(user): We could filter out values not in the var.
      std::vector<int64_t>& tmp = solver()->tmp_vector_;
      tmp.clear();
      tmp.insert(tmp.end(), values.begin(), values.end());
      std::sort(tmp.begin(), tmp.end());
      tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
      const int size = tmp.size();
      const int64_t vmin = Min();
      const int64_t vmax = Max();
      int first = 0;
      int last = size - 1;
      if (tmp.front() > vmax || tmp.back() < vmin) {
        solver()->Fail();
      }
      // TODO(user) : We could find the first position >= vmin by dichotomy.
      while (tmp[first] < vmin || !Contains(tmp[first])) {
        ++first;
        if (first > last || tmp[first] > vmax) {
          solver()->Fail();
        }
      }
      while (last > first && (tmp[last] > vmax || !Contains(tmp[last]))) {
        // Note that last >= first implies tmp[last] >= vmin.
        --last;
      }
      DCHECK_GE(last, first);
      SetRange(tmp[first], tmp[last]);
      while (first < last) {
        const int64_t start = tmp[first] + 1;
        const int64_t end = tmp[first + 1] - 1;
        if (start <= end) {
          RemoveInterval(start, end);
        }
        first++;
      }
    }
  }
}

void IntVar::RemoveValues(const std::vector<int64_t>& values) {
  // TODO(user): Check and maybe inline this code.
  const int size = values.size();
  DCHECK_GE(size, 0);
  switch (size) {
    case 0: {
      return;
    }
    case 1: {
      RemoveValue(values[0]);
      return;
    }
    case 2: {
      RemoveValue(values[0]);
      RemoveValue(values[1]);
      return;
    }
    case 3: {
      RemoveValue(values[0]);
      RemoveValue(values[1]);
      RemoveValue(values[2]);
      return;
    }
    default: {
      // 4 values, let's start doing some more clever things.
      // TODO(user) : Sort values!
      int start_index = 0;
      int64_t new_min = Min();
      if (values[start_index] <= new_min) {
        while (start_index < size - 1 &&
               values[start_index + 1] == values[start_index] + 1) {
          new_min = values[start_index + 1] + 1;
          start_index++;
        }
      }
      int end_index = size - 1;
      int64_t new_max = Max();
      if (values[end_index] >= new_max) {
        while (end_index > start_index + 1 &&
               values[end_index - 1] == values[end_index] - 1) {
          new_max = values[end_index - 1] - 1;
          end_index--;
        }
      }
      SetRange(new_min, new_max);
      for (int i = start_index; i <= end_index; ++i) {
        RemoveValue(values[i]);
      }
    }
  }
}

#undef CP_TRY  // We no longer need those.
#undef CP_ON_FAIL
#undef CP_DO_FAIL

class ActionDemon : public Demon {
 public:
  explicit ActionDemon(const Solver::Action& action) : action_(action) {
    CHECK(action != nullptr);
  }

  ~ActionDemon() override = default;

  void Run(Solver* solver) override { action_(solver); }

 private:
  Solver::Action action_;
};

class ClosureDemon : public Demon {
 public:
  explicit ClosureDemon(const Solver::Closure& closure) : closure_(closure) {
    CHECK(closure != nullptr);
  }

  ~ClosureDemon() override = default;

  void Run(Solver*) override { closure_(); }

 private:
  Solver::Closure closure_;
};

Demon* Solver::MakeConstraintInitialPropagateCallback(Constraint* ct) {
  return MakeConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                              "InitialPropagate");
}

Demon* Solver::MakeDelayedConstraintInitialPropagateCallback(Constraint* ct) {
  return MakeDelayedConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                                     "InitialPropagate");
}

Demon* Solver::MakeActionDemon(Solver::Action action) {
  return RevAlloc(new ActionDemon(action));
}

Demon* Solver::MakeClosureDemon(Solver::Closure closure) {
  return RevAlloc(new ClosureDemon(closure));
}

void Solver::InitCachedConstraint() {
  DCHECK(true_constraint_ == nullptr);
  true_constraint_ = RevAlloc(new TrueConstraint(this));
  DCHECK(false_constraint_ == nullptr);
  false_constraint_ = RevAlloc(new FalseConstraint(this));
}

}  // namespace operations_research
