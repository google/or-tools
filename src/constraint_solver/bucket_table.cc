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
#include "util/vector_map.h"

namespace operations_research {
namespace {
/*
 * Bucketted Tuple Table
 */

#define TABLE_TUPLE_NIL kint32max
#define TABLE_BUCKET_NIL kint32max
#define TABLE_MAP_NIL kint32max

class BtTable;
class TableCt;


// Abstract domain of a variable containing abstract values.
class Domain {
 public:
  // Abstract value containing links between tuples.
  struct Value {
    Value(const int num_buckets)
    : first_tuple_in_bucket(num_buckets, TABLE_TUPLE_NIL),
      next_bucket(num_buckets, TABLE_BUCKET_NIL) {}

    // returns the index of the first tuple containing the involved
    // value in the given bucket
    std::vector<int> first_tuple_in_bucket;
    // returns the index of the first bucket following the given
    // bucket and containing a tuple involving the value.
    std::vector<int> next_bucket;
    // ATTENTION if the bucket b contains such a tuple then
    // next_bucket_[b] returns b
  };  // Value

  Domain(const int num_buckets) : num_buckets_(num_buckets) {}

  ~Domain() {}

  int size() const {
    return values_.size();
  }

  // internal
  void AddValue(const int64 val) {
    if (!map_.Contains(val)) {
      map_.Add(val);
      values_.push_back(Value(num_buckets_));
      last_tuple_index_.push_back(TABLE_TUPLE_NIL);
    }
  }

  void LinkBuckets(const int value_index,
                   const int bucket_index,
                   const int tuple_index) {
    if (values_[value_index].first_tuple_in_bucket[bucket_index] ==
        TABLE_TUPLE_NIL) {
      // in this case the bucket bucket_index does not contain any
      // tuple involving the value

      // tuple_index is the first tuple in the bucket
      values_[value_index].first_tuple_in_bucket[bucket_index] = tuple_index;
      // next_bucket of b returns b if there is a tuple of b
      // containing the value
      values_[value_index].next_bucket[bucket_index] = bucket_index;
      // the previous next_buckets are updated
      for(int b = bucket_index - 1; b >= 0; --b) {
        if (values_[value_index].next_bucket[b] == TABLE_BUCKET_NIL) {
          values_[value_index].next_bucket[b] = bucket_index;
        } else {
          break;
        }
      }
    }
  }

  bool Contains(int64 value) const {
    return map_.Contains(value);
  }

  int IndexFromValue(int64 value) const {
    return map_.Index(value);
  }

  int64 ValueFromIndex(int index) const {
    return map_.Element(index);
  }

  int NextBucket(int value_index, int bucket) const {
    return values_[value_index].next_bucket[bucket];
  }

  int FirstTupleInBucket(int value_index, int bucket) const {
    return values_[value_index].first_tuple_in_bucket[bucket];
  }

  int LastTupleIndex(int value_index) const {
    return last_tuple_index_[value_index];
  }

  void SetLastTupleIndex(int value_index, int tuple_index) {
    last_tuple_index_[value_index] = tuple_index;
  }

 private:
  VectorMap<int64> map_;
  std::vector<Value> values_;
  // temporary array containing for each value its last tuple index:
  // this improves the creation of next pointers
  std::vector<int> last_tuple_index_;
  // number of buckets
  const int num_buckets_;
}; // end class Domain

class BtTable {
 public:
  struct Tuple {
    Tuple(const int arity)
    : value_indices(arity),
      next_at_position(arity, TABLE_TUPLE_NIL) {}

    // indices of the values in the tuple
    std::vector<int> value_indices;
    // for each indice (i.e. var) i, it returns the index of the next
    // tuple containing the value at the position i
    std::vector<int> next_at_position;
  };  // Tuple

  BtTable(const int n, int num_tuples, int size_bucket)
      : domains_(n, Domain(num_tuples / size_bucket + 1)),
        arity_(n),
        size_of_bucket_(size_bucket) {
  }

  ~BtTable() {}

  int bucket(const int tuple_index) const {
    return tuple_index / size_of_bucket_;
  }

  int domain_size(const int var_index) const {
    return domains_[var_index].size();
  }

  bool in_domain(const int var_index, const int64 val) const {
    return domains_[var_index].Contains(val);
  }

