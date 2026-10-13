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

#include "ortools/routing/search.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/simulated_clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/timestamp.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/filters.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/routing_test_utils.h"
#include "ortools/routing/types.h"
#include "ortools/routing/utils.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/testing_utils.h"

namespace operations_research::routing {
namespace {

using ::testing::UnorderedElementsAreArray;
using Attributes = Model::ResourceGroup::Attributes;
using ResourceGroup = Model::ResourceGroup;
using VisitTypePolicy = Model::VisitTypePolicy;

class InsertionSequenceGeneratorTest : public ::testing::Test {
 protected:
  using Insertion = InsertionSequenceContainer::Insertion;
  using InsertionSequence = InsertionSequenceContainer::InsertionSequence;
  void GenerateInsertions(int pickup, int delivery, int vehicle) {
    std::vector<bool> is_pickup;
    std::vector<bool> is_delivery;
    SetPathIsPickupDeliveryUntil(delivery, &is_pickup, &is_delivery);
    InsertionSequenceGenerator generator;
    generator.AppendPickupDeliveryMultitourInsertions(
        pickup, delivery, vehicle, path_, is_pickup, is_delivery, sequences_);
  }
  // Sets is_pickup and is_delivery for nodes on the path_.
  // For every node on the path_:
  // - is_pickup is false, except for even values in [2, max_delivery]
  // - is_delivery is false, except for odd values in [2, max_delivery]
  void SetPathIsPickupDeliveryUntil(int max_delivery,
                                    std::vector<bool>* is_pickup,
                                    std::vector<bool>* is_delivery) {
    is_pickup->clear();
    is_delivery->clear();
    for (int64_t node : path_) {
      is_pickup->push_back(node >= 2 && node <= max_delivery && node % 2 == 0);
      is_delivery->push_back(node >= 2 && node <= max_delivery &&
                             node % 2 != 0);
    }
  }

  // Maps a vector of insertions to a vector of insert_pickup_after,
  // insert_delivery_after pairs.
  std::vector<std::vector<Insertion>> ExtractInsertionSequences() {
    std::vector<std::vector<Insertion>> insertion_sequences;
    for (const auto& sequence : sequences_) {
      EXPECT_EQ(sequence.Size(), 2);
      insertion_sequences.push_back({});
      for (const auto& insertion : sequence) {
        insertion_sequences.back().push_back(insertion);
      }
    }
    return insertion_sequences;
  }

  void CheckEverySequenceVehicleIs(int vehicle) {
    for (const auto& sequence : sequences_) {
      EXPECT_EQ(sequence.Vehicle(), vehicle);
    }
  }

  void CheckEverySequenceSizeIs(size_t size) {
    for (const auto& sequence : sequences_) {
      EXPECT_EQ(sequence.Size(), size);
    }
  }

  void CheckEverySequenceCostIs(int64_t cost) {
    for (const auto& sequence : sequences_) {
      EXPECT_EQ(sequence.Cost(), cost);
    }
  }

  std::vector<int> path_;
  InsertionSequenceContainer sequences_;
};

TEST_F(InsertionSequenceGeneratorTest, PickupDeliveryMultitourEmptyPath) {
  // Initial path has shape se
  path_ = {0, 1};
  GenerateInsertions(2, 3, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 2}, {.pred = 2, .node = 3}},  // spde
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest, PickupDeliveryMultitourOnePairPath) {
  // Initial path has shape s()e
  path_ = {0, 2, 3, 1};
  GenerateInsertions(4, 5, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 4}, {.pred = 4, .node = 5}},  // spd()e
      {{.pred = 0, .node = 4}, {.pred = 2, .node = 5}},  // sp(d)e
      {{.pred = 2, .node = 4}, {.pred = 4, .node = 5}},  // s(pd)e
      {{.pred = 2, .node = 4}, {.pred = 3, .node = 5}},  // s(p)de
      {{.pred = 3, .node = 4}, {.pred = 4, .node = 5}},  // s()pde
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest, PickupDeliveryMultitourTwoNestedPair) {
  // Initial path has shape s(())e
  path_ = {0, 2, 4, 5, 3, 1};
  GenerateInsertions(6, 7, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 6}, {.pred = 6, .node = 7}},  //  spd(())e
      {{.pred = 0, .node = 6}, {.pred = 4, .node = 7}},  //  sp((d))e
      {{.pred = 2, .node = 6}, {.pred = 4, .node = 7}},  //  s(p(d))e
      {{.pred = 4, .node = 6}, {.pred = 6, .node = 7}},  //  s((pd))e
      {{.pred = 4, .node = 6}, {.pred = 5, .node = 7}},  //  s((p)d)e
      {{.pred = 4, .node = 6}, {.pred = 3, .node = 7}},  //  s((p))de
      {{.pred = 3, .node = 6}, {.pred = 6, .node = 7}},  //  s(())pde
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest,
       PickupDeliveryMultitourTwoSuccessivePairs) {
  // Initial path has shape s()()e
  path_ = {0, 2, 3, 4, 5, 1};
  GenerateInsertions(6, 7, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 6}, {.pred = 6, .node = 7}},  // spd()()e
      {{.pred = 0, .node = 6}, {.pred = 2, .node = 7}},  // sp(d)()e
      {{.pred = 2, .node = 6}, {.pred = 6, .node = 7}},  // s(pd)()e
      {{.pred = 3, .node = 6}, {.pred = 6, .node = 7}},  // s()pd()e
      {{.pred = 3, .node = 6}, {.pred = 4, .node = 7}},  // s()p(d)e
      {{.pred = 2, .node = 6}, {.pred = 3, .node = 7}},  // s(p)d()e
      {{.pred = 4, .node = 6}, {.pred = 6, .node = 7}},  // s()(pd)e
      {{.pred = 5, .node = 6}, {.pred = 6, .node = 7}},  // s()()pde
      {{.pred = 4, .node = 6}, {.pred = 5, .node = 7}},  // s()(p)de
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest,
       PickupDeliveryMultitourComplexPnDMultitour) {
  // Initial path has shape s(())()e. We try to insert p, d.
  path_ = {0, 2, 4, 5, 3, 6, 7, 1};
  GenerateInsertions(8, 9, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 8}, {.pred = 8, .node = 9}},  // spd(())()e
      {{.pred = 0, .node = 8}, {.pred = 4, .node = 9}},  // sp((d))()e
      {{.pred = 2, .node = 8}, {.pred = 4, .node = 9}},  // s(p(d))()e
      {{.pred = 4, .node = 8}, {.pred = 8, .node = 9}},  // s((pd))()e
      {{.pred = 4, .node = 8}, {.pred = 5, .node = 9}},  // s((p)d)()e
      {{.pred = 3, .node = 8}, {.pred = 8, .node = 9}},  // s(())pd()e
      {{.pred = 3, .node = 8}, {.pred = 6, .node = 9}},  // s(())p(d)e
      {{.pred = 4, .node = 8}, {.pred = 3, .node = 9}},  // s((p))d()e
      {{.pred = 6, .node = 8}, {.pred = 8, .node = 9}},  // s(())(pd)e
      {{.pred = 7, .node = 8}, {.pred = 8, .node = 9}},  // s(())()pde
      {{.pred = 6, .node = 8}, {.pred = 7, .node = 9}},  // s(())(p)de
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest, Unpaired) {
  // Initial path has shape s..e
  path_ = {0, 12, 13, 1};
  GenerateInsertions(2, 3, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 2}, {.pred = 2, .node = 3}},    // spd..e
      {{.pred = 0, .node = 2}, {.pred = 13, .node = 3}},   // sp..de
      {{.pred = 12, .node = 2}, {.pred = 2, .node = 3}},   // s.pd.e
      {{.pred = 12, .node = 2}, {.pred = 13, .node = 3}},  // s.p.de
      {{.pred = 0, .node = 2}, {.pred = 12, .node = 3}},   // sp.d.e
      {{.pred = 13, .node = 2}, {.pred = 2, .node = 3}},   // s..pde
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest, PickupDeliveryMultitourMixedOnePair) {
  // Initial path has shape s..(..)..e
  path_ = {0, 10, 11, 2, 12, 13, 3, 14, 15, 1};
  GenerateInsertions(4, 5, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 4}, {.pred = 4, .node = 5}},    // spd..(..)..e
      {{.pred = 0, .node = 4}, {.pred = 13, .node = 5}},   // sp..(..d)..e
      {{.pred = 0, .node = 4}, {.pred = 11, .node = 5}},   // sp..d(..)..e
      {{.pred = 10, .node = 4}, {.pred = 4, .node = 5}},   // s.pd.(..)..e
      {{.pred = 10, .node = 4}, {.pred = 13, .node = 5}},  // s.p.(..d)..e
      {{.pred = 10, .node = 4}, {.pred = 11, .node = 5}},  // s.p.d(..)..e
      {{.pred = 0, .node = 4}, {.pred = 10, .node = 5}},   // sp.d.(..)..e
      {{.pred = 11, .node = 4}, {.pred = 4, .node = 5}},   // s..pd(..)..e
      {{.pred = 11, .node = 4}, {.pred = 13, .node = 5}},  // s..p(..d)..e
      {{.pred = 2, .node = 4}, {.pred = 4, .node = 5}},    // s..(pd..)..e
      {{.pred = 2, .node = 4}, {.pred = 13, .node = 5}},   // s..(p..d)..e
      {{.pred = 12, .node = 4}, {.pred = 4, .node = 5}},   // s..(.pd.)..e
      {{.pred = 12, .node = 4}, {.pred = 13, .node = 5}},  // s..(.p.d)..e
      {{.pred = 2, .node = 4}, {.pred = 12, .node = 5}},   // s..(p.d.)..e
      {{.pred = 13, .node = 4}, {.pred = 4, .node = 5}},   // s..(..pd)..e
      {{.pred = 3, .node = 4}, {.pred = 4, .node = 5}},    // s..(..)pd..e
      {{.pred = 3, .node = 4}, {.pred = 15, .node = 5}},   // s..(..)p..de
      {{.pred = 2, .node = 4}, {.pred = 3, .node = 5}},    // s..(p..)d..e
      {{.pred = 14, .node = 4}, {.pred = 4, .node = 5}},   // s..(..).pd.e
      {{.pred = 14, .node = 4}, {.pred = 15, .node = 5}},  // s..(..).p.de
      {{.pred = 3, .node = 4}, {.pred = 14, .node = 5}},   // s..(..)p.d.e
      {{.pred = 2, .node = 4}, {.pred = 14, .node = 5}},   // s..(p..).d.e
      {{.pred = 15, .node = 4}, {.pred = 4, .node = 5}},   // s..(..)..pde
      {{.pred = 2, .node = 4}, {.pred = 15, .node = 5}},   // s..(p..)..de
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest,
       PickupDeliveryMultitourMixedIncreasingSymmetry) {
  // Initial path has shape s.((.).).((.).).e
  path_ = {0, 20, 2, 4, 21, 3, 22, 5, 23, 6, 8, 24, 7, 25, 9, 26, 1};
  GenerateInsertions(10, 11, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 10},
       {.pred = 10, .node = 11}},  // spd.((.).).((.).).e
      {{.pred = 0, .node = 10},
       {.pred = 21, .node = 11}},  // sp.((.d).).((.).).e
      {{.pred = 0, .node = 10},
       {.pred = 20, .node = 11}},  // sp.d((.).).((.).).e
      {{.pred = 20, .node = 10},
       {.pred = 10, .node = 11}},  // s.pd((.).).((.).).e
      {{.pred = 20, .node = 10},
       {.pred = 21, .node = 11}},  // s.p((.d).).((.).).e
      {{.pred = 2, .node = 10},
       {.pred = 21, .node = 11}},  // s.(p(.d).).((.).).e
      {{.pred = 4, .node = 10},
       {.pred = 10, .node = 11}},  // s.((pd.).).((.).).e
      {{.pred = 4, .node = 10},
       {.pred = 21, .node = 11}},  // s.((p.d).).((.).).e
      {{.pred = 21, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.pd).).((.).).e
      {{.pred = 4, .node = 10},
       {.pred = 3, .node = 11}},  // s.((p.)d.).((.).).e
      {{.pred = 4, .node = 10},
       {.pred = 22, .node = 11}},  // s.((p.).d).((.).).e
      {{.pred = 5, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).)pd.((.).).e
      {{.pred = 5, .node = 10},
       {.pred = 24, .node = 11}},  // s.((.).)p.((.d).).e
      {{.pred = 4, .node = 10},
       {.pred = 5, .node = 11}},  // s.((p.).)d.((.).).e
      {{.pred = 5, .node = 10},
       {.pred = 23, .node = 11}},  // s.((.).)p.d((.).).e
      {{.pred = 23, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).).pd((.).).e
      {{.pred = 23, .node = 10},
       {.pred = 24, .node = 11}},  // s.((.).).p((.d).).e
      {{.pred = 4, .node = 10},
       {.pred = 23, .node = 11}},  // s.((p.).).d((.).).e
      {{.pred = 6, .node = 10},
       {.pred = 24, .node = 11}},  // s.((.).).(p(.d).).e
      {{.pred = 8, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).).((pd.).).e
      {{.pred = 8, .node = 10},
       {.pred = 24, .node = 11}},  // s.((.).).((p.d).).e
      {{.pred = 24, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).).((.pd).).e
      {{.pred = 8, .node = 10},
       {.pred = 7, .node = 11}},  // s.((.).).((p.)d.).e
      {{.pred = 8, .node = 10},
       {.pred = 25, .node = 11}},  // s.((.).).((p.).d).e
      {{.pred = 9, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).).((.).)pd.e
      {{.pred = 8, .node = 10},
       {.pred = 9, .node = 11}},  // s.((.).).((p.).)d.e
      {{.pred = 9, .node = 10},
       {.pred = 26, .node = 11}},  // s.((.).).((.).)p.de
      {{.pred = 26, .node = 10},
       {.pred = 10, .node = 11}},  // s.((.).).((.).).pde
      {{.pred = 8, .node = 10},
       {.pred = 26, .node = 11}},  // s.((.).).((p.).).de
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

TEST_F(InsertionSequenceGeneratorTest,
       PickupDeliveryMultitourMixedDecreasingSymmetry) {
  // Initial path has shape s.(.(.)).(.(.)).e
  path_ = {0, 20, 2, 21, 4, 22, 3, 5, 23, 6, 24, 8, 25, 7, 9, 26, 1};
  GenerateInsertions(10, 11, 42);
  CheckEverySequenceSizeIs(2);
  CheckEverySequenceVehicleIs(42);
  CheckEverySequenceCostIs(0);
  std::vector<std::vector<Insertion>> expected_positions_pairs = {
      {{.pred = 0, .node = 10},
       {.pred = 10, .node = 11}},  // spd.(.(.)).(.(.)).e
      {{.pred = 0, .node = 10},
       {.pred = 22, .node = 11}},  // sp.(.(.d)).(.(.)).e
      {{.pred = 0, .node = 10},
       {.pred = 20, .node = 11}},  // sp.d(.(.)).(.(.)).e
      {{.pred = 20, .node = 10},
       {.pred = 10, .node = 11}},  // s.pd(.(.)).(.(.)).e
      {{.pred = 20, .node = 10},
       {.pred = 22, .node = 11}},  // s.p(.(.d)).(.(.)).e
      {{.pred = 2, .node = 10},
       {.pred = 22, .node = 11}},  // s.(p.(.d)).(.(.)).e
      {{.pred = 21, .node = 10},
       {.pred = 22, .node = 11}},  // s.(.p(.d)).(.(.)).e
      {{.pred = 4, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(pd.)).(.(.)).e
      {{.pred = 4, .node = 10},
       {.pred = 22, .node = 11}},  // s.(.(p.d)).(.(.)).e
      {{.pred = 22, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.pd)).(.(.)).e
      {{.pred = 4, .node = 10},
       {.pred = 3, .node = 11}},  // s.(.(p.)d).(.(.)).e
      {{.pred = 5, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.))pd.(.(.)).e
      {{.pred = 5, .node = 10},
       {.pred = 25, .node = 11}},  // s.(.(.))p.(.(.d)).e
      {{.pred = 5, .node = 10},
       {.pred = 23, .node = 11}},  // s.(.(.))p.d(.(.)).e
      {{.pred = 4, .node = 10},
       {.pred = 5, .node = 11}},  // s.(.(p.))d.(.(.)).e
      {{.pred = 23, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.)).pd(.(.)).e
      {{.pred = 23, .node = 10},
       {.pred = 25, .node = 11}},  // s.(.(.)).p(.(.d)).e
      {{.pred = 4, .node = 10},
       {.pred = 23, .node = 11}},  // s.(.(p.)).d(.(.)).e
      {{.pred = 6, .node = 10},
       {.pred = 25, .node = 11}},  // s.(.(.)).(p.(.d)).e
      {{.pred = 24, .node = 10},
       {.pred = 25, .node = 11}},  // s.(.(.)).(.p(.d)).e
      {{.pred = 8, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.)).(.(pd.)).e
      {{.pred = 8, .node = 10},
       {.pred = 25, .node = 11}},  // s.(.(.)).(.(p.d)).e
      {{.pred = 25, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.)).(.(.pd)).e
      {{.pred = 8, .node = 10},
       {.pred = 7, .node = 11}},  // s.(.(.)).(.(p.)d).e
      {{.pred = 9, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.)).(.(.))pd.e
      {{.pred = 9, .node = 10},
       {.pred = 26, .node = 11}},  // s.(.(.)).(.(.))p.de
      {{.pred = 8, .node = 10},
       {.pred = 9, .node = 11}},  // s.(.(.)).(.(p.))d.e
      {{.pred = 26, .node = 10},
       {.pred = 10, .node = 11}},  // s.(.(.)).(.(.)).pde
      {{.pred = 8, .node = 10},
       {.pred = 26, .node = 11}},  // s.(.(.)).(.(p.)).de
  };
  EXPECT_THAT(ExtractInsertionSequences(),
              UnorderedElementsAreArray(expected_positions_pairs));
}

// Parameters: has_evaluator, pair insertion strategy, has BinaryCapacities.
class LocalCheapestInsertionWithBinCapacitiesTest
    : public ::testing::TestWithParam<std::tuple<
          bool, bool, LocalCheapestInsertionParameters::PairInsertionStrategy,
          bool, bool>> {
 protected:
  const int kNumNodes = 12;
  const int kNumVehicles = 3;
  void SetUp() override {
    // Node indexing:
    // start1 ... startV, internal_nodes, end1 ... endV.
    std::vector<NodeIndex> vehicle_starts(kNumVehicles);
    std::vector<NodeIndex> vehicle_ends(kNumVehicles);
    for (int v = 0; v < kNumVehicles; ++v) {
      vehicle_starts[v] = NodeIndex(v);
      vehicle_ends[v] = NodeIndex(kNumNodes - kNumVehicles + v);
    }
    manager_ = std::make_unique<IndexManager>(kNumNodes, kNumVehicles,
                                              vehicle_starts, vehicle_ends);
    model_ = std::make_unique<Model>(*manager_);
  }

  std::unique_ptr<IndexManager> manager_;
  std::unique_ptr<Model> model_;
};

TEST_P(LocalCheapestInsertionWithBinCapacitiesTest, PairsHardLimits) {
  const bool has_evaluator = std::get<0>(GetParam());
  const bool use_node_pairs = std::get<1>(GetParam());
  const auto pair_insertion_strategy = std::get<2>(GetParam());
  const bool has_bin_capacities = std::get<3>(GetParam());
  const bool use_soft_cost_only = std::get<4>(GetParam());
  if (!has_evaluator && use_soft_cost_only) return;
  // All nodes are at the same place, 1 away from the depot.
  // This makes insertion avoid opening new routes.
  std::function<int64_t(int64_t, int64_t, int64_t)> evaluator = nullptr;
  if (has_evaluator) {
    evaluator = [model = model_.get()](int64_t src, int64_t dst, int64_t) {
      return (model->IsStart(src) || model->IsEnd(dst)) ? 1 : 0;
    };
  }
  // Exercises pair insertions.
  if (use_node_pairs) {
    model_->AddPickupAndDelivery(3, 4);
    model_->AddPickupAndDelivery(5, 6);
    model_->AddPickupAndDelivery(7, 8);
  }
  // Bin capacities allow 0 nodes on vehicle 0, 2 on vehicle 1, 4 on vehicle 2.
  // Without a BinCapacities, all nodes should be on the same vehicle.
  // With a BinCapacities, nodes have to be spread 0/2/4 between vehicles.
  std::unique_ptr<BinCapacities> bin_capacities = nullptr;
  std::vector<BinCapacities::LoadLimit> load_limit_per_bin(
      3, {.max_load = 100, .soft_max_load = 0, .cost_above_soft_max_load = 0});
  for (int v = 0; v < kNumVehicles; ++v) {
    if (use_soft_cost_only) {
      load_limit_per_bin[v].soft_max_load = 2 * v;
      load_limit_per_bin[v].cost_above_soft_max_load = 3;
    } else {
      load_limit_per_bin[v].max_load = 2 * v;
    }
  }
  if (has_bin_capacities) {
    bin_capacities = std::make_unique<BinCapacities>(kNumVehicles);
    bin_capacities->AddDimension([](int, int) { return 1; },
                                 load_limit_per_bin);
  }

  // Only cost filter: accept everything, only prefiltering forbids solutions.
  std::function<int64_t(int64_t, int64_t)> cost_function =
      [model = model_.get()](int64_t src, int64_t dst) -> int64_t {
    return (model->IsStart(src) || model->IsEnd(dst)) ? 1 : 0;
  };
  LocalSearchFilter* sum_objective = model_->solver()->MakeSumObjectiveFilter(
      model_->Nexts(), cost_function, Solver::LE);
  LocalSearchFilterManager filter_manager({sum_objective});

  LocalCheapestInsertionParameters lci_parameters;
  lci_parameters.set_pickup_delivery_strategy(pair_insertion_strategy);

  // Build a solution using only LCI, no local search.
  LocalCheapestInsertionFilteredHeuristic lci(
      model_.get(),
      /*stop_search=*/[]() { return false; },
      /*evaluator=*/evaluator, lci_parameters, &filter_manager,
      /*use_first_solution_hint=*/false, bin_capacities.get());
  model_->CloseModel();
  Assignment* solution = lci.BuildSolution();

  ASSERT_THAT(solution, ::testing::NotNull());
  // Count the number of visits on each vehicle.
  std::vector<int> visit_counts(kNumVehicles, 0);
  for (int v = 0; v < kNumVehicles; ++v) {
    for (int node = solution->Value(model_->NextVar(model_->Start(v)));
         !model_->IsEnd(node); node = solution->Value(model_->NextVar(node))) {
      ++visit_counts[v];
    }
  }

  if (has_bin_capacities) {
    // Nodes should be spread around vehicles according to their max_load.
    for (int v = 0; v < kNumVehicles; ++v) {
      const int64_t limit = use_soft_cost_only
                                ? load_limit_per_bin[v].soft_max_load
                                : load_limit_per_bin[v].max_load;
      EXPECT_EQ(visit_counts[v], limit);
    }
  } else {
    // The cost encourages using only one vehicle for all visits.
    bool some_vehicle_has_all_nodes = false;
    const int num_visits = kNumNodes - 2 * kNumVehicles;
    for (int v = 0; v < kNumVehicles; ++v) {
      some_vehicle_has_all_nodes |= visit_counts[v] == num_visits;
    }
    EXPECT_TRUE(some_vehicle_has_all_nodes);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllParameters, LocalCheapestInsertionWithBinCapacitiesTest,
    ::testing::Combine(
        ::testing::Bool(), ::testing::Bool(),
        ::testing::ValuesIn(
            google::protobuf::EnumerateEnumValues<
                LocalCheapestInsertionParameters::PairInsertionStrategy>()),
        ::testing::Bool(), ::testing::Bool()));

TEST(LocalCheapestInsertionTest, InsertPickupThenDelivery) {
  IndexManager manager(
      /*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(1, 2);
  std::function<int64_t(int64_t, int64_t)> cost_function =
      [](int64_t src, int64_t dst) -> int64_t {
    return src == 2 || dst == 2 ? 1 : 0;
  };
  LocalSearchFilter* sum_objective = model.solver()->MakeSumObjectiveFilter(
      model.Nexts(), cost_function, Solver::LE);
  LocalSearchFilterManager filter_manager({sum_objective});

  LocalCheapestInsertionParameters lci_parameters;
  lci_parameters.set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::BEST_PICKUP_THEN_BEST_DELIVERY);

  LocalCheapestInsertionFilteredHeuristic lci(
      &model,
      /*stop_search=*/[]() { return false; },
      /*evaluator=*/nullptr, lci_parameters, &filter_manager,
      /*use_first_solution_hint=*/false,
      /*bin_capacities=*/nullptr,
      /*optimize_on_insertion=*/nullptr);
  model.CloseModel();
  Assignment* solution = lci.BuildSolution();
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_NE(solution->Value(model.NextVar(1)), 1);
  EXPECT_NE(solution->Value(model.NextVar(2)), 2);
}

class LocalCheapestInsertionWithSecondarySolverTest
    : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override { use_pickup_and_deliveries_ = GetParam(); }
  void SetUpNodes(Model* model, int num_nodes) {
    if (use_pickup_and_deliveries_) {
      for (int i = 1; i + 1 < num_nodes; i += 2) {
        model->AddPickupAndDelivery(i, i + 1);
      }
    }
  }
  bool use_pickup_and_deliveries_;
};

TEST_P(LocalCheapestInsertionWithSecondarySolverTest, SecondaryFails) {
  IndexManager manager(
      /*num_nodes=*/11, /*num_vehicles=*/3, NodeIndex(0));
  Model model(manager);
  SetUpNodes(&model, manager.num_nodes());
  Model secondary_model(manager);
  SetUpNodes(&secondary_model, manager.num_nodes());
  RoutingSearchParameters secondary_parameters =
      DefaultRoutingSearchParameters();
  secondary_parameters.set_solution_limit(0);
  Model::SecondaryOptimizer secondary_optimizer(
      &secondary_model, &secondary_parameters, /*solve_period=*/10);

  std::function<int64_t(int64_t, int64_t)> cost_function =
      [&model](int64_t src, int64_t dst) -> int64_t {
    return (model.IsStart(src) || model.IsEnd(dst)) ? 1 : 0;
  };
  LocalSearchFilter* sum_objective = model.solver()->MakeSumObjectiveFilter(
      model.Nexts(), cost_function, Solver::LE);
  LocalSearchFilterManager filter_manager({sum_objective});

  LocalCheapestInsertionParameters lci_parameters;
  lci_parameters.set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::AUTOMATIC);

  LocalCheapestInsertionFilteredHeuristic lci(
      &model,
      /*stop_search=*/[]() { return false; },
      /*evaluator=*/nullptr, lci_parameters, &filter_manager,
      /*use_first_solution_hint=*/false,
      /*bin_capacities=*/nullptr,
      absl::bind_front(&Model::SecondaryOptimizer::Solve,
                       &secondary_optimizer));
  model.CloseModel();
  Assignment* solution = lci.BuildSolution();
  ASSERT_THAT(solution, ::testing::NotNull());
}

