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

/// Collection of objects used to extend the Constraint Solver library.
///
/// This file contains a set of objects that simplifies writing extensions
/// of the library.
///
/// The main objects that define extensions are:
///   - BaseIntExpr, the base class of all expressions that are not variables.
///   - SimpleRevFIFO, a reversible FIFO list with templatized values.
///     A reversible data structure is a data structure that reverts its
///     modifications when the search is going up in the search tree, usually
///     after a failure occurs.
///   - RevImmutableMultiMap, a reversible immutable multimap.
///   - MakeConstraintDemon<n> and MakeDelayedConstraintDemon<n> to wrap methods
///     of a constraint as a demon.
///   - RevSwitch, a reversible flip-once switch.
///   - SmallRevBitSet, RevBitSet, and RevBitMatrix: reversible 1D or 2D
///     bitsets.
///   - LocalSearchOperator, IntVarLocalSearchOperator, ChangeValue and
///     PathOperator, to create new local search operators.
///   - LocalSearchFilter and IntVarLocalSearchFilter, to create new local
///     search filters.
///   - BaseLns, to write Large Neighborhood Search operators.
///   - SymmetryBreaker, to describe model symmetries that will be broken during
///     search using the 'Symmetry Breaking During Search' framework
///     see Gent, I. P., Harvey, W., & Kelsey, T. (2002).
///     Groups and Constraints: Symmetry Breaking During Search.
///     Principles and Practice of Constraint Programming CP2002
///     (Vol. 2470, pp. 415-430). Springer. Retrieved from
///     http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.11.1442.
///
/// Then, there are some internal classes that are used throughout the solver
/// and exposed in this file:
///   - SearchLog, the root class of all periodic outputs during search.
///   - ModelCache, A caching layer to avoid creating twice the same object.

#ifndef ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
#define ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/base_export.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

/// This is the base class for all expressions that are not variables.
/// It provides a basic 'CastToVar()' implementation.
///
/// The class of expressions represent two types of objects: variables
/// and subclasses of BaseIntExpr. Variables are stateful objects that
/// provide a rich API (remove values, WhenBound...). On the other hand,
/// subclasses of BaseIntExpr represent range-only stateless objects.
/// That is, min(A + B) is recomputed each time as min(A) + min(B).
///
/// Furthermore, sometimes, the propagation on an expression is not complete,
/// and Min(), Max() are not monotonic with respect to SetMin() and SetMax().
/// For instance, if A is a var with domain [0 .. 5], and B another variable
/// with domain [0 .. 5], then Plus(A, B) has domain [0, 10].
///
/// If we apply SetMax(Plus(A, B), 4)), we will deduce that both A
/// and B have domain [0 .. 4]. In that case, Max(Plus(A, B)) is 8
/// and not 4.  To get back monotonicity, we 'cast' the expression
/// into a variable using the Var() method (that will call CastToVar()
/// internally). The resulting variable will be stateful and monotonic.
///
/// Finally, one should never store a pointer to a IntExpr, or
/// BaseIntExpr in the code. The safe code should always call Var() on an
/// expression built by the solver, and store the object as an IntVar*.
/// This is a consequence of the stateless nature of the expressions that
/// makes the code error-prone.
class LocalSearchMonitor;

class BaseIntExpr : public IntExpr {
 public:
  explicit BaseIntExpr(Solver* const s) : IntExpr(s), var_(nullptr) {}
  ~BaseIntExpr() override {}

  IntVar* Var() override;
  virtual IntVar* CastToVar();

 private:
  IntVar* var_;
};

/// This enum is used internally to do dynamic typing on subclasses of integer
/// variables.
enum VarTypes {
  UNSPECIFIED,
  DOMAIN_INT_VAR,
  BOOLEAN_VAR,
  CONST_VAR,
  VAR_ADD_CST,
  VAR_TIMES_CST,
  CST_SUB_VAR,
  OPP_VAR,
  TRACE_VAR
};

/// This class represent a reversible FIFO structure.
/// The main difference w.r.t a standard FIFO structure is that a Solver is
/// given as parameter to the modifiers such that the solver can store the
/// backtrack information
/// Iterator's traversing order should not be changed, as some algorithm
/// depend on it to be consistent.
/// It's main use is to store a list of demons in the various classes of
/// variables.
#ifndef SWIG
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

  void Push(Solver* const s, T val) {
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
  void PushIfNotTop(Solver* const s, T val) {
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

/// Hash functions
// TODO(user): use murmurhash.
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
#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) || \
    defined(__aarch64__) || (defined(_MIPS_SZPTR) && (_MIPS_SZPTR == 64))
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

/// Reversible Immutable MultiMap class.
/// Represents an immutable multi-map that backtracks with the solver.
template <class K, class V>
class RevImmutableMultiMap {
 public:
  RevImmutableMultiMap(Solver* const solver, int initial_size)
      : solver_(solver),
        array_(solver->UnsafeRevAllocArray(new Cell*[initial_size])),
        size_(initial_size),
        num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_.Value());
  }

  ~RevImmutableMultiMap() {}

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
    Cell(const K& key, const V& value, Cell* const next)
        : key_(key), value_(value), next_(next) {}

    void SetRevNext(Solver* const solver, Cell* const next) {
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

  void Switch(Solver* const solver) { solver->SaveAndSetValue(&value_, true); }

 private:
  bool value_;
};

/// This class represents a small reversible bitset (size <= 64).
/// This class is useful to maintain supports.
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
  ~RevBitSet();

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
  uint64_t* bits_;
  uint64_t* stamps_;
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

/// @{
/// These methods represent generic demons that will call back a
/// method on the constraint during their Run method.
/// This way, all propagation methods are members of the constraint class,
/// and demons are just proxies with a priority of NORMAL_PRIORITY.

/// Demon proxy to a method on the constraint with no arguments.
template <class T>
class CallMethod0 : public Demon {
 public:
  CallMethod0(T* const ct, void (T::*method)(), const std::string& name)
      : constraint_(ct), method_(method), name_(name) {}

  ~CallMethod0() override {}

  void Run(Solver* const) override { (constraint_->*method_)(); }

  std::string DebugString() const override {
    return "CallMethod_" + name_ + "(" + constraint_->DebugString() + ")";
  }

 private:
  T* const constraint_;
  void (T::* const method_)();
  const std::string name_;
};

template <class T>
Demon* MakeConstraintDemon0(Solver* const s, T* const ct, void (T::*method)(),
                            const std::string& name) {
  return s->RevAlloc(new CallMethod0<T>(ct, method, name));
}

template <class P>
std::string ParameterDebugString(P param) {
  return absl::StrCat(param);
}

/// Support limited to pointers to classes which define DebugString().
template <class P>
std::string ParameterDebugString(P* param) {
  return param->DebugString();
}

/// Demon proxy to a method on the constraint with one argument.
template <class T, class P>
class CallMethod1 : public Demon {
 public:
  CallMethod1(T* const ct, void (T::*method)(P), const std::string& name,
              P param1)
      : constraint_(ct), method_(method), name_(name), param1_(param1) {}

  ~CallMethod1() override {}

  void Run(Solver* const) override { (constraint_->*method_)(param1_); }

  std::string DebugString() const override {
    return absl::StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
                        ", ", ParameterDebugString(param1_), ")");
  }

 private:
  T* const constraint_;
  void (T::* const method_)(P);
  const std::string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeConstraintDemon1(Solver* const s, T* const ct, void (T::*method)(P),
                            const std::string& name, P param1) {
  return s->RevAlloc(new CallMethod1<T, P>(ct, method, name, param1));
}

/// Demon proxy to a method on the constraint with two arguments.
template <class T, class P, class Q>
class CallMethod2 : public Demon {
 public:
  CallMethod2(T* const ct, void (T::*method)(P, Q), const std::string& name,
              P param1, Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  ~CallMethod2() override {}

  void Run(Solver* const) override {
    (constraint_->*method_)(param1_, param2_);
  }

  std::string DebugString() const override {
    return absl::StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
                        ", ", ParameterDebugString(param1_), ", ",
                        ParameterDebugString(param2_), ")");
  }

 private:
  T* const constraint_;
  void (T::* const method_)(P, Q);
  const std::string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeConstraintDemon2(Solver* const s, T* const ct,
                            void (T::*method)(P, Q), const std::string& name,
                            P param1, Q param2) {
  return s->RevAlloc(
      new CallMethod2<T, P, Q>(ct, method, name, param1, param2));
}
/// Demon proxy to a method on the constraint with three arguments.
template <class T, class P, class Q, class R>
class CallMethod3 : public Demon {
 public:
  CallMethod3(T* const ct, void (T::*method)(P, Q, R), const std::string& name,
              P param1, Q param2, R param3)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2),
        param3_(param3) {}

  ~CallMethod3() override {}

  void Run(Solver* const) override {
    (constraint_->*method_)(param1_, param2_, param3_);
  }

  std::string DebugString() const override {
    return absl::StrCat(absl::StrCat("CallMethod_", name_),
                        absl::StrCat("(", constraint_->DebugString()),
                        absl::StrCat(", ", ParameterDebugString(param1_)),
                        absl::StrCat(", ", ParameterDebugString(param2_)),
                        absl::StrCat(", ", ParameterDebugString(param3_), ")"));
  }

 private:
  T* const constraint_;
  void (T::* const method_)(P, Q, R);
  const std::string name_;
  P param1_;
  Q param2_;
  R param3_;
};

template <class T, class P, class Q, class R>
Demon* MakeConstraintDemon3(Solver* const s, T* const ct,
                            void (T::*method)(P, Q, R), const std::string& name,
                            P param1, Q param2, R param3) {
  return s->RevAlloc(
      new CallMethod3<T, P, Q, R>(ct, method, name, param1, param2, param3));
}
/// @}

/// @{
/// These methods represents generic demons that will call back a
/// method on the constraint during their Run method. This demon will
/// have a priority DELAYED_PRIORITY.

/// Low-priority demon proxy to a method on the constraint with no arguments.
template <class T>
class DelayedCallMethod0 : public Demon {
 public:
  DelayedCallMethod0(T* const ct, void (T::*method)(), const std::string& name)
      : constraint_(ct), method_(method), name_(name) {}

  ~DelayedCallMethod0() override {}

  void Run(Solver* const) override { (constraint_->*method_)(); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return "DelayedCallMethod_" + name_ + "(" + constraint_->DebugString() +
           ")";
  }

 private:
  T* const constraint_;
  void (T::* const method_)();
  const std::string name_;
};

template <class T>
Demon* MakeDelayedConstraintDemon0(Solver* const s, T* const ct,
                                   void (T::*method)(),
                                   const std::string& name) {
  return s->RevAlloc(new DelayedCallMethod0<T>(ct, method, name));
}

/// Low-priority demon proxy to a method on the constraint with one argument.
template <class T, class P>
class DelayedCallMethod1 : public Demon {
 public:
  DelayedCallMethod1(T* const ct, void (T::*method)(P), const std::string& name,
                     P param1)
      : constraint_(ct), method_(method), name_(name), param1_(param1) {}

  ~DelayedCallMethod1() override {}

  void Run(Solver* const) override { (constraint_->*method_)(param1_); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return absl::StrCat("DelayedCallMethod_", name_, "(",
                        constraint_->DebugString(), ", ",
                        ParameterDebugString(param1_), ")");
  }

 private:
  T* const constraint_;
  void (T::* const method_)(P);
  const std::string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeDelayedConstraintDemon1(Solver* const s, T* const ct,
                                   void (T::*method)(P),
                                   const std::string& name, P param1) {
  return s->RevAlloc(new DelayedCallMethod1<T, P>(ct, method, name, param1));
}

/// Low-priority demon proxy to a method on the constraint with two arguments.
template <class T, class P, class Q>
class DelayedCallMethod2 : public Demon {
 public:
  DelayedCallMethod2(T* const ct, void (T::*method)(P, Q),
                     const std::string& name, P param1, Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  ~DelayedCallMethod2() override {}

  void Run(Solver* const) override {
    (constraint_->*method_)(param1_, param2_);
  }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return absl::StrCat(absl::StrCat("DelayedCallMethod_", name_),
                        absl::StrCat("(", constraint_->DebugString()),
                        absl::StrCat(", ", ParameterDebugString(param1_)),
                        absl::StrCat(", ", ParameterDebugString(param2_), ")"));
  }

 private:
  T* const constraint_;
  void (T::* const method_)(P, Q);
  const std::string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeDelayedConstraintDemon2(Solver* const s, T* const ct,
                                   void (T::*method)(P, Q),
                                   const std::string& name, P param1,
                                   Q param2) {
  return s->RevAlloc(
      new DelayedCallMethod2<T, P, Q>(ct, method, name, param1, param2));
}
/// @}

#endif  // !defined(SWIG)

// ----- LightIntFunctionElementCt -----

template <typename F>
class LightIntFunctionElementCt : public Constraint {
 public:
  LightIntFunctionElementCt(Solver* const solver, IntVar* const var,
                            IntVar* const index, F values,
                            std::function<bool()> deep_serialize)
      : Constraint(solver),
        var_(var),
        index_(index),
        values_(std::move(values)),
        deep_serialize_(std::move(deep_serialize)) {}
  ~LightIntFunctionElementCt() override {}

  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightIntFunctionElementCt::IndexBound, "IndexBound");
    index_->WhenBound(demon);
  }

  void InitialPropagate() override {
    if (index_->Bound()) {
      IndexBound();
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("LightIntFunctionElementCt(%s, %s)",
                           var_->DebugString(), index_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLightElementEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    // Warning: This will expand all values into a vector.
    if (deep_serialize_ == nullptr || deep_serialize_()) {
      visitor->VisitInt64ToInt64Extension(values_, index_->Min(),
                                          index_->Max());
    }
    visitor->EndVisitConstraint(ModelVisitor::kLightElementEqual, this);
  }

 private:
  void IndexBound() { var_->SetValue(values_(index_->Min())); }

  IntVar* const var_;
  IntVar* const index_;
  F values_;
  std::function<bool()> deep_serialize_;
};

// ----- LightIntIntFunctionElementCt -----

template <typename F>
class LightIntIntFunctionElementCt : public Constraint {
 public:
  LightIntIntFunctionElementCt(Solver* const solver, IntVar* const var,
                               IntVar* const index1, IntVar* const index2,
                               F values, std::function<bool()> deep_serialize)
      : Constraint(solver),
        var_(var),
        index1_(index1),
        index2_(index2),
        values_(std::move(values)),
        deep_serialize_(std::move(deep_serialize)) {}
  ~LightIntIntFunctionElementCt() override {}
  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightIntIntFunctionElementCt::IndexBound,
        "IndexBound");
    index1_->WhenBound(demon);
    index2_->WhenBound(demon);
  }
  void InitialPropagate() override { IndexBound(); }

  std::string DebugString() const override {
    return "LightIntIntFunctionElementCt";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLightElementEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index1_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                            index2_);
    // Warning: This will expand all values into a vector.
    const int64_t index1_min = index1_->Min();
    const int64_t index1_max = index1_->Max();
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, index1_min);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, index1_max);
    if (deep_serialize_ == nullptr || deep_serialize_()) {
      for (int i = index1_min; i <= index1_max; ++i) {
        visitor->VisitInt64ToInt64Extension(
            [this, i](int64_t j) { return values_(i, j); }, index2_->Min(),
            index2_->Max());
      }
    }
    visitor->EndVisitConstraint(ModelVisitor::kLightElementEqual, this);
  }

 private:
  void IndexBound() {
    if (index1_->Bound() && index2_->Bound()) {
      var_->SetValue(values_(index1_->Min(), index2_->Min()));
    }
  }

  IntVar* const var_;
  IntVar* const index1_;
  IntVar* const index2_;
  F values_;
  std::function<bool()> deep_serialize_;
};

// ----- LightIntIntIntFunctionElementCt -----

