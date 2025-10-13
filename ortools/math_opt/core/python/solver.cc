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

#include "ortools/math_opt/core/solver.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_debug.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/python/py_solve_interrupter.h"
#include "ortools/util/solve_interrupter.h"
#include "pybind11/attr.h"
#include "pybind11/cast.h"
#include "pybind11/gil.h"
#include "pybind11_abseil/import_status_module.h"
#include "pybind11_abseil/status_casters.h"  // IWYU pragma: keep
#include "pybind11_protobuf/native_proto_caster.h"

namespace operations_research::math_opt {
namespace {

// Returns the pointer on the underlying C++ SolveInterrupter.
//
// The returned pointer is valid as long as the (optional) input shared_ptr is
// non null.
const SolveInterrupter* absl_nullable CppSolveInterrupter(
    const absl_nullable std::shared_ptr<PySolveInterrupter> interrupter) {
  if (interrupter == nullptr) {
    return nullptr;
  }
  return interrupter->interrupter();
}

}  // namespace

namespace py = ::pybind11;

using PybindSolverCallback =
    std::function<CallbackResultProto(CallbackDataProto)>;

using PybindSolverMessageCallback =
    std::function<void(std::vector<std::string>)>;

// Wrapper for Solver::NonIncrementalSolve with flat arguments.
absl::StatusOr<SolveResultProto> PybindSolve(
    const ModelProto& model, const SolverTypeProto solver_type,
    SolverInitializerProto solver_initializer, SolveParametersProto parameters,
    ModelSolveParametersProto model_parameters,
    PybindSolverMessageCallback message_callback,
    CallbackRegistrationProto callback_registration,
    PybindSolverCallback user_cb,
    absl_nullable std::shared_ptr<PySolveInterrupter> interrupter) {
  return Solver::NonIncrementalSolve(
      model, solver_type, {.streamable = std::move(solver_initializer)},
      {.parameters = std::move(parameters),
       .model_parameters = std::move(model_parameters),
       .message_callback = std::move(message_callback),
       .callback_registration = std::move(callback_registration),
       .user_cb = std::move(user_cb),
       .interrupter = CppSolveInterrupter(std::move(interrupter))});
}

// Wrapper for Solver::NonIncrementalComputeInfeasibleSubsystem
absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
PybindComputeInfeasibleSubsystem(
    const ModelProto& model, const SolverTypeProto solver_type,
    SolverInitializerProto solver_initializer, SolveParametersProto parameters,
    PybindSolverMessageCallback message_callback,
    absl_nullable std::shared_ptr<PySolveInterrupter> interrupter) {
  return Solver::NonIncrementalComputeInfeasibleSubsystem(
      model, solver_type, {.streamable = std::move(solver_initializer)},
      {.parameters = std::move(parameters),
       .message_callback = std::move(message_callback),
       .interrupter = CppSolveInterrupter(std::move(interrupter))});
}

// Wrapper for the Solver class with flat arguments.
class PybindSolver {
 public:
  static absl::StatusOr<std::unique_ptr<PybindSolver>> New(
      const SolverTypeProto solver_type, const ModelProto& model,
      SolverInitializerProto solver_initializer) {
    ASSIGN_OR_RETURN(
        std::unique_ptr<Solver> solver,
        Solver::New(solver_type, model,
                    {.streamable = std::move(solver_initializer)}));
    return absl::WrapUnique<PybindSolver>(new PybindSolver(std::move(solver)));
  }

  static int64_t DebugNumSolver() { return internal::debug_num_solver.load(); }

  PybindSolver(const PybindSolver&) = delete;
  PybindSolver& operator=(const PybindSolver&) = delete;

  absl::StatusOr<SolveResultProto> Solve(
      SolveParametersProto parameters,
      ModelSolveParametersProto model_parameters,
      PybindSolverMessageCallback message_callback,
      CallbackRegistrationProto callback_registration,
      PybindSolverCallback user_cb,
      absl_nullable std::shared_ptr<PySolveInterrupter> interrupter) {
    return solver_->Solve(
        {.parameters = std::move(parameters),
         .model_parameters = std::move(model_parameters),
         .message_callback = std::move(message_callback),
         .callback_registration = std::move(callback_registration),
         .user_cb = std::move(user_cb),
         .interrupter = CppSolveInterrupter(std::move(interrupter))});
  }

  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) {
    return solver_->Update(model_update);
  }

 private:
  explicit PybindSolver(std::unique_ptr<Solver> solver)
      : solver_(std::move(solver)) {}

  const std::unique_ptr<Solver> solver_;
};

PYBIND11_MODULE(solver, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  pybind11::google::ImportStatusModule();

  // Make sure that the pybind solve interrupter module is loaded correctly
  // whenever this module is loaded. Without it, pybind11 doesn't know about the
  // type, and unless it is accidentally imported through some other way, it
  // wouldn't be able to bind `None` to a nullptr.
  pybind11::module::import("ortools.util.python.pybind_solve_interrupter");

  // The Global Interpreter Lock (GIL) is released with gil_scoped_release
  // during the solve to allow Python threads to run callbacks in parallel.
  m.def("solve", &PybindSolve, py::arg("model"), py::arg("solver_type"),
        py::arg("solver_initializer"), py::arg("parameters"),
        py::arg("model_parameters"), py::arg("message_callback"),
        py::arg("callback_registration"), py::arg("user_cb"),
        py::arg("interrupter"), py::call_guard<py::gil_scoped_release>());
  m.def("compute_infeasible_subsystem", &PybindComputeInfeasibleSubsystem,
        py::arg("model"), py::arg("solver_type"), py::arg("solver_initializer"),
        py::arg("parameters"), py::arg("message_callback"),
        py::arg("interrupter"), py::call_guard<py::gil_scoped_release>());
  m.def("new", &PybindSolver::New, py::arg("solver_type"), py::arg("model"),
        py::arg("solver_initializer"),
        py::call_guard<py::gil_scoped_release>());
  m.def("debug_num_solver", &PybindSolver::DebugNumSolver);

  py::class_<PybindSolver>(m, "Solver")
      .def("solve", &PybindSolver::Solve, py::arg("parameters"),
           py::arg("model_parameters"), py::arg("message_callback"),
           py::arg("callback_registration"), py::arg("user_cb"),
           py::arg("interrupter"), py::call_guard<py::gil_scoped_release>())
      .def("update", &PybindSolver::Update, py::arg("model_update"),
           py::call_guard<py::gil_scoped_release>());
}

}  // namespace operations_research::math_opt