TEST_P(LocalCheapestInsertionWithSecondarySolverTest, SecondaryNoImprovement) {
  for (bool use_cost : {true, false}) {
    IndexManager manager(
        /*num_nodes=*/11, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    SetUpNodes(&model, manager.num_nodes());
    Model secondary_model(manager);
    SetUpNodes(&secondary_model, manager.num_nodes());
    RoutingSearchParameters secondary_parameters =
        DefaultRoutingSearchParameters();
    secondary_parameters.set_solution_limit(1);
    Model::SecondaryOptimizer secondary_optimizer(
        &secondary_model, &secondary_parameters, /*solve_period=*/10);

    std::function<int64_t(int64_t, int64_t)> cost_function1 =
        [&model](int64_t src, int64_t dst) -> int64_t {
      return (model.IsStart(src) || model.IsEnd(dst)) ? 1 : 0;
    };
    std::function<int64_t(int64_t, int64_t)> cost_function2 =
        [](int64_t src, int64_t dst) -> int64_t { return std::abs(src - dst); };
    if (use_cost) {
      model.SetArcCostEvaluatorOfAllVehicles(
          model.RegisterTransitCallback(cost_function1));
      secondary_model.SetArcCostEvaluatorOfAllVehicles(
          secondary_model.RegisterTransitCallback(cost_function2));
    }
    LocalSearchFilter* sum_objective = model.solver()->MakeSumObjectiveFilter(
        model.Nexts(), cost_function1, Solver::LE);
    LocalSearchFilterManager filter_manager({sum_objective});

    LocalCheapestInsertionParameters lci_parameters;
    lci_parameters.set_pickup_delivery_strategy(
        LocalCheapestInsertionParameters::AUTOMATIC);

    LocalCheapestInsertionFilteredHeuristic lci(
        &model,
        /*stop_search=*/[]() { return false; }, /*evaluator=*/nullptr,
        lci_parameters, &filter_manager,
        /*use_first_solution_hint=*/false,
        /*bin_capacities=*/nullptr,
        absl::bind_front(&Model::SecondaryOptimizer::Solve,
                         &secondary_optimizer));
    model.CloseModel();
    Assignment* solution = lci.BuildSolution();
    ASSERT_THAT(solution, ::testing::NotNull());
    EXPECT_EQ(secondary_model.solver()->accepted_neighbors(), 0);
  }
}

TEST_P(LocalCheapestInsertionWithSecondarySolverTest, SecondaryImprovement) {
  for (int solve_period : {1, 0}) {
    IndexManager manager(
        /*num_nodes=*/11, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    SetUpNodes(&model, manager.num_nodes());
    Model secondary_model(manager);
    SetUpNodes(&secondary_model, manager.num_nodes());
    RoutingSearchParameters secondary_parameters =
        DefaultRoutingSearchParameters();
    Model::SecondaryOptimizer secondary_optimizer(
        &secondary_model, &secondary_parameters, solve_period);

    std::function<int64_t(int64_t, int64_t)> cost_function1 =
        [&model](int64_t src, int64_t dst) -> int64_t {
      return (model.IsStart(src) || model.IsEnd(dst)) ? 1 : 0;
    };
    std::function<int64_t(int64_t, int64_t)> cost_function2 =
        [](int64_t src, int64_t dst) -> int64_t { return std::abs(src - dst); };
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback(cost_function1));
    secondary_model.SetArcCostEvaluatorOfAllVehicles(
        secondary_model.RegisterTransitCallback(cost_function2));
    LocalSearchFilter* sum_objective = model.solver()->MakeSumObjectiveFilter(
        model.Nexts(), cost_function1, Solver::LE);
    LocalSearchFilterManager filter_manager({sum_objective});

    LocalCheapestInsertionParameters lci_parameters;
    lci_parameters.set_pickup_delivery_strategy(
        LocalCheapestInsertionParameters::AUTOMATIC);

    LocalCheapestInsertionFilteredHeuristic lci(
        &model,
        /*stop_search=*/[]() { return false; }, /*evaluator=*/nullptr,
        lci_parameters, &filter_manager,
        /*use_first_solution_hint=*/false,
        /*bin_capacities=*/nullptr,
        absl::bind_front(&Model::SecondaryOptimizer::Solve,
                         &secondary_optimizer));
    model.CloseModel();
    Assignment* solution = lci.BuildSolution();
    ASSERT_THAT(solution, ::testing::NotNull());
    if (solve_period == 1) {
      EXPECT_GT(secondary_model.solver()->accepted_neighbors(), 1);
    } else {
      EXPECT_EQ(secondary_model.solver()->accepted_neighbors(), 0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AllParameters,
                         LocalCheapestInsertionWithSecondarySolverTest,
                         ::testing::Bool());

class LocalCheapestInsertionWithHintTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(LocalCheapestInsertionWithHintTest, FullHint) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  // Hint: start -> 1 -> 2 -> 3 -> 4 -> end
  Assignment* hint = model.solver()->MakeAssignment();
  hint->Add(model.NextVar(model.Start(0)))->SetValue(1);
  hint->Add(model.NextVar(1))->SetValue(2);
  hint->Add(model.NextVar(2))->SetValue(3);
  hint->Add(model.NextVar(3))->SetValue(4);
  hint->Add(model.NextVar(4))->SetValue(model.End(0));
  if (use_hint) model.SetFirstSolutionHint(hint);
  // Penalize hint to see if it's chosen anyways.
  const int cost =
      model.RegisterTransitCallback([hint, &model](int64_t i, int64_t j) {
        if (i < model.Nexts().size() && hint->Contains(model.NextVar(i)) &&
            hint->Value(model.NextVar(i)) == j) {
          return 10;
        }
        return 0;
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      use_evaluator ? FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
                    : FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
  parameters.set_solution_limit(1);
  const Assignment* solution = model.SolveWithParameters(parameters);
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_EQ(solution->ObjectiveValue(), use_hint ? 5 * 10 : 0);
}

TEST_P(LocalCheapestInsertionWithHintTest, FullHintIgnored) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  // Hint: start -> 1 -> 2 -> 3 -> 4 -> end
  Assignment* hint = model.solver()->MakeAssignment();
  hint->Add(model.NextVar(model.Start(0)))->SetValue(1);
  hint->Add(model.NextVar(1))->SetValue(2);
  hint->Add(model.NextVar(2))->SetValue(3);
  hint->Add(model.NextVar(3))->SetValue(4);
  hint->Add(model.NextVar(4))->SetValue(model.End(0));
  if (use_hint) model.SetFirstSolutionHint(hint);
  // Penalize hint to see if it's chosen anyways.
  const int cost =
      model.RegisterTransitCallback([hint, &model](int64_t i, int64_t j) {
        if (i < model.Nexts().size() && hint->Contains(model.NextVar(i)) &&
            hint->Value(model.NextVar(i)) == j) {
          return 10;
        }
        return 0;
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  std::function<int64_t(int64_t, int64_t, int64_t)> evaluator;
  if (use_evaluator) {
    evaluator = [&model](int64_t i, int64_t j, int64_t vehicle) {
      return model.GetArcCostForVehicle(i, j, vehicle);
    };
  }

  LocalCheapestInsertionParameters lci_parameters;
  lci_parameters.set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::BEST_PICKUP_DELIVERY_PAIR);

  LocalCheapestInsertionFilteredHeuristic lci(
      &model,
      /*stop_search=*/[]() { return false; }, evaluator, lci_parameters,
      nullptr,
      /*use_first_solution_hint=*/false);
  model.CloseModel();
  Assignment* solution = lci.BuildSolution();
  ASSERT_THAT(solution, ::testing::NotNull());
  // The hint is ignored so the best solution is returned.
  for (IntVar* next_var : model.Nexts()) {
    EXPECT_NE(solution->Value(next_var), hint->Value(next_var));
  }
}

TEST_P(LocalCheapestInsertionWithHintTest, PartialHint) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  // Hint: 4 -> 1
  Assignment* hint = model.solver()->MakeAssignment();
  hint->Add(model.NextVar(4))->SetValue(1);
  if (use_hint) model.SetFirstSolutionHint(hint);
  // Penalize hint to see if it's chosen anyways.
  const int cost =
      model.RegisterTransitCallback([hint, &model](int64_t i, int64_t j) {
        if (i < model.Nexts().size() && hint->Contains(model.NextVar(i)) &&
            hint->Value(model.NextVar(i)) == j) {
          return 10;
        }
        return 0;
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      use_evaluator ? FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
                    : FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
  parameters.set_solution_limit(1);
  const Assignment* solution = model.SolveWithParameters(parameters);
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_EQ(solution->ObjectiveValue(), use_hint ? 1 * 10 : 0)
      << solution->DebugString();
}

TEST_P(LocalCheapestInsertionWithHintTest, PartialHintFromStart) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/2, NodeIndex(0));
  Model model(manager);
  // Hint: start -> 3
  Assignment* hint = model.solver()->MakeAssignment();
  hint->Add(model.NextVar(model.Start(0)))->SetValue(3);
  if (use_hint) model.SetFirstSolutionHint(hint);
  // Penalize hint to see if it's chosen anyways.
  const int cost =
      model.RegisterTransitCallback([hint, &model](int64_t i, int64_t j) {
        if (i < model.Nexts().size() && hint->Contains(model.NextVar(i)) &&
            hint->Value(model.NextVar(i)) == j) {
          return 10;
        }
        return 0;
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  // Limit the number of stops to 3 on each vehicle. If node 3 is inserted too
  // late, then it will end up on vehicle 1.
  model.AddConstantDimension(1, 3, /*fix_start_cumul_to_zero=*/true, "count");
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      use_evaluator ? FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
                    : FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
  parameters.set_solution_limit(1);
  const Assignment* solution = model.SolveWithParameters(parameters);
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_EQ(solution->ObjectiveValue(), use_hint ? 1 * 10 : 0);
}

TEST_P(LocalCheapestInsertionWithHintTest, FullHintWithPickupAndDelivery) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  for (auto strategy : google::protobuf::EnumerateEnumValues<
           LocalCheapestInsertionParameters::PairInsertionStrategy>()) {
    IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
    Model model(manager);
    model.AddPickupAndDelivery(1, 3);
    model.AddPickupAndDelivery(2, 4);
    // Hint: start -> 1 -> 2 -> 3 -> 4 -> end
    Assignment* hint = model.solver()->MakeAssignment();
    hint->Add(model.NextVar(model.Start(0)))->SetValue(1);
    hint->Add(model.NextVar(1))->SetValue(2);
    hint->Add(model.NextVar(2))->SetValue(3);
    hint->Add(model.NextVar(3))->SetValue(4);
    hint->Add(model.NextVar(4))->SetValue(model.End(0));
    if (use_hint) model.SetFirstSolutionHint(hint);
    // Penalize hint to see if it's chosen anyways.
    const int cost =
        model.RegisterTransitCallback([hint, &model](int64_t i, int64_t j) {
          if (i < model.Nexts().size() && hint->Contains(model.NextVar(i)) &&
              hint->Value(model.NextVar(i)) == j) {
            return 10;
          }
          return 0;
        });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_evaluator) {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION);
      parameters.mutable_local_cheapest_insertion_parameters()
          ->set_pickup_delivery_strategy(strategy);
    } else {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
      parameters.mutable_local_cheapest_cost_insertion_parameters()
          ->set_pickup_delivery_strategy(strategy);
    }
    parameters.set_solution_limit(1);
    const Assignment* solution = model.SolveWithParameters(parameters);
    ASSERT_THAT(solution, ::testing::NotNull());
    EXPECT_EQ(solution->ObjectiveValue(), use_hint ? 5 * 10 : 0);
  }
}

TEST_P(LocalCheapestInsertionWithHintTest, EmptyHint) {
  const bool use_hint = std::get<0>(GetParam());
  const bool use_evaluator = std::get<1>(GetParam());
  IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  Assignment* hint = model.solver()->MakeAssignment();
  if (use_hint) model.SetFirstSolutionHint(hint);
  // Penalize hint to see if it's chosen anyways.
  const int cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 0; });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      use_evaluator ? FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
                    : FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
  parameters.set_solution_limit(1);
  const Assignment* solution = model.SolveWithParameters(parameters);
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_EQ(solution->ObjectiveValue(), 0);
}

INSTANTIATE_TEST_SUITE_P(AllParameters, LocalCheapestInsertionWithHintTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST(SolveWithAlternativeSolversTest, NegativeMaxIterations) {
  IndexManager manager(
      /*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const int cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  const Assignment* solution =
      SolveWithAlternativeSolvers(&model, {}, parameters, -1);
  ASSERT_THAT(solution, ::testing::IsNull());
}

TEST(SolveWithAlternativeSolversTest, NoSolution) {
  IndexManager manager(
      /*num_nodes=*/3, /*num_vehicles=*/2, NodeIndex(0));
  Model model(manager);
  const int cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // This will fail since nodes are mandatory.
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  parameters.set_fallback_to_cp_sat_size_threshold(0);
  const Assignment* solution =
      SolveWithAlternativeSolvers(&model, {}, parameters, 1);
  ASSERT_THAT(solution, ::testing::IsNull());
}

TEST(SolveWithAlternativeSolversTest, NoAlternatives) {
  for (int max_iterations = 0; max_iterations < 10; ++max_iterations) {
    IndexManager manager(
        /*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
    Model model(manager);
    const int cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    const Assignment* solution =
        SolveWithAlternativeSolvers(&model, {}, parameters, max_iterations);
    ASSERT_THAT(solution, ::testing::NotNull());
    EXPECT_EQ(solution->ObjectiveValue(), 3);
  }
}

using SolveWithAlternativeSolversAndMetaheuristicsTest =
    ::testing::TestWithParam<bool>;

INSTANTIATE_TEST_SUITE_P(
    SolveWithAlternativeSolversAndMetaheuristicsTestInstance,
    SolveWithAlternativeSolversAndMetaheuristicsTest, ::testing::Bool());

TEST_P(SolveWithAlternativeSolversAndMetaheuristicsTest, WithAlternatives) {
  const bool use_metaheuristic = GetParam();
  for (int max_iterations = 0; max_iterations < 10; ++max_iterations) {
    IndexManager manager(
        /*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
    Model model(manager);
    // Prefer high indices first.
    const int cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return i >= j ? 1 : 10; });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    int count = 0;
    model.AddAtSolutionCallback([&count]() { ++count; });
    Model model1(manager);
    const int cost1 = model1.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model1.SetArcCostEvaluatorOfAllVehicles(cost1);
    int count1 = 0;
    model1.AddAtSolutionCallback([&count1]() { ++count1; });
    Model model2(manager);
    const int cost2 =
        model2.RegisterTransitCallback([](int64_t, int64_t) { return 2; });
    model2.SetArcCostEvaluatorOfAllVehicles(cost2);
    int count2 = 0;
    model2.AddAtSolutionCallback([&count2]() { ++count2; });
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_metaheuristic) {
      parameters.set_local_search_metaheuristic(
          LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
      parameters.set_solution_limit(3);
    }
    const Assignment* solution = SolveWithAlternativeSolvers(
        &model, {&model1, &model2}, parameters, max_iterations);
    ASSERT_THAT(solution, ::testing::NotNull());
    // Solution: 0 > 4 > 3 > 2 > 1 > 5, resulting in cost of 23.
    EXPECT_EQ(solution->ObjectiveValue(), 23);
    EXPECT_GE(count, 1);
    if (max_iterations > 0) {
      EXPECT_GE(count1, 1);
      EXPECT_GE(count2, 1);
    }
  }
}

TEST_P(SolveWithAlternativeSolversAndMetaheuristicsTest,
       WithAlternativeParameters) {
  const bool use_metaheuristic = GetParam();
  for (int max_iterations = 0; max_iterations < 10; ++max_iterations) {
    IndexManager manager(
        /*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
    Model model(manager);
    // Prefer high indices first.
    const int cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return i >= j ? 1 : 10; });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    int count = 0;
    model.AddAtSolutionCallback([&count]() { ++count; });

    // Invalid parameters.
    Model model1(manager);
    const int cost1 = model1.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model1.SetArcCostEvaluatorOfAllVehicles(cost1);
    int count1 = 0;
    model1.AddAtSolutionCallback([&count1]() { ++count1; });
    RoutingSearchParameters parameters1 = DefaultRoutingSearchParameters();
    parameters1.set_solution_limit(0);

    // CP-SAT will result in a solution with no failures.
    Model model2(manager);
    const int cost2 =
        model2.RegisterTransitCallback([](int64_t, int64_t) { return 2; });
    model2.SetArcCostEvaluatorOfAllVehicles(cost2);
    int count2 = 0;
    model2.AddAtSolutionCallback([&count2]() { ++count2; });
    RoutingSearchParameters parameters2 = DefaultRoutingSearchParameters();
    parameters2.set_use_cp(BOOL_FALSE);
    parameters2.set_use_cp_sat(BOOL_TRUE);

    // No improvements possible due to cost function, will result in failures
    // with one solution, passed by the previous model.
    Model model3(manager);
    const int cost3 =
        model3.RegisterTransitCallback([](int64_t, int64_t) { return 4; });
    model3.SetArcCostEvaluatorOfAllVehicles(cost3);
    int count3 = 0;
    model3.AddAtSolutionCallback([&count3]() { ++count3; });
    RoutingSearchParameters parameters3 = DefaultRoutingSearchParameters();

    // No time results in no solution and no failures.
    Model model4(manager);
    const int cost4 =
        model4.RegisterTransitCallback([](int64_t, int64_t) { return 3; });
    model4.SetArcCostEvaluatorOfAllVehicles(cost4);
    int count4 = 0;
    model4.AddAtSolutionCallback([&count4]() { ++count4; });
    RoutingSearchParameters parameters4 = DefaultRoutingSearchParameters();
    parameters4.mutable_time_limit()->set_seconds(0);

    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_metaheuristic) {
      parameters.set_local_search_metaheuristic(
          LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
      parameters.set_solution_limit(3);
    }
    const Assignment* solution =
        SolveFromAssignmentWithAlternativeSolversAndParameters(
            /*assignment=*/nullptr, &model, parameters,
            {&model1, &model2, &model3, &model4},
            {parameters1, parameters2, parameters3, parameters4},
            max_iterations);
    ASSERT_THAT(solution, ::testing::NotNull());
    // Solution: 0 > 4 > 3 > 2 > 1 > 5, resulting in cost of 23.
    EXPECT_EQ(solution->ObjectiveValue(), 23);
    EXPECT_GE(count, 1);
    EXPECT_EQ(count1, 0);
    EXPECT_EQ(count4, 0);
    if (max_iterations > 0) {
      EXPECT_GE(count2, 1);
      EXPECT_EQ(count3, 1);
      EXPECT_EQ(model1.solver()->failures(), 0);
      EXPECT_EQ(model2.solver()->failures(), 0);
      EXPECT_GT(model3.solver()->failures(), 0);
      EXPECT_EQ(model4.solver()->failures(), 0);
    }
  }
}

TEST_P(SolveWithAlternativeSolversAndMetaheuristicsTest,
       FromFirstSolutionWithAlternatives) {
  const bool use_metaheuristic = GetParam();
  for (int max_iterations = 0; max_iterations < 10; ++max_iterations) {
    IndexManager manager(
        /*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
    Model model(manager);
    // Prefer high indices first.
    const int cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return i >= j ? 1 : 10; });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    int count = 0;
    model.AddAtSolutionCallback([&count]() { ++count; });

    Model model1(manager);
    const int cost1 = model1.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model1.SetArcCostEvaluatorOfAllVehicles(cost1);
    int count1 = 0;
    model1.AddAtSolutionCallback([&count1]() { ++count1; });

    Model model2(manager);
    const int cost2 =
        model2.RegisterTransitCallback([](int64_t, int64_t) { return 2; });
    model2.SetArcCostEvaluatorOfAllVehicles(cost2);
    int count2 = 0;
    model2.AddAtSolutionCallback([&count2]() { ++count2; });

    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_metaheuristic) {
      parameters.set_local_search_metaheuristic(
          LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
      parameters.set_solution_limit(6);
    }
    Assignment* first_solution = model.solver()->MakeAssignment();
    first_solution->Add(model.NextVar(0))->SetValue(1);
    first_solution->Add(model.NextVar(1))->SetValue(2);
    first_solution->Add(model.NextVar(2))->SetValue(3);
    first_solution->Add(model.NextVar(3))->SetValue(4);
    first_solution->Add(model.NextVar(4))->SetValue(5);

    const Assignment* solution = SolveFromAssignmentWithAlternativeSolvers(
        first_solution, &model, {&model1, &model2}, parameters, max_iterations);
    ASSERT_THAT(solution, ::testing::NotNull());
    // Solution: 0 > 4 > 3 > 2 > 1 > 5, resulting in cost of 23.
    EXPECT_EQ(solution->ObjectiveValue(), 23);
    EXPECT_GE(count, 1);
    if (max_iterations > 0) {
      EXPECT_GE(count1, 1);
      EXPECT_GE(count2, 1);
    }
  }
}

TEST_P(SolveWithAlternativeSolversAndMetaheuristicsTest,
       WithFailingAlternative) {
  const bool use_metaheuristic = GetParam();
  IndexManager manager(
      /*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
  Model failing_model(manager);
  failing_model.SetArcCostEvaluatorOfAllVehicles(
      failing_model.RegisterTransitCallback(
          [](int64_t, int64_t) { return 1; }));
  failing_model.solver()->AddConstraint(
      failing_model.solver()->MakeFalseConstraint());
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  if (use_metaheuristic) {
    parameters.set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
    parameters.set_solution_limit(3);
  }
  const Assignment* solution =
      SolveWithAlternativeSolvers(&model, {&failing_model}, parameters,
                                  /*max_non_improving_iterations=*/10);
  // The solution was found by 'model' and robustly returned even if
  // 'failing_model' failed.
  ASSERT_THAT(solution, ::testing::NotNull());
  // Solution: 0 > 1 > 2 > 3 > 4 > 5, resulting in a cost of 5.
  EXPECT_EQ(solution->ObjectiveValue(), 5);
  if (use_metaheuristic) {
    EXPECT_EQ(model.solver()->accepted_neighbors(), 2);
  } else {
    EXPECT_EQ(model.solver()->accepted_neighbors(), 0);
  }
}

TEST_P(SolveWithAlternativeSolversAndMetaheuristicsTest,
       WithAlternativeParametersAndPrimaryTimeLimit) {
  const bool use_metaheuristic = GetParam();
  IndexManager manager(
      /*num_nodes=*/50, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  if (use_metaheuristic) {
    parameters.set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
    parameters.set_solution_limit(3);
  }
  const int time_limit_ms =
      kDebugMode || kAnyXsanEnabled || HACK_CodeCoverageIsEnabled() ? 100 : 10;
  parameters.mutable_time_limit()->set_nanos(time_limit_ms * 1e6);

  Model model1(manager);
  model1.SetArcCostEvaluatorOfAllVehicles(
      model1.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
  RoutingSearchParameters parameters1 = DefaultRoutingSearchParameters();

  const Assignment* solution =
      SolveFromAssignmentWithAlternativeSolversAndParameters(
          /*assignment=*/nullptr, &model, parameters, {&model1}, {parameters1},
          /*max_non_improving_iterations=*/2);
  ASSERT_THAT(solution, ::testing::NotNull());
  EXPECT_LE(model1.solver()->wall_time(), 2 * time_limit_ms);
}

TEST(SolveWithAlternativeSolversTest, WithFailingMainModel) {
  IndexManager manager(
      /*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(model.RegisterTransitCallback(
      [](int64_t i, int64_t j) { return i <= j ? 1 : 10; }));
  Model alt_model(manager);
  alt_model.SetArcCostEvaluatorOfAllVehicles(alt_model.RegisterTransitCallback(
      [](int64_t i, int64_t j) { return i >= j ? 1 : 10; }));
  // Subtle: making the main solver fail afterwards.
  alt_model.AddAtSolutionCallback([solver = model.solver()]() {
    solver->AddConstraint(solver->MakeFalseConstraint());
  });

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  const Assignment* solution = SolveWithAlternativeSolvers(
      &model, {&alt_model}, parameters, /*max_non_improving_iterations=*/10);
  // The solution was found before the second call to 'model' which failed.
  ASSERT_THAT(solution, ::testing::NotNull());
  // Solution: 0 > 1 > 2 > 3 > 4 > 5, resulting in a cost of 5.
  EXPECT_EQ(solution->ObjectiveValue(), 5);
}

/////////////////////// First solution heuristics tests ///////////////////////

TEST(FilteredSavingsTest, SingleMandatoryNode) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);
  params.set_fallback_to_cp_sat_size_threshold(0);
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  // This fails if AddUnassignedNodesToEmptyVehicles is not called.
  const Assignment* assignment = model.SolveWithParameters(params);
  EXPECT_NE(assignment, nullptr);
}

TEST(FilteredSavingsTest, SingleMandatoryNodeWithVehicleFixedCosts) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);
  params.set_fallback_to_cp_sat_size_threshold(0);
  IndexManager manager(2, 3, NodeIndex(0));
  Model model(manager);
  model.SetFixedCostOfVehicle(10, 0);
  model.SetFixedCostOfVehicle(5, 1);
  model.SetFixedCostOfVehicle(15, 2);
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  // This fails if AddUnassignedNodesToEmptyVehicles is not called.
  const Assignment* assignment = model.SolveWithParameters(params);
  EXPECT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 1 + 1 + 5);
}

