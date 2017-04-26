// Copyright 2010-2014 Google
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

// This is the C# SWIG wrapper for ../tuple_set.h. See that file.
// TODO(user): Refactor this file to comply with the SWIG style guide.

%include "ortools/base/base.i"


%{
// TODO(user): See if we really need <vector>.
#include <vector>
#include "ortools/base/integral_types.h"
#include "ortools/util/tuple_set.h"
%}

%include ortools/util/tuple_set.h
