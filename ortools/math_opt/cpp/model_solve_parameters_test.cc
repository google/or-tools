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

#include "ortools/math_opt/cpp/model_solve_parameters.h"

#include <array>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/solution.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "util/tuple/struct.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// Define the value of MapFilter.storage() we want in the test for a filter.
//
// In below tests, we cover all possible combinations of filters with same or
// different value for storage() (either for death testing of regular testing).
// To have total coverage, we only need to test with three models max since we
// have three filters.
//
// The cases to test are the one in the set {null, 1, 2, 3}^3. The cases with
// different non null models are the one expected to crash, the others (using
// one or more times the same model) are expected to pass.
enum FilterModel {
  // The filter has no model (no referencing any variable or constraint).
  kNullModel,
  // The filter points the first test model.
  kModel1,
  // The filter points the second test model.
  kModel2,
  // The filter points the third test model.
  kModel3,
};

const std::array<FilterModel, 4> kAllFilterModels = {kNullModel, kModel1,
                                                     kModel2, kModel3};

// A combination of filter models to test, one model for each filter in
// ModelSolveParameters.
struct FilterModelsCombination {
  TUPLE_DEFINE_STRUCT(FilterModelsCombination, (ostream, ctor),
                      (FilterModel, variable_values_filter_model),
                      (FilterModel, dual_values_filter_model),
                      (FilterModel, reduced_costs_filter_model));

  // Returns a non nullopt value if and only if all filters have either the same
  // model or a null model. If all filter have a null model, kNullModel is
  // returned.
  std::optional<FilterModel> common_storage() const;

  // Returns true if the combination is expected to pass (i.e. it has a common
  // model), false if it is expected to die (i.e. it references at least two
  // different models).
  bool ok() const;
};

std::optional<FilterModel> FilterModelsCombination::common_storage() const {
  const std::vector<FilterModel> models = {variable_values_filter_model,
                                           dual_values_filter_model,
                                           reduced_costs_filter_model};
  FilterModel ret = kNullModel;
  for (const auto model : models) {
    if (model != kNullModel) {
      if (ret == kNullModel) {
        ret = model;
      } else if (model != ret) {
        return std::nullopt;
      }
    }
  }
  return ret;
}

bool FilterModelsCombination::ok() const {
  return common_storage().has_value();
}

// Returns all possible model combinations.
std::vector<FilterModelsCombination> AllCombinations() {
  std::vector<FilterModelsCombination> ret;
  for (const FilterModel primal_vars_model : kAllFilterModels) {
    for (const FilterModel dual_cstrs_model : kAllFilterModels) {
      for (const FilterModel dual_vars_model : kAllFilterModels) {
        ret.emplace_back(primal_vars_model, dual_cstrs_model, dual_vars_model);
      }
    }
  }
  return ret;
}

// Test fixture for testing filter models combinations.
class FilterModelsCombinationTest : public ::testing::Test {
 public:
  FilterModelsCombinationTest()
      : a_1_(&model_1_, model_1_.AddVariable("a_1")),
        cstr_1_(&model_1_, model_1_.AddLinearConstraint("cstr_1")),
        a_2_(&model_2_, model_2_.AddVariable("a_2")),
        cstr_2_(&model_2_, model_2_.AddLinearConstraint("cstr_2")),
        a_3_(&model_3_, model_3_.AddVariable("a_3")),
        cstr_3_(&model_3_, model_3_.AddLinearConstraint("cstr_3")) {}

 protected:
  // Returns a filter for the given model. The input model_to_key is expected to
  // contain a value for each FilterModel but kNullModel.
  template <typename K>
  MapFilter<K> MakeMapFilter(
      const FilterModel model,
      const absl::flat_hash_map<FilterModel, K>& model_to_key) {
    MapFilter<K> ret;

    if (model == kNullModel) {
      return ret;
    }

    ret.filtered_keys = {gtl::FindOrDie(model_to_key, model)};
    return ret;
  }