TEST(FilteredSavingsTest, TimeLimited) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);
  params.set_log_search(true);

  for (const int64_t limit_milliseconds : {0, 1, 100, 1000, 10000}) {
    SCOPED_TRACE(absl::StrCat(limit_milliseconds, " milliseconds"));
    *params.mutable_time_limit() = *util_time::EncodeGoogleApiProto(
        absl::Milliseconds(limit_milliseconds));
    IndexManager manager(50, 10, NodeIndex(0));
    // FLAKINESS(viger): To avoid flakiness, we try again upon failure, until
    // *strictly more* than 2/3 of our attempts have been successful.
    int num_successes = 0;
    int num_failures = 0;
    while (num_failures * 2 >= num_successes) {
      ASSERT_LE(num_failures, 5);
      auto model = std::make_unique<Model>(manager);
      const int vehicle_cost =
          model->RegisterTransitCallback([](int64_t, int64_t) { return 1; });
      model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model->AddConstantDimension(1, 6, true, "count");
      const absl::Time start_time = absl::Now();
      model->SolveWithParameters(params);
      if (absl::Now() - start_time <=
          absl::Milliseconds(limit_milliseconds) - model->TimeBuffer() +
              (kDebugMode || kAnyXsanEnabled ? absl::Milliseconds(120)
                                             : absl::Milliseconds(40))) {
        ++num_successes;
      } else {
        ++num_failures;
      }
    }
  }
}

// Model with two pickup and delivery pairs (one mandatory), where the usual
// feasibility filters don't detect the unfeasible solution (mandatory nodes
// not added in the model due to vehicle capacity and no available vehicle left
// to serve them).
// The Savings heuristic therefore fallbacks to the second version with validity
// checking. Checks that the second version goes through correctly.
TEST(FilteredSavingsTest, OneVehicleWithPickupAndDeliveries) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);
  params.set_log_search(true);

  std::vector<std::vector<int64_t>> costs = {{0, 150, 0, 0, 0},
                                             {150, 0, 150, 150, 0},
                                             {0, 150, 0, 0, 0},
                                             {200, 50, 200, 0, 0},
                                             {0, 0, 0, 0, 0}};
  IndexManager manager(5, 1, NodeIndex(4));
  auto model = std::make_unique<Model>(manager);
  const int vehicle_cost = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model->AddConstantDimension(1, 3, true, "count");
  Solver* const solver = model->solver();
  const Dimension& dim = model->GetDimensionOrDie("count");
  std::vector<std::pair<int, int>> pairs = {{0, 1}, {2, 3}};
  for (auto pair : pairs) {
    solver->AddConstraint(solver->MakeEquality(model->VehicleVar(pair.first),
                                               model->VehicleVar(pair.second)));
    solver->AddConstraint(solver->MakeLessOrEqual(dim.CumulVar(pair.first),
                                                  dim.CumulVar(pair.second)));
    model->AddPickupAndDelivery(pair.first, pair.second);
  }
  model->AddDisjunction(std::vector<int64_t>({0}), Model::kNoPenalty);
  model->AddDisjunction(std::vector<int64_t>({1}), Model::kNoPenalty);
  model->AddDisjunction(std::vector<int64_t>({2}), 1000);
  model->AddDisjunction(std::vector<int64_t>({3}), 1000);
  const Assignment* assignment = model->SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  // Solution: 2 & 3 skipped with penalty, route: 4->0->1->5
  EXPECT_EQ(assignment->ObjectiveValue(), 2 * 1000 + 0 + 150 + 0);
}

// Tests that the filtered Savings heuristic chooses vehicles in order of
// increasing fixed costs.
TEST(FilteredSavingsTest, DifferentVehicleFixedCosts) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 5;
  const int kNumVehicles = 3;
  std::vector<std::vector<int64_t>> costs = {{0, 75, 100, 100, 200},
                                             {75, 0, 50, 200, 375},
                                             {100, 50, 0, 150, 350},
                                             {100, 200, 150, 0, 210},
                                             {200, 375, 350, 210, 0}};

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (FirstSolutionStrategy::Value savings :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    params.set_first_solution_strategy(savings);
    auto model = std::make_unique<Model>(manager);

    // First test: Only use the second vehicle.
    model->SetFixedCostOfVehicle(10, 0);
    model->SetFixedCostOfVehicle(2, 1);
    model->SetFixedCostOfVehicle(5, 2);
    const int vehicle_cost = model->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model->AddConstantDimension(1, 5, true, "Pickup");
    const Assignment* assignment = model->SolveWithParameters(params);
    EXPECT_EQ(assignment->ObjectiveValue(), 685 + 2);
    EXPECT_FALSE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
    EXPECT_FALSE(model->IsVehicleUsed(*assignment, 2));

    // Second test: Use second and third vehicles by reducing the vehicle
    // capacities.
    model = std::make_unique<Model>(manager);
    model->SetFixedCostOfVehicle(10, 0);
    model->SetFixedCostOfVehicle(2, 1);
    model->SetFixedCostOfVehicle(5, 2);
    const int vehicle_cost_2 = model->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost_2);
    model->AddConstantDimension(1, 3, true, "Pickup");
    assignment = model->SolveWithParameters(params);
    EXPECT_EQ(assignment->ObjectiveValue(), 225 + 2 + 510 + 5);
    EXPECT_FALSE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 2));
  }
}

TEST(FilteredSavingsTest, FilterNeighborsForSavings) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  std::vector<std::vector<int64_t>> costs = {{0, 75, 100, 100, 200},
                                             {75, 0, 50, 200, 375},
                                             {100, 50, 0, 150, 350},
                                             {100, 200, 150, 0, 210},
                                             {200, 375, 350, 210, 0}};

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (FirstSolutionStrategy::Value savings :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    params.set_first_solution_strategy(savings);
    auto model = std::make_unique<Model>(manager);

    const int vehicle_cost = model->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // First only consider 1 neighbor for each node when computing the savings.

    // The only savings considered here are (in decreasing order of value):
    // 1-->2 (125), 2-->1 (125), 4-->3 (90), 3-->2 (50)
    // One vehicle should therefore be used for the saving 1-->2, which can then
    // no longer be extended. A second vehicle is then used for 4-->3.
    params.mutable_savings_parameters()->set_neighbors_ratio(0.2);
    // Avoid adding reverse arcs so exactly 1 neighbor is considered for each
    // node.
    params.mutable_savings_parameters()->set_add_reverse_arcs(false);
    const Assignment* assignment = model->SolveWithParameters(params);
    EXPECT_EQ(assignment->ObjectiveValue(), 225 + 510);
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));

    // Add symmetric arcs for nearest neighbors when computing the savings.

    // By adding the reverse savings, we now have 6 savings:
    // 1-->2 (125), 2-->1 (125), 3-->4 (90), 4-->3 (90), 3-->2 (50), 2-->3 (50)
    // The second vehicle (lower fixed cost) is used to start a new route with
    // the saving 1-->2, which can then be extended to 1-->2-->3 and finally
    // 1-->2-->3-->4, so the first vehicle is no longer used.
    // In the parallel version, the second vehicle is used to start the route
    // with 1-->2. Then a second route is started with arc 3-->4 with the second
    // vehicle. These two routes are then merged on the second vehicle by
    // inserting 2-->3.
    params.mutable_savings_parameters()->set_add_reverse_arcs(true);
    model = std::make_unique<Model>(manager);
    const int vehicle_cost_2 = model->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost_2);
    model->SetFixedCostOfVehicle(10, 0);
    model->SetFixedCostOfVehicle(2, 1);
    assignment = model->SolveWithParameters(params);
    EXPECT_EQ(assignment->ObjectiveValue(), 685 + 2);
    EXPECT_FALSE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
  }
}

// Unit test for the add_reverse_arcs parameter.
// Node 1 is the depot, Node 0 is optional.
TEST(FilteredSavingsTest, AddReverseArcs) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 2;
  const int kNumVehicles = 1;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(1));
  Model model(manager);
  model.AddDisjunction({0}, 100);
  params.mutable_savings_parameters()->set_add_reverse_arcs(true);

  const Assignment* assignment = model.SolveWithParameters(params);

  ASSERT_NE(assignment, nullptr);
}

using FilteredSavingsWithUnaryCallbackTest = ::testing::TestWithParam<bool>;

// Create a routing model instance with two vehicles, each having their own
// (separate) depot (start/end) node. The first vehicle's depot is Node 0, and
// the second's Node 1.
// Nodes 2 and 3 are close both to each other (distance = 1) and to Node 0, and
// and are far from the other three nodes.
// Similarly, Nodes 1, 4 and 5 are close together, and far from the other nodes.
//
//     NODE_2                                               NODE_4
//     |     \                                             /      \
//   1 |      \ 10                                      3 /        \ 5
//     |       \                     100                 /          \
//   NODE_3-----NODE_0   --------------------------   NODE_1-----NODE_5
//           9                                                2
//
// This test verifies that nodes 2 and 3 are served by the first vehicle,
// starting and ending at node 0, and that nodes 4 and 5 are served by the
// vehicle with node 1 as a depot.
// It also verifies that in the case where either of these arcs cannot be
// inserted on the vehicle close to them due to low vehicle capacity, the arcs
// that do get inserted are inserted on the closer vehicle.
TEST_P(FilteredSavingsWithUnaryCallbackTest, ChooseClosestDepot) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);

  const int kNumNodes = 6;
  const int kNumVehicles = 2;
  std::vector<std::vector<int64_t>> costs = {
      {0, 100, 10, 9, 105, 105}, {100, 0, 110, 120, 3, 2},
      {10, 110, 0, 1, 150, 150}, {9, 120, 1, 0, 150, 150},
      {105, 3, 150, 150, 0, 5},  {105, 2, 150, 150, 5, 0}};
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end2 = std::make_pair(NodeIndex(1), NodeIndex(1));
  IndexManager manager(kNumNodes, kNumVehicles, {start_end1, start_end2});

  const std::vector<int64_t> kVehicleCapacities[] = {{3, 3}, {1, 3}, {3, 1}};
  const int kNodePenalty = 1000;
  const int kExpectedCost[] = {30, 2 * kNodePenalty + 10,
                               2 * kNodePenalty + 20};
  const bool kFirstVehicleUsed[] = {true, false, true};
  const bool kSecondVehicleUsed[] = {true, true, false};

  for (int i = 0; i < std::size(kVehicleCapacities); i++) {
    for (FirstSolutionStrategy::Value savings :
         {FirstSolutionStrategy::SAVINGS,
          FirstSolutionStrategy::PARALLEL_SAVINGS}) {
      params.set_first_solution_strategy(savings);
      Model model(manager);
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      const int evaluator_index =
          GetParam()
              ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
              : model.RegisterTransitCallback(
                    [](int64_t, int64_t) { return 1; });
      model.AddDimensionWithVehicleCapacity(
          evaluator_index, 0, kVehicleCapacities[i], true, "Pickup");
      // Make nodes optional for the cases with low vehicle capacities.
      for (NodeIndex node(0); node < kNumNodes; node++) {
        const int64_t index = manager.NodeToIndex(node);
        if (index >= 0) {
          model.AddDisjunction({index}, kNodePenalty);
        }
      }
      const Assignment* assignment = model.SolveWithParameters(params);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(model.IsVehicleUsed(*assignment, 0), kFirstVehicleUsed[i]);
      EXPECT_EQ(model.IsVehicleUsed(*assignment, 1), kSecondVehicleUsed[i]);
      EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost[i]);
    }
  }
}

// In the example below, verify the filtered Savings algorithm behaves as
// expected:
//
//   NODE0 --3-- Node3 -----------102----------- Node4 --3-- NODE1
//                 \                              /
//                       50                50
//                             \     /
//                              NODE2
//                             /     \
//                           10       10
//                           /         \
//                      Node5 ---10--- Node6
//
// The model has 2 available vehicles, the first vehicle starting at NODE0 and
// ending at NODE1, and the second's depot is NODE2. Nodes 3, 4, 5 and 6 are the
// nodes to serve, all are optional with a penalty of 1000.
//
// In this example, the first saving to be considered is Node3--Node4, for the
// first vehicle (NODE0/NODE1 as start/end, saving value = 108), but since this
// vehicle has a capacity of 1, this arc can't be served by the first vehicle.
// The next best saving for Node3--Node4 is then added for later, which is for
// the second vehicle type with a saving value of 50+50-102 = -2.
// Therefore, the best saving now becomes Node5--Node6 for the second vehicle,
// with a saving value of 10+10-10 = 10.
// This arc is served by the second vehicle, and the overall cost is then
// (10+10+10) + 2*1000 = 2030.
TEST_P(FilteredSavingsWithUnaryCallbackTest, SavingsQueue) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);

  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  std::vector<std::vector<int64_t>> costs = {
      {0, 108, 1000, 3, 105, 1000, 1000}, {108, 0, 1000, 105, 3, 1000, 1000},
      {1000, 1000, 0, 50, 50, 10, 10},    {3, 105, 50, 0, 102, 1000, 1000},
      {105, 3, 50, 102, 0, 1000, 1000},   {1000, 1000, 10, 1000, 1000, 0, 10},
      {1000, 1000, 10, 1000, 1000, 10, 0}};
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(1));
  const auto& start_end2 = std::make_pair(NodeIndex(2), NodeIndex(2));
  IndexManager manager(kNumNodes, kNumVehicles, {start_end1, start_end2});

  for (FirstSolutionStrategy::Value savings :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    params.set_first_solution_strategy(savings);
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const int evaluator_index =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
            : model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.AddDimensionWithVehicleCapacity(
        evaluator_index, 0, /*vehicle_capacities*/ {1, 3}, true, "Load");

    for (NodeIndex node(0); node < kNumNodes; node++) {
      const int64_t index = manager.NodeToIndex(node);
      if (index >= 0) {
        model.AddDisjunction({index}, /*penalty*/ 1000);
      }
    }

    const Assignment* assignment = model.SolveWithParameters(params);

    ASSERT_NE(assignment, nullptr);
    EXPECT_FALSE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
    EXPECT_EQ(assignment->ObjectiveValue(), 2 * 1000 + 30);
  }
}

// In the following example, the model has 3 vehicles and 2 nodes to serve.
// The first two vehicles start and end at Node0, and the third vehicle's
// depot is Node1, far from the other 3 nodes:
//
//            Node2
//          /   |   \
// NODE0 -2-    1    ----------------------- 1000 ----------------------- NODE1
//          \   |   /
//            Node3
//
// The first type of vehicle to be considered for the arc Node2-->Node3 is the
// type with the closest depot, NODE0. In this example, the first vehicle of
// type 1 has a low capacity and cannot serve this arc. This test verifies that
// different vehicle classes within one type are explored before moving on to
// the next best type, which ensures this arc is served by the second vehicle,
// with a total cost of 2+2+1 = 5.
TEST_P(FilteredSavingsWithUnaryCallbackTest,
       DifferentVehicleCapacitiesPerType) {
  const int kNumNodes = 4;
  const int kNumVehicles = 3;
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end2 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end3 = std::make_pair(NodeIndex(1), NodeIndex(1));
  IndexManager manager(kNumNodes, kNumVehicles,
                       {start_end1, start_end2, start_end3});

  std::vector<std::vector<int64_t>> costs = {
      {0, 1500, 2, 2}, {1500, 0, 1000, 1000}, {2, 1000, 0, 1}, {2, 1000, 1, 0}};

  for (FirstSolutionStrategy::Value savings :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const int evaluator_index =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
            : model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.AddDimensionWithVehicleCapacity(
        evaluator_index, 0, /*vehicle_capacities*/ {1, 3, 3}, true, "Load");

    RoutingSearchParameters params = DefaultRoutingSearchParameters();
    params.set_solution_limit(1);
    params.set_first_solution_strategy(savings);

    const Assignment* assignment = model.SolveWithParameters(params);

    ASSERT_NE(assignment, nullptr);
    EXPECT_FALSE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
    EXPECT_FALSE(model.IsVehicleUsed(*assignment, 2));
    EXPECT_EQ(assignment->ObjectiveValue(), 2 + 2 + 1);
  }
}

// Tests that routes are correctly merged in the parallel Savings algorithm.
// In the example below, the first two vehicles' depot is NODE0, and the third's
// is NODE1. In the parallel Savings, the first two Savings to be considered are
// Node2-->Node3 and Node4-->Node5, which are used to start new routes with the
// first and second vehicle respectively. The third Saving to be considered is
// then Node3-->Node4, first for the third vehicle (whith a closer depot), but
// since both Node3 and Node4 are already on two routes of the other type, the
// Saving with the correct type (Node3-->Node4 with NODE0 as depot) is
// automatically considered, and the two first routes are then merged onto the
// route of the first vehicle by adding this arc.
//
//                                 NODE1
//                          /                 \
//            /                                             \
//         Node3 ------------------ 10 ------------------- Node4
//           | \                                            / |
//           |          5                          5          |
//           |                   \        /                   |
//           5                      NODE0                     5
//           |                   /        \                   |
//           |          5                          5          |
//           | /                                            \ |
//         Node2                                           Node5
//
// This example is also tested by tweaking the arc costs a bit, so that the
// order in which the Savings for arcs 2-->3, 3-->4 and 4-->5 are considered
// vary. In all these examples, the expected cost remains the same, i.e. 3-->4
// should be used to merge the two routes started by 2-->3 and 4-->5.
TEST_P(FilteredSavingsWithUnaryCallbackTest, MergeParallelRoutes) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_SAVINGS);

  const int kNumNodes = 6;
  const int kNumVehicles = 3;
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end2 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end3 = std::make_pair(NodeIndex(1), NodeIndex(1));
  IndexManager manager(kNumNodes, kNumVehicles,
                       {start_end1, start_end2, start_end3});

  std::vector<std::vector<int64_t>> costs = {
      {0, 100, 5, 5, 5, 5},     {100, 0, 100, 4, 4, 100},
      {5, 100, 0, 5, 100, 100}, {5, 4, 5, 0, 10, 100},
      {5, 4, 100, 10, 0, 5},    {5, 100, 100, 100, 5, 0}};

  const std::vector<std::vector<int>> cost_diffs = {
      /*No change, 3-->4 considered last*/ {/*node1*/ 3, /*node2*/ 4,
                                            /*cost*/ 10},
      /*Reduce the cost of 3-->4to start with this arc*/ {3, 4, 2},
      /*Increase the cost of 4-->5 to do it last*/ {4, 5, 15},
      /*Increase the cost of 2-->3 o do it last*/ {2, 3, 15}};

  const std::vector<bool> kReduceVehicleCapacities = {
      false, /*reduce capacity of third vehicle*/ true, false, false};
  const std::vector<int64_t> kReducedCapacities = {10, 10, 1};

  for (int i = 0; i < cost_diffs.size(); i++) {
    const auto& cost_diff = cost_diffs[i];
    const int first_node = cost_diff[0];
    const int second_node = cost_diff[1];
    const int new_cost = cost_diff[2];
    const int original_cost = costs[first_node][second_node];

    costs[first_node][second_node] = costs[second_node][first_node] = new_cost;

    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    if (kReduceVehicleCapacities[i]) {
      const int evaluator_index =
          GetParam()
              ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
              : model.RegisterTransitCallback(
                    [](int64_t, int64_t) { return 1; });
      model.AddDimensionWithVehicleCapacity(evaluator_index, 0,
                                            kReducedCapacities, true, "Load");
    }

    const Assignment* assignment = model.SolveWithParameters(params);

    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(model.IsVehicleUsed(*assignment, 0) +
                  model.IsVehicleUsed(*assignment, 1),
              1);
    EXPECT_FALSE(model.IsVehicleUsed(*assignment, 2));
    const int expected_cost =
        costs[0][2] + costs[2][3] + costs[3][4] + costs[4][5] + costs[5][0];

    EXPECT_EQ(assignment->ObjectiveValue(), expected_cost);

    costs[first_node][second_node] = costs[second_node][first_node] =
        original_cost;
  }
}

// Tests that the parallel Savings heuristic correctly stores uncontained
// Savings it cannot insert and considers them later in the algorithm when one
// of the two ends gets inserted.
// In the example below, the model has a single vehicle with a depot at NODE0.
//
//                            Node1 --1-- Node2
//                              | \       / |
//                              |  5     5  |
//                              |   \   /   |
//                              |    \ /    |
//                              9   NODE0   9
//                              |    / \    |
//                              |   /   \   |
//                              |  5     5  |
//                              | /       \ |
//                            Node4 --1-- Node3
//
// The route is started by inserting the best Saving, Node1-->Node2. The next
// best Saving is then Node3-->Node4, which cannot be inserted because it would
// require starting a new route and there are no more vehicles. This Saving is
// then skipped. The next Saving inserted is then Node2-->Node3, which extends
// the first route to Node1-->Node2-->Node3. Since Node3 is inserted, the
// previously skipped Saving Node3-->Node4 is then re-considered, and used to
// extend the previous route, which results in an overall cost of 5+11+5 = 21.
// If Node3-->Node4 was no longer considered, the next Saving to get inserted
// would have been Node4-->Node1, resulting in a total cost of 5+19+5 = 29.
TEST(FilteredSavingsTest, SkippedSavingsInParallelRouteConstruction) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 5;
  const int kNumVehicles = 1;
  std::vector<std::vector<int64_t>> costs = {{0, 5, 5, 5, 5},
                                             {5, 0, 1, 10, 9},
                                             {5, 1, 0, 9, 10},
                                             {5, 10, 9, 0, 1},
                                             {5, 9, 10, 1, 0}};

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 5 + 11 + 5);
}

// Tests that the Savings are also correctly reinjected when starting a new
// route, and the reinjected Savings are explored by decreasing value.
// The model below has 2 vehicles with a depot at NODE0, and 9 nodes to serve.
// The first two Savings to be considered are 1-->2 and 3-->4, which are used to
// start two separate routes with the two vehicles.
// Then 5<-->8, 6<-->9, 5<-->7 and 6<-->7 are considered, but since there are no
// available vehicles and they cannot be connected to an existing route, they
// are skipped.
// The next best Saving is then 2-->3, which is used to merge the two previous
// routes into a single one, 0-->1-->2-->3-->4-->0, with a cost of 32.
// Then, 5-->6 is considered and a new route is started with this arc. The
// previously skipped Savings are then reconsidered in the same order as before,
// but since nodes 8 and 9 cannot be inserted, 7-->5 (with a higher Saving value
// of 13 vs. 12 for 6-->7) extends the second route to 0-->7-->5-->6-->0, with a
// cost of 19.
//
//                                |----11---- Node8
//          Node1---------10------|            |
//          /                     |           Node5
//      Node2--------             |         /  |     \
//        |          10           |      5     |      2
//        |              \        |  /         |       \
//        10               ---- NODE0-----------10-------Node7
//        |              /        |  \         |       /
//        |          10           |      5     2      3
//      Node3--------             |         \  |     /
//          \                     |           Node6
//          Node4-----------------10           |
//                                |---------- Node9
//
// The capacity of the two vehicles is set to 5 to avoid merging these two
// routes into one.
TEST(FilteredSavingsTest, ReinjectSavingsWhenStartingANewRoute) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 10;
  const int kNumVehicles = 2;
  std::vector<std::vector<int64_t>> costs = {
      {0, 10, 10, 10, 10, 5, 5, 10, 11, 10},
      {10, 0, 1, 50, 50, 100, 100, 100, 100, 100},
      {10, 1, 0, 10, 50, 100, 100, 100, 100, 100},
      {10, 50, 10, 0, 1, 100, 100, 100, 100, 100},
      {10, 50, 50, 1, 0, 100, 100, 100, 100, 100},
      {5, 100, 100, 100, 100, 0, 2, 2, 1, 100},
      {5, 100, 100, 100, 100, 2, 0, 3, 100, 1},
      {10, 100, 100, 100, 100, 2, 3, 0, 100, 100},
      {11, 100, 100, 100, 100, 1, 100, 100, 0, 100},
      {10, 100, 100, 100, 100, 100, 1, 100, 100, 0}};

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  // Make nodes 8 and 9 optional and other mandatory.
  for (int i = 1; i < kNumNodes; ++i) {
    if (i != 8 && i != 9) {
      model.AddDisjunction({i}, kint64max);
    }
    model.AddDisjunction({i}, 1000);
  }
  // To avoid the routes from being merged into one, and not add nodes 8 or 9
  // to any route.
  const int callback = model.RegisterTransitCallback([](int64_t i, int64_t j) {
    if (i == 8 || j == 8 || i == 9 || j == 9) {
      // from/to nodes 8 or 9, infeasible
      return 10;
    }
    return 1;
  });
  model.AddDimension(callback, 0, 5, true, "Pickup");

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 2 * 1000 + 32 + 19);
}

