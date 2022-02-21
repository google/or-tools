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

#include "ortools/sat/swig_helper.h"

#include <string>

#include "pybind11/include/pybind11/functional.h"
#include "pybind11/include/pybind11/pybind11.h"
#include "pybind11/include/pybind11/stl.h"
#include "pybind11_protobuf/wrapped_proto_caster.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"

using ::operations_research::sat::CpSatHelper;
using ::operations_research::sat::CpSolverResponse;
using ::operations_research::sat::SolutionCallback;
using ::operations_research::sat::SolveWrapper;
using ::pybind11::arg;
using ::pybind11_protobuf::WithWrappedProtos;

class PySolutionCallback : public SolutionCallback {
 public:
  using SolutionCallback::SolutionCallback; /* Inherit constructors */

  void OnSolutionCallback() const override {
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
  pybind11_protobuf::ImportWrappedProtoCasters();
  pybind11::module::import(

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
      .def("Response", WithWrappedProtos(&SolutionCallback::Response))
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
      .def("SetParameters", WithWrappedProtos(&SolveWrapper::SetParameters),
           arg("parameters"))
      .def("Solve", WithWrappedProtos(&SolveWrapper::Solve), arg("model_proto"))
      .def("StopSearch", &SolveWrapper::StopSearch);

  pybind11::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("ModelStats", WithWrappedProtos(&CpSatHelper::ModelStats))
      .def_static("SolverResponseStats",
                  WithWrappedProtos(&CpSatHelper::SolverResponseStats))
      .def_static("ValidateModel",
                  WithWrappedProtos(&CpSatHelper::ValidateModel),
                  arg("model_proto"))
      .def_static("VariableDomain",
                  WithWrappedProtos(&CpSatHelper::VariableDomain),
                  arg("variable_proto"))
      .def_static("WriteModelToFile",
                  WithWrappedProtos(&CpSatHelper::WriteModelToFile),
                  arg("model_proto"), arg("filename"));
}