  int index_from_value(const int var_index, const int64 val) const {
    return domains_[var_index].IndexFromValue(val);
  }

  int64 value(const int var_index, const int value_index_in_table) const {
    return domains_[var_index].ValueFromIndex(value_index_in_table);
  }

  int next_bucket(const int var_index,
                  const int value_index,
                  const int bucket) const {
    return domains_[var_index].NextBucket(value_index, bucket);
  }

  int first_ituple_in_bucket(const int var_index,
                             const int value_index,
                             const int b) const {
    return domains_[var_index].FirstTupleInBucket(value_index, b);
  }

  int last_ituple_in_bucket(const int bucket) {
    return (bucket + 1) * size_of_bucket_ - 1;
  }

  int tuple_index_from_value(const int tuple_index, const int value) const {
    return tuples_[tuple_index].value_indices[value];
  }

  int tuple_inext(const int t, const int x) const {
    return tuples_[t].next_at_position[x];
  }

  int num_tuples() const {
    return tuples_.size();
  }

  int num_vars() const {
    return arity_;
  }

  int num_buckets() const {
    return (tuples_.size() / size_of_bucket_ + 1);
  }

  // add the tuple in parameters
  void AddTuple(const std::vector<int64>& values) {
    // a new tuple is created
    const int tuple_index = tuples_.size();
    Tuple tuple(arity_);
    // we update the next_ of the tuple
    for(int i = 0; i < arity_; ++i) {
      if (!domains_[i].Contains(values[i])) {
        domains_[i].AddValue(values[i]);
      }
      const int value_index = domains_[i].IndexFromValue(values[i]);
      const int last_tuple_index = domains_[i].LastTupleIndex(value_index);
      if (last_tuple_index != TABLE_TUPLE_NIL) {
        tuples_[last_tuple_index].next_at_position[i] = tuple_index;
      }
      tuple.value_indices[i] = value_index;
      domains_[i].SetLastTupleIndex(value_index, tuple_index);
    }
    tuples_.push_back(tuple);
  }

  void CreateBuckets() { // Must be called after all the tuples have been added
    for(int tuple_index = 0; tuple_index < tuples_.size(); ++tuple_index) {
      for(int i = 0; i < arity_; ++i) {
        domains_[i].LinkBuckets(tuples_[tuple_index].value_indices[i],
                                bucket(tuple_index),
                                tuple_index);
      }
    }
  }

 private:
  std::vector<Tuple> tuples_;
  // domain of variables WITHIN TUPLES
  std::vector<Domain> domains_;
  const int arity_;
  const int size_of_bucket_;
};

/*
 * Table Ct
 */

#define TABLECT_RESTART 0
#define TABLECT_CONTINUE 1
#define TABLECT_INVERSE 2
#define TABLECT_ORIGINAL 3

// RMK: When we traverse the domain of a variable, we obtain values
// and not indices of values
class TableCtRestoreSupportAction : public Action {
 public:
  TableCtRestoreSupportAction(TableCt* ct,
                              int var_index,
                              int value_index,
                              int support)
      : ct_(ct),
        var_index_(var_index),
        value_index_(value_index),
        supporting_tuple_index_(support) {}

  virtual ~TableCtRestoreSupportAction() {}

  void Run(Solver* const solver);

 private:
  TableCt* const ct_;
  const int var_index_;
  const int value_index_;
  const int supporting_tuple_index_;
};

class TableVar {
 public:
  struct Value {
    Value(Solver* solver, int var_index, int value_index, int n)
        : prev_support_tuple(n),
          next_support_tuple(n),
          first_supported_tuple(0),
          stamp(solver->stamp() - 1),
          supporting_tuple_index(TABLE_TUPLE_NIL),
          var_index(var_index),
          value_index(value_index) {}

    // n elements: the n prev pointers for the support tuple
    std::vector<Value*> prev_support_tuple;
    // n elements : the n next pointeur for the support tuple
    std::vector<Value*> next_support_tuple;
    // first supported tuple
    Value* first_supported_tuple;
    // stamp of the last saving: the current support is saved at most
    // once per level
    int64 stamp;

    int supporting_tuple_index; // support tuple: i.e. tuple index
    const int var_index;
    const int value_index; // value index
    RevSwitch deleted;
  };  // Value