// Tests that the "next" Savings to be considered for an arc have the correct
// type if one end of the arc is already inserted on a route.
// In the example below, we have 3 vehicles with depots at NODE0, NODE1 and
// NODE6.
//
//                 4----------- Node3 --- 3 --- Node2 ------3
//                /               |               |          \
//  NODE0 -1- NODE1               1               1          NODE6
//                \               |               |          /
//                 4----------- Node4 --- 3 --- Node5 ------3
//
// The best Saving in this example is for arc 3-->4 and its closest vehicle
// depot, NODE1. The second vehicle having a low capacity, this arc cannot be
// served by it, and the next best Saving for this arc (depot at NODE0) is added
// to next_savings.
// The second best Saving available is then for arc 2-->3 with the third vehicle
// and this arc is used to start a new route. Since Node3 is inserted on the
// third vehicle, the Saving for arc 3-->4 in next_savings is updated to match
// the type of the vehicle containing Node3.
// Since the started route cannot be extended by any of the remaining Savings
// (by artificial constraints), the algorithm noves on to the next_savings,
// and extends the route of the third vehicle to NODE6-->2-->3-->4-->5-->NODE6.
TEST_P(FilteredSavingsWithUnaryCallbackTest, AddCorrectTypeToNextSavings) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 7;
  const int kNumVehicles = 3;
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end2 = std::make_pair(NodeIndex(1), NodeIndex(1));
  const auto& start_end3 = std::make_pair(NodeIndex(6), NodeIndex(6));
  IndexManager manager(kNumNodes, kNumVehicles,
                       {start_end1, start_end2, start_end3});
  std::vector<std::vector<int64_t>> costs = {
      {0, 1, 8, 5, 5, 8, 100},  {1, 0, 7, 4, 4, 7, 100},
      {8, 7, 0, 3, 100, 1, 3},  {5, 4, 3, 0, 1, 100, 6},
      {5, 4, 100, 1, 0, 3, 6},  {8, 7, 1, 100, 3, 0, 3},
      {100, 100, 3, 6, 6, 3, 0}};

  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  // Make first and second vehicles unable to serve any Saving.
  const int dimension_callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.AddDimensionWithVehicleCapacity(
      dimension_callback, /*slack_max*/ 0, /*vehicle_capacities*/ {1, 1, 10},
      /*fix_start_cumul_to_zero*/ true, "Demand");

  // Do not extend the first route by adding arcs 3-->5, 5-->2 or 4-->2.
  model.NextVar(3)->RemoveValue(5);
  model.NextVar(5)->RemoveValue(2);
  model.NextVar(4)->RemoveValue(2);

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 3 + 3 + 1 + 3 + 3);
}

INSTANTIATE_TEST_SUITE_P(FilteredSavingsWithUnaryCallbackTestInstance,
                         FilteredSavingsWithUnaryCallbackTest,
                         ::testing::Bool());

// Test that the skipped savings are correctly reinjected when there are
// multiple vehicle types.
// In the following example, we have 2 vehicles, the first starting at NODE0 and
// ending its route at NODE5, and the second starting and ending at NODE6.
//
//   NODE0 --20-- Node2 --20-- Node3 --20-- Node1 --20-- Node4 --20-- NODE5
//                  \             /
//                   \           /
//                    55       55
//                     \       /
//                      \     /
//                       NODE6
//
// The Saving value of the three arcs 2-->3, 3-->1, and 1-->4 is equal to 100.
// Since the first vehicle's depot is closest to all three arcs, these Savings
// will be the first ones considered, in increasing order of before_node, i.e.
// 1-->4, 2-->3, then 3-->1 (the cost of the arc 2-->1 is set to 50 instead of
// 40 so its Saving value is less than these 3).
// The first route is started by inserting the arc 1-->4. Then 2-->3 is
// considered, but since it cannot extend the first route and there are no more
// vehicles for that type, the Saving related to the type of the second vehicle
// is added to the priority queue (Saving value of 110 - 20 = 90), and the
// current Saving for 2-->3 is skipped.
// The next Saving considered is then 3-->1, which extends the first route by
// adding Node3. The previously skipped Saving 2-->3, for the first vehicle, is
// then also added to the priority queue, and is therefore considered before the
// other Saving for the same arc since its value is higher.
// The final route is therefore 0-->2-->3-->1-->4-->5, with a total cost of 100.
TEST(FilteredSavingsTest, SkippedSavingsWithMultipleTypes) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::PARALLEL_SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  std::vector<std::vector<int64_t>> costs = {
      {0, 60, 20, 40, 80, 100, 5000},     {60, 0, 40, 20, 20, 40, 5000},
      {20, 50, 0, 20, 60, 80, 55},        {40, 20, 20, 0, 40, 60, 55},
      {80, 20, 60, 40, 0, 20, 5000},      {100, 40, 80, 60, 20, 0, 5000},
      {5000, 5000, 55, 55, 5000, 5000, 0}};

  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(5));
  const auto& start_end2 = std::make_pair(NodeIndex(6), NodeIndex(6));
  IndexManager manager(kNumNodes, kNumVehicles, {start_end1, start_end2});
  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 100);
}

// Small test to ensure that node-vehicle incompatibilities are handled
// correctly in the Savings heuristic. In the following example, the model has
// two identical vehicles, but the first is incompatible with one of the nodes.
// This test ensures that the Savings heuristic correctly inserts the arc on the
// second vehicle even though both are of the same class.
TEST(FilteredSavingsTest, NodeVehicleIncompatibility) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 3;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  model.SetAllowedVehiclesForIndex({1}, manager.NodeToIndex(NodeIndex(1)));

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(
      assignment->Value(model.VehicleVar(manager.NodeToIndex(NodeIndex(1)))),
      kNumVehicles - 1);
  EXPECT_EQ(
      assignment->Value(model.VehicleVar(manager.NodeToIndex(NodeIndex(2)))),
      kNumVehicles - 1);
}

// The Savings heuristic ignores pickup and delivery constraints and will
// blindly extend routes with pairs partially performed. This test makes sure
// partially performed pairs are made unperformed. The heuristic will try to
// add as many nodes to a route as possible, adding a pair and only one node of
// the other pair due to capacity constraints.
TEST(FilteredSavingsTest, PickupAndDeliveries) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);
  params.set_log_search(true);

  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));

  // Optional pickup and deliveries.
  std::vector<std::pair<int, int>> pairs = {{1, 2}, {3, 4}};
  for (const auto [pickup, delivery] : pairs) {
    model.AddPickupAndDelivery(pickup, delivery);
    model.AddDisjunction(std::vector<int64_t>({pickup}), 10);
    model.AddDisjunction(std::vector<int64_t>({delivery}), 10);
    model.solver()->AddConstraint(model.solver()->MakeEquality(
        model.VehicleVar(pickup), model.VehicleVar(delivery)));
  }
  // Both pairs don't fit together on a single route.
  model.AddConstantDimension(1, 4, true, "count");

  const Assignment* assignment = model.SolveWithParameters(params);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), 2 * 10 + 3);
  // Solver should find solution on first pass of Savings (without CP checks).
  EXPECT_EQ(model.solver()->failures(),
            /* balancing */ 34 + /* backtracking */ 0);
}

}  // namespace

// Tests that RoutingFilteredHeuristic::BuildSolutionFromRoutes() correctly
// re-initializes the filters before attempting to build a solution from the
// given route.
// In the following test, Node1 and Node2 are part of a disjunction. The filter
// is synchronized with an initial solution performing Node1, and we call
// BuildSolutionFromRoutes() with a route containing Node2 only.
// Without the proper re-initialization, the NodeDisjunctionFilter will reject
// the route containing Node2 as it is synchronized with a solution containing
// Node1.
TEST(RoutingFilteredHeuristicTest, BuildSolutionFromRoutes) {
  const int kNumNodes = 4;
  const int kNumVehicles = 1;
  for (bool filter_cost : {true, false}) {
    const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

    Model model(manager);

    const std::vector<int64_t> kDisjunctionIndices = {1, 2};
    model.AddDisjunction(kDisjunctionIndices);

    model.CloseModel();

    // Create the disjunction filter and synchronize it with the solution
    // START-->1-->3-->END.
    LocalSearchFilter* disjunction_filter =
        MakeNodeDisjunctionFilter(model, filter_cost);
    const std::vector<int> first_solution = {1, 3, 2, 4};
    Assignment first_assignment(model.solver());
    for (int i = 0; i < first_solution.size(); i++) {
      if (first_solution[i] == i) continue;
      first_assignment.Add(model.NextVar(i))->SetValue(first_solution[i]);
    }
    disjunction_filter->Synchronize(&first_assignment, nullptr);

    GlobalCheapestInsertionParameters gci_parameters;
    gci_parameters.set_farthest_seeds_ratio(0.0);
    gci_parameters.set_neighbors_ratio(1.0);
    gci_parameters.set_min_neighbors(1);
    gci_parameters.set_use_neighbors_ratio_for_initialization(false);
    gci_parameters.set_add_unperformed_entries(false);

    LocalSearchFilterManager filter_manager({disjunction_filter});
    GlobalCheapestInsertionFilteredHeuristic heuristic(
        &model, []() { return false; },
        [&kDisjunctionIndices](int64_t from, int64_t to, int64_t) {
          if (from == kDisjunctionIndices[1] && to == 3) {
            // Arc 2-->3 is cheaper.
            return 0;
          }
          return 1000;
        },
        [](int64_t) { return kint64max; }, &filter_manager, gci_parameters,
        /*is_sequential=*/false);

    // Build a solution from the route START-->2-->END.
    const std::vector<int> route = {2, 1, 4, 3};
    const Assignment* const result_assignment =
        heuristic.BuildSolutionFromRoutes(
            [&route](int64_t i) { return route[i]; });

    ASSERT_NE(result_assignment, nullptr);
    // The solution should be START-->2-->3-->END.
    const std::vector<int> expected_result = {2, 1, 3, 4};
    for (int i = 0; i < expected_result.size(); i++) {
      EXPECT_EQ(result_assignment->Value(model.NextVar(i)), expected_result[i]);
    }
  }
}

// Tests BuildSolutionFromRoutes in a special scenario arising from b/219043402.
// Catches a very special corner case which DCHECK-failed because of
// infeasibilities not detected by filters when the filters are not first synced
// with a valid solution.
// The model has 2 vehicles and 2 optional non start/end nodes.
// The first vehicle needs a resource, which reduces its max span to 100.
// The fixed transit to/from each node is 250 (making the first vehicle
// unusable).
TEST(RoutingFilteredHeuristicTest, BuildSolutionFromRoutes_WithResource) {
  const int kNumNodes = 3;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (bool has_start_chain : {true, false}) {
    Model model(manager);

    // Make non start/end nodes optional.
    for (int node = 0; node < model.Size(); node++) {
      if (model.IsStart(node)) continue;
      model.AddDisjunction({node}, /*penalty*/ 1000);
    }

    const std::string kDimName = "time";
    model.AddConstantDimension(250, 1000000, false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // Add a resource with a max span of 500-400 = 100 and make vehicle 0
    // require it.
    ResourceGroup& resource_group = *model.AddResourceGroup();
    resource_group.AddResource(Attributes(Domain(400, 1000), Domain(-1, 500)),
                               dimension);
    resource_group.NotifyVehicleRequiresAResource(0);

    model.CloseModel();

    // Create the ResourceGroupAssignmentFilter.
    LocalSearchFilter* resource_filter = MakeResourceAssignmentFilter(
        model.GetMutableLocalCumulLPOptimizer(*dimension),
        model.GetMutableLocalCumulMPOptimizer(*dimension), false, false);

    GlobalCheapestInsertionParameters gci_parameters;
    gci_parameters.set_farthest_seeds_ratio(0.0);
    gci_parameters.set_neighbors_ratio(1.0);
    gci_parameters.set_min_neighbors(1);
    gci_parameters.set_use_neighbors_ratio_for_initialization(false);
    gci_parameters.set_add_unperformed_entries(false);

    LocalSearchFilterManager filter_manager({resource_filter});
    GlobalCheapestInsertionFilteredHeuristic heuristic(
        &model, []() { return false; },
        [](int64_t, int64_t, int64_t) { return 250; },
        [](int64_t) { return 1000; }, &filter_manager, gci_parameters,
        /*is_sequential=*/false);

    // Build a solution from routes
    // START0-->END4, START3-->1-->END5.
    std::vector<int> route = {4, 5, 2, 1};
    const auto next_accessor = [&route](int64_t i) { return route[i]; };
    const Assignment* result_assignment =
        heuristic.BuildSolutionFromRoutes(next_accessor);
    ASSERT_NE(result_assignment, nullptr);
    // The solution should be START3-->1-->2-->END5.
    const std::vector<int> expected_result = {4, 2, 5, 1};
    for (int i = 0; i < expected_result.size(); i++) {
      EXPECT_EQ(result_assignment->Value(model.NextVar(i)), expected_result[i]);
    }

    // Build a solution from routes START(0)-->1-->END4, START3-->END5.
    route = {1, 4, 2, 5};
    if (has_start_chain) {
      model.NextVar(0)->SetValue(1);
      ASSERT_TRUE(model.NextVar(0)->Bound());
    }
    result_assignment = heuristic.BuildSolutionFromRoutes(next_accessor);
    EXPECT_EQ(result_assignment, nullptr);
  }
}

// This class is a friend class to SavingsFilteredHeuristic.
class SavingsFilteredHeuristicTestPeer {
 public:
  explicit SavingsFilteredHeuristicTestPeer(
      std::unique_ptr<SavingsFilteredHeuristic> savings_db)
      : savings_db_(std::move(savings_db)) {}

  int MaxNumNeighbors(int num_vehicle_types) const {
    return savings_db_->MaxNumNeighborsPerNode(num_vehicle_types);
  }

 private:
  const std::unique_ptr<const SavingsFilteredHeuristic> savings_db_;
};

namespace {

class SavingsMaxNumNeighborsPerNodeTest : public testing::Test {
 protected:
  ~SavingsMaxNumNeighborsPerNodeTest() override = default;

  void SetupModel() {
    const int kNumNodes = 5000;
    const int kNumVehicles = 500;
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    EXPECT_EQ(model_->Size(), kNumNodes - 1 + kNumVehicles);
  }

  void SetupSavingsPeer(double neighbors_ratio, double max_memory_usage_bytes,
                        bool is_sequential) {
    SavingsParameters savings_parameters =
        DefaultRoutingSearchParameters().savings_parameters();
    savings_parameters.set_neighbors_ratio(neighbors_ratio);
    savings_parameters.set_max_memory_usage_bytes(max_memory_usage_bytes);

    if (is_sequential) {
      savings_peer_ = std::make_unique<SavingsFilteredHeuristicTestPeer>(
          std::make_unique<SequentialSavingsFilteredHeuristic>(
              model_.get(), []() { return false; }, savings_parameters,
              nullptr));
    } else {
      savings_peer_ = std::make_unique<SavingsFilteredHeuristicTestPeer>(
          std::make_unique<ParallelSavingsFilteredHeuristic>(
              model_.get(), []() { return false; }, savings_parameters,
              nullptr));
    }
  }

  int MaxNumNeighbors(int num_vehicle_types) const {
    return savings_peer_->MaxNumNeighbors(num_vehicle_types);
  }

  std::unique_ptr<Model> model_;
  std::unique_ptr<SavingsFilteredHeuristicTestPeer> savings_peer_;
};

// Tests that the number of neighbors taken into account by the sequential
// Savings heuristic based on the maximum memory is reasonable.
TEST_F(SavingsMaxNumNeighborsPerNodeTest, SequentialWithMemoryRestriction) {
  SetupModel();
  const int kSize = model_->Size();
  SetupSavingsPeer(/*neighbors_ratio*/ 1.0, /*max_memory_usage_bytes*/ 5e9,
                   /*is_sequential*/ true);

  EXPECT_EQ(MaxNumNeighbors(/*num_vehicle_types*/ 1), kSize);
  EXPECT_EQ(MaxNumNeighbors(/*num_vehicle_types*/ 2), kSize);

  for (int num_vehicle_types = 7; num_vehicle_types < 11; num_vehicle_types++) {
    const int max_num_neighbors = MaxNumNeighbors(num_vehicle_types);
    EXPECT_GT(max_num_neighbors, 1000);
    EXPECT_LT(max_num_neighbors, 2000);
  }

  // If all vehicles have different types.
  EXPECT_EQ(MaxNumNeighbors(/*num_vehicle_types*/ 500), 25);
}

// Same as above, but with a lower neighbors_ratio in the SavingsParameter
// to limit the number of neighbors to 2000 max.
TEST_F(SavingsMaxNumNeighborsPerNodeTest,
       SequentialWithMemoryAndRatioRestrictions) {
  SetupModel();
  const int kSize = model_->Size();
  const int kMaxNeighborsByRatio = 2000;
  SetupSavingsPeer(/*neighbors_ratio*/ (1.0 * kMaxNeighborsByRatio) / kSize,
                   /*max_memory_usage_bytes*/ 5e9,
                   /*is_sequential*/ true);

  // Below num_types_limit, the number of neighbors obtained with the
  // neighbors_ratio (2000) is lower than the number of neighbors given by the
  // memory limit, and above num_types_limit it's the opposite.
  const int num_types_limit = 6;

  for (int num_vehicle_types = 1; num_vehicle_types < 11; num_vehicle_types++) {
    const int max_num_neighbors = MaxNumNeighbors(num_vehicle_types);
    if (num_vehicle_types <= num_types_limit) {
      EXPECT_EQ(max_num_neighbors, kMaxNeighborsByRatio);
    } else {
      EXPECT_GT(max_num_neighbors, 1000);
      EXPECT_LT(max_num_neighbors, 2000);
    }
  }
}

// The following two tests are similar to the 2 above, but with the parallel
// Savings heuristic.
TEST_F(SavingsMaxNumNeighborsPerNodeTest, ParallelWithMemoryRestriction) {
  SetupModel();
  const int kSize = model_->Size();
  SetupSavingsPeer(/*neighbors_ratio*/ 1.0, /*max_memory_usage_bytes*/ 5e9,
                   /*is_sequential*/ false);

  EXPECT_EQ(MaxNumNeighbors(/*num_vehicle_types*/ 1), kSize);

  for (int num_vehicle_types = 6; num_vehicle_types < 11; num_vehicle_types++) {
    const int max_num_neighbors = MaxNumNeighbors(num_vehicle_types);
    EXPECT_GT(max_num_neighbors, 1000);
    EXPECT_LT(max_num_neighbors, 2000);
  }

  EXPECT_EQ(MaxNumNeighbors(/*num_vehicle_types*/ 500), 20);
}

TEST_F(SavingsMaxNumNeighborsPerNodeTest,
       ParallellWithMemoryAndRatioRestrictions) {
  SetupModel();
  const int kSize = model_->Size();
  const int kMaxNeighborsByRatio = 2000;
  SetupSavingsPeer(/*neighbors_ratio*/ (1.0 * kMaxNeighborsByRatio) / kSize,
                   /*max_memory_usage_bytes*/ 5e9,
                   /*is_sequential*/ false);

  // Below num_types_limit, the number of neighbors obtained with the
  // neighbors_ratio (2000) is lower than the number of neighbors given by the
  // memory limit, and above num_types_limit it's the opposite.
  const int num_types_limit = 5;

  for (int num_vehicle_types = 1; num_vehicle_types < 11; num_vehicle_types++) {
    const int max_num_neighbors = MaxNumNeighbors(num_vehicle_types);
    if (num_vehicle_types <= num_types_limit) {
      EXPECT_EQ(max_num_neighbors, kMaxNeighborsByRatio);
    } else {
      EXPECT_GT(max_num_neighbors, 1000);
      EXPECT_LT(max_num_neighbors, 2000);
    }
  }
}

using PathCheapestArcTest =
    ::testing::TestWithParam<::testing::tuple<bool, bool>>;

// Tests that optional nodes are used in the first solution when routes start
// and end at different nodes.
TEST_P(PathCheapestArcTest, WithOptionalNodes) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  const bool use_unfiltered_first_solution_strategy =
      ::testing::get<0>(GetParam());
  parameters.set_use_unfiltered_first_solution_strategy(
      use_unfiltered_first_solution_strategy);
  const int kNumNonVehicleNodes = 1;
  const int kNumVehicles = 2;
  const std::vector<NodeIndex> starts = {NodeIndex(1), NodeIndex(2)};
  const std::vector<NodeIndex> ends = {NodeIndex(3), NodeIndex(4)};
  IndexManager manager(kNumNonVehicleNodes + 2 * kNumVehicles, kNumVehicles,
                       starts, ends);
  Model model(manager);
  std::vector<int64_t> index(1, manager.NodeToIndex(NodeIndex(0)));
  model.AddDisjunction(index, 100);
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  EXPECT_EQ(index[0], assignment->Value(model.NextVar(model.Start(1))));
  EXPECT_EQ(model.End(1), assignment->Value(model.NextVar(index[0])));
  if (use_unfiltered_first_solution_strategy) {
    EXPECT_EQ(0, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(0, model.GetNumberOfRejectsInFirstSolution(parameters));
  } else {
    EXPECT_EQ(5, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(0, model.GetNumberOfRejectsInFirstSolution(parameters));
  }
}

// Tests first solution with node disjunctions and routes start and end at
// different nodes.
TEST_P(PathCheapestArcTest, WithDisjunctions) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  const bool use_unfiltered_first_solution_strategy =
      ::testing::get<0>(GetParam());
  parameters.set_use_unfiltered_first_solution_strategy(
      use_unfiltered_first_solution_strategy);
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 2;
  const std::vector<NodeIndex> starts = {NodeIndex(1), NodeIndex(2)};
  const std::vector<NodeIndex> ends = {NodeIndex(3), NodeIndex(4)};
  IndexManager manager(kNumNonVehicleNodes + 2 * kNumVehicles, kNumVehicles,
                       starts, ends);
  Model model(manager);
  std::vector<int64_t> indices =
      manager.NodesToIndices({NodeIndex(5), NodeIndex(0)});
  model.AddDisjunction(indices, 100);
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  EXPECT_EQ(indices[0], assignment->Value(model.NextVar(model.Start(1))));
  EXPECT_EQ(model.End(1), assignment->Value(model.NextVar(indices[0])));
  EXPECT_EQ(indices[1], assignment->Value(model.NextVar(indices[1])));
  if (use_unfiltered_first_solution_strategy) {
    EXPECT_EQ(0, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(0, model.GetNumberOfRejectsInFirstSolution(parameters));
  } else {
    EXPECT_EQ(5, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(0, model.GetNumberOfRejectsInFirstSolution(parameters));
  }
}

TEST_P(PathCheapestArcTest, WithFixedRoute) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  parameters.set_use_unfiltered_first_solution_strategy(
      ::testing::get<0>(GetParam()));
  const int kNumNonVehicleNodes = 1;
  const int kNumVehicles = 2;
  const std::vector<NodeIndex> starts = {NodeIndex(1), NodeIndex(2)};
  const std::vector<NodeIndex> ends = {NodeIndex(3), NodeIndex(4)};
  IndexManager manager(kNumNonVehicleNodes + 2 * kNumVehicles, kNumVehicles,
                       starts, ends);
  Model model(manager);
  const int64_t non_vehicle_node_var_index = manager.NodeToIndex(NodeIndex(0));
  model.NextVar(model.Start(0))->SetValue(non_vehicle_node_var_index);
  model.NextVar(non_vehicle_node_var_index)->SetValue(model.End(0));
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
}

// Test heuristic with a fixed chain of nodes from start of route.
TEST_P(PathCheapestArcTest, WithFixedStartChain) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  parameters.set_use_unfiltered_first_solution_strategy(
      ::testing::get<0>(GetParam()));
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(1)));
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
  EXPECT_EQ(3, assignment->ObjectiveValue());
}

// Test heuristic with a fixed chain of nodes to end of route.
TEST_P(PathCheapestArcTest, WithFixedEndChain) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  parameters.set_use_unfiltered_first_solution_strategy(
      ::testing::get<0>(GetParam()));
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  model.NextVar(manager.NodeToIndex(NodeIndex(1)))->SetValue(model.End(0));
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
  EXPECT_EQ(3, assignment->ObjectiveValue());
}