template <typename F>
class LightIntIntIntFunctionElementCt : public Constraint {
 public:
  LightIntIntIntFunctionElementCt(Solver* const solver, IntVar* const var,
                                  IntVar* const index1, IntVar* const index2,
                                  IntVar* const index3, F values)
      : Constraint(solver),
        var_(var),
        index1_(index1),
        index2_(index2),
        index3_(index3),
        values_(std::move(values)) {}
  ~LightIntIntIntFunctionElementCt() override {}
  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightIntIntIntFunctionElementCt::IndexBound,
        "IndexBound");
    index1_->WhenBound(demon);
    index2_->WhenBound(demon);
    index3_->WhenBound(demon);
  }
  void InitialPropagate() override { IndexBound(); }

  std::string DebugString() const override {
    return "LightIntIntFunctionElementCt";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLightElementEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index1_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                            index2_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex3Argument,
                                            index3_);
    visitor->EndVisitConstraint(ModelVisitor::kLightElementEqual, this);
  }

 private:
  void IndexBound() {
    if (index1_->Bound() && index2_->Bound() && index3_->Bound()) {
      var_->SetValue(values_(index1_->Min(), index2_->Min(), index3_->Min()));
    }
  }

  IntVar* const var_;
  IntVar* const index1_;
  IntVar* const index2_;
  IntVar* const index3_;
  F values_;
};

/// The base class for all local search operators.
///
/// A local search operator is an object that defines the neighborhood of a
/// solution. In other words, a neighborhood is the set of solutions which can
/// be reached from a given solution using an operator.
///
/// The behavior of the LocalSearchOperator class is similar to iterators.
/// The operator is synchronized with an assignment (gives the
/// current values of the variables); this is done in the Start() method.
///
/// Then one can iterate over the neighbors using the MakeNextNeighbor method.
/// This method returns an assignment which represents the incremental changes
/// to the current solution. It also returns a second assignment representing
/// the changes to the last solution defined by the neighborhood operator; this
/// assignment is empty if the neighborhood operator cannot track this
/// information.
///
// TODO(user): rename Start to Synchronize ?
// TODO(user): decouple the iterating from the defining of a neighbor.
class LocalSearchOperator : public BaseObject {
 public:
  LocalSearchOperator() {}
  ~LocalSearchOperator() override {}
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) = 0;
  virtual void EnterSearch() {}
  virtual void Start(const Assignment* assignment) = 0;
  virtual void Reset() {}
#ifndef SWIG
  virtual const LocalSearchOperator* Self() const { return this; }
#endif  // SWIG
  virtual bool HasFragments() const { return false; }
  virtual bool HoldsDelta() const { return false; }
};

class LocalSearchOperatorState {
 public:
  LocalSearchOperatorState() {}

  void SetCurrentDomainInjectiveAndKeepInverseValues(int max_value) {
    max_inversible_index_ = candidate_values_.size();
    candidate_value_to_index_.resize(max_value + 1, -1);
    committed_value_to_index_.resize(max_value + 1, -1);
  }

  /// Returns the value in the current assignment of the variable of given
  /// index.
  int64_t CandidateValue(int64_t index) const {
    DCHECK_LT(index, candidate_values_.size());
    return candidate_values_[index];
  }
  int64_t CommittedValue(int64_t index) const {
    return committed_values_[index];
  }
  int64_t CheckPointValue(int64_t index) const {
    return checkpoint_values_[index];
  }
  void SetCandidateValue(int64_t index, int64_t value) {
    candidate_values_[index] = value;
    if (index < max_inversible_index_) {
      candidate_value_to_index_[value] = index;
    }
    MarkChange(index);
  }

  bool CandidateIsActive(int64_t index) const {
    return candidate_is_active_[index];
  }
  void SetCandidateActive(int64_t index, bool active) {
    if (active) {
      candidate_is_active_.Set(index);
    } else {
      candidate_is_active_.Clear(index);
    }
    MarkChange(index);
  }

  void Commit() {
    for (const int64_t index : changes_.PositionsSetAtLeastOnce()) {
      const int64_t value = candidate_values_[index];
      committed_values_[index] = value;
      if (index < max_inversible_index_) {
        committed_value_to_index_[value] = index;
      }
      committed_is_active_.CopyBucket(candidate_is_active_, index);
    }
    changes_.ResetAllToFalse();
    incremental_changes_.ResetAllToFalse();
  }

  void CheckPoint() { checkpoint_values_ = committed_values_; }

  void Revert(bool only_incremental) {
    incremental_changes_.ResetAllToFalse();
    if (only_incremental) return;

    for (const int64_t index : changes_.PositionsSetAtLeastOnce()) {
      const int64_t committed_value = committed_values_[index];
      candidate_values_[index] = committed_value;
      if (index < max_inversible_index_) {
        candidate_value_to_index_[committed_value] = index;
      }
      candidate_is_active_.CopyBucket(committed_is_active_, index);
    }
    changes_.ResetAllToFalse();
  }

  const std::vector<int64_t>& CandidateIndicesChanged() const {
    return changes_.PositionsSetAtLeastOnce();
  }
  const std::vector<int64_t>& IncrementalIndicesChanged() const {
    return incremental_changes_.PositionsSetAtLeastOnce();
  }

  void Resize(int size) {
    candidate_values_.resize(size);
    committed_values_.resize(size);
    checkpoint_values_.resize(size);
    candidate_is_active_.Resize(size);
    committed_is_active_.Resize(size);
    changes_.ClearAndResize(size);
    incremental_changes_.ClearAndResize(size);
  }

  int64_t CandidateInverseValue(int64_t value) const {
    return candidate_value_to_index_[value];
  }
  int64_t CommittedInverseValue(int64_t value) const {
    return committed_value_to_index_[value];
  }

 private:
  void MarkChange(int64_t index) {
    incremental_changes_.Set(index);
    changes_.Set(index);
  }

  std::vector<int64_t> candidate_values_;
  std::vector<int64_t> committed_values_;
  std::vector<int64_t> checkpoint_values_;

  Bitset64<> candidate_is_active_;
  Bitset64<> committed_is_active_;

  SparseBitset<> changes_;
  SparseBitset<> incremental_changes_;

  int64_t max_inversible_index_ = -1;
  std::vector<int64_t> candidate_value_to_index_;
  std::vector<int64_t> committed_value_to_index_;
};

/// Specialization of LocalSearchOperator built from an array of IntVars
/// which specifies the scope of the operator.
/// This class also takes care of storing current variable values in Start(),
/// keeps track of changes done by the operator and builds the delta.
/// The Deactivate() method can be used to perform Large Neighborhood Search.
class IntVarLocalSearchOperator : public LocalSearchOperator {
 public:
  // If keep_inverse_values is true, assumes that vars models an injective
  // function f with domain [0, vars.size()) in which case the operator will
  // maintain the inverse function.
  explicit IntVarLocalSearchOperator(const std::vector<IntVar*>& vars,
                                     bool keep_inverse_values = false) {
    AddVars(vars);
    if (keep_inverse_values) {
      int64_t max_value = -1;
      for (const IntVar* const var : vars) {
        max_value = std::max(max_value, var->Max());
      }
      state_.SetCurrentDomainInjectiveAndKeepInverseValues(max_value);
    }
  }
  ~IntVarLocalSearchOperator() override {}

  bool HoldsDelta() const override { return true; }
  /// This method should not be overridden. Override OnStart() instead which is
  /// called before exiting this method.
  void Start(const Assignment* assignment) override {
    state_.CheckPoint();
    RevertChanges(false);
    const int size = Size();
    CHECK_LE(size, assignment->Size())
        << "Assignment contains fewer variables than operator";
    const Assignment::IntContainer& container = assignment->IntVarContainer();
    for (int i = 0; i < size; ++i) {
      const IntVarElement* element = &(container.Element(i));
      if (element->Var() != vars_[i]) {
        CHECK(container.Contains(vars_[i]))
            << "Assignment does not contain operator variable " << vars_[i];
        element = &(container.Element(vars_[i]));
      }
      state_.SetCandidateValue(i, element->Value());
      state_.SetCandidateActive(i, element->Activated());
    }
    state_.Commit();
    OnStart();
  }
  virtual bool IsIncremental() const { return false; }

  int Size() const { return vars_.size(); }
  /// Returns the value in the current assignment of the variable of given
  /// index.
  int64_t Value(int64_t index) const {
    DCHECK_LT(index, vars_.size());
    return state_.CandidateValue(index);
  }
  /// Returns the variable of given index.
  IntVar* Var(int64_t index) const { return vars_[index]; }
  virtual bool SkipUnchanged(int) const { return false; }
  int64_t OldValue(int64_t index) const { return state_.CommittedValue(index); }
  int64_t PrevValue(int64_t index) const {
    return state_.CheckPointValue(index);
  }
  void SetValue(int64_t index, int64_t value) {
    state_.SetCandidateValue(index, value);
  }
  bool Activated(int64_t index) const {
    return state_.CandidateIsActive(index);
  }
  void Activate(int64_t index) { state_.SetCandidateActive(index, true); }
  void Deactivate(int64_t index) { state_.SetCandidateActive(index, false); }

  bool ApplyChanges(Assignment* delta, Assignment* deltadelta) const {
    if (IsIncremental() && candidate_has_changes_) {
      for (const int64_t index : state_.IncrementalIndicesChanged()) {
        IntVar* var = Var(index);
        const int64_t value = Value(index);
        const bool activated = Activated(index);
        AddToAssignment(var, value, activated, nullptr, index, deltadelta);
        AddToAssignment(var, value, activated, &assignment_indices_, index,
                        delta);
      }
    } else {
      delta->Clear();
      for (const int64_t index : state_.CandidateIndicesChanged()) {
        const int64_t value = Value(index);
        const bool activated = Activated(index);
        if (!activated || value != OldValue(index) || !SkipUnchanged(index)) {
          AddToAssignment(Var(index), value, activated, &assignment_indices_,
                          index, delta);
        }
      }
    }
    return true;
  }

  void RevertChanges(bool change_was_incremental) {
    candidate_has_changes_ = change_was_incremental && IsIncremental();

    if (!candidate_has_changes_) {
      for (const int64_t index : state_.CandidateIndicesChanged()) {
        assignment_indices_[index] = -1;
      }
    }
    state_.Revert(candidate_has_changes_);
  }

  void AddVars(const std::vector<IntVar*>& vars) {
    if (!vars.empty()) {
      vars_.insert(vars_.end(), vars.begin(), vars.end());
      const int64_t size = Size();
      assignment_indices_.resize(size, -1);
      state_.Resize(size);
    }
  }

  /// Called by Start() after synchronizing the operator with the current
  /// assignment. Should be overridden instead of Start() to avoid calling
  /// IntVarLocalSearchOperator::Start explicitly.
  virtual void OnStart() {}

  /// OnStart() should really be protected, but then SWIG doesn't see it. So we
  /// make it public, but only subclasses should access to it (to override it).

  /// Redefines MakeNextNeighbor to export a simpler interface. The calls to
  /// ApplyChanges() and RevertChanges() are factored in this method, hiding
  /// both delta and deltadelta from subclasses which only need to override
  /// MakeOneNeighbor().
  /// Therefore this method should not be overridden. Override MakeOneNeighbor()
  /// instead.
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

 protected:
  /// Creates a new neighbor. It returns false when the neighborhood is
  /// completely explored.
  // TODO(user): make it pure virtual, implies porting all apps overriding
  /// MakeNextNeighbor() in a subclass of IntVarLocalSearchOperator.
  virtual bool MakeOneNeighbor();

  int64_t InverseValue(int64_t index) const {
    return state_.CandidateInverseValue(index);
  }
  int64_t OldInverseValue(int64_t index) const {
    return state_.CommittedInverseValue(index);
  }

  void AddToAssignment(IntVar* var, int64_t value, bool active,
                       std::vector<int>* assignment_indices, int64_t index,
                       Assignment* assignment) const {
    Assignment::IntContainer* const container =
        assignment->MutableIntVarContainer();
    IntVarElement* element = nullptr;
    if (assignment_indices != nullptr) {
      if ((*assignment_indices)[index] == -1) {
        (*assignment_indices)[index] = container->Size();
        element = assignment->FastAdd(var);
      } else {
        element = container->MutableElement((*assignment_indices)[index]);
      }
    } else {
      element = assignment->FastAdd(var);
    }
    if (active) {
      element->SetValue(value);
      element->Activate();
    } else {
      element->Deactivate();
    }
  }

 private:
  std::vector<IntVar*> vars_;
  mutable std::vector<int> assignment_indices_;
  bool candidate_has_changes_ = false;

  LocalSearchOperatorState state_;
};

/// This is the base class for building an Lns operator. An Lns fragment is a
/// collection of variables which will be relaxed. Fragments are built with
/// NextFragment(), which returns false if there are no more fragments to build.
/// Optionally one can override InitFragments, which is called from
/// LocalSearchOperator::Start to initialize fragment data.
///
/// Here's a sample relaxing one variable at a time:
///
/// class OneVarLns : public BaseLns {
///  public:
///   OneVarLns(const std::vector<IntVar*>& vars) : BaseLns(vars), index_(0) {}
///   virtual ~OneVarLns() {}
///   virtual void InitFragments() { index_ = 0; }
///   virtual bool NextFragment() {
///     const int size = Size();
///     if (index_ < size) {
///       AppendToFragment(index_);
///       ++index_;
///       return true;
///     } else {
///       return false;
///     }
///   }
///
///  private:
///   int index_;
/// };
class BaseLns : public IntVarLocalSearchOperator {
 public:
  explicit BaseLns(const std::vector<IntVar*>& vars);
  ~BaseLns() override;
  virtual void InitFragments();
  virtual bool NextFragment() = 0;
  void AppendToFragment(int index);
  int FragmentSize() const;
  bool HasFragments() const override { return true; }

 protected:
  /// This method should not be overridden. Override NextFragment() instead.
  bool MakeOneNeighbor() override;

 private:
  /// This method should not be overridden. Override InitFragments() instead.
  void OnStart() override;
  std::vector<int> fragment_;
};

/// Defines operators which change the value of variables;
/// each neighbor corresponds to *one* modified variable.
/// Sub-classes have to define ModifyValue which determines what the new
/// variable value is going to be (given the current value and the variable).
class ChangeValue : public IntVarLocalSearchOperator {
 public:
  explicit ChangeValue(const std::vector<IntVar*>& vars);
  ~ChangeValue() override;
  virtual int64_t ModifyValue(int64_t index, int64_t value) = 0;

 protected:
  /// This method should not be overridden. Override ModifyValue() instead.
  bool MakeOneNeighbor() override;

 private:
  void OnStart() override;

  int index_;
};

// Iterators on nodes used by Pathoperator to traverse the search space.

class AlternativeNodeIterator {
 public:
  explicit AlternativeNodeIterator(bool use_sibling)
      : use_sibling_(use_sibling) {}
  ~AlternativeNodeIterator() {}
  template <class PathOperator>
  void Reset(const PathOperator& path_operator, int base_index_reference) {
    index_ = 0;
    DCHECK(path_operator.ConsiderAlternatives(base_index_reference));
    const int64_t base_node = path_operator.BaseNode(base_index_reference);
    const int alternative_index =
        use_sibling_ ? path_operator.GetSiblingAlternativeIndex(base_node)
                     : path_operator.GetAlternativeIndex(base_node);
    alternative_set_ =
        alternative_index >= 0
            ? absl::Span<const int64_t>(
                  path_operator.alternative_sets_[alternative_index])
            : absl::Span<const int64_t>();
  }
  bool Next() { return ++index_ < alternative_set_.size(); }
  int GetValue() const {
    return (index_ >= alternative_set_.size()) ? -1 : alternative_set_[index_];
  }

 private:
  const bool use_sibling_;
  int index_ = 0;
  absl::Span<const int64_t> alternative_set_;
};

