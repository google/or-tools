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

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/options.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/gate_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/filelineiter.h"

ABSL_FLAG(std::string, circuit1, "/tmp/circuit1.bench",
          "Circuit A in bench format with only LUT of size 2 and sorted "
          "topologically.");

ABSL_FLAG(std::string, circuit2, "/tmp/circuit2.bench",
          "Circuit B in bench format with only LUT of size 2 and sorted "
          "topologically.");

namespace operations_research::sat {

// Basic .bench parser, supporting only LUT 2 appearing in topological order.
// This is the format that we uses in our ToBenchFile() and that ABC uses on the
// write_bench command
//
// TODO(user): This is a quick and not really robust parser. Improve as needed.
BinaryCircuit FromBenchFile(absl::string_view file) {
  absl::flat_hash_map<std::string, int> string_to_index;

  const auto get_index = [&string_to_index](std::string name) {
    return string_to_index.at(name);
  };

  const auto create_index = [&string_to_index](std::string name) {
    const int index = string_to_index.size();
    auto [it, inserted] = string_to_index.try_emplace(name, index);
    CHECK(inserted) << "Key already exists: " << name;
    return index;
  };

  BinaryCircuit circuit;
  std::vector<std::string> output_ids;
  for (std::string line : FileLines(file)) {
    // Remove all spaces and handle comments.
    line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
    if (line.empty() || line[0] == '#') continue;

    const std::string start_input = "INPUT(";
    const std::string start_output = "OUTPUT(";
    if (line.find(start_input) == 0) {
      const size_t first = line.find(start_input) + start_input.length();
      const size_t last = line.find(')', first);
      create_index(std::string(line.substr(first, last - first)));
      ++circuit.num_inputs;
    } else if (line.find(start_output) == 0) {
      const size_t first = line.find(start_output) + start_output.length();
      const size_t last = line.find(')', first);
      output_ids.push_back(line.substr(first, last - first));
    } else if (absl::StrContains(line, "LUT")) {
      // Format: Node = LUT 0xH ( In1, In2 )
      size_t eqPos = line.find('=');
      size_t lutPos = line.find("LUT");
      size_t openParen = line.find('(');
      size_t comma = line.find(',');
      size_t closeParen = line.find(')');

      BinaryGate gate;
      gate.target = create_index(line.substr(0, eqPos));

      // Hex to Int conversion for truth table
      std::string hexStr = line.substr(lutPos + 3, openParen - (lutPos + 3));
      gate.type = std::stoi(hexStr, nullptr, 16);

      // Both index should already exist.
      gate.a = get_index(line.substr(openParen + 1, comma - openParen - 1));
      gate.b = get_index(line.substr(comma + 1, closeParen - comma - 1));

      circuit.gates.push_back(gate);
    }
  }

  // All output indices should already exist.
  circuit.outputs.reserve(output_ids.size());
  for (const std::string& id : output_ids) {
    circuit.outputs.push_back(get_index(id));
  }

  circuit.num_vars = string_to_index.size();
  circuit.ResetBooleanMapping();
  return circuit;
}

void FixSomeInputs(BinaryCircuit& circuit) {
  std::vector<int> out_degree(circuit.num_vars, 0);
  for (const BinaryGate& gate : circuit.gates) {
    out_degree[gate.a]++;
    out_degree[gate.b]++;
  }

  // TEST. Usually the first input has a really high degree.
  // I think this is the "mode" for the circuit we are testing.
  if (out_degree[0] > 100) {
    RemoveEquivalences({}, &circuit, {Literal(BooleanVariable(0), true)});
  }
}

void OptimizeCircuit(std::string name, BinaryCircuit& circuit) {
  const BinaryCircuit initial_circuit = circuit;

  LOG(INFO) << "===== " << name << " ===================";
  LOG(INFO) << "full " << circuit.DebugString();

  RemoveEquivalences({}, &circuit);
  LOG(INFO) << "equiv " << circuit.DebugString();

  ReduceGates(&circuit);
  LOG(INFO) << "reduce " << circuit.DebugString();

  RemoveEquivalences({}, &circuit);
  LOG(INFO) << "equiv " << circuit.DebugString();

  ReduceGates(&circuit);
  LOG(INFO) << "reduce " << circuit.DebugString();

  ReduceGates(&circuit);
  LOG(INFO) << "CHECK " << circuit.DebugString();

  const auto cp_sat_solve = [](const CpModelProto proto) {
    SatParameters params;
    params.set_log_search_progress(false);
    params.set_log_to_stdout(false);
    params.set_catch_sigint_signal(false);
    params.set_linearization_level(0);
    params.set_cp_model_probing_level(0);
    params.set_max_time_in_seconds(2);
    params.set_use_sat_inprocessing(false);
    params.set_cp_model_presolve(false);
    return SolveWithParameters(proto, params);
  };

  // Just to give an idea.
  {
    absl::BitGen random;
    std::vector<std::vector<BooleanVariable>> solutions;
    const int num_sweep_passes = 1;  // 3;
    const int num_sat_solves = 0;    // 50;
    for (int i = 0; i < num_sweep_passes; ++i) {
      SimplifyCircuit(num_sat_solves, random, cp_sat_solve, &solutions,
                      &circuit);
      LOG(INFO) << "SIMPLIFIED " << circuit.DebugString();
    }
    SampleForEquivalences(circuit, random, {});
  }

  {
    std::string filename = absl::StrCat("/tmp/optim_bench_", name, ".bench");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK_OK(
        file::SetContents(filename, ToBenchFile(circuit), file::Defaults()));
  }

  // For investigation.
  {
    std::string filename = absl::StrCat("/tmp/dot_", name, ".dot");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK_OK(file::SetContents(filename, ToDotFile(circuit), file::Defaults()));
  }
  {
    std::string filename = absl::StrCat("/tmp/model_", name, ".pb.txt");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK(WriteModelProtoToFile(ConstructCpModelFromBinaryCircuit(circuit),
                                filename));
  }

  {
    std::string filename = absl::StrCat("/tmp/model_", name, "_ands.pb.txt");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK(WriteModelProtoToFile(CpModelUsingLargeAnds(circuit), filename));
  }

  {
    // Debug check that initial_version is the same as circuit.
    BinaryCircuit mitter = ConstructMitter(initial_circuit, circuit);
    std::string filename = absl::StrCat("/tmp/debug_model_", name, ".pb.txt");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK(WriteModelProtoToFile(ConstructCpModelFromBinaryCircuit(
                                    mitter, /* enforce_one_output= */ true),
                                filename));
  }
}

void Run(std::string filename1, std::string filename2) {
  BinaryCircuit circuit1 = FromBenchFile(filename1);
  FixSomeInputs(circuit1);
  OptimizeCircuit("circuit1", circuit1);

  BinaryCircuit circuit2 = FromBenchFile(filename2);
  FixSomeInputs(circuit2);
  OptimizeCircuit("circuit2", circuit2);

  BinaryCircuit mitter = ConstructMitter(circuit1, circuit2);

  {
    // Lets display circuit1 with special color for the node that seems
    // equivalent.
    absl::BitGen random;
    CompactVectorVector<int, Literal> equiv =
        SampleForEquivalences(mitter, random, {});
    std::vector<int> special_nodes;
    for (int i = 0; i < equiv.size(); ++i) {
      for (const Literal lit : equiv[i]) {
        const int node = mitter.mapping[lit.Variable()];
        if (node < circuit1.num_vars) {
          special_nodes.push_back(node);
        }
      }
    }
    LOG(INFO) << "Seems equiv: " << special_nodes.size() << " / "
              << circuit1.num_vars;
    std::string filename = absl::StrCat("/tmp/special.dot");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK(file::SetContents(filename, ToDotFile(circuit1, special_nodes),
                            file::Defaults())
              .ok());
  }

  OptimizeCircuit("mitter", mitter);

  // This is the mitter with the constraint that at least one output must be
  // different. Proving infeasibility means proving equivalence.
  {
    std::string filename = absl::StrCat("/tmp/mitter.pb.txt");
    LOG(INFO) << "Dumping to '" << filename << "'";
    CHECK(WriteModelProtoToFile(
        ConstructCpModelFromBinaryCircuit(mitter, /*enforce_one_output*/ true),
        filename));
  }
}

}  // namespace operations_research::sat

int main(int argc, char** argv) {
  InitGoogle("Create a mitter from two circuit in bench format.", &argc, &argv,
             /*remove_flags=*/true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::Run(absl::GetFlag(FLAGS_circuit1),
                                absl::GetFlag(FLAGS_circuit2));
}