// Test heuristic with a fixed chains of nodes at the start and at the end of
// the route.
TEST_P(PathCheapestArcTest, WithFixedStartEndChains) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  parameters.set_use_unfiltered_first_solution_strategy(
      ::testing::get<0>(GetParam()));
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(3)));
  model.NextVar(manager.NodeToIndex(NodeIndex(1)))->SetValue(model.End(0));
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
  EXPECT_EQ(4, assignment->ObjectiveValue());
}

int64_t ReturnFirstNodeValueFromVariableIndexUnary(const IndexManager& manager,
                                                   int64_t i) {
  return manager.IndexToNode(i).value();
}
int64_t ReturnFirstNodeValueFromVariableIndex(const IndexManager& manager,
                                              int64_t i, int64_t) {
  return manager.IndexToNode(i).value();
}

// Test heuristic with a single vehicle and pickup and delivery constraints.
TEST_P(PathCheapestArcTest, WithSingleVehicleAndPickupAndDelivery) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  parameters.set_use_unfiltered_first_solution_strategy(false);
  parameters.mutable_time_limit()->set_seconds(1);
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  const int callback =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, 1000, 1000, false, "dim");
  const operations_research::routing::Dimension& dim =
      model.GetDimensionOrDie("dim");
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.VehicleVar(1), model.VehicleVar(2)));
  model.AddPickupAndDelivery(1, 2);
  solver->AddConstraint(
      solver->MakeLessOrEqual(dim.CumulVar(1), dim.CumulVar(2)));
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback(absl::bind_front(
                ReturnFirstNodeValueFromVariableIndexUnary, manager))
          : model.RegisterTransitCallback(absl::bind_front(
                ReturnFirstNodeValueFromVariableIndex, manager));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // The heuristic should be able to find a solution to the model with
  // pickup and delivery constraints without backtracking, as filtering should
  // be perfect in this case. This corresponds to 35 actual failures which
  // are due to tree search balancing for local search + 0 failures for
  // the first solution strategy.
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
  // Shortest route is 0-1-2-3-0 resulting in arc costs of 0 + 1 + 2 + 3 = 6.
  EXPECT_EQ(6, assignment->ObjectiveValue());
}

// Utility filter to mock time passing by in the search.
class ClockFilter : public LocalSearchFilter {
 public:
  explicit ClockFilter(absl::SimulatedClock* const clock) : clock_(clock) {}
  bool Accept(const Assignment* /*delta*/, const Assignment* /*deltadelta*/,
              int64_t /*lower_bound*/, int64_t /*upper_bound*/) override {
    clock_->AdvanceTime(absl::Seconds(1));
    return true;
  }
  void Synchronize(const Assignment* /*assignment*/,
                   const Assignment* /*delta*/) override {}

 private:
  absl::SimulatedClock* const clock_;
};

// Test first solution heuristics with time limits, making sure they are
// enforced.
TEST(FirstSolutionHeuristicTest, CheckLimits) {
  for (auto heuristic :
       {FirstSolutionStrategy::PATH_CHEAPEST_ARC,
        FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC,
        FirstSolutionStrategy::SAVINGS, FirstSolutionStrategy::CHRISTOFIDES,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION}) {
    SCOPED_TRACE(FirstSolutionStrategy_Value_Name(heuristic));
    IndexManager manager(20, 1, NodeIndex(0));
    Model model(manager);
    absl::SimulatedClock clock;
    absl::Time start_time = clock.TimeNow();
    model.solver()->SetClock(&clock);
    model.SetFixedCostOfAllVehicles(10);
    for (NodeIndex node(1); node < 20; ++node) {
      model.AddDisjunction({manager.NodeToIndex(node)}, 1);
    }
    ClockFilter filter(&clock);
    model.AddLocalSearchFilter(&filter);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(heuristic);
    parameters.mutable_time_limit()->set_seconds(5);
    parameters.mutable_time_limit()->set_nanos(1e8);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    // Heuristic should have been interrupted by time limit after 5s.
    EXPECT_GE(clock.TimeNow() - start_time, absl::Seconds(5));
    EXPECT_LE(clock.TimeNow() - start_time, absl::Seconds(7));
    EXPECT_NE(assignment, nullptr);
  }
}

// Test first solution heuristics with time limits, making sure they are
// enforced for models with pairs.
TEST(FirstSolutionHeuristicTest, CheckLimitsWithPairs) {
  for (auto heuristic :
       {FirstSolutionStrategy::PATH_CHEAPEST_ARC,
        FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC,
        FirstSolutionStrategy::SAVINGS, FirstSolutionStrategy::CHRISTOFIDES,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION}) {
    SCOPED_TRACE(FirstSolutionStrategy_Value_Name(heuristic));
    IndexManager manager(21, 1, NodeIndex(0));
    Model model(manager);
    for (NodeIndex node(1); node < NodeIndex(21); node += 2) {
      model.AddPickupAndDelivery(manager.NodeToIndex(node),
                                 manager.NodeToIndex(node + 1));
      model.AddDisjunction({manager.NodeToIndex(node)}, 1);
      model.AddDisjunction({manager.NodeToIndex(node + 1)}, 1);
    }
    absl::SimulatedClock clock;
    absl::Time start_time = clock.TimeNow();
    model.solver()->SetClock(&clock);
    model.SetFixedCostOfAllVehicles(10);
    ClockFilter filter(&clock);
    model.AddLocalSearchFilter(&filter);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(heuristic);
    parameters.mutable_time_limit()->set_seconds(5);
    parameters.mutable_time_limit()->set_nanos(1e8);
    parameters.set_solution_limit(1);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    // Heuristic should have been interrupted by time limit after 5s.
    EXPECT_GE(clock.TimeNow() - start_time, absl::Seconds(5));
    EXPECT_LE(clock.TimeNow() - start_time, absl::Seconds(7));
    EXPECT_NE(assignment, nullptr);
  }
}

using FirstSolutionHeuristicWithUnaryCallbackTest =
    ::testing::TestWithParam<bool>;

// Test heuristics with a single vehicle and pickup and delivery constraints
// with disjunctions.
// Model has a pickup with 3 alternatives and a delivery with 3 alternatives.
// The vehicle has a route limit which only allows one of the pairs to be
// feasible.
TEST_P(FirstSolutionHeuristicWithUnaryCallbackTest,
       WithSingleVehicleAndPickupAndDeliveryWithDisjunctions) {
  for (auto heuristic :
       {FirstSolutionStrategy::PATH_CHEAPEST_ARC,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION}) {
    SCOPED_TRACE(FirstSolutionStrategy_Value_Name(heuristic));
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(heuristic);
    parameters.set_solution_limit(1);
    parameters.set_use_unfiltered_first_solution_strategy(false);
    parameters.mutable_time_limit()->set_seconds(1);
    const int kNumNonVehicleNodes = 2 * 3;
    const int kNumVehicles = 1;
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    const int callback =
        model.RegisterTransitCallback([](int64_t i, int64_t j) {
          if (i == 0 || j == 7) return 0LL;  // from/to depot
          if (i == 2 && j == 6) return 1LL;  // only feasible pair
          return 10LL;
        });
    model.AddDimension(callback, 2, 2, false, "dim");
    const operations_research::routing::Dimension& dim =
        model.GetDimensionOrDie("dim");
    Solver* solver = model.solver();
    const std::vector<int64_t> pickup_alt = {1, 2, 3};
    const std::vector<int64_t> delivery_alt = {4, 5, 6};
    std::vector<IntVar*> pickup_vehicle_variables;
    std::vector<IntVar*> pickup_cumuls;
    for (int pickup : pickup_alt) {
      pickup_vehicle_variables.push_back(model.VehicleVar(pickup));
      pickup_cumuls.push_back(
          solver->MakeProd(model.ActiveVar(pickup), dim.CumulVar(pickup))
              ->Var());
    }
    std::vector<IntVar*> delivery_vehicle_variables;
    std::vector<IntVar*> delivery_cumuls;
    for (int delivery : delivery_alt) {
      delivery_vehicle_variables.push_back(model.VehicleVar(delivery));
      delivery_cumuls.push_back(
          solver->MakeProd(model.ActiveVar(delivery), dim.CumulVar(delivery))
              ->Var());
    }
    solver->AddConstraint(
        solver->MakeEquality(solver->MakeMax(pickup_vehicle_variables),
                             solver->MakeMax(delivery_vehicle_variables)));
    model.AddPickupAndDeliverySets(model.AddDisjunction(pickup_alt),
                                   model.AddDisjunction(delivery_alt));
    solver->AddConstraint(solver->MakeLessOrEqual(
        solver->MakeSum(pickup_cumuls), solver->MakeSum(delivery_cumuls)));
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // The heuristic should be able to find a solution to the model with
    // pickup and delivery constraints without backtracking, as filtering
    // should be perfect in this case. This corresponds to 35 actual failures
    // which are due to tree search balancing for local search + 0 failures
    // for the first solution strategy.
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    // 2 -> 6 is taken
    EXPECT_EQ(assignment->Value(model.Nexts()[2]), 6);
    // Shortest route is 0-2-6-0 resulting in arc costs of 0 + 2 + 6 = 8.
    EXPECT_EQ(8, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(FirstSolutionHeuristicWithUnaryCallbackTestInstance,
                         FirstSolutionHeuristicWithUnaryCallbackTest,
                         ::testing::Bool());

// Unfeasible model (no room for non-optional node).
TEST_P(PathCheapestArcTest, UnfeasibleModel) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.set_solution_limit(1);
  const bool use_unfiltered_first_solution_strategy =
      ::testing::get<0>(GetParam());
  parameters.set_use_unfiltered_first_solution_strategy(
      use_unfiltered_first_solution_strategy);
  const int kNumNonVehicleNodes = 1;
  const int kNumVehicles = 2;
  const std::vector<NodeIndex> starts = {NodeIndex(1), NodeIndex(2)};
  const std::vector<NodeIndex> ends = {NodeIndex(3), NodeIndex(4)};
  IndexManager manager(kNumNonVehicleNodes + 2 * kNumVehicles, kNumVehicles,
                       starts, ends);
  Model model(manager);
  model.AddConstantDimension(/*value = */ 1, /*capacity = */ 1,
                             /*fix_start_cumul_to_zero=*/true, "Hop");
  const int64_t non_vehicle_node_var_index = manager.NodeToIndex(NodeIndex(0));
  model.ActiveVar(non_vehicle_node_var_index)->SetValue(1);
  const int vehicle_cost =
      ::testing::get<1>(GetParam())
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  EXPECT_EQ(assignment, nullptr);
  // Will fail because heuristic will start by adding to high-index vehicle
  // first.
  const int64_t kExtraFails = use_unfiltered_first_solution_strategy
                                  ? 0
                                  :  // One for filtered decision builder,
                                     // one to backtrack to unfiltered
                                  2;
  const int64_t kBalancingFailures = 36;
  // Tries solving once with flow and once with all-unperformed.
  const int64_t kQuickSolveFailures = 2;
  EXPECT_EQ(kBalancingFailures + kQuickSolveFailures + kExtraFails,
            model.solver()->failures());
  if (use_unfiltered_first_solution_strategy) {
    EXPECT_EQ(0, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(0, model.GetNumberOfRejectsInFirstSolution(parameters));
  } else {
    EXPECT_EQ(6, model.GetNumberOfDecisionsInFirstSolution(parameters));
    EXPECT_EQ(3, model.GetNumberOfRejectsInFirstSolution(parameters));
  }
}

INSTANTIATE_TEST_SUITE_P(PathCheapestArcTestInstance, PathCheapestArcTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

// Test that the nodes are considered by decreasing distance to the depot.
// The test case is a simple model with 2 vehicles and 3 non vehicle nodes.
// Node 0 is the depot for both vehicles, and each can only serve 2 nodes other
// than their depot.
//
//         NODE0 ---1--- NODE1 ---1--- NODE2 ---1--- NODE3
//
// In this example, the first node to be inserted should be NODE3, followed by
// NODE2 on the same vehicle. Then, since the vehicle has reached its capacity,
// NODE1 is inserted on the second vehicle. The cost (distance) is therefore
// (3+1+2) for the first vehicle, and (1+1) for the second, or a total of 8.
//
// If the nodes were inserted in their order of creation in the model (index),
// the first vehicle would have served nodes 1 and 2, and the second node3,
// which would have resulted in a total cost of (1+1+2) + (3+3) = 10.
TEST(LocalCheapestInsertionTest, FarthestNodesFirst) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 2, assignment->ObjectiveValue());
  }
}

TEST(LocalCheapestInsertionTest, FarthestNodesFirstWithSubSolves) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    RoutingSearchParameters secondary_parameters =
        DefaultSecondaryRoutingSearchParameters();
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));
    Model model(manager);
    Model secondary_model(manager);
    for (Model* m : {&model, &secondary_model}) {
      // Maximum 2 non-vehicle nodes per vehicle.
      m->AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                              /*fix_start_cumul_to_zero=*/true, "Load");

      std::vector<std::vector<int64_t>> costs = {
          {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
      const int vehicle_cost = m->RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      m->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    }
    model.SetSecondaryModel(&secondary_model, secondary_parameters);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 2, assignment->ObjectiveValue());
  }
}

// Similar to the above test, with implicit allowed vehicles determined based
// on node demand and vehicle capacity.
// By construction, NODE1 can only be served by vehicle0 and NODE2 by vehicle1.
// The farthest of the two is inserted first (NODE2), then NODE1. NODE3
// is then inserted on the last vehicle (no room left on the other vehicles).
TEST(LocalCheapestInsertionTest, WithImplicitAllowedVehicles) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    parameters.set_log_search(true);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 3;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Add a dimension making Node1 only compatible with vehicle 0.
    model.AddDimensionWithVehicleCapacity(
        model.RegisterUnaryTransitCallback([](int64_t node) {
          if (node == 1) {
            return 100;
          }
          return 1;
        }),
        0, /*vehicle_capacities=*/{101, 10, 10}, true, "Node1Dimension");
    // Add a dimension making Node2 only compatible with vehicle 1.
    model.AddDimensionWithVehicleCapacity(
        model.RegisterUnaryTransitCallback([](int64_t node) {
          if (node == 2) {
            return 100;
          }
          return 1;
        }),
        1, /*vehicle_capacities=*/{10, 101, 10}, true, "Node2Dimension");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(2 + 4 + 6, assignment->ObjectiveValue());
    for (int i = 1; i <= kNumNonVehicleNodes; ++i) {
      EXPECT_EQ(assignment->Value(model.VehicleVar(i)), i - 1);
    }
  }
}

// Same as above with allowed vehicles. NODE1 and NODE2 are constrained to one
// vehicle. The farthest of the two is inserted first (NODE2), then NODE1. NODE3
// is inserted on the cheapest route (the one with NODE2).
TEST(LocalCheapestInsertionTest, WithAllowedVehicles) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    parameters.set_log_search(true);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    // Allowed vehicles.
    model.SetAllowedVehiclesForIndex({0}, 1);
    model.SetAllowedVehiclesForIndex({1}, 2);

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 2, assignment->ObjectiveValue());
  }
}

// Same as above with penalties, with
// penalty of NODE1 > penalty of NODE2 > penalty of NODE3
// which forces the insertion order (NODE1 and NODE2 ending up together).
TEST(LocalCheapestInsertionTest, WithPenalties) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    model.AddDisjunction({1}, 30);
    model.AddDisjunction({2}, 20);
    model.AddDisjunction({3}, 10);

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(4 + 6, assignment->ObjectiveValue());
  }
}

// Same as above with penalties and one allowed vehicle for NODE3
// which forces the insertion order NODE3, NODE1, NODE2.
TEST(LocalCheapestInsertionTest, WithPenaltiesAndAllowedVehicles) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    model.AddDisjunction({1}, 30);
    model.AddDisjunction({2}, 20);
    model.SetAllowedVehiclesForIndex({1}, 3);

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3}, {1, 0, 1, 2}, {2, 1, 0, 1}, {3, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 4, assignment->ObjectiveValue());
  }
}

// Same as above except pairs further from the depot get inserted first.
// {3, 6} gets inserted first, then {2, 5} on the same vehicle, and then {1, 4}
// on the second vehicle.
TEST(LocalCheapestInsertionTest, PickupAndDelivery) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 6;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 pairs per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    model.AddPickupAndDelivery(1, 4);
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 2, assignment->ObjectiveValue());
  }
}

TEST(LocalCheapestInsertionTest, PickupAndDeliveryWithSubSolves) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    RoutingSearchParameters secondary_parameters =
        DefaultSecondaryRoutingSearchParameters();
    const int kNumNonVehicleNodes = 6;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    Model secondary_model(manager);
    // Maximum 2 pairs per vehicle.
    for (Model* m : {&model, &secondary_model}) {
      m->AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                              /*fix_start_cumul_to_zero=*/true, "Load");

      std::vector<std::vector<int64_t>> costs = {
          {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}};
      const int vehicle_cost = m->RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      m->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

      m->AddPickupAndDelivery(1, 4);
      m->AddPickupAndDelivery(2, 5);
      m->AddPickupAndDelivery(3, 6);
    }
    model.SetSecondaryModel(&secondary_model, secondary_parameters);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(6 + 2, assignment->ObjectiveValue());
  }
}

// Similar to the  above except that all costs are in tardiness costs instead of
// arc costs. Only LOCAL_CHEAPEST_COST_INSERTION manages to take that cost into
// account.
// Without arc costs, the pairs are inserted in their order of creation.
TEST(LocalCheapestInsertionTest, NonArcCosts) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 1;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);

    // Make vehicle 0 start "later" which makes it more expensive.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                               /*fix_start_cumul_to_zero=*/false, "Dim");
    Dimension* const dim = model.GetMutableDimension("Dim");
    dim->CumulVar(model.Start(/*vehicle=*/0))->SetValue(1);
    dim->CumulVar(model.Start(/*vehicle=*/1))->SetValue(0);

    dim->SetCumulVarSoftUpperBound(1, /*upper_bound=*/0, /*coefficient=*/10);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    if (strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION) {
      EXPECT_EQ(10, assignment->ObjectiveValue());
    } else {
      EXPECT_EQ(20, assignment->ObjectiveValue());
    }
  }
}

// Same as above except allowed vehicles enforce pair insertion order. An extra
// vehicle was added to ensure a strict ordering of VehicleVar domain sizes
// (node1 is compatible with 1 vehicle, node2 is compatible with 2 and
// node3 is compatible with all 3).
TEST(LocalCheapestInsertionTest, PickupAndDeliveryWithAllowedVehicles) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    for (bool constrain_pickup : {true, false}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.set_first_solution_strategy(strategy);
      parameters.set_solution_limit(1);
      const int kNumNonVehicleNodes = 6;
      const int kNumVehicles = 3;
      IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

      Model model(manager);
      // Maximum 2 pairs per vehicle.
      model.AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                                 /*fix_start_cumul_to_zero=*/true, "Load");

      std::vector<std::vector<int64_t>> costs = {
          {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}};
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

      model.AddPickupAndDelivery(1, 4);
      model.AddPickupAndDelivery(2, 5);
      model.AddPickupAndDelivery(3, 6);

      // Allowed vehicles.
      if (constrain_pickup) {
        model.SetAllowedVehiclesForIndex({0}, 1);
        model.SetAllowedVehiclesForIndex({0, 1}, 2);
      } else {
        model.SetAllowedVehiclesForIndex({0}, 4);
        model.SetAllowedVehiclesForIndex({0, 1}, 5);
      }

      const Assignment* const assignment =
          model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(4 + 6, assignment->ObjectiveValue());
    }
  }
}

TEST(LocalCheapestInsertionTest, PickupAndDeliveryWithPenalties) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 6;
    const int kNumVehicles = 3;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 2 pairs per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    model.AddPickupAndDelivery(1, 4);
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);

    model.AddDisjunction({1}, 40);
    model.AddDisjunction({2}, 20);
    model.AddDisjunction({3}, 30);
    model.AddDisjunction({4}, 0);
    model.AddDisjunction({5}, 15);
    model.AddDisjunction({6}, 0);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(4 + 6, assignment->ObjectiveValue());
  }
}

TEST(LocalCheapestInsertionTest,
     PickupAndDeliveryWithAllowedVehiclesAndPenalties) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    for (bool constrain_pickup : {true, false}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.set_first_solution_strategy(strategy);
      parameters.set_solution_limit(1);
      const int kNumNonVehicleNodes = 6;
      const int kNumVehicles = 3;
      IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

      Model model(manager);
      // Maximum 2 pairs per vehicle.
      model.AddConstantDimension(/*value = */ 1, /*capacity = */ 5,
                                 /*fix_start_cumul_to_zero=*/true, "Load");

      std::vector<std::vector<int64_t>> costs = {
          {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}};
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

      model.AddPickupAndDelivery(1, 4);
      model.AddPickupAndDelivery(2, 5);
      model.AddPickupAndDelivery(3, 6);

      model.AddDisjunction({1}, 30);
      model.AddDisjunction({2}, 20);
      model.AddDisjunction({3}, 40);
      model.AddDisjunction({4}, 0);
      model.AddDisjunction({5}, 15);
      model.AddDisjunction({6}, 0);

      if (constrain_pickup) {
        model.SetAllowedVehiclesForIndex({0}, 1);
        model.SetAllowedVehiclesForIndex({0, 1}, 2);
      } else {
        model.SetAllowedVehiclesForIndex({0}, 4);
        model.SetAllowedVehiclesForIndex({0, 1}, 5);
      }

      const Assignment* const assignment =
          model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(4 + 6, assignment->ObjectiveValue());
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_AllowedVehiclesAndPenaltiesNodeAtTop) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 6;
    // Only two vehicles, forcing to skip some nodes.
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 3 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 4,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);
    // Will be considered first due to allowed vehicles.
    model.AddDisjunction({1}, 1);
    model.AddDisjunction({2}, 20);
    // {3, 6} is considered third due to distance.
    model.AddDisjunction({3}, 30);
    // Will be considered second due to penalty.
    model.AddDisjunction({4}, 35);
    model.AddDisjunction({5}, 10);
    model.AddDisjunction({6}, 0);

    model.SetAllowedVehiclesForIndex({0}, 1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(2 + 6 + 30, assignment->ObjectiveValue());
    EXPECT_EQ(model.Next(*assignment, 2), 2);
    EXPECT_EQ(model.Next(*assignment, 5), 5);
  }
}

TEST(
    LocalCheapestInsertionTest,
    MixedNodeAndPickupAndDelivery_AllowedVehiclesAndPenaltiesNodeSortedWithPenaltyFirst) {  // NOLINT
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    parameters.set_first_solution_strategy(strategy);
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_PENALTY_OVER_ALLOWED_VEHICLES_RATIO);
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::SORTING_PROPERTY_PENALTY);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 6;
    // Only two vehicles, forcing to skip some nodes.
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 3 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 4,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);
    // Will be considered last due to low penalty.
    model.AddDisjunction({1}, 1);
    model.AddDisjunction({2}, 20);
    // {3, 6} is considered second due to distance.
    model.AddDisjunction({3}, 30);
    // Will be considered first due to penalty.
    model.AddDisjunction({4}, 35);
    model.AddDisjunction({5}, 10);
    model.AddDisjunction({6}, 0);

    model.SetAllowedVehiclesForIndex({0}, 1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(3 + 2 + 1 + 2 + 2 + 1, assignment->ObjectiveValue());
    EXPECT_EQ(model.Next(*assignment, 1), 1);
  }
}

