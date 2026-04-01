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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_EXPR_ARRAY_H_
#define ORTOOLS_CONSTRAINT_SOLVER_EXPR_ARRAY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/constraint_solver/visitor.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

// ----- TreeArrayConstraint -----

class TreeArrayConstraint : public CastConstraint {
 public:
  TreeArrayConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                      IntVar* sum_var);
  ~TreeArrayConstraint() override;

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 protected:
  std::string DebugStringInternal(absl::string_view name) const;
  void AcceptInternal(const std::string& name, ModelVisitor* visitor) const;
  void ReduceRange(int depth, int position, int64_t delta_min,
                   int64_t delta_max);
  void SetRange(int depth, int position, int64_t new_min, int64_t new_max);
  void InitLeaf(int position, int64_t var_min, int64_t var_max);
  void InitNode(int depth, int position, int64_t node_min, int64_t node_max);
  int64_t Min(int depth, int position) const;
  int64_t Max(int depth, int position) const;
  int64_t RootMin() const;
  int64_t RootMax() const;
  int Parent(int position) const;
  int ChildStart(int position) const;
  int ChildEnd(int depth, int position) const;
  bool IsLeaf(int depth) const;
  int MaxDepth() const;
  int Width(int depth) const;

  const std::vector<IntVar*> vars_;

 private:
  struct NodeInfo {
    NodeInfo() : node_min(0), node_max(0) {}
    Rev<int64_t> node_min;
    Rev<int64_t> node_max;
  };

  std::vector<std::vector<NodeInfo>> tree_;
  const int block_size_;
  NodeInfo* root_node_;
};

// ---------- Sum Array ----------

// Some of these optimizations here are described in:
// "Bounds consistency techniques for long linear constraints".  In
// Workshop on Techniques for Implementing Constraint Programming
// Systems (TRICS), a workshop of CP 2002, N. Beldiceanu, W. Harvey,
// Martin Henz, Francois Laburthe, Eric Monfroy, Tobias Müller,
// Laurent Perron and Christian Schulte editors, pages 39-46, 2002.

// ----- SumConstraint -----

// This constraint implements sum(vars) == sum_var.
class SumConstraint : public TreeArrayConstraint {
 public:
  SumConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                IntVar* sum_var);
  ~SumConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void SumChanged();
  void PushDown(int depth, int position, int64_t new_min, int64_t new_max);
  void LeafChanged(int term_index);
  void PushUp(int position, int64_t delta_min, int64_t delta_max);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  Demon* sum_demon_;
};

// ----- SmallSumConstraint -----

// This constraint implements sum(vars) == target_var.
class SmallSumConstraint : public Constraint {
 public:
  SmallSumConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                     IntVar* target_var);
  ~SmallSumConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void SumChanged();
  void VarChanged(IntVar* var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
  IntVar* target_var_;
  NumericalRev<int64_t> computed_min_;
  NumericalRev<int64_t> computed_max_;
  Demon* sum_demon_;
};

// ----- SafeSumConstraint -----

// This constraint implements sum(vars) == sum_var.
class SafeSumConstraint : public TreeArrayConstraint {
 public:
  SafeSumConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                    IntVar* sum_var);
  ~SafeSumConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void SafeComputeNode(int depth, int position, int64_t* sum_min,
                       int64_t* sum_max);
  void SumChanged();
  void PushDown(int depth, int position, int64_t new_min, int64_t new_max);
  void LeafChanged(int term_index);
  void PushUp(int position, int64_t delta_min, int64_t delta_max);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  bool CheckInternalState();
  void CheckLeaf(int position, int64_t var_min, int64_t var_max);
  void CheckNode(int depth, int position, int64_t node_min, int64_t node_max);

  Demon* sum_demon_;
};

// ---------- Min Array ----------

// ----- MinConstraint -----

// This constraint implements min(vars) == min_var.
class MinConstraint : public TreeArrayConstraint {
 public:
  MinConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                IntVar* min_var);
  ~MinConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void MinVarChanged();
  void PushDown(int depth, int position, int64_t new_min, int64_t new_max);
  void LeafChanged(int term_index);
  void PushUp(int position);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  Demon* min_demon_;
};

// ----- SmallMinConstraint -----

class SmallMinConstraint : public Constraint {
 public:
  SmallMinConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                     IntVar* target_var);
  ~SmallMinConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void MinVarChanged();
  void VarChanged(IntVar* var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  IntVar* const target_var_;
  Rev<int64_t> computed_min_;
  Rev<int64_t> computed_max_;
};

// ---------- Max Array ----------

// ----- MaxConstraint -----

