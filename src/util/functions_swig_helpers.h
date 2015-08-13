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

#ifndef OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_
#define OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_

// This file contains class definitions for the wrapping of C++ std::functions
// in Java and C#. It is #included by java/functions.swig and
// csharp/functions.swig.

#include <functional>
#include <string>

#include "base/integral_types.h"

namespace operations_research {
namespace swig_util {
class LongToLong {
 public:
  virtual ~LongToLong() {}
  virtual int64 run(int64) = 0;
#if !defined(SWIG)
  std::function<int64(int64)> GetFunction() {
    return [this](int64 i) { return run(i); };
  }
#endif
};

class LongLongToLong {
 public:
  virtual ~LongLongToLong() {}
  virtual int64 run(int64, int64) = 0;
#if !defined(SWIG)
  std::function<int64(int64, int64)> GetFunction() {
    return [this](int64 i, int64 j) { return run(i, j); };
  }
#endif
};

class IntIntToLong {
 public:
  virtual ~IntIntToLong() {}
  virtual int64 run(int, int) = 0;
#if !defined(SWIG)
  std::function<int64(int, int)> GetFunction() {
    return [this](int i, int j) { return run(i, j); };
  }
#endif
};

class LongLongLongToLong {
 public:
  virtual ~LongLongLongToLong() {}
  virtual int64 run(int64, int64, int64) = 0;
#if !defined(SWIG)
  std::function<int64(int64, int64, int64)> GetFunction() {
    return [this](int64 i, int64 j, int64 k) { return run(i, j, k); };
  }
#endif
};

class LongToBoolean {
 public:
  virtual ~LongToBoolean() {}
  virtual bool run(int64) = 0;
#if !defined(SWIG)
  std::function<bool(int64)> GetFunction() {
    return [this](int64 i) { return run(i); };
  }
#endif
};

class VoidToString {
 public:
  virtual ~VoidToString() {}
  virtual std::string run() = 0;
#if !defined(SWIG)
  std::function<std::string()> GetFunction() {
    return [this]() { return run(); };
  }
#endif
};

class VoidToBoolean {
 public:
  virtual ~VoidToBoolean() {}
  virtual bool run() = 0;
#if !defined(SWIG)
  std::function<bool()> GetFunction() {
    return [this]() { return run(); };
  }
#endif
};

class LongLongLongToBoolean {
 public:
  virtual ~LongLongLongToBoolean() {}
  virtual bool run(int64 i, int64 j, int64 k) = 0;
#if !defined(SWIG)
  std::function<bool(int64, int64, int64)> GetFunction() {
    return [this](int64 i, int64 j, int64 k) { return run(i, j, k); };
  }
#endif
};

class LongToVoid {
 public:
  virtual ~LongToVoid() {}
  virtual void run(int64 i) = 0;
#if !defined(SWIG)
  std::function<void(int64)> GetFunction() {
    return [this](int64 i) { run(i); };
  }
#endif
};

class VoidToVoid {
 public:
  virtual ~VoidToVoid() {}
  virtual void run() = 0;
#if !defined(SWIG)
  std::function<void()> GetFunction() {
    return [this]() { run(); };
  }
#endif
};
}  // namespace swig_util
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_
