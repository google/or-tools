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

#include "ortools/sat/python/pybind_constraint.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/types.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/python/linear_expr.h"
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

namespace operations_research::sat::python {

namespace {

namespace py = pybind11;

void ThrowError(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
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
    if (PositiveRef(literal->index()) >= model_proto_->variables_size()) {
      ThrowError(PyExc_TypeError, absl::StrCat("Invalid boolean literal: ",
                                               literal->ToString()));
    }
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
    proto->mutable_vars()->Reserve(vars.size());
    for (const auto& var : vars) {
      proto->add_vars(var->index());
    }
    proto->mutable_coeffs()->Reserve(coeffs.size());
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
    if (bound == kint64min || bound == kint64max) {
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
  ct->mutable_inverse()->mutable_f_direct()->Reserve(direct.size());
  for (const auto& var : direct) {
    ct->mutable_inverse()->add_f_direct(GetOrMakeVariableIndex(var));
  }
  ct->mutable_inverse()->mutable_f_inverse()->Reserve(inverse.size());
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
  proto->mutable_time_exprs()->Reserve(times.size());
  for (const auto& time : times) {
    LinearExprToProto(time, 1, proto->add_time_exprs());
  }
  proto->mutable_level_changes()->Reserve(level_changes.size());
  for (const auto& change : level_changes) {
    LinearExprToProto(change, 1, proto->add_level_changes());
  }
  proto->mutable_active_literals()->Reserve(actives.size());
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

  std::vector<std::shared_ptr<Literal>> PresenceLiterals() const {
    std::vector<std::shared_ptr<Literal>> literals;
    for (const int lit : proto()->enforcement_literal()) {
      if (RefIsPositive(lit)) {
        literals.push_back(std::make_shared<IntVar>(model_proto_, lit));
      } else {
        literals.push_back(std::make_shared<NotBooleanVariable>(
            model_proto_, NegatedRef(lit)));
      }
    }
    return literals;
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

}  // namespace

void DefinePybindWrapperForConstraints(py::module& m) {
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
                             py::return_value_policy::reference_internal,
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
      .def_property_readonly("proto", &IntervalVar::proto,
                             py::return_value_policy::reference_internal,
                             "Returns the interval constraint protobuf.")
      .def_property("name", &IntervalVar::name, &IntervalVar::SetName,
                    "The name of the interval variable.")
      .def(
          "start_expr",
          [](const std::shared_ptr<IntervalVar>& self)
              -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.start().vars().empty()) {
              return proto.start().offset();
            } else {
              return self->StartExpr();
            }
          },
          "Returns the start expression of the interval variable.")
      .def(
          "size_expr",
          [](const std::shared_ptr<IntervalVar>& self)
              -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.size().vars().empty()) {
              return proto.size().offset();
            } else {
              return self->SizeExpr();
            }
          },
          "Returns the size expression of the interval variable.")
      .def(
          "end_expr",
          [](const std::shared_ptr<IntervalVar>& self)
              -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
            const IntervalConstraintProto& proto = self->proto()->interval();
            if (proto.end().vars().empty()) {
              return proto.end().offset();
            } else {
              return self->EndExpr();
            }
          },
          "Returns the end expression of the interval variable.")
      .def("presence_literals", &IntervalVar::PresenceLiterals,
           "Returns the list of enforcement literals of the interval variable.")
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
           [](const std::shared_ptr<IntervalVar>& self)
               -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
             const IntervalConstraintProto& proto = self->proto()->interval();
             if (proto.start().vars().empty()) {
               return proto.start().offset();
             } else {
               return self->StartExpr();
             }
           })
      .def("SizeExpr",
           [](const std::shared_ptr<IntervalVar>& self)
               -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
             const IntervalConstraintProto& proto = self->proto()->interval();
             if (proto.size().vars().empty()) {
               return proto.size().offset();
             } else {
               return self->SizeExpr();
             }
           })
      .def("EndExpr",
           [](const std::shared_ptr<IntervalVar>& self)
               -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
             const IntervalConstraintProto& proto = self->proto()->interval();
             if (proto.end().vars().empty()) {
               return proto.end().offset();
             } else {
               return self->EndExpr();
             }
           });

  m.def(
      "rebuild_from_linear_expression_proto",
      [](const LinearExpressionProto& proto,
         std::shared_ptr<CpModelProto> model_proto)
          -> std::variant<int64_t, std::shared_ptr<LinearExpr>> {
        if (proto.vars().empty()) {
          return proto.offset();
        } else {
          return RebuildFromLinearExpressionProto(proto,
                                                  std::move(model_proto));
        }
      },
      py::arg("proto"), py::arg("model_proto"));

  m.def(
      "prettyprint_model_proto",
      [](const std::shared_ptr<CpModelProto>& model_proto) -> std::string {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
        return PrettyPrintModelProto(*model_proto);
#else
        throw std::runtime_error("unsupported: no proto descriptors");
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
      },
      py::arg("model_proto"));
}

}  // namespace operations_research::sat::python
