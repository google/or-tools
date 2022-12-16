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

#include <optional>

#include "absl/status/statusor.h"
#include "ortools/base/fuzztest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elemental_matcher.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::status::IsOkAndHolds;

void FromProtoNeverCrashes(const ModelProto& proto) {
  Elemental::FromModelProto(proto).IgnoreError();
}

FUZZ_TEST(ElementalFromProtoNoCrashTest, FromProtoNeverCrashes);

void RoundTripsIfParses(const ModelProto& proto) {
  absl::StatusOr<Elemental> e1 = Elemental::FromModelProto(proto);
  if (!e1.ok()) {
    return;
  }
  ASSERT_OK_AND_ASSIGN(ModelProto p2, e1->ExportModel());
  EXPECT_THAT(Elemental::FromModelProto(p2),
              IsOkAndHolds(EquivToElemental(*e1)));
}

FUZZ_TEST(ElementalRoundTripTest, RoundTripsIfParses);

void ApplyUpdateProtoNoCrash(const ModelProto& proto,
                             const ModelUpdateProto& u1,
                             const ModelUpdateProto& u2) {
  absl::StatusOr<Elemental> elemental = Elemental::FromModelProto(proto);
  if (!elemental.ok()) {
    return;
  }
  elemental->ApplyUpdateProto(u1).IgnoreError();
  elemental->ApplyUpdateProto(u2).IgnoreError();
}

FUZZ_TEST(ElementalApplyUpdateProtoNoCrashTest, ApplyUpdateProtoNoCrash);

void UpdateRoundTripsIfParses(const ModelProto& proto,
                              const ModelUpdateProto& update) {
  absl::StatusOr<Elemental> model = Elemental::FromModelProto(proto);
  if (!model.ok()) {
    return;
  }
  const Elemental::DiffHandle diff = model->AddDiff();
  if (!model->ApplyUpdateProto(update).ok()) {
    return;
  }
  ASSERT_OK_AND_ASSIGN(const std::optional<ModelUpdateProto> canonical_update,
                       model->ExportModelUpdate(diff));
  if (canonical_update.has_value()) {
    ASSERT_OK_AND_ASSIGN(Elemental model2, Elemental::FromModelProto(proto));
    ASSERT_OK(model2.ApplyUpdateProto(*canonical_update));
    EXPECT_THAT(*model, EquivToElemental(model2));
  }
}

FUZZ_TEST(ElementalUpdateRoundTripTest, UpdateRoundTripsIfParses);

}  // namespace
}  // namespace operations_research::math_opt
