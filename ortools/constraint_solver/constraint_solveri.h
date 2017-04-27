// Copyright 2010-2014 Google
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


// Collection of objects used to extend the Constraint Solver library.
//
// This file contains a set of objects that simplifies writing extensions
// of the library.
//
// The main objects that define extensions are:
//   - BaseIntExpr, the base class of all expressions that are not variables.
//   - SimpleRevFIFO, a reversible FIFO list with templatized values.
//     A reversible data structure is a data structure that reverts its
//     modifications when the search is going up in the search tree, usually
//     after a failure occurs.
//   - RevImmutableMultiMap, a reversible immutable multimap.
//   - MakeConstraintDemon<n> and MakeDelayedConstraintDemon<n> to wrap methods
//     of a constraint as a demon.
//   - RevSwitch, a reversible flip-once switch.
//   - SmallRevBitSet, RevBitSet, and RevBitMatrix: reversible 1D or 2D
//     bitsets.
//   - LocalSearchOperator, IntVarLocalSearchOperator, ChangeValue and
//     PathOperator, to create new local search operators.
//   - LocalSearchFilter and IntVarLocalSearchFilter, to create new local
//     search filters.
//   - BaseLns, to write Large Neighborhood Search operators.
//   - SymmetryBreaker, to describe model symmetries that will be broken during
//     search using the 'Symmetry Breaking During Search' framework
//     see Gent, I. P., Harvey, W., & Kelsey, T. (2002).
//     Groups and Constraints: Symmetry Breaking During Search.
//     Principles and Practice of Constraint Programming CP2002
//     (Vol. 2470, pp. 415-430). Springer. Retrieved from
//     http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.11.1442.
//
// Then, there are some internal classes that are used throughout the solver
// and exposed in this file:
//   - SearchLog, the root class of all periodic outputs during search.
//   - ModelCache, A caching layer to avoid creating twice the same object.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/sysinfo.h"
#include "ortools/base/timer.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/model.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/tuple_set.h"
#include "ortools/util/vector_map.h"

class WallTimer;

namespace operations_research {
class CPArgumentProto;
class CPConstraintProto;
class CPIntegerExpressionProto;
class CPIntervalVariableProto;

// This is the base class for all expressions that are not variables.
// It provides a basic 'CastToVar()' implementation.
//
// The class of expressions represent two types of objects: variables
// and subclasses of BaseIntExpr. Variables are stateful objects that
// provide a rich API (remove values, WhenBound...). On the other hand,
// subclasses of BaseIntExpr represent range-only stateless objects.
// That is, std::min(A + B) is recomputed each time as std::min(A) + std::min(B).
//
// Furthermore, sometimes, the propagation on an expression is not complete,
// and Min(), Max() are not monotonic with respect to SetMin() and SetMax().
// For instance, if A is a var with domain [0 .. 5], and B another variable
// with domain [0 .. 5], then Plus(A, B) has domain [0, 10].
//
// If we apply SetMax(Plus(A, B), 4)), we will deduce that both A
// and B have domain [0 .. 4]. In that case, Max(Plus(A, B)) is 8
// and not 4.  To get back monotonicity, we 'cast' the expression
// into a variable using the Var() method (that will call CastToVar()
// internally). The resulting variable will be stateful and monotonic.
//
// Finally, one should never store a pointer to a IntExpr, or
// BaseIntExpr in the code. The safe code should always call Var() on an
// expression built by the solver, and store the object as an IntVar*.
// This is a consequence of the stateless nature of the expressions that
// makes the code error-prone.
class BaseIntExpr : public IntExpr {
 public:
  explicit BaseIntExpr(Solver* const s) : IntExpr(s), var_(nullptr) {}
  ~BaseIntExpr() override {}

  IntVar* Var() override;
  virtual IntVar* CastToVar();

 private:
  IntVar* var_;
};

// This enum is used internally to do dynamic typing on subclasses of integer
// variables.
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

// ----- utility classes -----

// This class represent a reversible FIFO structure.
// The main difference w.r.t a standard FIFO structure is that a Solver is
// given as parameter to the modifiers such that the solver can store the
// backtrack information
// Iterator's traversing order should not be changed, as some algorithm
// depend on it to be consistent.
// It's main use is to store a list of demons in the various classes of
// variables.
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
  // This iterator is not stable with respect to deletion.
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

  // Pushes the var on top if is not a duplicate of the current top object.
  void PushIfNotTop(Solver* const s, T val) {
    if (chunks_ == nullptr || LastValue() != val) {
      Push(s, val);
    }
  }

  // Returns the last item of the FIFO.
  const T* Last() const {
    return chunks_ ? &chunks_->data_[pos_.Value()] : nullptr;
  }

  T* MutableLast() { return chunks_ ? &chunks_->data_[pos_.Value()] : nullptr; }

  // Returns the last value in the FIFO.
  const T& LastValue() const {
    DCHECK(chunks_);
    return chunks_->data_[pos_.Value()];
  }

  // Sets the last value in the FIFO.
  void SetLastValue(const T& v) {
    DCHECK(Last());
    chunks_->data_[pos_.Value()] = v;
  }

 private:
  Chunk* chunks_;
  NumericalRev<int> pos_;
};

// ---------- Reversible Hash Table ----------

// ----- Hash functions -----
// TODO(user): use murmurhash.
inline uint64 Hash1(uint64 value) {
  value = (~value) + (value << 21);  // value = (value << 21) - value - 1;
  value ^= value >> 24;
  value += (value << 3) + (value << 8);  // value * 265
  value ^= value >> 14;
  value += (value << 2) + (value << 4);  // value * 21
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
#if defined(ARCH_K8) || defined(__powerpc64__) || defined(__aarch64__)
  return Hash1(bit_cast<uint64>(ptr));
#else
  return Hash1(bit_cast<uint32>(ptr));
#endif
}

template <class T>
uint64 Hash1(const std::vector<T*>& ptrs) {
  if (ptrs.empty()) {
    return 0;
  } else if (ptrs.size() == 1) {
    return Hash1(ptrs[0]);
  } else {
    uint64 hash = Hash1(ptrs[0]);
    for (int i = 1; i < ptrs.size(); ++i) {
      hash = hash * i + Hash1(ptrs[i]);
    }
    return hash;
  }
}

