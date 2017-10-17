// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_CLEANUP_H_
#define OR_TOOLS_BASE_CLEANUP_H_

#include <utility>

namespace operations_research {
namespace util {

template <typename F>
class Cleanup {
 public:
  template <typename G>
  explicit Cleanup(G&& f)
      : f_(std::forward<G>(f)) {}
  ~Cleanup() { f_(); }

 private:
  F f_;
};

template <typename F>
Cleanup<F> MakeCleanup(F&& f) {
  return Cleanup<F>(std::forward<F>(f));
}

}  // namespace util
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_CLEANUP_H_
