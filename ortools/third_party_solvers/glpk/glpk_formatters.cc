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

#include "ortools/third_party_solvers/glpk/glpk_formatters.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"

extern "C" {
#include <glpk.h>
}

namespace operations_research {

std::string SolutionStatusString(const int status) {
  switch (status) {
    case GLP_UNDEF:
      return "undefined (UNDEF)";
    case GLP_FEAS:
      return "feasible (FEAS)";
    case GLP_INFEAS:
      return "infeasible (INFEAS)";
    case GLP_NOFEAS:
      return "no feasible solution (NOFEAS)";
    case GLP_OPT:
      return "optimal (OPT)";
    case GLP_UNBND:
      return "unbounded (UNBND)";
    default:
      return absl::StrCat("? (", status, ")");
  }
}

std::string BasisStatusString(const int stat) {
  switch (stat) {
    case GLP_BS:
      return "basic (BS)";
    case GLP_NL:
      return "lower bound (NL)";
    case GLP_NU:
      return "upper bound (NU)";
    case GLP_NF:
      return "unbounded (NF)";
    case GLP_NS:
      return "fixed (NS)";
    default:
      return absl::StrCat("? (", stat, ")");
  }
}

std::string ReturnCodeString(const int rc) {
  switch (rc) {
    case GLP_EBADB:
      return "[GLP_EBADB] invalid basis";
    case GLP_ESING:
      return "[GLP_ESING] singular matrix";
    case GLP_ECOND:
      return "[GLP_ECOND] ill-conditioned matrix";
    case GLP_EBOUND:
      return "[GLP_EBOUND] invalid bounds";
    case GLP_EFAIL:
      return "[GLP_EFAIL] solver failed";
    case GLP_EOBJLL:
      return "[GLP_EOBJLL] objective lower limit reached";
    case GLP_EOBJUL:
      return "[GLP_EOBJUL] objective upper limit reached";
    case GLP_EITLIM:
      return "[GLP_EITLIM] iteration limit exceeded";
    case GLP_ETMLIM:
      return "[GLP_ETMLIM] time limit exceeded";
    case GLP_ENOPFS:
      return "[GLP_ENOPFS] no primal feasible solution";
    case GLP_ENODFS:
      return "[GLP_ENODFS] no dual feasible solution";
    case GLP_EROOT:
      return "[GLP_EROOT] root LP optimum not provided";
    case GLP_ESTOP:
      return "[GLP_ESTOP] search terminated by application";
    case GLP_EMIPGAP:
      return "[GLP_EMIPGAP] relative mip gap tolerance reached";
    case GLP_ENOFEAS:
      return "[GLP_ENOFEAS] no primal/dual feasible solution";
    case GLP_ENOCVG:
      return "[GLP_ENOCVG] no convergence";
    case GLP_EINSTAB:
      return "[GLP_EINSTAB] numerical instability";
    case GLP_EDATA:
      return "[GLP_EDATA] invalid data";
    case GLP_ERANGE:
      return "[GLP_ERANGE] result out of range";
    default:
      return absl::StrCat("[?] unknown return code ", rc);
  }
}

std::string TruncateAndQuoteGLPKName(const std::string_view original_name) {
  std::ostringstream oss;
  std::size_t current_size = 0;
  for (const char c : original_name) {
    // We use \ for escape sequences; thus we must escape it too.
    if (c == '\\') {
      if (current_size + 2 > kMaxGLPKNameLen) {
        break;
      }
      oss << "\\\\";
      current_size += 2;
      continue;
    }

    // Simply insert non-control characters (that are not the escape character
    // above).
    if (!absl::ascii_iscntrl(c)) {
      if (current_size + 1 > kMaxGLPKNameLen) {
        break;
      }
      oss << c;
      ++current_size;
      continue;
    }

    // Escape control characters.
    const std::string escaped_c =
        absl::StrCat("\\x", absl::Hex(c, absl::kZeroPad2));
    if (current_size + escaped_c.size() > kMaxGLPKNameLen) {
      break;
    }
    oss << escaped_c;
    current_size += escaped_c.size();
  }

  const std::string ret = oss.str();
  DCHECK_EQ(ret.size(), current_size);

  return ret;
}

}  // namespace operations_research
