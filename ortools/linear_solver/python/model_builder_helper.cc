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

// A pybind11 wrapper for model_builder_helper.

#include "ortools/linear_solver/wrappers/model_builder_helper.h"

#include <Python.h>

#if PY_VERSION_HEX >= 0x030E00A7 && !defined(PYPY_VERSION)
#define Py_BUILD_CORE
#include "internal/pycore_frame.h"
#include "internal/pycore_interpframe.h"
#undef Py_BUILD_CORE
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "pybind11/cast.h"
#include "pybind11/eigen.h"
#include "pybind11/functional.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::Eigen::SparseMatrix;
using ::Eigen::VectorXd;
using ::operations_research::MPConstraintProto;
using ::operations_research::MPModelExportOptions;
using ::operations_research::MPModelProto;
using ::operations_research::MPModelRequest;
using ::operations_research::MPSolutionResponse;
using ::operations_research::MPVariableProto;
using ::operations_research::mb::AffineExpr;
using ::operations_research::mb::BoundedLinearExpression;
using ::operations_research::mb::FixedValue;
using ::operations_research::mb::FlatExpr;
using ::operations_research::mb::LinearExpr;
using ::operations_research::mb::ModelBuilderHelper;
using ::operations_research::mb::ModelSolverHelper;
using ::operations_research::mb::SolveStatus;
using ::operations_research::mb::SumArray;
using ::operations_research::mb::Variable;
using ::operations_research::mb::WeightedSumArray;

namespace py = pybind11;

void ThrowError(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
}

const MPModelProto& ToMPModelProto(ModelBuilderHelper* helper) {
  return helper->model();
}

// TODO(user): The interface uses serialized protos because of issues building
// pybind11_protobuf. See
// https://github.com/protocolbuffers/protobuf/issues/9464. After
// pybind11_protobuf is working, this workaround can be removed.
void BuildModelFromSparseData(
    const Eigen::Ref<const VectorXd>& variable_lower_bounds,
    const Eigen::Ref<const VectorXd>& variable_upper_bounds,
    const Eigen::Ref<const VectorXd>& objective_coefficients,
    const Eigen::Ref<const VectorXd>& constraint_lower_bounds,
    const Eigen::Ref<const VectorXd>& constraint_upper_bounds,
    const SparseMatrix<double, Eigen::RowMajor>& constraint_matrix,
    MPModelProto* model_proto) {
  const int num_variables = variable_lower_bounds.size();
  const int num_constraints = constraint_lower_bounds.size();

  if (variable_upper_bounds.size() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid size ", variable_upper_bounds.size(),
                     " for variable_upper_bounds. Expected: ", num_variables));
  }
  if (objective_coefficients.size() != num_variables) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", objective_coefficients.size(),
        " for linear_objective_coefficients. Expected: ", num_variables));
  }
  if (constraint_upper_bounds.size() != num_constraints) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", constraint_upper_bounds.size(),
        " for constraint_upper_bounds. Expected: ", num_constraints));
  }
  if (constraint_matrix.cols() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of columns ", constraint_matrix.cols(),
                     " in constraint_matrix. Expected: ", num_variables));
  }
  if (constraint_matrix.rows() != num_constraints) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of rows ", constraint_matrix.rows(),
                     " in constraint_matrix. Expected: ", num_constraints));
  }

  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* variable = model_proto->add_variable();
    variable->set_lower_bound(variable_lower_bounds[i]);
    variable->set_upper_bound(variable_upper_bounds[i]);
    variable->set_objective_coefficient(objective_coefficients[i]);
  }

  for (int row = 0; row < num_constraints; ++row) {
    MPConstraintProto* constraint = model_proto->add_constraint();
    constraint->set_lower_bound(constraint_lower_bounds[row]);
    constraint->set_upper_bound(constraint_upper_bounds[row]);
    for (SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(
             constraint_matrix, row);
         it; ++it) {
      constraint->add_coefficient(it.value());
      constraint->add_var_index(it.col());
    }
  }
}

std::vector<std::pair<int, double>> SortedGroupedTerms(
    absl::Span<const int> indices, absl::Span<const double> coefficients) {
  CHECK_EQ(indices.size(), coefficients.size());
  std::vector<std::pair<int, double>> terms;
  terms.reserve(indices.size());
  for (int i = 0; i < indices.size(); ++i) {
    terms.emplace_back(indices[i], coefficients[i]);
  }
  std::sort(
      terms.begin(), terms.end(),
      [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
        if (a.first != b.first) return a.first < b.first;
        if (std::abs(a.second) != std::abs(b.second)) {
          return std::abs(a.second) < std::abs(b.second);
        }
        return a.second < b.second;
      });
  int pos = 0;
  for (int i = 0; i < terms.size(); ++i) {
    const int var = terms[i].first;
    double coeff = terms[i].second;
    while (i + 1 < terms.size() && terms[i + 1].first == var) {
      coeff += terms[i + 1].second;
      ++i;
    }
    if (coeff == 0.0) continue;
    terms[pos] = {var, coeff};
    ++pos;
  }
  terms.resize(pos);
  return terms;
}

const char* kLinearExprClassDoc = R"doc(
Holds an linear expression.

A linear expression is built from constants and variables.
For example, `x + 2.0 * (y - z + 1.0)`.

Linear expressions are used in Model models in constraints and in the objective:

  * You can define linear constraints as in:

