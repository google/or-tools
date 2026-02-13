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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_
#define ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/model_cache.h"

namespace operations_research {

#ifndef SWIG
/// This class represent a reversible FIFO structure.
/// The main difference w.r.t a standard FIFO structure is that a Solver is
/// given as parameter to the modifiers such that the solver can store the
/// backtrack information
/// Iterator's traversing order should not be changed, as some algorithm
/// depend on it to be consistent.
/// It's main use is to store a list of demons in the various classes of
/// variables.template <class T>
template <class T>
class SimpleRevFIFO {
 private:
  enum { CHUNK_SIZE = 16 };  // TODO(user): could be an extra template param
  struct Chunk {
    T data[CHUNK_SIZE];
    const Chunk* const next;
    explicit Chunk(const Chunk* next) : next(next) {}
  };

 public:
  /// This iterator is not stable with respect to deletion.
  class Iterator {
   public:
    explicit Iterator(const SimpleRevFIFO<T>* l)
        : chunk_(l->chunks_), value_(l->Last()) {}
    bool ok() const { return (value_ != nullptr); }
    T operator*() const { return *value_; }
    void operator++() {
      ++value_;
      if (value_ == chunk_->data + CHUNK_SIZE) {
        chunk_ = chunk_->next;
        value_ = chunk_ ? chunk_->data : nullptr;
      }
    }

   private:
    const Chunk* chunk_;
    const T* value_;
  };

  SimpleRevFIFO() : chunks_(nullptr), pos_(0) {}

  void Push(Solver* s, T val) {
    if (pos_.Value() == 0) {
      Chunk* const chunk = s->UnsafeRevAlloc(new Chunk(chunks_));
      s->SaveAndSetValue(reinterpret_cast<void**>(&chunks_),
                         reinterpret_cast<void*>(chunk));
      pos_.SetValue(s, CHUNK_SIZE - 1);
    } else {
      pos_.Decr(s);
    }
    chunks_->data[pos_.Value()] = val;
  }

  /// Pushes the var on top if is not a duplicate of the current top object.
  void PushIfNotTop(Solver* s, T val) {
    if (chunks_ == nullptr || LastValue() != val) {
      Push(s, val);
    }
  }

  /// Returns the last item of the FIFO.
  const T* Last() const {
    return chunks_ ? &chunks_->data[pos_.Value()] : nullptr;
  }

  T* MutableLast() { return chunks_ ? &chunks_->data[pos_.Value()] : nullptr; }

  /// Returns the last value in the FIFO.
  const T& LastValue() const {
    DCHECK(chunks_);
    return chunks_->data[pos_.Value()];
  }

  /// Sets the last value in the FIFO.
  void SetLastValue(const T& v) {
    DCHECK(Last());
    chunks_->data[pos_.Value()] = v;
  }

 private:
  Chunk* chunks_;
  NumericalRev<int> pos_;
};
#endif  // SWIG

// ----- RevIntSet -----

/// This is a special class to represent a 'residual' set of T. T must
/// be an integer type.  You fill it at first, and then during search,
/// you can efficiently remove an element, and query the removed
/// elements.
template <class T>
class RevIntSet {
 public:
  static constexpr int kNoInserted = -1;

  /// Capacity is the fixed size of the set (it cannot grow).
  explicit RevIntSet(int capacity)
      : elements_(new T[capacity]),
        num_elements_(0),
        capacity_(capacity),
        position_(new int[capacity]),
        delete_position_(true) {
    for (int i = 0; i < capacity; ++i) {
      position_[i] = kNoInserted;
    }
  }

  /// Capacity is the fixed size of the set (it cannot grow).
  RevIntSet(int capacity, int* shared_positions, int shared_positions_size)
      : elements_(new T[capacity]),
        num_elements_(0),
        capacity_(capacity),
        position_(shared_positions),
        delete_position_(false) {
    for (int i = 0; i < shared_positions_size; ++i) {
      position_[i] = kNoInserted;
    }
  }

  ~RevIntSet() {
    if (delete_position_) {
      delete[] position_;
    }
  }

  int Size() const { return num_elements_.Value(); }

