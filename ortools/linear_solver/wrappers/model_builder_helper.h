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

#ifndef OR_TOOLS_LINEAR_SOLVER_WRAPPERS_MODEL_BUILDER_HELPER_H_
#define OR_TOOLS_LINEAR_SOLVER_WRAPPERS_MODEL_BUILDER_HELPER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/fixed_array.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace mb {

// Base implementation of linear expressions.

class BoundedLinearExpression;
class FlatExpression;
class ExprVisitor;
class LinearExpr;
class ModelBuilderHelper;
class ModelSolverHelper;
class Variable;

// A linear expression that can be either integer or floating point.
class LinearExpr {
 public:
  virtual ~LinearExpr() = default;
  virtual void Visit(ExprVisitor& /*lin*/, double /*c*/) const = 0;
  virtual std::string ToString() const = 0;
  virtual std::string DebugString() const = 0;

  static LinearExpr* Term(LinearExpr* expr, double coeff);
  static LinearExpr* Affine(LinearExpr* expr, double coeff, double constant);
  static LinearExpr* AffineCst(double value, double coeff, double constant);
  static LinearExpr* Constant(double value);

  LinearExpr* Add(LinearExpr* expr);
  LinearExpr* AddFloat(double cst);
  LinearExpr* Sub(LinearExpr* expr);
  LinearExpr* SubFloat(double cst);
  LinearExpr* RSubFloat(double cst);
  LinearExpr* MulFloat(double cst);
  LinearExpr* Neg();

  BoundedLinearExpression* Eq(LinearExpr* rhs);
  BoundedLinearExpression* EqCst(double rhs);
  BoundedLinearExpression* Ge(LinearExpr* rhs);
  BoundedLinearExpression* GeCst(double rhs);
  BoundedLinearExpression* Le(LinearExpr* rhs);
  BoundedLinearExpression* LeCst(double rhs);
};

// Compare the indices of variables.
struct VariableComparator {
  bool operator()(const Variable* lhs, const Variable* rhs) const;
};

// A visitor class to parse a floating point linear expression.
class ExprVisitor {
 public:
  virtual ~ExprVisitor() = default;
  void AddToProcess(const LinearExpr* expr, double coeff);
  void AddConstant(double constant);
  virtual void AddVarCoeff(const Variable* var, double coeff) = 0;
  void Clear();

 protected:
  std::vector<std::pair<const LinearExpr*, double>> to_process_;
  double offset_ = 0;
};

class ExprFlattener : public ExprVisitor {
 public:
  ~ExprFlattener() override = default;
  void AddVarCoeff(const Variable* var, double coeff) override;
  double Flatten(std::vector<const Variable*>* vars,
                 std::vector<double>* coeffs);

 private:
  absl::btree_map<const Variable*, double, VariableComparator> canonical_terms_;
};

class ExprEvaluator : public ExprVisitor {
 public:
  explicit ExprEvaluator(ModelSolverHelper* helper) : helper_(helper) {}
  ~ExprEvaluator() override = default;
  void AddVarCoeff(const Variable* var, double coeff) override;
  double Evaluate();

 private:
  ModelSolverHelper* helper_;
};

// A flat linear expression sum(vars[i] * coeffs[i]) + offset
class FlatExpression : public LinearExpr {
 public:
  explicit FlatExpression(const LinearExpr* expr);
  // Flatten pos - neg.
  FlatExpression(const LinearExpr* pos, const LinearExpr* neg);
  FlatExpression(const std::vector<const Variable*>&,
                 const std::vector<double>&, double);
  explicit FlatExpression(double offset);
  const std::vector<const Variable*>& vars() const { return vars_; }
  std::vector<int> VarIndices() const;
  const std::vector<double>& coeffs() const { return coeffs_; }
  double offset() const { return offset_; }

  void Visit(ExprVisitor& lin, double c) const override;
  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  std::vector<const Variable*> vars_;
  std::vector<double> coeffs_;
  double offset_;
};

// A class to hold a sum of linear expressions, and optional integer and
// double offsets.
class SumArray : public LinearExpr {
 public:
  explicit SumArray(const std::vector<LinearExpr*>& exprs, double offset)
      : exprs_(exprs.begin(), exprs.end()), offset_(offset) {}
  ~SumArray() override = default;

