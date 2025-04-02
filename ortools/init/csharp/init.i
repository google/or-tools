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

%include "ortools/base/base.i"

%include "stdint.i"
%include "std_string.i"
%import "ortools/util/csharp/absl_string_view.i"

%{
#include "ortools/init/init.h"
%}

%typemap(ctype)  uint8_t* "uint8_t*"
%typemap(imtype) uint8_t* "System.IntPtr"
%typemap(cstype) uint8_t* "System.IntPtr"
%typemap(csin)   uint8_t* "$csinput"
%typemap(in)     uint8_t* %{ $1 = $input; %}

%ignoreall

%unignore operations_research;

// Expose the flags structure.
%unignore operations_research::CppFlags;
%unignore operations_research::CppFlags::stderrthreshold;
%unignore operations_research::CppFlags::log_prefix;
%unignore operations_research::CppFlags::cp_model_dump_prefix;
%unignore operations_research::CppFlags::cp_model_dump_models;
%unignore operations_research::CppFlags::cp_model_dump_submodels;
%unignore operations_research::CppFlags::cp_model_dump_response;

// Expose the static methods of the bridge class.
%unignore operations_research::CppBridge;
%unignore operations_research::CppBridge::InitLogging;
%unignore operations_research::CppBridge::ShutdownLogging;
%unignore operations_research::CppBridge::SetFlags;
%unignore operations_research::CppBridge::LoadGurobiSharedLibrary;
%unignore operations_research::CppBridge::DeleteByteArray;

%unignore operations_research::OrToolsVersion;
%unignore operations_research::OrToolsVersion::MajorNumber;
%unignore operations_research::OrToolsVersion::MinorNumber;
%unignore operations_research::OrToolsVersion::PatchNumber;
%unignore operations_research::OrToolsVersion::VersionString;

%include "ortools/init/init.h"

%unignoreall
