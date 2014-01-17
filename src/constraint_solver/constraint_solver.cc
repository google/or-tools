// Copyright 2010-2013 Google
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
//
// This file implements the core objects of the constraint solver:
// Solver, Search, Queue, ... along with the main resolution loop.

#include "constraint_solver/constraint_solver.h"

#include <csetjmp>
#include <string>
#include <iosfwd>
#include "base/unique_ptr.h"

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/file.h"
#include "base/recordio.h"
#include "base/stringpiece.h"
#include "zlib.h"
#include "base/concise_iterator.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/model.pb.h"
#include "util/tuple_set.h"

DEFINE_bool(cp_trace_propagation, false,
            "Trace propagation events (constraint and demon executions,"
            " variable modifications).");
DEFINE_bool(cp_trace_search, false, "Trace search events");
DEFINE_bool(cp_show_constraints, false,
            "show all constraints added to the solver.");
DEFINE_bool(cp_print_model, false,
            "use PrintModelVisitor on model before solving.");
DEFINE_bool(cp_model_stats, false,
            "use StatisticsModelVisitor on model before solving.");
DEFINE_string(cp_export_file, "", "Export model to file using CPModelProto.");
DEFINE_bool(cp_no_solve, false, "Force failure at the beginning of a search.");
DEFINE_string(cp_profile_file, "", "Export profiling overview to file.");
DEFINE_bool(cp_verbose_fail, false, "Verbose output when failing.");
DEFINE_bool(cp_name_variables, false, "Force all variables to have names.");
DEFINE_bool(cp_name_cast_variables, false,
            "Name variables casted from expressions");

void ConstraintSolverFailsHere() { VLOG(3) << "Fail"; }

#if defined(_MSC_VER)  // WINDOWS
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ----- SolverParameters -----

SolverParameters::SolverParameters()
    : compress_trail(kDefaultTrailCompression),
      trail_block_size(kDefaultTrailBlockSize),
      array_split_size(kDefaultArraySplitSize),
      store_names(kDefaultNameStoring),
      profile_level(kDefaultProfileLevel),
      trace_level(kDefaultTraceLevel),
      name_all_variables(kDefaultNameAllVariables) {}

// ----- Forward Declarations and Profiling Support -----
extern DemonProfiler* BuildDemonProfiler(Solver* const solver);
extern void DeleteDemonProfiler(DemonProfiler* const monitor);
extern void InstallDemonProfiler(DemonProfiler* const monitor);

// TODO(user): remove this complex logic.
// We need the double test because parameters are set too late when using
// python in the open source. This is the cheapest work-around.
bool Solver::InstrumentsDemons() const {
  return IsProfilingEnabled() || InstrumentsVariables();
}

bool Solver::IsProfilingEnabled() const {
  return parameters_.profile_level != SolverParameters::NO_PROFILING ||
         !FLAGS_cp_profile_file.empty();
}

bool Solver::InstrumentsVariables() const {
  return parameters_.trace_level != SolverParameters::NO_TRACE ||
         FLAGS_cp_trace_propagation;
}

bool Solver::NameAllVariables() const {
  return parameters_.name_all_variables || FLAGS_cp_name_variables;
}

// ------------------ Demon class ----------------

Solver::DemonPriority Demon::priority() const {
  return Solver::NORMAL_PRIORITY;
}

std::string Demon::DebugString() const { return "Demon"; }

void Demon::inhibit(Solver* const s) {
  if (stamp_ < kuint64max) {
    s->SaveAndSetValue(&stamp_, kuint64max);
  }
}

void Demon::desinhibit(Solver* const s) {
  if (stamp_ == kuint64max) {
    s->SaveAndSetValue(&stamp_, s->stamp() - 1);
  }
}

// ------------------ Action class ------------------

std::string Action::DebugString() const { return "Action"; }

// ------------------ Queue class ------------------

namespace {
class FifoPriorityQueue {
 public:
  struct Cell {
    explicit Cell(Demon* const d) : demon(d), next(nullptr) {}
    Demon* demon;
    Cell* next;
  };

  FifoPriorityQueue() : first_(nullptr), last_(nullptr), free_cells_(nullptr) {}

  ~FifoPriorityQueue() {
    while (first_ != nullptr) {
      Cell* const tmp = first_;
      first_ = tmp->next;
      delete tmp;
    }
    while (free_cells_ != nullptr) {
      Cell* const tmp = free_cells_;
      free_cells_ = tmp->next;
      delete tmp;
    }
  }

  Demon* Next() {
    if (first_ != nullptr) {
      DCHECK(last_ != nullptr);
      Cell* const tmp_cell = first_;
      Demon* const demon = tmp_cell->demon;
      first_ = tmp_cell->next;
      if (first_ == nullptr) {
        last_ = nullptr;
      }
      tmp_cell->next = free_cells_;
      free_cells_ = tmp_cell;
      return demon;
    }
    return nullptr;
  }

  void Enqueue(Demon* const d) {
    Cell* cell = free_cells_;
    if (cell != nullptr) {
      cell->demon = d;
      free_cells_ = cell->next;
      cell->next = nullptr;
    } else {
      cell = new Cell(d);
    }
    if (last_ != nullptr) {
      last_->next = cell;
      last_ = cell;
    } else {
      first_ = cell;
      last_ = cell;
    }
  }

  void AfterFailure() {
    if (first_ != nullptr) {
      last_->next = free_cells_;
      free_cells_ = first_;
      first_ = nullptr;
      last_ = nullptr;
    }
  }

 private:
  Cell* first_;
  Cell* last_;
  Cell* free_cells_;
};
}  // namespace

class Queue {
 public:
  static const int64 kTestPeriod = 10000;

