// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_VARIABLES_H_
#define ORTOOLS_CONSTRAINT_SOLVER_VARIABLES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/base_export.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/utilities.h"

namespace operations_research {

class OR_DLL BooleanVar : public IntVar {
 public:
  static constexpr int kUnboundBooleanVarValue = 2;

  explicit BooleanVar(Solver* const s, const std::string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue) {}

  ~BooleanVar() override {}

  int64_t Min() const override { return (value_ == 1); }
  void SetMin(int64_t m) override;
  int64_t Max() const override { return (value_ != 0); }
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (value_ != kUnboundBooleanVarValue); }
  int64_t Value() const override {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override { WhenBound(d); }
  void WhenDomain(Demon* d) override { WhenBound(d); }
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  std::string DebugString() const override;
  int VarType() const override { return BOOLEAN_VAR; }

  IntVar* IsEqual(int64_t constant) override;
  IntVar* IsDifferent(int64_t constant) override;
  IntVar* IsGreaterOrEqual(int64_t constant) override;
  IntVar* IsLessOrEqual(int64_t constant) override;

  virtual void RestoreValue() = 0;
  std::string BaseName() const override { return "BooleanVar"; }

  int RawValue() const { return value_; }

 protected:
  int value_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
};

// ----- IntConst -----

class OR_DLL IntConst : public IntVar {
 public:
  IntConst(Solver* s, int64_t value, const std::string& name = "");
  ~IntConst() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override;
  void WhenDomain(Demon* d) override;
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  int64_t OldMin() const override;
  int64_t OldMax() const override;
  std::string DebugString() const override;
  int VarType() const override;
  IntVar* IsEqual(int64_t constant) override;
  IntVar* IsDifferent(int64_t constant) override;
  IntVar* IsGreaterOrEqual(int64_t constant) override;
  IntVar* IsLessOrEqual(int64_t constant) override;
  std::string name() const override;

 private:
  int64_t value_;
};

// ----- ConcreteBooleanVar -----
class OR_DLL ConcreteBooleanVar : public BooleanVar {
 public:
  class Handler : public Demon {
   public:
    explicit Handler(ConcreteBooleanVar* var);
    ~Handler() override;
    void Run(Solver* s) override;
    Solver::DemonPriority priority() const override;
    std::string DebugString() const override;

   private:
    ConcreteBooleanVar* const var_;
  };

  ConcreteBooleanVar(Solver* s, const std::string& name);
  ~ConcreteBooleanVar() override;

  void SetValue(int64_t v) override;
  void Process();
  int64_t OldMin() const override;
  int64_t OldMax() const override;
  void RestoreValue() override;

 private:
  Handler handler_;
};

// ----- DomainIntVar -----
// It Contains bounds and a bitset representation of possible values.
class OR_DLL DomainIntVar : public IntVar {
 public:
  class BitSetIterator;
  class BitSet;
  class QueueHandler : public Demon {
   public:
    explicit QueueHandler(DomainIntVar* var);
    ~QueueHandler() override;
    void Run(Solver* s) override;
    Solver::DemonPriority priority() const override;
    std::string DebugString() const override;

   private:
    DomainIntVar* const var_;
  };

  template <class T>
  class RevIntPtrMap;
  class BaseValueWatcher;
  class ValueWatcher;
  class DenseValueWatcher;
  class BaseUpperBoundWatcher;
  class UpperBoundWatcher;
  class DenseUpperBoundWatcher;

  DomainIntVar(Solver* s, int64_t vmin, int64_t vmax, const std::string& name);
  DomainIntVar(Solver* s, absl::Span<const int64_t> sorted_values,
               const std::string& name);
  ~DomainIntVar() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  int64_t Value() const override;
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  void CreateBits();
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override;
  void WhenDomain(Demon* d) override;

  IntVar* IsEqual(int64_t constant) override;
  Constraint* SetIsEqual(absl::Span<const int64_t> values,
                         const std::vector<IntVar*>& vars);
  IntVar* IsDifferent(int64_t constant) override;
  IntVar* IsGreaterOrEqual(int64_t constant) override;
  Constraint* SetIsGreaterOrEqual(absl::Span<const int64_t> values,
                                  const std::vector<IntVar*>& vars);
  IntVar* IsLessOrEqual(int64_t constant) override;

  void Process();
  void Push();
  void CleanInProcess();
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  int64_t OldMin() const override;
  int64_t OldMax() const override;

  std::string DebugString() const override;
  BitSet* bitset() const;
  int VarType() const override;
  std::string BaseName() const override;

 private:
  friend class PlusCstDomainIntVar;
  friend class LinkExprAndDomainIntVar;

  void CheckOldMin();
  void CheckOldMax();

  Rev<int64_t> min_;
  Rev<int64_t> max_;
  int64_t old_min_;
  int64_t old_max_;
  int64_t new_min_;
  int64_t new_max_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> range_demons_;
  SimpleRevFIFO<Demon*> domain_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
  SimpleRevFIFO<Demon*> delayed_range_demons_;
  SimpleRevFIFO<Demon*> delayed_domain_demons_;
  QueueHandler handler_;
  bool in_process_;
  BitSet* bits_;
  BaseValueWatcher* value_watcher_;
  BaseUpperBoundWatcher* bound_watcher_;
};

// Utility functions to link expressions and variables.

/// Returns true if var represents a product of a variable and a
/// constant.  In that case, it fills inner_var and coefficient with
/// these, and returns true. In the other case, it returns false.
bool IsVarProduct(IntExpr* expr, IntVar** inner_var, int64_t* coefficient);

void LinkVarExpr(Solver* s, IntExpr* expr, IntVar* var);
IntVar* NewVarPlusInt(Solver* s, IntVar* var, int64_t value);
IntVar* NewIntMinusVar(Solver* s, int64_t value, IntVar* var);
IntVar* NewVarTimesInt(Solver* s, IntVar* var, int64_t value);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_VARIABLES_H_
