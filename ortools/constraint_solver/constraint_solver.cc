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
#include <cstdlib>
#include <deque>
#include <iosfwd>
#include <limits>
#include <memory>
#include <numeric>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/alldiff_cst.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/count_cst.h"
#include "ortools/constraint_solver/default_search.h"
#include "ortools/constraint_solver/deviation.h"
#include "ortools/constraint_solver/diffn.h"
#include "ortools/constraint_solver/element.h"
#include "ortools/constraint_solver/expr_array.h"
#include "ortools/constraint_solver/expr_cst.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/graph_constraints.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/local_search.h"
#include "ortools/constraint_solver/model_cache.h"
#include "ortools/constraint_solver/sequence_var.h"
#include "ortools/constraint_solver/table.h"
#include "ortools/constraint_solver/timetabling.h"
#include "ortools/constraint_solver/trace.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/port/sysinfo.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/string_array.h"
#include "ortools/util/tuple_set.h"
#include "zlib.h"

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
ABSL_FLAG(std::string, cp_profile_file, "",
          "Export profiling overview to file.");
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
// Calls the given method with the provided arguments on all objects in the
// collection.
template <typename T, typename MethodPointer, typename... Args>
void ForAll(const std::vector<T*>& objects, MethodPointer method,
            const Args&... args) {
  for (T* const object : objects) {
    DCHECK(object != nullptr);
    (object->*method)(args...);
  }
}

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
        stamp_(1),
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
  uint64_t stamp_;
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

// ------------------ StateMarker / StateInfo struct -----------

struct StateInfo {  // This is an internal structure to store
                    // additional information on the choice point.
 public:
  StateInfo()
      : ptr_info(nullptr),
        int_info(0),
        depth(0),
        left_depth(0),
        reversible_action(nullptr) {}
  StateInfo(void* pinfo, int iinfo)
      : ptr_info(pinfo),
        int_info(iinfo),
        depth(0),
        left_depth(0),
        reversible_action(nullptr) {}
  StateInfo(void* pinfo, int iinfo, int d, int ld)
      : ptr_info(pinfo),
        int_info(iinfo),
        depth(d),
        left_depth(ld),
        reversible_action(nullptr) {}
  StateInfo(Solver::Action a, bool fast)
      : ptr_info(nullptr),
        int_info(static_cast<int>(fast)),
        depth(0),
        left_depth(0),
        reversible_action(std::move(a)) {}

  void* ptr_info;
  int int_info;
  int depth;
  int left_depth;
  Solver::Action reversible_action;
};

struct StateMarker {
 public:
  StateMarker(Solver::MarkerType t, const StateInfo& info);
  friend class Solver;
  friend struct Trail;

 private:
  Solver::MarkerType type;
  int rev_int_index;
  int rev_int64_index;
  int rev_uint64_index;
  int rev_double_index;
  int rev_ptr_index;
  int rev_boolvar_list_index;
  int rev_bools_index;
  int rev_int_memory_index;
  int rev_int64_memory_index;
  int rev_double_memory_index;
  int rev_object_memory_index;
  int rev_object_array_memory_index;
  int rev_memory_index;
  int rev_memory_array_index;
  StateInfo info;
};

StateMarker::StateMarker(Solver::MarkerType t, const StateInfo& i)
    : type(t),
      rev_int_index(0),
      rev_int64_index(0),
      rev_uint64_index(0),
      rev_double_index(0),
      rev_ptr_index(0),
      rev_boolvar_list_index(0),
      rev_bools_index(0),
      rev_int_memory_index(0),
      rev_int64_memory_index(0),
      rev_double_memory_index(0),
      rev_object_memory_index(0),
      rev_object_array_memory_index(0),
      info(i) {}

// ---------- Trail and Reversibility ----------

namespace {
// ----- addrval struct -----

// This template class is used internally to implement reversibility.
// It stores an address and the value that was at the address.
template <class T>
struct addrval {
 public:
  addrval() : address(nullptr) {}
  explicit addrval(T* adr) : address(adr), old_value(*adr) {}
  void restore() const { (*address) = old_value; }

 private:
  T* address;
  T old_value;
};

// ----- Compressed trail -----

// ---------- Trail Packer ---------
// Abstract class to pack trail blocks.

template <class T>
class TrailPacker {
 public:
  explicit TrailPacker(int block_size) : block_size_(block_size) {}

  // This type is neither copyable nor movable.
  TrailPacker(const TrailPacker&) = delete;
  TrailPacker& operator=(const TrailPacker&) = delete;
  virtual ~TrailPacker() {}
  int input_size() const { return block_size_ * sizeof(addrval<T>); }
  virtual void Pack(const addrval<T>* block, std::string* packed_block) = 0;
  virtual void Unpack(const std::string& packed_block, addrval<T>* block) = 0;

 private:
  const int block_size_;
};

template <class T>
class NoCompressionTrailPacker : public TrailPacker<T> {
 public:
  explicit NoCompressionTrailPacker(int block_size)
      : TrailPacker<T>(block_size) {}

  // This type is neither copyable nor movable.
  NoCompressionTrailPacker(const NoCompressionTrailPacker&) = delete;
  NoCompressionTrailPacker& operator=(const NoCompressionTrailPacker&) = delete;
  ~NoCompressionTrailPacker() override {}
  void Pack(const addrval<T>* block, std::string* packed_block) override {
    DCHECK(block != nullptr);
    DCHECK(packed_block != nullptr);
    absl::string_view block_str(reinterpret_cast<const char*>(block),
                                this->input_size());
    packed_block->assign(block_str.data(), block_str.size());
  }
  void Unpack(const std::string& packed_block, addrval<T>* block) override {
    DCHECK(block != nullptr);
    memcpy(block, packed_block.c_str(), packed_block.size());
  }
};

template <class T>
class ZlibTrailPacker : public TrailPacker<T> {
 public:
  explicit ZlibTrailPacker(int block_size)
      : TrailPacker<T>(block_size),
        tmp_size_(compressBound(this->input_size())),
        tmp_block_(new char[tmp_size_]) {}

  // This type is neither copyable nor movable.
  ZlibTrailPacker(const ZlibTrailPacker&) = delete;
  ZlibTrailPacker& operator=(const ZlibTrailPacker&) = delete;

  ~ZlibTrailPacker() override {}

  void Pack(const addrval<T>* block, std::string* packed_block) override {
    DCHECK(block != nullptr);
    DCHECK(packed_block != nullptr);
    uLongf size = tmp_size_;
    const int result =
        compress(reinterpret_cast<Bytef*>(tmp_block_.get()), &size,
                 reinterpret_cast<const Bytef*>(block), this->input_size());
    CHECK_EQ(Z_OK, result);
    absl::string_view block_str;
    block_str = absl::string_view(tmp_block_.get(), size);
    packed_block->assign(block_str.data(), block_str.size());
  }

  void Unpack(const std::string& packed_block, addrval<T>* block) override {
    DCHECK(block != nullptr);
    uLongf size = this->input_size();
    const int result =
        uncompress(reinterpret_cast<Bytef*>(block), &size,
                   reinterpret_cast<const Bytef*>(packed_block.c_str()),
                   packed_block.size());
    CHECK_EQ(Z_OK, result);
  }

 private:
  const uint64_t tmp_size_;
  std::unique_ptr<char[]> tmp_block_;
};

template <class T>
class CompressedTrail {
 public:
  CompressedTrail(
      int block_size,
      ConstraintSolverParameters::TrailCompression compression_level)
      : block_size_(block_size),
        blocks_(nullptr),
        free_blocks_(nullptr),
        data_(new addrval<T>[block_size]),
        buffer_(new addrval<T>[block_size]),
        buffer_used_(false),
        current_(0),
        size_(0) {
    switch (compression_level) {
      case ConstraintSolverParameters::NO_COMPRESSION: {
        packer_.reset(new NoCompressionTrailPacker<T>(block_size));
        break;
      }
      case ConstraintSolverParameters::COMPRESS_WITH_ZLIB: {
        packer_.reset(new ZlibTrailPacker<T>(block_size));
        break;
      }
      default: {
        LOG(ERROR) << "Should not be here";
      }
    }

    // We zero all memory used by addrval arrays.
    // Because of padding, all bytes may not be initialized, while compression
    // will read them all, even if the uninitialized bytes are never used.
    // This makes valgrind happy.

    memset(data_.get(), 0, sizeof(*data_.get()) * block_size);
    memset(buffer_.get(), 0, sizeof(*buffer_.get()) * block_size);
  }
  ~CompressedTrail() {
    FreeBlocks(blocks_);
    FreeBlocks(free_blocks_);
  }
  const addrval<T>& Back() const {
    // Back of empty trail.
    DCHECK_GT(current_, 0);
    return data_[current_ - 1];
  }
  void PopBack() {
    if (size_ > 0) {
      --current_;
      if (current_ <= 0) {
        if (buffer_used_) {
          data_.swap(buffer_);
          current_ = block_size_;
          buffer_used_ = false;
        } else if (blocks_ != nullptr) {
          packer_->Unpack(blocks_->compressed, data_.get());
          FreeTopBlock();
          current_ = block_size_;
        }
      }
      --size_;
    }
  }
  void PushBack(const addrval<T>& addr_val) {
    if (current_ >= block_size_) {
      if (buffer_used_) {  // Buffer is used.
        NewTopBlock();
        packer_->Pack(buffer_.get(), &blocks_->compressed);
        // O(1) operation.
        data_.swap(buffer_);
      } else {
        data_.swap(buffer_);
        buffer_used_ = true;
      }
      current_ = 0;
    }
    data_[current_] = addr_val;
    ++current_;
    ++size_;
  }
  int64_t size() const { return size_; }

 private:
  struct Block {
    std::string compressed;
    Block* next;
  };

  void FreeTopBlock() {
    Block* block = blocks_;
    blocks_ = block->next;
    block->compressed.clear();
    block->next = free_blocks_;
    free_blocks_ = block;
  }
  void NewTopBlock() {
    Block* block = nullptr;
    if (free_blocks_ != nullptr) {
      block = free_blocks_;
      free_blocks_ = block->next;
    } else {
      block = new Block;
    }
    block->next = blocks_;
    blocks_ = block;
  }
  void FreeBlocks(Block* blocks) {
    while (nullptr != blocks) {
      Block* next = blocks->next;
      delete blocks;
      blocks = next;
    }
  }

  std::unique_ptr<TrailPacker<T>> packer_;
  const int block_size_;
  Block* blocks_;
  Block* free_blocks_;
  std::unique_ptr<addrval<T>[]> data_;
  std::unique_ptr<addrval<T>[]> buffer_;
  bool buffer_used_;
  int current_;
  int size_;
};
}  // namespace

// ----- Trail -----

// Object are explicitly copied using the copy ctor instead of
// passing and storing a pointer. As objects are small, copying is
// much faster than allocating (around 35% on a complete solve).

extern void RestoreBoolValue(IntVar* var);

struct Trail {
  CompressedTrail<int> rev_ints;
  CompressedTrail<int64_t> rev_int64s;
  CompressedTrail<uint64_t> rev_uint64s;
  CompressedTrail<double> rev_doubles;
  CompressedTrail<void*> rev_ptrs;
  std::vector<IntVar*> rev_boolvar_list;
  std::vector<bool*> rev_bools;
  std::vector<bool> rev_bool_value;
  std::vector<int*> rev_int_memory;
  std::vector<int64_t*> rev_int64_memory;
  std::vector<double*> rev_double_memory;
  std::vector<BaseObject*> rev_object_memory;
  std::vector<BaseObject**> rev_object_array_memory;
  std::vector<void*> rev_memory;
  std::vector<void**> rev_memory_array;

  Trail(int block_size,
        ConstraintSolverParameters::TrailCompression compression_level)
      : rev_ints(block_size, compression_level),
        rev_int64s(block_size, compression_level),
        rev_uint64s(block_size, compression_level),
        rev_doubles(block_size, compression_level),
        rev_ptrs(block_size, compression_level) {}