```
  model.add(x + 2 * y <= 5.0)
  model.add(sum(array_of_vars) == 5.0)
```

  * In Model, the objective is a linear expression:

```
  model.minimize(x + 2.0 * y + z)
```

  * For large arrays, using the LinearExpr class is faster that using the python
  `sum()` function. You can create constraints and the objective from lists of
  linear expressions or coefficients as follows:

```
  model.minimize(model_builder.LinearExpr.sum(expressions))
  model.add(model_builder.LinearExpr.weighted_sum(expressions, coeffs) >= 0)
```
)doc";

const char* kVarClassDoc = R"doc(A variable (continuous or integral).

  A Variable is an object that can take on any integer value within defined
  ranges. Variables appear in constraint like:

      x + y >= 5

  Solving a model is equivalent to finding, for each variable, a single value
  from the set of initial values (called the initial domain), such that the
  model is feasible, or optimal if you provided an objective function.
)doc";

std::shared_ptr<LinearExpr> SumArguments(py::args args,
                                         const py::kwargs& kwargs) {
  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  double float_offset = 0.0;

  if (args.size() == 1 && py::isinstance<py::sequence>(args[0])) {
    // Normal list or tuple argument.
    const py::sequence elements = args[0].cast<py::sequence>();
    linear_exprs.reserve(elements.size());
    for (const py::handle arg : elements) {
      if (py::isinstance<LinearExpr>(arg)) {
        linear_exprs.push_back(arg.cast<std::shared_ptr<LinearExpr>>());
      } else {
        float_offset += arg.cast<double>();
      }
    }
  } else {  // Direct sum(x, y, 3, ..) without [].
    linear_exprs.reserve(args.size());
    for (const py::handle arg : args) {
      if (py::isinstance<LinearExpr>(arg)) {
        linear_exprs.push_back(arg.cast<std::shared_ptr<LinearExpr>>());
      } else {
        float_offset += arg.cast<double>();
      }
    }
  }

  if (kwargs) {
    for (const auto arg : kwargs) {
      const std::string arg_name = std::string(py::str(arg.first));
      if (arg_name == "constant") {
        float_offset += arg.second.cast<double>();
      } else {
        ThrowError(PyExc_ValueError,
                   absl::StrCat("Unknown keyword argument: ", arg_name));
      }
    }
  }

  if (linear_exprs.empty()) {
    return std::make_shared<FixedValue>(float_offset);
  } else if (linear_exprs.size() == 1) {
    if (float_offset == 0.0) {
      return linear_exprs[0];
    } else {
      return std::make_shared<AffineExpr>(linear_exprs[0], 1.0, float_offset);
    }
  } else {
    return std::make_shared<SumArray>(linear_exprs, float_offset);
  }
}

std::shared_ptr<LinearExpr> WeightedSumArguments(py::sequence expressions,
                                                 py::sequence coefficients,
                                                 double offset = 0.0) {
  const size_t size = expressions.size();
  if (size != coefficients.size()) {
    ThrowError(PyExc_ValueError,
               absl::StrCat("LinearExpr::weighted_sum() requires the same "
                            "number of arguments and coefficients: ",
                            size, " != ", coefficients.size()));
  }

  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  std::vector<double> coeffs;
  linear_exprs.reserve(size);
  coeffs.reserve(size);

  bool fast_coeffs = false;
  const void* raw_coeffs = nullptr;
  ssize_t coeff_stride = 0;
  enum { kInt64, kInt32, kDouble } coeff_type = kInt64;

  if (py::isinstance<py::array>(coefficients)) {
    py::array arr = coefficients.cast<py::array>();
    if (arr.ndim() == 1 && arr.size() == size) {
      if (py::isinstance<py::array_t<int64_t>>(arr)) {
        fast_coeffs = true;
        raw_coeffs = arr.data();
        coeff_stride = arr.strides(0);
        coeff_type = kInt64;
      } else if (py::isinstance<py::array_t<int32_t>>(arr)) {
        fast_coeffs = true;
        raw_coeffs = arr.data();
        coeff_stride = arr.strides(0);
        coeff_type = kInt32;
      } else if (py::isinstance<py::array_t<double>>(arr)) {
        fast_coeffs = true;
        raw_coeffs = arr.data();
        coeff_stride = arr.strides(0);
        coeff_type = kDouble;
      }
    }
  }

  for (size_t i = 0; i < size; ++i) {
    double coeff = 0.0;
    bool c_is_zero = false;

    if (fast_coeffs) {
      const char* ptr = static_cast<const char*>(raw_coeffs) + i * coeff_stride;
      if (coeff_type == kInt64) {
        const int64_t c_int = *reinterpret_cast<const int64_t*>(ptr);
        if (c_int == 0) {
          c_is_zero = true;
        } else {
          coeff = static_cast<double>(c_int);
        }
      } else if (coeff_type == kInt32) {
        const int32_t c_int = *reinterpret_cast<const int32_t*>(ptr);
        if (c_int == 0) {
          c_is_zero = true;
        } else {
          coeff = static_cast<double>(c_int);
        }
      } else {  // kDouble
        coeff = *reinterpret_cast<const double*>(ptr);
        if (coeff == 0.0) {
          c_is_zero = true;
        }
      }
    } else {
      const py::handle coeff_obj = coefficients[i];
      if (py::isinstance<py::int_>(coeff_obj) ||
          py::isinstance<py::float_>(coeff_obj) ||
          // Check for NumPy scalars specifically
          (py::hasattr(coeff_obj, "dtype") &&
           !py::isinstance<py::array>(coeff_obj))) {
        coeff = coeff_obj.cast<double>();
        if (coeff == 0.0) {
          c_is_zero = true;
        }
      } else {
        py::type objtype = py::type::of(coeff_obj);
        const std::string type_name =
            objtype.attr("__name__").cast<std::string>();
        ThrowError(
            PyExc_TypeError,
            absl::StrCat("LinearExpr::weighted_sum() only accept constants "
                         "as coefficients: '",
                         absl::CEscape(type_name), "'"));
      }
    }

    if (c_is_zero) continue;

    const py::handle arg = expressions[i];
    if (py::isinstance<LinearExpr>(arg)) {
      linear_exprs.push_back(arg.cast<std::shared_ptr<LinearExpr>>());
      coeffs.push_back(coeff);
    } else {
      offset += arg.cast<double>() * coeff;
    }
  }

  if (linear_exprs.empty()) {
    return std::make_shared<FixedValue>(offset);
  } else if (linear_exprs.size() == 1) {
    return LinearExpr::Affine(linear_exprs[0], coeffs[0], offset);
  } else {
    return std::make_shared<WeightedSumArray>(linear_exprs, coeffs, offset);
  }
}