  TableVar(Solver* const solver,
           BtTable* const table,
           IntVar* const var,
           const int var_index,
           const int n)
      : values_(var->Size()),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        var_(var),
        x_to_table_(var->Size(),TABLE_MAP_NIL),
        table_to_x_(table->domain_size(var_index),TABLE_MAP_NIL) {}

  ~TableVar() {
    // delete all elements of a vector (the null are managed).
    STLDeleteElements(&values_);
  }

  void CreateValues(Solver* const solver,
                    BtTable* const table,
                    int n,
                    int var_index) {
    // we do not create an instance of Value if the value does not
    // belong to the bttable
    IntVarIterator* const it = domain_iterator_;
    int value_index = 0;
    for(it->Init(); it->Ok(); it->Next()) {
      const int64 val = it->Value();
      map_.Add(val);
      const int value_index_in_table = table->index_from_value(var_index, val);
      if (value_index_in_table == TABLE_MAP_NIL) {//does not belong to BtTable
        values_[value_index] = NULL;
      } else {
        values_[value_index] = new Value(solver,
                                         var_index,
                                         value_index,
                                         n);
        x_to_table_[value_index] = value_index_in_table;
        table_to_x_[value_index_in_table] = value_index;
      }
      value_index++;
    }
  }

  IntVarIterator* domain_iterator() {
    return domain_iterator_;
  }

  int index_value_of_x_in_table(int value_index) const {
    return x_to_table_[value_index];
  }

  int index_value_of_table_in_x(int value_index_in_table) const {
    return table_to_x_[value_index_in_table];
  }

  bool in_domain(int64 val) {
    return var_->Contains(val);
  }

  friend class TableCt;

 private:
  // association with the BtTable
  VectorMap<int64> map_;
  // correspondance between an index of the value of the variable
  // and the index of the value in the BtTable
  std::vector<int> x_to_table_;
  // correspondance between an index of the value of BtTable and the
  // index of the value of the variable
  std::vector<int> table_to_x_;

  std::vector<Value*> values_;
  IntVarIterator* domain_iterator_;
  IntVarIterator* delta_domain_iterator_;
  IntVar* var_;
};

class TableCt : public Constraint {
 public:
  TableCt(Solver* const solver,
          BtTable* table,
          const std::vector<IntVar*>& vars,
          int ord,
          int type)
      : Constraint(solver),
        table_(table),
        ordered_x_(table->num_vars()),
        in_order_x_(table->num_vars()),
        conflicts_(table->num_vars(),0),
        vars_(table->num_vars()),
        arity_(table->num_vars()),
        count_valid_(0),
        ordering_(ord),
        type_(type) {
    const int n = table->num_vars();
    for(int i = 0; i < n; i++) {
      ordered_x_[i] = i;
      in_order_x_[i] = i;
      vars_[i] = new TableVar(solver, table, vars[i], i, n);
    }
  }

  ~TableCt() {
    STLDeleteElements(&vars_); // delete all elements of a vector
    delete table_;
  }

