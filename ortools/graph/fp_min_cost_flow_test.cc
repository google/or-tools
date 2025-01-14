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

#include "ortools/graph/fp_min_cost_flow.h"

#include <cmath>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/base/no_destructor.h"
#include "absl/strings/escaping.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "testing/base/public/mock-log.h"

namespace operations_research {
namespace {

using ::testing::AnyNumber;
using ::testing::DoubleNear;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Mock;
using ::testing::ScopedMockLog;

using Status = SimpleFloatingPointMinCostFlow::Status;
using FPFlowQuantity = SimpleFloatingPointMinCostFlow::FPFlowQuantity;
using CostValue = SimpleFloatingPointMinCostFlow::CostValue;
using NodeIndex = SimpleFloatingPointMinCostFlow::NodeIndex;
using ArcIndex = SimpleFloatingPointMinCostFlow::ArcIndex;
using ::operations_research::internal::ScaleFlow;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr SimpleFloatingPointMinCostFlow::CostValue kMaxCostValue =
    std::numeric_limits<SimpleFloatingPointMinCostFlow::CostValue>::max();
const SimpleFloatingPointMinCostFlow::FPFlowQuantity kMaxFPFlow =
    std::numeric_limits<SimpleFloatingPointMinCostFlow::FPFlowQuantity>::max();
constexpr SimpleFloatingPointMinCostFlow::FPFlowQuantity kMinFPFlow =
    std::numeric_limits<
        SimpleFloatingPointMinCostFlow::FPFlowQuantity>::denorm_min();

// Returns the path of the source file that emits LOG(ERROR) in case of failure.
//
// It is meant to be used for log mocking assertions. It needs to be a `const
// std::string&` and can't be an absl::string_view. Thus this function returns a
// reference on a static.
const std::string& SourceFilePath() {
  static const absl::NoDestructor<std::string> kModule(
      "ortools/graph/fp_min_cost_flow.cc");
  return *kModule;
}

TEST(SimpleFloatingPointMinCostFlowTest, Empty) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.NumNodes(), 0);
  EXPECT_EQ(fp_min_cost_flow.NumArcs(), 0);
}

TEST(SimpleFloatingPointMinCostFlowTest, SetNodeSupplyCreateNodes) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // Setting the supply of node 3 should create four nodes, even if the supply
  // is 0.0.
  fp_min_cost_flow.SetNodeSupply(/*node=*/3, /*supply=*/0.0);
  ASSERT_EQ(fp_min_cost_flow.NumNodes(), 4);
  EXPECT_EQ(fp_min_cost_flow.Supply(0), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(1), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(2), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(3), 0.0);

  // Setting arbitrary positive and negative values.
  fp_min_cost_flow.SetNodeSupply(/*node=*/1, /*supply=*/13.75);
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, /*supply=*/-28.125);
  ASSERT_EQ(fp_min_cost_flow.NumNodes(), 4);
  EXPECT_EQ(fp_min_cost_flow.Supply(0), -28.125);
  EXPECT_EQ(fp_min_cost_flow.Supply(1), 13.75);
  EXPECT_EQ(fp_min_cost_flow.Supply(2), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(3), 0.0);
}

TEST(SimpleFloatingPointMinCostFlowTest, AddArcCreateNodes) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // Adding an arc between nodes 1 and 3 should create four nodes, with zero
  // supply.
  const ArcIndex arc0 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/3, /*head=*/1, /*capacity=*/0.0, /*unit_cost=*/0);
  const ArcIndex arc1 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/15.25, /*unit_cost=*/3);
  ASSERT_EQ(fp_min_cost_flow.NumNodes(), 4);
  EXPECT_EQ(fp_min_cost_flow.Supply(0), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(1), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(2), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Supply(3), 0.0);

  ASSERT_EQ(fp_min_cost_flow.NumArcs(), 2);
  EXPECT_EQ(fp_min_cost_flow.Tail(arc0), 3);
  EXPECT_EQ(fp_min_cost_flow.Head(arc0), 1);
  EXPECT_EQ(fp_min_cost_flow.Capacity(arc0), 0.0);
  EXPECT_EQ(fp_min_cost_flow.UnitCost(arc0), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Tail(arc1), 0);
  EXPECT_EQ(fp_min_cost_flow.Head(arc1), 1);
  EXPECT_EQ(fp_min_cost_flow.Capacity(arc1), 15.25);
  EXPECT_EQ(fp_min_cost_flow.UnitCost(arc1), 3);
}

