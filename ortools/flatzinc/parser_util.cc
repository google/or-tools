// Copyright 2010-2017 Google
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

// Utility functions used by the code in parser.yy
// Included in parser.tab.cc.
#include "ortools/flatzinc/parser_util.h"

#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/parser.tab.hh"
#include "ortools/util/string_array.h"

extern int orfz_lex(YYSTYPE*, void* scanner);
extern int orfz_get_lineno(void* scanner);
extern int orfz_debug;

void orfz_error(operations_research::fz::ParserContext* context,
                operations_research::fz::Model* model, bool* ok, void* scanner,
                const char* str) {
  LOG(ERROR) << "Error: " << str << " in line no. " << orfz_get_lineno(scanner);
  *ok = false;
}

namespace operations_research {
namespace fz {
// Whether the given list of annotations contains the given identifier
// (or function call).
bool ContainsId(std::vector<Annotation>* annotations, const std::string& id) {
  if (annotations != nullptr) {
    for (int i = 0; i < annotations->size(); ++i) {
      if (((*annotations)[i].type == Annotation::IDENTIFIER ||
           (*annotations)[i].type == Annotation::FUNCTION_CALL) &&
          (*annotations)[i].id == id) {
        return true;
      }
    }
  }
  return false;
}

bool AllDomainsHaveOneValue(const std::vector<Domain>& domains) {
  for (int i = 0; i < domains.size(); ++i) {
    if (!domains[i].HasOneValue()) {
      return false;
    }
  }
  return true;
}

int64 ConvertAsIntegerOrDie(double d) {
  const double rounded = std::round(d);
  const int64 i = static_cast<int64>(rounded);
  CHECK_NEAR(d, i, 1e-9);
  return i;
}

// Array in flatzinc are 1 based. We use this trivial wrapper for all flatzinc
// arrays.
template <class T>
const T& Lookup(const std::vector<T>& v, int index) {
  // TODO(user): replace this by a macro for better logging.
  CHECK_GE(index, 1);
  CHECK_LE(index, v.size());
  return v[index - 1];
}
}  // namespace fz
}  // namespace operations_research
