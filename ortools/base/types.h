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

#ifndef ORTOOLS_BASE_TYPES_H_
#define ORTOOLS_BASE_TYPES_H_

#include <cstdint>

const uint8_t kuint8max{0xFF};
const uint16_t kuint16max{0xFFFF};
const uint32_t kuint32max{0xFFFFFFFF};
const uint64_t kuint64max{0xFFFFFFFFFFFFFFFF};
const int8_t kint8min{~0x7F};
const int8_t kint8max{0x7F};
const int16_t kint16min{~0x7FFF};
const int16_t kint16max{0x7FFF};
const int32_t kint32min{~0x7FFFFFFF};
const int32_t kint32max{0x7FFFFFFF};
const int64_t kint64min{~0x7FFFFFFFFFFFFFFF};
const int64_t kint64max{0x7FFFFFFFFFFFFFFF};

#endif  // ORTOOLS_BASE_TYPES_H_
