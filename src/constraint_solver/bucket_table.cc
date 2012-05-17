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

  bool TupleContainsValue(int x, int v) const {
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

  void push_back(int elt) {
    DCHECK_LT(num_elements_, capacity_);
    elements_[num_elements_++] = elt;
  }

  void push_back(int elt, int& pos) {
    DCHECK_LT(num_elements_, capacity_);
    pos = num_elements_;
    elements_[num_elements_++] = elt;
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
  scoped_array<int> elements_; // set of elts
  int num_elements_; // number of elts in the set
  const int capacity_;
};

class Ac4TableConstraint : public Constraint {
  class Var {
   public:
    Var(IntVar* var, const int x, IndexedTable* table)
        : values_(table->NumDifferentValuesInColumn(x)),
          stamps_(table->NumDifferentValuesInColumn(x),0),
          nonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
          indexInNonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
          var_(var),
          domain_iterator_(var->MakeDomainIterator(true)),
          delta_domain_iterator_(var->MakeHoleIterator(true)),
          stampNonEmptyTupleLists_(0) {
      const int numValues = table->NumDifferentValuesInColumn(x);
      for (int v = 0; v< numValues; v++) {
        values_[v] = new ListAsArray(table->NumTuplesContainingValue(x,v));
        nonEmptyTupleLists_.push_back(v, indexInNonEmptyTupleLists_[v]);
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
    std::vector<ListAsArray*> values_; // one LAA per value of the variable
    std::vector<uint64> stamps_; // on fait un tableau de stamp: un par valeur
    ListAsArray nonEmptyTupleLists_; // list of values: having a non empty tuple list
    std::vector<int> indexInNonEmptyTupleLists_;
    IntVar* const var_;
    IntVarIterator* const domain_iterator_;
    IntVarIterator* const delta_domain_iterator_;
    int64 stampNonEmptyTupleLists_;
  };

  int& tuple_index_in_value_list(int t, int x) {
    return tupleIndexInValueList_[t * n_ + x];
  }

  void erase_tuple(const int t) {
    for (int i=0;i<n_;i++) { // the tuple is erased for each value it contains
      const int v=table_->TupleValue(t,i);
      ListAsArray* const val=vars_[i]->values_[table_->TupleValue(t,i)];
      const int sizeiv = val->NumElements() - 1;
      const int index = tuple_index_in_value_list(t,i);
      const int bt = val->back();
      vars_[i]->save_size_once(solver(),v);
      val->erase(index,
                 t,
                 bt,
                 tuple_index_in_value_list(t, i),
                 tuple_index_in_value_list(bt, i));

      if (sizeiv == 0) {
        vars_[i]->Variable()->RemoveValue(table_->ValueFromIndex(i, v));
        vars_[i]->RemoveFromNonEmptyTupleList(solver(), v);
      }
    }
  }

  void erase_values_without_valid_tuple() {
    for (int i = 0;i < n_; i++) { // on clear les tuples des valeurs du domaine
      IntVarIterator* const it = vars_[i]->DomainIterator();
      int numSupp = 0;
      for (it->Init(); it->Ok(); it->Next()) {
        const int val = table_->IndexFromValue(i,it->Value());
        if (vars_[i]->values_[val]->NumElements() == 0) {
          vars_[i]->RemoveFromNonEmptyTupleList(solver(),val);
          numSupp++;
        }
      }
      // on supprime de var les valeurs ayant un tuple list vide
      const int s = vars_[i]->nonEmptyTupleLists_.NumElements();
      for (int cpt = 0; cpt < numSupp; cpt++) {
        const int v = vars_[i]->nonEmptyTupleLists_[s + cpt];
        vars_[i]->Variable()->RemoveValue(table_->ValueFromIndex(i, v));
      }
    }
  }

  void filter_from_value_deletion(const int x, const int a) {
    ListAsArray* const val = vars_[x]->values_[a];
    const int size = val->NumElements();
    for (int k = 0; k < size; k++) {
      erase_tuple((*val)[0]);
    }
  }

  void push_back_tuple_from_index(const int t) {
    for (int i = 0; i < n_; i++) {
      ListAsArray* const val = vars_[i]->values_[table_->TupleValue(t, i)];
      const int indexForValue = tuple_index_in_value_list(t, i);
      const int ebt = val->end_back();
      tuple_index_in_value_list(ebt, i) = indexForValue;
      tuple_index_in_value_list(t, i) = val->NumElements();
      val->push_back_from_index(indexForValue, t, ebt);
    }
  }

  void push_back_tuple(const int t) {
    for (int i = 0;i < n_; i++) {
      ListAsArray* const val = vars_[i]->values_[table_->TupleValue(t, i)];
      tuple_index_in_value_list(t, i) = val->NumElements();
      val->push_back(t);
    }
  }

  void reset(int x) {  // le reset est fait a partir de la variable selectedX
    int s = 0;
    tmp_.clear();
    IntVarIterator* const it = vars_[x]->DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int v = table_->IndexFromValue(x, it->Value());
      ListAsArray* const val = vars_[x]->values_[v];
      const int numTuples = val->NumElements();
      for (int j = 0; j <numTuples; j++) {
        tmp_.push_back((*val)[j]);
      }
    }
    // on effectue un clear sur TOUTES les valeurs de la liste nonEmptyTupleList
    // On sauvegarde size pour chacune de ces valeurs avant de les mettre à 0

    for (int i = 0;i < n_; i++) { // on clear les tuples des valeurs du domaine
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

  void compute_delta_domain(int x) {
    // calcul du delta domain de or-tools: on remplit le tableau
    // delta_ ATTENTION une val peut etre plusieurs fois dans le delta
    // : ici on s'en fout.
    IntVar* const var = vars_[x]->Variable();
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
      if (table_->TupleContainsValue(x,val)) {
        delta_.push_back(table_->IndexFromValue(x, val));
      }
    }
    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = vars_[x]->DeltaDomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      if (table_->TupleContainsValue(x,val)) {
        delta_.push_back(table_->IndexFromValue(x, val));
      }
    }
    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var->OldMax();
    const int64 maxdomain = var->Max();
    for (int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      if (table_->TupleContainsValue(x,val)) {
        delta_.push_back(table_->IndexFromValue(x, val));
      }
    }
  }

  bool check_reset_property(int x) {  // retourne true si on doit
                                      // faire un reset. on compte le
                                      // nb de tuples qui vont etre
                                      // supprimés
    int numDelTuples=0;
    for (int k = 0; k < delta_.size(); k++) {
      numDelTuples += vars_[x]->values_[delta_[k]]->NumElements();
    }
    // on calcule le nombre de tuples en balayant les valeurs du
    // domaine de la var x
    int numTuplesInDomain = 0;
    IntVarIterator* const it = vars_[x]->DomainIterator();
    for (it->Init(); it->Ok(); it->Next()) {
      const int v = table_->IndexFromValue(x, it->Value());
      numTuplesInDomain += vars_[x]->values_[v]->NumElements();
    }
    return (numTuplesInDomain < numDelTuples);
  }

  void initialize_data_structures() {
    const int numT = table_->NumTuples();
    for (int t = 0; t < numT; t++) {
      // on place les tuples et on met à jour les informations
      push_back_tuple(t);
    }
  }

  void initial_filter() {
    initialize_data_structures();
    // we remove from the domain the values that are not in the table
    for (int i=0;i<n_;i++) {
      delta_.clear(); // we use the delta as a temporary array
      IntVarIterator* const it = vars_[i]->DomainIterator();
      for (it->Init(); it->Ok(); it->Next()) {
        if (!table_->TupleContainsValue(i,it->Value())) {
          delta_.push_back(it->Value());
        }
      }
      for (int k=0;k<delta_.size();k++) {
        vars_[i]->Variable()->RemoveValue(delta_[k]);
      }
    }
    erase_values_without_valid_tuple();
  }

 public:
  Ac4TableConstraint(Solver* const solver,
                     IndexedTable* const table,
                     const std::vector<IntVar*>& vars)
      : Constraint(solver),
        vars_(table->NumVars()),
        tupleIndexInValueList_(table->NumTuples() * table->NumVars()),
        table_(table),
        tmp_(table->NumTuples()),
        delta_(table->NumTuples()),
        n_(table->NumVars()) {
    for (int i = 0; i < table->NumVars(); i++) {
      vars_[i] = new Var(vars[i], i, table);
    }
  }

  ~Ac4TableConstraint() {
    STLDeleteElements(&vars_); // delete all elements of a vector
    delete table_;
  }

  void Post() {
    for (int i = 0; i < n_; i++) {
      Demon* const d =
          MakeConstraintDemon1(solver(),
                               this,
                               &Ac4TableConstraint::FilterX,
                               "FilterX",
                               i);
      vars_[i]->Variable()->WhenDomain(d);
    }
  }

  void InitialPropagate() {
    initial_filter();
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
    for (int i = 0; i < n_; i++) {
      std::cout << tuple_index_in_value_list(t, i) << " ";
    }
    std::cout << std::endl;
  }

  void print_all_tuple() {
    for (int i = 0; i < tupleIndexInValueList_.size(); i++) {
      print_tuple(i);
    }
  }

 private:
  std::vector<Var*> vars_; // variable of the constraint
  std::vector<int> tupleIndexInValueList_;
  IndexedTable* const table_; // table
  std::vector<int> tmp_; // On peut le supprimer si on a un tableau temporaire d'entier qui est disponible. Peut contenir tous les tuples
  std::vector<int> delta_; // delta of the variable
  const int n_; // number of variables
};
}  // namespace

// External API.
Constraint* BuildAc4TableConstraint(Solver* const solver,
                                    IntTupleSet& tuples,
                                    const std::vector<IntVar*>& vars,
                                    int size_bucket) {
  const int num_tuples = tuples.NumTuples();
  const int arity = vars.size();
  IndexedTable* const table = new IndexedTable(tuples);
  return solver->RevAlloc(new Ac4TableConstraint(solver, table, vars));
}
} // namespace operations_research
