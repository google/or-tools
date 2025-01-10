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

// A pybind11 wrapper for the init library.

#include "ortools/init/init.h"

#include <string>

#include "ortools/init/python/init_doc.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"

using ::operations_research::CppBridge;
using ::operations_research::CppFlags;
using ::operations_research::OrToolsVersion;

namespace py = pybind11;
using ::py::arg;

PYBIND11_MODULE(init, m) {
  py::class_<CppFlags>(m, "CppFlags", DOC(operations_research, CppFlags))
      .def(py::init<>())
      .def_readwrite("stderrthreshold", &CppFlags::stderrthreshold,
                     DOC(operations_research, CppFlags, stderrthreshold))
      .def_readwrite("log_prefix", &CppFlags::log_prefix,
                     DOC(operations_research, CppFlags, log_prefix))
      .def_readwrite("cp_model_dump_prefix", &CppFlags::cp_model_dump_prefix,
                     DOC(operations_research, CppFlags, cp_model_dump_prefix))
      .def_readwrite("cp_model_dump_models", &CppFlags::cp_model_dump_models,
                     DOC(operations_research, CppFlags, cp_model_dump_models))
      .def_readwrite(
          "cp_model_dump_submodels", &CppFlags::cp_model_dump_submodels,
          DOC(operations_research, CppFlags, cp_model_dump_submodels))
      .def_readwrite(
          "cp_model_dump_response", &CppFlags::cp_model_dump_response,
          DOC(operations_research, CppFlags, cp_model_dump_response));

  pybind11::class_<CppBridge>(m, "CppBridge",
                              DOC(operations_research, CppBridge))
      .def_static("init_logging", &CppBridge::InitLogging,
                  DOC(operations_research, CppBridge, InitLogging),
                  arg("usage"))
      .def_static("shutdown_logging", &CppBridge::ShutdownLogging,
                  DOC(operations_research, CppBridge, ShutdownLogging))
      .def_static("set_flags", &CppBridge::SetFlags,
                  DOC(operations_research, CppBridge, SetFlags), arg("flags"))
      .def_static("load_gurobi_shared_library",
                  &CppBridge::LoadGurobiSharedLibrary,
                  DOC(operations_research, CppBridge, LoadGurobiSharedLibrary),
                  arg("full_library_path"))
      .def_static("delete_byte_array", &CppBridge::DeleteByteArray,
                  DOC(operations_research, CppBridge, DeleteByteArray),
                  arg("buffer"));

  pybind11::class_<OrToolsVersion>(m, "OrToolsVersion",
                                   DOC(operations_research, OrToolsVersion))
      .def_static("major_number", &OrToolsVersion::MajorNumber,
                  DOC(operations_research, OrToolsVersion, MajorNumber))
      .def_static("minor_number", &OrToolsVersion::MinorNumber,
                  DOC(operations_research, OrToolsVersion, MinorNumber))
      .def_static("patch_number", &OrToolsVersion::PatchNumber,
                  DOC(operations_research, OrToolsVersion, PatchNumber))
      .def_static("version_string", &OrToolsVersion::VersionString,
                  DOC(operations_research, OrToolsVersion, VersionString));
}
