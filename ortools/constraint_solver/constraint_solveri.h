// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/sysinfo.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/tuple_set.h"
#include "ortools/util/vector_map.h"

class WallTimer;

namespace operations_research {
class CPArgumentProto;
class CPConstraintProto;
class CPIntegerExpressionProto;
class CPIntervalVariableProto;

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
    T data_[CHUNK_SIZE];
    const Chunk* const next_;
    explicit Chunk(const Chunk* next) : next_(next) {}
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
      if (value_ == chunk_->data_ + CHUNK_SIZE) {
        chunk_ = chunk_->next_;
        value_ = chunk_ ? chunk_->data_ : nullptr;
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
    chunks_->data_[pos_.Value()] = val;
  }

  /// Pushes the var on top if is not a duplicate of the current top object.
  void PushIfNotTop(Solver* const s, T val) {
    if (chunks_ == nullptr || LastValue() != val) {
      Push(s, val);
    }
  }

  /// Returns the last item of the FIFO.
  const T* Last() const {
    return chunks_ ? &chunks_->data_[pos_.Value()] : nullptr;
  }

  T* MutableLast() { return chunks_ ? &chunks_->data_[pos_.Value()] : nullptr; }

  /// Returns the last value in the FIFO.
  const T& LastValue() const {
    DCHECK(chunks_);
    return chunks_->data_[pos_.Value()];
  }

  /// Sets the last value in the FIFO.
  void SetLastValue(const T& v) {
    DCHECK(Last());
    chunks_->data_[pos_.Value()] = v;
  }

 private:
  Chunk* chunks_;
  NumericalRev<int> pos_;
};

/// Hash functions
// TODO(user): use murmurhash.
inline uint64 Hash1(uint64 value) {
  value = (~value) + (value << 21);  /// value = (value << 21) - value - 1;
  value ^= value >> 24;
  value += (value << 3) + (value << 8);  /// value * 265
  value ^= value >> 14;
  value += (value << 2) + (value << 4);  /// value * 21
  value ^= value >> 28;
  value += (value << 31);
  return value;
}

inline uint64 Hash1(uint32 value) {
  uint64 a = value;
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint64 Hash1(int64 value) { return Hash1(static_cast<uint64>(value)); }

inline uint64 Hash1(int value) { return Hash1(static_cast<uint32>(value)); }

inline uint64 Hash1(void* const ptr) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) || \
    defined(__aarch64__)
  return Hash1(reinterpret_cast<uint64>(ptr));
#else
  return Hash1(reinterpret_cast<uint32>(ptr));
#endif
}

template <class T>
uint64 Hash1(const std::vector<T*>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64 hash = Hash1(ptrs[0]);
  for (int i = 1; i < ptrs.size(); ++i) {
    hash = hash * i + Hash1(ptrs[i]);
  }
  return hash;
}

inline uint64 Hash1(const std::vector<int64>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64 hash = Hash1(ptrs[0]);
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
    uint64 code = Hash1(key) % size_.Value();
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
    uint64 code = Hash1(key) % size_.Value();
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
        const uint64 new_position = Hash1(to_reinsert->key()) % size_.Value();
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
  explicit SmallRevBitSet(int64 size);
  /// Sets the 'pos' bit.
  void SetToOne(Solver* const solver, int64 pos);
  /// Erases the 'pos' bit.
  void SetToZero(Solver* const solver, int64 pos);
  /// Returns the number of bits set to one.
  int64 Cardinality() const;
  /// Is bitset null?
  bool IsCardinalityZero() const { return bits_.Value() == GG_ULONGLONG(0); }
  /// Does it contains only one bit set?
  bool IsCardinalityOne() const {
    return (bits_.Value() != 0) && !(bits_.Value() & (bits_.Value() - 1));
  }
  /// Gets the index of the first bit set starting from 0.
  /// It returns -1 if the bitset is empty.
  int64 GetFirstOne() const;

 private:
  Rev<uint64> bits_;
};

/// This class represents a reversible bitset.
/// This class is useful to maintain supports.
class RevBitSet {
 public:
  explicit RevBitSet(int64 size);
  ~RevBitSet();

  /// Sets the 'index' bit.
  void SetToOne(Solver* const solver, int64 index);
  /// Erases the 'index' bit.
  void SetToZero(Solver* const solver, int64 index);
  /// Returns whether the 'index' bit is set.
  bool IsSet(int64 index) const;
  /// Returns the number of bits set to one.
  int64 Cardinality() const;
  /// Is bitset null?
  bool IsCardinalityZero() const;
  /// Does it contains only one bit set?
  bool IsCardinalityOne() const;
  /// Gets the index of the first bit set starting from start.
  /// It returns -1 if the bitset is empty after start.
  int64 GetFirstBit(int start) const;
  /// Cleans all bits.
  void ClearAll(Solver* const solver);

  friend class RevBitMatrix;

 private:
  /// Save the offset's part of the bitset.
  void Save(Solver* const solver, int offset);
  const int64 size_;
  const int64 length_;
  uint64* bits_;
  uint64* stamps_;
};

/// Matrix version of the RevBitSet class.
class RevBitMatrix : private RevBitSet {
 public:
  RevBitMatrix(int64 rows, int64 columns);
  ~RevBitMatrix();

  /// Sets the 'column' bit in the 'row' row.
  void SetToOne(Solver* const solver, int64 row, int64 column);
  /// Erases the 'column' bit in the 'row' row.
  void SetToZero(Solver* const solver, int64 row, int64 column);
  /// Returns whether the 'column' bit in the 'row' row is set.
  bool IsSet(int64 row, int64 column) const {
    DCHECK_GE(row, 0);
    DCHECK_LT(row, rows_);
    DCHECK_GE(column, 0);
    DCHECK_LT(column, columns_);
    return RevBitSet::IsSet(row * columns_ + column);
  }
  /// Returns the number of bits set to one in the 'row' row.
  int64 Cardinality(int row) const;
  /// Is bitset of row 'row' null?
  bool IsCardinalityZero(int row) const;
  /// Does the 'row' bitset contains only one bit set?
  bool IsCardinalityOne(int row) const;
  /// Returns the first bit in the row 'row' which position is >= 'start'.
  /// It returns -1 if there are none.
  int64 GetFirstBit(int row, int start) const;
  /// Cleans all bits.
  void ClearAll(Solver* const solver);

 private:
  const int64 rows_;
  const int64 columns_;
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

  void Run(Solver* const s) override { (constraint_->*method_)(); }

  std::string DebugString() const override {
    return "CallMethod_" + name_ + "(" + constraint_->DebugString() + ")";
  }

 private:
  T* const constraint_;
  void (T::*const method_)();
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

  void Run(Solver* const s) override { (constraint_->*method_)(param1_); }

