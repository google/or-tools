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

#ifndef OR_TOOLS_SAT_THETA_TREE_H_
#define OR_TOOLS_SAT_THETA_TREE_H_

#include <algorithm>
#include <limits>
#include <vector>

#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

// The Theta-Lambda tree can be used to implement several scheduling algorithms.
//
// The tree structure itself is a binary tree coded in a vector, where node 0 is
// unused, node 1 is the root, node 2 is the left child of the root, node 3 its
// right child, etc. To represent num_events events, we use the smallest
// possible amount of leaves, num_leaves = 2^k >= num_events. The unused leaves
// are filled with dummy values, as if they were absent events.
// The API gives access to events that realize the maximal output. When it does
// so, it always gives the rightmost ones if there are several solutions, in
// an attempt to minimize the number of events used in explanations.
//
// See:
// _ (0) Petr Vilim's PhD thesis "Global Constraints in Scheduling".
// _ (1) Petr Vilim "Edge Finding Filtering Algorithm for Discrete Cumulative
//   Resources in O(kn log n)"
// _ (2) Petr Vilim "Max energy filtering algorithm for discrete cumulative
//   resources".
// _ (3) Wolf & Schrader "O(n log n) Overload Checking for the Cumulative
//   Constraint and Its Application".
// _ (4) Kameugne & Fotso "A cumulative not-first/not-last filtering algorithm
//   in O(n^2 log n)".
// _ (5) Ouellet & Quimper "Time-table extended-edge-finding for the cumulative
//   constraint".
//
// Instead of providing one declination of the theta-tree per possible filtering
// algorithm, this generalization intends to provide a data structure that can
// fit several algorithms.
// This tree is based around the notion of events. It has events at its leaves
// that can be present, optional, or absent, internal nodes compute values on
// the set of events that are present/optional.
// Each event has an initial_envelope and an energy.
// The tree maintains:
// _ energy(node) = sum_{leaf \in leaves(node)} energy(leaf)
// _ envelope(node) =
//     max_{leaf \in leaves(node)}
//       initial_envelope(leaf) +
//       sum_{leaf' \in leaves(node), leaf' at or right of leaf} energy(leaf').
//
// Thus, the envelope of a leaf representing an event, when present, is
// initial_envelope(event) + energy(event).
//
// envelope_opt and energy_opt are similar, but compute the maximum value a node
// could have if one of the optional event were present:
// _ energy_opt(node) = sum_{leaf \in leaves(node)} energy(leaf)
//                    + max_{leaf \in leaves(node)} energy_opt(leaf)
// _ envelope_opt(node) =
//     max_{leaf \in leaves(node)}
//       initial_envelope(leaf) +
//       sum_{leaf' \in leaves(node), leaf' at or right of leaf} energy(leaf') +
//       max_{leaf_opt \in leaves(node)} energy_opt(leaf_opt)
//     max_{leaf_opt \in leaves(node)}
//       initial_envelope(leaf_opt) + energy_opt(leaf_opt) +
//       sum_{leaf \in leaves(node), leaf at or right of leaf_opt} energy(leaf)
//
// Most articles using theta-tree variants hack Vilim's original theta tree
// for the disjunctive resource constraint by manipulating envelope and
// energy:
// _ in (0), initial_envelope = start_min, energy = duration
// _ in (3), initial_envelope = C * start_min, energy = demand * duration
// _ in (5), there are several trees in parallel:
//           initial_envelope = C * start_min or (C - h) * start_min
//           energy = demand * duration, h * (Horizon - start_min),
//                    or h * (end_min).
// _ in (2), same as (3), but putting the max energy instead of min in lambda.
// _ in OscaR's TimeTableOverloadChecker,
//   initial_envelope = C * start_min -
//                      energy of mandatory profile before start_min,
//   energy = demand * duration
//
// There is hope to unify the variants of these algorithms by abstracting the
// tasks away to reason only on events.
class ThetaLambdaTree {
 public:
  // Build a reusable tree. Initialization is done with Reset().
  ThetaLambdaTree();