  void BacktrackTo(StateMarker* m) {
    int target = m->rev_int_index;
    for (int curr = rev_ints.size(); curr > target; --curr) {
      const addrval<int>& cell = rev_ints.Back();
      cell.restore();
      rev_ints.PopBack();
    }
    DCHECK_EQ(rev_ints.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_int64_index;
    for (int curr = rev_int64s.size(); curr > target; --curr) {
      const addrval<int64_t>& cell = rev_int64s.Back();
      cell.restore();
      rev_int64s.PopBack();
    }
    DCHECK_EQ(rev_int64s.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_uint64_index;
    for (int curr = rev_uint64s.size(); curr > target; --curr) {
      const addrval<uint64_t>& cell = rev_uint64s.Back();
      cell.restore();
      rev_uint64s.PopBack();
    }
    DCHECK_EQ(rev_uint64s.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_double_index;
    for (int curr = rev_doubles.size(); curr > target; --curr) {
      const addrval<double>& cell = rev_doubles.Back();
      cell.restore();
      rev_doubles.PopBack();
    }
    DCHECK_EQ(rev_doubles.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_ptr_index;
    for (int curr = rev_ptrs.size(); curr > target; --curr) {
      const addrval<void*>& cell = rev_ptrs.Back();
      cell.restore();
      rev_ptrs.PopBack();
    }
    DCHECK_EQ(rev_ptrs.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_boolvar_list_index;
    for (int curr = rev_boolvar_list.size() - 1; curr >= target; --curr) {
      IntVar* const var = rev_boolvar_list[curr];
      RestoreBoolValue(var);
    }
    rev_boolvar_list.resize(target);

    DCHECK_EQ(rev_bools.size(), rev_bool_value.size());
    target = m->rev_bools_index;
    for (int curr = rev_bools.size() - 1; curr >= target; --curr) {
      *(rev_bools[curr]) = rev_bool_value[curr];
    }
    rev_bools.resize(target);
    rev_bool_value.resize(target);

    target = m->rev_int_memory_index;
    for (int curr = rev_int_memory.size() - 1; curr >= target; --curr) {
      delete[] rev_int_memory[curr];
    }
    rev_int_memory.resize(target);

    target = m->rev_int64_memory_index;
    for (int curr = rev_int64_memory.size() - 1; curr >= target; --curr) {
      delete[] rev_int64_memory[curr];
    }
    rev_int64_memory.resize(target);

    target = m->rev_double_memory_index;
    for (int curr = rev_double_memory.size() - 1; curr >= target; --curr) {
      delete[] rev_double_memory[curr];
    }
    rev_double_memory.resize(target);

    target = m->rev_object_memory_index;
    for (int curr = rev_object_memory.size() - 1; curr >= target; --curr) {
      delete rev_object_memory[curr];
    }
    rev_object_memory.resize(target);

    target = m->rev_object_array_memory_index;
    for (int curr = rev_object_array_memory.size() - 1; curr >= target;
         --curr) {
      delete[] rev_object_array_memory[curr];
    }
    rev_object_array_memory.resize(target);

    target = m->rev_memory_index;
    for (int curr = rev_memory.size() - 1; curr >= target; --curr) {
      // Explicitly call unsized delete
      ::operator delete(reinterpret_cast<char*>(rev_memory[curr]));
      // The previous cast is necessary to deallocate generic memory
      // described by a void* when passed to the RevAlloc procedure
      // We cannot do a delete[] there
      // This is useful for cells of RevFIFO and should not be used outside
      // of the product
    }
    rev_memory.resize(target);

    target = m->rev_memory_array_index;
    for (int curr = rev_memory_array.size() - 1; curr >= target; --curr) {
      delete[] rev_memory_array[curr];
      // delete [] version of the previous unsafe case.
    }
    rev_memory_array.resize(target);
  }
};

void Solver::InternalSaveValue(int* valptr) {
  trail_->rev_ints.PushBack(addrval<int>(valptr));
}

void Solver::InternalSaveValue(int64_t* valptr) {
  trail_->rev_int64s.PushBack(addrval<int64_t>(valptr));
}

void Solver::InternalSaveValue(uint64_t* valptr) {
  trail_->rev_uint64s.PushBack(addrval<uint64_t>(valptr));
}

void Solver::InternalSaveValue(double* valptr) {
  trail_->rev_doubles.PushBack(addrval<double>(valptr));
}

void Solver::InternalSaveValue(void** valptr) {
  trail_->rev_ptrs.PushBack(addrval<void*>(valptr));
}

// TODO(user) : this code is unsafe if you save the same alternating
// bool multiple times.
// The correct code should use a bitset and a single list.
void Solver::InternalSaveValue(bool* valptr) {
  trail_->rev_bools.push_back(valptr);
  trail_->rev_bool_value.push_back(*valptr);
}

BaseObject* Solver::SafeRevAlloc(BaseObject* ptr) {
  check_alloc_state();
  trail_->rev_object_memory.push_back(ptr);
  return ptr;
}

int* Solver::SafeRevAllocArray(int* ptr) {
  check_alloc_state();
  trail_->rev_int_memory.push_back(ptr);
  return ptr;
}

int64_t* Solver::SafeRevAllocArray(int64_t* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory.push_back(ptr);
  return ptr;
}

double* Solver::SafeRevAllocArray(double* ptr) {
  check_alloc_state();
  trail_->rev_double_memory.push_back(ptr);
  return ptr;
}

uint64_t* Solver::SafeRevAllocArray(uint64_t* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory.push_back(reinterpret_cast<int64_t*>(ptr));
  return ptr;
}

BaseObject** Solver::SafeRevAllocArray(BaseObject** ptr) {
  check_alloc_state();
  trail_->rev_object_array_memory.push_back(ptr);
  return ptr;
}

IntVar** Solver::SafeRevAllocArray(IntVar** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntVar**>(in);
}

IntExpr** Solver::SafeRevAllocArray(IntExpr** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntExpr**>(in);
}

Constraint** Solver::SafeRevAllocArray(Constraint** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<Constraint**>(in);
}

void* Solver::UnsafeRevAllocAux(void* ptr) {
  check_alloc_state();
  trail_->rev_memory.push_back(ptr);
  return ptr;
}

void** Solver::UnsafeRevAllocArrayAux(void** ptr) {
  check_alloc_state();
  trail_->rev_memory_array.push_back(ptr);
  return ptr;
}

void InternalSaveBooleanVarValue(Solver* solver, IntVar* var) {
  solver->trail_->rev_boolvar_list.push_back(var);
}

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

// These magic numbers are there to make sure we pop the correct
// sentinels throughout the search.
namespace {
enum SentinelMarker {
  INITIAL_SEARCH_SENTINEL = 10000000,
  ROOT_NODE_SENTINEL = 20000000,
  SOLVER_CTOR_SENTINEL = 40000000
};
}  // namespace

extern PropagationMonitor* BuildTrace(Solver* s);
extern LocalSearchMonitor* BuildLocalSearchMonitorPrimary(Solver* s);
extern ModelCache* BuildModelCache(Solver* solver);

std::string Solver::model_name() const { return name_; }

namespace {
void CheckSolverParameters(const ConstraintSolverParameters& parameters) {
  CHECK_GT(parameters.array_split_size(), 0)
      << "Were parameters built using Solver::DefaultSolverParameters() ?";
}
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
  queue_ = std::make_unique<Queue>(this);
  trail_ = std::make_unique<Trail>(parameters_.trail_block_size(),
                                   parameters_.compress_trail());
  state_ = OUTSIDE_SEARCH;
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
  model_cache_.reset(BuildModelCache(this));

  AddLocalSearchMonitor(
      reinterpret_cast<LocalSearchMonitor*>(local_search_profiler_));
}

Solver::~Solver() {
  // solver destructor called with searches open.
  CHECK_EQ(2, searches_.size());
  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);

  StateInfo info;
  Solver::MarkerType finalType = PopState(&info);
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
  Solver::MarkerType t = PopState(&info);
  CHECK_EQ(SIMPLE_MARKER, t);
}

void Solver::PushState(Solver::MarkerType t, const StateInfo& info) {
  StateMarker* m = new StateMarker(t, info);
  if (t != REVERSIBLE_ACTION || info.int_info == 0) {
    m->rev_int_index = trail_->rev_ints.size();
    m->rev_int64_index = trail_->rev_int64s.size();
    m->rev_uint64_index = trail_->rev_uint64s.size();
    m->rev_double_index = trail_->rev_doubles.size();
    m->rev_ptr_index = trail_->rev_ptrs.size();
    m->rev_boolvar_list_index = trail_->rev_boolvar_list.size();
    m->rev_bools_index = trail_->rev_bools.size();
    m->rev_int_memory_index = trail_->rev_int_memory.size();
    m->rev_int64_memory_index = trail_->rev_int64_memory.size();
    m->rev_double_memory_index = trail_->rev_double_memory.size();
    m->rev_object_memory_index = trail_->rev_object_memory.size();
    m->rev_object_array_memory_index = trail_->rev_object_array_memory.size();
    m->rev_memory_index = trail_->rev_memory.size();
    m->rev_memory_array_index = trail_->rev_memory_array.size();
  }
  searches_.back()->marker_stack_.push_back(m);
  queue_->increase_stamp();
}

void Solver::AddBacktrackAction(Action a, bool fast) {
  StateInfo info(std::move(a), fast);
  PushState(REVERSIBLE_ACTION, info);
}

Solver::MarkerType Solver::PopState(StateInfo* info) {
  CHECK(!searches_.back()->marker_stack_.empty())
      << "PopState() on an empty stack";
  CHECK(info != nullptr);
  StateMarker* const m = searches_.back()->marker_stack_.back();
  if (m->type != REVERSIBLE_ACTION || m->info.int_info == 0) {
    trail_->BacktrackTo(m);
  }
  Solver::MarkerType t = m->type;
  (*info) = m->info;
  searches_.back()->marker_stack_.pop_back();
  delete m;
  queue_->increase_stamp();
  return t;
}

void Solver::check_alloc_state() {
  switch (state_) {
    case OUTSIDE_SEARCH:
    case IN_ROOT_NODE:
    case IN_SEARCH:
    case NO_MORE_SOLUTIONS:
    case PROBLEM_INFEASIBLE:
      break;
    case AT_SOLUTION:
      LOG(FATAL) << "allocating at a leaf node";
    default:
      LOG(FATAL) << "This switch was supposed to be exhaustive, but it is not!";
  }
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

uint64_t Solver::stamp() const { return queue_->stamp(); }

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
    Solver::MarkerType t = PopState(&info);
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
    Solver::MarkerType t = PopState(&info);
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

// ---------- ModelVisitor ----------

// Tags for constraints, arguments, extensions.

const char ModelVisitor::kAbs[] = "Abs";
const char ModelVisitor::kAbsEqual[] = "AbsEqual";
const char ModelVisitor::kAllDifferent[] = "AllDifferent";
const char ModelVisitor::kAllowedAssignments[] = "AllowedAssignments";
const char ModelVisitor::kAtMost[] = "AtMost";
const char ModelVisitor::kBetween[] = "Between";
const char ModelVisitor::kConditionalExpr[] = "ConditionalExpr";
const char ModelVisitor::kCircuit[] = "Circuit";
const char ModelVisitor::kConvexPiecewise[] = "ConvexPiecewise";
const char ModelVisitor::kCountEqual[] = "CountEqual";
const char ModelVisitor::kCover[] = "Cover";
const char ModelVisitor::kCumulative[] = "Cumulative";
const char ModelVisitor::kDeviation[] = "Deviation";
const char ModelVisitor::kDifference[] = "Difference";
const char ModelVisitor::kDisjunctive[] = "Disjunctive";
const char ModelVisitor::kDistribute[] = "Distribute";
const char ModelVisitor::kDivide[] = "Divide";
const char ModelVisitor::kDurationExpr[] = "DurationExpression";
const char ModelVisitor::kElement[] = "Element";
const char ModelVisitor::kLightElementEqual[] = "LightElementEqual";
const char ModelVisitor::kElementEqual[] = "ElementEqual";
const char ModelVisitor::kEndExpr[] = "EndExpression";
const char ModelVisitor::kEquality[] = "Equal";
const char ModelVisitor::kFalseConstraint[] = "FalseConstraint";
const char ModelVisitor::kGlobalCardinality[] = "GlobalCardinality";
const char ModelVisitor::kGreater[] = "Greater";
const char ModelVisitor::kGreaterOrEqual[] = "GreaterOrEqual";
const char ModelVisitor::kIndexOf[] = "IndexOf";
const char ModelVisitor::kIntegerVariable[] = "IntegerVariable";
const char ModelVisitor::kIntervalBinaryRelation[] = "IntervalBinaryRelation";
const char ModelVisitor::kIntervalDisjunction[] = "IntervalDisjunction";
const char ModelVisitor::kIntervalUnaryRelation[] = "IntervalUnaryRelation";
const char ModelVisitor::kIntervalVariable[] = "IntervalVariable";
const char ModelVisitor::kInversePermutation[] = "InversePermutation";
const char ModelVisitor::kIsBetween[] = "IsBetween;";
const char ModelVisitor::kIsDifferent[] = "IsDifferent";
const char ModelVisitor::kIsEqual[] = "IsEqual";
const char ModelVisitor::kIsGreater[] = "IsGreater";
const char ModelVisitor::kIsGreaterOrEqual[] = "IsGreaterOrEqual";
const char ModelVisitor::kIsLess[] = "IsLess";
const char ModelVisitor::kIsLessOrEqual[] = "IsLessOrEqual";
const char ModelVisitor::kIsMember[] = "IsMember;";
const char ModelVisitor::kLess[] = "Less";
const char ModelVisitor::kLessOrEqual[] = "LessOrEqual";
const char ModelVisitor::kLexLess[] = "LexLess";
const char ModelVisitor::kLinkExprVar[] = "CastExpressionIntoVariable";
const char ModelVisitor::kMapDomain[] = "MapDomain";
const char ModelVisitor::kMax[] = "Max";
const char ModelVisitor::kMaxEqual[] = "MaxEqual";
const char ModelVisitor::kMember[] = "Member";
const char ModelVisitor::kMin[] = "Min";
const char ModelVisitor::kMinEqual[] = "MinEqual";
const char ModelVisitor::kModulo[] = "Modulo";
const char ModelVisitor::kNoCycle[] = "NoCycle";
const char ModelVisitor::kNonEqual[] = "NonEqual";
const char ModelVisitor::kNotBetween[] = "NotBetween";
const char ModelVisitor::kNotMember[] = "NotMember";
const char ModelVisitor::kNullIntersect[] = "NullIntersect";
const char ModelVisitor::kOpposite[] = "Opposite";
const char ModelVisitor::kPack[] = "Pack";
const char ModelVisitor::kPathCumul[] = "PathCumul";
const char ModelVisitor::kDelayedPathCumul[] = "DelayedPathCumul";
const char ModelVisitor::kPerformedExpr[] = "PerformedExpression";
const char ModelVisitor::kPower[] = "Power";
const char ModelVisitor::kProduct[] = "Product";
const char ModelVisitor::kScalProd[] = "ScalarProduct";
const char ModelVisitor::kScalProdEqual[] = "ScalarProductEqual";
const char ModelVisitor::kScalProdGreaterOrEqual[] =
    "ScalarProductGreaterOrEqual";
const char ModelVisitor::kScalProdLessOrEqual[] = "ScalarProductLessOrEqual";
const char ModelVisitor::kSemiContinuous[] = "SemiContinuous";
const char ModelVisitor::kSequenceVariable[] = "SequenceVariable";
const char ModelVisitor::kSortingConstraint[] = "SortingConstraint";
const char ModelVisitor::kSquare[] = "Square";
const char ModelVisitor::kStartExpr[] = "StartExpression";
const char ModelVisitor::kSum[] = "Sum";
const char ModelVisitor::kSumEqual[] = "SumEqual";
const char ModelVisitor::kSumGreaterOrEqual[] = "SumGreaterOrEqual";
const char ModelVisitor::kSumLessOrEqual[] = "SumLessOrEqual";
const char ModelVisitor::kTransition[] = "Transition";
const char ModelVisitor::kTrace[] = "Trace";
const char ModelVisitor::kTrueConstraint[] = "TrueConstraint";
const char ModelVisitor::kVarBoundWatcher[] = "VarBoundWatcher";
const char ModelVisitor::kVarValueWatcher[] = "VarValueWatcher";

const char ModelVisitor::kCountAssignedItemsExtension[] = "CountAssignedItems";
const char ModelVisitor::kCountUsedBinsExtension[] = "CountUsedBins";
const char ModelVisitor::kInt64ToBoolExtension[] = "Int64ToBoolFunction";
const char ModelVisitor::kInt64ToInt64Extension[] = "Int64ToInt64Function";
const char ModelVisitor::kObjectiveExtension[] = "Objective";
const char ModelVisitor::kSearchLimitExtension[] = "SearchLimit";
const char ModelVisitor::kUsageEqualVariableExtension[] = "UsageEqualVariable";

const char ModelVisitor::kUsageLessConstantExtension[] = "UsageLessConstant";
const char ModelVisitor::kVariableGroupExtension[] = "VariableGroup";
const char ModelVisitor::kVariableUsageLessConstantExtension[] =
    "VariableUsageLessConstant";
const char ModelVisitor::kWeightedSumOfAssignedEqualVariableExtension[] =
    "WeightedSumOfAssignedEqualVariable";

const char ModelVisitor::kActiveArgument[] = "active";
const char ModelVisitor::kAssumePathsArgument[] = "assume_paths";
const char ModelVisitor::kBranchesLimitArgument[] = "branches_limit";
const char ModelVisitor::kCapacityArgument[] = "capacity";
const char ModelVisitor::kCardsArgument[] = "cardinalities";
const char ModelVisitor::kCoefficientsArgument[] = "coefficients";
const char ModelVisitor::kCountArgument[] = "count";
const char ModelVisitor::kCumulativeArgument[] = "cumulative";
const char ModelVisitor::kCumulsArgument[] = "cumuls";
const char ModelVisitor::kDemandsArgument[] = "demands";
const char ModelVisitor::kDurationMinArgument[] = "duration_min";
const char ModelVisitor::kDurationMaxArgument[] = "duration_max";
const char ModelVisitor::kEarlyCostArgument[] = "early_cost";
const char ModelVisitor::kEarlyDateArgument[] = "early_date";
const char ModelVisitor::kEndMinArgument[] = "end_min";
const char ModelVisitor::kEndMaxArgument[] = "end_max";
const char ModelVisitor::kEndsArgument[] = "ends";
const char ModelVisitor::kExpressionArgument[] = "expression";
const char ModelVisitor::kFailuresLimitArgument[] = "failures_limit";
const char ModelVisitor::kFinalStatesArgument[] = "final_states";
const char ModelVisitor::kFixedChargeArgument[] = "fixed_charge";
const char ModelVisitor::kIndex2Argument[] = "index2";
const char ModelVisitor::kIndex3Argument[] = "index3";
const char ModelVisitor::kIndexArgument[] = "index";
const char ModelVisitor::kInitialState[] = "initial_state";
const char ModelVisitor::kIntervalArgument[] = "interval";
const char ModelVisitor::kIntervalsArgument[] = "intervals";
const char ModelVisitor::kLateCostArgument[] = "late_cost";
const char ModelVisitor::kLateDateArgument[] = "late_date";
const char ModelVisitor::kLeftArgument[] = "left";
const char ModelVisitor::kMaxArgument[] = "max_value";
const char ModelVisitor::kMaximizeArgument[] = "maximize";
const char ModelVisitor::kMinArgument[] = "min_value";
const char ModelVisitor::kModuloArgument[] = "modulo";
const char ModelVisitor::kNextsArgument[] = "nexts";
const char ModelVisitor::kOptionalArgument[] = "optional";
const char ModelVisitor::kPartialArgument[] = "partial";
const char ModelVisitor::kPositionXArgument[] = "position_x";
const char ModelVisitor::kPositionYArgument[] = "position_y";
const char ModelVisitor::kRangeArgument[] = "range";
const char ModelVisitor::kRelationArgument[] = "relation";
const char ModelVisitor::kRightArgument[] = "right";
const char ModelVisitor::kSequenceArgument[] = "sequence";
const char ModelVisitor::kSequencesArgument[] = "sequences";
const char ModelVisitor::kSmartTimeCheckArgument[] = "smart_time_check";
const char ModelVisitor::kSizeArgument[] = "size";
const char ModelVisitor::kSizeXArgument[] = "size_x";
const char ModelVisitor::kSizeYArgument[] = "size_y";
const char ModelVisitor::kSolutionLimitArgument[] = "solutions_limit";
const char ModelVisitor::kStartMinArgument[] = "start_min";
const char ModelVisitor::kStartMaxArgument[] = "start_max";
const char ModelVisitor::kStartsArgument[] = "starts";
const char ModelVisitor::kStepArgument[] = "step";
const char ModelVisitor::kTargetArgument[] = "target_variable";
const char ModelVisitor::kTimeLimitArgument[] = "time_limit";
const char ModelVisitor::kTransitsArgument[] = "transits";
const char ModelVisitor::kTuplesArgument[] = "tuples";
const char ModelVisitor::kValueArgument[] = "value";
const char ModelVisitor::kValuesArgument[] = "values";
const char ModelVisitor::kVarsArgument[] = "variables";
const char ModelVisitor::kEvaluatorArgument[] = "evaluator";

const char ModelVisitor::kVariableArgument[] = "variable";

const char ModelVisitor::kMirrorOperation[] = "mirror";
const char ModelVisitor::kRelaxedMaxOperation[] = "relaxed_max";
const char ModelVisitor::kRelaxedMinOperation[] = "relaxed_min";
const char ModelVisitor::kSumOperation[] = "sum";
const char ModelVisitor::kDifferenceOperation[] = "difference";
const char ModelVisitor::kProductOperation[] = "product";
const char ModelVisitor::kStartSyncOnStartOperation[] = "start_synced_on_start";
const char ModelVisitor::kStartSyncOnEndOperation[] = "start_synced_on_end";
const char ModelVisitor::kTraceOperation[] = "trace";

// Methods

ModelVisitor::~ModelVisitor() {}

void ModelVisitor::BeginVisitModel(
    [[maybe_unused]] const std::string& type_name) {}
void ModelVisitor::EndVisitModel(
    [[maybe_unused]] const std::string& type_name) {}

void ModelVisitor::BeginVisitConstraint(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const Constraint* constraint) {}
void ModelVisitor::EndVisitConstraint(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const Constraint* constraint) {}

void ModelVisitor::BeginVisitExtension(
    [[maybe_unused]] const std::string& type) {}
void ModelVisitor::EndVisitExtension([[maybe_unused]] const std::string& type) {
}

void ModelVisitor::BeginVisitIntegerExpression(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const IntExpr* expr) {}
void ModelVisitor::EndVisitIntegerExpression(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const IntExpr* expr) {}

void ModelVisitor::VisitIntegerVariable([[maybe_unused]] const IntVar* variable,
                                        IntExpr* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntegerVariable(
    [[maybe_unused]] const IntVar* variable,
    [[maybe_unused]] const std::string& operation,
    [[maybe_unused]] int64_t value, IntVar* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntervalVariable(
    [[maybe_unused]] const IntervalVar* variable,
    [[maybe_unused]] const std::string& operation,
    [[maybe_unused]] int64_t value, IntervalVar* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitSequenceVariable(const SequenceVar* variable) {
  for (int i = 0; i < variable->size(); ++i) {
    variable->Interval(i)->Accept(this);
  }
}

void ModelVisitor::VisitIntegerArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] int64_t value) {}

void ModelVisitor::VisitIntegerArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const std::vector<int64_t>& values) {}

void ModelVisitor::VisitIntegerMatrixArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const IntTupleSet& tuples) {}

void ModelVisitor::VisitIntegerExpressionArgument(
    [[maybe_unused]] const std::string& arg_name, IntExpr* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntegerVariableEvaluatorArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const Solver::Int64ToIntVar& arguments) {}

void ModelVisitor::VisitIntegerVariableArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<IntVar*>& arguments) {
  ForAll(arguments, &IntVar::Accept, this);
}

void ModelVisitor::VisitIntervalArgument(
    [[maybe_unused]] const std::string& arg_name, IntervalVar* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntervalArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<IntervalVar*>& arguments) {
  ForAll(arguments, &IntervalVar::Accept, this);
}

void ModelVisitor::VisitSequenceArgument(
    [[maybe_unused]] const std::string& arg_name, SequenceVar* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitSequenceArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<SequenceVar*>& arguments) {
  ForAll(arguments, &SequenceVar::Accept, this);
}

// ----- Helpers -----

void ModelVisitor::VisitInt64ToBoolExtension(Solver::IndexFilter1 filter,
                                             int64_t index_min,
                                             int64_t index_max) {
  if (filter != nullptr) {
    std::vector<int64_t> cached_results;
    for (int i = index_min; i <= index_max; ++i) {
      cached_results.push_back(filter(i));
    }
    BeginVisitExtension(kInt64ToBoolExtension);
    VisitIntegerArgument(kMinArgument, index_min);
    VisitIntegerArgument(kMaxArgument, index_max);
    VisitIntegerArrayArgument(kValuesArgument, cached_results);
    EndVisitExtension(kInt64ToBoolExtension);
  }
}

void ModelVisitor::VisitInt64ToInt64Extension(
    const Solver::IndexEvaluator1& eval, int64_t index_min, int64_t index_max) {
  CHECK(eval != nullptr);
  std::vector<int64_t> cached_results;
  for (int i = index_min; i <= index_max; ++i) {
    cached_results.push_back(eval(i));
  }
  BeginVisitExtension(kInt64ToInt64Extension);
  VisitIntegerArgument(kMinArgument, index_min);
  VisitIntegerArgument(kMaxArgument, index_max);
  VisitIntegerArrayArgument(kValuesArgument, cached_results);
  EndVisitExtension(kInt64ToInt64Extension);
}

void ModelVisitor::VisitInt64ToInt64AsArray(const Solver::IndexEvaluator1& eval,
                                            const std::string& arg_name,
                                            int64_t index_max) {
  CHECK(eval != nullptr);
  std::vector<int64_t> cached_results;
  for (int i = 0; i <= index_max; ++i) {
    cached_results.push_back(eval(i));
  }
  VisitIntegerArrayArgument(arg_name, cached_results);
}

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

// ---------- Trace ----------

class Trace : public PropagationMonitor {
 public:
  explicit Trace(Solver* s) : PropagationMonitor(s) {}

  ~Trace() override {}

  void BeginConstraintInitialPropagation(Constraint* constraint) override {
    ForAll(monitors_, &PropagationMonitor::BeginConstraintInitialPropagation,
           constraint);
  }

  void EndConstraintInitialPropagation(Constraint* constraint) override {
    ForAll(monitors_, &PropagationMonitor::EndConstraintInitialPropagation,
           constraint);
  }

  void BeginNestedConstraintInitialPropagation(Constraint* parent,
                                               Constraint* nested) override {
    ForAll(monitors_,
           &PropagationMonitor::BeginNestedConstraintInitialPropagation, parent,
           nested);
  }

  void EndNestedConstraintInitialPropagation(Constraint* parent,
                                             Constraint* nested) override {
    ForAll(monitors_,
           &PropagationMonitor::EndNestedConstraintInitialPropagation, parent,
           nested);
  }

  void RegisterDemon(Demon* demon) override {
    ForAll(monitors_, &PropagationMonitor::RegisterDemon, demon);
  }

  void BeginDemonRun(Demon* demon) override {
    ForAll(monitors_, &PropagationMonitor::BeginDemonRun, demon);
  }

  void EndDemonRun(Demon* demon) override {
    ForAll(monitors_, &PropagationMonitor::EndDemonRun, demon);
  }

  void StartProcessingIntegerVariable(IntVar* var) override {
    ForAll(monitors_, &PropagationMonitor::StartProcessingIntegerVariable, var);
  }

  void EndProcessingIntegerVariable(IntVar* var) override {
    ForAll(monitors_, &PropagationMonitor::EndProcessingIntegerVariable, var);
  }

  void PushContext(const std::string& context) override {
    ForAll(monitors_, &PropagationMonitor::PushContext, context);
  }

  void PopContext() override {
    ForAll(monitors_, &PropagationMonitor::PopContext);
  }

  // IntExpr modifiers.
  void SetMin(IntExpr* expr, int64_t new_min) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetMin(expr, new_min);
    }
  }

  void SetMax(IntExpr* expr, int64_t new_max) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetMax(expr, new_max);
    }
  }

  void SetRange(IntExpr* expr, int64_t new_min, int64_t new_max) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetRange(expr, new_min, new_max);
    }
  }

  // IntVar modifiers.
  void SetMin(IntVar* var, int64_t new_min) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetMin(var, new_min);
    }
  }

  void SetMax(IntVar* var, int64_t new_max) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetMax(var, new_max);
    }
  }

  void SetRange(IntVar* var, int64_t new_min, int64_t new_max) override {
    for (PropagationMonitor* const monitor : monitors_) {
      monitor->SetRange(var, new_min, new_max);
    }
  }

  void RemoveValue(IntVar* var, int64_t value) override {
    ForAll(monitors_, &PropagationMonitor::RemoveValue, var, value);
  }

  void SetValue(IntVar* var, int64_t value) override {
    ForAll(monitors_, &PropagationMonitor::SetValue, var, value);
  }

  void RemoveInterval(IntVar* var, int64_t imin, int64_t imax) override {
    ForAll(monitors_, &PropagationMonitor::RemoveInterval, var, imin, imax);
  }

  void SetValues(IntVar* var, const std::vector<int64_t>& values) override {
    ForAll(monitors_, &PropagationMonitor::SetValues, var, values);
  }

  void RemoveValues(IntVar* var, const std::vector<int64_t>& values) override {
    ForAll(monitors_, &PropagationMonitor::RemoveValues, var, values);
  }

  // IntervalVar modifiers.
  void SetStartMin(IntervalVar* var, int64_t new_min) override {
    ForAll(monitors_, &PropagationMonitor::SetStartMin, var, new_min);
  }

  void SetStartMax(IntervalVar* var, int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetStartMax, var, new_max);
  }

  void SetStartRange(IntervalVar* var, int64_t new_min,
                     int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetStartRange, var, new_min,
           new_max);
  }

  void SetEndMin(IntervalVar* var, int64_t new_min) override {
    ForAll(monitors_, &PropagationMonitor::SetEndMin, var, new_min);
  }

  void SetEndMax(IntervalVar* var, int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetEndMax, var, new_max);
  }

  void SetEndRange(IntervalVar* var, int64_t new_min,
                   int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetEndRange, var, new_min, new_max);
  }

  void SetDurationMin(IntervalVar* var, int64_t new_min) override {
    ForAll(monitors_, &PropagationMonitor::SetDurationMin, var, new_min);
  }

  void SetDurationMax(IntervalVar* var, int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetDurationMax, var, new_max);
  }

  void SetDurationRange(IntervalVar* var, int64_t new_min,
                        int64_t new_max) override {
    ForAll(monitors_, &PropagationMonitor::SetDurationRange, var, new_min,
           new_max);
  }

  void SetPerformed(IntervalVar* var, bool value) override {
    ForAll(monitors_, &PropagationMonitor::SetPerformed, var, value);
  }

  void RankFirst(SequenceVar* var, int index) override {
    ForAll(monitors_, &PropagationMonitor::RankFirst, var, index);
  }

  void RankNotFirst(SequenceVar* var, int index) override {
    ForAll(monitors_, &PropagationMonitor::RankNotFirst, var, index);
  }

  void RankLast(SequenceVar* var, int index) override {
    ForAll(monitors_, &PropagationMonitor::RankLast, var, index);
  }

  void RankNotLast(SequenceVar* var, int index) override {
    ForAll(monitors_, &PropagationMonitor::RankNotLast, var, index);
  }

  void RankSequence(SequenceVar* var, const std::vector<int>& rank_first,
                    const std::vector<int>& rank_last,
                    const std::vector<int>& unperformed) override {
    ForAll(monitors_, &PropagationMonitor::RankSequence, var, rank_first,
           rank_last, unperformed);
  }

  // Does not take ownership of monitor.
  void Add(PropagationMonitor* monitor) {
    if (monitor != nullptr) {
      monitors_.push_back(monitor);
    }
  }

  // The trace will dispatch propagation events. It needs to listen to search
  // events.
  void Install() override { SearchMonitor::Install(); }

  std::string DebugString() const override { return "Trace"; }

 private:
  std::vector<PropagationMonitor*> monitors_;
};