TEST(
    LocalCheapestInsertionTest,
    MixedNodeAndPickupAndDelivery_AllowedVehiclesAndPenaltiesNodeSortedWithPenaltyAllowedVehiclesRatio) {  // NOLINT
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    parameters.set_first_solution_strategy(strategy);
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_PENALTY_OVER_ALLOWED_VEHICLES_RATIO);
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::SORTING_PROPERTY_PENALTY);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 6;
    // Only two vehicles, forcing to skip some nodes.
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    // Maximum 3 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 4,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
        {3, 2, 1, 0, 2, 1, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);
    // Will be considered last due to low penalty.
    model.AddDisjunction({1}, 20);
    model.AddDisjunction({2}, 20);
    // {3, 6} is considered second due to distance.
    model.AddDisjunction({3}, 30);
    // Will be considered first due to penalty /allowed vehicles.
    model.AddDisjunction({4}, 35);
    model.AddDisjunction({5}, 10);
    model.AddDisjunction({6}, 0);

    model.SetAllowedVehiclesForIndex({0}, 1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(3 + 2 + 1 + 2 + 2 + 20, assignment->ObjectiveValue());
    EXPECT_EQ(model.Next(*assignment, 1), 1);
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_AllowedVehiclesAndPenaltiesPairAtTop) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    for (bool constrain_pickup : {true, false}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.set_first_solution_strategy(strategy);
      parameters.set_solution_limit(1);
      const int kNumNonVehicleNodes = 6;
      const int kNumVehicles = 2;
      IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

      Model model(manager);
      // Maximum 3 non-vehicle nodes per vehicle.
      model.AddConstantDimension(/*value = */ 1, /*capacity = */ 4,
                                 /*fix_start_cumul_to_zero=*/true, "Load");

      std::vector<std::vector<int64_t>> costs = {
          {0, 1, 2, 3, 1, 2, 3}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}, {1, 0, 1, 2, 0, 1, 2}, {2, 1, 0, 1, 1, 0, 1},
          {3, 2, 1, 0, 2, 1, 0}};
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

      // 1 and 4 are standalone nodes
      model.AddPickupAndDelivery(2, 5);
      model.AddPickupAndDelivery(3, 6);
      // Will be considered second due to allowed vehicles and distance.
      model.AddDisjunction({1}, 30);
      // {2, 5} is considered third, inserted on the second vehicle.
      model.AddDisjunction({2}, 20);
      // Will be considered first, the test is making sure this is not blocking
      // the insertion of single nodes.
      model.AddDisjunction({3}, 30);
      model.AddDisjunction({4}, 30);
      model.AddDisjunction({5}, 15);
      model.AddDisjunction({6}, 0);

      model.SetAllowedVehiclesForIndex({0}, 1);
      if (constrain_pickup) {
        model.SetAllowedVehiclesForIndex({0}, 3);
        model.SetAllowedVehiclesForIndex({0, 1}, 2);
      } else {
        model.SetAllowedVehiclesForIndex({0}, 6);
        model.SetAllowedVehiclesForIndex({0, 1}, 5);
      }

      const Assignment* const assignment =
          model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(6 + 4, assignment->ObjectiveValue());
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_FarthestNodesInsertionFirst) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const int kNumNonVehicleNodes = 6;
  // Only two vehicles, forcing to skip some nodes.
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  const std::vector<std::vector<int64_t>> costs = {
      {0, 8, 9, 9, 7, 9, 9}, {8, 0, 2, 2, 1, 2, 2}, {9, 2, 0, 2, 2, 2, 2},
      {9, 2, 2, 0, 2, 2, 2}, {7, 1, 2, 2, 0, 2, 2}, {9, 2, 2, 2, 2, 0, 2},
      {9, 2, 2, 2, 2, 2, 0}};

  const std::vector<int> kExpectedNexts = {2, 4, 5, 3, 9, 8, 6, 1};

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);

    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_HIGHEST_AVG_ARC_COST_TO_VEHICLE_START_ENDS);

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);

    // All nodes are optional, with some penalty.
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      model.AddDisjunction({node}, 10);
    }

    model.SetAllowedVehiclesForIndex({0}, 2);

    // Defining that 3 and 6 cannot be served by the same vehicle will prevent
    // the nodes from being performed.
    model.SetAllowedVehiclesForIndex({0}, 3);
    model.SetAllowedVehiclesForIndex({1}, 6);

    // The pair {2, 5} will be considered first because among individual nodes
    // and pickup and delivery pairs it is the farthest away from vehicles.
    // This pair will be inserted in vehicle 0 given that 2 is required to be
    // served by that vehicle.
    // This insertion fills vehicle 0 that cannot thus accept additional nodes.
    // Node 1 will then be considered and inserted in vehicle 1.
    // Finally, node 4 will be considered and inserted in vehicle 1 after
    // node 1. Nodes 3 and 6 cannot be inserted because both vehicles are full.
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(9 + 2 + 9 + 8 + 1 + 7 + 10 + 10, assignment->ObjectiveValue());
    for (int node = 0; node < model.Size(); ++node) {
      EXPECT_EQ(model.Next(*assignment, node), kExpectedNexts[node]);
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_ClosestNodesInsertionFirst) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const int kNumNonVehicleNodes = 6;
  // Only two vehicles, forcing to skip some nodes.
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  const std::vector<std::vector<int64_t>> costs = {
      {0, 2, 1, 1, 3, 1, 1}, {2, 0, 2, 2, 1, 2, 2}, {1, 2, 0, 2, 2, 2, 2},
      {1, 2, 2, 0, 2, 2, 2}, {3, 1, 2, 2, 0, 2, 2}, {1, 2, 2, 2, 2, 0, 2},
      {1, 2, 2, 2, 2, 2, 0}};

  const std::vector<int> kExpectedNexts = {2, 4, 5, 3, 9, 8, 6, 1};

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);

    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_LOWEST_AVG_ARC_COST_TO_VEHICLE_START_ENDS);

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);

    // All nodes are optional, with some penalty.
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      model.AddDisjunction({node}, 10);
    }

    model.SetAllowedVehiclesForIndex({0}, 2);

    // Defining that 3 and 6 cannot be served by the same vehicle will prevent
    // the nodes from being performed.
    model.SetAllowedVehiclesForIndex({0}, 3);
    model.SetAllowedVehiclesForIndex({1}, 6);

    // The pair {2, 5} will be considered first because among individual nodes
    // and pickup and delivery pairs it is the closest to vehicles.
    // This pair will be inserted in vehicle 0 given that 2 is required to be
    // served by that vehicle.
    // This insertion fills vehicle 0 that cannot thus accept additional nodes.
    // Node 1 will then be considered and inserted in vehicle 1.
    // Finally, node 4 will be considered and inserted in vehicle 1 after
    // node 1. Nodes 3 and 6 cannot be inserted because both vehicles are full.
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(1 + 2 + 1 + 2 + 1 + 3 + 10 + 10, assignment->ObjectiveValue());
    for (int node = 0; node < model.Size(); ++node) {
      EXPECT_EQ(model.Next(*assignment, node), kExpectedNexts[node]);
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_NodesClosestToNearestVehicleInsertedFirst) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const int kNumNonVehicleNodes = 6;
  // Only two vehicles, forcing to skip some nodes.
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  const std::vector<int> kExpectedNexts = {1, 4, 5, 3, 8, 9, 6, 2};

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);

    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_LOWEST_MIN_ARC_COST_TO_VEHICLE_START_ENDS);

    Model model(manager);
    // Maximum 2 non-vehicle nodes per vehicle.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Load");

    // The above selected LCI insertion property sorts nodes according to their
    // minimum distance from the closest vehicle. This distance is computed as
    // c(vehicle_start -> node) + c(node -> vehicle_end) for individual nodes
    // and c(vehicle_start -> pickup) + c(delivery, vehicle_end) for pickup and
    // delivery pairs. Given the model definition, the cost function below makes
    // sure nodes are considered in increasing index value. That is, in the
    // order: 1, (2, 5), (3, 6), 4, where for pickup and delivery pairs the
    // pickup determines the order given that for all nodes, arcs returning to
    // vehicles have the same cost. Finally, the cost function fixes the cost of
    // arcs (1, 4) and (2, 5) to ensure a determined direction in the resulting
    // solution routes.
    const auto cost = [&model](int64_t from, int64_t to) {
      if (model.IsStart(from)) return static_cast<int>(to);
      if (model.IsEnd(to)) return 50;
      if (from == 1 && to == 4) return 1;
      if (from == 2 && to == 5) return 1;
      return 2;
    };

    const int vehicle0_cost =
        model.RegisterTransitCallback([&cost](int64_t from, int64_t to) {
          // Nodes 2 and 5 have a high cost when considered by vehicle 0.
          if (from == 2 || to == 2) return 100;
          if (from == 5 || to == 5) return 100;
          return cost(from, to);
        });

    const int vehicle1_cost =
        model.RegisterTransitCallback([&cost](int64_t from, int64_t to) {
          // Nodes 1 and 4 have a high cost when considered by vehicle 1.
          if (from == 1 || to == 1) return 100;
          if (from == 4 || to == 4) return 100;
          return cost(from, to);
        });

    model.SetArcCostEvaluatorOfVehicle(vehicle0_cost, 0);
    model.SetArcCostEvaluatorOfVehicle(vehicle1_cost, 1);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(2, 5);
    model.AddPickupAndDelivery(3, 6);

    // All nodes are optional, with some penalty.
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      model.AddDisjunction({node}, 100);
    }

    model.SetAllowedVehiclesForIndex({0}, 1);
    model.SetAllowedVehiclesForIndex({1}, 2);

    // Defining that 3 and 6 cannot be served by the same vehicle will prevent
    // the nodes from being performed.
    model.SetAllowedVehiclesForIndex({0}, 3);
    model.SetAllowedVehiclesForIndex({1}, 6);

    // Given the above defined cost function, individual nodes will be
    // considered in increasing index value, while for pickup and delivery pairs
    // the pickup node index value will determine the ordering. Initially, node
    // 1 is inserted in vehicle 1. Then, the pair (2, 5) is inserted in
    // vehicle 2. Afterwards, the pair (3, 6) is considered. However, it is
    // skipped as 3 is required to be served by vehicle 0, while 6 by vehicle 1.
    // Subsequently, node 4 is inserted after node 1. Finally, when node 6 is
    // considered, it is skipped given that both vehicles are full.
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);

    EXPECT_EQ(1 + 1 + 50 + 2 + 1 + 50 + 100 + 100,
              assignment->ObjectiveValue());
    for (int node = 0; node < model.Size(); ++node) {
      EXPECT_EQ(model.Next(*assignment, node), kExpectedNexts[node]);
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_HighDimensionUsageNodesInsertionFirst) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const int kNumNonVehicleNodes = 6;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  const std::vector<int64_t> kDemands = {0, 1, -2, -3, 4, 5, 6};
  const std::vector<int> kExpectedNexts = {6, 8, 9, 1, 4, 2, 3, 5};

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);

    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::
            SORTING_PROPERTY_HIGHEST_DIMENSION_USAGE);

    Model model(manager);
    model.AddVectorDimension(kDemands, /*capacity = */ 6,
                             /*fix_start_cumul_to_zero=*/true, "Load");

    // Given that we currently only support unary dimensions, add a dummy
    // non-unary dimension to see whether the code still works properly.
    TransitCallback2 time_evaluator = [](int64_t, int64_t) { return 1; };
    model.AddDimension(model.RegisterTransitCallback(time_evaluator), 10, 10,
                       /*fix_start_cumul_to_zero=*/true, "time");

    const int vehicle_cost =
        model.RegisterTransitCallback([&model](int64_t from, int64_t to) {
          if (model.IsStart(from) && (to == 6 || to == 5)) return 1;
          if (from == 6 && to == 3) return 1;
          return 2;
        });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(5, 2);
    model.AddPickupAndDelivery(6, 3);

    // All nodes are optional, with some penalty.
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      model.AddDisjunction({node}, 10);
    }

    model.SetAllowedVehiclesForIndex({0}, 1);
    model.SetAllowedVehiclesForIndex({1}, 5);
    model.SetAllowedVehiclesForIndex({0}, 6);

    // Given the above defined sorting property, individual nodes are considered
    // in non-increasing order based on their demand, while pickup and delivery
    // pairs according to the max between the pickup and the delivery demand.
    // First, the pair (6, 3) is inserted in vehicle 0. Then, the pair
    // (5, 2) is inserted in vehicle 1. Next, node 4 is considered, however
    // serving this node would exceed the total possible load for both vehicles
    // and any insertion position. The node is thus skipped. Finally, node 1 is
    // inserted in vehicle 0.
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);

    EXPECT_EQ(1 + 1 + 2 + 2 + 1 + 2 + 2 + 10, assignment->ObjectiveValue());
    for (int node = 0; node < model.Size(); ++node) {
      EXPECT_EQ(model.Next(*assignment, node), kExpectedNexts[node]);
    }
  }
}

TEST(LocalCheapestInsertionTest,
     MixedNodeAndPickupAndDelivery_RandomOrderInsertion) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const int kNumNonVehicleNodes = 6;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  const std::vector<int64_t> kDemands = {0, 1, -1, -1, 1, 1, 1};

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);

    LocalCheapestInsertionParameters* lci_params =
        strategy == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
            ? parameters.mutable_local_cheapest_insertion_parameters()
            : parameters.mutable_local_cheapest_cost_insertion_parameters();
    lci_params->add_insertion_sorting_properties(
        LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM);

    Model model(manager);
    model.AddVectorDimension(kDemands, /*capacity = */ 6,
                             /*fix_start_cumul_to_zero=*/true, "Load");

    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // 1 and 4 are standalone nodes
    model.AddPickupAndDelivery(5, 2);
    model.AddPickupAndDelivery(6, 3);

    for (int64_t node = 1; node <= kNumNonVehicleNodes; node++) {
      model.AddDisjunction({node}, 1000);
    }

    const int kNumRuns = 10;
    // Since the insertion order is random, we solve the model multiple times.
    for (int i = 0; i < kNumRuns; ++i) {
      const Assignment* const assignment =
          model.SolveWithParameters(parameters);
      model.SolveWithParameters(parameters);
      model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      // Given the node demands and the vehicle capacities, we expect all nodes
      // to be performed regardless of their insertion order.
      for (int node = 0; node < model.Size(); ++node) {
        EXPECT_NE(model.Next(*assignment, node), node);
      }
    }
  }
}

// Make sure we are using the proper cost definitions for each route/vehicle.
TEST(LocalCheapestInsertionTest, PickupAndDeliveryWithNonHomogeneousCosts) {
  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(strategy);
    parameters.set_solution_limit(1);
    const int kNumNonVehicleNodes = 4;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));

    Model model(manager);
    model.AddPickupAndDelivery(1, 2);
    model.AddPickupAndDelivery(3, 4);
    model.SetArcCostEvaluatorOfVehicle(
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          // Favor putting pairs on second route.
          if (to == 1 || to == 3) return 10;
          // Make link 3->4 cheap on first route making it necessary to properly
          // evaluate its cost on the second route.
          if (from == 3 && to == 4) return 0;
          return 1;
        }),
        /*vehicle=*/0);
    model.SetArcCostEvaluatorOfVehicle(
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          // Favor putting pickup (3) at start of second route.
          if (from == 5 && to == 3) return 1;
          if (to == 3) return 10;
          // Make link 3->4 expensive on the second route (cf. comment on same
          // arc in the first route).
          if (from == 3 && to == 4) return 10;
          return 1;
        }),
        /*vehicle=*/1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(5, assignment->ObjectiveValue());
    EXPECT_FALSE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
  }
}

// Tests that nodes part of a pickup/delivery pair which is only partially
// inserted properly get considered for insertion as single nodes.
TEST(LocalCheapestInsertionTest, PickupAndDeliveryFromPartiallyInsertedPair) {
  const int kNumNonVehicleNodes = 2;
  // NOTE: We add 2 vehicles to avoid the preprocessing to auto-insert the
  // non-inserted pickup/delivery.
  const int kNumVehicles = 2;

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  // Disable the CP-SAT fallback.
  parameters.set_use_cp_sat(BOOL_FALSE);
  parameters.set_use_generalized_cp_sat(BOOL_FALSE);
  parameters.set_fallback_to_cp_sat_size_threshold(0);
  const IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles,
                             NodeIndex(0));

  for (auto strategy : {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    parameters.set_first_solution_strategy(strategy);
    for (bool enforce_pickup : {true, false}) {
      Model model(manager);
      model.AddPickupAndDelivery(1, 2);
      if (enforce_pickup) {
        model.NextVar(model.Start(0))
            ->SetValue(manager.NodeToIndex(NodeIndex(1)));
      } else {
        model.NextVar(manager.NodeToIndex(NodeIndex(2)))
            ->SetValue(model.End(0));
      }

      const Assignment* const assignment =
          model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
    }
  }
}

using GlobalCheapestInsertionWithUnaryTransitTest =
    ::testing::TestWithParam<bool>;

// Test with fixed chains of nodes at the start and at the end of the route.
TEST_P(GlobalCheapestInsertionWithUnaryTransitTest, WithFixedStartEndChains) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(3)));
  model.NextVar(manager.NodeToIndex(NodeIndex(1)))->SetValue(model.End(0));
  const int vehicle_cost =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
  EXPECT_EQ(4, assignment->ObjectiveValue());
}

TEST_P(GlobalCheapestInsertionWithUnaryTransitTest,
       WithFixedStartChainsAndPickupDelivery) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 6;
  const int kNumVehicles = 1;
  for (auto policy :
       {Model::PICKUP_AND_DELIVERY_NO_ORDER, Model::PICKUP_AND_DELIVERY_LIFO,
        Model::PICKUP_AND_DELIVERY_FIFO}) {
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    model.SetPickupAndDeliveryPolicyOfVehicle(policy, 0);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(node)),
                                 manager.NodeToIndex(NodeIndex(node + 1)));
    }
    model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(1)));
    model.NextVar(manager.NodeToIndex(NodeIndex(1)))
        ->SetValue(manager.NodeToIndex(NodeIndex(3)));
    const int vehicle_cost =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    EXPECT_EQ(7, assignment->ObjectiveValue());
  }
}

TEST_P(GlobalCheapestInsertionWithUnaryTransitTest,
       WithFixedEndChainsAndPickupDelivery) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 6;
  const int kNumVehicles = 1;
  for (auto policy :
       {Model::PICKUP_AND_DELIVERY_NO_ORDER, Model::PICKUP_AND_DELIVERY_LIFO,
        Model::PICKUP_AND_DELIVERY_FIFO}) {
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    model.SetPickupAndDeliveryPolicyOfVehicle(policy, 0);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(node)),
                                 manager.NodeToIndex(NodeIndex(node + 1)));
    }
    model.NextVar(manager.NodeToIndex(NodeIndex(2)))
        ->SetValue(manager.NodeToIndex(NodeIndex(4)));
    model.NextVar(manager.NodeToIndex(NodeIndex(4)))->SetValue(model.End(0));
    const int vehicle_cost =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    EXPECT_EQ(7, assignment->ObjectiveValue());
  }
}

TEST_P(GlobalCheapestInsertionWithUnaryTransitTest,
       WithFixedStartEndChainsAndPickupDelivery) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 6;
  const int kNumVehicles = 1;
  for (auto policy :
       {Model::PICKUP_AND_DELIVERY_NO_ORDER, Model::PICKUP_AND_DELIVERY_LIFO}) {
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    model.SetPickupAndDeliveryPolicyOfVehicle(policy, 0);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(node)),
                                 manager.NodeToIndex(NodeIndex(node + 1)));
    }
    model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(1)));
    model.NextVar(manager.NodeToIndex(NodeIndex(1)))
        ->SetValue(manager.NodeToIndex(NodeIndex(3)));
    model.NextVar(manager.NodeToIndex(NodeIndex(4)))
        ->SetValue(manager.NodeToIndex(NodeIndex(2)));
    model.NextVar(manager.NodeToIndex(NodeIndex(2)))->SetValue(model.End(0));
    const int vehicle_cost =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    EXPECT_EQ(7, assignment->ObjectiveValue());
  }
}

// Test with non-start/end fixed chain.
TEST_P(GlobalCheapestInsertionWithUnaryTransitTest,
       WithFixedNonStartEndChains) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  parameters.set_fallback_to_cp_sat_size_threshold(0);
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       NodeIndex(0));
  Model model(manager);
  model.NextVar(manager.NodeToIndex(NodeIndex(1)))
      ->SetValue(manager.NodeToIndex(NodeIndex(2)));
  const int vehicle_cost =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  // Currently GlobalCheapestInsertion does not support fixed non-start/end
  // chains, but this test checks it doesn't crash.
  EXPECT_EQ(assignment, nullptr);
}

// Varying penalties, high penalty nodes should get inserted before low
// penalty ones.
TEST_P(GlobalCheapestInsertionWithUnaryTransitTest, NodeDependentPenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      model.AddDisjunction(manager.NodesToIndices({NodeIndex(node)}),
                           10 * node);
    }
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // node 1 unperformed = penalty of 10,
    // node 2 alone on route, 0 -> 2 -> 3 (cost = 0 + 2)
    EXPECT_EQ(10 + 2, assignment->ObjectiveValue());
  }
}

// Same as NodeDependentPenalties test but with nodes in disjunctions.
TEST_P(GlobalCheapestInsertionWithUnaryTransitTest,
       DisjunctionNodeDependentPenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 2 * 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes / 2; ++node) {
      model.AddDisjunction(manager.NodesToIndices(
                               {NodeIndex(node * 2 - 1), NodeIndex(node * 2)}),
                           10 * node);
    }
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // nodes 1/2 unperformed = penalty of 10,
    // node 3 alone on route, 0 -> 3 -> 5 (cost = 0 + 3)
    EXPECT_EQ(10 + 3, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(GlobalCheapestInsertionWithUnaryTransitTestInstance,
                         GlobalCheapestInsertionWithUnaryTransitTest,
                         ::testing::Bool());

// In the case of optional nodes with penalties, when there is equality between
// two NodeEntry values, one inserting the node and one leaving it unperformed,
// prioritize the entry inserting the node.
TEST(GlobalCheapestInsertionTest, PrioritizeInsertionWithNodePenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const int kNumNodes = 2;
  const int kNumVehicles = 1;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 10);
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 5; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_EQ(5 + 5, assignment->ObjectiveValue());
  }
}

// The following tests verify that insertions on empty vehicles are properly
// handled when using the VehicleTypeCurator in the GlobalCheapestInsertion
// heuristic.
//
// This function tests a model with 2 vehicles of the same type (same start/end
// location and same cost class) and the same fixed cost, and 2 nodes (or
// pickup/delivery pairs if 'pickup_deliveries' is true) to insert.
//
// DEPOT -----10----- Node1(+Node3) ------15------ Node2(+Node4)
//
// - In the first scenario, Node2(+Node4) gets inserted on vehicle #1 first
//   since the domain of its VehicleVar is smaller than that of Node1(+Node3).
//   Node1(+Node3) gets inserted on vehicle #1, as it's the cheapest option.
// - In the second scenario where Node1 is not allowed on vehicle #0,
//   Node1(+Node3) gets inserted on vehicle #1, and Node2(+Node4) is inserted on
//   the same vehicle.
// - In the third scenario where Node1 is not allowed on vehicle #0 and Node2 is
//   not allowed on vehicle #1, Node1(+Node3) gets inserted on the second
//   vehicle again, but Node2(+Node4) cannot be on the same route and is
//   therefore inserted on vehicle #0.
void TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndSameFixedCost(
    bool pickup_deliveries, const std::vector<std::vector<int64_t>>& costs) {
  const int kNumNodes = pickup_deliveries ? 5 : 3;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const std::vector<
      std::vector<std::pair</*index*/ int64_t, /*allowed_vehicle*/ int>>>
      kAllowedVehicleScenarios = {
          {{2, 1}}, /* Node2 only allowed on Vehicle1 */
          {{1, 1}}, /* Node1 only allowed on Vehicle1 */
          {{1, 1}, {2, 0}} /* Node1 only allowed on v1 and Node2 only on v0 */};
  const std::vector<int64_t> kExpectedCosts = {50, 50, 70};
  const std::vector<std::vector<std::pair<int64_t, int>>>
      kExpectedNodeVehicles = {
          {{1, 1}, {2, 1}}, {{1, 1}, {2, 1}}, {{1, 1}, {2, 0}}};

  for (int s = 0; s < kAllowedVehicleScenarios.size(); s++) {
    Model model(manager);
    model.SetArcCostEvaluatorOfAllVehicles(model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs)));

    for (const auto& [index, allowed_vehicle] : kAllowedVehicleScenarios[s]) {
      model.SetAllowedVehiclesForIndex({allowed_vehicle}, index);
    }

    if (pickup_deliveries) {
      int pickup_index = 1;
      int delivery_index = 1 + (kNumNodes / 2);
      while (delivery_index < kNumNodes) {
        model.AddPickupAndDelivery(pickup_index, delivery_index);
        pickup_index++;
        delivery_index++;
      }
    }

    model.CloseModelWithParameters(parameters);
    ASSERT_EQ(model.GetVehicleTypeContainer().NumTypes(), 1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCosts[s]);
    for (const auto& [node, vehicle] : kExpectedNodeVehicles[s]) {
      EXPECT_EQ(assignment->Value(model.VehicleVar(node)), vehicle);
    }
  }
}

TEST(GlobalCheapestInsertionTest,
     EmptyVehicleSingleNodeInsertionTest_SameTypeSameFixedCost) {
  const std::vector<std::vector<int64_t>> costs = {
      {0, 10, 25}, {10, 0, 15}, {25, 15, 0}};
  TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndSameFixedCost(
      /*pickup_deliveries*/ false, costs);
}

TEST(GlobalCheapestInsertionTest,
     EmptyVehiclePairInsertionTest_SameTypeSameFixedCost) {
  const std::vector<std::vector<int64_t>> costs = {{0, 10, 25, 10, 25},
                                                   {10, 0, 15, 0, 15},
                                                   {25, 15, 0, 15, 0},
                                                   {10, 0, 15, 0, 15},
                                                   {25, 15, 0, 15, 0}};
  TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndSameFixedCost(
      /*pickup_deliveries*/ true, costs);
}