  void Visit(ExprVisitor& lin, double c) const override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin.AddToProcess(exprs_[i], c);
    }
    if (offset_ != 0.0) {
      lin.AddConstant(offset_ * c);
    }
  }

  std::string ToString() const override {
    if (exprs_.empty()) {
      if (offset_ != 0.0) {
        return absl::StrCat(offset_);
      }
    }
    std::string s = "(";
    for (int i = 0; i < exprs_.size(); ++i) {
      if (i > 0) {
        absl::StrAppend(&s, " + ");
      }
      absl::StrAppend(&s, exprs_[i]->ToString());
    }
    if (offset_ != 0.0) {
      if (offset_ > 0.0) {
        absl::StrAppend(&s, " + ", offset_);
      } else {
        absl::StrAppend(&s, " - ", -offset_);
      }
    }
    absl::StrAppend(&s, ")");
    return s;
  }

  std::string DebugString() const override {
    std::string s = absl::StrCat(
        "SumArray(",
        absl::StrJoin(exprs_, ", ", [](std::string* out, LinearExpr* expr) {
          absl::StrAppend(out, expr->DebugString());
        }));
    if (offset_ != 0.0) {
      absl::StrAppend(&s, ", offset=", offset_);
    }
    absl::StrAppend(&s, ")");
    return s;
  }

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const double offset_;
};

// A class to hold a weighted sum of floating point linear expressions.
class WeightedSumArray : public LinearExpr {
 public:
  WeightedSumArray(const std::vector<LinearExpr*>& exprs,
                   const std::vector<double>& coeffs, double offset);
  ~WeightedSumArray() override = default;

  void Visit(ExprVisitor& lin, double c) const override;
  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const absl::FixedArray<double, 2> coeffs_;
  double offset_;
};

// A class to hold linear_expr * a = b.
class AffineExpr : public LinearExpr {
 public:
  AffineExpr(LinearExpr* expr, double coeff, double offset);
  ~AffineExpr() override = default;

  void Visit(ExprVisitor& lin, double c) const override;

  std::string ToString() const override;
  std::string DebugString() const override;

  LinearExpr* expression() const { return expr_; }
  double coefficient() const { return coeff_; }
  double offset() const { return offset_; }

 private:
  LinearExpr* expr_;
  double coeff_;
  double offset_;
};

// A class to hold a fixed value.
class FixedValue : public LinearExpr {
 public:
  explicit FixedValue(double value) : value_(value) {}
  ~FixedValue() override = default;

  void Visit(ExprVisitor& lin, double c) const override;

  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  double value_;
};

// A class to hold a variable index.
class Variable : public LinearExpr {
 public:
  Variable(ModelBuilderHelper* helper, int index);
  Variable(ModelBuilderHelper* helper, double lb, double ub, bool is_integral);
  Variable(ModelBuilderHelper* helper, double lb, double ub, bool is_integral,
           const std::string& name);
  Variable(ModelBuilderHelper* helper, int64_t lb, int64_t ub,
           bool is_integral);
  Variable(ModelBuilderHelper* helper, int64_t lb, int64_t ub, bool is_integral,
           const std::string& name);
  ~Variable() override {}

  ModelBuilderHelper* helper() const { return helper_; }
  int index() const { return index_; }
  std::string name() const;
  void SetName(const std::string& name);
  double lower_bounds() const;
  void SetLowerBound(double lb);
  double upper_bound() const;
  void SetUpperBound(double ub);
  bool is_integral() const;
  void SetIsIntegral(bool is_integral);
  double objective_coefficient() const;
  void SetObjectiveCoefficient(double coeff);

  void Visit(ExprVisitor& lin, double c) const override {
    lin.AddVarCoeff(this, c);
  }

  std::string ToString() const override;

  std::string DebugString() const override;

  bool operator<(const Variable& other) const { return index_ < other.index_; }

 protected:
  ModelBuilderHelper* helper_;
  int index_;
};

template <typename H>
H AbslHashValue(H h, const Variable* i) {
  return H::combine(std::move(h), i->index());
}