PropagationMonitor* BuildTrace(Solver* s) { return new Trace(s); }

void Solver::AddPropagationMonitor(PropagationMonitor* monitor) {
  // TODO(user): Check solver state?
  reinterpret_cast<class Trace*>(propagation_monitor_.get())->Add(monitor);
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

// ---------- Solver Factory ----------

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars) {
  return MakeAllDifferent(vars, true);
}

Constraint* Solver::MakeAllDifferent(const std::vector<IntVar*>& vars,
                                     bool stronger_propagation) {
  const int size = vars.size();
  for (int i = 0; i < size; ++i) {
    CHECK_EQ(this, vars[i]->solver());
  }
  if (size < 2) {
    return MakeTrueConstraint();
  } else if (size == 2) {
    return MakeNonEquality(const_cast<IntVar* const>(vars[0]),
                           const_cast<IntVar* const>(vars[1]));
  } else {
    if (stronger_propagation) {
      return RevAlloc(new BoundsAllDifferent(this, vars));
    } else {
      return RevAlloc(new ValueAllDifferent(this, vars));
    }
  }
}

Constraint* Solver::MakeSortingConstraint(const std::vector<IntVar*>& vars,
                                          const std::vector<IntVar*>& sorted) {
  CHECK_EQ(vars.size(), sorted.size());
  return RevAlloc(new SortConstraint(this, vars, sorted));
}

Constraint* Solver::MakeAllDifferentExcept(const std::vector<IntVar*>& vars,
                                           int64_t escape_value) {
  int escape_candidates = 0;
  for (int i = 0; i < vars.size(); ++i) {
    escape_candidates += (vars[i]->Contains(escape_value));
  }
  if (escape_candidates <= 1) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(new AllDifferentExcept(this, vars, escape_value));
  }
}

Constraint* Solver::MakeNullIntersect(const std::vector<IntVar*>& first_vars,
                                      const std::vector<IntVar*>& second_vars) {
  return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars));
}

Constraint* Solver::MakeNullIntersectExcept(
    const std::vector<IntVar*>& first_vars,
    const std::vector<IntVar*>& second_vars, int64_t escape_value) {
  int first_escape_candidates = 0;
  for (int i = 0; i < first_vars.size(); ++i) {
    first_escape_candidates += (first_vars[i]->Contains(escape_value));
  }
  int second_escape_candidates = 0;
  for (int i = 0; i < second_vars.size(); ++i) {
    second_escape_candidates += (second_vars[i]->Contains(escape_value));
  }
  if (first_escape_candidates == 0 || second_escape_candidates == 0) {
    return RevAlloc(
        new NullIntersectArrayExcept(this, first_vars, second_vars));
  } else {
    return RevAlloc(new NullIntersectArrayExcept(this, first_vars, second_vars,
                                                 escape_value));
  }
}

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

Constraint* Solver::MakeTrueConstraint() {
  DCHECK(true_constraint_ != nullptr);
  return true_constraint_;
}

Constraint* Solver::MakeFalseConstraint() {
  DCHECK(false_constraint_ != nullptr);
  return false_constraint_;
}

Constraint* Solver::MakeFalseConstraint(const std::string& explanation) {
  return RevAlloc(new FalseConstraint(this, explanation));
}

void Solver::InitCachedConstraint() {
  DCHECK(true_constraint_ == nullptr);
  true_constraint_ = RevAlloc(new TrueConstraint(this));
  DCHECK(false_constraint_ == nullptr);
  false_constraint_ = RevAlloc(new FalseConstraint(this));
}

Constraint* Solver::MakeMapDomain(IntVar* var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives));
}

Constraint* Solver::MakeLexicalLess(const std::vector<IntVar*>& left,
                                    const std::vector<IntVar*>& right) {
  std::vector<IntVar*> adjusted_left = left;
  adjusted_left.push_back(MakeIntConst(1));
  std::vector<IntVar*> adjusted_right = right;
  adjusted_right.push_back(MakeIntConst(0));
  return MakeLexicalLessOrEqualWithOffsets(
      std::move(adjusted_left), std::move(adjusted_right),
      std::vector<int64_t>(left.size() + 1, 1));
}

Constraint* Solver::MakeLexicalLessOrEqual(const std::vector<IntVar*>& left,
                                           const std::vector<IntVar*>& right) {
  return MakeLexicalLessOrEqualWithOffsets(
      left, right, std::vector<int64_t>(left.size(), 1));
}

Constraint* Solver::MakeLexicalLessOrEqualWithOffsets(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets) {
  return RevAlloc(new LexicalLessOrEqual(this, std::move(left),
                                         std::move(right), std::move(offsets)));
}

Constraint* Solver::MakeIsLexicalLessOrEqualWithOffsetsCt(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets, IntVar* boolvar) {
  std::vector<IntVar*> adjusted_left = std::move(left);
  adjusted_left.insert(adjusted_left.begin(), boolvar);
  std::vector<IntVar*> adjusted_right = std::move(right);
  adjusted_right.insert(adjusted_right.begin(), MakeIntConst(1));
  std::vector<int64_t> adjusted_offsets = std::move(offsets);
  adjusted_offsets.insert(adjusted_offsets.begin(), 1);
  return MakeLexicalLessOrEqualWithOffsets(std::move(adjusted_left),
                                           std::move(adjusted_right),
                                           std::move(adjusted_offsets));
}

Constraint* Solver::MakeInversePermutationConstraint(
    const std::vector<IntVar*>& left, const std::vector<IntVar*>& right) {
  return RevAlloc(new InversePermutationConstraint(this, left, right));
}

Constraint* Solver::MakeIndexOfFirstMaxValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  return RevAlloc(new IndexOfFirstMaxValue(this, index, vars));
}

