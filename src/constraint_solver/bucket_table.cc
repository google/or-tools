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
#include "base/map-util.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {
// Interface: 1 tuple = vector of int64

/*
 * Bucketted Tuple Table
 */

#define TABLE_TUPLE_NIL kint32max
#define TABLE_BUCKET_NIL kint32max
#define TABLE_MAP_NIL kint32max

#define MAPINT32TOINDEX_DEFAULTVALUE kint32max

class MapInt64ToIndex {
  hash_map<int64, int> map_;
  std::vector<int64> data_;
 public:
  int64 value(int iv)const{return data_[iv];}
  int ivalue(int64 val)const{return FindWithDefault(map_,val,MAPINT32TOINDEX_DEFAULTVALUE);}
  int default_index()const{return MAPINT32TOINDEX_DEFAULTVALUE;}
  void push_back(int64 val){
    map_[val]=data_.size();
    data_.push_back(val);
  }
  bool in(int64 val){return ivalue(val) != default_index();}
};

class BtTable {
  class Domain {
   public:
    friend class BtTable;
    class Value { // value in table
      friend class BtTable;
      // returns the index of the first tuple containing the involved
      // value in the given bucket
      std::vector<int> firstTupleInBucket_;
      // returns the index of the first bucket following the given
      // bucket and containing a tuple involving the value.
      std::vector<int> nextBucket_;
      // ATTENTION if the bucket b contains such a tuple then nextBucket_[b] returns b
     public:
      Value(const int numBuckets)
      : firstTupleInBucket_(numBuckets, TABLE_TUPLE_NIL),
        nextBucket_(numBuckets, TABLE_BUCKET_NIL) {}

      // ib: bucket index:; it: tuple index
      void LinkBuckets(const int ib, const int it){
        if (firstTupleInBucket_[ib] == TABLE_TUPLE_NIL){
          // in this case the bucket ib does not contain any tuple involving the value

          // it is the first tuple in the bucket
          firstTupleInBucket_[ib]=it;
          // nextBucket of b returns b if there is a tuple of b containing the value
          nextBucket_[ib]=ib;
          // the previous nextBuckets are updated
          for(int b=ib-1;b >= 0;b--){
            if (nextBucket_[b] == TABLE_BUCKET_NIL){
              nextBucket_[b]=ib;
            } else break;
          }
        }
      }
    }; // end class Value

    MapInt64ToIndex map_;

    std::vector<Value*> values_;
    // temporary array containing for each value its last tuple index:
    // this improves the creation of next pointers
    std::vector<int> lastT_;
    // number of buckets
    const int numBuckets_;

    Domain(const int numBuckets) : numBuckets_(numBuckets) {}
    ~Domain();

    int size()const{return values_.size();}
    // internal
    void AddValue(const int64 val){
      if (!map_.in(val)){
        map_.push_back(val);
        Value* vobj=new Value(numBuckets_);
        values_.push_back(vobj);
        lastT_.push_back(TABLE_TUPLE_NIL);
      }
    }
    void LinkBuckets(const int iv, const int ib, const int it){values_[iv]->LinkBuckets(ib,it);}
  }; // end class Domain

  class Tuple {
    friend class BtTable;
    // indices of the values in the tuple
    std::vector<int> ivalues_;
    // for each indice (i.e. var) i, it returns the index of the next
    // tuple containing the value at the position i
    std::vector<int> inext_;
    Tuple(const int n)
    : ivalues_(n),
      inext_(n, TABLE_TUPLE_NIL) {}
  }; // end class Tuple

  std::vector<Tuple*> tuples_;
  // domain of variables WITHIN TUPLES
  std::vector<Domain*> doms_;
  // arity
  const int n_;
  // size of a bucket
  const int sb_;

 public:
  BtTable(const int n, int numTulpes, int size_bucket);
  ~BtTable();
  int bucket(const int t) const {
    return t / sb_;
  }

  int domain_size(const int i) const {
    return doms_[i]->size();
  }

  bool in_domain(const int i, const int64 val) const {
    return doms_[i]->map_.in(val);
  }

  int ivalue(const int i, const int64 val) const {
    return doms_[i]->map_.ivalue(val);
  }

  int64 value(const int i, const int ivt) const {
    return doms_[i]->map_.value(ivt);
  }

  int next_bucket(const int x, const int v, const int b) const {
    return doms_[x]->values_[v]->nextBucket_[b];
  }

