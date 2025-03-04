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

#include "ortools/sat/cp_model_symmetries.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

const char kBaseModel[] = R"pb(
  variables {
    name: 'x'
    domain: [ -5, 5 ]
  }
  variables {
    name: 'y'
    domain: [ 0, 10 ]
  }
  variables {
    name: 'z'
    domain: [ 0, 10 ]
  }
  constraints {
    linear {
      domain: [ 0, 10 ]
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 2, 2 ]
    }
  }
  constraints {
    linear {
      domain: [ 2, 12 ]
      vars: [ 0, 1, 2 ]
      coeffs: [ 3, 2, 2 ]
    }
  }
)pb";

TEST(FindCpModelSymmetries, FindsSymmetry) {
  const CpModelProto model = ParseTestProto(kBaseModel);

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  SolverLogger logger;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries, NoSymmetryIfDifferentVariableBounds) {
  CpModelProto model = ParseTestProto(kBaseModel);
  model.mutable_variables(1)->set_domain(1, 20);

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  SolverLogger logger;

  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

TEST(FindCpModelSymmetries, NoSymmetryIfDifferentConstraintCoefficients) {
  CpModelProto model = ParseTestProto(kBaseModel);
  model.mutable_constraints(1)->mutable_linear()->set_coeffs(1, 1);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

TEST(FindCpModelSymmetries, NoSymmetryIfDifferentObjectiveCoefficients) {
  CpModelProto model = ParseTestProto(kBaseModel);
  model.mutable_objective()->add_vars(1);
  model.mutable_objective()->add_coeffs(1);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

const char kConstraintSymmetryModel[] = R"pb(
  variables {
    name: 'x'
    domain: [ -5, 5 ]
  }
  variables {
    name: 'y'
    domain: [ 0, 10 ]
  }
  variables {
    name: 'z'
    domain: [ 0, 10 ]
  }
  constraints {
    linear {
      domain: [ 0, 10 ]
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 2, 3 ]
    }
  }
  constraints {
    linear {
      domain: [ 0, 10 ]
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 3, 2 ]
    }
  }
)pb";

TEST(FindCpModelSymmetries, FindsSymmetryIfSameConstraintBounds) {
  CpModelProto model = ParseTestProto(kConstraintSymmetryModel);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");

  // Make sure that if the constraint bounds are different, the symmetry is
  // broken.
  model.mutable_constraints(1)->mutable_linear()->set_domain(1, 20);
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

TEST(FindCpModelSymmetries,
     NoSymmetryIfDifferentConstraintEnforcementLiterals) {
  CpModelProto model = ParseTestProto(kConstraintSymmetryModel);
  model.mutable_constraints(0)->add_enforcement_literal(0);
  model.mutable_constraints(1)->add_enforcement_literal(1);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

TEST(FindCpModelSymmetries, FindsSymmetryIfSameConstraintEnforcementLiterals) {
  CpModelProto model = ParseTestProto(kConstraintSymmetryModel);
  model.mutable_constraints(0)->add_enforcement_literal(0);
  model.mutable_constraints(1)->add_enforcement_literal(0);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries,
     FindsSymmetryIfSameNegativeConstraintEnforcementLiterals) {
  CpModelProto model = ParseTestProto(kConstraintSymmetryModel);
  model.mutable_constraints(0)->add_enforcement_literal(-1);
  model.mutable_constraints(1)->add_enforcement_literal(-1);
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries, LinMaxConstraint) {
  CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1, coeffs: 2 }
        exprs { vars: 2, coeffs: 2 }
        exprs { vars: 3, coeffs: 3 }
      }
    }
  )pb");
  SolverLogger logger;

  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries, UnsupportedConstraintTypeReturnsNoGenerators) {
  CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ -5, 5 ]
    }
    variables {
      name: 'y'
      domain: [ 0, 10 ]
    }
    variables {
      name: 'z'
      domain: [ 0, 10 ]
    }
    constraints { routes {} }
  )pb");

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

// We ignore variables that do not appear in any constraint.
TEST(FindCpModelSymmetries, FindsSymmetryIfNoConstraints) {
  CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 0, 10 ]
    }
    variables {
      name: 'y'
      domain: [ -5, 5 ]
    }
    variables {
      name: 'z'
      domain: [ 0, 10 ]
    }
  )pb");

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);
  ASSERT_EQ(generators.size(), 0);
}

TEST(FindCpModelSymmetries, NoSymmetryIfDuplicateConstraints) {
  CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ -5, 5 ]
    }
    variables {
      name: 'y'
      domain: [ 0, 10 ]
    }
    variables {
      name: 'z'
      domain: [ -5, 10 ]
    }
    constraints {
      linear {
        domain: [ 0, 10 ]
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 3 ]
      }
    }
    constraints {
      linear {
        domain: [ 0, 10 ]
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 3 ]
      }
    }
  )pb");

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 0);
}

// a => not(b)
// not(a) => c >= 4
// not(b) => d >= 4
TEST(FindCpModelSymmetries, ImplicationTestThatUsedToFail) {
  CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ -2 ] }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 4, 10 ]
      }
    }
    constraints {
      enforcement_literal: -2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 4, 10 ]
      }
    }
  )pb");

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1) (2 3)");
}