  explicit Queue(Solver* const s)
      : solver_(s),
        stamp_(1),
        freeze_level_(0),
        in_process_(false),
        clear_action_(nullptr),
        in_add_(false),
        instruments_demons_(s->InstrumentsDemons()) {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      containers_[i] = new FifoPriorityQueue();
    }
  }

  ~Queue() {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      delete containers_[i];
      containers_[i] = nullptr;
    }
  }

  void Freeze() {
    freeze_level_++;
    stamp_++;
  }

  void Unfreeze() {
    if (--freeze_level_ == 0) {
      Process();
    }
  }

  void ProcessOneDemon(Demon* const demon) {
    demon->set_stamp(stamp_ - 1);
    if (!instruments_demons_) {
      if (++solver_->demon_runs_[demon->priority()] % kTestPeriod == 0) {
        solver_->TopPeriodicCheck();
      }
      demon->Run(solver_);
    } else {
      solver_->GetPropagationMonitor()->BeginDemonRun(demon);
      if (++solver_->demon_runs_[demon->priority()] % kTestPeriod == 0) {
        solver_->TopPeriodicCheck();
      }
      demon->Run(solver_);
      solver_->GetPropagationMonitor()->EndDemonRun(demon);
    }
  }

  void ProcessNormalDemon(Demon* const demon) {
    if (++solver_->demon_runs_[Solver::NORMAL_PRIORITY] % kTestPeriod == 0) {
      solver_->TopPeriodicCheck();
    }
    demon->Run(solver_);
  }

  void ProcessInstrumentedNormalDemon(Demon* const demon) {
    solver_->GetPropagationMonitor()->BeginDemonRun(demon);
    if (++solver_->demon_runs_[Solver::NORMAL_PRIORITY] % kTestPeriod == 0) {
      solver_->TopPeriodicCheck();
    }
    demon->Run(solver_);
    solver_->GetPropagationMonitor()->EndDemonRun(demon);
  }

  void Process() {
    if (!in_process_) {
      in_process_ = true;
      Demon* d = nullptr;
      while ((d = containers_[Solver::VAR_PRIORITY]->Next()) != nullptr ||
             (d = containers_[Solver::DELAYED_PRIORITY]->Next()) != nullptr) {
        ProcessOneDemon(d);
      }
      in_process_ = false;
    }
  }

  void Execute(Demon* const demon) {
    if (demon->stamp() < stamp_) {
      if (demon->priority() == Solver::NORMAL_PRIORITY) {
        if (!instruments_demons_) {
          ProcessNormalDemon(demon);
        } else {
          ProcessInstrumentedNormalDemon(demon);
        }
      } else {
        DCHECK_EQ(demon->priority(), Solver::DELAYED_PRIORITY);
        demon->set_stamp(stamp_);
        containers_[Solver::DELAYED_PRIORITY]->Enqueue(demon);
      }
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
        }
      }
    } else {
      for (SimpleRevFIFO<Demon*>::Iterator it(&demons); it.ok(); ++it) {
        Demon* const demon = *it;
        if (demon->stamp() < stamp_) {
          DCHECK_EQ(demon->priority(), Solver::NORMAL_PRIORITY);
          ProcessInstrumentedNormalDemon(demon);
        }
      }
    }
  }

  void EnqueueAll(const SimpleRevFIFO<Demon*>& demons) {
    for (SimpleRevFIFO<Demon*>::Iterator it(&demons); it.ok(); ++it) {
      Demon* const demon = *it;
      DCHECK_EQ(demon->priority(), Solver::DELAYED_PRIORITY);
      demon->set_stamp(stamp_);
      containers_[Solver::DELAYED_PRIORITY]->Enqueue(demon);
    }
  }

  void EnqueueVar(Demon* const demon) {
    DCHECK(demon->priority() == Solver::VAR_PRIORITY);
    if (demon->stamp() < stamp_) {
      demon->set_stamp(stamp_);
      containers_[Solver::VAR_PRIORITY]->Enqueue(demon);
      if (freeze_level_ == 0) {
        Process();
      }
    }
  }

  void EnqueueDelayedDemon(Demon* const demon) {
    DCHECK(demon->priority() == Solver::DELAYED_PRIORITY);
    if (demon->stamp() < stamp_) {
      demon->set_stamp(stamp_);
      containers_[Solver::DELAYED_PRIORITY]->Enqueue(demon);
    }
  }

  void AfterFailure() {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      containers_[i]->AfterFailure();
    }
    if (clear_action_ != nullptr) {
      clear_action_->Run(solver_);
      clear_action_ = nullptr;
    }
    freeze_level_ = 0;
    in_process_ = false;
    in_add_ = false;
    to_add_.clear();
  }

  void increase_stamp() { stamp_++; }

  uint64 stamp() const { return stamp_; }

  void set_action_on_fail(Action* const a) { clear_action_ = a; }

  void clear_action_on_fail() { clear_action_ = nullptr; }

  void AddConstraint(Constraint* const c) {
    to_add_.push_back(c);
    ProcessConstraints();
  }

  void ProcessConstraints() {
    if (!in_add_) {
      in_add_ = true;
      // We cannot store to_add_.size() as constraints can add other
      // constraints.
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
  FifoPriorityQueue* containers_[Solver::kNumPriorities];
  uint64 stamp_;
  // The number of nested freeze levels. The queue is frozen if and only if
  // freeze_level_ > 0.
  uint32 freeze_level_;
  bool in_process_;
  Action* clear_action_;
  std::vector<Constraint*> to_add_;
  bool in_add_;
  const bool instruments_demons_;
};

// ------------------ StateMarker / StateInfo struct -----------

struct StateInfo {  // This is an internal structure to store
                    // additional information on the choice point.
 public:
  StateInfo() : ptr_info(nullptr), int_info(0), depth(0), left_depth(0) {}
  StateInfo(void* pinfo, int iinfo)
      : ptr_info(pinfo), int_info(iinfo), depth(0), left_depth(0) {}
  StateInfo(void* pinfo, int iinfo, int d, int ld)
      : ptr_info(pinfo), int_info(iinfo), depth(d), left_depth(ld) {}
  void* ptr_info;
  int int_info;
  int depth;
  int left_depth;
};

struct StateMarker {
 public:
  StateMarker(Solver::MarkerType t, const StateInfo& info);
  friend class Solver;
  friend struct Trail;

 private:
  Solver::MarkerType type_;
  int rev_int_index_;
  int rev_int64_index_;
  int rev_uint64_index_;
  int rev_double_index_;
  int rev_ptr_index_;
  int rev_boolvar_list_index_;
  int rev_bools_index_;
  int rev_int_memory_index_;
  int rev_int64_memory_index_;
  int rev_double_memory_index_;
  int rev_object_memory_index_;
  int rev_object_array_memory_index_;
  int rev_memory_index_;
  int rev_memory_array_index_;
  StateInfo info_;
};

StateMarker::StateMarker(Solver::MarkerType t, const StateInfo& info)
    : type_(t),
      rev_int_index_(0),
      rev_int64_index_(0),
      rev_uint64_index_(0),
      rev_double_index_(0),
      rev_ptr_index_(0),
      rev_boolvar_list_index_(0),
      rev_bools_index_(0),
      rev_int_memory_index_(0),
      rev_int64_memory_index_(0),
      rev_double_memory_index_(0),
      rev_object_memory_index_(0),
      rev_object_array_memory_index_(0),
      info_(info) {}

// ---------- Trail and Reversibility ----------

namespace {
// ----- addrval struct -----

// This template class is used internally to implement reversibility.
// It stores an address and the value that was at the address.
template <class T>
struct addrval {
 public:
  addrval() : address_(nullptr) {}
  explicit addrval(T* adr) : address_(adr), old_value_(*adr) {}
  void restore() const { (*address_) = old_value_; }

 private:
  T* address_;
  T old_value_;
};

// ----- Compressed trail -----

// ---------- Trail Packer ---------
// Abstract class to pack trail blocks.

template <class T>
class TrailPacker {
 public:
  explicit TrailPacker(int block_size) : block_size_(block_size) {}
  virtual ~TrailPacker() {}
  int input_size() const { return block_size_ * sizeof(addrval<T>); }
  virtual void Pack(const addrval<T>* block, std::string* packed_block) = 0;
  virtual void Unpack(const std::string& packed_block, addrval<T>* block) = 0;

 private:
  const int block_size_;
  DISALLOW_COPY_AND_ASSIGN(TrailPacker);
};


template <class T>
class NoCompressionTrailPacker : public TrailPacker<T> {
 public:
  explicit NoCompressionTrailPacker(int block_size)
      : TrailPacker<T>(block_size) {}
  virtual ~NoCompressionTrailPacker() {}
  virtual void Pack(const addrval<T>* block, std::string* packed_block) {
    DCHECK(block != nullptr);
    DCHECK(packed_block != nullptr);
    StringPiece block_str;
    block_str.set(block, this->input_size());
    block_str.CopyToString(packed_block);
  }
  virtual void Unpack(const std::string& packed_block, addrval<T>* block) {
    DCHECK(block != nullptr);
    memcpy(block, packed_block.c_str(), packed_block.size());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoCompressionTrailPacker<T>);
};

template <class T>
class ZlibTrailPacker : public TrailPacker<T> {
 public:
  explicit ZlibTrailPacker(int block_size)
      : TrailPacker<T>(block_size),
        tmp_size_(compressBound(this->input_size())),
        tmp_block_(new char[tmp_size_]) {}

  virtual ~ZlibTrailPacker() {}

  virtual void Pack(const addrval<T>* block, std::string* packed_block) {
    DCHECK(block != nullptr);
    DCHECK(packed_block != nullptr);
    uLongf size = tmp_size_;
    const int result =
        compress(reinterpret_cast<Bytef*>(tmp_block_.get()), &size,
                 reinterpret_cast<const Bytef*>(block), this->input_size());
    CHECK_EQ(Z_OK, result);
    StringPiece block_str;
    block_str.set(tmp_block_.get(), size);
    block_str.CopyToString(packed_block);
  }

  virtual void Unpack(const std::string& packed_block, addrval<T>* block) {
    DCHECK(block != nullptr);
    uLongf size = this->input_size();
    const int result =
        uncompress(reinterpret_cast<Bytef*>(block), &size,
                   reinterpret_cast<const Bytef*>(packed_block.c_str()),
                   packed_block.size());
    CHECK_EQ(Z_OK, result);
  }

 private:
  const uint64 tmp_size_;
  std::unique_ptr<char[]> tmp_block_;
  DISALLOW_COPY_AND_ASSIGN(ZlibTrailPacker<T>);
};

template <class T>
class CompressedTrail {
 public:
  CompressedTrail(int block_size,
                  SolverParameters::TrailCompression compression_level)
      : block_size_(block_size),
        blocks_(nullptr),
        free_blocks_(nullptr),
        data_(new addrval<T>[block_size]),
        buffer_(new addrval<T>[block_size]),
        buffer_used_(false),
        current_(0),
        size_(0) {
    switch (compression_level) {
      case SolverParameters::NO_COMPRESSION: {
        packer_.reset(new NoCompressionTrailPacker<T>(block_size));
        break;
      }
      case SolverParameters::COMPRESS_WITH_ZLIB: {
        packer_.reset(new ZlibTrailPacker<T>(block_size));
        break;
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
  int64 size() const { return size_; }

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

  std::unique_ptr<TrailPacker<T> > packer_;
  const int block_size_;
  Block* blocks_;
  Block* free_blocks_;
  std::unique_ptr<addrval<T> []> data_;
  std::unique_ptr<addrval<T> []> buffer_;
  bool buffer_used_;
  int current_;
  int size_;
};
}  // namespace

// ----- Trail -----

// Object are explicitely copied using the copy ctor instead of
// passing and storing a pointer. As objects are small, copying is
// much faster than allocating (around 35% on a complete solve).

extern void RestoreBoolValue(IntVar* const var);

struct Trail {
  CompressedTrail<int> rev_ints_;
  CompressedTrail<int64> rev_int64s_;
  CompressedTrail<uint64> rev_uint64s_;
  CompressedTrail<double> rev_doubles_;
  CompressedTrail<void*> rev_ptrs_;
  std::vector<IntVar*> rev_boolvar_list_;
  std::vector<bool*> rev_bools_;
  std::vector<bool> rev_bool_value_;
  std::vector<int*> rev_int_memory_;
  std::vector<int64*> rev_int64_memory_;
  std::vector<double*> rev_double_memory_;
  std::vector<BaseObject*> rev_object_memory_;
  std::vector<BaseObject**> rev_object_array_memory_;
  std::vector<void*> rev_memory_;
  std::vector<void**> rev_memory_array_;

  Trail(int block_size, SolverParameters::TrailCompression compression_level)
      : rev_ints_(block_size, compression_level),
        rev_int64s_(block_size, compression_level),
        rev_uint64s_(block_size, compression_level),
        rev_doubles_(block_size, compression_level),
        rev_ptrs_(block_size, compression_level) {}

  void BacktrackTo(StateMarker* m) {
    int target = m->rev_int_index_;
    for (int curr = rev_ints_.size(); curr > target; --curr) {
      const addrval<int>& cell = rev_ints_.Back();
      cell.restore();
      rev_ints_.PopBack();
    }
    DCHECK_EQ(rev_ints_.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_int64_index_;
    for (int curr = rev_int64s_.size(); curr > target; --curr) {
      const addrval<int64>& cell = rev_int64s_.Back();
      cell.restore();
      rev_int64s_.PopBack();
    }
    DCHECK_EQ(rev_int64s_.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_uint64_index_;
    for (int curr = rev_uint64s_.size(); curr > target; --curr) {
      const addrval<uint64>& cell = rev_uint64s_.Back();
      cell.restore();
      rev_uint64s_.PopBack();
    }
    DCHECK_EQ(rev_uint64s_.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_double_index_;
    for (int curr = rev_doubles_.size(); curr > target; --curr) {
      const addrval<double>& cell = rev_doubles_.Back();
      cell.restore();
      rev_doubles_.PopBack();
    }
    DCHECK_EQ(rev_doubles_.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_ptr_index_;
    for (int curr = rev_ptrs_.size(); curr > target; --curr) {
      const addrval<void*>& cell = rev_ptrs_.Back();
      cell.restore();
      rev_ptrs_.PopBack();
    }
    DCHECK_EQ(rev_ptrs_.size(), target);
    // Incorrect trail size after backtrack.
    target = m->rev_boolvar_list_index_;
    for (int curr = rev_boolvar_list_.size() - 1; curr >= target; --curr) {
      IntVar* const var = rev_boolvar_list_[curr];
      RestoreBoolValue(var);
    }
    rev_boolvar_list_.resize(target);

    DCHECK_EQ(rev_bools_.size(), rev_bool_value_.size());
    target = m->rev_bools_index_;
    for (int curr = rev_bools_.size() - 1; curr >= target; --curr) {
      *(rev_bools_[curr]) = rev_bool_value_[curr];
    }
    rev_bools_.resize(target);
    rev_bool_value_.resize(target);

    target = m->rev_int_memory_index_;
    for (int curr = rev_int_memory_.size() - 1; curr >= target; --curr) {
      delete[] rev_int_memory_[curr];
    }
    rev_int_memory_.resize(target);

    target = m->rev_int64_memory_index_;
    for (int curr = rev_int64_memory_.size() - 1; curr >= target; --curr) {
      delete[] rev_int64_memory_[curr];
    }
    rev_int64_memory_.resize(target);

    target = m->rev_double_memory_index_;
    for (int curr = rev_double_memory_.size() - 1; curr >= target; --curr) {
      delete[] rev_double_memory_[curr];
    }
    rev_double_memory_.resize(target);

    target = m->rev_object_memory_index_;
    for (int curr = rev_object_memory_.size() - 1; curr >= target; --curr) {
      delete rev_object_memory_[curr];
    }
    rev_object_memory_.resize(target);

    target = m->rev_object_array_memory_index_;
    for (int curr = rev_object_array_memory_.size() - 1; curr >= target;
         --curr) {
      delete[] rev_object_array_memory_[curr];
    }
    rev_object_array_memory_.resize(target);

    target = m->rev_memory_index_;
    for (int curr = rev_memory_.size() - 1; curr >= target; --curr) {
      delete reinterpret_cast<char*>(rev_memory_[curr]);
      // The previous cast is necessary to deallocate generic memory
      // described by a void* when passed to the RevAlloc procedure
      // We cannot do a delete[] there
      // This is useful for cells of RevFIFO and should not be used outside
      // of the product
    }
    rev_memory_.resize(target);

    target = m->rev_memory_array_index_;
    for (int curr = rev_memory_array_.size() - 1; curr >= target; --curr) {
      delete[] rev_memory_array_[curr];
      // delete [] version of the previous unsafe case.
    }
    rev_memory_array_.resize(target);
  }
};

void Solver::InternalSaveValue(int* valptr) {
  trail_->rev_ints_.PushBack(addrval<int>(valptr));
}

void Solver::InternalSaveValue(int64* valptr) {
  trail_->rev_int64s_.PushBack(addrval<int64>(valptr));
}

void Solver::InternalSaveValue(uint64* valptr) {
  trail_->rev_uint64s_.PushBack(addrval<uint64>(valptr));
}

void Solver::InternalSaveValue(double* valptr) {
  trail_->rev_doubles_.PushBack(addrval<double>(valptr));
}

void Solver::InternalSaveValue(void** valptr) {
  trail_->rev_ptrs_.PushBack(addrval<void*>(valptr));
}

// TODO(user) : this code is unsafe if you save the same alternating
// bool multiple times.
// The correct code should use a bitset and a single list.
void Solver::InternalSaveValue(bool* valptr) {
  trail_->rev_bools_.push_back(valptr);
  trail_->rev_bool_value_.push_back(*valptr);
}

BaseObject* Solver::SafeRevAlloc(BaseObject* ptr) {
  check_alloc_state();
  trail_->rev_object_memory_.push_back(ptr);
  return ptr;
}

int* Solver::SafeRevAllocArray(int* ptr) {
  check_alloc_state();
  trail_->rev_int_memory_.push_back(ptr);
  return ptr;
}

int64* Solver::SafeRevAllocArray(int64* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory_.push_back(ptr);
  return ptr;
}

double* Solver::SafeRevAllocArray(double* ptr) {
  check_alloc_state();
  trail_->rev_double_memory_.push_back(ptr);
  return ptr;
}

uint64* Solver::SafeRevAllocArray(uint64* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory_.push_back(reinterpret_cast<int64*>(ptr));
  return ptr;
}

BaseObject** Solver::SafeRevAllocArray(BaseObject** ptr) {
  check_alloc_state();
  trail_->rev_object_array_memory_.push_back(ptr);
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
  trail_->rev_memory_.push_back(ptr);
  return ptr;
}

void** Solver::UnsafeRevAllocArrayAux(void** ptr) {
  check_alloc_state();
  trail_->rev_memory_array_.push_back(ptr);
  return ptr;
}

void InternalSaveBooleanVarValue(Solver* const solver, IntVar* const var) {
  solver->trail_->rev_boolvar_list_.push_back(var);
}

// ------------------ Search class -----------------

class Search {
 public:
  explicit Search(Solver* const s)
      : solver_(s),
        marker_stack_(),
        fail_buffer_(),
        solution_counter_(0),
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
  Search(Solver* const s, int /* dummy_argument */)
      : solver_(s),
        marker_stack_(),
        fail_buffer_(),
        solution_counter_(0),
        decision_builder_(nullptr),
        created_by_solve_(false),
        search_depth_(-1),
        left_search_depth_(-1),
        should_restart_(false),
        should_finish_(false),
        sentinel_pushed_(0),
        jmpbuf_filled_(false),
        backtrack_at_the_end_of_the_search_(true) {}

  ~Search() { STLDeleteElements(&marker_stack_); }

  void EnterSearch();
  void RestartSearch();
  void ExitSearch();
  void BeginNextDecision(DecisionBuilder* const b);
  void EndNextDecision(DecisionBuilder* const b, Decision* const d);
  void ApplyDecision(Decision* const d);
  void AfterDecision(Decision* const d, bool apply);
  void RefuteDecision(Decision* const d);
  void BeginFail();
  void EndFail();
  void BeginInitialPropagation();
  void EndInitialPropagation();
  bool AtSolution();
  bool AcceptSolution();
  void NoMoreSolutions();
  bool LocalOptimum();
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta);
  void AcceptNeighbor();
  void PeriodicCheck();
  int ProgressPercent();
  void Accept(ModelVisitor* const visitor) const;
  void push_monitor(SearchMonitor* const m);
  void Clear();
  void IncrementSolutionCounter() { ++solution_counter_; }
  int64 solution_counter() const { return solution_counter_; }
  void set_decision_builder(DecisionBuilder* const db) {
    decision_builder_ = db;
  }
  DecisionBuilder* decision_builder() const { return decision_builder_; }
  void set_created_by_solve(bool c) { created_by_solve_ = c; }
  bool created_by_solve() const { return created_by_solve_; }
  Solver::DecisionModification ModifyDecision();
  void SetBranchSelector(
      ResultCallback1<Solver::DecisionModification, Solver*>* const s);
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
  std::vector<SearchMonitor*> monitors_;
  jmp_buf fail_buffer_;
  int64 solution_counter_;
  DecisionBuilder* decision_builder_;
  bool created_by_solve_;
  std::unique_ptr<ResultCallback1<Solver::DecisionModification, Solver*> >
      selector_;
  int search_depth_;
  int left_search_depth_;
  bool should_restart_;
  bool should_finish_;
  int sentinel_pushed_;
  bool jmpbuf_filled_;
  bool backtrack_at_the_end_of_the_search_;
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
#define CP_TRY(search)                                                 \
  CHECK(!search->jmpbuf_filled_) << "Fail() called outside search";    \
  search->jmpbuf_filled_ = true;                                       \
  try
#define CP_ON_FAIL catch(FailException&)
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
class UndoBranchSelector : public Action {
 public:
  explicit UndoBranchSelector(int depth) : depth_(depth) {}
  virtual ~UndoBranchSelector() {}
  virtual void Run(Solver* const s) {
    if (s->SolveDepth() == depth_) {
      s->ActiveSearch()->SetBranchSelector(nullptr);
    }
  }
  virtual std::string DebugString() const {
    return StringPrintf("UndoBranchSelector(%i)", depth_);
  }

 private:
  const int depth_;
};

class ApplyBranchSelector : public DecisionBuilder {
 public:
  explicit ApplyBranchSelector(
      ResultCallback1<Solver::DecisionModification, Solver*>* const bs)
      : selector_(bs) {}
  virtual ~ApplyBranchSelector() {}

  virtual Decision* Next(Solver* const s) {
    s->SetBranchSelector(selector_);
    return nullptr;
  }

  virtual std::string DebugString() const { return "Apply(BranchSelector)"; }

 private:
  ResultCallback1<Solver::DecisionModification, Solver*>* const selector_;
};
}  // namespace

void Search::SetBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  CHECK(bs == selector_.get() || selector_ == nullptr || bs == nullptr);
  if (selector_.get() != bs) {
    selector_.reset(bs);
  }
}

void Solver::SetBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  bs->CheckIsRepeatable();
  // We cannot use the trail as the search can be nested and thus
  // deleted upon backtrack. Thus we guard the undo action by a
  // check on the number of nesting of solve().
  AddBacktrackAction(RevAlloc(new UndoBranchSelector(SolveDepth())), false);
  searches_.back()->SetBranchSelector(bs);
}

DecisionBuilder* Solver::MakeApplyBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  return RevAlloc(new ApplyBranchSelector(bs));
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
    return selector_->Run(solver_);
  }
  return Solver::NO_CHANGE;
}

void Search::push_monitor(SearchMonitor* const m) {
  if (m) {
    monitors_.push_back(m);
  }
}

void Search::Clear() {
  monitors_.clear();
  search_depth_ = 0;
  left_search_depth_ = 0;
  selector_.reset(nullptr);
  backtrack_at_the_end_of_the_search_ = true;
}

void Search::EnterSearch() {
  // The solution counter is reset when entering search and not when
  // leaving search. This enables the information to persist outside of
  // top-level search.
  solution_counter_ = 0;

  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->EnterSearch();
  }
}

void Search::ExitSearch() {
  // Backtrack to the correct state.

  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->ExitSearch();
  }
}

void Search::RestartSearch() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->RestartSearch();
  }
}