  std::string DebugString() const override {
    return absl::StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
                        ", ", ParameterDebugString(param1_), ")");
  }

 private:
  T* const constraint_;
  void (T::*const method_)(P);
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

  void Run(Solver* const s) override {
    (constraint_->*method_)(param1_, param2_);
  }

  std::string DebugString() const override {
    return absl::StrCat(absl::StrCat("CallMethod_", name_),
                        absl::StrCat("(", constraint_->DebugString()),
                        absl::StrCat(", ", ParameterDebugString(param1_)),
                        absl::StrCat(", ", ParameterDebugString(param2_), ")"));
  }

 private:
  T* const constraint_;
  void (T::*const method_)(P, Q);
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

  void Run(Solver* const s) override {
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
  void (T::*const method_)(P, Q, R);
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

  void Run(Solver* const s) override { (constraint_->*method_)(); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return "DelayedCallMethod_" + name_ + "(" + constraint_->DebugString() +
           ")";
  }

 private:
  T* const constraint_;
  void (T::*const method_)();
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

  void Run(Solver* const s) override { (constraint_->*method_)(param1_); }

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
  void (T::*const method_)(P);
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

  void Run(Solver* const s) override {
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
  void (T::*const method_)(P, Q);
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
  virtual void Start(const Assignment* assignment) = 0;
  virtual void Reset() {}
#ifndef SWIG
  virtual const LocalSearchOperator* Self() const { return this; }
#endif  // SWIG
  virtual bool HasFragments() const { return false; }
  virtual bool HoldsDelta() const { return false; }
};

/// Base operator class for operators manipulating variables.
template <class V, class Val, class Handler>
class VarLocalSearchOperator : public LocalSearchOperator {
 public:
  VarLocalSearchOperator() : activated_(), was_activated_(), cleared_(true) {}
  explicit VarLocalSearchOperator(Handler var_handler)
      : activated_(),
        was_activated_(),
        cleared_(true),
        var_handler_(var_handler) {}
  ~VarLocalSearchOperator() override {}
  bool HoldsDelta() const override { return true; }
  /// This method should not be overridden. Override OnStart() instead which is
  /// called before exiting this method.
  void Start(const Assignment* assignment) override {
    const int size = Size();
    CHECK_LE(size, assignment->Size())
        << "Assignment contains fewer variables than operator";
    for (int i = 0; i < size; ++i) {
      activated_.Set(i, var_handler_.ValueFromAssignment(*assignment, vars_[i],
                                                         i, &values_[i]));
    }
    prev_values_ = old_values_;
    old_values_ = values_;
    was_activated_.SetContentFromBitsetOfSameSize(activated_);
    OnStart();
  }
  virtual bool IsIncremental() const { return false; }
  int Size() const { return vars_.size(); }
  /// Returns the value in the current assignment of the variable of given
  /// index.
  const Val& Value(int64 index) const {
    DCHECK_LT(index, vars_.size());
    return values_[index];
  }
  /// Returns the variable of given index.
  V* Var(int64 index) const { return vars_[index]; }
  virtual bool SkipUnchanged(int index) const { return false; }
  const Val& OldValue(int64 index) const { return old_values_[index]; }
  void SetValue(int64 index, const Val& value) {
    values_[index] = value;
    MarkChange(index);
  }
  bool Activated(int64 index) const { return activated_[index]; }
  void Activate(int64 index) {
    activated_.Set(index);
    MarkChange(index);
  }
  void Deactivate(int64 index) {
    activated_.Clear(index);
    MarkChange(index);
  }
  bool ApplyChanges(Assignment* delta, Assignment* deltadelta) const {
    if (IsIncremental() && !cleared_) {
      for (const int64 index : delta_changes_.PositionsSetAtLeastOnce()) {
        V* var = Var(index);
        const Val& value = Value(index);
        const bool activated = activated_[index];
        var_handler_.AddToAssignment(var, value, activated, nullptr, index,
                                     deltadelta);
        var_handler_.AddToAssignment(var, value, activated,
                                     &assignment_indices_, index, delta);
      }
    } else {
      delta->Clear();
      for (const int64 index : changes_.PositionsSetAtLeastOnce()) {
        const Val& value = Value(index);
        const bool activated = activated_[index];
        if (!activated || value != OldValue(index) || !SkipUnchanged(index)) {
          var_handler_.AddToAssignment(Var(index), value, activated_[index],
                                       &assignment_indices_, index, delta);
        }
      }
    }
    return true;
  }
  void RevertChanges(bool incremental) {
    cleared_ = false;
    delta_changes_.SparseClearAll();
    if (incremental && IsIncremental()) return;
    cleared_ = true;
    for (const int64 index : changes_.PositionsSetAtLeastOnce()) {
      values_[index] = old_values_[index];
      var_handler_.OnRevertChanges(index, values_[index]);
      activated_.CopyBucket(was_activated_, index);
      assignment_indices_[index] = -1;
    }
    changes_.SparseClearAll();
  }
  void AddVars(const std::vector<V*>& vars) {
    if (!vars.empty()) {
      vars_.insert(vars_.end(), vars.begin(), vars.end());
      const int64 size = Size();
      values_.resize(size);
      old_values_.resize(size);
      prev_values_.resize(size);
      assignment_indices_.resize(size, -1);
      activated_.Resize(size);
      was_activated_.Resize(size);
      changes_.ClearAndResize(size);
      delta_changes_.ClearAndResize(size);
      var_handler_.OnAddVars();
    }
  }

  /// Called by Start() after synchronizing the operator with the current
  /// assignment. Should be overridden instead of Start() to avoid calling
  /// VarLocalSearchOperator::Start explicitly.
  virtual void OnStart() {}

  /// OnStart() should really be protected, but then SWIG doesn't see it. So we
  /// make it public, but only subclasses should access to it (to override it).
 protected:
  void MarkChange(int64 index) {
    delta_changes_.Set(index);
    changes_.Set(index);
  }

  std::vector<V*> vars_;
  std::vector<Val> values_;
  std::vector<Val> old_values_;
  std::vector<Val> prev_values_;
  mutable std::vector<int> assignment_indices_;
  Bitset64<> activated_;
  Bitset64<> was_activated_;
  SparseBitset<> changes_;
  SparseBitset<> delta_changes_;
  bool cleared_;
  Handler var_handler_;
};

/// Base operator class for operators manipulating IntVars.
class IntVarLocalSearchOperator;

class IntVarLocalSearchHandler {
 public:
  IntVarLocalSearchHandler() : op_(nullptr) {}
  IntVarLocalSearchHandler(const IntVarLocalSearchHandler& other)
      : op_(other.op_) {}
  explicit IntVarLocalSearchHandler(IntVarLocalSearchOperator* op) : op_(op) {}
  void AddToAssignment(IntVar* var, int64 value, bool active,
                       std::vector<int>* assignment_indices, int64 index,
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
  bool ValueFromAssignment(const Assignment& assignment, IntVar* var,
                           int64 index, int64* value);
  void OnRevertChanges(int64 index, int64 value);
  void OnAddVars() {}

 private:
  IntVarLocalSearchOperator* const op_;
};

/// Specialization of LocalSearchOperator built from an array of IntVars
/// which specifies the scope of the operator.
/// This class also takes care of storing current variable values in Start(),
/// keeps track of changes done by the operator and builds the delta.
/// The Deactivate() method can be used to perform Large Neighborhood Search.

#ifdef SWIG
/// Unfortunately, we must put this code here and not in
/// */constraint_solver.i, because it must be parsed by SWIG before the
/// derived C++ class.
// TODO(user): find a way to move this code back to the .i file, where it
/// belongs.
/// In python, we use a whitelist to expose the API. This whitelist must also
/// be extended here.
#if defined(SWIGPYTHON)
// clang-format off
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::Size;
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::Value;
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::OldValue;
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::SetValue;
%feature("director") VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::IsIncremental;
%feature("director") VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::OnStart;
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::IsIncremental;
%unignore VarLocalSearchOperator<IntVar, int64,
                                 IntVarLocalSearchHandler>::OnStart;
// clang-format on
#endif  // SWIGPYTHON

// clang-format off
%rename(IntVarLocalSearchOperatorTemplate)
        VarLocalSearchOperator<IntVar, int64, IntVarLocalSearchHandler>;
%template(IntVarLocalSearchOperatorTemplate)
        VarLocalSearchOperator<IntVar, int64, IntVarLocalSearchHandler>;
// clang-format on
#endif  // SWIG

class IntVarLocalSearchOperator
    : public VarLocalSearchOperator<IntVar, int64, IntVarLocalSearchHandler> {
 public:
  IntVarLocalSearchOperator() : max_inverse_value_(-1) {}
  // If keep_inverse_values is true, assumes that vars models an injective
  // function f with domain [0, vars.size()) in which case the operator will
  // maintain the inverse function.
  explicit IntVarLocalSearchOperator(const std::vector<IntVar*>& vars,
                                     bool keep_inverse_values = false)
      : VarLocalSearchOperator<IntVar, int64, IntVarLocalSearchHandler>(
            IntVarLocalSearchHandler(this)),
        max_inverse_value_(keep_inverse_values ? vars.size() - 1 : -1) {
    AddVars(vars);
    if (keep_inverse_values) {
      int64 max_value = -1;
      for (const IntVar* const var : vars) {
        max_value = std::max(max_value, var->Max());
      }
      inverse_values_.resize(max_value + 1, -1);
      old_inverse_values_.resize(max_value + 1, -1);
    }
  }
  ~IntVarLocalSearchOperator() override {}
  /// Redefines MakeNextNeighbor to export a simpler interface. The calls to
  /// ApplyChanges() and RevertChanges() are factored in this method, hiding
  /// both delta and deltadelta from subclasses which only need to override
  /// MakeOneNeighbor().
  /// Therefore this method should not be overridden. Override MakeOneNeighbor()
  /// instead.
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

 protected:
  friend class IntVarLocalSearchHandler;

  /// Creates a new neighbor. It returns false when the neighborhood is
  /// completely explored.
  // TODO(user): make it pure virtual, implies porting all apps overriding
  /// MakeNextNeighbor() in a subclass of IntVarLocalSearchOperator.
  virtual bool MakeOneNeighbor();

  bool IsInverseValue(int64 index) const {
    DCHECK_GE(index, 0);
    return index <= max_inverse_value_;
  }

  int64 InverseValue(int64 index) const { return inverse_values_[index]; }

  int64 OldInverseValue(int64 index) const {
    return old_inverse_values_[index];
  }

  void SetInverseValue(int64 index, int64 value) {
    inverse_values_[index] = value;
  }

  void SetOldInverseValue(int64 index, int64 value) {
    old_inverse_values_[index] = value;
  }

 private:
  const int64 max_inverse_value_;
  std::vector<int64> old_inverse_values_;
  std::vector<int64> inverse_values_;
};

inline bool IntVarLocalSearchHandler::ValueFromAssignment(
    const Assignment& assignment, IntVar* var, int64 index, int64* value) {
  const Assignment::IntContainer& container = assignment.IntVarContainer();
  const IntVarElement* element = &(container.Element(index));
  if (element->Var() != var) {
    CHECK(container.Contains(var))
        << "Assignment does not contain operator variable " << var;
    element = &(container.Element(var));
  }
  *value = element->Value();
  if (op_->IsInverseValue(index)) {
    op_->SetInverseValue(*value, index);
    op_->SetOldInverseValue(*value, index);
  }
  return element->Activated();
}

inline void IntVarLocalSearchHandler::OnRevertChanges(int64 index,
                                                      int64 value) {
  if (op_->IsInverseValue(index)) {
    op_->SetInverseValue(value, index);
  }
}

/// SequenceVarLocalSearchOperator
class SequenceVarLocalSearchOperator;

class SequenceVarLocalSearchHandler {
 public:
  SequenceVarLocalSearchHandler() : op_(nullptr) {}
  SequenceVarLocalSearchHandler(const SequenceVarLocalSearchHandler& other)
      : op_(other.op_) {}
  explicit SequenceVarLocalSearchHandler(SequenceVarLocalSearchOperator* op)
      : op_(op) {}
  void AddToAssignment(SequenceVar* var, const std::vector<int>& value,
                       bool active, std::vector<int>* assignment_indices,
                       int64 index, Assignment* assignment) const;
  bool ValueFromAssignment(const Assignment& assignment, SequenceVar* var,
                           int64 index, std::vector<int>* value);
  void OnRevertChanges(int64 index, const std::vector<int>& value);
  void OnAddVars();

 private:
  SequenceVarLocalSearchOperator* const op_;
};

#ifdef SWIG
/// Unfortunately, we must put this code here and not in
/// */constraint_solver.i, because it must be parsed by SWIG before the
/// derived C++ class.
// TODO(user): find a way to move this code back to the .i file, where it
/// belongs.
// clang-format off
%rename(SequenceVarLocalSearchOperatorTemplate) VarLocalSearchOperator<
    SequenceVar, std::vector<int>, SequenceVarLocalSearchHandler>;
%template(SequenceVarLocalSearchOperatorTemplate) VarLocalSearchOperator<
      SequenceVar, std::vector<int>, SequenceVarLocalSearchHandler>;
// clang-format on
#endif

typedef VarLocalSearchOperator<SequenceVar, std::vector<int>,
                               SequenceVarLocalSearchHandler>
    SequenceVarLocalSearchOperatorTemplate;

class SequenceVarLocalSearchOperator
    : public SequenceVarLocalSearchOperatorTemplate {
 public:
  SequenceVarLocalSearchOperator() {}
  explicit SequenceVarLocalSearchOperator(const std::vector<SequenceVar*>& vars)
      : SequenceVarLocalSearchOperatorTemplate(
            SequenceVarLocalSearchHandler(this)) {
    AddVars(vars);
  }
  ~SequenceVarLocalSearchOperator() override {}
  /// Returns the value in the current assignment of the variable of given
  /// index.
  const std::vector<int>& Sequence(int64 index) const { return Value(index); }
  const std::vector<int>& OldSequence(int64 index) const {
    return OldValue(index);
  }
  void SetForwardSequence(int64 index, const std::vector<int>& value) {
    SetValue(index, value);
  }
  void SetBackwardSequence(int64 index, const std::vector<int>& value) {
    backward_values_[index] = value;
    MarkChange(index);
  }

 protected:
  friend class SequenceVarLocalSearchHandler;

  std::vector<std::vector<int>> backward_values_;
};

inline void SequenceVarLocalSearchHandler::AddToAssignment(
    SequenceVar* var, const std::vector<int>& value, bool active,
    std::vector<int>* assignment_indices, int64 index,
    Assignment* assignment) const {
  Assignment::SequenceContainer* const container =
      assignment->MutableSequenceVarContainer();
  SequenceVarElement* element = nullptr;
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
    element->SetForwardSequence(value);
    element->SetBackwardSequence(op_->backward_values_[index]);
    element->Activate();
  } else {
    element->Deactivate();
  }
}

inline bool SequenceVarLocalSearchHandler::ValueFromAssignment(
    const Assignment& assignment, SequenceVar* var, int64 index,
    std::vector<int>* value) {
  const Assignment::SequenceContainer& container =
      assignment.SequenceVarContainer();
  const SequenceVarElement* element = &(container.Element(index));
  if (element->Var() != var) {
    CHECK(container.Contains(var))
        << "Assignment does not contain operator variable " << var;
    element = &(container.Element(var));
  }
  const std::vector<int>& element_value = element->ForwardSequence();
  CHECK_GE(var->size(), element_value.size());
  op_->backward_values_[index].clear();
  *value = element_value;
  return element->Activated();
}

inline void SequenceVarLocalSearchHandler::OnRevertChanges(
    int64 index, const std::vector<int>& value) {
  op_->backward_values_[index].clear();
}

inline void SequenceVarLocalSearchHandler::OnAddVars() {
  op_->backward_values_.resize(op_->Size());
}

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
  virtual int64 ModifyValue(int64 index, int64 value) = 0;

 protected:
  /// This method should not be overridden. Override ModifyValue() instead.
  bool MakeOneNeighbor() override;

 private:
  void OnStart() override;

  int index_;
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
class PathOperator : public IntVarLocalSearchOperator {
 public:
  /// Builds an instance of PathOperator from next and path variables.
  /// 'number_of_base_nodes' is the number of nodes needed to define a
  /// neighbor. 'start_empty_path_class' is a callback returning an index such
  /// that if
  /// c1 = start_empty_path_class(StartNode(p1)),
  /// c2 = start_empty_path_class(StartNode(p2)),
  /// p1 and p2 are path indices,
  /// then if c1 == c2, p1 and p2 are equivalent if they are empty.
  /// This is used to remove neighborhood symmetries on equivalent empty paths;
  /// for instance if a node cannot be moved to an empty path, then all moves
  /// moving the same node to equivalent empty paths will be skipped.
  /// 'start_empty_path_class' can be nullptr in which case no symmetries will
  /// be removed.
  PathOperator(const std::vector<IntVar*>& next_vars,
               const std::vector<IntVar*>& path_vars, int number_of_base_nodes,
               bool skip_locally_optimal_paths, bool accept_path_end_base,
               std::function<int(int64)> start_empty_path_class);
  ~PathOperator() override {}
  virtual bool MakeNeighbor() = 0;
  void Reset() override;

  // TODO(user): Make the following methods protected.
  bool SkipUnchanged(int index) const override;

  /// Returns the node after node in the current delta.
  int64 Next(int64 node) const {
    DCHECK(!IsPathEnd(node));
    return Value(node);
  }

  /// Returns the node before node in the current delta.
  int64 Prev(int64 node) const {
    DCHECK(!IsPathStart(node));
    DCHECK_EQ(Next(InverseValue(node)), node);
    return InverseValue(node);
  }

  /// Returns the index of the path to which node belongs in the current delta.
  /// Only returns a valid value if path variables are taken into account.
  int64 Path(int64 node) const {
    return ignore_path_vars_ ? 0LL : Value(node + number_of_nexts_);
  }

  /// Number of next variables.
  int number_of_nexts() const { return number_of_nexts_; }

 protected:
  /// This method should not be overridden. Override MakeNeighbor() instead.
  bool MakeOneNeighbor() override;
  /// Called by OnStart() after initializing node information. Should be
  /// overridden instead of OnStart() to avoid calling PathOperator::OnStart
  /// explicitly.
  virtual void OnNodeInitialization() {}

  /// Returns the ith base node of the operator.
  int64 BaseNode(int i) const { return base_nodes_[i]; }
  /// Returns the alternative for the ith base node.
  int BaseAlternative(int i) const { return base_alternatives_[i]; }
  /// Returns the alternative node for the ith base node.
  int64 BaseAlternativeNode(int i) const {
    if (!ConsiderAlternatives(i)) return BaseNode(i);
    const int alternative_index = alternative_index_[BaseNode(i)];
    return alternative_index >= 0
               ? alternative_sets_[alternative_index][base_alternatives_[i]]
               : BaseNode(i);
  }
  /// Returns the alternative for the sibling of the ith base node.
  int BaseSiblingAlternative(int i) const {
    return base_sibling_alternatives_[i];
  }
  /// Returns the alternative node for the sibling of the ith base node.
  int64 BaseSiblingAlternativeNode(int i) const {
    if (!ConsiderAlternatives(i)) return BaseNode(i);
    const int sibling_alternative_index =
        GetSiblingAlternativeIndex(BaseNode(i));
    return sibling_alternative_index >= 0
               ? alternative_sets_[sibling_alternative_index]
                                  [base_sibling_alternatives_[i]]
               : BaseNode(i);
  }
  /// Returns the start node of the ith base node.
  int64 StartNode(int i) const { return path_starts_[base_paths_[i]]; }
  /// Returns the vector of path start nodes.
  const std::vector<int64>& path_starts() const { return path_starts_; }
  /// Returns the class of the path of the ith base node.
  int PathClass(int i) const {
    return start_empty_path_class_ != nullptr
               ? start_empty_path_class_(StartNode(i))
               : StartNode(i);
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
  // TODO(user): ideally this should be OnSamePath(int64 node1, int64 node2);
  /// it's currently way more complicated to implement.
  virtual bool OnSamePathAsPreviousBase(int64 base_index) { return false; }
  /// Returns the index of the node to which the base node of index base_index
  /// must be set to when it reaches the end of a path.
  /// By default, it is set to the start of the current path.
  /// When this method is called, one can only assume that base nodes with
  /// indices < base_index have their final position.
  virtual int64 GetBaseNodeRestartPosition(int base_index) {
    return StartNode(base_index);
  }
  /// Set the next base to increment on next iteration. All base > base_index
  /// will be reset to their start value.
  virtual void SetNextBaseToIncrement(int64 base_index) {
    next_base_to_increment_ = base_index;
  }
  /// Indicates if alternatives should be considered when iterating over base
  /// nodes.
  virtual bool ConsiderAlternatives(int64 base_index) const { return false; }

  int64 OldNext(int64 node) const {
    DCHECK(!IsPathEnd(node));
    return OldValue(node);
  }

  int64 OldPrev(int64 node) const {
    DCHECK(!IsPathStart(node));
    return OldInverseValue(node);
  }

  int64 OldPath(int64 node) const {
    return ignore_path_vars_ ? 0LL : OldValue(node + number_of_nexts_);
  }

  /// Moves the chain starting after the node before_chain and ending at the
  /// node chain_end after the node destination
  bool MoveChain(int64 before_chain, int64 chain_end, int64 destination);

  /// Reverses the chain starting after before_chain and ending before
  /// after_chain
  bool ReverseChain(int64 before_chain, int64 after_chain, int64* chain_last);

  /// Insert the inactive node after destination.
  bool MakeActive(int64 node, int64 destination);
  /// Makes the nodes on the chain starting after before_chain and ending at
  /// chain_end inactive.
  bool MakeChainInactive(int64 before_chain, int64 chain_end);
  /// Replaces active by inactive in the current path, making active inactive.
  bool SwapActiveAndInactive(int64 active, int64 inactive);

  /// Sets 'to' to be the node after 'from' on the given path.
  void SetNext(int64 from, int64 to, int64 path) {
    DCHECK_LT(from, number_of_nexts_);
    SetValue(from, to);
    SetInverseValue(to, from);
    if (!ignore_path_vars_) {
      DCHECK_LT(from + number_of_nexts_, Size());
      SetValue(from + number_of_nexts_, path);
    }
  }

  /// Returns true if node is the last node on the path; defined by the fact
  /// that node is outside the range of the variable array.
  bool IsPathEnd(int64 node) const { return node >= number_of_nexts_; }

  /// Returns true if node is the first node on the path.
  bool IsPathStart(int64 node) const { return OldInverseValue(node) == -1; }

  /// Returns true if node is inactive.
  bool IsInactive(int64 node) const {
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
  /// two altrnatives.
  int AddAlternativeSet(const std::vector<int64>& alternative_set) {
    const int alternative = alternative_sets_.size();
    for (int64 node : alternative_set) {
      DCHECK_EQ(-1, alternative_index_[node]);
      alternative_index_[node] = alternative;
    }
    alternative_sets_.push_back(alternative_set);
    sibling_alternative_.push_back(-1);
    return alternative;
  }
#ifndef SWIG
  /// Adds all sets of node alternatives of a vector of alternative pairs. No
  /// node can be in two altrnatives.
  void AddPairAlternativeSets(
      const std::vector<std::pair<std::vector<int64>, std::vector<int64>>>&
          pair_alternative_sets) {
    for (const auto& pair_alternative_set : pair_alternative_sets) {
      const int alternative = AddAlternativeSet(pair_alternative_set.first);
      sibling_alternative_.back() = alternative + 1;
      AddAlternativeSet(pair_alternative_set.second);
    }
  }
#endif  // SWIG
  /// Returns the active node in the given alternative set.
  int64 GetActiveInAlternativeSet(int alternative_index) const {
    return alternative_index >= 0
               ? active_in_alternative_set_[alternative_index]
               : -1;
  }
  /// Returns the active node in the alternative set of the given node.
  int64 GetActiveAlternativeNode(int node) const {
    return GetActiveInAlternativeSet(alternative_index_[node]);
  }
  /// Returns the index of the alternative set of the sibling of node.
  int GetSiblingAlternativeIndex(int node) const {
    if (node >= alternative_index_.size()) return -1;
    const int alternative = alternative_index_[node];
    return alternative >= 0 ? sibling_alternative_[alternative] : -1;
  }
  /// Returns the active node in the alternative set of the sibling of the given
  /// node.
  int64 GetActiveAlternativeSibling(int node) const {
    if (node >= alternative_index_.size()) return -1;
    const int alternative = alternative_index_[node];
    const int sibling_alternative =
        alternative >= 0 ? sibling_alternative_[alternative] : -1;
    return GetActiveInAlternativeSet(sibling_alternative);
  }
  /// Returns true if the chain is a valid path without cycles from before_chain
  /// to chain_end and does not contain exclude.
  bool CheckChainValidity(int64 before_chain, int64 chain_end,
                          int64 exclude) const;

  const int number_of_nexts_;
  const bool ignore_path_vars_;
  int next_base_to_increment_;
  int num_paths_ = 0;
  std::vector<int64> start_to_path_;

 private:
  void OnStart() override;
  /// Returns true if two nodes are on the same path in the current assignment.
  bool OnSamePath(int64 node1, int64 node2) const;

  bool CheckEnds() const {
    const int base_node_size = base_nodes_.size();
    for (int i = base_node_size - 1; i >= 0; --i) {
      if (base_nodes_[i] != end_nodes_[i]) {
        return true;
      }
    }
    return false;
  }
  bool IncrementPosition();
  void InitializePathStarts();
  void InitializeInactives();
  void InitializeBaseNodes();
  void InitializeAlternatives();
  void Synchronize();

  std::vector<int> base_nodes_;
  std::vector<int> base_alternatives_;
  std::vector<int> base_sibling_alternatives_;
  std::vector<int> end_nodes_;
  std::vector<int> base_paths_;
  std::vector<int64> path_starts_;
  std::vector<bool> inactives_;
  bool just_started_;
  bool first_start_;
  const bool accept_path_end_base_;
  std::function<int(int64)> start_empty_path_class_;
  bool skip_locally_optimal_paths_;
  bool optimal_paths_enabled_;
  std::vector<int> path_basis_;
  std::vector<bool> optimal_paths_;
  /// Node alternative data.
#ifndef SWIG
  std::vector<std::vector<int64>> alternative_sets_;
#endif  // SWIG
  std::vector<int> alternative_index_;
  std::vector<int64> active_in_alternative_set_;
  std::vector<int> sibling_alternative_;
};

/// Operator Factories.
template <class T>
LocalSearchOperator* MakeLocalSearchOperator(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class);

/// Classes to which this template function can be applied to as of 04/2014.
/// Usage: LocalSearchOperator* op = MakeLocalSearchOperator<Relocate>(...);
class TwoOpt;
class Relocate;
class Exchange;
class Cross;
class MakeActiveOperator;
class MakeInactiveOperator;
class MakeChainInactiveOperator;
class SwapActiveOperator;
class ExtendedSwapActiveOperator;
class MakeActiveAndRelocate;
class RelocateAndMakeActiveOperator;
class RelocateAndMakeInactiveOperator;

#if !defined(SWIG)
// A LocalSearchState is a container for variables with bounds that can be
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
class LocalSearchVariable;
class LocalSearchState {
 public:
  LocalSearchVariable AddVariable(int64 initial_min, int64 initial_max);
  void Commit();
  void Revert();
  bool StateIsValid() const { return state_is_valid_; }

 private:
  friend class LocalSearchVariable;

  struct Bounds {
    int64 min;
    int64 max;
  };

  void RelaxVariableBounds(int variable_index);
  bool TightenVariableMin(int variable_index, int64 value);
  bool TightenVariableMax(int variable_index, int64 value);
  int64 VariableMin(int variable_index) const;
  int64 VariableMax(int variable_index) const;

  std::vector<Bounds> initial_variable_bounds_;
  std::vector<Bounds> variable_bounds_;
  std::vector<std::pair<Bounds, int>> saved_variable_bounds_trail_;
  std::vector<bool> variable_is_relaxed_;
  bool state_is_valid_ = true;
};

// A LocalSearchVariable can only be created by a LocalSearchState, then it is
// meant to be passed by copy. If at some point the duplication of
// LocalSearchState pointers is too expensive, we could switch to index only,
// and the user would have to know the relevant state. The present setup allows
// to ensure that variable users will not misuse the state.
class LocalSearchVariable {
 public:
  int64 Min() const { return state_->VariableMin(variable_index_); }
  int64 Max() const { return state_->VariableMax(variable_index_); }
  bool SetMin(int64 new_min) {
    return state_->TightenVariableMin(variable_index_, new_min);
  }
  bool SetMax(int64 new_max) {
    return state_->TightenVariableMax(variable_index_, new_max);
  }
  void Relax() { state_->RelaxVariableBounds(variable_index_); }

 private:
  // Only LocalSearchState can construct LocalSearchVariables.
  friend class LocalSearchState;

  LocalSearchVariable(LocalSearchState* state, int variable_index)
      : state_(state), variable_index_(variable_index) {}

  LocalSearchState* const state_;
  const int variable_index_;
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
  virtual void Relax(const Assignment* delta, const Assignment* deltadelta) {}
  /// Dual of Relax(), lets the filter know that the delta was accepted.
  virtual void Commit(const Assignment* delta, const Assignment* deltadelta) {}

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
                      int64 objective_min, int64 objective_max) = 0;
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
  virtual int64 GetSynchronizedObjectiveValue() const { return 0LL; }
  /// Objective value from the last time Accept() was called and returned true.
  // If the last Accept() call returned false, returns an undefined value.
  virtual int64 GetAcceptedObjectiveValue() const { return 0LL; }
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
  };

  std::string DebugString() const override {
    return "LocalSearchFilterManager";
  }
  // Builds a manager that calls filter methods using an explicit ordering.
  explicit LocalSearchFilterManager(std::vector<FilterEvent> filter_events);
  // Builds a manager that calls filter methods using the following ordering:
  // first Relax() in vector order, then Accept() in vector order.
  // Note that some filters might appear only once, if their Relax() or Accept()
  // are trivial.
  explicit LocalSearchFilterManager(std::vector<LocalSearchFilter*> filters);

  // Calls Revert() of filters, in reverse order of Relax events.
  void Revert();
  /// Returns true iff all filters return true, and the sum of their accepted
  /// objectives is between objective_min and objective_max.
  /// The monitor has its Begin/EndFiltering events triggered.
  bool Accept(LocalSearchMonitor* const monitor, const Assignment* delta,
              const Assignment* deltadelta, int64 objective_min,
              int64 objective_max);
  /// Synchronizes all filters to assignment.
  void Synchronize(const Assignment* assignment, const Assignment* delta);
  int64 GetSynchronizedObjectiveValue() const { return synchronized_value_; }
  int64 GetAcceptedObjectiveValue() const { return accepted_value_; }

 private:
  void InitializeForcedEvents();

  std::vector<FilterEvent> filter_events_;
  int last_event_called_ = -1;
  // If a filter is incremental, its Relax() and Accept() must be called for
  // every candidate, even if a previous Accept() rejected it.
  // To ensure that those filters have consistent inputs, all intermediate
  // Relax events are also triggered. All those events are called 'forced'.
  std::vector<int> next_forced_events_;
  int64 synchronized_value_;
  int64 accepted_value_;
};

class IntVarLocalSearchFilter : public LocalSearchFilter {
 public:
  explicit IntVarLocalSearchFilter(const std::vector<IntVar*>& vars);
  ~IntVarLocalSearchFilter() override;
  /// This method should not be overridden. Override OnSynchronize() instead
  /// which is called before exiting this method.
  void Synchronize(const Assignment* assignment,
                   const Assignment* delta) override;

  bool FindIndex(IntVar* const var, int64* index) const {
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
  int64 Value(int index) const {
    DCHECK(IsVarSynced(index));
    return values_[index];
  }
  bool IsVarSynced(int index) const { return var_synced_[index]; }

 protected:
  virtual void OnSynchronize(const Assignment* delta) {}
  void SynchronizeOnAssignment(const Assignment* assignment);

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64> values_;
  std::vector<bool> var_synced_;
  std::vector<int> var_index_to_index_;
  static const int kUnassigned;
};

class PropagationMonitor : public SearchMonitor {
 public:
  explicit PropagationMonitor(Solver* const solver);
  ~PropagationMonitor() override;
  std::string DebugString() const override { return "PropagationMonitor"; }

  /// Propagation events.
  virtual void BeginConstraintInitialPropagation(
      Constraint* const constraint) = 0;
  virtual void EndConstraintInitialPropagation(
      Constraint* const constraint) = 0;
  virtual void BeginNestedConstraintInitialPropagation(
      Constraint* const parent, Constraint* const nested) = 0;
  virtual void EndNestedConstraintInitialPropagation(
      Constraint* const parent, Constraint* const nested) = 0;
  virtual void RegisterDemon(Demon* const demon) = 0;
  virtual void BeginDemonRun(Demon* const demon) = 0;
  virtual void EndDemonRun(Demon* const demon) = 0;
  virtual void StartProcessingIntegerVariable(IntVar* const var) = 0;
  virtual void EndProcessingIntegerVariable(IntVar* const var) = 0;
  virtual void PushContext(const std::string& context) = 0;
  virtual void PopContext() = 0;
  /// IntExpr modifiers.
  virtual void SetMin(IntExpr* const expr, int64 new_min) = 0;
  virtual void SetMax(IntExpr* const expr, int64 new_max) = 0;
  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) = 0;
  /// IntVar modifiers.
  virtual void SetMin(IntVar* const var, int64 new_min) = 0;
  virtual void SetMax(IntVar* const var, int64 new_max) = 0;
  virtual void SetRange(IntVar* const var, int64 new_min, int64 new_max) = 0;
  virtual void RemoveValue(IntVar* const var, int64 value) = 0;
  virtual void SetValue(IntVar* const var, int64 value) = 0;
  virtual void RemoveInterval(IntVar* const var, int64 imin, int64 imax) = 0;
  virtual void SetValues(IntVar* const var,
                         const std::vector<int64>& values) = 0;
  virtual void RemoveValues(IntVar* const var,
                            const std::vector<int64>& values) = 0;
  /// IntervalVar modifiers.
  virtual void SetStartMin(IntervalVar* const var, int64 new_min) = 0;
  virtual void SetStartMax(IntervalVar* const var, int64 new_max) = 0;
  virtual void SetStartRange(IntervalVar* const var, int64 new_min,
                             int64 new_max) = 0;
  virtual void SetEndMin(IntervalVar* const var, int64 new_min) = 0;
  virtual void SetEndMax(IntervalVar* const var, int64 new_max) = 0;
  virtual void SetEndRange(IntervalVar* const var, int64 new_min,
                           int64 new_max) = 0;
  virtual void SetDurationMin(IntervalVar* const var, int64 new_min) = 0;
  virtual void SetDurationMax(IntervalVar* const var, int64 new_max) = 0;
  virtual void SetDurationRange(IntervalVar* const var, int64 new_min,
                                int64 new_max) = 0;
  virtual void SetPerformed(IntervalVar* const var, bool value) = 0;
  /// SequenceVar modifiers
  virtual void RankFirst(SequenceVar* const var, int index) = 0;
  virtual void RankNotFirst(SequenceVar* const var, int index) = 0;
  virtual void RankLast(SequenceVar* const var, int index) = 0;
  virtual void RankNotLast(SequenceVar* const var, int index) = 0;
  virtual void RankSequence(SequenceVar* const var,
                            const std::vector<int>& rank_first,
                            const std::vector<int>& rank_last,
                            const std::vector<int>& unperformed) = 0;
  /// Install itself on the solver.
  void Install() override;
};

class LocalSearchMonitor : public SearchMonitor {
  // TODO(user): Add monitoring of local search filters.
 public:
  explicit LocalSearchMonitor(Solver* const solver);
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

  /// Install itself on the solver.
  void Install() override;
};

class BooleanVar : public IntVar {
 public:
  static const int kUnboundBooleanVarValue;

  explicit BooleanVar(Solver* const s, const std::string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue) {}

  ~BooleanVar() override {}

  int64 Min() const override { return (value_ == 1); }
  void SetMin(int64 m) override;
  int64 Max() const override { return (value_ != 0); }
  void SetMax(int64 m) override;
  void SetRange(int64 mi, int64 ma) override;
  bool Bound() const override { return (value_ != kUnboundBooleanVarValue); }
  int64 Value() const override {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  void RemoveValue(int64 v) override;
  void RemoveInterval(int64 l, int64 u) override;
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override { WhenBound(d); }
  void WhenDomain(Demon* d) override { WhenBound(d); }
  uint64 Size() const override;
  bool Contains(int64 v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  std::string DebugString() const override;
  int VarType() const override { return BOOLEAN_VAR; }

  IntVar* IsEqual(int64 constant) override;
  IntVar* IsDifferent(int64 constant) override;
  IntVar* IsGreaterOrEqual(int64 constant) override;
  IntVar* IsLessOrEqual(int64 constant) override;

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

  void AddIntegerVariableEqualValueClause(IntVar* const var, int64 value);
  void AddIntegerVariableGreaterOrEqualValueClause(IntVar* const var,
                                                   int64 value);
  void AddIntegerVariableLessOrEqualValueClause(IntVar* const var, int64 value);

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
  SearchLog(Solver* const s, OptimizeVar* const obj, IntVar* const var,
            double scaling_factor, double offset,
            std::function<std::string()> display_callback,
            bool display_on_new_solutions_only, int period);
  ~SearchLog() override;
  void EnterSearch() override;
  void ExitSearch() override;
  bool AtSolution() override;
  void BeginFail() override;
  void NoMoreSolutions() override;
  void AcceptUncheckedNeighbor() override;
  void ApplyDecision(Decision* const decision) override;
  void RefuteDecision(Decision* const decision) override;
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
  IntVar* const var_;
  OptimizeVar* const obj_;
  const double scaling_factor_;
  const double offset_;
  std::function<std::string()> display_callback_;
  const bool display_on_new_solutions_only_;
  int nsol_;
  int64 tick_;
  int64 objective_min_;
  int64 objective_max_;
  int min_right_depth_;
  int max_depth_;
  int sliding_min_depth_;
  int sliding_max_depth_;
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

  explicit ModelCache(Solver* const solver);
  virtual ~ModelCache();

  virtual void Clear() = 0;

  /// Void constraints.

  virtual Constraint* FindVoidConstraint(VoidConstraintType type) const = 0;

  virtual void InsertVoidConstraint(Constraint* const ct,
                                    VoidConstraintType type) = 0;

  /// Var Constant Constraints.
  virtual Constraint* FindVarConstantConstraint(
      IntVar* const var, int64 value, VarConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstraint(Constraint* const ct,
                                           IntVar* const var, int64 value,
                                           VarConstantConstraintType type) = 0;

  /// Var Constant Constant Constraints.

  virtual Constraint* FindVarConstantConstantConstraint(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstantConstraint(
      Constraint* const ct, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) = 0;

  /// Expr Expr Constraints.

  virtual Constraint* FindExprExprConstraint(
      IntExpr* const expr1, IntExpr* const expr2,
      ExprExprConstraintType type) const = 0;

  virtual void InsertExprExprConstraint(Constraint* const ct,
                                        IntExpr* const expr1,
                                        IntExpr* const expr2,
                                        ExprExprConstraintType type) = 0;

  /// Expr Expressions.

  virtual IntExpr* FindExprExpression(IntExpr* const expr,
                                      ExprExpressionType type) const = 0;

  virtual void InsertExprExpression(IntExpr* const expression,
                                    IntExpr* const expr,
                                    ExprExpressionType type) = 0;

  /// Expr Constant Expressions.

  virtual IntExpr* FindExprConstantExpression(
      IntExpr* const expr, int64 value,
      ExprConstantExpressionType type) const = 0;

  virtual void InsertExprConstantExpression(
      IntExpr* const expression, IntExpr* const var, int64 value,
      ExprConstantExpressionType type) = 0;

  /// Expr Expr Expressions.

  virtual IntExpr* FindExprExprExpression(
      IntExpr* const var1, IntExpr* const var2,
      ExprExprExpressionType type) const = 0;

  virtual void InsertExprExprExpression(IntExpr* const expression,
                                        IntExpr* const var1,
                                        IntExpr* const var2,
                                        ExprExprExpressionType type) = 0;

  /// Expr Expr Constant Expressions.

  virtual IntExpr* FindExprExprConstantExpression(
      IntExpr* const var1, IntExpr* const var2, int64 constant,
      ExprExprConstantExpressionType type) const = 0;

  virtual void InsertExprExprConstantExpression(
      IntExpr* const expression, IntExpr* const var1, IntExpr* const var2,
      int64 constant, ExprExprConstantExpressionType type) = 0;

  /// Var Constant Constant Expressions.

  virtual IntExpr* FindVarConstantConstantExpression(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) const = 0;

  virtual void InsertVarConstantConstantExpression(
      IntExpr* const expression, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) = 0;

  /// Var Constant Array Expressions.

  virtual IntExpr* FindVarConstantArrayExpression(
      IntVar* const var, const std::vector<int64>& values,
      VarConstantArrayExpressionType type) const = 0;

  virtual void InsertVarConstantArrayExpression(
      IntExpr* const expression, IntVar* const var,
      const std::vector<int64>& values,
      VarConstantArrayExpressionType type) = 0;

  /// Var Array Expressions.

  virtual IntExpr* FindVarArrayExpression(
      const std::vector<IntVar*>& vars, VarArrayExpressionType type) const = 0;

  virtual void InsertVarArrayExpression(IntExpr* const expression,
                                        const std::vector<IntVar*>& vars,
                                        VarArrayExpressionType type) = 0;

  /// Var Array Constant Array Expressions.

  virtual IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) const = 0;

  virtual void InsertVarArrayConstantArrayExpression(
      IntExpr* const expression, const std::vector<IntVar*>& var,
      const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) = 0;

  /// Var Array Constant Expressions.

  virtual IntExpr* FindVarArrayConstantExpression(
      const std::vector<IntVar*>& vars, int64 value,
      VarArrayConstantExpressionType type) const = 0;

  virtual void InsertVarArrayConstantExpression(
      IntExpr* const expression, const std::vector<IntVar*>& var, int64 value,
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
  void SetIntegerArgument(const std::string& arg_name, int64 value);
  void SetIntegerArrayArgument(const std::string& arg_name,
                               const std::vector<int64>& values);
  void SetIntegerMatrixArgument(const std::string& arg_name,
                                const IntTupleSet& values);
  void SetIntegerExpressionArgument(const std::string& arg_name,
                                    IntExpr* const expr);
  void SetIntegerVariableArrayArgument(const std::string& arg_name,
                                       const std::vector<IntVar*>& vars);
  void SetIntervalArgument(const std::string& arg_name, IntervalVar* const var);
  void SetIntervalArrayArgument(const std::string& arg_name,
                                const std::vector<IntervalVar*>& vars);
  void SetSequenceArgument(const std::string& arg_name, SequenceVar* const var);
  void SetSequenceArrayArgument(const std::string& arg_name,
                                const std::vector<SequenceVar*>& vars);

  /// Checks if arguments exist.
  bool HasIntegerExpressionArgument(const std::string& arg_name) const;
  bool HasIntegerVariableArrayArgument(const std::string& arg_name) const;

  /// Getters.
  int64 FindIntegerArgumentWithDefault(const std::string& arg_name,
                                       int64 def) const;
  int64 FindIntegerArgumentOrDie(const std::string& arg_name) const;
  const std::vector<int64>& FindIntegerArrayArgumentOrDie(
      const std::string& arg_name) const;
  const IntTupleSet& FindIntegerMatrixArgumentOrDie(
      const std::string& arg_name) const;

  IntExpr* FindIntegerExpressionArgumentOrDie(
      const std::string& arg_name) const;
  const std::vector<IntVar*>& FindIntegerVariableArrayArgumentOrDie(
      const std::string& arg_name) const;

 private:
  std::string type_name_;
  absl::flat_hash_map<std::string, int64> integer_argument_;
  absl::flat_hash_map<std::string, std::vector<int64>> integer_array_argument_;
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
                            const Constraint* const constraint) override;
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override;
  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* const expr) override;
  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* const expr) override;
  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override;
  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override;
  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override;
  void VisitSequenceVariable(const SequenceVar* const variable) override;
  /// Integer arguments
  void VisitIntegerArgument(const std::string& arg_name, int64 value) override;
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override;
  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override;
  /// Variables.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override;
  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name,
      const std::vector<IntVar*>& arguments) override;
  /// Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override;
  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override;
  /// Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* const argument) override;
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
  ArrayWithOffset(int64 index_min, int64 index_max)
      : index_min_(index_min),
        index_max_(index_max),
        values_(new T[index_max - index_min + 1]) {
    DCHECK_LE(index_min, index_max);
  }

  ~ArrayWithOffset() override {}

  virtual T Evaluate(int64 index) const {
    DCHECK_GE(index, index_min_);
    DCHECK_LE(index, index_max_);
    return values_[index - index_min_];
  }

  void SetValue(int64 index, T value) {
    DCHECK_GE(index, index_min_);
    DCHECK_LE(index, index_max_);
    values_[index - index_min_] = value;
  }

  std::string DebugString() const override { return "ArrayWithOffset"; }

 private:
  const int64 index_min_;
  const int64 index_max_;
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
  explicit RevGrowingArray(int64 block_size)
      : block_size_(block_size), block_offset_(0) {
    CHECK_GT(block_size, 0);
  }

  ~RevGrowingArray() {
    for (int i = 0; i < elements_.size(); ++i) {
      delete[] elements_[i];
    }
  }

  T At(int64 index) const {
    const int64 block_index = ComputeBlockIndex(index);
    const int64 relative_index = block_index - block_offset_;
    if (relative_index < 0 || relative_index >= elements_.size()) {
      return T();
    }
    const T* block = elements_[relative_index];
    return block != nullptr ? block[index - block_index * block_size_] : T();
  }

  void RevInsert(Solver* const solver, int64 index, T value) {
    const int64 block_index = ComputeBlockIndex(index);
    T* const block = GetOrCreateBlock(block_index);
    const int64 residual = index - block_index * block_size_;
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

  int64 ComputeBlockIndex(int64 value) const {
    return value >= 0 ? value / block_size_
                      : (value - block_size_ + 1) / block_size_;
  }

  void GrowUp(int64 block_index) {
    elements_.resize(block_index - block_offset_ + 1);
  }

  void GrowDown(int64 block_index) {
    const int64 delta = block_offset_ - block_index;
    block_offset_ = block_index;
    DCHECK_GT(delta, 0);
    elements_.insert(elements_.begin(), delta, nullptr);
  }

  const int64 block_size_;
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
  void Init(Solver* const solver, const std::vector<uint64>& mask);

  /// This method subtracts the mask from the active bitset. It returns true if
  /// the active bitset was changed in the process.
  bool RevSubtract(Solver* const solver, const std::vector<uint64>& mask);

  /// This method ANDs the mask with the active bitset. It returns true if
  /// the active bitset was changed in the process.
  bool RevAnd(Solver* const solver, const std::vector<uint64>& mask);

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
  bool Intersects(const std::vector<uint64>& mask, int* support_index);

  /// Returns the number of bits given in the constructor of the bitset.
  int64 bit_size() const { return bit_size_; }
  /// Returns the number of 64 bit words used to store the bitset.
  int64 word_size() const { return word_size_; }
  /// Returns the set of active word indices.
  const RevIntSet<int>& active_words() const { return active_words_; }

 private:
  void CleanUpActives(Solver* const solver);

  const int64 bit_size_;
  const int64 word_size_;
  RevArray<uint64> bits_;
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
inline bool AreAllBoundTo(const std::vector<IntVar*>& vars, int64 value) {
  for (int i = 0; i < vars.size(); ++i) {
    if (!vars[i]->Bound() || vars[i]->Min() != value) {
      return false;
    }
  }
  return true;
}

inline int64 MaxVarArray(const std::vector<IntVar*>& vars) {
  DCHECK(!vars.empty());
  int64 result = kint64min;
  for (int i = 0; i < vars.size(); ++i) {
    /// The std::max<int64> is needed for compilation on MSVC.
    result = std::max<int64>(result, vars[i]->Max());
  }
  return result;
}

inline int64 MinVarArray(const std::vector<IntVar*>& vars) {
  DCHECK(!vars.empty());
  int64 result = kint64max;
  for (int i = 0; i < vars.size(); ++i) {
    /// The std::min<int64> is needed for compilation on MSVC.
    result = std::min<int64>(result, vars[i]->Min());
  }
  return result;
}

inline void FillValues(const std::vector<IntVar*>& vars,
                       std::vector<int64>* const values) {
  values->clear();
  values->resize(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    (*values)[i] = vars[i]->Value();
  }
}

inline int64 PosIntDivUp(int64 e, int64 v) {
  DCHECK_GT(v, 0);
  return (e < 0 || e % v == 0) ? e / v : e / v + 1;
}

inline int64 PosIntDivDown(int64 e, int64 v) {
  DCHECK_GT(v, 0);
  return (e >= 0 || e % v == 0) ? e / v : e / v - 1;
}

std::vector<int64> ToInt64Vector(const std::vector<int>& input);

#if !defined(SWIG)
// A PathState represents a set of paths and changed made on it.
//
// More accurately, let us define P_{num_nodes, starts, ends}-graphs the set of
// directed graphs with nodes [0, num_nodes) whose connected components are
// paths from starts[i] to ends[i] (for the same i) and loops.
// Let us fix num_nodes, starts and ends so we call these P-graphs.
//
// Let us define some notions on graphs with the same set of nodes:
//   tails(D) is the set of nodes that are the tail of some arc of D.
//   P0 inter P1 is the graph of all arcs both in P0 and P1.
//   P0 union P1 is the graph of all arcs either in P0 or P1.
//   P1 - P0 is the graph with arcs in P1 and not in P0.
//   P0 |> D is the graph with arcs of P0 whose tail is not in tails(D).
//   P0 + D is (P0 |> D) union D.
//
// Now suppose P0 and P1 are P-graphs.
// P0 + (P1 - P0) is exactly P1.
// Moreover, note that P0 |> D is not a union of paths from some starts[i] to
// ends[i] and loops like P0, because the operation removes arcs from P0.
// P0 |> D is a union of generic paths, loops, and isolated nodes.
// Let us call the generic paths and isolated nodes "chains".
// Then the paths of P0 + D are chains linked by arcs of D.
// Those chains are particularly interesting when examining a P-graph change.
//
// A PathState represents a P-graph for a fixed {num_nodes, starts, ends}.
// The value of a PathState can be changed incrementally from P0 to P1
// by passing the arcs of P1 - P0 to ChangeNext() and marking the end of the
// change with a call to CutChains().
// If P0 + D is not a P-graph, the behaviour is undefined.
// TODO(user): check whether we want to have a DCHECK that P0 + D
//   is a P-graph or if CutChains() should return false.
//
// After CutChains(), tails(D) can be traversed using an iterator,
// and the chains of P0 |> D can be browsed by chain-based iterators.
// An iterator allows to browse the set of paths that have changed.
// Then Commit() or Revert() can be called: Commit() changes the PathState to
// represent P1 = P0 + D, all further changes are made from P1; Revert() changes
// the PathState to forget D completely and return the state to P0.
//
// After a Commit(), Revert() or at initial state, the same iterators are
// available and represent the change by an empty D: the set of changed paths
// and the set of changed nodes is empty. Still, the chain-based iterator allows
// to browse paths: each path has exactly one chain.
class PathState {
 public:
  // A ChainRange allows to iterate on all chains of a path.
  // ChainRange is a range, its iterator Chain*, its value type Chain.
  class ChainRange;
  // A Chain allows to iterate on all nodes of a chain, and access some data:
  // first node, last node, number of nodes in the chain.
  // Chain is a range, its iterator ChainNodeIterator, its value type int.
  // Chains are returned by PathChainIterator's operator*().
  class Chain;
  // A NodeRange allows to iterate on all nodes of a path.
  // NodeRange is a range, its iterator PathNodeIterator, its value type int.
  class NodeRange;

  // Path constructor: path_start and path_end must be disjoint,
  // their values in [0, num_nodes).
  PathState(int num_nodes, std::vector<int> path_start,
            std::vector<int> path_end);

  // Instance-constant accessors.

  // Returns the number of nodes in the underlying graph.
  int NumNodes() const { return num_nodes_; }
  // Returns the number of paths (empty paths included).
  int NumPaths() const { return num_paths_; }
  // Returns the start of a path.
  int Start(int path) const { return path_start_end_[path].start; }
  // Returns the end of a path.
  int End(int path) const { return path_start_end_[path].end; }

  // State-dependent accessors.

  // Returns the committed path of a given node, -1 if it is a loop.
  int Path(int node) const {
    return committed_nodes_[committed_index_[node]].path;
  }
  // Returns the set of arcs that have been added,
  // i.e. that were changed and were not in the committed state.
  const std::vector<std::pair<int, int>>& ChangedArcs() const {
    return changed_arcs_;
  }
  // Returns the set of paths that actually changed,
  // i.e. that have an arc in ChangedArcs().
  const std::vector<int>& ChangedPaths() const { return changed_paths_; }
  // Returns the current range of chains of path.
  ChainRange Chains(int path) const;
  // Returns the current range of nodes of path.
  NodeRange Nodes(int path) const;

  // State modifiers.

  // Adds arc (node, new_next) to the changed state, more formally,
  // changes the state from (P0, D) to (P0, D + (node, new_next)).
  void ChangeNext(int node, int new_next) {
    changed_arcs_.emplace_back(node, new_next);
  }
  // Marks the end of ChangeNext() sequence, more formally,
  // changes the state from (P0, D) to (P0 |> D, D).
  void CutChains();
  // Makes the current temporary state permanent, more formally,
  // changes the state from (P0 |> D, D) to (P0 + D, \emptyset),
  void Commit();
  // Erase incremental changes made by ChangeNext() and CutChains(),
  // more formally, changes the state from (P0 |> D, D) to (P0, \emptyset).
  void Revert();

  // LNS Operators may not fix variables,
  // in which case we mark the candidate invalid.
  void SetInvalid() { is_invalid_ = true; }
  bool IsInvalid() const { return is_invalid_; }

 private:
  // Most structs below are named pairs of ints, for typing purposes.

  // Start and end are stored together to optimize (likely) simultaneous access.
  struct PathStartEnd {
    PathStartEnd(int start, int end) : start(start), end(end) {}
    int start;
    int end;
  };
  // Paths are ranges of chains, which are ranges of committed nodes, see below.
  struct PathBounds {
    int begin_index;
    int end_index;
  };
  struct ChainBounds {
    ChainBounds() = default;
    ChainBounds(int begin_index, int end_index)
        : begin_index(begin_index), end_index(end_index) {}
    int begin_index;
    int end_index;
  };
  struct CommittedNode {
    CommittedNode(int node, int path) : node(node), path(path) {}
    int node;
    // Path of node in the committed state, -1 for loop nodes.
    // TODO(user): check if path would be better stored
    // with committed_index_, or in its own vector, or just recomputed.
    int path;
  };
  // Used in temporary structures, see below.
  struct TailHeadIndices {
    int tail_index;
    int head_index;
  };
  struct IndexArc {
    int index;
    int arc;
    bool operator<(const IndexArc& other) const { return index < other.index; }
  };

  // From changed_paths_ and changed_arcs_, fill chains_ and paths_.
  // Selection-based algorithm in O(n^2), to use for small change sets.
  void MakeChainsFromChangedPathsAndArcsWithSelectionAlgorithm();
  // From changed_paths_ and changed_arcs_, fill chains_ and paths_.
  // Generic algorithm in O(std::sort(n)+n), to use for larger change sets.
  void MakeChainsFromChangedPathsAndArcsWithGenericAlgorithm();

  // Copies nodes in chains of path at the end of nodes,
  // and sets those nodes' path member to value path.
  void CopyNewPathAtEndOfNodes(int path);
  // Commits paths in O(#{changed paths' nodes}) time,
  // increasing this object's space usage by O(|changed path nodes|).
  void IncrementalCommit();
  // Commits paths in O(num_nodes + num_paths) time,
  // reducing this object's space usage to O(num_nodes + num_paths).
  void FullCommit();

  // Instance-constant data.
  const int num_nodes_;
  const int num_paths_;
  std::vector<PathStartEnd> path_start_end_;

  // Representation of the committed and changed paths.
  // A path is a range of chains, which is a range of nodes.
  // Ranges are represented internally by indices in vectors:
  // ChainBounds are indices in committed_nodes_. PathBounds are indices in
  // chains_. When committed (after construction, Revert() or Commit()):
  // - path ranges are [path, path+1): they have one chain.
  // - chain ranges don't overlap, chains_ has an empty sentinel at the end.
  // - committed_nodes_ contains all nodes and old duplicates may appear,
  //   the current version of a node is at the index given by
  //   committed_index_[node]. A Commit() can add nodes at the end of
  //   committed_nodes_ in a space/time tradeoff, but if committed_nodes_' size
  //   is above num_nodes_threshold_, Commit() must reclaim useless duplicates'
  //   space by rewriting the path/chain/nodes structure.
  // When changed (after CutChains()), new chains are computed,
  // and the structure is updated accordingly:
  // - path ranges that were changed have nonoverlapping values [begin, end)
  //   where begin is >= num_paths_ + 1, i.e. new chains are stored after
  //   committed state.
  // - additional chain ranges are stored after the committed chains
  //   to represent the new chains resulting from the changes.
  //   Those chains do not overlap with each other or with unchanged chains.
  //   An empty sentinel chain is added at the end of additional chains.
  // - committed_nodes_ are not modified, and still represent the committed
  // paths.
  //   committed_index_ is not modified either.
  std::vector<CommittedNode> committed_nodes_;
  std::vector<int> committed_index_;
  const int num_nodes_threshold_;
  std::vector<ChainBounds> chains_;
  std::vector<PathBounds> paths_;

  // Incremental information: indices of nodes whose successor have changed,
  // path that have changed nodes.
  std::vector<std::pair<int, int>> changed_arcs_;
  std::vector<int> changed_paths_;
  std::vector<bool> path_has_changed_;

  // Temporary structures, since they will be reused heavily,
  // those are members in order to be allocated once and for all.
  std::vector<TailHeadIndices> tail_head_indices_;
  std::vector<IndexArc> arcs_by_tail_index_;
  std::vector<IndexArc> arcs_by_head_index_;
  std::vector<int> next_arc_;

  // See IsInvalid() and SetInvalid().
  bool is_invalid_ = false;
};

// A Chain is a range of committed nodes.
class PathState::Chain {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      return *this;
    }
    int operator*() const { return current_node_->node; }
    bool operator!=(Iterator other) const {
      return current_node_ != other.current_node_;
    }

   private:
    // Only a Chain can construct its iterator.
    friend class PathState::Chain;
    explicit Iterator(const CommittedNode* node) : current_node_(node) {}
    const CommittedNode* current_node_;
  };

  // Chains hold CommittedNode* values, a Chain may be invalidated
  // if the underlying vector is modified.
  Chain(const CommittedNode* begin_node, const CommittedNode* end_node)
      : begin_(begin_node), end_(end_node) {}

  int NumNodes() const { return end_ - begin_; }
  int First() const { return begin_->node; }
  int Last() const { return (end_ - 1)->node; }
  Iterator begin() const { return Iterator(begin_); }
  Iterator end() const { return Iterator(end_); }

 private:
  const CommittedNode* const begin_;
  const CommittedNode* const end_;
};

// A ChainRange is a range of Chains, committed or not.
class PathState::ChainRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_chain_;
      return *this;
    }
    Chain operator*() const {
      return {first_node_ + current_chain_->begin_index,
              first_node_ + current_chain_->end_index};
    }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a ChainRange can construct its Iterator.
    friend class ChainRange;
    Iterator(const ChainBounds* chain, const CommittedNode* const first_node)
        : current_chain_(chain), first_node_(first_node) {}
    const ChainBounds* current_chain_;
    const CommittedNode* const first_node_;
  };

  // ChainRanges hold ChainBounds* and CommittedNode*,
  // a ChainRange may be invalidated if on of the underlying vector is modified.
  ChainRange(const ChainBounds* const begin_chain,
             const ChainBounds* const end_chain,
             const CommittedNode* const first_node)
      : begin_(begin_chain), end_(end_chain), first_node_(first_node) {}

  Iterator begin() const { return {begin_, first_node_}; }
  Iterator end() const { return {end_, first_node_}; }

 private:
  const ChainBounds* const begin_;
  const ChainBounds* const end_;
  const CommittedNode* const first_node_;
};

