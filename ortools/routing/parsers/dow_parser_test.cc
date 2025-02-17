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

#include "ortools/routing/parsers/dow_parser.h"

#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"
#include "ortools/routing/parsers/capacity_planning.pb.h"

namespace operations_research::routing {
namespace {
TEST(CapacityPlanningReaderTest, C33PassesOK) {
  CapacityPlanningInstance request;
  ::absl::Status status =
      ReadFile(file::JoinPathRespectAbsolute(
                   ::testing::SrcDir(), "operations_research_data/",
                   "MULTICOM_FIXED_CHARGE_NETWORK_DESIGN/C/c33.dow"),
               &request);
  EXPECT_OK(status);
  const NetworkTopology& topology = request.topology();
  const int num_arcs = topology.from_node_size();
  EXPECT_EQ(num_arcs, topology.to_node_size());
  EXPECT_EQ(num_arcs, topology.fixed_cost_size());
  EXPECT_EQ(num_arcs, topology.variable_cost_size());
  EXPECT_EQ(num_arcs, topology.capacity_size());
  EXPECT_EQ(num_arcs, 228);
  const Commodities& commodities = request.commodities();
  const int num_commodities = commodities.from_node_size();
  EXPECT_EQ(num_commodities, 39);
  EXPECT_EQ(commodities.to_node_size(), num_commodities);
  EXPECT_EQ(commodities.demand_size(), num_commodities);
}

TEST(CapacityPlanningReaderTest, C34DoesNotExist) {
  CapacityPlanningInstance request;
  ::absl::Status status =
      ReadFile(file::JoinPathRespectAbsolute(
                   ::testing::SrcDir(), "operations_research_data/",
                   "MULTICOM_FIXED_CHARGE_NETWORK_DESIGN/C/c34.dow"),
               &request);
  EXPECT_THAT(::util::StatusToString(status),
              testing::HasSubstr("generic::not_found"));
}

}  // namespace
}  // namespace operations_research::routing
