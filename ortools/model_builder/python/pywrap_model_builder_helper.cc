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

// A pybind11 wrapper for model_builder_helper.
// Use the pure python wrapper model_builder_helper.py instead of this
// interface.

#include <stdexcept>
#include <string>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/strings/str_cat.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/model_builder/wrappers/model_builder_helper.h"
#include "pybind11/eigen.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"

using ::Eigen::SparseMatrix;
using ::Eigen::VectorXd;
using ::operations_research::ModelBuilderHelper;
using ::operations_research::ModelSolverHelper;
using ::operations_research::MPConstraintProto;
using ::operations_research::MPModelExportOptions;
using ::operations_research::MPModelProto;
using ::operations_research::MPVariableProto;
using ::pybind11::arg;

// TODO(user): The interface uses serialized protos because of issues building
// pybind11_protobuf. See
// https://github.com/protocolbuffers/protobuf/issues/9464. After
// pybind11_protobuf is working, this workaround can be removed.

pybind11::bytes BuildModel(
    const Eigen::Ref<const VectorXd>& variable_lower_bounds,
    const Eigen::Ref<const VectorXd>& variable_upper_bounds,
    const Eigen::Ref<const VectorXd>& objective_coefficients,
    const Eigen::Ref<const VectorXd>& constraint_lower_bounds,
    const Eigen::Ref<const VectorXd>& constraint_upper_bounds,
    const SparseMatrix<double, Eigen::RowMajor>& constraint_matrix) {
  const int num_variables = variable_lower_bounds.size();
  const int num_constraints = constraint_lower_bounds.size();

  if (variable_upper_bounds.size() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid size ", variable_upper_bounds.size(),
                     " for variable_upper_bounds. Expected: ", num_variables));
  }
  if (objective_coefficients.size() != num_variables) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", objective_coefficients.size(),
        " for linear_objective_coefficients. Expected: ", num_variables));
  }
  if (constraint_upper_bounds.size() != num_constraints) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", constraint_upper_bounds.size(),
        " for constraint_upper_bounds. Expected: ", num_constraints));
  }
  if (constraint_matrix.cols() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of columns ", constraint_matrix.cols(),
                     " in constraint_matrix. Expected: ", num_variables));
  }
  if (constraint_matrix.rows() != num_constraints) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of rows ", constraint_matrix.rows(),
                     " in constraint_matrix. Expected: ", num_constraints));
  }

  MPModelProto model;
  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* variable = model.add_variable();
    variable->set_lower_bound(variable_lower_bounds[i]);
    variable->set_upper_bound(variable_upper_bounds[i]);
    variable->set_objective_coefficient(objective_coefficients[i]);
  }

  for (int row = 0; row < num_constraints; ++row) {
    MPConstraintProto* constraint = model.add_constraint();
    constraint->set_lower_bound(constraint_lower_bounds[row]);
    constraint->set_upper_bound(constraint_upper_bounds[row]);
    for (SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(
             constraint_matrix, row);
         it; ++it) {
      constraint->add_coefficient(it.value());
      constraint->add_var_index(it.col());
    }
  }

  return model.SerializeAsString();
}

PYBIND11_MODULE(pywrap_model_builder_helper, m) {
  m.def("BuildModel", BuildModel, arg("variable_lower_bounds"),
        arg("variable_upper_bounds"), arg("objective_coefficients"),
        arg("constraint_lower_bounds"), arg("constraint_upper_bounds"),
        arg("constraint_matrix"));

  pybind11::class_<MPModelExportOptions>(m, "MPModelExportOptions")
      .def(pybind11::init<>())
      .def_readwrite("obfuscate", &MPModelExportOptions::obfuscate)
      .def_readwrite("log_invalid_names",
                     &MPModelExportOptions::log_invalid_names)
      .def_readwrite("show_unused_variables",
                     &MPModelExportOptions::show_unused_variables)
      .def_readwrite("max_line_length", &MPModelExportOptions::max_line_length);

  m.def(
      "ExportModelProtoToMpsString",
      [](const std::string& input_model, const MPModelExportOptions& options) {
        MPModelProto model;
        if (!model.ParseFromString(input_model)) {
          throw std::invalid_argument(
              "Unable to parse input_model as MPModelProto.");
        }
        return ModelBuilderHelper::ExportModelProtoToMpsString(model, options);
      },
      arg("input_model"), arg("options") = MPModelExportOptions());
  m.def(
      "ExportModelProtoToLpString",
      [](const std::string& input_model, const MPModelExportOptions& options) {
        MPModelProto model;
        if (!model.ParseFromString(input_model)) {
          throw std::invalid_argument(
              "Unable to parse input_model as MPModelProto.");
        }
        return ModelBuilderHelper::ExportModelProtoToLpString(model, options);
      },
      arg("input_model"), arg("options") = MPModelExportOptions());

  m.def("ImportFromMpsString", [](const std::string& str) {
    MPModelProto model = ModelBuilderHelper::ImportFromMpsString(str);
    return pybind11::bytes(model.SerializeAsString());
  });
  m.def("ImportFromMpsFile", [](const std::string& str) {
    MPModelProto model = ModelBuilderHelper::ImportFromMpsFile(str);
    return pybind11::bytes(model.SerializeAsString());
  });
  m.def("ImportFromLpString", [](const std::string& str) {
    MPModelProto model = ModelBuilderHelper::ImportFromLpString(str);
    return pybind11::bytes(model.SerializeAsString());
  });
  m.def("ImportFromLpFile", [](const std::string& str) {
    MPModelProto model = ModelBuilderHelper::ImportFromLpFile(str);
    return pybind11::bytes(model.SerializeAsString());
  });

  pybind11::class_<ModelSolverHelper>(m, "ModelSolverHelper")
      .def(pybind11::init<>())
      .def(
          "Solve",
          [](ModelSolverHelper* solver_helper, const std::string& request_str) {
            operations_research::MPModelRequest request;
            if (!request.ParseFromString(request_str)) {
              throw std::invalid_argument(
                  "Unable to parse request as MPModelRequest.");
            }
            return pybind11::bytes(
                solver_helper->Solve(request).SerializeAsString());
          },
          // The GIL is released during the solve to allow Python threads to do
          // other things in parallel, e.g., log and interrupt.
          pybind11::call_guard<pybind11::gil_scoped_release>())
      .def("InterruptSolve", &ModelSolverHelper::InterruptSolve,
           "Returns true if the interrupt signal was correctly sent, that is, "
           "if the underlying solver supports it.")
      .def("SetLogCallback", &ModelSolverHelper::SetLogCallback);
}