inline uint64 Hash1(const std::vector<int64>& ptrs) {
  if (ptrs.empty()) {
    return 0;
  } else if (ptrs.size() == 1) {
    return Hash1(ptrs[0]);
  } else {
    uint64 hash = Hash1(ptrs[0]);
    for (int i = 1; i < ptrs.size(); ++i) {
      hash = hash * i + Hash1(ptrs[i]);
    }
    return hash;
  }
}

// ----- Immutable Multi Map -----

// Reversible Immutable MultiMap class.
// Represents an immutable multi-map that backtracks with the solver.
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

  // Returns true if the multi-map contains at least one instance of 'key'.
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

  // Returns one value attached to 'key', or 'defaut_value' if 'key'
  // is not in the multi-map. The actual value returned if more than one
  // values is attached to the same key is not specified.
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

  // Inserts (key, value) in the multi-map.
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

// A reversible switch that can switch once from false to true.
class RevSwitch {
 public:
  RevSwitch() : value_(false) {}

  bool Switched() const { return value_; }

  void Switch(Solver* const solver) { solver->SaveAndSetValue(&value_, true); }

 private:
  bool value_;
};

// This class represents a small reversible bitset (size <= 64).
// This class is useful to maintain supports.
class SmallRevBitSet {
 public:
  explicit SmallRevBitSet(int64 size);
  // Sets the 'pos' bit.
  void SetToOne(Solver* const solver, int64 pos);
  // Erases the 'pos' bit.
  void SetToZero(Solver* const solver, int64 pos);
  // Returns the number of bits set to one.
  int64 Cardinality() const;
  // Is bitset null?
  bool IsCardinalityZero() const { return bits_.Value() == GG_ULONGLONG(0); }
  // Does it contains only one bit set?
  bool IsCardinalityOne() const {
    return (bits_.Value() != 0) && !(bits_.Value() & (bits_.Value() - 1));
  }
  // Gets the index of the first bit set starting from 0.
  // It returns -1 if the bitset is empty.
  int64 GetFirstOne() const;

 private:
  Rev<uint64> bits_;
};

// This class represents a reversible bitset.
// This class is useful to maintain supports.
class RevBitSet {
 public:
  explicit RevBitSet(int64 size);
  ~RevBitSet();

  // Sets the 'pos' bit.
  void SetToOne(Solver* const solver, int64 pos);
  // Erases the 'pos' bit.
  void SetToZero(Solver* const solver, int64 pos);
  // Returns whether the 'pos' bit is set.
  bool IsSet(int64 pos) const;
  // Returns the number of bits set to one.
  int64 Cardinality() const;
  // Is bitset null?
  bool IsCardinalityZero() const;
  // Does it contains only one bit set?
  bool IsCardinalityOne() const;
  // Gets the index of the first bit set starting from start.
  // It returns -1 if the bitset is empty after start.
  int64 GetFirstBit(int start) const;
  // Cleans all bits.
  void ClearAll(Solver* const solver);

  friend class RevBitMatrix;

 private:
  // Save the offset's part of the bitset.
  void Save(Solver* const solver, int offset);
  const int64 size_;
  const int64 length_;
  uint64* bits_;
  uint64* stamps_;
};

// Matrix version of the RevBitSet class.
class RevBitMatrix : private RevBitSet {
 public:
  RevBitMatrix(int64 rows, int64 columns);
  ~RevBitMatrix();

  // Sets the 'column' bit in the 'row' row.
  void SetToOne(Solver* const solver, int64 row, int64 column);
  // Erases the 'column' bit in the 'row' row.
  void SetToZero(Solver* const solver, int64 row, int64 column);
  // Returns whether the 'column' bit in the 'row' row is set.
  bool IsSet(int64 row, int64 column) const {
    DCHECK_GE(row, 0);
    DCHECK_LT(row, rows_);
    DCHECK_GE(column, 0);
    DCHECK_LT(column, columns_);
    return RevBitSet::IsSet(row * columns_ + column);
  }
  // Returns the number of bits set to one in the 'row' row.
  int64 Cardinality(int row) const;
  // Is bitset of row 'row' null?
  bool IsCardinalityZero(int row) const;
  // Does the 'row' bitset contains only one bit set?
  bool IsCardinalityOne(int row) const;
  // Returns the first bit in the row 'row' which position is >= 'start'.
  // It returns -1 if there are none.
  int64 GetFirstBit(int row, int start) const;
  // Cleans all bits.
  void ClearAll(Solver* const solver);

 private:
  const int64 rows_;
  const int64 columns_;
};

// @{
// These methods represent generic demons that will call back a
// method on the constraint during their Run method.
// This way, all propagation methods are members of the constraint class,
// and demons are just proxies with a priority of NORMAL_PRIORITY.

// Demon proxy to a method on the constraint with no arguments.
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
  return StrCat(param);
}

// Support limited to pointers to classes which define DebugString().
template <class P>
std::string ParameterDebugString(P* param) {
  return param->DebugString();
}

// Demon proxy to a method on the constraint with one argument.
template <class T, class P>
class CallMethod1 : public Demon {
 public:
  CallMethod1(T* const ct, void (T::*method)(P), const std::string& name, P param1)
      : constraint_(ct), method_(method), name_(name), param1_(param1) {}

  ~CallMethod1() override {}

  void Run(Solver* const s) override { (constraint_->*method_)(param1_); }