// A NodeRange allows to iterate on all nodes of a path,
// by a two-level iteration on ChainBounds* and CommittedNode* of a PathState.
class PathState::NodeRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      if (current_node_ == end_node_) {
        ++current_chain_;
        // Note: dereferencing bounds is valid because there is a sentinel
        // value at the end of PathState::chains_ to that intent.
        const ChainBounds bounds = *current_chain_;
        current_node_ = first_node_ + bounds.begin_index;
        end_node_ = first_node_ + bounds.end_index;
      }
      return *this;
    }
    int operator*() const { return current_node_->node; }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a NodeRange can construct its Iterator.
    friend class NodeRange;
    Iterator(const ChainBounds* current_chain,
             const CommittedNode* const first_node)
        : current_node_(first_node + current_chain->begin_index),
          end_node_(first_node + current_chain->end_index),
          current_chain_(current_chain),
          first_node_(first_node) {}
    const CommittedNode* current_node_;
    const CommittedNode* end_node_;
    const ChainBounds* current_chain_;
    const CommittedNode* const first_node_;
  };

  // NodeRanges hold ChainBounds* and CommittedNode*,
  // a NodeRange may be invalidated if on of the underlying vector is modified.
  NodeRange(const ChainBounds* begin_chain, const ChainBounds* end_chain,
            const CommittedNode* first_node)
      : begin_chain_(begin_chain),
        end_chain_(end_chain),
        first_node_(first_node) {}
  Iterator begin() const { return {begin_chain_, first_node_}; }
  // Note: there is a sentinel value at the end of PathState::chains_,
  // so dereferencing chain_range_.end()->begin_ is always valid.
  Iterator end() const { return {end_chain_, first_node_}; }

 private:
  const ChainBounds* begin_chain_;
  const ChainBounds* end_chain_;
  const CommittedNode* const first_node_;
};

