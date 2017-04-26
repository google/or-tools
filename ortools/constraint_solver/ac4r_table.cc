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

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/int_type.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/vector_map.h"

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
    Column() : num_tuples_(0) {}

    void Init(const IntTupleSet& table, int var_index) {
      num_tuples_ = table.NumTuples();
      column_of_value_indices_.resize(num_tuples_);
      num_tuples_per_value_.resize(table.NumDifferentValuesInColumn(var_index),
                                   0);
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

    int IndexFromValue(int64 value) const { return value_map_.Index(value); }

    int64 ValueFromIndex(int value_index) const {
      return value_map_.Element(value_index);
    }

    int NumTuplesContainingValueIndex(int value_index) const {
      return num_tuples_per_value_[value_index];
    }

    int NumTuples() const { return num_tuples_; }

    int NumDifferentValues() const { return num_tuples_per_value_.size(); }

   private:
    std::vector<int> column_of_value_indices_;
    VectorMap<int64> value_map_;
    std::vector<int> num_tuples_per_value_;
    int num_tuples_;
  };

  IndexedTable(const IntTupleSet& tuple_set)
      : tuple_set_(tuple_set),
        arity_(tuple_set.Arity()),
        num_tuples_(tuple_set.NumTuples()),
        columns_(arity_) {
    for (int i = 0; i < arity_; i++) {
      columns_[i].Init(tuple_set, i);
    }
  }

  ~IndexedTable() {}

  int NumVars() const { return arity_; }

  const Column& column(int var_index) const { return columns_[var_index]; }

  int NumTuples() const { return num_tuples_; }

  const IntTupleSet& tuple_set() const { return tuple_set_; }

 private:
  const IntTupleSet tuple_set_;
  const int arity_;
  const int num_tuples_;
  std::vector<Column> columns_;
};

class TableVar {
 public:
  TableVar(Solver* const solver, IntVar* var,
           const IndexedTable::Column& column)
      : solver_(solver),
        column_(column),
        tuples_per_value_(column.NumDifferentValues()),
        active_values_(column.NumDifferentValues()),
        var_(var),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        shared_positions_(new int[column.NumTuples()]) {
    for (int value_index = 0; value_index < tuples_per_value_.size();
         value_index++) {
      tuples_per_value_[value_index] =
          new RevIntSet<int>(column.NumTuplesContainingValueIndex(value_index),
                             shared_positions_.get(),
                             column.NumTuples());
      active_values_.Insert(solver_, value_index);
    }
  }

  ~TableVar() {
    STLDeleteElements(&tuples_per_value_);  // delete all elements of a vector
  }

  IntVar* Variable() const { return var_; }

  int NumTuplesPerValue(int value_index) const {
    return tuples_per_value_[value_index]->Size();
  }

  // Checks whether we should do reset or not.
  bool ShouldReset(const std::vector<int>& delta) {
    int num_deleted_tuples = 0;
    for (int k = 0; k < delta.size(); k++) {
      num_deleted_tuples += NumTuplesPerValue(delta[k]);
    }

    if (num_deleted_tuples < 10) {
      return false;
    }

    int num_remaining_tuples = 0;
    IntVarIterator* const it = domain_iterator_;
    for (it->Init(); it->Ok(); it->Next()) {
      const int value_index = column_.IndexFromValue(it->Value());
      num_remaining_tuples += NumTuplesPerValue(value_index);
    }
    return (num_remaining_tuples < num_deleted_tuples);
  }

