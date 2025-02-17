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

#include "ortools/pdlp/scheduler.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::TestWithParam;

struct SchedulerTestCase {
  std::string test_name;
  SchedulerType type;
  int num_threads;
};

using SchedulerTest = TestWithParam<SchedulerTestCase>;

TEST(SchedulerTest, CheckUnspecifiedSchedulerReturnsNullptr) {
  std::unique_ptr<Scheduler> scheduler =
      MakeScheduler(SCHEDULER_TYPE_UNSPECIFIED, 1);
  EXPECT_THAT(scheduler, IsNull());
}

TEST_P(SchedulerTest, CheckThreadCount) {
  const SchedulerTestCase& test_case = GetParam();
  std::unique_ptr<Scheduler> scheduler =
      MakeScheduler(test_case.type, test_case.num_threads);
  ASSERT_THAT(scheduler, NotNull());
  EXPECT_THAT(scheduler->num_threads(), Eq(test_case.num_threads));
}

TEST_P(SchedulerTest, CheckInfoString) {
  const SchedulerTestCase& test_case = GetParam();
  std::unique_ptr<Scheduler> scheduler =
      MakeScheduler(test_case.type, test_case.num_threads);
  ASSERT_THAT(scheduler, NotNull());
  if (test_case.type == SchedulerType::SCHEDULER_TYPE_GOOGLE_THREADPOOL) {
    EXPECT_THAT(scheduler->info_string(), Eq("google_threadpool"));
  } else if (test_case.type == SchedulerType::SCHEDULER_TYPE_EIGEN_THREADPOOL) {
    EXPECT_THAT(scheduler->info_string(), Eq("eigen_threadpool"));
  } else {
    FAIL() << "Invalid test_case type: " << test_case.type;
  }
}

TEST_P(SchedulerTest, CheckParallelVectorSum) {
  const SchedulerTestCase& test_case = GetParam();
  const int num_shards = 100000;  // High enough to catch race conditions.
  std::unique_ptr<Scheduler> scheduler =
      MakeScheduler(test_case.type, test_case.num_threads);
  ASSERT_THAT(scheduler, NotNull());
  const std::vector<double> data(num_shards, 1.0);
  std::atomic<double> sum = 0.0;
  // Adds `x` to `sum` using a CAS loop.
  std::function<void(const double&)> do_fn = [&](int i) {
    for (double new_sum = sum;
         !sum.compare_exchange_weak(new_sum, new_sum + data[i]);) {
    };
  };
  scheduler->ParallelFor(0, num_shards, do_fn);
  EXPECT_THAT(sum, Eq(num_shards));
}

INSTANTIATE_TEST_SUITE_P(
    SchedulerTests, SchedulerTest,
    testing::ValuesIn<SchedulerTestCase>({
        {"GoogleThreadPool2", SCHEDULER_TYPE_GOOGLE_THREADPOOL, 2},
        {"GoogleThreadPool4", SCHEDULER_TYPE_GOOGLE_THREADPOOL, 4},
        {"EigenThreadPool2", SCHEDULER_TYPE_EIGEN_THREADPOOL, 2},
        {"EigenThreadPool4", SCHEDULER_TYPE_EIGEN_THREADPOOL, 4},
    }),
    [](const testing::TestParamInfo<SchedulerTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace operations_research::pdlp