void Search::BeginNextDecision(DecisionBuilder* const db) {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->BeginNextDecision(db);
  }
  CheckFail();
}

void Search::EndNextDecision(DecisionBuilder* const db, Decision* const d) {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->EndNextDecision(db, d);
  }
  CheckFail();
}

void Search::ApplyDecision(Decision* const d) {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->ApplyDecision(d);
  }
  CheckFail();
}

void Search::AfterDecision(Decision* const d, bool apply) {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->AfterDecision(d, apply);
  }
  CheckFail();
}

void Search::RefuteDecision(Decision* const d) {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->RefuteDecision(d);
  }
  CheckFail();
}

void Search::BeginFail() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->BeginFail();
  }
}

void Search::EndFail() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->EndFail();
  }
}

void Search::BeginInitialPropagation() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->BeginInitialPropagation();
  }
}

void Search::EndInitialPropagation() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->EndInitialPropagation();
  }
}

bool Search::AcceptSolution() {
  bool valid = true;
  for (int index = 0; index < monitors_.size(); ++index) {
    if (!monitors_[index]->AcceptSolution()) {
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
  for (int index = 0; index < monitors_.size(); ++index) {
    if (monitors_[index]->AtSolution()) {
      // Even though we know the return value, we cannot return yet: this would
      // break the contract we have with solution monitors. They all deserve
      // a chance to look at the solution.
      should_continue = true;
    }
  }
  return should_continue;
}

void Search::NoMoreSolutions() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->NoMoreSolutions();
  }
}