  int Capacity() const { return capacity_; }

  T Element(int i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, num_elements_.Value());
    return elements_[i];
  }

  T RemovedElement(int i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i + num_elements_.Value(), capacity_);
    return elements_[i + num_elements_.Value()];
  }

  void Insert(Solver* solver, const T& elt) {
    const int position = num_elements_.Value();
    DCHECK_LT(position, capacity_);  /// Valid.
    DCHECK(NotAlreadyInserted(elt));
    elements_[position] = elt;
    position_[elt] = position;
    num_elements_.Incr(solver);
  }

  void Remove(Solver* solver, const T& value_index) {
    num_elements_.Decr(solver);
    SwapTo(value_index, num_elements_.Value());
  }

  void Restore(Solver* solver, const T& value_index) {
    SwapTo(value_index, num_elements_.Value());
    num_elements_.Incr(solver);
  }

  void Clear(Solver* solver) { num_elements_.SetValue(solver, 0); }

  /// Iterators on the indices.
  typedef const T* const_iterator;
  const_iterator begin() const { return elements_.get(); }
  const_iterator end() const { return elements_.get() + num_elements_.Value(); }

 private:
  /// Used in DCHECK.
  bool NotAlreadyInserted(const T& elt) {
    for (int i = 0; i < num_elements_.Value(); ++i) {
      if (elt == elements_[i]) {
        return false;
      }
    }
    return true;
  }

  void SwapTo(T value_index, int next_position) {
    const int current_position = position_[value_index];
    if (current_position != next_position) {
      const T next_value_index = elements_[next_position];
      elements_[current_position] = next_value_index;
      elements_[next_position] = value_index;
      position_[value_index] = next_position;
      position_[next_value_index] = current_position;
    }
  }

  /// Set of elements.
  std::unique_ptr<T[]> elements_;
  /// Number of elements currently in the set.
  NumericalRev<int> num_elements_;
  /// The capacity of the set.
  const int capacity_;
  /// Reverse mapping.
  int* position_;
  /// Ownership of the position array.
  bool delete_position_;
};

#ifndef SWIG
class SmallRevBitSet {
 public:
  explicit SmallRevBitSet(int64_t size);
  /// Sets the 'pos' bit.
  void SetToOne(Solver* solver, int64_t pos);
  /// Erases the 'pos' bit.
  void SetToZero(Solver* solver, int64_t pos);
  /// Returns the number of bits set to one.
  int64_t Cardinality() const;
  /// Is bitset null?
  bool IsCardinalityZero() const { return bits_.Value() == uint64_t{0}; }
  /// Does it contains only one bit set?
  bool IsCardinalityOne() const {
    return (bits_.Value() != 0) && !(bits_.Value() & (bits_.Value() - 1));
  }
  /// Gets the index of the first bit set starting from 0.
  /// It returns -1 if the bitset is empty.
  int64_t GetFirstOne() const;

 private:
  Rev<uint64_t> bits_;
};

/// This class represents a reversible bitset.
/// This class is useful to maintain supports.
class RevBitSet {
 public:
  explicit RevBitSet(int64_t size);

  /// Sets the 'index' bit.
  void SetToOne(Solver* solver, int64_t index);
  /// Erases the 'index' bit.
  void SetToZero(Solver* solver, int64_t index);
  /// Returns whether the 'index' bit is set.
  bool IsSet(int64_t index) const;
  /// Returns the number of bits set to one.
  int64_t Cardinality() const;
  /// Is bitset null?
  bool IsCardinalityZero() const;
  /// Does it contains only one bit set?
  bool IsCardinalityOne() const;
  /// Gets the index of the first bit set starting from start.
  /// It returns -1 if the bitset is empty after start.
  int64_t GetFirstBit(int start) const;
  /// Cleans all bits.
  void ClearAll(Solver* solver);

  friend class RevBitMatrix;

 private:
  /// Save the offset's part of the bitset.
  void Save(Solver* solver, int offset);
  const int64_t size_;
  const int64_t length_;
  std::unique_ptr<uint64_t[]> bits_;
  std::unique_ptr<uint64_t[]> stamps_;
};

