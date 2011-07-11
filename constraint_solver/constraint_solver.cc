// Copyright 2010-2011 Google
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

#include <setjmp.h>

#include <deque>
#include <sstream>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "zlib.h"
#include "base/stringpiece.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/const_int_array.h"

DEFINE_bool(cp_trace_demons, false, "trace all demon executions.");
DEFINE_bool(cp_show_constraints, false,
            "show all constraints added to the solver.");
DEFINE_bool(cp_visit_model, false,
            "use PrintModelVisitor on model before solving.");

void ConstraintSolverFailHere() {
  VLOG(3) << "Fail";
}

#if defined(_MSC_VER)  // WINDOWS
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ----- Forward Declarations -----
extern DemonMonitor* BuildDemonMonitor(SolverParameters::ProfileLevel level);
extern void DeleteDemonMonitor(DemonMonitor* const monitor);
extern void DemonMonitorStartInitialPropagation(
    DemonMonitor* const monitor, const Constraint* const constraint);
extern void DemonMonitorEndInitialPropagation(
    DemonMonitor* const monitor, const Constraint* const constraint);
extern void DemonMonitorRestartSearch(DemonMonitor* const monitor);


// ------------------ Demon class ----------------

Solver::DemonPriority Demon::priority() const {
  return Solver::NORMAL_PRIORITY;
}

string Demon::DebugString() const {
  return "Demon";
}

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

string Action::DebugString() const {
  return "Action";
}

// ------------------ Queue class ------------------

class SinglePriorityQueue {
 public:
  virtual ~SinglePriorityQueue() {}
  virtual Demon* NextDemon() = 0;
  virtual void Enqueue(Demon* const d) = 0;
  virtual void AfterFailure() = 0;
  virtual void Init() = 0;
  virtual bool Empty() const = 0;
};

class FifoPriorityQueue : public SinglePriorityQueue {
 public:
  struct Cell {
    explicit Cell(Demon* const d) : demon(d), next(NULL) {}
    Demon* demon;
    Cell* next;
  };

  FifoPriorityQueue() : first_(NULL), last_(NULL), free_cells_(NULL) {}

  virtual ~FifoPriorityQueue() {
    while (first_ != NULL) {
      Cell* const tmp = first_;
      first_ = tmp->next;
      delete tmp;
    }
    while (free_cells_ != NULL) {
      Cell* const tmp = free_cells_;
      free_cells_ = tmp->next;
      delete tmp;
    }
  }

  virtual bool Empty() const {
    return first_ == NULL;
  }

  virtual Demon* NextDemon() {
    if (first_ != NULL) {
      DCHECK(last_ != NULL);
      Cell* const tmp_cell = first_;
      Demon* const demon = tmp_cell->demon;
      first_ = tmp_cell->next;
      if (first_ == NULL) {
        last_ = NULL;
      }
      tmp_cell->next = free_cells_;
      free_cells_ = tmp_cell;
      return demon;
    }
    return NULL;
  }

  virtual void Enqueue(Demon* const d) {
    Cell* cell = free_cells_;
    if (cell != NULL) {
      cell->demon = d;
      free_cells_ = cell->next;
      cell->next = NULL;
    } else {
      cell = new Cell(d);
    }
    if (last_ != NULL) {
      last_->next = cell;
      last_ = cell;
    } else {
      first_ = cell;
      last_ = cell;
    }
  }

  virtual void AfterFailure() {
    if (first_ != NULL) {
      last_->next = free_cells_;
      free_cells_ = first_;
      first_ = NULL;
      last_ = NULL;
    }
  }

  virtual void Init() {}

 private:
  Cell* first_;
  Cell* last_;
  Cell* free_cells_;
};

class Queue {
 public:
  explicit Queue(Solver* const s)
      : solver_(s),
        stamp_(1),
        freeze_level_(0),
        in_process_(false),
        clear_action_(NULL),
        in_add_(false) {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      containers_[i] = new FifoPriorityQueue();
      containers_[i]->Init();
    }
  }

  ~Queue() {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      delete containers_[i];
      containers_[i] = NULL;
    }
  }

  void Freeze() {
    freeze_level_++;
    stamp_++;
  }

  void Unfreeze() {
    freeze_level_--;
    ProcessIfUnfrozen();
  }

  void ProcessOneDemon(Solver::DemonPriority prio) {
    Demon* const demon = containers_[prio]->NextDemon();
    // A NULL demon will just be ignored
    if (demon != NULL) {
      if (FLAGS_cp_trace_demons) {
        LG << "### Running demon (" << prio << "):"
           << demon->DebugString() << " ###";
      }
      demon->set_stamp(stamp_ - 1);
      DCHECK_EQ(prio, demon->priority());
      solver_->demon_runs_[prio]++;
      demon->Run(solver_);
    }
  }

  void ProcessDemons() {
    while (!containers_[Solver::NORMAL_PRIORITY]->Empty()) {
      ProcessOneDemon(Solver::NORMAL_PRIORITY);
    }
  }

  void Process() {
    if (!in_process_) {
      in_process_ = true;
      while (!containers_[Solver::VAR_PRIORITY]->Empty() ||
             !containers_[Solver::NORMAL_PRIORITY]->Empty() ||
             !containers_[Solver::DELAYED_PRIORITY]->Empty()) {
        while (!containers_[Solver::VAR_PRIORITY]->Empty() ||
               !containers_[Solver::NORMAL_PRIORITY]->Empty()) {
          while (!containers_[Solver::NORMAL_PRIORITY]->Empty()) {
            ProcessOneDemon(Solver::NORMAL_PRIORITY);
          }
          ProcessOneDemon(Solver::VAR_PRIORITY);
        }
        ProcessOneDemon(Solver::DELAYED_PRIORITY);
      }
      in_process_ = false;
    }
  }

  void Enqueue(Demon* const demon) {
    if (demon->stamp() < stamp_) {
      demon->set_stamp(stamp_);
      containers_[demon->priority()]->Enqueue(demon);
      ProcessIfUnfrozen();
    }
  }

  void AfterFailure() {
    for (int i = 0; i < Solver::kNumPriorities; ++i) {
      containers_[i]->AfterFailure();
    }
    if (clear_action_ != NULL) {
      clear_action_->Run(solver_);
      clear_action_ = NULL;
    }
    freeze_level_ = 0;
    in_process_ = false;
    in_add_ = false;
    to_add_.clear();
  }

  void increase_stamp() {
    stamp_++;
  }

  uint64 stamp() const {
    return stamp_;
  }

  void set_action_on_fail(Action* const a) {
    clear_action_ = a;
  }

  void clear_action_on_fail() {
    clear_action_ = NULL;
  }

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
  void ProcessIfUnfrozen() {
    if (freeze_level_ == 0) {
      Process();
    }
  }

  Solver* const solver_;
  SinglePriorityQueue* containers_[Solver::kNumPriorities];
  uint64 stamp_;
  // The number of nested freeze levels. The queue is frozen if and only if
  // freeze_level_ > 0.
  uint32 freeze_level_;
  bool in_process_;
  Action* clear_action_;
  std::vector<Constraint*> to_add_;
  bool in_add_;
};

// ------------------ StateMarker / StateInfo struct -----------

