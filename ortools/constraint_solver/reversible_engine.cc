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

#include "ortools/constraint_solver/reversible_engine.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "zlib.h"

namespace operations_research {

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

// ------------------ StateMarker / StateInfo struct -----------

StateMarker::StateMarker(TrailMarkerType t, const StateInfo& i)
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
  std::function<void(IntVar* var)> restore_bool_value = nullptr;

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
      restore_bool_value(var);
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

ReversibleEngine::ReversibleEngine() : state_(OUTSIDE_SEARCH) {}

ReversibleEngine::~ReversibleEngine() {}

void ReversibleEngine::Init(
    int trail_block_size,
    ConstraintSolverParameters::TrailCompression trail_compression) {
  trail_ = std::make_unique<Trail>(trail_block_size, trail_compression);
  state_ = OUTSIDE_SEARCH;
}

void ReversibleEngine::SetRestoreBooleanVarValue(
    std::function<void(IntVar* var)> restore_bool_value) {
  trail_->restore_bool_value = restore_bool_value;
}

void ReversibleEngine::FillStateMarker(StateMarker* m) {
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

void ReversibleEngine::BacktrackTo(StateMarker* m) { trail_->BacktrackTo(m); }

void ReversibleEngine::InternalSaveValue(int* valptr) {
  trail_->rev_ints.PushBack(addrval<int>(valptr));
}

void ReversibleEngine::InternalSaveValue(int64_t* valptr) {
  trail_->rev_int64s.PushBack(addrval<int64_t>(valptr));
}

void ReversibleEngine::InternalSaveValue(uint64_t* valptr) {
  trail_->rev_uint64s.PushBack(addrval<uint64_t>(valptr));
}

void ReversibleEngine::InternalSaveValue(double* valptr) {
  trail_->rev_doubles.PushBack(addrval<double>(valptr));
}

void ReversibleEngine::InternalSaveValue(void** valptr) {
  trail_->rev_ptrs.PushBack(addrval<void*>(valptr));
}

// TODO(user) : this code is unsafe if you save the same alternating
// bool multiple times.
// The correct code should use a bitset and a single list.
void ReversibleEngine::InternalSaveValue(bool* valptr) {
  trail_->rev_bools.push_back(valptr);
  trail_->rev_bool_value.push_back(*valptr);
}

void ReversibleEngine::InternalSaveBooleanVarValue(IntVar* var) {
  trail_->rev_boolvar_list.push_back(var);
}

BaseObject* ReversibleEngine::SafeRevAlloc(BaseObject* ptr) {
  check_alloc_state();
  trail_->rev_object_memory.push_back(ptr);
  return ptr;
}

int* ReversibleEngine::SafeRevAllocArray(int* ptr) {
  check_alloc_state();
  trail_->rev_int_memory.push_back(ptr);
  return ptr;
}

int64_t* ReversibleEngine::SafeRevAllocArray(int64_t* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory.push_back(ptr);
  return ptr;
}

double* ReversibleEngine::SafeRevAllocArray(double* ptr) {
  check_alloc_state();
  trail_->rev_double_memory.push_back(ptr);
  return ptr;
}

uint64_t* ReversibleEngine::SafeRevAllocArray(uint64_t* ptr) {
  check_alloc_state();
  trail_->rev_int64_memory.push_back(reinterpret_cast<int64_t*>(ptr));
  return ptr;
}

BaseObject** ReversibleEngine::SafeRevAllocArray(BaseObject** ptr) {
  check_alloc_state();
  trail_->rev_object_array_memory.push_back(ptr);
  return ptr;
}

IntVar** ReversibleEngine::SafeRevAllocArray(IntVar** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntVar**>(in);
}

IntExpr** ReversibleEngine::SafeRevAllocArray(IntExpr** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<IntExpr**>(in);
}

Constraint** ReversibleEngine::SafeRevAllocArray(Constraint** ptr) {
  BaseObject** in = SafeRevAllocArray(reinterpret_cast<BaseObject**>(ptr));
  return reinterpret_cast<Constraint**>(in);
}

void* ReversibleEngine::UnsafeRevAllocAux(void* ptr) {
  check_alloc_state();
  trail_->rev_memory.push_back(ptr);
  return ptr;
}

void** ReversibleEngine::UnsafeRevAllocArrayAux(void** ptr) {
  check_alloc_state();
  trail_->rev_memory_array.push_back(ptr);
  return ptr;
}

void ReversibleEngine::check_alloc_state() {
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

}  // namespace operations_research