  std::string DebugString() const override {
    return StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
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

// Demon proxy to a method on the constraint with two arguments.
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
    return StrCat(StrCat("CallMethod_", name_),
                  StrCat("(", constraint_->DebugString()),
                  StrCat(", ", ParameterDebugString(param1_)),
                  StrCat(", ", ParameterDebugString(param2_), ")"));
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
// Demon proxy to a method on the constraint with three arguments.
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
    return StrCat(StrCat("CallMethod_", name_),
                  StrCat("(", constraint_->DebugString()),
                  StrCat(", ", ParameterDebugString(param1_)),
                  StrCat(", ", ParameterDebugString(param2_)),
                  StrCat(", ", ParameterDebugString(param3_), ")"));
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
// @}

// @{
// These methods represents generic demons that will call back a
// method on the constraint during their Run method. This demon will
// have a priority DELAYED_PRIORITY.

// Low-priority demon proxy to a method on the constraint with no arguments.
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
                                   void (T::*method)(), const std::string& name) {
  return s->RevAlloc(new DelayedCallMethod0<T>(ct, method, name));
}

// Low-priority demon proxy to a method on the constraint with one argument.
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
    return StrCat("DelayedCallMethod_", name_, "(",
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
                                   void (T::*method)(P), const std::string& name,
                                   P param1) {
  return s->RevAlloc(new DelayedCallMethod1<T, P>(ct, method, name, param1));
}

// Low-priority demon proxy to a method on the constraint with two arguments.
template <class T, class P, class Q>
class DelayedCallMethod2 : public Demon {
 public:
  DelayedCallMethod2(T* const ct, void (T::*method)(P, Q), const std::string& name,
                     P param1, Q param2)
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
    return StrCat(StrCat("DelayedCallMethod_", name_),
                  StrCat("(", constraint_->DebugString()),
                  StrCat(", ", ParameterDebugString(param1_)),
                  StrCat(", ", ParameterDebugString(param2_), ")"));
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
                                   void (T::*method)(P, Q), const std::string& name,
                                   P param1, Q param2) {
  return s->RevAlloc(
      new DelayedCallMethod2<T, P, Q>(ct, method, name, param1, param2));
}
// @}

#endif  // !defined(SWIG)

// ---------- Local search operators ----------

// The base class for all local search operators.
//
// A local search operator is an object that defines the neighborhood of a
// solution. In other words, a neighborhood is the set of solutions which can
// be reached from a given solution using an operator.
//
// The behavior of the LocalSearchOperator class is similar to iterators.
// The operator is synchronized with an assignment (gives the
// current values of the variables); this is done in the Start() method.
//
// Then one can iterate over the neighbors using the MakeNextNeighbor method.
// This method returns an assignment which represents the incremental changes
// to the current solution. It also returns a second assignment representing the
// changes to the last solution defined by the neighborhood operator; this
// assignment is empty if the neighborhood operator cannot track this
// information.
//
// TODO(user): rename Start to Synchronize ?
// TODO(user): decouple the iterating from the defining of a neighbor.
class LocalSearchOperator : public BaseObject {
 public:
  LocalSearchOperator() {}
  ~LocalSearchOperator() override {}
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) = 0;
  virtual void Start(const Assignment* assignment) = 0;
};

// ----- Base operator class for operators manipulating variables -----

template <class V, class Val, class Handler>
class VarLocalSearchOperator : public LocalSearchOperator {
 public:
  VarLocalSearchOperator() : activated_(), was_activated_(), cleared_(true) {}
  VarLocalSearchOperator(std::vector<V*> vars, Handler var_handler)
      : activated_(),
        was_activated_(),
        cleared_(true),
        var_handler_(var_handler) {}
  ~VarLocalSearchOperator() override {}
  // This method should not be overridden. Override OnStart() instead which is
  // called before exiting this method.
  void Start(const Assignment* assignment) override {
    const int size = Size();
    CHECK_LE(size, assignment->Size())
        << "Assignment contains fewer variables than operator";
    for (int i = 0; i < size; ++i) {
      activated_.Set(i, var_handler_.ValueFromAssignent(*assignment, vars_[i],
                                                        i, &values_[i]));
    }
    old_values_ = values_;
    was_activated_.SetContentFromBitsetOfSameSize(activated_);
    OnStart();
  }
  virtual bool IsIncremental() const { return false; }
  int Size() const { return vars_.size(); }
  // Returns the value in the current assignment of the variable of given index.
  const Val& Value(int64 index) const {
    DCHECK_LT(index, vars_.size());
    return values_[index];
  }
  // Returns the variable of given index.
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
    for (const int64 index : changes_.PositionsSetAtLeastOnce()) {
      V* var = Var(index);
      const Val& value = Value(index);
      const bool activated = activated_[index];
      if (!activated) {
        if (!cleared_ && delta_changes_[index] && IsIncremental()) {
          deltadelta->FastAdd(var)->Deactivate();
        }
        delta->FastAdd(var)->Deactivate();
      } else if (value != OldValue(index) || !SkipUnchanged(index)) {
        if (!cleared_ && delta_changes_[index] && IsIncremental()) {
          var_handler_.AddToAssignment(var, value, index, deltadelta);
        }
        var_handler_.AddToAssignment(var, value, index, delta);
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
      var_handler_.OnRevertChanges(index);
      activated_.CopyBucket(was_activated_, index);
    }
    changes_.SparseClearAll();
  }
  void AddVars(const std::vector<V*>& vars) {
    if (!vars.empty()) {
      vars_.insert(vars_.end(), vars.begin(), vars.end());
      const int64 size = Size();
      values_.resize(size);
      old_values_.resize(size);
      activated_.Resize(size);
      was_activated_.Resize(size);
      changes_.ClearAndResize(size);
      delta_changes_.ClearAndResize(size);
      var_handler_.OnAddVars();
    }
  }

  // Called by Start() after synchronizing the operator with the current
  // assignment. Should be overridden instead of Start() to avoid calling
  // VarLocalSearchOperator::Start explicitly.
  virtual void OnStart() {}

  // OnStart() should really be protected, but then SWIG doesn't see it. So we
  // make it public, but only subclasses should access to it (to override it).
 protected:
  void MarkChange(int64 index) {
    delta_changes_.Set(index);
    changes_.Set(index);
  }

  std::vector<V*> vars_;
  std::vector<Val> values_;
  std::vector<Val> old_values_;
  Bitset64<> activated_;
  Bitset64<> was_activated_;
  SparseBitset<> changes_;
  SparseBitset<> delta_changes_;
  bool cleared_;
  Handler var_handler_;
};

// ----- Base operator class for operators manipulating IntVars -----

class IntVarLocalSearchHandler {
 public:
  void AddToAssignment(IntVar* var, int64 value, int64 index,
                       Assignment* assignment) const {
    assignment->FastAdd(var)->SetValue(value);
  }
  bool ValueFromAssignent(const Assignment& assignment, IntVar* var,
                          int64 index, int64* value) {
    const Assignment::IntContainer& container = assignment.IntVarContainer();
    const IntVarElement* element = &(container.Element(index));
    if (element->Var() != var) {
      CHECK(container.Contains(var))
          << "Assignment does not contain operator variable " << var;
      element = &(container.Element(var));
    }
    *value = element->Value();
    return element->Activated();
  }
  void OnRevertChanges(int64 index) {}
  void OnAddVars() {}
};

// Specialization of LocalSearchOperator built from an array of IntVars
// which specifies the scope of the operator.
// This class also takes care of storing current variable values in Start(),
// keeps track of changes done by the operator and builds the delta.
// The Deactivate() method can be used to perform Large Neighborhood Search.