  ModelSolveParameters MakeParameters(
      const FilterModelsCombination& combination) {
    const absl::flat_hash_map<FilterModel, Variable> vars = {
        {kModel1, a_1_},
        {kModel2, a_2_},
        {kModel3, a_3_},
    };
    const absl::flat_hash_map<FilterModel, LinearConstraint> cstrs = {
        {kModel1, cstr_1_},
        {kModel2, cstr_2_},
        {kModel3, cstr_3_},
    };
    ModelSolveParameters params;
    params.variable_values_filter =
        MakeMapFilter(combination.variable_values_filter_model, vars);
    params.dual_values_filter =
        MakeMapFilter(combination.dual_values_filter_model, cstrs);
    params.reduced_costs_filter =
        MakeMapFilter(combination.reduced_costs_filter_model, vars);
    return params;
  }

  ModelStorage model_1_;
  const Variable a_1_;
  const LinearConstraint cstr_1_;
  ModelStorage model_2_;
  const Variable a_2_;
  const LinearConstraint cstr_2_;
  ModelStorage model_3_;
  const Variable a_3_;
  const LinearConstraint cstr_3_;
};

using FilterModelsCombinationDeathTest = FilterModelsCombinationTest;

TEST(ModelSolveParametersTest, Default) {
  ModelStorage model;
  const ModelSolveParameters params;
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto("")));
  EXPECT_OK(params.CheckModelStorage(&model));
}

TEST(ModelSolveParametersTest, SetFiltersSameModel) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));
  const Variable b(&model, model.AddVariable("b"));
  const LinearConstraint cstr(&model, model.AddLinearConstraint("cstr"));

  ModelSolveParameters params;
  params.variable_values_filter = MakeKeepKeysFilter({a});
  params.dual_values_filter = MakeKeepKeysFilter({cstr});
  params.reduced_costs_filter = MakeKeepKeysFilter({b});

  ModelSolveParametersProto expected;
  expected.mutable_variable_values_filter()->set_filter_by_ids(true);
  expected.mutable_variable_values_filter()->add_filtered_ids(a.id());
  expected.mutable_dual_values_filter()->set_filter_by_ids(true);
  expected.mutable_dual_values_filter()->add_filtered_ids(cstr.id());
  expected.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  expected.mutable_reduced_costs_filter()->add_filtered_ids(b.id());
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
  EXPECT_OK(params.CheckModelStorage(&model));
}

TEST_F(FilterModelsCombinationTest, ValidCombinations) {
  const absl::flat_hash_map<FilterModel, NullableModelStorageCPtr> models = {
      {kNullModel, nullptr},
      {kModel1, &model_1_},
      {kModel2, &model_2_},
      {kModel3, &model_3_},
  };
  for (const FilterModelsCombination& combination : AllCombinations()) {
    if (!combination.ok()) {
      continue;
    }

    SCOPED_TRACE(combination);
    const ModelSolveParameters params = MakeParameters(combination);

    const std::optional<FilterModel> model = combination.common_storage();
    ASSERT_TRUE(model.has_value());
    const NullableModelStorageCPtr expected_model =
        gtl::FindOrDie(models, *model);
    if (expected_model != nullptr) {
      EXPECT_OK(params.CheckModelStorage(expected_model));
    } else {
      EXPECT_OK(params.CheckModelStorage(&model_1_));
      EXPECT_OK(params.CheckModelStorage(&model_2_));
      EXPECT_OK(params.CheckModelStorage(&model_3_));
    }
  }
}

TEST_F(FilterModelsCombinationTest, InvalidCombinations) {
  for (const FilterModelsCombination& combination : AllCombinations()) {
    if (combination.ok()) {
      continue;
    }

    SCOPED_TRACE(combination);
    const ModelSolveParameters params = MakeParameters(combination);
    EXPECT_THAT(params.CheckModelStorage(&model_1_),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(internal::kInputFromInvalidModelStorage)));
    EXPECT_THAT(params.CheckModelStorage(&model_2_),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(internal::kInputFromInvalidModelStorage)));
    EXPECT_THAT(params.CheckModelStorage(&model_3_),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(internal::kInputFromInvalidModelStorage)));
  }
}

