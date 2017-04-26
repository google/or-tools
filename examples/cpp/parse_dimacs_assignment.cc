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


#include "examples/cpp/parse_dimacs_assignment.h"

#include "ortools/base/commandlineflags.h"

DEFINE_bool(assignment_maximize_cost, false,
            "Negate costs so a max-cost assignment is found.");
DEFINE_bool(assignment_optimize_layout, true,
            "Optimize graph layout for speed.");
