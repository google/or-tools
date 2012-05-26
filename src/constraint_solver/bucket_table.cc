// Copyright 2011-2012 Jean Charles Régin
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


#include "base/hash.h"
#include "base/int-type.h"
#include "base/int-type-indexed-vector.h"
#include "base/map-util.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "util/vector_map.h"

namespace operations_research {
namespace {
// ****************************************************************************
//
// GAC-4 Revisited (c) Jean-Charles Régin 2012
//
// ****************************************************************************
class Ac4TableConstraint;

class IndexedTable {
 public:
  IndexedTable(const IntTupleSet& table)
  : tuples_of_indices_(table.NumTuples() * table.Arity()),
    value_map_per_variable_(table.Arity()),
    num_tuples_per_value_(table.Arity()),
    arity_(table.Arity()),
    num_tuples_(table.NumTuples()) {
    for (int i = 0; i < arity_; i++) {
      num_tuples_per_value_[i].resize(table.NumDifferentValuesInColumn(i), 0);
      for (int t = 0; t < table.NumTuples(); t++) {
        const int64 val = table.Value(t, i);
        if (!value_map_per_variable_[i].Contains(val)) {
          value_map_per_variable_[i].Add(val);
        }
        const int index = IndexFromValue(i, val);
        tuples_of_indices_[t * arity_ + i] = index;
        num_tuples_per_value_[i][index]++;
      }
    }
  }

  ~IndexedTable() {}

  int NumVars() const { return arity_; }

  int ValueIndex(int tuple_index, int var_index) const {
    return tuples_of_indices_[tuple_index * arity_ + var_index];
  }

  int IndexFromValue(int var_index, int64 value) const {
    return value_map_per_variable_[var_index].Index(value);
  }

  int64 ValueFromIndex(int var_index, int value_index) const {
    return value_map_per_variable_[var_index].Element(value_index);
  }

  bool IsValueValid(int var_index, int64 value) const {
    return value_map_per_variable_[var_index].Contains(value);
  }

  int NumTuplesContainingValueIndex(int var_index, int value_index) const {
    return num_tuples_per_value_[var_index][value_index];
  }

  int NumTuples() const {
    return num_tuples_;
  }

  int NumDifferentValuesInColumn(int var_index) const {
    return num_tuples_per_value_[var_index].size();
  }

 private:
  std::vector<int> tuples_of_indices_;
  std::vector<VectorMap<int64> > value_map_per_variable_;
  // number of tuples per value
  std::vector<std::vector<int> > num_tuples_per_value_;
  const int arity_;
  const int num_tuples_;
};

template <class T> class FastRevIntList {
 public:
  FastRevIntList(const int capacity)
  : elements_(new T[capacity]),
    num_elements_(0),
    capacity_(capacity) {}

  ~FastRevIntList() {}

  int Size() const { return num_elements_.Value(); }

  int Capacity() const { return capacity_; }

  T operator[](int i) const {
    DCHECK_LT(i, capacity_);
    return elements_[i];
  }

  int push_back(Solver* const solver, T elt) {
    const int size = num_elements_.Value();
    DCHECK_LT(size, capacity_);
    elements_[size] = elt;
    num_elements_.Incr(solver);
    return size;
  }

  void push_back_from_index(Solver* const solver,
                            int i,
                            T iElt, T endBackElt) {
    elements_[i] = endBackElt;
    elements_[num_elements_.Value()] = iElt;
    num_elements_.Incr(solver);
  }

  T end_back() const { return elements_[num_elements_.Value()]; }

  T back() const { return elements_[num_elements_.Value() - 1]; }

  void erase(Solver* const solver,
             int i,
             T iElt,
             T backElt,
             int* posElt,
             int* posBack) {
    num_elements_.Decr(solver);
    elements_[num_elements_.Value()] = iElt;
    elements_[i] = backElt;
    *posElt = num_elements_.Value();
    *posBack = i;
  }

  void clear(Solver* const solver) {
    num_elements_.SetValue(solver, 0);
  }

 private:
  scoped_array<T> elements_; // set of elements.
  NumericalRev<int> num_elements_; // number of elements in the set.
  const int capacity_;
};

class TableVar {
 public:
  TableVar(Solver* const solver,
           IntVar* var,
           const int var_index,
           IndexedTable* const table)
      : solver_(solver),
        var_index_(var_index),
        tuples_per_value_(table->NumDifferentValuesInColumn(var_index)),
        active_values_(tuples_per_value_.size()),
        index_in_active_values_(tuples_per_value_.size()),
        var_(var),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        reverse_tuples_(table->NumTuples()) {
    for (int value_index = 0;
         value_index < tuples_per_value_.size();
         value_index++) {
      tuples_per_value_[value_index] = new FastRevIntList<int>(
          table->NumTuplesContainingValueIndex(var_index, value_index));
      index_in_active_values_[value_index] =
          active_values_.push_back(solver_, value_index);
    }
  }