bool Search::LocalOptimum() {
  bool res = false;
  for (int index = 0; index < monitors_.size(); ++index) {
    if (monitors_[index]->LocalOptimum()) {
      res = true;
    }
  }
  return res;
}

bool Search::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  bool accept = true;
  for (int index = 0; index < monitors_.size(); ++index) {
    if (!monitors_[index]->AcceptDelta(delta, deltadelta)) {
      accept = false;
    }
  }
  return accept;
}

void Search::AcceptNeighbor() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->AcceptNeighbor();
  }
}

void Search::PeriodicCheck() {
  for (int index = 0; index < monitors_.size(); ++index) {
    monitors_[index]->PeriodicCheck();
  }
}

int Search::ProgressPercent() {
  int progress = SearchMonitor::kNoProgress;
  for (int index = 0; index < monitors_.size(); ++index) {
    progress = std::max(progress, monitors_[index]->ProgressPercent());
  }
  return progress;
}

void Search::Accept(ModelVisitor* const visitor) const {
  for (int index = 0; index < monitors_.size(); ++index) {
    DCHECK(monitors_[index] != nullptr);
    monitors_[index]->Accept(visitor);
  }
  if (decision_builder_ != nullptr) {
    decision_builder_->Accept(visitor);
  }
}

bool LocalOptimumReached(Search* const search) {
  return search->LocalOptimum();
}

bool AcceptDelta(Search* const search, Assignment* delta,
                 Assignment* deltadelta) {
  return search->AcceptDelta(delta, deltadelta);
}

void AcceptNeighbor(Search* const search) { search->AcceptNeighbor(); }

