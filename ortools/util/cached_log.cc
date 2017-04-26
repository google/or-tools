// Copyright 2010-2014 Google
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


#include "ortools/util/cached_log.h"

#include "ortools/base/logging.h"

namespace operations_research {

CachedLog::CachedLog() {}

CachedLog::~CachedLog() {}

namespace {
double FastLog2(int64 input) {
  #if defined(_MSC_VER) || defined(__ANDROID__)
  return log(static_cast<double>(input)) / log(2.0L);
#else
  return log2(input);
#endif
}
}  // namespace

void CachedLog::Init(int size) {
  CHECK(cache_.empty());
  CHECK_GT(size, 0);
  cache_.resize(size, 0.0);
  for (int i = 0; i < size; ++i) {
    cache_[i] = FastLog2(i + 1);
  }
}

double CachedLog::Log2(int64 input) const {
  CHECK_GE(input, 1);
  if (input <= cache_.size()) {
    return cache_[input - 1];
  } else {
    return FastLog2(input);
  }
}

}  // namespace operations_research
