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

#include "ortools/sat/gate_utils.h"

#include <algorithm>
#include <bit>
#include <bitset>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {

std::string BinaryCircuit::DebugString() const {
  // All these case should be easily simplifiable.
  int num_todo = 0;
  for (const BinaryGate& gate : gates) {
    if (gate.a == gate.b || gate.type == 0b0000 || gate.type == 0b1111 ||
        gate.type == 0b0101 || gate.type == 0b1010 || gate.type == 0b0011 ||
        gate.type == 0b1100) {
      ++num_todo;
    }
  }

  return absl::StrCat("#inputs:", num_inputs, " #vars:", num_vars,
                      " #gates:", gates.size(), " #outputs:", outputs.size(),
                      " #constraints:", gates.size() - (num_vars - num_inputs),
                      " #simplifiable:", num_todo);
}

void RemoveConstraints(BinaryCircuit* circuit) {
  // This doesn't change the indices.
  int new_size = 0;
  std::vector<bool> defined(circuit->num_vars, false);
  for (int i = 0; i < circuit->num_inputs; ++i) {
    defined[i] = true;
  }
  for (const BinaryGate& gate : circuit->gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;
    if (defined[gate.target]) continue;

    CHECK_LT(gate.a, gate.target) << gate.DebugString();
    CHECK_LT(gate.b, gate.target) << gate.DebugString();
    defined[gate.target] = true;
    circuit->gates[new_size++] = gate;
  }
  circuit->gates.resize(new_size);
}