Constraint* Solver::MakeIndexOfFirstMinValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  std::vector<IntVar*> opp_vars(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    opp_vars[i] = MakeOpposite(vars[i])->Var();
  }
  return RevAlloc(new IndexOfFirstMaxValue(this, index, opp_vars));
}

Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                              int64_t max_count) {
  std::vector<IntVar*> tmp_sum;
  for (int i = 0; i < vars.size(); ++i) {
    if (vars[i]->Contains(value)) {
      if (vars[i]->Bound()) {
        max_count--;
      } else {
        tmp_sum.push_back(MakeIsEqualCstVar(vars[i], value));
      }
    }
  }
  return MakeSumEquality(tmp_sum, max_count);
}

Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                              IntVar* max_count) {
  if (max_count->Bound()) {
    return MakeCount(vars, value, max_count->Min());
  } else {
    std::vector<IntVar*> tmp_sum;
    int64_t num_vars_bound_to_v = 0;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Contains(value)) {
        if (vars[i]->Bound()) {
          ++num_vars_bound_to_v;
        } else {
          tmp_sum.push_back(MakeIsEqualCstVar(vars[i], value));
        }
      }
    }
    return MakeSumEquality(tmp_sum,
                           MakeSum(max_count, -num_vars_bound_to_v)->Var());
  }
}

Constraint* Solver::MakeAtMost(std::vector<IntVar*> vars, int64_t value,
                               int64_t max_count) {
  CHECK_GE(max_count, 0);
  if (max_count >= vars.size()) {
    return MakeTrueConstraint();
  }
  return RevAlloc(new AtMost(this, std::move(vars), value, max_count));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& values,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  CHECK_EQ(values.size(), cards.size());
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }

  // TODO(user) : we can sort values (and cards) before doing the test.
  bool fast = true;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != i) {
      fast = false;
      break;
    }
  }
  for (IntVar* card : cards) {
    CHECK_EQ(this, card->solver());
  }
  if (fast) {
    return RevAlloc(new FastDistribute(this, vars, cards));
  } else {
    return RevAlloc(new Distribute(this, vars, values, cards));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<IntVar*>& cards) {
  return MakeDistribute(vars, ToInt64Vector(values), cards);
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }
  for (IntVar* card : cards) {
    CHECK_EQ(this, card->solver());
  }
  return RevAlloc(new FastDistribute(this, vars, cards));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   int64_t card_min, int64_t card_max,
                                   int64_t card_size) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  for (IntVar* var : vars) {
    CHECK_EQ(this, var->solver());
  }
  if (card_min == 0 && card_max >= vsize) {
    return MakeTrueConstraint();
  } else if (card_min > vsize || card_max < 0 || card_max < card_min) {
    return MakeFalseConstraint();
  } else {
    std::vector<int64_t> mins(card_size, card_min);
    std::vector<int64_t> maxes(card_size, card_max);
    return RevAlloc(new BoundedFastDistribute(this, vars, mins, maxes));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& card_min,
                                   const std::vector<int64_t>& card_max) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  int64_t cmax = std::numeric_limits<int64_t>::max();
  int64_t cmin = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < card_max.size(); ++i) {
    cmax = std::min(cmax, card_max[i]);
    cmin = std::max(cmin, card_min[i]);
  }
  if (cmax < 0 || cmin > vsize) {
    return MakeFalseConstraint();
  } else if (cmax >= vsize && cmin == 0) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new BoundedFastDistribute(this, vars, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(card_min), ToInt64Vector(card_max));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& values,
                                   const std::vector<int64_t>& card_min,
                                   const std::vector<int64_t>& card_max) {
  CHECK_NE(vars.size(), 0);
  CHECK_EQ(card_min.size(), values.size());
  CHECK_EQ(card_min.size(), card_max.size());
  if (AreAllOnes(card_min) && AreAllOnes(card_max) &&
      values.size() == vars.size() && IsIncreasingContiguous(values) &&
      IsArrayInRange(vars, values.front(), values.back())) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(
        new BoundedDistribute(this, vars, values, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(values), ToInt64Vector(card_min),
                        ToInt64Vector(card_max));
}

Constraint* Solver::MakeDeviation(const std::vector<IntVar*>& vars,
                                  IntVar* deviation_var, int64_t total_sum) {
  return RevAlloc(new Deviation(this, vars, deviation_var, total_sum));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int64_t> x_size, absl::Span<const int64_t> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int> x_size, absl::Span<const int> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, true));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size) {
  return RevAlloc(new Diffn(this, x_vars, y_vars, x_size, y_size, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int64_t> x_size, absl::Span<const int64_t> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}

Constraint* Solver::MakeNonOverlappingNonStrictBoxesConstraint(
    const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
    absl::Span<const int> x_size, absl::Span<const int> y_size) {
  std::vector<IntVar*> dx(x_size.size());
  std::vector<IntVar*> dy(y_size.size());
  for (int i = 0; i < x_size.size(); ++i) {
    dx[i] = MakeIntConst(x_size[i]);
    dy[i] = MakeIntConst(y_size[i]);
  }
  return RevAlloc(new Diffn(this, x_vars, y_vars, dx, dy, false));
}

namespace {
std::string StringifyEvaluatorBare(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_start, int64_t range_end) {
  std::string out;
  for (int64_t i = range_start; i < range_end; ++i) {
    if (i != range_start) {
      out += ", ";
    }
    out += absl::StrFormat("%d -> %s", i, evaluator(i)->DebugString());
  }
  return out;
}

std::string StringifyInt64ToIntVar(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_begin, int64_t range_end) {
  std::string out;
  if (range_end - range_begin > 10) {
    out = absl::StrFormat(
        "IntToIntVar(%s, ...%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_begin + 5),
        StringifyEvaluatorBare(evaluator, range_end - 5, range_end));
  } else {
    out = absl::StrFormat(
        "IntToIntVar(%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_end));
  }
  return out;
}

// ----- Solver::MakeElement(int array, int var) -----
IntExpr* BuildElement(Solver* solver, const std::vector<int64_t>& values,
                      IntVar* index) {
  // Various checks.
  // Is array constant?
  if (IsArrayConstant(values, values[0])) {
    solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
    return solver->MakeIntConst(values[0]);
  }
  // Is array built with booleans only?
  // TODO(user): We could maintain the index of the first one.
  if (IsArrayBoolean(values)) {
    std::vector<int64_t> ones;
    int first_zero = -1;
    for (int i = 0; i < values.size(); ++i) {
      if (values[i] == 1) {
        ones.push_back(i);
      } else {
        first_zero = i;
      }
    }
    if (ones.size() == 1) {
      DCHECK_EQ(int64_t{1}, values[ones.back()]);
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsEqualCstVar(index, ones.back());
    } else if (ones.size() == values.size() - 1) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsDifferentCstVar(index, first_zero);
    } else if (ones.size() == ones.back() - ones.front() + 1) {  // contiguous.
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      IntVar* b = solver->MakeBoolVar("ContiguousBooleanElementVar");
      solver->AddConstraint(
          solver->MakeIsBetweenCt(index, ones.front(), ones.back(), b));
      return b;
    } else {
      IntVar* b = solver->MakeBoolVar("NonContiguousBooleanElementVar");
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      solver->AddConstraint(solver->MakeIsMemberCt(index, ones, b));
      return b;
    }
  }
  IntExpr* cache = nullptr;
  if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
    cache = solver->Cache()->FindVarConstantArrayExpression(
        index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    IntExpr* result = nullptr;
    if (values.size() >= 2 && index->Min() == 0 && index->Max() == 1) {
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (values.size() == 2 && index->Contains(0) && index->Contains(1)) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, 1));
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (IsIncreasingContiguous(values)) {
      result = solver->MakeSum(index, values[0]);
    } else if (IsIncreasing(values)) {
      result = RegisterIntExpr(solver->RevAlloc(
          new IncreasingIntExprElement(solver, values, index)));
    } else {
      if (solver->parameters().use_element_rmq()) {
        result = RegisterIntExpr(solver->RevAlloc(
            new RangeMinimumQueryExprElement(solver, values, index)));
      } else {
        result = RegisterIntExpr(
            solver->RevAlloc(new IntExprElement(solver, values, index)));
      }
    }
    if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
      solver->Cache()->InsertVarConstantArrayExpression(
          result, index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
    }
    return result;
  }
}

// Factory helper.

Constraint* MakeElementEqualityFunc(Solver* solver,
                                    const std::vector<int64_t>& vals,
                                    IntVar* index, IntVar* target) {
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vals.size()) {
      return solver->MakeFalseConstraint();
    } else {
      return solver->MakeEquality(target, vals[val]);
    }
  } else {
    if (IsIncreasingContiguous(vals)) {
      return solver->MakeEquality(target, solver->MakeSum(index, vals[0]));
    } else {
      return solver->RevAlloc(
          new IntElementConstraint(solver, vals, index, target));
    }
  }
}
}  // namespace

Constraint* Solver::MakeIfThenElseCt(IntVar* condition, IntExpr* then_expr,
                                     IntExpr* else_expr, IntVar* target_var) {
  return RevAlloc(
      new IfThenElseCt(this, condition, then_expr, else_expr, target_var));
}

IntExpr* Solver::MakeElement(const std::vector<IntVar*>& vars, IntVar* index) {
  if (index->Bound()) {
    return vars[index->Min()];
  }
  const int size = vars.size();
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(size);
    for (int i = 0; i < size; ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElement(values, index);
  }
  if (index->Size() == 2 && index->Min() + 1 == index->Max() &&
      index->Min() >= 0 && index->Max() < vars.size()) {
    // Let's get the index between 0 and 1.
    IntVar* scaled_index = MakeSum(index, -index->Min())->Var();
    IntVar* zero = vars[index->Min()];
    IntVar* one = vars[index->Max()];
    const std::string name = absl::StrFormat("ElementVar([%s], %s)",
                                             JoinNamePtr(vars), index->name());
    IntVar* target = MakeIntVar(std::min(zero->Min(), one->Min()),
                                std::max(zero->Max(), one->Max()), name);
    AddConstraint(
        RevAlloc(new IfThenElseCt(this, scaled_index, one, zero, target)));
    return target;
  }
  int64_t emin = std::numeric_limits<int64_t>::max();
  int64_t emax = std::numeric_limits<int64_t>::min();
  std::unique_ptr<IntVarIterator> iterator(index->MakeDomainIterator(false));
  for (const int64_t index_value : InitAndGetValues(iterator.get())) {
    if (index_value >= 0 && index_value < size) {
      emin = std::min(emin, vars[index_value]->Min());
      emax = std::max(emax, vars[index_value]->Max());
    }
  }
  const std::string vname =
      size > 10 ? absl::StrFormat("ElementVar(var array of size %d, %s)", size,
                                  index->DebugString())
                : absl::StrFormat("ElementVar([%s], %s)", JoinNamePtr(vars),
                                  index->name());
  IntVar* element_var = MakeIntVar(emin, emax, vname);
  AddConstraint(
      RevAlloc(new IntExprArrayElementCt(this, vars, index, element_var)));
  return element_var;
}

IntExpr* Solver::MakeElement(Int64ToIntVar vars, int64_t range_start,
                             int64_t range_end, IntVar* argument) {
  const std::string index_name =
      !argument->name().empty() ? argument->name() : argument->DebugString();
  const std::string vname = absl::StrFormat(
      "ElementVar(%s, %s)",
      StringifyInt64ToIntVar(vars, range_start, range_end), index_name);
  IntVar* element_var = MakeIntVar(std::numeric_limits<int64_t>::min(),
                                   std::numeric_limits<int64_t>::max(), vname);
  IntExprEvaluatorElementCt* evaluation_ct = new IntExprEvaluatorElementCt(
      this, std::move(vars), range_start, range_end, argument, element_var);
  AddConstraint(RevAlloc(evaluation_ct));
  evaluation_ct->Propagate();
  return element_var;
}

Constraint* Solver::MakeElementEquality(const std::vector<int64_t>& vals,
                                        IntVar* index, IntVar* target) {
  return MakeElementEqualityFunc(this, vals, index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<int>& vals,
                                        IntVar* index, IntVar* target) {
  return MakeElementEqualityFunc(this, ToInt64Vector(vals), index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* index, IntVar* target) {
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(vars.size());
    for (int i = 0; i < vars.size(); ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElementEquality(values, index, target);
  }
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vars.size()) {
      return MakeFalseConstraint();
    } else {
      return MakeEquality(target, vars[val]);
    }
  } else {
    if (target->Bound()) {
      return RevAlloc(
          new IntExprArrayElementCstCt(this, vars, index, target->Min()));
    } else {
      return RevAlloc(new IntExprArrayElementCt(this, vars, index, target));
    }
  }
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* index, int64_t target) {
  if (AreAllBound(vars)) {
    std::vector<int> valid_indices;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Value() == target) {
        valid_indices.push_back(i);
      }
    }
    return MakeMemberCt(index, valid_indices);
  }
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprArrayElementCstCt(this, vars, index, target));
  }
}

Constraint* Solver::MakeIndexOfConstraint(const std::vector<IntVar*>& vars,
                                          IntVar* index, int64_t target) {
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprIndexOfCt(this, vars, index, target));
  }
}

IntExpr* Solver::MakeIndexExpression(const std::vector<IntVar*>& vars,
                                     int64_t value) {
  IntExpr* cache = model_cache_->FindVarArrayConstantExpression(
      vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    const std::string name =
        absl::StrFormat("Index(%s, %d)", JoinNamePtr(vars), value);
    IntVar* index = MakeIntVar(0, vars.size() - 1, name);
    AddConstraint(MakeIndexOfConstraint(vars, index, value));
    model_cache_->InsertVarArrayConstantExpression(
        index, vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
    return index;
  }
}

IntExpr* Solver::MakeElement(const std::vector<int64_t>& values,
                             IntVar* index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, values, index);
}

IntExpr* Solver::MakeElement(const std::vector<int>& values, IntVar* index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, ToInt64Vector(values), index);
}

IntExpr* Solver::MakeElement(Solver::IndexEvaluator1 values, IntVar* index) {
  CHECK_EQ(this, index->solver());
  return RegisterIntExpr(
      RevAlloc(new IntExprFunctionElement(this, std::move(values), index)));
}

IntExpr* Solver::MakeMonotonicElement(Solver::IndexEvaluator1 values,
                                      bool increasing, IntVar* index) {
  CHECK_EQ(this, index->solver());
  if (increasing) {
    return RegisterIntExpr(RevAlloc(
        new IncreasingIntExprFunctionElement(this, std::move(values), index)));
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new IncreasingIntExprFunctionElement(
            this,
            [values = std::move(values)](int64_t i) { return -values(i); },
            index))));
  }
}

IntExpr* Solver::MakeElement(Solver::IndexEvaluator2 values, IntVar* index1,
                             IntVar* index2) {
  CHECK_EQ(this, index1->solver());
  CHECK_EQ(this, index2->solver());
  return RegisterIntExpr(RevAlloc(
      new IntIntExprFunctionElement(this, std::move(values), index1, index2)));
}

DecisionBuilder* Solver::MakeDefaultPhase(const std::vector<IntVar*>& vars) {
  DefaultPhaseParameters parameters;
  return MakeDefaultPhase(vars, parameters);
}

DecisionBuilder* Solver::MakeDefaultPhase(
    const std::vector<IntVar*>& vars,
    const DefaultPhaseParameters& parameters) {
  return RevAlloc(new DefaultIntegerSearch(this, vars, parameters));
}

Demon* Solver::RegisterDemon(Demon* demon) {
  CHECK(demon != nullptr);
  if (InstrumentsDemons()) {
    propagation_monitor_->RegisterDemon(demon);
  }
  return demon;
}

Constraint* Solver::MakeEquality(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

Constraint* Solver::MakeEquality(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreater(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

Constraint* Solver::MakeGreater(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLess(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

Constraint* Solver::MakeLess(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

Constraint* Solver::MakeNonEquality(IntExpr* e, int64_t v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}

Constraint* Solver::MakeNonEquality(IntExpr* e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}

IntVar* Solver::MakeIsEqualCstVar(IntExpr* var, int64_t value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualVar(left, MakeSum(right, value));
  }
  if (CapSub(var->Max(), var->Min()) == 1) {
    if (value == var->Min()) {
      return MakeDifference(value + 1, var)->Var();
    } else if (value == var->Max()) {
      return MakeSum(var, -value + 1)->Var();
    } else {
      return MakeIntConst(0);
    }
  }
  if (var->IsVar()) {
    return var->Var()->IsEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s == %d)", var->DebugString(), value));
    AddConstraint(MakeIsEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCstCt(IntExpr* var, int64_t value,
                                     IntVar* boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
      return MakeEquality(MakeDifference(value + 1, var), boolvar);
    }
    return MakeIsLessOrEqualCstCt(var, value, boolvar);
  }
  if (value == var->Max()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
      return MakeEquality(MakeSum(var, -value + 1), boolvar);
    }
    return MakeIsGreaterOrEqualCstCt(var, value, boolvar);
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeNonEquality(var, value);
    } else {
      return MakeEquality(var, value);
    }
  }
  // TODO(user) : what happens if the constraint is not posted?
  // The cache becomes tainted.
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsEqualCstCt(this, var->Var(), value, boolvar));
  }
}

IntVar* Solver::MakeIsDifferentCstVar(IntExpr* var, int64_t value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentVar(left, MakeSum(right, value));
  }
  return var->Var()->IsDifferent(value);
}

