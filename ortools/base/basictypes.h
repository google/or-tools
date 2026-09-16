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

#ifndef ORTOOLS_UTIL_BASICTYPES_H_
#define ORTOOLS_UTIL_BASICTYPES_H_

// Argument type used in interfaces that can optionally take ownership
// of a passed in argument.  If TAKE_OWNERSHIP is passed, the called
// object takes ownership of the argument.  Otherwise it does not.
//
// Note: In most cases where ownership transfer is needed it is preferable
// to make it unconditional, typically by passing a std::unique_ptr.
// See go/cppprimer#google_pointers for more suggestions.
enum Ownership { DO_NOT_TAKE_OWNERSHIP, TAKE_OWNERSHIP };

#endif  // ORTOOLS_UTIL_BASICTYPES_H_