class NodeNeighborIterator {
 public:
  NodeNeighborIterator() {}
  ~NodeNeighborIterator() {}
  template <class PathOperator>
  void Reset(const PathOperator& path_operator, int base_index_reference) {
    using Span = absl::Span<const int>;
    index_ = 0;
    const int64_t base_node = path_operator.BaseNode(base_index_reference);
    const int64_t start_node = path_operator.StartNode(base_index_reference);
    const auto& get_incoming_neighbors =
        path_operator.iteration_parameters_.get_incoming_neighbors;
    incoming_neighbors_ =
        path_operator.IsPathStart(base_node) || !get_incoming_neighbors
            ? Span()
            : Span(get_incoming_neighbors(base_node, start_node));
    const auto& get_outgoing_neighbors =
        path_operator.iteration_parameters_.get_outgoing_neighbors;
    outgoing_neighbors_ =
        path_operator.IsPathEnd(base_node) || !get_outgoing_neighbors
            ? Span()
            : Span(get_outgoing_neighbors(base_node, start_node));
  }
  bool Next() {
    return ++index_ < incoming_neighbors_.size() + outgoing_neighbors_.size();
  }
  int GetValue() const {
    if (index_ < incoming_neighbors_.size()) {
      return incoming_neighbors_[index_];
    }
    const int index = index_ - incoming_neighbors_.size();
    return (index >= outgoing_neighbors_.size()) ? -1
                                                 : outgoing_neighbors_[index];
  }
  bool IsIncomingNeighbor() const {
    return index_ < incoming_neighbors_.size();
  }
  bool IsOutgoingNeighbor() const {
    return index_ >= incoming_neighbors_.size();
  }

 private:
  int index_ = 0;
  absl::Span<const int> incoming_neighbors_;
  absl::Span<const int> outgoing_neighbors_;
};

template <class PathOperator>
class BaseNodeIterators {
 public:
  BaseNodeIterators(const PathOperator* path_operator, int base_index_reference)
      : path_operator_(*path_operator),
        base_index_reference_(base_index_reference) {}
  AlternativeNodeIterator* GetSiblingAlternativeIterator() const {
    DCHECK(!alternatives_.empty());
    DCHECK(!finished_);
    return alternatives_[0].get();
  }
  AlternativeNodeIterator* GetAlternativeIterator() const {
    DCHECK(!alternatives_.empty());
    DCHECK(!finished_);
    return alternatives_[1].get();
  }
  NodeNeighborIterator* GetNeighborIterator() const {
    DCHECK(neighbors_);
    DCHECK(!finished_);
    return neighbors_.get();
  }
  void Initialize() {
    if (path_operator_.ConsiderAlternatives(base_index_reference_)) {
      alternatives_.push_back(std::make_unique<AlternativeNodeIterator>(
          /*is_sibling=*/true));
      alternatives_.push_back(std::make_unique<AlternativeNodeIterator>(
          /*is_sibling=*/false));
    }
    if (path_operator_.HasNeighbors()) {
      neighbors_ = std::make_unique<NodeNeighborIterator>();
    }
  }
  void Reset(bool update_end_nodes = false) {
    finished_ = false;
    for (auto& alternative_iterator : alternatives_) {
      alternative_iterator->Reset(path_operator_, base_index_reference_);
    }
    if (neighbors_) {
      neighbors_->Reset(path_operator_, base_index_reference_);
      if (update_end_nodes) neighbor_end_node_ = neighbors_->GetValue();
    }
  }
  bool Increment() {
    DCHECK(!finished_);
    for (auto& alternative_iterator : alternatives_) {
      if (alternative_iterator->Next()) return true;
      alternative_iterator->Reset(path_operator_, base_index_reference_);
    }
    if (neighbors_) {
      if (neighbors_->Next()) return true;
      neighbors_->Reset(path_operator_, base_index_reference_);
    }
    finished_ = true;
    return false;
  }
  bool HasReachedEnd() const {
    // TODO(user): Extend to other iterators.
    if (!neighbors_) return true;
    return neighbor_end_node_ == neighbors_->GetValue();
  }

 private:
  const PathOperator& path_operator_;
  int base_index_reference_ = -1;
#ifndef SWIG
  std::vector<std::unique_ptr<AlternativeNodeIterator>> alternatives_;
#endif  // SWIG
  std::unique_ptr<NodeNeighborIterator> neighbors_;
  int neighbor_end_node_ = -1;
  bool finished_ = false;
};

/// Base class of the local search operators dedicated to path modifications
/// (a path is a set of nodes linked together by arcs).
/// This family of neighborhoods supposes they are handling next variables
/// representing the arcs (var[i] represents the node immediately after i on
/// a path).
/// Several services are provided:
/// - arc manipulators (SetNext(), ReverseChain(), MoveChain())
/// - path inspectors (Next(), Prev(), IsPathEnd())
/// - path iterators: operators need a given number of nodes to define a
///   neighbor; this class provides the iteration on a given number of (base)
///   nodes which can be used to define a neighbor (through the BaseNode method)
/// Subclasses only need to override MakeNeighbor to create neighbors using
/// the services above (no direct manipulation of assignments).
template <bool ignore_path_vars>
class PathOperator : public IntVarLocalSearchOperator {
 public:
  /// Set of parameters used to configure how the neighborhood is traversed.
  struct IterationParameters {
    /// Number of nodes needed to define a neighbor.
    int number_of_base_nodes;
    /// Skip paths which have been proven locally optimal. Note this might skip
    /// neighbors when paths are not independent.
    bool skip_locally_optimal_paths;
    /// True if path ends should be considered when iterating over neighbors.
    bool accept_path_end_base;
    /// Callback returning an index such that if
    /// c1 = start_empty_path_class(StartNode(p1)),
    /// c2 = start_empty_path_class(StartNode(p2)),
    /// p1 and p2 are path indices,
    /// then if c1 == c2, p1 and p2 are equivalent if they are empty.
    /// This is used to remove neighborhood symmetries on equivalent empty
    /// paths; for instance if a node cannot be moved to an empty path, then all
    /// moves moving the same node to equivalent empty paths will be skipped.
    /// 'start_empty_path_class' can be nullptr in which case no symmetries will
    /// be removed.
    std::function<int(int64_t)> start_empty_path_class;
    /// Callbacks returning incoming/outgoing neighbors of a node on a path
    /// starting at start_node.
    std::function<const std::vector<int>&(
        /*node=*/int, /*start_node=*/int)>
        get_incoming_neighbors;
    std::function<const std::vector<int>&(
        /*node=*/int, /*start_node=*/int)>
        get_outgoing_neighbors;
  };
  /// Builds an instance of PathOperator from next and path variables.
  PathOperator(const std::vector<IntVar*>& next_vars,
               const std::vector<IntVar*>& path_vars,
               IterationParameters iteration_parameters)
      : IntVarLocalSearchOperator(next_vars, true),
        number_of_nexts_(next_vars.size()),
        base_nodes_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        end_nodes_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        base_paths_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        node_path_starts_(number_of_nexts_, -1),
        node_path_ends_(number_of_nexts_, -1),
        just_started_(false),
        first_start_(true),
        next_base_to_increment_(iteration_parameters.number_of_base_nodes),
        iteration_parameters_(std::move(iteration_parameters)),
        optimal_paths_enabled_(false),
        active_paths_(number_of_nexts_),
        alternative_index_(next_vars.size(), -1) {
    DCHECK_GT(iteration_parameters_.number_of_base_nodes, 0);
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      base_node_iterators_.push_back(BaseNodeIterators<PathOperator>(this, i));
    }
    if constexpr (!ignore_path_vars) {
      AddVars(path_vars);
    }
    path_basis_.push_back(0);
    for (int i = 1; i < iteration_parameters_.number_of_base_nodes; ++i) {
      if (!OnSamePathAsPreviousBase(i)) path_basis_.push_back(i);
    }
    if ((path_basis_.size() > 2) ||
        (!next_vars.empty() && !next_vars.back()
                                    ->solver()
                                    ->parameters()
                                    .skip_locally_optimal_paths())) {
      iteration_parameters_.skip_locally_optimal_paths = false;
    }
  }
  PathOperator(
      const std::vector<IntVar*>& next_vars,
      const std::vector<IntVar*>& path_vars, int number_of_base_nodes,
      bool skip_locally_optimal_paths, bool accept_path_end_base,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
      std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors)
      : PathOperator(next_vars, path_vars,
                     {number_of_base_nodes, skip_locally_optimal_paths,
                      accept_path_end_base, std::move(start_empty_path_class),
                      std::move(get_incoming_neighbors),
                      std::move(get_outgoing_neighbors)}) {}
  ~PathOperator() override {}
  virtual bool MakeNeighbor() = 0;
  void EnterSearch() override {
    first_start_ = true;
    ResetIncrementalism();
  }
  void Reset() override {
    active_paths_.Clear();
    ResetIncrementalism();
  }

  // TODO(user): Make the following methods protected.
  bool SkipUnchanged(int index) const override {
    if constexpr (ignore_path_vars) return true;
    if (index < number_of_nexts_) {
      const int path_index = index + number_of_nexts_;
      return Value(path_index) == OldValue(path_index);
    }
    const int next_index = index - number_of_nexts_;
    return Value(next_index) == OldValue(next_index);
  }

  /// Returns the node after node in the current delta.
  int64_t Next(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return Value(node);
  }

  /// Returns the node before node in the current delta.
  int64_t Prev(int64_t node) const {
    DCHECK(!IsPathStart(node));
    DCHECK_EQ(Next(InverseValue(node)), node);
    return InverseValue(node);
  }

  /// Returns the index of the path to which node belongs in the current delta.
  /// Only returns a valid value if path variables are taken into account.
  int64_t Path(int64_t node) const {
    if constexpr (ignore_path_vars) return 0LL;
    return Value(node + number_of_nexts_);
  }

  /// Number of next variables.
  int number_of_nexts() const { return number_of_nexts_; }

 protected:
  /// This method should not be overridden. Override MakeNeighbor() instead.
  bool MakeOneNeighbor() override {
    while (IncrementPosition()) {
      // Need to revert changes here since MakeNeighbor might have returned
      // false and have done changes in the previous iteration.
      RevertChanges(true);
      if (MakeNeighbor()) {
        return true;
      }
    }
    return false;
  }

  /// Called by OnStart() after initializing node information. Should be
  /// overridden instead of OnStart() to avoid calling PathOperator::OnStart
  /// explicitly.
  virtual void OnNodeInitialization() {}
  /// When entering a new search or using metaheuristics, path operators
  /// reactivate optimal routes and iterating re-starts at route starts, which
  /// can potentially be out of sync with the last incremental moves.
  /// This requires resetting incrementalism.
  virtual void ResetIncrementalism() {}

  /// Returns the ith base node of the operator.
  int64_t BaseNode(int i) const { return base_nodes_[i]; }
  /// Returns the alternative node for the ith base node.
  int64_t BaseAlternativeNode(int i) const {
    return GetNodeWithDefault(base_node_iterators_[i].GetAlternativeIterator(),
                              BaseNode(i));
  }
  /// Returns the alternative node for the sibling of the ith base node.
  int64_t BaseSiblingAlternativeNode(int i) const {
    return GetNodeWithDefault(
        base_node_iterators_[i].GetSiblingAlternativeIterator(), BaseNode(i));
  }
  /// Returns the start node of the ith base node.
  int64_t StartNode(int i) const { return path_starts_[base_paths_[i]]; }
  /// Returns the end node of the ith base node.
  int64_t EndNode(int i) const { return path_ends_[base_paths_[i]]; }
  /// Returns the vector of path start nodes.
  const std::vector<int64_t>& path_starts() const { return path_starts_; }
  /// Returns the class of the path of the ith base node.
  int PathClass(int i) const { return PathClassFromStartNode(StartNode(i)); }
  int PathClassFromStartNode(int64_t start_node) const {
    return iteration_parameters_.start_empty_path_class != nullptr
               ? iteration_parameters_.start_empty_path_class(start_node)
               : start_node;
  }

  /// When the operator is being synchronized with a new solution (when Start()
  /// is called), returns true to restart the exploration of the neighborhood
  /// from the start of the last paths explored; returns false to restart the
  /// exploration at the last nodes visited.
  /// This is used to avoid restarting on base nodes which have changed paths,
  /// leading to potentially skipping neighbors.
  // TODO(user): remove this when automatic detection of such cases in done.
  virtual bool RestartAtPathStartOnSynchronize() { return false; }
  /// Returns true if a base node has to be on the same path as the "previous"
  /// base node (base node of index base_index - 1).
  /// Useful to limit neighborhood exploration to nodes on the same path.
  // TODO(user): ideally this should be OnSamePath(int64_t node1, int64_t
  // node2);
  /// it's currently way more complicated to implement.
  virtual bool OnSamePathAsPreviousBase(int64_t) { return false; }
  /// Returns the index of the node to which the base node of index base_index
  /// must be set to when it reaches the end of a path.
  /// By default, it is set to the start of the current path.
  /// When this method is called, one can only assume that base nodes with
  /// indices < base_index have their final position.
  virtual int64_t GetBaseNodeRestartPosition(int base_index) {
    return StartNode(base_index);
  }
  /// Set the next base to increment on next iteration. All base > base_index
  /// will be reset to their start value.
  virtual void SetNextBaseToIncrement(int64_t base_index) {
    next_base_to_increment_ = base_index;
  }
  /// Indicates if alternatives should be considered when iterating over base
  /// nodes.
  virtual bool ConsiderAlternatives(int64_t) const { return false; }

  int64_t OldNext(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return OldValue(node);
  }

  int64_t PrevNext(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return PrevValue(node);
  }

  int64_t OldPrev(int64_t node) const {
    DCHECK(!IsPathStart(node));
    return OldInverseValue(node);
  }

  int64_t OldPath(int64_t node) const {
    if constexpr (ignore_path_vars) return 0LL;
    return OldValue(node + number_of_nexts_);
  }

  int CurrentNodePathStart(int64_t node) const {
    return node_path_starts_[node];
  }

  int CurrentNodePathEnd(int64_t node) const { return node_path_ends_[node]; }

  /// Moves the chain starting after the node before_chain and ending at the
  /// node chain_end after the node destination
  bool MoveChain(int64_t before_chain, int64_t chain_end, int64_t destination) {
    if (destination == before_chain || destination == chain_end) return false;
    DCHECK(CheckChainValidity(before_chain, chain_end, destination) &&
           !IsPathEnd(chain_end) && !IsPathEnd(destination));
    const int64_t destination_path = Path(destination);
    const int64_t after_chain = Next(chain_end);
    SetNext(chain_end, Next(destination), destination_path);
    if constexpr (!ignore_path_vars) {
      int current = destination;
      int next = Next(before_chain);
      while (current != chain_end) {
        SetNext(current, next, destination_path);
        current = next;
        next = Next(next);
      }
    } else {
      SetNext(destination, Next(before_chain), destination_path);
    }
    SetNext(before_chain, after_chain, Path(before_chain));
    return true;
  }

  /// Reverses the chain starting after before_chain and ending before
  /// after_chain
  bool ReverseChain(int64_t before_chain, int64_t after_chain,
                    int64_t* chain_last) {
    if (CheckChainValidity(before_chain, after_chain, -1)) {
      int64_t path = Path(before_chain);
      int64_t current = Next(before_chain);
      if (current == after_chain) {
        return false;
      }
      int64_t current_next = Next(current);
      SetNext(current, after_chain, path);
      while (current_next != after_chain) {
        const int64_t next = Next(current_next);
        SetNext(current_next, current, path);
        current = current_next;
        current_next = next;
      }
      SetNext(before_chain, current, path);
      *chain_last = current;
      return true;
    }
    return false;
  }

  /// Swaps the nodes node1 and node2.
  bool SwapNodes(int64_t node1, int64_t node2) {
    if (IsPathEnd(node1) || IsPathEnd(node2) || IsPathStart(node1) ||
        IsPathStart(node2)) {
      return false;
    }
    if (node1 == node2) return false;
    const int64_t prev_node1 = Prev(node1);
    const bool ok = MoveChain(prev_node1, node1, Prev(node2));
    return MoveChain(Prev(node2), node2, prev_node1) || ok;
  }

  /// Insert the inactive node after destination.
  bool MakeActive(int64_t node, int64_t destination) {
    if (IsPathEnd(destination)) return false;
    const int64_t destination_path = Path(destination);
    SetNext(node, Next(destination), destination_path);
    SetNext(destination, node, destination_path);
    return true;
  }
  /// Makes the nodes on the chain starting after before_chain and ending at
  /// chain_end inactive.
  bool MakeChainInactive(int64_t before_chain, int64_t chain_end) {
    const int64_t kNoPath = -1;
    if (CheckChainValidity(before_chain, chain_end, -1) &&
        !IsPathEnd(chain_end)) {
      const int64_t after_chain = Next(chain_end);
      int64_t current = Next(before_chain);
      while (current != after_chain) {
        const int64_t next = Next(current);
        SetNext(current, current, kNoPath);
        current = next;
      }
      SetNext(before_chain, after_chain, Path(before_chain));
      return true;
    }
    return false;
  }

  /// Replaces active by inactive in the current path, making active inactive.
  bool SwapActiveAndInactive(int64_t active, int64_t inactive) {
    if (active == inactive) return false;
    const int64_t prev = Prev(active);
    return MakeChainInactive(prev, active) && MakeActive(inactive, prev);
  }
  /// Swaps both chains, making nodes on active_chain inactive and inserting
  /// active_chain at the position where inactive_chain was.
  bool SwapActiveAndInactiveChains(absl::Span<const int64_t> active_chain,
                                   absl::Span<const int64_t> inactive_chain) {
    if (active_chain.empty()) return false;
    if (active_chain == inactive_chain) return false;
    const int before_active_chain = Prev(active_chain.front());
    if (!MakeChainInactive(before_active_chain, active_chain.back())) {
      return false;
    }
    for (auto it = inactive_chain.crbegin(); it != inactive_chain.crend();
         ++it) {
      if (!MakeActive(*it, before_active_chain)) return false;
    }
    return true;
  }

  /// Sets 'to' to be the node after 'from' on the given path.
  void SetNext(int64_t from, int64_t to, int64_t path) {
    DCHECK_LT(from, number_of_nexts_);
    SetValue(from, to);
    if constexpr (!ignore_path_vars) {
      DCHECK_LT(from + number_of_nexts_, Size());
      SetValue(from + number_of_nexts_, path);
    }
  }

  /// Returns true if node is the last node on the path; defined by the fact
  /// that node is outside the range of the variable array.
  bool IsPathEnd(int64_t node) const { return node >= number_of_nexts_; }

  /// Returns true if node is the first node on the path.
  bool IsPathStart(int64_t node) const { return OldInverseValue(node) == -1; }

  /// Returns true if node is inactive.
  bool IsInactive(int64_t node) const {
    return !IsPathEnd(node) && inactives_[node];
  }

  /// Returns true if the operator needs to restart its initial position at each
  /// call to Start()
  virtual bool InitPosition() const { return false; }
  /// Reset the position of the operator to its position when Start() was last
  /// called; this can be used to let an operator iterate more than once over
  /// the paths.
  void ResetPosition() { just_started_ = true; }

  /// Handling node alternatives.
  /// Adds a set of node alternatives to the neighborhood. No node can be in
  /// two alternatives.
  int AddAlternativeSet(const std::vector<int64_t>& alternative_set) {
    const int alternative = alternative_sets_.size();
    for (int64_t node : alternative_set) {
      DCHECK_EQ(-1, alternative_index_[node]);
      alternative_index_[node] = alternative;
    }
    alternative_sets_.push_back(alternative_set);
    sibling_alternative_.push_back(-1);
    return alternative;
  }