// A class to hold a linear expression with bounds.
class BoundedLinearExpression {
 public:
  BoundedLinearExpression(const LinearExpr* expr, double lower_bound,
                          double upper_bound);
  BoundedLinearExpression(const LinearExpr* pos, const LinearExpr* neg,
                          double lower_bound, double upper_bound);
  BoundedLinearExpression(const LinearExpr* expr, int64_t lower_bound,
                          int64_t upper_bound);
  BoundedLinearExpression(const LinearExpr* pos, const LinearExpr* neg,
                          int64_t lower_bound, int64_t upper_bound);

  ~BoundedLinearExpression() = default;

  double lower_bound() const;
  double upper_bound() const;
  const std::vector<const Variable*>& vars() const;
  const std::vector<double>& coeffs() const;
  std::string ToString() const;
  std::string DebugString() const;
  bool CastToBool(bool* result) const;

 private:
  std::vector<const Variable*> vars_;
  std::vector<double> coeffs_;
  double lower_bound_;
  double upper_bound_;
};

// The arguments of the functions defined below must follow these rules
// to be wrapped by SWIG correctly:
// 1) Their types must include the full operations_research::
//    namespace.
// 2) Their names must correspond to the ones declared in the .i
//    file (see the java/ and csharp/ subdirectories).

// Helper for importing/exporting models and model protobufs.
//
// Wrapping global function is brittle with SWIG. It is much easier to
// wrap static class methods.
//
// Note: all these methods rely on c++ code that uses absl::Status or
// absl::StatusOr. Unfortunately, these are inconsistently wrapped in non C++
// languages. As a consequence, we need to provide an API that does not involve
// absl::Status or absl::StatusOr.
class ModelBuilderHelper {
 public:
  void OverwriteModel(const ModelBuilderHelper& other_helper);
  std::string ExportToMpsString(const operations_research::MPModelExportOptions&
                                    options = MPModelExportOptions());
  std::string ExportToLpString(const operations_research::MPModelExportOptions&
                                   options = MPModelExportOptions());
  bool WriteToMpsFile(const std::string& filename,
                      const operations_research::MPModelExportOptions& options =
                          MPModelExportOptions());
  bool ReadModelFromProtoFile(const std::string& filename);
  bool WriteModelToProtoFile(const std::string& filename);

  bool ImportFromMpsString(const std::string& mps_string);
  bool ImportFromMpsFile(const std::string& mps_file);
#if defined(USE_LP_PARSER)
  bool ImportFromLpString(const std::string& lp_string);
  bool ImportFromLpFile(const std::string& lp_file);
#endif  // defined(USE_LP_PARSER)

  const MPModelProto& model() const;
  MPModelProto* mutable_model();

  // Direct low level model building API.
  int AddVar();
  void SetVarLowerBound(int var_index, double lb);
  void SetVarUpperBound(int var_index, double ub);
  void SetVarIntegrality(int var_index, bool is_integer);
  void SetVarObjectiveCoefficient(int var_index, double coeff);
  void SetVarName(int var_index, const std::string& name);

  double VarLowerBound(int var_index) const;
  double VarUpperBound(int var_index) const;
  bool VarIsIntegral(int var_index) const;
  double VarObjectiveCoefficient(int var_index) const;
  std::string VarName(int var_index) const;

  double ConstraintLowerBound(int ct_index) const;
  double ConstraintUpperBound(int ct_index) const;
  int AddLinearConstraint();
  std::string ConstraintName(int ct_index) const;
  std::vector<double> ConstraintCoefficients(int ct_index) const;
  std::vector<int> ConstraintVarIndices(int ct_index) const;
  void AddConstraintTerm(int ct_index, int var_index, double coeff);
  void ClearConstraintTerms(int ct_index);
  void SafeAddConstraintTerm(int ct_index, int var_index, double coeff);
  void SetConstraintCoefficient(int ct_index, int var_index, double coeff);
  void SetConstraintLowerBound(int ct_index, double lb);
  void SetConstraintName(int ct_index, const std::string& name);
  void SetConstraintUpperBound(int ct_index, double ub);

