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

DEFINE_int32(cp_bucket_table_type, 3,
             "Type for the bucket table, 0 = restart, 1 = continue, "
             "2 = inverse, 3 = original");
DEFINE_int32(cp_bucket_table_ordering, 0,
             "Ordering of variables in bucket table constraint: "
             " 0 = none, 1 = min domain size, 2 = max conflicts");

namespace operations_research {
namespace {

DEFINE_INT_TYPE(VarIndex, int);
DEFINE_INT_TYPE(TupleIndex, int);
DEFINE_INT_TYPE(BucketIndex, int);
DEFINE_INT_TYPE(TableValueIndex, int);
DEFINE_INT_TYPE(VarValueIndex, int);

static const TupleIndex kNilTuple = TupleIndex(-1);
static const BucketIndex kNilBucket = BucketIndex(-1);
static const TableValueIndex kNilTableValue = TableValueIndex(-1);
static const VarValueIndex kNilVarValue = VarValueIndex(-1);

class BtTable;
class TableCt;

// Abstract domain of a variable containing abstract values.
class Domain {
 public:
  // Abstract value containing links between tuples.
  struct Value {
    Value(BucketIndex num_buckets)
    : first_tuple_in_bucket(num_buckets.value(), kNilTuple),
      next_bucket(num_buckets.value(), kNilBucket) {}

    // Returns the index of the first tuple containing the involved
    // value in the given bucket
    ITIVector<BucketIndex, TupleIndex> first_tuple_in_bucket;
    // Returns the index of the first bucket following the given
    // bucket and containing a tuple involving the value.
    ITIVector<BucketIndex, BucketIndex> next_bucket;
    // ATTENTION if the bucket b contains such a tuple then
    // next_bucket_[b] returns b
  };  // Value

  Domain(BucketIndex num_buckets) : num_buckets_(num_buckets) {}

  ~Domain() {}

  TableValueIndex size() const {
    return TableValueIndex(values_.size());
  }

  void AddValue(const int64 val) {
    if (!map_.Contains(val)) {
      map_.Add(val);
      values_.push_back(Value(num_buckets_));
      last_tuple_index_.push_back(kNilTuple);
    }
  }

