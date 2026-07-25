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

#include "ortools/linear_solver/gurobi_util.h"

#include <functional>

#include "absl/cleanup/cleanup.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/third_party_solvers/gurobi_environment.h"

namespace operations_research {
namespace {

constexpr int kGurobiOkCode = 0;

void SetTestParameters(GRBenv* const env, const int output_flag,
                       const int threads, const double mip_gap,
                       const char* const log_file) {
  ASSERT_EQ(GRBsetintparam(env, GRB_INT_PAR_OUTPUTFLAG, output_flag),
            kGurobiOkCode)
      << GRBgeterrormsg(env);
  ASSERT_EQ(GRBsetintparam(env, GRB_INT_PAR_THREADS, threads), kGurobiOkCode)
      << GRBgeterrormsg(env);
  ASSERT_EQ(GRBsetdblparam(env, GRB_DBL_PAR_MIPGAP, mip_gap), kGurobiOkCode)
      << GRBgeterrormsg(env);
  ASSERT_EQ(GRBsetstrparam(env, GRB_STR_PAR_LOGFILE, log_file), kGurobiOkCode)
      << GRBgeterrormsg(env);
}

void ExpectTestParameters(GRBenv* const env, const int expected_output_flag,
                          const int expected_threads,
                          const double expected_mip_gap,
                          const char* const expected_log_file) {
  int output_flag = -1;
  ASSERT_EQ(GRBgetintparam(env, GRB_INT_PAR_OUTPUTFLAG, &output_flag),
            kGurobiOkCode)
      << GRBgeterrormsg(env);
  EXPECT_EQ(output_flag, expected_output_flag);

  int threads = 0;
  ASSERT_EQ(GRBgetintparam(env, GRB_INT_PAR_THREADS, &threads), kGurobiOkCode)
      << GRBgeterrormsg(env);
  EXPECT_EQ(threads, expected_threads);

  double mip_gap = 0.0;
  ASSERT_EQ(GRBgetdblparam(env, GRB_DBL_PAR_MIPGAP, &mip_gap), kGurobiOkCode)
      << GRBgeterrormsg(env);
  EXPECT_DOUBLE_EQ(mip_gap, expected_mip_gap);

  char log_file[GRB_MAX_STRLEN + 1];
  ASSERT_EQ(GRBgetstrparam(env, GRB_STR_PAR_LOGFILE, log_file), kGurobiOkCode)
      << GRBgeterrormsg(env);
  log_file[GRB_MAX_STRLEN] = '\0';
  EXPECT_STREQ(log_file, expected_log_file);
}

void RunCopyGurobiParametersTest(const bool force_fallback) {
  absl::StatusOr<GRBenv*> src_status = GetGurobiEnv();
  if (!src_status.ok()) {
    GTEST_SKIP() << src_status.status();
  }
  GRBenv* const src = src_status.value();
  absl::Cleanup src_cleanup = [src] { GRBfreeenv(src); };

  absl::StatusOr<GRBenv*> dest_status = GetGurobiEnv();
  if (!dest_status.ok()) {
    GTEST_SKIP() << dest_status.status();
  }
  GRBenv* const dest = dest_status.value();
  absl::Cleanup dest_cleanup = [dest] { GRBfreeenv(dest); };

  SetTestParameters(src, /*output_flag=*/0, /*threads=*/1, /*mip_gap=*/0.123,
                    "gurobi_util_test_src.log");
  SetTestParameters(dest, /*output_flag=*/1, /*threads=*/2, /*mip_gap=*/0.321,
                    "gurobi_util_test_dest.log");

  if (force_fallback) {
    const std::function<int(GRBenv*, GRBenv*)> saved_copyparams = GRBcopyparams;
    GRBcopyparams = nullptr;
    absl::Cleanup restore_copyparams = [saved_copyparams] {
      GRBcopyparams = saved_copyparams;
    };
    ASSERT_OK(CopyGurobiParameters(dest, src));
  } else {
    ASSERT_OK(CopyGurobiParameters(dest, src));
  }

  ExpectTestParameters(dest, /*expected_output_flag=*/0, /*expected_threads=*/1,
                       /*expected_mip_gap=*/0.123,
                       "gurobi_util_test_src.log");
}

TEST(GurobiUtilTest, CopyGurobiParametersCopiesChangedParameters) {
  RunCopyGurobiParametersTest(/*force_fallback=*/false);
}

TEST(GurobiUtilTest, CopyGurobiParametersFallbackCopiesChangedParameters) {
  RunCopyGurobiParametersTest(/*force_fallback=*/true);
}

TEST(GurobiUtilTest, LoadGurobiDynamicLibraryCanBeCalledTwice) {
  const absl::Status first_load = LoadGurobiDynamicLibrary({});
  if (!first_load.ok()) {
    GTEST_SKIP() << first_load;
  }
  ASSERT_OK(LoadGurobiDynamicLibrary({}));

  int major = -1;
  int minor = -1;
  int technical = -1;
  GRBversion(&major, &minor, &technical);
  EXPECT_GE(major, 0);
  EXPECT_GE(minor, 0);
  EXPECT_GE(technical, 0);
}

}  // namespace
}  // namespace operations_research
