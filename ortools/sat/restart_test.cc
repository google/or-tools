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

#include "ortools/sat/restart.h"

#include "absl/base/macros.h"
#include "gtest/gtest.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

TEST(SUnivTest, Luby) {
  const int kSUniv[] = {1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, 1};
  for (int i = 0; i < ABSL_ARRAYSIZE(kSUniv); ++i) {
    EXPECT_EQ(kSUniv[i], SUniv(i + 1));
  }
}

TEST(RestartPolicyTest, BasicRunningAverageTest) {
  Model model;
  RestartPolicy* restart = model.GetOrCreate<RestartPolicy>();
  SatParameters* params = model.GetOrCreate<SatParameters>();

  // The parameters for this test.
  params->clear_restart_algorithms();
  params->add_restart_algorithms(SatParameters::DL_MOVING_AVERAGE_RESTART);
  params->set_use_blocking_restart(false);
  params->set_restart_dl_average_ratio(1.0);
  params->set_restart_running_window_size(10);
  restart->Reset();

  EXPECT_FALSE(restart->ShouldRestart());
  int i = 0;
  for (; i < 100; ++i) {
    const int unused = 0;
    const int decision_level = i;
    if (restart->ShouldRestart()) break;
    restart->OnConflict(unused, decision_level, unused);
  }

  // Increasing decision levels, so as soon as we have 11 conflicts and 10 in
  // the window, the window average is > global average.
  EXPECT_EQ(i, 11);

  // Now the window is reset, but not the global average. So as soon as we have
  // 10 conflicts, we restart.
  i = 0;
  for (; i < 100; ++i) {
    const int unused = 0;
    const int decision_level = 1000 - i;
    if (restart->ShouldRestart()) break;
    restart->OnConflict(unused, decision_level, unused);
  }
  EXPECT_EQ(i, 10);

  // If we call Reset() the global average is reaset, so if we have conflicts at
  // a decreasing decision level, we never restart.
  restart->Reset();
  i = 0;
  for (; i < 1000; ++i) {
    const int unused = 0;
    const int decision_level = 1000 - i;
    if (restart->ShouldRestart()) break;
    restart->OnConflict(unused, decision_level, unused);
  }
  EXPECT_EQ(i, 1000);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