  int first_ituple_in_bucket(const int x, const int v, const int b) const {
    return doms_[x]->values_[v]->firstTupleInBucket_[b];
  }

  int last_ituple_in_bucket(const int b) {
    return (b+1)*sb_ - 1;
  }

  int tuple_ivalue(const int t, const int x) const {
    return tuples_[t]->ivalues_[x];
  }

  int tuple_inext(const int t, const int x) const {
    return tuples_[t]->inext_[x];
  }

  int num_tuples() const {
    return tuples_.size();
  }

  int num_vars()const{
    return n_;
  }

  int num_buckets() const {
    return (tuples_.size()/sb_+1);
  }

  // add the tuple in parameters
  void AddTuple(const std::vector<int64>& values){
    // a new tuple is created
    const int t=tuples_.size();
    Tuple* const tuple = new Tuple(n_);
    // we update the next_ of the tuple
    for(int i=0;i<n_;i++){
      if ( !doms_[i]->map_.in(values[i])){
        doms_[i]->AddValue(values[i]);
      }
      const int iv=doms_[i]->map_.ivalue(values[i]);
      const int oldt=doms_[i]->lastT_[iv];
      if (oldt != TABLE_TUPLE_NIL){
        tuples_[oldt]->inext_[i]=t;
      }
      tuple->ivalues_[i]=iv;
      doms_[i]->lastT_[iv]=t;
    }
    tuples_.push_back(tuple);
  }

  void CreateBuckets(){ // Must be called after all the tuples have been added
    const int numt=tuples_.size();
    for(int t=0;t<numt;t++){
      Tuple* tuple=tuples_[t];
      for(int i=0;i<n_;i++){
        doms_[i]->LinkBuckets(tuple->ivalues_[i], bucket(t),t);
      }
    }
  }
};

BtTable::BtTable(int n, int num_tuples, int size_bucket)
    : doms_(n),
      n_(n),
      sb_(size_bucket) {
  for(int i = 0; i < n; i++){
    doms_[i] = new Domain(num_tuples / size_bucket + 1);
  }
}

BtTable::~BtTable(){
  STLDeleteElements(&doms_); // delete all elements of a vector
}

BtTable::Domain::~Domain(){
  STLDeleteElements(&values_); // delete all elements of a vector
}

/*
 * Table Ct
 */

#define TABLECT_RESTART 0
#define TABLECT_CONTINUE 1
#define TABLECT_INVERSE 2
#define TABLECT_ORIGINAL 3

// RMK: When we traverse the domain of a variable, we obtain values and not indices of values
class TableCt;

class TableCtRestoreSupportAction : public Action {
  TableCt* ct_;
  const int x_; // var index
  const int iv_; // value index
  const int t_; // support
 public:
  TableCtRestoreSupportAction(TableCt* ct, int x, int iv, int t):ct_(ct),x_(x), iv_(iv), t_(t){}
  void Run(Solver* const solver);
};

class TableCt : public Constraint{
  class Var {
   public:
    class Value {
      friend class TableCt;
      std::vector<Value*> prevSC_; // n elements: the n prev pointers for the support tuple
      std::vector<Value*> nextSC_; // n elements : the n next pointeur for the support tuple
      Value* firstSC_; // first supported tuple
      int64 stamp_; // stamp of the last saving: the current support is saved at most once per level
      int support_; // support tuple: i.e. tuple index
      int ix_; // var index
      int iv_; // value index
      int del_; // true (>0) if the value is deleted; otherwise it is 0

      Value(Solver* solver, int ix, int iv, int n)
          : prevSC_(n),
            nextSC_(n),
            firstSC_(0),
            stamp_(solver->stamp() - 1),
            support_(TABLE_TUPLE_NIL),
            ix_(ix),
            iv_(iv),
            del_(0) {}

      int ivalue()const {return iv_;}

      int ix()const{return ix_;}

      bool deleted()const{return del_>0;}

      void SetDeleted(Solver* solver){
        solver->SaveValue(&del_);
        del_=1;
      }

      int64 stamp()const{return stamp_;}
    }; // end Value class

    friend class TableCt;
    // association with the BtTable
    MapInt64ToIndex map_;
    // correspondance between an index of the value of the variable
    // and the index of the value in the BtTable
    std::vector<int> xToTable_;
    // correspondance between an index of the value of BtTable and the
    // index of the value of the variable
    std::vector<int> tableToX_;

