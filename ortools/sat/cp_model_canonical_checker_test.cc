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

#include "ortools/sat/cp_model_canonical_checker.h"

#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_status.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(CpModelCanonicalCheckerTest, CheckSortedAndUnsortedPermutedModel) {
  const CpModelProto permuted_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 2, -1 ]
      linear {
        vars: [ 4, 3 ]
        coeffs: [ 1, -2 ]
        domain: [ -10, 5 ]
      }
    }
    constraints { bool_or { literals: [ 2, -1, 1 ] } }
    constraints { bool_xor { literals: [ -3, 0, 1 ] } }
    constraints {
      lin_max {
        target {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 4, 3 ]
          coeffs: [ 1, 1 ]
          offset: 2
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
          offset: 1
        }
      }
    }
  )pb");

  const std::vector<Domain> domains = {Domain(0, 1), Domain(0, 1), Domain(0, 1),
                                       Domain(0, 10), Domain(0, 10)};
  CanonicalizationChecker checker(&domains);
  for (const ConstraintProto& ct : permuted_model.constraints()) {
    EXPECT_FALSE(checker.ConstraintIsCanonical(ct, /*check_sorted=*/true));
    EXPECT_TRUE(checker.ConstraintIsCanonical(ct, /*check_sorted=*/false));
  }
  EXPECT_FALSE(ModelProtoIsCanonical(permuted_model, /*lrat_mode=*/false,
                                     /*check_sorted=*/true));
  EXPECT_TRUE(ModelProtoIsCanonical(permuted_model, /*lrat_mode=*/false,
                                    /*check_sorted=*/false));
  EXPECT_TRUE(ModelProtoIsCanonical(permuted_model));

  // Duplicate or complementary variables are still rejected when
  // check_sorted = false.
  CpModelProto dup_bool_or = permuted_model;
  dup_bool_or.mutable_constraints(1)->mutable_bool_or()->add_literals(-3);
  EXPECT_FALSE(ModelProtoIsCanonical(dup_bool_or));

  CpModelProto dup_linear = permuted_model;
  dup_linear.mutable_constraints(0)->mutable_linear()->add_vars(4);
  dup_linear.mutable_constraints(0)->mutable_linear()->add_coeffs(1);
  EXPECT_FALSE(ModelProtoIsCanonical(dup_linear));
}

TEST(CpModelCanonicalCheckerTest, LinearGcdAndLeadingSignChecks) {
  const std::vector<Domain> domains = {Domain(0, 10), Domain(0, 10)};
  CanonicalizationChecker checker(&domains);

  // Non-unit GCD -> non-canonical.
  const ConstraintProto non_coprime = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1 ]
      coeffs: [ 2, 4 ]
      domain: [ 0, 10 ]
    }
  )pb");
  EXPECT_FALSE(checker.ConstraintIsCanonical(non_coprime));

  // Negative leading coefficient -> non-canonical.
  const ConstraintProto neg_leading = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1 ]
      coeffs: [ -1, 1 ]
      domain: [ 0, 5 ]
    }
  )pb");
  EXPECT_FALSE(checker.ConstraintIsCanonical(neg_leading));
}

