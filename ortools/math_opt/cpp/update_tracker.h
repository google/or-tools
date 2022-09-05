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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_UPDATE_TRACKER_H_
#define OR_TOOLS_MATH_OPT_CPP_UPDATE_TRACKER_H_

#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"  // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// Tracks the changes of the model.
//
// This is an advanced feature that most users won't need. It is used internally
// to implement incrementalism but users don't have to understand how it works
// to use incremental solve.
//
// For each update tracker we define a checkpoint that is the starting point
// used to compute the ModelUpdateProto.
//
// No member function should be called after the destruction of the Model
// object. Note though that it is safe to call the destructor of UpdateTracker
// even if the Model object has been destroyed already.
//
// Thread-safety: UpdateTracker methods must not be used while modifying the
// model (variables, constraints, ...). The user is expected to use proper
// synchronization primitives to serialize changes to the model and the use of
// the update trackers. The methods of different instances of UpdateTracker are
// safe to be called concurrently (i.e. multiple trackers can be called
// concurrently on ExportModelUpdate() or AdvanceCheckpoint()). The destructor
// of UpdateTracker is thread-safe.
//
// Example:
//   Model model;
//   ...
//   const std::unique_ptr<UpdateTracker> update_tracker =
//     model.NewUpdateTracker();
//
//   model.AddVariable(0.0, 1.0, true, "y");
//   model.set_maximize(true);
//
//   ASSIGN_OR_RETURN(const std::optional<ModelUpdateProto> update_proto,
//                    update_tracker.ExportModelUpdate());
//   RETURN_IF_ERROR(update_tracker.AdvanceCheckpoint());
//
//   if (update_proto) {
//     ... use *update_proto here ...
//   }
class UpdateTracker {
 public:
  // This constructor should not be used directly. Instead use
  // Model::NewUpdateTracker().
  explicit UpdateTracker(const std::shared_ptr<ModelStorage>& storage);

  ~UpdateTracker();

  // Returns a proto representation of the changes to the model since the most
  // recent checkpoint (i.e. last time AdvanceCheckpoint() was called); nullopt
  // if the update would have been empty.
  //
  // If fails if the Model has been destroyed.
  absl::StatusOr<std::optional<ModelUpdateProto>> ExportModelUpdate();

  // Uses the current model state as the starting point to calculate the
  // ModelUpdateProto next time ExportModelUpdate() is called.
  //
  // If fails if the Model has been destroyed.
  absl::Status AdvanceCheckpoint();

  // Returns a proto representation of the whole model.
  //
  // This is a shortcut method that is equivalent to calling
  // Model::ExportModel(). It is there so that users of the UpdateTracker
  // can avoid having to keep a reference to the Model model.
  //
  // If fails if the Model has been destroyed.
  absl::StatusOr<ModelProto> ExportModel() const;

 private:
  const std::weak_ptr<ModelStorage> storage_;
  const UpdateTrackerId update_tracker_;
};

namespace internal {

// The failure message used when a function of UpdateTracker is called after the
// destruction of the model..
constexpr absl::string_view kModelIsDestroyed =
    "can't call this function after the associated model has been destroyed";

}  // namespace internal

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_UPDATE_TRACKER_H_
