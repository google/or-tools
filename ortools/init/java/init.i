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

%include "ortools/base/base.i"

%include "std_string.i"

%{
#include "ortools/init/init.h"
%}

%ignoreall

%unignore operations_research;

// Expose the flags structure.
%unignore operations_research::CppFlags;
%unignore operations_research::CppFlags::logtostderr;
%unignore operations_research::CppFlags::log_prefix;
%unignore operations_research::CppFlags::cp_model_dump_prefix;
%unignore operations_research::CppFlags::cp_model_dump_models;
%unignore operations_research::CppFlags::cp_model_dump_lns;
%unignore operations_research::CppFlags::cp_model_dump_response;

// Expose the static methods of the bridge class.
%unignore operations_research::CppBridge;
%rename (initLogging) operations_research::CppBridge::InitLogging;
%rename (shutdownLogging) operations_research::CppBridge::ShutdownLogging;
%rename (setFlags) operations_research::CppBridge::SetFlags;
%rename (logGurobiSharedLibrary) operations_research::CppBridge::LoadGurobiSharedLibrary;

%unignore operations_research::OrToolsVersion;
%rename (getMajorNumber) operations_research::OrToolsVersion::MajorNumber;
%rename (getMinorNumber) operations_research::OrToolsVersion::MinorNumber;
%rename (getPatchNumber) operations_research::OrToolsVersion::PatchNumber;
%rename (getVersionString) operations_research::OrToolsVersion::VersionString;

%include "ortools/init/init.h"

%unignoreall
