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

#include "ortools/math_opt/cpp/update_tracker.h"

#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/model.pb.h"

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
UpdateTracker::ExportModelUpdate() {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  return storage->ExportModelUpdate(update_tracker_);
}

absl::Status UpdateTracker::Checkpoint() {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  storage->Checkpoint(update_tracker_);
  return absl::OkStatus();
}

absl::StatusOr<ModelProto> UpdateTracker::ExportModel() const {
  const std::shared_ptr<ModelStorage> storage = storage_.lock();
  if (storage == nullptr) {
    return absl::InvalidArgumentError(internal::kModelIsDestroyed);
  }
  return storage->ExportModel();
}

}  // namespace math_opt
}  // namespace operations_research
