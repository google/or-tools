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

%module JniSolveInterrupter;
%include "ortools/base/base.i"

%{
#include "ortools/util/java/java_swig_solve_interrupter.h"
%}

SWIG_JAVABODY_PROXY(public, public, SWIGTYPE)
SWIG_JAVABODY_TYPEWRAPPER(public, public, public, SWIGTYPE)

%ignoreall

%unignore operations_research;

%unignore operations_research::JavaSwigSolveInterrupter;
%unignore operations_research::JavaSwigSolveInterrupter::JavaSwigSolveInterrupter;
%unignore operations_research::JavaSwigSolveInterrupter::~JavaSwigSolveInterrupter;
%unignore operations_research::JavaSwigSolveInterrupter::interrupt();
%unignore operations_research::JavaSwigSolveInterrupter::isInterrupted();

%include "ortools/util/java/java_swig_solve_interrupter.h"

%unignoreall
