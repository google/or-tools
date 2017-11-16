// Copyright 2010-2017 Google
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

// This .i file exposes the sat cp_model API.

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

// std::function utilities.
%include "ortools/util/python/functions.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%pythoncode {
import sys
from ortools.sat import cp_model_pb2

class PySolutionCallback(object):

  def WrapAux(self, proto_str):
    try:
      response = cp_model_pb2.CpSolverResponse()
      if sys.version_info[0] < 3:
        status = response.MergeFromString(proto_str)
      else:
        status = response.MergeFromString(bytes(proto_str))
      self.Wrap(response)
    except:
      print("Unexpected error: %s" % sys.exc_info()[0])

  def Wrap(self, proto):
    pass
}  // %pythoncode

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpModelProto,
                 operations_research::sat::CpModelProto);

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpSolverResponse,
                 operations_research::sat::CpSolverResponse);

PY_PROTO_TYPEMAP(ortools.sat.sat_parameters_pb2,
                 SatParameters,
                 operations_research::sat::SatParameters);


// Wrap std::function<void(const operations_research::sat::CpSolverResponse&)>

%{
void CallSolutionCallback(PyObject* cb,
                          const operations_research::sat::CpSolverResponse& r) {
  std::string encoded_protobuf;
  r.SerializeToString(&encoded_protobuf);
#if defined(PY3)
  PyObject* const python_encoded_protobuf =
      PyBytes_FromStringAndSize(encoded_protobuf.c_str(),
                                encoded_protobuf.size());
#else  // PY3
  PyObject* const python_encoded_protobuf =
      PyString_FromStringAndSize(encoded_protobuf.c_str(),
                                 encoded_protobuf.size());
#endif  // PY3
  PyObject* result =
      PyObject_CallMethod(cb, "WrapAux", "(O)", python_encoded_protobuf);
  Py_XDECREF(python_encoded_protobuf);
  Py_XDECREF(result);
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<void(
    const operations_research::sat::CpSolverResponse& response)> {
  $1 = true;
}

%typemap(in) std::function<void(
    const operations_research::sat::CpSolverResponse& response)> {
  SharedPyPtr input($input);
  $1 = [input](const operations_research::sat::CpSolverResponse& r) {
    return CallSolutionCallback(input.get(), r);
  };
}

%ignoreall

%unignore operations_research;
%unignore operations_research::sat;
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithParameters;
%unignore operations_research::sat::SatHelper::SolveWithParametersAndSolutionObserver;

%include "ortools/sat/swig_helper.h"

%unignoreall