// Value-parameterized test of invalid node supply values.
class BadNodeSupplyTest : public testing::TestWithParam<double> {};

TEST_P(BadNodeSupplyTest, FailingSolve) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, /*supply=*/GetParam());

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log).Times(AnyNumber());  // Ignore unexpected logs.
  EXPECT_CALL(log, Log(base_logging::ERROR, SourceFilePath(),
                       HasSubstr("supply is not finite")));
  log.StartCapturingLogs();

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::BAD_CAPACITY_RANGE);

  log.StopCapturingLogs();
  Mock::VerifyAndClearExpectations(&log);
}

// Tests that failures properly resets computed arc flows before returning.
TEST_P(BadNodeSupplyTest, FailureResetsArcFlows) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;

  // First make a successful solve.
  const ArcIndex arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/5.25, /*unit_cost=*/0);
  fp_min_cost_flow.SetNodeSupply(0, 10.0);
  fp_min_cost_flow.SetNodeSupply(1, -10.0);
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 5.25);

  // Then set the invalid supply, the next solve should fail and resets the arc
  // flow.
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, /*supply=*/GetParam());
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::BAD_CAPACITY_RANGE);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 0.0);
}

INSTANTIATE_TEST_SUITE_P(All, BadNodeSupplyTest,
                         testing::Values(kInf, -kInf, kNaN));

// Value-parameterized test of invalid arc capacity values.
class BadArcCapacityTest : public testing::TestWithParam<double> {};

TEST_P(BadArcCapacityTest, FailingSolve) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/GetParam(), /*unit_cost=*/0);

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log).Times(AnyNumber());  // Ignore unexpected logs.
  EXPECT_CALL(log, Log(base_logging::ERROR, SourceFilePath(),
                       HasSubstr("capacity is not finite")));
  log.StartCapturingLogs();

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::BAD_CAPACITY_RANGE);

  log.StopCapturingLogs();
  Mock::VerifyAndClearExpectations(&log);
}

// Tests that failures properly resets computed arc flows before returning.
TEST_P(BadArcCapacityTest, FailureResetsArcFlows) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;

  // First make a successful solve.
  const ArcIndex arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/5.25, /*unit_cost=*/0);
  fp_min_cost_flow.SetNodeSupply(0, 10.0);
  fp_min_cost_flow.SetNodeSupply(1, -10.0);
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 5.25);

  // Then add an arc with the invalid capacity, the next solve should fail and
  // resets the arc flow.
  const ArcIndex bad_arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/GetParam(), /*unit_cost=*/0);
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            SimpleFloatingPointMinCostFlow::Status::BAD_CAPACITY_RANGE);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Flow(bad_arc), 0.0);
}

INSTANTIATE_TEST_SUITE_P(All, BadArcCapacityTest,
                         testing::Values(kInf, -kInf, kNaN));

// Parameters for the TwoNodesOneArcTest value-parameterized test.
struct TwoNodesOneArcTestParams {
  // Name of the test; this will be used as a suffix to the test name and thus
  // should be snake_case with no spaces.
  std::string name;
  FPFlowQuantity head_node_supply = 0.0;
  FPFlowQuantity tail_node_supply = 0.0;
  FPFlowQuantity arc_capacity = 0.0;
  CostValue arc_unit_cost = 0;
  // The expected computed flow on the arc; it is tested using
  // expected_flow_tolerance absolute tolerance.
  FPFlowQuantity expected_flow = 0.0;
  FPFlowQuantity expected_flow_tolerance = 0.0;

