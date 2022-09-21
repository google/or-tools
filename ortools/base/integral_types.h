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

#ifndef OR_TOOLS_BASE_INTEGRAL_TYPES_H_
#define OR_TOOLS_BASE_INTEGRAL_TYPES_H_

#include <cinttypes>
#include <cstdint>

static const uint8_t kuint8max = UINT8_MAX;
static const uint16_t kuint16max = UINT16_MAX;
static const uint32_t kuint32max = UINT32_MAX;
static const uint64_t kuint64max = UINT64_MAX;

static const int8_t kint8min = INT8_MIN;
static const int8_t kint8max = INT8_MAX;
static const int16_t kint16min = INT16_MIN;
static const int16_t kint16max = INT16_MAX;
static const int32_t kint32min = INT32_MIN;
static const int32_t kint32max = INT32_MAX;
static const int64_t kint64min = INT64_MIN;
static const int64_t kint64max = INT64_MAX;

#endif  // OR_TOOLS_BASE_INTEGRAL_TYPES_H_
