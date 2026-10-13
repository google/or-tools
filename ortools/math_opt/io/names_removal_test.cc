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

#include "ortools/math_opt/io/names_removal.h"

#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;

TEST(RemoveNamesTest, AllNames) {
  Model model("model name");
  const Variable model_x = model.AddVariable("x");
  const LinearConstraint model_c = model.AddLinearConstraint("c");
  const Objective before_o = model.AddAuxiliaryObjective(2, "before_o");
  const QuadraticConstraint before_q =
      model.AddQuadraticConstraint(model_x <= 1, "before_q");
  const SecondOrderConeConstraint before_soc =
      model.AddSecondOrderConeConstraint({}, 0.0, "before_soc");
  const Sos1Constraint before_sos1 =
      model.AddSos1Constraint({}, {}, "before_sos1");
  const Sos2Constraint before_sos2 =
      model.AddSos2Constraint({}, {}, "before_sos2");
  const IndicatorConstraint before_i =
      model.AddIndicatorConstraint(model_x, model_x <= 0.0, false, "before_i");

  const std::unique_ptr<UpdateTracker> update_tracker =
      model.NewUpdateTracker();
  ASSERT_OK_AND_ASSIGN(const ModelProto base_model_proto,
                       update_tracker->ExportModel());

  const Variable model_y = model.AddVariable("y");
  const LinearConstraint model_d = model.AddLinearConstraint("d");
  const Objective after_o = model.AddAuxiliaryObjective(2, "after_o");
  const QuadraticConstraint after_q =
      model.AddQuadraticConstraint(model_x <= 1, "after_q");
  const SecondOrderConeConstraint after_soc =
      model.AddSecondOrderConeConstraint({}, 0.0, "after_soc");
  const Sos1Constraint after_sos1 =
      model.AddSos1Constraint({}, {}, "after_sos1");
  const Sos2Constraint after_sos2 =
      model.AddSos2Constraint({}, {}, "after_sos2");
  const IndicatorConstraint after_i =
      model.AddIndicatorConstraint(model_x, model_x <= 0.0, false, "after_i");

  ASSERT_OK_AND_ASSIGN(const std::optional<ModelUpdateProto> model_update_proto,
                       update_tracker->ExportModelUpdate());
  ASSERT_TRUE(model_update_proto.has_value());

  ModelProto anonymized_base_model_proto = base_model_proto;
  RemoveNames(anonymized_base_model_proto);

  ModelUpdateProto anonymized_model_update_proto = model_update_proto.value();
  RemoveNames(anonymized_model_update_proto);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<Model> anonymized_model,
                       Model::FromModelProto(anonymized_base_model_proto));
  ASSERT_OK(anonymized_model->ApplyUpdateProto(anonymized_model_update_proto));

  const Variable anonymized_model_x =
      anonymized_model->variable(model_x.typed_id());
  const Variable anonymized_model_y =
      anonymized_model->variable(model_y.typed_id());
  const LinearConstraint anonymized_model_c =
      anonymized_model->linear_constraint(model_c.typed_id());
  const LinearConstraint anonymized_model_d =
      anonymized_model->linear_constraint(model_d.typed_id());
  const Objective anonymized_before_o =
      anonymized_model->auxiliary_objective(*before_o.typed_id());
  const Objective anonymized_after_o =
      anonymized_model->auxiliary_objective(*after_o.typed_id());
  const QuadraticConstraint anonymized_before_q =
      anonymized_model->quadratic_constraint(before_q.typed_id());
  const QuadraticConstraint anonymized_after_q =
      anonymized_model->quadratic_constraint(after_q.typed_id());
  const SecondOrderConeConstraint anonymized_before_soc =
      anonymized_model->second_order_cone_constraint(before_soc.typed_id());
  const SecondOrderConeConstraint anonymized_after_soc =
      anonymized_model->second_order_cone_constraint(after_soc.typed_id());
  const Sos1Constraint anonymized_before_sos1 =
      anonymized_model->sos1_constraint(before_sos1.typed_id());
  const Sos1Constraint anonymized_after_sos1 =
      anonymized_model->sos1_constraint(after_sos1.typed_id());
  const Sos2Constraint anonymized_before_sos2 =
      anonymized_model->sos2_constraint(before_sos2.typed_id());
  const Sos2Constraint anonymized_after_sos2 =
      anonymized_model->sos2_constraint(after_sos2.typed_id());
  const IndicatorConstraint anonymized_before_i =
      anonymized_model->indicator_constraint(before_i.typed_id());
  const IndicatorConstraint anonymized_after_i =
      anonymized_model->indicator_constraint(after_i.typed_id());

  EXPECT_THAT(anonymized_model->name(), IsEmpty());
  EXPECT_THAT(anonymized_model_x.name(), IsEmpty());
  EXPECT_THAT(anonymized_model_y.name(), IsEmpty());
  EXPECT_THAT(anonymized_model_c.name(), IsEmpty());
  EXPECT_THAT(anonymized_model_d.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_o.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_o.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_q.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_q.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_soc.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_soc.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_sos1.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_sos1.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_sos2.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_sos2.name(), IsEmpty());
  EXPECT_THAT(anonymized_before_i.name(), IsEmpty());
  EXPECT_THAT(anonymized_after_i.name(), IsEmpty());
}

}  // namespace
}  // namespace operations_research::math_opt