#if PY_VERSION_HEX >= 0x030E00A7 && !defined(PYPY_VERSION)
bool was_optimized_in_function_call(PyObject* op) {
  PyFrameObject* frame = PyEval_GetFrame();
  if (frame == NULL) {
    return false;
  }
  _PyInterpreterFrame* f = frame->f_frame;
  _PyStackRef* base = _PyFrame_Stackbase(f);
  _PyStackRef* stackpointer = f->stackpointer;

  while (stackpointer > base) {
    stackpointer--;
    if (op == PyStackRef_AsPyObjectBorrow(*stackpointer)) {
      // We want detect if the object is a temporary and borrowed. If so, it
      // should be only referenced once in the stack, but it should not be safe.
      return !PyStackRef_IsHeapSafe(*stackpointer);
    }
  }
  return false;
}

bool IsOnwedExclusivelyThroughPyBind11(PyObject* op) {
#if !defined(Py_GIL_DISABLED)
  return Py_REFCNT(op) == 3;
#else
  // NOTE: the entire ob_ref_shared field must be zero, including flags, to
  // ensure that other threads cannot concurrently create new references to
  // this object.
  return (_Py_IsOwnedByCurrentThread(op) &&
          _Py_atomic_load_uint32_relaxed(&op->ob_ref_local) == 3 &&
          _Py_atomic_load_ssize_relaxed(&op->ob_ref_shared) == 0);
#endif
}

template <class T>
bool IsFree(std::shared_ptr<T> expr) {
  PyObject* op = py::cast(expr).ptr();
  return IsOnwedExclusivelyThroughPyBind11(op) &&
         !was_optimized_in_function_call(op);
}
#else
template <class T>
bool IsFree(std::shared_ptr<T> expr) {
  return Py_REFCNT(py::cast(expr).ptr()) == 4;
}
#endif