#ifndef SWIG
  /// Adds all sets of node alternatives of a vector of alternative pairs. No
  /// node can be in two alternatives.
  template <typename PairType>
  void AddPairAlternativeSets(
      const std::vector<PairType>& pair_alternative_sets) {
    for (const auto& [alternative_set, sibling_alternative_set] :
         pair_alternative_sets) {
      sibling_alternative_.back() = AddAlternativeSet(alternative_set) + 1;
      AddAlternativeSet(sibling_alternative_set);
    }
  }
#endif  // SWIG
  /// Returns the active node in the given alternative set.
  int64_t GetActiveInAlternativeSet(int alternative_index) const {
    return alternative_index >= 0
               ? active_in_alternative_set_[alternative_index]
               : -1;
  }
  /// Returns the active node in the alternative set of the given node.
  int64_t GetActiveAlternativeNode(int node) const {
    return GetActiveInAlternativeSet(alternative_index_[node]);
  }
  /// Returns the index of the alternative set of the sibling of node.
  int GetSiblingAlternativeIndex(int node) const {
    const int alternative = GetAlternativeIndex(node);
    return alternative >= 0 ? sibling_alternative_[alternative] : -1;
  }
  /// Returns the index of the alternative set of the node.
  int GetAlternativeIndex(int node) const {
    return (node >= alternative_index_.size()) ? -1 : alternative_index_[node];
  }
  /// Returns the active node in the alternative set of the sibling of the given
  /// node.
  int64_t GetActiveAlternativeSibling(int node) const {
    return GetActiveInAlternativeSet(GetSiblingAlternativeIndex(node));
  }
  /// Returns true if the chain is a valid path without cycles from before_chain
  /// to chain_end and does not contain exclude.
  /// In particular, rejects a chain if chain_end is not strictly after
  /// before_chain on the path.
  /// Cycles are detected through chain length overflow.
  bool CheckChainValidity(int64_t before_chain, int64_t chain_end,
                          int64_t exclude) const {
    if (before_chain == chain_end || before_chain == exclude) return false;
    int64_t current = before_chain;
    int chain_size = 0;
    while (current != chain_end) {
      if (chain_size > number_of_nexts_) return false;
      if (IsPathEnd(current)) return false;
      current = Next(current);
      ++chain_size;
      if (current == exclude) return false;
    }
    return true;
  }

  bool HasNeighbors() const {
    return iteration_parameters_.get_incoming_neighbors != nullptr ||
           iteration_parameters_.get_outgoing_neighbors != nullptr;
  }

  struct Neighbor {
    // Index of the neighbor node.
    int neighbor;
    // True if 'neighbor' is an outgoing neighbor (i.e. arc main_node->neighbor)
    // and false if it's an incoming one (arc neighbor->main_node).
    bool outgoing;
  };
  Neighbor GetNeighborForBaseNode(int64_t base_index) const {
    auto* iterator = base_node_iterators_[base_index].GetNeighborIterator();
    return {.neighbor = iterator->GetValue(),
            .outgoing = iterator->IsOutgoingNeighbor()};
  }

  const int number_of_nexts_;

 private:
  template <class NodeIterator>
  static int GetNodeWithDefault(const NodeIterator* node_iterator,
                                int default_value) {
    const int node = node_iterator->GetValue();
    return node >= 0 ? node : default_value;
  }

  void OnStart() override {
    optimal_paths_enabled_ = false;
    if (!iterators_initialized_) {
      iterators_initialized_ = true;
      for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
        base_node_iterators_[i].Initialize();
      }
    }
    InitializeBaseNodes();
    InitializeAlternatives();
    OnNodeInitialization();
  }

  /// Returns true if two nodes are on the same path in the current assignment.
  bool OnSamePath(int64_t node1, int64_t node2) const {
    if (IsInactive(node1) != IsInactive(node2)) {
      return false;
    }
    for (int node = node1; !IsPathEnd(node); node = OldNext(node)) {
      if (node == node2) {
        return true;
      }
    }
    for (int node = node2; !IsPathEnd(node); node = OldNext(node)) {
      if (node == node1) {
        return true;
      }
    }
    return false;
  }

  bool CheckEnds() const {
    for (int i = iteration_parameters_.number_of_base_nodes - 1; i >= 0; --i) {
      if (base_nodes_[i] != end_nodes_[i] ||
          !base_node_iterators_[i].HasReachedEnd()) {
        return true;
      }
    }
    return false;
  }
  bool IncrementPosition() {
    const int base_node_size = iteration_parameters_.number_of_base_nodes;

    if (just_started_) {
      just_started_ = false;
      return true;
    }
    const int number_of_paths = path_starts_.size();
    // Finding next base node positions.
    // Increment the position of inner base nodes first (higher index nodes);
    // if a base node is at the end of a path, reposition it at the start
    // of the path and increment the position of the preceding base node (this
    // action is called a restart).
    int last_restarted = base_node_size;
    for (int i = base_node_size - 1; i >= 0; --i) {
      if (base_nodes_[i] < number_of_nexts_ && i <= next_base_to_increment_) {
        if (base_node_iterators_[i].Increment()) break;
        base_nodes_[i] = OldNext(base_nodes_[i]);
        base_node_iterators_[i].Reset();
        if (iteration_parameters_.accept_path_end_base ||
            !IsPathEnd(base_nodes_[i])) {
          break;
        }
      }
      base_nodes_[i] = StartNode(i);
      base_node_iterators_[i].Reset();
      last_restarted = i;
    }
    next_base_to_increment_ = base_node_size;
    // At the end of the loop, base nodes with indexes in
    // [last_restarted, base_node_size[ have been restarted.
    // Restarted base nodes are then repositioned by the virtual
    // GetBaseNodeRestartPosition to reflect position constraints between
    // base nodes (by default GetBaseNodeRestartPosition leaves the nodes
    // at the start of the path).
    // Base nodes are repositioned in ascending order to ensure that all
    // base nodes "below" the node being repositioned have their final
    // position.
    for (int i = last_restarted; i < base_node_size; ++i) {
      base_nodes_[i] = GetBaseNodeRestartPosition(i);
      base_node_iterators_[i].Reset();
    }
    if (last_restarted > 0) {
      return CheckEnds();
    }
    // If all base nodes have been restarted, base nodes are moved to new paths.
    // First we mark the current paths as locally optimal if they have been
    // completely explored.
    if (optimal_paths_enabled_ &&
        iteration_parameters_.skip_locally_optimal_paths) {
      if (path_basis_.size() > 1) {
        for (int i = 1; i < path_basis_.size(); ++i) {
          active_paths_.DeactivatePathPair(StartNode(path_basis_[i - 1]),
                                           StartNode(path_basis_[i]));
        }
      } else {
        active_paths_.DeactivatePathPair(StartNode(path_basis_[0]),
                                         StartNode(path_basis_[0]));
      }
    }
    std::vector<int> current_starts(base_node_size);
    for (int i = 0; i < base_node_size; ++i) {
      current_starts[i] = StartNode(i);
    }
    // Exploration of next paths can lead to locally optimal paths since we are
    // exploring them from scratch.
    optimal_paths_enabled_ = true;
    while (true) {
      for (int i = base_node_size - 1; i >= 0; --i) {
        const int next_path_index = base_paths_[i] + 1;
        if (next_path_index < number_of_paths) {
          base_paths_[i] = next_path_index;
          base_nodes_[i] = path_starts_[next_path_index];
          base_node_iterators_[i].Reset();
          if (i == 0 || !OnSamePathAsPreviousBase(i)) {
            break;
          }
        } else {
          base_paths_[i] = 0;
          base_nodes_[i] = path_starts_[0];
          base_node_iterators_[i].Reset();
        }
      }
      if (!iteration_parameters_.skip_locally_optimal_paths) return CheckEnds();
      // If the new paths have already been completely explored, we can
      // skip them from now on.
      if (path_basis_.size() > 1) {
        for (int j = 1; j < path_basis_.size(); ++j) {
          if (active_paths_.IsPathPairActive(StartNode(path_basis_[j - 1]),
                                             StartNode(path_basis_[j]))) {
            return CheckEnds();
          }
        }
      } else {
        if (active_paths_.IsPathPairActive(StartNode(path_basis_[0]),
                                           StartNode(path_basis_[0]))) {
          return CheckEnds();
        }
      }
      // If we are back to paths we just iterated on or have reached the end
      // of the neighborhood search space, we can stop.
      if (!CheckEnds()) return false;
      bool stop = true;
      for (int i = 0; i < base_node_size; ++i) {
        if (StartNode(i) != current_starts[i]) {
          stop = false;
          break;
        }
      }
      if (stop) return false;
    }
    return CheckEnds();
  }

  void InitializePathStarts() {
    // Detect nodes which do not have any possible predecessor in a path; these
    // nodes are path starts.
    int max_next = -1;
    std::vector<bool> has_prevs(number_of_nexts_, false);
    for (int i = 0; i < number_of_nexts_; ++i) {
      const int next = OldNext(i);
      if (next < number_of_nexts_) {
        has_prevs[next] = true;
      }
      max_next = std::max(max_next, next);
    }
    // Update locally optimal paths.
    if (iteration_parameters_.skip_locally_optimal_paths) {
      active_paths_.Initialize(
          /*is_start=*/[&has_prevs](int node) { return !has_prevs[node]; });
      for (int i = 0; i < number_of_nexts_; ++i) {
        if (!has_prevs[i]) {
          int current = i;
          while (!IsPathEnd(current)) {
            if ((OldNext(current) != PrevNext(current))) {
              active_paths_.ActivatePath(i);
              break;
            }
            current = OldNext(current);
          }
        }
      }
    }
    // Create a list of path starts, dropping equivalent path starts of
    // currently empty paths.
    std::vector<bool> empty_found(number_of_nexts_, false);
    std::vector<int64_t> new_path_starts;
    for (int i = 0; i < number_of_nexts_; ++i) {
      if (!has_prevs[i]) {
        if (IsPathEnd(OldNext(i))) {
          if (iteration_parameters_.start_empty_path_class != nullptr) {
            if (empty_found[iteration_parameters_.start_empty_path_class(i)])
              continue;
            empty_found[iteration_parameters_.start_empty_path_class(i)] = true;
          }
        }
        new_path_starts.push_back(i);
      }
    }
    if (!first_start_) {
      // Synchronizing base_paths_ with base node positions. When the last move
      // was performed a base node could have been moved to a new route in which
      // case base_paths_ needs to be updated. This needs to be done on the path
      // starts before we re-adjust base nodes for new path starts.
      std::vector<int> node_paths(max_next + 1, -1);
      for (int i = 0; i < path_starts_.size(); ++i) {
        int node = path_starts_[i];
        while (!IsPathEnd(node)) {
          node_paths[node] = i;
          node = OldNext(node);
        }
        node_paths[node] = i;
      }
      for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
        if (IsInactive(base_nodes_[j]) || node_paths[base_nodes_[j]] == -1) {
          // Base node was made inactive or was moved to a new path, reposition
          // the base node to its restart position.
          base_nodes_[j] = GetBaseNodeRestartPosition(j);
          base_paths_[j] = node_paths[base_nodes_[j]];
        } else {
          base_paths_[j] = node_paths[base_nodes_[j]];
        }
        // Always restart from first alternative.
        base_node_iterators_[j].Reset();
      }
      // Re-adjust current base_nodes and base_paths to take into account new
      // path starts (there could be fewer if a new path was made empty, or more
      // if nodes were added to a formerly empty path).
      int new_index = 0;
      absl::flat_hash_set<int> found_bases;
      for (int i = 0; i < path_starts_.size(); ++i) {
        int index = new_index;
        // Note: old and new path starts are sorted by construction.
        while (index < new_path_starts.size() &&
               new_path_starts[index] < path_starts_[i]) {
          ++index;
        }
        const bool found = (index < new_path_starts.size() &&
                            new_path_starts[index] == path_starts_[i]);
        if (found) {
          new_index = index;
        }
        for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
          if (base_paths_[j] == i && !found_bases.contains(j)) {
            found_bases.insert(j);
            base_paths_[j] = new_index;
            // If the current position of the base node is a removed empty path,
            // readjusting it to the last visited path start.
            if (!found) {
              base_nodes_[j] = new_path_starts[new_index];
            }
          }
        }
      }
    }
    path_starts_.swap(new_path_starts);
    // For every base path, store the end corresponding to the path start.
    // TODO(user): make this faster, maybe by pairing starts with ends.
    path_ends_.clear();
    path_ends_.reserve(path_starts_.size());
    int64_t max_node_index = number_of_nexts_ - 1;
    for (const int start_node : path_starts_) {
      int64_t node = start_node;
      while (!IsPathEnd(node)) node = OldNext(node);
      path_ends_.push_back(node);
      max_node_index = std::max(max_node_index, node);
    }
    node_path_starts_.assign(max_node_index + 1, -1);
    node_path_ends_.assign(max_node_index + 1, -1);
    for (int i = 0; i < path_starts_.size(); ++i) {
      const int64_t start_node = path_starts_[i];
      const int64_t end_node = path_ends_[i];
      int64_t node = start_node;
      while (!IsPathEnd(node)) {
        node_path_starts_[node] = start_node;
        node_path_ends_[node] = end_node;
        node = OldNext(node);
      }
      node_path_starts_[node] = start_node;
      node_path_ends_[node] = end_node;
    }
  }
  void InitializeInactives() {
    inactives_.clear();
    for (int i = 0; i < number_of_nexts_; ++i) {
      inactives_.push_back(OldNext(i) == i);
    }
  }
  void InitializeBaseNodes() {
    // Inactive nodes must be detected before determining new path starts.
    InitializeInactives();
    InitializePathStarts();
    if (first_start_ || InitPosition()) {
      // Only do this once since the following starts will continue from the
      // preceding position
      for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
        base_paths_[i] = 0;
        base_nodes_[i] = path_starts_[0];
      }
      first_start_ = false;
    }
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      // If base node has been made inactive, restart from path start.
      int64_t base_node = base_nodes_[i];
      if (RestartAtPathStartOnSynchronize() || IsInactive(base_node)) {
        base_node = path_starts_[base_paths_[i]];
        base_nodes_[i] = base_node;
      }
      end_nodes_[i] = base_node;
    }
    // Repair end_nodes_ in case some must be on the same path and are not
    // anymore (due to other operators moving these nodes).
    for (int i = 1; i < iteration_parameters_.number_of_base_nodes; ++i) {
      if (OnSamePathAsPreviousBase(i) &&
          !OnSamePath(base_nodes_[i - 1], base_nodes_[i])) {
        const int64_t base_node = base_nodes_[i - 1];
        base_nodes_[i] = base_node;
        end_nodes_[i] = base_node;
        base_paths_[i] = base_paths_[i - 1];
      }
    }
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      base_node_iterators_[i].Reset(/*update_end_nodes=*/true);
    }
    just_started_ = true;
  }
  void InitializeAlternatives() {
    active_in_alternative_set_.resize(alternative_sets_.size(), -1);
    for (int i = 0; i < alternative_sets_.size(); ++i) {
      const int64_t current_active = active_in_alternative_set_[i];
      if (current_active >= 0 && !IsInactive(current_active)) continue;
      for (int64_t index : alternative_sets_[i]) {
        if (!IsInactive(index)) {
          active_in_alternative_set_[i] = index;
          break;
        }
      }
    }
  }

  class ActivePaths {
   public:
    explicit ActivePaths(int num_nodes) : start_to_path_(num_nodes, -1) {}
    void Clear() { to_reset_ = true; }
    template <typename T>
    void Initialize(T is_start) {
      if (is_path_pair_active_.empty()) {
        num_paths_ = 0;
        absl::c_fill(start_to_path_, -1);
        for (int i = 0; i < start_to_path_.size(); ++i) {
          if (is_start(i)) {
            start_to_path_[i] = num_paths_;
            ++num_paths_;
          }
        }
      }
    }
    void DeactivatePathPair(int start1, int start2) {
      if (to_reset_) Reset();
      is_path_pair_active_[start_to_path_[start1] * num_paths_ +
                           start_to_path_[start2]] = false;
    }
    void ActivatePath(int start) {
      if (to_reset_) Reset();
      const int p1 = start_to_path_[start];
      const int p1_block = num_paths_ * p1;
      for (int p2 = 0; p2 < num_paths_; ++p2) {
        is_path_pair_active_[p1_block + p2] = true;
      }
      for (int p2_block = 0; p2_block < is_path_pair_active_.size();
           p2_block += num_paths_) {
        is_path_pair_active_[p2_block + p1] = true;
      }
    }
    bool IsPathPairActive(int start1, int start2) const {
      if (to_reset_) return true;
      return is_path_pair_active_[start_to_path_[start1] * num_paths_ +
                                  start_to_path_[start2]];
    }

   private:
    void Reset() {
      if (!to_reset_) return;
      is_path_pair_active_.assign(num_paths_ * num_paths_, true);
      to_reset_ = false;
    }

    bool to_reset_ = true;
    int num_paths_ = 0;
    std::vector<int64_t> start_to_path_;
    std::vector<bool> is_path_pair_active_;
  };

  std::unique_ptr<int[]> base_nodes_;
  std::unique_ptr<int[]> end_nodes_;
  std::unique_ptr<int[]> base_paths_;
  std::vector<int> node_path_starts_;
  std::vector<int> node_path_ends_;
  bool iterators_initialized_ = false;
