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

//
// Prints a model of Frequency Assignment Problem.
// Format: http://www.inra.fr/mia/T/schiex/Doc/CELAR.shtml#synt
//

#ifndef OR_TOOLS_EXAMPLES_FAP_MODEL_PRINTER_H_
#define OR_TOOLS_EXAMPLES_FAP_MODEL_PRINTER_H_

#include <map>
#include <vector>

#include "examples/cpp/fap_parser.h"

namespace operations_research {

// Prints the instance of the Frequency Assignment Problem.
class FapModelPrinter {
 public:
  FapModelPrinter(const std::map<int, FapVariable>& variables,
                  const std::vector<FapConstraint>& constraints,
                  const std::string& objective, const std::vector<int>& values);
  ~FapModelPrinter();

  void PrintFapObjective();
  void PrintFapVariables();
  void PrintFapConstraints();
  void PrintFapValues();

 private:
  const std::map<int, FapVariable> variables_;
  const std::vector<FapConstraint> constraints_;
  const std::string objective_;
  const std::vector<int> values_;
  DISALLOW_COPY_AND_ASSIGN(FapModelPrinter);
};

}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FAP_MODEL_PRINTER_H_
