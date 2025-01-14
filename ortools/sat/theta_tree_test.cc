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

#include "ortools/sat/theta_tree.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/sat/integer_base.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

template <typename T>
class ThetaTreeTest : public ::testing::Test {};

using IntegerTypes = ::testing::Types<IntegerValue, int64_t>;
TYPED_TEST_SUITE(ThetaTreeTest, IntegerTypes);

TYPED_TEST(ThetaTreeTest, EnvelopeOfEmptySet) {
  ThetaLambdaTree<TypeParam> tree;
  tree.Reset(0);
  EXPECT_EQ(IntegerTypeMinimumValue<TypeParam>(), tree.GetEnvelope());
}

template <typename IntegerType>
std::vector<IntegerType> IntegerTypeVector(std::vector<int> arg) {
  return std::vector<IntegerType>(arg.begin(), arg.end());
}

TYPED_TEST(ThetaTreeTest, Envelope) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 2, 2});
  tree.Reset(5);

  for (int i = 0; i < 5; i++) {
    tree.AddOrUpdateEvent(i, envelope[i], energy[i], energy[i]);
  }
  EXPECT_EQ(1, tree.GetEnvelope());  // (-7) + (1+3+2+2) or (-6) + (3+2+2)
  EXPECT_EQ(2, tree.GetMaxEventWithEnvelopeGreaterThan(TypeParam(0)));
  EXPECT_EQ(4, tree.GetMaxEventWithEnvelopeGreaterThan(TypeParam(-1)));
  EXPECT_EQ(0, tree.GetEnvelopeOf(0));
  EXPECT_EQ(1, tree.GetEnvelopeOf(1));
  EXPECT_EQ(1, tree.GetEnvelopeOf(2));
  EXPECT_EQ(0, tree.GetEnvelopeOf(3));
  EXPECT_EQ(0, tree.GetEnvelopeOf(4));
}

TYPED_TEST(ThetaTreeTest, EnvelopeOpt) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  int event, optional_event;
  TypeParam energy_max;

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(0), energy[3]);
  tree.AddOrUpdateEvent(4, envelope[4], energy[4], energy[4]);
  EXPECT_EQ(1, tree.GetOptionalEnvelope());

  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(0), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(3, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);

  tree.RemoveEvent(4);
  tree.AddOrUpdateEvent(2, envelope[2], energy[2], energy[2]);
  EXPECT_EQ(0, tree.GetOptionalEnvelope());
  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(-1), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(2, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);
  EXPECT_EQ(-4, tree.GetEnvelopeOf(0));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(1));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(2));
}

TYPED_TEST(ThetaTreeTest, EnvelopeOptWithAddOptional) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  int event, optional_event;
  TypeParam energy_max;

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  tree.AddOrUpdateEvent(4, envelope[4], energy[4], energy[4]);
  EXPECT_EQ(1, tree.GetOptionalEnvelope());

  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(0), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(3, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);

  tree.RemoveEvent(4);
  tree.AddOrUpdateEvent(2, envelope[2], energy[2], energy[2]);
  EXPECT_EQ(0, tree.GetOptionalEnvelope());
  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(-1), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(2, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);
  EXPECT_EQ(-4, tree.GetEnvelopeOf(0));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(1));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(2));
}

TYPED_TEST(ThetaTreeTest, AddingAndGettingOptionalEvents) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({0, 3, 4, 6, 8});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  EXPECT_EQ(4, tree.GetEnvelope());

  // Even with 0 energy, standard update takes task 3's envelope into account.
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(0), energy[3]);
  EXPECT_EQ(6, tree.GetEnvelope());
  EXPECT_EQ(9, tree.GetOptionalEnvelope());
  tree.RemoveEvent(3);

  // Changing task 3 to optional makes it disappear from GetEnvelope().
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  EXPECT_EQ(4, tree.GetEnvelope());  // Same as before adding task 3.
  EXPECT_EQ(9, tree.GetOptionalEnvelope());

  // Changing task 3 to optional changes its optional values.
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(1), TypeParam(9));
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  EXPECT_EQ(4, tree.GetEnvelope());
  EXPECT_EQ(9, tree.GetOptionalEnvelope());
}

