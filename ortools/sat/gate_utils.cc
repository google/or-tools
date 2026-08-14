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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/numeric/bits.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"

namespace operations_research::sat {

std::string BinaryCircuit::DebugString() const {
  int max_depth = 0;
  std::vector<int> depths(num_vars);

  // All these case should be easily simplifiable.
  int num_todo = 0;
  for (const BinaryGate& gate : gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;

    depths[gate.target] = std::max(depths[gate.target], depths[gate.a] + 1);
    depths[gate.target] = std::max(depths[gate.target], depths[gate.b] + 1);
    max_depth = std::max(max_depth, depths[gate.target]);

    if (gate.a == gate.b || gate.type == 0b0000 || gate.type == 0b1111 ||
        gate.type == 0b0101 || gate.type == 0b1010 || gate.type == 0b0011 ||
        gate.type == 0b1100) {
      ++num_todo;
    }
  }

  return absl::StrCat("#inputs:", num_inputs, " #vars:", num_vars,
                      " #gates:", gates.size(), " #outputs:", outputs.size(),
                      " #constraints:",
                      static_cast<int>(gates.size()) - (num_vars - num_inputs),
                      " #simplifiable:", num_todo, " #depth:", max_depth);
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
        circuit->gates[new_gate_index].Simplify();
      }
    }
  }

  SubcircuitExtractor extractor(*circuit);
  *circuit = extractor.Extract(circuit->outputs);
}

