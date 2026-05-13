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

#include "ortools/sat/cp_model_copy.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;
using ::testing::IsEmpty;

TEST(ModelCopyTest, IntervalsAddLinearConstraints) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      enforcement_literal: 0
      interval {
        start: { vars: 1 coeffs: 1 }
        size: { vars: 2 coeffs: 1 }
        end: { vars: 3 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  CpModelProto new_cp_model;
  PresolveContext context(&model, &new_cp_model, nullptr);

  ImportModelWithBasicPresolveIntoContext(initial_model, &context);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      enforcement_literal: 0
      interval {
        start: { vars: 1 coeffs: 1 }
        size: { vars: 2 coeffs: 1 }
        end: { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1, 2, 3 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }

    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, IntervalsWithFixedStartAndEnd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    constraints {
      interval {
        start: { vars: 0 coeffs: 1 }
        size: { vars: 1 coeffs: 1 }
        end: { vars: 2 coeffs: 2 }
      }
    }
  )pb");

  Model model;
  CpModelProto new_cp_model;
  PresolveContext context(&model, &new_cp_model, nullptr);

  ImportModelWithBasicPresolveIntoContext(initial_model, &context);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 10, 10 ] }
    constraints {
      interval {
        start { offset: 10 }
        end { offset: 20 }
        size { offset: 10 }
      }
    }
  )pb");
  EXPECT_THAT(new_cp_model, EqualsProto(expected_model));
}

TEST(ModelCopyTest, RemoveDuplicateFromClauses) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3, 2 ]
      bool_or { literals: [ -4, 9, 8 ] }
    }
  )pb");
  const CpModelProto expected_moded = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -2, -3, -4, 9, 8 ] } }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  PresolveContext context(&model, &new_cp_model, nullptr);
  ImportModelWithBasicPresolveIntoContext(initial_model, &context);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_moded));
}

TEST(ModelCopyTest, RemoveDuplicateFromEnforcementLiterals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3, 2 ]  # duplicate
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 100 ]
      }
    }
    constraints {
      enforcement_literal: [ 2, -3, 2 ]  # can never be enforced.
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 2, 1, 3 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");
  const CpModelProto expected_moded = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      enforcement_literal: [ 1, 2, 3 ]
      linear {
        vars: [ 4, 5, 6 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");
  CpModelProto new_cp_model;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  PresolveContext context(&model, &new_cp_model, nullptr);
  ImportModelWithBasicPresolveIntoContext(initial_model, &context);
  EXPECT_THAT(new_cp_model, EqualsProto(expected_moded));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
