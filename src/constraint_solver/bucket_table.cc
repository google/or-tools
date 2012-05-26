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
  class Column {
   public:
    Column()
    : num_tuples_(0) {}

    void Init(const IntTupleSet& table, int var_index) {
      num_tuples_ = table.NumTuples();
      column_of_value_indices_.resize(num_tuples_);
      num_tuples_per_value_.resize(
          table.NumDifferentValuesInColumn(var_index), 0);
      for (int tuple_index = 0; tuple_index < num_tuples_; tuple_index++) {
        const int64 val = table.Value(tuple_index, var_index);
        if (!value_map_.Contains(val)) {
          value_map_.Add(val);
        }
        const int index = value_map_.Index(val);
        column_of_value_indices_[tuple_index] = index;
        num_tuples_per_value_[index]++;
      }
    }

    int ValueIndex(int tuple_index) const {
      return column_of_value_indices_[tuple_index];
    }

    int IndexFromValue(int64 value) const {
      return value_map_.Index(value);
    }

    int64 ValueFromIndex(int value_index) const {
      return value_map_.Element(value_index);
    }

    bool IsValueValid(int64 value) const {
      return value_map_.Contains(value);
    }

    int NumTuplesContainingValueIndex(int value_index) const {
      return num_tuples_per_value_[value_index];
    }

    int NumTuples() const {
      return num_tuples_;
    }

    int NumDifferentValues() const {
      return num_tuples_per_value_.size();
    }

   private:
    std::vector<int> column_of_value_indices_;
    VectorMap<int64> value_map_;
    std::vector<int> num_tuples_per_value_;
    int num_tuples_;
  };

  IndexedTable(const IntTupleSet& table)
  : arity_(table.Arity()),
    num_tuples_(table.NumTuples()),
    columns_(arity_) {
    for (int i = 0; i < arity_; i++) {
      columns_[i].Init(table, i);
    }
  }

  ~IndexedTable() {}

  int NumVars() const { return arity_; }

  const Column& column(int var_index) const {
    return columns_[var_index];
  }

  int NumTuples() const {
    return num_tuples_;
  }

 private:
  const int arity_;
  const int num_tuples_;
  std::vector<Column> columns_;
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
           const IndexedTable::Column& column)
      : solver_(solver),
        column_(column),
        tuples_per_value_(column.NumDifferentValues()),
        active_values_(column.NumDifferentValues()),
        index_in_active_values_(column.NumDifferentValues()),
        var_(var),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        reverse_tuples_(column.NumTuples()) {
    for (int value_index = 0;
         value_index < tuples_per_value_.size();
         value_index++) {
      tuples_per_value_[value_index] = new FastRevIntList<int>(
          column.NumTuplesContainingValueIndex(value_index));
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

  void RemoveOneTuple(int erased_tuple_index) {
    const int value_index = column_.ValueIndex(erased_tuple_index);
    FastRevIntList<int>* const var_value = tuples_per_value_[value_index];
    const int tuple_index_in_value = reverse_tuples_[erased_tuple_index];
    const int back_tuple_index = var_value->back();
    var_value->erase(
        solver_,
        tuple_index_in_value,
        erased_tuple_index,
        back_tuple_index,
        &reverse_tuples_[erased_tuple_index],
        &reverse_tuples_[back_tuple_index]);
    if (var_value->Size() == 0) {
      var_->RemoveValue(column_.ValueFromIndex(value_index));
      RemoveActiveValue(solver_, value_index);
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

  void RestoreTuple(int tuple_index) {
    FastRevIntList<int>* const active_tuples =
        tuples_per_value_[column_.ValueIndex(tuple_index)];
    const int index_of_value = reverse_tuples_[tuple_index];
    const int ebt = active_tuples->end_back();
    reverse_tuples_[ebt] = index_of_value;
    reverse_tuples_[tuple_index] = active_tuples->Size();
    active_tuples->push_back_from_index(
        solver_, index_of_value, tuple_index, ebt);
  }

  // Checks whether we should do reset or not.
  bool CheckResetProperty(const std::vector<int>& delta) {
    int num_deleted_tuples = 0;
    for (int k = 0; k < delta.size(); k++) {
      num_deleted_tuples += NumTuplesPerValue(delta[k]);
    }

    int num_remaining_tuples = 0;
    IntVarIterator* const it = DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = column_.IndexFromValue(it->Value());
      num_remaining_tuples += NumTuplesPerValue(value_index);
    }
    return (num_remaining_tuples < num_deleted_tuples);
  }

  void ComputeDeltaDomain(std::vector<int>* delta) {
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
      if (column_.IsValueValid(val)) {
        delta->push_back(column_.IndexFromValue(val));
      }
    }
    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = DeltaDomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      if (column_.IsValueValid(val)) {
        delta->push_back(column_.IndexFromValue(val));
      }
    }
    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var_->OldMax();
    const int64 maxdomain = var_->Max();
    for (int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      if (column_.IsValueValid(val)) {
        delta->push_back(column_.IndexFromValue(val));
      }
    }
  }

  void PropagateDeletedValues(const std::vector<int>& delta,
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

  void RemoveUnsupportedValues() {
    IntVarIterator* const it = DomainIterator();
    int num_removed = 0;
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = column_.IndexFromValue(it->Value());
      if (NumTuplesPerValue(value_index) == 0) {
        RemoveActiveValue(solver_, value_index);
        num_removed++;
      }
    }
    // Removed values have been pushed after the lists.
    const int last_valid_value = active_values_.Size();
    for (int cpt = 0; cpt < num_removed; cpt++) {
      const int value_index = ActiveValue(last_valid_value + cpt);
      const int64 removed_value =
          column_.ValueFromIndex(value_index);
      var_->RemoveValue(removed_value);
    }
  }

  void InitialPropagate(std::vector<int64>* const to_remove) {
    // Initialize data structures.
    const int num_tuples = column_.NumTuples();
    for (int tuple_index = 0; tuple_index < num_tuples; tuple_index++) {
      FastRevIntList<int>* const active_tuples =
          tuples_per_value_[column_.ValueIndex(tuple_index)];
      reverse_tuples_[tuple_index] = active_tuples->Size();
      active_tuples->push_back(solver_, tuple_index);
    }

    // we remove from the domain the values that are not in the table,
    // or that have no supporting tuples.
    to_remove->clear();
    IntVarIterator* const it = DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      if (!column_.IsValueValid(value) ||
          NumTuplesPerValue(column_.IndexFromValue(value)) == 0) {
        to_remove->push_back(value);
      }
    }
    var_->RemoveValues(*to_remove);
  }

  void CollectActiveTuples(std::vector<int>* const tuples_to_keep) const {
    tuples_to_keep->clear();
    for (domain_iterator_->Init();
         domain_iterator_->Ok();
         domain_iterator_->Next()) {
      const int value_index = column_.IndexFromValue(domain_iterator_->Value());
      FastRevIntList<int>* const active_tuples = tuples_per_value_[value_index];
      const int num_tuples = active_tuples->Size();
      for (int j = 0; j < num_tuples; j++) {
        tuples_to_keep->push_back((*active_tuples)[j]);
      }
    }
  }

 private:
  Solver* const solver_;
  const IndexedTable::Column& column_;
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
          new TableVar(solver, vars[var_index], table->column(var_index));
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
                               &Ac4TableConstraint::FilterOneVariable,
                               "FilterOneVariable",
                               var_index);
      vars_[var_index]->Variable()->WhenDomain(demon);
    }
  }

  void InitialPropagate() {
    std::vector<int64> to_remove;
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->InitialPropagate(&to_remove);
    }
  }

  void FilterOneVariable(int var_index) {
    TableVar* const var = vars_[var_index];
    var->ComputeDeltaDomain(&delta_of_value_indices_);
    if (var->CheckResetProperty(delta_of_value_indices_)) {
      Reset(var_index);
    }
    var->PropagateDeletedValues(delta_of_value_indices_, &tmp_tuples_);
    for (int i = 0; i < tmp_tuples_.size(); ++i) {
      RemoveOneTupleFromAllVariables(tmp_tuples_[i]);
    }
  }

 private:
  void RemoveOneTupleFromAllVariables(int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      TableVar* const var = vars_[var_index];
      var->RemoveOneTuple(tuple_index);
    }
  }

  // We scan values to check the ones without the supported values.
  void RemoveUnsupportedValuesOnAllVariables() {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->RemoveUnsupportedValues();
    }
  }

  // Clean and re-add all active tuples.
  void Reset(int var_index) {
    vars_[var_index]->CollectActiveTuples(&tmp_tuples_);

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
        vars_[var_index]->RestoreTuple(tuple_index);
      }
    }

    RemoveUnsupportedValuesOnAllVariables();
  }

  // Variables of the constraint.
  std::vector<TableVar*> vars_;
  // Table.
  IndexedTable* const table_;
  // Temporary tuple array for delayed add or delete operations.
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