/// Matrix version of the RevBitSet class.
class RevBitMatrix : private RevBitSet {
 public:
  RevBitMatrix(int64_t rows, int64_t columns);
  ~RevBitMatrix();

  /// Sets the 'column' bit in the 'row' row.
  void SetToOne(Solver* solver, int64_t row, int64_t column);
  /// Erases the 'column' bit in the 'row' row.
  void SetToZero(Solver* solver, int64_t row, int64_t column);
  /// Returns whether the 'column' bit in the 'row' row is set.
  bool IsSet(int64_t row, int64_t column) const {
    DCHECK_GE(row, 0);
    DCHECK_LT(row, rows_);
    DCHECK_GE(column, 0);
    DCHECK_LT(column, columns_);
    return RevBitSet::IsSet(row * columns_ + column);
  }
  /// Returns the number of bits set to one in the 'row' row.
  int64_t Cardinality(int row) const;
  /// Is bitset of row 'row' null?
  bool IsCardinalityZero(int row) const;
  /// Does the 'row' bitset contains only one bit set?
  bool IsCardinalityOne(int row) const;
  /// Returns the first bit in the row 'row' which position is >= 'start'.
  /// It returns -1 if there are none.
  int64_t GetFirstBit(int row, int start) const;
  /// Cleans all bits.
  void ClearAll(Solver* solver);

 private:
  const int64_t rows_;
  const int64_t columns_;
};
#endif  // SWIG

/// This class represents a reversible bitset. It is meant to represent a set of
/// active bits. It does not offer direct access, but just methods that can
/// reversibly subtract another bitset, or check if the current active bitset
/// intersects with another bitset.
class UnsortedNullableRevBitset {
 public:
  /// Size is the number of bits to store in the bitset.
  explicit UnsortedNullableRevBitset(int bit_size);

  ~UnsortedNullableRevBitset() = default;

  /// This methods overwrites the active bitset with the mask. This method
  /// should be called only once.
  void Init(Solver* solver, const std::vector<uint64_t>& mask);

  /// This method subtracts the mask from the active bitset. It returns true if
  /// the active bitset was changed in the process.
  bool RevSubtract(Solver* solver, const std::vector<uint64_t>& mask);

  /// This method ANDs the mask with the active bitset. It returns true if
  /// the active bitset was changed in the process.
  bool RevAnd(Solver* solver, const std::vector<uint64_t>& mask);

  /// This method returns the number of non null 64 bit words in the bitset
  /// representation.
  int ActiveWordSize() const { return active_words_.Size(); }

  /// This method returns true if the active bitset is null.
  bool Empty() const { return active_words_.Size() == 0; }

  /// This method returns true iff the mask and the active bitset have a non
  /// null intersection. support_index is used as an accelerator:
  ///   - The first word tested to check the intersection will be thecan you
  ///     '*support_index'th one.
  ///   - If the intersection is not null, the support_index will be filled with
  ///     the index of the word that does intersect with the mask. This can be
  ///     reused later to speed-up the check.
  bool Intersects(const std::vector<uint64_t>& mask, int* support_index);

  /// Returns the number of bits given in the constructor of the bitset.
  int64_t bit_size() const { return bit_size_; }
  /// Returns the number of 64 bit words used to store the bitset.
  int64_t word_size() const { return word_size_; }
  /// Returns the set of active word indices.
  const RevIntSet<int>& active_words() const { return active_words_; }

 private:
  void CleanUpActives(Solver* solver);

  const int64_t bit_size_;
  const int64_t word_size_;
  RevArray<uint64_t> bits_;
  RevIntSet<int> active_words_;
  std::vector<int> to_remove_;
};