struct StateInfo {  // This is an internal structure to store
                    // additional information on the choice point.
 public:
  StateInfo() : ptr_info(NULL), int_info(0), depth(0), left_depth(0) {}
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
  StateMarker(MarkerType t, const StateInfo& info);
  friend class Solver;
  friend struct Trail;
 private:
  MarkerType type_;
  int rev_int_index_;
  int rev_int64_index_;
  int rev_uint64_index_;
  int rev_ptr_index_;
  int rev_boolvar_list_index_;
  int rev_bools_index_;
  int rev_int_memory_index_;
  int rev_int64_memory_index_;
  int rev_object_memory_index_;
  int rev_object_array_memory_index_;
  int rev_memory_index_;
  int rev_memory_array_index_;
  StateInfo info_;
};

StateMarker::StateMarker(MarkerType t, const StateInfo& info)
    : type_(t),
      rev_int_index_(0),
      rev_int64_index_(0),
      rev_uint64_index_(0),
      rev_ptr_index_(0),
      rev_boolvar_list_index_(0),
      rev_bools_index_(0),
      rev_int_memory_index_(0),
      rev_int64_memory_index_(0),
      rev_object_memory_index_(0),
      rev_object_array_memory_index_(0),
      info_(info) {}

// ---------- Trail and Reversibility ----------

// ----- addrval struct -----

// This template class is used internally to implement reversibility.
// It stores an address and the value that was at the address.

template <class T> struct addrval {
 public:
  addrval() : address_(NULL) {}
  explicit addrval(T* adr) : address_(adr), old_value_(*adr) {}
  void restore() const { (*address_) = old_value_; }
 private:
  T* address_;
  T old_value_;
};

// ----- Compressed trail -----

// ---------- Trail Packer ---------
// Abstract class to pack trail blocks.

template <class T> class TrailPacker {
 public:
  explicit TrailPacker(int block_size) : block_size_(block_size) {}
  virtual ~TrailPacker() {}
  int input_size() const { return block_size_ * sizeof(addrval<T>); }
  virtual void Pack(const addrval<T>* block, string* packed_block) = 0;
  virtual void Unpack(const string& packed_block, addrval<T>* block) = 0;
 private:
  const int block_size_;
  DISALLOW_COPY_AND_ASSIGN(TrailPacker);
};


