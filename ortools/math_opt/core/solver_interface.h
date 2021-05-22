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

#ifndef OR_TOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_H_
#define OR_TOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

// Interface implemented by actual solvers.
//
// This interface is not meant to be used directly. The actual API is the one of
// the Solver class. The Solver class validates the models before calling this
// interface.
//
// Implementations of this interface should not have public constructors but
// instead have a static `New` function with the signature of Factory function
// as defined below. They should register this factory using the macro
// MATH_OPT_REGISTER_SOLVER().
class SolverInterface {
 public:
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
  // to CHECK-fail if this is not the case.
  using Factory =
      std::function<absl::StatusOr<std::unique_ptr<SolverInterface>>(
          const ModelProto& model, const SolverInitializerProto& initializer)>;

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
  virtual absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      const CallbackRegistrationProto& callback_registration, Callback cb) = 0;

  // Updates the model to solve.
  //
  // The implementation should assume the input ModelUpdate is valid and is free
  // to assert if this is not the case.
  virtual absl::Status Update(const ModelUpdateProto& model_update) = 0;

  // Return true if this solver can handle the given update.
  //
  // The implementation should assume the input ModelUpdate is valid and is free
  // to assert if this is not the case.
  virtual bool CanUpdate(const ModelUpdateProto& model_update) = 0;
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
  void Register(SolverType solver_type, SolverInterface::Factory factory);

  // Invokes the factory associated to the solver type with the provided
  // arguments.
  absl::StatusOr<std::unique_ptr<SolverInterface>> Create(
      SolverType solver_type, const ModelProto& model,
      const SolverInitializerProto& initializer) const;

  // Whether a solver type is supported.
  bool IsRegistered(SolverType solver_type) const;

  // List all supported solver types.
  std::vector<SolverType> RegisteredSolvers() const;

  // Returns a human-readable list of supported solver types.
  std::string RegisteredSolversToString() const;

 private:
  AllSolversRegistry() = default;

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<SolverType, SolverInterface::Factory> registered_solvers_;
};

// Use to ensure that a solver is registered exactly one time. Invoke in each cc
// file implementing a SolverInterface. Example use:
//
// MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GSCIP, GScipSolver::New)
//
// Can only be used once per cc file.
//
// Arguments:
//   solver_type: A SolverType proto enum.
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
