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

#include "ortools/glop/parameters_validation.h"

#include <limits>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"

namespace operations_research::glop {
namespace {

using ::testing::HasSubstr;

TEST(ValidateParameters, CorrectCase) {
  GlopParameters params;
  params.set_max_time_in_seconds(10);
  params.set_use_dual_simplex(true);
  params.set_primal_feasibility_tolerance(1e-15);
  EXPECT_EQ(ValidateParameters(params), "");
}

TEST(ValidateParameters, MaxTimeInSeconds) {
  GlopParameters params;
  params.set_max_time_in_seconds(-1);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("non-negative"));
}

TEST(ValidateParameters, MarkowitzZlatevParameter) {
  GlopParameters params;
  params.set_markowitz_zlatev_parameter(0);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("must be >= 1"));
}

TEST(ValidateParameters, NaNs) {
  const google::protobuf::Descriptor& descriptor =
      *GlopParameters::descriptor();
  const google::protobuf::Reflection& reflection =
      *GlopParameters::GetReflection();
  for (int i = 0; i < descriptor.field_count(); ++i) {
    const google::protobuf::FieldDescriptor* const field = descriptor.field(i);
    SCOPED_TRACE(field->name());

    GlopParameters params;
    switch (field->type()) {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        reflection.SetDouble(&params, field,
                             std::numeric_limits<double>::quiet_NaN());
        break;
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        reflection.SetFloat(&params, field,
                            std::numeric_limits<float>::quiet_NaN());
        break;
      default:
        continue;
    }

    EXPECT_THAT(ValidateParameters(params),
                AllOf(HasSubstr(field->name()), HasSubstr("NaN")));
  }
}

}  // namespace
}  // namespace operations_research::glop
