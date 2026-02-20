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

#ifndef ORTOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_TESTING_H_
#define ORTOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_TESTING_H_

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/core/solver_interface.h"

namespace operations_research::math_opt {

// Configuration parameters for WithAlternateAllSolversRegistry.
struct WithAlternateAllSolversRegistryConfiguration {
  // The solver types to keep from the actual registry.
  //
  // By default keep nothing. If a solver type is listed but not registered a
  // LOG(FATAL) ends the test.
  absl::flat_hash_set<SolverTypeProto> kept;

  // The solver types to override in the temporary registry.
  //
  // They must not appear in the `kept` set or else it will LOG(FATAL) on
  // construction of WithAlternateAllSolversRegistry.
  absl::flat_hash_map<SolverTypeProto, absl_nonnull SolverInterface::Factory>
      overridden;
};

// Scoped object for temporarily replacing the AllSolversRegistry::Instance() in
// a unit test.
//
// Usage:
//
//   TEST(MyTest, Something) {
//     const WithAlternateAllSolversRegistry alternate_registry({
//       .kept={SOLVER_TYPE_GSCIP},
//       .overridden={{SOLVER_TYPE_GLOP, FakeGlopFactory}},
//     });
//     // At this point we have two registered solvers:
//     // * SOLVER_TYPE_GSCIP: using the usual factory
//     // * SOLVER_TYPE_GLOP: using FakeGlopFactory()
//     ...
//   }
class WithAlternateAllSolversRegistry {
 public:
  // After the constructor returns and until this object is destroyed, the
  // AllSolversRegistry::Instance() will be replaced by a new instance with the
  // changes described in the configuration.
  //
  // It is an error to have two instances alive at the same time. If this
  // happens a LOG(FATAL) will stop the test.
  explicit WithAlternateAllSolversRegistry(
      WithAlternateAllSolversRegistryConfiguration configuration);

  WithAlternateAllSolversRegistry(const WithAlternateAllSolversRegistry&) =
      delete;
  WithAlternateAllSolversRegistry& operator=(
      const WithAlternateAllSolversRegistry&) = delete;

  // Removes the temporary registry and restore the previous value of
  // AllSolversRegistry::Instance().
  ~WithAlternateAllSolversRegistry();

 private:
  AllSolversRegistry temporary_registry_;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_SOLVER_INTERFACE_TESTING_H_
