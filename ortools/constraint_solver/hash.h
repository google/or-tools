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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_HASH_H_
#define ORTOOLS_CONSTRAINT_SOLVER_HASH_H_

#include <cstdint>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/log/check.h"

namespace operations_research {

inline uint64_t Hash1(uint64_t value) {
  value = (~value) + (value << 21);  /// value = (value << 21) - value - 1;
  value ^= value >> 24;
  value += (value << 3) + (value << 8);  /// value * 265
  value ^= value >> 14;
  value += (value << 2) + (value << 4);  /// value * 21
  value ^= value >> 28;
  value += (value << 31);
  return value;
}

inline uint64_t Hash1(uint32_t value) {
  uint64_t a = value;
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint64_t Hash1(int64_t value) {
  return Hash1(static_cast<uint64_t>(value));
}

inline uint64_t Hash1(int value) { return Hash1(static_cast<uint32_t>(value)); }

inline uint64_t Hash1(void* const ptr) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) ||      \
    defined(__aarch64__) || (defined(_MIPS_SZPTR) && (_MIPS_SZPTR == 64)) || \
    (defined(INTPTR_MAX) && defined(INT64_MAX) && (INTPTR_MAX == INT64_MAX))
  return Hash1(reinterpret_cast<uint64_t>(ptr));
#else
  return Hash1(reinterpret_cast<uint32_t>(ptr));
#endif
}

template <class T>
uint64_t Hash1(const std::vector<T*>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64_t hash = Hash1(ptrs[0]);
  for (int i = 1; i < ptrs.size(); ++i) {
    hash = hash * i + Hash1(ptrs[i]);
  }
  return hash;
}

inline uint64_t Hash1(const std::vector<int64_t>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64_t hash = Hash1(ptrs[0]);
  for (int i = 1; i < ptrs.size(); ++i) {
    hash = hash * i + Hash1(ptrs[i]);
  }
  return hash;
}

template <class T>
bool IsEqual(const T& a1, const T& a2) {
  return a1 == a2;
}

template <class T>
bool IsEqual(const std::vector<T*>& a1, const std::vector<T*>& a2) {
  if (a1.size() != a2.size()) {
    return false;
  }
  for (int i = 0; i < a1.size(); ++i) {
    if (a1[i] != a2[i]) {
      return false;
    }
  }
  return true;
}

template <class C>
void Double(C*** array_ptr, int* size_ptr) {
  DCHECK(array_ptr != nullptr);
  DCHECK(size_ptr != nullptr);
  C** const old_cell_array = *array_ptr;
  const int old_size = *size_ptr;
  (*size_ptr) *= 2;
  (*array_ptr) = new C*[(*size_ptr)];
  memset(*array_ptr, 0, (*size_ptr) * sizeof(**array_ptr));
  for (int i = 0; i < old_size; ++i) {
    C* tmp = old_cell_array[i];
    while (tmp != nullptr) {
      C* const to_reinsert = tmp;
      tmp = tmp->next();
      const uint64_t position = to_reinsert->Hash() % (*size_ptr);
      to_reinsert->set_next((*array_ptr)[position]);
      (*array_ptr)[position] = to_reinsert;
    }
  }
  delete[] (old_cell_array);
}

// ----- Cache objects built with 1 object -----

template <class C, class A1>
class Cache1 {
 public:
  explicit Cache1(int initial_size)
      : array_(new Cell*[initial_size]), size_(initial_size), num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache1() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1) const {
    uint64_t code = Hash1(a1) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, C* const c) {
    const int position = Hash1(a1) % size_;
    Cell* const cell = new Cell(a1, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, C* const container, Cell* const next)
        : a1_(a1), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1) const {
      if (IsEqual(a1_, a1)) {
        return container_;
      }
      return nullptr;
    }

    uint64_t Hash() const { return Hash1(a1_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

// ----- Cache objects built with 2 objects -----

template <class C, class A1, class A2>
class Cache2 {
 public:
  explicit Cache2(int initial_size)
      : array_(new Cell*[initial_size]), size_(initial_size), num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache2() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1, const A2& a2) const {
    uint64_t code = absl::HashOf(a1, a2) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1, a2);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, const A2& a2, C* const c) {
    const int position = absl::HashOf(a1, a2) % size_;
    Cell* const cell = new Cell(a1, a2, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, const A2& a2, C* const container, Cell* const next)
        : a1_(a1), a2_(a2), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1, const A2& a2) const {
      if (IsEqual(a1_, a1) && IsEqual(a2_, a2)) {
        return container_;
      }
      return nullptr;
    }

    uint64_t Hash() const { return absl::HashOf(a1_, a2_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    const A2 a2_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

// ----- Cache objects built with 2 objects -----

template <class C, class A1, class A2, class A3>
class Cache3 {
 public:
  explicit Cache3(int initial_size)
      : array_(new Cell*[initial_size]), size_(initial_size), num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache3() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1, const A2& a2, const A3& a3) const {
    uint64_t code = absl::HashOf(a1, a2, a3) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1, a2, a3);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, const A2& a2, const A3& a3, C* const c) {
    const int position = absl::HashOf(a1, a2, a3) % size_;
    Cell* const cell = new Cell(a1, a2, a3, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, const A2& a2, const A3& a3, C* const container,
         Cell* const next)
        : a1_(a1), a2_(a2), a3_(a3), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1, const A2& a2, const A3& a3) const {
      if (IsEqual(a1_, a1) && IsEqual(a2_, a2) && IsEqual(a3_, a3)) {
        return container_;
      }
      return nullptr;
    }

    uint64_t Hash() const { return absl::HashOf(a1_, a2_, a3_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    const A2 a2_;
    const A3 a3_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_HASH_H_