TEST(DetectAndAddSymmetryToProto, BasicTest) {
  // A model with one (0, 1) (2, 3) symmetry.
  CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ -2 ] }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 4, 10 ]
      }
    }
    constraints {
      enforcement_literal: -2
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 4, 10 ]
      }
    }
  )pb");

  SolverLogger logger;
  SatParameters params;
  params.set_log_search_progress(true);
  TimeLimit time_limit;
  DetectAndAddSymmetryToProto(params, &model, &logger, &time_limit);

  // TODO(user): canonicalize the order in each cycle?
  const SymmetryProto expected = ParseTestProto(R"pb(
    permutations {
      support: [ 1, 0, 3, 2 ]
      cycle_sizes: [ 2, 2 ]
    }
  )pb");

  EXPECT_THAT(model.symmetry(), testing::EqualsProto(expected));
}

const char kBooleanModel[] = R"pb(
  variables {
    name: 'x'
    domain: [ 0, 1 ]
  }
  variables {
    name: 'y'
    domain: [ 0, 1 ]
  }
  variables {
    name: 'z'
    domain: [ 0, 1 ]
  }
)pb";

TEST(FindCpModelSymmetries, FindsSymmetryInBoolOr) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints { bool_or { literals: [ 0, 1 ] } }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInNegatedBoolOr) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints { bool_or { literals: [ -1, -3 ] } }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInBoolOrWithEnforcementLiteral) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints {
      enforcement_literal: 1
      bool_or { literals: [ 0, 2 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInBoolXor) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints { bool_xor { literals: [ 0, 2 ] } }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInNegatedBoolXor) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints { bool_or { literals: [ -2, -3 ] } }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInBoolXorWithEnforcementLiteral) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints {
      enforcement_literal: 0
      bool_or { literals: [ 1, 2 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(1 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInBoolAnd) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints {
      enforcement_literal: 1
      bool_and { literals: [ 0, 2 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInNegatedBoolAnd) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints {
      enforcement_literal: 2
      bool_and { literals: [ -1, -2 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1)");
}

TEST(FindCpModelSymmetries, FindsSymmetryInBoolAndWithEnforcementLiteral) {
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints {
      enforcement_literal: 2
      bool_and { literals: [ 0, 1 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1)");
}

TEST(FindCpModelSymmetries,
     FindsSymmetryInBoolOrsAndBoolAndWithEnforcementLiteral) {
  // The two BoolOrs and the BoolAnd are equivalent, so this should find a
  // symmetry between literals 0 and 1.
  CpModelProto model = ParseTestProto(absl::StrCat(kBooleanModel, R"pb(
    constraints { bool_or { literals: [ 2, 1 ] } }
    constraints { bool_or { literals: [ 0, 2 ] } }
    constraints {
      enforcement_literal: -3
      bool_and { literals: [ 0, 1 ] }
    }
  )pb"));

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 1)");
}

TEST(FindCpModelSymmetries, BasicSchedulingCase) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }  # start 1
    variables { domain: [ 0, 10 ] }  # start 2
    variables { domain: [ 0, 10 ] }  # start 3
    constraints {
      interval {
        start { vars: 0 coeffs: 1 offset: 0 }
        size { offset: 4 }
        end { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 offset: 0 }
        size { offset: 5 }
        end { vars: 1 coeffs: 1 offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 offset: 0 }
        size { offset: 4 }
        end { vars: 2 coeffs: 1 offset: 4 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  SolverLogger logger;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  TimeLimit time_limit;
  FindCpModelSymmetries({}, model, &generators, &logger, &time_limit);

  // The two intervals with the same size can be swapped.
  ASSERT_EQ(generators.size(), 1);
  EXPECT_EQ(generators[0]->DebugString(), "(0 2)");
}

// Assigning n items to b identical bins is an example of orbitope since the
// bins can be freely permuted.
TEST(FindCpModelSymmetries, BinPacking) {
  constexpr int num_items = 10;
  constexpr int num_bins = 7;
  CpModelProto proto;

  // One Boolean per possible assignment.
  int item_to_bin[num_items][num_bins];
  for (int i = 0; i < num_items; ++i) {
    for (int b = 0; b < num_bins; ++b) {
      item_to_bin[i][b] = proto.variables().size();
      auto* var = proto.add_variables();
      var->add_domain(0);
      var->add_domain(1);
    }
  }

  // At most one per row.
  for (int i = 0; i < num_items; ++i) {
    auto* amo = proto.add_constraints()->mutable_at_most_one();
    for (int b = 0; b < num_bins; ++b) {
      amo->add_literals(item_to_bin[i][b]);
    }
  }

  // Simple capacity constraint.
  for (int b = 0; b < num_bins; ++b) {
    auto* linear = proto.add_constraints()->mutable_linear();
    for (int i = 0; i < num_items; ++i) {
      linear->add_vars(item_to_bin[i][b]);
      linear->add_coeffs(i + 1);
    }
    linear->add_domain(0);
    linear->add_domain(10);  // <= 10.
  }

  Model model;
  model.GetOrCreate<SolverLogger>()->EnableLogging(true);
  model.GetOrCreate<SolverLogger>()->SetLogToStdOut(true);
  PresolveContext context(&model, &proto, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  context.ReadObjectiveFromProto();
  EXPECT_TRUE(DetectAndExploitSymmetriesInPresolve(&context));
  context.LogInfo();

  // We have a 10 x 7 orbitope.
  // Note that here we do not do propagation, just fixing to zero according
  // to the orbitope and the at most ones. We should fix 6 on the first row,
  // and one less per row after that.
  for (int i = 0; i < num_items; ++i) {
    int num_fixed = 0;
    for (int b = 0; b < num_bins; ++b) {
      if (context.IsFixed(item_to_bin[i][b])) ++num_fixed;
    }
    CHECK_EQ(num_fixed, std::max(0, 6 - i)) << i;
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
