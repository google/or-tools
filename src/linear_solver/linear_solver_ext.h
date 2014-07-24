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

#ifndef OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_EXT_H_
#define OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_EXT_H_

#include "base/hash.h"
#include "base/hash.h"
#include <limits>
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/timer.h"
#include "base/strutil.h"
#include "base/sparsetable.h"
#include "base/hash.h"
#include "linear_solver/linear_solver.h"

#if defined(USE_SLM)

    extern "C" {
      #include "sulumc.h"
    }

#endif

namespace operations_research {
// Special API
#if defined(USE_SLM)

    // Helper functions specific to using Google OR Tools with Sulum Optimizer
    // The advantage is these functions does directly to the outer API

    // Set a integer parameter in sulum as underlying solver
    // Return : 'true' if set 'false' if out of range
    bool SulumSetIntParam(MPSolver &solver,
                          int      iprm,
                          int      ival)
    {
      return ( SlmRetOk == SlmSetIntParam(solver.underlying_solver(), static_cast<SlmParamInt>(iprm), ival ) );
    }

    // Get a integer parameter from sulum as underlying solver
    // Return : 'true' if get 'false' if out of range
    bool SulumGetIntParam(MPSolver &solver,
                          int      iprm,
                          int      &ival)
    {
      return ( SlmRetOk == SlmGetIntParam(solver.underlying_solver(), static_cast<SlmParamInt>(iprm), &ival ) );
    }

    // Set a double parameter in sulum as underlying solver
    // Return : 'true' if set 'false' if out of range
    bool SulumSetDbParam(MPSolver &solver,
                          int     dprm,
                          double  dval)
    {
      return ( SlmRetOk == SlmSetDbParam(solver.underlying_solver(), static_cast<SlmParamDb>(dprm), dval ) );
    }

    // Get a double parameter from sulum as underlying solver
    // Return : 'true' if get 'false' if out of range
    bool SulumGetDbParam(MPSolver &solver,
                         int      dprm,
                         double   &dval)
    {
      return ( SlmRetOk == SlmGetDbParam(solver.underlying_solver(), static_cast<SlmParamDb>(dprm), &dval ) );
    }

#endif
}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_EXT_H_