TEST(ModelSolveParametersTest, OnlyPrimalVariables) {
  ModelStorage model;
  const ModelSolveParameters params =
      ModelSolveParameters::OnlyPrimalVariables();

  EXPECT_THAT(params.Proto(),
              IsOkAndHolds(EquivToProto(
                  R"pb(dual_values_filter { filter_by_ids: true }
                       quadratic_dual_values_filter { filter_by_ids: true }
                       reduced_costs_filter { filter_by_ids: true })pb")));
  EXPECT_OK(params.CheckModelStorage(&model));
}

TEST(ModelSolveParametersTest, OnlySomePrimalVariablesInitializerList) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));

  const ModelSolveParameters params =
      ModelSolveParameters::OnlySomePrimalVariables({a});

  ModelSolveParametersProto expected;
  expected.mutable_variable_values_filter()->set_filter_by_ids(true);
  expected.mutable_variable_values_filter()->add_filtered_ids(a.id());
  expected.mutable_dual_values_filter()->set_filter_by_ids(true);
  expected.mutable_quadratic_dual_values_filter()->set_filter_by_ids(true);
  expected.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
  EXPECT_OK(params.CheckModelStorage(&model));
}

TEST(ModelSolveParametersTest, OnlySomePrimalVariablesVector) {
  ModelStorage model;
  const Variable a(&model, model.AddVariable("a"));

  const std::vector<Variable> vars = {a};
  const ModelSolveParameters params =
      ModelSolveParameters::OnlySomePrimalVariables(vars);

  ModelSolveParametersProto expected;
  expected.mutable_variable_values_filter()->set_filter_by_ids(true);
  expected.mutable_variable_values_filter()->add_filtered_ids(a.id());
  expected.mutable_dual_values_filter()->set_filter_by_ids(true);
  expected.mutable_quadratic_dual_values_filter()->set_filter_by_ids(true);
  expected.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
  EXPECT_OK(params.CheckModelStorage(&model));
}