  bool EnforcedIndicatorValue(int ct_index) const;
  bool IsEnforcedConstraint(int ct_index) const;
  double EnforcedConstraintLowerBound(int ct_index) const;
  double EnforcedConstraintUpperBound(int ct_index) const;
  int AddEnforcedLinearConstraint();
  int EnforcedIndicatorVariableIndex(int ct_index) const;
  std::string EnforcedConstraintName(int ct_index) const;
  std::vector<double> EnforcedConstraintCoefficients(int ct_index) const;
  std::vector<int> EnforcedConstraintVarIndices(int ct_index) const;
  void AddEnforcedConstraintTerm(int ct_index, int var_index, double coeff);
  void ClearEnforcedConstraintTerms(int ct_index);
  void SafeAddEnforcedConstraintTerm(int ct_index, int var_index, double coeff);
  void SetEnforcedConstraintCoefficient(int ct_index, int var_index,
                                        double coeff);
  void SetEnforcedConstraintLowerBound(int ct_index, double lb);
  void SetEnforcedConstraintName(int ct_index, const std::string& name);
  void SetEnforcedConstraintUpperBound(int ct_index, double ub);
  void SetEnforcedIndicatorValue(int ct_index, bool positive);
  void SetEnforcedIndicatorVariableIndex(int ct_index, int var_index);

  int num_constraints() const;
  int num_variables() const;

  std::string name() const;
  void SetName(const std::string& name);

  void ClearObjective();
  bool maximize() const;
  void SetMaximize(bool maximize);
  double ObjectiveOffset() const;
  void SetObjectiveOffset(double offset);

  void ClearHints();
  void AddHint(int var_index, double var_value);

 private:
  MPModelProto model_;
};

// Simple director class for C#.
class MbLogCallback {
 public:
  virtual ~MbLogCallback() {}
  virtual void NewMessage(const std::string& message) = 0;
};

enum SolveStatus {
  OPTIMAL,
  FEASIBLE,
  INFEASIBLE,
  UNBOUNDED,
  ABNORMAL,
  NOT_SOLVED,
  MODEL_IS_VALID,
  CANCELLED_BY_USER,
  UNKNOWN_STATUS,
  MODEL_INVALID,
  INVALID_SOLVER_PARAMETERS,
  SOLVER_TYPE_UNAVAILABLE,
  INCOMPATIBLE_OPTIONS,
};

// Class used to solve a request. This class is not meant to be exposed to the
// public. Its responsibility is to bridge the MPModelProto in the non-C++
// languages with the C++ Solve method.
//
// It contains 2 helper objects: a logger, and an atomic bool to interrupt
// search.
class ModelSolverHelper {
 public:
  explicit ModelSolverHelper(const std::string& solver_name);
  bool SolverIsSupported() const;
  void Solve(const ModelBuilderHelper& model);

  // Only used by the CVXPY interface. Does not store the response internally.
  std::optional<MPSolutionResponse> SolveRequest(const MPModelRequest& request);

  // Returns true if the interrupt signal was correctly sent, that is if the
  // underlying solver supports it.
  bool InterruptSolve();

  void SetLogCallback(std::function<void(const std::string&)> log_callback);
  void SetLogCallbackFromDirectorClass(MbLogCallback* log_callback);
  void ClearLogCallback();

  bool has_response() const;
  bool has_solution() const;
  const MPSolutionResponse& response() const;
  SolveStatus status() const;

  // If not defined, or no solution, they will silently return 0.
  double objective_value() const;
  double best_objective_bound() const;
  double variable_value(int var_index) const;
  double expression_value(LinearExpr* expr) const;
  double reduced_cost(int var_index) const;
  double dual_value(int ct_index) const;
  double activity(int ct_index);

  std::string status_string() const;
  double wall_time() const;
  double user_time() const;

  // Solve parameters.
  void SetTimeLimitInSeconds(double limit);
  void SetSolverSpecificParameters(
      const std::string& solver_specific_parameters);
  void EnableOutput(bool enabled);

  // TODO(user): set parameters.

 private:
  SolveInterrupter interrupter_;
  std::atomic<bool> interrupt_solve_ = false;
  std::function<void(const std::string&)> log_callback_;
  std::optional<MPSolutionResponse> response_;
  std::optional<MPModelRequest::SolverType> solver_type_;
  std::optional<double> time_limit_in_second_;
  std::string solver_specific_parameters_;
  std::optional<const MPModelProto*> model_of_last_solve_;
  std::vector<double> activities_;
  bool solver_output_ = false;
  mutable ExprEvaluator evaluator_;
};

}  // namespace mb
}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_WRAPPERS_MODEL_BUILDER_HELPER_H_