  ~TableVar() {
    STLDeleteElements(&tuples_per_value_); // delete all elements of a vector
  }

  IntVar* Variable() const {
    return var_;
  }

  IntVarIterator* DomainIterator() const {
    return domain_iterator_;
  }

  IntVarIterator* DeltaDomainIterator() const {
    return delta_domain_iterator_;
  }

  void RemoveActiveValue(Solver* solver, int value_index) {
    const int back_value_index = active_values_.back();
    active_values_.erase(
        solver_,
        index_in_active_values_[value_index],
        value_index,
        back_value_index,
        &index_in_active_values_[value_index],
        &index_in_active_values_[back_value_index]);
  }

  void RemoveOneTuple(int erased_tuple_index, IndexedTable* const table) {
    const int value_index =
        table->ValueIndex(erased_tuple_index, var_index_);
    FastRevIntList<int>* const var_value = tuples_per_value_[value_index];
    const int tuple_index_in_value = reverse_tuples_[erased_tuple_index];
    const int back_tuple_index = var_value->back();
    var_value->erase(
        var_->solver(),
        tuple_index_in_value,
        erased_tuple_index,
        back_tuple_index,
        &reverse_tuples_[erased_tuple_index],
        &reverse_tuples_[back_tuple_index]);
    if (var_value->Size() == 0) {
      var_->RemoveValue(table->ValueFromIndex(var_index_, value_index));
      RemoveActiveValue(var_->solver(), value_index);
    }
  }

  int NumTuplesPerValue(int value_index) const {
    return tuples_per_value_[value_index]->Size();
  }

  FastRevIntList<int>* ActiveTuples(int value_index) const {
    return tuples_per_value_[value_index];
  }

  int NumActiveValues() const {
    return active_values_.Size();
  }

  int ActiveValue(int index) const {
    return active_values_[index];
  }

  void RestoreTuple(int tuple_index, IndexedTable* const table) {
    FastRevIntList<int>* const active_tuples =
        tuples_per_value_[table->ValueIndex(tuple_index, var_index_)];
    const int index_of_value = reverse_tuples_[tuple_index];
    const int ebt = active_tuples->end_back();
    reverse_tuples_[ebt] = index_of_value;
    reverse_tuples_[tuple_index] = active_tuples->Size();
    active_tuples->push_back_from_index(
        var_->solver(), index_of_value, tuple_index, ebt);
  }

  void Init(IndexedTable* const table) {
    const int num_tuples = table->NumTuples();
    for (int tuple_index = 0; tuple_index < num_tuples; tuple_index++) {
      FastRevIntList<int>* const active_tuples =
          tuples_per_value_[table->ValueIndex(tuple_index, var_index_)];
      reverse_tuples_[tuple_index] = active_tuples->Size();
      active_tuples->push_back(var_->solver(), tuple_index);
    }
  }

  bool CheckResetProperty(const std::vector<int>& delta,
                          IndexedTable* const table) {
    // retourne true si on doit faire un reset. on compte le nb de
    // tuples qui vont etre supprimés.
    int num_deleted_tuples = 0;
    for (int k = 0; k < delta.size(); k++) {
      num_deleted_tuples += NumTuplesPerValue(delta[k]);
    }
    // on calcule le nombre de tuples en balayant les valeurs du
    // domaine de la var x
    int num_tuples_in_domain = 0;
    IntVarIterator* const it = DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = table->IndexFromValue(var_index_, it->Value());
      num_tuples_in_domain += NumTuplesPerValue(value_index);
    }
    return (num_tuples_in_domain < num_deleted_tuples);
  }