  void InitialPropagate(const std::vector<int>& valid_tuples,
                        std::vector<int64>* const to_remove) {
    // Insert tuples in correct maps.
    for (int tuple_ref = 0; tuple_ref < valid_tuples.size(); ++tuple_ref) {
      const int tuple_index = valid_tuples[tuple_ref];
      const int value_index = column_.ValueIndex(tuple_index);
      tuples_per_value_[value_index]->Insert(solver_, tuple_index);
    }

    // we remove from the domain the values that are not in the table,
    // or that have no supporting tuples.
    to_remove->clear();
    IntVarIterator* const it = domain_iterator_;
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      const int index = column_.IndexFromValue(value);
      if (index == -1 || NumTuplesPerValue(index) == 0) {
        to_remove->push_back(value);
      }
    }
    var_->RemoveValues(*to_remove);
  }

  void ComputeDeltaDomain(std::vector<int>* delta) {
    delta->clear();
    // we iterate the delta of the variable
    //
    // ATTENTION code for or-tools: the delta iterator does not
    // include the values between oldmin and min and the values
    // between max and oldmax.
    //
    // therefore we decompose the iteration into 3 parts
    // - from oldmin to min
    // - for the deleted values between min and max
    // - from max to oldmax

    // First iteration: from old_min to min.
    const int64 old_min_domain = var_->OldMin();
    const int64 min_domain = var_->Min();
    const int64 max_domain = var_->Max();
    for (int64 val = old_min_domain; val < min_domain; ++val) {
      const int index = column_.IndexFromValue(val);
      if (index != -1) {  // -1 means not in column.
        delta->push_back(index);
      }
    }
    // Second iteration: "delta" domain iteration.
    IntVarIterator* const it = delta_domain_iterator_;
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      if (value > min_domain && value < max_domain) {
        const int index = column_.IndexFromValue(it->Value());
        if (index != -1) {  // -1 means not in column.
          delta->push_back(index);
        }
      }
    }
    // Third iteration: from max to old_max.
    const int64 old_max_domain = var_->OldMax();
    for (int64 val = max_domain + 1; val <= old_max_domain; ++val) {
      const int index = column_.IndexFromValue(val);
      if (index != -1) {  // -1 means not in column.
        delta->push_back(index);
      }
    }
  }

  void CollectTuplesToRemove(const std::vector<int>& delta,
                             std::vector<int>* const tuples_to_remove) {
    tuples_to_remove->clear();
    const int delta_size = delta.size();
    for (int k = 0; k < delta_size; k++) {
      RevIntSet<int>* const active_tuples = tuples_per_value_[delta[k]];
      const int num_tuples_to_erase = active_tuples->Size();
      for (int index = 0; index < num_tuples_to_erase; index++) {
        tuples_to_remove->push_back(active_tuples->Element(index));
      }
    }
  }

  void CollectTuplesToKeep(std::vector<int>* const tuples_to_keep) const {
    tuples_to_keep->clear();
    for (domain_iterator_->Init(); domain_iterator_->Ok();
         domain_iterator_->Next()) {
      const int value_index = column_.IndexFromValue(domain_iterator_->Value());
      RevIntSet<int>* const active_tuples = tuples_per_value_[value_index];
      const int num_tuples = active_tuples->Size();
      for (int j = 0; j < num_tuples; j++) {
        tuples_to_keep->push_back(active_tuples->Element(j));
      }
    }
  }

  void RemoveTuples(const std::vector<int>& tuples) {
    for (int i = 0; i < tuples.size(); ++i) {
      const int erased_tuple_index = tuples[i];
      const int value_index = column_.ValueIndex(erased_tuple_index);
      RevIntSet<int>* const active_tuples = tuples_per_value_[value_index];
      active_tuples->Remove(solver_, erased_tuple_index);
      if (active_tuples->Size() == 0) {
        var_->RemoveValue(column_.ValueFromIndex(value_index));
        active_values_.Remove(solver_, value_index);
      }
    }
  }

  void OverwriteTuples(const std::vector<int>& tuples) {
    for (int k = 0; k < active_values_.Size(); k++) {
      tuples_per_value_[active_values_.Element(k)]->Clear(solver_);
    }
    for (int i = 0; i < tuples.size(); ++i) {
      const int tuple_index = tuples[i];
      const int value_index = column_.ValueIndex(tuple_index);
      tuples_per_value_[value_index]->Restore(solver_, tuple_index);
    }

    // We check for unsupported values and remove them.
    int count = 0;
    for (int k = active_values_.Size() - 1; k >= 0; k--) {
      const int64 value_index = active_values_.Element(k);
      if (tuples_per_value_[value_index]->Size() == 0) {
        active_values_.Remove(solver_, value_index);
        count++;
      }
    }
    // Removed values have been inserted after the last active value.
    for (int k = 0; k < count; ++k) {
      const int64 value_index = active_values_.RemovedElement(k);
      var_->RemoveValue(column_.ValueFromIndex(value_index));
    }
  }

 private:
  Solver* const solver_;
  const IndexedTable::Column& column_;
  // one LAA per value of the variable
  std::vector<RevIntSet<int>*> tuples_per_value_;
  // list of values: having a non empty tuple list
  RevIntSet<int> active_values_;
  IntVar* const var_;
  IntVarIterator* const domain_iterator_;
  IntVarIterator* const delta_domain_iterator_;
  std::unique_ptr<int[]> shared_positions_;
};