#ifndef SWIG
  std::vector<BaseNodeIterators<PathOperator>> base_node_iterators_;
#endif  // SWIG
  std::vector<int64_t> path_starts_;
  std::vector<int64_t> path_ends_;
  std::vector<bool> inactives_;
  bool just_started_;
  bool first_start_;
  int next_base_to_increment_;
  IterationParameters iteration_parameters_;
  bool optimal_paths_enabled_;
  std::vector<int> path_basis_;
  ActivePaths active_paths_;
  /// Node alternative data.
#ifndef SWIG
  std::vector<std::vector<int64_t>> alternative_sets_;
#endif  // SWIG
  std::vector<int> alternative_index_;
  std::vector<int64_t> active_in_alternative_set_;
  std::vector<int> sibling_alternative_;

  friend class BaseNodeIterators<PathOperator>;
  friend class AlternativeNodeIterator;
  friend class NodeNeighborIterator;
};

#ifndef SWIG
/// ----- 2Opt -----

/// Reverses a sub-chain of a path. It is called 2Opt because it breaks
/// 2 arcs on the path; resulting paths are called 2-optimal.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
/// (where (1, 5) are first and last nodes of the path and can therefore not be
/// moved):
/// 1 -> 3 -> 2 -> 4 -> 5
/// 1 -> 4 -> 3 -> 2 -> 5
/// 1 -> 2 -> 4 -> 3 -> 5

LocalSearchOperator* MakeTwoOpt(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- Relocate -----

/// Moves a sub-chain of a path to another position; the specified chain length
/// is the fixed length of the chains being moved. When this length is 1 the
/// operator simply moves a node to another position.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5, for a chain length
/// of 2 (where (1, 5) are first and last nodes of the path and can
/// therefore not be moved):
/// 1 -> 4 -> 2 -> 3 -> 5
/// 1 -> 3 -> 4 -> 2 -> 5
///
/// Using Relocate with chain lengths of 1, 2 and 3 together is equivalent to
/// the OrOpt operator on a path. The OrOpt operator is a limited version of
/// 3Opt (breaks 3 arcs on a path).

LocalSearchOperator* MakeRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr,
    int64_t chain_length = 1LL, bool single_path = false,
    const std::string& name = "Relocate");

/// ----- Exchange -----

/// Exchanges the positions of two nodes.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
/// (where (1, 5) are first and last nodes of the path and can therefore not
/// be moved):
/// 1 -> 3 -> 2 -> 4 -> 5
/// 1 -> 4 -> 3 -> 2 -> 5
/// 1 -> 2 -> 4 -> 3 -> 5

