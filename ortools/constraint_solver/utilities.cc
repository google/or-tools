// Copyright 2010-2014 Google
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


#include <unordered_map>
#include <unordered_set>
#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/bitset.h"

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

// ----- UnsortedNullableRevBitset -----

UnsortedNullableRevBitset::UnsortedNullableRevBitset(int bit_size)
    : bit_size_(bit_size),
      word_size_(BitLength64(bit_size)),
      bits_(word_size_, 0),
      active_words_(word_size_) {}

void UnsortedNullableRevBitset::Init(Solver* const solver,
                                     const std::vector<uint64>& mask) {
  CHECK_LE(mask.size(), word_size_);
  for (int i = 0; i < mask.size(); ++i) {
    if (mask[i]) {
      bits_.SetValue(solver, i, mask[i]);
      active_words_.Insert(solver, i);
    }
  }
}

bool UnsortedNullableRevBitset::RevSubtract(Solver* const solver,
                                            const std::vector<uint64>& mask) {
  bool changed = false;
  to_remove_.clear();
  for (int index : active_words_) {
    if (index < mask.size() && (bits_[index] & mask[index]) != 0) {
      changed = true;
      const uint64 result = bits_[index] & ~mask[index];
      bits_.SetValue(solver, index, result);
      if (result == 0) {
        to_remove_.push_back(index);
      }
    }
  }

  CleanUpActives(solver);
  return changed;
}

void UnsortedNullableRevBitset::CleanUpActives(Solver* const solver) {
  // We remove indices of null words in reverse order, as this may be a simpler
  // operations on the RevIntSet (no actual swap).
  for (int i = to_remove_.size() - 1; i >= 0; --i) {
    active_words_.Remove(solver, to_remove_[i]);
  }
}

bool UnsortedNullableRevBitset::RevAnd(Solver* const solver,
                                       const std::vector<uint64>& mask) {
  bool changed = false;
  to_remove_.clear();
  for (int index : active_words_) {
    if (index < mask.size()) {
      if ((bits_[index] & ~mask[index]) != 0) {
        changed = true;
        const uint64 result = bits_[index] & mask[index];
        bits_.SetValue(solver, index, result);
        if (result == 0) {
          to_remove_.push_back(index);
        }
      }
    } else {
      // Zero the word as the mask is implicitely null.
      changed = true;
      bits_.SetValue(solver, index, 0);
      to_remove_.push_back(index);
    }
  }
  CleanUpActives(solver);
  return changed;
}

bool UnsortedNullableRevBitset::Intersects(const std::vector<uint64>& mask,
                                           int* support_index) {
  DCHECK_GE(*support_index, 0);
  DCHECK_LT(*support_index, word_size_);
  if (mask[*support_index] & bits_[*support_index]) {
    return true;
  }
  for (int index : active_words_) {
    if (bits_[index] & mask[index]) {
      *support_index = index;
      return true;
    }
  }
  return false;
}

// ----- PrintModelVisitor -----

namespace {
class PrintModelVisitor : public ModelVisitor {
 public:
  PrintModelVisitor() : indent_(0) {}
  ~PrintModelVisitor() override {}

  // Header/footers.
  void BeginVisitModel(const std::string& solver_name) override {
    LOG(INFO) << "Model " << solver_name << " {";
    Increase();
  }