// TODO(user): A similar code can be used to detect gates for which not all
// 4 kind of inputs are possible (a kind of "don't care"). After verification,
// we could thus change an AND to a XOR gate or vice-versa.
CompactVectorVector<int, Literal> SampleForEquivalences(
    const BinaryCircuit& circuit, absl::BitGenRef random,
    const std::vector<std::vector<BooleanVariable>>& saved_solutions) {
  // Try all possibilities. 64 at the time.
  //
  // TODO(user): take into account small/binary constraint between these
  // variables?
  const int num_inputs = circuit.num_inputs;
  const int num_vars = circuit.num_vars;
  const int num_derived_vars = num_vars - num_inputs;
  if (num_derived_vars == 0) return {};

  FixedCapacityVector<uint64_t> values;
  values.ClearAndReserve(num_vars);

  // We implement our own "DynamicPartition" because we can be a lot faster
  // here. We start with a single class with all literals on num_vars.
  std::vector<int> buffer(2 * num_derived_vars);
  std::vector<int> tmp_part[2];
  for (int i = 0; i < buffer.size(); ++i) buffer[i] = i;
  std::vector<std::pair<int, int>> equiv2;  // Special case.
  std::vector<std::pair<uint64_t, int>> local_classes;
  std::vector<absl::Span<const int>> equivalences;
  std::vector<absl::Span<const int>> new_equivalences;
  equivalences.push_back(buffer);

  // TODO(user): use effort instead?
  // Complexity is in roughly num_samples * num_vars.
  int num_compatibles = 0;
  const int kMaxExponents = 14;
  const int shifted_num_inputs = std::max(0, num_inputs - 6);
  const bool is_exact = shifted_num_inputs <= kMaxExponents;
  const int num_samples =
      is_exact ? (1 << shifted_num_inputs) : (1 << kMaxExponents);
  bool first = true;

  // If we are exact, the first 6 inputs will never changes, they contain all
  // possibilities.
  for (int i = 0; i < std::min(6, num_inputs); ++i) {
    values[i] = 0;
    for (uint64_t j = 0; j < 64; ++j) {
      values[i] |= ((j >> i) & 1) << j;
    }
  }
  int solution_index = 0;
  for (int start = 0; start < num_samples; ++start) {
    if (is_exact) {
      for (int i = 6; i < num_inputs; ++i) {
        values[i] = (start >> (i - 6)) & 1 ? ~uint64_t{0} : uint64_t{0};
      }
    } else {
      // Exploit old solution before sampling as they might separate hard to
      // get "non-equivalences".
      if (solution_index < saved_solutions.size()) {
        for (int i = 0; i < num_inputs; ++i) {
          values[i] = 0;
        }
        for (int j = 0; j < 64; ++j) {
          if (solution_index + j >= saved_solutions.size()) continue;
          for (const BooleanVariable b : saved_solutions[solution_index + j]) {
            const int index = circuit.mapping[b];
            if (index >= 0 && index < circuit.num_inputs) {
              values[index] |= 1 << j;
            }
          }
        }
        solution_index += 64;
      } else {
        for (int i = 0; i < num_inputs; ++i) {
          values[i] = absl::Uniform<uint64_t>(random);
        }
      }
    }
    uint64_t incompatible = 0;
    int assigned_limit = num_inputs;
    for (const auto& [type, target, a, b] : circuit.gates) {
      const uint64_t value = CombineGate2(type, values[a], values[b]);
      if (target == BinaryGate::kConstraintTarget) {
        incompatible |= ~value;
        continue;
      }
      if (target < assigned_limit) {
        // This is a double definition!
        // Any possition that differ is incompatible.
        incompatible |= value ^ values[target];
        continue;
      }

      // TODO(user): We assign the value in order, we should be able to be
      // faster than Set() here. Note however that we need to read them right
      // away above. A simple way is to sample/enumerate 64 values at the time.
      // But the equivalence class split might still be slow though.
      CHECK_EQ(target, assigned_limit);
      assigned_limit = target + 1;
      values[target] = value;
    }

    // If this assignment is not possible, skip.
    const uint64_t compatible = ~incompatible;
    if (compatible == 0) continue;
    num_compatibles += absl::popcount(compatible);

    // Start with special case for class of size 2.
    // we expect this to be quite frequent.
    {
      int new_size = 0;
      for (const auto [a, b] : equiv2) {
        uint64_t va = values[num_inputs + a / 2];
        uint64_t vb = values[num_inputs + b / 2];
        if ((a & 1) != (b & 1)) vb = ~vb;
        if ((va & compatible) == (vb & compatible)) {
          equiv2[new_size++] = {a, b};  // keep.
        }
      }
      equiv2.resize(new_size);
    }

    // Once we have shorter equivalence list, it is a lot faster
    // to use sorting than 64 passes.
    if (!first) {
      int new_size = 0;
      new_equivalences.clear();
      for (const absl::Span<const int> equiv : equivalences) {
        local_classes.clear();
        for (const int i : equiv) {
          uint64_t v = values[num_inputs + i / 2];
          if ((i & 1)) v = ~v;
          local_classes.push_back({v & compatible, i});
        }
        absl::c_sort(local_classes);
        const int size = local_classes.size();
        for (int i = 0;;) {
          const int start = i;
          const uint64_t v = local_classes[start].first;
          while (true) {
            ++i;
            if (i >= size || local_classes[i].first != v) break;
          }
          if (i - start > 1) {
            new_equivalences.push_back(
                absl::MakeSpan(&buffer[new_size], i - start));
            for (int j = start; j < i; ++j) {
              buffer[new_size++] = local_classes[j].second;
            }
          }
          if (i + 1 >= size) break;
        }
      }
      std::swap(equivalences, new_equivalences);
      if (equivalences.empty() && equiv2.empty()) break;
      continue;
    }

    for (int j = 0; j < 64; ++j) {
      if ((incompatible >> j) & 1) continue;

      // Split each equivalence class in two, depending on the value of the
      // literal in the assignment.
      int new_size = 0;
      new_equivalences.clear();
      for (const absl::Span<const int> equiv : equivalences) {
        for (const int i : equiv) {
          tmp_part[((values[num_inputs + i / 2] >> j) ^ i) & 1].push_back(i);
        }
        for (int i = 0; i < 2; ++i) {
          if (!first && tmp_part[i].size() == 2) {
            equiv2.push_back({tmp_part[i][0], tmp_part[i][1]});
          } else if (tmp_part[i].size() > 1) {
            // It is okay to reuse buffer since span are always in order.
            new_equivalences.push_back(
                absl::MakeSpan(&buffer[new_size], tmp_part[i].size()));
            for (const int e : tmp_part[i]) buffer[new_size++] = e;
          }
          tmp_part[i].clear();
        }
      }
      std::swap(equivalences, new_equivalences);
      if (equivalences.empty() && equiv2.empty()) break;

      // On the first split, we always split the set in two, one containing the
      // negated literals of the other. We only need to handle one.
      // This divide the time to update equivalences by two!
      if (first) {
        CHECK_EQ(equivalences.size(), 2);
        CHECK_EQ(equivalences[0].size(), equivalences[1].size());
        equivalences.pop_back();
        first = false;
      }
    }
  }

  if (num_compatibles == 0) {
    VLOG(2) << "!!!!!!!!!! No feasible assignment while sampling !!!!!!!!!!";
  }

  int num_equivalences = 0;
  CompactVectorVector<int, Literal> result;
  result.reserve(equiv2.size() + equivalences.size());
  for (const auto [a, b] : equiv2) {
    ++num_equivalences;
    result.Add({Literal(circuit.reverse_mapping[num_inputs + a / 2], a & 1),
                Literal(circuit.reverse_mapping[num_inputs + b / 2], b & 1)});
  }
  for (const absl::Span<const int> equiv : equivalences) {
    num_equivalences += equiv.size() - 1;
    result.Add({});
    for (const int e : equiv) {
      result.AppendToLastVector(
          Literal(circuit.reverse_mapping[num_inputs + e / 2], e & 1));
    }
  }
  VLOG(2) << "#classes:" << result.size() << " equiv?:" << num_equivalences;
  return result;
}

