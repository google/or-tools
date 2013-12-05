// Copyright 2010-2013 Google
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

#include <string.h>
#include "base/hash.h"
#include "base/hash.h"
#include <string>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/string_array.h"

namespace operations_research {

// ---------- SmallRevBitSet ----------

SmallRevBitSet::SmallRevBitSet(int64 size) : bits_(0LL) {
  DCHECK_GT(size, 0);
  DCHECK_LE(size, 64);
}

void SmallRevBitSet::SetToOne(Solver* const solver, int64 pos) {
  DCHECK_GE(pos, 0);
  bits_.SetValue(solver, bits_.Value() | OneBit64(pos));
}

void SmallRevBitSet::SetToZero(Solver* const solver, int64 pos) {
  DCHECK_GE(pos, 0);
  bits_.SetValue(solver, bits_.Value() & ~OneBit64(pos));
}

int64 SmallRevBitSet::Cardinality() const { return BitCount64(bits_.Value()); }

int64 SmallRevBitSet::GetFirstOne() const {
  if (bits_.Value() == 0) {
    return -1;
  }
  return LeastSignificantBitPosition64(bits_.Value());
}

// ---------- RevBitSet ----------

RevBitSet::RevBitSet(int64 size)
    : size_(size),
      length_(BitLength64(size)),
      bits_(new uint64[length_]),
      stamps_(new uint64[length_]) {
  DCHECK_GE(size, 1);
  memset(bits_, 0, sizeof(*bits_) * length_);
  memset(stamps_, 0, sizeof(*stamps_) * length_);
}

RevBitSet::~RevBitSet() {
  delete[] bits_;
  delete[] stamps_;
}

void RevBitSet::Save(Solver* const solver, int offset) {
  const uint64 current_stamp = solver->stamp();
  if (current_stamp > stamps_[offset]) {
    stamps_[offset] = current_stamp;
    solver->SaveValue(&bits_[offset]);
  }
}

void RevBitSet::SetToOne(Solver* const solver, int64 index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  const int64 offset = BitOffset64(index);
  const int64 pos = BitPos64(index);
  if (!(bits_[offset] & OneBit64(pos))) {
    Save(solver, offset);
    bits_[offset] |= OneBit64(pos);
  }
}

void RevBitSet::SetToZero(Solver* const solver, int64 index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  const int64 offset = BitOffset64(index);
  const int64 pos = BitPos64(index);
  if (bits_[offset] & OneBit64(pos)) {
    Save(solver, offset);
    bits_[offset] &= ~OneBit64(pos);
  }
}

bool RevBitSet::IsSet(int64 index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  return IsBitSet64(bits_, index);
}

int64 RevBitSet::Cardinality() const {
  int64 card = 0;
  for (int i = 0; i < length_; ++i) {
    card += BitCount64(bits_[i]);
  }
  return card;
}

bool RevBitSet::IsCardinalityZero() const {
  for (int i = 0; i < length_; ++i) {
    if (bits_[i]) {
      return false;
    }
  }
  return true;
}

bool RevBitSet::IsCardinalityOne() const {
  bool found_one = false;
  for (int i = 0; i < length_; ++i) {
    const uint64 partial = bits_[i];
    if (partial) {
      if (!(partial & (partial - 1))) {
        if (found_one) {
          return false;
        }
        found_one = true;
      } else {
        return false;
      }
    }
  }
  return found_one;
}

int64 RevBitSet::GetFirstBit(int start) const {
  return LeastSignificantBitPosition64(bits_, start, size_ - 1);
}

void RevBitSet::ClearAll(Solver* const solver) {
  for (int offset = 0; offset < length_; ++offset) {
    if (bits_[offset]) {
      Save(solver, offset);
      bits_[offset] = GG_ULONGLONG(0);
    }
  }
}

// ----- RevBitMatrix -----

RevBitMatrix::RevBitMatrix(int64 rows, int64 columns)
    : RevBitSet(rows * columns), rows_(rows), columns_(columns) {
  DCHECK_GE(rows, 1);
  DCHECK_GE(columns, 1);
}

RevBitMatrix::~RevBitMatrix() {}

void RevBitMatrix::SetToOne(Solver* const solver, int64 row, int64 column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  RevBitSet::SetToOne(solver, row * columns_ + column);
}

void RevBitMatrix::SetToZero(Solver* const solver, int64 row, int64 column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  RevBitSet::SetToZero(solver, row * columns_ + column);
}

int64 RevBitMatrix::Cardinality(int row) const {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  const int start = row * columns_;
  return BitCountRange64(bits_, start, start + columns_ - 1);
}

bool RevBitMatrix::IsCardinalityOne(int row) const {
  // TODO(user) : Optimize this one.
  return Cardinality(row) == 1;
}

bool RevBitMatrix::IsCardinalityZero(int row) const {
  const int start = row * columns_;
  return IsEmptyRange64(bits_, start, start + columns_ - 1);
}

int64 RevBitMatrix::GetFirstBit(int row, int start) const {
  DCHECK_GE(start, 0);
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_LT(start, columns_);
  const int beginning = row * columns_;
  const int end = beginning + columns_ - 1;
  int64 position = LeastSignificantBitPosition64(bits_, beginning + start, end);
  if (position == -1) {
    return -1;
  } else {
    return position - beginning;
  }
}

void RevBitMatrix::ClearAll(Solver* const solver) {
  RevBitSet::ClearAll(solver);
}

// ----- PrintModelVisitor -----

namespace {
class PrintModelVisitor : public ModelVisitor {
 public:
  PrintModelVisitor() : indent_(0) {}
  virtual ~PrintModelVisitor() {}

