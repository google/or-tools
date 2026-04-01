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

#include "ortools/math_opt/cpp/update_tracker.h"

#include <memory>
#include <optional>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

UpdateTracker::~UpdateTracker() {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();

  // If the model has already been destroyed, the update tracker has been
  // automatically cleaned.
  if (storage != nullptr) {
    storage->DeleteUpdateTracker(update_tracker_);
  }
}

UpdateTracker::UpdateTracker(const std::shared_ptr<ModelStorage>& storage)
    : storage_(ABSL_DIE_IF_NULL(storage)),
      update_tracker_(storage->NewUpdateTracker()) {}

absl::StatusOr<std::optional<ModelUpdateProto>>
UpdateTracker::ExportModelUpdate(const bool remove_names) {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  return storage->ExportModelUpdate(update_tracker_, remove_names);
}

absl::Status UpdateTracker::AdvanceCheckpoint() {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  storage->AdvanceCheckpoint(update_tracker_);
  return absl::OkStatus();
}

absl::StatusOr<ModelProto> UpdateTracker::ExportModel(
    const bool remove_names) const {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  return storage->ExportModel(remove_names);
}

}  // namespace math_opt
}  // namespace operations_research
