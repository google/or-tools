// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

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

void SetSerializedParameters(const std::string& serialized_parameters,
                             SolveWrapper* solve_wrapper) {
  SatParameters parameters;
  if (parameters.ParseFromString(serialized_parameters)) {
    solve_wrapper->SetParameters(parameters);
  }
}

pybind11::bytes SerializedSolve(const std::string& serialized_model,
                                SolveWrapper* solve_wrapper) {
  ::pybind11::gil_scoped_release release;
  CpModelProto model_proto;
  model_proto.ParseFromString(serialized_model);
  return solve_wrapper->Solve(model_proto).SerializeAsString();
}

std::string SerializedModelStats(const std::string& serialized_model) {
  CpModelProto model_proto;
  model_proto.ParseFromString(serialized_model);
  return CpSatHelper::ModelStats(model_proto);
}

std::string SerializedSolverResponseStats(
    const std::string& serialized_response) {
  CpSolverResponse response_proto;
  response_proto.ParseFromString(serialized_response);
  return CpSatHelper::SolverResponseStats(response_proto);
}

std::string SerializedValidateModel(const std::string& serialized_model) {
  CpModelProto model_proto;
  model_proto.ParseFromString(serialized_model);
  return CpSatHelper::ValidateModel(model_proto);
}

Domain SerializedVariableDomain(const std::string& serialized_variable) {
  IntegerVariableProto variable_proto;
  variable_proto.ParseFromString(serialized_variable);
  return CpSatHelper::VariableDomain(variable_proto);
}

bool SerializedWriteModelToFile(const std::string& serialized_model,
                                const std::string& filename) {
  CpModelProto model_proto;
  model_proto.ParseFromString(serialized_model);
  return CpSatHelper::WriteModelToFile(model_proto, filename);
}

pybind11::bytes SerializedResponse(const SolutionCallback& solution_callback) {
  return solution_callback.Response().SerializeAsString();
}

PYBIND11_MODULE(swig_helper, m) {
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
      .def_static("SerializedResponse", &SerializedResponse,
                  arg("solution_callback"))
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
      .def_static("SetSerializedParameters", &SetSerializedParameters,
                  arg("serialized_parameters"), arg("solve_wrapper"))
      .def_static("SerializedSolve", &SerializedSolve, arg("model_proto"),
                  arg("solve_wrapper"))
      .def("StopSearch", &SolveWrapper::StopSearch);

  pybind11::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("SerializedModelStats", &SerializedModelStats,
                  arg("serialized_model"))
      .def_static("SerializedSolverResponseStats",
                  &SerializedSolverResponseStats, arg("serialized_response"))
      .def_static("SerializedValidateModel", &SerializedValidateModel,
                  arg("serialized_model"))
      .def_static("SerializedVariableDomain", &SerializedVariableDomain,
                  arg("serialized_variable"))
      .def_static("SerializedWriteModelToFile", &SerializedWriteModelToFile,
                  arg("serialized_model"), arg("filename"));
}