/// Reversible Immutable MultiMap class.
/// Represents an immutable multi-map that backtracks with the solver.
template <class K, class V>
class RevImmutableMultiMap {
 public:
  RevImmutableMultiMap(Solver* solver, int initial_size)
      : solver_(solver),
        array_(solver->UnsafeRevAllocArray(new Cell*[initial_size])),
        size_(initial_size),
        num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_.Value());
  }

  ~RevImmutableMultiMap() = default;

  int num_items() const { return num_items_.Value(); }

  /// Returns true if the multi-map contains at least one instance of 'key'.
  bool ContainsKey(const K& key) const {
    uint64_t code = Hash1(key) % size_.Value();
    Cell* tmp = array_[code];
    while (tmp) {
      if (tmp->key() == key) {
        return true;
      }
      tmp = tmp->next();
    }
    return false;
  }

  /// Returns one value attached to 'key', or 'default_value' if 'key'
  /// is not in the multi-map. The actual value returned if more than one
  /// values is attached to the same key is not specified.
  const V& FindWithDefault(const K& key, const V& default_value) const {
    uint64_t code = Hash1(key) % size_.Value();
    Cell* tmp = array_[code];
    while (tmp) {
      if (tmp->key() == key) {
        return tmp->value();
      }
      tmp = tmp->next();
    }
    return default_value;
  }

  /// Inserts (key, value) in the multi-map.
  void Insert(const K& key, const V& value) {
    const int position = Hash1(key) % size_.Value();
    Cell* const cell =
        solver_->UnsafeRevAlloc(new Cell(key, value, array_[position]));
    solver_->SaveAndSetValue(reinterpret_cast<void**>(&array_[position]),
                             reinterpret_cast<void*>(cell));
    num_items_.Incr(solver_);
    if (num_items_.Value() > 2 * size_.Value()) {
      Double();
    }
  }

 private:
  class Cell {
   public:
    Cell(const K& key, const V& value, Cell* next)
        : key_(key), value_(value), next_(next) {}

    void SetRevNext(Solver* solver, Cell* next) {
      solver->SaveAndSetValue(reinterpret_cast<void**>(&next_),
                              reinterpret_cast<void*>(next));
    }

    Cell* next() const { return next_; }

    const K& key() const { return key_; }

    const V& value() const { return value_; }

   private:
    const K key_;
    const V value_;
    Cell* next_;
  };

  void Double() {
    Cell** const old_cell_array = array_;
    const int old_size = size_.Value();
    size_.SetValue(solver_, size_.Value() * 2);
    solver_->SaveAndSetValue(
        reinterpret_cast<void**>(&array_),
        reinterpret_cast<void*>(
            solver_->UnsafeRevAllocArray(new Cell*[size_.Value()])));
    memset(array_, 0, size_.Value() * sizeof(*array_));
    for (int i = 0; i < old_size; ++i) {
      Cell* tmp = old_cell_array[i];
      while (tmp != nullptr) {
        Cell* const to_reinsert = tmp;
        tmp = tmp->next();
        const uint64_t new_position = Hash1(to_reinsert->key()) % size_.Value();
        to_reinsert->SetRevNext(solver_, array_[new_position]);
        solver_->SaveAndSetValue(
            reinterpret_cast<void**>(&array_[new_position]),
            reinterpret_cast<void*>(to_reinsert));
      }
    }
  }

  Solver* const solver_;
  Cell** array_;
  NumericalRev<int> size_;
  NumericalRev<int> num_items_;
};

/// A reversible switch that can switch once from false to true.
class RevSwitch {
 public:
  RevSwitch() : value_(false) {}

  bool Switched() const { return value_; }

  void Switch(Solver* solver) { solver->SaveAndSetValue(&value_, true); }

 private:
  bool value_;
};

/// This class is a reversible growing array. In can grow in both
/// directions, and even accept negative indices.  The objects stored
/// have a type T. As it relies on the solver for reversibility, these
/// objects can be up-casted to 'C' when using Solver::SaveValue().
template <class T, class C>
class RevGrowingArray {
 public:
  explicit RevGrowingArray(int64_t block_size)
      : block_size_(block_size), block_offset_(0) {
    CHECK_GT(block_size, 0);
  }

  ~RevGrowingArray() {
    for (int i = 0; i < elements_.size(); ++i) {
      delete[] elements_[i];
    }
  }