    std::vector<Value*> values_;
    IntVarIterator* itDomain_;
    IntVarIterator* itDeltaDomain_;
    IntVar* var_;

    Var(Solver* solver,
        BtTable* table,
        IntVar* var,
        const int ix,
        const int n)
        : values_(var->Size()),
          itDomain_(var->MakeDomainIterator(true)),
          itDeltaDomain_(var->MakeHoleIterator(true)),
          var_(var),
          xToTable_(var->Size(),TABLE_MAP_NIL),
          tableToX_(table->domain_size(ix),TABLE_MAP_NIL) {}

    ~Var(){
      STLDeleteElements(&values_); // delete all elements of a vector (the null are managed)
    }

    void CreateValues(Solver* solver, BtTable* table, int n, int ix){
      // we do not create an instance of Value if the value does not belong to the bttable
      IntVarIterator* it=itDomain_;
      int iv=0;
      for(it->Init();it->Ok();it->Next()){
        int64 val=it->Value();
        map_.push_back(val);
        //map_.add(val,iv);
        const int ivt=table->ivalue(ix,val);
        if (ivt == TABLE_MAP_NIL){//does not belong to BtTable
          values_[iv]=0;
        } else {
          values_[iv]=new Value(solver,ix,iv,n);
          xToTable_[iv]=ivt;
          tableToX_[ivt]=iv;
        }
        iv++;
      }
    }
    IntVarIterator* domain_iterator(){return itDomain_;}
    int ivalue_of_x_in_table(int iv)const{return xToTable_[iv];}
    int ivalue_of_table_in_x(int ivt)const{return tableToX_[ivt];}
    bool in_domain(int64 val){return var_->Contains(val);}
  };

  BtTable* table_; // table

  std::vector<int> orderedX_; // order of the var array
  std::vector<int> inOrderX_; // position in the order of var array
  std::vector<int> conflicts_; // number of conflicts
  std::vector<Var*> vars_; // variable of the constraint
  const int n_; // number of variables
  int countValid_;
  int ord_;
  int type_;
 public:
  TableCt(Solver* const solver,
          BtTable* table,
          const std::vector<IntVar*>& vars,
          int ord,
          int type)
      : Constraint(solver),
        table_(table),
        orderedX_(table->num_vars()),
        inOrderX_(table->num_vars()),
        conflicts_(table->num_vars(),0),
        vars_(table->num_vars()),
        n_(table->num_vars()),
        countValid_(0),
        ord_(ord),
        type_(type){
    const int n = table->num_vars();
    for(int i = 0; i < n; i++){
      orderedX_[i] = i;
      inOrderX_[i] = i;
      vars_[i] = new Var(solver, table, vars[i], i, n);
    }
  }

  ~TableCt(){
    STLDeleteElements(&vars_); // delete all elements of a vector
  }

  void Post(){
    const int n=n_;
    for(int i=0;i<n;i++){
      vars_[i]->CreateValues(solver(), table_, n, i);
      Demon* const d = MakeConstraintDemon1(
          solver(),
          this,
          &TableCt::FilterX,
          "FilterX",
          i);
      vars_[i]->var_->WhenDomain(d);
    }
  }

  void InitialPropagate(){
    SeekInitialSupport();
  }

  // Orderings: TODO could be improved if we have a lot of variables ???
  void OrderX(){// insertion sort: non decreasing size of domains
    const int n=vars_.capacity();
    for(int i=1;i<n;i++){
      const int x=orderedX_[i];
      for(int j=i;j>0;j--){
        const int y=orderedX_[j-1];
        if (vars_[x]->var_->Size() < vars_[y]->var_->Size()){
          orderedX_[j]=y;
          orderedX_[j-1]=x;
        } else {
          break;
        }
      }
    }
  }
  void OrderXConflicts(){// insertion sort: non decreasing size of the domain
    const int n=vars_.capacity();
    for(int i=1;i<n;i++){
      const int x=orderedX_[i];
      for(int j=i;j>0;j--){
        const int y=orderedX_[j-1];
        if (conflicts_[x] > conflicts_[y]){
          orderedX_[j]=y;
          orderedX_[j-1]=x;
        } else {
          break;
        }
      }
    }
  }