LocalSearchOperator* MakeExchange(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- Cross -----

/// Cross echanges the starting chains of 2 paths, including exchanging the
/// whole paths.
/// First and last nodes are not moved.
/// Possible neighbors for the paths 1 -> 2 -> 3 -> 4 -> 5 and 6 -> 7 -> 8
/// (where (1, 5) and (6, 8) are first and last nodes of the paths and can
/// therefore not be moved):
/// 1 -> 7 -> 3 -> 4 -> 5  6 -> 2 -> 8
/// 1 -> 7 -> 4 -> 5       6 -> 2 -> 3 -> 8
/// 1 -> 7 -> 5            6 -> 2 -> 3 -> 4 -> 8

LocalSearchOperator* MakeCross(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- MakeActive -----

/// MakeActive inserts an inactive node into a path.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 2 -> 3 -> 4
/// 1 -> 2 -> 5 -> 3 -> 4
/// 1 -> 2 -> 3 -> 5 -> 4

LocalSearchOperator* MakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ---- RelocateAndMakeActive -----

/// RelocateAndMakeActive relocates a node and replaces it by an inactive node.
/// The idea is to make room for inactive nodes.
/// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
/// 0 -> 2 -> 4, 1 -> 3 -> 5.
/// TODO(user): Naming is close to MakeActiveAndRelocate but this one is
/// correct; rename MakeActiveAndRelocate if it is actually used.

LocalSearchOperator* RelocateAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

// ----- ExchangeAndMakeActive -----

// ExchangeAndMakeActive exchanges two nodes and inserts an inactive node.
// Possible neighbors for paths 0 -> 2 -> 4, 1 -> 3 -> 6 and 5 inactive are:
// 0 -> 3 -> 4, 1 -> 5 -> 2 -> 6
// 0 -> 3 -> 5 -> 4, 1 -> 2 -> 6
// 0 -> 5 -> 3 -> 4, 1 -> 2 -> 6
// 0 -> 3 -> 4, 1 -> 2 -> 5 -> 6
//
// Warning this operator creates a very large neighborhood, with O(m*n^3)
// neighbors (n: number of active nodes, m: number of non active nodes).
// It should be used with only a small number of non active nodes.
// TODO(user): Add support for neighbors which would make this operator more
// usable.

LocalSearchOperator* ExchangeAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

// ----- ExchangePathEndsAndMakeActive -----

// An operator which exchanges the first and last nodes of two paths and makes a
// node active.
// Possible neighbors for paths 0 -> 1 -> 2 -> 7, 6 -> 3 -> 4 -> 8
// and 5 inactive are:
// 0 -> 5 -> 3 -> 4 -> 7, 6 -> 1 -> 2 -> 8
// 0 -> 3 -> 4 -> 7, 6 -> 1 -> 5 -> 2 -> 8
// 0 -> 3 -> 4 -> 7, 6 -> 1 -> 2 -> 5 -> 8
// 0 -> 3 -> 5 -> 4 -> 7, 6 -> 1 -> 2 -> 8
// 0 -> 3 -> 4 -> 5 -> 7, 6 -> 1 -> 2 -> 8
//
// This neighborhood is an artificially reduced version of
// ExchangeAndMakeActiveOperator. It can still be used to opportunistically
// insert inactive nodes.

LocalSearchOperator* ExchangePathStartEndsAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeActiveAndRelocate -----

/// MakeActiveAndRelocate makes a node active next to a node being relocated.
/// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
/// 0 -> 3 -> 2 -> 4, 1 -> 5.

LocalSearchOperator* MakeActiveAndRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeInactive -----

/// MakeInactive makes path nodes inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
/// and last nodes of the path) are:
/// 1 -> 3 -> 4 & 2 inactive
/// 1 -> 2 -> 4 & 3 inactive

LocalSearchOperator* MakeInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- RelocateAndMakeInactive -----

/// RelocateAndMakeInactive relocates a node to a new position and makes the
/// node which was at that position inactive.
/// Possible neighbors for paths 0 -> 2 -> 4, 1 -> 3 -> 5 are:
/// 0 -> 3 -> 4, 1 -> 5 & 2 inactive
/// 0 -> 4, 1 -> 2 -> 5 & 3 inactive

LocalSearchOperator* RelocateAndMakeInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeChainInactive -----

/// Operator which makes a "chain" of path nodes inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
/// and last nodes of the path) are:
///   1 -> 3 -> 4 with 2 inactive
///   1 -> 2 -> 4 with 3 inactive
///   1 -> 4 with 2 and 3 inactive

LocalSearchOperator* MakeChainInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- SwapActive -----

/// SwapActive replaces an active node by an inactive one.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 3 -> 4 & 2 inactive
/// 1 -> 2 -> 5 -> 4 & 3 inactive

LocalSearchOperator* MakeSwapActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- SwapActiveChain -----

LocalSearchOperator* MakeSwapActiveChain(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, int max_chain_size);

/// ----- ExtendedSwapActive -----

/// ExtendedSwapActive makes an inactive node active and an active one
/// inactive. It is similar to SwapActiveOperator excepts that it tries to
/// insert the inactive node in all possible positions instead of just the
/// position of the node made inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 3 -> 4 & 2 inactive
/// 1 -> 3 -> 5 -> 4 & 2 inactive
/// 1 -> 5 -> 2 -> 4 & 3 inactive
/// 1 -> 2 -> 5 -> 4 & 3 inactive

LocalSearchOperator* MakeExtendedSwapActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- TSP-based operators -----

/// Sliding TSP operator
/// Uses an exact dynamic programming algorithm to solve the TSP corresponding
/// to path sub-chains.
/// For a subchain 1 -> 2 -> 3 -> 4 -> 5 -> 6, solves the TSP on nodes A, 2, 3,
/// 4, 5, where A is a merger of nodes 1 and 6 such that cost(A,i) = cost(1,i)
/// and cost(i,A) = cost(i,6).

LocalSearchOperator* MakeTSPOpt(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<IntVar*>& secondary_vars,
                                Solver::IndexEvaluator3 evaluator,
                                int chain_length);

/// TSP-base lns.
/// Randomly merge consecutive nodes until n "meta"-nodes remain and solve the
/// corresponding TSP. This can be seen as a large neighborhood search operator
/// although decisions are taken with the operator.
/// This is an "unlimited" neighborhood which must be stopped by search limits.
/// To force diversification, the operator iteratively forces each node to serve
/// as base of a meta-node.

LocalSearchOperator* MakeTSPLns(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<IntVar*>& secondary_vars,
                                Solver::IndexEvaluator3 evaluator,
                                int tsp_size);

/// ----- Lin-Kernighan -----

LocalSearchOperator* MakeLinKernighan(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    const Solver::IndexEvaluator3& evaluator, bool topt);

/// ----- Path-based Large Neighborhood Search -----

/// Breaks "number_of_chunks" chains of "chunk_size" arcs, and deactivate all
/// inactive nodes if "unactive_fragments" is true.
/// As a special case, if chunk_size=0, then we break full paths.

LocalSearchOperator* MakePathLns(Solver* solver,
                                 const std::vector<IntVar*>& vars,
                                 const std::vector<IntVar*>& secondary_vars,
                                 int number_of_chunks, int chunk_size,
                                 bool unactive_fragments);
#endif  // SWIG

#if !defined(SWIG)
// After building a Directed Acyclic Graph, allows to generate sub-DAGs
// reachable from a node.
// Workflow:
// - Call AddArc() repeatedly to add arcs describing a DAG. Nodes are allocated
//   on the user side, they must be nonnegative, and it is better but not
//   mandatory for the set of nodes to be dense.
// - Call BuildGraph(). This precomputes all the information needed to make
//   subsequent requests for sub-DAGs.
// - Call ComputeSortedSubDagArcs(node). This returns a view to arcs reachable
//   from node, in topological order.
// All arcs must be added before calling BuildGraph(),
// and ComputeSortedSubDagArcs() can only be called after BuildGraph().
// If the arcs form a graph that has directed cycles,
// - in debug mode, BuildGraph() will crash.
// - otherwise, BuildGraph() will not crash, but ComputeSortedSubDagArcs()
//   will only return a subset of arcs reachable by the given node.
class SubDagComputer {
 public:
  DEFINE_STRONG_INT_TYPE(ArcId, int);
  DEFINE_STRONG_INT_TYPE(NodeId, int);
  SubDagComputer() = default;
  // Adds an arc between node 'tail' and node 'head'. Nodes must be nonnegative.
  // Returns the index of the new arc, those are used to identify arcs when
  // calling ComputeSortedSubDagArcs().
  ArcId AddArc(NodeId tail, NodeId head) {
    DCHECK(!graph_was_built_);
    num_nodes_ = std::max(num_nodes_, std::max(tail.value(), head.value()) + 1);
    const ArcId arc_id(arcs_.size());
    arcs_.push_back({.tail = tail, .head = head, .arc_id = arc_id});
    return arc_id;
  }
  // Finishes the construction of the DAG.  'num_nodes' is the number of nodes
  // in the DAG and must be greater than all node indices passed to AddArc().
  void BuildGraph(int num_nodes);
  // Computes the arcs of the sub-DAG reachable from node returns a view of
  // those arcs in topological order.
  const std::vector<ArcId>& ComputeSortedSubDagArcs(NodeId node);

 private:
  // Checks whether the underlying graph has a directed cycle.
  // Should be called after the graph has been built.
  bool HasDirectedCycle() const;

  struct Arc {
    NodeId tail;
    NodeId head;
    ArcId arc_id;
    bool operator<(const Arc& other) const {
      return std::tie(tail, arc_id) < std::tie(other.tail, other.arc_id);
    }
  };
  int num_nodes_ = 0;
  std::vector<Arc> arcs_;
  // Initialized by BuildGraph(), after which the outgoing arcs of node n are
  // the range from arcs_[arcs_of_node_[n]] included to
  // arcs_[arcs_of_node_[n+1]] excluded.
  util_intops::StrongVector<NodeId, int> arcs_of_node_;
  // Must be false before BuildGraph() is called, true afterwards.
  bool graph_was_built_ = false;
  // Used by ComputeSortedSubDagArcs.
  util_intops::StrongVector<NodeId, int> indegree_of_node_;
  // Used by ComputeSortedSubDagArcs.
  std::vector<NodeId> nodes_to_visit_;
  // Used as output, set up as a member to allow reuse.
  std::vector<ArcId> sorted_arcs_;
};

// A LocalSearchState is a container for variables with domains that can be
// relaxed and tightened, saved and restored. It represents the solution state
// of a local search engine, and allows it to go from solution to solution by
// relaxing some variables to form a new subproblem, then tightening those
// variables to move to a new solution representation. That state may be saved
// to an internal copy, or reverted to the last saved internal copy.
// Relaxing a variable returns its bounds to their initial state.
// Tightening a variable's bounds may make its min larger than its max,
// in that case, the tightening function will return false, and the state will
// be marked as invalid. No other operations than Revert() can be called on an
// invalid state: in particular, an invalid state cannot be saved.
class LocalSearchState {
 public:
  class Variable;
  DEFINE_STRONG_INT_TYPE(VariableDomainId, int);
  DEFINE_STRONG_INT_TYPE(ConstraintId, int);
  // Adds a variable domain to this state, returns a handler to the new domain.
  VariableDomainId AddVariableDomain(int64_t relaxed_min, int64_t relaxed_max);
  // Relaxes the domain, returns false iff the domain was already relaxed.
  bool RelaxVariableDomain(VariableDomainId domain_id);
  bool TightenVariableDomainMin(VariableDomainId domain_id, int64_t value);
  bool TightenVariableDomainMax(VariableDomainId domain_id, int64_t value);
  int64_t VariableDomainMin(VariableDomainId domain_id) const;
  int64_t VariableDomainMax(VariableDomainId domain_id) const;
  void ChangeRelaxedVariableDomain(VariableDomainId domain_id, int64_t min,
                                   int64_t max);

  // Propagation of all events.
  void PropagateRelax(VariableDomainId domain_id);
  bool PropagateTighten(VariableDomainId domain_id);
  // Makes a variable, an object with restricted operations on the underlying
  // domain identified by domain_id: only Relax, Tighten and Min/Max read
  // operations are available.
  Variable MakeVariable(VariableDomainId domain_id);
  // Makes a variable from an interval without going through a domain_id.
  // Can be used when no direct manipulation of the domain is needed.
  Variable MakeVariableWithRelaxedDomain(int64_t min, int64_t max);
  // Makes a variable with no state, this is meant as a placeholder.
  static Variable DummyVariable();
  void Commit();
  void Revert();
  bool StateIsFeasible() const {
    return state_domains_are_all_nonempty_ && num_committed_empty_domains_ == 0;
  }
  // Adds a constraint that output = input_offset + sum_i weight_i * input_i.
  void AddWeightedSumConstraint(
      const std::vector<VariableDomainId>& input_domain_ids,
      const std::vector<int64_t>& input_weights, int64_t input_offset,
      VariableDomainId output_domain_id);
  // Precomputes which domain change triggers which constraint(s).
  // Should be run after adding all constraints, before any Relax()/Tighten().
  void CompileConstraints();

 private:
  // VariableDomains implement the domain of Variables.
  // Those are trailed, meaning they are saved on their first modification,
  // and can be reverted or committed in O(1) per modification.
  struct VariableDomain {
    int64_t min;
    int64_t max;
  };
  bool IntersectionIsEmpty(const VariableDomain& d1,
                           const VariableDomain& d2) const {
    return d1.max < d2.min || d2.max < d1.min;
  }
  util_intops::StrongVector<VariableDomainId, VariableDomain> relaxed_domains_;
  util_intops::StrongVector<VariableDomainId, VariableDomain> current_domains_;
  struct TrailedVariableDomain {
    VariableDomain committed_domain;
    VariableDomainId domain_id;
  };
  std::vector<TrailedVariableDomain> trailed_domains_;
  util_intops::StrongVector<VariableDomainId, bool> domain_is_trailed_;
  // True iff all domains have their min <= max.
  bool state_domains_are_all_nonempty_ = true;
  bool state_has_relaxed_domains_ = false;
  // Number of domains d for which the intersection of
  // current_domains_[d] and relaxed_domains_[d] is empty.
  int num_committed_empty_domains_ = 0;
  int trailed_num_committed_empty_domains_ = 0;

  // Constraints may be trailed too, they decide how to track their internal
  // structure.
  class Constraint;
  void TrailConstraint(Constraint* constraint) {
    trailed_constraints_.push_back(constraint);
  }
  std::vector<Constraint*> trailed_constraints_;

  // Stores domain-constraint dependencies, allows to generate topological
  // orderings of dependency arcs reachable from nodes.
  class DependencyGraph {
   public:
    DependencyGraph() {}
    // There are two kinds of domain-constraint dependencies:
    // - domain -> constraint when the domain is an input to the constraint.
    //   Then the label is the index of the domain in the input tuple.
    // - constraint -> domain when the domain is the output of the constraint.
    //   Then, the label is -1.
    struct Dependency {
      VariableDomainId domain_id;
      int input_index;
      ConstraintId constraint_id;
    };
    // Adds all dependencies domains[i] -> constraint labelled by i.
    void AddDomainsConstraintDependencies(
        const std::vector<VariableDomainId>& domain_ids,
        ConstraintId constraint_id);
    // Adds a dependency domain -> constraint labelled by -1.
    void AddConstraintDomainDependency(ConstraintId constraint_id,
                                       VariableDomainId domain_id);
    // After all dependencies have been added,
    // builds the DAG representation that allows to compute sorted dependencies.
    void BuildDependencyDAG(int num_domains);
    // Returns a view on the list of arc dependencies reachable by given domain,
    // in some topological order of the overall DAG. Modifying the graph or
    // calling ComputeSortedDependencies() again invalidates the view.
    const std::vector<Dependency>& ComputeSortedDependencies(
        VariableDomainId domain_id);

   private:
    using ArcId = SubDagComputer::ArcId;
    using NodeId = SubDagComputer::NodeId;
    // Returns dag_node_of_domain_[domain_id] if already defined,
    // or makes a node for domain_id, possibly extending dag_node_of_domain_.
    NodeId GetOrCreateNodeOfDomainId(VariableDomainId domain_id);
    // Returns dag_node_of_constraint_[constraint_id] if already defined,
    // or makes a node for constraint_id, possibly extending
    // dag_node_of_constraint_.
    NodeId GetOrCreateNodeOfConstraintId(ConstraintId constraint_id);
    // Structure of the expression DAG, used to buffer propagation storage.
    SubDagComputer dag_;
    // Maps arcs of dag_ to domain/constraint dependencies.
    util_intops::StrongVector<ArcId, Dependency> dependency_of_dag_arc_;
    // Maps domain ids to dag_ nodes.
    util_intops::StrongVector<VariableDomainId, NodeId> dag_node_of_domain_;
    // Maps constraint ids to dag_ nodes.
    util_intops::StrongVector<ConstraintId, NodeId> dag_node_of_constraint_;
    // Number of nodes currently allocated in dag_.
    // Reserve node 0 as a default dummy node with no dependencies.
    int num_dag_nodes_ = 1;
    // Used as reusable output of ComputeSortedDependencies().
    std::vector<Dependency> sorted_dependencies_;
  };
  DependencyGraph dependency_graph_;

  // Propagation order storage: each domain change triggers constraints.
  // Each trigger tells a constraint that a domain changed, and identifies
  // the domain by the index in the list of the constraint's inputs.
  struct Trigger {
    Constraint* constraint;
    int input_index;
  };
  // Triggers of all constraints, concatenated.
  // The triggers of domain i are stored from triggers_of_domain_[i]
  // to triggers_of_domain_[i+1] excluded.
  std::vector<Trigger> triggers_;
  util_intops::StrongVector<VariableDomainId, int> triggers_of_domain_;

  // Constraints are used to form expressions that make up the objective.
  // Constraints are directed: they have inputs and an output, moreover the
  // constraint-domain graph must not have directed cycles.
  class Constraint {
   public:
    virtual ~Constraint() = default;
    virtual LocalSearchState::VariableDomain Propagate(int input_index) = 0;
    virtual VariableDomainId Output() const = 0;
    virtual void Commit() = 0;
    virtual void Revert() = 0;
  };
  // WeightedSum constraints enforces the equation:
  //   output = offset + sum_i input_weights[i] * input_domain_ids[i]
  class WeightedSum final : public Constraint {
   public:
    WeightedSum(LocalSearchState* state,
                const std::vector<VariableDomainId>& input_domain_ids,
                const std::vector<int64_t>& input_weights, int64_t input_offset,
                VariableDomainId output);
    ~WeightedSum() override = default;
    LocalSearchState::VariableDomain Propagate(int input_index) override;
    void Commit() override;
    void Revert() override;
    VariableDomainId Output() const override { return output_; }

   private:
    // Weighted variable holds a variable's domain, an associated weight,
    // and the variable's last known min and max.
    struct WeightedVariable {
      int64_t min;
      int64_t max;
      int64_t committed_min;
      int64_t committed_max;
      int64_t weight;
      VariableDomainId domain;
      bool is_trailed;
      void Commit() {
        committed_min = min;
        committed_max = max;
        is_trailed = false;
      }
      void Revert() {
        min = committed_min;
        max = committed_max;
        is_trailed = false;
      }
    };
    std::vector<WeightedVariable> inputs_;
    std::vector<WeightedVariable*> trailed_inputs_;
    // Invariants held by this constraint to allow O(1) propagation.
    struct Invariants {
      // Number of inputs_[i].min equal to kint64min.
      int64_t num_neg_inf;
      // Sum of inputs_[i].min that are different from kint64min.
      int64_t wsum_mins;
      // Number of inputs_[i].max equal to kint64max.
      int64_t num_pos_inf;
      // Sum of inputs_[i].max that are different from kint64max.
      int64_t wsum_maxs;
    };
    Invariants invariants_;
    Invariants committed_invariants_;

    const VariableDomainId output_;
    LocalSearchState* const state_;
    bool constraint_is_trailed_ = false;
  };
  // Used to identify constraints and hold ownership.
  util_intops::StrongVector<ConstraintId, std::unique_ptr<Constraint>>
      constraints_;
};

// A LocalSearchState Variable can only be created by a LocalSearchState,
// then it is meant to be passed by copy. If at some point the duplication of
// LocalSearchState pointers is too expensive, we could switch to index only,
// and the user would have to know the relevant state. The present setup allows
// to ensure that variable users will not misuse the state.
class LocalSearchState::Variable {
 public:
  Variable() : state_(nullptr), domain_id_(VariableDomainId(-1)) {}
  int64_t Min() const {
    DCHECK(Exists());
    return state_->VariableDomainMin(domain_id_);
  }
  int64_t Max() const {
    DCHECK(Exists());
    return state_->VariableDomainMax(domain_id_);
  }
  // Sets variable's minimum to max(Min(), new_min) and propagates the change.
  // Returns true iff the variable domain is nonempty and propagation succeeded.
  bool SetMin(int64_t new_min) const {
    if (!Exists()) return true;
    return state_->TightenVariableDomainMin(domain_id_, new_min) &&
           state_->PropagateTighten(domain_id_);
  }
  // Sets variable's maximum to min(Max(), new_max) and propagates the change.
  // Returns true iff the variable domain is nonempty and propagation succeeded.
  bool SetMax(int64_t new_max) const {
    if (!Exists()) return true;
    return state_->TightenVariableDomainMax(domain_id_, new_max) &&
           state_->PropagateTighten(domain_id_);
  }
  void Relax() const {
    if (state_ == nullptr) return;
    if (state_->RelaxVariableDomain(domain_id_)) {
      state_->PropagateRelax(domain_id_);
    }
  }
  bool Exists() const { return state_ != nullptr; }

 private:
  // Only LocalSearchState can construct LocalSearchVariables.
  friend class LocalSearchState;

  Variable(LocalSearchState* state, VariableDomainId domain_id)
      : state_(state), domain_id_(domain_id) {}

  LocalSearchState* state_;
  VariableDomainId domain_id_;
};
#endif  // !defined(SWIG)

/// Local Search Filters are used for fast neighbor pruning.
/// Filtering a move is done in several phases:
/// - in the Relax phase, filters determine which parts of their internals
///   will be changed by the candidate, and modify intermediary State
/// - in the Accept phase, filters check that the candidate is feasible,
/// - if the Accept phase succeeds, the solver may decide to trigger a
///   Synchronize phase that makes filters change their internal representation
///   to the last candidate,
/// - otherwise (Accept fails or the solver does not want to synchronize),
///   a Revert phase makes filters erase any intermediary State generated by the
///   Relax and Accept phases.
/// A given filter has phases called with the following pattern:
/// (Relax.Accept.Synchronize | Relax.Accept.Revert | Relax.Revert)*.
/// Filters's Revert() is always called in the reverse order their Accept() was
/// called, to allow late filters to use state done/undone by early filters'
/// Accept()/Revert().
class LocalSearchFilter : public BaseObject {
 public:
  /// Lets the filter know what delta and deltadelta will be passed in the next
  /// Accept().
  virtual void Relax(const Assignment*, const Assignment*) {}
  /// Dual of Relax(), lets the filter know that the delta was accepted.
  virtual void Commit(const Assignment*, const Assignment*) {}

  /// Accepts a "delta" given the assignment with which the filter has been
  /// synchronized; the delta holds the variables which have been modified and
  /// their new value.
  /// If the filter represents a part of the global objective, its contribution
  /// must be between objective_min and objective_max.
  /// Sample: supposing one wants to maintain a[0,1] + b[0,1] <= 1,
  /// for the assignment (a,1), (b,0), the delta (b,1) will be rejected
  /// but the delta (a,0) will be accepted.
  /// TODO(user): Remove arguments when there are no more need for those.
  virtual bool Accept(const Assignment* delta, const Assignment* deltadelta,
                      int64_t objective_min, int64_t objective_max) = 0;
  virtual bool IsIncremental() const { return false; }

  /// Synchronizes the filter with the current solution, delta being the
  /// difference with the solution passed to the previous call to Synchronize()
  /// or IncrementalSynchronize(). 'delta' can be used to incrementally
  /// synchronizing the filter with the new solution by only considering the
  /// changes in delta.
  virtual void Synchronize(const Assignment* assignment,
                           const Assignment* delta) = 0;
  /// Cancels the changes made by the last Relax()/Accept() calls.
  virtual void Revert() {}

  /// Sets the filter to empty solution.
  virtual void Reset() {}

  /// Objective value from last time Synchronize() was called.
  virtual int64_t GetSynchronizedObjectiveValue() const { return 0LL; }
  /// Objective value from the last time Accept() was called and returned true.
  // If the last Accept() call returned false, returns an undefined value.
  virtual int64_t GetAcceptedObjectiveValue() const { return 0LL; }
};

/// Filter manager: when a move is made, filters are executed to decide whether
/// the solution is feasible and compute parts of the new cost. This class
/// schedules filter execution and composes costs as a sum.
class LocalSearchFilterManager : public BaseObject {
 public:
  // This class is responsible for calling filters methods in a correct order.
  // For now, an order is specified explicitly by the user.
  enum FilterEventType { kAccept, kRelax };
  struct FilterEvent {
    LocalSearchFilter* filter;
    FilterEventType event_type;
    int priority;
  };

  std::string DebugString() const override {
    return "LocalSearchFilterManager";
  }
  // Builds a manager that calls filter methods ordered by increasing priority.
  // Note that some filters might appear only once, if their Relax() or Accept()
  // are trivial.
  explicit LocalSearchFilterManager(std::vector<FilterEvent> filter_events);
  // Builds a manager that calls filter methods using the following ordering:
  // first Relax() in vector order, then Accept() in vector order.
  explicit LocalSearchFilterManager(std::vector<LocalSearchFilter*> filters);

  // Calls Revert() of filters, in reverse order of Relax events.
  void Revert();
  /// Returns true iff all filters return true, and the sum of their accepted
  /// objectives is between objective_min and objective_max.
  /// The monitor has its Begin/EndFiltering events triggered.
  bool Accept(LocalSearchMonitor* monitor, const Assignment* delta,
              const Assignment* deltadelta, int64_t objective_min,
              int64_t objective_max);
  /// Synchronizes all filters to assignment.
  void Synchronize(const Assignment* assignment, const Assignment* delta);
  int64_t GetSynchronizedObjectiveValue() const { return synchronized_value_; }
  int64_t GetAcceptedObjectiveValue() const { return accepted_value_; }

 private:
  // Finds the last event (incremental -itself- or not) with the same priority
  // as the last incremental event.
  void FindIncrementalEventEnd();

  std::vector<FilterEvent> events_;
  int last_event_called_ = -1;
  // If a filter is incremental, its Relax() and Accept() must be called for
  // every candidate, even if the Accept() of a prior filter rejected it.
  // To ensure that those incremental filters have consistent inputs, all
  // intermediate events with Relax() must also be called.
  int incremental_events_end_ = 0;
  int64_t synchronized_value_;
  int64_t accepted_value_;
};

class OR_DLL IntVarLocalSearchFilter : public LocalSearchFilter {
 public:
  explicit IntVarLocalSearchFilter(const std::vector<IntVar*>& vars);
  ~IntVarLocalSearchFilter() override;
  /// This method should not be overridden. Override OnSynchronize() instead
  /// which is called before exiting this method.
  void Synchronize(const Assignment* assignment,
                   const Assignment* delta) override;

  bool FindIndex(IntVar* const var, int64_t* index) const {
    DCHECK(index != nullptr);
    const int var_index = var->index();
    *index = (var_index < var_index_to_index_.size())
                 ? var_index_to_index_[var_index]
                 : kUnassigned;
    return *index != kUnassigned;
  }

  /// Add variables to "track" to the filter.
  void AddVars(const std::vector<IntVar*>& vars);
  int Size() const { return vars_.size(); }
  IntVar* Var(int index) const { return vars_[index]; }
  int64_t Value(int index) const {
    DCHECK(IsVarSynced(index));
    return values_[index];
  }
  bool IsVarSynced(int index) const { return var_synced_[index]; }

 protected:
  virtual void OnSynchronize(const Assignment*) {}
  void SynchronizeOnAssignment(const Assignment* assignment);

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> values_;
  std::vector<bool> var_synced_;
  std::vector<int> var_index_to_index_;
  static const int kUnassigned;
};

class PropagationMonitor : public SearchMonitor {
 public:
  explicit PropagationMonitor(Solver* solver);
  ~PropagationMonitor() override;
  std::string DebugString() const override { return "PropagationMonitor"; }

  /// Propagation events.
  virtual void BeginConstraintInitialPropagation(Constraint* constraint) = 0;
  virtual void EndConstraintInitialPropagation(Constraint* constraint) = 0;
  virtual void BeginNestedConstraintInitialPropagation(Constraint* parent,
                                                       Constraint* nested) = 0;
  virtual void EndNestedConstraintInitialPropagation(Constraint* parent,
                                                     Constraint* nested) = 0;
  virtual void RegisterDemon(Demon* demon) = 0;
  virtual void BeginDemonRun(Demon* demon) = 0;
  virtual void EndDemonRun(Demon* demon) = 0;
  virtual void StartProcessingIntegerVariable(IntVar* var) = 0;
  virtual void EndProcessingIntegerVariable(IntVar* var) = 0;
  virtual void PushContext(const std::string& context) = 0;
  virtual void PopContext() = 0;
  /// IntExpr modifiers.
  virtual void SetMin(IntExpr* expr, int64_t new_min) = 0;
  virtual void SetMax(IntExpr* expr, int64_t new_max) = 0;
  virtual void SetRange(IntExpr* expr, int64_t new_min, int64_t new_max) = 0;
  /// IntVar modifiers.
  virtual void SetMin(IntVar* var, int64_t new_min) = 0;
  virtual void SetMax(IntVar* var, int64_t new_max) = 0;
  virtual void SetRange(IntVar* var, int64_t new_min, int64_t new_max) = 0;
  virtual void RemoveValue(IntVar* var, int64_t value) = 0;
  virtual void SetValue(IntVar* var, int64_t value) = 0;
  virtual void RemoveInterval(IntVar* var, int64_t imin, int64_t imax) = 0;
  virtual void SetValues(IntVar* var, const std::vector<int64_t>& values) = 0;
  virtual void RemoveValues(IntVar* var,
                            const std::vector<int64_t>& values) = 0;
  /// IntervalVar modifiers.
  virtual void SetStartMin(IntervalVar* var, int64_t new_min) = 0;
  virtual void SetStartMax(IntervalVar* var, int64_t new_max) = 0;
  virtual void SetStartRange(IntervalVar* var, int64_t new_min,
                             int64_t new_max) = 0;
  virtual void SetEndMin(IntervalVar* var, int64_t new_min) = 0;
  virtual void SetEndMax(IntervalVar* var, int64_t new_max) = 0;
  virtual void SetEndRange(IntervalVar* var, int64_t new_min,
                           int64_t new_max) = 0;
  virtual void SetDurationMin(IntervalVar* var, int64_t new_min) = 0;
  virtual void SetDurationMax(IntervalVar* var, int64_t new_max) = 0;
  virtual void SetDurationRange(IntervalVar* var, int64_t new_min,
                                int64_t new_max) = 0;
  virtual void SetPerformed(IntervalVar* var, bool value) = 0;
  /// SequenceVar modifiers
  virtual void RankFirst(SequenceVar* var, int index) = 0;
  virtual void RankNotFirst(SequenceVar* var, int index) = 0;
  virtual void RankLast(SequenceVar* var, int index) = 0;
  virtual void RankNotLast(SequenceVar* var, int index) = 0;
  virtual void RankSequence(SequenceVar* var,
                            const std::vector<int>& rank_first,
                            const std::vector<int>& rank_last,
                            const std::vector<int>& unperformed) = 0;
  /// Install itself on the solver.
  void Install() override;
};

class LocalSearchMonitor : public SearchMonitor {
  // TODO(user): Add monitoring of local search filters.
 public:
  explicit LocalSearchMonitor(Solver* solver);
  ~LocalSearchMonitor() override;
  std::string DebugString() const override { return "LocalSearchMonitor"; }

  /// Local search operator events.
  virtual void BeginOperatorStart() = 0;
  virtual void EndOperatorStart() = 0;
  virtual void BeginMakeNextNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndMakeNextNeighbor(const LocalSearchOperator* op,
                                   bool neighbor_found, const Assignment* delta,
                                   const Assignment* deltadelta) = 0;
  virtual void BeginFilterNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndFilterNeighbor(const LocalSearchOperator* op,
                                 bool neighbor_found) = 0;
  virtual void BeginAcceptNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndAcceptNeighbor(const LocalSearchOperator* op,
                                 bool neighbor_found) = 0;
  virtual void BeginFiltering(const LocalSearchFilter* filter) = 0;
  virtual void EndFiltering(const LocalSearchFilter* filter, bool reject) = 0;

  virtual bool IsActive() const = 0;

  /// Install itself on the solver.
  void Install() override;
};

class OR_DLL BooleanVar : public IntVar {
 public:
  static const int kUnboundBooleanVarValue;

  explicit BooleanVar(Solver* const s, const std::string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue) {}

  ~BooleanVar() override {}

  int64_t Min() const override { return (value_ == 1); }
  void SetMin(int64_t m) override;
  int64_t Max() const override { return (value_ != 0); }
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (value_ != kUnboundBooleanVarValue); }
  int64_t Value() const override {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override { WhenBound(d); }
  void WhenDomain(Demon* d) override { WhenBound(d); }
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  std::string DebugString() const override;
  int VarType() const override { return BOOLEAN_VAR; }

  IntVar* IsEqual(int64_t constant) override;
  IntVar* IsDifferent(int64_t constant) override;
  IntVar* IsGreaterOrEqual(int64_t constant) override;
  IntVar* IsLessOrEqual(int64_t constant) override;

  virtual void RestoreValue() = 0;
  std::string BaseName() const override { return "BooleanVar"; }

  int RawValue() const { return value_; }

 protected:
  int value_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
};

class SymmetryManager;

/// A symmetry breaker is an object that will visit a decision and
/// create the 'symmetrical' decision in return.
/// Each symmetry breaker represents one class of symmetry.
class SymmetryBreaker : public DecisionVisitor {
 public:
  SymmetryBreaker()
      : symmetry_manager_(nullptr), index_in_symmetry_manager_(-1) {}
  ~SymmetryBreaker() override {}

  void AddIntegerVariableEqualValueClause(IntVar* var, int64_t value);
  void AddIntegerVariableGreaterOrEqualValueClause(IntVar* var, int64_t value);
  void AddIntegerVariableLessOrEqualValueClause(IntVar* var, int64_t value);

 private:
  friend class SymmetryManager;
  void set_symmetry_manager_and_index(SymmetryManager* manager, int index) {
    CHECK(symmetry_manager_ == nullptr);
    CHECK_EQ(-1, index_in_symmetry_manager_);
    symmetry_manager_ = manager;
    index_in_symmetry_manager_ = index;
  }
  SymmetryManager* symmetry_manager() const { return symmetry_manager_; }
  int index_in_symmetry_manager() const { return index_in_symmetry_manager_; }

  SymmetryManager* symmetry_manager_;
  /// Index of the symmetry breaker when used inside the symmetry manager.
  int index_in_symmetry_manager_;
};

/// The base class of all search logs that periodically outputs information when
/// the search is running.
class SearchLog : public SearchMonitor {
 public:
  SearchLog(Solver* solver, std::vector<IntVar*> vars, std::string vars_name,
            std::vector<double> scaling_factors, std::vector<double> offsets,
            std::function<std::string()> display_callback,
            bool display_on_new_solutions_only, int period);
  ~SearchLog() override;
  void EnterSearch() override;
  void ExitSearch() override;
  bool AtSolution() override;
  void BeginFail() override;
  void NoMoreSolutions() override;
  void AcceptUncheckedNeighbor() override;
  void ApplyDecision(Decision* decision) override;
  void RefuteDecision(Decision* decision) override;
  void OutputDecision();
  void Maintain();
  void BeginInitialPropagation() override;
  void EndInitialPropagation() override;
  std::string DebugString() const override;

 protected:
  /* Bottleneck function used for all UI related output. */
  virtual void OutputLine(const std::string& line);

 private:
  static std::string MemoryUsage();

  const int period_;
  std::unique_ptr<WallTimer> timer_;
  const std::vector<IntVar*> vars_;
  const std::string vars_name_;
  const std::vector<double> scaling_factors_;
  const std::vector<double> offsets_;
  std::function<std::string()> display_callback_;
  const bool display_on_new_solutions_only_;
  int nsol_;
  absl::Duration tick_;
  std::vector<int64_t> objective_min_;
  std::vector<int64_t> objective_max_;
  std::vector<int64_t> last_objective_value_;
  absl::Duration last_objective_timestamp_;
  int min_right_depth_;
  int max_depth_;
  int sliding_min_depth_;
  int sliding_max_depth_;
  int neighbors_offset_ = 0;
};

/// Implements a complete cache for model elements: expressions and
/// constraints.  Caching is based on the signatures of the elements, as
/// well as their types.  This class is used internally to avoid creating
/// duplicate objects.
class ModelCache {
 public:
  enum VoidConstraintType {
    VOID_FALSE_CONSTRAINT = 0,
    VOID_TRUE_CONSTRAINT,
    VOID_CONSTRAINT_MAX,
  };

  enum VarConstantConstraintType {
    VAR_CONSTANT_EQUALITY = 0,
    VAR_CONSTANT_GREATER_OR_EQUAL,
    VAR_CONSTANT_LESS_OR_EQUAL,
    VAR_CONSTANT_NON_EQUALITY,
    VAR_CONSTANT_CONSTRAINT_MAX,
  };

  enum VarConstantConstantConstraintType {
    VAR_CONSTANT_CONSTANT_BETWEEN = 0,
    VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX,
  };

  enum ExprExprConstraintType {
    EXPR_EXPR_EQUALITY = 0,
    EXPR_EXPR_GREATER,
    EXPR_EXPR_GREATER_OR_EQUAL,
    EXPR_EXPR_LESS,
    EXPR_EXPR_LESS_OR_EQUAL,
    EXPR_EXPR_NON_EQUALITY,
    EXPR_EXPR_CONSTRAINT_MAX,
  };

  enum ExprExpressionType {
    EXPR_OPPOSITE = 0,
    EXPR_ABS,
    EXPR_SQUARE,
    EXPR_EXPRESSION_MAX,
  };

  enum ExprExprExpressionType {
    EXPR_EXPR_DIFFERENCE = 0,
    EXPR_EXPR_PROD,
    EXPR_EXPR_DIV,
    EXPR_EXPR_MAX,
    EXPR_EXPR_MIN,
    EXPR_EXPR_SUM,
    EXPR_EXPR_IS_LESS,
    EXPR_EXPR_IS_LESS_OR_EQUAL,
    EXPR_EXPR_IS_EQUAL,
    EXPR_EXPR_IS_NOT_EQUAL,
    EXPR_EXPR_EXPRESSION_MAX,
  };

  enum ExprExprConstantExpressionType {
    EXPR_EXPR_CONSTANT_CONDITIONAL = 0,
    EXPR_EXPR_CONSTANT_EXPRESSION_MAX,
  };

  enum ExprConstantExpressionType {
    EXPR_CONSTANT_DIFFERENCE = 0,
    EXPR_CONSTANT_DIVIDE,
    EXPR_CONSTANT_PROD,
    EXPR_CONSTANT_MAX,
    EXPR_CONSTANT_MIN,
    EXPR_CONSTANT_SUM,
    EXPR_CONSTANT_IS_EQUAL,
    EXPR_CONSTANT_IS_NOT_EQUAL,
    EXPR_CONSTANT_IS_GREATER_OR_EQUAL,
    EXPR_CONSTANT_IS_LESS_OR_EQUAL,
    EXPR_CONSTANT_EXPRESSION_MAX,
  };
  enum VarConstantConstantExpressionType {
    VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS = 0,
    VAR_CONSTANT_CONSTANT_EXPRESSION_MAX,
  };

  enum VarConstantArrayExpressionType {
    VAR_CONSTANT_ARRAY_ELEMENT = 0,
    VAR_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantArrayExpressionType {
    VAR_ARRAY_CONSTANT_ARRAY_SCAL_PROD = 0,
    VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayExpressionType {
    VAR_ARRAY_MAX = 0,
    VAR_ARRAY_MIN,
    VAR_ARRAY_SUM,
    VAR_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantExpressionType {
    VAR_ARRAY_CONSTANT_INDEX = 0,
    VAR_ARRAY_CONSTANT_EXPRESSION_MAX,
  };

  explicit ModelCache(Solver* solver);
  virtual ~ModelCache();

  virtual void Clear() = 0;

  /// Void constraints.

  virtual Constraint* FindVoidConstraint(VoidConstraintType type) const = 0;

  virtual void InsertVoidConstraint(Constraint* ct,
                                    VoidConstraintType type) = 0;

  /// Var Constant Constraints.
  virtual Constraint* FindVarConstantConstraint(
      IntVar* var, int64_t value, VarConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstraint(Constraint* ct, IntVar* var,
                                           int64_t value,
                                           VarConstantConstraintType type) = 0;

  /// Var Constant Constant Constraints.

  virtual Constraint* FindVarConstantConstantConstraint(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstantConstraint(
      Constraint* ct, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type) = 0;

  /// Expr Expr Constraints.

  virtual Constraint* FindExprExprConstraint(
      IntExpr* expr1, IntExpr* expr2, ExprExprConstraintType type) const = 0;

  virtual void InsertExprExprConstraint(Constraint* ct, IntExpr* expr1,
                                        IntExpr* expr2,
                                        ExprExprConstraintType type) = 0;

  /// Expr Expressions.

  virtual IntExpr* FindExprExpression(IntExpr* expr,
                                      ExprExpressionType type) const = 0;

  virtual void InsertExprExpression(IntExpr* expression, IntExpr* expr,
                                    ExprExpressionType type) = 0;

  /// Expr Constant Expressions.

  virtual IntExpr* FindExprConstantExpression(
      IntExpr* expr, int64_t value, ExprConstantExpressionType type) const = 0;

  virtual void InsertExprConstantExpression(
      IntExpr* expression, IntExpr* var, int64_t value,
      ExprConstantExpressionType type) = 0;

  /// Expr Expr Expressions.

  virtual IntExpr* FindExprExprExpression(
      IntExpr* var1, IntExpr* var2, ExprExprExpressionType type) const = 0;

  virtual void InsertExprExprExpression(IntExpr* expression, IntExpr* var1,
                                        IntExpr* var2,
                                        ExprExprExpressionType type) = 0;

  /// Expr Expr Constant Expressions.

  virtual IntExpr* FindExprExprConstantExpression(
      IntExpr* var1, IntExpr* var2, int64_t constant,
      ExprExprConstantExpressionType type) const = 0;

  virtual void InsertExprExprConstantExpression(
      IntExpr* expression, IntExpr* var1, IntExpr* var2, int64_t constant,
      ExprExprConstantExpressionType type) = 0;

  /// Var Constant Constant Expressions.

  virtual IntExpr* FindVarConstantConstantExpression(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type) const = 0;

  virtual void InsertVarConstantConstantExpression(
      IntExpr* expression, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type) = 0;

  /// Var Constant Array Expressions.

  virtual IntExpr* FindVarConstantArrayExpression(
      IntVar* var, const std::vector<int64_t>& values,
      VarConstantArrayExpressionType type) const = 0;

  virtual void InsertVarConstantArrayExpression(
      IntExpr* expression, IntVar* var, const std::vector<int64_t>& values,
      VarConstantArrayExpressionType type) = 0;

  /// Var Array Expressions.

  virtual IntExpr* FindVarArrayExpression(
      const std::vector<IntVar*>& vars, VarArrayExpressionType type) const = 0;

  virtual void InsertVarArrayExpression(IntExpr* expression,
                                        const std::vector<IntVar*>& vars,
                                        VarArrayExpressionType type) = 0;

  /// Var Array Constant Array Expressions.

  virtual IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type) const = 0;

  virtual void InsertVarArrayConstantArrayExpression(
      IntExpr* expression, const std::vector<IntVar*>& var,
      const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type) = 0;

  /// Var Array Constant Expressions.

  virtual IntExpr* FindVarArrayConstantExpression(
      const std::vector<IntVar*>& vars, int64_t value,
      VarArrayConstantExpressionType type) const = 0;

  virtual void InsertVarArrayConstantExpression(
      IntExpr* expression, const std::vector<IntVar*>& var, int64_t value,
      VarArrayConstantExpressionType type) = 0;

  Solver* solver() const;

 private:
  Solver* const solver_;
};

/// Argument Holder: useful when visiting a model.
#if !defined(SWIG)
class ArgumentHolder {
 public:
  /// Type of the argument.
  const std::string& TypeName() const;
  void SetTypeName(const std::string& type_name);

  /// Setters.
  void SetIntegerArgument(const std::string& arg_name, int64_t value);
  void SetIntegerArrayArgument(const std::string& arg_name,
                               const std::vector<int64_t>& values);
  void SetIntegerMatrixArgument(const std::string& arg_name,
                                const IntTupleSet& values);
  void SetIntegerExpressionArgument(const std::string& arg_name, IntExpr* expr);
  void SetIntegerVariableArrayArgument(const std::string& arg_name,
                                       const std::vector<IntVar*>& vars);
  void SetIntervalArgument(const std::string& arg_name, IntervalVar* var);
  void SetIntervalArrayArgument(const std::string& arg_name,
                                const std::vector<IntervalVar*>& vars);
  void SetSequenceArgument(const std::string& arg_name, SequenceVar* var);
  void SetSequenceArrayArgument(const std::string& arg_name,
                                const std::vector<SequenceVar*>& vars);

  /// Checks if arguments exist.
  bool HasIntegerExpressionArgument(const std::string& arg_name) const;
  bool HasIntegerVariableArrayArgument(const std::string& arg_name) const;

  /// Getters.
  int64_t FindIntegerArgumentWithDefault(const std::string& arg_name,
                                         int64_t def) const;
  int64_t FindIntegerArgumentOrDie(const std::string& arg_name) const;
  const std::vector<int64_t>& FindIntegerArrayArgumentOrDie(
      const std::string& arg_name) const;
  const IntTupleSet& FindIntegerMatrixArgumentOrDie(
      const std::string& arg_name) const;

  IntExpr* FindIntegerExpressionArgumentOrDie(
      const std::string& arg_name) const;
  const std::vector<IntVar*>& FindIntegerVariableArrayArgumentOrDie(
      const std::string& arg_name) const;

 private:
  std::string type_name_;
  absl::flat_hash_map<std::string, int64_t> integer_argument_;
  absl::flat_hash_map<std::string, std::vector<int64_t>>
      integer_array_argument_;
  absl::flat_hash_map<std::string, IntTupleSet> matrix_argument_;
  absl::flat_hash_map<std::string, IntExpr*> integer_expression_argument_;
  absl::flat_hash_map<std::string, IntervalVar*> interval_argument_;
  absl::flat_hash_map<std::string, SequenceVar*> sequence_argument_;
  absl::flat_hash_map<std::string, std::vector<IntVar*>>
      integer_variable_array_argument_;
  absl::flat_hash_map<std::string, std::vector<IntervalVar*>>
      interval_array_argument_;
  absl::flat_hash_map<std::string, std::vector<SequenceVar*>>
      sequence_array_argument_;
};

/// Model Parser
class ModelParser : public ModelVisitor {
 public:
  ModelParser();

  ~ModelParser() override;

  /// Header/footers.
  void BeginVisitModel(const std::string& solver_name) override;
  void EndVisitModel(const std::string& solver_name) override;
  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* constraint) override;
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* constraint) override;
  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* expr) override;
  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* expr) override;
  void VisitIntegerVariable(const IntVar* variable, IntExpr* delegate) override;
  void VisitIntegerVariable(const IntVar* variable,
                            const std::string& operation, int64_t value,
                            IntVar* delegate) override;
  void VisitIntervalVariable(const IntervalVar* variable,
                             const std::string& operation, int64_t value,
                             IntervalVar* delegate) override;
  void VisitSequenceVariable(const SequenceVar* variable) override;
  /// Integer arguments
  void VisitIntegerArgument(const std::string& arg_name,
                            int64_t value) override;
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64_t>& values) override;
  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override;
  /// Variables.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* argument) override;
  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name,
      const std::vector<IntVar*>& arguments) override;
  /// Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* argument) override;
  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override;
  /// Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* argument) override;
  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override;

 protected:
  void PushArgumentHolder();
  void PopArgumentHolder();
  ArgumentHolder* Top() const;

 private:
  std::vector<ArgumentHolder*> holders_;
};