#ifdef SWIG
// Unfortunately, we must put this code here and not in
// */constraint_solver.i, because it must be parsed by SWIG before the
// derived C++ class.
// TODO(user): find a way to move this code back to the .i file, where it
// belongs.
// In python, we use a whitelist to expose the API. This whitelist must also
// be extended here.
#if defined(SWIGPYTHON)
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
  IntVarLocalSearchOperator() {}
  explicit IntVarLocalSearchOperator(const std::vector<IntVar*>& vars)
      : VarLocalSearchOperator<IntVar, int64, IntVarLocalSearchHandler>(
            vars, IntVarLocalSearchHandler()) {
    AddVars(vars);
  }
  ~IntVarLocalSearchOperator() override {}
  // Redefines MakeNextNeighbor to export a simpler interface. The calls to
  // ApplyChanges() and RevertChanges() are factored in this method, hiding both
  // delta and deltadelta from subclasses which only need to override
  // MakeOneNeighbor().
  // Therefore this method should not be overridden. Override MakeOneNeighbor()
  // instead.
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

 protected:
  // Creates a new neighbor. It returns false when the neighborhood is
  // completely explored.
  // TODO(user): make it pure virtual, implies porting all apps overriding
  // MakeNextNeighbor() in a subclass of IntVarLocalSearchOperator.
  virtual bool MakeOneNeighbor();
};

// ----- SequenceVarLocalSearchOperator -----

class SequenceVarLocalSearchOperator;

class SequenceVarLocalSearchHandler {
 public:
  SequenceVarLocalSearchHandler() : op_(nullptr) {}
  SequenceVarLocalSearchHandler(const SequenceVarLocalSearchHandler& other)
      : op_(other.op_) {}
  explicit SequenceVarLocalSearchHandler(SequenceVarLocalSearchOperator* op)
      : op_(op) {}
  void AddToAssignment(SequenceVar* var, const std::vector<int>& value,
                       int64 index, Assignment* assignment) const;
  bool ValueFromAssignent(const Assignment& assignment, SequenceVar* var,
                          int64 index, std::vector<int>* value);
  void OnRevertChanges(int64 index);
  void OnAddVars();

 private:
  SequenceVarLocalSearchOperator* const op_;
};

#ifdef SWIG
// Unfortunately, we must put this code here and not in
// */constraint_solver.i, because it must be parsed by SWIG before the
// derived C++ class.
// TODO(user): find a way to move this code back to the .i file, where it
// belongs.
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
            vars, SequenceVarLocalSearchHandler(this)) {
    AddVars(vars);
  }
  ~SequenceVarLocalSearchOperator() override {}
  // Returns the value in the current assignment of the variable of given index.
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

  std::vector<std::vector<int> > backward_values_;
};

inline void SequenceVarLocalSearchHandler::AddToAssignment(
    SequenceVar* var, const std::vector<int>& value, int64 index,
    Assignment* assignment) const {
  SequenceVarElement* const element = assignment->FastAdd(var);
  element->SetForwardSequence(value);
  element->SetBackwardSequence(op_->backward_values_[index]);
}

