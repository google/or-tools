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

// A pybind11 wrapper for pdlp.

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "Eigen/Core"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/pdlp/primal_dual_hybrid_gradient.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/quadratic_program_io.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/pdlp/termination.h"
#include "pybind11/eigen.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace operations_research::pdlp {

using ::pybind11::arg;

namespace {

class Interrupter {
 public:
  void Interrupt() { interrupted_.store(true, std::memory_order_release); }

  bool IsInterrupted() const {
    return interrupted_.load(std::memory_order_acquire);
  }

  const std::atomic<bool>* flag() const { return &interrupted_; }

 private:
  std::atomic<bool> interrupted_{false};
};

}  // namespace

PYBIND11_MODULE(pdlp, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  // ---------------------------------------------------------------------------
  // quadratic_program.h
  // ---------------------------------------------------------------------------

  // It's ok to read and assign to the fields of QuadraticProgram. Attempts to
  // mutate the fields will likely fail silently because of the copies back and
  // forth from Python and C++.
  pybind11::class_<QuadraticProgram>(m, "QuadraticProgram")
      .def(pybind11::init<>())
      .def("resize_and_initialize", &QuadraticProgram::ResizeAndInitialize)
      .def("apply_objective_scaling_and_offset",
           &QuadraticProgram::ApplyObjectiveScalingAndOffset)
      .def_readwrite("objective_vector", &QuadraticProgram::objective_vector)
      .def_readwrite("constraint_matrix", &QuadraticProgram::constraint_matrix)
      .def_readwrite("constraint_lower_bounds",
                     &QuadraticProgram::constraint_lower_bounds)
      .def_readwrite("constraint_upper_bounds",
                     &QuadraticProgram::constraint_upper_bounds)
      .def_readwrite("variable_lower_bounds",
                     &QuadraticProgram::variable_lower_bounds)
      .def_readwrite("variable_upper_bounds",
                     &QuadraticProgram::variable_upper_bounds)
      .def_readwrite("problem_name", &QuadraticProgram::problem_name)
      .def_readwrite("variable_names", &QuadraticProgram::variable_names)
      .def_readwrite("constraint_names", &QuadraticProgram::constraint_names)
      .def_readwrite("objective_offset", &QuadraticProgram::objective_offset)
      .def_readwrite("objective_scaling_factor",
                     &QuadraticProgram::objective_scaling_factor)
      // It appears that pybind11/eigen.h provides only a C++ -> Python
      // converter for DiagonalMatrix, so we can't expose objective_matrix as a
      // readwrite field. Below are extra methods for setting the objective
      // matrix.
      .def_readonly("objective_matrix", &QuadraticProgram::objective_matrix)
      .def("set_objective_matrix_diagonal",
           [](QuadraticProgram& qp,
              const Eigen::VectorXd& objective_matrix_diagonal) {
             qp.objective_matrix.emplace();
             qp.objective_matrix->diagonal() = objective_matrix_diagonal;
           })
      .def("clear_objective_matrix",
           [](QuadraticProgram& qp) { qp.objective_matrix.reset(); });

  m.def("validate_quadratic_program_dimensions",
        [](const QuadraticProgram& qp) {
          const absl::Status status = ValidateQuadraticProgramDimensions(qp);
          if (status.ok()) {
            return;
          } else {
            throw std::invalid_argument(absl::StrCat(status.message()));
          }
        });

  m.def("is_linear_program", &IsLinearProgram);

  m.def(
      "qp_from_mpmodel_proto",
      [](const MPModelProto& proto, bool relax_integer_variables,
         bool include_names) {
        absl::StatusOr<QuadraticProgram> qp =
            QpFromMpModelProto(proto, relax_integer_variables, include_names);
        if (qp.ok()) {
          return *qp;
        } else {
          throw std::invalid_argument(absl::StrCat(qp.status().message()));
        }
      },
      arg("proto_str"), arg("relax_integer_variables"),
      arg("include_names") = false);

  m.def("qp_to_mpmodel_proto", [](const QuadraticProgram& qp) {
    absl::StatusOr<MPModelProto> proto = QpToMpModelProto(qp);
    if (proto.ok()) {
      return *proto;
    } else {
      throw std::invalid_argument(absl::StrCat(proto.status().message()));
    }
  });

  // ---------------------------------------------------------------------------
  // quadratic_program_io.h
  // ---------------------------------------------------------------------------

  m.def("read_quadratic_program_or_die", &ReadQuadraticProgramOrDie,
        arg("filename"), arg("include_names") = false);

  // ---------------------------------------------------------------------------
  // primal_dual_hybrid_gradient.h
  // ---------------------------------------------------------------------------

  pybind11::class_<PrimalAndDualSolution>(m, "PrimalAndDualSolution")
      .def(pybind11::init<>())
      .def_readwrite("primal_solution", &PrimalAndDualSolution::primal_solution)
      .def_readwrite("dual_solution", &PrimalAndDualSolution::dual_solution);

  pybind11::class_<SolverResult>(m, "SolverResult")
      .def(pybind11::init<>())
      .def_readwrite("primal_solution", &SolverResult::primal_solution)
      .def_readwrite("dual_solution", &SolverResult::dual_solution)
      .def_readwrite("reduced_costs", &SolverResult::reduced_costs)
      .def_readwrite("solve_log", &SolverResult::solve_log);

  pybind11::enum_<IterationType>(m, "IterationType")
      .value("kNormal", IterationType::kNormal)
      .value("kPrimalFeasibility", IterationType::kPrimalFeasibility)
      .value("kDualFeasibility", IterationType::kDualFeasibility)
      .value("kPresolveTermination", IterationType::kPresolveTermination)
      .value("kNormalTermination", IterationType::kNormalTermination)
      .value("kFeasibilityPolishingTermination",
             IterationType::kFeasibilityPolishingTermination);

  pybind11::class_<QuadraticProgramBoundNorms>(m, "QuadraticProgramBoundNorms")
      .def_readonly(
          "l2_norm_primal_linear_objective",
          &QuadraticProgramBoundNorms::l2_norm_primal_linear_objective)
      .def_readonly("l2_norm_constraint_bounds",
                    &QuadraticProgramBoundNorms::l2_norm_constraint_bounds)
      .def_readonly(
          "l_inf_norm_primal_linear_objective",
          &QuadraticProgramBoundNorms::l_inf_norm_primal_linear_objective)
      .def_readonly("l_inf_norm_constraint_bounds",
                    &QuadraticProgramBoundNorms::l_inf_norm_constraint_bounds);

  pybind11::class_<IterationCallbackInfo>(m, "IterationCallbackInfo")
      .def_readonly("iteration_type", &IterationCallbackInfo::iteration_type)
      .def_readonly("termination_criteria",
                    &IterationCallbackInfo::termination_criteria)
      .def_readonly("iteration_stats", &IterationCallbackInfo::iteration_stats)
      .def_readonly("bound_norms", &IterationCallbackInfo::bound_norms);

  pybind11::class_<Interrupter, std::shared_ptr<Interrupter>>(m, "Interrupter")
      .def(pybind11::init<>())
      .def("interrupt", &Interrupter::Interrupt)
      .def_property_readonly("interrupted", &Interrupter::IsInterrupted);

  m.def(
      "primal_dual_hybrid_gradient",
      [](QuadraticProgram qp, PrimalDualHybridGradientParams params,
         std::optional<PrimalAndDualSolution> initial_solution,
         std::shared_ptr<Interrupter> interrupter,
         pybind11::object iteration_stats_callback) {
        std::function<void(const IterationCallbackInfo&)> cpp_callback;
        if (!iteration_stats_callback.is_none()) {
          if (!PyCallable_Check(iteration_stats_callback.ptr())) {
            throw pybind11::type_error(
                "iteration_stats_callback must be callable or None");
          }
          // Borrowed pointer: the argument keeps this object alive for the
          // whole synchronous solve, and no refcount is touched without GIL.
          PyObject* const callback_ptr = iteration_stats_callback.ptr();
          cpp_callback = [callback_ptr](const IterationCallbackInfo& info) {
            pybind11::gil_scoped_acquire acquire;
            pybind11::object callback =
                pybind11::reinterpret_borrow<pybind11::object>(callback_ptr);
            callback(pybind11::cast(info, pybind11::return_value_policy::copy));
          };
        }
        {
          pybind11::gil_scoped_release release;
          return PrimalDualHybridGradient(
              std::move(qp), params, std::move(initial_solution),
              interrupter ? interrupter->flag() : nullptr,
              /*message_callback=*/nullptr, std::move(cpp_callback));
        }
      },
      arg("qp"), arg("params"), arg("initial_solution") = std::nullopt,
      arg("interrupter") = std::shared_ptr<Interrupter>(),
      arg("iteration_stats_callback") = pybind11::none());
}

}  // namespace operations_research::pdlp