  friend std::ostream& operator<<(std::ostream& out,
                                  const TwoNodesOneArcTestParams& params) {
    out << "{ name: '" << absl::CEscape(params.name) << "', head_node_supply: "
        << RoundTripDoubleFormat(params.head_node_supply)
        << ", tail_node_supply: "
        << RoundTripDoubleFormat(params.tail_node_supply)
        << ", arc_capacity: " << RoundTripDoubleFormat(params.arc_capacity)
        << ", arc_unit_cost: " << params.arc_unit_cost
        << ", expected_flow: " << RoundTripDoubleFormat(params.expected_flow)
        << ", expected_flow_tolerance: "
        << RoundTripDoubleFormat(params.expected_flow_tolerance) << " }";
    return out;
  }
};

// Value-parameterized tests with a graph containing a single arc and two nodes.
class TwoNodesOneArcTest
    : public testing::TestWithParam<TwoNodesOneArcTestParams> {};

TEST_P(TwoNodesOneArcTest, SolveMaxFlowWithMinCost) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  const NodeIndex kTailNode = 0;
  const NodeIndex kHeadNode = 1;
  const ArcIndex arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/kTailNode, /*head=*/kHeadNode,
      /*capacity=*/GetParam().arc_capacity,
      /*unit_cost=*/GetParam().arc_unit_cost);
  fp_min_cost_flow.SetNodeSupply(kTailNode, GetParam().tail_node_supply);
  fp_min_cost_flow.SetNodeSupply(kHeadNode, GetParam().head_node_supply);

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);

  EXPECT_THAT(
      fp_min_cost_flow.Flow(arc),
      DoubleNear(GetParam().expected_flow, GetParam().expected_flow_tolerance));
}

INSTANTIATE_TEST_SUITE_P(
    All, TwoNodesOneArcTest,
    testing::Values(
        // All parameters at 0. Expect no flow.
        TwoNodesOneArcTestParams{.name = "empty"},
        // A flow of 1.0 with small integer capacities and supply.
        TwoNodesOneArcTestParams{
            .name = "small_integers",
            .head_node_supply = -1.0,
            .tail_node_supply = 2.0,
            .arc_capacity = 3.0,
            .arc_unit_cost = 0,
            .expected_flow = 1.0,
            .expected_flow_tolerance = 0.0,
        },
        // Some big but not huge capacity and supply numbers.
        TwoNodesOneArcTestParams{
            .name = "big_numbers",
            .head_node_supply = -1.0e18,
            .tail_node_supply = 2.0e16,
            .arc_capacity = 5.0e16,
            .arc_unit_cost = 0,
            .expected_flow = 2.0e16,
            .expected_flow_tolerance = 0.0,
        },
        // Big capacity and small supply. We use -1.1 so that we have a mantissa
        // with a lot of non-zeros (in base-2 -1.1 is -1.0001100110011...).
        //
        // With a value of 5.3e10, the largest scaling factor so that:
        //
        //   std::round(2^p * 5.3e10) < (2^63 - 1)
        //
        // is 2**27. With this value, the 1.1 theoretical flow after scaling and
        // unscaling will be:
        //
        //   std::round(1.1 * 2.0^27) * 2.0^-27 = 1.1000000014901161
        //
        // the error is thus:
        //
        //   1.4901160305669237e-09
        //
        // We simply use 1.5e-9 as tolerance.
        TwoNodesOneArcTestParams{
            .name = "big_capacity_small_supply",
            .head_node_supply = -1.1,
            .tail_node_supply = 2.5,
            .arc_capacity = 5.3e10,
            .arc_unit_cost = 0,
            .expected_flow = 1.1,
            .expected_flow_tolerance = 1.5e-9,  // See comment above.
        },
        // Small capacity and big supply. See previous test for rationale.
        TwoNodesOneArcTestParams{
            .name = "small_capacity_big_supply",
            .head_node_supply = -5.3e10,
            .tail_node_supply = 4.2e8,
            .arc_capacity = 1.1,
            .arc_unit_cost = 0,
            .expected_flow = 1.1,
            .expected_flow_tolerance = 1.0e-5,  // See comment above.
        },
        // tiny capacity (1.5) and huge supply (2^100). We expect scaling to
        // result in a 0 integer capacity as the scale factor that makes 2^100
        // fit into an int64_t will make the small capacity close to zero.
        TwoNodesOneArcTestParams{
            .name = "tiny_capacity_huge_supply",
            .head_node_supply = -std::pow(2.0, 100.0),
            .tail_node_supply = std::pow(2.0, 80.0),
            .arc_capacity = 1.5,
            .arc_unit_cost = 0,
            .expected_flow = 0,
            .expected_flow_tolerance = 0.0,
        },
        // Huge capacity (2^100) and tiny supply values (1.5). See previous test
        // for rationale.
        TwoNodesOneArcTestParams{
            .name = "huge_capacity_tiny_supply",
            .head_node_supply = -1.5,
            .tail_node_supply = 1.5,
            .arc_capacity = std::pow(2.0, 100.0),
            .arc_unit_cost = 0,
            .expected_flow = 0,
            .expected_flow_tolerance = 0.0,
        },
        // A capacity flow should be considered 0.
        TwoNodesOneArcTestParams{
            .name = "negative_capacity",
            .head_node_supply = -1.0,
            .tail_node_supply = 2.0,
            .arc_capacity = -3.0,
            .arc_unit_cost = 0,
            .expected_flow = 0.0,
            .expected_flow_tolerance = 0.0,
        },
        // Test scaling when the numbers are the smallest possible. The computed
        // flow is likely 0. We use 4 * kMinFPFlow as a reasonable tolerance.
        TwoNodesOneArcTestParams{
            .name = "min_denormal_flow",
            .head_node_supply = -kMinFPFlow,
            .tail_node_supply = kMinFPFlow,
            .arc_capacity = kMinFPFlow,
            .arc_unit_cost = 0,
            .expected_flow = kMinFPFlow,
            .expected_flow_tolerance = 4 * kMinFPFlow,
        },
        // Test when supply are the smallest possible and capacity is zero (so
        // that for each node we only have kMinFPFlow in or out flow).
        TwoNodesOneArcTestParams{
            .name = "min_denormal_supply_no_flow",
            .head_node_supply = -kMinFPFlow,
            .tail_node_supply = kMinFPFlow,
            .arc_capacity = 0.0,
            .arc_unit_cost = 0,
            .expected_flow = 0.0,
            .expected_flow_tolerance = 0.0,
        }, ),
    [](const testing::TestParamInfo<TwoNodesOneArcTestParams>& info) {
      return info.param.name;
    });

