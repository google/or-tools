// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_SORTED_H_
#define OR_TOOLS_MATH_OPT_STORAGE_SORTED_H_

#include <algorithm>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/map.h"

namespace operations_research::math_opt {

template <typename T>
std::vector<T> SortedElements(const absl::flat_hash_set<T>& elements) {
  std::vector<T> result(elements.begin(), elements.end());
  absl::c_sort(result);
  return result;
}

template <typename K, typename V>
std::vector<K> MapKeys(const absl::flat_hash_map<K, V>& in_map) {
  std::vector<K> keys;
  keys.reserve(in_map.size());
  for (const auto& key_pair : in_map) {
    keys.push_back(key_pair.first);
  }
  return keys;
}

template <typename K, typename V>
std::vector<K> MapKeys(const google::protobuf::Map<K, V>& in_map) {
  std::vector<K> keys;
  keys.reserve(in_map.size());
  for (const auto& key_pair : in_map) {
    keys.push_back(key_pair.first);
  }
  return keys;
}

template <typename K, typename V>
std::vector<K> SortedMapKeys(const absl::flat_hash_map<K, V>& in_map) {
  std::vector<K> keys = MapKeys(in_map);
  std::sort(keys.begin(), keys.end());
  return keys;
}

template <typename K, typename V>
std::vector<K> SortedMapKeys(const google::protobuf::Map<K, V>& in_map) {
  std::vector<K> keys = MapKeys(in_map);
  std::sort(keys.begin(), keys.end());
  return keys;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_SORTED_H_