  T At(int64_t index) const {
    const int64_t block_index = ComputeBlockIndex(index);
    const int64_t relative_index = block_index - block_offset_;
    if (relative_index < 0 || relative_index >= elements_.size()) {
      return T();
    }
    const T* block = elements_[relative_index];
    return block != nullptr ? block[index - block_index * block_size_] : T();
  }

  void RevInsert(Solver* solver, int64_t index, T value) {
    const int64_t block_index = ComputeBlockIndex(index);
    T* const block = GetOrCreateBlock(block_index);
    const int64_t residual = index - block_index * block_size_;
    solver->SaveAndSetValue(reinterpret_cast<C*>(&block[residual]),
                            reinterpret_cast<C>(value));
  }

 private:
  T* NewBlock() const {
    T* const result = new T[block_size_];
    for (int i = 0; i < block_size_; ++i) {
      result[i] = T();
    }
    return result;
  }

  T* GetOrCreateBlock(int block_index) {
    if (elements_.size() == 0) {
      block_offset_ = block_index;
      GrowUp(block_index);
    } else if (block_index < block_offset_) {
      GrowDown(block_index);
    } else if (block_index - block_offset_ >= elements_.size()) {
      GrowUp(block_index);
    }
    T* block = elements_[block_index - block_offset_];
    if (block == nullptr) {
      block = NewBlock();
      elements_[block_index - block_offset_] = block;
    }
    return block;
  }

  int64_t ComputeBlockIndex(int64_t value) const {
    return value >= 0 ? value / block_size_
                      : (value - block_size_ + 1) / block_size_;
  }

  void GrowUp(int64_t block_index) {
    elements_.resize(block_index - block_offset_ + 1);
  }

  void GrowDown(int64_t block_index) {
    const int64_t delta = block_offset_ - block_index;
    block_offset_ = block_index;
    DCHECK_GT(delta, 0);
    elements_.insert(elements_.begin(), delta, nullptr);
  }

  const int64_t block_size_;
  std::vector<T*> elements_;
  int block_offset_;
};

class RevPartialSequence {
 public:
  explicit RevPartialSequence(const std::vector<int>& items)
      : elements_(items),
        first_ranked_(0),
        last_ranked_(items.size() - 1),
        size_(items.size()),
        position_(new int[size_]) {
    for (int i = 0; i < size_; ++i) {
      elements_[i] = items[i];
      position_[i] = i;
    }
  }

  explicit RevPartialSequence(int size)
      : elements_(size),
        first_ranked_(0),
        last_ranked_(size - 1),
        size_(size),
        position_(new int[size_]) {
    for (int i = 0; i < size_; ++i) {
      elements_[i] = i;
      position_[i] = i;
    }
  }

  ~RevPartialSequence() {}

  int NumFirstRanked() const { return first_ranked_.Value(); }

  int NumLastRanked() const { return size_ - 1 - last_ranked_.Value(); }

  int Size() const { return size_; }

#if !defined(SWIG)
  const int& operator[](int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, size_);
    return elements_[index];
  }
#endif

  void RankFirst(Solver* solver, int elt) {
    DCHECK_LE(first_ranked_.Value(), last_ranked_.Value());
    SwapTo(elt, first_ranked_.Value());
    first_ranked_.Incr(solver);
  }

  void RankLast(Solver* solver, int elt) {
    DCHECK_LE(first_ranked_.Value(), last_ranked_.Value());
    SwapTo(elt, last_ranked_.Value());
    last_ranked_.Decr(solver);
  }

  bool IsRanked(int elt) const {
    const int position = position_[elt];
    return (position < first_ranked_.Value() ||
            position > last_ranked_.Value());
  }

  std::string DebugString() const {
    std::string result = "[";
    for (int i = 0; i < first_ranked_.Value(); ++i) {
      absl::StrAppend(&result, elements_[i]);
      if (i != first_ranked_.Value() - 1) {
        result.append("-");
      }
    }
    result.append("|");
    for (int i = first_ranked_.Value(); i <= last_ranked_.Value(); ++i) {
      absl::StrAppend(&result, elements_[i]);
      if (i != last_ranked_.Value()) {
        result.append("-");
      }
    }
    result.append("|");
    for (int i = last_ranked_.Value() + 1; i < size_; ++i) {
      absl::StrAppend(&result, elements_[i]);
      if (i != size_ - 1) {
        result.append("-");
      }
    }
    result.append("]");
    return result;
  }

 private:
  void SwapTo(int elt, int next_position) {
    const int current_position = position_[elt];
    if (current_position != next_position) {
      const int next_elt = elements_[next_position];
      elements_[current_position] = next_elt;
      elements_[next_position] = elt;
      position_[elt] = next_position;
      position_[next_elt] = current_position;
    }
  }

  /// Set of elements.
  std::vector<int> elements_;
  /// Position of the element after the last element ranked from the start.
  NumericalRev<int> first_ranked_;
  /// Position of the element before the last element ranked from the end.
  NumericalRev<int> last_ranked_;
  /// Number of elements in the sequence.
  const int size_;
  /// Reverse mapping.
  std::unique_ptr<int[]> position_;
};