TEST(SimpleFloatingPointMinCostFlowTest, IntegerSolverFailing) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // To generate a failure, we use a unit_cost that is too high.
  const ArcIndex arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1,
      /*capacity=*/1.0,
      /*unit_cost=*/kMaxCostValue);
  fp_min_cost_flow.SetNodeSupply(0, 1.0);
  fp_min_cost_flow.SetNodeSupply(1, -1.0);

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::BAD_COST_RANGE);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 0.0);
}

TEST(SimpleFloatingPointMinCostFlowTest, IntegerSolverFailingResetsFlow) {
  // First make a successful solve.
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  const ArcIndex arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1,
      /*capacity=*/1.0,
      /*unit_cost=*/0);
  fp_min_cost_flow.SetNodeSupply(0, 1.0);
  fp_min_cost_flow.SetNodeSupply(1, -1.0);
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 1.0);

  // Then generate a failure using a too high cost value.
  const ArcIndex failing_arc = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1, /*capacity=*/1.0, /*unit_cost=*/kMaxCostValue);
  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::BAD_COST_RANGE);
  EXPECT_EQ(fp_min_cost_flow.Flow(arc), 0.0);
  EXPECT_EQ(fp_min_cost_flow.Flow(failing_arc), 0.0);
}

TEST(SimpleFloatingPointMinCostFlowTest, FloatingPointSumOverflow) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // We use two arcs pointing the same node with floating point capacities that
  // overflows the `double` exponent range. This will result in an overflow when
  // computing the maximum intake of the node which should result in an error.
  //
  // Note that we could update SimpleFloatingPointMinCostFlow to deal with that
  // by scaling the numbers down eventually.
  fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1,
      /*capacity=*/kMaxFPFlow,
      /*unit_cost=*/0);
  fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/1,
      /*capacity=*/kMaxFPFlow,
      /*unit_cost=*/0);

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log).Times(AnyNumber());  // Ignore unexpected logs.
  EXPECT_CALL(
      log, Log(base_logging::ERROR, SourceFilePath(), HasSubstr("not finite")));
  log.StartCapturingLogs();

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(),
            Status::BAD_CAPACITY_RANGE);

  log.StopCapturingLogs();
  Mock::VerifyAndClearExpectations(&log);
}

