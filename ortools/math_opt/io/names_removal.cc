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

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

namespace {

template <typename T>
void RemoveMapNames(google::protobuf::Map<int64_t, T>& map) {
  for (auto& [unused, value] : map) {
    value.clear_name();
  }
}

}  // namespace

void RemoveNames(ModelProto& model) {
  model.clear_name();
  model.mutable_variables()->clear_names();
  model.mutable_linear_constraints()->clear_names();
  RemoveMapNames(*model.mutable_auxiliary_objectives());
  RemoveMapNames(*model.mutable_quadratic_constraints());
  RemoveMapNames(*model.mutable_second_order_cone_constraints());
  RemoveMapNames(*model.mutable_sos1_constraints());
  RemoveMapNames(*model.mutable_sos2_constraints());
  RemoveMapNames(*model.mutable_indicator_constraints());
}

void RemoveNames(ModelUpdateProto& update) {
  update.mutable_new_variables()->clear_names();
  update.mutable_new_linear_constraints()->clear_names();
  RemoveMapNames(
      *update.mutable_auxiliary_objectives_updates()->mutable_new_objectives());
  RemoveMapNames(*update.mutable_quadratic_constraint_updates()
                      ->mutable_new_constraints());
  RemoveMapNames(*update.mutable_second_order_cone_constraint_updates()
                      ->mutable_new_constraints());
  RemoveMapNames(
      *update.mutable_sos1_constraint_updates()->mutable_new_constraints());
  RemoveMapNames(
      *update.mutable_sos2_constraint_updates()->mutable_new_constraints());
  RemoveMapNames(*update.mutable_indicator_constraint_updates()
                      ->mutable_new_constraints());
}

}  // namespace operations_research::math_opt