// This constraint implements max(vars) == max_var.
class MaxConstraint : public TreeArrayConstraint {
 public:
  MaxConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                IntVar* max_var);
  ~MaxConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void MaxVarChanged();
  void PushDown(int depth, int position, int64_t new_min, int64_t new_max);
  void LeafChanged(int term_index);
  void PushUp(int position);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  Demon* max_demon_;
};

// ----- SmallMaxConstraint -----

class SmallMaxConstraint : public Constraint {
 public:
  SmallMaxConstraint(Solver* solver, const std::vector<IntVar*>& vars,
                     IntVar* target_var);
  ~SmallMaxConstraint() override;

  void Post() override;
  void InitialPropagate() override;

  void MaxVarChanged();
  void VarChanged(IntVar* var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  IntVar* const target_var_;
  Rev<int64_t> computed_min_;
  Rev<int64_t> computed_max_;
};

// ----- ArrayBoolAndEq -----

class ArrayBoolAndEq : public CastConstraint {
 public:
  ArrayBoolAndEq(Solver* solver, const std::vector<IntVar*>& vars,
                 IntVar* target_var);
  ~ArrayBoolAndEq() override;

  void Post() override;
  void InitialPropagate() override;

  void PropagateVar(IntVar* var);
  void PropagateTarget();

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  void InhibitAll();
  void ForceToZero();

  const std::vector<IntVar*> vars_;
  std::vector<Demon*> demons_;
  NumericalRev<int> unbounded_;
  RevSwitch decided_;
};

// ----- ArrayBoolOrEq -----

class ArrayBoolOrEq : public CastConstraint {
 public:
  ArrayBoolOrEq(Solver* solver, const std::vector<IntVar*>& vars,
                IntVar* target_var);
  ~ArrayBoolOrEq() override;

  void Post() override;
  void InitialPropagate() override;

  void PropagateVar(IntVar* var);
  void PropagateTarget();

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  void InhibitAll();
  void ForceToOne();

  const std::vector<IntVar*> vars_;
  std::vector<Demon*> demons_;
  NumericalRev<int> unbounded_;
  RevSwitch decided_;
};

// ----- BaseSumBooleanConstraint -----

class BaseSumBooleanConstraint : public Constraint {
 public:
  BaseSumBooleanConstraint(Solver* solver, const std::vector<IntVar*>& vars);
  ~BaseSumBooleanConstraint() override;

 protected:
  std::string DebugStringInternal(absl::string_view name) const;

  const std::vector<IntVar*> vars_;
  RevSwitch inactive_;
};

// ----- SumBooleanLessOrEqualToOne -----

class SumBooleanLessOrEqualToOne : public BaseSumBooleanConstraint {
 public:
  SumBooleanLessOrEqualToOne(Solver* solver, const std::vector<IntVar*>& vars);
  ~SumBooleanLessOrEqualToOne() override;

  void Post() override;
  void InitialPropagate() override;

  void Update(IntVar* var);
  void PushAllToZeroExcept(IntVar* var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;
};

// ----- SumBooleanGreaterOrEqualToOne -----

// We implement this one as a Max(array) == 1.
class SumBooleanGreaterOrEqualToOne : public BaseSumBooleanConstraint {
 public:
  SumBooleanGreaterOrEqualToOne(Solver* solver,
                                const std::vector<IntVar*>& vars);
  ~SumBooleanGreaterOrEqualToOne() override;

  void Post() override;
  void InitialPropagate() override;

  void Update(int index);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  RevBitSet bits_;
};

// ----- SumBooleanEqualToOne -----

class SumBooleanEqualToOne : public BaseSumBooleanConstraint {
 public:
  SumBooleanEqualToOne(Solver* solver, const std::vector<IntVar*>& vars);
  ~SumBooleanEqualToOne() override;

  void Post() override;
  void InitialPropagate() override;

  void Update(int index);
  void PushAllToZeroExcept(int index);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  NumericalRev<int> active_vars_;
};

// ----- SumBooleanEqualToVar -----

class SumBooleanEqualToVar : public BaseSumBooleanConstraint {
 public:
  SumBooleanEqualToVar(Solver* solver, const std::vector<IntVar*>& vars,
                       IntVar* sum_var);
  ~SumBooleanEqualToVar() override;

  void Post() override;
  void InitialPropagate() override;

  void Update(int index);
  void UpdateVar();
  void PushAllUnboundToZero();
  void PushAllUnboundToOne();

  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;

 private:
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
  IntVar* const sum_var_;
};

// ----- BooleanScalProdLessConstant -----

// This constraint implements sum(vars) == var.  It is delayed such
// that propagation only occurs when all variables have been touched.
class BooleanScalProdLessConstant : public Constraint {
 public:
  BooleanScalProdLessConstant(Solver* s, const std::vector<IntVar*>& vars,
                              const std::vector<int64_t>& coefs,
                              int64_t upper_bound);
  ~BooleanScalProdLessConstant() override;