// The following function tests the GlobalCheapestInsertion heuristic on a model
// with 2 vehicles of the same type (same start/end location and same cost
// class) and 2 nodes (or pairs) to insert, where the first vehicle has a higher
// fixed cost that the second.
//
// DEPOT -----10----- Node1(+Node3) ------15------ Node2(+Node4)
//
// The model has a dimension for which Node1 has a demand of -1 and Node2 a
// demand of +1. Vehicle #1's start cumul is fixed to 0, such that Node1 can
// only be inserted on this route once Node2 is inserted.
// - In the first scenario, Node1 is allowed on both vehicles. Since Node1 can't
//   be inserted before Node2 gets inserted, Node2 gets inserted on the cheapest
//   vehicle #1 first, and Node1 gets inserted after on the same vehicle.
// - In the second scenario, Node1 is only allowed on vehicle #1. Since Node1
//   can't be inserted before Node2 gets inserted, Node2 gets inserted on the
//   cheapest vehicle #1 first, and Node1 gets inserted after on the same
//   vehicle.
// - In the third scenario, Node1 is only allowed on the more expensive vehicle
//   #0, and it gets inserted on this route before Node2 due to the smaller
//   domain size of its VehicleVar; Node2 is also inserted on vehicle #0 to
//   amortize its fixed cost.
void TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndDifferentFixedCost(
    bool pickup_deliveries, const std::vector<std::vector<int64_t>>& costs) {
  const int kNumNodes = pickup_deliveries ? 5 : 3;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const std::vector<std::vector<int>> kAllowedVehicleScenarios = {
      {0, 1}, /* Node1 allowed on both vehicles */
      {1},    /* Node 1 allowed in Vehicle1 */
      {0} /* Node1 only allowed on Vehicle0 */};
  const int kFirstVehicleFixedCost = 1234;
  const std::vector<int64_t> kExpectedCosts = {
      (10 + 15) * 2, (10 + 15) * 2, kFirstVehicleFixedCost + 10 + 15 + 25};
  const std::vector<std::vector<std::pair<int64_t, int>>>
      kExpectedNodeVehicles = {
          {{1, 1}, {2, 1}}, {{1, 1}, {2, 1}}, {{1, 0}, {2, 0}}};

  for (int s = 0; s < kAllowedVehicleScenarios.size(); s++) {
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    Model model(manager);
    model.SetArcCostEvaluatorOfAllVehicles(model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs)));

    model.SetAllowedVehiclesForIndex(kAllowedVehicleScenarios[s], /*index*/ 1);

    model.SetFixedCostOfVehicle(kFirstVehicleFixedCost, 0);
    if (pickup_deliveries) {
      int pickup_index = 1;
      int delivery_index = 1 + (kNumNodes / 2);
      while (delivery_index < kNumNodes) {
        model.AddPickupAndDelivery(pickup_index, delivery_index);
        pickup_index++;
        delivery_index++;
      }
    }

    const std::string kDimensionName = "dim";
    const int node_demands = model.RegisterUnaryTransitCallback([](int index) {
      if (index == 1) {
        return -1;
      }
      if (index == 2) {
        return 1;
      }
      return 0;
    });
    model.AddDimension(node_demands, /*slack_max*/ 0,
                       /*capacity*/ kint64max, false, kDimensionName);
    model.GetMutableDimension(kDimensionName)
        ->SetCumulVarRange(model.Start(1), 0, 0);

    model.CloseModelWithParameters(parameters);
    ASSERT_EQ(model.GetVehicleTypeContainer().NumTypes(), 1);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCosts[s]);
    for (const auto& [node, vehicle] : kExpectedNodeVehicles[s]) {
      EXPECT_EQ(assignment->Value(model.VehicleVar(node)), vehicle);
    }
  }
}

TEST(GlobalCheapestInsertionTest,
     EmptyVehicleSingleNodeInsertionTest_SameTypeDifferentFixedCost) {
  const std::vector<std::vector<int64_t>> costs = {
      {0, 10, 25}, {10, 0, 15}, {25, 15, 0}};
  TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndDifferentFixedCost(
      /*pickup_delivery*/ false, costs);
}

TEST(GlobalCheapestInsertionTest,
     EmptyVehiclePickupDeliveryInsertionTest_SameTypeDifferentFixedCost) {
  const std::vector<std::vector<int64_t>> costs = {{0, 10, 25, 10, 25},
                                                   {10, 0, 15, 0, 15},
                                                   {25, 15, 0, 15, 0},
                                                   {10, 0, 15, 0, 15},
                                                   {25, 15, 0, 15, 0}};
  TestGlobalCheapestInsertionEmptyVehicleWithSameTypeAndDifferentFixedCost(
      /*pickup_delivery*/ true, costs);
}

// Utility class to test the neighbors_ratio in the GlobalCheapestInsertion
// first solution heuristic with single nodes.
//
// The model is as follows:
//
//                            - Node3 --------
//                           1                8
//                          /                  \
// Start0:NODE0 ---2--- Node2 ------ 7 ------ NODE5:End0
//                        \                     /
//                         ---4--- Node4 ---4---
//                                   |
//                                   6
//                                   |
//                              NODE1:Depot1
//
class GlobalCheapestInsertionSingleNeighborsTest : public testing::Test {
 public:
  void SetUp() override {
    const int kNumNodes = 6;
    const int kNumVehicles = 2;
    const auto start_end1 = std::make_pair(NodeIndex(0), NodeIndex(5));
    const auto start_end2 = std::make_pair(NodeIndex(1), NodeIndex(1));
    manager_ = std::make_unique<IndexManager>(
        kNumNodes, kNumVehicles,
        std::vector<std::pair<NodeIndex, NodeIndex>>{start_end1, start_end2});
  }

 protected:
  Model* CreateModel() {
    model_ = std::make_unique<Model>(*manager_);

    const int vehicle_cost = model_->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, *manager_, costs_));
    model_->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    return model_.get();
  }

  std::function<int64_t(int64_t, int64_t, int64_t)> GetCostFunction() {
    return [this](int64_t from, int64_t to, int64_t /*vehicle*/) {
      return GetCostFromMatrix(*manager_, costs_, from, to);
    };
  }

  int NumCheapestInsertionNeighbors(double neighbors_ratio,
                                    int min_neighbors) const {
    const int num_non_start_end_nodes = model_->Size() - model_->vehicles();
    const int num_neighbors_with_ratio =
        MathUtil::Round<int64_t>(neighbors_ratio * num_non_start_end_nodes);
    return std::min(std::max(min_neighbors, num_neighbors_with_ratio),
                    num_non_start_end_nodes - 1);
  }

 private:
  std::unique_ptr<const IndexManager> manager_;
  std::unique_ptr<Model> model_;
  const std::vector<std::vector<int64_t>> costs_ = {{0, 100, 2, 100, 6, 9},
                                                    {1, 0, 100, 11, 6, 10},
                                                    {2, 10, 0, 1, 4, 7},
                                                    {100, 11, 1, 0, 100, 8},
                                                    {6, 6, 100, 5, 0, 4}};
};

// Tests the neighborhoods when updating the insertion positions.
//
// The first node that gets inserted is Node2, on the first vehicle
// (NODE0->Node2->NODE5).
// Then, if we consider all nodes as neighbors, the next best insertion is Node4
// after Node2 (4+4-7=1), so 4 would get inserted after 2, thus filling the
// first vehicle which has a capacity of 2 nodes.
// Node3 would then have to be served by the second vehicle, leading to a total
// cost of 10+(2*11) = 32.
// On the same example, if we restrict the neighborhood to 1 closest neighbor,
// then after inserting Node2, the next best insertion becomes 2-->3 (2->4
// being no longer considered), which leaves Node4 to be inserted on the second
// route, with an overall cost of 11+(2*6) = 23.
TEST_F(GlobalCheapestInsertionSingleNeighborsTest, NeighborhoodInUpdates) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const std::vector<double> kNeighborhoodRatios = {0.2, 0.4, 0.8, 1.0};
  const std::vector<int> kMinNeighbors = {1, 2, 100};
  absl::flat_hash_map<int, int> kNumNeighborsToExpectedCost = {{1, 23},
                                                               {2, 32}};

  for (const double ratio : kNeighborhoodRatios) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_neighbors_ratio(ratio);
    for (const int min_neighbors : kMinNeighbors) {
      parameters.mutable_global_cheapest_insertion_first_solution_parameters()
          ->set_min_neighbors(min_neighbors);

      Model* const model = CreateModel();
      model->AddConstantDimension(/*value=*/1, /*capacity=*/1 + 2,
                                  /*fix_start_cumul_to_zero=*/true, "Load");

      const Assignment* const assignment =
          model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      const int num_neighbors =
          NumCheapestInsertionNeighbors(ratio, min_neighbors);
      EXPECT_EQ(assignment->ObjectiveValue(),
                kNumNeighborsToExpectedCost[num_neighbors]);
      EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
      EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
    }
  }
}

// Same example as before, without the vehicle capacity constraint, and also
// considering the neighborhood when initializing the insertion positions.
// When the neighborhood ratio restricts the number of neighbors to 1, Node4's
// possible insertion positions are either after a vehicle start or after Node3.
// However, when Node3 gets inserted after Node2, Node4 is considered for
// insertion since it's an incoming neighbor of Node4, leading to the optimal
// route NODE0 --> Node2 --> Node4 --> Node3 --> NODE5, with a cost of 19.
// TODO(user): Tweak the test so Node4 is not considered for insertion
// before Node3 when there is a single neighbor.
TEST_F(GlobalCheapestInsertionSingleNeighborsTest,
       NeighborhoodInInitializationAndUpdates) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const std::vector<double> kNeighborhoodRatios = {0.1, 0.2, 0.8, 1.0};
  const std::vector<int> kMinNeighbors = {1, 2, 100};
  const int64_t kExpectedCost = 19;

  GlobalCheapestInsertionParameters gci_params;
  gci_params.set_farthest_seeds_ratio(0);
  gci_params.set_add_unperformed_entries(false);

  for (const double ratio : kNeighborhoodRatios) {
    gci_params.set_neighbors_ratio(ratio);
    for (const int min_neighbors : kMinNeighbors) {
      gci_params.set_min_neighbors(min_neighbors);
      for (const bool use_neighbors_ratio_for_initialization : {true, false}) {
        SCOPED_TRACE(absl::StrCat("ratio = ", ratio,
                                  ", min_neighbors = ", min_neighbors,
                                  ", use_neighbors_ratio_for_initialization = ",
                                  use_neighbors_ratio_for_initialization));
        gci_params.set_use_neighbors_ratio_for_initialization(
            use_neighbors_ratio_for_initialization);

        Model* const model = CreateModel();
        model->CloseModelWithParameters(parameters);

        GlobalCheapestInsertionFilteredHeuristic gci_heuristic(
            model, []() { return false; }, GetCostFunction(),
            [](int64_t) { return 10000; }, {}, gci_params,
            /*is_sequential=*/false);

        const Assignment* assignment = gci_heuristic.BuildSolution();
        ASSERT_NE(assignment, nullptr);

        assignment =
            model->SolveFromAssignmentWithParameters(assignment, parameters);

        ASSERT_NE(assignment, nullptr);
        EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost);
      }
    }
  }
}

// Same as the previous examples, but with pickup and delivery pairs instead of
// single nodes.
//
//                            - Node3,6 --------
//                           1                 8
//                          /                   \
// Start1:NODE0 ---2--- Node2,5 ------ 7 ------ NODE8:End1
//                        \                       /
//                         ---4--- Node4,7 ---4---
//                                   |
//                                   6
//                                   |
//                              NODE1:Depot1
//
class GlobalCheapestInsertionPickupDeliveryNeighborsTest
    : public testing::Test {
 public:
  void SetUp() override {
    const int kNumNodes = 9;
    const int kNumVehicles = 2;
    const auto start_end1 = std::make_pair(NodeIndex(0), NodeIndex(8));
    const auto start_end2 = std::make_pair(NodeIndex(1), NodeIndex(1));
    manager_ = std::make_unique<IndexManager>(
        kNumNodes, kNumVehicles,
        std::vector<std::pair<NodeIndex, NodeIndex>>{start_end1, start_end2});
  }

 protected:
  Model* CreateModel() {
    model_ = std::make_unique<Model>(*manager_);

    const int vehicle_cost = model_->RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, *manager_, costs_));
    model_->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    for (int node = 2; node < 5; node++) {
      model_->AddPickupAndDelivery(manager_->NodeToIndex(NodeIndex(node)),
                                   manager_->NodeToIndex(NodeIndex(node + 3)));
    }

    return model_.get();
  }

  int NumCheapestInsertionNeighbors(double neighbors_ratio,
                                    int min_neighbors) const {
    const int num_non_start_end_nodes = model_->Size() - model_->vehicles();
    const int num_neighbors_with_ratio =
        MathUtil::Round<int64_t>(neighbors_ratio * num_non_start_end_nodes);
    return std::min(std::max(min_neighbors, num_neighbors_with_ratio),
                    num_non_start_end_nodes - 1);
  }

  const IndexManager* manager() const { return manager_.get(); }

  std::function<int64_t(int64_t, int64_t, int64_t)> GetCostFunction() {
    return [this](int64_t from, int64_t to, int64_t /*vehicle*/) {
      return GetCostFromMatrix(*manager_, costs_, from, to);
    };
  }

 private:
  std::unique_ptr<const IndexManager> manager_;
  std::unique_ptr<Model> model_;
  const std::vector<std::vector<int64_t>> costs_ = {
      {0, 100, 2, 100, 6, 2, 100, 6, 9},  {1, 0, 100, 11, 6, 100, 11, 1, 10},
      {2, 10, 0, 1, 4, 0, 1, 4, 7},       {100, 11, 1, 0, 100, 1, 0, 100, 8},
      {6, 6, 100, 5, 0, 100, 5, 0, 4},    {2, 10, 0, 1, 4, 0, 1, 4, 7},
      {100, 11, 1, 0, 100, 1, 0, 100, 8}, {6, 6, 100, 5, 0, 100, 5, 0, 4}};
};

// In this example, the neighborhood is taken into account in the insertion
// updates only (not during initialization).
TEST_F(GlobalCheapestInsertionPickupDeliveryNeighborsTest,
       PickupAndDeliveriesWithNeighborhoodRatio) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const std::vector<double> kNeighborhoodRatios = {0.3, 0.4, 0.7, 0.8, 1.0};
  const std::vector<int> kMinNeighbors = {1, 2, 5, 100};
  absl::flat_hash_map<int, int> kNumNeighborsToExpectedCost = {
      {2, 23}, {4, 32}, {5, 32}};

  for (const double ratio : kNeighborhoodRatios) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_neighbors_ratio(ratio);
    for (const int min_neighbors : kMinNeighbors) {
      parameters.mutable_global_cheapest_insertion_first_solution_parameters()
          ->set_min_neighbors(min_neighbors);

      Model* model = CreateModel();
      model->AddConstantDimension(/*value=*/1, /*capacity=*/1 + 2 * 2,
                                  /*fix_start_cumul_to_zero=*/true, "Load");

      const Assignment* assignment = model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      const int num_neighbors =
          NumCheapestInsertionNeighbors(ratio, min_neighbors);
      EXPECT_EQ(assignment->ObjectiveValue(),
                kNumNeighborsToExpectedCost[num_neighbors]);
      EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
      EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
    }
  }
}

// In this example, the neighborhood is taken into account in the insertion
// updates and during initialization.
// When taking a single neighbor into account for initialization, since each
// pickup/delivery's closest node is its respective delivery/pickup node, each
// pickup/delivery pair can only be inserted right after a vehicle start (as all
// nodes are connected to vehicle start/ends), resulting in the routes
// NODE0 --> 3 --> 6 --> 2 --> 5 --> NODE8 and Depot1 --> 4 --> 7 --> Depot1,
// with a cost of 120.
// When 2 or 3 neighbors are considered, pickup/delivery pairs are no longer
// necessarily consecutive, leading to the following routes with cost 23:
// NODE0 --> 2 --> 3 --> 6 --> 5 --> NODE8 and Depot1 --> 4 --> 7 --> Depot1.
// For 4 neighbors and above, the best solution is
// NODE0 --> 2 --> 3 --> 6 --> 5 --> 4 --> 7 --> NODE8, with a cost of 12.
TEST_F(GlobalCheapestInsertionPickupDeliveryNeighborsTest,
       PickupAndDeliveriesWithNeighborhoodRatioInInitializationAndUpdates) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);

  const std::vector<double> kNeighborhoodRatios = {0.2, 0.35, 0.5,
                                                   0.7, 0.8,  1.0};
  const std::vector<int> kMinNeighbors = {1, 2, 4, 5, 100};
  absl::flat_hash_map<int, int> kNumNeighborsToExpectedCost = {
      {1, 120}, {2, 23}, {3, 23}, {4, 12}, {5, 12}};

  GlobalCheapestInsertionParameters gci_params;
  gci_params.set_farthest_seeds_ratio(0);
  gci_params.set_add_unperformed_entries(
      parameters.global_cheapest_insertion_first_solution_parameters()
          .add_unperformed_entries());

  for (const double ratio : kNeighborhoodRatios) {
    gci_params.set_neighbors_ratio(ratio);
    for (const int min_neighbors : kMinNeighbors) {
      gci_params.set_min_neighbors(min_neighbors);
      for (const bool use_neighbors_ratio_for_initialization : {true, false}) {
        SCOPED_TRACE(absl::StrCat("ratio = ", ratio,
                                  ", min_neighbors = ", min_neighbors,
                                  ", use_neighbors_ratio_for_initialization = ",
                                  use_neighbors_ratio_for_initialization));
        gci_params.set_use_neighbors_ratio_for_initialization(
            use_neighbors_ratio_for_initialization);

        Model* const model = CreateModel();
        model->CloseModelWithParameters(parameters);

        GlobalCheapestInsertionFilteredHeuristic gci_heuristic(
            model, []() { return false; }, GetCostFunction(),
            [](int64_t) { return 10000; }, {}, gci_params,
            /*is_sequential=*/false);

        const Assignment* assignment = gci_heuristic.BuildSolution();
        ASSERT_NE(assignment, nullptr);

        assignment =
            model->SolveFromAssignmentWithParameters(assignment, parameters);

        ASSERT_NE(assignment, nullptr);
        const int num_neighbors =
            NumCheapestInsertionNeighbors(ratio, min_neighbors);
        EXPECT_EQ(assignment->ObjectiveValue(),
                  kNumNeighborsToExpectedCost[num_neighbors]);
      }
    }
  }
}

// Exact same example as before, except Node2 is constrained to be the first
// node on the first vehicle.
// In this case, nodes 2 and 5 are inserted on the first route during the
// initialization. Then, when initializing other pair positions, we consider all
// existing positions, so we will consider the link 2->4, no matter the
// neighborhood_ratio.
//
//                            - Node3,6 --------
//                           1                 8
//                          /                   \
// Start1:NODE0 ---2--- Node2,5 ------ 7 ------ NODE8:End1
//                        \                       /
//                         ---4--- Node4,7 ---4---
//                                   |
//                                   6
//                                   |
//                              NODE1:Depot1
//
TEST_F(GlobalCheapestInsertionPickupDeliveryNeighborsTest,
       PickupAndDeliveriesWithNeighborhoodRatioAndFixedStartChain) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const int kExpectedCost = 32;

  for (const double neighbors_ratio : {0.2, 0.25, 0.4, 0.8, 1.0}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_neighbors_ratio(neighbors_ratio);

    Model* model = CreateModel();

    // Forcing 0->2 link.
    model->NextVar(model->Start(0))
        ->SetValue(manager()->NodeToIndex(NodeIndex(2)));

    model->AddConstantDimension(/*value=*/1, /*capacity=*/1 + 2 * 2,
                                /*fix_start_cumul_to_zero=*/true, "Load");

    const Assignment* assignment = model->SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost);
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
  }
}

// Same as before, with Node2 constrained to be the first node on the first
// vehicle, and Node7 the last node on the second.
// In this case, node 4 is necessarily inserted on the second vehicle during
// initialization, so node 3 will get inserted on the first route regardless of
// the neighborhood_ratio.
//
//                            - Node3,6 --------
//                           1                 8
//                          /                   \
// Start1:NODE0 ---2--- Node2,5 ------ 7 ------ NODE8:End1
//                        \                       /
//                         ---4--- Node4,7 ---4---
//                                   |
//                                   6
//                                   |
//                              NODE1:Depot1
//
TEST_F(GlobalCheapestInsertionPickupDeliveryNeighborsTest,
       PickupAndDeliveriesWithNeighborhoodRatioAndFixedStartOrEndChains) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const int kExpectedCost = 23;

  for (const double neighbors_ratio : {0.2, 0.25, 0.4, 0.8, 1.0}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_neighbors_ratio(neighbors_ratio);

    Model* model = CreateModel();

    // Forcing links Start(0)->2 and 7->Start(1).
    model->NextVar(model->Start(0))
        ->SetValue(manager()->NodeToIndex(NodeIndex(2)));
    model->NextVar(manager()->NodeToIndex(NodeIndex(7)))
        ->SetValue(model->End(1));

    model->AddConstantDimension(/*value=*/1, /*capacity=*/1 + 2 * 2,
                                /*fix_start_cumul_to_zero=*/true, "Load");

    const Assignment* assignment = model->SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost);
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
  }
}

// b/183246952: Small test to make sure that GlobalCheapestInsertion doesn't
// crash when the neighbor of a pickup or delivery is made unperformed by
// the heuristic.
// The following example has 2 pickup/delivery pairs and one vehicle.
// We try to build a solution, fixing the first pickup to be at the beginning
// of the route.
// The first delivery can't be inserted on the vehicle (demand too high), so it
// becomes unperformed, and the second pair cannot get inserted because the only
// neighbors for every node are the first delivery and the vehicle start.
TEST(GlobalCheapestInsertionPickupDeliveryInsertionWithNeighborTest,
     PickupAndDeliveryWithNeighborMadeUnperformed) {
  const int kNumNodes = 5;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Setup pickup/deliveries.
  const int kPenalty = 1000;
  for (int node = 1; node < kNumNodes; node += 2) {
    model.AddPickupAndDeliverySets(model.AddDisjunction({node}, kPenalty),
                                   model.AddDisjunction({node + 1}, kPenalty));
  }

  // Setup costs.
  const std::vector<std::vector<int64_t>> costs = {{0, 99, 99, 99, 99},
                                                   {99, 0, 1, 99, 99},
                                                   {99, 1, 0, 1, 1},
                                                   {99, 99, 1, 0, 99},
                                                   {99, 99, 1, 99, 0}};
  model.SetArcCostEvaluatorOfAllVehicles(model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs)));

  // Add a dimension making the insertion of the delivery impossible.
  model.AddDimension(model.RegisterUnaryTransitCallback(
                         [](int node) { return node == 2 ? 1 : 0; }),
                     kint64max, /*capacity*/ 0, true, "dim");

  // Add the first pickup to the route and make its delivery optional.
  model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(1)));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_add_unperformed_entries(true);
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_neighbors_ratio(0.001);
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_min_neighbors(1);
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_use_neighbors_ratio_for_initialization(true);

  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(model.Next(*assignment, 0), 1);
  EXPECT_EQ(model.Next(*assignment, 1), 5);
}

// Check that the sequential GlobalCheapestInsertion first solution heuristic
// works as intended.
// The example chosen is a model with two vehicles, each having their own
// separate depot (nodes 0 and 4), and 3 nodes to be served.
//
//             NODE1 -----------------------6
//           /     |   \                    |
//        1 /      |      3                 |
//         /       |         \              |
// DEPOT(NODE0)----|--3--- NODE2 ---2--- DEPOT(NODE4)
//     \           |      /                 |
//      \          6     /                  |
//       \         |    /                   |
//        \        |   / 5                  |
//         \       |  /                     |
//          \      | /                      |
//           \     |/                       |
//            5--- NODE3 -------------------7
//
// In this example, the first node chosen to start a route is NODE1 (in both
// parallel and sequential versions), which has the smallest distance to the
// first depot.
//
// Then, in the sequential version, the route is extended as much as possible by
// iteratively adding the cheapest nodes at their cheapest position on this
// route. At this stage, the value of the insertion for NODE2 after NODE0 or
// NODE1 is 3+3-1 = 5, whereas for NODE3 it is 6+6-1=11. NODE2 is therefore
// added first, after NODE0 (lower index) : NODE0 --> NODE2 --> NODE1 --> NODE0
// Then the best insertion for NODE3 is after NODE0 with a value of 5+5-3 = 7,
// so NODE3 is inserted after NODE0:
// NODE0 --> NODE3 --> NODE2 --> NODE1 --> NODE0, cost = 5+5+3+1 = 14.
//
// However, in the parallel version, all routes are considered simultaneously,
// so after inserting NODE1 after NODE0, the cheapest insertion overall is
// for NODE2 after NODE4, with a value of 2+2 = 4. We therefore have two routes:
// NODE0 --> NODE1 --> NODE0, and NODE4 --> NODE2 --> NODE4.
// The insertion value of NODE3 is then equal to 10 on both routes (6+5-1 on the
// first and 5+7-2 on the second), so it is inserted between NODE0 and NODE1.
// In this case, the total cost is (5+6+1) + (2+2) = 16.
TEST(GlobalCheapestInsertionTest, SequentialRouteConstruction) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 2;
  const auto& start_end1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto& start_end2 = std::make_pair(NodeIndex(4), NodeIndex(4));
  IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                       {start_end1, start_end2});

  std::vector<std::vector<int64_t>> costs = {{0, 1, 3, 5, 5},
                                             {1, 0, 3, 6, 6},
                                             {3, 3, 0, 5, 2},
                                             {5, 6, 5, 0, 7},
                                             {5, 6, 2, 7, 0}};

  auto model = std::make_unique<Model>(manager);
  const int vehicle_cost = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION);
  const Assignment* assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(5 + 5 + 3 + 1, assignment->ObjectiveValue());
  EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
  EXPECT_FALSE(model->IsVehicleUsed(*assignment, 1));

  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  model = std::make_unique<Model>(manager);
  const int vehicle_cost_2 = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost_2);
  assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(5 + 6 + 1 + 2 + 2, assignment->ObjectiveValue());
  EXPECT_TRUE(model->IsVehicleUsed(*assignment, 0));
  EXPECT_TRUE(model->IsVehicleUsed(*assignment, 1));
}

