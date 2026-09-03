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
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/options.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/gate_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/filelineiter.h"

ABSL_FLAG(std::string, circuit1, "/tmp/circuit1.bench",
          "Circuit A in bench format with only LUT of size 2 and sorted "
          "topologically.");

ABSL_FLAG(std::string, circuit2, "/tmp/circuit2.bench",
          "Circuit B in bench format with only LUT of size 2 and sorted "
          "topologically.");

ABSL_FLAG(bool, decompose, false,
          "Just decompose the circuit assuming the output is (normal_out, "
          "debug_out)");
ABSL_FLAG(int, decompose_num_initial_outputs, 20, "Size of initial output");

ABSL_FLAG(std::string, nway_adder, "",
          "If non-empty, try to show that the given circuit is equivalent to a "
          "n-way adder");

ABSL_FLAG(std::string, recover_nway_adder_inputs, "",
          "If non-empty, try to recover the nodes that can be converted to and "
          "addition on the output.");

ABSL_FLAG(std::string, dump_prefix, "", "Add as prefix of some dump filename");

ABSL_FLAG(
    bool, count_common_outputs, false,
    "If true, display how many output are equivalent between two circuit");

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

bool ModelIsInfeasible(const CpModelProto& proto) {
  LOG(INFO) << "Verifying equivalences with CP-SAT ...";
  SatParameters params;
  params.set_num_workers(32);
  params.set_num_full_subsolvers(32);
  params.add_subsolvers("no_lp");
  params.set_inprocessing_dtime_ratio(0.4);
  params.set_shared_tree_num_workers(0);
  params.set_log_search_progress(false);
  const CpSolverResponse response = SolveWithParameters(proto, params);
  return response.status() == CpSolverStatus::INFEASIBLE;
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

    const CpModelProto proto =
        ConstructCpModelFromBinaryCircuit(mitter, /*enforce_one_output*/ true);

    CHECK(WriteModelProtoToFile(proto, filename));
    CHECK(ModelIsInfeasible(proto));
  }
}

// From a base circuit (in) -> (out1, out2)
// We extract a few subcircuits:
struct CircuitDecomposition {
  BinaryCircuit goal;             // (in) -> (out1)
  BinaryCircuit hard;             // (in) -> (out2);
  BinaryCircuit simplified_goal;  // (in, out2) -> (out1)
};
CircuitDecomposition SimplerDecomposition(int num_original_outputs,
                                          absl::string_view name,
                                          BinaryCircuit& circuit) {
  circuit.ResetBooleanMapping();
  LOG(INFO) << "DECOMPOSING " << circuit.DebugString();

  // Split output in 2.
  const absl::Span<const int> all_outputs = circuit.outputs;
  const absl::Span<const int> out1 =
      all_outputs.subspan(0, num_original_outputs);
  const absl::Span<const int> out2 = all_outputs.subspan(num_original_outputs);
  LOG(INFO) << "SIZES " << out1.size() << " " << out2.size();

  // Extract subcircuits.
  CircuitDecomposition result;

  // Goal.
  {
    SubcircuitExtractor extractor(circuit);
    result.goal = extractor.Extract(out1);
    OptimizeCircuit(absl::StrCat("goal", name), result.goal);
  }

  // Hard.
  {
    SubcircuitExtractor extractor(circuit);
    result.hard = extractor.Extract(out2);
    OptimizeCircuit(absl::StrCat("hard", name), result.hard);
  }

  // Simplified goal.
  {
    BinaryCircuit temp = ConvertInnerNodeToInputs(circuit, out2);
    SubcircuitExtractor extractor(temp);
    result.simplified_goal = extractor.Extract(
        absl::MakeSpan(temp.outputs).subspan(0, num_original_outputs));
    OptimizeCircuit(absl::StrCat("simplied_goal", name),
                    result.simplified_goal);
  }

  return result;
}

void DumpMitter(absl::string_view name, BinaryCircuit a, BinaryCircuit b,
                bool verify = false) {
  a.ResetBooleanMapping();
  b.ResetBooleanMapping();
  BinaryCircuit mitter = ConstructMitter(a, b);
  std::string filename = absl::StrCat("/tmp/mitter_", name, ".pb.txt");
  LOG(INFO) << "Dumping to '" << filename << "'";

  const CpModelProto proto =
      ConstructCpModelFromBinaryCircuit(mitter, /* enforce_one_output= */ true);
  CHECK(WriteModelProtoToFile(proto, filename));
  if (verify) CHECK(ModelIsInfeasible(proto));
}

void IsNWayAdder(const BinaryCircuit& circuit) {
  {
    // Simple test that our gemini MakeNBitAdder() is correct.
    const BinaryCircuit adder = MakeNBitAdder(14);
    CHECK(RecoverNWayAddition(adder));
  }

  if (RecoverNWayAddition(circuit)) {
    // TODO(user): Find a faster way to prove this ?
    // Note that it is just a few seconds per model, so relatively quick.
    std::vector<BinaryCircuit> models = GetNWayAdditionSubmodels(circuit);
    for (int i = 0; i < models.size(); ++i) {
      const CpModelProto proto = ConstructCpModelFromBinaryCircuit(
          models[i], /*enforce_one_output*/ true);

      std::string filename = absl::StrCat("/tmp/adder_", i, ".pb.txt");
      LOG(INFO) << "Dumping to '" << filename << "'";
      CHECK(WriteModelProtoToFile(proto, filename));

      CHECK(ModelIsInfeasible(proto))
          << "Failed to prove equivalence to N-way adder !";
    }

    LOG(INFO)
        << "The given circuit was proven to be equivalent to a N-way adder !";
    return;
  }

  LOG(INFO) << "Failed to prove equivalence to N-way adder !";
}