namespace {

// ---------- Fail Decision ----------

class FailDecision : public Decision {
 public:
  virtual void Apply(Solver* const s) { s->Fail(); }
  virtual void Refute(Solver* const s) { s->Fail(); }
};

// Balancing decision

class BalancingDecision : public Decision {
 public:
  virtual ~BalancingDecision() {}
  virtual void Apply(Solver* const s) {}
  virtual void Refute(Solver* const s) {}
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

extern Action* NewDomainIntVarCleaner();
extern PropagationMonitor* BuildTrace(Solver* const s);
extern ModelCache* BuildModelCache(Solver* const solver);
extern DependencyGraph* BuildDependencyGraph(Solver* const solver);

std::string Solver::model_name() const { return name_; }

Solver::Solver(const std::string& name, const SolverParameters& parameters)
    : name_(name),
      parameters_(parameters),
      queue_(new Queue(this)),
      trail_(new Trail(parameters.trail_block_size, parameters.compress_trail)),
      state_(OUTSIDE_SEARCH),
      branches_(0),
      fails_(0),
      decisions_(0),
      neighbors_(0),
      filtered_neighbors_(0),
      accepted_neighbors_(0),
      variable_cleaner_(NewDomainIntVarCleaner()),
      timer_(new ClockTimer),
      searches_(1, new Search(this, 0)),
      random_(ACMRandom::DeterministicSeed()),
      fail_hooks_(nullptr),
      fail_stamp_(GG_ULONGLONG(1)),
      balancing_decision_(new BalancingDecision),
      fail_intercept_(nullptr),
      demon_profiler_(BuildDemonProfiler(this)),
      true_constraint_(nullptr),
      false_constraint_(nullptr),
      fail_decision_(new FailDecision()),
      constraint_index_(0),
      additional_constraint_index_(0),
      propagation_monitor_(BuildTrace(this)),
      print_trace_(nullptr),
      anonymous_variable_index_(0) {
  Init();
}

Solver::Solver(const std::string& name)
    : name_(name),
      parameters_(),
      queue_(new Queue(this)),
      trail_(
          new Trail(parameters_.trail_block_size, parameters_.compress_trail)),
      state_(OUTSIDE_SEARCH),
      branches_(0),
      fails_(0),
      decisions_(0),
      neighbors_(0),
      filtered_neighbors_(0),
      accepted_neighbors_(0),
      variable_cleaner_(NewDomainIntVarCleaner()),
      timer_(new ClockTimer),
      searches_(1, new Search(this, 0)),
      random_(ACMRandom::DeterministicSeed()),
      fail_hooks_(nullptr),
      fail_stamp_(GG_ULONGLONG(1)),
      balancing_decision_(new BalancingDecision),
      fail_intercept_(nullptr),
      demon_profiler_(BuildDemonProfiler(this)),
      true_constraint_(nullptr),
      false_constraint_(nullptr),
      fail_decision_(new FailDecision()),
      constraint_index_(0),
      additional_constraint_index_(0),
      propagation_monitor_(BuildTrace(this)),
      print_trace_(nullptr),
      anonymous_variable_index_(0) {
  Init();
}

void Solver::Init() {
  for (int i = 0; i < kNumPriorities; ++i) {
    demon_runs_[i] = 0;
  }
  searches_.push_back(new Search(this));
  PushSentinel(SOLVER_CTOR_SENTINEL);
  InitCachedIntConstants();  // to be called after the SENTINEL is set.
  InitCachedConstraint();    // Cache the true constraint.
  InitBuilders();
  timer_->Restart();
  model_cache_.reset(BuildModelCache(this));
  dependency_graph_.reset(BuildDependencyGraph(this));
  AddPropagationMonitor(reinterpret_cast<PropagationMonitor*>(demon_profiler_));
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
  STLDeleteElements(&searches_);
  DeleteDemonProfiler(demon_profiler_);
  DeleteBuilders();
}

const SolverParameters::TrailCompression
SolverParameters::kDefaultTrailCompression = SolverParameters::NO_COMPRESSION;
const int SolverParameters::kDefaultTrailBlockSize = 8000;
const int SolverParameters::kDefaultArraySplitSize = 16;
const bool SolverParameters::kDefaultNameStoring = true;
const SolverParameters::ProfileLevel SolverParameters::kDefaultProfileLevel =
    SolverParameters::NO_PROFILING;
const SolverParameters::TraceLevel SolverParameters::kDefaultTraceLevel =
    SolverParameters::NO_TRACE;
const bool SolverParameters::kDefaultNameAllVariables = false;

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
  StringAppendF(&out, ", branches = %" GG_LL_FORMAT "d, fails = %" GG_LL_FORMAT
                      "d, decisions = %" GG_LL_FORMAT
                      "d, delayed demon runs = %" GG_LL_FORMAT
                      "d, var demon runs = %" GG_LL_FORMAT
                      "d, normal demon runs = %" GG_LL_FORMAT
                      "d, Run time = %" GG_LL_FORMAT "d ms)",
                branches_, fails_, decisions_, demon_runs_[DELAYED_PRIORITY],
                demon_runs_[VAR_PRIORITY], demon_runs_[NORMAL_PRIORITY],
                wall_time());
  return out;
}

int64 Solver::MemoryUsage() {
  return GetProcessMemoryUsage();
}


int64 Solver::wall_time() const { return timer_->GetInMs(); }

int64 Solver::solutions() const { return TopLevelSearch()->solution_counter(); }

void Solver::TopPeriodicCheck() { TopLevelSearch()->PeriodicCheck(); }

int Solver::TopProgressPercent() { return TopLevelSearch()->ProgressPercent(); }

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
    m->rev_int_index_ = trail_->rev_ints_.size();
    m->rev_int64_index_ = trail_->rev_int64s_.size();
    m->rev_uint64_index_ = trail_->rev_uint64s_.size();
    m->rev_double_index_ = trail_->rev_doubles_.size();
    m->rev_ptr_index_ = trail_->rev_ptrs_.size();
    m->rev_boolvar_list_index_ = trail_->rev_boolvar_list_.size();
    m->rev_bools_index_ = trail_->rev_bools_.size();
    m->rev_int_memory_index_ = trail_->rev_int_memory_.size();
    m->rev_int64_memory_index_ = trail_->rev_int64_memory_.size();
    m->rev_double_memory_index_ = trail_->rev_double_memory_.size();
    m->rev_object_memory_index_ = trail_->rev_object_memory_.size();
    m->rev_object_array_memory_index_ = trail_->rev_object_array_memory_.size();
    m->rev_memory_index_ = trail_->rev_memory_.size();
    m->rev_memory_array_index_ = trail_->rev_memory_array_.size();
  }
  searches_.back()->marker_stack_.push_back(m);
  queue_->increase_stamp();
}

void Solver::AddBacktrackAction(Action* a, bool fast) {
  StateInfo info(a, static_cast<int>(fast));
  PushState(REVERSIBLE_ACTION, info);
}

Solver::MarkerType Solver::PopState(StateInfo* info) {
  CHECK(!searches_.back()->marker_stack_.empty())
      << "PopState() on an empty stack";
  CHECK(info != nullptr);
  StateMarker* m = searches_.back()->marker_stack_.back();
  if (m->type_ != REVERSIBLE_ACTION || m->info_.int_info == 0) {
    trail_->BacktrackTo(m);
  }
  Solver::MarkerType t = m->type_;
  (*info) = m->info_;
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

void Solver::AddFailHook(Action* a) {
  if (fail_hooks_ == nullptr) {
    SaveValue(reinterpret_cast<void**>(&fail_hooks_));
    fail_hooks_ = UnsafeRevAlloc(new SimpleRevFIFO<Action*>);
  }
  fail_hooks_->Push(this, a);
}

void Solver::CallFailHooks() {
  if (fail_hooks_ != nullptr) {
    for (SimpleRevFIFO<Action*>::Iterator it(fail_hooks_); it.ok(); ++it) {
      (*it)->Run(this);
    }
  }
}

void Solver::FreezeQueue() { queue_->Freeze(); }

void Solver::UnfreezeQueue() { queue_->Unfreeze(); }

void Solver::EnqueueVar(Demon* const d) { queue_->EnqueueVar(d); }

void Solver::EnqueueDelayedDemon(Demon* const d) {
  queue_->EnqueueDelayedDemon(d);
}

void Solver::Execute(Demon* const d) { queue_->Execute(d); }

void Solver::ExecuteAll(const SimpleRevFIFO<Demon*>& demons) {
  queue_->ExecuteAll(demons);
}

void Solver::EnqueueAll(const SimpleRevFIFO<Demon*>& demons) {
  queue_->EnqueueAll(demons);
}

uint64 Solver::stamp() const { return queue_->stamp(); }

uint64 Solver::fail_stamp() const { return fail_stamp_; }

void Solver::set_queue_action_on_fail(Action* a) {
  queue_->set_action_on_fail(a);
}

void SetQueueCleanerOnFail(Solver* const solver, IntVar* const var) {
  solver->set_queue_cleaner_on_fail(var);
}

void Solver::clear_queue_action_on_fail() { queue_->clear_action_on_fail(); }

void Solver::AddConstraint(Constraint* const c) {
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
    if (FLAGS_cp_show_constraints) {
      LOG(INFO) << c->DebugString();
    }
    constraints_list_.push_back(c);
  }
}

void Solver::AddCastConstraint(CastConstraint* const constraint,
                               IntVar* const target_var, IntExpr* const expr) {
  if (constraint != nullptr) {
    if (state_ != IN_SEARCH) {
      cast_constraints_.insert(constraint);
      cast_information_[target_var] =
          Solver::IntegerCastInfo(target_var, expr, constraint);
    }
    AddConstraint(constraint);
  }
}

