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

#include "ortools/sat/lrat_proof_handler.h"

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {
namespace {

TEST(AddAndProveInferredClauseByEnumerationTest, XorEquivalence) {
  // We assume a = XOR(c,d) and b = XOR(c,d) and we want to prove a => b
  const Literal a(+1);
  const Literal b(+2);
  const Literal c(+3);
  const Literal d(+4);

  // Encode the two XOR gates.
  CompactVectorVector<int, Literal> clauses;
  for (const Literal x : {a, b}) {
    clauses.Add({c, d, x.Negated()});
    clauses.Add({c.Negated(), d, x});
    clauses.Add({c, d.Negated(), x});
    clauses.Add({c.Negated(), d.Negated(), x.Negated()});
  }

  // Create the lrat checker.
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_check_lrat_proof(true);
  params->set_check_drat_proof(true);
  std::unique_ptr<LratProofHandler> lrat =
      LratProofHandler::MaybeCreate(&model);

  // Lets create ids for all these clauses.
  auto* id_generator = model.GetOrCreate<ClauseIdGenerator>();
  std::vector<ClauseId> clause_ids;
  for (int i = 0; i < clauses.size(); ++i) {
    const ClauseId id = id_generator->GetNextId();
    clause_ids.push_back(id);
    lrat->AddProblemClause(id, clauses[i]);
  }
  lrat->EndProblemClauses();

  // This should be enough to prove equivalence.
  {
    std::vector<Literal> to_prove = {b.Negated(), a};
    EXPECT_TRUE(lrat->AddAndProveInferredClauseByEnumeration(
        ClauseId(b.Negated(), a), to_prove, clause_ids, clauses));
  }
  {
    std::vector<Literal> to_prove = {a.Negated(), b};
    EXPECT_TRUE(lrat->AddAndProveInferredClauseByEnumeration(
        ClauseId(a.Negated(), b), to_prove, clause_ids, clauses));
  }
}

}  // namespace
}  // namespace operations_research::sat
