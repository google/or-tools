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

#include <Python.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/string_view_migration.h"
#include "ortools/port/proto_utils.h"  // IWYU: keep
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/python/linear_expr.h"
#include "ortools/sat/python/linear_expr_doc.h"
#include "ortools/sat/sat_parameters.pb.h"  // IWYU: keep
#include "ortools/sat/swig_helper.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/attr.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace operations_research::sat::python {

void ThrowError(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
}

// We extend the SolverWrapper class to keep track of the local error already
// set.
class ExtSolveWrapper : public SolveWrapper {
 public:
  mutable std::optional<py::error_already_set> local_error_already_set_;
};

// A trampoline class to override the OnSolutionCallback method to acquire the
// GIL.
class PySolutionCallback : public SolutionCallback {
 public:
  using SolutionCallback::SolutionCallback; /* Inherit constructors */
  void OnSolutionCallback() const override {
    ::py::gil_scoped_acquire acquire;
    try {
      PYBIND11_OVERRIDE_PURE(
          void,               /* Return type */
          SolutionCallback,   /* Parent class */
          OnSolutionCallback, /* Name of function */
          /* This function has no arguments. The trailing comma
             in the previous line is needed for some compilers */
      );
    } catch (py::error_already_set& e) {
      // We assume this code is serialized as the gil is held.
      ExtSolveWrapper* solve_wrapper = static_cast<ExtSolveWrapper*>(wrapper());
      if (!solve_wrapper->local_error_already_set_.has_value()) {
        solve_wrapper->local_error_already_set_ = e;
      }
      StopSearch();
    }
  }
};

class ResponseHelper {
 public:
  static bool BooleanValue(std::shared_ptr<CpSolverResponse> response,
                           std::shared_ptr<Literal> lit) {
    const int index = lit->index();
    if (index >= 0) {
      return response->solution(index) != 0;
    } else {
      return response->solution(NegatedRef(index)) == 0;
    }
  }

  static bool FixedBooleanValue(std::shared_ptr<CpSolverResponse> response,
                                bool lit) {
    return lit;
  }

  static std::vector<int> SufficientAssumptionsForInfeasibility(
      std::shared_ptr<CpSolverResponse> response) {
    return std::vector<int>(
        response->sufficient_assumptions_for_infeasibility().begin(),
        response->sufficient_assumptions_for_infeasibility().end());
  }

  static double FloatValue(std::shared_ptr<CpSolverResponse> response,
                           std::shared_ptr<LinearExpr> expr) {
    FloatExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    return visitor.Evaluate(*response);
  }

  static double FixedFloatValue(std::shared_ptr<CpSolverResponse> response,
                                double value) {
    return value;
  }

  static int64_t Value(std::shared_ptr<CpSolverResponse> response,
                       std::shared_ptr<LinearExpr> expr) {
    int64_t value;
    IntExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    if (!visitor.Evaluate(*response, &value)) {
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Failed to evaluate linear expression: ",
                              expr->DebugString()));
    }
    return value;
  }

  static int64_t FixedValue(std::shared_ptr<CpSolverResponse> response,
                            int64_t value) {
    return value;
  }
};

// Checks that the result is not null and throws an error if it is.
std::shared_ptr<BoundedLinearExpression> CheckBoundedLinearExpression(
    std::shared_ptr<BoundedLinearExpression> result,
    std::shared_ptr<LinearExpr> lhs,
    std::shared_ptr<LinearExpr> rhs = nullptr) {
  if (!result->ok()) {
    if (rhs == nullptr) {
      ThrowError(PyExc_TypeError,
                 absl::StrCat("Linear constraints only accept integer values "
                              "and coefficients: ",
                              lhs->DebugString()));
    } else {
      ThrowError(PyExc_TypeError,
                 absl::StrCat("Linear constraints only accept integer values "
                              "and coefficients: ",
                              lhs->DebugString(), " and ", rhs->DebugString()));
    }
  }
  return result;
}

void RaiseIfNone(std::shared_ptr<LinearExpr> expr) {
  if (expr == nullptr) {
    ThrowError(PyExc_TypeError,
               "Linear constraints do not accept None as argument.");
  }
}

void ProcessExprArg(
    const py::handle& arg,
    absl::AnyInvocable<void(std::shared_ptr<LinearExpr>)> on_linear_expr,
    absl::AnyInvocable<void(int64_t)> on_int_constant,
    absl::AnyInvocable<void(double)> on_float_constant) {
  if (py::isinstance<LinearExpr>(arg)) {
    on_linear_expr(arg.cast<std::shared_ptr<LinearExpr>>());
  } else if (py::isinstance<py::int_>(arg)) {
    on_int_constant(arg.cast<int64_t>());
  } else if (py::isinstance<py::float_>(arg)) {
    on_float_constant(arg.cast<double>());
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      on_int_constant(arg.cast<int64_t>());
    } else {
      on_float_constant(arg.cast<double>());
    }
  } else {
    py::type objtype = py::type::of(arg);
    const std::string type_name = objtype.attr("__name__").cast<std::string>();
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::sum() only accept linear "
                            "expressions and constants as argument: '",
                            absl::CEscape(type_name), "'"));
  }
}

void ProcessConstantArg(const py::handle& arg,
                        absl::AnyInvocable<void(int64_t)> on_int_constant,
                        absl::AnyInvocable<void(double)> on_float_constant) {
  if (py::isinstance<py::int_>(arg)) {
    on_int_constant(arg.cast<int64_t>());
  } else if (py::isinstance<py::float_>(arg)) {
    on_float_constant(arg.cast<double>());
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      on_int_constant(arg.cast<int64_t>());
    } else {
      on_float_constant(arg.cast<double>());
    }
  } else {
    py::type objtype = py::type::of(arg);
    const std::string type_name = objtype.attr("__name__").cast<std::string>();
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::weighted_sum() only accept constants "
                            "as coefficients: '",
                            absl::CEscape(type_name), "'"));
  }
}

std::shared_ptr<LinearExpr> SumArguments(py::args expressions) {
  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  const auto process_arg = [&](const py::handle& arg) -> void {
    ProcessExprArg(
        arg,
        [&](std::shared_ptr<LinearExpr> expr) { linear_exprs.push_back(expr); },
        [&](int64_t value) { int_offset += value; },
        [&](double value) {
          if (value != 0.0) {
            float_offset += value;
            has_floats = true;
          }
        });
  };

  if (expressions.size() == 1 && py::isinstance<py::sequence>(expressions[0])) {
    // Normal list or tuple argument.
    py::sequence elements = expressions[0].cast<py::sequence>();
    linear_exprs.reserve(elements.size());
    for (const py::handle& expr : elements) {
      process_arg(expr);
    }
  } else {  // Direct sum(x, y, 3, ..) without [].
    linear_exprs.reserve(expressions.size());
    for (const py::handle expr : expressions) {
      process_arg(expr);
    }
  }

  // If there are floats, we add the int offset to the float offset.
  if (has_floats) {
    float_offset += static_cast<double>(int_offset);
    int_offset = 0;
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return std::make_shared<FloatConstant>(float_offset);
    } else {
      return std::make_shared<IntConstant>(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      if (float_offset == 0.0) {
        return linear_exprs[0];
      } else {
        return std::make_shared<FloatAffine>(linear_exprs[0], 1.0,
                                             float_offset);
      }
    } else if (int_offset != 0) {
      return std::make_shared<IntAffine>(linear_exprs[0], 1, int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return std::make_shared<SumArray>(linear_exprs, 0, float_offset);
    } else {
      return std::make_shared<SumArray>(linear_exprs, int_offset, 0.0);
    }
  }
}

std::shared_ptr<LinearExpr> WeightedSumArguments(py::sequence expressions,
                                                 py::sequence coefficients) {
  if (expressions.size() != coefficients.size()) {
    ThrowError(PyExc_ValueError,
               absl::StrCat("LinearExpr::weighted_sum() requires the same "
                            "number of arguments and coefficients: ",
                            expressions.size(), " != ", coefficients.size()));
  }

  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  std::vector<int64_t> int_coeffs;
  std::vector<double> float_coeffs;
  linear_exprs.reserve(expressions.size());
  int_coeffs.reserve(expressions.size());
  float_coeffs.reserve(expressions.size());
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  for (int i = 0; i < expressions.size(); ++i) {
    auto on_expr = [&](std::shared_ptr<LinearExpr> expr) {
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t value) {
            if (value == 0) return;
            linear_exprs.push_back(expr);
            int_coeffs.push_back(value);
            float_coeffs.push_back(static_cast<double>(value));
          },
          [&](double value) {
            if (value == 0.0) return;
            linear_exprs.push_back(expr);
            float_coeffs.push_back(value);
            has_floats = true;
          });
    };
    auto on_int = [&](int64_t expr_value) {
      if (expr_value == 0) return;
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t coeff_value) { int_offset += coeff_value * expr_value; },
          [&](double coeff_value) {
            has_floats = true;
            float_offset += coeff_value * static_cast<double>(expr_value);
          });
    };
    auto on_float = [&](double expr_value) {
      if (expr_value == 0.0) return;
      has_floats = true;
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t coeff_value) {
            float_offset += static_cast<double>(coeff_value) * expr_value;
          },
          [&](double coeff_value) {
            if (coeff_value == 0.0) return;
            float_offset += coeff_value * expr_value;
          });
    };
    ProcessExprArg(expressions[i], std::move(on_expr), std::move(on_int),
                   std::move(on_float));
  }

  // Correct the float offset if there are int offsets.
  if (has_floats) {
    float_offset += static_cast<double>(int_offset);
    int_offset = 0;
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return std::make_shared<FloatConstant>(float_offset);
    } else {
      return std::make_shared<IntConstant>(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      return std::make_shared<FloatAffine>(linear_exprs[0], float_coeffs[0],
                                           float_offset);
    } else if (int_offset != 0 || int_coeffs[0] != 1) {
      return std::make_shared<IntAffine>(linear_exprs[0], int_coeffs[0],
                                         int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return std::make_shared<FloatWeightedSum>(linear_exprs, float_coeffs,
                                                float_offset);
    } else {
      return std::make_shared<IntWeightedSum>(linear_exprs, int_coeffs,
                                              int_offset);
    }
  }
}