inline bool SequenceVarLocalSearchHandler::ValueFromAssignent(
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

inline void SequenceVarLocalSearchHandler::OnRevertChanges(int64 index) {
  op_->backward_values_[index].clear();
}

inline void SequenceVarLocalSearchHandler::OnAddVars() {
  op_->backward_values_.resize(op_->Size());
}

// ----- Base Large Neighborhood Search operator class ----

// This is the base class for building an Lns operator. An Lns fragment is a
// collection of variables which will be relaxed. Fragments are built with
// NextFragment(), which returns false if there are no more fragments to build.
// Optionally one can override InitFragments, which is called from
// LocalSearchOperator::Start to initialize fragment data.
//
// Here's a sample relaxing one variable at a time:
//
// class OneVarLns : public BaseLns {
//  public:
//   OneVarLns(const std::vector<IntVar*>& vars) : BaseLns(vars), index_(0) {}
//   virtual ~OneVarLns() {}
//   virtual void InitFragments() { index_ = 0; }
//   virtual bool NextFragment() {
//     const int size = Size();
//     if (index_ < size) {
//       AppendToFragment(index_);
//       ++index_;
//       return true;
//     } else {
//       return false;
//     }
//   }
//
//  private:
//   int index_;
// };
class BaseLns : public IntVarLocalSearchOperator {
 public:
  explicit BaseLns(const std::vector<IntVar*>& vars);
  ~BaseLns() override;
  virtual void InitFragments();
  virtual bool NextFragment() = 0;
  void AppendToFragment(int index);
  int FragmentSize() const;

 protected:
  // This method should not be overridden. Override NextFragment() instead.
  bool MakeOneNeighbor() override;

 private:
  // This method should not be overridden. Override InitFragments() instead.
  void OnStart() override;
  std::vector<int> fragment_;
};

// ----- ChangeValue Operators -----

// Defines operators which change the value of variables;
// each neighbor corresponds to *one* modified variable.
// Sub-classes have to define ModifyValue which determines what the new
// variable value is going to be (given the current value and the variable).
class ChangeValue : public IntVarLocalSearchOperator {
 public:
  explicit ChangeValue(const std::vector<IntVar*>& vars);
  ~ChangeValue() override;
  virtual int64 ModifyValue(int64 index, int64 value) = 0;

 protected:
  // This method should not be overridden. Override ModifyValue() instead.
  bool MakeOneNeighbor() override;

 private:
  void OnStart() override;

  int index_;
};

// ----- Path-based Operators -----

// Base class of the local search operators dedicated to path modifications
// (a path is a set of nodes linked together by arcs).
// This family of neighborhoods supposes they are handling next variables
// representing the arcs (var[i] represents the node immediately after i on
// a path).
// Several services are provided:
// - arc manipulators (SetNext(), ReverseChain(), MoveChain())
// - path inspectors (Next(), IsPathEnd())
// - path iterators: operators need a given number of nodes to define a
//   neighbor; this class provides the iteration on a given number of (base)
//   nodes which can be used to define a neighbor (through the BaseNode method)
// Subclasses only need to override MakeNeighbor to create neighbors using
// the services above (no direct manipulation of assignments).
class PathOperator : public IntVarLocalSearchOperator {
 public:
  // Builds an instance of PathOperator from next and path variables.
  // 'number_of_base_nodes' is the number of nodes needed to define a
  // neighbor. 'start_empty_path_class' is a callback returning an index such
  // that if
  // c1 = start_empty_path_class(StartNode(p1)),
  // c2 = start_empty_path_class(StartNode(p2)),
  // p1 and p2 are path indices,
  // then if c1 == c2, p1 and p2 are equivalent if they are empty.
  // This is used to remove neighborhood symmetries on equivalent empty paths;
  // for instance if a node cannot be moved to an empty path, then all moves
  // moving the same node to equivalent empty paths will be skipped.
  // 'start_empty_path_class' can be nullptr in which case no symmetries will be
  // removed.
  PathOperator(const std::vector<IntVar*>& next_vars,
               const std::vector<IntVar*>& path_vars, int number_of_base_nodes,
               std::function<int(int64)> start_empty_path_class);
  ~PathOperator() override {}
  virtual bool MakeNeighbor() = 0;

  // TODO(user): Make the following methods protected.
  bool SkipUnchanged(int index) const override;

  // Returns the index of the node after the node of index node_index in the
  // current assignment.
  int64 Next(int64 node_index) const {
    DCHECK(!IsPathEnd(node_index));
    return Value(node_index);
  }

  // Returns the index of the path to which the node of index node_index
  // belongs in the current assignment.
  int64 Path(int64 node_index) const {
    return ignore_path_vars_ ? 0LL : Value(node_index + number_of_nexts_);
  }

  // Number of next variables.
  int number_of_nexts() const { return number_of_nexts_; }

 protected:
  // This method should not be overridden. Override MakeNeighbor() instead.
  bool MakeOneNeighbor() override;

  // Returns the index of the variable corresponding to the ith base node.
  int64 BaseNode(int i) const { return base_nodes_[i]; }
  // Returns the index of the variable corresponding to the current path
  // of the ith base node.
  int64 StartNode(int i) const { return path_starts_[base_paths_[i]]; }
  // Returns the vector of path start nodes.
  const std::vector<int64>& path_starts() const { return path_starts_; }
  // Returns the class of the current path of the ith base node.
  int PathClass(int i) const {
    return start_empty_path_class_ != nullptr
               ? start_empty_path_class_(StartNode(i))
               : StartNode(i);
  }

  // When the operator is being synchronized with a new solution (when Start()
  // is called), returns true to restart the exploration of the neighborhood
  // from the start of the last paths explored; returns false to restart the
  // exploration at the last nodes visited.
  // This is used to avoid restarting on base nodes which have changed paths,
  // leading to potentially skipping neighbors.
  // TODO(user): remove this when automatic detection of such cases in done.
  virtual bool RestartAtPathStartOnSynchronize() { return false; }
  // Returns true if a base node has to be on the same path as the "previous"
  // base node (base node of index base_index - 1).
  // Useful to limit neighborhood exploration to nodes on the same path.
  // TODO(user): ideally this should be OnSamePath(int64 node1, int64 node2);
  // it's currently way more complicated to implement.
  virtual bool OnSamePathAsPreviousBase(int64 base_index) { return false; }
  // Returns the index of the node to which the base node of index base_index
  // must be set to when it reaches the end of a path.
  // By default, it is set to the start of the current path.
  // When this method is called, one can only assume that base nodes with
  // indices < base_index have their final position.
  virtual int64 GetBaseNodeRestartPosition(int base_index) {
    return StartNode(base_index);
  }

  int64 OldNext(int64 node_index) const {
    DCHECK(!IsPathEnd(node_index));
    return OldValue(node_index);
  }

  int64 OldPath(int64 node_index) const {
    return ignore_path_vars_ ? 0LL : OldValue(node_index + number_of_nexts_);
  }

  // Moves the chain starting after the node before_chain and ending at the node
  // chain_end after the node destination
  bool MoveChain(int64 before_chain, int64 chain_end, int64 destination);

  // Reverses the chain starting after before_chain and ending before
  // after_chain
  bool ReverseChain(int64 before_chain, int64 after_chain, int64* chain_last);

  bool MakeActive(int64 node, int64 destination);
  bool MakeChainInactive(int64 before_chain, int64 chain_end);

  // Sets the to to be the node after from
  void SetNext(int64 from, int64 to, int64 path) {
    DCHECK_LT(from, number_of_nexts_);
    SetValue(from, to);
    if (!ignore_path_vars_) {
      DCHECK_LT(from + number_of_nexts_, Size());
      SetValue(from + number_of_nexts_, path);
    }
  }

  // Returns true if i is the last node on the path; defined by the fact that
  // i outside the range of the variable array
  bool IsPathEnd(int64 i) const { return i >= number_of_nexts_; }

  // Returns true if node is inactive
  bool IsInactive(int64 i) const { return !IsPathEnd(i) && inactives_[i]; }

  // Returns true if operator needs to restart its initial position at each
  // call to Start()
  virtual bool InitPosition() const { return false; }
  // Reset the position of the operator to its position when Start() was last
  // called; this can be used to let an operator iterate more than once over
  // the paths.
  void ResetPosition() { just_started_ = true; }

  const int number_of_nexts_;
  const bool ignore_path_vars_;

 private:
  void OnStart() override;
  // Called by OnStart() after initializing node information. Should be
  // overriden instead of OnStart() to avoid calling PathOperator::OnStart
  // explicitly.
  virtual void OnNodeInitialization() {}
  // Returns true if two nodes are on the same path in the current assignment.
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
  bool CheckChainValidity(int64 chain_start, int64 chain_end,
                          int64 exclude) const;
  void Synchronize();

  std::vector<int> base_nodes_;
  std::vector<int> end_nodes_;
  std::vector<int> base_paths_;
  std::vector<int64> path_starts_;
  std::vector<bool> inactives_;
  bool just_started_;
  bool first_start_;
  std::function<int(int64)> start_empty_path_class_;
};

// Simple PathOperator wrapper that also stores the current previous nodes,
// and is thus able to provide the "Prev" and "IsPathStart" functions.
class PathWithPreviousNodesOperator : public PathOperator {
 public:
  PathWithPreviousNodesOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, int number_of_base_nodes,
      std::function<int(int64)> start_empty_path_class);
  ~PathWithPreviousNodesOperator() override {}

  bool IsPathStart(int64 node_index) const { return prevs_[node_index] == -1; }

  int64 Prev(int64 node_index) const {
    DCHECK(!IsPathStart(node_index));
    return prevs_[node_index];
  }

  std::string DebugString() const override {
    return "PathWithPreviousNodesOperator";
  }

 protected:
  void OnNodeInitialization() override;  // Initializes the "prevs_" array.

 private:
  std::vector<int64> prevs_;
};