void Solver::Accept(ModelVisitor* const visitor) const {
  std::vector<SearchMonitor*> monitors;
  Accept(visitor, monitors, nullptr);
}

void Solver::Accept(ModelVisitor* const visitor,
                    const std::vector<SearchMonitor*>& monitors) const {
  Accept(visitor, monitors, nullptr);
}

void Solver::Accept(ModelVisitor* const visitor,
                    const std::vector<SearchMonitor*>& monitors,
                    DecisionBuilder* const db) const {
  visitor->BeginVisitModel(name_);
  for (int index = 0; index < constraints_list_.size(); ++index) {
    Constraint* const constraint = constraints_list_[index];
    constraint->Accept(visitor);
  }
  if (state_ == IN_ROOT_NODE) {
    TopLevelSearch()->Accept(visitor);
  } else {
    for (int i = 0; i < monitors.size(); ++i) {
      monitors[i]->Accept(visitor);
    }
  }
  if (db != nullptr) {
    db->Accept(visitor);
  }
  visitor->EndVisitModel(name_);
}

void Solver::ProcessConstraints() {
  // Both constraints_list_ and additional_constraints_list_ are used in
  // a FIFO way.
  if (FLAGS_cp_print_model) {
    ModelVisitor* const visitor = MakePrintModelVisitor();
    Accept(visitor);
  }
  if (FLAGS_cp_model_stats) {
    ModelVisitor* const visitor = MakeStatisticsModelVisitor();
    Accept(visitor);
  }
  if (!FLAGS_cp_export_file.empty()) {
    File* file = File::Open(FLAGS_cp_export_file, "wb");
    if (file == nullptr) {
      LOG(WARNING) << "Cannot open " << FLAGS_cp_export_file;
    } else {
      CPModelProto export_proto;
      ExportModel(&export_proto);
      VLOG(1) << export_proto.DebugString();
      RecordWriter writer(file);
      writer.WriteProtocolMessage(export_proto);
      writer.Close();
    }
  }

  if (FLAGS_cp_no_solve) {
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

bool Solver::Solve(DecisionBuilder* const db, SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return Solve(db, monitors);
}

bool Solver::Solve(DecisionBuilder* const db) {
  std::vector<SearchMonitor*> monitors;
  return Solve(db, monitors);
}

bool Solver::Solve(DecisionBuilder* const db, SearchMonitor* const m1,
                   SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return Solve(db, monitors);
}

bool Solver::Solve(DecisionBuilder* const db, SearchMonitor* const m1,
                   SearchMonitor* const m2, SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return Solve(db, monitors);
}

bool Solver::Solve(DecisionBuilder* const db, SearchMonitor* const m1,
                   SearchMonitor* const m2, SearchMonitor* const m3,
                   SearchMonitor* const m4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  monitors.push_back(m4);
  return Solve(db, monitors);
}

bool Solver::Solve(DecisionBuilder* const db,
                   const std::vector<SearchMonitor*>& monitors) {
  NewSearch(db, monitors);
  searches_.back()->set_created_by_solve(true);  // Overwrites default.
  NextSolution();
  const bool solution_found = searches_.back()->solution_counter() > 0;
  EndSearch();
  return solution_found;
}

void Solver::NewSearch(DecisionBuilder* const db, SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return NewSearch(db, monitors);
}

void Solver::NewSearch(DecisionBuilder* const db) {
  std::vector<SearchMonitor*> monitors;
  return NewSearch(db, monitors);
}

void Solver::NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                       SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return NewSearch(db, monitors);
}

void Solver::NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                       SearchMonitor* const m2, SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return NewSearch(db, monitors);
}

void Solver::NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                       SearchMonitor* const m2, SearchMonitor* const m3,
                       SearchMonitor* const m4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  monitors.push_back(m4);
  return NewSearch(db, monitors);
}

extern PropagationMonitor* BuildPrintTrace(Solver* const s);

// Opens a new top level search.
void Solver::NewSearch(DecisionBuilder* const db,
                       const std::vector<SearchMonitor*>& monitors) {
  // TODO(user) : reset statistics

  const int size = monitors.size();

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

  // Always install the main propagation monitor.
  propagation_monitor_->Install();
  if (demon_profiler_ != nullptr) {
    InstallDemonProfiler(demon_profiler_);
  }

  // Push monitors and enter search.
  for (int i = 0; i < size; ++i) {
    if (monitors[i] != nullptr) {
      monitors[i]->Install();
    }
  }
  std::vector<SearchMonitor*> extras;
  db->AppendMonitors(this, &extras);
  for (ConstIter<std::vector<SearchMonitor*> > it(extras); !it.at_end(); ++it) {
    SearchMonitor* const monitor = *it;
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
    if (FLAGS_cp_trace_propagation) {
      print_trace_ = BuildPrintTrace(this);
      print_trace_->Install();
    } else if (FLAGS_cp_trace_search) {
      // This is useful to trace the exact behavior of the search.
      // The '######## ' prefix is the same as the progagation trace.
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
bool Solver::BacktrackOneLevel(Decision** const fail_decision) {
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
        Action* d = reinterpret_cast<Action*>(info.ptr_info);
        d->Run(this);
        break;
      }
    }
  }
  Search* const search = searches_.back();
  search->EndFail();
  CallFailHooks();
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
        Demon* d = reinterpret_cast<Demon*>(info.ptr_info);
        d->Run(this);
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
    if (m->type_ == REVERSIBLE_ACTION) {
      p->marker_stack_.push_back(m);
    } else {
      if (m->type_ == SENTINEL) {
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
  virtual ~ReverseDecision() {}

  virtual void Apply(Solver* const s) { decision_->Refute(s); }

  virtual void Refute(Solver* const s) { decision_->Apply(s); }

  virtual void Accept(DecisionVisitor* const visitor) const {
    decision_->Accept(visitor);
  }

  virtual std::string DebugString() const {
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
              FALLTHROUGH_INTENDED;
            }
            case NO_CHANGE: {
              decisions_++;
              StateInfo i2(d, 0, search->search_depth(),
                           search->left_search_depth());  // 0 for left branch
              PushState(CHOICE_POINT, i2);
              search->ApplyDecision(d);
              branches_++;
              d->Apply(this);
              search->AfterDecision(d, true);
              search->LeftMove();
              break;
            }
            case KEEP_LEFT: {
              search->ApplyDecision(d);
              d->Apply(this);
              search->AfterDecision(d, true);
              break;
            }
            case KEEP_RIGHT: {
              search->RefuteDecision(d);
              d->Refute(this);
              search->AfterDecision(d, false);
              break;
            }
            case KILL_BOTH: { Fail(); }
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
    // Checks if we want to export the profile info.
    if (!FLAGS_cp_profile_file.empty()) {
      LOG(INFO) << "Exporting profile to " << FLAGS_cp_profile_file;
      ExportProfilingOverview(FLAGS_cp_profile_file);
    }
  } else {  // We clean the nested Search.
    delete search;
    searches_.pop_back();
  }
}

bool Solver::CheckAssignment(Assignment* const solution) {
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
  explicit AddConstraintDecisionBuilder(Constraint* const ct)
      : constraint_(ct) {
    CHECK(ct != nullptr);
  }

  virtual ~AddConstraintDecisionBuilder() {}

  virtual Decision* Next(Solver* const solver) {
    solver->AddConstraint(constraint_);
    return nullptr;
  }

  virtual std::string DebugString() const {
    return StringPrintf("AddConstraintDecisionBuilder(%s)",
                        constraint_->DebugString().c_str());
  }

 private:
  Constraint* const constraint_;
};
}  // namespace

DecisionBuilder* Solver::MakeConstraintAdder(Constraint* const ct) {
  return RevAlloc(new AddConstraintDecisionBuilder(ct));
}

bool Solver::CheckConstraint(Constraint* const ct) {
  return Solve(MakeConstraintAdder(ct));
}

bool Solver::SolveAndCommit(DecisionBuilder* const db,
                            SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return SolveAndCommit(db, monitors);
}

bool Solver::SolveAndCommit(DecisionBuilder* const db) {
  std::vector<SearchMonitor*> monitors;
  return SolveAndCommit(db, monitors);
}

bool Solver::SolveAndCommit(DecisionBuilder* const db, SearchMonitor* const m1,
                            SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return SolveAndCommit(db, monitors);
}

bool Solver::SolveAndCommit(DecisionBuilder* const db, SearchMonitor* const m1,
                            SearchMonitor* const m2, SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return SolveAndCommit(db, monitors);
}

