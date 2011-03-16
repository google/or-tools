// Copyright 2010 Google
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

#ifndef CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
#define CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_

#include <math.h>

#include <string>
#include <vector>

#include "base/callback-types.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/sysinfo.h"
#include "base/timer.h"
#include "base/join.h"
#include "base/bitmap.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solver.h"

class WallTimer;

namespace operations_research {
// This is the base class for all non-variable expressions.
// It proposes a basic 'cast-to-var' implementation.

class BaseIntExpr : public IntExpr {
 public:
  explicit BaseIntExpr(Solver* const s) : IntExpr(s), var_(NULL) {}
  virtual ~BaseIntExpr() {}

  virtual IntVar* Var();
  virtual IntVar* CastToVar();
  void AddDelegateName(const string& prefix,
                       const PropagationBaseObject* delegate) const;
 private:
  IntVar* var_;
};

// ----- utility classes -----

// This class represent a reversible FIFO structure.
// The main diffence w.r.t a standart FIFO structure is that a Solver is
// given as parameter to the modifiers such that the solver can store the
// backtrack information
// Iterator's traversing order should not be changed, as some algorithm
// depend on it to be .
#ifndef SWIG
template <class T> class SimpleRevFIFO {
 private:
  enum { CHUNK_SIZE = 16 };  // TODO(user): could be an extra template param
  struct Chunk {
    T data_[CHUNK_SIZE];
    const Chunk* const next_;
    explicit Chunk(const Chunk* next) : next_(next) {}
  };

 public:
  // This iterator is non stable w.r.t. deletion.
  class Iterator {
   public:
    explicit Iterator(const SimpleRevFIFO<T>* l)
        : chunk_(l->chunks_), data_(l->Last()) {}
    bool ok() const { return (data_ != NULL); }
    T operator*() const { return *data_; }
    void operator++() {
      ++data_;
      if (data_ == chunk_->data_ + CHUNK_SIZE) {
        chunk_ = chunk_->next_;
        data_ = chunk_ ? chunk_->data_ : NULL;
      }
    }

   private:
    const Chunk* chunk_;
    const T* data_;
  };

  SimpleRevFIFO() : chunks_(NULL), pos_(0) {}

  void Push(Solver* const s, T val) {
    if (pos_ == 0) {
      Chunk* const chunk = s->UnsafeRevAlloc(new Chunk(chunks_));
      s->SaveAndSetValue(reinterpret_cast<void**>(&chunks_),
                         reinterpret_cast<void*>(chunk));
      s->SaveAndSetValue(&pos_, CHUNK_SIZE - 1);
    } else {
      s->SaveAndAdd(&pos_, -1);
    }
    chunks_->data_[pos_] = val;
  }

  void PushIfNotTop(Solver* const s, T val) {
    if (chunks_ == NULL || LastValue() != val) {
      Push(s, val);
    }
  }
  const T* Last() const { return chunks_ ? &chunks_->data_[pos_] : NULL; }
  const T& LastValue() const {
    DCHECK(chunks_);
    return chunks_->data_[pos_];
  }
  void SetLastValue(const T& v) {
    DCHECK(Last());
    chunks_->data_[pos_] = v;
  }

 private:
  Chunk *chunks_;
  int pos_;
};

// These methods represents generic demons that will call back a
// method on the constraint during their Run method.

// ----- Call one method with no param -----
template <class T> class CallMethod0 : public Demon {
 public:
  CallMethod0(T* const ct, void (T::*method)(), const string& name)
      : constraint_(ct), method_(method), name_(name) {}

  virtual ~CallMethod0() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)();
  }

  virtual string DebugString() const {
    return "CallMethod_" + name_ + "(" + constraint_->DebugString() + ")";
  }

 private:
  T* const constraint_;
  void (T::* const method_)();
  const string name_;
};

template <class T> Demon* MakeConstraintDemon0(Solver* const s,
                                               T* const ct,
                                               void (T::*method)(),
                                               const string& name) {
  return s->RevAlloc(new CallMethod0<T>(ct, method, name));
}