  // Seek functions
  int SeekBucketForVar(const int x, const int bk){
    // we search for the value having the smallest next_bucket value from bk
    int minbk=kint32max;		// minbk is the smallest value over the domains
    IntVarIterator* it=vars_[x]->domain_iterator();
    for(it->Init();it->Ok();it->Next()){ // We traverse the domain of x
      const int64 val=it->Value();
      const int iv=vars_[x]->map_.ivalue(val);
      const int supportBucket= table_->bucket(vars_[x]->values_[iv]->support_); // there is no valid bucket before the supporting one
      const int ivt=vars_[x]->ivalue_of_x_in_table(iv);
      const int nBucket=table_->next_bucket(x,ivt,bk);
      const int q = (supportBucket > nBucket) ? supportBucket : nBucket;
      if (q == bk) return bk; // we immediately returns bk
      if (q < minbk) minbk=q;
    }
    return minbk;
  }

  void AddToListSc(const int x, Var::Value* vv,const int t){
    for(int i=0;i<n_;i++){
      const int ivt=table_->tuple_ivalue(t,i);
      const int iv=vars_[i]->ivalue_of_table_in_x(ivt);
      Var::Value* vvi=vars_[i]->values_[iv];
      Var::Value* ifirst=vvi->firstSC_;
      if (ifirst != 0){
        ifirst->prevSC_[i]=vv;
      }
      vv->prevSC_[i]=0;
      vv->nextSC_[i]=vvi->firstSC_;
      vvi->firstSC_=vv;
    }
  }

  void InternalRemoveFromListSc(Var::Value* vv){ // vv is removed from every list listSC
    for(int i=0;i<n_;i++){
      Var::Value* nvv=vv->nextSC_[i];
      if (nvv != 0){
        nvv->prevSC_[i]=vv->prevSC_[i];
      }
      Var::Value* pvv=vv->prevSC_[i];
      if (pvv != 0){
        pvv->nextSC_[i]=vv->nextSC_[i];
      } else {// vv is the first in the listSC of the value of var i
        const int ivti=table_->tuple_ivalue(vv->support_,i);
        const int iv=vars_[i]->ivalue_of_table_in_x(ivti);
        vars_[i]->values_[iv]->firstSC_=vv->nextSC_[i];
      }
    }
  }

  void RemoveFromListSc(Var::Value* vv){ // vv is removed from every list listSC
    SaveSupport(vv->ix(),vv->ivalue()); // the support is saved
    InternalRemoveFromListSc(vv); // it is removed from the list ListSC
    vv->support_=TABLE_TUPLE_NIL; // the support is now NIL
  }

  void SaveSupport(int x, int iv){
    Var::Value* vv=vars_[x]->values_[iv];
    if (vv->stamp_ < solver()->stamp()){
      const int t=vv->support_;
      TableCtRestoreSupportAction* action= new TableCtRestoreSupportAction(this,x,iv,t);
      solver()->AddBacktrackAction(action,true);
      vv->stamp_ = solver()->stamp();
    }
  }

  void RestoreSupport(int x, int iv, int t){
    Var::Value* vv=vars_[x]->values_[iv];
    if (vv->support_ != TABLE_TUPLE_NIL){
      InternalRemoveFromListSc(vv);
    }
    AddToListSc(x,vv,t);
    vv->support_=t;
  }

  void SeekInitialSupport(const int x){
    IntVarIterator* it=vars_[x]->domain_iterator();
    for(it->Init();it->Ok();it->Next()){ // We traverse the domain of x
      const int64 val=it->Value();
      const int iv=vars_[x]->map_.ivalue(val); // index of value in the domain of var
      const int ivt=vars_[x]->ivalue_of_x_in_table(iv); // index of value in the table
      if (ivt != TABLE_TUPLE_NIL){ // the value is also in the table
        // we look at the value of nextBucket of 0 then we take the first tuple in bucket
        const int t=table_->first_ituple_in_bucket(x,ivt,table_->next_bucket(x,ivt,0));
        vars_[x]->values_[iv]->support_=t;
        AddToListSc(x,vars_[x]->values_[iv],t);
      } else { // the value is not in the table: we remove it from the variable
        vars_[x]->var_->RemoveValue(val);
      }
    }
  }

  void SeekInitialSupport(){
    for(int i=0;i<n_;i++){
      SeekInitialSupport(i);
    }
  }

  int count_valid()const{return countValid_;}