  void ComputeDeltaDomain(IndexedTable* const table, std::vector<int>* delta) {
    // calcul du delta domain de or-tools: on remplit le tableau
    // delta_ ATTENTION une val peut etre plusieurs fois dans le delta
    // : ici on s'en fout.
    delta->clear();
    // we iterate the delta of the variable
    //
    // ATTENTION code for or-tools: the delta iterator does not
    // include the values between oldmin and min and the values
    // between max and oldmax
    //
    // therefore we decompose the iteration into 3 parts
    // - from oldmin to min
    // - for the deleted values between min and max
    // - from max to oldmax
    // First iteration: from oldmin to min
    const int64 oldmindomain = var_->OldMin();
    const int64 mindomain = var_->Min();
    for (int64 val = oldmindomain; val < mindomain; ++val) {
      if (table->IsValueValid(var_index_, val)) {
        delta->push_back(table->IndexFromValue(var_index_, val));
      }
    }
    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = DeltaDomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      if (table->IsValueValid(var_index_, val)) {
        delta->push_back(table->IndexFromValue(var_index_, val));
      }
    }
    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var_->OldMax();
    const int64 maxdomain = var_->Max();
    for (int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      if (table->IsValueValid(var_index_, val)) {
        delta->push_back(table->IndexFromValue(var_index_, val));
      }
    }
  }

  void PropagateDeletedValues(IndexedTable* const table,
                              const std::vector<int>& delta,
                              std::vector<int>* const removed_tuples) {
    removed_tuples->clear();
    const int delta_size = delta.size();
    for (int k = 0; k < delta_size; k++) {
      FastRevIntList<int>* const tuples_to_remove = tuples_per_value_[delta[k]];
      const int num_tuples_to_erase = tuples_to_remove->Size();
      for (int index = 0; index < num_tuples_to_erase; index++) {
        removed_tuples->push_back((*tuples_to_remove)[index]);
      }
    }
  }

  void RemoveUnsupportedValues(IndexedTable* const table) {
    IntVarIterator* const it = DomainIterator();
    int num_removed = 0;
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = table->IndexFromValue(var_index_, it->Value());
      if (NumTuplesPerValue(value_index) == 0) {
        RemoveActiveValue(var_->solver(), value_index);
        num_removed++;
      }
    }
    // Removed values have been pushed after the lists.
    const int last_valid_value = active_values_.Size();
    for (int cpt = 0; cpt < num_removed; cpt++) {
      const int value_index = ActiveValue(last_valid_value + cpt);
      const int64 removed_value =
          table->ValueFromIndex(var_index_, value_index);
      var_->RemoveValue(removed_value);
    }
  }

 private:
  Solver* const solver_;
  const int var_index_;
  // one LAA per value of the variable
  std::vector<FastRevIntList<int>*> tuples_per_value_;
  // list of values: having a non empty tuple list
  FastRevIntList<int> active_values_;
  std::vector<int> index_in_active_values_;
  IntVar* const var_;
  IntVarIterator* const domain_iterator_;
  IntVarIterator* const delta_domain_iterator_;
  // Flat tuples of value indices.
  std::vector<int> reverse_tuples_;
};

class Ac4TableConstraint : public Constraint {
 public:
  Ac4TableConstraint(Solver* const solver,
                     IndexedTable* const table,
                     const std::vector<IntVar*>& vars)
      : Constraint(solver),
        vars_(table->NumVars()),
        table_(table),
        tmp_tuples_(table->NumTuples()),
        delta_of_value_indices_(table->NumTuples()),
        num_variables_(table->NumVars()) {
    for (int var_index = 0; var_index < table->NumVars(); var_index++) {
      vars_[var_index] =
          new TableVar(solver, vars[var_index], var_index, table);
    }
  }

