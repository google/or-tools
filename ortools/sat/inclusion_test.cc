// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/inclusion.h"

#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

TEST(InclusionDetectorTest, SymmetricExample) {
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);
  detector.AddPotentialSet(storage.Add({1, 2}));
  detector.AddPotentialSet(storage.Add({1, 3}));
  detector.AddPotentialSet(storage.Add({1, 2, 3}));
  detector.AddPotentialSet(storage.Add({1, 4, 3, 2}));

  std::vector<std::pair<int, int>> included;
  detector.DetectInclusions([&included](int subset, int superset) {
    included.push_back({subset, superset});
  });
  EXPECT_THAT(included,
              ::testing::ElementsAre(std::make_pair(0, 2), std::make_pair(1, 2),
                                     std::make_pair(0, 3), std::make_pair(1, 3),
                                     std::make_pair(2, 3)));
}

// If sets are duplicates, we do not detect both inclusions, but just one.
TEST(InclusionDetectorTest, DuplicateBehavior) {
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);
  detector.AddPotentialSet(storage.Add({1, 2}));
  detector.AddPotentialSet(storage.Add({1, 2}));
  detector.AddPotentialSet(storage.Add({1, 2}));
  detector.AddPotentialSet(storage.Add({1, 2}));

  std::vector<std::pair<int, int>> included;
  detector.DetectInclusions([&included](int subset, int superset) {
    included.push_back({subset, superset});
  });
  EXPECT_THAT(included, ::testing::ElementsAre(
                            std::make_pair(0, 1), std::make_pair(0, 2),
                            std::make_pair(1, 2), std::make_pair(0, 3),
                            std::make_pair(2, 3), std::make_pair(1, 3)));
}

TEST(InclusionDetectorTest, NonSymmetricExample) {
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);

  // Index 0, 1, 2
  detector.AddPotentialSubset(storage.Add({1, 2}));
  detector.AddPotentialSubset(storage.Add({1, 3}));
  detector.AddPotentialSubset(storage.Add({1, 2, 3}));

  // Index 3, 4, 5, 6
  detector.AddPotentialSuperset(storage.Add({1, 2}));
  detector.AddPotentialSuperset(storage.Add({1, 4, 3}));
  detector.AddPotentialSuperset(storage.Add({1, 4, 3}));
  detector.AddPotentialSuperset(storage.Add({1, 5, 2, 3}));

  std::vector<std::pair<int, int>> included;
  detector.DetectInclusions([&included](int subset, int superset) {
    included.push_back({subset, superset});
  });
  EXPECT_THAT(included, ::testing::ElementsAre(
                            std::make_pair(0, 3), std::make_pair(1, 4),
                            std::make_pair(1, 5), std::make_pair(0, 6),
                            std::make_pair(2, 6), std::make_pair(1, 6)));

  // Class can be used multiple time.
  // Here we test exclude a subset for appearing twice.
  included.clear();
  detector.DetectInclusions([&detector, &included](int subset, int superset) {
    included.push_back({subset, superset});
    detector.StopProcessingCurrentSubset();
  });
  EXPECT_THAT(included,
              ::testing::ElementsAre(std::make_pair(0, 3), std::make_pair(1, 4),
                                     std::make_pair(2, 6)));

  // Here we test exclude a superset for appearing twice.
  included.clear();
  detector.DetectInclusions([&detector, &included](int subset, int superset) {
    included.push_back({subset, superset});
    detector.StopProcessingCurrentSuperset();
  });
  EXPECT_THAT(included, ::testing::ElementsAre(
                            std::make_pair(0, 3), std::make_pair(1, 4),
                            std::make_pair(1, 5), std::make_pair(0, 6)));

  // Here we stop on first match.
  included.clear();
  detector.DetectInclusions([&detector, &included](int subset, int superset) {
    included.push_back({subset, superset});
    detector.Stop();
  });
  EXPECT_THAT(included, ::testing::ElementsAre(std::make_pair(0, 3)));
}

TEST(InclusionDetectorTest, InclusionChain) {
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);
  detector.AddPotentialSet(storage.Add({1}));
  detector.AddPotentialSet(storage.Add({1, 2}));
  detector.AddPotentialSet(storage.Add({1, 2, 3}));

  std::vector<std::pair<int, int>> included;
  detector.DetectInclusions([&included](int subset, int superset) {
    included.push_back({subset, superset});
  });
  EXPECT_THAT(included,
              ::testing::ElementsAre(std::make_pair(0, 1), std::make_pair(0, 2),
                                     std::make_pair(1, 2)));

  // If we stop processing a superset that can also be a subset, it should
  // not appear as such.
  included.clear();
  detector.DetectInclusions([&](int subset, int superset) {
    detector.StopProcessingCurrentSuperset();
    included.push_back({subset, superset});
  });
  EXPECT_THAT(included, ::testing::ElementsAre(std::make_pair(0, 1),
                                               std::make_pair(0, 2)));
}

// We just check that nothing crashes.
TEST(InclusionDetectorTest, RandomTest) {
  absl::BitGen random;
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);

  std::vector<int> temp;
  for (int i = 0; i < 1000; ++i) {
    temp.clear();
    const int size = absl::Uniform<int>(random, 0, 100);
    for (int j = 0; j < size; ++j) {
      temp.push_back(absl::Uniform<int>(random, 0, 10000));
    }
    if (absl::Bernoulli(random, 0.5)) {
      detector.AddPotentialSet(storage.Add(temp));
    } else {
      if (absl::Bernoulli(random, 0.5)) {
        detector.AddPotentialSubset(storage.Add(temp));
      } else {
        detector.AddPotentialSuperset(storage.Add(temp));
      }
    }
  }

  int num_inclusions = 0;
  detector.DetectInclusions(
      [&num_inclusions](int subset, int superset) { ++num_inclusions; });
}

TEST(InclusionDetectorTest, AlternativeApi) {
  CompactVectorVector<int> storage;
  TimeLimit time_limit;
  InclusionDetector detector(storage, &time_limit);

  // Lets add some subset.
  detector.AddPotentialSubset(storage.Add({1, 2}));
  detector.AddPotentialSubset(storage.Add({4, 3}));
  detector.AddPotentialSubset(storage.Add({1, 2, 3}));
  detector.AddPotentialSubset(storage.Add({2, 3}));
  detector.IndexAllSubsets();

  // Now we can query any "superset".
  // Note that there is no guarantee on the order of discovery.
  std::vector<int> included;
  int index = 0;
  detector.FindSubsets({2, 3, 4}, &index, [&](int subset) {
    included.push_back(subset);
    detector.StopProcessingCurrentSubset();  // This will remove them.
  });
  EXPECT_THAT(included, ::testing::UnorderedElementsAre(1, 3));

  // Now because we removed sets, we only get others.
  included.clear();
  index = 0;
  detector.FindSubsets({1, 2, 3, 4}, &index,
                       [&](int subset) { included.push_back(subset); });
  EXPECT_THAT(included, ::testing::UnorderedElementsAre(0, 2));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
