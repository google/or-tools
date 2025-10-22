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

// To run the fuzzer, use:
//
//   blaze run
//   //ortools/math_opt/core:solver_session_proto_fuzzer_launcher
//
#include <memory>

#include "absl/base/call_once.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solve_session_fuzzer.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/session.pb.h"
#include "ortools/util/solve_interrupter.h"
#include "security/fuzzing/blaze/proto_message_mutator.h"

namespace operations_research::math_opt {
namespace {

// It is OK for SolverTypeProto to contain an invalid value (we use that in unit
// tests).
constexpr SolverTypeProto kFakeSolverType =
    static_cast<SolverTypeProto>(SolverTypeProto_MAX + 1);

// A fake solver that never fails.
class FakeSolver : public SolverInterface {
 public:
  absl::StatusOr<SolveResultProto> Solve(const SolveParametersProto&,
                                         const ModelSolveParametersProto&,
                                         MessageCallback,
                                         const CallbackRegistrationProto&,
                                         Callback,
                                         const SolveInterrupter*) override {
    SolveResultProto result;
    result.mutable_termination()->set_reason(TERMINATION_REASON_INFEASIBLE);
    return result;
  }

  absl::StatusOr<bool> Update(const ModelUpdateProto&) override { return true; }

  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                             MessageCallback message_cb,
                             const SolveInterrupter* interrupter) override {
    ComputeInfeasibleSubsystemResultProto result;
    result.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
    return result;
  }
};

absl::StatusOr<std::unique_ptr<SolverInterface>> FakeSolverFactory(
    const ModelProto& model, const SolverInterface::InitArgs& init_args) {
  return std::make_unique<FakeSolver>();
}

DEFINE_PROTO_FUZZER(const SolveSessionProto& session) {
  static absl::once_flag once;
  absl::call_once(once, []() {
    AllSolversRegistry::Instance()->Register(kFakeSolverType,
                                             FakeSolverFactory);
  });

  RunSolveSessionForFuzzer(kFakeSolverType, session, /*config=*/{});
}

}  // namespace
}  // namespace operations_research::math_opt
