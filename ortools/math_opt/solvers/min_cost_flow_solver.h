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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_MIN_COST_FLOW_SOLVER_H_
#define ORTOOLS_MATH_OPT_SOLVERS_MIN_COST_FLOW_SOLVER_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {

class MinCostFlowSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const InitArgs& init_args);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      const SolveInterrupter* absl_nullable interrupter) override;

  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;

  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(
      const SolveParametersProto& parameters, MessageCallback message_cb,
      const SolveInterrupter* absl_nullable interrupter) override;

 private:
  DEFINE_STRONG_INT_TYPE(ArcIndex, int32_t);
  using VariableId = int64_t;
  MinCostFlowSolver(std::unique_ptr<SimpleMinCostFlow> mcf,
                    util_intops::StrongVector<ArcIndex, VariableId> arc_to_var,
                    bool is_maximize, double objective_offset);

  absl::StatusOr<SolveResultProto> MakeSolveResult(
      SimpleMinCostFlow::Status status) const;

  const std::unique_ptr<SimpleMinCostFlow> mcf_;
  const util_intops::StrongVector<ArcIndex, VariableId> arc_to_var_;
  const bool is_maximize_;
  const double objective_offset_;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVERS_MIN_COST_FLOW_SOLVER_H_
