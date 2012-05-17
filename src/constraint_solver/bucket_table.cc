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

  int TupleValue(int t, int i) const {
    return tuples_of_indices_[t * arity_ + i];
  }

  int IndexFromValue(int x, int64 val) const {
    return value_map_per_variable_[x].Index(val);
  }

  int64 ValueFromIndex(int x, int v) const {
    return value_map_per_variable_[x].Element(v);
  }

  bool TupleContainsValueFromIndex(int x, int v) const {
    return value_map_per_variable_[x].Contains(v);
  }

  int NumTuplesContainingValue(int x, int v) const {
    return num_tuples_per_value_[x][v];
  }

  int NumTuples() const {
    return num_tuples_;
  }

  int NumDifferentValuesInColumn(int i) const {
    return num_tuples_per_value_[i].size();
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

  void erase(int i, int iElt, int backElt, int& posElt, int& posBack) {
    num_elements_--;
    elements_[num_elements_] = iElt;
    elements_[i] = backElt;
    posElt = num_elements_;
    posBack = i;
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
    for (int i = 0; i < table->NumVars(); i++) {
      vars_[i] = new Var(vars[i], i, table);
    }
  }

  ~Ac4TableConstraint() {
    STLDeleteElements(&vars_); // delete all elements of a vector
    delete table_;
  }

  void Post() {
    for (int i = 0; i < num_variables_; i++) {
      Demon* const demon =
          MakeConstraintDemon1(solver(),
                               this,
                               &Ac4TableConstraint::FilterX,
                               "FilterX",
                               i);
      vars_[i]->Variable()->WhenDomain(demon);
    }
  }

  void InitialPropagate() {
    initialize_data_structures();
    // we remove from the domain the values that are not in the table
    for (int i = 0; i <num_variables_; i++) {
      delta_.clear(); // we use the delta as a temporary array
      IntVarIterator* const it = vars_[i]->DomainIterator();
      for (it->Init(); it->Ok(); it->Next()) {
        if (!table_->TupleContainsValueFromIndex(i, it->Value())) {
          delta_.push_back(it->Value());
        }
      }
      for (int k = 0; k < delta_.size(); k++) {
        vars_[i]->Variable()->RemoveValue(delta_[k]);
      }
    }
    erase_values_without_valid_tuple();
  }

  void FilterX(int x) {
    compute_delta_domain(x);
    if (check_reset_property(x)) {
      reset(x);
    }
    const int deltaSize = delta_.size();
    for (int k = 0;k < deltaSize; k++) {
      filter_from_value_deletion(x, delta_[k]);
    }
  }

  void print_tuple(const int t) {
    std::cout << "pos in values of " << t << " ";
    for (int i = 0; i < num_variables_; i++) {
      std::cout << tuple_index_in_value_list(t, i) << " ";
    }
    std::cout << std::endl;
  }

  void print_all_tuple() {
    for (int i = 0; i < tuple_index_in_value_list_.size(); i++) {
      print_tuple(i);
    }
  }

 private:
  class Var {
   public:
    Var(IntVar* var, const int x, IndexedTable* const table)
        : values_(table->NumDifferentValuesInColumn(x)),
          stamps_(table->NumDifferentValuesInColumn(x),0),
          nonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
          indexInNonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
          var_(var),
          domain_iterator_(var->MakeDomainIterator(true)),
          delta_domain_iterator_(var->MakeHoleIterator(true)),
          stampNonEmptyTupleLists_(0) {
      const int numValues = table->NumDifferentValuesInColumn(x);
      for (int v = 0; v < numValues; v++) {
        values_[v] = new ListAsArray(table->NumTuplesContainingValue(x,v));
        indexInNonEmptyTupleLists_[v] = nonEmptyTupleLists_.push_back(v);
      }
    }

    ~Var() {
      STLDeleteElements(&values_); // delete all elements of a vector
    }

    IntVar* Variable()const{return var_;}

    IntVarIterator* DomainIterator()const{return domain_iterator_;}

    IntVarIterator* DeltaDomainIterator()const{return delta_domain_iterator_;}

    void RemoveFromNonEmptyTupleList(Solver* solver, int v) {
      if (stampNonEmptyTupleLists_ < solver->stamp()) {
        solver->SaveValue(&nonEmptyTupleLists_.num_elements_);
        stampNonEmptyTupleLists_=solver->stamp();
      }
      const int bv=nonEmptyTupleLists_.back();
      nonEmptyTupleLists_.erase(indexInNonEmptyTupleLists_[v],
                                v,
                                bv,
                                indexInNonEmptyTupleLists_[v],
                                indexInNonEmptyTupleLists_[bv]);
    }

    void save_size_once(Solver* solver, int v) {
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
    ListAsArray nonEmptyTupleLists_;
    std::vector<int> indexInNonEmptyTupleLists_;
    IntVar* const var_;
    IntVarIterator* const domain_iterator_;
    IntVarIterator* const delta_domain_iterator_;
    int64 stampNonEmptyTupleLists_;
  };


  int& tuple_index_in_value_list(int tuple_index, int var_index) {
    return tuple_index_in_value_list_[tuple_index * num_variables_ + var_index];
  }

  void erase_tuple(const int erased_tuple_index) {
    // The tuple is erased for each value it contains.
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      const int value_index =
          table_->TupleValue(erased_tuple_index, var_index);
      ListAsArray* const var_value =
          vars_[var_index]->values_[table_->TupleValue(erased_tuple_index,
                                                       var_index)];
      const int sizeiv = var_value->NumElements() - 1;
      const int tuple_index_in_value =
          tuple_index_in_value_list(erased_tuple_index, var_index);
      const int bt = var_value->back();
      vars_[var_index]->save_size_once(solver(), value_index);
      var_value->erase(tuple_index_in_value,
                       erased_tuple_index,
                       bt,
                       tuple_index_in_value_list(erased_tuple_index, var_index),
                       tuple_index_in_value_list(bt, var_index));

      if (sizeiv == 0) {
        vars_[var_index]->Variable()->RemoveValue(
            table_->ValueFromIndex(var_index, value_index));
        vars_[var_index]->RemoveFromNonEmptyTupleList(solver(), value_index);
      }
    }
  }

  void erase_values_without_valid_tuple() {
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
      const int s = vars_[var_index]->nonEmptyTupleLists_.NumElements();
      for (int cpt = 0; cpt < num_removed; cpt++) {
        const int value = vars_[var_index]->nonEmptyTupleLists_[s + cpt];
        vars_[var_index]->Variable()->RemoveValue(
            table_->ValueFromIndex(var_index, value));
      }
    }
  }

  void filter_from_value_deletion(const int var_index, const int value_index) {
    ListAsArray* const var_value = vars_[var_index]->values_[value_index];
    const int size = var_value->NumElements();
    for (int index = 0; index < size; index++) {
      erase_tuple((*var_value)[0]);
    }
  }

  void push_back_tuple_from_index(const int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      ListAsArray* const val =
          vars_[var_index]->values_[table_->TupleValue(tuple_index, var_index)];
      const int index_of_value =
          tuple_index_in_value_list(tuple_index, var_index);
      const int ebt = val->end_back();
      tuple_index_in_value_list(ebt, var_index) = index_of_value;
      tuple_index_in_value_list(tuple_index, var_index) = val->NumElements();
      val->push_back_from_index(index_of_value, tuple_index, ebt);
    }
  }

  void push_back_tuple(const int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      ListAsArray* const var_value =
          vars_[var_index]->values_[table_->TupleValue(tuple_index, var_index)];
      tuple_index_in_value_list(tuple_index, var_index) =
          var_value->NumElements();
      var_value->push_back(tuple_index);
    }
  }

  // le reset est fait a partir de la variable selectedX.
  void reset(int var_index) {
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
      for (int k = 0; k < vars_[i]->nonEmptyTupleLists_.NumElements(); k++) {
        const int v = vars_[i]->nonEmptyTupleLists_[k];
        vars_[i]->save_size_once(solver(), v);
        vars_[i]->values_[v]->clear();
      }
    }

    // on balaie tupleTMP et on remet les tuples dans les domaines
    const int size = tmp_.size();
    for (int j = 0; j < size; j++) {
      push_back_tuple_from_index(tmp_[j]);
    }
    // Si une valeur n'a plus de support on la supprime
    erase_values_without_valid_tuple();
  }

  void compute_delta_domain(int var_index) {
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

  bool check_reset_property(int var_index) {
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

  void initialize_data_structures() {
    const int num_tuples = table_->NumTuples();
    for (int t = 0; t < num_tuples; t++) {
      // on place les tuples et on met à jour les informations
      push_back_tuple(t);
    }
  }


  std::vector<Var*> vars_; // variable of the constraint
  std::vector<int> tuple_index_in_value_list_;
  IndexedTable* const table_; // table
  std::vector<int> tmp_; // On peut le supprimer si on a un tableau temporaire d'entier qui est disponible. Peut contenir tous les tuples
  std::vector<int64> delta_; // delta of the variable
  const int num_variables_; // number of variables
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