template <class T>
class ArrayWithOffset : public BaseObject {
 public:
  ArrayWithOffset(int64_t index_min, int64_t index_max)
      : index_min_(index_min),
        index_max_(index_max),
        values_(new T[index_max - index_min + 1]) {
    DCHECK_LE(index_min, index_max);
  }

  ~ArrayWithOffset() override {}

  virtual T Evaluate(int64_t index) const {
    DCHECK_GE(index, index_min_);
    DCHECK_LE(index, index_max_);
    return values_[index - index_min_];
  }

  void SetValue(int64_t index, T value) {
    DCHECK_GE(index, index_min_);
    DCHECK_LE(index, index_max_);
    values_[index - index_min_] = value;
  }

  std::string DebugString() const override { return "ArrayWithOffset"; }

 private:
  const int64_t index_min_;
  const int64_t index_max_;
  std::unique_ptr<T[]> values_;
};
#endif  // SWIG

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

  void RevInsert(Solver* const solver, int64_t index, T value) {
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

  void Insert(Solver* const solver, const T& elt) {
    const int position = num_elements_.Value();
    DCHECK_LT(position, capacity_);  /// Valid.
    DCHECK(NotAlreadyInserted(elt));
    elements_[position] = elt;
    position_[elt] = position;
    num_elements_.Incr(solver);
  }

  void Remove(Solver* const solver, const T& value_index) {
    num_elements_.Decr(solver);
    SwapTo(value_index, num_elements_.Value());
  }

  void Restore(Solver* const solver, const T& value_index) {
    SwapTo(value_index, num_elements_.Value());
    num_elements_.Incr(solver);
  }

  void Clear(Solver* const solver) { num_elements_.SetValue(solver, 0); }

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
  /// Number of elements in the set.
  NumericalRev<int> num_elements_;
  /// Number of elements in the set.
  const int capacity_;
  /// Reverse mapping.
  int* position_;
  /// Does the set owns the position array.
  const bool delete_position_;
};

