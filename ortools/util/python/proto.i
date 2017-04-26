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

// TODO(user): make this SWIG file comply with the SWIG style guide.
%include "ortools/base/base.i"

%import "ortools/base/integral_types.h"

namespace operations_research {
%define PY_PROTO_TYPEMAP(PythonModule, PythonType, CppType)
%typecheck(SWIG_TYPECHECK_POINTER) const CppType&, CppType* {
  bool ok = false;
  PyObject* const module = PyImport_ImportModule("PythonModule");
  if (module != nullptr) {
    PyObject* const dict = PyModule_GetDict(module);
    if (dict != nullptr) {
      PyObject* const clss = PyDict_GetItemString(dict, "PythonType");
      if (clss != nullptr) {
        if (PyObject_IsInstance($input, clss)) {
          ok = true;
        }
      }
    }
    Py_DECREF(module);
  }
  $1 = ok ? 1 : 0;
}

%typemap(in) const CppType&, CppType* const, CppType* {
  $1 = new CppType;
  PyObject* const pyresult = PyObject_CallMethod(
      $input, const_cast<char*>("SerializeToString"), nullptr);
  if (pyresult != nullptr) {
    char* buffer = nullptr;
    Py_ssize_t length = 0;
    int result = PyString_AsStringAndSize(pyresult, &buffer, &length);
    if (buffer != nullptr) {
      $1->ParseFromArray(buffer, length);
    }
    Py_DECREF(pyresult);
  }
}

%typemap(freearg) const CppType&, CppType* const, CppType* {
  delete $1;
}

%typemap(argout) CppType* const, CppType* {
  std::string encoded_protobuf;
  $1->SerializeToString(&encoded_protobuf);
#if defined(PY3)
  PyObject* const python_encoded_protobuf =
      PyBytes_FromStringAndSize(encoded_protobuf.c_str(),
                                encoded_protobuf.size());
#else  // PY3
  PyObject* const python_encoded_protobuf =
      PyString_FromStringAndSize(encoded_protobuf.c_str(),
                                 encoded_protobuf.size());
#endif  // PY3
  if (python_encoded_protobuf != nullptr) {
    PyObject* const result = PyObject_CallMethod(
        $input, const_cast<char*>("ParseFromString"),
        const_cast<char*>("(O)"), python_encoded_protobuf);
    Py_DECREF(python_encoded_protobuf);
    if (result != nullptr) { Py_DECREF(result); }
  }
}

%typemap(out) CppType {
  PyObject* const module = PyImport_ImportModule("PythonModule");
  if (module != nullptr) {
    PyObject* const dict = PyModule_GetDict(module);
    if (dict != nullptr) {
      PyObject* const clss = PyDict_GetItemString(dict, "PythonType");
      if (clss != nullptr) {
        std::string encoded_protobuf;
        $1.SerializeToString(&encoded_protobuf);
#if defined(PY3)
        PyObject* const python_encoded_protobuf = PyBytes_FromStringAndSize(
            encoded_protobuf.c_str(), encoded_protobuf.size());
#else  // PY3
        PyObject* const python_encoded_protobuf = PyString_FromStringAndSize(
            encoded_protobuf.c_str(), encoded_protobuf.size());
#endif  // PY3
        PyObject* const result = PyObject_CallMethod(
              clss, const_cast<char*>("FromString"),
              const_cast<char*>("(O)"),
              python_encoded_protobuf);
        Py_DECREF(python_encoded_protobuf);
        $result = result;
      }
    }
    Py_DECREF(module);
  }
}
%typemap(newfree) CppType {
  delete $1;
}
%enddef  // PY_PROTO_TYPEMAP
}  // namespace operations_research