  // Reset() initializes this class; other operations refer to the values
  // passed by the last Reset().
  // Instead of allocating and de-allocating trees at every usage, i.e. at
  // every Propagate() of the scheduling algorithms that uses it,
  // this class allows to keep the same memory for each call.
  // Reset() should be called with (vector) attributes for events,
  // then the resulting Add/Remove/Get operations will use these attributes.
  // Events should be sorted by nondecreasing initial_envelope.
  void Reset(std::vector<IntegerValue> event_initial_envelope,
             std::vector<IntegerValue> event_energy);

  // Make event present, compute the new envelopes in O(log n).
  // The event must not be already present or optional.
  void AddEvent(int event);

  // Make event optional, compute the new envelopes in O(log n).
  // The event must not be already present or optional.
  void AddOptionalEvent(int event);

  // Make event absent, compute the new envelope in O(log n).
  // The event must be present or optional.
  void RemoveEvent(int event);

  // Returns true iff event is present, O(1).
  bool IsPresent(int event) const;

  // Returns true iff event is optional, O(1).
  bool IsOptional(int event) const;

  // Returns the current envelope of present tasks in O(1).
  IntegerValue GetEnvelope() const;

  // Return the current envelope of present tasks + 1 optional task in O(1).
  IntegerValue GetOptionalEnvelope() const;

  // Returns the rightmost event such that
  // initial_envelope(event) + sum_{event' at or after event} energy(event'),
  // is larger or equal to the given envelope, in O(log n).
  // The given envelope can be at most GetEnvelope().
  int GetRightmostEventWithEnvelopeGreaterOrEqualTo(
      IntegerValue target_envelope) const;

  // Returns the envelope obtained by using the given event's initial_envelope
  // plus the sum of energies of present events at or on the right of given
  // event in O(log n).
  IntegerValue GetEnvelopeOf(int event) const;

  // Returns the rightmost optional event responsible for the current optional
  // envelope in O(log n). There must be an optional event,
  // i.e. GetOptionalEnvelope() > GetEnvelope().
  int GetOptionalEventRealizingOptionalEnvelope() const;

  // Returns the rightmost event responsible for the initial_envelope part of
  // the current optional envelope, in O(log n). The event can be present or
  // optional.
  int GetEventRealizingOptionalEnvelope() const;

 private:
  // Propagates the change of leaf energies and envelopes towards the root.
  void RefreshNode(int leaf);

  // Returns the rightmost present leaf under node responsible for given
  // envelope, see GetRightmostEventWithEnvelopeGreaterOrEqualTo().
  int GetLeafRealizingEnvelope(int node, IntegerValue envelope) const;

  // Returns the rightmost optional leaf of maximal energy under node.
  // There must be an optional leaf under node.
  int GetOptionalLeafOfMaximalEnergy(int node) const;

  // If initial_envelope_leaf_wanted is true, returns the rightmost leaf
  // such that the optional envelope is initial_envelope(leaf) plus some
  // energies. It can be a present or an optional leaf.
  // If initial_envelope_leaf_wanted is false, returns the rightmost optional
  // leaf whose energy is in the optional envelope above.
  // Notice that with a given set of present and optional events,
  // the leaf returned by calling with initial_envelope_leaf_wanted set to true
  // is the same or at the left as the leaf returned by calling with the boolean
  // set to false.
  // See class description for more details.
  template <bool initial_envelope_leaf_wanted>
  int GetLeafRealizingOptionalEnvelope(int node) const;

  // Number of events of the last Reset();
  int num_events_;

  // Number of leaves used by the last Reset(), the smallest power of 2 such
  // that 2 <= num_leaves_ and num_events_ <= num_leaves_.
  int num_leaves_;

  // Event characteristics: initial_envelope and energy.
  std::vector<IntegerValue> event_initial_envelope_;
  std::vector<IntegerValue> event_energy_;

  // Envelopes and energies of nodes.
  std::vector<IntegerValue> tree_envelope_;
  std::vector<IntegerValue> tree_energy_;
  std::vector<IntegerValue> tree_envelope_opt_;
  std::vector<IntegerValue> tree_energy_opt_;

  // Stores whether an event is currently present/optional.
  std::vector<bool> is_present_;
  std::vector<bool> is_optional_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_THETA_TREE_H_
