
#ifndef OR_TOOLS_GLOP_PNG_DUMP_H_
#define OR_TOOLS_GLOP_PNG_DUMP_H_

#include <string>

#include "glop/lp_data.h"

namespace operations_research {
namespace glop {

// Returns a PNG std::string representing the fill-in of the matrix.
std::string DumpConstraintMatrixToPng(const LinearProgram& linear_program);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_PNG_DUMP_H_