inline int64_t PosIntDivUp(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e < 0 || e % v == 0) ? e / v : e / v + 1;
}

inline int64_t PosIntDivDown(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e >= 0 || e % v == 0) ? e / v : e / v - 1;
}

template <class ValType, class VarType>
bool IsArrayInRange(const std::vector<VarType*>& vars, ValType min,
                    ValType max) {
  for (VarType* var : vars) {
    if (var->Min() < min || var->Max() > max) return false;
  }
  return true;
}

template <class VarType>
int64_t MinVarArray(const std::vector<VarType*>& vars) {
  int64_t min_val = std::numeric_limits<int64_t>::max();
  for (VarType* var : vars) {
    min_val = std::min(min_val, var->Min());
  }
  return min_val;
}

template <class VarType>
int64_t MaxVarArray(const std::vector<VarType*>& vars) {
  int64_t max_val = std::numeric_limits<int64_t>::min();
  for (VarType* var : vars) {
    max_val = std::max(max_val, var->Max());
  }
  return max_val;
}

template <class VarType>
bool AreAllBound(const std::vector<VarType*>& vars) {
  for (VarType* var : vars) {
    if (!var->Bound()) return false;
  }
  return true;
}

template <class VarType>
void FillValues(const std::vector<VarType*>& vars,
                std::vector<int64_t>* values) {
  values->clear();
  values->reserve(vars.size());
  for (VarType* var : vars) {
    values->push_back(var->Value());
  }
}

template <class T>
bool AreAllOnes(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 1) return false;
  }
  return true;
}

template <class T>
bool IsIncreasingContiguous(const std::vector<T>& values) {
  if (values.empty()) return true;
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) return false;
  }
  return true;
}

template <class T>
bool IsArrayConstant(const std::vector<T>& values, const T& value) {
  for (const T v : values) {
    if (v != value) return false;
  }
  return true;
}

template <class T>
bool IsArrayBoolean(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 0 && value != 1) return false;
  }
  return true;
}

template <class T>
bool IsIncreasing(const std::vector<T>& values) {
  if (values.empty()) return true;
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] < values[i]) return false;
  }
  return true;
}

template <class T>
bool AreAllNull(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 0) return false;
  }
  return true;
}

template <class VarType, class T>
bool AreAllBoundOrNull(const std::vector<VarType*>& vars,
                       const std::vector<T>& values) {
  if (values.size() != vars.size()) return false;
  for (int i = 0; i < vars.size(); ++i) {
    if (values[i] != 0 && !vars[i]->Bound()) return false;
  }
  return true;
}

template <class VarType>
bool AreAllBooleans(const std::vector<VarType*>& vars) {
  for (VarType* var : vars) {
    if (var->Min() < 0 || var->Max() > 1) return false;
  }
  return true;
}

template <class T>
bool AreAllPositive(const std::vector<T>& values) {
  for (const T value : values) {
    if (value < 0) return false;
  }
  return true;
}

template <class T>
bool AreAllNegative(const std::vector<T>& values) {
  for (const T value : values) {
    if (value > 0) return false;
  }
  return true;
}

std::vector<int64_t> ToInt64Vector(const std::vector<int>& input);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_