template <class T> class NoCompressionTrailPacker : public TrailPacker<T> {
 public:
  explicit NoCompressionTrailPacker(int block_size)
      : TrailPacker<T>(block_size) {}
  virtual ~NoCompressionTrailPacker() {}
  virtual void Pack(const addrval<T>* block, string* packed_block) {
    DCHECK(block != NULL);
    DCHECK(packed_block != NULL);
    StringPiece block_str;
    block_str.set(block, this->input_size());
    block_str.CopyToString(packed_block);
  }
  virtual void Unpack(const string& packed_block, addrval<T>* block) {
    DCHECK(block != NULL);
    memcpy(block, packed_block.c_str(), packed_block.size());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(NoCompressionTrailPacker<T>);
};

template <class T> class ZlibTrailPacker : public TrailPacker<T> {
 public:
  explicit ZlibTrailPacker(int block_size)
      : TrailPacker<T>(block_size),
        tmp_size_(compressBound(this->input_size())),
        tmp_block_(new char[tmp_size_]) {}

  virtual ~ZlibTrailPacker() {}

  virtual void Pack(const addrval<T>* block, string* packed_block) {
    DCHECK(block != NULL);
    DCHECK(packed_block != NULL);
    uLongf size = tmp_size_;
    const int result = compress(reinterpret_cast<Bytef*>(tmp_block_.get()),
                                &size,
                                reinterpret_cast<const Bytef*>(block),
                                this->input_size());
    CHECK_EQ(Z_OK, result);
    StringPiece block_str;
    block_str.set(tmp_block_.get(), size);
    block_str.CopyToString(packed_block);
  }

  virtual void Unpack(const string& packed_block, addrval<T>* block) {
    DCHECK(block != NULL);
    uLongf size = this->input_size();
    const int result =
        uncompress(reinterpret_cast<Bytef*>(block),
                   &size,
                   reinterpret_cast<const Bytef*>(packed_block.c_str()),
                   packed_block.size());
    CHECK_EQ(Z_OK, result);
  }

 private:
  const uint64 tmp_size_;
  scoped_array<char> tmp_block_;
  DISALLOW_COPY_AND_ASSIGN(ZlibTrailPacker<T>);
};


template <class T> class CompressedTrail {
 public:
  CompressedTrail(int block_size,
                  SolverParameters::TrailCompression compression_level)
      : block_size_(block_size),
        blocks_(NULL),
        free_blocks_(NULL),
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
        } else if (blocks_ != NULL) {
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
  int size() const { return size_; }

 private:
  struct Block {
    string compressed;
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
    Block* block = NULL;
    if (free_blocks_ != NULL) {
      block = free_blocks_;
      free_blocks_ = block->next;
    } else {
      block = new Block;
    }
    block->next = blocks_;
    blocks_ = block;
  }
  void FreeBlocks(Block* blocks) {
    while (NULL != blocks) {
      Block* next = blocks->next;
      delete blocks;
      blocks = next;
    }
  }

  scoped_ptr<TrailPacker<T> > packer_;
  const int block_size_;
  Block* blocks_;
  Block* free_blocks_;
  scoped_array<addrval<T> > data_;
  scoped_array<addrval<T> > buffer_;
  bool buffer_used_;
  int current_;
  int size_;
};

// ----- Trail -----

// Object are explicitely copied using the copy ctor instead of
// passing and storing a pointer. As objects are small, copying is
// much faster than allocating (around 35% on a complete solve).

extern void RestoreBoolValue(BooleanVar* const var);

struct Trail {
  CompressedTrail<int> rev_ints_;
  CompressedTrail<int64> rev_int64s_;
  CompressedTrail<uint64> rev_uint64s_;
  CompressedTrail<void*> rev_ptrs_;
  std::vector<BooleanVar*> rev_boolvar_list_;
  std::vector<bool*> rev_bools_;
  std::vector<bool> rev_bool_value_;
  std::vector<int*> rev_int_memory_;
  std::vector<int64*> rev_int64_memory_;
  std::vector<BaseObject*> rev_object_memory_;
  std::vector<BaseObject**> rev_object_array_memory_;
  std::vector<void*> rev_memory_;
  std::vector<void**> rev_memory_array_;

  Trail(int block_size, SolverParameters::TrailCompression compression_level)
      : rev_ints_(block_size, compression_level),
        rev_int64s_(block_size, compression_level),
        rev_uint64s_(block_size, compression_level),
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
      BooleanVar* const var = rev_boolvar_list_[curr];
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

    target = m->rev_object_memory_index_;
    for (int curr = rev_object_memory_.size() - 1; curr >= target; --curr) {
      delete rev_object_memory_[curr];
    }
    rev_object_memory_.resize(target);

    target = m->rev_object_array_memory_index_;
    for (int curr = rev_object_array_memory_.size() - 1;
         curr >= target; --curr) {
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
      delete [] rev_memory_array_[curr];
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

int* Solver::SafeRevAlloc(int* ptr) {
  check_alloc_state();
  trail_->rev_int_memory_.push_back(ptr);
  return ptr;
}

int64* Solver::SafeRevAlloc(int64* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory_.push_back(ptr);
  return ptr;
}

uint64* Solver::SafeRevAlloc(uint64* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory_.push_back(reinterpret_cast<int64*>(ptr));
  return ptr;
}

BaseObject* Solver::SafeRevAlloc(BaseObject* ptr) {
  check_alloc_state();
  trail_->rev_object_memory_.push_back(ptr);
  return ptr;
}

BaseObject** Solver::SafeRevAlloc(BaseObject** ptr) {
  check_alloc_state();
  trail_->rev_object_array_memory_.push_back(ptr);
  return ptr;
}

IntVar** Solver::SafeRevAlloc(IntVar** ptr) {
  BaseObject** in = SafeRevAlloc(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntVar**>(in);
}

IntExpr** Solver::SafeRevAlloc(IntExpr** ptr) {
  BaseObject** in = SafeRevAlloc(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntExpr**>(in);
}

Constraint** Solver::SafeRevAlloc(Constraint** ptr) {
  BaseObject** in = SafeRevAlloc(reinterpret_cast<BaseObject**>(ptr));
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

void Solver::InternalSaveBooleanVarValue(BooleanVar* const var) {
  trail_->rev_boolvar_list_.push_back(var);
}

// ------------------ Search class -----------------

class Search {
 public:
  explicit Search(Solver* const s)
      : solver_(s), marker_stack_(), fail_buffer_(), solution_counter_(0),
        decision_builder_(NULL), created_by_solve_(false),
        selector_(NULL), search_depth_(0), left_search_depth_(0),
        should_restart_(false), should_finish_(false),
        sentinel_pushed_(0), jmpbuf_filled_(false) {}

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
  void RightMove() {
    search_depth_++;
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
  scoped_ptr<ResultCallback1<Solver::DecisionModification, Solver*> > selector_;
  int search_depth_;
  int left_search_depth_;
  bool should_restart_;
  bool should_finish_;
  int sentinel_pushed_;
  bool jmpbuf_filled_;
};

// Backtrack is implemented using 3 primitives:
// CP_TRY to start searching
// CP_DO_FAIL to signal a failure. The program will continue on the CP_ON_FAIL
// primitive.
// CP_FAST_BACKTRACK protects an implementation of backtrack using
// setjmp/longjmp.  The clean portable way is to use exceptions,
// unfortunately, it can be much slower.  Thus we use ideas from
// Prolog, CP/CLP implementations, continuations in C and implement failing
// and backtracking using setjmp/longjmp.
#define CP_FAST_BACKTRACK
#if defined(CP_FAST_BACKTRACK)
// We cannot use a method/function for this as we would lose the
// context in the setjmp implementation.
#define CP_TRY(search)                                              \
  CHECK(!search->jmpbuf_filled_) << "Fail() called outside search"; \
  search->jmpbuf_filled_ = true;                                    \
  if (setjmp(search->fail_buffer_) == 0)
#define CP_ON_FAIL else
#define CP_DO_FAIL(search) longjmp(search->fail_buffer_, 1)
#else  // CP_FAST_BACKTRACK
class FailException {};
#define CP_TRY(search)                                                 \
  CHECK(!search->jmpbuf_filled_) << "Fail() called outside search";    \
  search->jmpbuf_filled_ = true;                                       \
  try
#define CP_ON_FAIL catch(FailException&)
#define CP_DO_FAIL(search) throw FailException()
#endif  // CP_FAST_BACKTRACK

void Search::JumpBack() {
  ClearBuffer();
  CP_DO_FAIL(this);
}

void Search::SetBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  CHECK(bs == selector_ || selector_ == NULL || bs == NULL);
  if (selector_ != bs) {
    selector_.reset(bs);
  }
}

class UndoBranchSelector : public Action {
 public:
  explicit UndoBranchSelector(int depth) : depth_(depth) {}
  virtual ~UndoBranchSelector() {}
  virtual void Run(Solver* const s) {
    if (s->searches_.size() == depth_) {
      s->searches_.back()->SetBranchSelector(NULL);
    }
  }
  virtual string DebugString() const {
    return StringPrintf("UndoBranchSelector(%i)", depth_);
  }
 private:
  const int depth_;
};

void Solver::SetBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  bs->CheckIsRepeatable();
  if (searches_.size() > 0 && searches_.back() != NULL) {
    // We cannot use the trail as the search can be nested and thus
    // deleted upon backtrack. Thus we guard the undo action by a
    // check on the number of nesting of solve().
    AddBacktrackAction(RevAlloc(new UndoBranchSelector(searches_.size())),
                       false);
    searches_.back()->SetBranchSelector(bs);
  }
}

class ApplyBranchSelector : public DecisionBuilder {
 public:
  explicit ApplyBranchSelector(
      ResultCallback1<Solver::DecisionModification, Solver*>* const bs)
      : selector_(bs) {}
  virtual ~ApplyBranchSelector() {}

  virtual Decision* Next(Solver* const s) {
    s->SetBranchSelector(selector_);
    return NULL;
  }

  virtual string DebugString() const {
    return "Apply(BranchSelector)";
  }
 private:
  ResultCallback1<Solver::DecisionModification, Solver*>* const selector_;
};

DecisionBuilder* Solver::MakeApplyBranchSelector(
    ResultCallback1<Solver::DecisionModification, Solver*>* const bs) {
  return RevAlloc(new ApplyBranchSelector(bs));
}

int Solver::SolveDepth() const {
  switch (state_) {
    case IN_SEARCH:
      return searches_.size();
    default:
      return 0;
  }
}

int Solver::SearchDepth() const {
  if (searches_.size() > 0 && searches_.back() != NULL) {
    return searches_.back()->search_depth();
  }
  return -1;
}

int Solver::SearchLeftDepth() const {
  if (searches_.size() > 0 && searches_.back() != NULL) {
    return searches_.back()->left_search_depth();
  }
  return -1;
}

Solver::DecisionModification Search::ModifyDecision() {
  if (selector_ != NULL) {
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
}

void Search::EnterSearch() {
  // The solution counter is reset when entering search and not when
  // leaving search. This enables the information to persist outside of
  // top-level search.
  solution_counter_ = 0;

  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->EnterSearch();
  }
}

void Search::ExitSearch() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->ExitSearch();
  }
}

void Search::RestartSearch() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->RestartSearch();
  }
}

void Search::BeginNextDecision(DecisionBuilder* const db) {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->BeginNextDecision(db);
  }
  CheckFail();
}

void Search::EndNextDecision(DecisionBuilder* const db, Decision* const d) {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->EndNextDecision(db, d);
  }
  CheckFail();
}

void Search::ApplyDecision(Decision* const d) {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->ApplyDecision(d);
  }
  CheckFail();
}

void Search::AfterDecision(Decision* const d, bool apply) {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->AfterDecision(d, apply);
  }
  CheckFail();
}

void Search::RefuteDecision(Decision* const d) {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->RefuteDecision(d);
  }
  CheckFail();
}

void Search::BeginFail() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->BeginFail();
  }
}

