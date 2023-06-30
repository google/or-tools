// Copyright 2010-2022 Google LLC
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

// This file wraps the swig_helper.h classes in python using pybind11.
// Because pybind11_protobuf does not support building with CMake for OR-Tools,
// the API has been transformed to use serialized protos from Python to C++ and
// from C++ to python:
//   from Python to C++: use proto.SerializeToString(). This creates a python
//     string that is passed to C++ and parsed back to proto.
//   from C++ to Python, we cast the result of proto.SerializeAsString() to
//     pybind11::bytes. This is passed back to python, which will reconstruct
//     the proto using PythonProto.FromString(byte[]).

#include "ortools/sat/swig_helper.h"

#include <string>

#include "absl/strings/string_view.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::Domain;
using ::operations_research::sat::CpModelProto;
using ::operations_research::sat::CpSatHelper;
using ::operations_research::sat::CpSolverResponse;
using ::operations_research::sat::IntegerVariableProto;
using ::operations_research::sat::SatParameters;
using ::operations_research::sat::SolutionCallback;
using ::operations_research::sat::SolveWrapper;
using ::pybind11::arg;

class PySolutionCallback : public SolutionCallback {
 public:
  using SolutionCallback::SolutionCallback; /* Inherit constructors */

  void OnSolutionCallback() const override {
    ::pybind11::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        void,               /* Return type */
        SolutionCallback,   /* Parent class */
        OnSolutionCallback, /* Name of function */
        /* This function has no arguments. The trailing comma
           in the previous line is needed for some compilers */
    );
  }
};

PYBIND11_MODULE(swig_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  pybind11::module::import("ortools.util.python.sorted_interval_list");

  pybind11::class_<SolutionCallback, PySolutionCallback>(m, "SolutionCallback")
      .def(pybind11::init<>())
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
      .def("Response", &SolutionCallback::Response)
      .def("SolutionBooleanValue", &SolutionCallback::SolutionBooleanValue,
           arg("index"))
      .def("SolutionIntegerValue", &SolutionCallback::SolutionIntegerValue,
           arg("index"))
      .def("StopSearch", &SolutionCallback::StopSearch)
      .def("UserTime", &SolutionCallback::UserTime)
      .def("WallTime", &SolutionCallback::WallTime);

  pybind11::class_<SolveWrapper>(m, "SolveWrapper")
      .def(pybind11::init<>())
      .def("AddLogCallback", &SolveWrapper::AddLogCallback, arg("log_callback"))
      .def("AddSolutionCallback", &SolveWrapper::AddSolutionCallback,
           arg("callback"))
      .def("ClearSolutionCallback", &SolveWrapper::ClearSolutionCallback)
      .def("SetParameters", &SolveWrapper::SetParameters, arg("parameters"))
      .def("Solve",
           [](SolveWrapper* solve_wrapper,
              const CpModelProto& model_proto) -> CpSolverResponse {
             ::pybind11::gil_scoped_release release;
             return solve_wrapper->Solve(model_proto);
           })
      .def("StopSearch", &SolveWrapper::StopSearch);

  pybind11::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("ModelStats", &CpSatHelper::ModelStats, arg("model_proto"))
      .def_static("SolverResponseStats", &CpSatHelper::SolverResponseStats,
                  arg("response"))
      .def_static("ValidateModel", &CpSatHelper::ValidateModel,
                  arg("model_proto"))
      .def_static("VariableDomain", &CpSatHelper::VariableDomain,
                  arg("variable_proto"))
      .def_static("WriteModelToFile", &CpSatHelper::WriteModelToFile,
                  arg("model_proto"), arg("filename"));
}
