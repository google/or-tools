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

  int TupleValue(int tuple_index, int var_index) const {
    return tuples_of_indices_[tuple_index * arity_ + var_index];
  }

  int IndexFromValue(int var_index, int64 value) const {
    return value_map_per_variable_[var_index].Index(value);
  }

  int64 ValueFromIndex(int var_index, int value_index) const {
    return value_map_per_variable_[var_index].Element(value_index);
  }

  bool TupleContainsValueFromIndex(int var_index, int value_index) const {
    return value_map_per_variable_[var_index].Contains(value_index);
  }

  int NumTuplesContainingValue(int var_index, int value_index) const {
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

class ListAsArray {
 public:
  ListAsArray(const int capacity)
  : elements_(new int[capacity]),
    num_elements_(0),
    capacity_(capacity) {}

  ~ListAsArray() {}

  int NumElements() const { return num_elements_; }

  int Capacity() const { return capacity_; }

  int operator[](int i) const {
    // si l'array est const alors on ne peut pas le modifier // const T&
    DCHECK_LT(i, capacity_);
    return elements_[i];
  }

  int push_back(int elt) {
    DCHECK_LT(num_elements_, capacity_);
    elements_[num_elements_++] = elt;
    return num_elements_ - 1;
  }

  void push_back_from_index(int i, int iElt, int endBackElt) {
    elements_[i] = endBackElt;
    elements_[num_elements_] = iElt;
    num_elements_++;
  }

  int end_back() const { return elements_[num_elements_]; }

  int back() const { return elements_[num_elements_ - 1]; }

  void erase(int i, int iElt, int backElt, int* posElt, int* posBack) {
    num_elements_--;
    elements_[num_elements_] = iElt;
    elements_[i] = backElt;
    *posElt = num_elements_;
    *posBack = i;
  }

  void clear() {
    num_elements_=0;
  }

 private:
  friend class Ac4TableConstraint;
  scoped_array<int> elements_; // set of elements.
  int num_elements_; // number of elements in the set.
  const int capacity_;
};

class Ac4TableConstraint : public Constraint {
 public:
  Ac4TableConstraint(Solver* const solver,
                     IndexedTable* const table,
                     const std::vector<IntVar*>& vars)
      : Constraint(solver),
        vars_(table->NumVars()),
        tuple_index_in_value_list_(table->NumTuples() * table->NumVars()),
        table_(table),
        tmp_(table->NumTuples()),
        delta_(table->NumTuples()),
        num_variables_(table->NumVars()) {
    for (int var_index = 0; var_index < table->NumVars(); var_index++) {
      vars_[var_index] = new Var(vars[var_index], var_index, table);
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
    Init();
    // we remove from the domain the values that are not in the table
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      delta_.clear(); // we use the delta as a temporary array
      IntVarIterator* const it = vars_[var_index]->DomainIterator();
      for (it->Init(); it->Ok(); it->Next()) {
        if (!table_->TupleContainsValueFromIndex(var_index, it->Value())) {
          delta_.push_back(it->Value());
        }
      }
      for (int delta_index = 0; delta_index < delta_.size(); delta_index++) {
        vars_[var_index]->Variable()->RemoveValue(delta_[delta_index]);
      }
    }
    EraseValuesWithoutValidTuples();
  }

  void FilterX(int x) {
    ComputeDeltaDomain(x);
    if (CheckResetProperty(x)) {
      Reset(x);
    }
    const int deltaSize = delta_.size();
    for (int k = 0;k < deltaSize; k++) {
      FilterFromValueDeletion(x, delta_[k]);
    }
  }

 private:
  class Var {
   public:
    Var(IntVar* var, const int var_index, IndexedTable* const table)
        : values_(table->NumDifferentValuesInColumn(var_index)),
          stamps_(values_.size(), 0),
          non_empty_tuples_(values_.size()),
          index_in_non_empty_tuples_(values_.size()),
          var_(var),
          domain_iterator_(var->MakeDomainIterator(true)),
          delta_domain_iterator_(var->MakeHoleIterator(true)),
          stamp_non_empty_tuples_(0) {
      for (int value_index = 0; value_index < values_.size(); value_index++) {
        values_[value_index] =
            new ListAsArray(table->NumTuplesContainingValue(var_index,
                                                            value_index));
        index_in_non_empty_tuples_[value_index] =
            non_empty_tuples_.push_back(value_index);
      }
    }

    ~Var() {
      STLDeleteElements(&values_); // delete all elements of a vector
    }

    IntVar* Variable() const { return var_; }

    IntVarIterator* DomainIterator() const { return domain_iterator_; }

    IntVarIterator* DeltaDomainIterator() const {
      return delta_domain_iterator_;
    }

    void RemoveFromNonEmptyTupleList(Solver* solver, int value_index) {
      if (stamp_non_empty_tuples_ < solver->stamp()) {
        solver->SaveValue(&non_empty_tuples_.num_elements_);
        stamp_non_empty_tuples_ = solver->stamp();
      }
      const int back_value_index = non_empty_tuples_.back();
      non_empty_tuples_.erase(
          index_in_non_empty_tuples_[value_index],
          value_index,
          back_value_index,
          &index_in_non_empty_tuples_[value_index],
          &index_in_non_empty_tuples_[back_value_index]);
    }

    void SaveSizeOnce(Solver* solver, int v) {
      if (stamps_[v] < solver->stamp()) {
        solver->SaveValue(&values_[v]->num_elements_);
        stamps_[v]=solver->stamp();
      }
    }

   private:
    friend class Ac4TableConstraint;
    // one LAA per value of the variable
    std::vector<ListAsArray*> values_;
    std::vector<uint64> stamps_;
    // list of values: having a non empty tuple list
    ListAsArray non_empty_tuples_;
    std::vector<int> index_in_non_empty_tuples_;
    IntVar* const var_;
    IntVarIterator* const domain_iterator_;
    IntVarIterator* const delta_domain_iterator_;
    uint64 stamp_non_empty_tuples_;
  };


  int TupleIndexInValueList(int tuple_index, int var_index) {
    return tuple_index_in_value_list_[tuple_index * num_variables_ + var_index];
  }


  void SetTupleIndexInValueList(int tuple_index, int var_index, int v) {
    tuple_index_in_value_list_[tuple_index * num_variables_ + var_index] = v;
  }

  int* TupleIndexInValueListAddress(int tuple_index, int var_index) {
    return &tuple_index_in_value_list_[tuple_index * num_variables_ +
                                       var_index];
  }

  void EraseTuple(const int erased_tuple_index) {
    // The tuple is erased for each value it contains.
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      const int value_index = table_->TupleValue(erased_tuple_index, var_index);
      ListAsArray* const var_value = vars_[var_index]->values_[value_index];
      const int sizeiv = var_value->NumElements() - 1;
      const int tuple_index_in_value =
          TupleIndexInValueList(erased_tuple_index, var_index);
      const int back_tuple_index = var_value->back();
      vars_[var_index]->SaveSizeOnce(solver(), value_index);
      var_value->erase(
          tuple_index_in_value,
          erased_tuple_index,
          back_tuple_index,
          TupleIndexInValueListAddress(erased_tuple_index, var_index),
          TupleIndexInValueListAddress(back_tuple_index, var_index));

      if (sizeiv == 0) {
        vars_[var_index]->Variable()->RemoveValue(
            table_->ValueFromIndex(var_index, value_index));
        vars_[var_index]->RemoveFromNonEmptyTupleList(solver(), value_index);
      }
    }
  }

  void EraseValuesWithoutValidTuples() {
    // on clear les tuples des valeurs du domaine
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      IntVarIterator* const it = vars_[var_index]->DomainIterator();
      int num_removed = 0;
      for (it->Init(); it->Ok(); it->Next()) {
        const int value_index = table_->IndexFromValue(var_index, it->Value());
        if (vars_[var_index]->values_[value_index]->NumElements() == 0) {
          vars_[var_index]->RemoveFromNonEmptyTupleList(solver(), value_index);
          num_removed++;
        }
      }
      // on supprime de var les valeurs ayant un tuple list vide
      const int s = vars_[var_index]->non_empty_tuples_.NumElements();
      for (int cpt = 0; cpt < num_removed; cpt++) {
        const int value = vars_[var_index]->non_empty_tuples_[s + cpt];
        vars_[var_index]->Variable()->RemoveValue(
            table_->ValueFromIndex(var_index, value));
      }
    }
  }

  void FilterFromValueDeletion(const int var_index, const int value_index) {
    ListAsArray* const var_value = vars_[var_index]->values_[value_index];
    const int size = var_value->NumElements();
    for (int index = 0; index < size; index++) {
      EraseTuple((*var_value)[0]);
    }
  }

  void PushBackTupleFromIndex(const int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      ListAsArray* const val =
          vars_[var_index]->values_[table_->TupleValue(tuple_index, var_index)];
      const int index_of_value =
          TupleIndexInValueList(tuple_index, var_index);
      const int ebt = val->end_back();
      SetTupleIndexInValueList(ebt, var_index, index_of_value);
      SetTupleIndexInValueList(tuple_index, var_index, val->NumElements());
      val->push_back_from_index(index_of_value, tuple_index, ebt);
    }
  }

  void PushBackTuple(const int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      ListAsArray* const var_value =
          vars_[var_index]->values_[table_->TupleValue(tuple_index, var_index)];
      SetTupleIndexInValueList(tuple_index,
                               var_index,
                               var_value->NumElements());
      var_value->push_back(tuple_index);
    }
  }

  // le reset est fait a partir de la variable selectedX.
  void Reset(int var_index) {
    int s = 0;
    tmp_.clear();
    IntVarIterator* const it = vars_[var_index]->DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int v = table_->IndexFromValue(var_index, it->Value());
      ListAsArray* const val = vars_[var_index]->values_[v];
      const int num_tuples = val->NumElements();
      for (int j = 0; j < num_tuples; j++) {
        tmp_.push_back((*val)[j]);
      }
    }
    // on effectue un clear sur TOUTES les valeurs de la liste nonEmptyTupleList
    // On sauvegarde size pour chacune de ces valeurs avant de les mettre à 0

    // on clear les tuples des valeurs du domaine
    for (int i = 0; i < num_variables_; i++) {
      for (int k = 0; k < vars_[i]->non_empty_tuples_.NumElements(); k++) {
        const int v = vars_[i]->non_empty_tuples_[k];
        vars_[i]->SaveSizeOnce(solver(), v);
        vars_[i]->values_[v]->clear();
      }
    }

    // on balaie tupleTMP et on remet les tuples dans les domaines
    const int size = tmp_.size();
    for (int j = 0; j < size; j++) {
      PushBackTupleFromIndex(tmp_[j]);
    }
    // Si une valeur n'a plus de support on la supprime
    EraseValuesWithoutValidTuples();
  }

  void ComputeDeltaDomain(int var_index) {
    // calcul du delta domain de or-tools: on remplit le tableau
    // delta_ ATTENTION une val peut etre plusieurs fois dans le delta
    // : ici on s'en fout.
    IntVar* const var = vars_[var_index]->Variable();
    delta_.clear();
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
    const int64 oldmindomain = var->OldMin();
    const int64 mindomain = var->Min();
    for (int64 val = oldmindomain; val < mindomain; ++val) {
      if (table_->TupleContainsValueFromIndex(var_index,val)) {
        delta_.push_back(table_->IndexFromValue(var_index, val));
      }
    }
    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = vars_[var_index]->DeltaDomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      if (table_->TupleContainsValueFromIndex(var_index,val)) {
        delta_.push_back(table_->IndexFromValue(var_index, val));
      }
    }
    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var->OldMax();
    const int64 maxdomain = var->Max();
    for (int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      if (table_->TupleContainsValueFromIndex(var_index,val)) {
        delta_.push_back(table_->IndexFromValue(var_index, val));
      }
    }
  }

  bool CheckResetProperty(int var_index) {
    // retourne true si on doit faire un reset. on compte le nb de
    // tuples qui vont etre supprimés.
    int num_deleted_tuples = 0;
    for (int k = 0; k < delta_.size(); k++) {
      num_deleted_tuples += vars_[var_index]->values_[delta_[k]]->NumElements();
    }
    // on calcule le nombre de tuples en balayant les valeurs du
    // domaine de la var x
    int num_tuples_in_domain = 0;
    IntVarIterator* const it = vars_[var_index]->DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = table_->IndexFromValue(var_index, it->Value());
      num_tuples_in_domain +=
          vars_[var_index]->values_[value_index]->NumElements();
    }
    return (num_tuples_in_domain < num_deleted_tuples);
  }

  void Init() {
    const int num_tuples = table_->NumTuples();
    for (int tuple_index = 0; tuple_index < num_tuples; tuple_index++) {
      // on place les tuples et on met à jour les informations
      PushBackTuple(tuple_index);
    }
  }

  std::vector<Var*> vars_; // variable of the constraint
  std::vector<int> tuple_index_in_value_list_;
  IndexedTable* const table_; // table
  // On peut le supprimer si on a un tableau temporaire d'entier qui
  // est disponible. Peut contenir tous les tuples
  std::vector<int> tmp_;
  // Temporary storage for delta of one variable.
  std::vector<int64> delta_;
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
