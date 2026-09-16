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

#include "ortools/math_opt/core/java/solver.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(CppSolveTest, SimpleSolve) {
  Model model;
  Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.Maximize(x);
  SolveParametersProto parameters;
  ModelSolveParametersProto model_params;
  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto result,
      cppSolve(model.ExportModel(), SOLVER_TYPE_GLOP, parameters, model_params,
               nullptr, CallbackRegistrationProto::default_instance(), nullptr,
               nullptr));
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_OPTIMAL);
}

class TestMessageCallback : public CppMessageCallback {
 public:
  void onMessage(const std::string& message) override {
    const absl::MutexLock lock(mutex_);
    for (const absl::string_view line : absl::StrSplit(message, '\n')) {
      logs_by_line_.push_back(std::string(line));
    }
  }

  const std::vector<std::string>& logs_by_line() const {
    const absl::MutexLock lock(mutex_);
    return logs_by_line_;
  }

 private:
  mutable absl::Mutex mutex_;
  std::vector<std::string> logs_by_line_ ABSL_GUARDED_BY(mutex_);
};

TEST(CppSolveTest, WithMessageCallback) {
  Model model;
  Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.Maximize(x);
  SolveParametersProto parameters;
  ModelSolveParametersProto model_params;
  TestMessageCallback message_cb;
  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto result,
      cppSolve(model.ExportModel(), SOLVER_TYPE_GLOP, parameters, model_params,
               &message_cb, CallbackRegistrationProto::default_instance(),
               nullptr, nullptr));
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_THAT(message_cb.logs_by_line(),
              Contains(HasSubstr("status: OPTIMAL")));
}

class TestSolveCallback : public CppSolveCallback {
 public:
  CallbackResultProto onEvent(const CallbackDataProto& cb_data) override {
    if (cb_data.event() == CALLBACK_EVENT_MIP_SOLUTION) {
      std::vector<int> solution;
      for (const double var_value : cb_data.primal_solution_vector().values()) {
        solution.push_back(static_cast<int>(std::round(var_value)));
      }
      absl::MutexLock lock(mutex_);
      solutions_.push_back(std::move(solution));
    }
    return {};
  }

  const std::vector<std::vector<int>>& solutions() const {
    absl::MutexLock lock(mutex_);
    return solutions_;
  }

 private:
  mutable absl::Mutex mutex_;
  std::vector<std::vector<int>> solutions_ ABSL_GUARDED_BY(mutex_);
};

TEST(CppSolveTest, WithSolveCallback) {
  Model model;
  Variable x = model.AddBinaryVariable();
  model.Maximize(x);
  SolveParametersProto parameters;
  ModelSolveParametersProto model_params;
  CallbackRegistrationProto cb_reg;
  cb_reg.add_request_registration(CALLBACK_EVENT_MIP_SOLUTION);
  TestSolveCallback solve_cb;
  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto result,
      cppSolve(model.ExportModel(), SOLVER_TYPE_CP_SAT, parameters,
               model_params, nullptr, cb_reg, &solve_cb, nullptr));
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_THAT(solve_cb.solutions(), Contains(ElementsAre(1)));
}

}  // namespace
}  // namespace operations_research::math_opt
