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

#include "gtest/gtest.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(FlakyTest, Issue3108) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 2
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 1 }
        size { offset: 1 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 1 }
        size { offset: 1 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 4 coeffs: 1 }
        intervals: 0
        intervals: 1
        demands { offset: 1 }
        demands { offset: 1 }
      }
    }
    constraints {
      enforcement_literal: 2
      linear { vars: 0 coeffs: 1 domain: 0 domain: 1 }
    }
    constraints {
      enforcement_literal: -3
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: -1
        domain: 2
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 3
      linear { vars: 1 coeffs: 1 domain: 0 domain: 1 }
    }
    constraints {
      enforcement_literal: -4
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: -1
        domain: 2
        domain: 9223372036854775807
      }
    }
    objective { vars: 4 coeffs: 1 }
  )pb");
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  parameters.set_cp_model_probing_level(0);
  parameters.set_num_workers(1);
  const CpSolverResponse response =
      SolveWithParameters(model_proto, parameters);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