// ----- Call one method with one param -----
template <class T, class P> class CallMethod1 : public Demon {
 public:
  CallMethod1(T* const ct,
              void (T::*method)(P),
              const string& name,
              P param1)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1) {}

  virtual ~CallMethod1() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)(param1_);
  }

  virtual string DebugString() const {
    return StrCat(StrCat("CallMethod_", name_),
                  StrCat("(", constraint_->DebugString(), ", "),
                  StrCat(param1_, ")"));
  }
 private:
  T* const constraint_;
  void (T::* const method_)(P);
  const string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeConstraintDemon1(Solver* const s,
                            T* const ct,
                            void (T::*method)(P),
                            const string& name,
                            P param1) {
  return s->RevAlloc(new CallMethod1<T, P>(ct, method, name, param1));
}

// ----- Call one method with two params -----
template <class T, class P, class Q> class CallMethod2 : public Demon {
 public:
  CallMethod2(T* const ct,
              void (T::*method)(P, Q),
              const string& name,
              P param1,
              Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  virtual ~CallMethod2() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)(param1_, param2_);
  }

  virtual string DebugString() const {
    return StrCat(StrCat("CallMethod_", name_),
                  StrCat("(", constraint_->DebugString()),
                  StrCat(", ", param1_),
                  StrCat(", ", param2_, ")"));
  }
 private:
  T* const constraint_;
  void (T::* const method_)(P, Q);
  const string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeConstraintDemon2(Solver* const s,
                            T* const ct,
                            void (T::*method)(P, Q),
                            const string& name,
                            P param1,
                            Q param2) {
  return s->RevAlloc(new CallMethod2<T, P, Q>(ct,
                                              method,
                                              name,
                                              param1,
                                              param2));
}

// These methods represents generic demons that will call back a
// method on the constraint during their Run method. This demon will
// have a priority DELAYED_PRIORITY.

// ----- DelayedCall one method with no param -----
template <class T> class DelayedCallMethod0 : public Demon {
 public:
  DelayedCallMethod0(T* const ct, void (T::*method)(), const string& name)
      : constraint_(ct), method_(method), name_(name) {}

  virtual ~DelayedCallMethod0() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)();
  }

  virtual Solver::DemonPriority priority() const {
    return Solver::DELAYED_PRIORITY;
  }

  virtual string DebugString() const {
    return "DelayedCallMethod_"
        + name_ + "(" + constraint_->DebugString() + ")";
  }

 private:
  T* const constraint_;
  void (T::* const method_)();
  const string name_;
};

template <class T> Demon* MakeDelayedConstraintDemon0(Solver* const s,
                                                      T* const ct,
                                                      void (T::*method)(),
                                                      const string& name) {
  return s->RevAlloc(new DelayedCallMethod0<T>(ct, method, name));
}

// ----- DelayedCall one method with one param -----
template <class T, class P> class DelayedCallMethod1 : public Demon {
 public:
  DelayedCallMethod1(T* const ct,
                     void (T::*method)(P),
                     const string& name,
                     P param1)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1) {}

  virtual ~DelayedCallMethod1() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)(param1_);
  }

  virtual Solver::DemonPriority priority() const {
    return Solver::DELAYED_PRIORITY;
  }

  virtual string DebugString() const {
    return StrCat(StrCat("DelayedCallMethod_", name_),
                  StrCat("(", constraint_->DebugString(), ", "),
                  StrCat(param1_, ")"));
  }
 private:
  T* const constraint_;
  void (T::* const method_)(P);
  const string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeDelayedConstraintDemon1(Solver* const s,
                                   T* const ct,
                                   void (T::*method)(P),
                                   const string& name,
                                   P param1) {
  return s->RevAlloc(new DelayedCallMethod1<T, P>(ct, method, name, param1));
}

