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
// This file contains the in-memory data structure used by the SAT solver to
// generate unsatisfiability proof and UNSAT cores.
//
// Good references for the algorithm used are:
// - Roberto A., Robert N., Albert O., Enric R.-C. "Efficient Generation of
//   Unsatisfiability Proofs and Cores in SAT",
//   http://www.lsi.upc.edu/~oliveras/espai/papers/lpar08.pdf
// - Paul Beame, Henry Kautz, Ashish Sabharwal, "Understanding the Power of
//   Clause Learning", https://www.cs.rochester.edu/~kautz/papers/learnIjcai.pdf
// - TraceCheck: http://fmv.jku.at/tracecheck/index.html

#ifndef OR_TOOLS_SAT_UNSAT_PROOF_H_
#define OR_TOOLS_SAT_UNSAT_PROOF_H_

#include "base/hash.h"
#include <vector>

#include "base/hash.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// Forward declaration. A client only need to manipulates pointer to this class
// which is defined in the .cc
class ResolutionNode;

// An UNSAT resolution proof will be given as a Directed Acyclic Graph (DAG) of
// clauses. Each clause corresponds to a ResolutionNode. Nodes without parent
// correspond to initial problems clauses. The other nodes correspond to new
// clauses that can be infered from its parents using the basic "resolution
// rule" or subsumption: (A v x) and (B v not(x)) => A v B.
//
// The order of the parents of each node will be such that we can reconstruct
// the clause associated to it by starting by the first parent clause and then
// resolving it by each of the following clause in order. There will be only
// one way to perform each resolution.
class UnsatProof {
 public:
  UnsatProof() : num_nodes_(0) {}
  ~UnsatProof();

  // Creates a new root node corresponding to an original problem clause with
  // given index. UnlockNode() will need to be called before this class is
  // deleted, see the comment there.
  ResolutionNode* CreateNewRootNode(int clause_index);

  // Creates a new ResolutionNode with given parents. The vector parents must
  // not be empty and will be swapped with an empty vector. UnlockNode() will
  // need to be called before this class is deleted, see the comment there. Note
  // that we check that all the given parents are locked.
  //
  // For CheckUnsatProof() to work, the parents must be provided as decribed in
  // the top level comment of this class. It is possible to remove this
  // restriction, but it is a small price to pay for the SAT solver and it
  // simplifies the code of CheckUnsatProof().
  ResolutionNode* CreateNewResolutionNode(std::vector<ResolutionNode*>* parents);

  // Unlocks the given node so it can be deleted if it is not used as a parents
  // to any other node. This can only be called on locked node (there is a
  // check).
  //
  // The idea is that the SAT solver can call UnlockNode() as soon as it known
  // that the node can't be used directly to infer another clause. This way,
  // this class may be able to free up some memory.
  void UnlockNode(ResolutionNode* node);

  // Returns the number of ResolutionNode currently stored by this class.
  // Nodes that where deleted are not counted.
  int NumNodes() const { return num_nodes_; }

  // Returns the set of original clause indices (the one provided to
  // CreateNewRootNode()) from which we can deduce the clause corresponding to
  // the given final_node. If final_node is associated with the the empty
  // conflict, this will return an UNSAT core.
  void ComputeUnsatCore(ResolutionNode* final_node, std::vector<int>* core) const;

  // TODO(user): to implement. This will need to know the clause associated to
  // each root node to be able to reconstruct the clause associated to each
  // node. There is also some complications for more general constraints like
  // pseudo-boolean ones because they can produce many different reason clauses
  // and so we need more than one root for each of these clauses.
  bool CheckUnsatProof(ResolutionNode* final_node) const;

 private:
  // See NumNodes().
  int num_nodes_;

  // Temporary vector used by UnlockNode() and GetCore().
  mutable std::vector<ResolutionNode*> node_stack_;

  // Temporary vector used by GetCore().
  mutable std::vector<ResolutionNode*> to_unmark_;

  // Index to identify in the original problem the constraint corresponding to
  // this root node. Note that duplicate indices are allowed which make sense
  // when an original constraint was expanded into multiple clauses internally.
  hash_map<ResolutionNode*, int> root_node_to_clause_index_;

  DISALLOW_COPY_AND_ASSIGN(UnsatProof);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_UNSAT_PROOF_H_
