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

#include "ortools/sat/python/pybind_linearexpr.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ortools/base/types.h"

#if PY_VERSION_HEX >= 0x030E0000 && !defined(PYPY_VERSION)  // Python >= 3.14
#define Py_BUILD_CORE
#include "internal/pycore_frame.h"
#include "internal/pycore_interpframe.h"
#undef Py_BUILD_CORE
#endif

#define RunningOnValgrind AbslRunningOnValgrind
#define ValgrindSlowdown AbslValgrindSlowdown
#include "absl/base/dynamic_annotations.h"  // IWYU pragma: keep
#undef RunningOnValgrind
#undef ValgrindSlowdown

#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "ortools/sat/python/linear_expr.h"
#include "ortools/sat/python/linear_expr_doc.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/cast.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace operations_research::sat::python {

namespace {

namespace py = pybind11;

void ThrowError(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
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
  const int64_t size = expressions.size();
  if (size != coefficients.size()) {
    ThrowError(PyExc_ValueError,
               absl::StrCat("LinearExpr::weighted_sum() requires the same "
                            "number of arguments and coefficients: ",
                            size, " != ", coefficients.size()));
  }

  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  std::vector<int64_t> int_coeffs;
  std::vector<double> float_coeffs;
  linear_exprs.reserve(size);
  int_coeffs.reserve(size);
  float_coeffs.reserve(size);
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;
  bool fast_coeffs = false;
  const void* raw_coeffs = nullptr;
  Py_ssize_t coeff_stride = 0;

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

  for (int64_t i = 0; i < size; ++i) {
    // --- Parse Coefficient ---
    int64_t c_int = 0;
    double c_float = 0.0;
    bool c_is_float = false;
    bool c_is_zero = false;
    if (fast_coeffs) {
      const char* ptr = static_cast<const char*>(raw_coeffs) + i * coeff_stride;
      if (coeff_type == kInt64) {
        c_int = *reinterpret_cast<const int64_t*>(ptr);
        if (c_int == 0) {
          c_is_zero = true;
        } else {
          c_float = static_cast<double>(c_int);
        }
      } else if (coeff_type == kInt32) {
        c_int = *reinterpret_cast<const int32_t*>(ptr);
        if (c_int == 0) {
          c_is_zero = true;
        } else {
          c_float = static_cast<double>(c_int);
        }
      } else {  // kDouble
        c_float = *reinterpret_cast<const double*>(ptr);
        if (c_float == 0.0) {
          c_is_zero = true;
        } else {
          c_is_float = true;
          has_floats = true;
        }
      }
    } else {
      const py::handle coeff_obj = coefficients[i];
      if (py::isinstance<py::int_>(coeff_obj)) {
        c_int = coeff_obj.cast<int64_t>();
        if (c_int == 0) {
          c_is_zero = true;
        } else {
          c_float = static_cast<double>(c_int);
        }
      } else if (py::isinstance<py::float_>(coeff_obj)) {
        c_float = coeff_obj.cast<double>();
        if (c_float == 0.0) {
          c_is_zero = true;
        } else {
          c_is_float = true;
          has_floats = true;
        }
      } else if (hasattr(coeff_obj, "dtype") &&
                 hasattr(coeff_obj, "is_integer")) {
        if (getattr(coeff_obj, "is_integer")().cast<bool>()) {
          c_int = coeff_obj.cast<int64_t>();
          if (c_int == 0) {
            c_is_zero = true;
          } else {
            c_float = static_cast<double>(c_int);
          }
        } else {
          c_float = coeff_obj.cast<double>();
          if (c_float == 0.0) {
            c_is_zero = true;
          } else {
            c_is_float = true;
            has_floats = true;
          }
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

    // --- Parse Expression ---
    const py::handle expr_obj = expressions[i];
    if (py::isinstance<LinearExpr>(expr_obj)) {
      linear_exprs.push_back(expr_obj.cast<std::shared_ptr<LinearExpr>>());
      if (c_is_float) {
        float_coeffs.push_back(c_float);
      } else {
        int_coeffs.push_back(c_int);
        float_coeffs.push_back(c_float);
      }
    } else if (py::isinstance<py::int_>(expr_obj)) {
      int64_t val = expr_obj.cast<int64_t>();
      if (val == 0) continue;
      if (c_is_float) {
        float_offset += c_float * static_cast<double>(val);
      } else {
        int_offset += c_int * val;
      }
    } else if (py::isinstance<py::float_>(expr_obj)) {
      double val = expr_obj.cast<double>();
      if (val == 0.0) continue;
      has_floats = true;
      float_offset += c_float * val;
    } else if (hasattr(expr_obj, "dtype") && hasattr(expr_obj, "is_integer")) {
      if (getattr(expr_obj, "is_integer")().cast<bool>()) {
        int64_t val = expr_obj.cast<int64_t>();
        if (val == 0) continue;
        if (c_is_float) {
          float_offset += c_float * static_cast<double>(val);
        } else {
          int_offset += c_int * val;
        }
      } else {
        double val = expr_obj.cast<double>();
        if (val == 0.0) continue;
        has_floats = true;
        float_offset += c_float * val;
      }
    } else {
      py::type objtype = py::type::of(expr_obj);
      const std::string type_name =
          objtype.attr("__name__").cast<std::string>();
      ThrowError(PyExc_TypeError,
                 absl::StrCat("LinearExpr::weighted_sum() only accept linear "
                              "expressions and constants as argument: '",
                              absl::CEscape(type_name), "'"));
    }
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

}  // namespace

void DefinePybindWrapperForLinearExpr(py::module& m) {
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
            if (rhs == kint64max || rhs == kint64min) {
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
            if (rhs == kint64min) {
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
            if (rhs == kint64min) {
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
            if (rhs == kint64max) {
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
            if (rhs == kint64max) {
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

  py::class_<SumArray, std::shared_ptr<SumArray>, LinearExpr>(
      m, "SumArray", DOC(operations_research, sat, python, SumArray))
      .def(
          py::init<std::vector<std::shared_ptr<LinearExpr>>, int64_t, double>())
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddInPlace(other) : expr->Add(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Add))
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddIntInPlace(cst) : expr->AddInt(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__add__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(cst)
                                : expr->AddFloat(cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddInPlace(other) : expr->Add(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Add))
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddIntInPlace(cst) : expr->AddInt(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__radd__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(cst)
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
            return IsFree(expr) ? expr->AddInPlace(other->Neg())
                                : expr->Sub(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Sub))
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             int64_t cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddIntInPlace(-cst) : expr->SubInt(cst);
          },
          py::arg("cst"),
          DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def(
          "__sub__",
          [](std::shared_ptr<SumArray> expr,
             double cst) -> std::shared_ptr<LinearExpr> {
            return IsFree(expr) ? expr->AddFloatInPlace(-cst)
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
                        "Evaluating a Literal as a Boolean value is "
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
          "proto", &IntVar::proto, py::return_value_policy::reference_internal,
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
      .def("Proto", &IntVar::proto, py::return_value_policy::reference,
           py::keep_alive<1, 0>(),
           "Returns the IntegerVariableProto of this variable.")
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
}  // NOLINT(readability/fn_size)

}  // namespace operations_research::sat::python