  void EndVisitModel(const std::string& solver_name) override {
    LOG(INFO) << "}";
    Decrease();
    CHECK_EQ(0, indent_);
  }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* const constraint) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    Decrease();
  }

  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* const expr) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* const expr) override {
    Decrease();
  }

  void BeginVisitExtension(const std::string& type_name) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitExtension(const std::string& type_name) override { Decrease(); }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
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

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    LOG(INFO) << Spaces() << "IntVar";
    Increase();
    LOG(INFO) << Spaces() << value;
    LOG(INFO) << Spaces() << operation;
    delegate->Accept(this);
    Decrease();
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
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

  void VisitSequenceVariable(const SequenceVar* const sequence) override {
    LOG(INFO) << Spaces() << sequence->DebugString();
  }

  // Variables.
  void VisitIntegerArgument(const std::string& arg_name, int64 value) override {
    LOG(INFO) << Spaces() << arg_name << ": " << value;
  }

  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override {
    LOG(INFO) << Spaces() << arg_name << ": [" << strings::Join(values, ", ")
              << "]";
  }

  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override {
    const int rows = values.NumTuples();
    const int columns = values.Arity();
    std::string array = "[";
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

  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitIntervalArgumentArray(
      const std::string& arg_name, const std::vector<IntervalVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* const argument) override {
    set_prefix(StringPrintf("%s: ", arg_name.c_str()));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitSequenceArgumentArray(
      const std::string& arg_name, const std::vector<SequenceVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  std::string DebugString() const override { return "PrintModelVisitor"; }

 private:
  void Increase() { indent_ += 2; }

  void Decrease() { indent_ -= 2; }

  std::string Spaces() {
    std::string result;
    for (int i = 0; i < indent_ - 2 * (!prefix_.empty()); ++i) {
      result.append(" ");
    }
    if (!prefix_.empty()) {
      result.append(prefix_);
      prefix_ = "";
    }
    return result;
  }

  void set_prefix(const std::string& prefix) { prefix_ = prefix; }

  int indent_;
  std::string prefix_;
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

  ~ModelStatisticsVisitor() override {}

  // Begin/End visit element.
  void BeginVisitModel(const std::string& solver_name) override {
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

  void EndVisitModel(const std::string& solver_name) override {
    // Display statistics.
    LOG(INFO) << "Model has:";
    LOG(INFO) << "  - " << num_constraints_ << " constraints.";
    for (const auto& it : constraint_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
    LOG(INFO) << "  - " << num_variables_ << " integer variables.";
    LOG(INFO) << "  - " << num_expressions_ << " integer expressions.";
    for (const auto& it : expression_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
    LOG(INFO) << "  - " << num_casts_ << " expressions casted into variables.";
    LOG(INFO) << "  - " << num_intervals_ << " interval variables.";
    LOG(INFO) << "  - " << num_sequences_ << " sequence variables.";
    LOG(INFO) << "  - " << num_extensions_ << " model extensions.";
    for (const auto& it : extension_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
  }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* const constraint) override {
    num_constraints_++;
    AddConstraintType(type_name);
  }

  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* const expr) override {
    AddExpressionType(type_name);
    num_expressions_++;
  }

  void BeginVisitExtension(const std::string& type_name) override {
    AddExtensionType(type_name);
    num_extensions_++;
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    num_variables_++;
    Register(variable);
    if (delegate) {
      num_casts_++;
      VisitSubArgument(delegate);
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    num_variables_++;
    Register(variable);
    num_casts_++;
    VisitSubArgument(delegate);
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
    num_intervals_++;
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitSequenceVariable(const SequenceVar* const sequence) override {
    num_sequences_++;
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  std::string DebugString() const override { return "ModelStatisticsVisitor"; }

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

  void AddConstraintType(const std::string& constraint_type) {
    constraint_types_[constraint_type]++;
  }

  void AddExpressionType(const std::string& expression_type) {
    expression_types_[expression_type]++;
  }

  void AddExtensionType(const std::string& extension_type) {
    extension_types_[extension_type]++;
  }

  std::unordered_map<std::string, int> constraint_types_;
  std::unordered_map<std::string, int> expression_types_;
  std::unordered_map<std::string, int> extension_types_;
  int num_constraints_;
  int num_variables_;
  int num_expressions_;
  int num_casts_;
  int num_intervals_;
  int num_sequences_;
  int num_extensions_;
  std::unordered_set<const BaseObject*> already_visited_;
};

// ---------- Variable Degree Visitor ---------

class VariableDegreeVisitor : public ModelVisitor {
 public:
  explicit VariableDegreeVisitor(std::unordered_map<const IntVar*, int>* const map)
      : map_(map) {}

  ~VariableDegreeVisitor() override {}

  // Begin/End visit element.
  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (ContainsKey(*map_, var)) {
      (*map_)[var]++;
    }
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (ContainsKey(*map_, var)) {
      (*map_)[var]++;
    }
    VisitSubArgument(delegate);
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitSequenceVariable(const SequenceVar* const sequence) override {
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  std::string DebugString() const override { return "VariableDegreeVisitor"; }

 private:
  // T should derive from BaseObject
  template <typename T>
  void VisitSubArgument(T* object) {
    object->Accept(this);
  }

  std::unordered_map<const IntVar*, int>* const map_;
};
}  // namespace

ModelVisitor* Solver::MakePrintModelVisitor() {
  return RevAlloc(new PrintModelVisitor);
}

ModelVisitor* Solver::MakeStatisticsModelVisitor() {
  return RevAlloc(new ModelStatisticsVisitor);
}

ModelVisitor* Solver::MakeVariableDegreeVisitor(
    std::unordered_map<const IntVar*, int>* const map) {
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
}  // namespace operations_research
