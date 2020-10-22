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

#ifndef OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_
#define OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_

// This file contains class definitions for the wrapping of C++ std::functions
// in Java. It is #included by java/functions.i.

#include <functional>
#include <string>

#include "ortools/base/integral_types.h"

namespace operations_research {
namespace swig_util {
class LongToLong {
 public:
  virtual ~LongToLong() {}
  virtual int64 Run(int64) = 0;
};

class LongLongToLong {
 public:
  virtual ~LongLongToLong() {}
  virtual int64 Run(int64, int64) = 0;
};

class IntToLong {
 public:
  virtual ~IntToLong() {}
  virtual int64 Run(int) = 0;
};

class IntIntToLong {
 public:
  virtual ~IntIntToLong() {}
  virtual int64 Run(int, int) = 0;
};

class LongLongLongToLong {
 public:
  virtual ~LongLongLongToLong() {}
  virtual int64 Run(int64, int64, int64) = 0;
};

class LongToBoolean {
 public:
  virtual ~LongToBoolean() {}
  virtual bool Run(int64) = 0;
};

class VoidToString {
 public:
  virtual ~VoidToString() {}
  virtual std::string Run() = 0;
};

class VoidToBoolean {
 public:
  virtual ~VoidToBoolean() {}
  virtual bool Run() = 0;
};

class LongLongLongToBoolean {
 public:
  virtual ~LongLongLongToBoolean() {}
  virtual bool Run(int64 i, int64 j, int64 k) = 0;
};

class LongToVoid {
 public:
  virtual ~LongToVoid() {}
  virtual void Run(int64 i) = 0;
};

class VoidToVoid {
 public:
  virtual ~VoidToVoid() {}
  virtual void Run() = 0;
};
}  // namespace swig_util
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_FUNCTIONS_SWIG_HELPERS_H_