// ----- DelayedCall one method with two params -----
template <class T, class P, class Q> class DelayedCallMethod2 : public Demon {
 public:
  DelayedCallMethod2(T* const ct,
                     void (T::*method)(P, Q),
                     const string& name,
                     P param1,
                     Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  virtual ~DelayedCallMethod2() {}

  virtual void Run(Solver* const s) {
    (constraint_->*method_)(param1_, param2_);
  }

  virtual Solver::DemonPriority priority() const {
    return Solver::DELAYED_PRIORITY;
  }

  virtual string DebugString() const {
    return StrCat(StrCat("DelayedCallMethod_", name_),
                  StrCat("(", constraint_->DebugString()),
                  StrCat(", ", param1_),
                  StrCat(", ", param2_, ")"));
  }
 private:
  T* const constraint_;
  void (T::* const method_)(P, Q);
  const string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeDelayedConstraintDemon2(Solver* const s,
                                   T* const ct,
                                   void (T::*method)(P, Q),
                                   const string& name,
                                   P param1,
                                   Q param2) {
  return s->RevAlloc(new DelayedCallMethod2<T, P, Q>(ct,
                                                     method,
                                                     name,
                                                     param1,
                                                     param2));
}

#endif   // !defined(SWIG)

class NestedSolveDecision;

// ---------- Local search operators ----------

// A local search operator is an object which defines the neighborhood of a
// solution; in other words, a neighborhood is the set of solutions which can
// be reached from a given solution using an operator.
// The behavior of the LocalSearchOperator class is similar to the one of an
// iterator. The operator is synchronized with an assignment (gives the
// current values of the variables); this is done in the Start() method.
// Then one can iterate over the neighbors using the MakeNextNeighbor method.
// This method returns an assignment which represents the incremental changes
// to the curent solution. It also returns a second assignment representing the
// changes to the last solution defined by the neighborhood operator; this
// assignment is empty is the neighborhood operator cannot track this
// information.
// TODO(user): rename Start to Synchronize ?
// TODO(user): decouple the iterating from the defining of a neighbor

// Mother of all local search operators
class LocalSearchOperator : public BaseObject {
 public:
  LocalSearchOperator() {}
  virtual ~LocalSearchOperator() {}
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) = 0;
  virtual void Start(const Assignment* assignment) = 0;
};

// ----- Base operator class for operators manipulating IntVars -----

// Built from an array of IntVars which specifies the scope of the operator
// This class also takes care of storing current variable values in Start(),
// keeps track of changes done by the operator and builds the delta.
// The Deactivate() method can be used to perform Large Neighborhood Search.

class IntVarLocalSearchOperator : public LocalSearchOperator {
 public:
  IntVarLocalSearchOperator(const IntVar* const* vars, int size);
  virtual ~IntVarLocalSearchOperator();
  // This method should not be overridden. Override OnStart() instead which is
  // called before exiting this method.
  virtual void Start(const Assignment* assignment);
  virtual bool IsIncremental() const { return false; }
  int Size() const { return size_; }
  // Returns the value in the current assignment of the variable of given index.
  int64 Value(int64 index) const {
    DCHECK_LT(index, size_);
    return values_[index];
  }
  // Returns the variable of given index.
  IntVar* Var(int64 index) const { return vars_[index]; }
  virtual bool SkipUnchanged(int index) const { return false; }
 protected:
  // Called by Start() after synchronizing the operator with the current
  // assignment. Should be overridden instead of Start() to avoid calling
  // IntVarLocalSearchOperator::Start explicitly.
  virtual void OnStart() {}
  int64 OldValue(int64 index) const { return old_values_[index]; }
  void SetValue(int64 index, int64 value);
  bool Activated(int64 index) const;
  void Activate(int64 index);
  void Deactivate(int64 index);
  bool ApplyChanges(Assignment* delta, Assignment* deltadelta) const;
  void RevertChanges(bool incremental);
  void AddVars(const IntVar* const* vars, int size);
 private:
  void MarkChange(int64 index);

  scoped_array<IntVar*> vars_;
  int size_;
  scoped_array<int64> values_;
  scoped_array<int64> old_values_;
  Bitmap activated_;
  Bitmap was_activated_;
  vector<int64> changes_;
  Bitmap has_changed_;
  Bitmap has_delta_changed_;
  bool cleared_;
};

// ----- Base Large Neighborhood Search operator class ----

// This is the base class for building an LNS operator. An LNS fragment is a
// collection of variables which will be relaxed. Fragments are build with
// NextFragment(), which returns false if there are no more fragments to build.
// Optionally one can override InitFragments, which is called from
// LocalSearchOperator::Start to initialize fragment data.
//
// Here's a sample relaxing each variable:
//
// class OneVarLNS : public BaseLNS {
//  public:
//   OneVarLNS(const IntVar* const* vars, int size)
//       : BaseLNS(vars, size), index_(0) {}
//   virtual ~OneVarLNS() {}
//   virtual void InitFragments() { index_ = 0; }
//   virtual bool NextFragment(vector<int>* fragment) {
//     const int size = Size();
//     if (index_ < size) {
//       fragment->push_back(index_);
//       ++index_;
//       return true;
//     } else {
//       return false;
//     }
//   }
//  private:
//   int index_;
// };

class BaseLNS : public IntVarLocalSearchOperator {
 public:
  BaseLNS(const IntVar* const* vars, int size);
  virtual ~BaseLNS();
  // This method should not be overridden (it calls NextFragment()).
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta);
  virtual void InitFragments();
  virtual bool NextFragment(vector<int>* fragment) = 0;
 protected:
  // This method should not be overridden. Override InitFragments() instead.
  virtual void OnStart();
};