class Ac4TableConstraint : public Constraint {
 public:
  Ac4TableConstraint(Solver* const solver, IndexedTable* const table,
                     bool delete_table, const std::vector<IntVar*>& vars)
      : Constraint(solver),
        original_vars_(vars),
        vars_(table->NumVars()),
        table_(table),
        delete_table_(delete_table),
        tmp_tuples_(table->NumTuples()),
        delta_of_value_indices_(table->NumTuples()),
        num_variables_(table->NumVars()) {
    for (int var_index = 0; var_index < table->NumVars(); var_index++) {
      vars_[var_index] =
          new TableVar(solver, vars[var_index], table->column(var_index));
    }
  }

  ~Ac4TableConstraint() {
    STLDeleteElements(&vars_);  // delete all elements of a vector
    if (delete_table_) {
      delete table_;
    }
  }

  void Post() {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      Demon* const demon = MakeConstraintDemon1(
          solver(), this, &Ac4TableConstraint::FilterOneVariable,
          "FilterOneVariable", var_index);
      vars_[var_index]->Variable()->WhenDomain(demon);
    }
  }

  bool IsTupleSupported(int tuple_index) {
    for (int var_index = 0; var_index < num_variables_; ++var_index) {
      if (!original_vars_[var_index]->Contains(
              table_->tuple_set().Value(tuple_index, var_index))) {
        return false;
      }
    }
    return true;
  }

  void InitialPropagate() {
    std::vector<int> valid_tuples;
    for (int i = 0; i < table_->NumTuples(); ++i) {
      if (IsTupleSupported(i)) {
        valid_tuples.push_back(i);
      }
    }
    if (valid_tuples.empty()) {
      solver()->Fail();
    }

    std::vector<int64> to_remove;
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->InitialPropagate(valid_tuples, &to_remove);
    }
  }

  void FilterOneVariable(int var_index) {
    TableVar* const var = vars_[var_index];
    var->ComputeDeltaDomain(&delta_of_value_indices_);
    // We decide if we prefer to restart with the remaining set of
    // tuples, or if we incrementaly remove unsupported tuples.
    if (var->ShouldReset(delta_of_value_indices_)) {
      var->CollectTuplesToKeep(&tmp_tuples_);
      for (int i = 0; i < num_variables_; i++) {
        vars_[i]->OverwriteTuples(tmp_tuples_);
      }
    } else {
      var->CollectTuplesToRemove(delta_of_value_indices_, &tmp_tuples_);
      for (int i = 0; i < num_variables_; ++i) {
        vars_[i]->RemoveTuples(tmp_tuples_);
      }
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("AllowedAssignments(arity = %d, tuple_count = %d)",
                        table_->NumVars(), table_->NumTuples());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAllowedAssignments, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               original_vars_);
    visitor->VisitIntegerMatrixArgument(ModelVisitor::kTuplesArgument,
                                        table_->tuple_set());
    visitor->EndVisitConstraint(ModelVisitor::kAllowedAssignments, this);
  }

 private:
  std::vector<IntVar*> original_vars_;
  // Variables of the constraint.
  std::vector<TableVar*> vars_;
  // Table.
  IndexedTable* const table_;
  const bool delete_table_;
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
                                    const IntTupleSet& tuples,
                                    const std::vector<IntVar*>& vars) {
  return solver->RevAlloc(
      new Ac4TableConstraint(solver, new IndexedTable(tuples), true, vars));
}

Constraint* BuildAc4TableConstraint(Solver* const solver,
                                    IndexedTable* const table,
                                    const std::vector<IntVar*>& vars) {
  return solver->RevAlloc(new Ac4TableConstraint(solver, table, true, vars));
}

IndexedTable* BuildIndexedTable(const IntTupleSet& tuples) {
  return new IndexedTable(tuples);
}
}  // namespace operations_research