  void Post() {
    const int n=arity_;
    for(int i = 0; i < n; i++) {
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

  void InitialPropagate() {
    SeekInitialSupport();
  }

  // Orderings: TODO could be improved if we have a lot of variables ???
  void OrderX() {  // insertion sort: non decreasing size of domains
    const int n = vars_.capacity();
    for(int i = 1; i < n; ++i) {
      const int x = ordered_x_[i];
      for(int j = i;j > 0; j--) {
        const int y = ordered_x_[j - 1];
        if (vars_[x]->var_->Size() < vars_[y]->var_->Size()) {
          ordered_x_[j] = y;
          ordered_x_[j - 1] = x;
        } else {
          break;
        }
      }
    }
  }

  // insertion sort: non decreasing size of the domain.
  void OrderXConflicts() {
    const int n = vars_.capacity();
    for(int i = 1; i < n; ++i) {
      const int x=ordered_x_[i];
      for(int j = i; j > 0; --j) {
        const int y = ordered_x_[j - 1];
        if (conflicts_[x] > conflicts_[y]) {
          ordered_x_[j] = y;
          ordered_x_[j - 1] = x;
        } else {
          break;
        }
      }
    }
  }

  // Seek functions
  int SeekBucketForVar(const int x, const int bk) {
    // we search for the value having the smallest next_bucket value from bk
    int minbk = kint32max; // minbk is the smallest value over the domains
    IntVarIterator* const it = vars_[x]->domain_iterator();
    for(it->Init(); it->Ok(); it->Next()) { // We traverse the domain of x
      const int64 val = it->Value();
      const int value_index = vars_[x]->map_.Index(val);
      // there is no valid bucket before the supporting one
      const int supportBucket =
          table_->bucket(
              vars_[x]->values_[value_index]->supporting_tuple_index);
      const int value_index_in_table =
          vars_[x]->index_value_of_x_in_table(value_index);
      const int nBucket = table_->next_bucket(x,value_index_in_table,bk);
      const int q = (supportBucket > nBucket) ? supportBucket : nBucket;
      if (q == bk) {
        return bk; // we immediately returns bk
      }
      if (q < minbk) {
        minbk=q;
      }
    }
    return minbk;
  }

  void AddToListSc(int x, TableVar::Value* const vv, int t) {
    for(int i = 0; i < arity_; ++i) {
      const int value_index_in_table = table_->tuple_index_from_value(t, i);
      const int value_index =
          vars_[i]->index_value_of_table_in_x(value_index_in_table);
      TableVar::Value* const vvi = vars_[i]->values_[value_index];
      TableVar::Value* const ifirst = vvi->first_supported_tuple;
      if (ifirst != 0) {
        ifirst->prev_support_tuple[i] = vv;
      }
      vv->prev_support_tuple[i] = NULL;
      vv->next_support_tuple[i] = vvi->first_supported_tuple;
      vvi->first_supported_tuple = vv;
    }
  }

  // vv is removed from every list listSC
  void InternalRemoveFromListSc(TableVar::Value* const vv) {
    for(int i = 0; i < arity_; ++i) {
      TableVar::Value* const nvv = vv->next_support_tuple[i];
      if (nvv != 0) {
        nvv->prev_support_tuple[i] = vv->prev_support_tuple[i];
      }
      TableVar::Value* const pvv = vv->prev_support_tuple[i];
      if (pvv != 0) {
        pvv->next_support_tuple[i] = vv->next_support_tuple[i];
      } else {  // vv is the first in the listSC of the value of var i
        const int value_index_in_table_i =
            table_->tuple_index_from_value(vv->supporting_tuple_index, i);
        const int value_index =
            vars_[i]->index_value_of_table_in_x(value_index_in_table_i);
        vars_[i]->values_[value_index]->first_supported_tuple =
            vv->next_support_tuple[i];
      }
    }
  }

  // vv is removed from every list listSC
  void RemoveFromListSc(TableVar::Value* const vv) {
    SaveSupport(vv->var_index, vv->value_index);
    InternalRemoveFromListSc(vv); // it is removed from the list ListSC
    vv->supporting_tuple_index = TABLE_TUPLE_NIL; // the support is now NIL
  }

  void SaveSupport(int x, int value_index) {
    TableVar::Value* const vv = vars_[x]->values_[value_index];
    if (vv->stamp < solver()->stamp()) {
      const int tuple_index = vv->supporting_tuple_index;
      TableCtRestoreSupportAction* const action =
          solver()->RevAlloc(
              new TableCtRestoreSupportAction(
                  this, x, value_index, tuple_index));
      solver()->AddBacktrackAction(action, true);
      vv->stamp = solver()->stamp();
    }
  }

  void RestoreSupport(int var_index, int value_index, int tuple_index) {
    TableVar::Value* const vv = vars_[var_index]->values_[value_index];
    if (vv->supporting_tuple_index != TABLE_TUPLE_NIL) {
      InternalRemoveFromListSc(vv);
    }
    AddToListSc(var_index, vv, tuple_index);
    vv->supporting_tuple_index = tuple_index;
  }

  void SeekInitialSupport(int var_index) {
    IntVarIterator* const it = vars_[var_index]->domain_iterator();
    for(it->Init(); it->Ok(); it->Next()) { // We traverse the domain of x
      const int64 val = it->Value();
      // index of value in the domain of var
      const int value_index = vars_[var_index]->map_.Index(val);
      const int value_index_in_table =
          vars_[var_index]->index_value_of_x_in_table(value_index);
      // the value is also in the table
      if (value_index_in_table != TABLE_TUPLE_NIL) {
        // we look at the value of next_bucket of 0 then we take the
        // first tuple in bucket
        const int tuple_index =
            table_->first_ituple_in_bucket(
                var_index,
                value_index_in_table,
                table_->next_bucket(
                    var_index,
                    value_index_in_table,
                    0));
        vars_[var_index]->values_[value_index]->supporting_tuple_index =
            tuple_index;
        AddToListSc(var_index,
                    vars_[var_index]->values_[value_index],
                    tuple_index);
      } else { // the value is not in the table: we remove it from the variable
        vars_[var_index]->var_->RemoveValue(val);
      }
    }
  }

  void SeekInitialSupport() {
    for(int i = 0; i < arity_; ++i) {
      SeekInitialSupport(i);
    }
  }

  int count_valid() const {
    return count_valid_;
  }

  int valid_tuple(const int t) {
    count_valid_++;
    for(int i = 0; i < arity_; ++i) {
      const int64 val = table_->value(i, table_->tuple_index_from_value(t, i));
      if (!vars_[i]->in_domain(val)) {
        return false;
      }
    }
    return true;
  }

  int SeekSupportInBucket(const int x, const int t) {
    // assume that tuple t IS NOT VALID
    const int last_t = table_->last_ituple_in_bucket(table_->bucket(t));
    for(int nt = table_->tuple_inext(t, x);
        nt <= last_t;
        nt = table_->tuple_inext(nt, x)) {
      if (valid_tuple(nt)) {
        return nt;
      }
    }
    return TABLE_TUPLE_NIL;
  }

  int SeekBucket(const int y, const int ibt, const int bk, const int type) {
    if (bk >= table_->num_buckets()) {
      return TABLE_BUCKET_NIL;
    }
    // we select the desired algorithm
    switch(type) {
      case TABLECT_RESTART : return SeekBucketRestart(y,ibt,bk);
      case TABLECT_CONTINUE : return SeekBucketContinue(y,ibt,bk);
      case TABLECT_INVERSE : return SeekBucketInverse(y,ibt,bk);
      case TABLECT_ORIGINAL : return SeekBucketOriginal(y,ibt,bk);
    };
    return TABLE_BUCKET_NIL;
  }

  int SeekBucketRestart(const int y, const int ibt, const int bk) {
    int nbk = bk;
    int j = 0; // variable index
    while(j < arity_) {
      int q = (ordered_x_[j] == y) ?
          table_->next_bucket(y, ibt, nbk) :
          SeekBucketForVar(ordered_x_[j], nbk);
      if (q == nbk) {
        j++;
      }  else {// a progression occurs
        conflicts_[ordered_x_[j]]++;
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        q = table_->next_bucket(y, ibt, q);
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        nbk = q;
        j = 0;
      }
    }
    return nbk;
  }

  int SeekBucketContinue(const int y, const int ibt, const int bk) {
    // var y, ibt index_from_value in table, current bucket is bk
    int nbk = bk;
    int j = 0; // variable index
    while(j < arity_) {
      int q = (ordered_x_[j] == y) ?
          table_->next_bucket(y, ibt, nbk) :
          SeekBucketForVar(ordered_x_[j], nbk);
      if (q > nbk) {  // a progression occurs
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        q = table_->next_bucket(y, ibt, q);
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        nbk = q;
      }
      j++;
    }
    return nbk;
  }

  int SeekBucketInverse(int y, int ibt, int bk) {
    // var y, ibt index_from_value in table, current bucket is bk
    int nbk = bk;
    int j = 0; // variable index
    while(j < arity_) {
      int q = (ordered_x_[j] == y) ?
          table_->next_bucket(y, ibt, nbk) :
          SeekBucketForVar(ordered_x_[j], nbk);
      if (q == nbk) {
        j++;
      }  else {  // a progression occurs
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        q = table_->next_bucket(y, ibt, q);
        if (q==TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        nbk = q;
        if (j > 0) {
          j--;
        }
      }
    }
    return nbk;
  }

  int SeekBucketOriginal(const int y, const int ibt, const int bk) {
    // var y, ibt index_from_value in table, current bucket is bk
    int nq = bk;
    int nbk;
    int j = 0; // variable index
    do {
      nbk = nq;
      while(j < arity_) {
        const int q =(ordered_x_[j] == y) ?
            table_->next_bucket(y, ibt, nbk) :
            SeekBucketForVar(ordered_x_[j], nbk);
        if (q == TABLE_BUCKET_NIL) {
          return TABLE_BUCKET_NIL;
        }
        j++;
      }
      nq = table_->next_bucket(y, ibt, nbk);
    } while (nbk < nq);
    return nbk;
  }

  // search a support for (x,a)
  int SeekSupport(const int x, const int ia, const int t, const int type) {
    const int iat = vars_[x]->index_value_of_x_in_table(ia);
    int ct = t;
    while (ct != TABLE_TUPLE_NIL) {
      int nt = SeekSupportInBucket(x,ct);
      if (nt != TABLE_TUPLE_NIL) {
        return nt;
      }
      const int b = SeekBucket(x, iat, table_->bucket(ct) + 1, type);
      if (b == TABLE_TUPLE_NIL) {
        break;
      }
      ct = table_->first_ituple_in_bucket(x, iat, b);
      if (valid_tuple(ct)) {
        return ct;
      }
    }
    return TABLE_TUPLE_NIL;
  }

  void DeleteValue(int x, int type, TableVar::Value* const vv) {
    // first supported value
    TableVar::Value* vvsupp = vv->first_supported_tuple;
    while (vvsupp != 0) {
      // vvsupp is removed from the supported list of values
      const int old_support = vvsupp->supporting_tuple_index;
      RemoveFromListSc(vvsupp);
      // we check if vvsupp is valid
      const int y = vvsupp->var_index;
      const int b = vvsupp->value_index;
      const int64 bval = vars_[y]->map_.Element(b);
      if (vars_[y]->in_domain(bval)) {
        // vvsupp is valid. A new support must be sought
        const int nt = SeekSupport(y, b, old_support, type);
        if (nt == TABLE_TUPLE_NIL) {  // no more support: (y,b) is deleted
          vars_[y]->var_->RemoveValue(bval);
        } else {  // a new support is found
          vars_[y]->values_[b]->supporting_tuple_index = nt;
          AddToListSc(y, vars_[y]->values_[b], nt);
        }
      }
      vvsupp = vv->first_supported_tuple;
    }
    vv->deleted.Switch(solver());
  }

  void FilterX(const int x) {
    // the variable whose index is x is modified
    if (ordering_ > 0) {
      if (ordering_==1) OrderX();
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

    const TableVar* const xv = vars_[x];
    // First iteration: from oldmin to min
    const int64 oldmindomain = xv->var_->OldMin();
    const int64 mindomain = xv->var_->Min();
    for(int64 val = oldmindomain; val < mindomain; ++val) {
      const int value_index = xv->map_.Index(val);
      const int value_index_in_table =
          xv->index_value_of_x_in_table(value_index);
      if (value_index_in_table != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const vv = xv->values_[value_index];
        if (!vv->deleted.Switched()) {
          // the value deletion has never been considered
          DeleteValue(x, type_, vv);
        }
      }
    }

    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = xv->delta_domain_iterator_;
    for(it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      const int value_index = xv->map_.Index(val);
      const int value_index_in_table =
          xv->index_value_of_x_in_table(value_index);
      if (value_index_in_table != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const vv = xv->values_[value_index];
        DeleteValue(x, type_, vv);
      }
    }

    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = xv->var_->OldMax();
    const int64 maxdomain = xv->var_->Max();
    for(int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      const int value_index = xv->map_.Index(val);
      const int value_index_in_table =
          xv->index_value_of_x_in_table(value_index);
      if (value_index_in_table != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const vv = xv->values_[value_index];
        if (!vv->deleted.Switched()) {
          // the value deletion has never been considered
          DeleteValue(x, type_, vv);
        }
      }
    }
  }

 private:
  BtTable* const table_;

  std::vector<int> ordered_x_; // order of the var array
  std::vector<int> in_order_x_; // position in the order of var array
  std::vector<int> conflicts_; // number of conflicts
  std::vector<TableVar*> vars_; // variable of the constraint
  const int arity_; // number of variables
  int count_valid_;
  const int ordering_;
  const int type_;
};

void TableCtRestoreSupportAction::Run(Solver* const solver) {
  ct_->RestoreSupport(var_index_, value_index_, supporting_tuple_index_);
}
}  // namespace

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
  return solver->RevAlloc(new TableCt(solver, table, vars, order, type));
}

} // namespace operations_research