// ----- ChangeValue Operators -----

// Defines operators which change the value of variables;
// each neighbor corresponds to *one* modified variable.
// Sub-classes have to define ModifyValue which determines what the new
// variable value is going to be (given the current value and the variable).

class ChangeValue : public IntVarLocalSearchOperator {
 public:
  ChangeValue(const IntVar* const* vars, int size);
  virtual ~ChangeValue();
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta);
  virtual int64 ModifyValue(int64 index, int64 value) = 0;
 protected:
  virtual void OnStart();
 private:
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
  PathOperator(const IntVar* const* next_vars,
               const IntVar* const* path_vars,
               int size,
               int number_of_base_nodes);
  virtual ~PathOperator() {}
  virtual bool MakeNeighbor() = 0;
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta);

  // TODO(user): Make the following methods protected.
  virtual bool SkipUnchanged(int index) const;

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
  virtual void OnStart();
  // Called by OnStart() after initializing node information. Should be
  // overriden instead of OnStart() to avoid calling PathOperator::OnStart
  // explicitly.
  virtual void OnNodeInitialization() {}
  // Returns the index of the variable corresponding to the ith base node.
  int64 BaseNode(int i) const { return base_nodes_[i]; }
  int64 StartNode(int i) const { return path_starts_[base_paths_[i]]; }

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
  void ResetPosition() {
    just_started_ = true;
  }
 protected:
  const int number_of_nexts_;
  const bool ignore_path_vars_;
 private:
  bool CheckEnds() const;
  bool IncrementPosition();
  void InitializePathStarts();
  void InitializeInactives();
  void InitializeBaseNodes();
  bool CheckChainValidity(int64 chain_start,
                          int64 chain_end,
                          int64 exclude) const;
  void Synchronize();

  vector<int> base_nodes_;
  vector<int> end_nodes_;
  vector<int> base_paths_;
  vector<int64> path_starts_;
  vector<bool> inactives_;
  bool just_started_;
  bool first_start_;
};

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

  // Synchronizes the filter with the current solution
  virtual void Synchronize(const Assignment* assignment) = 0;
  virtual bool IsIncremental() const { return false; }
};

// ----- IntVarLocalSearchFilter -----