// This checker enforces unary dimension requirements.
// A unary dimension requires that there is some valuation of
// node_capacity and demand such that for all paths,
// if arc A -> B is on a path of path_class p,
// then node_capacity[A] + demand[p][A] = node_capacity[B].
// Moreover, all node_capacities of a path must be inside interval
// path_capacity[path].
// Note that Intervals have two meanings:
// - for demand and node_capacity, those are values allowed for each associated
//   decision variable.
// - for path_capacity, those are set of values that node_capacities of the path
//   must respect.
// If the path capacity of a path is [kint64min, kint64max],
// then the unary dimension requirements are not enforced on this path.
class UnaryDimensionChecker {
 public:
  struct Interval {
    int64 min;
    int64 max;
  };

  UnaryDimensionChecker(const PathState* path_state,
                        std::vector<Interval> path_capacity,
                        std::vector<int> path_class,
                        std::vector<std::vector<Interval>> demand,
                        std::vector<Interval> node_capacity);

  // Given the change made in PathState, checks that the unary dimension
  // constraint is still feasible.
  bool Check() const;

  // Commits to the changes made in PathState,
  // must be called before PathState::Commit().
  void Commit();

 private:
  // Range min/max query on partial_demand_sums_.
  // The first_node and last_node MUST form a subpath in the committed state.
  // Nodes first_node and last_node are passed by their index in precomputed
  // data, they must be committed in some path, and it has to be the same path.
  // See partial_demand_sums_.
  Interval GetMinMaxPartialDemandSum(int first_node_index,
                                     int last_node_index) const;

