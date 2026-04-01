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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_REVERSIBLE_ENGINE_H_
#define ORTOOLS_CONSTRAINT_SOLVER_REVERSIBLE_ENGINE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "ortools/constraint_solver/solver_parameters.pb.h"

namespace operations_research {

class Constraint;
class IntExpr;
class IntVar;
class ModelCache;
class Solver;
struct StateInfo;
struct StateMarker;
struct Trail;

/// A BaseObject is the root of all reversibly allocated objects.
/// A DebugString method and the associated << operator are implemented
/// as a convenience.
class BaseObject {
 public:
  BaseObject() {}

#ifndef SWIG
  // This type is neither copyable nor movable.
  BaseObject(const BaseObject&) = delete;
  BaseObject& operator=(const BaseObject&) = delete;
#endif
  virtual ~BaseObject() = default;
  virtual std::string DebugString() const { return "BaseObject"; }
};

std::ostream& operator<<(std::ostream& out, const BaseObject* o);  /// NOLINT

typedef std::function<void(Solver*)> SolverAction;
typedef std::function<void()> SolverClosure;

/// This enum is used internally in private methods Solver::PushState and
/// Solver::PopState to tag states in the search tree.
enum TrailMarkerType {
  SENTINEL,
  SIMPLE_MARKER,
  CHOICE_POINT,
  REVERSIBLE_ACTION
};

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
  StateInfo(SolverAction a, bool fast)
      : ptr_info(nullptr),
        int_info(static_cast<int>(fast)),
        depth(0),
        left_depth(0),
        reversible_action(std::move(a)) {}

  void* ptr_info;
  int int_info;
  int depth;
  int left_depth;
  SolverAction reversible_action;
};

struct StateMarker {
 public:
  StateMarker(TrailMarkerType t, const StateInfo& info);
  friend class ReversibleEngine;
  friend class Solver;
  friend struct Trail;

 private:
  TrailMarkerType type;
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

class ReversibleEngine {
 public:
  /// This enum represents the state of the engine w.r.t. the search.
  enum SolverState {
    /// Before search, after search.
    OUTSIDE_SEARCH,
    /// Executing the root node.
    IN_ROOT_NODE,
    /// Executing the search code.
    IN_SEARCH,
    /// After successful NextSolution and before EndSearch.
    AT_SOLUTION,
    /// After failed NextSolution and before EndSearch.
    NO_MORE_SOLUTIONS,
    /// After search, the model is infeasible.
    PROBLEM_INFEASIBLE
  };

  ReversibleEngine();
  virtual ~ReversibleEngine();

  void Init(int trail_block_size,
            ConstraintSolverParameters::TrailCompression trail_compression);

  /// State of the engine.
  SolverState state() const { return state_; }
  void check_alloc_state();
  void FillStateMarker(StateMarker* m);
  void BacktrackTo(StateMarker* m);

  /// reversibility

  /// SaveValue() saves the value of the corresponding object. It must be
  /// called before modifying the object. The value will be restored upon
  /// backtrack.
  template <class T>
  void SaveValue(T* o) {
    InternalSaveValue(o);
  }

  /// Registers the given object as being reversible. By calling this method,
  /// the caller gives ownership of the object to the engine, which will
  /// delete it when there is a backtrack out of the current state.
  ///
  /// Returns the argument for convenience: this way, the caller may directly
  /// invoke a constructor in the argument, without having to store the pointer
  /// first.
  ///
  /// This function is only for users that define their own subclasses of
  /// BaseObject: for all subclasses predefined in the library, the
  /// corresponding factory methods (e.g., MakeIntVar(...),
  /// MakeAllDifferent(...) already take care of the registration.
  template <typename T>
  T* RevAlloc(T* object) {
    return reinterpret_cast<T*>(SafeRevAlloc(object));
  }

  /// Like RevAlloc() above, but for an array of objects: the array
  /// must have been allocated with the new[] operator. The entire array
  /// will be deleted when backtracking out of the current state.
  ///
  /// This method is valid for arrays of int, int64_t, uint64_t, bool,
  /// BaseObject*, IntVar*, IntExpr*, and Constraint*.
  template <typename T>
  T* RevAllocArray(T* object) {
    return reinterpret_cast<T*>(SafeRevAllocArray(object));
  }

  /// All-in-one SaveAndSetValue.
  template <class T>
  void SaveAndSetValue(T* adr, T val) {
    if (*adr != val) {
      InternalSaveValue(adr);
      *adr = val;
    }
  }

  /// All-in-one SaveAndAdd_value.
  template <class T>
  void SaveAndAdd(T* adr, T val) {
    if (val != 0) {
      InternalSaveValue(adr);
      (*adr) += val;
    }
  }

  /// The stamp indicates how many moves in the search tree we have performed.
  /// It is useful to detect if we need to update same lazy structures.
  uint64_t stamp() const { return stamp_; }

  uint64_t& stamp_ref() { return stamp_; }

#if !defined(SWIG)
  template <class>
  friend class SimpleRevFIFO;
#endif  /// !defined(SWIG)

  // Faster version of SaveValue for bool.
  void InternalSaveBooleanVarValue(IntVar* var);
  void SetRestoreBooleanVarValue(
      std::function<void(IntVar* var)> restore_bool_value);

 protected:
  std::unique_ptr<Trail> trail_;
  SolverState state_;

 private:
  void InternalSaveValue(int* valptr);
  void InternalSaveValue(int64_t* valptr);
  void InternalSaveValue(uint64_t* valptr);
  void InternalSaveValue(double* valptr);
  void InternalSaveValue(bool* valptr);
  void InternalSaveValue(void** valptr);
  void InternalSaveValue(int64_t** valptr) {
    InternalSaveValue(reinterpret_cast<void**>(valptr));
  }

  BaseObject* SafeRevAlloc(BaseObject* ptr);

  int* SafeRevAllocArray(int* ptr);
  int64_t* SafeRevAllocArray(int64_t* ptr);
  uint64_t* SafeRevAllocArray(uint64_t* ptr);
  double* SafeRevAllocArray(double* ptr);
  BaseObject** SafeRevAllocArray(BaseObject** ptr);
  IntVar** SafeRevAllocArray(IntVar** ptr);
  IntExpr** SafeRevAllocArray(IntExpr** ptr);
  Constraint** SafeRevAllocArray(Constraint** ptr);
  /// UnsafeRevAlloc is used internally for cells in SimpleRevFIFO
  /// and other structures like this.
  void* UnsafeRevAllocAux(void* ptr);
  template <class T>
  T* UnsafeRevAlloc(T* ptr) {
    return reinterpret_cast<T*>(
        UnsafeRevAllocAux(reinterpret_cast<void*>(ptr)));
  }
  void** UnsafeRevAllocArrayAux(void** ptr);
  template <class T>
  T** UnsafeRevAllocArray(T** ptr) {
    return reinterpret_cast<T**>(
        UnsafeRevAllocArrayAux(reinterpret_cast<void**>(ptr)));
  }

  uint64_t stamp_ = 1;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_REVERSIBLE_ENGINE_H_
