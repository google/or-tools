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

#ifndef OR_TOOLS_MATH_OPT_CPP_REMOTE_STREAMING_MODE_H_
#define OR_TOOLS_MATH_OPT_CPP_REMOTE_STREAMING_MODE_H_

#include <ostream>
#include <string>

#include "absl/strings/string_view.h"

namespace operations_research::math_opt {

// Operation mode of remote streaming solver.
//
// Default mode is to make an RPC call. Other modes enables using local solving
// (either in a subprocess or in the same process).
//
// Most users should use a non-default mode in unit tests or as a debug tool. It
// is recommended to use subprocess solving as a replacement for an RPC call as
// this behaves similarly to RPC, especially regarding cancellation.
enum class RemoteStreamingSolveMode {
  // Default mode which uses a regular streaming RPC call.
  kDefault,

  // Use a local sub-process instead of making a remote call. When this mode is
  // used the stub parameter is ignored.
  //
  // The application must be linked with the target corresponding to the solver
  // type, i.e. something like:
  // //ortools/math_opt/subprocess/solvers:xxx_subprocess_solver
  //
  // The bonus of using this mode is that as with an RPC, the call can be
  // cancelled immediately. And crashes in the solver can't crash the main
  // application. There may be a slight overhead when using this mode as it has
  // to spawn a subprocess for each solve.
  kSubprocess,

  // Make a direct call to the solver in the same process. When this mode is
  // used the stub parameter is ignored.
  //
  // The application must be linked with the target corresponding to the solver
  // type, i.e. something like:
  // //ortools/math_opt/solvers:xxx_solver
  //
  // The call to the solver is done in a background thread making sure it is
  // still compatible with fibers (and their cancellation) in this mode. See
  // ThreadSolve() documentation for details.
  //
  // The cancellation is handled using cooperative interruption, that is as if a
  // SolveInterrupter was used with Solve().
  //
  // The bonus of using this mode is that crashes are easier to debug. The
  // downside is that cancellation is delayed until the solver decides to stop.
  kInProcess,
};

// Parses a flag of type RemoteStreamingSolveMode.
bool AbslParseFlag(absl::string_view text, RemoteStreamingSolveMode* value,
                   std::string* error);

// Unparses a flag of type RemoteStreamingSolveMode.
std::string AbslUnparseFlag(RemoteStreamingSolveMode value);

// Prints the enum using AbslUnparseFlag().
std::ostream& operator<<(std::ostream& out, RemoteStreamingSolveMode mode);

// Prints the enum using AbslUnparseFlag().
template <typename Sink>
void AbslStringify(Sink& sink, const RemoteStreamingSolveMode mode) {
  sink.Append(AbslUnparseFlag(mode));
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_REMOTE_STREAMING_MODE_H_