BinaryCircuit ConvertInnerNodeToInputs(const BinaryCircuit& circuit,
                                       absl::Span<const int> new_inputs) {
  // We will need a big remapping.
  std::vector<int> mapping(circuit.num_vars, -1);
  int new_index = 0;
  for (int i = 0; i < circuit.num_inputs; ++i) mapping[i] = new_index++;
  for (const int i : new_inputs) {
    if (mapping[i] != -1) continue;  // Already seen.
    mapping[i] = new_index++;
  }
  const int new_num_inputs = new_index;
  for (int i = 0; i < mapping.size(); ++i) {
    if (mapping[i] == -1) {
      mapping[i] = new_index++;
    }
  }
  CHECK_EQ(new_index, circuit.num_vars);

  // IMPORTANT special case: If one of the new_inputs is a negation of another
  // variable then we want any dependency on the other variabe to be the
  // negation of that input instead !
  std::vector<int> rewrite_as_negation(circuit.num_vars, -1);
  for (BinaryGate gate : circuit.gates) {
    if (gate.a == gate.b && mapping[gate.target] <= new_num_inputs) {
      const int subtype = (gate.type & 1) + 2 * ((gate.type >> 3) & 1);
      if (subtype == 0b01) {
        rewrite_as_negation[gate.a] = gate.target;
      }
    }
  }

  BinaryCircuit new_circuit;
  new_circuit.num_vars = circuit.num_vars;
  new_circuit.num_inputs = new_num_inputs;

  // Now remap the gates.
  for (BinaryGate gate : circuit.gates) {
    // We remove constraint for now.
    if (gate.type == BinaryGate::kConstraintTarget) continue;
    if (mapping[gate.target] <= new_num_inputs) continue;  // Remove.

    // First rewrite as negation !
    if (rewrite_as_negation[gate.target] != -1) {
      gate.a = gate.b = rewrite_as_negation[gate.target];
      gate.type = 0b0001;
      CHECK(mapping[gate.a] <= new_num_inputs);
    }

    gate.target = mapping[gate.target];
    gate.a = mapping[gate.a];
    gate.b = mapping[gate.b];
    new_circuit.gates.push_back(gate);
  }

  new_circuit.outputs.reserve(circuit.outputs.size());
  for (const int out : circuit.outputs) {
    new_circuit.outputs.push_back(mapping[out]);
  }

  // Update the mapping.
  new_circuit.reverse_mapping.resize(new_circuit.num_vars);
  for (int i = 0; i < new_circuit.num_vars; ++i) {
    new_circuit.reverse_mapping[mapping[i]] = circuit.reverse_mapping[i];
  }
  new_circuit.mapping = circuit.mapping;
  for (int& node : new_circuit.mapping) {
    if (node == -1) continue;
    node = mapping[node];
  }

  return new_circuit;
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
  CompactVectorVectorBuilder<int, int> dependency_builder;
  for (const BinaryGate& gate : circuit.gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;

    types[gate.target] = gate.type;
    out_degree[gate.a]++;
    out_degree[gate.b]++;
    num_def[gate.target]++;

    if (gate.a < gate.target && gate.b < gate.target) {
      dependency_builder.Add(gate.target, gate.a);
      if (gate.a != gate.b) {
        dependency_builder.Add(gate.target, gate.b);
      }
    }
  }
  const CompactVectorVector<int, int> dependency(dependency_builder,
                                                 circuit.num_vars);

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
  CHECK_EQ(circuit.reverse_mapping.size(), circuit.num_vars);

  // Do some precomputation.
  CompactVectorVectorBuilder<int, int> dependency_builder;
  for (const BinaryGate& gate : circuit.gates) {
    if (gate.target == BinaryGate::kConstraintTarget) continue;
    if (gate.a < gate.target && gate.b < gate.target) {
      dependency_builder.Add(gate.target, gate.a);
      if (gate.a != gate.b) {
        dependency_builder.Add(gate.target, gate.b);
      }
    }
  }
  dependency_.ResetFromBuilder(dependency_builder, circuit.num_vars);
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
  int num_duplicate_outputs = 0;
  for (const int index : new_outputs) {
    if (!seen_[index]) {
      seen_[index] = true;
      queue_.push_back(index);
      subproblem.outputs.push_back(index);  // Will be remapped below
    } else {
      ++num_duplicate_outputs;
      subproblem.outputs.push_back(index);
    }
  }
  if (num_duplicate_outputs > 0) {
    VLOG(2) << num_duplicate_outputs << " duplicate outputs !";
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

  // Let's create new gate for the output "differences";
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

BinaryCircuit ConstructDecomposition(int m, const BinaryCircuit& circuit) {
  BinaryCircuit result;

  const int n = circuit.num_inputs;
  CHECK_LT(m, n);
  result.num_inputs = n + (n - m);
  result.num_vars = result.num_inputs;

  // Evaluate f(a, b).
  std::vector<int> input_map(n);
  for (int i = 0; i < n; ++i) {
    input_map[i] = i;
  }
  const std::vector<int> outputs_a_b =
      AppendCircuit(input_map, circuit, &result);

  // Evaluate f(0, b).
  for (int i = 0; i < n; ++i) {
    input_map[i] = i < m ? -1 : i;
  }
  const std::vector<int> outputs_0_b =
      AppendCircuit(input_map, circuit, &result);

  // Evaluate f(a, b2).
  for (int i = 0; i < n; ++i) {
    input_map[i] = i < m ? i : n + (i - m);
  }
  const std::vector<int> outputs_a_b2 =
      AppendCircuit(input_map, circuit, &result);

  // Evaluate f(0, b2).
  for (int i = 0; i < n; ++i) {
    input_map[i] = i < m ? -1 : n + (i - m);
  }
  const std::vector<int> outputs_0_b2 =
      AppendCircuit(input_map, circuit, &result);

  // Constraint f(0, b) to be f(0, b2).
  const int num_outputs = circuit.outputs.size();
  for (int i = 0; i < num_outputs; ++i) {
    result.gates.emplace_back(0b1001, BinaryGate::kConstraintTarget,
                              outputs_0_b[i], outputs_0_b2[i]);
  }

  // The new output is f(a,b) != f(a, b2).
  for (int i = 0; i < num_outputs; ++i) {
    result.gates.emplace_back(0b0110, result.num_vars, outputs_a_b[i],
                              outputs_a_b2[i]);
    result.outputs.push_back(result.num_vars);
    ++result.num_vars;
  }

  result.ResetBooleanMapping();
  return result;
}

bool SampleDecomposition(int m, const BinaryCircuit& circuit) {
  const int n = m + circuit.outputs.size();  // new inputs.
  if (n >= 20) return false;
  if (circuit.outputs.size() >= 64) return false;

  // The function g().
  int num_seen = 0;
  std::vector<bool> g_seen((1 << n), false);
  std::vector<uint64_t> g_values(1 << n);

  FixedCapacityVector<uint64_t> values;
  FixedCapacityVector<uint64_t> m_values;
  values.ClearAndReserve(circuit.num_vars);
  m_values.ClearAndReserve(circuit.num_vars);

  // We can sample 64 bits at the time.
  const int num_samples = 1 << 20;
  absl::BitGen random;
  for (int start = 0; start < num_samples; ++start) {
    for (int i = 0; i < circuit.num_inputs; ++i) {
      values[i] = absl::Uniform<uint64_t>(random);
      if (i >= m) {
        m_values[i] = values[i];
      } else {
        m_values[i] = 0;
      }
    }

    // We evaluate both f(m_input, other_inputs) and f(0, other_inputs) at the
    // same time.
    for (const auto& [type, target, a, b] : circuit.gates) {
      values[target] = CombineGate2(type, values[a], values[b]);
      m_values[target] = CombineGate2(type, m_values[a], m_values[b]);
    }

    // Reconstruct the 64 evaluation of g().
    for (uint64_t pos = 0; pos < 64; ++pos) {
      uint64_t g_input = 0;
      uint64_t g_output = 0;
      int k = 0;
      int l = 0;
      for (int i = 0; i < m; ++i) {
        g_input |= ((values[i] >> pos) & 1) << k;
        k++;
      }
      for (const int o : circuit.outputs) {
        g_input |= ((m_values[o] >> pos) & 1) << k;
        g_output |= ((values[o] >> pos) & 1) << l;
        k++;
        l++;
      }
      DCHECK_EQ(k, n);
      DCHECK_EQ(l, circuit.outputs.size());
      if (!g_seen[g_input]) {
        ++num_seen;
        g_seen[g_input] = true;
        g_values[g_input] = g_output;
      } else {
        if (g_values[g_input] != g_output) {
          LOG(INFO) << "Not decomposable ! " << FormatCounter(64 * start) << " "
                    << std::bitset<20>(g_input) << " "
                    << std::bitset<20>(g_output) << " was "
                    << std::bitset<20>(g_values[g_input]);
          return false;
        }
      }
    }
  }

  LOG(INFO) << "Seems decomposable " << FormatCounter(num_seen) << "/ "
            << FormatCounter(1 << n) << " #samples "
            << FormatCounter(64 * num_samples);
  return true;
}

bool RecoverNWayAddition(const BinaryCircuit& circuit, int num_samples) {
  if (circuit.outputs.size() >= 64) return false;

  FixedCapacityVector<uint64_t> values;
  values.ClearAndReserve(circuit.num_vars);

  // First find out the mapping input bit -> output bit.
  std::vector<int64_t> mapping(circuit.num_inputs);
  for (int input_pos = 0; input_pos < circuit.num_inputs; ++input_pos) {
    for (int i = 0; i < circuit.num_inputs; ++i) {
      if (i == input_pos) {
        values[i] = 1;
      } else {
        values[i] = 0;
      }
    }
    for (const auto& [type, target, a, b] : circuit.gates) {
      values[target] = CombineGate2(type, values[a], values[b]);
    }

    // Fecth the output of f(1_i);
    int k = 0;
    int64_t out = 0;
    for (const int o : circuit.outputs) {
      out |= (values[o] & 1) << k++;
    }

    mapping[input_pos] = out;
    LOG(INFO) << input_pos << " -> " << std::bitset<20>(out);
  }

  // Does the circuit is sum of mapping[i] ??
  absl::BitGen random;
  for (int start = 0; start < num_samples; ++start) {
    for (int i = 0; i < circuit.num_inputs; ++i) {
      values[i] = absl::Uniform<uint64_t>(random);
    }
    for (const auto& [type, target, a, b] : circuit.gates) {
      values[target] = CombineGate2(type, values[a], values[b]);
    }

    // Reconstruct the 64 evaluation of g().
    for (uint64_t pos = 0; pos < 64; ++pos) {
      int64_t out_sum = 0;
      for (int i = 0; i < circuit.num_inputs; ++i) {
        if ((values[i] >> pos) & 1) {
          out_sum += mapping[i];
        }
      }
      out_sum &= (1 << circuit.outputs.size()) - 1;

      int k = 0;
      uint64_t out = 0;
      for (const int o : circuit.outputs) {
        out |= ((values[o] >> pos) & 1) << k++;
      }

      if (out != out_sum) {
        LOG(INFO) << "Not equal to simple sum, output differs: "
                  << std::bitset<20>(out) << " " << std::bitset<20>(out_sum);
        return false;
      }
    }
  }

  LOG(INFO) << "Circuit seems like simple sum on "
            << FormatCounter(64 * num_samples) << " samples";
  return true;
}

std::vector<std::pair<int, uint64_t>> SampleForAdditionCandidates(
    const BinaryCircuit& circuit, int num_samples) {
  // Starts with all nodes as candidate.
  // The second member of the pair will be set on the first iteration below.
  std::vector<std::pair<int, uint64_t>> candidates(circuit.num_vars);
  for (int i = 0; i < circuit.num_vars; ++i) {
    candidates[i] = {i, 0};
  }

  FixedCapacityVector<uint64_t> values;
  FixedCapacityVector<uint64_t> other_values;
  values.ClearAndReserve(circuit.num_vars);
  other_values.ClearAndReserve(circuit.num_vars);

  const uint64_t mask = (1 << circuit.outputs.size()) - 1;

  absl::BitGen random;
  for (int start = 0; start < num_samples; ++start) {
    for (int i = 0; i < circuit.num_inputs; ++i) {
      values[i] = absl::Uniform<uint64_t>(random);
    }
    for (const auto& [type, target, a, b] : circuit.gates) {
      values[target] = CombineGate2(type, values[a], values[b]);
    }

    uint64_t out[64];
    for (int pos = 0; pos < 64; ++pos) {
      out[pos] = 0;
      for (int k = 0; k < circuit.outputs.size(); ++k) {
        const int o = circuit.outputs[k];
        out[pos] |= ((values[o] >> pos) & 1) << k;
      }
    }

    int new_size = 0;
    int gate_start_index = 0;
    for (auto [node, term] : candidates) {
      // Evaluate as if node took the opposite value.
      other_values[node] = ~values[node];
      for (int i = gate_start_index; i < circuit.gates.size(); ++i) {
        const auto& [type, target, a, b] = circuit.gates[i];
        if (target <= node) {
          gate_start_index = i;  // Gates are in order.
          continue;
        }
        const uint64_t v_a = a < node ? values[a] : other_values[a];
        const uint64_t v_b = b < node ? values[b] : other_values[b];
        other_values[target] = CombineGate2(type, v_a, v_b);
      }

      bool ok = true;
      for (int pos = 0; pos < 64; ++pos) {
        uint64_t other_out = 0;
        for (int k = 0; k < circuit.outputs.size(); ++k) {
          const int o = circuit.outputs[k];
          other_out |= (((o < node ? values[o] : other_values[o]) >> pos) & 1)
                       << k;
        }

        const uint64_t candidate_term =
            (((values[node] >> pos) & 1) ? out[pos] - other_out
                                         : other_out - out[pos]) &
            mask;
        if (start == 0 && pos == 0) {
          term = candidate_term;
        } else if (term != candidate_term) {
          VLOG(2) << "removing " << node << " samples: " << start << " is "
                  << std::bitset<20>(candidate_term) << " was "
                  << std::bitset<20>(term);
          ok = false;
          break;
        }
      }

      if (ok) {
        candidates[new_size++] = {node, term};
      }
    }
    candidates.resize(new_size);
  }

  LOG(INFO) << "Candidates after sampling: " << candidates.size();
  for (const auto [node, term] : candidates) {
    VLOG(2) << node << " " << std::bitset<20>(term);
  }
  return candidates;
}

// == Adpated from Gemini  ===========================================

// Truth table bitmasks for 2-input binary gates
constexpr uint8_t kAnd = 0b1000;  // a AND b
constexpr uint8_t kXor = 0b0110;  // a XOR b
constexpr uint8_t kOr = 0b1110;   // a OR b

// Computes output = sum_{i=0}^{n-1} (input[i] * constants[i]) mod 2^m
// processing column by column (bits 0 to m-1) using 3-to-2 and 2-to-2
// compressor trees.
//
// Note(user): Apperently this is Dadda/Wallace addition.
BinaryCircuit BuildColumnWiseLinearCombinationCircuit(
    int m, absl::Span<const uint32_t> constants) {
  const int n = constants.size();

  BinaryCircuit circuit;
  circuit.num_inputs = n;
  circuit.num_vars = n;  // Inputs x_0 ... x_{n-1}

  if (n == 0 || m == 0) {
    circuit.ResetBooleanMapping();
    return circuit;
  }

  // Carries generated from column k that need to be added in column k+1
  std::vector<int> carries_in;
  std::vector<int> final_outputs(m);

  for (int k = 0; k < m; ++k) {
    // 1. Gather all bit-k contributions from the input terms (x_i *
    // constants[i])
    std::vector<int> current_column_bits;
    for (int i = 0; i < n; ++i) {
      if ((constants[i] >> k) & 1) {
        current_column_bits.push_back(i);  // x_i * 1 = x_i
      }
    }

    // 2. Add incoming carries from column k - 1
    current_column_bits.insert(current_column_bits.end(), carries_in.begin(),
                               carries_in.end());
    carries_in.clear();

    // 3. Compress current column down to a single output bit (final_outputs[k])
    //    using Full Adders (3->2) and Half Adders (2->2).
    while (current_column_bits.size() > 1) {
      std::vector<int> next_column_bits;

      size_t i = 0;
      // Use 3-to-2 Full Adders wherever 3 bits are available
      while (i + 2 < current_column_bits.size()) {
        int a = current_column_bits[i];
        int b = current_column_bits[i + 1];
        int c = current_column_bits[i + 2];

        // Full-adder sum: a ^ b ^ c
        int a_xor_b = circuit.AddGate(kXor, a, b);
        int sum = circuit.AddGate(kXor, a_xor_b, c);

        // Full-adder carry: (a & b) | (c & (a ^ b))
        int a_and_b = circuit.AddGate(kAnd, a, b);
        int c_and_xor = circuit.AddGate(kAnd, c, a_xor_b);
        int carry = circuit.AddGate(kOr, a_and_b, c_and_xor);

        next_column_bits.push_back(sum);
        carries_in.push_back(carry);  // Sent to column k + 1
        i += 3;
      }

      // Use a 2-to-2 Half Adder if 2 bits remain
      if (i + 1 < current_column_bits.size()) {
        int a = current_column_bits[i];
        int b = current_column_bits[i + 1];

        int sum = circuit.AddGate(kXor, a, b);
        int carry = circuit.AddGate(kAnd, a, b);

        next_column_bits.push_back(sum);
        carries_in.push_back(carry);  // Sent to column k + 1
        i += 2;
      }

      // Pass forward any leftover single bit
      if (i < current_column_bits.size()) {
        next_column_bits.push_back(current_column_bits[i]);
      }

      current_column_bits = std::move(next_column_bits);
    }

    // If the column has at least one bit, it becomes the output bit.
    // Otherwise, bit k is 0 (encoded as a constant gate 0b0000).
    if (!current_column_bits.empty()) {
      final_outputs[k] = current_column_bits[0];
    } else {
      final_outputs[k] = circuit.AddGate(0b0000, 0, 0);
    }
  }

  circuit.outputs = final_outputs;
  circuit.ResetBooleanMapping();
  return circuit;
}

// == Adapted from Gemini ======================================================
// Note(user): I asked for a different implementation of the n-way adder, which
// should be more robust to the input order, which is important for easy of
// verification.

// Helper to add two multi-bit integer vectors A and B.
std::vector<int> AddIntegers(BinaryCircuit& circuit, const std::vector<int>& A,
                             const std::vector<int>& B) {
  constexpr uint8_t kAnd = 0b1000;
  constexpr uint8_t kXor = 0b0110;
  constexpr uint8_t kOr = 0b1110;

  size_t len = std::max(A.size(), B.size());
  std::vector<int> sum;
  int carry = -1;

  for (size_t i = 0; i < len || carry != -1; ++i) {
    int a = (i < A.size()) ? A[i] : -1;
    int b = (i < B.size()) ? B[i] : -1;

    // Filter out missing inputs (e.g. if one vector is shorter)
    if (a == -1 && b == -1) {
      sum.push_back(carry);
      carry = -1;
      break;
    }
    if (a == -1) {
      a = b;
      b = -1;
    }

    if (b == -1) {
      if (carry == -1) {
        sum.push_back(a);
      } else {
        // Half-adder with carry
        sum.push_back(circuit.AddGate(kXor, a, carry));
        carry = circuit.AddGate(kAnd, a, carry);
      }
    } else {
      if (carry == -1) {
        sum.push_back(circuit.AddGate(kXor, a, b));
        carry = circuit.AddGate(kAnd, a, b);
      } else {
        // Full-adder
        int a_xor_b = circuit.AddGate(kXor, a, b);
        sum.push_back(circuit.AddGate(kXor, a_xor_b, carry));

        int a_and_b = circuit.AddGate(kAnd, a, b);
        int carry_and_xor = circuit.AddGate(kAnd, carry, a_xor_b);
        carry = circuit.AddGate(kOr, a_and_b, carry_and_xor);
      }
    }
  }

  return sum;
}

// Computes the total integer sum of a list of single-bit variables via a binary
// addition tree.
std::vector<int> TotalPopCount(BinaryCircuit& circuit,
                               const std::vector<int>& bits) {
  if (bits.empty()) return {};

  // Each bit is initially a 1-bit integer vector [bit]
  std::vector<std::vector<int>> current_terms;
  current_terms.reserve(bits.size());
  for (int b : bits) {
    current_terms.push_back({b});
  }

  // Reduce down to a single multi-bit integer
  while (current_terms.size() > 1) {
    std::vector<std::vector<int>> next_terms;
    next_terms.reserve((current_terms.size() + 1) / 2);

    for (size_t i = 0; i < current_terms.size(); i += 2) {
      if (i + 1 < current_terms.size()) {
        next_terms.push_back(
            AddIntegers(circuit, current_terms[i], current_terms[i + 1]));
      } else {
        next_terms.push_back(current_terms[i]);
      }
    }
    current_terms = std::move(next_terms);
  }

  return current_terms[0];
}

// Computes output = sum_{i=0}^{n-1} (input[i] * constants[i]) mod 2^m
// using explicit multi-bit integer popcounts per bit stage.
BinaryCircuit BuildPopcountCarryChainCircuit(
    int m, absl::Span<const uint32_t> constants) {
  const int n = constants.size();
  BinaryCircuit circuit;
  circuit.num_inputs = n;
  circuit.num_vars = circuit.num_inputs;

  if (n == 0 || m == 0) {
    circuit.ResetBooleanMapping();
    return circuit;
  }

  // Multi-bit integer carry vector from stage k - 1
  std::vector<int> carry_integer;
  std::vector<int> final_outputs(m);

  for (int k = 0; k < m; ++k) {
    // 1. Gather all input bits contributing to column k
    std::vector<int> bits_in_col;
    for (int i = 0; i < n; ++i) {
      if ((constants[i] >> k) & 1) {
        bits_in_col.push_back(i);  // x_i
      }
    }

    // 2. Total count of set bits in this column
    std::vector<int> col_sum = TotalPopCount(circuit, bits_in_col);

    // 3. Add the multi-bit integer carry from previous stage: S_k = col_sum +
    // carry_integer
    std::vector<int> total_sum = AddIntegers(circuit, col_sum, carry_integer);

    if (total_sum.empty()) {
      // If 0 bits, output is 0
      final_outputs[k] = circuit.AddGate(0b0000, 0, 0);
      carry_integer.clear();
    } else {
      // 4. Bit 0 of total_sum is output bit k
      final_outputs[k] = total_sum[0];

      // 5. Bits [1...end] form the multi-bit carry integer for stage k + 1
      carry_integer.assign(total_sum.begin() + 1, total_sum.end());
    }
  }

  circuit.outputs = final_outputs;
  circuit.ResetBooleanMapping();
  return circuit;
}

// == Adapted form Gemini, mainly for experiment on circuit efficiency =========

// Helper struct to represent a 2-operand Kogge-Stone prefix node
struct PrefixNode {
  int g;  // Generate bit
  int p;  // Propagate bit
};

// Adds two m-bit vectors A and B modulo 2^m using a Kogge-Stone Parallel Prefix
// Adder. Depth: O(log m)
std::vector<int> AddKoggeStone(BinaryCircuit& circuit,
                               const std::vector<int>& A,
                               const std::vector<int>& B, int m) {
  if (m == 0) return {};

  // 1. Initial Generate (g_i = a_i AND b_i) and Propagate (p_i = a_i XOR b_i)
  // signals
  std::vector<PrefixNode> current(m);
  for (int i = 0; i < m; ++i) {
    current[i].g = circuit.AddGate(kAnd, A[i], B[i]);
    current[i].p = circuit.AddGate(kXor, A[i], B[i]);
  }

  // 2. Prefix tree computation: O(log2 m) depth
  for (int stride = 1; stride < m; stride *= 2) {
    std::vector<PrefixNode> next = current;
    for (int i = stride; i < m; ++i) {
      // g_new = g_i OR (p_i AND g_{i-stride})
      int p_and_g = circuit.AddGate(kAnd, current[i].p, current[i - stride].g);
      next[i].g = circuit.AddGate(kOr, current[i].g, p_and_g);

      // p_new = p_i AND p_{i-stride}
      next[i].p = circuit.AddGate(kAnd, current[i].p, current[i - stride].p);
    }
    current = std::move(next);
  }

  // 3. Final Sum computation
  // sum_0 = p_0 = a_0 XOR b_0
  // sum_i = p_i XOR g_{i-1} (for i > 0)
  std::vector<int> sum(m);
  sum[0] = circuit.AddGate(kXor, A[0], B[0]);
  for (int i = 1; i < m; ++i) {
    // Initial p_i before prefix combining
    int initial_p_i = circuit.AddGate(kXor, A[i], B[i]);
    sum[i] = circuit.AddGate(kXor, initial_p_i, current[i - 1].g);
  }

  return sum;
}

// Computes output = sum_{i=0}^{n-1} (input[i] * constants[i]) mod 2^m
// using a Dadda Tree reduction followed by a Kogge-Stone adder.
// Total Depth: O(log n + log m)
BinaryCircuit BuildDaddaKoggeStoneCircuit(
    int m, absl::Span<const uint32_t> constants) {
  const int n = constants.size();
  BinaryCircuit circuit;
  circuit.num_inputs = n;
  circuit.num_vars = n;  // Inputs x_0 ... x_{n-1}

  if (n == 0 || m == 0) {
    circuit.ResetBooleanMapping();
    return circuit;
  }

  // 1. Group bits by column weight k (for k in [0, m-1])
  std::vector<std::vector<int>> columns(m);
  for (int i = 0; i < n; ++i) {
    for (int k = 0; k < m; ++k) {
      if ((constants[i] >> k) & 1) {
        columns[k].push_back(i);  // Contribution x_i
      }
    }
  }

  // 2. Calculate Dadda reduction sequence profile: 2, 3, 4, 6, 9, 13, 19, 28,
  // ...
  std::vector<int> dadda_profile = {2};
  while (dadda_profile.back() < n) {
    dadda_profile.push_back(dadda_profile.back() * 3 / 2);
  }

  // 3. Perform parallel Dadda Tree reduction down to at most 2 bits per column
  for (int p = static_cast<int>(dadda_profile.size()) - 1; p >= 0; --p) {
    int target_height = dadda_profile[p];

    for (int k = 0; k < m; ++k) {
      while (columns[k].size() > target_height) {
        if (columns[k].size() - target_height >= 2) {
          // Use a Full Adder (3-to-2 compressor)
          int a = columns[k][0];
          int b = columns[k][1];
          int c = columns[k][2];
          columns[k].erase(columns[k].begin(), columns[k].begin() + 3);

          // Full-adder sum stays in column k
          int a_xor_b = circuit.AddGate(kXor, a, b);
          int sum = circuit.AddGate(kXor, a_xor_b, c);
          columns[k].push_back(sum);

          // Full-adder carry goes to column k+1 (if within mod 2^m bounds)
          int a_and_b = circuit.AddGate(kAnd, a, b);
          int c_and_xor = circuit.AddGate(kAnd, c, a_xor_b);
          int carry = circuit.AddGate(kOr, a_and_b, c_and_xor);

          if (k + 1 < m) {
            columns[k + 1].push_back(carry);
          }
        } else if (columns[k].size() - target_height == 1) {
          // Use a Half Adder (2-to-2 compressor)
          int a = columns[k][0];
          int b = columns[k][1];
          columns[k].erase(columns[k].begin(), columns[k].begin() + 2);

          int sum = circuit.AddGate(kXor, a, b);
          int carry = circuit.AddGate(kAnd, a, b);

          columns[k].push_back(sum);
          if (k + 1 < m) {
            columns[k + 1].push_back(carry);
          }
        }
      }
    }
  }

  // 4. Extract Vector A and Vector B (at most 2 bits per column)
  std::vector<int> A(m);
  std::vector<int> B(m);
  int zero_var = -1;

  for (int k = 0; k < m; ++k) {
    if (!columns[k].empty()) {
      A[k] = columns[k][0];
    } else {
      if (zero_var == -1) zero_var = circuit.AddConstant(false);
      A[k] = zero_var;
    }

    if (columns[k].size() >= 2) {
      B[k] = columns[k][1];
    } else {
      if (zero_var == -1) zero_var = circuit.AddConstant(false);
      B[k] = zero_var;
    }
  }

  // 5. Compute final sum using Kogge-Stone Parallel Prefix Adder
  circuit.outputs = AddKoggeStone(circuit, A, B, m);
  circuit.ResetBooleanMapping();
  return circuit;
}

// =======================================================

AdditionDecompositionResult ValidateAdditionCandidates(
    absl::Span<const std::pair<int, uint64_t>> candidates,
    const BinaryCircuit& circuit,
    const std::function<CpSolverResponse(const CpModelProto& cp_model)>&
        solve) {
  const int m = circuit.outputs.size();
  const BinaryCircuit adder = MakeNBitAdder(m);
  LOG(INFO) << "Initial circuit " << circuit.DebugString();

  // Lets skip the outputs, it is harder to reason about otherwise.
  // And also I believe our model the verify that indeed one the output bit
  // is in linear dependence with the output is broken, so we don't
  // mark them as such.
  std::vector<bool> is_output(circuit.num_vars, false);
  for (const int o : circuit.outputs) is_output[o] = true;

  int index = 0;
  BinaryCircuit current = circuit;
  current.ResetBooleanMapping();
  std::vector<std::pair<int, uint64_t>> validated;
  for (const auto& [node, term] : candidates) {
    if (is_output[node]) continue;
    BinaryCircuit next = current;

    // Lets make all gates using "node" take zero as input, and propagate
    // constants.
    bool skip = false;
    std::vector<bool> value_is_zero(next.num_vars, false);
    std::vector<bool> value_is_one(next.num_vars, false);
    value_is_zero[node] = true;
    for (auto& [type, target, a, b] : next.gates) {
      if (target == node && (type == 0b0000 || type == 0b1111)) {
        skip = true;
        break;
      }
      if (value_is_zero[a]) a = 0, type = (type & 1) * 3 + (type & 4) * 3;
      if (value_is_one[a])
        a = 0, type = ((type & 2) >> 1) * 3 + ((type & 8) >> 1) * 3;
      if (value_is_zero[b]) b = 0, type = (type & 3) + (type & 3) * 4;
      if (value_is_one[b]) b = 0, type = ((type >> 2) & 3) * 5;
      if (type == 0b0000) value_is_zero[target] = true;
      if (type == 0b1111) value_is_one[target] = true;
    }
    if (skip) {
      VLOG(2) << index++ << "/" << candidates.size() << ": " << node << " "
              << std::bitset<20>(term)
              << "At zero or one, skipping without changes";
      continue;
    }

    // Once we show that current = simplified + node * term, we can try to
    // simplify "simplified" next !
    // Note that since we process gate in topo order, the value of node
    // will never change again in our "simplified" circuit.
    BinaryCircuit simplified = next;

    // Now create the node * "term" inputs.
    std::vector<int> adder_input;
    adder_input = next.outputs;
    adder_input.resize(m);
    for (int k = 0; k < m; ++k) {
      if ((term >> k) & 1) {
        adder_input.push_back(next.AddCopyOf(node));
      } else {
        adder_input.push_back(next.AddConstant(false));
      }
    }

    // Add an adder which is the new output.
    next.outputs = AppendCircuit(adder_input, adder, &next);

    // copy the other outputs afterwards.
    next.outputs.insert(next.outputs.end(),
                        absl::MakeSpan(current.outputs).subspan(m).begin(),
                        absl::MakeSpan(current.outputs).subspan(m).end());

    next.ResetBooleanMapping();
    VLOG(1) << next.DebugString();

    // Now construct and check equivalence via mitter.
    const BinaryCircuit mitter = ConstructMitter(next, current);
    const CpModelProto proto =
        ConstructCpModelFromBinaryCircuit(mitter, /*enforce_one_output=*/true);
    const CpSolverResponse response = solve(proto);

    if (response.status() == CpSolverStatus::INFEASIBLE) {
      validated.push_back({node, term});
      // current = next;
      current = simplified;
      LOG(INFO) << "Validated    " << ++index << "/" << candidates.size()
                << ": " << node << " " << std::bitset<20>(term) << " "
                << response.wall_time() << "s";
    } else {
      LOG(INFO) << "Wrong sample " << ++index << "/" << candidates.size()
                << ": " << node << " " << std::bitset<20>(term) << " "
                << response.wall_time() << "s";
    }
  }

  // Because we process them in order, these should not be fixed.
  LOG(INFO) << "Final stats: " << validated.size();

  // Lets construct a final circuit that should be equivalent to the first one
  // by construction (but it will be hard to prove directly).
  BinaryCircuit final = current;
  AdditionDecompositionResult result;

  // Deal with the constant bit of the outputs.
  uint64_t constant_out = 0;
  {
    std::vector<bool> fixed_at_0(current.num_vars, false);
    std::vector<bool> fixed_at_1(current.num_vars, false);
    for (const auto& [type, target, a, b] : current.gates) {
      if (type == 0b0000) fixed_at_0[target] = true;
      if (type == 0b1111) fixed_at_1[target] = true;
    }
    for (int k = 0; k < current.outputs.size(); ++k) {
      const int o = current.outputs[k];
      if (fixed_at_0[o]) {
      } else if (fixed_at_1[o]) {
        constant_out |= (uint64_t{1} << k);
      } else {
        LOG(INFO) << "output " << k << " not constant.";
        validated.push_back({o, 1 << k});
      }
    }
  }
  LOG(INFO) << "Constant out: " << std::bitset<20>(constant_out);

  // Preprocess to simplify and canonicalize the adder.
  uint64_t offset = constant_out;
  const uint64_t mask = m == 64 ? ~uint64_t{0} : (uint64_t{1} << m) - 1;
  for (auto& [node, term] : validated) {
    const uint64_t negated = -term & mask;
    if (absl::popcount(negated) < absl::popcount(term)) {
      offset += term;
      term = negated;
      node = final.AddNegationOf(node);
    }
  }
  offset &= mask;
  if (offset != 0) {
    // Add as constant.
    //
    // TODO(user): that might mess up a bit the "tree" in our adder. Can we
    // integrate such constant term more efficiently ?
    LOG(INFO) << "offset: " << std::bitset<20>(offset);
    validated.push_back({final.AddConstant(true), offset});
  }
  result.reduced_circuit = final;

  // To simplify further equivalence checking as much as possible, we want
  // to sort the outputs by their dependency on the input.
  std::vector<std::bitset<500>> input_dependency(final.num_vars);
  for (int i = 0; i < final.num_inputs; ++i) {
    input_dependency[i].set(i);
  }
  for (const auto& [type, target, a, b] : final.gates) {
    if (type == 0b0000) continue;
    if (type == 0b1111) continue;
    if (type == 0b1010 || type == 0b0101) {
      input_dependency[target] = input_dependency[a];
      continue;
    }
    if (type == 0b0011 || type == 0b1100) {
      input_dependency[target] = input_dependency[b];
      continue;
    }
    input_dependency[target] = input_dependency[a] | input_dependency[b];
  }

  // This should improve stability across circuit, and the adder input
  // that are the same should hopefully be consumed in the same way.
  absl::c_stable_sort(validated,
                      [&input_dependency](const std::pair<int, uint64_t>& a,
                                          const std::pair<int, uint64_t>& b) {
                        if (a.second == b.second)
                          return input_dependency[a.first].to_string() <
                                 input_dependency[b.first].to_string();
                        return a.second < b.second;
                      });

  // Lets display some summary
  absl::btree_map<uint64_t, int> count_map;

  std::vector<int> input_map;
  std::vector<uint32_t> constants;
  input_map.reserve(validated.size());
  constants.reserve(validated.size());
  for (const auto [node, term] : validated) {
    input_map.push_back(node);
    constants.push_back(term);
    count_map[term]++;
  }
  for (const auto [term, count] : count_map) {
    LOG(INFO) << std::bitset<20>(term) << ": " << count;
  }

  // Here we prefer a "nway" encoding that is less sensible to input order.
  const BinaryCircuit nway_adder = BuildPopcountCarryChainCircuit(m, constants);
  LOG(INFO) << "nway_adder " << nway_adder.DebugString();
  {
    // For info. Samller circuit, but harder ot verify.
    LOG(INFO)
        << "Basic version "
        << BuildColumnWiseLinearCombinationCircuit(m, constants).DebugString();
    LOG(INFO) << "Dada version "
              << BuildDaddaKoggeStoneCircuit(m, constants).DebugString();
  }

  final.outputs = AppendCircuit(input_map, nway_adder, &final);
  final.ResetBooleanMapping();
  LOG(INFO) << final.DebugString();

  result.final_circuit = final;
  result.input_term_pairs = validated;
  return result;
}

// Generates an n-bit adder circuit that computes output = (A + B) mod (2^n).
// Input layout:  A = [0, n), B = [n, 2*n)
// Output layout: Sum bits [S_0, S_1, ..., S_{n-1}]
BinaryCircuit MakeNBitAdder(int n) {
  BinaryCircuit circuit;
  if (n <= 0) return circuit;

  circuit.num_inputs = 2 * n;
  circuit.num_vars = circuit.num_inputs;

  // We will maintain the carry bit across bit positions.
  int carry = -1;

  for (int i = 0; i < n; ++i) {
    int a_i = i;      // Bit i of input A
    int b_i = n + i;  // Bit i of input B

    if (i == 0) {
      // Half-adder for bit 0
      // Sum bit: S_0 = A_0 ^ B_0 (XOR -> 0b0110)
      const int sum_i = circuit.AddGate(0b0110, a_i, b_i);
      circuit.outputs.push_back(sum_i);

      // Carry bit: C_0 = A_0 & B_0 (AND -> 0b1000)
      carry = circuit.AddGate(0b1000, a_i, b_i);
    } else {
      // Full-adder for bit i > 0
      // 1. xor_ab = A_i ^ B_i
      const int xor_ab = circuit.AddGate(0b0110, a_i, b_i);

      // 2. Sum bit: S_i = xor_ab ^ C_{i-1}
      const int sum_i = circuit.AddGate(0b0110, xor_ab, carry);
      circuit.outputs.push_back(sum_i);

      // 3. and_ab = A_i & B_i
      const int and_ab = circuit.AddGate(0b1000, a_i, b_i);

      // 4. and_carry = xor_ab & C_{i-1}
      const int and_carry = circuit.AddGate(0b1000, xor_ab, carry);

      // 5. Next Carry: C_i = and_ab | and_carry (OR -> 0b1110)
      carry = circuit.AddGate(0b1110, and_ab, and_carry);
    }
  }

  circuit.ResetBooleanMapping();
  return circuit;
}

std::vector<int> AppendCircuit(absl::Span<const int> input_map,
                               const BinaryCircuit& circuit,
                               BinaryCircuit* result) {
  CHECK_EQ(input_map.size(), circuit.num_inputs);

  const int n = circuit.num_inputs;
  const int new_start = result->num_vars - n;
  const auto remap = [n, new_start, input_map](int index) {
    if (index < n) return input_map[index];
    return new_start + index;
  };
  for (const BinaryGate& gate : circuit.gates) {
    const int a = remap(gate.a);
    const int b = remap(gate.b);
    const int t = remap(gate.target);
    CHECK_EQ(t, result->num_vars++);

    // -1 means input is always zero.
    int type = gate.type;
    if (a == -1) type = (type & 1) * 3 + (type & 4) * 3;
    if (b == -1) type = (type & 3) + (type & 3) * 4;

    result->gates.emplace_back(type, t, (a < 0 ? 0 : a), (b < 0 ? 0 : b));
  }
  std::vector<int> outputs;
  outputs.reserve(circuit.outputs.size());
  for (const int o : circuit.outputs) {
    outputs.push_back(remap(o));
  }
  return outputs;
}

std::vector<BinaryCircuit> GetNWayAdditionSubmodels(
    const BinaryCircuit& circuit) {
  std::vector<BinaryCircuit> result;
  result.reserve(circuit.num_inputs);

  const BinaryCircuit adder = MakeNBitAdder(circuit.outputs.size());

  std::vector<int> input_map;
  const int n = circuit.num_inputs;
  for (int number = 0; number < circuit.num_inputs; ++number) {
    BinaryCircuit local_mitter;
    local_mitter.num_inputs = n;
    local_mitter.num_vars = local_mitter.num_inputs;

    // Evaluate f(0, a_i, b).
    input_map.assign(n, -1);
    for (int i = number; i < input_map.size(); ++i) {
      input_map[i] = i;
    }
    const std::vector<int> outputs_full =
        AppendCircuit(input_map, circuit, &local_mitter);
    CHECK_EQ(outputs_full.size(), circuit.outputs.size());

    // Evaluate f(0, a_i, 0).
    input_map.assign(n, -1);
    input_map[number] = number;
    const std::vector<int> outputs_single =
        AppendCircuit(input_map, circuit, &local_mitter);
    CHECK_EQ(outputs_single.size(), circuit.outputs.size());

    // Evaluate f(0, 0, b).
    input_map.assign(n, -1);
    for (int i = number + 1; i < input_map.size(); ++i) {
      input_map[i] = i;
    }
    const std::vector<int> outputs_suffix =
        AppendCircuit(input_map, circuit, &local_mitter);
    CHECK_EQ(outputs_suffix.size(), circuit.outputs.size());

    // Add outputs_single with outputs_suffix.
    input_map.assign(2 * circuit.outputs.size(), -1);
    for (int i = 0; i < input_map.size(); ++i) {
      input_map[i] = i < outputs_single.size()
                         ? outputs_single[i]
                         : outputs_suffix[i - outputs_single.size()];
    }
    const std::vector<int> outputs_adder =
        AppendCircuit(input_map, adder, &local_mitter);
    CHECK_EQ(outputs_adder.size(), circuit.outputs.size());

    // The new output is output_full != outputs_adder.
    for (int i = 0; i < circuit.outputs.size(); ++i) {
      local_mitter.gates.emplace_back(0b0110, local_mitter.num_vars,
                                      outputs_full[i], outputs_adder[i]);
      local_mitter.outputs.push_back(local_mitter.num_vars);
      ++local_mitter.num_vars;
    }

    local_mitter.ResetBooleanMapping();
    result.push_back(local_mitter);
  }

  return result;
}

// TODO(user): If one call proved all potential equivalences, we can stop.
// TODO(user): congruence closure is faster... resuse sat code somehow?
std::vector<std::pair<Literal, Literal>> SimplifyCircuit(
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
  for (const absl::Span<const Literal> literals : equiv) {
    for (int k = 1; k < literals.size(); ++k) {
      const Literal a = literals[0];
      const Literal b = literals[k];

      CHECK_LT(a.Index(), b.Index());
      BinaryCircuit lmp = extractor.Extract({a, b});

      if (lmp.num_inputs <= 20) {
        AddNotEquivalentConstraint(a, b, &lmp);
        const bool are_equivalent = !BinaryCircuitIsFeasible(lmp);
        VLOG(2) << lmp.DebugString() << " equiv: " << are_equivalent << " " << a
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
    if (solve != nullptr && ++num_tried <= max_num_solves) {
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
      } else {
        if (VLOG_IS_ON(2)) {
          // Dump info for investigation.
          const std::string dot_filename =
              absl::StrCat("/tmp/dot_unclear_", i, ".dot");
          VLOG(2) << "Dumping to '" << dot_filename << "'";
          CHECK_OK(file::SetContents(dot_filename, ToDotFile(lmp),
                                     file::Defaults()));

          std::string filename =
              absl::StrCat("/tmp/submodel_unclear_", i, ".pb.txt");
          VLOG(2) << " Dumping equiv checking submodel to '" << filename << "'";
          CHECK(WriteModelProtoToFile(local_cp_model, filename));
        }

        // Lets disable sat subsolve as soon as we can't solve one.
        num_tried = max_num_solves;
      }

      VLOG(2) << i + 1 << "/" << complexity.size() << " " << lmp.DebugString()
              << " equiv: " << proven_equiv << " (with solver) "
              << complexity[i].a << " " << complexity[i].b;
      if (proven_equiv) {
        new_equiv.push_back({complexity[i].a, complexity[i].b});
      }
      continue;
    }

    if (++num_displayed <= 5 || i + 5 >= complexity.size()) {
      if (i + 5 == complexity.size()) {
        VLOG(2) << "...";
      } else {
        VLOG(2) << "vars " << complexity[i].num_vars << " inputs "
                << complexity[i].num_inputs;
      }
    }
  }

  VLOG(3) << "NEW equivalences" << new_equiv.size();
  RemoveEquivalences(new_equiv, circuit);
  ReduceGates(circuit);
  return new_equiv;
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

  // Remap outputs that are equal to their representative.
  int num_negated_output = 0;
  std::vector<int> negation_of(circuit->num_vars, -1);
  for (int& out_ref : circuit->outputs) {
    if (representative[out_ref] == kNoLiteralIndex) continue;
    const Literal lit(representative[out_ref]);
    const int var = circuit->mapping[lit.Variable()];
    if (lit.IsPositive()) {
      out_ref = var;
    } else {
      if (negation_of[var] == -1) {
        circuit->reverse_mapping.push_back(circuit->reverse_mapping[out_ref]);

        // Lets create a new gate to at least directly depend on the
        // representative.
        ++num_negated_output;
        BinaryGate gate;
        gate.type = 0b0001;
        gate.target = circuit->num_vars++;
        gate.a = gate.b = var;
        negation_of[var] = gate.target;
        circuit->gates.push_back(gate);
      }
      out_ref = negation_of[var];
    }
  }

  if (num_negated_output > 0) {
    VLOG(2) << "Warning: " << num_negated_output
            << " unary gate still needed for negated output";
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