// ----- Operator Factories ------

template <class T>
LocalSearchOperator* MakeLocalSearchOperator(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class);

// Classes to which this template function can be applied to as of 04/2014.
// Usage: LocalSearchOperator* op = MakeLocalSearchOperator<Relocate>(...);
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

// ----- Local Search Filters ------

// For fast neighbor pruning
class LocalSearchFilter : public BaseObject {
 public:
  // Accepts a "delta" given the assignment with which the filter has been
  // synchronized; the delta holds the variables which have been modified and
  // their new value.
  // Sample: supposing one wants to maintain a[0,1] + b[0,1] <= 1,
  // for the assignment (a,1), (b,0), the delta (b,1) will be rejected
  // but the delta (a,0) will be accepted.
  virtual bool Accept(const Assignment* delta,
                      const Assignment* deltadelta) = 0;

  // Synchronizes the filter with the current solution, delta being the
  // difference with the solution passed to the previous call to Synchronize()
  // or IncrementalSynchronize(). 'delta' can be used to incrementally
  // synchronizing the filter with the new solution by only considering the
  // changes in delta.
  virtual void Synchronize(const Assignment* assignment,
                           const Assignment* delta) = 0;
  virtual bool IsIncremental() const { return false; }
};

// ----- IntVarLocalSearchFilter -----

class IntVarLocalSearchFilter : public LocalSearchFilter {
 public:
  explicit IntVarLocalSearchFilter(const std::vector<IntVar*>& vars);
  ~IntVarLocalSearchFilter() override;
  // This method should not be overridden. Override OnSynchronize() instead
  // which is called before exiting this method.
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

  // Add variables to "track" to the filter.
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

// ---------- PropagationMonitor ----------

class PropagationMonitor : public SearchMonitor {
 public:
  explicit PropagationMonitor(Solver* const solver);
  ~PropagationMonitor() override;

  // Propagation events.
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
  // IntExpr modifiers.
  virtual void SetMin(IntExpr* const expr, int64 new_min) = 0;
  virtual void SetMax(IntExpr* const expr, int64 new_max) = 0;
  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) = 0;
  // IntVar modifiers.
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
  // IntervalVar modifiers.
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
  // SequenceVar modifiers
  virtual void RankFirst(SequenceVar* const var, int index) = 0;
  virtual void RankNotFirst(SequenceVar* const var, int index) = 0;
  virtual void RankLast(SequenceVar* const var, int index) = 0;
  virtual void RankNotLast(SequenceVar* const var, int index) = 0;
  virtual void RankSequence(SequenceVar* const var,
                            const std::vector<int>& rank_first,
                            const std::vector<int>& rank_last,
                            const std::vector<int>& unperformed) = 0;
  // Install itself on the solver.
  void Install() override;
};

// ---------- LocalSearchMonitor ----------

class LocalSearchMonitor : public SearchMonitor {
  // TODO(user): Add monitoring of local search filters.
 public:
  explicit LocalSearchMonitor(Solver* const solver);
  ~LocalSearchMonitor() override;

  // Local search operator events.
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

  // Install itself on the solver.
  void Install() override;
};

// ----- Boolean Variable -----

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
  void SetRange(int64 l, int64 u) override;
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

// ---------- SymmetryBreaker ----------

class SymmetryManager;

// A symmetry breaker is an object that will visit a decision and
// create the 'symmetrical' decision in return.
// Each symmetry breaker represents one class of symmetry.
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
  // Index of the symmetry breaker when used inside the symmetry manager.
  int index_in_symmetry_manager_;
};

// ---------- Search Log ---------

// The base class of all search logs that periodically outputs information when
// the search is running.
class SearchLog : public SearchMonitor {
 public:
  SearchLog(Solver* const s, OptimizeVar* const obj, IntVar* const var,
            std::function<std::string()> display_callback, int period);
  ~SearchLog() override;
  void EnterSearch() override;
  void ExitSearch() override;
  bool AtSolution() override;
  void BeginFail() override;
  void NoMoreSolutions() override;
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
  std::function<std::string()> display_callback_;
  int nsol_;
  int64 tick_;
  int64 objective_min_;
  int64 objective_max_;
  int min_right_depth_;
  int max_depth_;
  int sliding_min_depth_;
  int sliding_max_depth_;
};

