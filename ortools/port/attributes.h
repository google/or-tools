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

#ifndef ORTOOLS_PORT_ATTRIBUTES_H_
#define ORTOOLS_PORT_ATTRIBUTES_H_

#include "absl/base/attributes.h"

// The ORTOOLS_REQUIRE_EXPLICIT_INIT replaces ABSL_REQUIRE_EXPLICIT_INIT in
// cases where the ABSL macro does not compile.
//
// This typically happens with MSVC when the field is a std::string. The reason
// is that ABSL_REQUIRE_EXPLICIT_INIT adds `= xxx;` where `xxx` is a type with a
// template conversion operator and MSVC complains that multiple constructors of
// the std::string can be selected by the overload resolution.
//
// See https://github.com/abseil/abseil-cpp/issues/2157.
#ifndef _MSC_VER  // MSVC
#define ORTOOLS_REQUIRE_EXPLICIT_INIT ABSL_REQUIRE_EXPLICIT_INIT
#else  // _MSC_VER
#define ORTOOLS_REQUIRE_EXPLICIT_INIT
#endif  // _MSC_VER

#endif  // ORTOOLS_PORT_ATTRIBUTES_H_