/// ----- RevPartialSequence -----

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

  void RankFirst(Solver* const solver, int elt) {
    DCHECK_LE(first_ranked_.Value(), last_ranked_.Value());
    SwapTo(elt, first_ranked_.Value());
    first_ranked_.Incr(solver);
  }

  void RankLast(Solver* const solver, int elt) {
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

/// This class represents a reversible bitset. It is meant to represent a set of
/// active bits. It does not offer direct access, but just methods that can
/// reversibly subtract another bitset, or check if the current active bitset
/// intersects with another bitset.
class UnsortedNullableRevBitset {
 public:
  /// Size is the number of bits to store in the bitset.
  explicit UnsortedNullableRevBitset(int bit_size);

  ~UnsortedNullableRevBitset() {}

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
  ///   - The first word tested to check the intersection will be the
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

template <class T>
bool IsArrayConstant(const std::vector<T>& values, const T& value) {
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != value) {
      return false;
    }
  }
  return true;
}

template <class T>
bool IsArrayBoolean(const std::vector<T>& values) {
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != 0 && values[i] != 1) {
      return false;
    }
  }
  return true;
}

template <class T>
bool AreAllOnes(const std::vector<T>& values) {
  return IsArrayConstant(values, T(1));
}

template <class T>
bool AreAllNull(const std::vector<T>& values) {
  return IsArrayConstant(values, T(0));
}

template <class T>
bool AreAllGreaterOrEqual(const std::vector<T>& values, const T& value) {
  for (const T& current_value : values) {
    if (current_value < value) {
      return false;
    }
  }
  return true;
}

template <class T>
bool AreAllLessOrEqual(const std::vector<T>& values, const T& value) {
  for (const T& current_value : values) {
    if (current_value > value) {
      return false;
    }
  }
  return true;
}

template <class T>
bool AreAllPositive(const std::vector<T>& values) {
  return AreAllGreaterOrEqual(values, T(0));
}

template <class T>
bool AreAllNegative(const std::vector<T>& values) {
  return AreAllLessOrEqual(values, T(0));
}

template <class T>
bool AreAllStrictlyPositive(const std::vector<T>& values) {
  return AreAllGreaterOrEqual(values, T(1));
}

template <class T>
bool AreAllStrictlyNegative(const std::vector<T>& values) {
  return AreAllLessOrEqual(values, T(-1));
}

template <class T>
bool IsIncreasingContiguous(const std::vector<T>& values) {
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) {
      return false;
    }
  }
  return true;
}

template <class T>
bool IsIncreasing(const std::vector<T>& values) {
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] < values[i]) {
      return false;
    }
  }
  return true;
}

template <class T>
bool IsArrayInRange(const std::vector<IntVar*>& vars, T range_min,
                    T range_max) {
  for (int i = 0; i < vars.size(); ++i) {
    if (vars[i]->Min() < range_min || vars[i]->Max() > range_max) {
      return false;
    }
  }
  return true;
}

inline bool AreAllBound(const std::vector<IntVar*>& vars) {
  for (int i = 0; i < vars.size(); ++i) {
    if (!vars[i]->Bound()) {
      return false;
    }
  }
  return true;
}

inline bool AreAllBooleans(const std::vector<IntVar*>& vars) {
  return IsArrayInRange(vars, 0, 1);
}

/// Returns true if all the variables are assigned to a single value,
/// or if their corresponding value is null.
template <class T>
bool AreAllBoundOrNull(const std::vector<IntVar*>& vars,
                       const std::vector<T>& values) {
  for (int i = 0; i < vars.size(); ++i) {
    if (values[i] != 0 && !vars[i]->Bound()) {
      return false;
    }
  }
  return true;
}

/// Returns true if all variables are assigned to 'value'.
inline bool AreAllBoundTo(const std::vector<IntVar*>& vars, int64_t value) {
  for (int i = 0; i < vars.size(); ++i) {
    if (!vars[i]->Bound() || vars[i]->Min() != value) {
      return false;
    }
  }
  return true;
}

inline int64_t MaxVarArray(const std::vector<IntVar*>& vars) {
  DCHECK(!vars.empty());
  int64_t result = kint64min;
  for (int i = 0; i < vars.size(); ++i) {
    /// The std::max<int64_t> is needed for compilation on MSVC.
    result = std::max<int64_t>(result, vars[i]->Max());
  }
  return result;
}

inline int64_t MinVarArray(const std::vector<IntVar*>& vars) {
  DCHECK(!vars.empty());
  int64_t result = kint64max;
  for (int i = 0; i < vars.size(); ++i) {
    /// The std::min<int64_t> is needed for compilation on MSVC.
    result = std::min<int64_t>(result, vars[i]->Min());
  }
  return result;
}

inline void FillValues(const std::vector<IntVar*>& vars,
                       std::vector<int64_t>* const values) {
  values->clear();
  values->resize(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    (*values)[i] = vars[i]->Value();
  }
}

inline int64_t PosIntDivUp(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e < 0 || e % v == 0) ? e / v : e / v + 1;
}

inline int64_t PosIntDivDown(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e >= 0 || e % v == 0) ? e / v : e / v - 1;
}

std::vector<int64_t> ToInt64Vector(const std::vector<int>& input);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
