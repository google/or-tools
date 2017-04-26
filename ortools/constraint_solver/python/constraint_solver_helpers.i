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

// This file contains SWIG macros, so you must %include it and not %import it.
// These macros are put here because they're used in several files.

// We *do* need to use SWIGTYPE_... type names directly, because the
// (recommended replacement) $descriptor macro fails, as of 2014-06, with
// types such as operations_research::Solver.
// The absence of whitespace before 'swiglint' is mandatory.
//swiglint: disable swigtype-name

// Needed by the callback wrapping.
// TODO(user): remove this when we no longer use callbacks in the constraint
// solver and the routing library.
%{
template<>
PyObject* PyObjFrom<int64>(const int64& c) { return PyLong_FromLongLong(c); }
%}

// Conversion utilities, to be able to expose APIs that return a C++ object
// pointer. The PyObjAs template must be able to deal with all such types.
%define PY_CONVERT_HELPER_PTR(CType)
%{
template<>
bool PyObjAs(PyObject *py_obj, operations_research::CType** b) {
  return SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(b),
                         SWIGTYPE_p_operations_research__ ## CType,
                         SWIG_POINTER_EXCEPTION) >= 0;
}
%}
%enddef

// Conversion of IntExpr* and IntVar* are a bit special because of the two
// possible casts from IntExpr and Constraint.
%define PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(Class)
%{
template<>
bool PyObjAs(PyObject *py_obj, operations_research::Class** var) {
  // First, try to interpret the python object as an IntExpr.
  operations_research::IntExpr* t;
  if (SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(&t),
                      SWIGTYPE_p_operations_research__IntExpr,
                      SWIG_POINTER_EXCEPTION) >= 0) {
    if (t == nullptr) return false;
    *var = t->Var();
    return true;
  }
  // Then, try to interpret it as a Constraint.
  operations_research::Constraint* c;
  if (SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(&c),
                      SWIGTYPE_p_operations_research__Constraint,
                      SWIG_POINTER_EXCEPTION) >= 0) {
    if (c == nullptr || c->Var() == nullptr) return false;
    *var = c->Var();
    return true;
  }
  // Give up.
  return false;
}
%}
%enddef