bool Solver::SolveAndCommit(DecisionBuilder* const db,
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
    fail_intercept_->Run();
    return;
  }
  ConstraintSolverFailsHere();
  fails_++;
  searches_.back()->BeginFail();
  searches_.back()->JumpBack();
}

// ----- Cast Expression -----

IntExpr* Solver::CastExpression(const IntVar* const var) const {
  const IntegerCastInfo* const cast_info = FindOrNull(cast_information_, var);
  if (cast_info != nullptr) {
    return cast_info->expression;
  }
  return nullptr;
}

// --- Propagation object names ---

std::string Solver::GetName(const PropagationBaseObject* object) {
  const std::string* name = FindOrNull(propagation_object_names_, object);
  if (name != nullptr) {
    return *name;
  }
  const IntegerCastInfo* const cast_info =
      FindOrNull(cast_information_, object);
  if (cast_info != nullptr && cast_info->expression != nullptr) {
    if (cast_info->expression->HasName()) {
      return StringPrintf("Var<%s>", cast_info->expression->name().c_str());
    } else if (FLAGS_cp_name_cast_variables) {
      return StringPrintf("Var<%s>",
                          cast_info->expression->DebugString().c_str());
    } else {
      const std::string new_name =
          StringPrintf("CastVar<%d>", anonymous_variable_index_++);
      propagation_object_names_[object] = new_name;
      return new_name;
    }
  }
  const std::string base_name = object->BaseName();
  if (FLAGS_cp_name_variables && !base_name.empty()) {
    const std::string new_name =
        StringPrintf("%s_%d", base_name.c_str(), anonymous_variable_index_++);
    propagation_object_names_[object] = new_name;
    return new_name;
  }
  return empty_name_;
}

void Solver::SetName(const PropagationBaseObject* object, const std::string& name) {
  if (parameters_.store_names &&
      GetName(object).compare(name) != 0) {  // in particular if name.empty()
    propagation_object_names_[object] = name;
  }
}

bool Solver::HasName(const PropagationBaseObject* const object) const {
  return ContainsKey(propagation_object_names_,
                     const_cast<PropagationBaseObject*>(object)) ||
         (!object->BaseName().empty() && FLAGS_cp_name_variables);
}

// ------------------ Useful Operators ------------------

std::ostream& operator<<(std::ostream& out, const Solver* const s) {  // NOLINT
  out << s->DebugString();
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const BaseObject* const o) {  // NOLINT
  out << o->DebugString();
  return out;
}

// ---------- PropagationBaseObject ---------

std::string PropagationBaseObject::name() const {
  // TODO(user) : merge with GetName() code to remove a std::string copy.
  return solver_->GetName(this);
}