  void LinkBuckets(TableValueIndex value_index,
                   BucketIndex bucket_index,
                   TupleIndex tuple_index) {
    if (values_[value_index].first_tuple_in_bucket[bucket_index] ==
        kNilTuple) {
      // In this case the bucket bucket_index does not contain any
      // tuple involving the value.

      // Tuple_index is the first tuple in the bucket.
      values_[value_index].first_tuple_in_bucket[bucket_index] = tuple_index;
      // Next_bucket of b returns b if there is a tuple of b
      // containing the value.
      values_[value_index].next_bucket[bucket_index] = bucket_index;
      // The previous next_buckets are updated.
      for(BucketIndex b = bucket_index - 1; b >= 0; --b) {
        if (values_[value_index].next_bucket[b] == kNilBucket) {
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

  TableValueIndex IndexFromValue(int64 value) const {
    return map_.Index(value);
  }

  int64 ValueFromIndex(TableValueIndex index) const {
    return map_.Element(index);
  }

  BucketIndex NextBucket(TableValueIndex value_index,
                         BucketIndex bucket) const {
    return values_[value_index].next_bucket[bucket];
  }

  TupleIndex FirstTupleInBucket(TableValueIndex value_index,
                                BucketIndex bucket) const {
    return values_[value_index].first_tuple_in_bucket[bucket];
  }

  TupleIndex LastTupleIndex(TableValueIndex value_index) const {
    return last_tuple_index_[value_index];
  }

  void SetLastTupleIndex(TableValueIndex value_index, TupleIndex tuple_index) {
    last_tuple_index_[value_index] = tuple_index;
  }

 private:
  TypedVectorMap<TableValueIndex, int64> map_;
  ITIVector<TableValueIndex, Value> values_;
  // Temporary array containing for each value its last tuple index:
  // this improves the creation of next pointers.
  ITIVector<TableValueIndex, TupleIndex> last_tuple_index_;
  const BucketIndex num_buckets_;
};  // Domain

class BtTable {
 public:
  struct Tuple {
    Tuple(VarIndex arity)
    : value_indices(arity.value()),
      next_at_position(arity.value(), kNilTuple) {}

    // Indices of the values in the tuple.
    ITIVector<VarIndex, TableValueIndex> value_indices;
    // For each indice (i.e. var) i, it returns the index of the next
    // tuple containing the value at the position i.
    ITIVector<VarIndex, TupleIndex> next_at_position;
  };  // Tuple

  BtTable(VarIndex arity, int num_tuples, int size_of_bucket)
      : domains_(arity.value(),
                 Domain(BucketIndex(num_tuples / size_of_bucket + 1))),
        arity_(arity),
        size_of_bucket_(size_of_bucket) {
  }

  ~BtTable() {}

  BucketIndex bucket(TupleIndex tuple_index) const {
    DCHECK_NE(tuple_index, kNilTuple);
    return BucketIndex(tuple_index.value() / size_of_bucket_);
  }

  TableValueIndex DomainSize(VarIndex var_index) const {
    return domains_[var_index].size();
  }

  bool InDomain(VarIndex var_index, const int64 val) const {
    return domains_[var_index].Contains(val);
  }

  TableValueIndex IndexFromValue(VarIndex var_index, const int64 val) const {
    return domains_[var_index].IndexFromValue(val);
  }

  int64 value(VarIndex var_index, TableValueIndex table_value_index) const {
    return domains_[var_index].ValueFromIndex(table_value_index);
  }

  BucketIndex NextBucket(VarIndex var_index,
                         TableValueIndex value_index,
                         BucketIndex bucket) const {
    return domains_[var_index].NextBucket(value_index, bucket);
  }

  TupleIndex FirstTupleInBucket(VarIndex var_index,
                                TableValueIndex value_index,
                                BucketIndex bucket_index) const {
    return domains_[var_index].FirstTupleInBucket(value_index, bucket_index);
  }

  TupleIndex LastTupleInBucket(BucketIndex bucket) {
    return TupleIndex((bucket.value() + 1) * size_of_bucket_ - 1);
  }

  TableValueIndex ValueIndexFromPositionInTuple(TupleIndex tuple_index,
                                                VarIndex var_index) const {
    return tuples_[tuple_index].value_indices[var_index];
  }

  TupleIndex NextTupleFromPosition(TupleIndex tuple_index,
                                   VarIndex var_index) const {
    return tuples_[tuple_index].next_at_position[var_index];
  }

  TupleIndex NumTuples() const {
    return TupleIndex(tuples_.size());
  }

  VarIndex NumVars() const {
    return arity_;
  }

  BucketIndex NumBuckets() const {
    return BucketIndex(tuples_.size() / size_of_bucket_ + 1);
  }

  // Adds the tuple in parameters.
  void AddTuple(const std::vector<int64>& values) {
    // A new tuple is created.
    TupleIndex tuple_index = NumTuples();
    Tuple tuple(arity_);
    // We update the next_ of the tuple.
    for(VarIndex i = VarIndex(0); i < arity_; ++i) {
      if (!domains_[i].Contains(values[i.value()])) {
        domains_[i].AddValue(values[i.value()]);
      }
      const TableValueIndex value_index =
          domains_[i].IndexFromValue(values[i.value()]);
      const TupleIndex last_tuple_index =
          domains_[i].LastTupleIndex(value_index);
      if (last_tuple_index != kNilTuple) {
        tuples_[last_tuple_index].next_at_position[i] = tuple_index;
      }
      tuple.value_indices[i] = value_index;
      domains_[i].SetLastTupleIndex(value_index, tuple_index);
    }
    tuples_.push_back(tuple);
  }

  // Must be called after all the tuples have been added.
  void CreateBuckets() {
    for(TupleIndex tuple_index = TupleIndex(0);
        tuple_index < tuples_.size();
        ++tuple_index) {
      for(VarIndex i = VarIndex(0); i < arity_; ++i) {
        domains_[i].LinkBuckets(tuples_[tuple_index].value_indices[i],
                                bucket(tuple_index),
                                tuple_index);
      }
    }
  }

 private:
  ITIVector<TupleIndex, Tuple> tuples_;
  // domain of variables WITHIN TUPLES
  ITIVector<VarIndex, Domain> domains_;
  const VarIndex arity_;
  const int size_of_bucket_;
};



// RMK: When we traverse the domain of a variable, we obtain values
// and not indices of values
class TableCtRestoreSupportAction : public Action {
 public:
  TableCtRestoreSupportAction(TableCt* ct,
                              VarIndex var_index,
                              VarValueIndex value_index,
                              TupleIndex support)
      : ct_(ct),
        var_index_(var_index),
        value_index_(value_index),
        supporting_tuple_index_(support) {}

  virtual ~TableCtRestoreSupportAction() {}

  void Run(Solver* const solver);

 private:
  TableCt* const ct_;
  VarIndex var_index_;
  const VarValueIndex value_index_;
  const TupleIndex supporting_tuple_index_;
};

class TableVar {
 public:
  struct Value {
    Value(Solver* solver,
          VarIndex var_index,
          VarValueIndex value_index,
          VarIndex n)
        : prev_support_tuple(n.value()),
          next_support_tuple(n.value()),
          first_supported_tuple(NULL),
          stamp(solver->stamp() - 1),
          supporting_tuple_index(kNilTuple),
          var_index(var_index),
          value_index(value_index) {}

    // n elements: the n prev pointers for the support tuple.
    ITIVector<VarIndex, Value*> prev_support_tuple;
    // n elements : the n next pointeur for the support tuple.
    ITIVector<VarIndex, Value*> next_support_tuple;
    // First supported tuple.
    Value* first_supported_tuple;
    // Stamp of the last saving: the current support is saved at most
    // once per level
    int64 stamp;
    // support tuple: i.e. tuple index
    TupleIndex supporting_tuple_index;
    const VarIndex var_index;
    const VarValueIndex value_index;
    RevSwitch deleted;
  };  // Value

  TableVar(Solver* const solver,
           BtTable* const table,
           IntVar* const var,
           VarIndex var_index)
      : values_(var->Size()),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        var_(var),
        var_to_table_(var->Size(), kNilTableValue),
        table_to_var_(table->DomainSize(var_index).value(), kNilVarValue) {}

  ~TableVar() {
    // Delete all elements of a vector (the null are managed).
    STLDeleteElements(&values_);
  }

  void CreateValues(Solver* const solver,
                    BtTable* const table,
                    VarIndex arity,
                    VarIndex var_index) {
    // We do not create an instance of Value if the value does not
    // belong to the BtTable.
    IntVarIterator* const it = domain_iterator_;
    VarValueIndex value_index = VarValueIndex(0);
    for(it->Init(); it->Ok(); it->Next()) {
      const int64 val = it->Value();
      map_.Add(val);
      const TableValueIndex table_value_index =
          table->IndexFromValue(var_index, val);
      if (table_value_index == kNilTableValue) {
        //does not belong to BtTable
        values_[value_index] = NULL;
      } else {
        values_[value_index] = new Value(solver,
                                         var_index,
                                         value_index,
                                         arity);
        var_to_table_[value_index] = table_value_index;
        table_to_var_[table_value_index] = value_index;
      }
      value_index++;
    }
  }

  IntVarIterator* domain_iterator() {
    return domain_iterator_;
  }

  TableValueIndex VarIndexToTableIndex(VarValueIndex value_index) const {
    return var_to_table_[value_index];
  }

  VarValueIndex TableIndexToVarIndex(TableValueIndex table_value_index) const {
    return table_to_var_[table_value_index];
  }

  bool InDomain(int64 val) {
    return var_->Contains(val);
  }

  IntVar* Var() const {
    return var_;
  }

  VarValueIndex IndexFromValue(int64 value) const {
    return map_.Index(value);
  }

  int64 ValueFromIndex(VarValueIndex index) const {
    return map_.Element(index);
  }

  TupleIndex SupportingTupleIndex(VarValueIndex value_index) const {
    return values_[value_index]->supporting_tuple_index;
  }

  Value* value(VarValueIndex value_index) const {
    return values_[value_index];
  }

  IntVarIterator* DeltaDomainIterator() const {
    return delta_domain_iterator_;
  }

 private:
  // Association with the BtTable.
  TypedVectorMap<VarValueIndex, int64> map_;
  // Correspondance between an index of the value of the variable
  // and the index of the value in the BtTable,
  ITIVector<VarValueIndex, TableValueIndex> var_to_table_;
  // Correspondance between an index of the value of BtTable and the
  // index of the value of the variable.
  ITIVector<TableValueIndex, VarValueIndex> table_to_var_;

  ITIVector<VarValueIndex, Value*> values_;
  IntVarIterator* domain_iterator_;
  IntVarIterator* delta_domain_iterator_;
  IntVar* var_;
};

class TableCt : public Constraint {
 public:
  enum Type { TABLECT_RESTART = 0,
              TABLECT_CONTINUE = 1,
              TABLECT_INVERSE = 2,
              TABLECT_ORIGINAL = 3 };

  enum Ordering { NONE = 0,
                  DOMAIN_MIN = 1,
                  CONFLICT_MAX = 2 };

  TableCt(Solver* const solver,
          BtTable* table,
          const std::vector<IntVar*>& vars,
          Ordering ord,
          Type type)
      : Constraint(solver),
        table_(table),
        ordered_x_(table->NumVars().value()),
        in_order_x_(table->NumVars().value()),
        conflicts_(table->NumVars().value(),0),
        vars_(table->NumVars().value()),
        arity_(table->NumVars().value()),
        ordering_(ord),
        type_(type) {
    const VarIndex arity = table->NumVars();
    for(VarIndex i = VarIndex(0); i < arity; ++i) {
      ordered_x_[i] = i;
      in_order_x_[i] = i;
      vars_[i] = new TableVar(solver, table, vars[i.value()], i);
    }
  }

  ~TableCt() {
    STLDeleteElements(&vars_); // delete all elements of a vector
    delete table_;
  }

  void Post() {
    for(VarIndex i = VarIndex(0); i < arity_; i++) {
      vars_[i]->CreateValues(solver(), table_, arity_, i);
      Demon* const d = MakeConstraintDemon1(
          solver(),
          this,
          &TableCt::FilterX,
          "FilterX",
          i.value());
      vars_[i]->Var()->WhenDomain(d);
    }
  }

  void InitialPropagate() {
    SeekInitialSupport();
  }

  // Orderings: TODO could be improved if we have a lot of variables ???
  void OrderX() {  // insertion sort: non decreasing size of domains
    for(VarIndex i = VarIndex(1); i < arity_; ++i) {
      VarIndex var_index = ordered_x_[i];
      for(VarIndex j = i; j > 0; j--) {
        const VarIndex other_var_index = ordered_x_[j - 1];
        if (vars_[var_index]->Var()->Size() <
            vars_[other_var_index]->Var()->Size()) {
          ordered_x_[j] = other_var_index;
          ordered_x_[j - 1] = var_index;
        } else {
          break;
        }
      }
    }
  }

  // insertion sort: non decreasing size of the domain.
  void OrderXConflicts() {
    for(VarIndex i = VarIndex(1); i < arity_; ++i) {
      VarIndex var_index = ordered_x_[i];
      for(VarIndex j = i; j > 0; --j) {
        const VarIndex other_var_index = ordered_x_[j - 1];
        if (conflicts_[var_index] > conflicts_[other_var_index]) {
          ordered_x_[j] = other_var_index;
          ordered_x_[j - 1] = var_index;
        } else {
          break;
        }
      }
    }
  }

  // Seek functions
  BucketIndex SeekBucketForVar(VarIndex var_index, BucketIndex bucket) {
    // we search for the value having the smallest next_bucket value
    // from bucket

    // min_bucket is the smallest value over the domains
    BucketIndex min_bucket = BucketIndex(kint32max);
    IntVarIterator* const it = vars_[var_index]->domain_iterator();
    for(it->Init(); it->Ok(); it->Next()) { // We traverse the domain of var.
      const int64 val = it->Value();
      const VarValueIndex value_index = vars_[var_index]->IndexFromValue(val);
      // there is no valid bucket before the supporting one
      const BucketIndex support_bucket =
          table_->bucket(vars_[var_index]->SupportingTupleIndex(value_index));
      const TableValueIndex table_value_index =
          vars_[var_index]->VarIndexToTableIndex(value_index);
      const BucketIndex next_bucket =
          table_->NextBucket(var_index, table_value_index, bucket);
      const BucketIndex q = std::max(support_bucket, next_bucket);
      if (q == bucket) {
        return bucket; // we immediately return bucket.
      }
      if (q < min_bucket) {
        min_bucket = q;
      }
    }
    return min_bucket == kint32max ? kNilBucket : min_bucket;
  }

  void AddToListSc(TableVar::Value* const var_value, TupleIndex tuple_index) {
    for(VarIndex i = VarIndex(0); i < arity_; ++i) {
      const TableValueIndex table_value_index =
          table_->ValueIndexFromPositionInTuple(tuple_index, i);
      const VarValueIndex value_index =
          vars_[i]->TableIndexToVarIndex(table_value_index);
      TableVar::Value* const var_value_i = vars_[i]->value(value_index);
      TableVar::Value* const ifirst = var_value_i->first_supported_tuple;
      if (ifirst != 0) {
        ifirst->prev_support_tuple[i] = var_value;
      }
      var_value->prev_support_tuple[i] = NULL;
      var_value->next_support_tuple[i] = var_value_i->first_supported_tuple;
      var_value_i->first_supported_tuple = var_value;
    }
  }

  // var_value is removed from every list listSC
  void InternalRemoveFromListSc(TableVar::Value* const var_value) {
    for(VarIndex i = VarIndex(0); i < arity_; ++i) {
      TableVar::Value* const next_var_value = var_value->next_support_tuple[i];
      if (next_var_value != 0) {
        next_var_value->prev_support_tuple[i] =
            var_value->prev_support_tuple[i];
      }
      TableVar::Value* const previous_var_value =
          var_value->prev_support_tuple[i];
      if (previous_var_value != 0) {
        previous_var_value->next_support_tuple[i] =
            var_value->next_support_tuple[i];
      } else {  // var_value is the first in the listSC of the value of var i
        const TableValueIndex table_value_index_i =
            table_->ValueIndexFromPositionInTuple(
                var_value->supporting_tuple_index, i);
        const VarValueIndex value_index =
            vars_[i]->TableIndexToVarIndex(table_value_index_i);
        vars_[i]->value(value_index)->first_supported_tuple =
            var_value->next_support_tuple[i];
      }
    }
  }

  // var_value is removed from every list listSC
  void RemoveFromListSc(TableVar::Value* const var_value) {
    SaveSupport(var_value->var_index, var_value->value_index);
    InternalRemoveFromListSc(var_value);
    var_value->supporting_tuple_index = kNilTuple;
  }

  void SaveSupport(VarIndex var_index, VarValueIndex value_index) {
    TableVar::Value* const var_value = vars_[var_index]->value(value_index);
    if (var_value->stamp < solver()->stamp()) {
      TupleIndex tuple_index = var_value->supporting_tuple_index;
      TableCtRestoreSupportAction* const action =
          solver()->RevAlloc(
              new TableCtRestoreSupportAction(
                  this, var_index, value_index, tuple_index));
      solver()->AddBacktrackAction(action, true);
      var_value->stamp = solver()->stamp();
    }
  }

  void RestoreSupport(VarIndex var_index,
                      VarValueIndex value_index,
                      TupleIndex tuple_index) {
    TableVar::Value* const var_value = vars_[var_index]->value(value_index);
    if (var_value->supporting_tuple_index != kNilTuple) {
      InternalRemoveFromListSc(var_value);
    }
    AddToListSc(var_value, tuple_index);
    var_value->supporting_tuple_index = tuple_index;
  }

  void SeekInitialSupport(VarIndex var_index) {
    IntVarIterator* const it = vars_[var_index]->domain_iterator();
    for(it->Init(); it->Ok(); it->Next()) { // We traverse the domain of x
      const int64 val = it->Value();
      // index of value in the domain of var
      const VarValueIndex value_index =
          vars_[var_index]->IndexFromValue(val);
      const TableValueIndex table_value_index =
          vars_[var_index]->VarIndexToTableIndex(value_index);
      // the value is also in the table
      if (table_value_index != kNilTableValue) {
        // we look at the value of next_bucket of 0 then we take the
        // first tuple in bucket
        TupleIndex tuple_index =
            table_->FirstTupleInBucket(
                var_index,
                table_value_index,
                table_->NextBucket(
                    var_index,
                    table_value_index,
                    BucketIndex(0)));
        vars_[var_index]->value(value_index)->supporting_tuple_index =
            tuple_index;
        AddToListSc(vars_[var_index]->value(value_index), tuple_index);
      } else { // the value is not in the table: we remove it from the variable
        vars_[var_index]->Var()->RemoveValue(val);
      }
    }
  }

  void SeekInitialSupport() {
    for (VarIndex i = VarIndex(0); i < arity_; ++i) {
      SeekInitialSupport(i);
    }
  }

  bool IsTupleValid(TupleIndex t) {
    DCHECK_NE(t, kNilTuple);
    for(VarIndex i = VarIndex(0); i < arity_; ++i) {
      const int64 val =
          table_->value(i, table_->ValueIndexFromPositionInTuple(t, i));
      if (!vars_[i]->InDomain(val)) {
        return false;
      }
    }
    return true;
  }

  TupleIndex SeekSupportInBucket(VarIndex var_index, TupleIndex tuple_index) {
    DCHECK(!IsTupleValid(tuple_index));
    const TupleIndex last_tuple_index =
        table_->LastTupleInBucket(table_->bucket(tuple_index));
    for(TupleIndex next_tuple_index =
            table_->NextTupleFromPosition(tuple_index, var_index);
        next_tuple_index <= last_tuple_index && next_tuple_index != kNilTuple;
        next_tuple_index = table_->NextTupleFromPosition(next_tuple_index,
                                                         var_index)) {
      if (IsTupleValid(next_tuple_index)) {
        return next_tuple_index;
      }
    }
    return kNilTuple;
  }

  BucketIndex SeekBucket(VarIndex var_index,
                         TableValueIndex ibt,
                         BucketIndex bucket,
                         Type type) {
    if (bucket >= table_->NumBuckets() || bucket == kNilBucket) {
      return kNilBucket;
    }
    // we select the desired algorithm
    switch(type) {
      case TABLECT_RESTART : return SeekBucketRestart(var_index, ibt, bucket);
      case TABLECT_CONTINUE : return SeekBucketContinue(var_index, ibt, bucket);
      case TABLECT_INVERSE : return SeekBucketInverse(var_index, ibt, bucket);
      case TABLECT_ORIGINAL : return SeekBucketOriginal(var_index, ibt, bucket);
    };
    return kNilBucket;
  }

  BucketIndex SeekBucketRestart(VarIndex var_index,
                                TableValueIndex ibt,
                                BucketIndex bucket) {
    BucketIndex next_bucket = bucket;
    VarIndex j = VarIndex(0);  // variable index
    while(j < arity_) {
      BucketIndex q = (ordered_x_[j] == var_index) ?
          table_->NextBucket(var_index, ibt, next_bucket) :
          SeekBucketForVar(ordered_x_[j], next_bucket);
      if (q == next_bucket) {
        j++;
      }  else {  // a progression occurs
        conflicts_[ordered_x_[j]]++;
        if (q == kNilBucket) {
          return kNilBucket;
        }
        q = table_->NextBucket(var_index, ibt, q);
        if (q == kNilBucket) {
          return kNilBucket;
        }
        next_bucket = q;
        j = 0;
      }
    }
    return next_bucket;
  }

  BucketIndex SeekBucketContinue(VarIndex var_index,
                                 TableValueIndex ibt,
                                 BucketIndex bucket) {
    // var var_index, ibt IndexFromValue in table, current bucket is bucket
    BucketIndex next_bucket = bucket;
    VarIndex j = VarIndex(0); // variable index
    while(j < arity_) {
      BucketIndex q = (ordered_x_[j] == var_index) ?
          table_->NextBucket(var_index, ibt, next_bucket) :
          SeekBucketForVar(ordered_x_[j], next_bucket);
      if (q > next_bucket) {  // a progression occurs
        if (q == kNilBucket) {
          return kNilBucket;
        }
        q = table_->NextBucket(var_index, ibt, q);
        if (q == kNilBucket) {
          return kNilBucket;
        }
        next_bucket = q;
      }
      j++;
    }
    return next_bucket;
  }

  BucketIndex SeekBucketInverse(VarIndex var_index,
                                TableValueIndex ibt,
                                BucketIndex bucket) {
    // var var_index, ibt IndexFromValue in table, current bucket is bucket
    BucketIndex next_bucket = bucket;
    VarIndex j = VarIndex(0); // variable index
    while (j < arity_) {
      BucketIndex q = (ordered_x_[j] == var_index) ?
          table_->NextBucket(var_index, ibt, next_bucket) :
          SeekBucketForVar(ordered_x_[j], next_bucket);
      if (q == next_bucket) {
        j++;
      }  else {  // a progression occurs
        if (q == kNilBucket) {
          return kNilBucket;
        }
        q = table_->NextBucket(var_index, ibt, q);
        if (q==kNilBucket) {
          return kNilBucket;
        }
        next_bucket = q;
        if (j > 0) {
          j--;
        }
      }
    }
    return next_bucket;
  }

  BucketIndex SeekBucketOriginal(VarIndex var_index,
                                 TableValueIndex ibt,
                                 BucketIndex bucket) {
    // var var_index, ibt IndexFromValue in table, current bucket is bucket
    BucketIndex nq = bucket;
    BucketIndex next_bucket;
    VarIndex j = VarIndex(0); // variable index
    do {
      next_bucket = nq;
      while (j < arity_) {
        const BucketIndex q =(ordered_x_[j] == var_index) ?
            table_->NextBucket(var_index, ibt, next_bucket) :
            SeekBucketForVar(ordered_x_[j], next_bucket);
        if (q == kNilBucket) {
          return kNilBucket;
        }
        j++;
      }
      nq = table_->NextBucket(var_index, ibt, next_bucket);
    } while (next_bucket < nq);
    return next_bucket;
  }

  // search a support for (x,a)
  TupleIndex SeekSupport(VarIndex var_index,
                         VarValueIndex value_index,
                         TupleIndex tuple_index,
                         Type type) {
    const TableValueIndex table_value_index =
        vars_[var_index]->VarIndexToTableIndex(value_index);
    TupleIndex current_tuple = tuple_index;
    while (current_tuple != kNilTuple) {
      TupleIndex next_tuple = SeekSupportInBucket(var_index, current_tuple);
      if (next_tuple != kNilTuple) {
        return next_tuple;
      }
      const BucketIndex bucket = SeekBucket(var_index,
                                            table_value_index,
                                            table_->bucket(current_tuple) + 1,
                                            type);
      if (bucket == kNilBucket) {
        break;
      }
      current_tuple = table_->FirstTupleInBucket(var_index,
                                                 table_value_index,
                                                 bucket);
      if (IsTupleValid(current_tuple)) {
        return current_tuple;
      }
    }
    return kNilTuple;
  }

  void DeleteVarValue(Type type, TableVar::Value* const var_value) {
    // first supported value
    TableVar::Value* support_value = var_value->first_supported_tuple;
    while (support_value != 0) {
      // support_value is removed from the supported list of values
      const TupleIndex old_support = support_value->supporting_tuple_index;
      RemoveFromListSc(support_value);
      // we check if support_value is valid
      VarIndex var_index = support_value->var_index;
      const VarValueIndex value_index = support_value->value_index;
      const int64 value = vars_[var_index]->ValueFromIndex(value_index);
      if (vars_[var_index]->InDomain(value)) {
        // support_value is valid. A new support must be sought
        const TupleIndex next_tuple =
            SeekSupport(var_index, value_index, old_support, type);
        if (next_tuple == kNilTuple) {
          // no more support: (y, b) is deleted
          vars_[var_index]->Var()->RemoveValue(value);
        } else {  // a new support is found
          vars_[var_index]->value(value_index)->supporting_tuple_index =
              next_tuple;
          AddToListSc(vars_[var_index]->value(value_index), next_tuple);
        }
      }
      support_value = var_value->first_supported_tuple;
    }
    var_value->deleted.Switch(solver());
  }

  void FilterX(int raw_var_index) {
    // the variable whose index is var_index is modified
    if (ordering_ == DOMAIN_MIN) {
      OrderX();
    } else if (ordering_ == CONFLICT_MAX) {
      OrderXConflicts();
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

    VarIndex var_index = VarIndex(raw_var_index);

    const TableVar* const xv = vars_[var_index];
    IntVar* const var = xv->Var();
    // First iteration: from oldmin to min
    const int64 oldmindomain = var->OldMin();
    const int64 mindomain = var->Min();
    for(int64 val = oldmindomain; val < mindomain; ++val) {
      const VarValueIndex value_index = xv->IndexFromValue(val);
      const TableValueIndex table_value_index =
          xv->VarIndexToTableIndex(value_index);
      if (table_value_index != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const var_value = xv->value(value_index);
        if (!var_value->deleted.Switched()) {
          // the value deletion has never been considered
          DeleteVarValue(type_, var_value);
        }
      }
    }

    // Second iteration: "delta" domain iteration
    IntVarIterator* const it = xv->DeltaDomainIterator();
    for(it->Init(); it->Ok(); it->Next()) {
      int64 val = it->Value();
      const VarValueIndex value_index = xv->IndexFromValue(val);
      const TableValueIndex table_value_index =
          xv->VarIndexToTableIndex(value_index);
      if (table_value_index != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const var_value = xv->value(value_index);
        DeleteVarValue(type_, var_value);
      }
    }

    // Third iteration: from max to oldmax
    const int64 oldmaxdomain = var->OldMax();
    const int64 maxdomain = var->Max();
    for(int64 val = maxdomain + 1; val <= oldmaxdomain; ++val) {
      const VarValueIndex value_index = xv->IndexFromValue(val);
      const TableValueIndex table_value_index =
          xv->VarIndexToTableIndex(value_index);
      if (table_value_index != -1) {
        // the index is in the TableVar::Value array
        TableVar::Value* const var_value = xv->value(value_index);
        if (!var_value->deleted.Switched()) {
          // the value deletion has never been considered
          DeleteVarValue(type_, var_value);
        }
      }
    }
  }

 private:
  BtTable* const table_;
  // order of the var array
  ITIVector<VarIndex, VarIndex> ordered_x_;
  // position in the order of var array
  ITIVector<VarIndex, VarIndex> in_order_x_;
  // number of conflicts
  ITIVector<VarIndex, int> conflicts_;
  // variable of the constraint
  ITIVector<VarIndex, TableVar*> vars_;
  const VarIndex arity_; // number of variables
  const Ordering ordering_;
  const Type type_;
};

void TableCtRestoreSupportAction::Run(Solver* const solver) {
  ct_->RestoreSupport(var_index_, value_index_, supporting_tuple_index_);
}
}  // namespace

// External API.

Constraint* BuildTableCt(Solver* const solver,
                         const IntTupleSet& tuples,
                         const std::vector<IntVar*>& vars,
                         int size_bucket) {
  const int num_tuples = tuples.NumTuples();
  const int arity = vars.size();
  BtTable* const table = new BtTable(VarIndex(arity), num_tuples, size_bucket);
  std::vector<int64> one_tuple(arity);
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < vars.size(); ++j) {
      one_tuple[j] = tuples.Value(i, j);
    }
    table->AddTuple(one_tuple);
  }
  table->CreateBuckets();
  TableCt::Type type = static_cast<TableCt::Type>(FLAGS_cp_bucket_table_type);
  TableCt::Ordering order =
      static_cast<TableCt::Ordering>(FLAGS_cp_bucket_table_ordering);
  return solver->RevAlloc(new TableCt(solver, table, vars, order, type));
}
} // namespace operations_research
