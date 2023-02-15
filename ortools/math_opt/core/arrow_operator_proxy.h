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

#ifndef OR_TOOLS_MATH_OPT_CORE_ARROW_OPERATOR_PROXY_H_
#define OR_TOOLS_MATH_OPT_CORE_ARROW_OPERATOR_PROXY_H_

#include <utility>  // IWYU pragma: keep

namespace operations_research {
namespace math_opt {
namespace internal {

// Proxy used to implement operator-> on iterators that return temporary
// objects.
//
// The operator-> in C++ is interpreted by a drill-down operation that looks for
// a pointer. It is not possible to return a value. So this class is used as a
// proxy to return a pointer from a value.
template <typename T>
class ArrowOperatorProxy {
 public:
  explicit ArrowOperatorProxy(T value) : value_(std::move(value)) {}
  const T* operator->() const { return &value_; }

 private:
  T value_;
};

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_ARROW_OPERATOR_PROXY_H_