// The test verifies that node penalties are correctly taken into account by the
// sequential route construction in GlobalCheapestInsertion.
// In the following example there are 3 nodes to insert, NODE3 having a penalty
// cost, and 2 vehicles with a depot respectively at NODE0 and NODE1.
//
//
//                     120---- NODE4
//                     |        |
// NODE0(Depot0)--25--NODE3     |
//                      \      150
//                       \      |
//                        130- NODE2
//                              |
//                              15
//                              |
//                         NODE1(Depot1)
//
// The first node to get inserted is NODE2 on the second vehicle.
// When the penalty cost of NODE3 is < 100, this node is not considered for
// insertion on this route, as its cheapest insertion cost on this route is
// between NODE2 and NODE4 at a cost of 100. So in this case the 2 vehicles are
// used.
// When NODE3 has a penalty cost >= 100, this node is considered for insertion
// on NODE2's route and thus a single route is used for all 3 nodes.
TEST(GlobalCheapestInsertionTest, SequentialRouteConstructionWithNodePenalty) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles,
                             {std::make_pair(NodeIndex(0), NodeIndex(0)),
                              std::make_pair(NodeIndex(1), NodeIndex(1))});

  const int64_t inf = kint64max;
  const std::vector<std::vector<int64_t>> costs = {
      {0, inf, 155, 25, 145, 0, inf},
      {inf, 0, 15, 145, 165, inf, 0},
      {155, 15, 0, 130, 150, 155, 15},
      {25, 145, 130, 0, 120, 25, 145},
      {145, 165, 150, 120, 0, 145, 165}};

  const int penalty_limit = 100;
  for (int64_t penalty : std::initializer_list<int64_t>{0, 25, 50, 75, 99, 100,
                                                        150, 100000, inf}) {
    for (bool add_unperformed_entries : {true, false}) {
      SCOPED_TRACE(absl::StrCat(
          "penalty = ", penalty,
          ", add_unperformed_entries = ", add_unperformed_entries));
      Model model(manager);
      model.SetArcCostEvaluatorOfAllVehicles(model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs)));
      model.AddDisjunction({3}, penalty);

      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.mutable_global_cheapest_insertion_first_solution_parameters()
          ->set_add_unperformed_entries(add_unperformed_entries);
      parameters.set_solution_limit(1);
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION);
      const Assignment* assignment = model.SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      if (penalty < penalty_limit) {
        // We expect NODE3 to be on the first route.
        EXPECT_EQ(50 + 330, assignment->ObjectiveValue());
        EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
        EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
      } else {
        // We expect all 3 nodes to be on the same route.
        EXPECT_EQ(15 + 130 + 120 + 150 + 15, assignment->ObjectiveValue());
        EXPECT_FALSE(model.IsVehicleUsed(*assignment, 0));
        EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
      }
    }
  }
}

// Test that the cheapest_insertion_farthest_seeds_ratio parameter works as
// intended.
// In the example below, we have two vehicles, both having NODE0 as their
// depot and a capacity of 2 additional nodes. Without adding a farthest seed,
// the two first nodes to be selected are NODE1 and NODE2, and are served by the
// first vehicle, with a cost of 3+4+5 = 12. The vehicle having now reached its
// capacity, the third and only remaining node is then placed on the second
// vehicle, costing 9+9 = 18. The total cost is therefore 12 + 18 = 30.
//
//        NODE1
//       /     \
//      /         4
//     3              \
//    /                   \
//  NODE0 ------ 5 ------ NODE2
//    |                       /
//    |                     /
//    |                   /
//    |                 6
//    |               /
//    |             /
//    9--------- NODE3
//
//
// In the same model, when we start by adding the farthest node from the depot
// as a seed node, the first route is initialized with NODE3, and then completed
// by adding NODE2, costing 9+6+5 = 20. The second vehicle is then used to serve
// NODE0, in turn costing 3+3 = 6. The expected total cost in this case is then
// 20+6 = 26.
TEST(GlobalCheapestInsertionTest, InsertFarthestNodeAsSeed) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));
  std::vector<std::vector<int64_t>> costs = {
      {0, 3, 5, 9}, {3, 0, 4, 12}, {5, 4, 0, 6}, {9, 12, 6, 0}};

  auto model = std::make_unique<Model>(manager);
  const int vehicle_cost = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model->AddConstantDimension(/*value = */ 1, /*capacity = */ 1 + 2,
                              /*fix_start_cumul_to_zero=*/true, "Load");
  const Assignment* assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(12 + 18, assignment->ObjectiveValue());

  // Insert farthest seeds on 50% of the vehicles.
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_farthest_seeds_ratio(0.5);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  model = std::make_unique<Model>(manager);
  const int vehicle_cost_2 = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost_2);
  model->AddConstantDimension(/*value = */ 1, /*capacity = */ 1 + 2,
                              /*fix_start_cumul_to_zero=*/true, "Load");
  assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(20 + 6, assignment->ObjectiveValue());
}

// The sequential version of Global Cheapest Insertion should yield the same
// results on the previous example.
TEST(GlobalCheapestInsertionTest, SequentialInsertionWithFarthestNodeAsSeed) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION);
  const int kNumNonVehicleNodes = 3;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));
  std::vector<std::vector<int64_t>> costs = {
      {0, 3, 5, 9}, {3, 0, 4, 12}, {5, 4, 0, 6}, {9, 12, 6, 0}};

  auto model = std::make_unique<Model>(manager);
  const int vehicle_cost = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model->AddConstantDimension(/*value = */ 1, /*capacity = */ 1 + 2,
                              /*fix_start_cumul_to_zero=*/true, "Load");
  const Assignment* assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(12 + 18, assignment->ObjectiveValue());

  // Insert farthest seeds on 50% of the vehicles.
  parameters.mutable_global_cheapest_insertion_first_solution_parameters()
      ->set_farthest_seeds_ratio(0.5);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION);
  model = std::make_unique<Model>(manager);
  const int vehicle_cost_2 = model->RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost_2);
  model->AddConstantDimension(/*value = */ 1, /*capacity = */ 1 + 2,
                              /*fix_start_cumul_to_zero=*/true, "Load");
  assignment = model->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(20 + 6, assignment->ObjectiveValue());
}

using GlobalCheapestInsertionWithUnaryCallbackTest =
    ::testing::TestWithParam<bool>;

// Mix optional and mandatory nodes, madatory nodes have to be inserted.
TEST_P(GlobalCheapestInsertionWithUnaryCallbackTest,
       MixOptionalMandatoryNodes) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    // Optional
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 10);
    // Mandatory
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), kint64max);
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // node 1 unperformed = penalty of 10,
    // node 2 alone on route, 0 -> 2 -> 3 (cost = 0 + 2)
    EXPECT_EQ(10 + 2, assignment->ObjectiveValue());
  }
}

// More mandatory nodes than what can fit.
TEST_P(GlobalCheapestInsertionWithUnaryCallbackTest,
       OverConstrainedMandatoryNodes) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    EXPECT_EQ(assignment, nullptr);
  }
}

// Varying penalties for pairs, high penalty pairs should get inserted before
// low penalty ones. Only pickups have penalties.
TEST_P(GlobalCheapestInsertionWithUnaryCallbackTest, PickupDependentPenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 4;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(node))}, 100 * node);
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(node + 1))}, 0);
      model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(node)),
                                 manager.NodeToIndex(NodeIndex(node + 1)));
    }
    // Only one pair can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // pair (1,2) unperformed = penalty of 100,
    // pair (3,4) alone on route, 0 -> 3 -> 4 -> 5 (cost = 0 + 3 + 4)
    EXPECT_EQ(100 + 7, assignment->ObjectiveValue());
  }
}

// Varying penalties for pairs, high penalty pairs should get inserted before
// low penalty ones. Only deliveries have penalties.
TEST_P(GlobalCheapestInsertionWithUnaryCallbackTest,
       DeliveryDependentPenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 4;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    SCOPED_TRACE(add_unperformed_entries);
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      const int64_t pickup = manager.NodeToIndex(NodeIndex(node));
      const int64_t delivery = manager.NodeToIndex(NodeIndex(node + 1));
      model.AddDisjunction({pickup}, 0);
      model.AddDisjunction({delivery}, 100 * node);
      model.AddPickupAndDelivery(pickup, delivery);
      model.solver()->AddConstraint(model.solver()->MakeEquality(
          model.VehicleVar(pickup), model.VehicleVar(delivery)));
    }
    // Only one pair can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // pair (1,2) unperformed = penalty of 100,
    // pair (3,4) alone on route, 0 -> 3 -> 4 -> 5 (cost = 0 + 3 + 4)
    EXPECT_EQ(100 + 7, assignment->ObjectiveValue());
  }
}

// Varying penalties for pairs, high penalty pairs should get inserted before
// low penalty ones. Both pickup and deliveries have penalties.
TEST_P(GlobalCheapestInsertionWithUnaryCallbackTest, PairDependentPenalties) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);
  const int kNumNonVehicleNodes = 4;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                             NodeIndex(0));

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes; node += 2) {
      std::vector<int64_t> index = {manager.NodeToIndex(NodeIndex(node))};
      model.AddDisjunction(index, 100 * node);
      std::vector<int64_t> index_2 = {manager.NodeToIndex(NodeIndex(node + 1))};
      model.AddDisjunction(index_2, 100 * node);
      model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(node)),
                                 manager.NodeToIndex(NodeIndex(node + 1)));
    }
    // Only one pair can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // pair (1,2) unperformed = penalty of 200,
    // pair (3,4) alone on route, 0 -> 3 -> 4 -> 5 (cost = 0 + 3 + 4)
    EXPECT_EQ(200 + 7, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(GlobalCheapestInsertionWithUnaryCallbackTestInstance,
                         GlobalCheapestInsertionWithUnaryCallbackTest,
                         testing::Bool());

// Tests that the GlobalCheapestInsertion heuristic correctly inserts nodes
// based on the topological order of their type in the requirement graph.
// In this example, the model has a single vehicle and two non start/end nodes
// to visit: Node1 of type 1 and policy "TYPE_ADDED_TO_VEHICLE", and Node2 of
// type 2 and policy "TYPE_ON_VEHICLE_UP_TO_VISIT".
// Type 1 requires type 2 when it's being added to the vehicle.
//
// START/END           NODE1(Door)                     NODE2(Key)
//
// When the topological order of the nodes is not considered, NODE1 is
// considered for insertion first, and cannot be inserted because the key is not
// on the vehicle yet. Node2 then gets inserted, and Node1 cannot go after it
// (and has already been declined after the start), so it'll be unperformed.
// When considering the requirement topological order, Node2 gets inserted
// before Node1, so both nodes get inserted.
TEST(GlobalCheapestInsertionTest, RequiredNodeInsertedFirst) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    model.SetVisitType(1, 1, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(2, 2, VisitTypePolicy::TYPE_ON_VEHICLE_UP_TO_VISIT);
    model.AddRequiredTypeAlternativesWhenAddingType(1, {2});

    std::vector<std::vector<int64_t>> costs = {
        {0, 5, 10}, {5, 0, 5}, {10, 5, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->Value(model.NextVar(1)), 2);
  }
}

// Similar to the above test, but verifies that a required node in a
// pickup/delivery pair is inserted before a dependent pickup/delivery pair.
TEST(GlobalCheapestInsertionTest, RequiredPairInsertedFirst) {
  const int kNumNodes = 5;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);
    Model model(manager);
    model.AddPickupAndDelivery(1, 2);
    model.SetVisitType(1, 1, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(2, 1, VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE);
    model.AddPickupAndDelivery(3, 4);
    model.SetVisitType(3, 3, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(4, 3, VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE);
    // The second pickup/delivery pair requires the first pair.
    model.AddRequiredTypeAlternativesWhenAddingType(3, {1});
    model.AddRequiredTypeAlternativesWhenRemovingType(3, {1});

    // Make the second pair closer to the depot so it gets picked for insertion
    // first without the requirement topological order.
    std::vector<std::vector<int64_t>> costs = {{0, 100, 100, 1, 1},
                                               {100, 0, 100, 10, 10},
                                               {100, 100, 0, 10, 10},
                                               {1, 10, 10, 0, 10},
                                               {1, 10, 10, 10, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected route: START-->1-->3-->4-->2-->END
    EXPECT_EQ(assignment->Value(model.NextVar(1)), 3);
    EXPECT_EQ(assignment->Value(model.NextVar(3)), 4);
    EXPECT_EQ(assignment->Value(model.NextVar(4)), 2);
  }
}

// Simple test with one vehicle and two nodes: One zero-penalty node and one
// mandatory node, with the zero-penalty node required by the mandatory node.
// When we add entries for making nodes unperformed, the zero-penalty node
// is made unperformed by the heuristic first, so the mandatory node then cannot
// be inserted.
// When nodes aren't made unperformed, the required zero-penalty node gets
// inserted, so the mandatory node can also be inserted after it.
TEST(GlobalCheapestInsertionTest, PenaltyZeroNodeRequired) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);

    Model model(manager);
    // Set a penalty 0 for node 1.
    model.AddDisjunction({1}, /*penalty*/ 0);
    // Make node 1 required for node 2.
    model.SetVisitType(1, 1, Model::TYPE_ON_VEHICLE_UP_TO_VISIT);
    model.SetVisitType(2, 2, Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    model.AddRequiredTypeAlternativesWhenAddingType(2, {1});
    // Set a cost for performing nodes.
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 10; }));

    const Assignment* const assignment = model.SolveWithParameters(parameters);
    if (add_unperformed_entries) {
      EXPECT_EQ(assignment, nullptr);
    } else {
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(assignment->Value(model.NextVar(2)), 1);
    }
  }
}

// Tests than pickup/delivery nodes aren't inserted as single nodes in the
// second part of the heuristic if they couldn't be inserted as pairs, as this
// results in solutions rejected by the solver.
TEST(GlobalCheapestInsertionTest, PickupDeliveriesInsertedAsPairsOnly) {
  const int kNumNodes = 5;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  for (bool add_unperformed_entries : {true, false}) {
    parameters.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_add_unperformed_entries(add_unperformed_entries);

    Model model(manager);
    // Add 2 pickup/delivery pairs.
    model.AddPickupAndDelivery(1, 2);
    model.AddPickupAndDelivery(3, 4);
    // Make pair 3/4 optional.
    model.AddDisjunction({3}, /*penalty*/ 10);
    model.AddDisjunction({4}, /*penalty*/ 10);
    // Make the two pickups incompatible.
    model.SetVisitType(1, 1, Model::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(3, 3, Model::TYPE_ADDED_TO_VEHICLE);
    model.AddHardTypeIncompatibility(1, 3);

    const Assignment* const assignment = model.SolveWithParameters(parameters);

    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), 2 * 10);
  }
}

// Tests that precedence constraints are correctly considered.
TEST(GlobalCheapestInsertionTest, WithPrecedenceConstraints) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  Model model(manager);
  model.AddConstantDimension(
      /*value=*/1, /*capacity=*/10, /*fix_start_cumul_to_zero=*/true,
      /*name=*/"count_with_offset");
  Dimension* dimension = model.GetMutableDimension("count_with_offset");
  dimension->AddNodePrecedence(4, 3, /*offset=*/1);
  dimension->AddNodePrecedence(3, 2, /*offset=*/1);
  dimension->AddNodePrecedence(3, 1, /*offset=*/2);

  // Make node 3 closer to node 2 so it gets inserted first
  // after it without considering the offsets.
  std::vector<std::vector<int64_t>> costs = {{0, 0, 0, 1, 1},
                                             {1, 0, 1, 1, 1},
                                             {1, 1, 0, 1, 1},
                                             {1, 1, 1, 0, 1},
                                             {1, 1, 1, 1, 0}};
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // Expected routes: START-->4-->3-->2-->1-->END and START-->END
  ASSERT_TRUE(model.IsVehicleUsed(*assignment, 0));
  ASSERT_FALSE(model.IsVehicleUsed(*assignment, 1));
  EXPECT_EQ(assignment->Value(model.NextVar(4)), 3);
  EXPECT_EQ(assignment->Value(model.NextVar(3)), 2);
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 1);
}

// Tests that the requirement tightness is correctly considered when inserting
// nodes of the same topological level.
TEST(GlobalCheapestInsertionTest, RequirementTightnessConsidered) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  Model model(manager);
  for (int node = 1; node < kNumNodes; node++) {
    model.SetVisitType(node, node, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  }

  // Node3 requires Node1 or Node2.
  model.AddRequiredTypeAlternativesWhenAddingType(3, {1, 2});
  // Node4 requires Node1.
  model.AddRequiredTypeAlternativesWhenAddingType(4, {1});
  // Node1 and Node2 are incompatible.
  model.AddHardTypeIncompatibility(1, 2);

  // Make node 3 closer to the first node so it gets inserted first
  // after it without considering the requirement tightness.
  std::vector<std::vector<int64_t>> costs = {{0, 0, 0, 1, 100},
                                             {1, 0, 1, 1, 100},
                                             {1, 1, 0, 100, 100},
                                             {1, 1, 1, 0, 1},
                                             {1, 1, 1, 1, 0}};
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddConstantDimension(/*value*/ 1, /*capacity*/ 3,
                             /*fix_start_cumul_to_zero*/ true, "dim");

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.set_solution_limit(1);

  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // Expected routes: START-->1-->4-->END and START-->2-->3-->END
  ASSERT_TRUE(model.IsVehicleUsed(*assignment, 0));
  ASSERT_TRUE(model.IsVehicleUsed(*assignment, 1));
  EXPECT_EQ(assignment->Value(model.NextVar(1)), 4);
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 3);
}

using ChristofidesWithUnaryCallbackTest = ::testing::TestWithParam<bool>;

// Test with start fixed chain.
TEST_P(ChristofidesWithUnaryCallbackTest, WithFixedStartChain) {
  for (bool use_minimum_matching : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
    parameters.set_solution_limit(1);
    parameters.set_christofides_use_minimum_matching(use_minimum_matching);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 1;
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    // Forcing 0->3 link.
    model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(3)));
    const int vehicle_cost =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    EXPECT_EQ(4, assignment->ObjectiveValue());
  }
}

// Test with fixed route.
TEST_P(ChristofidesWithUnaryCallbackTest, WithFixedRoute) {
  for (bool use_minimum_matching : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
    parameters.set_solution_limit(1);
    parameters.set_christofides_use_minimum_matching(use_minimum_matching);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    // Forcing 0->3->4 route.
    model.NextVar(model.Start(0))->SetValue(manager.NodeToIndex(NodeIndex(3)));
    model.NextVar(manager.NodeToIndex(NodeIndex(3)))->SetValue(model.End(0));
    const int vehicle_cost =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(/* tree balancing failure */ 35, model.solver()->failures());
    EXPECT_EQ(6, assignment->ObjectiveValue());
  }
}

// Overflowing arcs for matching algorithm.
TEST(ChristofidesTest, OverflowingArcs) {
  for (bool use_minimum_matching : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
    parameters.set_solution_limit(1);
    parameters.set_christofides_use_minimum_matching(use_minimum_matching);
    const int kNumNonVehicleNodes = 2;
    const int kNumVehicles = 1;
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    const int cost =
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 1 && to == 2) return kint64max - 3;
          return int64_t{1};
        });
    model.SetArcCostEvaluatorOfAllVehicles(cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected route: 0-1-2-3 with a cost of kint64max - 1.
    EXPECT_EQ(kint64max - 1, assignment->ObjectiveValue());
  }
}

// Optional nodes which cannot all be performed.
TEST_P(ChristofidesWithUnaryCallbackTest, OptionalNodes) {
  for (bool use_minimum_matching : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
    parameters.set_solution_limit(1);
    parameters.set_christofides_use_minimum_matching(use_minimum_matching);
    const int kNumNonVehicleNodes = 2;
    const int kNumVehicles = 1;
    IndexManager manager(kNumNonVehicleNodes + kNumVehicles, kNumVehicles,
                         NodeIndex(0));
    Model model(manager);
    for (int node = 1; node <= kNumNonVehicleNodes; ++node) {
      std::vector<int64_t> index = {manager.NodeToIndex(NodeIndex(node))};
      model.AddDisjunction(index, 10 * node);
    }
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // node 2 unperformed = penalty of 20,
    // node 1 alone on route, 0 -> 1 -> 3 (cost = 0 + 1)
    EXPECT_EQ(20 + 1, assignment->ObjectiveValue());
  }
}

TEST_P(ChristofidesWithUnaryCallbackTest, MultipleVehicles) {
  for (bool use_minimum_matching : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
    parameters.set_solution_limit(1);
    parameters.set_christofides_use_minimum_matching(use_minimum_matching);
    const int kNumNonVehicleNodes = 3;
    const int kNumVehicles = 3;
    IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));
    Model model(manager);
    // Only one non-start-end node can fit on route.
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 2,
                               /*fix_start_cumul_to_zero=*/true, "Count");
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndexUnary, manager))
                   : model.RegisterTransitCallback(absl::bind_front(
                         ReturnFirstNodeValueFromVariableIndex, manager));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    // Expected value:
    // nodes 1, 2, 3 are each on a route
    EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(ChristofidesWithUnaryCallbackTestInstance,
                         ChristofidesWithUnaryCallbackTest, testing::Bool());

// The Christofides algorithm assumes a complete graph and cannot cope with
// forbidden arcs. This test makes sure the failure is handled gracefully (no
// crash), and that all nodes end up unperformed.
TEST(ChristofidesTest, ForbiddenArc) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(FirstSolutionStrategy::CHRISTOFIDES);
  parameters.set_solution_limit(1);
  parameters.set_christofides_use_minimum_matching(true);
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback([&manager](int i,
                                                                    int j) {
    const int64_t cost[][4] = {{0, 0, 0, 0},
                               {0, 0, 1, kint64max / 2},
                               {0, 1, 0, 1},
                               {0, kint64max / 2, 1, 0}};
    return cost[manager.IndexToNode(i).value()][manager.IndexToNode(j).value()];
  });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  for (int i = 0; i < model.Size(); ++i) {
    if (model.IsStart(i) || model.IsEnd(i)) continue;
    model.AddDisjunction({i}, 1000);
  }
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(3 * 1000, assignment->ObjectiveValue());
  for (int i = 0; i < model.Size(); ++i) {
    if (model.IsStart(i) || model.IsEnd(i)) continue;
    EXPECT_EQ(i, assignment->Value(model.NextVar(i)));
  }
}

TEST(FirstSolutionStrategyTest, AutomaticStrategy) {
  std::vector<std::function<void(Model * model)>> model_modifiers = {
      [](Model* /*model*/) {},
      [](Model* model) { model->AddPickupAndDelivery(1, 2); },
      [](Model* model) { model->AddConstantDimension(1, 1, true, "dim"); },
      [](Model* model) {
        model->AddConstantDimension(1, 1, true, "dim");
        model->GetMutableDimension("dim")->AddNodePrecedence({1, 2, 10});
      }};
  std::vector<FirstSolutionStrategy::Value> expected_strategy = {
      FirstSolutionStrategy::PATH_CHEAPEST_ARC,
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
      FirstSolutionStrategy::PATH_CHEAPEST_ARC,
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION};
  for (int i = 0; i < model_modifiers.size(); ++i) {
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
    EXPECT_EQ(FirstSolutionStrategy::UNSET,
              model.GetAutomaticFirstSolutionStrategy());
    model_modifiers[i](&model);
    model.CloseModelWithParameters(DefaultRoutingSearchParameters());
    EXPECT_EQ(expected_strategy[i], model.GetAutomaticFirstSolutionStrategy());
  }
}

TEST(InsertionTest, MustAllBeUnperformed) {
  const FirstSolutionStrategy::Value kInsertionHeuristics[] = {
      FirstSolutionStrategy::BEST_INSERTION,
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
      FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION};
  const bool kExpectSolution[] = {true, true, true};
  const int64_t kOptionalNodeCost = 1000000L;
  for (int insertion = 0; insertion < std::size(kInsertionHeuristics);
       ++insertion) {
    IndexManager manager(3, 1, NodeIndex(0));
    Model model(manager);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(kInsertionHeuristics[insertion]);
    const std::vector<int64_t> dim_values = {0, 49, 2};
    model.AddVectorDimension(dim_values, 50, true, "dim");
    model.AddConstantDimension(1, 5, true, "sequence");
    Dimension* const sequence_dimension = model.GetMutableDimension("sequence");
    const NodeIndex pickup_node(1);
    const NodeIndex dropoff_node(2);
    const int64_t pickup_index = manager.NodeToIndex(pickup_node);
    const int64_t dropoff_index = manager.NodeToIndex(dropoff_node);
    Solver* const solver = model.solver();
    solver->AddConstraint(solver->MakeEquality(
        model.VehicleVar(pickup_index), model.VehicleVar(dropoff_index)));
    solver->AddConstraint(
        solver->MakeLess(sequence_dimension->CumulVar(pickup_index),
                         sequence_dimension->CumulVar(dropoff_index)));
    model.AddPickupAndDelivery(manager.NodeToIndex(pickup_node),
                               manager.NodeToIndex(dropoff_node));
    model.AddDisjunction(manager.NodesToIndices({pickup_node}),
                         kOptionalNodeCost);
    model.AddDisjunction(manager.NodesToIndices({dropoff_node}),
                         kOptionalNodeCost);
    const Assignment* solution = model.SolveWithParameters(parameters);
    if (kExpectSolution[insertion]) {
      ASSERT_NE(solution, nullptr);
      EXPECT_EQ(kOptionalNodeCost * 2, solution->ObjectiveValue());
    } else {
      EXPECT_EQ(solution, nullptr);
    }
  }
}

}  // namespace
}  // namespace operations_research::routing
