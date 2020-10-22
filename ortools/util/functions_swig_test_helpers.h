// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_UTIL_FUNCTIONS_SWIG_TEST_HELPERS_H_
#define OR_TOOLS_UTIL_FUNCTIONS_SWIG_TEST_HELPERS_H_

// These are simple static methods to test std::function in the swig wrapper
// tests.
//
// NOTE: It was simpler to use static methods of a public class rather than
// simple static methods; because the Java wrapping of the latter made them hard
// to find (whereas the class methods are easy to find).

#include <functional>
#include <string>
#include <utility>

#include "ortools/base/integral_types.h"

namespace operations_research {
class FunctionSwigTestHelpers {
 public:
  static std::string NoOpVoidToString(std::function<std::string()> fun) {
    return fun();
  }

  static int64 NoOpInt64ToInt64(std::function<int64(int64)> fun, int64 x) {
    return fun(x);
  }

  static int64 NoOpInt64PairToInt64(std::function<int64(int64, int64)> fun,
                                    int64 x, int64 y) {
    return fun(x, y);
  }

  static int64 NoOpIntToInt64(std::function<int64(int)> fun, int x) {
    return fun(x);
  }

  static int64 NoOpIntPairToInt64(std::function<int64(int, int)> fun, int x,
                                  int y) {
    return fun(x, y);
  }

  static int64 NoOpInt64TripleToInt64(
      std::function<int64(int64, int64, int64)> fun, int64 x, int64 y,
      int64 z) {
    return fun(x, y, z);
  }

  static bool NoOpInt64TripleToBool(
      std::function<bool(int64, int64, int64)> fun, int64 x, int64 y, int64 z) {
    return fun(x, y, z);
  }

  static bool NoOpInt64ToBool(std::function<bool(int64)> fun, int64 x) {
    return fun(x);
  }

  static bool NoOpVoidToBool(std::function<bool()> fun) { return fun(); }

  static void NoOpInt64ToVoid(std::function<void(int64)> fun, int64 x) {
    fun(x);
  }

  static void NoOpVoidToVoid(std::function<void()> fun) { fun(); }

  static void NoOpStringToVoid(std::function<void(std::string)> fun,
                               std::string x) {
    fun(x);
  }
};

class DelayedFunctionSwigTestHelpers {
 public:
  explicit DelayedFunctionSwigTestHelpers(
      std::function<int64(int64, int64)> fun)
      : fun_(fun) {}

  int64 NoOpInt64PairToInt64(int64 x, int64 y) { return fun_(x, y); }

 private:
  const std::function<int64(int64, int64)> fun_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_FUNCTIONS_SWIG_TEST_HELPERS_H_