void PropagationBaseObject::set_name(const std::string& name) {
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

void DecisionBuilder::AppendMonitors(Solver* const solver,
                                     std::vector<SearchMonitor*>* const extras) {}

void DecisionBuilder::Accept(ModelVisitor* const visitor) const {}

// ---------- Decision and DecisionVisitor ----------

void Decision::Accept(DecisionVisitor* const visitor) const {
  visitor->VisitUnknownDecision();
}

void DecisionVisitor::VisitSetVariableValue(IntVar* const var, int64 value) {}
void DecisionVisitor::VisitSplitVariableDomain(IntVar* const var, int64 value,
                                               bool lower) {}
void DecisionVisitor::VisitUnknownDecision() {}
void DecisionVisitor::VisitScheduleOrPostpone(IntervalVar* const var,
                                              int64 est) {}
void DecisionVisitor::VisitScheduleOrExpedite(IntervalVar* const var,
                                              int64 est) {}
void DecisionVisitor::VisitRankFirstInterval(SequenceVar* const sequence,
                                             int index) {}

void DecisionVisitor::VisitRankLastInterval(SequenceVar* const sequence,
                                            int index) {}

// ---------- ModelVisitor ----------

// Tags for constraints, arguments, extensions.

const char ModelVisitor::kAbs[] = "Abs";
const char ModelVisitor::kAbsEqual[] = "AbsEqual";
const char ModelVisitor::kAllDifferent[] = "AllDifferent";
const char ModelVisitor::kAllowedAssignments[] = "AllowedAssignments";
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
const char ModelVisitor::kInverse[] = "Inverse";
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
const char ModelVisitor::kNullIntersect[] = "NullIntersect";
const char ModelVisitor::kOpposite[] = "Opposite";
const char ModelVisitor::kPack[] = "Pack";
const char ModelVisitor::kPathCumul[] = "PathCumul";
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
const char ModelVisitor::kExpressionArgument[] = "expression";
const char ModelVisitor::kFailuresLimitArgument[] = "failures_limit";
const char ModelVisitor::kFinalStatesArgument[] = "final_states";
const char ModelVisitor::kFixedChargeArgument[] = "fixed_charge";
const char ModelVisitor::kIndex2Argument[] = "index2";
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
const char ModelVisitor::kStepArgument[] = "step";
const char ModelVisitor::kTargetArgument[] = "target_variable";
const char ModelVisitor::kTimeLimitArgument[] = "time_limit";
const char ModelVisitor::kTransitsArgument[] = "transits";
const char ModelVisitor::kTuplesArgument[] = "tuples";
const char ModelVisitor::kValueArgument[] = "value";
const char ModelVisitor::kValuesArgument[] = "values";
const char ModelVisitor::kVarsArgument[] = "variables";
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

void ModelVisitor::BeginVisitModel(const std::string& type_name) {}
void ModelVisitor::EndVisitModel(const std::string& type_name) {}

void ModelVisitor::BeginVisitConstraint(const std::string& type_name,
                                        const Constraint* const constraint) {}
void ModelVisitor::EndVisitConstraint(const std::string& type_name,
                                      const Constraint* const constraint) {}

void ModelVisitor::BeginVisitExtension(const std::string& type) {}
void ModelVisitor::EndVisitExtension(const std::string& type) {}

void ModelVisitor::BeginVisitIntegerExpression(const std::string& type_name,
                                               const IntExpr* const expr) {}
void ModelVisitor::EndVisitIntegerExpression(const std::string& type_name,
                                             const IntExpr* const expr) {}

void ModelVisitor::VisitIntegerVariable(const IntVar* const variable,
                                        IntExpr* const delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntegerVariable(const IntVar* const variable,
                                        const std::string& operation, int64 value,
                                        IntVar* const delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntervalVariable(const IntervalVar* const variable,
                                         const std::string& operation, int64 value,
                                         IntervalVar* const delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitSequenceVariable(const SequenceVar* const variable) {
  for (int i = 0; i < variable->size(); ++i) {
    variable->Interval(i)->Accept(this);
  }
}

void ModelVisitor::VisitIntegerArgument(const std::string& arg_name, int64 value) {}

void ModelVisitor::VisitIntegerArrayArgument(const std::string& arg_name,
                                             const std::vector<int64>& values) {}

void ModelVisitor::VisitIntegerMatrixArgument(const std::string& arg_name,
                                              const IntTupleSet& tuples) {}

void ModelVisitor::VisitIntegerExpressionArgument(const std::string& arg_name,
                                                  IntExpr* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntegerVariableArrayArgument(
    const std::string& arg_name, const std::vector<IntVar*>& arguments) {
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelVisitor::VisitIntervalArgument(const std::string& arg_name,
                                         IntervalVar* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntervalArrayArgument(
    const std::string& arg_name, const std::vector<IntervalVar*>& arguments) {
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelVisitor::VisitSequenceArgument(const std::string& arg_name,
                                         SequenceVar* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitSequenceArrayArgument(
    const std::string& arg_name, const std::vector<SequenceVar*>& arguments) {
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

// ----- Helpers -----

void ModelVisitor::VisitInt64ToBoolExtension(
    ResultCallback1<bool, int64>* const callback, int64 index_min,
    int64 index_max) {
  if (callback == nullptr) {
    return;
  }
  std::vector<int64> cached_results;
  for (int i = index_min; i <= index_max; ++i) {
    cached_results.push_back(callback->Run(i));
  }
  BeginVisitExtension(kInt64ToBoolExtension);
  VisitIntegerArgument(kMinArgument, index_min);
  VisitIntegerArgument(kMaxArgument, index_max);
  VisitIntegerArrayArgument(kValuesArgument, cached_results);
  EndVisitExtension(kInt64ToBoolExtension);
}

void ModelVisitor::VisitInt64ToInt64Extension(
    Solver::IndexEvaluator1* const callback, int64 index_min, int64 index_max) {
  if (callback == nullptr) {
    return;
  }
  std::vector<int64> cached_results;
  for (int i = index_min; i <= index_max; ++i) {
    cached_results.push_back(callback->Run(i));
  }
  BeginVisitExtension(kInt64ToInt64Extension);
  VisitIntegerArgument(kMinArgument, index_min);
  VisitIntegerArgument(kMaxArgument, index_max);
  VisitIntegerArrayArgument(kValuesArgument, cached_results);
  EndVisitExtension(kInt64ToInt64Extension);
}

void ModelVisitor::VisitInt64ToInt64AsArray(
    Solver::IndexEvaluator1* const callback, const std::string& arg_name,
    int64 index_max) {
  if (callback == nullptr) {
    return;
  }
  std::vector<int64> cached_results;
  for (int i = 0; i <= index_max; ++i) {
    cached_results.push_back(callback->Run(i));
  }
  VisitIntegerArrayArgument(arg_name, cached_results);
}

// ---------- Search Monitor ----------

void SearchMonitor::EnterSearch() {}
void SearchMonitor::RestartSearch() {}
void SearchMonitor::ExitSearch() {}
void SearchMonitor::BeginNextDecision(DecisionBuilder* const b) {}
void SearchMonitor::EndNextDecision(DecisionBuilder* const b,
                                    Decision* const d) {}
void SearchMonitor::ApplyDecision(Decision* const d) {}
void SearchMonitor::RefuteDecision(Decision* const d) {}
void SearchMonitor::AfterDecision(Decision* const d, bool apply) {}
void SearchMonitor::BeginFail() {}
void SearchMonitor::EndFail() {}
void SearchMonitor::BeginInitialPropagation() {}
void SearchMonitor::EndInitialPropagation() {}
bool SearchMonitor::AcceptSolution() { return true; }
bool SearchMonitor::AtSolution() { return false; }
void SearchMonitor::NoMoreSolutions() {}
bool SearchMonitor::LocalOptimum() { return false; }
bool SearchMonitor::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  return true;
}
void SearchMonitor::AcceptNeighbor() {}
void SearchMonitor::FinishCurrentSearch() {
  solver()->searches_.back()->set_should_finish(true);
}
void SearchMonitor::RestartCurrentSearch() {
  solver()->searches_.back()->set_should_restart(true);
}
void SearchMonitor::PeriodicCheck() {}
void SearchMonitor::Accept(ModelVisitor* const visitor) const {}
// A search monitors adds itself on the active search.
void SearchMonitor::Install() {
  solver()->searches_.back()->push_monitor(this);
}

// ---------- Propagation Monitor -----------
PropagationMonitor::PropagationMonitor(Solver* const s) : SearchMonitor(s) {}

PropagationMonitor::~PropagationMonitor() {}

// A propagation monitor listens to search events as well as
// propagation events.
void PropagationMonitor::Install() {
  SearchMonitor::Install();
  solver()->AddPropagationMonitor(this);
}

// ---------- Trace ----------

class Trace : public PropagationMonitor {
 public:
  explicit Trace(Solver* const s) : PropagationMonitor(s) {}

  virtual ~Trace() {}

  virtual void BeginConstraintInitialPropagation(Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginConstraintInitialPropagation(constraint);
    }
  }

  virtual void EndConstraintInitialPropagation(Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndConstraintInitialPropagation(constraint);
    }
  }

  virtual void BeginNestedConstraintInitialPropagation(
      Constraint* const parent, Constraint* const nested) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginNestedConstraintInitialPropagation(parent, nested);
    }
  }

  virtual void EndNestedConstraintInitialPropagation(Constraint* const parent,
                                                     Constraint* const nested) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndNestedConstraintInitialPropagation(parent, nested);
    }
  }

  virtual void RegisterDemon(Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RegisterDemon(demon);
    }
  }

  virtual void BeginDemonRun(Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginDemonRun(demon);
    }
  }

  virtual void EndDemonRun(Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndDemonRun(demon);
    }
  }

  virtual void StartProcessingIntegerVariable(IntVar* const var) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->StartProcessingIntegerVariable(var);
    }
  }

  virtual void EndProcessingIntegerVariable(IntVar* const var) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndProcessingIntegerVariable(var);
    }
  }

  virtual void PushContext(const std::string& context) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->PushContext(context);
    }
  }

  virtual void PopContext() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->PopContext();
    }
  }

  // IntExpr modifiers.
  virtual void SetMin(IntExpr* const expr, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMin(expr, new_min);
    }
  }

  virtual void SetMax(IntExpr* const expr, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMax(expr, new_max);
    }
  }

  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetRange(expr, new_min, new_max);
    }
  }

  // IntVar modifiers.
  virtual void SetMin(IntVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMin(var, new_min);
    }
  }

  virtual void SetMax(IntVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMax(var, new_max);
    }
  }

  virtual void SetRange(IntVar* const var, int64 new_min, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetRange(var, new_min, new_max);
    }
  }

  virtual void RemoveValue(IntVar* const var, int64 value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveValue(var, value);
    }
  }

  virtual void SetValue(IntVar* const var, int64 value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetValue(var, value);
    }
  }

  virtual void RemoveInterval(IntVar* const var, int64 imin, int64 imax) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveInterval(var, imin, imax);
    }
  }

  virtual void SetValues(IntVar* const var, const std::vector<int64>& values) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetValues(var, values);
    }
  }

  virtual void RemoveValues(IntVar* const var, const std::vector<int64>& values) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveValues(var, values);
    }
  }

  // IntervalVar modifiers.
  virtual void SetStartMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartMin(var, new_min);
    }
  }

  virtual void SetStartMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartMax(var, new_max);
    }
  }

  virtual void SetStartRange(IntervalVar* const var, int64 new_min,
                             int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartRange(var, new_min, new_max);
    }
  }

  virtual void SetEndMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndMin(var, new_min);
    }
  }

  virtual void SetEndMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndMax(var, new_max);
    }
  }

  virtual void SetEndRange(IntervalVar* const var, int64 new_min,
                           int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndRange(var, new_min, new_max);
    }
  }

  virtual void SetDurationMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationMin(var, new_min);
    }
  }

  virtual void SetDurationMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationMax(var, new_max);
    }
  }

  virtual void SetDurationRange(IntervalVar* const var, int64 new_min,
                                int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationRange(var, new_min, new_max);
    }
  }

  virtual void SetPerformed(IntervalVar* const var, bool value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetPerformed(var, value);
    }
  }

  virtual void RankFirst(SequenceVar* const var, int index) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RankFirst(var, index);
    }
  }

  virtual void RankNotFirst(SequenceVar* const var, int index) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RankNotFirst(var, index);
    }
  }

  virtual void RankLast(SequenceVar* const var, int index) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RankLast(var, index);
    }
  }

  virtual void RankNotLast(SequenceVar* const var, int index) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RankNotLast(var, index);
    }
  }

  virtual void RankSequence(SequenceVar* const var,
                            const std::vector<int>& rank_first,
                            const std::vector<int>& rank_last,
                            const std::vector<int>& unperformed) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RankSequence(var, rank_first, rank_last, unperformed);
    }
  }

  // Does not take ownership of monitor.
  void Add(PropagationMonitor* const monitor) {
    if (monitor != nullptr) {
      monitors_.push_back(monitor);
    }
  }

  // The trace will dispatch propagation events. It needs to listen to search
  // events.
  virtual void Install() { SearchMonitor::Install(); }

  virtual std::string DebugString() const { return "Trace"; }

 private:
  std::vector<PropagationMonitor*> monitors_;
};

PropagationMonitor* BuildTrace(Solver* const s) { return new Trace(s); }

void Solver::AddPropagationMonitor(PropagationMonitor* const monitor) {
  // TODO(user): Check solver state?
  reinterpret_cast<class Trace*>(propagation_monitor_.get())->Add(monitor);
}

PropagationMonitor* Solver::GetPropagationMonitor() const {
  return propagation_monitor_.get();
}

// ----------------- Constraint class -------------------

std::string Constraint::DebugString() const { return "Constraint"; }

void Constraint::PostAndPropagate() {
  FreezeQueue();
  Post();
  InitialPropagate();
  UnfreezeQueue();
}

void Constraint::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint("unknown", this);
  VLOG(3) << "Unknown constraint " << DebugString();
  visitor->EndVisitConstraint("unknown", this);
}

bool Constraint::IsCastConstraint() const {
  return ContainsKey(solver()->cast_constraints_, this);
}

IntVar* Constraint::Var() { return nullptr; }

// ----- Class IntExpr -----

void IntExpr::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitIntegerExpression("unknown", this);
  VLOG(3) << "Unknown expression " << DebugString();
  visitor->EndVisitIntegerExpression("unknown", this);
}

#undef CP_TRY  // We no longer need those.
#undef CP_ON_FAIL
#undef CP_DO_FAIL

}  // namespace operations_research
