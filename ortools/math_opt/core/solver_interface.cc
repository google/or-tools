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

#include "ortools/math_opt/core/solver_interface.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

// Returns the name of the solver type or "unknown(xx)" if the integer does not
// match a known value.
std::string SolverTypeName(const SolverTypeProto solver_type) {
  const std::string name = SolverTypeProto_Name(solver_type);
  if (!name.empty()) {
    return name;
  }
  return absl::StrCat("unknown(", static_cast<int>(solver_type), ")");
}

}  // namespace

AllSolversRegistry* absl_nullable AllSolversRegistry::temporary_test_instance_ =
    nullptr;

AllSolversRegistry* absl_nonnull AllSolversRegistry::Instance() {
  static AllSolversRegistry* const instance = new AllSolversRegistry;

  // If a temporary_test_instance_ is set, returns it.
  if (temporary_test_instance_ != nullptr) {
    return temporary_test_instance_;
  }

  return instance;
}

AllSolversRegistry::AllSolversRegistry(
    const AllSolversRegistry& other,
    const absl::flat_hash_set<SolverTypeProto>& kept) {
  // Extract copies of the factories we will keep from other.
  absl::flat_hash_map<SolverTypeProto, SolverInterface::Factory>
      kept_registered_solvers;
  {  // Limit the scope of lock.
    const absl::MutexLock lock(other.mutex_);
    for (const auto& [solver_type, factory] : other.registered_solvers_) {
      if (kept.contains(solver_type)) {
        kept_registered_solvers.try_emplace(solver_type, factory);
      }
    }
  }

  // Find out if all expected solver types were found.
  for (const SolverTypeProto solver_type : kept) {
    CHECK(kept_registered_solvers.contains(solver_type))
        << "Kept solver type " << SolverTypeName(solver_type)
        << " was not registered in AllSolversRegistry::Instance().";
  }

  registered_solvers_ = std::move(kept_registered_solvers);
}

void AllSolversRegistry::SetTemporaryTestInstance(AllSolversRegistry* const
                                                  absl_nullable temp_instance) {
  // When we reset we don't test the current value as it is not possible to
  // override a value anyway.
  if (temp_instance == nullptr) {
    CHECK(temporary_test_instance_ != nullptr)
        << "Can't reset temporary_test_instance_ if not already set!";
    temporary_test_instance_ = nullptr;
    return;
  }

  // At this point temp_instance is not nullptr, set the value of
  // temporary_test_instance_ iff the current value is null.
  CHECK(temporary_test_instance_ == nullptr)
      << "Can't set temporary_test_instance_ to " << temp_instance
      << ", it is already set to " << temporary_test_instance_ << "!";
  temporary_test_instance_ = temp_instance;
}

void AllSolversRegistry::Register(const SolverTypeProto solver_type,
                                  SolverInterface::Factory factory) {
  bool inserted;
  {
    const absl::MutexLock lock(mutex_);
    inserted =
        registered_solvers_.emplace(solver_type, std::move(factory)).second;
  }
  CHECK(inserted) << "Solver type: " << ProtoEnumToString(solver_type)
                  << " already registered.";
}

absl::StatusOr<std::unique_ptr<SolverInterface>> AllSolversRegistry::Create(
    SolverTypeProto solver_type, const ModelProto& model,
    const SolverInterface::InitArgs& init_args) const {
  const SolverInterface::Factory* factory = nullptr;
  {
    const absl::MutexLock lock(mutex_);
    factory = gtl::FindOrNull(registered_solvers_, solver_type);
  }
  if (factory == nullptr) {
    return util::InvalidArgumentErrorBuilder()
           << "solver type " << SolverTypeName(solver_type)
           << " is not registered"
           << ", support for this solver has not been compiled";
  }
  return (*factory)(model, init_args);
}

bool AllSolversRegistry::IsRegistered(const SolverTypeProto solver_type) const {
  const absl::MutexLock lock(mutex_);
  return registered_solvers_.contains(solver_type);
}

std::vector<SolverTypeProto> AllSolversRegistry::RegisteredSolvers() const {
  std::vector<SolverTypeProto> result;
  {
    const absl::MutexLock lock(mutex_);
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
    const absl::MutexLock lock(mutex_);
    for (const auto& kv_pair : registered_solvers_) {
      solver_names.push_back(ProtoEnumToString(kv_pair.first));
    }
  }
  std::sort(solver_names.begin(), solver_names.end());
  return absl::StrCat("[", absl::StrJoin(solver_names, ","), "]");
}

}  // namespace math_opt
}  // namespace operations_research