TEST(CpModelCanonicalCheckerTest, OptionalIntervalStubAndRoutingLiterals) {
  // Variables:
  // 0: fixed false [0, 0]
  // 1: fixed true [1, 1]
  // 2: unfixed bool [0, 1] (representative)
  // 3: unfixed bool [0, 1] (non-representative)
  std::vector<Domain> domains = {Domain(0), Domain(1), Domain(0, 1),
                                 Domain(0, 1)};
  CanonicalizationChecker::Options options;
  options.var_is_representative = [](int var) { return var != 3; };
  CanonicalizationChecker checker(&domains, std::move(options));

  // Unperformed interval stub enforced by fixed-false literal 0 is canonical
  // mid-presolve (with `var_is_representative`), and rejected post-presolve.
  const ConstraintProto unperformed_interval_stub = ParseTestProto(R"pb(
    enforcement_literal: [ 0 ]
    interval {}
  )pb");
  EXPECT_TRUE(checker.ConstraintIsCanonical(unperformed_interval_stub));
  EXPECT_FALSE(CanonicalizationChecker(&domains).ConstraintIsCanonical(
      unperformed_interval_stub));
  // Enforced by fixed-true literal 1 (or non-empty body) is non-canonical.
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 1 ]
    interval {}
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 0 ]
    interval { start { offset: 0 } }
  )pb")));

  // Fixed-true literals are allowed in kCircuit/kRoutes/kReservoir, while
  // fixed-false literals, empty graphs/reservoirs, non-representative literals,
  // or enforcement variables (positive or negated) are rejected.
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 2 ]
    circuit {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ -1, 1 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    circuit {}
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    circuit {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ 0, 1 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    circuit {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ 2, -4 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 2 ]
    circuit {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ 1, 2 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    routes {}
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    routes {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ 0, 1 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 2 ]
    routes {
      tails: [ 0, 1 ]
      heads: [ 1, 0 ]
      literals: [ 1, -3 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    reservoir { min_level: 0 max_level: 10 }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    reservoir {
      min_level: 0
      max_level: 10
      time_exprs { offset: 0 }
      level_changes { offset: 1 }
      active_literals: [ 0 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ -3 ]
    reservoir {
      min_level: 0
      max_level: 10
      time_exprs { offset: 0 }
      level_changes { offset: 1 }
      active_literals: [ 2 ]
    }
  )pb")));

  // Growing `domains` dynamically remains visible through `&domains`.
  domains.push_back(Domain(0, 5));
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    enforcement_literal: [ 2 ]
    linear {
      vars: [ 4 ]
      coeffs: [ 1 ]
      domain: [ 1, 3 ]
    }
  )pb")));
}

TEST(CpModelCanonicalCheckerTest, RejectsLegacyPreCopyVariableFields) {
  const std::vector<Domain> domains = {Domain(0, 5), Domain(0, 5)};
  CanonicalizationChecker checker(&domains);

  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    element {
      index: 0
      target: 1
      vars: [ 0, 1 ]
    }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    element {}
  )pb")));
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    element {
      linear_index {
        vars: [ 0 ]
        coeffs: [ 1 ]
      }
      linear_target {
        vars: [ 1 ]
        coeffs: [ 1 ]
      }
      exprs { offset: 2 }
      exprs { offset: 4 }
    }
  )pb")));

  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    table {
      vars: [ 0, 1 ]
      values: [ 1, 2 ]
    }
  )pb")));
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    table {
      exprs {
        vars: [ 0 ]
        coeffs: [ 1 ]
      }
      exprs {
        vars: [ 1 ]
        coeffs: [ 1 ]
      }
      values: [ 1, 2 ]
    }
  )pb")));

  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    inverse { f_direct: [ 0 ] }
  )pb")));
  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    inverse { f_inverse: [ 0 ] }
  )pb")));
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    inverse {
      f_expr_direct {
        vars: [ 0 ]
        coeffs: [ 1 ]
      }
      f_expr_inverse {
        vars: [ 1 ]
        coeffs: [ 1 ]
      }
    }
  )pb")));

  EXPECT_FALSE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    automaton {
      starting_state: 0
      final_states: [ 1 ]
      transition_tail: [ 0 ]
      transition_head: [ 1 ]
      transition_label: [ 1 ]
      vars: [ 0 ]
    }
  )pb")));
  EXPECT_TRUE(checker.ConstraintIsCanonical(ParseTestProto(R"pb(
    automaton {
      starting_state: 0
      final_states: [ 1 ]
      transition_tail: [ 0 ]
      transition_head: [ 1 ]
      transition_label: [ 1 ]
      exprs {
        vars: [ 0 ]
        coeffs: [ 1 ]
      }
    }
  )pb")));
}

TEST(CpModelCanonicalCheckerTest, EmptyDomainAndLratMode) {
  const CpModelProto unsat_model = ParseTestProto(R"pb(
    variables { domain: [] }
    constraints { bool_or { literals: [ 0 ] } }
  )pb");
  EXPECT_TRUE(ModelProtoIsCanonical(unsat_model));

  const CpModelProto lrat_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 1, 0, 1 ] } }
    constraints { bool_and { literals: [ 1, 0 ] } }
  )pb");
  EXPECT_FALSE(ModelProtoIsCanonical(lrat_model, /*lrat_mode=*/false));
  EXPECT_TRUE(ModelProtoIsCanonical(lrat_model, /*lrat_mode=*/true));
}

TEST(CpModelCanonicalCheckerTest, CanonicalizationStatusFromModification) {
  EXPECT_EQ(CanonicalizationStatus(kUnchanged), kCanonical);
  EXPECT_EQ(CanonicalizationStatus(kModified), kNeedsCanonicalization);

  CanonicalizationStatus status = kCanonical;
  status |= kUnchanged;
  EXPECT_EQ(status, kCanonical);
  status |= kModified;
  EXPECT_EQ(status, kNeedsCanonicalization);

  CanonicalizationStatus unsat = kUnsat;
  unsat |= kModified;
  EXPECT_EQ(unsat, kUnsat);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
