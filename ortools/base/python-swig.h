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

// Static part of SWIG-generated C++ wrapper for Python (_module_name.cc).
//
// This file should only be included in base.i inside Python-specific part:
//   #ifdef SWIGPYTHON
//   %{
//   #include "ortools/base/swig/python-swig.cc"
//   %}
//   #endif
// It has no XXX_H_ guard because SWIG protects all %include'd files to be used
// only once.

#ifndef OR_TOOLS_BASE_PYTHON_SWIG_H_
#define OR_TOOLS_BASE_PYTHON_SWIG_H_

#if PY_VERSION_HEX >= 0x03030000  // Py3.3+
// Use Py3 unicode str() type for C++ strings.
#ifdef PyString_FromStringAndSize
#undef PyString_FromStringAndSize
#endif
#define PyString_FromStringAndSize PyUnicode_FromStringAndSize

#ifdef PyString_AsString
#undef PyString_AsString
#endif
#define PyString_AsString PyUnicode_AsUTF8

#ifdef PyString_AsStringAndSize
#undef PyString_AsStringAndSize
#endif
static inline int PyString_AsStringAndSize(PyObject* obj, char** buf,
                                           Py_ssize_t* psize) {
  if (PyUnicode_Check(obj)) {
    *buf = PyUnicode_AsUTF8AndSize(obj, psize);
    return *buf == NULL ? -1 : 0;
  } else if (PyBytes_Check(obj)) {
    return PyBytes_AsStringAndSize(obj, buf, psize);
  }
  PyErr_SetString(PyExc_TypeError, "Expecting str or bytes");
  return -1;
}
#endif  // Py3.3+

template <class T>
inline bool PyObjAs(PyObject* pystr, T* cstr) {
  T::undefined;  // You need to define specialization PyObjAs<T>
}
template <class T>
inline PyObject* PyObjFrom(const T& c) {
  T::undefined;  // You need to define specialization PyObjFrom<T>
}

#ifdef HAS_GLOBAL_STRING
template <>
inline bool PyObjAs(PyObject* pystr, ::std::string* cstr) {
  char* buf;
  Py_ssize_t len;
#if PY_VERSION_HEX >= 0x03030000
  if (PyUnicode_Check(pystr)) {
    buf = PyUnicode_AsUTF8AndSize(pystr, &len);
    if (!buf) return false;
  } else  // NOLINT
#endif
      if (PyBytes_AsStringAndSize(pystr, &buf, &len) == -1)
    return false;
  if (cstr) cstr->assign(buf, len);
  return true;
}
#endif
template <class T>
inline bool PyObjAs(PyObject* pystr, std::string* cstr) {
  char* buf;
  Py_ssize_t len;
#if PY_VERSION_HEX >= 0x03030000
  if (PyUnicode_Check(pystr)) {
    buf = PyUnicode_AsUTF8AndSize(pystr, &len);
    if (!buf) return false;
  } else  // NOLINT
#endif
      if (PyBytes_AsStringAndSize(pystr, &buf, &len) == -1)
    return false;
  if (cstr) cstr->assign(buf, len);
  return true;
}
#ifdef HAS_GLOBAL_STRING
template <>
inline PyObject* PyObjFrom(const ::std::string& c) {
  return PyString_FromStringAndSize(c.data(), c.size());
}
#endif
template <>
inline PyObject* PyObjFrom(const std::string& c) {
  return PyString_FromStringAndSize(c.data(), c.size());
}

// Do numeric specialization.

#include <limits>

template <>
inline bool PyObjAs(PyObject* py, int* c) {
  long i = PyInt_AsLong(py);        // NOLINT
  if (i == -1 && PyErr_Occurred())  // TypeError or OverflowError.
    return false;                   // Not a Python int.
  if (i < std::numeric_limits<int>::min() ||
      i > std::numeric_limits<int>::max())
    return false;  // Not C int.
  if (c) *c = static_cast<int>(i);
  return true;
}

