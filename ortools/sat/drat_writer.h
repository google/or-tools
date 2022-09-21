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

#ifndef OR_TOOLS_SAT_DRAT_WRITER_H_
#define OR_TOOLS_SAT_DRAT_WRITER_H_

#include <string>

#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/file.h"
#else
class File {};
#endif  // !__PORTABLE_PLATFORM__
#include "absl/types/span.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// DRAT is a SAT proof format that allows a simple program to check that the
// problem is really UNSAT. The description of the format and a checker are
// available at: // http://www.cs.utexas.edu/~marijn/drat-trim/
//
// Note that DRAT proofs are often huge (can be GB), and take about as much time
// to check as it takes for the solver to find the proof in the first place!
class DratWriter {
 public:
  DratWriter(bool in_binary_format, File* output)
      : in_binary_format_(in_binary_format), output_(output) {}
  ~DratWriter();

  // Writes a new clause to the DRAT output. Note that the RAT property is only
  // checked on the first literal.
  void AddClause(absl::Span<const Literal> clause);

  // Writes a "deletion" information about a clause that has been added before
  // to the DRAT output. Note that it is also possible to delete a clause from
  // the problem.
  void DeleteClause(absl::Span<const Literal> clause);

 private:
  void WriteClause(absl::Span<const Literal> clause);

  // TODO(user): Support binary format as proof in text format can be large.
  bool in_binary_format_;
  File* output_;

  std::string buffer_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DRAT_WRITER_H_
