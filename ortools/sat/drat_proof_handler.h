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

#ifndef OR_TOOLS_SAT_DRAT_PROOF_HANDLER_H_
#define OR_TOOLS_SAT_DRAT_PROOF_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/file.h"
#endif  // !defined(__PORTABLE_PLATFORM__)
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_writer.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// DRAT is a SAT proof format that allows a simple program to check that the
// problem is really UNSAT. The description of the format and a checker are
// available at: // http://www.cs.utexas.edu/~marijn/drat-trim/
//
// Note that DRAT proofs are often huge (can be GB), and take about as much time
// to check as it takes for the solver to find the proof in the first place!
//
// This class is used to build the SAT proof, and can either save it to disk,
// and/or store it in memory (in which case the proof can be checked when it is
// complete).
class DratProofHandler {
 public:
  // Use this constructor to store the DRAT proof in memory. The proof will not
  // be written to disk, and can be checked with Check() when it is complete.
  DratProofHandler();
  // Use this constructor to write the DRAT proof to disk, and to optionally
  // store it in memory as well (in which case the proof can be checked with
  // Check() when it is complete).
  DratProofHandler(bool in_binary_format, File* output, bool check = false);
  ~DratProofHandler() {}

  // During the presolve step, variable get deleted and the set of non-deleted
  // variable is remaped in a dense set. This allows to keep track of that and
  // always output the DRAT clauses in term of the original variables. Must be
  // called before adding or deleting clauses AddClause() or DeleteClause().
  //
  // TODO(user): This is exactly the same mecanism as in the SatPostsolver
  // class. Factor out the code.
  void ApplyMapping(
      const absl::StrongVector<BooleanVariable, BooleanVariable>& mapping);

  // This need to be called when new variables are created.
  void SetNumVariables(int num_variables);
  void AddOneVariable();

  // Adds a clause of the UNSAT problem. This must be called before any call to
  // AddClause() or DeleteClause(), in order to be able to check the DRAT proof
  // with the Check() method when it is complete.
  void AddProblemClause(absl::Span<const Literal> clause);

  // Writes a new clause to the DRAT output. The output clause is sorted so that
  // newer variables always comes first. This is needed because in the DRAT
  // format, the clause is checked for the RAT property with only its first
  // literal. Must not be called after Check().
  void AddClause(absl::Span<const Literal> clause);

  // Writes a "deletion" information about a clause that has been added before
  // to the DRAT output. Note that it is also possible to delete a clause from
  // the problem. Must not be called after Check().
  //
  // Because of a limitation a the DRAT-trim tool, it seems the order of the
  // literals during addition and deletion should be EXACTLY the same. Because
  // of this we get warnings for problem clauses.
  void DeleteClause(absl::Span<const Literal> clause);

  // Returns VALID if the DRAT proof is correct, INVALID if it is not correct,
  // or UNKNOWN if proof checking was not enabled (by choosing the right
  // constructor) or timed out. This requires the problem clauses to be
  // specified with AddProblemClause(), before the proof itself.
  //
  // WARNING: no new clause must be added or deleted after this method has been
  // called.
  DratChecker::Status Check(double max_time_in_seconds);

 private:
  void MapClause(absl::Span<const Literal> clause);

  // We need to keep track of the variable newly created.
  int variable_index_;

  // Temporary vector used for sorting the outputted clauses.
  std::vector<Literal> values_;

  // This mapping will be applied to all clause passed to AddClause() or
  // DeleteClause() so that they are in term of the original problem.
  absl::StrongVector<BooleanVariable, BooleanVariable> reverse_mapping_;

  std::unique_ptr<DratChecker> drat_checker_;
  std::unique_ptr<DratWriter> drat_writer_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DRAT_PROOF_HANDLER_H_