template <>
inline bool PyObjAs(PyObject* py, unsigned int* c) {
  long i = PyInt_AsLong(py);                      // NOLINT
  if (i == -1 && PyErr_Occurred()) return false;  // Not a Python int.
  if (i < 0 || i > std::numeric_limits<unsigned int>::max()) return false;
  if (c) *c = static_cast<unsigned int>(i);
  return true;
}

template <>
inline bool PyObjAs(PyObject* py, long* c) {             // NOLINT
  long i = PyInt_AsLong(py);                      // NOLINT
  if (i == -1 && PyErr_Occurred()) return false;  // Not a Python int.
  if (c) *c = i;
  return true;
}

template <>
inline bool PyObjAs(PyObject* py, long long* c) {  // NOLINT
  long long i;                              // NOLINT
#if PY_MAJOR_VERSION < 3
  if (PyInt_Check(py)) {
    i = PyInt_AsLong(py);
  } else {
    if (!PyLong_Check(py)) return false;  // Not a Python long.
#else
  {
#endif
    i = PyLong_AsLongLong(py);
    if (i == -1 && PyErr_Occurred()) return false;  // Not a C long long.
  }
  if (c) *c = i;
  return true;
}

template <>
inline bool PyObjAs(PyObject* py, unsigned long long* c) {  // NOLINT
  unsigned long long i;                              // NOLINT
#if PY_MAJOR_VERSION < 3
  if (PyInt_Check(py)) {
    i = PyInt_AsUnsignedLongLongMask(py);
  } else  // NOLINT
#endif
  {
    if (!PyLong_Check(py)) return false;  // Not a Python long.
    i = PyLong_AsUnsignedLongLong(py);
    if (i == (unsigned long long)-1 && PyErr_Occurred())  // NOLINT
      return false;
  }
  if (c) *c = i;
  return true;
}

template <>
inline bool PyObjAs(PyObject* py, double* c) {
  double d;
  if (PyFloat_Check(py)) {
    d = PyFloat_AsDouble(py);
#if PY_MAJOR_VERSION < 3
  } else if (PyInt_Check(py)) {
    d = PyInt_AsLong(py);
  } else if (!PyLong_Check(py)) {
    return false;  // float or int/long expected
#endif
  } else {
    d = PyLong_AsDouble(py);
    if (d == -1.0 && PyErr_Occurred()) {
      return false;  // Overflow (or TypeError for PY3)
    }
  }
  if (c) *c = d;
  return true;
}

template <>
inline PyObject* PyObjFrom(const double& c) {
  return PyFloat_FromDouble(c);
}
template <>
inline bool PyObjAs(PyObject* py, float* c) {
  double d;
  if (!PyObjAs(py, &d)) return false;
  if (c) *c = static_cast<float>(d);
  return true;
}

template <>
inline PyObject* PyObjFrom(const float& c) {
  return PyFloat_FromDouble(c);
}
template <>
inline bool PyObjAs(PyObject* py, bool* c) {
  if (!PyBool_Check(py)) return false;  // Not a Python bool.
  if (c) *c = PyObject_Not(py) ? false : true;
  return true;
}

inline int SwigPyIntOrLong_Check(PyObject* o) {
  return (PyLong_Check(o)
#if PY_MAJOR_VERSION <= 2
          || PyInt_Check(o)
#endif
          );  // NOLINT
}

inline PyObject* SwigString_FromString(const std::string& s) {
  return PyString_FromStringAndSize(s.data(), s.size());
}

inline std::string SwigString_AsString(PyObject* o) {
  return std::string(PyString_AsString(o));
}

// STL std::vector<T> for common types

namespace {  // NOLINT

template <typename T>
struct vector_pusher {
  typedef T* ptr;
  static void push(std::vector<T>* o, ptr e) { o->push_back(*e); }
};

template <typename T>
struct vector_pusher<T*> {
  typedef T* ptr;
  static void push(std::vector<T*>* o, ptr e) { o->push_back(e); }
};

};  // namespace

template <class T>
inline bool vector_input_helper(PyObject* seq, std::vector<T>* out,
                                bool (*convert)(PyObject*, T* const)) {
  PyObject* item, *it = PyObject_GetIter(seq);
  if (!it) return false;
  T elem;
  while ((item = PyIter_Next(it))) {
    bool success = convert(item, &elem);
    Py_DECREF(item);
    if (!success) {
      Py_DECREF(it);
      return false;
    }
    if (out) out->push_back(elem);
  }
  Py_DECREF(it);
  return static_cast<bool>(!PyErr_Occurred());
}

template <class T>
inline bool vector_input_wrap_helper(PyObject* seq, std::vector<T>* out,
                                     swig_type_info* swig_Tp_type) {
  PyObject* item, *it = PyObject_GetIter(seq);
  if (!it) {
    PyErr_SetString(PyExc_TypeError, "sequence expected");
    return false;
  }
  typename vector_pusher<T>::ptr elem;
  while ((item = PyIter_Next(it))) {
    if (SWIG_ConvertPtr(item, reinterpret_cast<void**>(&elem),
                        swig_Tp_type, 0) == -1) {
      Py_DECREF(it);
      it = PyObject_Repr(item);
      Py_DECREF(item);
      const char* repr = it ? PyString_AsString(it) : "not";
      PyErr_Format(PyExc_TypeError, "'%s' expected, %s found",
                   SWIG_TypePrettyName(swig_Tp_type), repr);
      Py_XDECREF(it);
      return false;
    }
    Py_DECREF(item);
    if (out) vector_pusher<T>::push(out, elem);
  }
  Py_DECREF(it);
  return true;
}

// Helper function for turning a C++ std::vector<T> (or any other instance that
// supports the std::vector<T>-like iterator interface) into a Python list of
// Ts.
// The converter function converts a C++ object of type const T or const T&
// into the corresponding Python object.
template <class T, class Converter>
inline PyObject* list_output_helper(const T* vec, Converter converter) {
  if (vec == NULL) Py_RETURN_NONE;  // Return a nice out-of-band value.
  PyObject* lst = PyList_New(vec->size());
  if (lst == NULL) return NULL;
  int i = 0;
  for (typename T::const_reference pt : *vec) {
    PyObject* obj = converter(pt);
    if (!obj) {
      Py_DECREF(lst);
      return NULL;
    }
    PyList_SET_ITEM(lst, i++, obj);
  }
  return lst;
}

template <class T>
struct OutConverter {
  PyObject* operator()(const T x) const {
    return SWIG_NewPointerObj((void*)x, type_, new_);  // NOLINT
  }
  swig_type_info* type_;
  int new_;
  OutConverter(swig_type_info* type, int new_object)
      : type_(type), new_(new_object) {}
};

template <class T, class TR>
inline PyObject* vector_output_helper(const std::vector<T>* vec,
                                      PyObject* (*converter)(const TR x)) {
  return list_output_helper(vec, converter);
}

template <class T>
inline PyObject* vector_output_helper(const std::vector<T*>* vec,
                                      const OutConverter<T*>& converter) {
  return list_output_helper(vec, converter);
}

template <class T>
inline PyObject* vector_output_wrap_helper(const std::vector<T*>* vec,
                                           swig_type_info* swig_Tp_type,
                                           bool newobj = false) {
#if 1
  OutConverter<T*> converter(swig_Tp_type, newobj);
  return vector_output_helper(vec, converter);
#else  // Lambda version
  auto converter = [](const T* x) {
    return SWIG_NewPointerObj((void*)x, swig_Tp_type, newobj);  // NOLINT
  } return list_output_helper(vec, converter);
#endif
}

#if PY_MAJOR_VERSION > 2
/* SWIG 2's own C preprocessor macro for this is too strict.
 * It requires a (x) parameter which doesn't work for the case where the
 * function is being passed by & as a converter into a helper such as
 * vector_output_helper above. */
#undef PyInt_FromLong
#define PyInt_FromLong PyLong_FromLong
#endif

#endif  // OR_TOOLS_BASE_PYTHON_SWIG_H_