void Decompose(absl::string_view name, int m, const BinaryCircuit& circuit) {
  // See if we have an issue, also reconstruct g().
  if (!SampleDecomposition(m, circuit)) return;

  const BinaryCircuit decompo = ConstructDecomposition(m, circuit);
  LOG(INFO) << "Decompo " << m << ": " << decompo.DebugString();

  std::string filename = absl::StrCat("/tmp/decompo_", name, "_", m, ".pb.txt");
  LOG(INFO) << "Dumping to '" << filename << "'";
  CHECK(WriteModelProtoToFile(
      ConstructCpModelFromBinaryCircuit(decompo, /*enforce_one_output*/ true),
      filename));
}

void Run(std::string filename1, std::string filename2) {
  if (!absl::GetFlag(FLAGS_nway_adder).empty()) {
    IsNWayAdder(FromBenchFile(absl::GetFlag(FLAGS_nway_adder)));
    return;
  }

  if (!absl::GetFlag(FLAGS_recover_nway_adder_inputs).empty()) {
    const BinaryCircuit circuit =
        FromBenchFile(absl::GetFlag(FLAGS_recover_nway_adder_inputs));
    LOG(INFO) << circuit.DebugString();
    const auto candidates = SampleForAdditionCandidates(circuit);

    const auto cp_sat_solve = [](const CpModelProto& proto) {
      SatParameters params;
      params.set_log_search_progress(false);
      params.set_catch_sigint_signal(false);
      params.set_num_workers(16);
      params.set_num_full_subsolvers(16);
      params.set_inprocessing_dtime_ratio(0.5);
      params.add_subsolvers("no_lp");
      return SolveWithParameters(proto, params);
    };

    std::string prefix = absl::GetFlag(FLAGS_dump_prefix);
    if (!prefix.empty()) absl::StrAppend(&prefix, "_");

    auto result = ValidateAdditionCandidates(candidates, circuit, cp_sat_solve);
    {
      // This can be used to investigate how much of the adder inputs are in
      // common.
      BinaryCircuit setup_circuit = result.reduced_circuit;
      setup_circuit.outputs.clear();
      for (const auto [node, unused] : result.input_term_pairs) {
        setup_circuit.outputs.push_back(node);
      }
      setup_circuit.ResetBooleanMapping();
      OptimizeCircuit(absl::StrCat(prefix, "setup"), setup_circuit);
    }

    OptimizeCircuit(absl::StrCat(prefix, "final"), result.final_circuit);
    return;
  }

  BinaryCircuit circuit1 = FromBenchFile(filename1);
  BinaryCircuit circuit2 = FromBenchFile(filename2);

  // Simpler decomposition.
  if (absl::GetFlag(FLAGS_decompose)) {
    const int num_initial_outputs =
        absl::GetFlag(FLAGS_decompose_num_initial_outputs);
    const CircuitDecomposition decomp1 =
        SimplerDecomposition(num_initial_outputs, "A", circuit1);
    const CircuitDecomposition decomp2 =
        SimplerDecomposition(num_initial_outputs, "B", circuit2);

    DumpMitter("goal", decomp1.goal, decomp2.goal);
    DumpMitter("simplified_goal", decomp1.simplified_goal,
               decomp2.simplified_goal, /*verify=*/true);
    DumpMitter("hard", decomp1.hard, decomp2.hard);
    return;
  }

  OptimizeCircuit("circuit1", circuit1);
  OptimizeCircuit("circuit2", circuit2);

  if (absl::GetFlag(FLAGS_count_common_outputs)) {
    SubcircuitExtractor extract1(circuit1);
    SubcircuitExtractor extract2(circuit2);
    const int m = std::min(circuit1.outputs.size(), circuit2.outputs.size());
    if (circuit1.outputs.size() != circuit2.outputs.size()) {
      LOG(INFO) << "WARNING: Circuit do not have the same number of outputs, "
                   "testing only the first ones!";
    }
    int num_in_common = 0;
    for (int k = 0; k < m; ++k) {
      circuit1.ResetBooleanMapping();
      circuit2.ResetBooleanMapping();
      BinaryCircuit a = circuit1;
      a.outputs = {circuit1.outputs[k]};
      BinaryCircuit b = circuit2;
      b.outputs = {circuit2.outputs[k]};
      BinaryCircuit mitter = ConstructMitter(a, b);
      const CpModelProto proto = ConstructCpModelFromBinaryCircuit(
          mitter, /*enforce_one_output=*/true);
      if (ModelIsInfeasible(proto)) {
        ++num_in_common;
        LOG(INFO) << k << " equiv";
      } else {
        LOG(INFO) << k << " not_equiv";
      }
    }
    LOG(INFO) << num_in_common << "/" << m << " outputs in common";
    return;
  }

  BinaryCircuit mitter = ConstructMitter(circuit1, circuit2);

  {
    // Lets display circuit1 with special color for the node that seems
    // equivalent.
    absl::BitGen random;
    CompactVectorVector<int, Literal> equiv =
        SampleForEquivalences(mitter, random, {});
    std::vector<int> special_nodes;
    for (const absl::Span<const Literal> literals : equiv) {
      for (const Literal lit : literals) {
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
