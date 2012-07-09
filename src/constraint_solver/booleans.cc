// Copyright 2010-2012 Google
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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/hash.h"
#include "base/int-type-indexed-vector.h"
#include "base/int-type.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map-util.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/const_int_array.h"

namespace operations_research {
namespace {
DEFINE_INT_TYPE(AtomIndex, int);

const static AtomIndex kFailAtom = AtomIndex(0);

template <class T> class UnorderedRevArray {
 public:
  UnorderedRevArray(int initial_capacity)
  : elements_(), num_elements_(0) {
    elements_.reserve(initial_capacity);
  }

  UnorderedRevArray(const std::vector<T>& elements)
      : elements_(elements), num_elements_(elements.size()) {}

  ~UnorderedRevArray() {}

  int Size() const { return num_elements_.Value(); }

  T Element(int i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, num_elements_.Value());
    return elements_[i];
  }

  void Insert(Solver* const solver, T elt) {
    elements_.push_back(elt);
    num_elements_.Incr(solver);
  }

  void Remove(Solver* const solver, int position_of_element_to_remove) {
    num_elements_.Decr(solver);
    SwapTo(position_of_element_to_remove, num_elements_.Value());
  }

  void Clear(Solver* const solver) {
    num_elements_.SetValue(solver, 0);
  }

 private:
  void SwapTo(int current_position, int next_position) {
    if (current_position != next_position) {
      const T next_value = elements_[next_position];
      elements_[next_position] = elements_[current_position];
      elements_[current_position] = next_value;
    }
  }

  std::vector<T> elements_; // set of elements.
  NumericalRev<int> num_elements_; // number of elements in the set.
};

class Store;
class SumLessConstant;
class SumTriggerAction;

class Atom {
 public:
  Atom(AtomIndex index) : atom_index_(index) {}
  ~Atom() {}

  void Listen(SumLessConstant* const ct) {
    sum_less_constant_constraints_.push_back(ct);
  }

  void Listen(SumTriggerAction* const ct) {
    sum_trigger_actions_constraints_.insert(ct);
  }

  void StopListening(SumTriggerAction* const ct) {
    sum_trigger_actions_constraints_.erase(ct);
  }

  void AddFlipAction(AtomIndex action) {
    actions_.push_back(action);
  }

  void Flip(Store* const store);

  bool IsFlipped() const {
    return flipped_.Switched();
  }

 private:
  const AtomIndex atom_index_;
  std::vector<SumLessConstant*> sum_less_constant_constraints_;
  hash_set<SumTriggerAction*> sum_trigger_actions_constraints_;
  std::vector<AtomIndex> actions_;
  RevSwitch flipped_;
};

class Store : public Constraint {
 public:
  Store(Solver* const solver) : Constraint(solver) {}

  ~Store() {}

  AtomIndex TrueIndex(IntVar* const var) {
    int raw_index = indices_.Add(var);
    if (raw_index >= true_atoms_.size()) {
      true_atoms_.resize(raw_index);
      false_atoms_.resize(raw_index);
    }
    return 1 + AtomIndex(raw_index);
  }

  AtomIndex FalseIndex(IntVar* const var) {
    return -TrueIndex(var);
  }

  void VariableBound(int index) {
    if (indices_.Element(index)->Min() == 0) {
      Flip(AtomIndex(-1 - index));
    } else {
      Flip(AtomIndex(1 + index));
    }
  }

  void Listen(AtomIndex atom, SumLessConstant* const ct) {
    FindAtom(atom)->Listen(ct);
  }

  void Listen(AtomIndex atom, SumTriggerAction* const ct) {
    FindAtom(atom)->Listen(ct);
  }

  void StopListening(AtomIndex atom, SumTriggerAction* const ct) {
    FindAtom(atom)->StopListening(ct);
  }

  void Flip(AtomIndex atom) {
    if (atom == kFailAtom || IsFlipped(-atom)) {
      solver()->Fail();
    } else {
      FindAtom(atom)->Flip(this);
    }
  }

  bool IsFlipped(AtomIndex atom) {
    if (atom == kFailAtom) {
      return false;
    } else {
      return FindAtom(atom)->IsFlipped();
    }
  }

  virtual void Post() {
    for (int i = 0; i < indices_.size(); ++i) {
      Demon* const d = MakeConstraintDemon1(solver(),
                                            this,
                                            &Store::VariableBound,
                                            "VariableBound",
                                            i);
      indices_.Element(i)->WhenDomain(d);
    }
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < indices_.size(); ++i) {
      if (indices_.Element(i)->Bound()) {
        VariableBound(i);
      }
    }
  }

 private:
  Atom* FindAtom(AtomIndex atom) const {
    CHECK_NE(atom, kFailAtom);
    if (atom > kFailAtom) {
      return true_atoms_[atom.value() - 1];
    } else {
      return false_atoms_[-1 - atom.value()];
    }
  }

  VectorMap<IntVar*> indices_;
  std::vector<Atom*> true_atoms_;
  std::vector<Atom*> false_atoms_;
};

class SumLessConstant {
 public:
  SumLessConstant(std::vector<AtomIndex>& vars, int constant)
      : vars_(vars),
        constant_(constant),
        sum_(0) {}

  ~SumLessConstant() {}

  void Post(Store* const store) {
    for (int i = 0; i < vars_.size(); ++i) {
      store->Listen(vars_[i], this);
    }
  }

  void Flip(Store* const store, AtomIndex index) {
    sum_.Incr(store->solver());
    if (sum_.Value() > constant_) {
      store->solver()->Fail();
    } else if (sum_.Value() == constant_) {
      UnflipAllPending(store);
    }
  }

  void UnflipAllPending(Store* const store) {
    int count = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      if (!store->IsFlipped(vars_[i])) {
        count++;
        store->Flip(-vars_[i]);
      }
    }
    // Check count.
  }

 private:
  std::vector<AtomIndex> vars_;
  const int constant_;
  NumericalRev<int> sum_;
};

class SumTriggerAction {
 public:
  SumTriggerAction(std::vector<AtomIndex>& vars,
                   int constant,
                   std::vector<AtomIndex>& actions)
      : vars_(vars),
        constant_(constant),
        actions_(actions),
        sum_(0) {}

  ~SumTriggerAction() {}

  void Post(Store* const store) {
    for (int i = 0; i < vars_.size(); ++i) {
      store->Listen(vars_[i], this);
    }
  }

  void Flip(Store* const store, AtomIndex index) {
    sum_.Incr(store->solver());
    if (sum_.Value() >= constant_) {
      StopListening(store);
      FlipAllAction(store);
    }
  }

  void StopListening(Store* const store) {
    for (int i = 0; i < vars_.size(); ++i) {
      store->StopListening(vars_[i], this);
    }
  }

  void FlipAllAction(Store* const store) {
    for (int i = 0; i < actions_.size(); ++i) {
      store->Flip(actions_[i]);
    }
  }

 private:
  std::vector<AtomIndex> vars_;
  const int constant_;
  std::vector<AtomIndex> actions_;
  NumericalRev<int> sum_;
};

void Atom::Flip(Store* const store) {
  for (int i = 0; i < actions_.size(); ++i) {
    store->Flip(actions_[i]);
  }
  for (int i = 0; i < sum_less_constant_constraints_.size(); ++i) {
    sum_less_constant_constraints_[i]->Flip(store, atom_index_);
  }
  for (ConstIter<hash_set<SumTriggerAction*> > iter(
           sum_trigger_actions_constraints_); !iter.at_end(); ++iter) {
    (*iter)->Flip(store, atom_index_);
  }
}
}  // namespace




}  // namespace operations_research