Constraint* Solver::MakeIsDifferentCstCt(IntExpr* var, int64_t value,
                                         IntVar* boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstCt(var, value + 1, boolvar);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstCt(var, value - 1, boolvar);
  }
  if (var->IsVar() && !var->Var()->Contains(value)) {
    return MakeEquality(boolvar, int64_t{1});
  }
  if (var->Bound() && var->Min() == value) {
    return MakeEquality(boolvar, Zero());
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeEquality(var, value);
    } else {
      return MakeNonEquality(var, value);
    }
  }
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_NOT_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsDiffCstCt(this, var->Var(), value, boolvar));
  }
}

IntVar* Solver::MakeIsGreaterOrEqualCstVar(IntExpr* var, int64_t value) {
  if (var->Min() >= value) {
    return MakeIntConst(int64_t{1});
  }
  if (var->Max() < value) {
    return MakeIntConst(int64_t{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsGreaterOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s >= %d)", var->DebugString(), value));
    AddConstraint(MakeIsGreaterOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsGreaterCstVar(IntExpr* var, int64_t value) {
  return MakeIsGreaterOrEqualCstVar(var, value + 1);
}

Constraint* Solver::MakeIsGreaterOrEqualCstCt(IntExpr* var, int64_t value,
                                              IntVar* boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeLess(var, value);
    } else {
      return MakeGreaterOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
  return RevAlloc(new IsGreaterEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsGreaterCstCt(IntExpr* v, int64_t c, IntVar* b) {
  return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
}

IntVar* Solver::MakeIsLessOrEqualCstVar(IntExpr* var, int64_t value) {
  if (var->Max() <= value) {
    return MakeIntConst(int64_t{1});
  }
  if (var->Min() > value) {
    return MakeIntConst(int64_t{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsLessOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s <= %d)", var->DebugString(), value));
    AddConstraint(MakeIsLessOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsLessCstVar(IntExpr* var, int64_t value) {
  return MakeIsLessOrEqualCstVar(var, value - 1);
}

Constraint* Solver::MakeIsLessOrEqualCstCt(IntExpr* var, int64_t value,
                                           IntVar* boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeGreater(var, value);
    } else {
      return MakeLessOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_LESS_OR_EQUAL);
  return RevAlloc(new IsLessEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsLessCstCt(IntExpr* v, int64_t c, IntVar* b) {
  return MakeIsLessOrEqualCstCt(v, c - 1, b);
}

Constraint* Solver::MakeBetweenCt(IntExpr* expr, int64_t l, int64_t u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeFalseConstraint();
    return MakeEquality(expr, l);
  }
  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeFalseConstraint();
  if (emin >= l && emax <= u) return MakeTrueConstraint();
  // Catch one-sided constraints.
  if (emax <= u) return MakeGreaterOrEqual(expr, l);
  if (emin >= l) return MakeLessOrEqual(expr, u);
  // Simplify the common factor, if any.
  int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff));
  } else {
    // No further reduction is possible.
    return RevAlloc(new BetweenCt(this, expr, l, u));
  }
}

Constraint* Solver::MakeNotBetweenCt(IntExpr* expr, int64_t l, int64_t u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty interval.
  if (l > u) {
    return MakeTrueConstraint();
  }

  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeTrueConstraint();
  if (emin >= l && emax <= u) return MakeFalseConstraint();
  // Catch one-sided constraints.
  if (emin >= l) return MakeGreater(expr, u);
  if (emax <= u) return MakeLess(expr, l);
  // TODO(user): Add back simplification code if expr is constant *
  // other_expr.
  return RevAlloc(new NotBetweenCt(this, expr, l, u));
}

Constraint* Solver::MakeIsBetweenCt(IntExpr* expr, int64_t l, int64_t u,
                                    IntVar* b) {
  CHECK_EQ(this, expr->solver());
  CHECK_EQ(this, b->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeEquality(b, Zero());
    return MakeIsEqualCstCt(expr, l, b);
  }
  int64_t emin = 0;
  int64_t emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeEquality(b, Zero());
  if (emin >= l && emax <= u) return MakeEquality(b, 1);
  // Catch one-sided constraints.
  if (emax <= u) return MakeIsGreaterOrEqualCstCt(expr, l, b);
  if (emin >= l) return MakeIsLessOrEqualCstCt(expr, u, b);
  // Simplify the common factor, if any.
  int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeIsBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff),
                           b);
  } else {
    // No further reduction is possible.
    return RevAlloc(new IsBetweenCt(this, expr, l, u, b));
  }
}

IntVar* Solver::MakeIsBetweenVar(IntExpr* v, int64_t l, int64_t u) {
  CHECK_EQ(this, v->solver());
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsBetweenCt(v, l, u, b));
  return b;
}

Constraint* Solver::MakeMemberCt(IntExpr* expr,
                                 const std::vector<int64_t>& values) {
  const int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return absl::c_find(values, 0) == values.end() ? MakeFalseConstraint()
                                                   : MakeTrueConstraint();
  }
  std::vector<int64_t> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64_t v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64_t emin;
  int64_t emax;
  expr->Range(&emin, &emax);
  for (const int64_t v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeFalseConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    // Note: MakeBetweenCt() has a fast-track for trivially true constraints.
    return MakeBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // NotMemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to list the values *not* allowed.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64_t v : copied_values)
      is_among_input_values[v - emin] = true;
    // We use the zero valued indices of is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64_t v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeNonEquality(expr, copied_values[0]);
    }
    return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use MemberCt. No further reduction is possible.
  return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeMemberCt(IntExpr* expr,
                                 const std::vector<int>& values) {
  return MakeMemberCt(expr, ToInt64Vector(values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    const std::vector<int64_t>& values) {
  const int64_t coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return absl::c_find(values, 0) == values.end() ? MakeTrueConstraint()
                                                   : MakeFalseConstraint();
  }
  std::vector<int64_t> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64_t v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64_t emin;
  int64_t emax;
  expr->Range(&emin, &emax);
  for (const int64_t v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeTrueConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeNonEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    return MakeNotBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // MemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to a dense boolean vector.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64_t v : copied_values)
      is_among_input_values[v - emin] = true;
    // Use zero valued indices for is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64_t v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeEquality(expr, copied_values[0]);
    }
    return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use NotMemberCt. No further reduction is possible.
  return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    const std::vector<int>& values) {
  return MakeNotMemberCt(expr, ToInt64Vector(values));
}

Constraint* Solver::MakeIsMemberCt(IntExpr* expr,
                                   const std::vector<int64_t>& values,
                                   IntVar* boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

Constraint* Solver::MakeIsMemberCt(IntExpr* expr,
                                   const std::vector<int>& values,
                                   IntVar* boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

IntVar* Solver::MakeIsMemberVar(IntExpr* expr,
                                const std::vector<int64_t>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntExpr* expr, const std::vector<int>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr, std::vector<int64_t> starts,
                                    std::vector<int64_t> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr, std::vector<int> starts,
                                    std::vector<int> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    SortedDisjointIntervalList intervals) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), std::move(intervals)));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler,
                                bool assume_paths) {
  CHECK_EQ(nexts.size(), active.size());
  if (sink_handler == nullptr) {
    const int64_t size = nexts.size();
    sink_handler = [size](int64_t index) { return index >= size; };
  }
  return RevAlloc(
      new NoCycle(this, nexts, active, std::move(sink_handler), assume_paths));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler) {
  return MakeNoCycle(nexts, active, std::move(sink_handler), true);
}

// TODO(user): Merge NoCycle and Circuit.
Constraint* Solver::MakeCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, false));
}

Constraint* Solver::MakeSubCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, true));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new PathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2PathCumul(this, nexts, active, cumuls,
                                               std::move(transit_evaluator)));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& slacks,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2SlackPathCumul(
      this, nexts, active, cumuls, slacks, std::move(transit_evaluator)));
}

Constraint* Solver::MakeDelayedPathCumul(const std::vector<IntVar*>& nexts,
                                         const std::vector<IntVar*>& active,
                                         const std::vector<IntVar*>& cumuls,
                                         const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new DelayedPathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathConnected(std::vector<IntVar*> nexts,
                                      std::vector<int64_t> sources,
                                      std::vector<int64_t> sinks,
                                      std::vector<IntVar*> status) {
  return RevAlloc(new PathConnectedConstraint(
      this, std::move(nexts), sources, std::move(sinks), std::move(status)));
}

Constraint* Solver::MakePathPrecedenceConstraint(
    std::vector<IntVar*> nexts,
    const std::vector<std::pair<int, int>>& precedences) {
  return MakePathTransitPrecedenceConstraint(std::move(nexts), {}, precedences);
}

Constraint* Solver::MakePathPrecedenceConstraint(
    std::vector<IntVar*> nexts,
    const std::vector<std::pair<int, int>>& precedences,
    absl::Span<const int> lifo_path_starts,
    absl::Span<const int> fifo_path_starts) {
  absl::flat_hash_map<int, PathTransitPrecedenceConstraint::PrecedenceType>
      precedence_types;
  for (int start : lifo_path_starts) {
    precedence_types[start] = PathTransitPrecedenceConstraint::LIFO;
  }
  for (int start : fifo_path_starts) {
    precedence_types[start] = PathTransitPrecedenceConstraint::FIFO;
  }
  return MakePathTransitTypedPrecedenceConstraint(
      this, std::move(nexts), {}, precedences, std::move(precedence_types));
}

Constraint* Solver::MakePathTransitPrecedenceConstraint(
    std::vector<IntVar*> nexts, std::vector<IntVar*> transits,
    const std::vector<std::pair<int, int>>& precedences) {
  return MakePathTransitTypedPrecedenceConstraint(
      this, std::move(nexts), std::move(transits), precedences, {{}});
}

Constraint* Solver::MakePathEnergyCostConstraint(
    PathEnergyCostConstraintSpecification specification) {
  return RevAlloc(new PathEnergyCostConstraint(this, std::move(specification)));
}

namespace {
void DeepLinearize(Solver* solver, const std::vector<IntVar*>& pre_vars,
                   absl::Span<const int64_t> pre_coefs,
                   std::vector<IntVar*>* vars, std::vector<int64_t>* coefs,
                   int64_t* constant) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);
  CHECK(coefs != nullptr);
  CHECK(constant != nullptr);
  *constant = 0;
  vars->reserve(pre_vars.size());
  coefs->reserve(pre_coefs.size());
  // Try linear scan of the variables to check if there is nothing to do.
  bool need_linearization = false;
  for (int i = 0; i < pre_vars.size(); ++i) {
    IntVar* const variable = pre_vars[i];
    const int64_t coefficient = pre_coefs[i];
    if (variable->Bound()) {
      *constant = CapAdd(*constant, CapProd(coefficient, variable->Min()));
    } else if (solver->CastExpression(variable) == nullptr) {
      vars->push_back(variable);
      coefs->push_back(coefficient);
    } else {
      need_linearization = true;
      vars->clear();
      coefs->clear();
      break;
    }
  }
  if (need_linearization) {
    // Introspect the variables to simplify the sum.
    absl::flat_hash_map<IntVar*, int64_t> variables_to_coefficients;
    ExprLinearizer linearizer(&variables_to_coefficients);
    for (int i = 0; i < pre_vars.size(); ++i) {
      linearizer.Visit(pre_vars[i], pre_coefs[i]);
    }
    *constant = linearizer.Constant();
    for (const auto& variable_to_coefficient : variables_to_coefficients) {
      if (variable_to_coefficient.second != 0) {
        vars->push_back(variable_to_coefficient.first);
        coefs->push_back(variable_to_coefficient.second);
      }
    }
  }
}

Constraint* MakeScalProdEqualityFct(Solver* solver,
                                    const std::vector<IntVar*>& pre_vars,
                                    absl::Span<const int64_t> pre_coefs,
                                    int64_t cst) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  cst = CapSub(cst, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull(coefs)) {
    return cst == 0 ? solver->MakeTrueConstraint()
                    : solver->MakeFalseConstraint();
  }
  if (AreAllBoundOrNull(vars, coefs)) {
    int64_t sum = 0;
    for (int i = 0; i < size; ++i) {
      sum = CapAdd(sum, CapProd(coefs[i], vars[i]->Min()));
    }
    return sum == cst ? solver->MakeTrueConstraint()
                      : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumEquality(vars, cst);
  }
  if (AreAllBooleans(vars) && size > 2) {
    if (AreAllPositive(coefs)) {
      return solver->RevAlloc(
          new PositiveBooleanScalProdEqCst(solver, vars, coefs, cst));
    }
    if (AreAllNegative(coefs)) {
      std::vector<int64_t> opp_coefs(coefs.size());
      for (int i = 0; i < coefs.size(); ++i) {
        opp_coefs[i] = -coefs[i];
      }
      return solver->RevAlloc(
          new PositiveBooleanScalProdEqCst(solver, vars, opp_coefs, -cst));
    }
  }

  // Simplications.
  int constants = 0;
  int positives = 0;
  int negatives = 0;
  for (int i = 0; i < size; ++i) {
    if (coefs[i] == 0 || vars[i]->Bound()) {
      constants++;
    } else if (coefs[i] > 0) {
      positives++;
    } else {
      negatives++;
    }
  }
  if (positives > 0 && negatives > 0) {
    std::vector<IntVar*> pos_terms;
    std::vector<IntVar*> neg_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    if (negatives == 1) {
      if (rhs != 0) {
        pos_terms.push_back(solver->MakeIntConst(-rhs));
      }
      return solver->MakeSumEquality(pos_terms, neg_terms[0]);
    } else if (positives == 1) {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeSumEquality(neg_terms, pos_terms[0]);
    } else {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeEquality(solver->MakeSum(pos_terms),
                                  solver->MakeSum(neg_terms));
    }
  } else if (positives == 1) {
    IntExpr* pos_term = nullptr;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_term = solver->MakeProd(vars[i], coefs[i]);
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeEquality(pos_term, rhs);
  } else if (negatives == 1) {
    IntExpr* neg_term = nullptr;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_term = solver->MakeProd(vars[i], -coefs[i]);
      }
    }
    return solver->MakeEquality(neg_term, -rhs);
  } else if (positives > 1) {
    std::vector<IntVar*> pos_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeSumEquality(pos_terms, rhs);
  } else if (negatives > 1) {
    std::vector<IntVar*> neg_terms;
    int64_t rhs = cst;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    return solver->MakeSumEquality(neg_terms, -rhs);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumEquality(terms, solver->MakeIntConst(cst));
}

Constraint* MakeScalProdEqualityVarFct(Solver* solver,
                                       const std::vector<IntVar*>& pre_vars,
                                       absl::Span<const int64_t> pre_coefs,
                                       IntVar* target) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return solver->MakeEquality(target, constant);
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumEquality(vars,
                                   solver->MakeSum(target, -constant)->Var());
  }
  if (AreAllBooleans(vars) && AreAllPositive<int64_t>(coefs)) {
    // TODO(user) : bench BooleanScalProdEqVar with IntConst.
    return solver->RevAlloc(new PositiveBooleanScalProdEqVar(
        solver, vars, coefs, solver->MakeSum(target, -constant)->Var()));
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumEquality(terms,
                                 solver->MakeSum(target, -constant)->Var());
}

Constraint* MakeScalProdGreaterOrEqualFct(Solver* solver,
                                          const std::vector<IntVar*>& pre_vars,
                                          absl::Span<const int64_t> pre_coefs,
                                          int64_t cst) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  cst = CapSub(cst, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return cst <= 0 ? solver->MakeTrueConstraint()
                    : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumGreaterOrEqual(vars, cst);
  }
  if (cst == 1 && AreAllBooleans(vars) && AreAllPositive(coefs)) {
    // can move all coefs to 1.
    std::vector<IntVar*> terms;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] > 0) {
        terms.push_back(vars[i]);
      }
    }
    return solver->MakeSumGreaterOrEqual(terms, 1);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeSumGreaterOrEqual(terms, cst);
}

