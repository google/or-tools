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

#include "ortools/sat/linear_model.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;

TEST(LinearModelTest, DetectFullEncoding) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 0, 5, 5, 10, 10 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 6
      linear { vars: 5 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -7
      linear { vars: 5 coeffs: 1 domain: 1 domain: 2 }
    }
    constraints {
      exactly_one { literals: 7 literals: 8 literals: 9 literals: 10 }
    }
    constraints {
      enforcement_literal: 7
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -8
      linear { vars: 0 coeffs: 1 domain: 1 domain: 3 }
    }
    constraints {
      enforcement_literal: 7
      linear { vars: 5 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -8
      linear { vars: 5 coeffs: 1 domain: 0 domain: 0 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: 8
      linear { vars: 0 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -9
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 domain: 2 domain: 3 }
    }
    constraints {
      enforcement_literal: 9
      linear { vars: 0 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -10
      linear { vars: 0 coeffs: 1 domain: 0 domain: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: 9
      linear { vars: 5 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -10
      linear { vars: 5 coeffs: 1 domain: 0 domain: 1 }
    }
    constraints {
      enforcement_literal: 10
      linear { vars: 0 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -11
      linear { vars: 0 coeffs: 1 domain: 0 domain: 2 }
    }
    constraints { exactly_one { literals: -7 literals: 8 literals: 10 } }
    constraints {
      linear { vars: 1 vars: 5 coeffs: 1 coeffs: -5 domain: 0 domain: 0 }
    }
  )pb");

  LinearModel linear_model(cp_model);
  int num_ignored = 0;
  for (const bool is_ignored : linear_model.ignored_constraints()) {
    if (is_ignored) num_ignored++;
  }
  EXPECT_EQ(num_ignored, 14);
  ASSERT_EQ(linear_model.additional_constraints().size(), 3);
  const ConstraintProto ct0 = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 8, 9, 10 ]
      coeffs: [ -1, 1, 2, 3 ]
      domain: [ 0, 0 ]
    }
  )pb");
  const ConstraintProto ct1 = ParseTestProto(R"pb(
    linear {
      vars: [ 5, 7, 9 ]
      coeffs: [ -1, 1, 2 ]
      domain: [ 0, 0 ]
    }
  )pb");
  const ConstraintProto ct2 = ParseTestProto(R"pb(
    exactly_one { literals: [ 6, 7, 9 ] }
  )pb");
  EXPECT_THAT(ct0, EqualsProto(linear_model.additional_constraints()[0]));
  EXPECT_THAT(ct1, EqualsProto(linear_model.additional_constraints()[1]));
  EXPECT_THAT(ct2, EqualsProto(linear_model.additional_constraints()[2]));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
