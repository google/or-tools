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
class RTableCt;

class RIndexedTable {
 public:
  RIndexedTable(const IntTupleSet& table)
  : tuples_(table.NumTuples() * table.Arity()),
    maps_(table.Arity()),
    num_tuples_per_value_(table.Arity()),
    arity_(table.Arity()),
    num_tuples_(table.NumTuples()) {
    for(int i = 0;i < arity_; i++) {
      num_tuples_per_value_[i].resize(table.NumDifferentValuesInColumn(i), 0);
      for(int t = 0;t < table.NumTuples(); t++) {
        int64 val = table.Value(t, i);
        if (!maps_[i].Contains(val)) {
          maps_[i].Add(val);
        }
        const int index = index_from_value(i, val);
        tuples_[t * arity_ + i] = index;
        num_tuples_per_value_[i][index]++;
      }
    }
  }

  ~RIndexedTable() {}

  int num_vars() const { return arity_; }

  int tuple_value(int t, int i) const {
    return tuples_[t * arity_ + i];
  }

  int index_from_value(int x, int64 val) const {
    return maps_[x].Index(val);
  }

  int64 value_from_index(int x, int v) const {
    return maps_[x].Element(v);
  }

  bool in(int x, int v) const {
    return maps_[x].Contains(v);
  }

  int num_tuples_of_value(int x, int v) const {
    return num_tuples_per_value_[x][v];
  }

  int num_tuples() const {
    return num_tuples_;
  } // TODO A verifier que l'allocation avec le ctor de n elt est bien
    // une allocation avec n elements
  int NumDifferentValuesInColumn(int i) const {
    return num_tuples_per_value_[i].size();
  }

 private:
  std::vector<int> tuples_;
  std::vector<VectorMap<int64> > maps_;
  // number of tuples per value
  std::vector<std::vector<int> > num_tuples_per_value_;
  const int arity_;
  const int num_tuples_;
};

class RListAsArray {
 public:
  RListAsArray(const int n) : elts_(n), size_(0) {}

  ~RListAsArray() {}

  int size() const { return size_; }

  int capacity() const { return elts_.capacity(); }

  int operator[](int i) const {
    // si l'array est const alors on ne peut pas le modifier // const T&
    assert(i < capacity());
    return elts_[i];
  }

  void push_back(int elt) {
    elts_[size_++] = elt;
  }

  void push_back(int elt, int& pos){
    pos=size_;
    elts_[size_++] = elt;
  }
  void push_back_from_index(int i) {
    // place l'elt qui est à l'index i en dernier et met à sa place
    // l'élément qui était dernier
    const int elt = elts_[i];
    elts_[i] = elts_[size_];
    elts_[size_] = elt;
    size_++;
  }

  void push_back_from_index(int i, int iElt, int endBackElt) {
    elts_[i] = endBackElt;
    elts_[size_] = iElt;
    size_++;
  }

  int end_back() const { return elts_[size_]; }

  int back() const { return elts_[size_ - 1]; }

  void erase(int i) {
    size_--;
    const int elt = elts_[i];
    elts_[i] = elts_[size_];
    elts_[size_] = elt;
  }

  void erase(int i, int iElt, int backElt){
    size_--;
    elts_[size_] = iElt;
    elts_[i] = backElt;
  }

  void erase(int i, int iElt, int backElt, int& posElt, int& posBack){
    size_--;
    elts_[size_] = iElt;
    elts_[i] = backElt;
    posElt = size_;
    posBack = i;
  }

  void clear(){size_=0;}

 private:
  friend class RTableCt;
  std::vector<int> elts_; // set of elts
  int size_; // number of elts in the set
};

int XXXEraseTuple=0;
int XXXPushTuple=0;

class RTableCt : public Constraint {
  struct Var {
    std::vector<RListAsArray*> values_; // one LAA per value of the variable
    std::vector<int64> stamps_; // on fait un tableau de stamp: un par valeur
    RListAsArray nonEmptyTupleLists_; // list of values: having a non empty tuple list
    std::vector<int> indexInNonEmptyTupleLists_;
    IntVar* var_;
    IntVarIterator* domain_iterator_;
    IntVarIterator* delta_domain_iterator_;
    int64 stampNonEmptyTupleLists_;