  void Post() override;
  void InitialPropagate() override;

  void Update(int var_index);
  void PushFromTop();

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> coefs_;
  int64_t upper_bound_;
  Rev<int> first_unbound_backward_;
  Rev<int64_t> sum_of_bound_variables_;
  Rev<int64_t> max_coefficient_;
};

// ----- PositiveBooleanScalProdEqVar -----

class PositiveBooleanScalProdEqVar : public CastConstraint {
 public:
  PositiveBooleanScalProdEqVar(Solver* s, const std::vector<IntVar*>& vars,
                               const std::vector<int64_t>& coefs, IntVar* var);

  ~PositiveBooleanScalProdEqVar() override;

  void Post() override;
  void Propagate();
  void InitialPropagate() override;

  void Update(int var_index);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> coefs_;
  Rev<int> first_unbound_backward_;
  Rev<int64_t> sum_of_bound_variables_;
  Rev<int64_t> sum_of_all_variables_;
  Rev<int64_t> max_coefficient_;
};

// ----- PositiveBooleanScalProd -----

class PositiveBooleanScalProd : public BaseIntExpr {
 public:
  // this constructor will copy the array. The caller can safely delete the
  // exprs array himself
  PositiveBooleanScalProd(Solver* s, const std::vector<IntVar*>& vars,
                          const std::vector<int64_t>& coefs);
  ~PositiveBooleanScalProd() override;

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> coefs_;
};

// ----- PositiveBooleanScalProdEqCst ----- (all constants >= 0)

class PositiveBooleanScalProdEqCst : public Constraint {
 public:
  PositiveBooleanScalProdEqCst(Solver* s, const std::vector<IntVar*>& vars,
                               const std::vector<int64_t>& coefs,
                               int64_t constant);
  ~PositiveBooleanScalProdEqCst() override;

  void Post() override;
  void Propagate();
  void InitialPropagate() override;

  void Update(int var_index);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> coefs_;
  Rev<int> first_unbound_backward_;
  Rev<int64_t> sum_of_bound_variables_;
  Rev<int64_t> sum_of_all_variables_;
  int64_t constant_;
  Rev<int64_t> max_coefficient_;
};

// ----- Linearizer -----

class ExprLinearizer : public ModelParser {
 public:
  explicit ExprLinearizer(
      absl::flat_hash_map<IntVar*, int64_t>* variables_to_coefficients);
  ~ExprLinearizer() override;

  // Begin/End visit element.
  void BeginVisitModel(const std::string& solver_name) override;
  void EndVisitModel(const std::string& solver_name) override;
  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* constraint) override;
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* constraint) override;
  void BeginVisitExtension(const std::string& type) override;
  void EndVisitExtension(const std::string& type) override;
  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* expr) override;
  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* expr) override;
  void VisitIntegerVariable(const IntVar* variable,
                            const std::string& operation, int64_t value,
                            IntVar* delegate) override;
  void VisitIntegerVariable(const IntVar* variable, IntExpr* delegate) override;
  void VisitIntegerArgument(const std::string& arg_name,
                            int64_t value) override;
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64_t>& values) override;
  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override;
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* argument) override;
  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name,
      const std::vector<IntVar*>& arguments) override;
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* argument) override;
  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& argument) override;

  void Visit(const IntExpr* expr, int64_t multiplier);
  int64_t Constant() const;
  std::string DebugString() const override;

 private:
  void BeginVisit(bool active);
  void EndVisit();
  void VisitSubExpression(const IntExpr* cp_expr);
  void VisitSum(const IntExpr* cp_expr);
  void VisitScalProd(const IntExpr* cp_expr);
  void VisitDifference(const IntExpr* cp_expr);
  void VisitOpposite(const IntExpr* cp_expr);
  void VisitProduct(const IntExpr* cp_expr);
  void VisitTrace(const IntExpr* cp_expr);
  void VisitIntegerExpression(const IntExpr* cp_expr);
  void RegisterExpression(const IntExpr* expr, int64_t coef);
  void AddConstant(int64_t constant);
  void PushMultiplier(int64_t multiplier);
  void PopMultiplier();

  // We do need a IntVar* as key, and not const IntVar*, because clients of this
  // class typically iterate over the map keys and use them as mutable IntVar*.
  absl::flat_hash_map<IntVar*, int64_t>* variables_to_coefficients_;
  std::vector<int64_t> multipliers_;
  int64_t constant_;
};

// ----- Utils -----

bool DetectSumOverflow(const std::vector<IntVar*>& vars);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_EXPR_ARRAY_H_