  int valid_tuple(const int t){
    countValid_++;
    for(int i=0;i<n_;i++){
      const int64 val=table_->value(i,table_->tuple_ivalue(t,i));
      if (!vars_[i]->in_domain(val)) return false;
    }
    return true;
  }

  int SeekSupport_in_bucket(const int x, const int t){
    // assume that tuple t IS NOT VALID
    const int lastT=table_->last_ituple_in_bucket(table_->bucket(t));
    for(int nt=table_->tuple_inext(t,x);nt <= lastT;nt=table_->tuple_inext(nt,x)){
      if (valid_tuple(nt)){
        return nt;
      }
    }
    return TABLE_TUPLE_NIL;
  }

  int SeekBucket(const int y, const int ibt, const int bk, const int type){
    if (bk >= table_->num_buckets()){
      return TABLE_BUCKET_NIL;
    }
    // we select the desired algorithm
    switch(type){
      case TABLECT_RESTART : return SeekBucketRestart(y,ibt,bk);
      case TABLECT_CONTINUE : return SeekBucketContinue(y,ibt,bk);
      case TABLECT_INVERSE : return SeekBucketInverse(y,ibt,bk);
      case TABLECT_ORIGINAL : return SeekBucketOriginal(y,ibt,bk);
    };
    return TABLE_BUCKET_NIL;
  }

  int SeekBucketRestart(const int y, const int ibt, const int bk){
    int nbk=bk;
    int j=0; // variable index
    while(j < n_){
      int q=(orderedX_[j]==y) ? table_->next_bucket(y,ibt,nbk) : SeekBucketForVar(orderedX_[j],nbk);
      if (q==nbk){j++;}
      else {// a progression occurs
        conflicts_[orderedX_[j]]++;
        if (q==TABLE_BUCKET_NIL){
          return TABLE_BUCKET_NIL;
        }
        q=table_->next_bucket(y,ibt,q);
        if (q==TABLE_BUCKET_NIL){
          return TABLE_BUCKET_NIL;
        }
        nbk=q;
        j=0;
      }
    }
    return nbk;
  }

  int SeekBucketContinue(const int y, const int ibt, const int bk){
    // var y, ibt ivalue in table, current bucket is bk
    int nbk=bk;
    int j=0; // variable index
    while(j < n_){
      // const int q=(orderedX_[j]==y) ? d_oms[y]->values_[b]->n_extPart[np] : seek_part_for_var(orderedX_[j],np);
      int q=(orderedX_[j]==y) ? table_->next_bucket(y,ibt,nbk) : SeekBucketForVar(orderedX_[j],nbk);
      if (q > nbk){// a progression occurs
        if (q==TABLE_BUCKET_NIL) return TABLE_BUCKET_NIL;
        q=table_->next_bucket(y,ibt,q);
        if (q==TABLE_BUCKET_NIL) return TABLE_BUCKET_NIL;
        nbk=q;
      }
      j++;
    }
    return nbk;
  }

  int SeekBucketInverse(int y, int ibt, int bk){
    // var y, ibt ivalue in table, current bucket is bk
    int nbk=bk;
    int j=0; // variable index
    while(j < n_){
      int q=(orderedX_[j]==y) ? table_->next_bucket(y,ibt,nbk) : SeekBucketForVar(orderedX_[j],nbk);
      if (q==nbk){j++;}
      else {// a progression occurs
        if (q==TABLE_BUCKET_NIL) return TABLE_BUCKET_NIL;
        q=table_->next_bucket(y,ibt,q);
        if (q==TABLE_BUCKET_NIL) return TABLE_BUCKET_NIL;
        nbk=q;
        if (j > 0) j--;
      }
    }
    return nbk;
  }

  int SeekBucketOriginal(const int y, const int ibt, const int bk){
    // var y, ibt ivalue in table, current bucket is bk
    int nq=bk;
    int nbk;
    int j=0; // variable index
    do {
      nbk=nq;
      while(j < n_){
        const int q=(orderedX_[j]==y) ? table_->next_bucket(y,ibt,nbk) : SeekBucketForVar(orderedX_[j],nbk);
        if (q==TABLE_BUCKET_NIL) return TABLE_BUCKET_NIL;
        j++;
      }
      nq=table_->next_bucket(y,ibt,nbk);
    } while (nbk < nq);
    return nbk;
  }