// Similar to SampleForEquivalences() but enumerate all cases, and do not
// maintain any equivalent candidate since the goal is to show infeasibility.
//
// TODO(user): remove duplication with SampleForEquivalences() ?
bool BinaryCircuitIsFeasible(const BinaryCircuit& circuit) {
  // Try all possibilities. 64 at the time.
  const int num_inputs = circuit.num_inputs;
  const int num_vars = circuit.num_vars;
  CHECK_LE(num_inputs, 20);

  FixedCapacityVector<uint64_t> values;
  values.ClearAndReserve(num_vars);

  // TODO(user): use effort instead?
  // Complexity is in roughly num_samples * num_vars.
  const int num_samples = 1 << std::max(0, num_inputs - 6);

  // The first 6 inputs will never changes, they contain all possibilities.
  for (int i = 0; i < std::min(6, num_inputs); ++i) {
    values[i] = 0;
    for (uint64_t j = 0; j < 64; ++j) {
      values[i] |= ((j >> i) & 1) << j;
    }
  }
  for (int start = 0; start < num_samples; ++start) {
    for (int i = 6; i < num_inputs; ++i) {
      values[i] = (start >> (i - 6)) & 1 ? ~uint64_t{0} : uint64_t{0};
    }

    uint64_t incompatible = 0;
    int assigned_limit = num_inputs;
    for (const auto& [type, target, a, b] : circuit.gates) {
      const uint64_t value = CombineGate2(type, values[a], values[b]);
      if (target == BinaryGate::kConstraintTarget) {
        incompatible |= ~value;
        continue;
      }
      if (target < assigned_limit) {
        // This is a double definition!
        // Any position that differ is incompatible.
        incompatible |= value ^ values[target];
        if (~incompatible == 0) break;
        continue;
      }

      // TODO(user): We assign the value in order, we should be able to be
      // faster than Set() here. Note however that we need to read them right
      // away above. A simple way is to sample/enumerate 64 values at the time.
      // But the equivalence class split might still be slow though.
      CHECK_EQ(target, assigned_limit);
      assigned_limit = target + 1;
      values[target] = value;
    }

    // If this assignment is not possible, skip.
    const uint64_t compatible = ~incompatible;
    if (compatible == 0) continue;

    // We have a solution !
    return true;
  }

  // We enumerated everything and found no solution.
  return false;
}

void AddNotEquivalentConstraint(Literal a, Literal b, BinaryCircuit* circuit) {
  int index1 = circuit->mapping[a.Variable()];
  int index2 = circuit->mapping[b.Variable()];
  CHECK_NE(index1, -1);
  CHECK_NE(index2, -1);
  if (index1 > index2) std::swap(index1, index2);
  const bool swap = a.IsPositive() == b.IsPositive();

  for (BinaryGate& gate : circuit->gates) {
    if (gate.a >= gate.target) continue;
    if (gate.b >= gate.target) continue;

    if (gate.target == index2) {
      // If both literal are positive, we want to find a solution with variable
      // that are different !
      gate.target = index1;
      if (swap) gate.type ^= 0b1111;
    }
  }
}