  // Header/footers.
  virtual void BeginVisitModel(const string& solver_name) {
    LOG(INFO) << "Model " << solver_name << " {";
    Increase();
  }

  virtual void EndVisitModel(const string& solver_name) {
    LOG(INFO) << "}";
    Decrease();
    CHECK_EQ(0, indent_);
  }

  virtual void BeginVisitConstraint(const string& type_name,
                                    const Constraint* const constraint) {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  virtual void EndVisitConstraint(const string& type_name,
                                  const Constraint* const constraint) {
    Decrease();
  }

  virtual void BeginVisitIntegerExpression(const string& type_name,
                                           const IntExpr* const expr) {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  virtual void EndVisitIntegerExpression(const string& type_name,
                                         const IntExpr* const expr) {
    Decrease();
  }

  virtual void BeginVisitExtension(const string& type_name) {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  virtual void EndVisitExtension(const string& type_name) { Decrease(); }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    IntExpr* const delegate) {
    if (delegate != nullptr) {
      delegate->Accept(this);
    } else {
      if (variable->Bound() && variable->name().empty()) {
        LOG(INFO) << Spaces() << variable->Min();
      } else {
        LOG(INFO) << Spaces() << variable->DebugString();
      }
    }
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const string& operation, int64 value,
                                    IntVar* const delegate) {
    LOG(INFO) << Spaces() << "IntVar";
    Increase();
    LOG(INFO) << Spaces() << value;
    LOG(INFO) << Spaces() << operation;
    delegate->Accept(this);
    Decrease();
  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string& operation, int64 value,
                                     IntervalVar* const delegate) {
    if (delegate != nullptr) {
      LOG(INFO) << Spaces() << operation << " <" << value << ", ";
      Increase();
      delegate->Accept(this);
      Decrease();
      LOG(INFO) << Spaces() << ">";
    } else {
      LOG(INFO) << Spaces() << variable->DebugString();
    }
  }

  virtual void VisitSequenceVariable(const SequenceVar* const sequence) {
    LOG(INFO) << Spaces() << sequence->DebugString();
  }

  // Variables.
  virtual void VisitIntegerArgument(const string& arg_name, int64 value) {
    LOG(INFO) << Spaces() << arg_name << ": " << value;
  }

  virtual void VisitIntegerArrayArgument(const string& arg_name,
                                         const std::vector<int64>& values) {
    LOG(INFO) << Spaces() << arg_name << ": ["
              << IntVectorToString(values, ", ") << "]";
  }

  virtual void VisitIntegerMatrixArgument(const string& arg_name,
                                          const IntTupleSet& values) {
    const int rows = values.NumTuples();
    const int columns = values.Arity();
    string array = "[";
    for (int i = 0; i < rows; ++i) {
      if (i != 0) {
        array.append(", ");
      }
      array.append("[");
      for (int j = 0; j < columns; ++j) {
        if (j != 0) {
          array.append(", ");
        }
        StringAppendF(&array, "%lld", values.Value(i, j));
      }
      array.append("]");
    }
    array.append("]");
    LOG(INFO) << Spaces() << arg_name << ": " << array;
  }

  virtual void VisitIntegerExpressionArgument(const string& arg_name,
                                              IntExpr* const argument) {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name, const std::vector<IntVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     IntervalVar* const argument) {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitIntervalArgumentArray(
      const string& arg_name, const std::vector<IntervalVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit sequence argument.
  virtual void VisitSequenceArgument(const string& arg_name,
                                     SequenceVar* const argument) {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitSequenceArgumentArray(
      const string& arg_name, const std::vector<SequenceVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  virtual string DebugString() const { return "PrintModelVisitor"; }

 private:
  void Increase() { indent_ += 2; }

  void Decrease() { indent_ -= 2; }

  string Spaces() {
    string result;
    for (int i = 0; i < indent_ - 2 * (!prefix_.empty()); ++i) {
      result.append(" ");
    }
    if (!prefix_.empty()) {
      result.append(prefix_);
      prefix_ = "";
    }
    return result;
  }

  void set_prefix(const string& prefix) { prefix_ = prefix; }

  int indent_;
  string prefix_;
};

// ---------- ModelStatisticsVisitor -----------

class ModelStatisticsVisitor : public ModelVisitor {
 public:
  ModelStatisticsVisitor()
      : num_constraints_(0),
        num_variables_(0),
        num_expressions_(0),
        num_casts_(0),
        num_intervals_(0),
        num_sequences_(0),
        num_extensions_(0) {}

  virtual ~ModelStatisticsVisitor() {}

  // Begin/End visit element.
  virtual void BeginVisitModel(const string& solver_name) {
    // Reset statistics.
    num_constraints_ = 0;
    num_variables_ = 0;
    num_expressions_ = 0;
    num_casts_ = 0;
    num_intervals_ = 0;
    num_sequences_ = 0;
    num_extensions_ = 0;
    already_visited_.clear();
    constraint_types_.clear();
    expression_types_.clear();
    extension_types_.clear();
  }

  virtual void EndVisitModel(const string& solver_name) {
    // Display statistics.
    LOG(INFO) << "Model has:";
    LOG(INFO) << "  - " << num_constraints_ << " constraints.";
    for (ConstIter<hash_map<string, int> > it(constraint_types_); !it.at_end();
         ++it) {
      LOG(INFO) << "    * " << it->second << " " << it->first;
    }
    LOG(INFO) << "  - " << num_variables_ << " integer variables.";
    LOG(INFO) << "  - " << num_expressions_ << " integer expressions.";
    for (ConstIter<hash_map<string, int> > it(expression_types_); !it.at_end();
         ++it) {
      LOG(INFO) << "    * " << it->second << " " << it->first;
    }
    LOG(INFO) << "  - " << num_casts_ << " expressions casted into variables.";
    LOG(INFO) << "  - " << num_intervals_ << " interval variables.";
    LOG(INFO) << "  - " << num_sequences_ << " sequence variables.";
    LOG(INFO) << "  - " << num_extensions_ << " model extensions.";
    for (ConstIter<hash_map<string, int> > it(extension_types_); !it.at_end();
         ++it) {
      LOG(INFO) << "    * " << it->second << " " << it->first;
    }
  }

  virtual void BeginVisitConstraint(const string& type_name,
                                    const Constraint* const constraint) {
    num_constraints_++;
    AddConstraintType(type_name);
  }

  virtual void BeginVisitIntegerExpression(const string& type_name,
                                           const IntExpr* const expr) {
    AddExpressionType(type_name);
    num_expressions_++;
  }

  virtual void BeginVisitExtension(const string& type_name) {
    AddExtensionType(type_name);
    num_extensions_++;
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    IntExpr* const delegate) {
    num_variables_++;
    Register(variable);
    if (delegate) {
      num_casts_++;
      VisitSubArgument(delegate);
    }
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const string& operation, int64 value,
                                    IntVar* const delegate) {
    num_variables_++;
    Register(variable);
    num_casts_++;
    VisitSubArgument(delegate);
  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string& operation, int64 value,
                                     IntervalVar* const delegate) {
    num_intervals_++;
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  virtual void VisitSequenceVariable(const SequenceVar* const sequence) {
    num_sequences_++;
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  virtual void VisitIntegerExpressionArgument(const string& arg_name,
                                              IntExpr* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name, const std::vector<IntVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     IntervalVar* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitIntervalArrayArgument(
      const string& arg_name, const std::vector<IntervalVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  virtual void VisitSequenceArgument(const string& arg_name,
                                     SequenceVar* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitSequenceArrayArgument(
      const string& arg_name, const std::vector<SequenceVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  virtual string DebugString() const { return "ModelStatisticsVisitor"; }

 private:
  void Register(const BaseObject* const object) {
    already_visited_.insert(object);
  }

  bool AlreadyVisited(const BaseObject* const object) {
    return ContainsKey(already_visited_, object);
  }

  // T should derive from BaseObject
  template <typename T>
  void VisitSubArgument(T* object) {
    if (!AlreadyVisited(object)) {
      Register(object);
      object->Accept(this);
    }
  }

  void AddConstraintType(const string& constraint_type) {
    constraint_types_[constraint_type]++;
  }

  void AddExpressionType(const string& expression_type) {
    expression_types_[expression_type]++;
  }

  void AddExtensionType(const string& extension_type) {
    extension_types_[extension_type]++;
  }

  hash_map<string, int> constraint_types_;
  hash_map<string, int> expression_types_;
  hash_map<string, int> extension_types_;
  int num_constraints_;
  int num_variables_;
  int num_expressions_;
  int num_casts_;
  int num_intervals_;
  int num_sequences_;
  int num_extensions_;
  hash_set<const BaseObject*> already_visited_;
};

// ---------- Variable Degree Visitor ---------

class VariableDegreeVisitor : public ModelVisitor {
 public:
  VariableDegreeVisitor(hash_map<const IntVar*, int>* const map) : map_(map) {}

  virtual ~VariableDegreeVisitor() {}

  // Begin/End visit element.
  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    IntExpr* const delegate) {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (ContainsKey(*map_, var)) {
      (*map_)[var]++;
    }
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const string& operation, int64 value,
                                    IntVar* const delegate) {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (ContainsKey(*map_, var)) {
      (*map_)[var]++;
    }
    VisitSubArgument(delegate);
  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string& operation, int64 value,
                                     IntervalVar* const delegate) {
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  virtual void VisitSequenceVariable(const SequenceVar* const sequence) {
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  virtual void VisitIntegerExpressionArgument(const string& arg_name,
                                              IntExpr* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name, const std::vector<IntVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     IntervalVar* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitIntervalArrayArgument(
      const string& arg_name, const std::vector<IntervalVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  virtual void VisitSequenceArgument(const string& arg_name,
                                     SequenceVar* const argument) {
    VisitSubArgument(argument);
  }

  virtual void VisitSequenceArrayArgument(
      const string& arg_name, const std::vector<SequenceVar*>& arguments) {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  virtual string DebugString() const { return "VariableDegreeVisitor"; }

 private:
  // T should derive from BaseObject
  template <typename T>
  void VisitSubArgument(T* object) {
    object->Accept(this);
  }

  hash_map<const IntVar*, int>* const map_;
};
}  // namespace

ModelVisitor* Solver::MakePrintModelVisitor() {
  return RevAlloc(new PrintModelVisitor);
}

ModelVisitor* Solver::MakeStatisticsModelVisitor() {
  return RevAlloc(new ModelStatisticsVisitor);
}

ModelVisitor* Solver::MakeVariableDegreeVisitor(
    hash_map<const IntVar*, int>* const map) {
  return RevAlloc(new VariableDegreeVisitor(map));
}

// ----- Vector manipulations -----

std::vector<int64> ToInt64Vector(const std::vector<int>& input) {
  std::vector<int64> result(input.size());
  for (int i = 0; i < input.size(); ++i) {
    result[i] = input[i];
  }
  return result;
}

std::vector<int64> SortedNoDuplicates(const std::vector<int64>& input) {
  std::vector<int64> result(input);
  std::sort(result.begin(), result.end());
  std::vector<int64>::iterator it = std::unique(result.begin(), result.end());
  result.resize(std::distance(result.begin(), it));
  return result;
}
}  // namespace operations_research