    Var(IntVar* var, const int x, RIndexedTable* table):
        values_(table->NumDifferentValuesInColumn(x)),
        stamps_(table->NumDifferentValuesInColumn(x),0),
        nonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
        indexInNonEmptyTupleLists_(table->NumDifferentValuesInColumn(x)),
        var_(var),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        stampNonEmptyTupleLists_(0){
      const int numValues=table->NumDifferentValuesInColumn(x);
      for(int v=0;v<numValues;v++){
        values_[v]=new RListAsArray(table->num_tuples_of_value(x,v));
        nonEmptyTupleLists_.push_back(v,indexInNonEmptyTupleLists_[v]);
      }
    }
    ~Var(){
      STLDeleteElements(&values_); // delete all elements of a vector
    }
    IntVar* Variable()const{return var_;}
    IntVarIterator* DomainIterator()const{return domain_iterator_;}
    IntVarIterator* DeltaDomainIterator()const{return delta_domain_iterator_;}
    void RemoveFromNonEmptyTupleList(Solver* solver, int v){
      if (stampNonEmptyTupleLists_ < solver->stamp()){
        solver->SaveValue(&nonEmptyTupleLists_.size_);
        stampNonEmptyTupleLists_=solver->stamp();
      }
      const int bv=nonEmptyTupleLists_.back();
      nonEmptyTupleLists_.erase(indexInNonEmptyTupleLists_[v],v,bv,indexInNonEmptyTupleLists_[v],indexInNonEmptyTupleLists_[bv]);
    }
    void save_size_once(Solver* solver, int v){
      if (stamps_[v] < solver->stamp()){
        solver->SaveValue(&values_[v]->size_);
        stamps_[v]=solver->stamp();
      }
    }
  };
  std::vector<Var*> vars_; // variable of the constraint
  std::vector<int> tupleIndexInValueList_;
  RIndexedTable* table_; // table
  std::vector<int> tmp_; // On peut le supprimer si on a un tableau temporaire d'entier qui est disponible. Peut contenir tous les tuples
  std::vector<int> delta_; // delta of the variable
  const int n_; // number of variables
  int& tuple_index_in_value_list(int t, int x){return tupleIndexInValueList_[t*n_+x];}
  void erase_tuple(const int t){
    XXXEraseTuple++;
    for(int i=0;i<n_;i++){ // the tuple is erased for each value it contains
      const int v=table_->tuple_value(t,i);
      RListAsArray* const val=vars_[i]->values_[table_->tuple_value(t,i)];
      const int sizeiv=val->size()-1;
      //			cout << "i: " << i << " v: " << v << " t:" << t <<endl;
      const int index=tuple_index_in_value_list(t,i);
      const int bt=val->back();
      vars_[i]->save_size_once(solver(),v);
      val->erase(index,t,bt,tuple_index_in_value_list(t,i),tuple_index_in_value_list(bt,i));
      if(sizeiv==0){
        vars_[i]->Variable()->RemoveValue(table_->value_from_index(i,v));
        vars_[i]->RemoveFromNonEmptyTupleList(solver(),v);
      }
    }
  }
  void erase_values_without_valid_tuple(){
    for(int i=0;i<n_;i++){ // on clear les tuples des valeurs du domaine
      IntVarIterator* const it = vars_[i]->DomainIterator();
      int numSupp=0;
      for(it->Init(); it->Ok(); it->Next()) {
        const int val = table_->index_from_value(i,it->Value());
        if (vars_[i]->values_[val]->size()==0){
          vars_[i]->RemoveFromNonEmptyTupleList(solver(),val);
          numSupp++;
        }
      }
      // on supprime de var les valeurs ayant un tuple list vide
      const int s=vars_[i]->nonEmptyTupleLists_.size();
      for(int cpt=0;cpt<numSupp;cpt++){
        const int v=vars_[i]->nonEmptyTupleLists_[s+cpt];
        vars_[i]->Variable()->RemoveValue(table_->value_from_index(i,v));
      }
    }
  }
  void filter_from_value_deletion(const int x,const int a){
    RListAsArray* val=vars_[x]->values_[a];
    const int size=val->size();
    for(int k=0;k<size;k++){
      erase_tuple((*val)[0]);
    }
  }
  void push_back_tuple_from_index(const int t){
    XXXPushTuple++;
    for(int i=0;i<n_;i++){
      RListAsArray* const val=vars_[i]->values_[table_->tuple_value(t,i)];
      const int indexForValue=tuple_index_in_value_list(t,i);
      const int ebt=val->end_back();
      tuple_index_in_value_list(ebt,i)=indexForValue;
      tuple_index_in_value_list(t,i)=val->size();
      val->push_back_from_index(indexForValue,t,ebt);
    }
  }
  void push_back_tuple(const int t){
    XXXPushTuple++;
    for(int i=0;i<n_;i++){
      RListAsArray* const val=vars_[i]->values_[table_->tuple_value(t,i)];
      tuple_index_in_value_list(t,i)=val->size();
      val->push_back(t);
    }
  }
  void reset(int x){// le reset est fait a partir de la variable selectedX
    int s=0;
    tmp_.clear();
    IntVarIterator* const it = vars_[x]->DomainIterator();
    for(it->Init(); it->Ok(); it->Next()) {
      const int v = table_->index_from_value(x,it->Value());
      RListAsArray* val=vars_[x]->values_[v];
      const int numTuples=val->size();
      for(int j=0;j<numTuples;j++){
        tmp_.push_back((*val)[j]);
      }
    }
    // on effectue un clear sur TOUTES les valeurs de la liste nonEmptyTupleList
    // On sauvegarde size pour chacune de ces valeurs avant de les mettre à 0

    for(int i=0;i<n_;i++){ // on clear les tuples des valeurs du domaine
      for(int k=0;k<vars_[i]->nonEmptyTupleLists_.size();k++){
        const int v=vars_[i]->nonEmptyTupleLists_[k];
        vars_[i]->save_size_once(solver(),v);
        vars_[i]->values_[v]->clear();
      }
    }
    // on balaie tupleTMP et on remet les tuples dans les domaines
    const int size=tmp_.size();
    for(int j=0;j<size;j++){
      push_back_tuple_from_index(tmp_[j]);
    }
    // Si une valeur n'a plus de support on la supprime
    erase_values_without_valid_tuple();
  }
  void compute_delta_domain(int x){// calcul du delta domain de or-tools: on remplit le tableau delta_ ATTENTION une val peut etre plusieurs fois dans le delta : ici on s'en fout.
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
    for(int64 val = oldmindomain; val < mindomain; ++val) {
      if (table_->in(x,val)){
        delta_.push_back(table_->index_from_value(x,val));
      }
    }
    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = vars_[x]->DeltaDomainIterator();
    for(it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      if (table_->in(x,val)){
        delta_.push_back(table_->index_from_value(x,val));
      }
    }
    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var->OldMax();
    const int64 maxdomain = var->Max();
    for(int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      if (table_->in(x,val)){
        delta_.push_back(table_->index_from_value(x,val));
      }
    }
  }
  bool check_reset_property(int x){// retourne true si on doit faire un reset
    // on compte le nb de tuples qui vont etre supprimés
    int numDelTuples=0;
    for(int k=0;k<delta_.size();k++){
      numDelTuples+=vars_[x]->values_[delta_[k]]->size();
    }
    // on calcule le nombre de tuples en balayant les valeurs du domaine de la var x
    int numTuplesInDomain=0;
    IntVarIterator* const it = vars_[x]->DomainIterator();
    for(it->Init(); it->Ok(); it->Next()) {
      const int v = table_->index_from_value(x,it->Value());
      numTuplesInDomain+=vars_[x]->values_[v]->size();
    }
    return (numTuplesInDomain < numDelTuples);
  }
  void initialize_data_structures(){
    const int numT=table_->num_tuples();
    for(int t=0;t<numT;t++){		// on place les tuples et on met à jour les informations
      push_back_tuple(t);
    }
  }
  void initial_filter(){
    initialize_data_structures();
    // we remove from the domain the values that are not in the table
    for(int i=0;i<n_;i++){
      delta_.clear(); // we use the delta as a temporary array
      IntVarIterator* const it = vars_[i]->DomainIterator();
      for(it->Init(); it->Ok(); it->Next()){
        if (!table_->in(i,it->Value())){
          delta_.push_back(it->Value());
        }
      }
      for(int k=0;k<delta_.size();k++){
        vars_[i]->Variable()->RemoveValue(delta_[k]);
      }
    }
    erase_values_without_valid_tuple();
  }
 public:
  RTableCt(Solver* const solver,RIndexedTable* table,const std::vector<IntVar*>& vars):Constraint(solver),
                                                                                               vars_(table->num_vars()),tupleIndexInValueList_(table->num_tuples()*table->num_vars()),table_(table),tmp_(table->num_tuples()),delta_(table->num_tuples()),n_(table->num_vars()){
    for(int i=0;i<table->num_vars();i++){
      vars_[i]=new Var(vars[i],i,table);
    }
  }
  ~RTableCt(){
    STLDeleteElements(&vars_); // delete all elements of a vector
  }
  void Post(){
    for(int i = 0; i < n_; i++) {
      Demon* const d = MakeConstraintDemon1(solver(),this,&RTableCt::FilterX,"FilterX",i);
      vars_[i]->Variable()->WhenDomain(d);
    }
  }
  void InitialPropagate(){
    initial_filter();
  }
  void FilterX(int x){
    compute_delta_domain(x);
    if (check_reset_property(x)){
      reset(x);
    }
    const int deltaSize=delta_.size();
    for(int k=0;k<deltaSize;k++){
      filter_from_value_deletion(x,delta_[k]);
    }
  }
  void print_tuple(const int t){
    std::cout << "pos in values of " << t << " ";
    for(int i =0;i<n_;i++){
      std::cout << tuple_index_in_value_list(t,i) << " ";
    }
    std::cout << std::endl;
  }
  void print_all_tuple(){
    for(int i=0;i<tupleIndexInValueList_.capacity();i++){
      print_tuple(i);
    }
  }
};



}  // namespace

// External API.
Constraint* BuildRTableCt(Solver* const solver,
                              IntTupleSet& tuples,
                              const std::vector<IntVar*>& vars,
                              int size_bucket) {
  const int num_tuples = tuples.NumTuples();
  const int arity = vars.size();
  RIndexedTable* const table = new RIndexedTable(tuples);
  return solver->RevAlloc(new RTableCt(solver, table, vars));
}
} // namespace operations_research
