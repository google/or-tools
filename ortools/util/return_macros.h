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

#ifndef OR_TOOLS_UTIL_RETURN_MACROS_H_
#define OR_TOOLS_UTIL_RETURN_MACROS_H_

// Macros to replace CHECK_NOTNULL() so we don't crash in production.
// Logs a FATAL message in debug mode, and an ERROR message in production.
// It is not perfect, but more robust than crashing right away.
#define RETURN_IF_NULL(x)            \
  if (x == nullptr) {                \
    LOG(DFATAL) << #x << " == NULL"; \
    return;                          \
  }

#define RETURN_VALUE_IF_NULL(x, v)   \
  if (x == nullptr) {                \
    LOG(DFATAL) << #x << " == NULL"; \
    return v;                        \
  }

#endif  // OR_TOOLS_UTIL_RETURN_MACROS_H_
