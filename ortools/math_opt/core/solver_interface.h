// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_H_
#define OR_TOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {
namespace internal {

// The message of the InvalidArgumentError returned by solvers that are passed a
// non null message callback when they don't support it.
inline constexpr absl::string_view kMessageCallbackNotSupported =
    "This solver does not support message callbacks.";

}  // namespace internal

// Interface implemented by actual solvers.
//
// This interface is not meant to be used directly. The actual API is the one of
// the Solver class. The Solver class validates the models before calling this
// interface. It makes sure no concurrent calls happen on Solve(), CanUpdate()
// and Update(). It makes sure no other function is called after Solve(),
// Update() or a callback have failed.
//
// Implementations of this interface should not have public constructors but
// instead have a static `New` function with the signature of Factory function
// as defined below. They should register this factory using the macro
// MATH_OPT_REGISTER_SOLVER().
class SolverInterface {
 public:
  // Initialization arguments.
  struct InitArgs {
    // All parameters that can be stored in a proto and exchange with other
    // processes.
    SolverInitializerProto streamable;

    // All parameters that can't be exchanged with another process. The caller
    // keeps ownership of non_streamable.
    const NonStreamableSolverInitArguments* non_streamable = nullptr;
  };

  // A callback function (if non null) for messages emitted by the solver.
  //
  // See Solver::MessageCallback documentation for details.
  using MessageCallback = std::function<void(const std::vector<std::string>&)>;

  // A callback function (if non null) is a function that validates its input
  // and its output, and if fails, return a status. The invariant is that the
  // solver implementation can rely on receiving valid data. The implementation
  // of this interface must provide valid input (which will be validated) and
  // in error, it will return a status (without actually calling the callback
  // function). This is enforced in the solver.cc layer.
  using Callback = std::function<absl::StatusOr<CallbackResultProto>(
      const CallbackDataProto&)>;

  // A factory builds a solver based on the input model and parameters.
  //
  // Implementation should have a static `New()` function with this signature
  // and no public constructors.
  //
  // The implementation should assume the input ModelProto is valid and is free
  // to CHECK-fail if this is not the case. It should also assume that the input
  // init_args.streamable and init_args.non_streamable are also either not set
  // of set to the arguments of the correct solver.
  using Factory =
      std::function<absl::StatusOr<std::unique_ptr<SolverInterface>>(
          const ModelProto& model, const InitArgs& init_args)>;

  SolverInterface() = default;
  SolverInterface(const SolverInterface&) = delete;
  SolverInterface& operator=(const SolverInterface&) = delete;
  virtual ~SolverInterface() = default;

  // Solves the current model (included all updates).
  //
  // All input arguments are ensured (by solver.cc) to be valid. Furthermore,
  // since all parameters are references or functions (which could be a lambda
  // expression), the implementation should not keep a reference or copy of
  // them, as they may become invalid reference after the invocation if this
  // function.
  //
  // Parameters `message_cb`, `cb` and `interrupter` are optional. They are
  // nullptr when not set.
  //
  // When parameter `message_cb` is not null and the underlying solver does not
  // supports message callbacks, it must return an InvalidArgumentError with the
  // message internal::kMessageCallbackNotSupported.
  //
  // Solvers should return a InvalidArgumentError when called with events on
  // callback_registration that are not supported by the solver for the type of
  // model being solved (for example MIP events if the model is an LP, or events
  // that are not emitted by the solver). Solvers should use
  // CheckRegisteredCallbackEvents() to implement that.
  virtual absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      SolveInterrupter* interrupter) = 0;

  // Updates the model to solve and returns true, or returns false if this
  // update is not supported.
  //
  // The implementation should assume the input ModelUpdate is valid and is free
  // to assert if this is not the case.
  virtual absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) = 0;
};

class AllSolversRegistry {
 public:
  AllSolversRegistry(const AllSolversRegistry&) = delete;
  AllSolversRegistry& operator=(const AllSolversRegistry&) = delete;

  static AllSolversRegistry* Instance();

  // Maps the given factory to the given solver type. Calling this twice will
  // result in an error, using static initialization is recommended, e.g. see
  // MATH_OPT_REGISTER_SOLVER defined below.
  //
  // Required: factory must be threadsafe.
  void Register(SolverTypeProto solver_type, SolverInterface::Factory factory);

  // Invokes the factory associated to the solver type with the provided
  // arguments.
  absl::StatusOr<std::unique_ptr<SolverInterface>> Create(
      SolverTypeProto solver_type, const ModelProto& model,
      const SolverInterface::InitArgs& init_args) const;

  // Whether a solver type is supported.
  bool IsRegistered(SolverTypeProto solver_type) const;

  // List all supported solver types.
  std::vector<SolverTypeProto> RegisteredSolvers() const;

  // Returns a human-readable list of supported solver types.
  std::string RegisteredSolversToString() const;

 private:
  AllSolversRegistry() = default;

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<SolverTypeProto, SolverInterface::Factory>
      registered_solvers_;
};

// Use to ensure that a solver is registered exactly one time. Invoke in each cc
// file implementing a SolverInterface. Example use:
//
// MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GSCIP, GScipSolver::New)
//
// Can only be used once per cc file.
//
// Arguments:
//   solver_type: A SolverTypeProto proto enum.
//   solver_factory: A SolverInterface::Factory for solver_type.
#define MATH_OPT_REGISTER_SOLVER(solver_type, solver_factory)              \
  namespace {                                                              \
  const void* const kRegisterSolver ABSL_ATTRIBUTE_UNUSED = [] {           \
    AllSolversRegistry::Instance()->Register(solver_type, solver_factory); \
    return nullptr;                                                        \
  }();                                                                     \
  }  // namespace

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_H_