void Search::EndFail() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->EndFail();
  }
}

void Search::BeginInitialPropagation() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->BeginInitialPropagation();
  }
}

void Search::EndInitialPropagation() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->EndInitialPropagation();
  }
}

bool Search::AcceptSolution() {
  bool valid = true;
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    if (!(*it)->AcceptSolution()) {
      valid = false;
    }
  }
  return valid;
}

bool Search::AtSolution() {
  bool should_continue = false;
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    if ((*it)->AtSolution()) {
      should_continue = true;
    }
  }
  return should_continue;
}

void Search::NoMoreSolutions() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->NoMoreSolutions();
  }
}

bool Search::LocalOptimum() {
  bool res = false;
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    if ((*it)->LocalOptimum()) {
      res = true;
    }
  }
  return res;
}

bool Search::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  bool accept = true;
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    if (!(*it)->AcceptDelta(delta, deltadelta)) {
      accept = false;
    }
  }
  return accept;
}

void Search::AcceptNeighbor() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->AcceptNeighbor();
  }
}

void Search::PeriodicCheck() {
  for (std::vector<SearchMonitor*>::iterator it = monitors_.begin();
       it != monitors_.end();
       ++it) {
    (*it)->PeriodicCheck();
  }
}

// ---------- Fail Decision ----------

class FailDecision : public Decision {
 public:
  virtual void Apply(Solver* const s) {
    s->Fail();
  }
  virtual void Refute(Solver* const s) {
    s->Fail();
  }
};

Decision* Solver::MakeFailDecision() {
  return fail_decision_.get();
}

// Balancing decision

namespace {
class BalancingDecision : public Decision {
 public:
  virtual ~BalancingDecision() {}
  virtual void Apply(Solver* const s) {}
  virtual void Refute(Solver* const s) {}
};
}


// ------------------ Solver class -----------------

// These magic numbers are there to make sure we pop the correct
// sentinels throughout the search.
namespace {
enum SentinelMarker {
  INITIAL_SEARCH_SENTINEL = 10000000,
  ROOT_NODE_SENTINEL      = 20000000,
  SOLVER_CTOR_SENTINEL    = 40000000
};
}

Solver::Solver(const string& name, const SolverParameters& parameters)
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
      variable_cleaner_(new VariableQueueCleaner()),
      timer_(new ClockTimer),
      searches_(),
      random_(ACMRandom::DeterministicSeed()),
      fail_hooks_(NULL),
      fail_stamp_(GG_ULONGLONG(1)),
      balancing_decision_(new BalancingDecision),
      fail_intercept_(NULL),
      demon_monitor_(BuildDemonMonitor(parameters.profile_level)),
      true_constraint_(NULL),
      false_constraint_(NULL),
      equality_var_cst_cache_(NULL),
      unequality_var_cst_cache_(NULL),
      greater_equal_var_cst_cache_(NULL),
      less_equal_var_cst_cache_(NULL),
      fail_decision_(new FailDecision()),
      constraints_(0) {
  Init();
}

Solver::Solver(const string& name)
    : name_(name),
      parameters_(),
      queue_(new Queue(this)),
      trail_(new Trail(parameters_.trail_block_size,
                       parameters_.compress_trail)),
      state_(OUTSIDE_SEARCH),
      branches_(0),
      fails_(0),
      decisions_(0),
      neighbors_(0),
      filtered_neighbors_(0),
      accepted_neighbors_(0),
      variable_cleaner_(new VariableQueueCleaner()),
      timer_(new ClockTimer),
      searches_(),
      random_(ACMRandom::DeterministicSeed()),
      fail_hooks_(NULL),
      fail_stamp_(GG_ULONGLONG(1)),
      balancing_decision_(new BalancingDecision),
      fail_intercept_(NULL),
      demon_monitor_(BuildDemonMonitor(parameters_.profile_level)),
      true_constraint_(NULL),
      false_constraint_(NULL),
      equality_var_cst_cache_(NULL),
      unequality_var_cst_cache_(NULL),
      greater_equal_var_cst_cache_(NULL),
      less_equal_var_cst_cache_(NULL),
      fail_decision_(new FailDecision()),
      constraints_(0) {
  Init();
}

void Solver::Init() {
  for (int i = 0; i < kNumPriorities; ++i) {
    demon_runs_[i] = 0;
  }
  searches_.push_back(new Search(this));
  PushSentinel(SOLVER_CTOR_SENTINEL);
  InitCachedIntConstants();  // to be called after the SENTINEL is set.
  InitCachedConstraint();  // Cache the true constraint.
  InitBoolVarCaches();
  timer_->Restart();
}

Solver::~Solver() {
  // solver destructor called with searches open.
  CHECK_EQ(searches_.size(), 1);
  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);

  StateInfo info;
  MarkerType finalType = PopState(&info);
  // Not popping a SENTINEL in Solver destructor.
  DCHECK_EQ(finalType, SENTINEL);
  // Not popping initial SENTINEL in Solver destructor.
  DCHECK_EQ(info.int_info, SOLVER_CTOR_SENTINEL);

  Search* search = searches_.back();
  searches_.pop_back();
  CHECK(searches_.empty())
      << "non empty list of searches when ending the solver";
  delete search;
  DeleteDemonMonitor(demon_monitor_);
}

const SolverParameters::TrailCompression
SolverParameters::kDefaultTrailCompression = SolverParameters::NO_COMPRESSION;
const int SolverParameters::kDefaultTrailBlockSize = 8000;
const int SolverParameters::kDefaultArraySplitSize = 16;
const bool SolverParameters::kDefaultNameStoring = true;
const SolverParameters::ProfileLevel SolverParameters::kDefaultProfileLevel =
         SolverParameters::NO_PROFILING;

