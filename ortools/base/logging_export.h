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

#ifndef OR_TOOLS_BASE_LOGGING_EXPORT_H_
#define OR_TOOLS_BASE_LOGGING_EXPORT_H_

// Annoying stuff for windows -- makes sure clients can import these functions
#if defined(_MSC_VER) && defined(OR_TOOLS_AS_DYNAMIC_LIB)
#if defined(OR_TOOLS_EXPORTS)
#define GOOGLE_GLOG_DLL_DECL __declspec(dllexport)
#else
#define GOOGLE_GLOG_DLL_DECL __declspec(dllimport)
#endif  // OR_TOOLS_EXPORTS
#endif  // _MSC_VER && OR_TOOLS_AS_DYNAMIC_LIB

#ifndef GOOGLE_GLOG_DLL_DECL
#define GOOGLE_GLOG_DLL_DECL
#endif

#endif  // OR_TOOLS_BASE_LOGGING_EXPORT_H_