TEST(SimpleFloatingPointMinCostFlowTest, FirstScaleFailed) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // We build a corner case where the evaluation of the in/out-flow of nodes in
  // double would lead to a starting scale value that is too high.
  //
  // The chosen `supply` value is such that adding arc capacities one by one
  // (each arc has 1.0 capacity) does not change it. Thus the computed in-flow
  // and out-flow of both nodes is `supply`.
  //
  // On top of that computing the initial scale will lead to a scale of 2, which
  // would be OK as 2 * supply is a valid integer supply. The issue is that each
  // arc integer capacity is 2 and we have 2048 of them. And now:
  //
  //   2 * supply + 2 * 2048 > 2^63 - 1
  //
  // We thus will have to loop and try using a scale of 1.0 which will work.
  //
  // Note that this test we relies on the implementation adding nodes supply
  // first, then arc capacities.
  const FPFlowQuantity supply = std::nextafter(std::ldexp(1.0, 62), -kInf);
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, supply);
  fp_min_cost_flow.SetNodeSupply(/*node=*/1, -supply);
  std::vector<ArcIndex> arcs;
  for (int i = 0; i < 2048; ++i) {
    arcs.push_back(fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
        /*tail=*/0, /*head=*/1,
        /*capacity=*/1.0, /*unit_cost=*/0));
  }

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.LastSolveStats().num_tested_scales, 2);
  for (const ArcIndex arc : arcs) {
    // Use ASSERT_EQ() here to prevent having too many errors. All arcs are
    // equivalent so if there is an error it is likely to happen for each arc
    // anyway.
    ASSERT_EQ(fp_min_cost_flow.Flow(arc), 1.0) << "arc: " << arc;
  }
}

TEST(SimpleFloatingPointMinCostFlowTest, FirstScaleSucceeds) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // We build a corner case where the computed initial scale is good enough and
  // we test that we get the expected result. This happens when the max
  // in/out-flow is a power of two.
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, 2.0);

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);
  EXPECT_EQ(fp_min_cost_flow.LastSolveStats().num_tested_scales, 1);
}

TEST(SimpleFloatingPointMinCostFlowTest, Assignment) {
  SimpleFloatingPointMinCostFlow fp_min_cost_flow;
  // Tests that costs are taken into account by doing a simple assignment:
  //
  //         cap:5.1 cost:2
  //   n0:4 ---------------> n3:-5
  //                        ^
  //                       / cap:4
  //                      / cost:1
  //   n1:3 -------------+
  //                      \ cap:5
  //                       \ cost:2
  //                        v
  //   n2:5 ---------------> n4:-4
  //         cap:3 cost:3
  //
  // Here we expect:
  // * n3 to match its demand of 5 from n0 and n1, using n1 first as it has the
  //   lowest cost, then n0.
  // * n4 to match its demand of 4 from n1 and n2, using n1 first as it has the
  //   lower cost, then n2.
  // * n3 to consume from n1 only 2 and not 3 as doing so would starve n4 that
  //   would not be able to match its demand.
  fp_min_cost_flow.SetNodeSupply(/*node=*/0, 4.0);
  fp_min_cost_flow.SetNodeSupply(/*node=*/1, 3.0);
  fp_min_cost_flow.SetNodeSupply(/*node=*/2, 4.0);
  fp_min_cost_flow.SetNodeSupply(/*node=*/3, -5.0);
  fp_min_cost_flow.SetNodeSupply(/*node=*/4, -4.0);
  const ArcIndex a03 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/0, /*head=*/3, /*capacity=*/5.1, /*unit_cost=*/2);
  const ArcIndex a13 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/1, /*head=*/3, /*capacity=*/4.0, /*unit_cost=*/1);
  const ArcIndex a14 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/1, /*head=*/4, /*capacity=*/5.0, /*unit_cost=*/2);
  const ArcIndex a24 = fp_min_cost_flow.AddArcWithCapacityAndUnitCost(
      /*tail=*/2, /*head=*/4, /*capacity=*/3.0, /*unit_cost=*/3);

  EXPECT_EQ(fp_min_cost_flow.SolveMaxFlowWithMinCost(), Status::OPTIMAL);

  EXPECT_EQ(fp_min_cost_flow.Flow(a03), 3);
  EXPECT_EQ(fp_min_cost_flow.Flow(a13), 2);
  EXPECT_EQ(fp_min_cost_flow.Flow(a14), 1);
  EXPECT_EQ(fp_min_cost_flow.Flow(a24), 3);
}

