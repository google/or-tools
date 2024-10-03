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

#include "ortools/sat/cp_model_postsolve.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

// Note that the postsolve is already tested in many of our solver tests or
// random presolve tests. We just have a small unit test here.
TEST(PostsolveResponseTest, BasicExample) {
  // Fixing z will allow the postsolve code to reconstruct all values.
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    variables { name: 'z' domain: 0 domain: 10 }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, -3 ]
        domain: [ 5, 5 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 3, -1 ]
        domain: [ 5, 5 ]
      }
    }
  )pb");

  std::vector<int64_t> solution = {1};
  std::vector<int> postsolve_mapping = {2};  // The solution fix z.
  PostsolveResponse(/*num_variables_in_original_model=*/3, mapping_proto,
                    postsolve_mapping, &solution);

  // x + 2y - 3z = 5
  // 3y - z = 5
  // z = 1
  EXPECT_THAT(solution, ::testing::ElementsAre(4, 2, 1));
}

TEST(PostsolveResponseTest, ExactlyOneExample1) {
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'z' domain: 0 domain: 1 }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
  )pb");

  std::vector<int64_t> solution = {1};
  std::vector<int> postsolve_mapping = {2};  // The solution fix z.
  PostsolveResponse(/*num_variables_in_original_model=*/3, mapping_proto,
                    postsolve_mapping, &solution);
  EXPECT_THAT(solution, ::testing::ElementsAre(0, 0, 1));
}

TEST(PostsolveResponseTest, ExactlyOneExample2) {
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'z' domain: 0 domain: 1 }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
  )pb");

  std::vector<int64_t> solution = {0};
  std::vector<int> postsolve_mapping = {2};  // The solution fix z.
  PostsolveResponse(/*num_variables_in_original_model=*/3, mapping_proto,
                    postsolve_mapping, &solution);

  // One variable is set to one.
  EXPECT_THAT(solution, ::testing::ElementsAre(0, 1, 0));
}

TEST(PostsolveResponseTest, Element) {
  // Fixing z will allow the postsolve code to reconstruct all values.
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables {
      name: 'index'
      domain: [ 0, 1 ]
    }
    variables {
      name: 'a'
      domain: [ 1, 10 ]
    }
    variables {
      name: 'b'
      domain: [ 0, 10 ]
    }
    variables {
      name: 'target'
      domain: [ 0, 10 ]
    }
    constraints {
      element {
        index: 0
        vars: [ 1, 2 ]
        target: 3
      }
    }
  )pb");

  std::vector<int64_t> solution;
  std::vector<int> postsolve_mapping = {};
  PostsolveResponse(/*num_variables_in_original_model=*/4, mapping_proto,
                    postsolve_mapping, &solution);
  EXPECT_THAT(solution, ::testing::ElementsAre(0, 1, 0, 1));
}

TEST(PostsolveResponseTest, VariableElement) {
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 129 ] }
    variables { domain: [ 1, 5 ] }
    variables { domain: [ 0, 129 ] }
    variables { domain: [ 2, 2 ] }
    constraints { element { index: 3 target: 2 vars: 0 vars: 1 vars: 0 } }
  )pb");

  std::vector<int64_t> solution;
  std::vector<int> postsolve_mapping = {};
  PostsolveResponse(/*num_variables_in_original_model=*/4, mapping_proto,
                    postsolve_mapping, &solution);
  EXPECT_THAT(solution, ::testing::ElementsAre(0, 1, 0, 2));
}