class IntVarLocalSearchFilter : public LocalSearchFilter {
 public:
  IntVarLocalSearchFilter(const IntVar* const* vars, int size);
  ~IntVarLocalSearchFilter();
 protected:
  // Add variables to "track" to the filter.
  void AddVars(const IntVar* const* vars, int size);
  // This method should not be overridden. Override OnSynchronize() instead
  // which is called before exiting this method.
  virtual void Synchronize(const Assignment* assignment);
  virtual void OnSynchronize() {}
  bool FindIndex(const IntVar* const var, int64* index) const {
    DCHECK(index != NULL);
    return FindCopy(var_to_index_, var, index);
  }
  int Size() const { return size_; }
  IntVar* Var(int index) const { return vars_[index]; }
  int64 Value(int index) const { return values_[index]; }
 private:
  scoped_array<IntVar*> vars_;
  scoped_array<int64> values_;
  int size_;
  hash_map<const IntVar*, int64> var_to_index_;
};

// ----- Local search decision builder -----

// Given a first solution (resulting from either an initial assignment or the
// result of a decision builder), it searches for neighbors using a local
// search operator. The first solution corresponds to the first leaf of the
// search.
// The local search applies to the variables contained either in the assignment
// or the vector of variables passed.

class LocalSearch : public DecisionBuilder {
 public:
  LocalSearch(Assignment* const assignment,
              SolutionPool* const pool,
              LocalSearchOperator* const ls_operator,
              DecisionBuilder* const sub_decision_builder,
              SearchLimit* const limit,
              const vector<LocalSearchFilter*>& filters);
  // TODO(user): find a way to not have to pass vars here: redundant with
  // variables in operators
  LocalSearch(IntVar* const* vars,
              int size,
              SolutionPool* const pool,
              DecisionBuilder* const first_solution,
              LocalSearchOperator* const ls_operator,
              DecisionBuilder* const sub_decision_builder,
              SearchLimit* const limit,
              const vector<LocalSearchFilter*>& filters);
  virtual ~LocalSearch();
  virtual Decision* Next(Solver* const solver);
  virtual string DebugString() const {
    return "LocalSearch";
  }
 protected:
  void PushFirstSolutionDecision(DecisionBuilder* first_solution);
  void PushLocalSearchDecision();
 private:
  Assignment* assignment_;
  SolutionPool* const pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  vector<NestedSolveDecision*> nested_decisions_;
  int nested_decision_index_;
  SearchLimit* const limit_;
  const vector<LocalSearchFilter*> filters_;
  bool has_started_;
};

// ---------- SymmetryBreaker ----------

class SymmetryManager;

class SymmetryBreaker : public DecisionVisitor {
 public:
  SymmetryBreaker() : symmetry_manager_(NULL) {}
  virtual ~SymmetryBreaker() {}

  void set_symmetry_manager(SymmetryManager* manager) {
    symmetry_manager_ = manager;
  }
  SymmetryManager* symmetry_manager() const { return symmetry_manager_; }
  void AddToClause(IntVar* term);
 private:
  SymmetryManager* symmetry_manager_;
};

// ---------- Search Log ---------

class SearchLog : public SearchMonitor {
 public:
  SearchLog(Solver* const s,
            OptimizeVar* const obj,
            IntVar* const var,
            ResultCallback<string>* display_callback,
            int period);
  virtual ~SearchLog();
  virtual void EnterSearch();
  virtual void ExitSearch();
  virtual bool AtSolution();
  virtual void BeginFail();
  virtual void NoMoreSolutions();
  virtual void ApplyDecision(Decision* const decision);
  virtual void RefuteDecision(Decision* const decision);
  void OutputDecision();
  void Maintain();
  virtual void BeginInitialPropagation();
  virtual void EndInitialPropagation();
 protected:
  /* Bottleneck function used for all UI related output. */
  virtual void OutputLine(const string& line);
 private:
  static string MemoryUsage();

  const int period_;
  scoped_ptr<WallTimer> timer_;
  IntVar* const var_;
  OptimizeVar* const obj_;
  scoped_ptr<ResultCallback<string> > display_callback_;
  int nsol_;
  int64 tick_;
  int64 objective_min_;
  int64 objective_max_;
  int min_right_depth_;
  int max_depth_;
  int sliding_min_depth_;
  int sliding_max_depth_;
};
}  // namespace operations_research

#endif  // CONSTRAINT_SOLVER_CONSTRAINT_SOLVERI_H_