TYPED_TEST(ThetaTreeTest, RemoveAndDelayedAddOrUpdateEventTest) {
  ThetaLambdaTree<TypeParam> tree;
  // The tree encoding is tricky, check that RecomputeTreeForDelayedOperations()
  // works for all values from a power of two until the next.
  for (int num_events = 4; num_events < 8; ++num_events) {
    tree.Reset(num_events);
    std::vector<TypeParam> envelope;
    std::vector<TypeParam> energy;
    // Event start envelope = event, energy min = 2, energy max = 3
    for (int event = 0; event < num_events; ++event) {
      envelope.push_back(TypeParam{event});
      energy.push_back(TypeParam{2});
    }
    EXPECT_EQ(tree.GetEnvelope(), IntegerTypeMinimumValue<TypeParam>());
    EXPECT_EQ(tree.GetOptionalEnvelope(), IntegerTypeMinimumValue<TypeParam>());
    // Envelope of events [0, i) is (0) + 2 * i.
    for (int event = 0; event < num_events; ++event) {
      tree.DelayedAddOrUpdateEvent(event, envelope[event], energy[event],
                                   energy[event] + 1);
      tree.RecomputeTreeForDelayedOperations();
      EXPECT_EQ(tree.GetEnvelope(), 2 * (event + 1));
      EXPECT_EQ(tree.GetOptionalEnvelope(), 2 * (event + 1) + 1);
    }
    // Envelope of events [i, n) is (n-1) + 2 + (n - i)
    for (int event = 0; event < num_events; ++event) {
      EXPECT_EQ(tree.GetEnvelope(), 2 * num_events - event);
      EXPECT_EQ(tree.GetOptionalEnvelope(), 2 * num_events - event + 1);
      tree.DelayedRemoveEvent(event);
      tree.RecomputeTreeForDelayedOperations();
    }
    EXPECT_EQ(tree.GetEnvelope(), IntegerTypeMinimumValue<TypeParam>());
    EXPECT_EQ(tree.GetOptionalEnvelope(), IntegerTypeMinimumValue<TypeParam>());
  }
}

TYPED_TEST(ThetaTreeTest, DelayedAddOrUpdateOptionalEventTest) {
  ThetaLambdaTree<TypeParam> tree;
  // The tree encoding is tricky, check that RecomputeTreeForDelayedOperations()
  // works for all values from a power of two until the next.
  for (int num_events = 4; num_events < 8; ++num_events) {
    tree.Reset(num_events);
    std::vector<TypeParam> envelope;
    std::vector<TypeParam> energy;
    // Event start envelope = event, event energy max = 2.
    for (int event = 0; event < num_events; ++event) {
      envelope.push_back(TypeParam{event});
      energy.push_back(TypeParam{2});
    }
    EXPECT_EQ(tree.GetEnvelope(), IntegerTypeMinimumValue<TypeParam>());
    EXPECT_EQ(tree.GetOptionalEnvelope(), IntegerTypeMinimumValue<TypeParam>());
    // Optional envelope of events [0, i) is i + 2.
    for (int event = 0; event < num_events; ++event) {
      tree.DelayedAddOrUpdateOptionalEvent(event, envelope[event],
                                           energy[event]);
      tree.RecomputeTreeForDelayedOperations();
      EXPECT_EQ(tree.GetEnvelope(), IntegerTypeMinimumValue<TypeParam>());
      EXPECT_EQ(tree.GetOptionalEnvelope(), event + 2);
    }
  }
}

static void BM_update(benchmark::State& state) {
  random_engine_t random_;
  const int size = state.range(0);
  const int num_updates = 4 * size;
  ThetaLambdaTree<IntegerValue> tree;
  std::uniform_int_distribution<int> event_dist(0, size - 1);
  std::uniform_int_distribution<int> enveloppe_dist(-10000, 10000);
  std::uniform_int_distribution<int> energy_dist(0, 10000);
  for (auto _ : state) {
    tree.Reset(size);
    for (int i = 0; i < num_updates; ++i) {
      const int event = event_dist(random_);
      const IntegerValue enveloppe(enveloppe_dist(random_));
      const IntegerValue energy1(energy_dist(random_));
      const IntegerValue energy2(energy_dist(random_));
      tree.AddOrUpdateEvent(event, enveloppe, std::min(energy1, energy2),
                            std::max(energy1, energy2));
    }
  }
  // Number of updates.
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          num_updates);
}

// Note that we didn't pick only power of two
BENCHMARK(BM_update)->Arg(10)->Arg(20)->Arg(64)->Arg(100)->Arg(256)->Arg(800);

static void BM_delayed_update(benchmark::State& state) {
  random_engine_t random_;
  const int size = state.range(0);
  const int num_updates = 4 * size;
  ThetaLambdaTree<IntegerValue> tree;
  std::uniform_int_distribution<int> event_dist(0, size - 1);
  std::uniform_int_distribution<int> enveloppe_dist(-10000, 10000);
  std::uniform_int_distribution<int> energy_dist(0, 10000);
  for (auto _ : state) {
    tree.Reset(size);
    for (int i = 0; i < num_updates; ++i) {
      const int event = event_dist(random_);
      const IntegerValue enveloppe(enveloppe_dist(random_));
      const IntegerValue energy1(energy_dist(random_));
      const IntegerValue energy2(energy_dist(random_));
      tree.DelayedAddOrUpdateEvent(event, enveloppe, std::min(energy1, energy2),
                                   std::max(energy1, energy2));
    }
    tree.RecomputeTreeForDelayedOperations();
  }
  // Number of updates.
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          num_updates);
}

// Note that we didn't pick only power of two
BENCHMARK(BM_delayed_update)
    ->Arg(10)
    ->Arg(20)
    ->Arg(64)
    ->Arg(100)
    ->Arg(256)
    ->Arg(800);

}  // namespace
}  // namespace sat
}  // namespace operations_research
