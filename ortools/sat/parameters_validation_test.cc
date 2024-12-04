// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/parameters_validation.h"

#include <limits>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(ValidateParameters, MaxTimeInSeconds) {
  SatParameters params;
  params.set_max_time_in_seconds(-1);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("non-negative"));
}

TEST(ValidateParameters, ParametersInRange) {
  SatParameters params;
  params.set_mip_max_bound(-1);
  EXPECT_THAT(ValidateParameters(params),
              HasSubstr("'mip_max_bound' should be in"));
}

TEST(ValidateParameters, NumWorkers) {
  SatParameters params;
  params.set_num_workers(-1);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("should be in [0,10000]"));
}

TEST(ValidateParameters, NumSearchWorkers) {
  SatParameters params;
  params.set_num_search_workers(-1);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("should be in [0,10000]"));
}

TEST(ValidateParameters, LinearizationLevel) {
  SatParameters params;
  params.set_linearization_level(-1);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("non-negative"));
}

TEST(ValidateParameters, NumSharedTreeSearchWorkers) {
  SatParameters params;
  params.set_shared_tree_num_workers(-2);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("should be in [-1,10000]"));
}

TEST(ValidateParameters, SharedTreeSearchMaxNodesPerWorker) {
  SatParameters params;
  params.set_shared_tree_max_nodes_per_worker(0);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("positive"));
}

TEST(ValidateParameters, SharedTreeSearchOpenLeavesPerWorker) {
  SatParameters params;
  params.set_shared_tree_open_leaves_per_worker(0.0);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("should be in [1,10000]"));
}

TEST(ValidateParameters, UseSharedTreeSearch) {
  SatParameters params;
  params.set_use_shared_tree_search(true);
  EXPECT_THAT(ValidateParameters(params), HasSubstr("only be set on workers"));
}

TEST(ValidateParameters, NaNs) {
  const google::protobuf::Descriptor& descriptor = *SatParameters::descriptor();
  const google::protobuf::Reflection& reflection =
      *SatParameters::GetReflection();
  for (int i = 0; i < descriptor.field_count(); ++i) {
    const google::protobuf::FieldDescriptor* const field = descriptor.field(i);
    SCOPED_TRACE(field->name());

    SatParameters params;
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

TEST(ValidateParameters, ValidateSubsolvers) {
  SatParameters params;
  params.add_extra_subsolvers("not_defined");
  EXPECT_THAT(ValidateParameters(params), HasSubstr("is not valid"));

  params.add_subsolver_params()->set_name("not_defined");
  EXPECT_THAT(ValidateParameters(params), IsEmpty());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