Constraint* MakeScalProdLessOrEqualFct(Solver* solver,
                                       const std::vector<IntVar*>& pre_vars,
                                       absl::Span<const int64_t> pre_coefs,
                                       int64_t upper_bound) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);
  upper_bound = CapSub(upper_bound, constant);

  const int size = vars.size();
  if (size == 0 || AreAllNull<int64_t>(coefs)) {
    return upper_bound >= 0 ? solver->MakeTrueConstraint()
                            : solver->MakeFalseConstraint();
  }
  // TODO(user) : compute constant on the fly.
  if (AreAllBoundOrNull(vars, coefs)) {
    int64_t cst = 0;
    for (int i = 0; i < size; ++i) {
      cst = CapAdd(cst, CapProd(vars[i]->Min(), coefs[i]));
    }
    return cst <= upper_bound ? solver->MakeTrueConstraint()
                              : solver->MakeFalseConstraint();
  }
  if (AreAllOnes(coefs)) {
    return solver->MakeSumLessOrEqual(vars, upper_bound);
  }
  if (AreAllBooleans(vars) && AreAllPositive<int64_t>(coefs)) {
    return solver->RevAlloc(
        new BooleanScalProdLessConstant(solver, vars, coefs, upper_bound));
  }
  // Some simplifications
  int constants = 0;
  int positives = 0;
  int negatives = 0;
  for (int i = 0; i < size; ++i) {
    if (coefs[i] == 0 || vars[i]->Bound()) {
      constants++;
    } else if (coefs[i] > 0) {
      positives++;
    } else {
      negatives++;
    }
  }
  if (positives > 0 && negatives > 0) {
    std::vector<IntVar*> pos_terms;
    std::vector<IntVar*> neg_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    if (negatives == 1) {
      IntExpr* const neg_term = solver->MakeSum(neg_terms[0], rhs);
      return solver->MakeLessOrEqual(solver->MakeSum(pos_terms), neg_term);
    } else if (positives == 1) {
      IntExpr* const pos_term = solver->MakeSum(pos_terms[0], -rhs);
      return solver->MakeGreaterOrEqual(solver->MakeSum(neg_terms), pos_term);
    } else {
      if (rhs != 0) {
        neg_terms.push_back(solver->MakeIntConst(rhs));
      }
      return solver->MakeLessOrEqual(solver->MakeSum(pos_terms),
                                     solver->MakeSum(neg_terms));
    }
  } else if (positives == 1) {
    IntExpr* pos_term = nullptr;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_term = solver->MakeProd(vars[i], coefs[i]);
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeLessOrEqual(pos_term, rhs);
  } else if (negatives == 1) {
    IntExpr* neg_term = nullptr;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_term = solver->MakeProd(vars[i], -coefs[i]);
      }
    }
    return solver->MakeGreaterOrEqual(neg_term, -rhs);
  } else if (positives > 1) {
    std::vector<IntVar*> pos_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        pos_terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
      } else {
        LOG(FATAL) << "Should not be here";
      }
    }
    return solver->MakeSumLessOrEqual(pos_terms, rhs);
  } else if (negatives > 1) {
    std::vector<IntVar*> neg_terms;
    int64_t rhs = upper_bound;
    for (int i = 0; i < size; ++i) {
      if (coefs[i] == 0 || vars[i]->Bound()) {
        rhs = CapSub(rhs, CapProd(coefs[i], vars[i]->Min()));
      } else if (coefs[i] > 0) {
        LOG(FATAL) << "Should not be here";
      } else {
        neg_terms.push_back(solver->MakeProd(vars[i], -coefs[i])->Var());
      }
    }
    return solver->MakeSumGreaterOrEqual(neg_terms, -rhs);
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return solver->MakeLessOrEqual(solver->MakeSum(terms), upper_bound);
}

IntExpr* MakeSumArrayAux(Solver* solver, const std::vector<IntVar*>& vars,
                         int64_t constant) {
  const int size = vars.size();
  DCHECK_GT(size, 2);
  int64_t new_min = 0;
  int64_t new_max = 0;
  for (int i = 0; i < size; ++i) {
    if (new_min != std::numeric_limits<int64_t>::min()) {
      new_min = CapAdd(vars[i]->Min(), new_min);
    }
    if (new_max != std::numeric_limits<int64_t>::max()) {
      new_max = CapAdd(vars[i]->Max(), new_max);
    }
  }
  IntExpr* const cache =
      solver->Cache()->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM);
  if (cache != nullptr) {
    return solver->MakeSum(cache, constant);
  } else {
    const std::string name = absl::StrFormat("Sum([%s])", JoinNamePtr(vars));
    IntVar* const sum_var = solver->MakeIntVar(new_min, new_max, name);
    if (AreAllBooleans(vars)) {
      solver->AddConstraint(
          solver->RevAlloc(new SumBooleanEqualToVar(solver, vars, sum_var)));
    } else if (size <= solver->parameters().array_split_size()) {
      solver->AddConstraint(
          solver->RevAlloc(new SmallSumConstraint(solver, vars, sum_var)));
    } else {
      solver->AddConstraint(
          solver->RevAlloc(new SumConstraint(solver, vars, sum_var)));
    }
    solver->Cache()->InsertVarArrayExpression(sum_var, vars,
                                              ModelCache::VAR_ARRAY_SUM);
    return solver->MakeSum(sum_var, constant);
  }
}

IntExpr* MakeSumAux(Solver* solver, const std::vector<IntVar*>& vars,
                    int64_t constant) {
  const int size = vars.size();
  if (size == 0) {
    return solver->MakeIntConst(constant);
  } else if (size == 1) {
    return solver->MakeSum(vars[0], constant);
  } else if (size == 2) {
    return solver->MakeSum(solver->MakeSum(vars[0], vars[1]), constant);
  } else {
    return MakeSumArrayAux(solver, vars, constant);
  }
}

IntExpr* MakeScalProdAux(Solver* solver, const std::vector<IntVar*>& vars,
                         const std::vector<int64_t>& coefs, int64_t constant) {
  if (AreAllOnes(coefs)) {
    return MakeSumAux(solver, vars, constant);
  }

  const int size = vars.size();
  if (size == 0) {
    return solver->MakeIntConst(constant);
  } else if (size == 1) {
    return solver->MakeSum(solver->MakeProd(vars[0], coefs[0]), constant);
  } else if (size == 2) {
    if (coefs[0] > 0 && coefs[1] < 0) {
      return solver->MakeSum(
          solver->MakeDifference(solver->MakeProd(vars[0], coefs[0]),
                                 solver->MakeProd(vars[1], -coefs[1])),
          constant);
    } else if (coefs[0] < 0 && coefs[1] > 0) {
      return solver->MakeSum(
          solver->MakeDifference(solver->MakeProd(vars[1], coefs[1]),
                                 solver->MakeProd(vars[0], -coefs[0])),
          constant);
    } else {
      return solver->MakeSum(
          solver->MakeSum(solver->MakeProd(vars[0], coefs[0]),
                          solver->MakeProd(vars[1], coefs[1])),
          constant);
    }
  } else {
    if (AreAllBooleans(vars)) {
      if (AreAllPositive(coefs)) {
        if (vars.size() > 8) {
          return solver->MakeSum(
              RegisterIntExpr(solver->RevAlloc(new PositiveBooleanScalProd(
                                  solver, vars, coefs)))
                  ->Var(),
              constant);
        } else {
          return solver->MakeSum(
              RegisterIntExpr(solver->RevAlloc(
                  new PositiveBooleanScalProd(solver, vars, coefs))),
              constant);
        }
      } else {
        // If some coefficients are non-positive, partition coefficients in two
        // sets, one for the positive coefficients P and one for the negative
        // ones N.
        // Create two PositiveBooleanScalProd expressions, one on P (s1), the
        // other on Opposite(N) (s2).
        // The final expression is then s1 - s2.
        // If P is empty, the expression is Opposite(s2).
        std::vector<int64_t> positive_coefs;
        std::vector<int64_t> negative_coefs;
        std::vector<IntVar*> positive_coef_vars;
        std::vector<IntVar*> negative_coef_vars;
        for (int i = 0; i < size; ++i) {
          const int coef = coefs[i];
          if (coef > 0) {
            positive_coefs.push_back(coef);
            positive_coef_vars.push_back(vars[i]);
          } else if (coef < 0) {
            negative_coefs.push_back(-coef);
            negative_coef_vars.push_back(vars[i]);
          }
        }
        CHECK_GT(negative_coef_vars.size(), 0);
        IntExpr* const negatives =
            MakeScalProdAux(solver, negative_coef_vars, negative_coefs, 0);
        if (!positive_coef_vars.empty()) {
          IntExpr* const positives = MakeScalProdAux(solver, positive_coef_vars,
                                                     positive_coefs, constant);
          return solver->MakeDifference(positives, negatives);
        } else {
          return solver->MakeDifference(constant, negatives);
        }
      }
    }
  }
  std::vector<IntVar*> terms;
  for (int i = 0; i < size; ++i) {
    terms.push_back(solver->MakeProd(vars[i], coefs[i])->Var());
  }
  return MakeSumArrayAux(solver, terms, constant);
}

IntExpr* MakeScalProdFct(Solver* solver, const std::vector<IntVar*>& pre_vars,
                         absl::Span<const int64_t> pre_coefs) {
  int64_t constant = 0;
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  DeepLinearize(solver, pre_vars, pre_coefs, &vars, &coefs, &constant);

  if (vars.empty()) {
    return solver->MakeIntConst(constant);
  }
  // Can we simplify using some gcd computation.
  int64_t gcd = std::abs(coefs[0]);
  for (int i = 1; i < coefs.size(); ++i) {
    gcd = std::gcd(gcd, std::abs(coefs[i]));
    if (gcd == 1) {
      break;
    }
  }
  if (constant != 0 && gcd != 1) {
    gcd = std::gcd(gcd, std::abs(constant));
  }
  if (gcd > 1) {
    for (int i = 0; i < coefs.size(); ++i) {
      coefs[i] /= gcd;
    }
    return solver->MakeProd(
        MakeScalProdAux(solver, vars, coefs, constant / gcd), gcd);
  }
  return MakeScalProdAux(solver, vars, coefs, constant);
}

IntExpr* MakeSumFct(Solver* solver, const std::vector<IntVar*>& pre_vars) {
  absl::flat_hash_map<IntVar*, int64_t> variables_to_coefficients;
  ExprLinearizer linearizer(&variables_to_coefficients);
  for (int i = 0; i < pre_vars.size(); ++i) {
    linearizer.Visit(pre_vars[i], 1);
  }
  const int64_t constant = linearizer.Constant();
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs;
  for (const auto& variable_to_coefficient : variables_to_coefficients) {
    if (variable_to_coefficient.second != 0) {
      vars.push_back(variable_to_coefficient.first);
      coefs.push_back(variable_to_coefficient.second);
    }
  }
  return MakeScalProdAux(solver, vars, coefs, constant);
}
}  // namespace

// ----- API -----

IntExpr* Solver::MakeSum(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    return MakeIntConst(int64_t{0});
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeSum(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM);
    if (cache != nullptr) {
      return cache;
    } else {
      int64_t new_min = 0;
      int64_t new_max = 0;
      for (int i = 0; i < size; ++i) {
        if (new_min != std::numeric_limits<int64_t>::min()) {
          new_min = CapAdd(vars[i]->Min(), new_min);
        }
        if (new_max != std::numeric_limits<int64_t>::max()) {
          new_max = CapAdd(vars[i]->Max(), new_max);
        }
      }
      IntExpr* sum_expr = nullptr;
      const bool all_booleans = AreAllBooleans(vars);
      if (all_booleans) {
        const std::string name =
            absl::StrFormat("BooleanSum([%s])", JoinNamePtr(vars));
        sum_expr = MakeIntVar(new_min, new_max, name);
        AddConstraint(
            RevAlloc(new SumBooleanEqualToVar(this, vars, sum_expr->Var())));
      } else if (new_min != std::numeric_limits<int64_t>::min() &&
                 new_max != std::numeric_limits<int64_t>::max()) {
        sum_expr = MakeSumFct(this, vars);
      } else {
        const std::string name =
            absl::StrFormat("Sum([%s])", JoinNamePtr(vars));
        sum_expr = MakeIntVar(new_min, new_max, name);
        AddConstraint(
            RevAlloc(new SafeSumConstraint(this, vars, sum_expr->Var())));
      }
      model_cache_->InsertVarArrayExpression(sum_expr, vars,
                                             ModelCache::VAR_ARRAY_SUM);
      return sum_expr;
    }
  }
}

IntExpr* Solver::MakeMin(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    LOG(WARNING) << "operations_research::Solver::MakeMin() was called with an "
                    "empty list of variables. Was this intentional?";
    return MakeIntConst(std::numeric_limits<int64_t>::max());
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMin(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_MIN);
    if (cache != nullptr) {
      return cache;
    } else {
      if (AreAllBooleans(vars)) {
        IntVar* const new_var = MakeBoolVar();
        AddConstraint(RevAlloc(new ArrayBoolAndEq(this, vars, new_var)));
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      } else {
        int64_t new_min = std::numeric_limits<int64_t>::max();
        int64_t new_max = std::numeric_limits<int64_t>::max();
        for (int i = 0; i < size; ++i) {
          new_min = std::min(new_min, vars[i]->Min());
          new_max = std::min(new_max, vars[i]->Max());
        }
        IntVar* const new_var = MakeIntVar(new_min, new_max);
        if (size <= parameters_.array_split_size()) {
          AddConstraint(RevAlloc(new SmallMinConstraint(this, vars, new_var)));
        } else {
          AddConstraint(RevAlloc(new MinConstraint(this, vars, new_var)));
        }
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      }
    }
  }
}

IntExpr* Solver::MakeMax(const std::vector<IntVar*>& vars) {
  const int size = vars.size();
  if (size == 0) {
    LOG(WARNING) << "operations_research::Solver::MakeMax() was called with an "
                    "empty list of variables. Was this intentional?";
    return MakeIntConst(std::numeric_limits<int64_t>::min());
  } else if (size == 1) {
    return vars[0];
  } else if (size == 2) {
    return MakeMax(vars[0], vars[1]);
  } else {
    IntExpr* const cache =
        model_cache_->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_MAX);
    if (cache != nullptr) {
      return cache;
    } else {
      if (AreAllBooleans(vars)) {
        IntVar* const new_var = MakeBoolVar();
        AddConstraint(RevAlloc(new ArrayBoolOrEq(this, vars, new_var)));
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MIN);
        return new_var;
      } else {
        int64_t new_min = std::numeric_limits<int64_t>::min();
        int64_t new_max = std::numeric_limits<int64_t>::min();
        for (int i = 0; i < size; ++i) {
          new_min = std::max(new_min, vars[i]->Min());
          new_max = std::max(new_max, vars[i]->Max());
        }
        IntVar* const new_var = MakeIntVar(new_min, new_max);
        if (size <= parameters_.array_split_size()) {
          AddConstraint(RevAlloc(new SmallMaxConstraint(this, vars, new_var)));
        } else {
          AddConstraint(RevAlloc(new MaxConstraint(this, vars, new_var)));
        }
        model_cache_->InsertVarArrayExpression(new_var, vars,
                                               ModelCache::VAR_ARRAY_MAX);
        return new_var;
      }
    }
  }
}

Constraint* Solver::MakeMinEquality(const std::vector<IntVar*>& vars,
                                    IntVar* min_var) {
  const int size = vars.size();
  if (size > 2) {
    if (AreAllBooleans(vars)) {
      return RevAlloc(new ArrayBoolAndEq(this, vars, min_var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallMinConstraint(this, vars, min_var));
    } else {
      return RevAlloc(new MinConstraint(this, vars, min_var));
    }
  } else if (size == 2) {
    return MakeEquality(MakeMin(vars[0], vars[1]), min_var);
  } else if (size == 1) {
    return MakeEquality(vars[0], min_var);
  } else {
    LOG(WARNING) << "operations_research::Solver::MakeMinEquality() was called "
                    "with an empty list of variables. Was this intentional?";
    return MakeEquality(min_var, std::numeric_limits<int64_t>::max());
  }
}

Constraint* Solver::MakeMaxEquality(const std::vector<IntVar*>& vars,
                                    IntVar* max_var) {
  const int size = vars.size();
  if (size > 2) {
    if (AreAllBooleans(vars)) {
      return RevAlloc(new ArrayBoolOrEq(this, vars, max_var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallMaxConstraint(this, vars, max_var));
    } else {
      return RevAlloc(new MaxConstraint(this, vars, max_var));
    }
  } else if (size == 2) {
    return MakeEquality(MakeMax(vars[0], vars[1]), max_var);
  } else if (size == 1) {
    return MakeEquality(vars[0], max_var);
  } else {
    LOG(WARNING) << "operations_research::Solver::MakeMaxEquality() was called "
                    "with an empty list of variables. Was this intentional?";
    return MakeEquality(max_var, std::numeric_limits<int64_t>::min());
  }
}

Constraint* Solver::MakeSumLessOrEqual(const std::vector<IntVar*>& vars,
                                       int64_t cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanLessOrEqualToOne(this, vars));
  } else {
    return MakeLessOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumGreaterOrEqual(const std::vector<IntVar*>& vars,
                                          int64_t cst) {
  const int size = vars.size();
  if (cst == 1LL && AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanGreaterOrEqualToOne(this, vars));
  } else {
    return MakeGreaterOrEqual(MakeSum(vars), cst);
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars,
                                    int64_t cst) {
  const int size = vars.size();
  if (size == 0) {
    return cst == 0 ? MakeTrueConstraint() : MakeFalseConstraint();
  }
  if (AreAllBooleans(vars) && size > 2) {
    if (cst == 1) {
      return RevAlloc(new SumBooleanEqualToOne(this, vars));
    } else if (cst < 0 || cst > size) {
      return MakeFalseConstraint();
    } else {
      return RevAlloc(new SumBooleanEqualToVar(this, vars, MakeIntConst(cst)));
    }
  } else {
    if (vars.size() == 1) {
      return MakeEquality(vars[0], cst);
    } else if (vars.size() == 2) {
      return MakeEquality(vars[0], MakeDifference(cst, vars[1]));
    }
    if (DetectSumOverflow(vars)) {
      return RevAlloc(new SafeSumConstraint(this, vars, MakeIntConst(cst)));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallSumConstraint(this, vars, MakeIntConst(cst)));
    } else {
      return RevAlloc(new SumConstraint(this, vars, MakeIntConst(cst)));
    }
  }
}

Constraint* Solver::MakeSumEquality(const std::vector<IntVar*>& vars,
                                    IntVar* var) {
  const int size = vars.size();
  if (size == 0) {
    return MakeEquality(var, Zero());
  }
  if (AreAllBooleans(vars) && size > 2) {
    return RevAlloc(new SumBooleanEqualToVar(this, vars, var));
  } else if (size == 0) {
    return MakeEquality(var, Zero());
  } else if (size == 1) {
    return MakeEquality(vars[0], var);
  } else if (size == 2) {
    return MakeEquality(MakeSum(vars[0], vars[1]), var);
  } else {
    if (DetectSumOverflow(vars)) {
      return RevAlloc(new SafeSumConstraint(this, vars, var));
    } else if (size <= parameters_.array_split_size()) {
      return RevAlloc(new SmallSumConstraint(this, vars, var));
    } else {
      return RevAlloc(new SumConstraint(this, vars, var));
    }
  }
}

Constraint* Solver::MakeScalProdEquality(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct(this, vars, coefficients, cst);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityFct(this, vars, ToInt64Vector(coefficients), cst);
}

Constraint* Solver::MakeScalProdEquality(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    IntVar* target) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityVarFct(this, vars, coefficients, target);
}

