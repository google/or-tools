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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVER_RESOURCES_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVER_RESOURCES_H_

#include <optional>
#include <ostream>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/rpc.pb.h"

namespace operations_research::math_opt {

// The hints on the resources a remote solve is expected to use. These
// parameters are hints and may be ignored by the remote server (in particular
// in case of solve in a local subprocess, for example).
//
// When using:
// - RemoteSolve(),
// - RemoteComputeInfeasibleSubsystem(),
// - XxxRemoteStreamingSolve(),
// - XxxRemoteStreamingComputeInfeasibleSubsystem(),
// these hints are recommended but optional. When they are not provided,
// resource usage will be estimated based on other parameters.
//
// When using NewXxxRemoteStreamingIncrementalSolver() these hints are used to
// dimension the resources available during the execution of every action; thus
// it is recommended to set them.
//
struct SolverResources {
  // The number of solver threads that are expected to actually execute in
  // parallel. Must be finite and >0.0.
  //
  // For example a value of 3.0 means that if the solver has 5 threads that can
  // execute we expect at least 3 of these threads to be scheduled in parallel
  // for any given time slice of the operating system scheduler.
  //
  // A fractional value indicates that we don't expect the operating system to
  // constantly schedule the solver's work. For example with 0.5 we would expect
  // the solver's threads to be scheduled half the time.
  //
  // This parameter is usually used in conjunction with
  // SolveParameters.threads. For some solvers like Gurobi it makes sense to use
  // SolverResources.cpu = SolveParameters.threads. For other solvers like
  // CP-SAT, it may makes sense to use a value lower than the number of threads
  // as not all threads may be ready to be scheduled at the same time. It is
  // better to consult each solver documentation to set this parameter.
  //
  // Note that if the SolveParameters.threads is not set then this parameter
  // should also be left unset.
  std::optional<double> cpu;

  // The limit of RAM for the solve in bytes. Must be finite and >=1.0 (even
  // though it should in practice be much larger).
  std::optional<double> ram;

  SolverResourcesProto Proto() const;
  static absl::StatusOr<SolverResources> FromProto(
      const SolverResourcesProto& proto);
};

std::ostream& operator<<(std::ostream& out, const SolverResources& resources);

bool AbslParseFlag(absl::string_view text, SolverResources* solver_resources,
                   std::string* error);

std::string AbslUnparseFlag(const SolverResources& solver_resources);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVER_RESOURCES_H_
