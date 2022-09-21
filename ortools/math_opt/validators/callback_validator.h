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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_CALLBACK_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_CALLBACK_VALIDATOR_H_

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"

namespace operations_research {
namespace math_opt {

// Checks that CallbackRegistrationProto is valid given a valid model summary.
absl::Status ValidateCallbackRegistration(
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary);

// Checks that CallbackDataProto is valid given a valid model summary and
// CallbackRegistrationProto.
absl::Status ValidateCallbackDataProto(
    const CallbackDataProto& cb_data,
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary);

// Checks that CallbackResultProto is valid given a valid model summary,
// CallbackEventProto (which since it is assumed to be valid, can not be
// CALLBACK_EVENT_UNSPECIFIED) and a CallbackRegistrationProto.
absl::Status ValidateCallbackResultProto(
    const CallbackResultProto& callback_result, CallbackEventProto event,
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary);

// Returns an InvalidArgumentError if some of the registered events are not
// supported.
absl::Status CheckRegisteredCallbackEvents(
    const CallbackRegistrationProto& registration,
    const absl::flat_hash_set<CallbackEventProto>& supported_events);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_CALLBACK_VALIDATOR_H_
