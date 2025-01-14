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

#ifndef OR_TOOLS_UTIL_CACHED_LOG_H_
#define OR_TOOLS_UTIL_CACHED_LOG_H_

#include <cstdint>
#include <vector>

namespace operations_research {
// This class is used when manipulating search space estimations.  It
// provides fast access to log of a domain size.
// Future Extensions:
//   - Sum of log on an array.
//   - Sum of log on an array with callback.

class CachedLog {
 public:
  CachedLog();

  // This type is neither copyable nor movable.
  CachedLog(const CachedLog&) = delete;
  CachedLog& operator=(const CachedLog&) = delete;

  ~CachedLog();

  // This method can only be called once, and with a cache_size > 0.
  void Init(int cache_size);

  // Returns the log2 of 'input'.
  double Log2(int64_t input) const;

 private:
  std::vector<double> cache_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_CACHED_LOG_H_
