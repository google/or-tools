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

#include "ortools/model_builder/wrappers/model_builder_helper.h"

#include <functional>
#include <string>
#include <utility>

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/mps_reader.h"

namespace operations_research {

std::string ModelBuilderHelper::ExportModelProtoToMpsString(
    const MPModelProto& input_model, const MPModelExportOptions& options) {
  return operations_research::ExportModelAsMpsFormat(input_model, options)
      .value_or("");
}

std::string ModelBuilderHelper::ExportModelProtoToLpString(
    const MPModelProto& input_model, const MPModelExportOptions& options) {
  return operations_research::ExportModelAsLpFormat(input_model, options)
      .value_or("");
}

// See comment in the header file why we need to wrap absl::Status code with
// code having simpler APIs.
MPModelProto ModelBuilderHelper::ImportFromMpsString(
    const std::string& mps_string) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsDataToMPModelProto(mps_string);
  if (model_or.ok()) return model_or.value();
  MPModelProto model;
  model.set_name("Invalid model");
  return model;
}

MPModelProto ModelBuilderHelper::ImportFromMpsFile(
    const std::string& mps_file) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsDataToMPModelProto(mps_file);
  if (model_or.ok()) return model_or.value();
  MPModelProto model;
  model.set_name("Invalid model");
  return model;
}

MPModelProto ModelBuilderHelper::ImportFromLpString(
    const std::string& lp_string) {
  absl::StatusOr<MPModelProto> model_proto = ModelProtoFromLpFormat(lp_string);
  if (model_proto.ok()) return model_proto.value();
  MPModelProto model;
  model.set_name("Invalid model");
  return model;
}

MPModelProto ModelBuilderHelper::ImportFromLpFile(const std::string& lp_file) {
  absl::StatusOr<MPModelProto> model_proto = ModelProtoFromLpFormat(lp_file);
  if (model_proto.ok()) return model_proto.value();
  MPModelProto model;
  model.set_name("Invalid model");
  return model;
}

MPSolutionResponse ModelSolverHelper::Solve(const MPModelRequest& request) {
  MPSolutionResponse response;
  // TODO(user): Make compatible with solvers that don't support interrupt.
  // TODO(user): Register the log callback.
  MPSolver::SolveWithProto(request, &response, &interrupt_solve_);
  return response;
}

void ModelSolverHelper::SetLogCallback(
    std::function<void(const std::string&)> log_callback) {
  log_callback_ = std::move(log_callback);
}

void ModelSolverHelper::SetLogCallbackFromDirectorClass(
    LogCallback* log_callback) {
  log_callback_ = [log_callback](const std::string& message) {
    log_callback->NewMessage(message);
  };
}

bool ModelSolverHelper::InterruptSolve() {
  interrupt_solve_ = true;
  return true;
}

}  // namespace operations_research