  ~Ac4TableConstraint() {
    STLDeleteElements(&vars_); // delete all elements of a vector
    delete table_;
  }

  void Post() {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      Demon* const demon =
          MakeConstraintDemon1(solver(),
                               this,
                               &Ac4TableConstraint::FilterX,
                               "FilterX",
                               var_index);
      vars_[var_index]->Variable()->WhenDomain(demon);
    }
  }

  void InitialPropagate() {
    InitAllVariables();
    std::vector<int64> to_remove;
    // we remove from the domain the values that are not in the table,
    // or that have no supporting tuples.
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      to_remove.clear();
      IntVarIterator* const it = vars_[var_index]->DomainIterator();
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (!table_->IsValueValid(var_index, value) ||
            vars_[var_index]->NumTuplesPerValue(
                table_->IndexFromValue(var_index, value)) == 0) {
          to_remove.push_back(value);
        }
      }
      vars_[var_index]->Variable()->RemoveValues(to_remove);
    }
    //    RemoveUnsupportedValues();
  }

  void FilterX(int var_index) {
    TableVar* const var = vars_[var_index];
    var->ComputeDeltaDomain(table_, &delta_of_value_indices_);
    if (var->CheckResetProperty(delta_of_value_indices_, table_)) {
      Reset(var_index);
    }
    var->PropagateDeletedValues(table_, delta_of_value_indices_, &tmp_tuples_);
    // The tuple is erased for each value it contains.
    for (int i = 0; i < tmp_tuples_.size(); ++i) {
      RemoveOneTupleFromAllVariables(tmp_tuples_[i]);
    }
  }

 private:
  void RemoveOneTupleFromAllVariables(int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      TableVar* const var = vars_[var_index];
      var->RemoveOneTuple(tuple_index, table_);
    }
  }

  void RemoveUnsupportedValuesOnAllVariables() {
    // We scan values to check the ones without the supported values.
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->RemoveUnsupportedValues(table_);
    }
  }

  // Clean and re-add all active tuples.
  void Reset(int var_index) {
    TableVar* const var = vars_[var_index];
    int s = 0;
    tmp_tuples_.clear();
    IntVarIterator* const it = var->DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = table_->IndexFromValue(var_index, it->Value());
      FastRevIntList<int>* const active_tuples = var->ActiveTuples(value_index);
      const int num_tuples = active_tuples->Size();
      for (int j = 0; j < num_tuples; j++) {
        tmp_tuples_.push_back((*active_tuples)[j]);
      }
    }
    // We clear all values in the TableVar structure.
    for (int i = 0; i < num_variables_; i++) {
      for (int k = 0; k < vars_[i]->NumActiveValues(); k++) {
        const int value_index = vars_[i]->ActiveValue(k);
        vars_[i]->ActiveTuples(value_index)->clear(solver());
      }
    }

    // We add tuples one by one from the tmp_tuples_ structure.
    const int size = tmp_tuples_.size();
    for (int j = 0; j < size; j++) {
      const int tuple_index = tmp_tuples_[j];
      for (int var_index = 0; var_index < num_variables_; var_index++) {
        vars_[var_index]->RestoreTuple(tuple_index, table_);
      }
    }

    // Si une valeur n'a plus de support on la supprime
    RemoveUnsupportedValuesOnAllVariables();
  }

  void InitAllVariables() {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->Init(table_);
    }
  }

  std::vector<TableVar*> vars_; // variable of the constraint
  IndexedTable* const table_; // table
  // On peut le supprimer si on a un tableau temporaire d'entier qui
  // est disponible. Peut contenir tous les tuples
  std::vector<int> tmp_tuples_;
  // Temporary storage for delta of one variable.
  std::vector<int> delta_of_value_indices_;
  // Number of variables.
  const int num_variables_;
};
}  // namespace

// External API.
Constraint* BuildAc4TableConstraint(Solver* const solver,
                                    IntTupleSet& tuples,
                                    const std::vector<IntVar*>& vars,
                                    int size_bucket) {
  return solver->RevAlloc(
      new Ac4TableConstraint(solver, new IndexedTable(tuples), vars));
}
} // namespace operations_research