CpModelProto ConstructCpModelFromBinaryCircuit(const BinaryCircuit& circuit,
                                               bool enforce_one_output) {
  CpModelProto cp_model;
  for (int i = 0; i < circuit.num_vars; ++i) {
    auto* var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (const auto [type, target, a, b] : circuit.gates) {
    if (target == BinaryGate::kConstraintTarget) {
      for (int i = 0; i < 4; ++i) {
        if (((type >> i) & 1) == 1) continue;

        // Exclude the assignment.
        auto* ct = cp_model.add_constraints();
        ct->mutable_bool_or()->add_literals(i & 1 ? NegatedRef(a) : a);
        ct->mutable_bool_or()->add_literals((i / 2) & 1 ? NegatedRef(b) : b);
      }
      continue;
    }

    for (int i = 0; i < 4; ++i) {
      auto* ct = cp_model.add_constraints();
      ct->add_enforcement_literal(i & 1 ? a : NegatedRef(a));
      ct->add_enforcement_literal((i / 2) & 1 ? b : NegatedRef(b));
      ct->mutable_bool_or()->add_literals((type >> i) & 1 ? target
                                                          : NegatedRef(target));
    }
  }

  if (enforce_one_output) {
    auto* bool_or = cp_model.add_constraints()->mutable_bool_or();
    for (const int index : circuit.outputs) {
      bool_or->add_literals(index);
    }
  }

  return cp_model;
}

CpModelProto CpModelUsingLargeAnds(const BinaryCircuit& circuit,
                                   bool enforce_one_output) {
  std::vector<int> defining_gate(circuit.num_vars, -1);
  for (int g = 0; g < circuit.gates.size(); ++g) {
    const BinaryGate& gate = circuit.gates[g];
    CHECK_NE(gate.target, BinaryGate::kConstraintTarget);  // Not supported.
    if (defining_gate[gate.target] >= 0) continue;
    defining_gate[gate.target] = g;
  }

  std::vector<std::vector<std::pair<int, int>>> and_gates(circuit.num_vars);

  int num_large_ands = 0;
  for (const BinaryGate& main_gate : circuit.gates) {
    std::vector<std::pair<int, int>> assignment;
    assignment.push_back(
        {main_gate.target, absl::popcount(main_gate.type) == 1 ? 1 : 0});
    for (int i = 0; i < assignment.size(); ++i) {
      const auto [target, value] = assignment[i];
      const int g = defining_gate[target];
      if (g == -1) continue;

      const BinaryGate gate = circuit.gates[g];
      if ((value == 1 && std::popcount(gate.type) == 1) ||
          (value == 0 && std::popcount(gate.type) == 3)) {
        for (int j = 0; j < 4; ++j) {
          if (((gate.type >> j) & 1) == value) {
            assignment[i] = {gate.a, j & 1};
            assignment.push_back({gate.b, (j >> 1) & 1});
            break;
          }
        }

        // Process again position i.
        i--;
      }
    }

    // TODO(user): If there is duplicate, check value are the same.
    gtl::STLSortAndRemoveDuplicates(&assignment);
    if (assignment.size() > 2) {
      ++num_large_ands;
      and_gates[main_gate.target] = assignment;
    }
  }

  // Analyze the dependency to find useful variables.
  std::vector<int> queue;
  std::vector<bool> used(circuit.num_vars, false);
  for (const int var : circuit.outputs) {
    if (!used[var]) {
      queue.push_back(var);
      used[var] = true;
    }
  }
  while (!queue.empty()) {
    const int v = queue.back();
    queue.pop_back();

    if (!and_gates[v].empty()) {
      for (const auto [var, unused] : and_gates[v]) {
        if (!used[var]) {
          used[var] = true;
          queue.push_back(var);
        }
      }
    } else if (defining_gate[v] >= 0) {
      const BinaryGate& gate = circuit.gates[defining_gate[v]];
      for (const int var : {gate.a, gate.b}) {
        if (!used[var]) {
          used[var] = true;
          queue.push_back(var);
        }
      }
    }
  }

  int num_vars = 0;
  for (int v = 0; v < circuit.num_vars; ++v) {
    if (used[v]) ++num_vars;
  }
  VLOG(2) << num_vars << " / " << circuit.num_vars << " large ands "
          << num_large_ands;

  // Generate proto.
  CpModelProto cp_model;
  for (int i = 0; i < circuit.num_vars; ++i) {
    auto* var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int v = 0; v < circuit.num_vars; ++v) {
    if (!used[v]) continue;
    if (defining_gate[v] == -1) continue;
    const auto [type, target, a, b] = circuit.gates[defining_gate[v]];

    if (!and_gates[v].empty()) {
      // Encode large and gate.
      const int main_value = absl::popcount(type) == 1 ? 1 : 0;
      auto* bool_or = cp_model.add_constraints()->mutable_bool_or();
      bool_or->add_literals(main_value == 1 ? v : NegatedRef(v));
      for (const auto [var, value] : and_gates[v]) {
        // all [var, value] pairs => v == main_value
        bool_or->add_literals(value == 1 ? NegatedRef(var) : var);

        // v == main_value => all [var, value] pairs.
        auto* implication = cp_model.add_constraints();
        implication->add_enforcement_literal(main_value == 1 ? v
                                                             : NegatedRef(v));
        implication->mutable_bool_and()->add_literals(
            value == 1 ? var : NegatedRef(var));
      }

    } else if (defining_gate[v] >= 0) {
      for (int i = 0; i < 4; ++i) {
        auto* ct = cp_model.add_constraints();
        ct->add_enforcement_literal(i & 1 ? a : NegatedRef(a));
        ct->add_enforcement_literal((i / 2) & 1 ? b : NegatedRef(b));
        ct->mutable_bool_or()->add_literals(
            (type >> i) & 1 ? target : NegatedRef(target));
      }
    }
  }

  if (enforce_one_output) {
    auto* bool_or = cp_model.add_constraints()->mutable_bool_or();
    for (const int index : circuit.outputs) {
      bool_or->add_literals(index);
    }
  }

  return cp_model;
}

void ReduceGates(BinaryCircuit* circuit) {
  const int num_vars = circuit->num_vars;
  std::vector<std::vector<int>> var_to_gates(num_vars);
  for (int i = 0; i < circuit->gates.size(); ++i) {
    const BinaryGate& gate = circuit->gates[i];

    // Skip constraints.
    if (gate.target == BinaryGate::kConstraintTarget) continue;
    if (!var_to_gates[gate.target].empty()) continue;

    var_to_gates[gate.target].push_back(i);
  }

  std::vector<int> queue;
  std::vector<bool> values(num_vars, false);
  std::vector<bool> seen(num_vars, false);

  // TODO(user): optimize / rework ?
  std::vector<int> expanded;
  int num_changes = 0;
  for (int v = 0; v < num_vars; ++v) {
    if (var_to_gates[v].empty()) continue;

    // We leave gate of arity one as is,
    // These should always be removed.
    if (circuit->gates[var_to_gates[v][0]].a ==
        circuit->gates[var_to_gates[v][0]].b) {
      continue;
    }

    queue.clear();
    seen[v] = true;
    queue.push_back(v);
    expanded.clear();
    for (int num_expand = 0; num_expand < 10 && !queue.empty(); ++num_expand) {
      const int top = queue.front();

      // Because we process in topo order,
      // when we are here, only inputs are left.
      if (var_to_gates[top].empty()) {
        CHECK_LT(top, circuit->num_inputs);
        break;
      }

      absl::c_pop_heap(queue);
      queue.pop_back();

      // Expand.
      expanded.push_back(top);
      for (const int index : var_to_gates[top]) {
        const BinaryGate g = circuit->gates[index];
        for (const int index : {g.a, g.b}) {
          if (seen[index]) continue;
          seen[index] = true;
          queue.push_back(index);
          absl::c_push_heap(queue);
        }
      }

      // Done, we have a reduction.
      if (expanded.size() > 1 && queue.size() == 2) break;
    }

    // Sparse clear.
    for (const int i : expanded) seen[i] = false;
    for (const int i : queue) seen[i] = false;

    if (expanded.size() > 1 && queue.size() == 2) {
      absl::c_reverse(expanded);

      if (queue[0] > queue[1]) std::swap(queue[0], queue[1]);

      bool skip = false;
      SmallBitset new_type = 0;
      for (int i = 0; i < 4; ++i) {
        values[queue[0]] = i & 1;
        values[queue[1]] = (i / 2) & 1;
        for (const int var : expanded) {
          int first = true;
          int value = 0;
          for (const int index : var_to_gates[var]) {
            BinaryGate g = circuit->gates[index];
            const int val = (g.type >> (values[g.a] + 2 * values[g.b])) & 1;
            if (first) {
              first = false;
              value = val;
            } else {
              if (value != val) skip = true;
            }
          }
          values[var] = value;
        }
        new_type |= values[v] << i;
      }
      if (!skip) {
        ++num_changes;
        const int new_gate_index = var_to_gates[v][0];
        circuit->gates[new_gate_index].type = new_type;
        circuit->gates[new_gate_index].a = queue[0];
        circuit->gates[new_gate_index].b = queue[1];
      }
    }
  }

  SubcircuitExtractor extractor(*circuit);
  *circuit = extractor.Extract(circuit->outputs);
}

// In order to reduce the amount of nodes, we "expand" all node with a single
// usage of their output. That result in node that are still a Boolean function
// with one output, but can have a lot more than 2 inputs.
//
// Note that such function are "easy" candidate for rewriting if the goal is to
// optimize the circuit.
std::string ToDotFile(const BinaryCircuit& circuit,
                      absl::Span<const int> special_nodes) {
  std::vector<int> out_degree(circuit.num_vars, 0);
  std::vector<int> num_def(circuit.num_vars, 0);
  std::vector<int> types(circuit.num_vars, 0);
  std::vector<std::pair<int, int>> reverse_arcs;
  for (const BinaryGate& gate : circuit.gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;

    types[gate.target] = gate.type;
    out_degree[gate.a]++;
    out_degree[gate.b]++;
    num_def[gate.target]++;

    if (gate.a < gate.target && gate.b < gate.target) {
      reverse_arcs.push_back({gate.target, gate.a});
      if (gate.a != gate.b) {
        reverse_arcs.push_back({gate.target, gate.b});
      }
    }
  }
  CompactVectorVector<int, int> dependency;
  dependency.ResetFromPairs(reverse_arcs, circuit.num_vars);

  std::vector<int> nodes;
  std::vector<std::pair<int, int>> arcs;

  std::vector<bool> seen(circuit.num_vars, false);
  std::vector<int> queue;
  for (int node = 0; node < circuit.num_vars; ++node) {
    if (out_degree[node] == 1) {
      if (node < circuit.num_inputs) nodes.push_back(node);
      continue;
    }
    nodes.push_back(node);

    // Expand all node of out_degree[] 1.
    queue.clear();
    for (const int before : dependency[node]) {
      if (!seen[before]) {
        seen[before] = true;
        queue.push_back(before);
      }
    }
    absl::c_make_heap(queue);

    // Follow the dependency.
    int num_expanded = 1;
    std::vector<int> left;
    while (!queue.empty()) {
      const int top = queue.front();
      absl::c_pop_heap(queue);
      queue.pop_back();
      seen[top] = false;  // sparse clear.

      if (out_degree[top] > 1 || top < circuit.num_inputs) {
        left.push_back(top);
      } else {
        ++num_expanded;
        for (const int before : dependency[top]) {
          CHECK_LT(before, top);
          if (!seen[before]) {
            seen[before] = true;
            queue.push_back(before);
            absl::c_push_heap(queue);
          }
        }
      }
    }

    for (const int tail : left) {
      arcs.push_back({tail, node});
    }
  }

  VLOG(2) << "DOT NODES " << nodes.size() << " ARCS " << arcs.size();

  // Try basic.
  std::string dot = "digraph {\n";

  // Output nodes.
  absl::flat_hash_set<int> special_node_set(special_nodes.begin(),
                                            special_nodes.end());
  for (const int node : nodes) {
    absl::StrAppend(&dot, node, "[label=\"", node, "\n",
                    std::bitset<4>(types[node]).to_string(), "\"");
    if (out_degree[node] == 0) {
      absl::StrAppend(&dot, ",color=red");
    } else if (dependency[node].empty()) {
      absl::StrAppend(&dot, ",color=blue");
    } else if (num_def[node] > 1) {
      absl::StrAppend(&dot, ",color=red");
    }
    if (special_node_set.contains(node)) {
      absl::StrAppend(&dot, ",style=filled,fillcolor=lightblue");
    }
    absl::StrAppend(&dot, "];\n");
  }

  // Output edges.
  for (const auto [a, b] : arcs) {
    absl::StrAppend(&dot, a, "->", b, "\n");
  }

  // Finish.
  absl::StrAppend(&dot, "}\n");
  return dot;
}

std::string ToBenchFile(const BinaryCircuit& circuit) {
  std::string output;
  std::vector<std::string> names(circuit.num_vars);
  for (int i = 0; i < circuit.num_vars; ++i) {
    if (i < circuit.num_inputs) {
      names[i] = absl::StrCat("I", i);
      absl::StrAppend(&output, "INPUT(", names[i], ")\n");
    } else {
      names[i] = absl::StrCat("V", i);
    }
  }

  absl::StrAppend(&output, "\n");
  for (const int i : circuit.outputs) {
    absl::StrAppend(&output, "OUTPUT(", names[i], ")\n");
  }

  absl::StrAppend(&output, "\n");
  for (const BinaryGate& gate : circuit.gates) {
    absl::StrAppend(&output, names[gate.target], "=LUT 0x",
                    absl::Hex(gate.type), " (", names[gate.a], ",",
                    names[gate.b], ")\n");
  }

  return output;
}

SubcircuitExtractor::SubcircuitExtractor(const BinaryCircuit& circuit)
    : mitter_(circuit) {
  // Do some precomputation.
  std::vector<std::pair<int, int>> reverse_arcs;
  for (const BinaryGate& gate : circuit.gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;
    if (gate.a < gate.target && gate.b < gate.target) {
      reverse_arcs.push_back({gate.target, gate.a});
      if (gate.a != gate.b) {
        reverse_arcs.push_back({gate.target, gate.b});
      }
    }
  }
  dependency_.ResetFromPairs(reverse_arcs, circuit.num_vars);
}

BinaryCircuit SubcircuitExtractor::Extract(absl::Span<const Literal> literals) {
  std::vector<int> temp;
  temp.reserve(literals.size());
  for (const Literal l : literals) {
    temp.push_back(mitter_.mapping[l.Variable()]);
  }
  return Extract(temp);
}

BinaryCircuit SubcircuitExtractor::Extract(absl::Span<const int> new_outputs) {
  BinaryCircuit subproblem;

  queue_.clear();
  seen_.assign(mitter_.num_vars, false);
  for (const int index : new_outputs) {
    if (!seen_[index]) {
      seen_[index] = true;
      queue_.push_back(index);
      subproblem.outputs.push_back(index);  // Will be remapped below
    }
  }
  absl::c_make_heap(queue_);

  // Follow the dependency to the new inputs.
  int num_seen = 0;
  std::vector<int> new_inputs;
  std::vector<int> new_dependant_vars;
  while (!queue_.empty()) {
    const int top = queue_.front();
    std::pop_heap(queue_.begin(), queue_.end());
    queue_.pop_back();
    if (dependency_[top].empty()) {
      new_inputs.push_back(top);
    } else {
      new_dependant_vars.push_back(top);
      for (const int index : dependency_[top]) {
        if (!seen_[index]) {
          ++num_seen;
          seen_[index] = true;
          queue_.push_back(index);
          absl::c_push_heap(queue_);
        }
      }
    }
  }

  // We sort the new inputs to "keep" the order of the original circuit.
  // This preserve a bit more the semantic.
  absl::c_sort(new_inputs);

  // Extract the subproblem.
  subproblem.num_inputs = new_inputs.size();
  subproblem.num_vars = new_inputs.size() + new_dependant_vars.size();
  std::vector<int> local_mapping(mitter_.num_vars, -1);
  for (const int input : new_inputs) {
    CHECK_EQ(local_mapping[input], -1);
    local_mapping[input] = subproblem.reverse_mapping.size();
    subproblem.reverse_mapping.push_back(mitter_.reverse_mapping[input]);
  }

  for (const BinaryGate& gate : mitter_.gates) {
    if (local_mapping[gate.a] == -1) continue;
    if (local_mapping[gate.b] == -1) continue;

    if (gate.target == BinaryGate::kConstraintTarget) {
      if (local_mapping[gate.a] != -1 && local_mapping[gate.b] != -1) {
        // Keep constraints.
        subproblem.gates.emplace_back(gate.type, gate.target,
                                      local_mapping[gate.a],
                                      local_mapping[gate.b]);
      }
      continue;
    }

    if (!seen_[gate.target]) continue;
    if (local_mapping[gate.target] == -1) {
      local_mapping[gate.target] = subproblem.reverse_mapping.size();
      subproblem.reverse_mapping.push_back(
          mitter_.reverse_mapping[gate.target]);
    }
    subproblem.gates.emplace_back(gate.type, local_mapping[gate.target],
                                  local_mapping[gate.a], local_mapping[gate.b]);
  }

  subproblem.mapping.assign(mitter_.mapping.size(), -1);
  for (int i = 0; i < subproblem.reverse_mapping.size(); ++i) {
    subproblem.mapping[subproblem.reverse_mapping[i]] = i;
  }

  // Remap the outputs.
  for (int& ref : subproblem.outputs) {
    ref = subproblem.mapping[mitter_.reverse_mapping[ref]];
  }

  return subproblem;
}

BinaryCircuit ConstructMitter(const BinaryCircuit& circuit_a,
                              const BinaryCircuit& circuit_b) {
  CHECK_GE(circuit_a.num_inputs, circuit_b.num_inputs);
  CHECK_EQ(circuit_b.outputs.size(), circuit_b.outputs.size());

  const int num_outputs = circuit_a.outputs.size();

  // Start by copying circuit_a.
  BinaryCircuit mitter = circuit_a;
  mitter.gates.reserve(circuit_a.gates.size() + circuit_b.gates.size() +
                       num_outputs);

  // All inputs from circuit b should correspond to an input in circuit a.
  std::vector<int> input_mapping(circuit_b.num_inputs);
  for (int i = 0; i < circuit_b.num_inputs; ++i) {
    const BooleanVariable var = circuit_b.reverse_mapping[i];
    CHECK_LT(var, circuit_a.mapping.size());
    const int image = circuit_a.mapping[var];
    CHECK_LT(image, circuit_a.num_inputs);
    input_mapping[i] = image;
  }

  // Inputs do not change, but all subsequent one will be shifted.
  const int shift = circuit_a.num_vars - circuit_b.num_inputs;
  const auto remap = [shift, input_mapping](int index) {
    if (index < input_mapping.size()) {
      return input_mapping[index];
    }
    return shift + index;
  };
  for (const BinaryGate& gate : circuit_b.gates) {
    mitter.gates.emplace_back(gate.type, remap(gate.target), remap(gate.a),
                              remap(gate.b));
  }
  mitter.num_vars += circuit_b.num_vars - circuit_b.num_inputs;

  // Lets create new gate for the output "differences";
  // These are the new inputs.
  mitter.outputs.clear();
  for (int i = 0; i < num_outputs; ++i) {
    mitter.gates.emplace_back(0b0110, mitter.num_vars, circuit_a.outputs[i],
                              remap(circuit_b.outputs[i]));
    mitter.outputs.push_back(mitter.num_vars);
    ++mitter.num_vars;
  }

  // The mapping/reverse mapping do not really make sense in this context.
  // We re-initialize it.
  mitter.ResetBooleanMapping();
  return mitter;
}

// TODO(user): If one call proved all potential equivalences, we can stop.
// TODO(user): congruence closure is faster... resuse sat code somehow?
void SimplifyCircuit(
    int max_num_solves, absl::BitGenRef random,
    std::function<CpSolverResponse(const CpModelProto& cp_model)> solve,
    std::vector<std::vector<BooleanVariable>>* saved_solutions,
    BinaryCircuit* circuit) {
  CompactVectorVector<int, Literal> equiv =
      SampleForEquivalences(*circuit, random, *saved_solutions);

  struct ComplexityEntry {
    int num_vars;
    int num_inputs;
    Literal a;
    Literal b;
  };
  std::vector<ComplexityEntry> complexity;

  SubcircuitExtractor extractor(*circuit);

  std::vector<std::pair<Literal, Literal>> new_equiv;
  for (int c = 0; c < equiv.size(); ++c) {
    const auto& literals = equiv[c];
    for (int k = 1; k < literals.size(); ++k) {
      const Literal a = literals[0];
      const Literal b = literals[k];

      CHECK_LT(a.Index(), b.Index());
      BinaryCircuit lmp = extractor.Extract({a, b});

      if (lmp.num_inputs <= 20) {
        AddNotEquivalentConstraint(a, b, &lmp);
        const bool are_equivalent = !BinaryCircuitIsFeasible(lmp);
        VLOG(3) << lmp.DebugString() << " equiv: " << are_equivalent << " " << a
                << " " << b;
        if (are_equivalent) new_equiv.push_back({a, b});
        continue;
      }

      complexity.push_back({lmp.num_vars, lmp.num_inputs, a, b});
    }
  }

  std::sort(complexity.begin(), complexity.end(),
            [](const ComplexityEntry& a, const ComplexityEntry& b) {
              return a.num_vars < b.num_vars;
            });

  CpModelProto local_cp_model;
  int num_tried = 0;
  int num_displayed = 0;
  std::vector<BooleanVariable> solution;
  for (int i = 0; i < complexity.size(); ++i) {
    if (++num_tried <= max_num_solves) {
      BinaryCircuit lmp = extractor.Extract({complexity[i].a, complexity[i].b});
      AddNotEquivalentConstraint(complexity[i].a, complexity[i].b, &lmp);
      local_cp_model = ConstructCpModelFromBinaryCircuit(lmp);
      const std::string info = absl::StrCat(lmp.num_vars, "_", lmp.num_inputs);

      bool proven_equiv = false;
      const CpSolverResponse response = solve(local_cp_model);
      if (response.status() == CpSolverStatus::INFEASIBLE) {
        proven_equiv = true;
      } else if (response.status() == CpSolverStatus::OPTIMAL) {
        // We extract the inputs that show non-equivalence for future sampling.
        solution.clear();
        for (int i = 0; i < lmp.num_inputs; ++i) {
          CHECK_LT(i, response.solution().size());
          const bool value = response.solution(i) == 1;
          if (value) {
            solution.push_back(lmp.reverse_mapping[i]);
          }
        }
        saved_solutions->push_back(solution);
      }

      VLOG(3) << i + 1 << "/" << complexity.size() << " " << lmp.DebugString()
              << " equiv: " << proven_equiv << " (with solver) "
              << complexity[i].a << " " << complexity[i].b;
      if (proven_equiv) {
        new_equiv.push_back({complexity[i].a, complexity[i].b});
      }
      continue;
    }

    if (++num_displayed <= 5 || i + 5 >= complexity.size()) {
      if (i + 5 == complexity.size()) {
        VLOG(3) << "...";
      } else {
        VLOG(3) << "vars " << complexity[i].num_vars << " inputs "
                << complexity[i].num_inputs;
      }
    }
  }

  VLOG(3) << "NEW equivalences" << new_equiv.size();
  RemoveEquivalences(new_equiv, circuit);
  ReduceGates(circuit);
}

void RemoveEquivalences(absl::Span<const std::pair<Literal, Literal>> equiv,
                        BinaryCircuit* circuit,
                        absl::Span<const Literal> extra_fixing) {
  // TODO(user): use an union find since we augment this with unary gate
  // equivalences.
  std::vector<LiteralIndex> representative(circuit->num_vars, kNoLiteralIndex);
  for (auto [a, b] : equiv) {
    if (a.Index() > b.Index()) std::swap(a, b);
    if (!b.IsPositive()) {
      a = a.Negated();
      b = b.Negated();
    }
    representative[circuit->mapping[b.Variable()]] = a.Index();
  }

  // For fixed variables.
  std::vector<bool> is_fixed(circuit->num_vars, false);
  std::vector<bool> is_fixed_to_true(circuit->num_vars, false);
  for (const Literal lit : extra_fixing) {
    const int var = circuit->mapping[lit.Variable()];
    is_fixed[var] = true;
    is_fixed_to_true[var] = lit.IsPositive();
  }

  // Just loop over the gates and remap inputs.
  int num_extra_equivalences = 0;
  int num_fixed = 0;
  for (BinaryGate& gate : circuit->gates) {
    if (representative[gate.a] != kNoLiteralIndex) {
      const Literal lit(representative[gate.a]);
      gate.a = circuit->mapping[lit.Variable()];
      if (!lit.IsPositive()) {
        // swap bits 0,1 and 2,3.
        int new_type = 0;
        for (int i = 0; i < 4; ++i) {
          new_type |= ((gate.type >> i) & 1) << (i ^ 1);
        }
        gate.type = new_type;
      }
    }

    if (representative[gate.b] != kNoLiteralIndex) {
      const Literal lit(representative[gate.b]);
      gate.b = circuit->mapping[lit.Variable()];
      if (!lit.IsPositive()) {
        // swap bit 0,2 and 1, 3.
        int new_type = 0;
        for (int i = 0; i < 4; ++i) {
          new_type |= ((gate.type >> i) & 1) << (i ^ 2);
        }
        gate.type = new_type;
      }
    }

    if (is_fixed[gate.a] && is_fixed[gate.b]) {
      // Target is also fixed.
      const int index = (is_fixed_to_true[gate.a] ? 1 : 0) +
                        (is_fixed_to_true[gate.b] ? 2 : 0);
      if ((gate.type >> index) & 1) {
        gate.type = 0b1111;
      } else {
        gate.type = 0b0000;
      }
    } else if (is_fixed[gate.a]) {
      // Function of b.
      const int index = (is_fixed_to_true[gate.a] ? 1 : 0);
      const int subtype =
          ((gate.type >> index) & 1) + 2 * ((gate.type >> (index ^ 2)) & 1);
      gate.type = subtype + (subtype << 2);
      gate.a = gate.b;
    } else if (is_fixed[gate.b]) {
      // Function of a.
      const int index = (is_fixed_to_true[gate.b] ? 2 : 0);
      const int subtype =
          ((gate.type >> index) & 1) + 2 * ((gate.type >> (index ^ 1)) & 1);
      gate.type = subtype + (subtype << 2);
      gate.b = gate.a;
    }

    gate.Simplify();

    if (gate.type == 0b1111) {
      ++num_fixed;
      is_fixed[gate.target] = true;
      is_fixed_to_true[gate.target] = true;
      continue;
    }

    if (gate.type == 0b0000) {
      ++num_fixed;
      is_fixed[gate.target] = true;
      is_fixed_to_true[gate.target] = false;
      continue;
    }

    // Deal with unary gates.
    if (representative[gate.target] == kNoLiteralIndex) {
      if (gate.a == gate.b) {
        CHECK_LT(gate.a, gate.target);
        const int subtype = (gate.type & 1) + 2 * ((gate.type >> 3) & 1);
        if (subtype == 0b10) {
          // target = a.
          ++num_extra_equivalences;
          representative[gate.target] =
              Literal(circuit->reverse_mapping[gate.a], true).Index();
          CHECK_EQ(circuit->mapping[circuit->reverse_mapping[gate.a]], gate.a);
        } else if (subtype == 0b01) {
          // target = neg(a).
          ++num_extra_equivalences;
          representative[gate.target] =
              Literal(circuit->reverse_mapping[gate.a], false).Index();
          CHECK_EQ(circuit->mapping[circuit->reverse_mapping[gate.a]], gate.a);
        } else if (subtype == 0b00) {
          ++num_fixed;
          is_fixed[gate.target] = true;
          is_fixed_to_true[gate.target] = false;
        } else {
          DCHECK_EQ(subtype, 0b11);
          ++num_fixed;
          is_fixed[gate.target] = true;
          is_fixed_to_true[gate.target] = true;
        }
      }
    }
  }

  if (num_extra_equivalences > 0) {
    VLOG(2) << "num extra equivalences = " << num_extra_equivalences;
  }

  if (num_fixed > 0) {
    VLOG(2) << "num fixed = " << num_fixed;
  }

  // Remove unreachable.
  SubcircuitExtractor extractor(*circuit);
  *circuit = extractor.Extract(circuit->outputs);
}

}  // namespace operations_research::sat