void LinearExprToProto(const py::handle& arg, int64_t multiplier,
                       LinearExpressionProto* proto) {
  proto->Clear();
  if (py::isinstance<LinearExpr>(arg)) {
    std::shared_ptr<LinearExpr> expr = arg.cast<std::shared_ptr<LinearExpr>>();
    IntExprVisitor visitor;
    visitor.AddToProcess(expr, multiplier);
    std::vector<std::shared_ptr<IntVar>> vars;
    std::vector<int64_t> coeffs;
    int64_t offset = 0;
    if (!visitor.Process(&vars, &coeffs, &offset)) {
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Failed to convert integer linear expression: ",
                              expr->DebugString()));
    }
    for (const auto& var : vars) {
      proto->add_vars(var->index());
    }
    for (const int64_t coeff : coeffs) {
      proto->add_coeffs(coeff);
    }
    proto->set_offset(offset);
  } else if (py::isinstance<py::int_>(arg)) {
    int64_t value = arg.cast<int64_t>();
    proto->set_offset(value * multiplier);
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer") &&
             getattr(arg, "is_integer")().cast<bool>()) {
    int64_t value = arg.cast<int64_t>();
    proto->set_offset(value * multiplier);
  } else {
    py::type objtype = py::type::of(arg);
    const std::string type_name = objtype.attr("__name__").cast<std::string>();
    py::print(arg);
    ThrowError(PyExc_TypeError,
               absl::StrCat("Cannot convert '", absl::CEscape(type_name),
                            "' to a linear expression."));
  }
}

class Constraint;
class IntervalVar;

enum class BoolArgumentConstraint {
  kAtMostOne,
  kBoolAnd,
  kBoolOr,
  kBoolXor,
  kExactlyOne,
};

enum class LinearArgumentConstraint {
  kDiv,
  kMax,
  kMin,
  kMod,
  kProd,
};

class CpBaseModel : public std::enable_shared_from_this<CpBaseModel> {
 public:
  explicit CpBaseModel(std::shared_ptr<CpModelProto> model_proto)
      : model_proto_(model_proto == nullptr ? std::make_shared<CpModelProto>()
                                            : model_proto),
        numpy_bool_type_(py::dtype::of<bool>().attr("type").cast<py::type>()) {
    if (model_proto != nullptr) RebuildConstantMap();
  }

  std::shared_ptr<CpModelProto> model_proto() const { return model_proto_; }

  int GetOrMakeIndexFromConstant(int64_t value) {
    auto it = cache_.find(value);
    if (it != cache_.end()) return it->second;
    const int index = model_proto_->variables_size();
    IntegerVariableProto* const_var = model_proto_->add_variables();
    const_var->add_domain(value);
    const_var->add_domain(value);
    cache_[value] = index;
    return index;
  }

  void RebuildConstantMap() {
    cache_.clear();
    for (int i = 0; i < model_proto_->variables_size(); ++i) {
      const IntegerVariableProto& var = model_proto_->variables(i);
      if (var.domain_size() == 2 && var.domain(0) == var.domain(1) &&
          var.name().empty()) {  // Constants do not have names.
        cache_[var.domain(0)] = i;
      }
    }
  }

  int GetOrMakeBooleanIndex(py::handle literal) {
    if (py::isinstance<IntVar>(literal)) {
      std::shared_ptr<IntVar> var = literal.cast<std::shared_ptr<IntVar>>();
      AssertVariableIsBoolean(var);
      return var->index();
    } else if (py::isinstance<NotBooleanVariable>(literal)) {
      std::shared_ptr<NotBooleanVariable> not_var =
          literal.cast<std::shared_ptr<NotBooleanVariable>>();
      AssertVariableIsBoolean(not_var);
      return not_var->index();
    } else if (IsBooleanValue(literal)) {
      const bool value = literal.cast<py::bool_>();
      if (value) {
        return GetOrMakeIndexFromConstant(1);
      } else {
        return GetOrMakeIndexFromConstant(0);
      }
    } else if (py::isinstance<py::int_>(literal)) {
      const int64_t value = literal.cast<int64_t>();
      if (value == 1 || value == -1) {  // -1 = ~False.
        return GetOrMakeIndexFromConstant(1);
      }
      if (value == 0 || value == -2) {  // -2 = ~True.
        return GetOrMakeIndexFromConstant(0);
      }
      ThrowError(PyExc_TypeError, absl::StrCat("Invalid literal: ", value));
    } else {
      py::type objtype = py::type::of(literal);
      const std::string type_name =
          objtype.attr("__name__").cast<std::string>();
      ThrowError(PyExc_TypeError, absl::StrCat("Invalid boolean literal:  '",
                                               absl::CEscape(type_name), "'"));
    }
    return 0;  // Unreachable.
  }

  int GetOrMakeVariableIndex(py::handle arg) {
    if (py::isinstance<IntVar>(arg)) {
      std::shared_ptr<IntVar> var = arg.cast<std::shared_ptr<IntVar>>();
      return var->index();
    } else if (py::isinstance<py::int_>(arg)) {
      return GetOrMakeIndexFromConstant(arg.cast<int64_t>());
    } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer") &&
               getattr(arg, "is_integer")().cast<bool>()) {
      return GetOrMakeIndexFromConstant(arg.cast<int64_t>());
    } else {
      py::type objtype = py::type::of(arg);
      const std::string type_name =
          objtype.attr("__name__").cast<std::string>();
      ThrowError(PyExc_TypeError,
                 absl::StrCat("GetOrMakeVariableIndex() only accept integer "
                              "variables or constants as argument: '",
                              absl::CEscape(type_name), "'"));
    }
    return 0;  // Unreachable.
  }

  void AssertVariableIsBoolean(std::shared_ptr<Literal> literal) {
    IntegerVariableProto* var =
        model_proto_->mutable_variables(PositiveRef(literal->index()));
    if (var->domain_size() != 2 || var->domain(0) < 0 || var->domain(1) > 1) {
      ThrowError(PyExc_TypeError, absl::StrCat("Invalid boolean literal: ",
                                               literal->ToString()));
    }
  }

  bool IsBooleanValue(py::handle value) {
    return py::isinstance<py::bool_>(value) ||
           py::isinstance(value, numpy_bool_type_);
  }

  std::shared_ptr<Constraint> AddAllDifferentInternal(py::args exprs);

  std::shared_ptr<Constraint> AddAutomatonInternal(
      py::sequence transition_expressions, int64_t starting_state,
      const std::vector<int64_t>& final_states,
      const std::vector<std::vector<int64_t>>& transition_triples);

  std::shared_ptr<Constraint> AddBoolArgumentConstraintInternal(
      BoolArgumentConstraint type, py::args literals);

  std::shared_ptr<Constraint> AddBoundedLinearExpressionInternal(
      BoundedLinearExpression* ble);

  std::shared_ptr<Constraint> AddElementInternal(const py::handle& index,
                                                 py::sequence exprs,
                                                 const py::handle& target);

  std::shared_ptr<Constraint> AddInverseInternal(py::sequence direct,
                                                 py::sequence inverse);

  std::shared_ptr<Constraint> AddLinearArgumentConstraintInternal(
      LinearArgumentConstraint type, const py::handle& target, py::args exprs);

  std::shared_ptr<Constraint> AddReservoirInternal(py::sequence times,
                                                   py::sequence level_changes,
                                                   py::sequence actives,
                                                   int64_t min_level,
                                                   int64_t max_level);

  std::shared_ptr<Constraint> AddTableInternal(
      py::sequence exprs, const std::vector<std::vector<int64_t>>& tuples,
      bool negated);

  std::shared_ptr<IntervalVar> NewIntervalVarInternal(const std::string& name,
                                                      const py::handle& start,
                                                      const py::handle& size,
                                                      const py::handle& end,
                                                      py::sequence literals);

  std::shared_ptr<Constraint> AddNoOverlapInternal(
      const std::vector<std::shared_ptr<IntervalVar>>& intervals);

  std::shared_ptr<Constraint> AddNoOverlap2DInternal(
      const std::vector<std::shared_ptr<IntervalVar>>& x_intervals,
      const std::vector<std::shared_ptr<IntervalVar>>& y_intervals);

  std::shared_ptr<Constraint> AddCumulativeInternal(
      const std::vector<std::shared_ptr<IntervalVar>>& intervals,
      py::sequence demands, const py::handle& capacity);

  std::shared_ptr<Constraint> AddCircuitInternal(
      const std::vector<std::tuple<int, int, py::handle>>& arcs);

  std::shared_ptr<Constraint> AddRoutesInternal(
      const std::vector<std::tuple<int, int, py::handle>>& arcs);

 private:
  std::shared_ptr<CpModelProto> model_proto_;
  absl::flat_hash_map<int64_t, int> cache_;
  py::type numpy_bool_type_;
};

