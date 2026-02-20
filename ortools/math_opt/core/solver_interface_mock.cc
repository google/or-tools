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

#include "ortools/math_opt/core/solver_interface_mock.h"

#include <functional>
#include <memory>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/synchronization/mutex.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"

namespace operations_research {
namespace math_opt {

namespace {

// Returns a random solver type.
SolverTypeProto RandomSolverType() {
  // Pick random values in [SolverType_MIN + 1, SolverType_MAX] and return the
  // first valid one (using SolverType_MIN + 1 excludes
  // SOLVER_TYPE_UNSPECIFIED).
  //
  // As of 2025-10-16 solver type values are dense so we should not loop.
  constexpr int kNumAttempts = 1;
  absl::BitGen gen;
  for (int i = 0; i < kNumAttempts; ++i) {
    static_assert(SOLVER_TYPE_UNSPECIFIED == SolverTypeProto_MIN);
    const int choice =
        absl::Uniform(absl::IntervalClosedClosed, gen,
                      // Excludes SOLVER_TYPE_UNSPECIFIED; see static_assert().
                      SolverTypeProto_MIN + 1, SolverTypeProto_MAX);
    if (SolverTypeProto_IsValid(choice)) {
      return static_cast<SolverTypeProto>(choice);
    }
  }
  LOG(FATAL) << "Failed to pick a random SOLVER_TYPE after " << kNumAttempts
             << " attempts.";
}

}  // namespace

SolverFactoryRegistration::SolverFactoryRegistration(
    SolverInterface::Factory factory)
    : caller_data_(std::make_shared<CallerData>(factory)),
      solver_type_(RandomSolverType()) {
  // We make a copy of the shared_ptr since we make a copy of the member field
  // in the capture below. It must be an identifier.
  const auto caller_data = caller_data_;

  // We register a lambda that shares the same CallerData instance at this class
  // using a shared_ptr. Note that here alternate_registry_ has been constructed
  // and injected into AllSolversRegistry thus AllSolversRegistry::Instance() is
  // a temporary and empty registry.
  AllSolversRegistry::Instance()->Register(
      solver_type_, [caller_data](const ModelProto& model,
                                  const SolverInterface::InitArgs& init_args) {
        // We hold the lock during the call of the factory since we want to
        // delay the destruction of the registration during the call to the
        // factory (the factory may be invalid after the destruction).
        absl::MutexLock lock(caller_data->mutex);
        CHECK(caller_data->factory != nullptr)
            << "Can't use this solver factory after the destruction of the "
               "SolverFactoryRegistration!";
        return caller_data->factory(model, init_args);
      });
}

SolverFactoryRegistration::~SolverFactoryRegistration() {
  absl::MutexLock lock(caller_data_->mutex);
  caller_data_->factory = nullptr;
}

SolverFactoryRegistration::CallerData::CallerData(
    SolverInterface::Factory factory)
    : factory(ABSL_DIE_IF_NULL(factory)) {}

}  // namespace math_opt
}  // namespace operations_research
