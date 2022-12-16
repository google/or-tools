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

#include "ortools/math_opt/storage/model_storage_v2.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {
namespace {

template <typename T>
std::vector<T> ConvertIdVector(absl::Span<const int64_t> ids) {
  std::vector<T> result;
  result.reserve(ids.size());
  for (const int64_t id : ids) {
    result.push_back(T{id});
  }
  return result;
}

template <typename T, int i, typename AttrKeyT>
std::vector<T> ConvertKeyVector(const std::vector<AttrKeyT>& keys) {
  std::vector<T> result;
  result.reserve(keys.size());
  for (const AttrKeyT key : keys) {
    result.push_back(T{key[i]});
  }
  return result;
}

template <typename T>
std::vector<T> Sorted(std::vector<T> vec) {
  absl::c_sort(vec);
  return vec;
}

}  // namespace

void ModelStorageV2::DeleteVariable(const VariableId id) {
  CHECK(elemental_.DeleteElement(id))
      << "cannot delete variable with id: " << id << ", it is not in the model";
}

void ModelStorageV2::DeleteLinearConstraint(LinearConstraintId id) {
  CHECK(elemental_.DeleteElement(id))
      << "cannot delete linear constraint with id: " << id
      << ", it is not in the model";
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<ModelStorageV2>>>
ModelStorageV2::FromModelProto(const ModelProto& model_proto) {
  ASSIGN_OR_RETURN(Elemental e, Elemental::FromModelProto(model_proto));
  return absl::WrapUnique(new ModelStorageV2(std::move(e)));
}

absl::Nonnull<std::unique_ptr<ModelStorageV2>> ModelStorageV2::Clone(
    const std::optional<absl::string_view> new_name) const {
  return absl::WrapUnique(new ModelStorageV2(elemental_.Clone(new_name)));
}

std::vector<VariableId> ModelStorageV2::Variables() const {
  return ConvertIdVector<VariableId>(
      elemental_.AllElementsUntyped(ElementType::kVariable));
}
std::vector<VariableId> ModelStorageV2::SortedVariables() const {
  return Sorted(Variables());
}

std::vector<VariableId> ModelStorageV2::LinearObjectiveNonzeros(
    const ObjectiveId id) const {
  if (id.has_value()) {
    const auto slice =
        elemental_.Slice<0>(DoubleAttr2::kAuxObjLinCoef, id->value());
    return ConvertKeyVector<VariableId, 1>(slice);
  } else {
    return ConvertKeyVector<VariableId, 0>(
        elemental_.AttrNonDefaults(DoubleAttr1::kObjLinCoef));
  }
}

std::vector<LinearConstraintId> ModelStorageV2::LinearConstraints() const {
  return ConvertIdVector<LinearConstraintId>(
      elemental_.AllElementsUntyped(ElementType::kLinearConstraint));
}

std::vector<LinearConstraintId> ModelStorageV2::SortedLinearConstraints()
    const {
  return Sorted(LinearConstraints());
}

absl::StatusOr<ModelProto> ModelStorageV2::ExportModelV2(
    const bool remove_names) const {
  return elemental_.ExportModel(remove_names);
}

UpdateTrackerId ModelStorageV2::NewUpdateTracker() {
  return UpdateTrackerId(elemental_.AddDiff().id());
}

void ModelStorageV2::DeleteUpdateTracker(const UpdateTrackerId update_tracker) {
  const std::optional<Elemental::DiffHandle> diff =
      elemental_.GetDiffHandle(update_tracker.value());
  CHECK(diff.has_value()) << "UpdateTrackerId " << update_tracker
                          << " not found";
  elemental_.DeleteDiff(*diff);
}

absl::StatusOr<std::optional<ModelUpdateProto>>
ModelStorageV2::ExportModelUpdateV2(const UpdateTrackerId update_tracker,
                                    const bool remove_names) const {
  const std::optional<Elemental::DiffHandle> diff =
      elemental_.GetDiffHandle(update_tracker.value());
  CHECK(diff.has_value()) << "UpdateTrackerId " << update_tracker
                          << " not found";
  return elemental_.ExportModelUpdate(*diff, remove_names);
}

void ModelStorageV2::AdvanceCheckpoint(const UpdateTrackerId update_tracker) {
  const std::optional<Elemental::DiffHandle> diff =
      elemental_.GetDiffHandle(update_tracker.value());
  CHECK(diff.has_value()) << "UpdateTrackerId " << update_tracker
                          << " not found";
  elemental_.Advance(*diff);
}

absl::Status ModelStorageV2::ApplyUpdateProto(
    const ModelUpdateProto& update_proto) {
  LOG(FATAL) << "not implemented";
}

}  // namespace operations_research::math_opt