// Note that our postolve code is "limited" when it come to solving a single
// linear equation since we should only encounter "simple" case.
TEST(PostsolveResponseTest, TrickyLinearCase) {
  // The equation is 2x + y = z
  //
  // It mostly work all the time, except if we decide to make z - y not a
  // multiple of two. This is not necessarily detected by our presolve since
  // 2 * [0, 124] is too complex to represent. Yet for any value of x and y
  // there is a possible z, but the reverse is not true, since y = 1, z = 0 is
  // not feasible.
  //
  // The preosolve should deal with that by putting z first so that the
  // postsolve code do not fail.
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 0, 124 ]
    }
    variables {
      name: 'y'
      domain: [ 0, 1 ]
    }
    variables {
      name: 'z'
      domain: [ 0, 255 ]
    }
    constraints {
      linear {
        vars: [ 2, 0, 1 ]
        coeffs: [ -1, 2, 1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");

  // The likely response (there are many possible).
  std::vector<int64_t> solution;
  CpSolverResponse response;
  response.set_status(OPTIMAL);
  std::vector<int> postsolve_mapping;
  PostsolveResponse(/*num_variables_in_original_model=*/3, mapping_proto,
                    postsolve_mapping, &solution);
  EXPECT_THAT(solution, ::testing::ElementsAre(0, 0, 0));
}

// This used to fail because we where computing the EXACT domain atteignable
// by the sum of discrete domains, which have a lot of disjoint part.
//
// But our presolve was fine, because adding each of them to the loose rhs
// domain just result in a domain with a small complexity.
TEST(PostsolveResponseTest, ComplexityIssue) {
  CpModelProto mapping_proto;

  // N variables such that their sum can be and even number. If we try to
  // compute the exact domains of their sum, we are quadratic in compexity.
  const int num_variables = 30;
  for (int i = 0; i < num_variables; ++i) {
    IntegerVariableProto* var = mapping_proto.add_variables();
    var->add_domain(0);
    var->add_domain(0);
    const int value = 1 << (1 + i);
    var->add_domain(value);
    var->add_domain(value);
  }

  // A linear constraint sum variable in [0, 1e9].
  ConstraintProto* ct = mapping_proto.add_constraints();
  ct->mutable_linear()->add_domain(0);
  ct->mutable_linear()->add_domain(1e9);
  for (int i = 0; i < num_variables; ++i) {
    ct->mutable_linear()->add_vars(i);
    ct->mutable_linear()->add_coeffs(1);
  }

  // The likely response (there are many possible).
  std::vector<int64_t> solution;
  std::vector<int> postsolve_mapping;
  PostsolveResponse(num_variables, mapping_proto, postsolve_mapping, &solution);
  ASSERT_EQ(solution.size(), num_variables);
}

TEST(FillTightenedDomainInResponseTest, BasicBehavior) {
  // Original model.
  const CpModelProto original_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 0, 124 ]
    }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 255 ] }
  )pb");

  // We might have more variable there.
  // Also the domains might be tighter.
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 255 ] }
    variables { domain: [ 0, 17 ] }
    variables { domain: [ 0, 18 ] }
  )pb");

  // Lets assume the presolved mode contains 3 variables, 2 in common.
  std::vector<int> postsolve_mapping{0, 2, 4};
  std::vector<Domain> search_bounds{Domain(0, 100), Domain(0, 0), Domain(3, 7)};

  // Call the postsolving.
  SolverLogger logger;
  CpSolverResponse response;
  FillTightenedDomainInResponse(original_model, mapping_proto,
                                postsolve_mapping, search_bounds, &response,
                                &logger);

  // Lets test by constructing a model for easy comparison.
  CpModelProto returned_model;
  for (const IntegerVariableProto& var : response.tightened_variables()) {
    *returned_model.add_variables() = var;
  }

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 0, 100 ]
    }  # presolve reduced the domain.
    variables { domain: [ 0, 1 ] }  # no info.
    variables { domain: [ 0, 0 ] }  # was fixed by search.
  )pb");
  EXPECT_THAT(returned_model, testing::EqualsProto(expected_model));
}

TEST(FillTightenedDomainInResponseTest, WithAffine) {
  // Original model.
  const CpModelProto original_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 124 ] }
    variables { domain: [ 0, 50 ] }
    variables { domain: [ 0, 255 ] }
  )pb");

  // We might have more variable there.
  // Also the domains might be tighter.
  const CpModelProto mapping_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 50 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 17 ] }
    variables { domain: [ 0, 18 ] }
    variables { domain: [ 0, 19 ] }
    constraints {
      linear {
        vars: [ 0, 3 ]
        coeffs: [ 2, 1 ]
        domain: [ 10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 4 ]
        coeffs: [ 1, 1 ]
        domain: [ 10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 5, 2 ]
        coeffs: [ 2, 1 ]
        domain: [ 10, 10 ]
      }
    }
  )pb");

  std::vector<int> postsolve_mapping{3, 4, 5};
  std::vector<Domain> search_bounds{Domain(0, 20), Domain(0, 20), Domain(3, 5)};

  // Call the postsolving.
  SolverLogger logger;
  logger.EnableLogging(true);
  CpSolverResponse response;
  FillTightenedDomainInResponse(original_model, mapping_proto,
                                postsolve_mapping, search_bounds, &response,
                                &logger);

  // Lets test by constructing a model for easy comparison.
  CpModelProto returned_model;
  for (const IntegerVariableProto& var : response.tightened_variables()) {
    *returned_model.add_variables() = var;
  }

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }   # 2 * v = 10 - [0, 17]
    variables { domain: [ 0, 10 ] }  # v = 10 - [0, 18]
    variables { domain: [ 0, 4 ] }   # v = 10 - 2 * [3, 5]
  )pb");
  EXPECT_THAT(returned_model, testing::EqualsProto(expected_model));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
