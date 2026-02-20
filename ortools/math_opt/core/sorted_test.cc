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

#include "ortools/math_opt/core/sorted.h"

#include "absl/container/flat_hash_map.h"
#include "google/protobuf/map.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(SortedElementsTest, Empty) {
  EXPECT_THAT(SortedSetElements<int>({}), IsEmpty());
}

TEST(SortedElementsTest, NotEmpty) {
  EXPECT_THAT(SortedSetElements<int>({
                  2,
                  4,
                  6,
                  1,
              }),
              ElementsAre(1, 2, 4, 6));
}

TEST(MapKeysTest, Empty) {
  EXPECT_THAT(MapKeys(absl::flat_hash_map<int, double>()), IsEmpty());
  EXPECT_THAT(MapKeys(google::protobuf::Map<int, double>()), IsEmpty());
}

TEST(MapKeysTest, NotEmpty) {
  EXPECT_THAT(
      MapKeys(absl::flat_hash_map<int, double>{{1, 2.0}, {0, 3.0}, {3, 4.0}}),
      UnorderedElementsAre(0, 1, 3));
  {
    google::protobuf::Map<int, double> proto_map;
    proto_map[1] = 2.0;
    proto_map[0] = 3.0;
    proto_map[3] = 4.0;
    EXPECT_THAT(MapKeys(proto_map), UnorderedElementsAre(0, 1, 3));
  }
}

TEST(SortedMapKeysTest, Empty) {
  EXPECT_THAT(SortedMapKeys(absl::flat_hash_map<int, double>()), IsEmpty());
  EXPECT_THAT(SortedMapKeys(google::protobuf::Map<int, double>()), IsEmpty());
}

TEST(SortedMapKeysTest, NotEmpty) {
  EXPECT_THAT(SortedMapKeys(absl::flat_hash_map<int, double>{
                  {1, 2.0}, {0, 3.0}, {3, 4.0}}),
              ElementsAre(0, 1, 3));
  {
    google::protobuf::Map<int, double> proto_map;
    proto_map[1] = 2.0;
    proto_map[0] = 3.0;
    proto_map[3] = 4.0;
    EXPECT_THAT(SortedMapKeys(proto_map), ElementsAre(0, 1, 3));
  }
}

}  // namespace
}  // namespace operations_research::math_opt