TEST(ScaleFlowTest, AllValues) {
  constexpr SimpleMinCostFlow::FlowQuantity kMaxFlowQuantity =
      std::numeric_limits<SimpleMinCostFlow::FlowQuantity>::max();
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/kMaxFPFlow,
                      /*scale=*/std::numeric_limits<double>::max()),
            kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/-kMaxFPFlow,
                      /*scale=*/std::numeric_limits<double>::max()),
            -kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/kMaxFPFlow, /*scale=*/1.0), kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/-kMaxFPFlow, /*scale=*/1.0),
            -kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/2.0 * kMaxFlowQuantity, /*scale=*/1.0),
            kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/-2.0 * kMaxFlowQuantity, /*scale=*/1.0),
            -kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/kMaxFlowQuantity, /*scale=*/1.0),
            kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/-kMaxFlowQuantity, /*scale=*/1.0),
            -kMaxFlowQuantity);
  // The integer that is the `double` just before the rounded value of
  // kMaxFlowQuantity. This rounded value does not fit in an integer but its
  // predecessor will.
  const FPFlowQuantity kMaxFlowQuantityPredecessor =
      std::nextafter(static_cast<FPFlowQuantity>(kMaxFlowQuantity), 0.0);
  const SimpleMinCostFlow::FlowQuantity kMaxFlowQuantityPredecessorAsInt =
      static_cast<SimpleMinCostFlow::FlowQuantity>(kMaxFlowQuantityPredecessor);
  ASSERT_LT(kMaxFlowQuantityPredecessorAsInt, kMaxFlowQuantity);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/kMaxFlowQuantityPredecessor, /*scale=*/1.0),
            kMaxFlowQuantityPredecessor);
  EXPECT_EQ(ScaleFlow(/*fp_flow=*/-kMaxFlowQuantityPredecessor, /*scale=*/1.0),
            -kMaxFlowQuantityPredecessor);
}

TEST(ScaleFlowDeathTest, NaNScale) {
  EXPECT_DEATH(ScaleFlow(/*fp_flow=*/1.0, /*scale=*/kNaN), "scale");
}

TEST(ScaleFlowDeathTest, InfScale) {
  EXPECT_DEATH(ScaleFlow(/*fp_flow=*/1.0, /*scale=*/kInf), "scale");
}

TEST(ScaleFlowDeathTest, NaNFlow) {
  EXPECT_DEATH(ScaleFlow(/*fp_flow=*/kNaN, /*scale=*/1.0), "fp_flow");
}

TEST(ScaleFlowDeathTest, InfFlow) {
  EXPECT_DEATH(ScaleFlow(/*fp_flow=*/kInf, /*scale=*/1.0), "fp_flow");
}

TEST(SolveStatsTest, Stream) {
  std::ostringstream oss;
  oss << SimpleFloatingPointMinCostFlow::SolveStats{
      .scale = 0.1,
      .num_tested_scales = 4,
  };
  EXPECT_EQ(oss.str(), "{ scale: 0.1, num_tested_scales: 4 }");
}

}  // namespace
}  // namespace operations_research