// Implements a complete cache for model elements: expressions and
// constraints.  Caching is based on the signatures of the elements, as
// well as their types.  This class is used internally to avoid creating
// duplicate objects.
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

  // Void constraints.

  virtual Constraint* FindVoidConstraint(VoidConstraintType type) const = 0;

  virtual void InsertVoidConstraint(Constraint* const ct,
                                    VoidConstraintType type) = 0;

  // Var Constant Constraints.
  virtual Constraint* FindVarConstantConstraint(
      IntVar* const var, int64 value, VarConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstraint(Constraint* const ct,
                                           IntVar* const var, int64 value,
                                           VarConstantConstraintType type) = 0;

  // Var Constant Constant Constraints.

  virtual Constraint* FindVarConstantConstantConstraint(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstantConstraint(
      Constraint* const ct, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) = 0;

  // Expr Expr Constraints.

  virtual Constraint* FindExprExprConstraint(
      IntExpr* const expr1, IntExpr* const expr2,
      ExprExprConstraintType type) const = 0;

  virtual void InsertExprExprConstraint(Constraint* const ct,
                                        IntExpr* const expr1,
                                        IntExpr* const expr2,
                                        ExprExprConstraintType type) = 0;

  // Expr Expressions.

  virtual IntExpr* FindExprExpression(IntExpr* const expr,
                                      ExprExpressionType type) const = 0;

  virtual void InsertExprExpression(IntExpr* const expression,
                                    IntExpr* const expr,
                                    ExprExpressionType type) = 0;

  // Expr Constant Expressions.

  virtual IntExpr* FindExprConstantExpression(
      IntExpr* const expr, int64 value,
      ExprConstantExpressionType type) const = 0;

  virtual void InsertExprConstantExpression(
      IntExpr* const expression, IntExpr* const var, int64 value,
      ExprConstantExpressionType type) = 0;

  // Expr Expr Expressions.

  virtual IntExpr* FindExprExprExpression(
      IntExpr* const var1, IntExpr* const var2,
      ExprExprExpressionType type) const = 0;

  virtual void InsertExprExprExpression(IntExpr* const expression,
                                        IntExpr* const var1,
                                        IntExpr* const var2,
                                        ExprExprExpressionType type) = 0;

  // Expr Expr Constant Expressions.

  virtual IntExpr* FindExprExprConstantExpression(
      IntExpr* const var1, IntExpr* const var2, int64 constant,
      ExprExprConstantExpressionType type) const = 0;

  virtual void InsertExprExprConstantExpression(
      IntExpr* const expression, IntExpr* const var1, IntExpr* const var2,
      int64 constant, ExprExprConstantExpressionType type) = 0;

  // Var Constant Constant Expressions.

  virtual IntExpr* FindVarConstantConstantExpression(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) const = 0;

  virtual void InsertVarConstantConstantExpression(
      IntExpr* const expression, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) = 0;

  // Var Constant Array Expressions.

  virtual IntExpr* FindVarConstantArrayExpression(
      IntVar* const var, const std::vector<int64>& values,
      VarConstantArrayExpressionType type) const = 0;

  virtual void InsertVarConstantArrayExpression(
      IntExpr* const expression, IntVar* const var,
      const std::vector<int64>& values,
      VarConstantArrayExpressionType type) = 0;

  // Var Array Expressions.

  virtual IntExpr* FindVarArrayExpression(
      const std::vector<IntVar*>& vars, VarArrayExpressionType type) const = 0;

  virtual void InsertVarArrayExpression(IntExpr* const expression,
                                        const std::vector<IntVar*>& vars,
                                        VarArrayExpressionType type) = 0;

  // Var Array Constant Array Expressions.

  virtual IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) const = 0;

  virtual void InsertVarArrayConstantArrayExpression(
      IntExpr* const expression, const std::vector<IntVar*>& var,
      const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) = 0;

  // Var Array Constant Expressions.

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

// Argument Holder: useful when visiting a model.
#if !defined(SWIG)
class ArgumentHolder {
 public:
  // Type of the argument.
  const std::string& TypeName() const;
  void SetTypeName(const std::string& type_name);

  // Setters.
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

  // Checks if arguments exist.
  bool HasIntegerExpressionArgument(const std::string& arg_name) const;
  bool HasIntegerVariableArrayArgument(const std::string& arg_name) const;

  // Getters.
  int64 FindIntegerArgumentWithDefault(const std::string& arg_name, int64 def) const;
  int64 FindIntegerArgumentOrDie(const std::string& arg_name) const;
  const std::vector<int64>& FindIntegerArrayArgumentOrDie(
      const std::string& arg_name) const;
  const IntTupleSet& FindIntegerMatrixArgumentOrDie(
      const std::string& arg_name) const;

  IntExpr* FindIntegerExpressionArgumentOrDie(const std::string& arg_name) const;
  const std::vector<IntVar*>& FindIntegerVariableArrayArgumentOrDie(
      const std::string& arg_name) const;

 private:
  std::string type_name_;
  std::unordered_map<std::string, int64> integer_argument_;
  std::unordered_map<std::string, std::vector<int64> > integer_array_argument_;
  std::unordered_map<std::string, IntTupleSet> matrix_argument_;
  std::unordered_map<std::string, IntExpr*> integer_expression_argument_;
  std::unordered_map<std::string, IntervalVar*> interval_argument_;
  std::unordered_map<std::string, SequenceVar*> sequence_argument_;
  std::unordered_map<std::string, std::vector<IntVar*> > integer_variable_array_argument_;
  std::unordered_map<std::string, std::vector<IntervalVar*> > interval_array_argument_;
  std::unordered_map<std::string, std::vector<SequenceVar*> > sequence_array_argument_;
};

// Model Parser

class ModelParser : public ModelVisitor {
 public:
  ModelParser();

  ~ModelParser() override;

  // Header/footers.
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
  // Integer arguments
  void VisitIntegerArgument(const std::string& arg_name, int64 value) override;
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override;
  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override;
  // Variables.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override;
  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override;
  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override;
  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override;
  // Visit sequence argument.
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
#endif  // SWIG

// ---------- CpModelLoader -----------

// The class CpModelLoader is responsible for reading a protocol
// buffer representing a CP model and creating the corresponding CP
// model with the expressions and constraints. It should not be used directly.
class CpModelLoader {
 public:
  explicit CpModelLoader(Solver* const solver) : solver_(solver) {}
  ~CpModelLoader() {}

  Solver* solver() const { return solver_; }

  // Returns stored integer expression.
  IntExpr* IntegerExpression(int index) const;
  // Returns the number of stored integer expressions.
  int NumIntegerExpressions() const { return expressions_.size(); }
  // Returns stored interval variable.
  IntervalVar* IntervalVariable(int index) const;
  // Returns the number of stored interval variables.
  int NumIntervalVariables() const { return intervals_.size(); }


#if !defined(SWIG)
  // Internal, do not use.

  // Builds integer expression from proto and stores it. It returns
  // true upon success.
  bool BuildFromProto(const CpIntegerExpression& proto);
  // Builds constraint from proto and returns it.
  Constraint* BuildFromProto(const CpConstraint& proto);
  // Builds interval variable from proto and stores it. It returns
  // true upon success.
  bool BuildFromProto(const CpIntervalVariable& proto);
  // Builds sequence variable from proto and stores it. It returns
  // true upon success.
  bool BuildFromProto(const CpSequenceVariable& proto);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       int64* to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       IntExpr** to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       std::vector<int64>* to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       IntTupleSet* to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       std::vector<IntVar*>* to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       IntervalVar** to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       std::vector<IntervalVar*>* to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       SequenceVar** to_fill);

  bool ScanOneArgument(int type_index, const CpArgument& arg_proto,
                       std::vector<SequenceVar*>* to_fill);

  template <class P, class A>
  bool ScanArguments(const std::string& type, const P& proto, A* to_fill) {
    const int index = tags_.Index(type);
    for (int i = 0; i < proto.arguments_size(); ++i) {
      if (ScanOneArgument(index, proto.arguments(i), to_fill)) {
        return true;
      }
    }
    return false;
  }

  int TagIndex(const std::string& tag) const { return tags_.Index(tag); }

  void AddTag(const std::string& tag) { tags_.Add(tag); }

  // TODO(user): Use.
  void SetSequenceVariable(int index, SequenceVar* const var) {}
#endif  // !defined(SWIG)

 private:
  Solver* const solver_;
  std::vector<IntExpr*> expressions_;
  std::vector<IntervalVar*> intervals_;
  std::vector<SequenceVar*> sequences_;
  VectorMap<std::string> tags_;
};

#if !defined(SWIG)
// ----- Utility Class for Callbacks -----

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

template <class T>
std::function<T(int64)> MakeFunctionFromProto(CpModelLoader* const builder,
                                              const CpExtension& proto,
                                              int tag_index) {
  DCHECK_EQ(tag_index, proto.type_index());
  Solver* const solver = builder->solver();
  int64 index_min = 0;
  CHECK(builder->ScanArguments(ModelVisitor::kMinArgument, proto, &index_min));
  int64 index_max = 0;
  CHECK(builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &index_max));
  std::vector<int64> values;
  CHECK(builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values));
  ArrayWithOffset<T>* const array =
      solver->RevAlloc(new ArrayWithOffset<T>(index_min, index_max));
  for (int i = index_min; i <= index_max; ++i) {
    array->SetValue(i, values[i - index_min]);
  }
  return [array](int64 index) { return array->Evaluate(index); };
}
#endif  // SWIG