  int SeekSupport(const int x, const int ia, const int t, const int type){// search a support for (x,a)
    const int iat=vars_[x]->ivalue_of_x_in_table(ia);
    int ct=t;
    while (ct != TABLE_TUPLE_NIL){
      int nt=SeekSupport_in_bucket(x,ct);
      if (nt != TABLE_TUPLE_NIL){
        return nt;
      }
      const int b = SeekBucket(x,iat,table_->bucket(ct)+1,type);
      if (b == TABLE_TUPLE_NIL){
        break;
      }
      ct = table_->first_ituple_in_bucket(x,iat,b);
      if (valid_tuple(ct)){
        return ct;
      }
    }
    return TABLE_TUPLE_NIL;
  }

  void DeleteValue(int x, int type, Var::Value* vv){
    Var::Value* vvsupp=vv->firstSC_; // first supported value
    while(vvsupp != 0){
      // vvsupp is removed from the supported list of values
      const int oldSupport=vvsupp->support_;
      RemoveFromListSc(vvsupp);
      // we check if vvsupp is valid
      const int y=vvsupp->ix();
      const int b=vvsupp->ivalue();
      const int64 bval=vars_[y]->map_.value(b);
      if (vars_[y]->in_domain(bval)){ // vvsupp is valid. A new support must be sought
        const int nt=SeekSupport(y,b,oldSupport,type);
        if (nt==TABLE_TUPLE_NIL){ // no more support: (y,b) is deleted
          vars_[y]->var_->RemoveValue(bval);
        } else { // a new support is found
          vars_[y]->values_[b]->support_ = nt;
          AddToListSc(y,vars_[y]->values_[b],nt);
        }
      }
      vvsupp=vv->firstSC_;
    }
    vv->SetDeleted(solver());
  }

  void FilterX(const int x){
    // the variable whose index is x is modified
    if (ord_ > 0){
      if (ord_==1) OrderX();
      else OrderXConflicts();
    }
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

    const Var* const xv = vars_[x];
    // First iteration: from oldmin to min
    const int oldmindomain = xv->var_->OldMin();
    const int mindomain = xv->var_->Min();
    for(int64 val = oldmindomain;val<mindomain;val++){
      const int iv = xv->map_.ivalue(val);
      const int ivt = xv->ivalue_of_x_in_table(iv);
      if (ivt != xv->map_.default_index()){// the index is in the Var::Value array
        Var::Value* vv = xv->values_[iv];
        if (!vv->deleted()){ // the value deletion has never been considered
          DeleteValue(x, type_, vv);
        }
      }
    }

    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = xv->itDeltaDomain_;
    for(it->Init(); it->Ok(); it->Next()){
      int64 val = it->Value();
      const int iv = xv->map_.ivalue(val);
      const int ivt = xv->ivalue_of_x_in_table(iv);
      if (ivt != xv->map_.default_index()){// the index is in the Var::Value array
        Var::Value* vv = xv->values_[iv];
        DeleteValue(x, type_, vv);
      }
    }

    // Third iteration: from max to oldmax
    const int oldmaxdomain = xv->var_->OldMax();
    const int maxdomain = xv->var_->Max();
    for(int64 val = maxdomain + 1; val <= oldmaxdomain; val++){
      const int iv = xv->map_.ivalue(val);
      const int ivt = xv->ivalue_of_x_in_table(iv);
      if (ivt != xv->map_.default_index()){// the index is in the Var::Value array
        Var::Value* const vv = xv->values_[iv];
        if (!vv->deleted()){ // the value deletion has never been considered
          DeleteValue(x, type_, vv);
        }
      }
    }
  }
};

void TableCtRestoreSupportAction::Run(Solver* const solver){
  ct_->RestoreSupport(x_, iv_, t_);
}

// External API.

Constraint* BuildTableCt(Solver* const solver,
                         const IntTupleSet& tuples,
                         const std::vector<IntVar*>& vars,
                         int order,
                         int type,
                         int size_bucket) {
  const int num_tuples = tuples.NumTuples();
  const int arity = vars.size();
  BtTable* const table = new BtTable(arity, num_tuples, size_bucket);
  std::vector<int64> one_tuple(arity);
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < vars.size(); ++j) {
      one_tuple[j] = tuples.Value(i, j);
    }
    table->AddTuple(one_tuple);
  }
  table->CreateBuckets();
  return new TableCt(solver, table, vars, order, type);
}

} // namespace operations_research
