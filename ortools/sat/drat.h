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

#ifndef OR_TOOLS_SAT_DRAT_H_
#define OR_TOOLS_SAT_DRAT_H_

#include "ortools/base/file.h"
#include "ortools/sat/model.h"
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
      : variable_index_(0),
        in_binary_format_(in_binary_format),
        output_(output) {}
  ~DratWriter();

  // This tries to open the FLAGS_drat_file file and if it succeed it will
  // return a non-nullptr DratWriter class.
  static DratWriter* CreateInModel(Model* model);

  // During the presolve step, variable get deleted and the set of non-deleted
  // variable is remaped in a dense set. This allows to keep track of that and
  // always output the DRAT clauses in term of the original variables.
  //
  // TODO(user): This is exactly the same mecanism as in the SatPostsolver
  // class. Factor out the code.
  void ApplyMapping(const ITIVector<BooleanVariable, BooleanVariable>& mapping);

  // This need to be called when new variables are created.
  void SetNumVariables(int num_variables);
  void AddOneVariable();

  // Writes a new clause to the DRAT output. The output clause is sorted so that
  // newer variables always comes first. This is needed because in the DRAT
  // format, the clause is checked for the RAT property with only its first
  // literal.
  void AddClause(gtl::Span<Literal> clause);

  // Writes a "deletion" information about a clause that has been added before
  // to the DRAT output. Note that it is also possible to delete a clause from
  // the problem.
  //
  // Because of a limitation a the DRAT-trim tool, it seems the order of the
  // literals during addition and deletion should be EXACTLY the same. Because
  // of that, we currently can't delete problem clauses since we don't keep the
  // literal order in our memory representation. We use the ignore_call argument
  // to simply do nothing by default, and we only set it to false in the places
  // where we are sure the clause was outputed by an AddClause() call.
  //
  // TODO(user): an alternative would be to call AddClause() on all the problem
  // clause first.
  void DeleteClause(gtl::Span<Literal> clause, bool ignore_call = true);

 private:
  void WriteClause(gtl::Span<Literal> clause);

  // We need to keep track of the variable newly created.
  int variable_index_;

  // TODO(user): Support binary format as proof in text format can be large.
  bool in_binary_format_;
  File* output_;

  std::string buffer_;

  // Temporary vector used for sorting the outputed clauses.
  std::vector<int> values_;

  // This mapping will be applied to all clause passed to AddClause() or
  // DeleteClause() so that they are in term of the original problem.
  ITIVector<BooleanVariable, BooleanVariable> reverse_mapping_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DRAT_H_
