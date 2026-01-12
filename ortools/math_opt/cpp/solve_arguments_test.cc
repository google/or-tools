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

#include "ortools/math_opt/cpp/solve_arguments.h"

#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(CheckModelStorageTest, CorrectModelAndCallback) {
  Model model;
  const Variable x = model.AddVariable("x");

  const SolveArguments args = {
      .model_parameters = ModelSolveParameters::OnlySomePrimalVariables({x}),
      .callback_registration =
          {
              .events = {CallbackEvent::kMipSolution},
              .mip_solution_filter = MakeKeepKeysFilter({x}),
          },
      .callback = [](const CallbackData&) { return CallbackResult{}; },
  };

  EXPECT_OK(args.CheckModelStorage(model.storage()));
}

TEST(CheckModelStorageTest, WrongModelInModelParameters) {
  Model model;
  const Variable x = model.AddVariable("x");

  const SolveArguments args = {
      .model_parameters = ModelSolveParameters::OnlySomePrimalVariables({x}),
  };

  Model other_model;
  EXPECT_THAT(args.CheckModelStorage(other_model.storage()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("model_parameters")));
}

TEST(CheckModelStorageTest, WrongModelInCallbackRegistration) {
  Model model;
  const Variable x = model.AddVariable("x");

  const SolveArguments args = {
      .callback_registration =
          {
              .events = {CallbackEvent::kMipSolution},
              .mip_solution_filter = MakeKeepKeysFilter({x}),
          },
      .callback = [](const CallbackData&) { return CallbackResult{}; },
  };

  Model other_model;
  EXPECT_THAT(args.CheckModelStorage(other_model.storage()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("callback_registration"),
                             HasSubstr("mip_solution_filter"))));
}

}  // namespace
}  // namespace operations_research::math_opt