class Constraint {
 public:
  // We need to store the CpBaseModel to convert enforcement literals to
  // indices.
  Constraint(std::shared_ptr<CpBaseModel> model, int index)
      : model_(model), index_(index) {}

  int index() const { return index_; }

  std::shared_ptr<CpModelProto> model_proto() const {
    return model_->model_proto();
  }

  ConstraintProto* proto() const {
    return model_->model_proto()->mutable_constraints(index_);
  }

  std::shared_ptr<CpBaseModel> model() const { return model_; }

  std::string name() const { return proto()->name(); }
  void SetName(const std::string& name) { proto()->set_name(name); }
  void ClearName() { proto()->clear_name(); }

  std::string ToString() const {
    return absl::StrCat("Constraint(index=", index_, ", ",
                        ProtobufDebugString(*proto()), ")");
  }

 private:
  std::shared_ptr<CpBaseModel> model_;
  int index_;
};

std::shared_ptr<Constraint> CpBaseModel::AddAllDifferentInternal(
    py::args exprs) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  if (exprs.size() == 1 && py::isinstance<py::iterable>(exprs[0])) {
    for (const auto& expr : exprs[0]) {
      LinearExprToProto(expr, 1, ct->mutable_all_diff()->add_exprs());
    }
  } else {
    for (const auto& expr : exprs) {
      LinearExprToProto(expr, 1, ct->mutable_all_diff()->add_exprs());
    }
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddAutomatonInternal(
    py::sequence transition_expressions, int64_t starting_state,
    const std::vector<int64_t>& final_states,
    const std::vector<std::vector<int64_t>>& transition_triples) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  for (const auto& expr : transition_expressions) {
    LinearExprToProto(expr, 1, ct->mutable_automaton()->add_exprs());
  }
  ct->mutable_automaton()->set_starting_state(starting_state);
  ct->mutable_automaton()->mutable_final_states()->Add(final_states.begin(),
                                                       final_states.end());
  for (const auto& tuple : transition_triples) {
    if (tuple.size() != 3) {
      ThrowError(PyExc_ValueError,
                 absl::StrCat("transition (", absl::StrJoin(tuple, ","),
                              ") has the wrong arity != 3"));
    }
    ct->mutable_automaton()->add_transition_tail(tuple[0]);
    ct->mutable_automaton()->add_transition_label(tuple[1]);
    ct->mutable_automaton()->add_transition_head(tuple[2]);
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddBoolArgumentConstraintInternal(
    BoolArgumentConstraint type, py::args literals) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  BoolArgumentProto* proto = nullptr;
  switch (type) {
    case BoolArgumentConstraint::kAtMostOne:
      proto = ct->mutable_at_most_one();
      break;
    case BoolArgumentConstraint::kBoolAnd:
      proto = ct->mutable_bool_and();
      break;
    case BoolArgumentConstraint::kBoolOr:
      proto = ct->mutable_bool_or();
      break;
    case BoolArgumentConstraint::kBoolXor:
      proto = ct->mutable_bool_xor();
      break;
    case BoolArgumentConstraint::kExactlyOne:
      proto = ct->mutable_exactly_one();
      break;
    default:
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Unknown boolean argument constraint: ", type));
  }
  if (literals.size() == 1 && py::isinstance<py::iterable>(literals[0])) {
    for (const auto& literal : literals[0]) {
      proto->add_literals(GetOrMakeBooleanIndex(literal));
    }
  } else {
    for (const auto& literal : literals) {
      proto->add_literals(GetOrMakeBooleanIndex(literal));
    }
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddBoundedLinearExpressionInternal(
    BoundedLinearExpression* ble) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  for (const auto& var : ble->vars()) {
    ct->mutable_linear()->add_vars(var->index());
  }
  for (const int64_t coeff : ble->coeffs()) {
    ct->mutable_linear()->add_coeffs(coeff);
  }
  const int64_t offset = ble->offset();
  const Domain& bounds = ble->bounds();
  for (const int64_t bound : bounds.FlattenedIntervals()) {
    if (bound == std::numeric_limits<int64_t>::min() ||
        bound == std::numeric_limits<int64_t>::max()) {
      ct->mutable_linear()->add_domain(bound);
    } else {
      ct->mutable_linear()->add_domain(CapSub(bound, offset));
    }
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddElementInternal(
    const py::handle& index, py::sequence exprs, const py::handle& target) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  LinearExprToProto(index, 1, ct->mutable_element()->mutable_linear_index());
  for (const auto& expr : exprs) {
    LinearExprToProto(expr, 1, ct->mutable_element()->add_exprs());
  }
  LinearExprToProto(target, 1, ct->mutable_element()->mutable_linear_target());
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddInverseInternal(
    py::sequence direct, py::sequence inverse) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  for (const auto& var : direct) {
    ct->mutable_inverse()->add_f_direct(GetOrMakeVariableIndex(var));
  }
  for (const auto& var : inverse) {
    ct->mutable_inverse()->add_f_inverse(GetOrMakeVariableIndex(var));
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddLinearArgumentConstraintInternal(
    LinearArgumentConstraint type, const py::handle& target, py::args exprs) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  LinearArgumentProto* proto;
  int64_t multiplier = 1;
  switch (type) {
    case LinearArgumentConstraint::kDiv:
      proto = ct->mutable_int_div();
      break;
    case LinearArgumentConstraint::kMax:
      proto = ct->mutable_lin_max();
      break;
    case LinearArgumentConstraint::kMin:
      proto = ct->mutable_lin_max();
      multiplier = -1;
      break;
    case LinearArgumentConstraint::kMod:
      proto = ct->mutable_int_mod();
      break;
    case LinearArgumentConstraint::kProd:
      proto = ct->mutable_int_prod();
      break;
    default:
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Unknown integer argument constraint: ", type));
  }

  LinearExprToProto(target, multiplier, proto->mutable_target());

  if (exprs.size() == 1 && py::isinstance<py::iterable>(exprs[0])) {
    for (const auto& expr : exprs[0]) {
      LinearExprToProto(expr, multiplier, proto->add_exprs());
    }
  } else {
    for (const auto& expr : exprs) {
      LinearExprToProto(expr, multiplier, proto->add_exprs());
    }
  }

  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddReservoirInternal(
    py::sequence times, py::sequence level_changes, py::sequence actives,
    int64_t min_level, int64_t max_level) {
  const int ct_index = model_proto_->constraints_size();
  ReservoirConstraintProto* proto =
      model_proto_->add_constraints()->mutable_reservoir();
  for (const auto& time : times) {
    LinearExprToProto(time, 1, proto->add_time_exprs());
  }
  for (const auto& change : level_changes) {
    LinearExprToProto(change, 1, proto->add_level_changes());
  }
  for (const auto& active : actives) {
    proto->add_active_literals(GetOrMakeBooleanIndex(active));
  }
  proto->set_min_level(min_level);
  proto->set_max_level(max_level);
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddTableInternal(
    py::sequence exprs, const std::vector<std::vector<int64_t>>& tuples,
    bool negated) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  const int num_exprs = exprs.size();
  for (const auto& expr : exprs) {
    LinearExprToProto(expr, 1, ct->mutable_table()->add_exprs());
  }
  for (const auto& tuple : tuples) {
    if (tuple.size() != num_exprs) {
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Tuple (", absl::StrJoin(tuple, ","),
                              ") has the wrong arity != ", num_exprs));
    }
    ct->mutable_table()->mutable_values()->Add(tuple.begin(), tuple.end());
  }
  ct->mutable_table()->set_negated(negated);
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::string ShortName(int literal, std::shared_ptr<CpModelProto> model_proto) {
  const int var = PositiveRef(literal);
  const IntegerVariableProto& var_proto = model_proto->variables(var);
  const std::string& var_name =
      var_proto.name().empty() ? absl::StrCat("i", var) : var_proto.name();
  if (literal < 0) {
    return absl::StrCat("not(", var_name, ")");
  } else {
    return var_name;
  }
}

std::string ShortExprName(const LinearExpressionProto& expr,
                          std::shared_ptr<CpModelProto> model_proto) {
  if (expr.vars().empty()) {
    return absl::StrCat(expr.offset());
  } else {
    const IntegerVariableProto& var_proto =
        model_proto->variables(expr.vars(0));
    const std::string& var_name = var_proto.name().empty()
                                      ? absl::StrCat("i", expr.vars(0))
                                      : var_proto.name();
    const int64_t coeff = expr.coeffs(0);
    std::string result;
    if (coeff == 1) {
      result = var_name;
    } else if (coeff == -1) {
      result = absl::StrCat("-", var_name);
    } else if (coeff != 0) {
      result = absl::StrCat(coeff, " * ", var_name);
    }
    if (expr.offset() > 0) {
      absl::StrAppend(&result, " + ", expr.offset());
    } else if (expr.offset() < 0) {
      absl::StrAppend(&result, " - ", -expr.offset());
    }
    return result;
  }
}

std::shared_ptr<LinearExpr> RebuildFromLinearExpressionProto(
    const LinearExpressionProto& proto,
    std::shared_ptr<CpModelProto> model_proto) {
  if (proto.vars().empty()) {
    return LinearExpr::ConstantInt(proto.offset());
  } else if (proto.vars_size() == 1) {
    return LinearExpr::AffineInt(
        std::make_shared<IntVar>(model_proto, proto.vars(0)), proto.coeffs(0),
        proto.offset());
  } else {
    std::vector<std::shared_ptr<LinearExpr>> vars;
    vars.reserve(proto.vars_size());
    for (const int var : proto.vars()) {
      vars.push_back(std::make_shared<IntVar>(model_proto, var));
    }
    return std::make_shared<IntWeightedSum>(vars, proto.coeffs(),
                                            proto.offset());
  }
}

class IntervalVar {
 public:
  IntervalVar(std::shared_ptr<CpModelProto> model_proto, int index)
      : model_proto_(model_proto), index_(index) {
    DCHECK_GE(index, 0);
  }

  int index() const { return index_; }

  std::shared_ptr<CpModelProto> model_proto() const { return model_proto_; }

  ConstraintProto* proto() const {
    return model_proto_->mutable_constraints(index_);
  }

  std::string ToString() const {
    const std::string name = proto()->name();
    if (name.empty()) {
      return absl::StrCat("iv", index_);
    } else {
      return name;
    }
  }

  std::string DebugString() const {
    if (proto()->enforcement_literal().empty()) {
      return absl::StrCat(
          name(), "(start = ",
          ShortExprName(proto()->interval().start(), model_proto()),
          ", size = ", ShortExprName(proto()->interval().size(), model_proto()),
          ", end = ", ShortExprName(proto()->interval().end(), model_proto()),
          ")");
    } else {
      return absl::StrCat(
          name(), "(start = ",
          ShortExprName(proto()->interval().start(), model_proto()),
          ", size = ", ShortExprName(proto()->interval().size(), model_proto()),
          ", end = ", ShortExprName(proto()->interval().end(), model_proto()),
          ", is_present = ",
          ShortName(proto()->enforcement_literal(0), model_proto()), ")");
    }
  }

  std::string name() const { return proto()->name(); }

  void SetName(const std::string& name) { proto()->set_name(name); }

  std::shared_ptr<LinearExpr> StartExpr() const {
    return RebuildFromLinearExpressionProto(proto()->interval().start(),
                                            model_proto_);
  }
  std::shared_ptr<LinearExpr> SizeExpr() const {
    return RebuildFromLinearExpressionProto(proto()->interval().size(),
                                            model_proto_);
  }
  std::shared_ptr<LinearExpr> EndExpr() const {
    return RebuildFromLinearExpressionProto(proto()->interval().end(),
                                            model_proto_);
  }

 private:
  std::shared_ptr<CpModelProto> model_proto_;
  int index_;
};

std::shared_ptr<IntervalVar> CpBaseModel::NewIntervalVarInternal(
    const std::string& name, const py::handle& start, const py::handle& size,
    const py::handle& end, py::sequence literals) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  if (!name.empty()) ct->set_name(name);
  ct->mutable_interval()->mutable_start()->set_offset(1);
  LinearExprToProto(start, 1, ct->mutable_interval()->mutable_start());
  LinearExprToProto(size, 1, ct->mutable_interval()->mutable_size());
  LinearExprToProto(end, 1, ct->mutable_interval()->mutable_end());
  for (const auto& lit : literals) {
    ct->add_enforcement_literal(GetOrMakeBooleanIndex(lit));
  }
  const std::string method = literals.empty()
                                 ? "cp_model.new_interval_var"
                                 : "cp_model.new_optional_interval_var";
  if (ct->interval().start().vars_size() > 1) {
    ThrowError(PyExc_TypeError,
               absl::StrCat(method, ": start must be affine or constant."));
  }
  if (ct->interval().size().vars_size() > 1) {
    ThrowError(PyExc_TypeError,
               absl::StrCat(method, ": size must be affine or constant."));
  }
  if (ct->interval().end().vars_size() > 1) {
    ThrowError(PyExc_TypeError,
               absl::StrCat(method, ": end must be affine or constant."));
  }
  return std::make_shared<IntervalVar>(model_proto_, ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddNoOverlapInternal(
    const std::vector<std::shared_ptr<IntervalVar>>& intervals) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  ct->mutable_no_overlap()->mutable_intervals()->Reserve(intervals.size());
  for (const std::shared_ptr<IntervalVar>& interval : intervals) {
    ct->mutable_no_overlap()->add_intervals(interval->index());
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddNoOverlap2DInternal(
    const std::vector<std::shared_ptr<IntervalVar>>& x_intervals,
    const std::vector<std::shared_ptr<IntervalVar>>& y_intervals) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  ct->mutable_no_overlap_2d()->mutable_x_intervals()->Reserve(
      x_intervals.size());
  for (const std::shared_ptr<IntervalVar>& x_interval : x_intervals) {
    ct->mutable_no_overlap_2d()->add_x_intervals(x_interval->index());
  }
  ct->mutable_no_overlap_2d()->mutable_y_intervals()->Reserve(
      y_intervals.size());
  for (const std::shared_ptr<IntervalVar>& y_interval : y_intervals) {
    ct->mutable_no_overlap_2d()->add_y_intervals(y_interval->index());
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddCumulativeInternal(
    const std::vector<std::shared_ptr<IntervalVar>>& intervals,
    const py::sequence demands, const py::handle& capacity) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  CumulativeConstraintProto* proto = ct->mutable_cumulative();

  proto->mutable_intervals()->Reserve(intervals.size());
  for (const std::shared_ptr<IntervalVar>& interval : intervals) {
    proto->add_intervals(interval->index());
  }

  proto->mutable_demands()->Reserve(demands.size());
  for (const auto& expr : demands) {
    LinearExprToProto(expr, 1, proto->add_demands());
  }

  LinearExprToProto(capacity, 1, proto->mutable_capacity());
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddCircuitInternal(
    const std::vector<std::tuple<int, int, py::handle>>& arcs) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  CircuitConstraintProto* proto = ct->mutable_circuit();
  proto->mutable_tails()->Reserve(arcs.size());
  proto->mutable_heads()->Reserve(arcs.size());
  proto->mutable_literals()->Reserve(arcs.size());
  for (const auto& [tail, head, lit] : arcs) {
    proto->add_tails(tail);
    proto->add_heads(head);
    proto->add_literals(GetOrMakeBooleanIndex(lit));
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

std::shared_ptr<Constraint> CpBaseModel::AddRoutesInternal(
    const std::vector<std::tuple<int, int, py::handle>>& arcs) {
  const int ct_index = model_proto_->constraints_size();
  ConstraintProto* ct = model_proto_->add_constraints();
  RoutesConstraintProto* proto = ct->mutable_routes();
  proto->mutable_tails()->Reserve(arcs.size());
  proto->mutable_heads()->Reserve(arcs.size());
  proto->mutable_literals()->Reserve(arcs.size());
  for (const auto& [tail, head, lit] : arcs) {
    proto->add_tails(tail);
    proto->add_heads(head);
    proto->add_literals(GetOrMakeBooleanIndex(lit));
  }
  return std::make_shared<Constraint>(shared_from_this(), ct_index);
}

PYBIND11_MODULE(cp_model_helper, m) {
  py::module::import("ortools.util.python.sorted_interval_list");

  // We keep the CamelCase name for the SolutionCallback class to be
  // compatible with the pre PEP8 python code.
  py::class_<SolutionCallback, PySolutionCallback>(m, "SolutionCallback")
      .def(py::init<>())
      .def("OnSolutionCallback", &SolutionCallback::OnSolutionCallback)
      .def("BestObjectiveBound", &SolutionCallback::BestObjectiveBound)
      .def("DeterministicTime", &SolutionCallback::DeterministicTime)
      .def("HasResponse", &SolutionCallback::HasResponse)
      .def("NumBinaryPropagations", &SolutionCallback::NumBinaryPropagations)
      .def("NumBooleans", &SolutionCallback::NumBooleans)
      .def("NumBranches", &SolutionCallback::NumBranches)
      .def("NumConflicts", &SolutionCallback::NumConflicts)
      .def("NumIntegerPropagations", &SolutionCallback::NumIntegerPropagations)
      .def("ObjectiveValue", &SolutionCallback::ObjectiveValue)
      .def("Response", &SolutionCallback::SharedResponse)
      .def("SolutionBooleanValue", &SolutionCallback::SolutionBooleanValue,
           py::arg("index"))
      .def("SolutionIntegerValue", &SolutionCallback::SolutionIntegerValue,
           py::arg("index"))
      .def("StopSearch", &SolutionCallback::StopSearch)
      .def("UserTime", &SolutionCallback::UserTime)
      .def("WallTime", &SolutionCallback::WallTime)
      .def(
          "Value",
          [](const SolutionCallback& self, std::shared_ptr<LinearExpr> expr) {
            return ResponseHelper::Value(self.SharedResponse(), expr);
          },
          "Returns the value of a linear expression after solve.")
      .def(
          "Value", [](const SolutionCallback&, int64_t value) { return value; },
          "Returns the value of a linear expression after solve.")
      .def(
          "FloatValue",
          [](const SolutionCallback& self, std::shared_ptr<LinearExpr> expr) {
            return ResponseHelper::FloatValue(self.SharedResponse(), expr);
          },
          "Returns the value of a floating point linear expression after "
          "solve.")
      .def(
          "FloatValue",
          [](const SolutionCallback&, double value) { return value; },
          "Returns the value of a floating point linear expression after "
          "solve.")
      .def(
          "BooleanValue",
          [](const SolutionCallback& self, std::shared_ptr<Literal> lit) {
            return ResponseHelper::BooleanValue(self.SharedResponse(), lit);
          },
          "Returns the Boolean value of a literal after solve.")
      .def(
          "BooleanValue", [](const SolutionCallback&, bool lit) { return lit; },
          "Returns the Boolean value of a literal after solve.");

  py::class_<ResponseHelper>(m, "ResponseHelper")
      .def_static("boolean_value", &ResponseHelper::BooleanValue,
                  py::arg("response").none(false), py::arg("lit").none(false))
      .def_static("boolean_value", &ResponseHelper::FixedBooleanValue,
                  py::arg("response").none(false), py::arg("lit").none(false))
      .def_static("float_value", &ResponseHelper::FloatValue,
                  py::arg("response").none(false), py::arg("expr").none(false))
      .def_static("float_value", &ResponseHelper::FixedFloatValue,
                  py::arg("response").none(false), py::arg("value").none(false))
      .def_static("sufficient_assumptions_for_infeasibility",
                  &ResponseHelper::SufficientAssumptionsForInfeasibility,
                  py::arg("response").none(false))
      .def_static("value", &ResponseHelper::Value,
                  py::arg("response").none(false), py::arg("expr").none(false))
      .def_static("value", &ResponseHelper::FixedValue,
                  py::arg("response").none(false),
                  py::arg("value").none(false));

  py::class_<ExtSolveWrapper>(m, "SolveWrapper")
      .def(py::init<>())
      .def(
          "add_log_callback",
          [](ExtSolveWrapper* solve_wrapper,
             std::function<void(const std::string&)> log_callback) {
            std::function<void(const std::string&)> safe_log_callback =
                [solve_wrapper, log_callback](std::string message) -> void {
              ::py::gil_scoped_acquire acquire;
              try {
                log_callback(message);
              } catch (py::error_already_set& e) {
                // We assume this code is serialized as the gil is held.
                if (!solve_wrapper->local_error_already_set_.has_value()) {
                  solve_wrapper->local_error_already_set_ = e;
                }
                solve_wrapper->StopSearch();
              }
            };
            solve_wrapper->AddLogCallback(safe_log_callback);
          },
          py::arg("log_callback").none(false))
      .def("add_solution_callback", &SolveWrapper::AddSolutionCallback,
           py::arg("callback"))
      .def("clear_solution_callback", &SolveWrapper::ClearSolutionCallback)
      .def(
          "add_best_bound_callback",
          [](ExtSolveWrapper* solve_wrapper,
             std::function<void(double)> best_bound_callback) {
            std::function<void(double)> safe_best_bound_callback =
                [solve_wrapper, best_bound_callback](double bound) -> void {
              ::py::gil_scoped_acquire acquire;
              try {
                best_bound_callback(bound);
              } catch (py::error_already_set& e) {
                // We assume this code is serialized as the gil is held.
                if (!solve_wrapper->local_error_already_set_.has_value()) {
                  solve_wrapper->local_error_already_set_ = e;
                }
                solve_wrapper->StopSearch();
              }
            };
            solve_wrapper->AddBestBoundCallback(safe_best_bound_callback);
          },
          py::arg("best_bound_callback").none(false))
      .def(
          "set_parameters",
          [](ExtSolveWrapper* solve_wrapper,
             std::shared_ptr<SatParameters> parameters) {
            solve_wrapper->SetParameters(*parameters);
          },
          py::arg("parameters").none(false))
      .def(
          "solve",
          [](ExtSolveWrapper* solve_wrapper,
             std::shared_ptr<CpModelProto> model_proto) -> CpSolverResponse {
            const auto result = [=]() -> CpSolverResponse {
              ::py::gil_scoped_release release;
              return solve_wrapper->Solve(*model_proto);
            }();
            if (solve_wrapper->local_error_already_set_.has_value()) {
              solve_wrapper->local_error_already_set_->restore();
              solve_wrapper->local_error_already_set_.reset();
              throw py::error_already_set();
            }
            return result;
          },
          py::arg("model_proto").none(false))
      .def("stop_search", &SolveWrapper::StopSearch);

  py::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("model_stats", &CpSatHelper::ModelStats,
                  py::arg("model_proto"))
      .def_static("solver_response_stats", &CpSatHelper::SolverResponseStats,
                  py::arg("response"))
      .def_static("validate_model", &CpSatHelper::ValidateModel,
                  py::arg("model_proto"))
      .def_static("variable_domain", &CpSatHelper::VariableDomain,
                  py::arg("variable_proto"))
      .def_static("write_model_to_file", &CpSatHelper::WriteModelToFile,
                  py::arg("model_proto"), py::arg("filename"));

  py::class_<LinearExpr, std::shared_ptr<LinearExpr>>(
      m, "LinearExpr", DOC(operations_research, sat, python, LinearExpr))
      .def_static("sum", &SumArguments, "Returns the sum(expressions).")
      .def_static("weighted_sum", &WeightedSumArguments, py::arg("expressions"),
                  py::arg("coefficients"),
                  "Returns the sum of (expressions[i] * coefficients[i])")
      .def_static("term", &LinearExpr::TermInt, py::arg("expr").none(false),
                  py::arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermInt))
      .def_static("term", &LinearExpr::TermFloat, py::arg("expr").none(false),
                  py::arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermFloat))
      .def_static("affine", &LinearExpr::AffineInt, py::arg("expr").none(false),
                  py::arg("coeff"), py::arg("offset"),
                  DOC(operations_research, sat, python, LinearExpr, AffineInt))
      .def_static(
          "affine", &LinearExpr::AffineFloat, py::arg("expr").none(false),
          py::arg("coeff"), py::arg("offset"),
          DOC(operations_research, sat, python, LinearExpr, AffineFloat))
      .def_static(
          "constant", &LinearExpr::ConstantInt, py::arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantInt))
      .def_static(
          "constant", &LinearExpr::ConstantFloat, py::arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantFloat))
      // Pre PEP8 compatibility layer.
      .def_static("Sum", &SumArguments)
      .def_static("WeightedSum", &WeightedSumArguments, py::arg("expressions"),
                  py::arg("coefficients"))
      .def_static("Term", &LinearExpr::TermInt, py::arg("expr").none(false),
                  py::arg("coeff"), "Returns expr * coeff.")
      .def_static("Term", &LinearExpr::TermFloat, py::arg("expr").none(false),
                  py::arg("coeff"), "Returns expr * coeff.")
      // Methods.
      .def("__str__",
           [](std::shared_ptr<LinearExpr> expr) -> std::string {
             return expr->ToString();
           })
      .def("__repr__",
           [](std::shared_ptr<LinearExpr> expr) -> std::string {
             return expr->DebugString();
           })
      .def(
          "is_integer",
          [](std::shared_ptr<LinearExpr> expr) -> bool {
            return expr->IsInteger();
          },
          DOC(operations_research, sat, python, LinearExpr, IsInteger))
      // Operators.
      .def("__add__", &LinearExpr::Add, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Add))
      .def("__add__", &LinearExpr::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__add__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__radd__", &LinearExpr::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__radd__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__sub__", &LinearExpr::Sub, py::arg("h").none(false),
           DOC(operations_research, sat, python, LinearExpr, Sub))
      .def("__sub__", &LinearExpr::SubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def("__sub__", &LinearExpr::SubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def("__rsub__", &LinearExpr::RSub, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, RSub))
      .def("__rsub__", &LinearExpr::RSubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubInt))
      .def("__rsub__", &LinearExpr::RSubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubFloat))
      .def("__mul__", &LinearExpr::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__mul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__rmul__", &LinearExpr::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__rmul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__neg__", &LinearExpr::Neg,
           DOC(operations_research, sat, python, LinearExpr, Neg))
      .def(
          "__eq__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Eq(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Eq))
      .def(
          "__eq__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max() ||
                rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ValueError,
                         "== INT_MIN or INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->EqCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, EqCst))
      .def(
          "__ne__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ne(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ne))
      .def(
          "__ne__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            return CheckBoundedLinearExpression(lhs->NeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, NeCst))
      .def(
          "__le__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Le(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Le))
      .def(
          "__le__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "<= INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LeCst))
      .def(
          "__lt__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Lt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Lt))
      .def(
          "__lt__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "< INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LtCst))
      .def(
          "__ge__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ge(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ge))
      .def(
          "__ge__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, ">= INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GeCst))
      .def(
          "__gt__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Gt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Gt))
      .def(
          "__gt__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, "> INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GtCst))
      // Disable other operators as they are not supported.
      .def("__div__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling / on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling // on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__mod__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling %% on a linear expression is not supported, "
                        "please use CpModel.add_modulo_equality");
           })
      .def("__pow__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling ** on a linear expression is not supported, "
                        "please use CpModel.add_multiplication_equality");
           })
      .def("__lshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling left shift on a linear expression is not "
                        "supported");
           })
      .def("__rshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling right shift on a linear expression is "
                        "not supported");
           })
      .def("__and__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling and on a linear expression is not supported");
           })
      .def("__or__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling or on a linear expression is not supported");
           })
      .def("__xor__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling xor on a linear expression is not supported");
           })
      .def("__abs__",
           [](std::shared_ptr<LinearExpr> /*self*/) {
             ThrowError(
                 PyExc_NotImplementedError,
                 "calling abs() on a linear expression is not supported, "
                 "please use CpModel.add_abs_equality");
           })
      .def("__bool__", [](std::shared_ptr<LinearExpr> /*self*/) {
        ThrowError(PyExc_NotImplementedError,
                   "Evaluating a LinearExpr instance as a Boolean is "
                   "not supported.");
      });

  // Expose Internal classes, mostly for testing.
  py::class_<FlatFloatExpr, std::shared_ptr<FlatFloatExpr>, LinearExpr>(
      m, "FlatFloatExpr", DOC(operations_research, sat, python, FlatFloatExpr))
      .def(py::init<std::shared_ptr<LinearExpr>>())
      .def_property_readonly("vars", &FlatFloatExpr::vars)
      .def_property_readonly("coeffs", &FlatFloatExpr::coeffs)
      .def_property_readonly("offset", &FlatFloatExpr::offset);

  py::class_<FlatIntExpr, std::shared_ptr<FlatIntExpr>, LinearExpr>(
      m, "FlatIntExpr", DOC(operations_research, sat, python, FlatIntExpr))
      .def(py::init([](std::shared_ptr<LinearExpr> expr) {
        FlatIntExpr* result = new FlatIntExpr(expr);
        if (!result->ok()) {
          ThrowError(PyExc_TypeError,
                     absl::StrCat("Tried to build a FlatIntExpr from a linear "
                                  "expression with "
                                  "floating point coefficients or constants:  ",
                                  expr->DebugString()));
        }
        return result;
      }))
      .def_property_readonly("vars", &FlatIntExpr::vars)
      .def_property_readonly("coeffs", &FlatIntExpr::coeffs)
      .def_property_readonly("offset", &FlatIntExpr::offset)
      .def_property_readonly("ok", &FlatIntExpr::ok);

  py::class_<SumArray, std::shared_ptr<SumArray>, LinearExpr>(
      m, "SumArray", DOC(operations_research, sat, python, SumArray))
      .def(
          py::init<std::vector<std::shared_ptr<LinearExpr>>, int64_t, double>())
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddInPlace(other) : expr->Add(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Add))
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddIntInPlace(cst)
                                   : expr->AddInt(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddFloatInPlace(cst)
                                   : expr->AddFloat(cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddIntInPlace(cst)
                                   : expr->AddInt(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddFloatInPlace(cst)
                                   : expr->AddFloat(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__iadd__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return expr->AddInPlace(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Add))
      .def(
          "__iadd__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddIntInPlace(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__iadd__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddFloatInPlace(cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddInPlace(other->Neg())
                                   : expr->Sub(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Sub))
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddIntInPlace(-cst)
                                   : expr->SubInt(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(py::cast(expr).ptr());
            return (num_uses == 4) ? expr->AddFloatInPlace(-cst)
                                   : expr->SubFloat(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def(
          "__isub__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return expr->AddInPlace(other->Neg());
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Sub))
      .def(
          "__isub__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddIntInPlace(-cst);
          },
          DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def(
          "__isub__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddFloatInPlace(-cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def_property_readonly("num_exprs", &SumArray::num_exprs)
      .def_property_readonly("int_offset", &SumArray::int_offset)
      .def_property_readonly("double_offset", &SumArray::double_offset);

  py::class_<FloatAffine, std::shared_ptr<FloatAffine>, LinearExpr>(
      m, "FloatAffine", DOC(operations_research, sat, python, FloatAffine))
      .def(py::init<std::shared_ptr<LinearExpr>, double, double>())
      .def_property_readonly("expression", &FloatAffine::expression)
      .def_property_readonly("coefficient", &FloatAffine::coefficient)
      .def_property_readonly("offset", &FloatAffine::offset);

  py::class_<IntAffine, std::shared_ptr<IntAffine>, LinearExpr>(
      m, "IntAffine", DOC(operations_research, sat, python, IntAffine))
      .def(py::init<std::shared_ptr<LinearExpr>, int64_t, int64_t>())
      .def_property_readonly("expression", &IntAffine::expression,
                             "Returns the linear expression.")
      .def_property_readonly("coefficient", &IntAffine::coefficient,
                             "Returns the coefficient.")
      .def_property_readonly("offset", &IntAffine::offset,
                             "Returns the offset.");

  py::class_<Literal, std::shared_ptr<Literal>, LinearExpr>(
      m, "Literal", DOC(operations_research, sat, python, Literal))
      .def_property_readonly(
          "index", &Literal::index,
          DOC(operations_research, sat, python, Literal, index))
      .def("negated", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__invert__", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__bool__",
           [](std::shared_ptr<Literal> /*self*/) {
             ThrowError(PyExc_NotImplementedError,
                        "Evaluating a Literal as a Boolean valueis "
                        "not supported.");
           })
      .def("__hash__", &Literal::Hash)
      // Pre PEP8 compatibility layer.
      .def("Not", &Literal::negated)
      .def("Index", &Literal::index);

  // IntVar and NotBooleanVariable both hold a shared_ptr to the model_proto.
  py::class_<IntVar, std::shared_ptr<IntVar>, Literal>(
      m, "IntVar", DOC(operations_research, sat, python, IntVar))
      .def(py::init<std::shared_ptr<CpModelProto>, int>())
      .def(py::init<std::shared_ptr<CpModelProto>>())  // new variable.
      .def_property_readonly(
          "proto", &IntVar::proto, py::return_value_policy::reference,
          py::keep_alive<1, 0>(),
          "Returns the IntegerVariableProto of this variable.")
      .def_property_readonly("model_proto", &IntVar::model_proto,
                             "Returns the CP model protobuf")
      .def_property_readonly(
          "index", &IntVar::index, py::return_value_policy::reference,
          DOC(operations_research, sat, python, IntVar, index))
      .def_property_readonly(
          "is_boolean", &IntVar::is_boolean,
          DOC(operations_research, sat, python, IntVar, is_boolean))
      .def_property("name", &IntVar::name, &IntVar::SetName,
                    "The name of the variable.")
      .def(
          "with_name",
          [](std::shared_ptr<IntVar> self, const std::string& name) {
            self->SetName(name);
            return self;
          },
          py::arg("name"),
          "Sets the name of the variable and returns the variable.")
      .def_property("domain", &IntVar::domain, &IntVar::SetDomain,
                    "The domain of the variable.")
      .def(
          "with_domain",
          [](std::shared_ptr<IntVar> self, const Domain& domain) {
            self->SetDomain(domain);
            return self;
          },
          py::arg("domain"),
          "Sets the domain of the variable and returns the variable.")
      .def("__str__", &IntVar::ToString)
      .def("__repr__", &IntVar::DebugString)
      .def(
          "negated",
          [](std::shared_ptr<IntVar> self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, IntVar, negated))
      .def(
          "__invert__",
          [](std::shared_ptr<IntVar> self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, IntVar, negated))
      .def("__copy__",
           [](const std::shared_ptr<IntVar>& self) {
             return std::make_shared<IntVar>(self->model_proto(),
                                             self->index());
           })
      .def(py::pickle(
          [](std::shared_ptr<IntVar> p) {  // __getstate__
            /* Return a tuple that fully encodes the state of the object */
            return py::make_tuple(p->model_proto(), p->index());
          },
          [](py::tuple t) {  // __setstate__
            if (t.size() != 2) throw std::runtime_error("Invalid state!");

            return std::make_shared<IntVar>(
                t[0].cast<std::shared_ptr<CpModelProto>>(), t[1].cast<int>());
          }))
      // Pre PEP8 compatibility layer.
      .def("Name", &IntVar::name)
      .def("Proto", &IntVar::proto)
      .def("Not",
           [](std::shared_ptr<IntVar> self) {
             if (!self->is_boolean()) {
               ThrowError(PyExc_TypeError,
                          "negated() is only supported for Boolean variables.");
             }
             return self->negated();
           })
      .def("Index", &IntVar::index);

  py::class_<NotBooleanVariable, std::shared_ptr<NotBooleanVariable>, Literal>(
      m, "NotBooleanVariable",
      DOC(operations_research, sat, python, NotBooleanVariable))
      .def_property_readonly(
          "index",
          [](std::shared_ptr<NotBooleanVariable> not_var) -> int {
            return not_var->index();
          },
          DOC(operations_research, sat, python, NotBooleanVariable, index))
      .def("__str__",
           [](std::shared_ptr<NotBooleanVariable> not_var) -> std::string {
             return not_var->ToString();
           })
      .def("__repr__",
           [](std::shared_ptr<NotBooleanVariable> not_var) -> std::string {
             return not_var->DebugString();
           })
      .def(
          "negated",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> { return not_var->negated(); },
          DOC(operations_research, sat, python, NotBooleanVariable, negated))
      .def(
          "__invert__",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> { return not_var->negated(); },
          DOC(operations_research, sat, python, NotBooleanVariable, negated))
      // Pre PEP8 compatibility layer.
      .def(
          "Not",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> { return not_var->negated(); },
          DOC(operations_research, sat, python, NotBooleanVariable, negated));

  py::class_<BoundedLinearExpression, std::shared_ptr<BoundedLinearExpression>>(
      m, "BoundedLinearExpression",
      DOC(operations_research, sat, python, BoundedLinearExpression))
      .def(py::init<const std::shared_ptr<LinearExpr>, Domain>())
      .def(py::init<const std::shared_ptr<LinearExpr>,
                    const std::shared_ptr<LinearExpr>, Domain>())
      .def_property_readonly("bounds", &BoundedLinearExpression::bounds)
      .def_property_readonly("vars", &BoundedLinearExpression::vars)
      .def_property_readonly("coeffs", &BoundedLinearExpression::coeffs)
      .def_property_readonly("offset", &BoundedLinearExpression::offset)
      .def_property_readonly("ok", &BoundedLinearExpression::ok)
      .def("__str__", &BoundedLinearExpression::ToString)
      .def("__repr__", &BoundedLinearExpression::DebugString)
      .def("__bool__", [](const BoundedLinearExpression& self) {
        bool result;
        if (self.CastToBool(&result)) return result;
        ThrowError(PyExc_NotImplementedError,
                   absl::StrCat("Evaluating a BoundedLinearExpression '",
                                self.ToString(),
                                "'instance as a Boolean is "
                                "not supported."));
        return false;
      });

  py::enum_<BoolArgumentConstraint>(m, "BoolArgumentConstraint")
      .value("at_most_one", BoolArgumentConstraint::kAtMostOne)
      .value("bool_and", BoolArgumentConstraint::kBoolAnd)
      .value("bool_or", BoolArgumentConstraint::kBoolOr)
      .value("bool_xor", BoolArgumentConstraint::kBoolXor)
      .value("exactly_one", BoolArgumentConstraint::kExactlyOne)
      .export_values();

  py::enum_<LinearArgumentConstraint>(m, "LinearArgumentConstraint")
      .value("div", LinearArgumentConstraint::kDiv)
      .value("max", LinearArgumentConstraint::kMax)
      .value("min", LinearArgumentConstraint::kMin)
      .value("mod", LinearArgumentConstraint::kMod)
      .value("prod", LinearArgumentConstraint::kProd)
      .export_values();

  py::class_<CpBaseModel, std::shared_ptr<CpBaseModel>>(
      m, "CpBaseModel", "Base class for the CP model.")
      .def(py::init<std::shared_ptr<CpModelProto>>())
      .def_property_readonly("model_proto", &CpBaseModel::model_proto,
                             "Returns the CP model protobuf")
      .def("get_or_make_index_from_constant",
           &CpBaseModel::GetOrMakeIndexFromConstant, py::arg("value"),
           "Returns the index of the given constant value.")
      .def("get_or_make_boolean_index", &CpBaseModel::GetOrMakeBooleanIndex,
           py::arg("value"), "Returns the index of the given boolean value.")
      .def("get_or_make_variable_index", &CpBaseModel::GetOrMakeVariableIndex,
           py::arg("arg"),
           "Returns the index of the given variable or constant variable.")
      .def("is_boolean_value", &CpBaseModel::IsBooleanValue, py::arg("value"))
      .def("rebuild_constant_map", &CpBaseModel::RebuildConstantMap)
      .def("_add_all_different", &CpBaseModel::AddAllDifferentInternal)
      .def("_add_automaton", &CpBaseModel::AddAutomatonInternal,
           py::arg("transition_expressions"), py::arg("starting_state"),
           py::arg("final_states"), py::arg("transition_triples"))
      .def("_add_bool_argument_constraint",
           &CpBaseModel::AddBoolArgumentConstraintInternal, py::arg("name"))
      .def("_add_bounded_linear_expression",
           &CpBaseModel::AddBoundedLinearExpressionInternal, py::arg("ble"))
      .def("_add_element", &CpBaseModel::AddElementInternal,
           py::arg("index").none(false), py::arg("expressions"),
           py::arg("target").none(false))
      .def("_add_linear_argument_constraint",
           &CpBaseModel::AddLinearArgumentConstraintInternal,
           py::arg("name").none(false), py::arg("target").none(false))
      .def("_add_inverse", &CpBaseModel::AddInverseInternal, py::arg("direct"),
           py::arg("inverse"))
      .def("_add_reservoir", &CpBaseModel::AddReservoirInternal,
           py::arg("times"), py::arg("level_changes"), py::arg("actives"),
           py::arg("min_level"), py::arg("max_level"))
      .def("_add_table", &CpBaseModel::AddTableInternal, py::arg("expressions"),
           py::arg("values"), py::arg("negated"))
      // Scheduling support.
      .def("_new_interval_var", &CpBaseModel::NewIntervalVarInternal,
           py::arg("name"), py::arg("start"), py::arg("size"), py::arg("end"),
           py::arg("Literals"))
      .def("_add_no_overlap", &CpBaseModel::AddNoOverlapInternal,
           py::arg("intervals"))
      .def("_add_no_overlap_2d", &CpBaseModel::AddNoOverlap2DInternal,
           py::arg("x_intervals"), py::arg("y_intervals"))
      .def("_add_cumulative", &CpBaseModel::AddCumulativeInternal,
           py::arg("intervals"), py::arg("demands"), py::arg("capacity"))
      // Routing support.
      .def("_add_circuit", &CpBaseModel::AddCircuitInternal, py::arg("arcs"))
      .def("_add_routes", &CpBaseModel::AddRoutesInternal, py::arg("arcs"));

  static const char* kConstraintDoc = R"doc(
  Base class for constraints.

  Constraints are built by the CpModel through the add<XXX> methods.
  Once created by the CpModel class, they are automatically added to the model.
  The purpose of this class is to allow specification of enforcement literals
  for this constraint.

      b = model.new_bool_var('b')
      x = model.new_int_var(0, 10, 'x')
      y = model.new_int_var(0, 10, 'y')

      model.add(x + 2 * y == 5).only_enforce_if(b.negated())
    )doc";

  static const char* kConstraintOnlyEnforceIfDoc = R"doc(
  Adds one or more enforcement literals to the constraint.

  This method adds one or more literals (that is, a boolean variable or its
  negation) as enforcement literals. The conjunction of all these literals
  determines whether the constraint is active or not. It acts as an
  implication, so if the conjunction is true, it implies that the constraint
  must be enforced. If it is false, then the constraint is ignored.

  BoolOr, BoolAnd, and linear constraints all support enforcement literals.

  Args:
    *literals: One or more Boolean literals.

  Returns:
    self.)doc";

  py::class_<Constraint, std::shared_ptr<Constraint>>(m, "Constraint",
                                                      kConstraintDoc)
      .def(py::init<std::shared_ptr<CpBaseModel>, int>())
      .def_property_readonly(
          "index", &Constraint::index,
          "Returns the index of the constraint in the model protobuf.")
      .def_property_readonly("model_proto", &Constraint::model_proto,
                             "Returns the model protobuf.")
      .def_property_readonly("proto", &Constraint::proto,
                             py::return_value_policy::reference,
                             py::keep_alive<1, 0>(),
                             "Returns the ConstraintProto of this constraint.")
      .def_property("name", &Constraint::name, &Constraint::SetName,
                    "The name of the constraint.")
      .def(
          "with_name",
          [](Constraint* self, const std::string& name) {
            if (name.empty()) {
              self->ClearName();
            } else {
              self->SetName(name);
            }
            return self;
          },
          "Sets the name of the constraint and returns the constraints")
      .def(
          "only_enforce_if",
          [](std::shared_ptr<Constraint> self,
             std::shared_ptr<Literal> literal) {
            self->proto()->add_enforcement_literal(literal->index());
            return self;
          },
          py::arg("literal"), kConstraintOnlyEnforceIfDoc)
      .def(
          "only_enforce_if",
          [](std::shared_ptr<Constraint> self, bool literal) {
            self->proto()->add_enforcement_literal(
                self->model()->GetOrMakeIndexFromConstant(literal));
            return self;
          },
          py::arg("literal"), kConstraintOnlyEnforceIfDoc)
      .def(
          "only_enforce_if",
          [](std::shared_ptr<Constraint> self,
             const std::vector<std::shared_ptr<Literal>>& literals) {
            for (const std::shared_ptr<Literal>& literal : literals) {
              self->proto()->add_enforcement_literal(literal->index());
            }
          },
          py::arg("literals"), kConstraintOnlyEnforceIfDoc)
      .def(
          "only_enforce_if",
          [](std::shared_ptr<Constraint> self, py::args literals) {
            if (literals.size() == 1 &&
                py::isinstance<py::sequence>(literals[0])) {
              py::sequence seq = literals[0].cast<py::sequence>();
              for (const auto& literal : seq) {
                self->proto()->add_enforcement_literal(
                    self->model()->GetOrMakeBooleanIndex(literal));
              }
            } else {
              for (const auto& literal : literals) {
                self->proto()->add_enforcement_literal(
                    self->model()->GetOrMakeBooleanIndex(literal));
              }
            }
          },
          kConstraintOnlyEnforceIfDoc)
      // Pre PEP8 compatibility.
      .def("Name", &Constraint::name)
      .def("Index", &Constraint::index)
      .def("Proto", &Constraint::proto)
      .def("WithName",
           [](Constraint* self, const std::string& name) {
             if (name.empty()) {
               self->ClearName();
             } else {
               self->SetName(name);
             }
             return self;
           })
      .def("OnlyEnforceIf", [](std::shared_ptr<Constraint> self,
                               py::args literals) {
        if (literals.size() == 1 && py::isinstance<py::sequence>(literals[0])) {
          py::sequence seq = literals[0].cast<py::sequence>();
          for (const auto& literal : seq) {
            self->proto()->add_enforcement_literal(
                self->model()->GetOrMakeBooleanIndex(literal));
          }
        } else {
          for (const auto& literal : literals) {
            self->proto()->add_enforcement_literal(
                self->model()->GetOrMakeBooleanIndex(literal));
          }
        }
      });

  static const char* kIntervalVarDoc = R"doc(
Represents an Interval variable.

An interval variable is both a constraint and a variable. It is defined by
three integer variables: start, size, and end.

It is a constraint because, internally, it enforces that start + size == end.

It is also a variable as it can appear in specific scheduling constraints:
NoOverlap, NoOverlap2D, Cumulative.

Optionally, an enforcement literal can be added to this constraint, in which
case these scheduling constraints will ignore interval variables with
enforcement literals assigned to false. Conversely, these constraints will
also set these enforcement literals to false if they cannot fit these
intervals into the schedule.

Raises:
  ValueError: if start, size, end are not defined, or have the wrong type.  
)doc";

  py::class_<IntervalVar, std::shared_ptr<IntervalVar>>(m, "IntervalVar",
                                                        kIntervalVarDoc)
      .def(py::init<std::shared_ptr<CpModelProto>, int>())
      .def_property_readonly("index", &IntervalVar::index,
                             "Returns the index of the interval variable.")
      .def_property_readonly("model_proto", &IntervalVar::model_proto,
                             "Returns the model protobuf.")
      .def_property_readonly(
          "proto", &IntervalVar::proto, py::return_value_policy::reference,
          py::keep_alive<1, 0>(), "Returns the interval constraint protobuf.")
      .def_property("name", &IntervalVar::name, &IntervalVar::SetName,
                    "The name of the interval variable.")
      .def(
          "start_expr",
          [](std::shared_ptr<IntervalVar> self) -> py::object {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.start().vars().empty()) {
              return py::cast(proto.start().offset());
            } else {
              return py::cast(self->StartExpr());
            }
          },
          "Returns the start expression of the interval variable.")
      .def(
          "size_expr",
          [](std::shared_ptr<IntervalVar> self) -> py::object {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.size().vars().empty()) {
              return py::cast(proto.size().offset());
            } else {
              return py::cast(self->SizeExpr());
            }
          },
          "Returns the size expression of the interval variable.")
      .def(
          "end_expr",
          [](std::shared_ptr<IntervalVar> self) -> py::object {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.end().vars().empty()) {
              return py::cast(proto.end().offset());
            } else {
              return py::cast(self->EndExpr());
            }
          },
          "Returns the end expression of the interval variable.")
      .def("__str__", &IntervalVar::ToString)
      .def("__repr__", &IntervalVar::DebugString)
      .def(py::pickle(
          [](std::shared_ptr<IntervalVar> p) {  // __getstate__
            /* Return a tuple that fully encodes the state of the object */
            return py::make_tuple(p->model_proto(), p->index());
          },
          [](py::tuple t) {  // __setstate__
            if (t.size() != 2) throw std::runtime_error("Invalid state!");

            return std::make_shared<IntervalVar>(
                t[0].cast<std::shared_ptr<CpModelProto>>(), t[1].cast<int>());
          }))
      // Pre PEP8 compatibility layer.
      .def("Proto", &IntervalVar::proto)
      .def("Index", &IntervalVar::index)
      .def("Name", &IntervalVar::name)
      .def("StartExpr",
           [](std::shared_ptr<IntervalVar> self) -> py::object {
             const IntervalConstraintProto& proto = self->proto()->interval();
             if (proto.start().vars().empty()) {
               return py::cast(proto.start().offset());
             } else {
               return py::cast(self->StartExpr());
             }
           })
      .def("SizeExpr",
           [](std::shared_ptr<IntervalVar> self) -> py::object {
             const IntervalConstraintProto& proto = self->proto()->interval();
             if (proto.size().vars().empty()) {
               return py::cast(proto.size().offset());
             } else {
               return py::cast(self->SizeExpr());
             }
           })
      .def("EndExpr", [](std::shared_ptr<IntervalVar> self) -> py::object {
        const IntervalConstraintProto& proto = self->proto()->interval();
        if (proto.end().vars().empty()) {
          return py::cast(proto.end().offset());
        } else {
          return py::cast(self->EndExpr());
        }
      });

  m.def(
      "rebuild_from_linear_expression_proto",
      [](const LinearExpressionProto& proto,
         std::shared_ptr<CpModelProto> model_proto) -> py::object {
        if (proto.vars().empty()) {
          return py::cast(proto.offset());
        } else {
          return py::cast(RebuildFromLinearExpressionProto(proto, model_proto));
        }
      },
      py::arg("proto"), py::arg("model_proto"));

#define IMPORT_PROTO_WRAPPER_CODE
#include "ortools/sat/python/proto_builder_pybind11.h"
#undef IMPORT_PROTO_WRAPPER_CODE
}  // NOLINT(readability/fn_size)

}  // namespace operations_research::sat::python
