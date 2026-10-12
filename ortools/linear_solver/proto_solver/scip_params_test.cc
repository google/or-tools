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

#include "ortools/linear_solver/proto_solver/scip_params.h"

#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/def.h"
#include "scip/scip.h"
#include "scip/scip_general.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scipdefplugins.h"

namespace operations_research {
namespace {

using ::testing::status::StatusIs;

class LegacyScipParamsTest : public ::testing::Test {
 public:
  LegacyScipParamsTest() : scip_(nullptr) {
    CHECK_OK(SCIP_TO_STATUS((SCIPcreate(&scip_))));
    CHECK_OK(SCIP_TO_STATUS((SCIPincludeDefaultPlugins(scip_))));
    CHECK_OK(
        SCIP_TO_STATUS((SCIPcreateProbBasic(scip_, "scip_legacy_param_test"))));
  }
  ~LegacyScipParamsTest() override {
    CHECK_OK(SCIP_TO_STATUS(SCIPfree(&scip_)));
  }

 protected:
  SCIP* scip_;
};

TEST_F(LegacyScipParamsTest, SingleLineParams) {
  ASSERT_OK(LegacyScipSetSolverSpecificParameters(
      "conflict/minmaxvars=17,branching/scorefac=0.87", scip_));
  int minmaxvars;
  ASSERT_OK(SCIP_TO_STATUS(
      SCIPgetIntParam(scip_, "conflict/minmaxvars", &minmaxvars)));
  EXPECT_EQ(minmaxvars, 17);
  double scorefac;
  ASSERT_OK(
      SCIP_TO_STATUS(SCIPgetRealParam(scip_, "branching/scorefac", &scorefac)));
  EXPECT_EQ(scorefac, 0.87);
}

TEST_F(LegacyScipParamsTest, SetIntParam) {
  ASSERT_OK(
      LegacyScipSetSolverSpecificParameters("conflict/minmaxvars = 17", scip_));
  int minmaxvars;
  ASSERT_OK(SCIP_TO_STATUS(
      SCIPgetIntParam(scip_, "conflict/minmaxvars", &minmaxvars)));
  EXPECT_EQ(minmaxvars, 17);
}

TEST_F(LegacyScipParamsTest, SetBoolParam) {
  {
    ASSERT_OK(LegacyScipSetSolverSpecificParameters(
        "branching/preferbinary = true", scip_));
    SCIP_Bool prefer_binary;
    ASSERT_OK(SCIP_TO_STATUS(
        SCIPgetBoolParam(scip_, "branching/preferbinary", &prefer_binary)));
    EXPECT_TRUE(static_cast<bool>(prefer_binary));
  }
  {
    ASSERT_OK(LegacyScipSetSolverSpecificParameters(
        "branching/preferbinary = false", scip_));
    SCIP_Bool prefer_binary;
    ASSERT_OK(SCIP_TO_STATUS(
        SCIPgetBoolParam(scip_, "branching/preferbinary", &prefer_binary)));
    EXPECT_FALSE(static_cast<bool>(prefer_binary));
  }
}

TEST_F(LegacyScipParamsTest, SetLongParam) {
  ASSERT_OK(LegacyScipSetSolverSpecificParameters("limits/nodes = 5", scip_));
  SCIP_Longint nodes;
  ASSERT_OK(SCIP_TO_STATUS(SCIPgetLongintParam(scip_, "limits/nodes", &nodes)));
  EXPECT_EQ(nodes, 5);
}

TEST_F(LegacyScipParamsTest, SetCharParam) {
  ASSERT_OK(
      LegacyScipSetSolverSpecificParameters("branching/scorefunc = q", scip_));
  char scorefunc;
  ASSERT_OK(SCIP_TO_STATUS(
      SCIPgetCharParam(scip_, "branching/scorefunc", &scorefunc)));
  EXPECT_EQ(scorefunc, 'q');
}

TEST_F(LegacyScipParamsTest, SetStringParam) {
  ASSERT_OK(LegacyScipSetSolverSpecificParameters(
      "heuristics/undercover/fixingalts = nl", scip_));
  char* fixingalts;
  ASSERT_OK(SCIP_TO_STATUS(SCIPgetStringParam(
      scip_, "heuristics/undercover/fixingalts", &fixingalts)));
  EXPECT_EQ(std::string(fixingalts), "nl");
}

TEST_F(LegacyScipParamsTest, SetDoubleParam) {
  ASSERT_OK(LegacyScipSetSolverSpecificParameters("branching/scorefac = 0.87",
                                                  scip_));
  double scorefac;
  ASSERT_OK(
      SCIP_TO_STATUS(SCIPgetRealParam(scip_, "branching/scorefac", &scorefac)));
  EXPECT_EQ(scorefac, 0.87);
}

TEST_F(LegacyScipParamsTest, SetMultipleParam) {
  std::string params(absl::StrCat("conflict/minmaxvars = 3\n",
                                  "branching/scorefac = 0.87\n",
                                  "branching/clamp = 0.1\n"));
  ASSERT_OK(LegacyScipSetSolverSpecificParameters(params, scip_));
  int minmaxvars;
  ASSERT_OK(SCIP_TO_STATUS(
      SCIPgetIntParam(scip_, "conflict/minmaxvars", &minmaxvars)));
  EXPECT_EQ(minmaxvars, 3);
  double scorefac;
  ASSERT_OK(
      SCIP_TO_STATUS(SCIPgetRealParam(scip_, "branching/scorefac", &scorefac)));
  EXPECT_EQ(scorefac, 0.87);
  double clamp;
  ASSERT_OK(SCIP_TO_STATUS(SCIPgetRealParam(scip_, "branching/clamp", &clamp)));
  EXPECT_EQ(clamp, 0.1);
}

TEST_F(LegacyScipParamsTest, SetEmptyString) {
  ASSERT_OK(LegacyScipSetSolverSpecificParameters("", scip_));
  // Check that a random parameter takes its default value.
  SCIP_Longint node_limit;
  ASSERT_OK(
      SCIP_TO_STATUS(SCIPgetLongintParam(scip_, "limits/nodes", &node_limit)));
  EXPECT_EQ(node_limit, -1);
}

TEST_F(LegacyScipParamsTest, InvalidParamName) {
  EXPECT_THAT(
      LegacyScipSetSolverSpecificParameters("fake_param_name = 8", scip_),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Invalid parameter name 'fake_param_name'"));
}

TEST_F(LegacyScipParamsTest, ParamValueDoesNotParse) {
  EXPECT_THAT(
      LegacyScipSetSolverSpecificParameters("limits/nodes = bad_value", scip_),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Invalid parameter value 'limits/nodes = bad_value'"));
}

TEST_F(LegacyScipParamsTest, ParamValueOutOfBounds) {
  EXPECT_THAT(LegacyScipSetSolverSpecificParameters("limits/nodes = -2", scip_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       testing::HasSubstr("SCIPsetLongintParam")));
}

TEST_F(LegacyScipParamsTest, MisformatedParamString) {
  EXPECT_THAT(
      LegacyScipSetSolverSpecificParameters("bad_format_no_value", scip_),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          testing::HasSubstr("Cannot parse parameter 'bad_format_no_value'")));
}

}  // namespace
}  // namespace operations_research
