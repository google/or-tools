#pragma once

#include "ortools/linear_solver/linear_solver.h"

namespace operations_research
{
   MPSolverInterface *BuildSiriusInterfaceLP(MPSolver *const solver);
   MPSolverInterface *BuildSiriusInterfaceMIP(MPSolver *const solver);
}