  // Queries whether all nodes in the committed subpath [first_node, last_node]
  // have fixed demands and trivial node_capacity [kint64min, kint64max].
  // first_node and last_node MUST form a subpath in the committed state.
  // Nodes are passed by their index in precomputed data.
  bool SubpathOnlyHasTrivialNodes(int first_node_index,
                                  int last_node_index) const;

  // Commits to the current solution and rebuilds structures from scratch.
  void FullCommit();
  // Commits to the current solution and only build structures for paths that
  // changed, using additional space to do so in a time-memory tradeoff.
  void IncrementalCommit();
  // Adds sums of given path to the bottom layer of the RMQ structure,
  // updates index_ and previous_nontrivial_index_.
  void AppendPathDemandsToSums(int path);
  // Updates the RMQ structure from its bottom layer,
  // with [begin_index, end_index) the range of the change,
  // which must be at the end of the bottom layer.
  // Supposes that requests overlapping the range will be inside the range,
  // to avoid updating all layers.
  void UpdateRMQStructure(int begin_index, int end_index);

  const PathState* const path_state_;
  const std::vector<Interval> path_capacity_;
  const std::vector<int> path_class_;
  const std::vector<std::vector<Interval>> demand_;
  const std::vector<Interval> node_capacity_;

  // Precomputed data.
  // Maps nodes to their pre-computed data, except for isolated nodes,
  // which do not have precomputed data.
  // Only valid for nodes that are in some path in the committed state.
  std::vector<int> index_;
  // Implementation of a <O(n log n), O(1)> range min/max query, n = #nodes.
  // partial_demand_sums_rmq_[0][index_[node]] contains the sum of demands
  // from the start of the node's path to the node.
  // If node is the start of path, the sum is demand_[path_class_[path]][node],
  // moreover partial_demand_sums_rmq_[0][index_[node]-1] is {0, 0}.
  // partial_demand_sums_rmq_[layer][index] contains an interval
  // [min_value, max_value] such that min_value is
  // min(partial_demand_sums_rmq_[0][index+i].min | i in [0, 2^layer)),
  // similarly max_value is the maximum of .max on the same range.
  std::vector<std::vector<Interval>> partial_demand_sums_rmq_;
  // The incremental branch of Commit() may waste space in the layers of the
  // RMQ structure. This is the upper limit of a layer's size.
  const int maximum_partial_demand_layer_size_;
  // previous_nontrivial_index_[index_[node]] has the index of the previous
  // node on its committed path that has nonfixed demand or nontrivial node
  // capacity. This allows for O(1) queries that all nodes on a subpath
  // are nonfixed and nontrivial.
  std::vector<int> previous_nontrivial_index_;
};

// Make a filter that takes ownership of a PathState and synchronizes it with
// solver events. The solver represents a graph with array of variables 'nexts'.
// Solver events are embodied by Assignment* deltas, that are translated to node
// changes during Relax(), committed during Synchronize(), and reverted on
// Revert().
LocalSearchFilter* MakePathStateFilter(Solver* solver,
                                       std::unique_ptr<PathState> path_state,
                                       const std::vector<IntVar*>& nexts);

// Make a filter that translates solver events to the input checker's interface.
// Since UnaryDimensionChecker has a PathState, the filter returned by this
// must be synchronized to the corresponding PathStateFilter:
// - Relax() must be called after the PathStateFilter's.
// - Accept() must be called after.
// - Synchronize() must be called before.
// - Revert() must be called before.
LocalSearchFilter* MakeUnaryDimensionFilter(
    Solver* solver, std::unique_ptr<UnaryDimensionChecker> checker);

#endif  // !defined(SWIG)

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