TEST(ModelSolveParametersTest, BasisStart) {
  ModelStorage model;
  const Variable x1(&model, model.AddVariable("x1"));
  const Variable x2(&model, model.AddVariable("x2"));
  const LinearConstraint c1(&model, model.AddLinearConstraint("c1"));
  const LinearConstraint c2(&model, model.AddLinearConstraint("c2"));
  ModelSolveParameters params;
  auto& initial_basis = params.initial_basis.emplace();
  initial_basis.variable_status.insert(
      {{x1, BasisStatus::kAtUpperBound}, {x2, BasisStatus::kBasic}});
  initial_basis.constraint_status.insert(
      {{c1, BasisStatus::kAtLowerBound}, {c2, BasisStatus::kBasic}});

  EXPECT_OK(params.CheckModelStorage(&model));

  ModelSolveParametersProto expected;
  expected.mutable_initial_basis()->mutable_constraint_status()->add_ids(
      c1.id());
  expected.mutable_initial_basis()->mutable_constraint_status()->add_ids(
      c2.id());
  expected.mutable_initial_basis()->mutable_constraint_status()->add_values(
      BASIS_STATUS_AT_LOWER_BOUND);
  expected.mutable_initial_basis()->mutable_constraint_status()->add_values(
      BASIS_STATUS_BASIC);
  expected.mutable_initial_basis()->mutable_variable_status()->add_ids(x1.id());
  expected.mutable_initial_basis()->mutable_variable_status()->add_ids(x2.id());
  expected.mutable_initial_basis()->mutable_variable_status()->add_values(
      BASIS_STATUS_AT_UPPER_BOUND);
  expected.mutable_initial_basis()->mutable_variable_status()->add_values(
      BASIS_STATUS_BASIC);
  expected.mutable_initial_basis()->set_basic_dual_feasibility(
      SolutionStatusProto::SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
}

TEST(ModelSolveParametersTest, FilterAndBasisDifferentModels) {
  ModelStorage model_a;
  const Variable a_x(&model_a, model_a.AddVariable("x"));
  ModelStorage model_b;
  const Variable b_x(&model_b, model_b.AddVariable("x"));

  const ModelSolveParameters params = {
      .variable_values_filter = {.filtered_keys = {{a_x}}},
      .initial_basis = Basis{.variable_status = {{b_x, BasisStatus::kFree}}},
  };

  EXPECT_THAT(params.CheckModelStorage(&model_a),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
  EXPECT_THAT(params.CheckModelStorage(&model_b),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(ModelSolveParametersTest, SolutionHint) {
  ModelStorage model;
  const Variable x1(&model, model.AddVariable("x1"));
  const Variable x2(&model, model.AddVariable("x2"));
  const Variable x3(&model, model.AddVariable("x3"));
  const LinearConstraint c1(&model, model.AddLinearConstraint("c1"));
  ModelSolveParameters params;

  ModelSolveParameters::SolutionHint first_hint;
  first_hint.variable_values.emplace(x1, 1.0);
  first_hint.variable_values.emplace(x3, 0.0);
  first_hint.dual_values.emplace(c1, 5.25);
  params.solution_hints.emplace_back(first_hint);
  ModelSolveParameters::SolutionHint second_hint;
  second_hint.variable_values.emplace(x1, 1.0);
  second_hint.variable_values.emplace(x2, 0.0);
  params.solution_hints.emplace_back(second_hint);

  EXPECT_OK(params.CheckModelStorage(&model));
  EXPECT_OK(first_hint.CheckModelStorage(&model));
  EXPECT_OK(second_hint.CheckModelStorage(&model));

  ModelSolveParametersProto expected;
  SolutionHintProto& first_expected_hint = *expected.add_solution_hints();
  first_expected_hint.mutable_variable_values()->add_ids(x1.id());
  first_expected_hint.mutable_variable_values()->add_values(1.0);
  first_expected_hint.mutable_variable_values()->add_ids(x3.id());
  first_expected_hint.mutable_variable_values()->add_values(0.0);
  first_expected_hint.mutable_dual_values()->add_ids(c1.id());
  first_expected_hint.mutable_dual_values()->add_values(5.25);
  SolutionHintProto& second_expected_hint = *expected.add_solution_hints();
  second_expected_hint.mutable_variable_values()->add_ids(x1.id());
  second_expected_hint.mutable_variable_values()->add_values(1.0);
  second_expected_hint.mutable_variable_values()->add_ids(x2.id());
  second_expected_hint.mutable_variable_values()->add_values(0.0);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
  EXPECT_THAT(first_hint.Proto(), EquivToProto(first_expected_hint));
  EXPECT_THAT(second_hint.Proto(), EquivToProto(second_expected_hint));
}

TEST(ModelSolveParametersTest, FilterAndHintDifferentModels) {
  ModelStorage model_a;
  const Variable a_x(&model_a, model_a.AddVariable("x"));
  ModelStorage model_b;
  const Variable b_x(&model_b, model_b.AddVariable("x"));

  const ModelSolveParameters params = {
      .variable_values_filter = {.filtered_keys = {{a_x}}},
      .solution_hints = {{.variable_values = {{b_x, 1.0}}}},
  };

  EXPECT_THAT(params.CheckModelStorage(&model_a),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
  EXPECT_THAT(params.CheckModelStorage(&model_b),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(ModelSolveParametersTest, BranchingPriority) {
  ModelStorage model;
  const Variable x1(&model, model.AddVariable("x1"));
  const Variable x2(&model, model.AddVariable("x2"));
  const Variable x3(&model, model.AddVariable("x3"));
  ModelSolveParameters params;

  params.branching_priorities.emplace(x1, 2);
  params.branching_priorities.emplace(x3, 1);

  ModelSolveParametersProto expected;
  expected.mutable_branching_priorities()->add_ids(x1.id());
  expected.mutable_branching_priorities()->add_values(2);
  expected.mutable_branching_priorities()->add_ids(x3.id());
  expected.mutable_branching_priorities()->add_values(1);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
}

TEST(ModelSolveParametersTest, BranchingPriorityOtherModel) {
  Model model;
  Model other;
  const Variable x = other.AddVariable("x");
  const ModelSolveParameters params{.branching_priorities = {{x, 2}}};
  EXPECT_THAT(params.CheckModelStorage(model.storage()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(ModelSolveParametersTest, ObjectiveParameters) {
  ModelStorage model;
  const auto primary = Objective::Primary(&model);
  const auto secondary = Objective::Auxiliary(&model, AuxiliaryObjectiveId(2));
  ModelSolveParameters params;
  params.objective_parameters[primary]
      .objective_degradation_absolute_tolerance = 3.0;
  params.objective_parameters[primary]
      .objective_degradation_relative_tolerance = 4.0;
  params.objective_parameters[primary].time_limit = absl::Seconds(10);
  params.objective_parameters[secondary]
      .objective_degradation_absolute_tolerance = 5.0;
  params.objective_parameters[secondary]
      .objective_degradation_relative_tolerance = 6.0;
  params.objective_parameters[secondary].time_limit = absl::Seconds(20);

  ModelSolveParametersProto expected;
  expected.mutable_primary_objective_parameters()
      ->set_objective_degradation_absolute_tolerance(3.0);
  expected.mutable_primary_objective_parameters()
      ->set_objective_degradation_relative_tolerance(4.0);
  expected.mutable_primary_objective_parameters()
      ->mutable_time_limit()
      ->set_seconds(10);
  (*expected.mutable_auxiliary_objective_parameters())[2]
      .set_objective_degradation_absolute_tolerance(5.0);
  (*expected.mutable_auxiliary_objective_parameters())[2]
      .set_objective_degradation_relative_tolerance(6.0);
  expected.mutable_auxiliary_objective_parameters()
      ->at(2)
      .mutable_time_limit()
      ->set_seconds(20);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
}

TEST(ModelSolveParametersTest, ObjectiveParametersOtherModel) {
  Model model;
  Model other;
  const auto o = other.primary_objective();
  const ModelSolveParameters params{.objective_parameters = {{o, {}}}};
  EXPECT_THAT(params.CheckModelStorage(model.storage()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(ModelSolveParametersTest, LazyLinearConstraints) {
  Model model;
  const LinearConstraint c = model.AddLinearConstraint("c");
  model.AddLinearConstraint("d");
  const LinearConstraint e = model.AddLinearConstraint("e");
  ModelSolveParameters params{.lazy_linear_constraints = {c, e}};

  ModelSolveParametersProto expected;
  expected.add_lazy_linear_constraint_ids(c.id());
  expected.add_lazy_linear_constraint_ids(e.id());
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
}

TEST(ModelSolveParametersTest, LazyLinearConstraintsOtherModel) {
  Model model;
  Model other;
  const LinearConstraint c = other.AddLinearConstraint("c");
  const ModelSolveParameters params{.lazy_linear_constraints = {c}};
  EXPECT_THAT(params.CheckModelStorage(model.storage()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(ModelSolveParametersTest, FromProtoEmptyRoundTrip) {
  const Model model;
  const ModelSolveParametersProto proto;

  ASSERT_OK_AND_ASSIGN(const auto params,
                       ModelSolveParameters::FromProto(model, proto));
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(proto)));
}

TEST(ModelSolveParametersTest, FromProtoFullRoundTrip) {
  Model model;
  const Variable x = model.AddVariable();
  model.AddVariable();
  model.AddLinearConstraint();
  model.AddLinearConstraint();
  model.AddQuadraticConstraint(x * x <= 0.0);
  model.AddQuadraticConstraint(x * x <= 0.0);
  model.AddAuxiliaryObjective(2);
  model.AddAuxiliaryObjective(3);

  ModelSolveParametersProto proto;
  proto.mutable_variable_values_filter()->set_filter_by_ids(true);
  proto.mutable_variable_values_filter()->add_filtered_ids(1);

  proto.mutable_dual_values_filter()->set_filter_by_ids(true);
  proto.mutable_dual_values_filter()->add_filtered_ids(0);

  proto.mutable_quadratic_dual_values_filter()->set_filter_by_ids(true);
  proto.mutable_quadratic_dual_values_filter()->add_filtered_ids(0);

  proto.mutable_reduced_costs_filter()->set_skip_zero_values(true);

  proto.mutable_initial_basis()->set_basic_dual_feasibility(
      SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
  auto& basis_vars = *proto.mutable_initial_basis()->mutable_variable_status();
  basis_vars.add_ids(0);
  basis_vars.add_ids(1);
  basis_vars.add_values(BasisStatusProto::BASIS_STATUS_BASIC);
  basis_vars.add_values(BasisStatusProto::BASIS_STATUS_BASIC);

  auto& basis_cons =
      *proto.mutable_initial_basis()->mutable_constraint_status();
  basis_cons.add_ids(0);
  basis_cons.add_ids(1);
  basis_cons.add_values(BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND);
  basis_cons.add_values(BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND);

  SolutionHintProto& hint = *proto.add_solution_hints();
  hint.mutable_variable_values()->add_ids(0);
  hint.mutable_variable_values()->add_ids(1);
  hint.mutable_variable_values()->add_values(10.0);
  hint.mutable_variable_values()->add_values(20.0);

  proto.mutable_branching_priorities()->add_ids(1);
  proto.mutable_branching_priorities()->add_values(3);

  proto.mutable_primary_objective_parameters()
      ->set_objective_degradation_absolute_tolerance(0.5);
  proto.mutable_primary_objective_parameters()
      ->mutable_time_limit()
      ->set_seconds(10);

  (*proto.mutable_auxiliary_objective_parameters())[1]
      .set_objective_degradation_relative_tolerance(0.2);
  (*proto.mutable_auxiliary_objective_parameters())[1]
      .mutable_time_limit()
      ->set_seconds(20);

  ASSERT_OK_AND_ASSIGN(const auto params,
                       ModelSolveParameters::FromProto(model, proto));
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(proto)));
}

TEST(ModelSolveParametersTest, FromProtoInvalidAuxObj) {
  const Model model;
  ModelSolveParametersProto proto;
  (*proto.mutable_auxiliary_objective_parameters())[1]
      .set_objective_degradation_absolute_tolerance(0.5);

  EXPECT_THAT(ModelSolveParameters::FromProto(model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("auxiliary_objective_parameters")));
}

TEST(ModelSolveParametersTest, FromProtoInvalidLazyConstraintIdsIsError) {
  const Model model;
  ModelSolveParametersProto proto;
  proto.add_lazy_linear_constraint_ids(2);

  EXPECT_THAT(ModelSolveParameters::FromProto(model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("lazy_linear_constraint")));
}

TEST(ObjectiveParametersTest, Proto) {
  ModelSolveParameters::ObjectiveParameters params;
  params.objective_degradation_absolute_tolerance = 3.0;
  params.objective_degradation_relative_tolerance = 4.0;
  params.time_limit = absl::Seconds(10);

  ObjectiveParametersProto expected;
  expected.set_objective_degradation_absolute_tolerance(3.0);
  expected.set_objective_degradation_relative_tolerance(4.0);
  expected.mutable_time_limit()->set_seconds(10);
  EXPECT_THAT(params.Proto(), IsOkAndHolds(EquivToProto(expected)));
}

TEST(ObjectiveParametersTest, FromProtoFull) {
  ObjectiveParametersProto proto;
  proto.set_objective_degradation_absolute_tolerance(3.0);
  proto.set_objective_degradation_relative_tolerance(4.0);
  proto.mutable_time_limit()->set_seconds(10);

  ASSERT_OK_AND_ASSIGN(
      const auto params,
      ModelSolveParameters::ObjectiveParameters::FromProto(proto));
  EXPECT_THAT(params.objective_degradation_absolute_tolerance,
              testing::Optional(3.0));
  EXPECT_THAT(params.objective_degradation_relative_tolerance,
              testing::Optional(4.0));
  EXPECT_THAT(params.time_limit, absl::Seconds(10));
}

TEST(ObjectiveParametersTest, FromProtoEmpty) {
  ObjectiveParametersProto proto;
  ASSERT_OK_AND_ASSIGN(
      const auto params,
      ModelSolveParameters::ObjectiveParameters::FromProto(proto));
  EXPECT_EQ(params.objective_degradation_absolute_tolerance, std::nullopt);
  EXPECT_EQ(params.objective_degradation_relative_tolerance, std::nullopt);
  EXPECT_EQ(params.time_limit, absl::InfiniteDuration());
}

TEST(SolutionHintTest, HintMixedModels) {
  ModelStorage model_a;
  const Variable a_x(&model_a, model_a.AddVariable("x"));
  ModelStorage model_b;
  const LinearConstraint b_c(&model_b, model_b.AddLinearConstraint("c"));

  const ModelSolveParameters::SolutionHint hint = {
      .variable_values = {{a_x, 1.0}},
      .dual_values = {{b_c, 3.2}},
  };

  EXPECT_THAT(hint.CheckModelStorage(&model_a),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
  EXPECT_THAT(hint.CheckModelStorage(&model_b),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(SolutionHintTest, FromValidProto) {
  Model model;
  const Variable x1 = model.AddVariable("x1");
  model.AddVariable("x2");
  const Variable x3 = model.AddVariable("x3");
  model.AddLinearConstraint("c1");
  const LinearConstraint c2 = model.AddLinearConstraint("c2");

  SolutionHintProto hint_proto;
  hint_proto.mutable_variable_values()->add_ids(x1.id());
  hint_proto.mutable_variable_values()->add_values(1.0);
  hint_proto.mutable_variable_values()->add_ids(x3.id());
  hint_proto.mutable_variable_values()->add_values(0.0);
  hint_proto.mutable_dual_values()->add_ids(c2.id());
  hint_proto.mutable_dual_values()->add_values(-1.0);

  ASSERT_OK_AND_ASSIGN(
      const ModelSolveParameters::SolutionHint hint,
      ModelSolveParameters::SolutionHint::FromProto(model, hint_proto));
  EXPECT_THAT(hint.variable_values,
              UnorderedElementsAre(Pair(x1, 1.0), Pair(x3, 0.0)));
  EXPECT_THAT(hint.dual_values, UnorderedElementsAre(Pair(c2, -1.0)));
}

TEST(SolutionHintTest, FromProtoInvalidVariableValues) {
  // This test only tests one failing case. It relies on the fact that we use
  // VariableValuesFromProto() which is already properly unit tested.
  Model model;
  const Variable x1 = model.AddVariable("x1");
  model.AddVariable("x2");
  model.AddVariable("x3");

  SolutionHintProto hint_proto;
  hint_proto.mutable_variable_values()->add_ids(x1.id());
  hint_proto.mutable_variable_values()->add_values(1.0);
  // We use an index that does not exist in the model.
  hint_proto.mutable_variable_values()->add_ids(model.next_variable_id());
  hint_proto.mutable_variable_values()->add_values(0.0);

  EXPECT_THAT(ModelSolveParameters::SolutionHint::FromProto(model, hint_proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable_values")));
}

TEST(SolutionHintTest, FromProtoInvalidDualValues) {
  // This test only tests one failing case. It relies on the fact that we use
  // LinearConstraintValuesFromProto() which is already properly unit tested.
  Model model;
  const LinearConstraint c1 = model.AddLinearConstraint("c1");
  model.AddLinearConstraint("c2");
  model.AddLinearConstraint("c3");

  SolutionHintProto hint_proto;
  hint_proto.mutable_dual_values()->add_ids(c1.id());
  hint_proto.mutable_dual_values()->add_values(1.0);
  // We use an index that does not exist in the model.
  hint_proto.mutable_dual_values()->add_ids(model.next_linear_constraint_id());
  hint_proto.mutable_dual_values()->add_values(0.0);

  EXPECT_THAT(
      ModelSolveParameters::SolutionHint::FromProto(model, hint_proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("dual_values")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