PYBIND11_MODULE(model_builder_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  py::class_<LinearExpr, std::shared_ptr<LinearExpr>>(m, "LinearExpr",
                                                      kLinearExprClassDoc)
      .def_static("sum", &SumArguments,
                  "Creates `sum(expressions) [+ constant]`.")
      .def_static(
          "weighted_sum", &WeightedSumArguments,
          "Creates `sum(expressions[i] * coefficients[i]) [+ constant]`.",
          py::arg("expressions"), py::arg("coefficients"), py::kw_only(),
          py::arg("constant") = 0.0)
      .def_static("term", &LinearExpr::Term, py::arg("expr").none(false),
                  py::arg("coeff"), "Returns expr * coeff.")
      .def_static("term", &LinearExpr::Affine, py::arg("expr").none(false),
                  py::arg("coeff"), py::kw_only(), py::arg("constant"),
                  "Returns expr * coeff [+ constant].")
      .def_static("term", &LinearExpr::AffineCst, py::arg("value"),
                  py::arg("coeff"), py::kw_only(), py::arg("constant"),
                  "Returns value * coeff [+ constant].")
      .def_static("affine", &LinearExpr::Affine, py::arg("expr").none(false),
                  py::arg("coeff"), py::arg("constant") = 0.0,
                  "Returns expr * coeff + constant.")
      .def_static("affine", &LinearExpr::AffineCst, py::arg("value"),
                  py::arg("coeff"), py::arg("constant") = 0.0,
                  "Returns value * coeff + constant.")
      .def_static("constant", &LinearExpr::Constant, py::arg("value"),
                  "Returns a constant linear expression.")
      // Methods.
      .def("__str__", &LinearExpr::ToString)
      .def("__repr__", &LinearExpr::DebugString)
      // Operators.
      .def("__add__", &LinearExpr::Add, py::arg("other").none(false))
      .def("__add__", &LinearExpr::AddFloat, py::arg("cst"))
      .def("__radd__", &LinearExpr::AddFloat, py::arg("cst"))
      .def("__sub__", &LinearExpr::Sub, py::arg("other").none(false))
      .def("__sub__", &LinearExpr::SubFloat, py::arg("cst"))
      .def("__rsub__", &LinearExpr::RSubFloat, py::arg("cst"))
      .def("__mul__", &LinearExpr::MulFloat, py::arg("cst"))
      .def("__rmul__", &LinearExpr::MulFloat, py::arg("cst"))
      .def("__truediv__",
           [](std::shared_ptr<LinearExpr> self, double cst) {
             if (cst == 0.0) {
               ThrowError(PyExc_ZeroDivisionError,
                          "Division by zero is not supported.");
             }
             return self->MulFloat(1.0 / cst);
           })
      .def("__neg__", &LinearExpr::Neg)
      // Comparison operators.
      .def("__eq__", &LinearExpr::Eq, py::arg("other").none(false),
           "Creates the constraint `self == other`.")
      .def("__eq__", &LinearExpr::EqCst, py::arg("cst"),
           "Creates the constraint `self == cst`.")
      .def("__le__", &LinearExpr::Le, py::arg("other").none(false),
           "Creates the constraint `self <= other`.")
      .def("__le__", &LinearExpr::LeCst, py::arg("cst"),
           "Creates the constraint `self <= cst`.")
      .def("__ge__", &LinearExpr::Ge, py::arg("other").none(false),
           "Creates the constraint `self >= other`.")
      .def("__ge__", &LinearExpr::GeCst, py::arg("cst"),
           "Creates the constraint `self >= cst`.")
      // Disable other operators as they are not supported.
      .def("__floordiv__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling // on a linear expression is not supported.");
           })
      .def("__mod__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling %% on a linear expression is not supported.");
           })
      .def("__pow__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling ** on a linear expression is not supported.");
           })
      .def("__lshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(
                 PyExc_NotImplementedError,
                 "calling left shift on a linear expression is not supported");
           })
      .def("__rshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(
                 PyExc_NotImplementedError,
                 "calling right shift on a linear expression is not supported");
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
                 "calling abs() on a linear expression is not supported.");
           })
      .def("__bool__", [](std::shared_ptr<LinearExpr> /*self*/) {
        ThrowError(PyExc_NotImplementedError,
                   "Evaluating a LinearExpr instance as a Boolean is "
                   "not supported.");
      });

  // Expose Internal classes, mostly for testing.
  py::class_<FlatExpr, std::shared_ptr<FlatExpr>, LinearExpr>(m, "FlatExpr")
      .def(py::init<std::shared_ptr<LinearExpr>>())
      .def(py::init<std::shared_ptr<LinearExpr>, std::shared_ptr<LinearExpr>>())
      .def(py::init<const std::vector<std::shared_ptr<Variable>>&,
                    const std::vector<double>&, double>())
      .def(py::init<double>())
      .def_property_readonly("vars", &FlatExpr::vars)
      .def("variable_indices", &FlatExpr::VarIndices)
      .def_property_readonly("coeffs", &FlatExpr::coeffs)
      .def_property_readonly("offset", &FlatExpr::offset);

  py::class_<SumArray, std::shared_ptr<SumArray>, LinearExpr>(
      m, "SumArray", "Holds a sum of linear expressions, and constants.")
      .def(py::init<std::vector<std::shared_ptr<LinearExpr>>, double>())
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddInPlace(other) : expr->Add(other);
          },
          py::arg("other").none(false),
          "Returns the sum of `self` and `other`.")
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(cst)
                                : expr->AddFloat(cst);
          },
          py::arg("cst"), "Returns `self` + `cst`.")
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddInPlace(other) : expr->Add(other);
          },
          py::arg("cst"), "Returns `self` + `cst`.")
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(cst)
                                : expr->AddFloat(cst);
          },
          py::arg("cst"), "Returns `self` + `cst`.")
      .def(
          "__iadd__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return expr->AddInPlace(other);
          },
          py::arg("other").none(false),
          "Returns the sum of `self` and `other`.")
      .def(
          "__iadd__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddFloatInPlace(cst);
          },
          py::arg("cst"), "Returns `self` + `cst`.")
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddInPlace(other->Neg())
                                : expr->Sub(other);
          },
          py::arg("other").none(false), "Returns `self` - `other`.")
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(-cst)
                                : expr->SubFloat(cst);
          },
          py::arg("cst"), "Returns `self` - `cst`.")
      .def(
          "__isub__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return expr->AddInPlace(other->Neg());
          },
          py::arg("other").none(false), "Returns `self` - `other`.")
      .def(
          "__isub__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return expr->AddFloatInPlace(-cst);
          },
          py::arg("cst"), "Returns `self` - `cst`.")
      .def_property_readonly(
          "num_exprs", &SumArray::num_exprs,
          "Returns the number of linear expressions in the sum.")
      .def_property_readonly("offset", &SumArray::offset,
                             "Returns the offset of the sum.");

  py::class_<AffineExpr, std::shared_ptr<AffineExpr>, LinearExpr>(m,
                                                                  "AffineExpr")
      .def(py::init<std::shared_ptr<LinearExpr>, double, double>())
      .def_property_readonly("expression", &AffineExpr ::expression)
      .def_property_readonly("coefficient", &AffineExpr::coefficient)
      .def_property_readonly("offset", &AffineExpr::offset);

  py::class_<Variable, std::shared_ptr<Variable>, LinearExpr>(m, "Variable",
                                                              kVarClassDoc)
      .def(py::init<ModelBuilderHelper*, int>())
      .def(py::init<ModelBuilderHelper*, double, double, bool>())
      .def(py::init<ModelBuilderHelper*, double, double, bool, std::string>())
      .def(py::init<ModelBuilderHelper*, int64_t, int64_t, bool>())
      .def(py::init<ModelBuilderHelper*, int64_t, int64_t, bool, std::string>())
      .def_property_readonly("index", &Variable::index,
                             "The index of the variable in the model.")
      .def_property_readonly("helper", &Variable::helper,
                             "The ModelBuilderHelper instance.")
      .def_property("name", &Variable::name, &Variable::SetName,
                    "The name of the variable in the model.")
      .def_property("lower_bound", &Variable::lower_bounds,
                    &Variable::SetLowerBound)
      .def_property("upper_bound", &Variable::upper_bound,
                    &Variable::SetUpperBound)
      .def_property("is_integral", &Variable::is_integral,
                    &Variable::SetIsIntegral)
      .def_property("objective_coefficient", &Variable::objective_coefficient,
                    &Variable::SetObjectiveCoefficient)
      .def("__str__", &Variable::ToString)
      .def("__repr__", &Variable::DebugString)
      .def("__hash__", [](const Variable& self) {
        return absl::HashOf(std::make_tuple(self.helper(), self.index()));
      });

  py::class_<BoundedLinearExpression, std::shared_ptr<BoundedLinearExpression>>(
      m, "BoundedLinearExpression")
      .def(py::init<std::shared_ptr<LinearExpr>, double, double>())
      .def(py::init<std::shared_ptr<LinearExpr>, std::shared_ptr<LinearExpr>,
                    double, double>())
      .def(py::init<std::shared_ptr<LinearExpr>, int64_t, int64_t>())
      .def(py::init<std::shared_ptr<LinearExpr>, std::shared_ptr<LinearExpr>,
                    int64_t, int64_t>())
      .def_property_readonly("vars", &BoundedLinearExpression::vars)
      .def_property_readonly("coeffs", &BoundedLinearExpression::coeffs)
      .def_property_readonly("lower_bound",
                             &BoundedLinearExpression::lower_bound)
      .def_property_readonly("upper_bound",
                             &BoundedLinearExpression::upper_bound)
      .def("__bool__",
           [](const BoundedLinearExpression& self) {
             bool result;
             if (self.CastToBool(&result)) return result;
             ThrowError(PyExc_NotImplementedError,
                        absl::StrCat("Evaluating a BoundedLinearExpression '",
                                     self.ToString(),
                                     "'instance as a Boolean is "
                                     "not supported."));
             return false;
           })
      .def("__str__", &BoundedLinearExpression::ToString)
      .def("__repr__", &BoundedLinearExpression::DebugString);

  m.def("to_mpmodel_proto", &ToMPModelProto, py::arg("helper"));

  py::class_<MPModelExportOptions>(m, "MPModelExportOptions")
      .def(py::init<>())
      .def_readwrite("obfuscate", &MPModelExportOptions::obfuscate)
      .def_readwrite("log_invalid_names",
                     &MPModelExportOptions::log_invalid_names)
      .def_readwrite("show_unused_variables",
                     &MPModelExportOptions::show_unused_variables)
      .def_readwrite("max_line_length", &MPModelExportOptions::max_line_length);

  py::class_<ModelBuilderHelper>(m, "ModelBuilderHelper")
      .def(py::init<>())
      .def("overwrite_model", &ModelBuilderHelper::OverwriteModel,
           py::arg("other_helper"))
      .def("export_to_mps_string", &ModelBuilderHelper::ExportToMpsString,
           py::arg("options") = MPModelExportOptions())
      .def("export_to_lp_string", &ModelBuilderHelper::ExportToLpString,
           py::arg("options") = MPModelExportOptions())
      .def("write_to_mps_file", &ModelBuilderHelper::WriteToMpsFile,
           py::arg("filename"), py::arg("options") = MPModelExportOptions())
      .def("read_model_from_proto_file",
           &ModelBuilderHelper::ReadModelFromProtoFile, py::arg("filename"))
      .def("write_model_to_proto_file",
           &ModelBuilderHelper::WriteModelToProtoFile, py::arg("filename"))
      .def("import_from_mps_string", &ModelBuilderHelper::ImportFromMpsString,
           py::arg("mps_string"))
      .def("import_from_mps_file", &ModelBuilderHelper::ImportFromMpsFile,
           py::arg("mps_file"))
      .def("import_from_lp_string", &ModelBuilderHelper::ImportFromLpString,
           py::arg("lp_string"))
      .def("import_from_lp_file", &ModelBuilderHelper::ImportFromLpFile,
           py::arg("lp_file"))
      .def(
          "fill_model_from_sparse_data",
          [](ModelBuilderHelper* helper,
             const Eigen::Ref<const VectorXd>& variable_lower_bounds,
             const Eigen::Ref<const VectorXd>& variable_upper_bounds,
             const Eigen::Ref<const VectorXd>& objective_coefficients,
             const Eigen::Ref<const VectorXd>& constraint_lower_bounds,
             const Eigen::Ref<const VectorXd>& constraint_upper_bounds,
             const SparseMatrix<double, Eigen::RowMajor>& constraint_matrix) {
            BuildModelFromSparseData(
                variable_lower_bounds, variable_upper_bounds,
                objective_coefficients, constraint_lower_bounds,
                constraint_upper_bounds, constraint_matrix,
                helper->mutable_model());
          },
          py::arg("variable_lower_bound"), py::arg("variable_upper_bound"),
          py::arg("objective_coefficients"), py::arg("constraint_lower_bounds"),
          py::arg("constraint_upper_bounds"), py::arg("constraint_matrix"))
      .def("add_var", &ModelBuilderHelper::AddVar)
      .def("add_var_array",
           [](ModelBuilderHelper* helper, std::vector<size_t> shape, double lb,
              double ub, bool is_integral, absl::string_view name_prefix) {
             int size = shape[0];
             for (int i = 1; i < shape.size(); ++i) {
               size *= shape[i];
             }
             py::array_t<int> result(size);
             py::buffer_info info = result.request();
             result.resize(shape);
             auto ptr = static_cast<int*>(info.ptr);
             for (int i = 0; i < size; ++i) {
               const int index = helper->AddVar();
               ptr[i] = index;
               helper->SetVarLowerBound(index, lb);
               helper->SetVarUpperBound(index, ub);
               helper->SetVarIntegrality(index, is_integral);
               if (!name_prefix.empty()) {
                 helper->SetVarName(index, absl::StrCat(name_prefix, i));
               }
             }
             return result;
           })
      .def("add_var_array_with_bounds",
           [](ModelBuilderHelper* helper, py::array_t<double> lbs,
              py::array_t<double> ubs, py::array_t<bool> are_integral,
              absl::string_view name_prefix) {
             py::buffer_info buf_lbs = lbs.request();
             py::buffer_info buf_ubs = ubs.request();
             py::buffer_info buf_are_integral = are_integral.request();
             const int size = buf_lbs.size;
             if (size != buf_ubs.size || size != buf_are_integral.size) {
               throw std::runtime_error("Input sizes must match");
             }
             const auto shape = buf_lbs.shape;
             if (shape != buf_ubs.shape || shape != buf_are_integral.shape) {
               throw std::runtime_error("Input shapes must match");
             }

             auto lower_bounds = static_cast<double*>(buf_lbs.ptr);
             auto upper_bounds = static_cast<double*>(buf_ubs.ptr);
             auto integers = static_cast<bool*>(buf_are_integral.ptr);
             py::array_t<int> result(size);
             result.resize(shape);
             py::buffer_info result_info = result.request();
             auto ptr = static_cast<int*>(result_info.ptr);
             for (int i = 0; i < size; ++i) {
               const int index = helper->AddVar();
               ptr[i] = index;
               helper->SetVarLowerBound(index, lower_bounds[i]);
               helper->SetVarUpperBound(index, upper_bounds[i]);
               helper->SetVarIntegrality(index, integers[i]);
               if (!name_prefix.empty()) {
                 helper->SetVarName(index, absl::StrCat(name_prefix, i));
               }
             }
             return result;
           })
      .def("set_var_lower_bound", &ModelBuilderHelper::SetVarLowerBound,
           py::arg("var_index"), py::arg("lb"))
      .def("set_var_upper_bound", &ModelBuilderHelper::SetVarUpperBound,
           py::arg("var_index"), py::arg("ub"))
      .def("set_var_integrality", &ModelBuilderHelper::SetVarIntegrality,
           py::arg("var_index"), py::arg("is_integer"))
      .def("set_var_objective_coefficient",
           &ModelBuilderHelper::SetVarObjectiveCoefficient,
           py::arg("var_index"), py::arg("coeff"))
      .def("set_objective_coefficients",
           [](ModelBuilderHelper* helper, const std::vector<int>& indices,
              const std::vector<double>& coefficients) {
             for (const auto& [i, c] :
                  SortedGroupedTerms(indices, coefficients)) {
               helper->SetVarObjectiveCoefficient(i, c);
             }
           })
      .def("set_var_name", &ModelBuilderHelper::SetVarName,
           py::arg("var_index"), py::arg("name"))
      .def("var_lower_bound", &ModelBuilderHelper::VarLowerBound,
           py::arg("var_index"))
      .def("var_upper_bound", &ModelBuilderHelper::VarUpperBound,
           py::arg("var_index"))
      .def("var_is_integral", &ModelBuilderHelper::VarIsIntegral,
           py::arg("var_index"))
      .def("var_objective_coefficient",
           &ModelBuilderHelper::VarObjectiveCoefficient, py::arg("var_index"))
      .def("var_name", &ModelBuilderHelper::VarName, py::arg("var_index"))
      .def("add_linear_constraint", &ModelBuilderHelper::AddLinearConstraint)
      .def("set_constraint_lower_bound",
           &ModelBuilderHelper::SetConstraintLowerBound, py::arg("ct_index"),
           py::arg("lb"))
      .def("set_constraint_upper_bound",
           &ModelBuilderHelper::SetConstraintUpperBound, py::arg("ct_index"),
           py::arg("ub"))
      .def("add_term_to_constraint", &ModelBuilderHelper::AddConstraintTerm,
           py::arg("ct_index"), py::arg("var_index"), py::arg("coeff"))
      .def("add_terms_to_constraint",
           [](ModelBuilderHelper* helper, int ct_index,
              const std::vector<std::shared_ptr<Variable>>& vars,
              const std::vector<double>& coefficients) {
             for (int i = 0; i < vars.size(); ++i) {
               helper->AddConstraintTerm(ct_index, vars[i]->index(),
                                         coefficients[i]);
             }
           })
      .def("safe_add_term_to_constraint",
           &ModelBuilderHelper::SafeAddConstraintTerm, py::arg("ct_index"),
           py::arg("var_index"), py::arg("coeff"))
      .def("set_constraint_name", &ModelBuilderHelper::SetConstraintName,
           py::arg("ct_index"), py::arg("name"))
      .def("set_constraint_coefficient",
           &ModelBuilderHelper::SetConstraintCoefficient, py::arg("ct_index"),
           py::arg("var_index"), py::arg("coeff"))
      .def("constraint_lower_bound", &ModelBuilderHelper::ConstraintLowerBound,
           py::arg("ct_index"))
      .def("constraint_upper_bound", &ModelBuilderHelper::ConstraintUpperBound,
           py::arg("ct_index"))
      .def("constraint_name", &ModelBuilderHelper::ConstraintName,
           py::arg("ct_index"))
      .def("constraint_var_indices", &ModelBuilderHelper::ConstraintVarIndices,
           py::arg("ct_index"))
      .def("constraint_coefficients",
           &ModelBuilderHelper::ConstraintCoefficients, py::arg("ct_index"))
      .def("add_enforced_linear_constraint",
           &ModelBuilderHelper::AddEnforcedLinearConstraint)
      .def("is_enforced_linear_constraint",
           &ModelBuilderHelper::IsEnforcedConstraint)
      .def("set_enforced_constraint_lower_bound",
           &ModelBuilderHelper::SetEnforcedConstraintLowerBound,
           py::arg("ct_index"), py::arg("lb"))
      .def("set_enforced_constraint_upper_bound",
           &ModelBuilderHelper::SetEnforcedConstraintUpperBound,
           py::arg("ct_index"), py::arg("ub"))
      .def("add_term_to_enforced_constraint",
           &ModelBuilderHelper::AddEnforcedConstraintTerm, py::arg("ct_index"),
           py::arg("var_index"), py::arg("coeff"))
      .def("add_terms_to_enforced_constraint",
           [](ModelBuilderHelper* helper, int ct_index,
              const std::vector<std::shared_ptr<Variable>>& vars,
              const std::vector<double>& coefficients) {
             for (int i = 0; i < vars.size(); ++i) {
               helper->AddEnforcedConstraintTerm(ct_index, vars[i]->index(),
                                                 coefficients[i]);
             }
           })
      .def("safe_add_term_to_enforced_constraint",
           &ModelBuilderHelper::SafeAddEnforcedConstraintTerm,
           py::arg("ct_index"), py::arg("var_index"), py::arg("coeff"))
      .def("set_enforced_constraint_name",
           &ModelBuilderHelper::SetEnforcedConstraintName, py::arg("ct_index"),
           py::arg("name"))
      .def("set_enforced_constraint_coefficient",
           &ModelBuilderHelper::SetEnforcedConstraintCoefficient,
           py::arg("ct_index"), py::arg("var_index"), py::arg("coeff"))
      .def("enforced_constraint_lower_bound",
           &ModelBuilderHelper::EnforcedConstraintLowerBound,
           py::arg("ct_index"))
      .def("enforced_constraint_upper_bound",
           &ModelBuilderHelper::EnforcedConstraintUpperBound,
           py::arg("ct_index"))
      .def("enforced_constraint_name",
           &ModelBuilderHelper::EnforcedConstraintName, py::arg("ct_index"))
      .def("enforced_constraint_var_indices",
           &ModelBuilderHelper::EnforcedConstraintVarIndices,
           py::arg("ct_index"))
      .def("enforced_constraint_coefficients",
           &ModelBuilderHelper::EnforcedConstraintCoefficients,
           py::arg("ct_index"))
      .def("set_enforced_constraint_indicator_variable_index",
           &ModelBuilderHelper::SetEnforcedIndicatorVariableIndex,
           py::arg("ct_index"), py::arg("var_index"))
      .def("set_enforced_constraint_indicator_value",
           &ModelBuilderHelper::SetEnforcedIndicatorValue, py::arg("ct_index"),
           py::arg("positive"))
      .def("enforced_constraint_indicator_variable_index",
           &ModelBuilderHelper::EnforcedIndicatorVariableIndex,
           py::arg("ct_index"))
      .def("enforced_constraint_indicator_value",
           &ModelBuilderHelper::EnforcedIndicatorValue, py::arg("ct_index"))
      .def("num_variables", &ModelBuilderHelper::num_variables)
      .def("num_constraints", &ModelBuilderHelper::num_constraints)
      .def("name", &ModelBuilderHelper::name)
      .def("set_name", &ModelBuilderHelper::SetName, py::arg("name"))
      .def("clear_objective", &ModelBuilderHelper::ClearObjective)
      .def("maximize", &ModelBuilderHelper::maximize)
      .def("set_maximize", &ModelBuilderHelper::SetMaximize,
           py::arg("maximize"))
      .def("set_objective_offset", &ModelBuilderHelper::SetObjectiveOffset,
           py::arg("offset"))
      .def("objective_offset", &ModelBuilderHelper::ObjectiveOffset)
      .def("clear_hints", &ModelBuilderHelper::ClearHints)
      .def("add_hint", &ModelBuilderHelper::AddHint, py::arg("var_index"),
           py::arg("var_value"));

  py::enum_<SolveStatus>(m, "SolveStatus")
      .value("OPTIMAL", SolveStatus::OPTIMAL)
      .value("FEASIBLE", SolveStatus::FEASIBLE)
      .value("INFEASIBLE", SolveStatus::INFEASIBLE)
      .value("UNBOUNDED", SolveStatus::UNBOUNDED)
      .value("ABNORMAL", SolveStatus::ABNORMAL)
      .value("NOT_SOLVED", SolveStatus::NOT_SOLVED)
      .value("MODEL_IS_VALID", SolveStatus::MODEL_IS_VALID)
      .value("CANCELLED_BY_USER", SolveStatus::CANCELLED_BY_USER)
      .value("UNKNOWN_STATUS", SolveStatus::UNKNOWN_STATUS)
      .value("MODEL_INVALID", SolveStatus::MODEL_INVALID)
      .value("INVALID_SOLVER_PARAMETERS",
             SolveStatus::INVALID_SOLVER_PARAMETERS)
      .value("SOLVER_TYPE_UNAVAILABLE", SolveStatus::SOLVER_TYPE_UNAVAILABLE)
      .value("INCOMPATIBLE_OPTIONS", SolveStatus::INCOMPATIBLE_OPTIONS)
      .export_values();

  py::class_<ModelSolverHelper>(m, "ModelSolverHelper")
      .def(py::init<const std::string&>())
      .def("solver_is_supported", &ModelSolverHelper::SolverIsSupported)
      .def("solve", &ModelSolverHelper::Solve, py::arg("model"),
           // The GIL is released during the solve to allow Python threads to do
           // other things in parallel, e.g., log and interrupt.
           py::call_guard<py::gil_scoped_release>())
      .def("solve_serialized_request",
           [](ModelSolverHelper* solver, absl::string_view request_str) {
             std::string result;
             {
               // The GIL is released during the solve to allow Python threads
               // to do other things in parallel, e.g., log and interrupt.
               py::gil_scoped_release release;
               MPModelRequest request;
               if (!request.ParseFromString(std::string(request_str))) {
                 throw std::invalid_argument(
                     "Unable to parse request as MPModelRequest.");
               }
               std::optional<MPSolutionResponse> solution =
                   solver->SolveRequest(request);
               if (solution.has_value()) {
                 result = solution.value().SerializeAsString();
               }
             }
             return py::bytes(result);
           })
      .def("interrupt_solve", &ModelSolverHelper::InterruptSolve,
           "Returns true if the interrupt signal was correctly sent, that is, "
           "if the underlying solver supports it.")
      .def("set_log_callback", &ModelSolverHelper::SetLogCallback)
      .def("clear_log_callback", &ModelSolverHelper::ClearLogCallback)
      .def("set_time_limit_in_seconds",
           &ModelSolverHelper::SetTimeLimitInSeconds, py::arg("limit"))
      .def("set_solver_specific_parameters",
           &ModelSolverHelper::SetSolverSpecificParameters,
           py::arg("solver_specific_parameters"))
      .def("enable_output", &ModelSolverHelper::EnableOutput, py::arg("output"))
      .def("has_solution", &ModelSolverHelper::has_solution)
      .def("has_response", &ModelSolverHelper::has_response)
      .def("response", &ModelSolverHelper::response)
      .def("status", &ModelSolverHelper::status)
      .def("status_string", &ModelSolverHelper::status_string)
      .def("wall_time", &ModelSolverHelper::wall_time)
      .def("user_time", &ModelSolverHelper::user_time)
      .def("objective_value", &ModelSolverHelper::objective_value)
      .def("best_objective_bound", &ModelSolverHelper::best_objective_bound)
      .def("variable_value", &ModelSolverHelper::variable_value,
           py::arg("var_index"))
      .def("expression_value",
           [](const ModelSolverHelper& helper,
              std::shared_ptr<LinearExpr> expr) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             return helper.expression_value(expr);
           })
      .def("reduced_cost", &ModelSolverHelper::reduced_cost,
           py::arg("var_index"))
      .def("dual_value", &ModelSolverHelper::dual_value, py::arg("ct_index"))
      .def("activity", &ModelSolverHelper::activity, py::arg("ct_index"))
      .def("variable_values",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.variable_value_size());
             for (int i = 0; i < response.variable_value_size(); ++i) {
               vec[i] = response.variable_value(i);
             }
             return vec;
           })
      .def("reduced_costs",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.reduced_cost_size());
             for (int i = 0; i < response.reduced_cost_size(); ++i) {
               vec[i] = response.reduced_cost(i);
             }
             return vec;
           })
      .def("dual_values", [](const ModelSolverHelper& helper) {
        if (!helper.has_response()) {
          throw std::logic_error(
              "Accessing a solution value when none has been found.");
        }
        const MPSolutionResponse& response = helper.response();
        Eigen::VectorXd vec(response.dual_value_size());
        for (int i = 0; i < response.dual_value_size(); ++i) {
          vec[i] = response.dual_value(i);
        }
        return vec;
      });
}  // NOLINT(readability/fn_size)