string Solver::DebugString() const {
  string out = "Solver(name = \"" + name_ + "\", state = ";
  switch (state_) {
    case OUTSIDE_SEARCH:
      out += "OUTSIDE_SEARCH";
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
  StringAppendF(&out, ", branches = %" GG_LL_FORMAT
                "d, fails = %" GG_LL_FORMAT
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


int64 Solver::wall_time() const {
  return timer_->GetInMs();
}

int64 Solver::solutions() const {
  return searches_.front()->solution_counter();
}

bool Solver::LocalOptimum() {
  return searches_.front()->LocalOptimum();
}

bool Solver::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  return searches_.front()->AcceptDelta(delta, deltadelta);
}

void Solver::AcceptNeighbor() {
  return searches_.front()->AcceptNeighbor();
}

void Solver::TopPeriodicCheck() {
  searches_.front()->PeriodicCheck();
}

void Solver::PushState() {
  StateInfo info;
  PushState(SIMPLE_MARKER, info);
}

void Solver::PopState() {
  StateInfo info;
  MarkerType t = PopState(&info);
  CHECK_EQ(SIMPLE_MARKER, t);
}

void Solver::PushState(MarkerType t, const StateInfo& info) {
  StateMarker* m = new StateMarker(t, info);
  if (t != REVERSIBLE_ACTION || info.int_info == 0) {
    m->rev_int_index_ = trail_->rev_ints_.size();
    m->rev_int64_index_ = trail_->rev_int64s_.size();
    m->rev_uint64_index_ = trail_->rev_uint64s_.size();
    m->rev_ptr_index_ = trail_->rev_ptrs_.size();
    m->rev_boolvar_list_index_ = trail_->rev_boolvar_list_.size();
    m->rev_bools_index_ = trail_->rev_bools_.size();
    m->rev_int_memory_index_ = trail_->rev_int_memory_.size();
    m->rev_int64_memory_index_ = trail_->rev_int64_memory_.size();
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

MarkerType Solver::PopState(StateInfo* info) {
  CHECK(!searches_.back()->marker_stack_.empty())
      << "PopState() on an empty stack";
  CHECK(info != NULL);
  StateMarker* m = searches_.back()->marker_stack_.back();
  if (m->type_ != REVERSIBLE_ACTION || m->info_.int_info == 0) {
    trail_->BacktrackTo(m);
  }
  MarkerType t = m->type_;
  (*info) = m->info_;
  searches_.back()->marker_stack_.pop_back();
  delete m;
  queue_->increase_stamp();
  return t;
}

void Solver::check_alloc_state() {
  switch (state_) {
    case OUTSIDE_SEARCH:
    case IN_SEARCH:
    case NO_MORE_SOLUTIONS:
    case PROBLEM_INFEASIBLE:
      break;
    case AT_SOLUTION:
      LOG(FATAL) << "allocating at a leaf node";
  }
}

void Solver::AddFailHook(Action* a) {
  if (fail_hooks_ == NULL) {
    SaveValue(reinterpret_cast<void**>(&fail_hooks_));
    fail_hooks_ = UnsafeRevAlloc(new SimpleRevFIFO<Action*>);
  }
  fail_hooks_->Push(this, a);
}

void Solver::CallFailHooks() {
  if (fail_hooks_ != NULL) {
    for (SimpleRevFIFO<Action*>::Iterator it(fail_hooks_); it.ok(); ++it) {
      (*it)->Run(this);
    }
  }
}

void Solver::FreezeQueue() {
  queue_->Freeze();
}

void Solver::UnfreezeQueue() {
  queue_->Unfreeze();
}

void Solver::Enqueue(Demon* d) {
  queue_->Enqueue(d);
}

void Solver::ProcessDemonsOnQueue() {
  queue_->ProcessDemons();
}

uint64 Solver::stamp() const {
  return queue_->stamp();
}

uint64 Solver::fail_stamp() const {
  return fail_stamp_;
}

void Solver::set_queue_action_on_fail(Action* a) {
  queue_->set_action_on_fail(a);
}

void Solver::set_queue_cleaner_on_fail(DomainIntVar* const var) {
  variable_cleaner_->set_var(var);
  set_queue_action_on_fail(variable_cleaner_.get());
}

void Solver::clear_queue_action_on_fail() {
  queue_->clear_action_on_fail();
}

void Solver::AddConstraint(Constraint* const c) {
  if (state_ == IN_SEARCH) {
    queue_->AddConstraint(c);
  } else {
    if (FLAGS_cp_show_constraints) {
      LOG(INFO) << c->DebugString();
    }
    constraints_list_.push_back(c);
  }
}

void Solver::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitModel(name_);
  for (int index = 0; index < constraints_list_.size(); ++index) {
    Constraint* const constraint = constraints_list_[index];
    constraint->Accept(visitor);
  }
  visitor->EndVisitModel(name_);
}

void Solver::ProcessConstraints() {
  if (FLAGS_cp_visit_model) {
    ModelVisitor* const visitor = MakePrintModelVisitor();
    Accept(visitor);
  }

  for (constraints_ = 0;
       constraints_ < constraints_list_.size();
       ++constraints_) {
    Constraint* const constraint = constraints_list_[constraints_];
    if (parameters_.profile_level != SolverParameters::NO_PROFILING) {
      DemonMonitorStartInitialPropagation(demon_monitor_, constraint);
    }
    constraint->PostAndPropagate();
    if (parameters_.profile_level != SolverParameters::NO_PROFILING) {
      DemonMonitorEndInitialPropagation(demon_monitor_, constraint);
    }
  }
}

bool Solver::CurrentlyInSolve() const {
  DCHECK_GT(searches_.size(), 0);
  DCHECK(searches_.back() != NULL);
  return searches_.back()->created_by_solve();
}

bool Solver::Solve(DecisionBuilder* const db,
                   const std::vector<SearchMonitor*>& monitors) {
  return Solve(db, monitors.data(), monitors.size());
}

bool Solver::Solve(DecisionBuilder* const db, SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return Solve(db, monitors.data(), monitors.size());
}

bool Solver::Solve(DecisionBuilder* const db) {
  return Solve(db, NULL, Zero());
}

bool Solver::Solve(DecisionBuilder* const db,
                   SearchMonitor* const m1,
                   SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return Solve(db, monitors.data(), monitors.size());
}

bool Solver::Solve(DecisionBuilder* const db,
                   SearchMonitor* const m1,
                   SearchMonitor* const m2,
                   SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return Solve(db, monitors.data(), monitors.size());
}

bool Solver::Solve(DecisionBuilder* const db,
                   SearchMonitor* const m1,
                   SearchMonitor* const m2,
                   SearchMonitor* const m3,
                   SearchMonitor* const m4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  monitors.push_back(m4);
  return Solve(db, monitors.data(), monitors.size());
}

bool Solver::Solve(DecisionBuilder* const db,
                   SearchMonitor* const * monitors,
                   int size) {
  NewSearch(db, monitors, size);
  searches_.back()->set_created_by_solve(true);  // Overwrites default.
  NextSolution();
  const bool solution_found = searches_.back()->solution_counter() > 0;
  EndSearch();
  return solution_found;
}

void Solver::NewSearch(DecisionBuilder* const db,
                       const std::vector<SearchMonitor*>& monitors) {
  return NewSearch(db, monitors.data(), monitors.size());
}

void Solver::NewSearch(DecisionBuilder* const db, SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return NewSearch(db, monitors.data(), monitors.size());
}

void Solver::NewSearch(DecisionBuilder* const db) {
  return NewSearch(db, NULL, Zero());
}

void Solver::NewSearch(DecisionBuilder* const db,
                       SearchMonitor* const m1,
                       SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return NewSearch(db, monitors.data(), monitors.size());
}

void Solver::NewSearch(DecisionBuilder* const db,
                       SearchMonitor* const m1,
                       SearchMonitor* const m2,
                       SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return NewSearch(db, monitors.data(), monitors.size());
}

void Solver::NewSearch(DecisionBuilder* const db,
                       SearchMonitor* const m1,
                       SearchMonitor* const m2,
                       SearchMonitor* const m3,
                       SearchMonitor* const m4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  monitors.push_back(m4);
  return NewSearch(db, monitors.data(), monitors.size());
}

// Opens a new top level search.
void Solver::NewSearch(DecisionBuilder* const db,
                       SearchMonitor* const * monitors,
                       int size) {
  // TODO(user) : reset statistics

  CHECK_NOTNULL(db);
  DCHECK_GE(size, 0);

  if (state_ == IN_SEARCH) {
    LOG(FATAL) << "Use NestedSolve() inside search";
  }
  // Check state and go to OUTSIDE_SEARCH.
  Search* const search = searches_.back();
  search->set_created_by_solve(false);  // default behavior.

  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
  state_ = OUTSIDE_SEARCH;

  // Push monitors and enter search.
  for (int i = 0; i < size; ++i) {
    search->push_monitor(monitors[i]);
  }
  std::vector<SearchMonitor*> extras;
  db->AppendMonitors(this, &extras);
  for (ConstIter<std::vector<SearchMonitor*> > it(extras); !it.at_end(); ++it) {
    search->push_monitor(*it);
  }
  search->EnterSearch();

  // Push sentinel and set decision builder.
  DCHECK_EQ(1, searches_.size());
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
    MarkerType t = PopState(&info);
    switch (t) {
      case SENTINEL:
        CHECK_EQ(info.ptr_info, this) << "Wrong sentinel found";
        CHECK((info.int_info == ROOT_NODE_SENTINEL && searches_.size() == 1) ||
              (info.int_info == INITIAL_SEARCH_SENTINEL &&
               searches_.size() > 1));
        searches_.back()->sentinel_pushed_--;
        no_more_solutions = true;
        end_loop = true;
        break;
      case SIMPLE_MARKER:
        LOG(ERROR)
            << "Simple markers should not be encountered during search";
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
  if (searches_.size() == 1) {  // top level.
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

  if (parameters_.profile_level != SolverParameters::NO_PROFILING) {
    CHECK_NOTNULL(demon_monitor_);
    DemonMonitorRestartSearch(demon_monitor_);
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
    MarkerType t = PopState(&info);
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
  CHECK_GT(searches_.size(), 1) << "calling JumpToSentinel from top level";
  Search* c = searches_.back();
  Search* p = searches_[searches_.size() - 2];
  bool found = false;
  while (!c->marker_stack_.empty()) {
    StateMarker* m = c->marker_stack_.back();
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

class ReverseDecision : public Decision {
 public:
  explicit ReverseDecision(Decision* const d) : decision_(d) {
    CHECK(d != NULL);
  }
  virtual ~ReverseDecision() {}

  virtual void Apply(Solver* const s) {
    decision_->Refute(s);
  }

  virtual void Refute(Solver* const s) {
    decision_->Apply(s);
  }

  virtual void Accept(DecisionVisitor* const visitor) const {
    decision_->Accept(visitor);
  }

  virtual string DebugString() const {
    string str = "Reverse(";
    str += decision_->DebugString();
    str += ")";
    return str;
  }

 private:
  Decision* const decision_;
};


// Search for the next solution in the search tree.
bool Solver::NextSolution() {
  Search* const search = searches_.back();
  Decision* fd = NULL;
  const bool top_level = (searches_.size() == 1);

  if (top_level && state_ == OUTSIDE_SEARCH && !search->decision_builder()) {
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
        search->BeginInitialPropagation();
        CP_TRY(search) {
          ProcessConstraints();
          search->EndInitialPropagation();
          PushSentinel(ROOT_NODE_SENTINEL);
          state_ = IN_SEARCH;
          search->ClearBuffer();
        } CP_ON_FAIL {
          queue_->AfterFailure();
          BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
          state_ = PROBLEM_INFEASIBLE;
          return false;
        }
        break;
      }
      case IN_SEARCH:  // Usually after a RestartSearch
        break;
    }
  }

  volatile bool finish = false;
  volatile bool result = false;
  DecisionBuilder* const db = search->decision_builder();

  while (!finish) {
    CP_TRY(search) {
      if (fd != NULL) {
        StateInfo i1(fd,
                     1,
                     search->search_depth(),
                     search->left_search_depth());  // 1 for right branch
        PushState(CHOICE_POINT, i1);
        search->RefuteDecision(fd);
        branches_++;
        fd->Refute(this);
        search->AfterDecision(fd, false);
        search->RightMove();
        fd = NULL;
      }
      Decision* d = NULL;
      for (;;) {
        search->BeginNextDecision(db);
        d = db->Next(this);
        search->EndNextDecision(db, d);
        if (d == fail_decision_) {
          Fail();  // fail now instead of after 2 branches.
        }
        if (d != NULL) {
          DecisionModification modification = search->ModifyDecision();
          switch (modification) {
            case SWITCH_BRANCHES: {
              d = RevAlloc(new ReverseDecision(d));
            }  // We reverse the decision and fall through the normal code.
            case NO_CHANGE: {
              decisions_++;
              StateInfo i2(d,
                           0,
                           search->search_depth(),
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
    } CP_ON_FAIL {
      queue_->AfterFailure();
      if (search->should_finish()) {
        fd = NULL;
        BacktrackToSentinel(top_level ?
                            ROOT_NODE_SENTINEL :
                            INITIAL_SEARCH_SENTINEL);
        result = false;
        finish = true;
        search->set_should_finish(false);
        search->set_should_restart(false);
        // We do not need to push back the sentinel as we are exiting anyway.
      } else if (search->should_restart()) {
        fd = NULL;
        BacktrackToSentinel(top_level ?
                            ROOT_NODE_SENTINEL :
                            INITIAL_SEARCH_SENTINEL);
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
  CHECK_EQ(1, searches_.size());
  Search* const search = searches_.back();
  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
  search->ExitSearch();
  search->Clear();
  state_ = OUTSIDE_SEARCH;
}

bool Solver::CheckAssignment(Assignment* const solution) {
  CHECK(solution);
  if (state_ == IN_SEARCH) {
    LOG(FATAL) << "Use NestedSolve() inside search";
  }
  // Check state and go to OUTSIDE_SEARCH.
  Search* const search = searches_.back();
  search->set_created_by_solve(false);  // default behavior.

  BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
  state_ = OUTSIDE_SEARCH;

  // Push monitors and enter search.
  search->EnterSearch();

  // Push sentinel and set decision builder.
  DCHECK_EQ(1, searches_.size());
  PushSentinel(INITIAL_SEARCH_SENTINEL);
  search->BeginInitialPropagation();
  CP_TRY(search) {
    DecisionBuilder * const restore = MakeRestoreAssignment(solution);
    restore->Next(this);
    ProcessConstraints();
    search->EndInitialPropagation();
    BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    search->ClearBuffer();
    // TODO(user): Why INFEASIBLE?
    state_ = PROBLEM_INFEASIBLE;
    return true;
  } CP_ON_FAIL {
    Constraint* const ct = constraints_list_[constraints_];
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

bool Solver::NestedSolve(DecisionBuilder* const db,
                         bool restore,
                         const std::vector<SearchMonitor*>& monitors) {
  return NestedSolve(db, restore,  monitors.data(), monitors.size());
}

bool Solver::NestedSolve(DecisionBuilder* const db,
                         bool restore,
                         SearchMonitor* const m1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  return NestedSolve(db, restore, monitors.data(), monitors.size());
}

bool Solver::NestedSolve(DecisionBuilder* const db, bool restore) {
  return NestedSolve(db, restore, NULL, Zero());
}

bool Solver::NestedSolve(DecisionBuilder* const db,
                         bool restore,
                         SearchMonitor* const m1,
                         SearchMonitor* const m2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  return NestedSolve(db, restore, monitors.data(), monitors.size());
}

bool Solver::NestedSolve(DecisionBuilder* const db,
                         bool restore,
                         SearchMonitor* const m1,
                         SearchMonitor* const m2,
                         SearchMonitor* const m3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(m1);
  monitors.push_back(m2);
  monitors.push_back(m3);
  return NestedSolve(db, restore, monitors.data(), monitors.size());
}

bool Solver::NestedSolve(DecisionBuilder* const db,
                         bool restore,
                         SearchMonitor* const * monitors,
                         int size) {
  Search new_search(this);
  for (int i = 0; i < size; ++i) {
    new_search.push_monitor(monitors[i]);
  }
  std::vector<SearchMonitor*> extras;
  db->AppendMonitors(this, &extras);
  for (ConstIter<std::vector<SearchMonitor*> > it(extras); !it.at_end(); ++it) {
    new_search.push_monitor(*it);
  }
  searches_.push_back(&new_search);
  searches_.back()->set_created_by_solve(true);  // Overwrites default.
  new_search.EnterSearch();
  PushSentinel(INITIAL_SEARCH_SENTINEL);
  new_search.set_decision_builder(db);
  bool res = NextSolution();
  if (res) {
    if (restore) {
      BacktrackToSentinel(INITIAL_SEARCH_SENTINEL);
    } else {
      JumpToSentinelWhenNested();
    }
  }
  new_search.ExitSearch();
  new_search.Clear();
  searches_.pop_back();
  return res;
}

void Solver::Fail() {
  if (fail_intercept_) {
    fail_intercept_->Run();
    return;
  }
  ConstraintSolverFailHere();
  fails_++;
  NotifyFailureToDemonMonitor();
  searches_.back()->BeginFail();
  if (FLAGS_cp_trace_demons) {
    LOG(INFO) << "### Failure ###";
  }
  searches_.back()->JumpBack();
}

// --- Propagation object names ---

string Solver::GetName(const PropagationBaseObject* object) const {
  const string* name = FindOrNull(propagation_object_names_, object);
  if (name != NULL) {
    return *name;
  }
  const std::pair<string, const PropagationBaseObject*>* delegate_object =
      FindOrNull(delegate_objects_, object);
  if (delegate_object != NULL) {
    const string& prefix = delegate_object->first;
    const PropagationBaseObject* delegate = delegate_object->second;
    return prefix + "<" + delegate->name() + ">";
  }
  return empty_name_;
}

void Solver::SetName(const PropagationBaseObject* object, const string& name) {
  if (parameters_.store_names
      && GetName(object).compare(name) != 0) {  // in particular if name.empty()
    propagation_object_names_[object] = name;
  }
}

// ------------------ Useful Operators ------------------

std::ostream& operator << (std::ostream& out, const Solver* const s) {  // NOLINT
  out << s->DebugString();
  return out;
}

std::ostream& operator <<(std::ostream& out, const BaseObject* o) {  // NOLINT
  out << o->DebugString();
  return out;
}

// ---------- PropagationBaseObject ---------

string PropagationBaseObject::name() const {
  // TODO(user) : merge with GetName() code to remove a string copy.
  return solver_->GetName(this);
}

void PropagationBaseObject::set_name(const string& name) {
  solver_->SetName(this, name);
}

// ---------- Decision Builder ----------

string DecisionBuilder::DebugString() const {
  return "DecisionBuilder";
}

void DecisionBuilder::AppendMonitors(Solver* const solver,
                                    std::vector<SearchMonitor*>* const extras) {}

// ---------- Decision and DecisionVisitor ----------

void Decision::Accept(DecisionVisitor* const visitor) const {
  visitor->VisitUnknownDecision();
}

void DecisionVisitor::VisitSetVariableValue(IntVar* const var, int64 value) {}
void DecisionVisitor::VisitSplitVariableDomain(IntVar* const var,
                                               int64 value,
                                               bool lower) {}
void DecisionVisitor::VisitUnknownDecision() {}
void DecisionVisitor::VisitScheduleOrPostpone(IntervalVar* const var,
                                              int64 est) {}
void DecisionVisitor::VisitTryRankFirst(Sequence* const sequence, int index) {}

// ---------- ModelVisitor ----------

// Enums.

const char ModelVisitor::kAbs[] = "Abs";
const char ModelVisitor::kAllDifferent[] = "AllDifferent";
const char ModelVisitor::kAllowedAssignments[] = "AllowedAssignments";
const char ModelVisitor::kBetween[] = "Between";
const char ModelVisitor::kConvexPiecewise[] = "ConvexPiecewise";
const char ModelVisitor::kCountEqual[] = "CountEqual";
const char ModelVisitor::kCumulative[] = "Cumulative";
const char ModelVisitor::kDeviation[] = "Deviation";
const char ModelVisitor::kDifference[] = "Difference";
const char ModelVisitor::kDistribute[] = "Distribute";
const char ModelVisitor::kDivide[] = "Divide";
const char ModelVisitor::kDurationExpr[] = "DurationExpr";
const char ModelVisitor::kElement[] = "Element";
const char ModelVisitor::kElementConstraint[] = "ElementConstraint";
const char ModelVisitor::kEndExpr[] = "EndExpr";
const char ModelVisitor::kEquality[] = "Equality";
const char ModelVisitor::kFalseConstraint[] = "FalseConstraint";
const char ModelVisitor::kGreater[] = "Greater";
const char ModelVisitor::kGreaterOrEqual[] = "GreaterOrEqual";
const char ModelVisitor::kIntervalBinaryRelation[] = "IntervalBinaryRelation";
const char ModelVisitor::kIntervalDisjunction[] = "IntervalDisjunction";
const char ModelVisitor::kIntervalUnaryRelation[] = "IntervalUnaryRelation";
const char ModelVisitor::kIsBetween[] = "IsBetween;";
const char ModelVisitor::kIsDifferent[] = "IsDifferent";
const char ModelVisitor::kIsEqual[] = "IsEqual";
const char ModelVisitor::kIsGreaterOrEqual[] = "IsGreaterOrEqual";
const char ModelVisitor::kIsLessOrEqual[] = "IsLessOrEqual";
const char ModelVisitor::kIsMember[] = "IsMember;";
const char ModelVisitor::kLess[] = "Less";
const char ModelVisitor::kLessOrEqual[] = "LessOrEqual";
const char ModelVisitor::kLinkExprVar[] = "LinkExprVar";
const char ModelVisitor::kMapDomain[] = "MapDomain";
const char ModelVisitor::kMax[] = "Max";
const char ModelVisitor::kMaxEqual[] = "MaxEqual";
const char ModelVisitor::kMember[] = "Member";
const char ModelVisitor::kMin[] = "Min";
const char ModelVisitor::kMinEqual[] = "MinEqual";
const char ModelVisitor::kNoCycle[] = "NoCycle";
const char ModelVisitor::kNonEqual[] = "NonEqual";
const char ModelVisitor::kOpposite[] = "Opposite";
const char ModelVisitor::kPack[] = "Pack";
const char ModelVisitor::kPathCumul[] = "PathCumul";
const char ModelVisitor::kPerformedExpr[] = "PerformedExpr";
const char ModelVisitor::kProd[] = "Product";
const char ModelVisitor::kProduct[] = "Product";
const char ModelVisitor::kScalProd[] = "ScalarProduct";
const char ModelVisitor::kScalProdEqual[] = "ScalarProductEqual";
const char ModelVisitor::kScalProdGreaterOrEqual[] =
    "ScalarProductGreaterOrEqual";
const char ModelVisitor::kScalProdLessOrEqual[] = "ScalarProductLessOrEqual";
const char ModelVisitor::kSemiContinuous[] = "SemiContinuous";
const char ModelVisitor::kSequence[] = "Sequence";
const char ModelVisitor::kSquare[] = "Square";
const char ModelVisitor::kStartExpr[]= "StartExpr";
const char ModelVisitor::kSum[] = "Sum";
const char ModelVisitor::kSumEqual[] = "SumEqual";
const char ModelVisitor::kSumGreater[] = "SumGreater";
const char ModelVisitor::kSumGreaterOrEqual[] = "SumGreaterOrEqual";
const char ModelVisitor::kSumLess[] = "SumLess";
const char ModelVisitor::kSumLessOrEqual[] = "SumLessOrEqual";
const char ModelVisitor::kTransition[]= "Transition";
const char ModelVisitor::kTrueConstraint[] = "TrueConstraint";

const char ModelVisitor::kActiveArgument[] = "active";
const char ModelVisitor::kCardsArgument[] = "cardinalities";
const char ModelVisitor::kCoefficientsArgument[] = "coefficients";
const char ModelVisitor::kCountArgument[] = "count";
const char ModelVisitor::kCumulsArgument[] = "cumuls";
const char ModelVisitor::kEarlyCostArgument[] = "early_cost";
const char ModelVisitor::kEarlyDateArgument[] = "early_date";
const char ModelVisitor::kExpressionArgument[] = "expression";
const char ModelVisitor::kFinalStates[] = "final_states";
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
const char ModelVisitor::kMinArgument[] = "min_value";
const char ModelVisitor::kNextsArgument[] = "nexts";
const char ModelVisitor::kRangeArgument[] = "range";
const char ModelVisitor::kRelationArgument[] = "relation";
const char ModelVisitor::kRightArgument[] = "right";
const char ModelVisitor::kSizeArgument[] = "size";
const char ModelVisitor::kStepArgument[] = "step";
const char ModelVisitor::kTargetArgument[] = "target_variable";
const char ModelVisitor::kTransitsArgument[] = "transits";
const char ModelVisitor::kTuplesArgument[] = "tuples";
const char ModelVisitor::kValueArgument[] = "value";
const char ModelVisitor::kValuesArgument[] = "values";
const char ModelVisitor::kVarsArgument[] = "variables";

// Methods

ModelVisitor::~ModelVisitor() {}

void ModelVisitor::BeginVisitModel(const string& type_name) {}
void ModelVisitor::EndVisitModel(const string& type_name) {}

void ModelVisitor::BeginVisitConstraint(const string& type_name,
                                        const Constraint* const constraint) {}
void ModelVisitor::EndVisitConstraint(const string& type_name,
                                      const Constraint* const constraint) {}

void ModelVisitor::BeginVisitExtension(const string& type, const string& name) {
}
void ModelVisitor::EndVisitExtension(const string& type, const string& name) {}

void ModelVisitor::BeginVisitIntegerExpression(const string& type_name,
                                               const IntExpr* const expr) {}
void ModelVisitor::EndVisitIntegerExpression(const string& type_name,
                                             const IntExpr* const expr) {}

void ModelVisitor::VisitIntegerVariable(const IntVar* const variable,
                                        const IntExpr* const delegate) {
  if (delegate != NULL) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntervalVariable(const IntervalVar* const variable,
                                         const string operation,
                                         const IntervalVar* const delegate) {
  if (delegate != NULL) {
    delegate->Accept(this);
  }
}


void ModelVisitor::VisitIntegerArgument(const Constraint* const master,
                                        const string& arg_name,
                                        int64 value) {}
void ModelVisitor::VisitIntegerArgument(const IntExpr* const master,
                                        const string& arg_name,
                                        int64 value) {}

void ModelVisitor::VisitIntegerArrayArgument(const Constraint* const master,
                                             const string& arg_name,
                                             const int64 * const values,
                                             int size) {
}
void ModelVisitor::VisitIntegerArrayArgument(const IntExpr* const master,
                                             const string& arg_name,
                                             const int64 * const values,
                                             int size) {
}

void ModelVisitor::VisitIntegerExpressionArgument(
    const Constraint* const master,
    const string& arg_name,
    const IntExpr* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntegerExpressionArgument(
    const IntExpr* const master,
    const string& arg_name,
    const IntExpr* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntegerVariableArrayArgument(
    const Constraint* const master,
    const string& arg_name,
    const IntVar* const * arguments,
    int size) {
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelVisitor::VisitIntegerVariableArrayArgument(
    const IntExpr* const master,
    const string& arg_name,
    const IntVar* const * arguments,
    int size) {
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelVisitor::VisitIntervalArgument(
    const IntExpr* const master,
    const string& arg_name,
    const IntervalVar* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntervalArgument(
    const Constraint* const master,
    const string& arg_name,
    const IntervalVar* const argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntervalArrayArgument(
    const IntExpr* const master,
    const string& arg_name,
    const IntervalVar* const * arguments,
    int size) {
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelVisitor::VisitIntervalArrayArgument(
    const Constraint* const master,
    const string& arg_name,
    const IntervalVar* const * arguments,
    int size) {
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

// ----- Helpers -----

void ModelVisitor::VisitConstIntArrayArgument(const Constraint* const master,
                                              const string& arg_name,
                                              const ConstIntArray&  values) {
  VisitIntegerArrayArgument(master, arg_name, values.RawData(), values.size());
}

void ModelVisitor::VisitConstIntArrayArgument(const IntExpr* const master,
                                              const string& arg_name,
                                              const ConstIntArray& values) {
  VisitIntegerArrayArgument(master, arg_name, values.RawData(), values.size());
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
bool SearchMonitor::AcceptDelta(Assignment* delta,
                                Assignment* deltadelta) { return true; }
void SearchMonitor::AcceptNeighbor() {}
void SearchMonitor::FinishCurrentSearch() {
  solver()->searches_.back()->set_should_finish(true);
}
void SearchMonitor::RestartCurrentSearch() {
  solver()->searches_.back()->set_should_restart(true);
}
void SearchMonitor::PeriodicCheck() {}

// ----------------- Constraint class -------------------

string Constraint::DebugString() const {
  return "Constraint";
}

void Constraint::PostAndPropagate() {
  FreezeQueue();
  Post();
  InitialPropagate();
  UnfreezeQueue();
}

void Constraint::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint("unknown", this);
  visitor->EndVisitConstraint("unknown", this);
}

// ----- Class IntExpr -----

void IntExpr::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitIntegerExpression("unknown", this);
  visitor->EndVisitIntegerExpression("unknown", this);
}

#undef CP_TRY  // We no longer need those.
#undef CP_ON_FAIL
#undef CP_DO_FAIL

}  // namespace operations_research
