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

#ifndef OR_TOOLS_SAT_ROUTING_CUTS_H_
#define OR_TOOLS_SAT_ROUTING_CUTS_H_

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Given a graph with nodes in [0, num_nodes) and a set of arcs (the order is
// important), this will:
//   - Start with each nodes in separate "subsets".
//   - Consider the arc in order, and each time one connects two separate
//     subsets, merge the two subsets into a new one.
//   - Stops when there is only 2 subset left.
//   - Output all subsets generated this way (at most 2 * num_nodes). The
//     subsets spans will point in the subset_data vector (which will be of size
//     exactly num_nodes).
//
// Only subsets of size >= min_subset_size will be returned. This is mainly here
// to exclude subsets of size 1.
//
// This is an heuristic to generate interesting cuts for TSP or other graph
// based constraints. We roughly follow the algorithm described in section 6 of
// "The Traveling Salesman Problem, A computational Study", David L. Applegate,
// Robert E. Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
void GenerateInterestingSubsets(int num_nodes,
                                const std::vector<std::pair<int, int>>& arcs,
                                int min_subset_size, int stop_at_num_components,
                                std::vector<int>* subset_data,
                                std::vector<absl::Span<const int>>* subsets);

// Cut generator for the circuit constraint, where in any feasible solution, the
// arcs that are present (variable at 1) must form a circuit through all the
// nodes of the graph. Self arc are forbidden in this case.
//
// In more generality, this currently enforce the resulting graph to be strongly
// connected. Note that we already assume basic constraint to be in the lp, so
// we do not add any cuts for components of size 1.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, std::vector<int> tails, std::vector<int> heads,
    std::vector<Literal> literals, Model* model);

// Almost the same as CreateStronglyConnectedGraphCutGenerator() but for each
// components, computes the demand needed to serves it, and depending on whether
// it contains the depot (node zero) or not, compute the minimum number of
// vehicle that needs to cross the component border.
CutGenerator CreateCVRPCutGenerator(int num_nodes, std::vector<int> tails,
                                    std::vector<int> heads,
                                    std::vector<Literal> literals,
                                    std::vector<int64_t> demands,
                                    int64_t capacity, Model* model);

// Try to find a subset where the current LP capacity of the outgoing or
// incoming arc is not enough to satisfy the demands.
//
// We support the special value -1 for tail or head that means that the arc
// comes from (or is going to) outside the nodes in [0, num_nodes). Such arc
// must still have a capacity assigned to it.
//
// TODO(user): Support general linear expression for capacities.
// TODO(user): Some model applies the same capacity to both an arc and its
// reverse. Also support this case.
CutGenerator CreateFlowCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<AffineExpression>& arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ROUTING_CUTS_H_