Constraint* Solver::MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coefficients,
                                         IntVar* target) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdEqualityVarFct(this, vars, ToInt64Vector(coefficients),
                                    target);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coeffs,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct(this, vars, coeffs, cst);
}

Constraint* Solver::MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                               const std::vector<int>& coeffs,
                                               int64_t cst) {
  DCHECK_EQ(vars.size(), coeffs.size());
  return MakeScalProdGreaterOrEqualFct(this, vars, ToInt64Vector(coeffs), cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct(this, vars, coefficients, cst);
}

Constraint* Solver::MakeScalProdLessOrEqual(
    const std::vector<IntVar*>& vars, const std::vector<int>& coefficients,
    int64_t cst) {
  DCHECK_EQ(vars.size(), coefficients.size());
  return MakeScalProdLessOrEqualFct(this, vars, ToInt64Vector(coefficients),
                                    cst);
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int64_t>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct(this, vars, coefs);
}

IntExpr* Solver::MakeScalProd(const std::vector<IntVar*>& vars,
                              const std::vector<int>& coefs) {
  DCHECK_EQ(vars.size(), coefs.size());
  return MakeScalProdFct(this, vars, ToInt64Vector(coefs));
}

IntExpr* Solver::MakeSum(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (right->Bound()) {
    return MakeSum(left, right->Min());
  }
  if (left->Bound()) {
    return MakeSum(right, left->Min());
  }
  if (left == right) {
    return MakeProd(left, 2);
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_SUM);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(right, left,
                                                 ModelCache::EXPR_EXPR_SUM);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    bool may_overflow = false;
    may_overflow |= AddOverflows(left->Max(), right->Max());
    may_overflow |= AddOverflows(left->Min(), right->Min());
    if (!may_overflow) {
      // The result itself would not overflow, but intermediate computations
      // could, needing a safe implementation.
      // For example: l in [kint64min, 0], r in [0, 5].
      // sum->SetMax(4) implies r->SetMax(4 - left->Min()), which overflows.
      const int64_t min_sum = left->Min() + right->Min();
      const int64_t max_sum = left->Max() + right->Max();
      may_overflow |= SubOverflows(max_sum, left->Min());
      may_overflow |= SubOverflows(max_sum, right->Min());
      may_overflow |= SubOverflows(min_sum, left->Max());
      may_overflow |= SubOverflows(min_sum, right->Max());
    }
    IntExpr* const result =
        may_overflow
            ? RegisterIntExpr(RevAlloc(new SafePlusIntExpr(this, left, right)))
            : RegisterIntExpr(RevAlloc(new PlusIntExpr(this, left, right)));
    model_cache_->InsertExprExprExpression(result, left, right,
                                           ModelCache::EXPR_EXPR_SUM);
    return result;
  }
}

IntExpr* Solver::MakeSum(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(CapAdd(expr->Min(), value));
  }
  if (value == 0) {
    return expr;
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_SUM);
  if (result == nullptr) {
    if (expr->IsVar() && !AddOverflows(value, expr->Max()) &&
        !AddOverflows(value, expr->Min())) {
      result = NewVarPlusInt(this, expr->Var(), value);
    } else {
      result = RegisterIntExpr(RevAlloc(new PlusIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_SUM);
  }
  return result;
}

IntExpr* Solver::MakeDifference(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeDifference(left->Min(), right);
  }
  if (right->Bound()) {
    return MakeSum(left, -right->Min());
  }
  IntExpr* sub_left = nullptr;
  IntExpr* sub_right = nullptr;
  int64_t left_coef = 1;
  int64_t right_coef = 1;
  if (IsExprProduct(left, &sub_left, &left_coef) &&
      IsExprProduct(right, &sub_right, &right_coef)) {
    const int64_t abs_gcd = std::gcd(std::abs(left_coef), std::abs(right_coef));
    if (abs_gcd != 0 && abs_gcd != 1) {
      return MakeProd(MakeDifference(MakeProd(sub_left, left_coef / abs_gcd),
                                     MakeProd(sub_right, right_coef / abs_gcd)),
                      abs_gcd);
    }
  }

  IntExpr* result = Cache()->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_DIFFERENCE);
  if (result == nullptr) {
    if (!SubOverflows(left->Min(), right->Max()) &&
        !SubOverflows(left->Max(), right->Min())) {
      result = RegisterIntExpr(RevAlloc(new SubIntExpr(this, left, right)));
    } else {
      result = RegisterIntExpr(RevAlloc(new SafeSubIntExpr(this, left, right)));
    }
    Cache()->InsertExprExprExpression(result, left, right,
                                      ModelCache::EXPR_EXPR_DIFFERENCE);
  }
  return result;
}

// warning: this is 'value - expr'.
IntExpr* Solver::MakeDifference(int64_t value, IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    DCHECK(!SubOverflows(value, expr->Min()));
    return MakeIntConst(value - expr->Min());
  }
  if (value == 0) {
    return MakeOpposite(expr);
  }
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_DIFFERENCE);
  if (result == nullptr) {
    if (expr->IsVar() && expr->Min() != std::numeric_limits<int64_t>::min() &&
        !SubOverflows(value, expr->Min()) &&
        !SubOverflows(value, expr->Max())) {
      result = NewIntMinusVar(this, value, expr->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new SubIntCstExpr(this, expr, value)));
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_DIFFERENCE);
  }
  return result;
}

IntExpr* Solver::MakeOpposite(IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(CapOpp(expr->Min()));
  }
  IntExpr* result =
      Cache()->FindExprExpression(expr, ModelCache::EXPR_OPPOSITE);
  if (result == nullptr) {
    if (expr->IsVar()) {
      result = NewIntMinusVar(this, 0, expr->Var());
    } else {
      result = RegisterIntExpr(RevAlloc(new OppIntExpr(this, expr)));
    }
    Cache()->InsertExprExpression(result, expr, ModelCache::EXPR_OPPOSITE);
  }
  return result;
}

IntExpr* Solver::MakeProd(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  IntExpr* result = Cache()->FindExprConstantExpression(
      expr, value, ModelCache::EXPR_CONSTANT_PROD);
  if (result != nullptr) {
    return result;
  } else {
    IntExpr* m_expr = nullptr;
    int64_t coefficient = 1;
    if (IsExprProduct(expr, &m_expr, &coefficient)) {
      coefficient = CapProd(coefficient, value);
    } else {
      m_expr = expr;
      coefficient = value;
    }
    if (m_expr->Bound()) {
      return MakeIntConst(CapProd(coefficient, m_expr->Min()));
    } else if (coefficient == 1) {
      return m_expr;
    } else if (coefficient == -1) {
      return MakeOpposite(m_expr);
    } else if (coefficient > 0) {
      if (m_expr->Max() > std::numeric_limits<int64_t>::max() / coefficient ||
          m_expr->Min() < std::numeric_limits<int64_t>::min() / coefficient) {
        result = RegisterIntExpr(
            RevAlloc(new SafeTimesPosIntCstExpr(this, m_expr, coefficient)));
      } else {
        result = RegisterIntExpr(
            RevAlloc(new TimesPosIntCstExpr(this, m_expr, coefficient)));
      }
    } else if (coefficient == 0) {
      result = MakeIntConst(0);
    } else {  // coefficient < 0.
      result = RegisterIntExpr(
          RevAlloc(new TimesIntNegCstExpr(this, m_expr, coefficient)));
    }
    if (m_expr->IsVar() &&
        !absl::GetFlag(FLAGS_cp_disable_expression_optimization)) {
      result = result->Var();
    }
    Cache()->InsertExprConstantExpression(result, expr, value,
                                          ModelCache::EXPR_CONSTANT_PROD);
    return result;
  }
}

IntExpr* Solver::MakeProd(IntExpr* left, IntExpr* right) {
  if (left->Bound()) {
    return MakeProd(right, left->Min());
  }

  if (right->Bound()) {
    return MakeProd(left, right->Min());
  }

  // ----- Discover squares and powers -----

  IntExpr* tmp_left = left;
  IntExpr* tmp_right = right;
  int64_t left_exponant = 1;
  int64_t right_exponant = 1;
  IsExprPower(left, &tmp_left, &left_exponant);
  IsExprPower(right, &tmp_right, &right_exponant);

  if (tmp_left == tmp_right) {
    return MakePower(tmp_left, left_exponant + right_exponant);
  }

  // ----- Discover nested products -----

  tmp_left = left;
  tmp_right = right;
  int64_t coefficient = 1;

  // Parse left.
  for (;;) {
    IntExpr* sub_expr = nullptr;
    int64_t sub_coefficient = 1;
    if (IsExprProduct(tmp_left, &sub_expr, &sub_coefficient)) {
      coefficient = CapProd(coefficient, sub_coefficient);
      tmp_left = sub_expr;
    } else {
      break;
    }
  }

  // Parse right.
  for (;;) {
    IntExpr* sub_expr = nullptr;
    int64_t sub_coefficient = 1;
    if (IsExprProduct(tmp_right, &sub_expr, &sub_coefficient)) {
      coefficient = CapProd(coefficient, sub_coefficient);
      tmp_right = sub_expr;
    } else {
      break;
    }
  }

  if (coefficient != 1) {
    return MakeProd(MakeProd(tmp_left, tmp_right), coefficient);
  }

  // ----- Standard build -----

  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  IntExpr* result = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_PROD);
  if (result == nullptr) {
    result = model_cache_->FindExprExprExpression(right, left,
                                                  ModelCache::EXPR_EXPR_PROD);
  }
  if (result != nullptr) {
    return result;
  }
  if (left->IsVar() && left->Var()->VarType() == IntVar::BOOLEAN_VAR) {
    if (right->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(left), right)));
    }
  } else if (right->IsVar() && reinterpret_cast<IntVar*>(right)->VarType() ==
                                   IntVar::BOOLEAN_VAR) {
    if (left->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanPosIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    } else {
      result = RegisterIntExpr(RevAlloc(new TimesBooleanIntExpr(
          this, reinterpret_cast<BooleanVar*>(right), left)));
    }
  } else if (left->Min() >= 0 && right->Min() >= 0) {
    if (CapProd(left->Max(), right->Max()) ==
        std::numeric_limits<int64_t>::max()) {  // Potential overflow.
      result =
          RegisterIntExpr(RevAlloc(new SafeTimesPosIntExpr(this, left, right)));
    } else {
      result =
          RegisterIntExpr(RevAlloc(new TimesPosIntExpr(this, left, right)));
    }
  } else {
    result = RegisterIntExpr(RevAlloc(new TimesIntExpr(this, left, right)));
  }
  model_cache_->InsertExprExprExpression(result, left, right,
                                         ModelCache::EXPR_EXPR_PROD);
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* numerator, IntExpr* denominator) {
  CHECK(numerator != nullptr);
  CHECK(denominator != nullptr);
  if (denominator->Bound()) {
    return MakeDiv(numerator, denominator->Min());
  }
  IntExpr* result = model_cache_->FindExprExprExpression(
      numerator, denominator, ModelCache::EXPR_EXPR_DIV);
  if (result != nullptr) {
    return result;
  }

  if (denominator->Min() <= 0 && denominator->Max() >= 0) {
    AddConstraint(MakeNonEquality(denominator, 0));
  }

  if (denominator->Min() >= 0) {
    if (numerator->Min() >= 0) {
      result = RevAlloc(new DivPosPosIntExpr(this, numerator, denominator));
    } else {
      result = RevAlloc(new DivPosIntExpr(this, numerator, denominator));
    }
  } else if (denominator->Max() <= 0) {
    if (numerator->Max() <= 0) {
      result = RevAlloc(new DivPosPosIntExpr(this, MakeOpposite(numerator),
                                             MakeOpposite(denominator)));
    } else {
      result = MakeOpposite(RevAlloc(
          new DivPosIntExpr(this, numerator, MakeOpposite(denominator))));
    }
  } else {
    result = RevAlloc(new DivIntExpr(this, numerator, denominator));
  }
  model_cache_->InsertExprExprExpression(result, numerator, denominator,
                                         ModelCache::EXPR_EXPR_DIV);
  return result;
}

IntExpr* Solver::MakeDiv(IntExpr* expr, int64_t value) {
  CHECK(expr != nullptr);
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(expr->Min() / value);
  } else if (value == 1) {
    return expr;
  } else if (value == -1) {
    return MakeOpposite(expr);
  } else if (value > 0) {
    return RegisterIntExpr(RevAlloc(new DivPosIntCstExpr(this, expr, value)));
  } else if (value == 0) {
    LOG(FATAL) << "Cannot divide by 0";
    return nullptr;
  } else {
    return RegisterIntExpr(
        MakeOpposite(RevAlloc(new DivPosIntCstExpr(this, expr, -value))));
    // TODO(user) : implement special case.
  }
}

Constraint* Solver::MakeAbsEquality(IntVar* var, IntVar* abs_var) {
  if (Cache()->FindExprExpression(var, ModelCache::EXPR_ABS) == nullptr) {
    Cache()->InsertExprExpression(abs_var, var, ModelCache::EXPR_ABS);
  }
  return RevAlloc(new IntAbsConstraint(this, var, abs_var));
}

IntExpr* Solver::MakeAbs(IntExpr* e) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= 0) {
    return e;
  } else if (e->Max() <= 0) {
    return MakeOpposite(e);
  }
  IntExpr* result = Cache()->FindExprExpression(e, ModelCache::EXPR_ABS);
  if (result == nullptr) {
    int64_t coefficient = 1;
    IntExpr* expr = nullptr;
    if (IsExprProduct(e, &expr, &coefficient)) {
      result = MakeProd(MakeAbs(expr), std::abs(coefficient));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntAbs(this, e)));
    }
    Cache()->InsertExprExpression(result, e, ModelCache::EXPR_ABS);
  }
  return result;
}

IntExpr* Solver::MakeSquare(IntExpr* expr) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    const int64_t v = expr->Min();
    return MakeIntConst(v * v);
  }
  IntExpr* result = Cache()->FindExprExpression(expr, ModelCache::EXPR_SQUARE);
  if (result == nullptr) {
    if (expr->Min() >= 0) {
      result = RegisterIntExpr(RevAlloc(new PosIntSquare(this, expr)));
    } else {
      result = RegisterIntExpr(RevAlloc(new IntSquare(this, expr)));
    }
    Cache()->InsertExprExpression(result, expr, ModelCache::EXPR_SQUARE);
  }
  return result;
}

IntExpr* Solver::MakePower(IntExpr* expr, int64_t n) {
  CHECK_EQ(this, expr->solver());
  CHECK_GE(n, 0);
  if (expr->Bound()) {
    const int64_t v = expr->Min();
    if (v >= IntPowerOverflowLimit(n)) {  // Overflow.
      return MakeIntConst(std::numeric_limits<int64_t>::max());
    }
    return MakeIntConst(IntPowerValue(v, n));
  }
  switch (n) {
    case 0:
      return MakeIntConst(1);
    case 1:
      return expr;
    case 2:
      return MakeSquare(expr);
    default: {
      IntExpr* result = nullptr;
      if (n % 2 == 0) {  // even.
        if (expr->Min() >= 0) {
          result =
              RegisterIntExpr(RevAlloc(new PosIntEvenPower(this, expr, n)));
        } else {
          result = RegisterIntExpr(RevAlloc(new IntEvenPower(this, expr, n)));
        }
      } else {
        result = RegisterIntExpr(RevAlloc(new IntOddPower(this, expr, n)));
      }
      return result;
    }
  }
}

