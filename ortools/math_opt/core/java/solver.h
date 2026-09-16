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

#ifndef ORTOOLS_MATH_OPT_CORE_JAVA_SOLVER_H_
#define ORTOOLS_MATH_OPT_CORE_JAVA_SOLVER_H_

#include <string>

#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/java/java_swig_solve_interrupter.h"

namespace operations_research::math_opt {

class CppMessageCallback {
 public:
  virtual ~CppMessageCallback() = default;
  virtual void onMessage(const std::string& message) = 0;
};

class CppSolveCallback {
 public:
  virtual ~CppSolveCallback() = default;
  virtual operations_research::math_opt::CallbackResultProto onEvent(
      const operations_research::math_opt::CallbackDataProto& cbData) = 0;
};

absl::StatusOr<operations_research::math_opt::SolveResultProto> cppSolve(
    const operations_research::math_opt::ModelProto& model,
    operations_research::math_opt::SolverTypeProto solverType,
    const operations_research::math_opt::SolveParametersProto& solveParameters,
    const operations_research::math_opt::ModelSolveParametersProto&
        modelSolveParameters,
    operations_research::math_opt::CppMessageCallback* messageCallback,
    const operations_research::math_opt::CallbackRegistrationProto&
        cbRegistration,
    operations_research::math_opt::CppSolveCallback* callback,
    operations_research::JavaSwigSolveInterrupter* interrupter);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_JAVA_SOLVER_H_
