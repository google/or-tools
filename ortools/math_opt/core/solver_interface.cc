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

#include "ortools/math_opt/core/solver_interface.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

AllSolversRegistry* AllSolversRegistry::Instance() {
  static AllSolversRegistry* const instance = new AllSolversRegistry;
  return instance;
}

void AllSolversRegistry::Register(const SolverType solver_type,
                                  SolverInterface::Factory factory) {
  bool inserted;
  {
    const absl::MutexLock lock(&mutex_);
    inserted =
        registered_solvers_.emplace(solver_type, std::move(factory)).second;
  }
  CHECK(inserted) << "Solver type: " << ProtoEnumToString(solver_type)
                  << " already registered.";
}

absl::StatusOr<std::unique_ptr<SolverInterface>> AllSolversRegistry::Create(
    SolverType solver_type, const ModelProto& model,
    const SolverInitializerProto& initializer) const {
  const SolverInterface::Factory* factory = nullptr;
  {
    const absl::MutexLock lock(&mutex_);
    factory = gtl::FindOrNull(registered_solvers_, solver_type);
  }
  if (factory == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Solver type: ", ProtoEnumToString(solver_type),
                     " is not registered."));
  }
  return (*factory)(model, initializer);
}

bool AllSolversRegistry::IsRegistered(const SolverType solver_type) const {
  const absl::MutexLock lock(&mutex_);
  return registered_solvers_.contains(solver_type);
}

std::vector<SolverType> AllSolversRegistry::RegisteredSolvers() const {
  std::vector<SolverType> result;
  {
    const absl::MutexLock lock(&mutex_);
    for (const auto& kv_pair : registered_solvers_) {
      result.push_back(kv_pair.first);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::string AllSolversRegistry::RegisteredSolversToString() const {
  std::vector<std::string> solver_names;
  {
    const absl::MutexLock lock(&mutex_);
    for (const auto& kv_pair : registered_solvers_) {
      solver_names.push_back(ProtoEnumToString(kv_pair.first));
    }
  }
  std::sort(solver_names.begin(), solver_names.end());
  return absl::StrCat("[", absl::StrJoin(solver_names, ","), "]");
}

}  // namespace math_opt
}  // namespace operations_research