IntExpr* Solver::MakeMin(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeMin(right, left->Min());
  }
  if (right->Bound()) {
    return MakeMin(left, right->Min());
  }
  if (left->Min() >= right->Max()) {
    return right;
  }
  if (right->Min() >= left->Max()) {
    return left;
  }
  return RegisterIntExpr(RevAlloc(new MinIntExpr(this, left, right)));
}

IntExpr* Solver::MakeMin(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (value <= expr->Min()) {
    return MakeIntConst(value);
  }
  if (expr->Bound()) {
    return MakeIntConst(std::min(expr->Min(), value));
  }
  if (expr->Max() <= value) {
    return expr;
  }
  return RegisterIntExpr(RevAlloc(new MinCstIntExpr(this, expr, value)));
}

IntExpr* Solver::MakeMin(IntExpr* expr, int value) {
  return MakeMin(expr, static_cast<int64_t>(value));
}

IntExpr* Solver::MakeMax(IntExpr* left, IntExpr* right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeMax(right, left->Min());
  }
  if (right->Bound()) {
    return MakeMax(left, right->Min());
  }
  if (left->Min() >= right->Max()) {
    return left;
  }
  if (right->Min() >= left->Max()) {
    return right;
  }
  return RegisterIntExpr(RevAlloc(new MaxIntExpr(this, left, right)));
}

IntExpr* Solver::MakeMax(IntExpr* expr, int64_t value) {
  CHECK_EQ(this, expr->solver());
  if (expr->Bound()) {
    return MakeIntConst(std::max(expr->Min(), value));
  }
  if (value <= expr->Min()) {
    return expr;
  }
  if (expr->Max() <= value) {
    return MakeIntConst(value);
  }
  return RegisterIntExpr(RevAlloc(new MaxCstIntExpr(this, expr, value)));
}

IntExpr* Solver::MakeMax(IntExpr* expr, int value) {
  return MakeMax(expr, static_cast<int64_t>(value));
}

IntExpr* Solver::MakeConvexPiecewiseExpr(IntExpr* expr, int64_t early_cost,
                                         int64_t early_date, int64_t late_date,
                                         int64_t late_cost) {
  return RegisterIntExpr(RevAlloc(new SimpleConvexPiecewiseExpr(
      this, expr, early_cost, early_date, late_date, late_cost)));
}

IntExpr* Solver::MakeSemiContinuousExpr(IntExpr* expr, int64_t fixed_charge,
                                        int64_t step) {
  if (step == 0) {
    if (fixed_charge == 0) {
      return MakeIntConst(int64_t{0});
    } else {
      return RegisterIntExpr(
          RevAlloc(new SemiContinuousStepZeroExpr(this, expr, fixed_charge)));
    }
  } else if (step == 1) {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousStepOneExpr(this, expr, fixed_charge)));
  } else {
    return RegisterIntExpr(
        RevAlloc(new SemiContinuousExpr(this, expr, fixed_charge, step)));
  }
  // TODO(user) : benchmark with virtualization of
  // PosIntDivDown and PosIntDivUp - or function pointers.
}

IntExpr* Solver::MakePiecewiseLinearExpr(IntExpr* expr,
                                         const PiecewiseLinearFunction& f) {
  return RegisterIntExpr(RevAlloc(new PiecewiseLinearExpr(this, expr, f)));
}

// ----- Conditional Expression -----

IntExpr* Solver::MakeConditionalExpression(IntVar* condition, IntExpr* expr,
                                           int64_t unperformed_value) {
  if (condition->Min() == 1) {
    return expr;
  } else if (condition->Max() == 0) {
    return MakeIntConst(unperformed_value);
  } else {
    IntExpr* cache = Cache()->FindExprExprConstantExpression(
        condition, expr, unperformed_value,
        ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL);
    if (cache == nullptr) {
      cache = RevAlloc(
          new ExprWithEscapeValue(this, condition, expr, unperformed_value));
      Cache()->InsertExprExprConstantExpression(
          cache, condition, expr, unperformed_value,
          ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL);
    }
    return cache;
  }
}

// ----- Modulo -----

IntExpr* Solver::MakeModulo(IntExpr* x, int64_t mod) {
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  if (mod >= 0) {
    AddConstraint(MakeBetweenCt(result, 0, mod - 1));
  } else {
    AddConstraint(MakeBetweenCt(result, mod + 1, 0));
  }
  return result;
}

IntExpr* Solver::MakeModulo(IntExpr* x, IntExpr* mod) {
  if (mod->Bound()) {
    return MakeModulo(x, mod->Min());
  }
  IntVar* const result =
      MakeDifference(x, MakeProd(MakeDiv(x, mod), mod))->Var();
  AddConstraint(MakeLess(result, MakeAbs(mod)));
  AddConstraint(MakeGreater(result, MakeOpposite(MakeAbs(mod))));
  return result;
}

// It's good to have the two extreme values being symmetrical around zero:
// it makes mirroring easier.
const int64_t IntervalVar::kMaxValidValue =
    std::numeric_limits<int64_t>::max() >> 2;
const int64_t IntervalVar::kMinValidValue = -kMaxValidValue;

void IntervalVar::WhenAnything(Demon* d) {
  WhenStartRange(d);
  WhenDurationRange(d);
  WhenEndRange(d);
  WhenPerformedBound(d);
}

IntervalVar* Solver::MakeMirrorInterval(IntervalVar* interval_var) {
  return RegisterIntervalVar(
      RevAlloc(new MirrorIntervalVar(this, interval_var)));
}

IntervalVar* Solver::MakeIntervalRelaxedMax(IntervalVar* interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMax(interval_var)));
  }
}

IntervalVar* Solver::MakeIntervalRelaxedMin(IntervalVar* interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMin(interval_var)));
  }
}

IntervalVar* Solver::MakeFixedInterval(int64_t start, int64_t duration,
                                       const std::string& name) {
  return RevAlloc(new FixedInterval(this, start, duration, name));
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(int64_t start_min,
                                                  int64_t start_max,
                                                  int64_t duration,
                                                  bool optional,
                                                  const std::string& name) {
  if (start_min == start_max && !optional) {
    return MakeFixedInterval(start_min, duration, name);
  } else if (!optional) {
    return RegisterIntervalVar(RevAlloc(new FixedDurationPerformedIntervalVar(
        this, start_min, start_max, duration, name)));
  }
  return RegisterIntervalVar(RevAlloc(new FixedDurationIntervalVar(
      this, start_min, start_max, duration, optional, name)));
}

void Solver::MakeFixedDurationIntervalVarArray(
    int count, int64_t start_min, int64_t start_max, int64_t duration,
    bool optional, absl::string_view name, std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_min, start_max, duration, optional, var_name));
  }
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* start_variable,
                                                  int64_t duration,
                                                  const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK_GE(duration, 0);
  return RegisterIntervalVar(RevAlloc(
      new StartVarPerformedIntervalVar(this, start_variable, duration, name)));
}

// Creates an interval var with a fixed duration, and performed var.
// The duration must be greater than 0.
IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* start_variable,
                                                  int64_t duration,
                                                  IntVar* performed_variable,
                                                  const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK(performed_variable != nullptr);
  CHECK_GE(duration, 0);
  if (!performed_variable->Bound()) {
    StartVarIntervalVar* const interval =
        reinterpret_cast<StartVarIntervalVar*>(
            RegisterIntervalVar(RevAlloc(new StartVarIntervalVar(
                this, start_variable, duration, performed_variable, name))));
    AddConstraint(RevAlloc(new LinkStartVarIntervalVar(
        this, interval, start_variable, performed_variable)));
    return interval;
  } else if (performed_variable->Min() == 1) {
    return RegisterIntervalVar(RevAlloc(new StartVarPerformedIntervalVar(
        this, start_variable, duration, name)));
  }
  return nullptr;
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables, int64_t duration,
    absl::string_view name, std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(
        MakeFixedDurationIntervalVar(start_variables[i], duration, var_name));
  }
}

// This method fills the vector with interval variables built with
// the corresponding start variables.
void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int64_t> durations, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int> durations, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int> durations,
    const std::vector<IntVar*>& performed_variables, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    absl::Span<const int64_t> durations,
    const std::vector<IntVar*>& performed_variables, absl::string_view name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

// Variable Duration Interval Var

IntervalVar* Solver::MakeIntervalVar(int64_t start_min, int64_t start_max,
                                     int64_t duration_min, int64_t duration_max,
                                     int64_t end_min, int64_t end_max,
                                     bool optional, const std::string& name) {
  return RegisterIntervalVar(RevAlloc(new VariableDurationIntervalVar(
      this, start_min, start_max, duration_min, duration_max, end_min, end_max,
      optional, name)));
}

void Solver::MakeIntervalVarArray(int count, int64_t start_min,
                                  int64_t start_max, int64_t duration_min,
                                  int64_t duration_max, int64_t end_min,
                                  int64_t end_max, bool optional,
                                  absl::string_view name,
                                  std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = absl::StrCat(name, i);
    array->push_back(MakeIntervalVar(start_min, start_max, duration_min,
                                     duration_max, end_min, end_max, optional,
                                     var_name));
  }
}

// Synced Interval Vars
IntervalVar* Solver::MakeFixedDurationStartSyncedOnStartIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationStartSyncedOnEndIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(interval_var,
                                                            duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnStartIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, CapSub(offset, duration))));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnEndIntervalVar(
    IntervalVar* interval_var, int64_t duration, int64_t offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(
          interval_var, duration, CapSub(offset, duration))));
}

// ----- Factories from timetabling.cc and table.cc -----

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t,
                                            Solver::UnaryIntervalRelation r,
                                            int64_t d) {
  return RevAlloc(new IntervalUnaryRelation(this, t, d, r));
}

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t1,
                                            Solver::BinaryIntervalRelation r,
                                            IntervalVar* const t2) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, 0));
}

Constraint* Solver::MakeIntervalVarRelationWithDelay(
    IntervalVar* const t1, Solver::BinaryIntervalRelation r,
    IntervalVar* const t2, int64_t delay) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, delay));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2,
                                            IntVar* const alt) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, alt));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, nullptr));
}

Constraint* Solver::MakeAllowedAssignments(const std::vector<IntVar*>& vars,
                                           const IntTupleSet& tuples) {
  if (HasCompactDomains(vars)) {
    if (tuples.NumTuples() < kBitsInUint64 && parameters_.use_small_table()) {
      return RevAlloc(
          new SmallCompactPositiveTableConstraint(this, vars, tuples));
    } else {
      return RevAlloc(new CompactPositiveTableConstraint(this, vars, tuples));
    }
  }
  return RevAlloc(new PositiveTableConstraint(this, vars, tuples));
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64_t initial_state, const std::vector<int64_t>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

Constraint* Solver::MakeTransitionConstraint(
    const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
    int64_t initial_state, const std::vector<int>& final_states) {
  return RevAlloc(new TransitionConstraint(this, vars, transition_table,
                                           initial_state, final_states));
}

IntVar* Solver::MakeIntVar(int64_t min, int64_t max, const std::string& name) {
  if (min == max) {
    return MakeIntConst(min, name);
  }
  if (min == 0 && max == 1) {
    return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, name)));
  } else if (CapSub(max, min) == 1) {
    const std::string inner_name = "inner_" + name;
    return RegisterIntVar(
        MakeSum(RevAlloc(new ConcreteBooleanVar(this, inner_name)), min)
            ->VarWithName(name));
  } else {
    return RegisterIntVar(RevAlloc(new DomainIntVar(this, min, max, name)));
  }
}

IntVar* Solver::MakeIntVar(int64_t min, int64_t max) {
  return MakeIntVar(min, max, "");
}

IntVar* Solver::MakeBoolVar(const std::string& name) {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, name)));
}

IntVar* Solver::MakeBoolVar() {
  return RegisterIntVar(RevAlloc(new ConcreteBooleanVar(this, "")));
}

IntVar* Solver::MakeIntVar(const std::vector<int64_t>& values,
                           const std::string& name) {
  DCHECK(!values.empty());
  // Fast-track the case where we have a single value.
  if (values.size() == 1) return MakeIntConst(values[0], name);
  // Sort and remove duplicates.
  std::vector<int64_t> unique_sorted_values = values;
  gtl::STLSortAndRemoveDuplicates(&unique_sorted_values);
  // Case when we have a single value, after clean-up.
  if (unique_sorted_values.size() == 1) return MakeIntConst(values[0], name);
  // Case when the values are a dense interval of integers.
  if (unique_sorted_values.size() ==
      unique_sorted_values.back() - unique_sorted_values.front() + 1) {
    return MakeIntVar(unique_sorted_values.front(), unique_sorted_values.back(),
                      name);
  }
  // Compute the GCD: if it's not 1, we can express the variable's domain as
  // the product of the GCD and of a domain with smaller values.
  int64_t gcd = 0;
  for (const int64_t v : unique_sorted_values) {
    if (gcd == 0) {
      gcd = std::abs(v);
    } else {
      gcd = std::gcd(gcd, std::abs(v));  // Supports v==0.
    }
    if (gcd == 1) {
      // If it's 1, though, we can't do anything special, so we
      // immediately return a new DomainIntVar.
      return RegisterIntVar(
          RevAlloc(new DomainIntVar(this, unique_sorted_values, name)));
    }
  }
  DCHECK_GT(gcd, 1);
  for (int64_t& v : unique_sorted_values) {
    DCHECK_EQ(0, v % gcd);
    v /= gcd;
  }
  const std::string new_name = name.empty() ? "" : "inner_" + name;
  // Catch the case where the divided values are a dense set of integers.
  IntVar* inner_intvar = nullptr;
  if (unique_sorted_values.size() ==
      unique_sorted_values.back() - unique_sorted_values.front() + 1) {
    inner_intvar = MakeIntVar(unique_sorted_values.front(),
                              unique_sorted_values.back(), new_name);
  } else {
    inner_intvar = RegisterIntVar(
        RevAlloc(new DomainIntVar(this, unique_sorted_values, new_name)));
  }
  return MakeProd(inner_intvar, gcd)->Var();
}

IntVar* Solver::MakeIntVar(const std::vector<int64_t>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values,
                           const std::string& name) {
  return MakeIntVar(ToInt64Vector(values), name);
}

IntVar* Solver::MakeIntVar(const std::vector<int>& values) {
  return MakeIntVar(values, "");
}

IntVar* Solver::MakeIntConst(int64_t val, const std::string& name) {
  // If IntConst is going to be named after its creation,
  // cp_share_int_consts should be set to false otherwise names can potentially
  // be overwritten.
  if (absl::GetFlag(FLAGS_cp_share_int_consts) && name.empty() &&
      val >= MIN_CACHED_INT_CONST && val <= MAX_CACHED_INT_CONST) {
    return cached_constants_[val - MIN_CACHED_INT_CONST];
  }
  return RevAlloc(new IntConst(this, val, name));
}

IntVar* Solver::MakeIntConst(int64_t val) { return MakeIntConst(val, ""); }

namespace {
std::string IndexedName(absl::string_view prefix, int index, int max_index) {
#if 0
#if defined(_MSC_VER)
  const int digits = max_index > 0 ?
      static_cast<int>(log(1.0L * max_index) / log(10.0L)) + 1 :
      1;
#else
  const int digits = max_index > 0 ? static_cast<int>(log10(max_index)) + 1: 1;
#endif
  return absl::StrFormat("%s%0*d", prefix, digits, index);
#else
  return absl::StrCat(prefix, index);
#endif
}
}  // namespace

void Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                             const std::string& name,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax, IndexedName(name, i, var_count)));
  }
}

void Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                             std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeIntVar(vmin, vmax));
  }
}

IntVar** Solver::MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                                 const std::string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeIntVar(vmin, vmax, IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::MakeBoolVarArray(int var_count, const std::string& name,
                              std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar(IndexedName(name, i, var_count)));
  }
}

void Solver::MakeBoolVarArray(int var_count, std::vector<IntVar*>* vars) {
  for (int i = 0; i < var_count; ++i) {
    vars->push_back(MakeBoolVar());
  }
}

IntVar** Solver::MakeBoolVarArray(int var_count, const std::string& name) {
  IntVar** vars = new IntVar*[var_count];
  for (int i = 0; i < var_count; ++i) {
    vars[i] = MakeBoolVar(IndexedName(name, i, var_count));
  }
  return vars;
}

void Solver::InitCachedIntConstants() {
  for (int i = MIN_CACHED_INT_CONST; i <= MAX_CACHED_INT_CONST; ++i) {
    cached_constants_[i - MIN_CACHED_INT_CONST] =
        RevAlloc(new IntConst(this, i, ""));  // note the empty name
  }
}

Assignment* Solver::MakeAssignment() { return RevAlloc(new Assignment(this)); }

Assignment* Solver::MakeAssignment(const Assignment* const a) {
  return RevAlloc(new Assignment(a));
}

}  // namespace operations_research