// This class is a reversible growing array. In can grow in both
// directions, and even accept negative indices.  The objects stored
// have a type T. As it relies on the solver for reversibility, these
// objects can be up-casted to 'C' when using Solver::SaveValue().
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

// ----- RevIntSet -----

// This is a special class to represent a 'residual' set of T. T must
// be an integer type.  You fill it at first, and then during search,
// you can efficiently remove an element, and query the removed
// elements.
template <class T>
class RevIntSet {
 public:
  static const int kNoInserted = -1;

  // Capacity is the fixed size of the set (it cannot grow).
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

  // Capacity is the fixed size of the set (it cannot grow).
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
    DCHECK_LT(position, capacity_);  // Valid.
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

  // Iterators on the indices.
  typedef const T* const_iterator;
  const_iterator begin() const { return elements_.get(); }
  const_iterator end() const { return elements_.get() + num_elements_.Value(); }

 private:
  // Used in DCHECK.
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

  // Set of elements.
  std::unique_ptr<T[]> elements_;
  // Number of elements in the set.
  NumericalRev<int> num_elements_;
  // Number of elements in the set.
  const int capacity_;
  // Reverse mapping.
  int* position_;
  // Does the set owns the position array.
  const bool delete_position_;
};

// ----- RevPartialSequence -----

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
      StrAppend(&result, elements_[i]);
      if (i != first_ranked_.Value() - 1) {
        result.append("-");
      }
    }
    result.append("|");
    for (int i = first_ranked_.Value(); i <= last_ranked_.Value(); ++i) {
      StrAppend(&result, elements_[i]);
      if (i != last_ranked_.Value()) {
        result.append("-");
      }
    }
    result.append("|");
    for (int i = last_ranked_.Value() + 1; i < size_; ++i) {
      StrAppend(&result, elements_[i]);
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

  // Set of elements.
  std::vector<int> elements_;
  // Position of the element after the last element ranked from the start.
  NumericalRev<int> first_ranked_;
  // Position of the element before the last element ranked from the end.
  NumericalRev<int> last_ranked_;
  // Number of elements in the sequence.
  const int size_;
  // Reverse mapping.
  std::unique_ptr<int[]> position_;
};

// This class represents a reversible bitset. It is meant to represent a set of
// active bits. It does not offer direct access, but just methods that can
// reversibly substract another bitset, or check if the current active bitset
// intersects with another bitset.
class UnsortedNullableRevBitset {
 public:
  // Size is the number of bits to store in the bitset.
  explicit UnsortedNullableRevBitset(int bit_size);

  ~UnsortedNullableRevBitset() {}

  // This methods overwrites the active bitset with the mask. This method should
  // be called only once.
  void Init(Solver* const solver, const std::vector<uint64>& mask);

  // This method substracts the mask from the active bitset. It returns true if
  // the active bitset was changed in the process.
  bool RevSubtract(Solver* const solver, const std::vector<uint64>& mask);

  // This method ANDs the mask with the active bitset. It returns true if
  // the active bitset was changed in the process.
  bool RevAnd(Solver* const solver, const std::vector<uint64>& mask);

  // This method returns the number of non null 64 bit words in the bitset
  // representation.
  int ActiveWordSize() const { return active_words_.Size(); }

  // This method returns true if the active bitset is null.
  bool Empty() const { return active_words_.Size() == 0; }

  // This method returns true iff the mask and the active bitset have a non
  // null intersection. support_index is used as an accelerator:
  //   - The first word tested to check the intersection will be the
  //     '*support_index'th one.
  //   - If the intersection is not null, the support_index will be filled with
  //     the index of the word that does intersect with the mask. This can be
  //     reused later to speed-up the check.
  bool Intersects(const std::vector<uint64>& mask, int* support_index);

  // Returns the number of bits given in the constructor of the bitset.
  int64 bit_size() const { return bit_size_; }
  // Returns the number of 64 bit words used to store the bitset.
  int64 word_size() const { return word_size_; }
  // Returns the set of active word indices.
  const RevIntSet<int>& active_words() const { return active_words_; }

 private:
  void CleanUpActives(Solver* const solver);

  const int64 bit_size_;
  const int64 word_size_;
  RevArray<uint64> bits_;
  RevIntSet<int> active_words_;
  std::vector<int> to_remove_;
};

// ---------- Helpers ----------

// ----- On integer vectors -----

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

// ----- On integer variable vector -----

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

// Returns true if all the variables are assigned to a single value,
// or if their corresponding value is null.
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

// Returns true if all variables are assigned to 'value'.
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
    // The std::max<int64> is needed for compilation on MSVC.
    result = std::max<int64>(result, vars[i]->Max());
  }
  return result;
}

inline int64 MinVarArray(const std::vector<IntVar*>& vars) {
  DCHECK(!vars.empty());
  int64 result = kint64max;
  for (int i = 0; i < vars.size(); ++i) {
    // The std::min<int64> is needed for compilation on MSVC.
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

// ----- Arithmetic operations -----

inline int64 PosIntDivUp(int64 e, int64 v) {
  DCHECK_GT(v, 0);
  if (e >= 0) {
    return e % v == 0 ? e / v : e / v + 1;
  } else {
    return -(-e / v);
  }
}

inline int64 PosIntDivDown(int64 e, int64 v) {
  DCHECK_GT(v, 0);
  if (e >= 0) {
    return e / v;
  } else {
    return e % v == 0 ? e / v : e / v - 1;
  }
}

// ----- Vector of integer manipulations -----
std::vector<int64> ToInt64Vector(const std::vector<int>& input);
}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
