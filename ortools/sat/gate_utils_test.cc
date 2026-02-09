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

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_base.h"

namespace operations_research::sat {
namespace {

TEST(GetNumBitsAtOneTest, BasicTest) {
  EXPECT_EQ(GetNumBitsAtOne(1), 1);
  EXPECT_EQ(GetNumBitsAtOne(2), 3);
  EXPECT_EQ(GetNumBitsAtOne(8), 255);
  EXPECT_EQ(GetNumBitsAtOne(32), ~SmallBitset(0));
}

TEST(CanonicalizeTruthTableTest, BasicBehavior1) {
  std::array<int, 3> key = {0, 2, 1};

  // no change here.
  SmallBitset bitmask = 0b10101010;
  CanonicalizeTruthTable<int>(absl::MakeSpan(key), bitmask);
  EXPECT_EQ(std::bitset<8>(bitmask), std::bitset<8>(0b10101010));
}

TEST(CanonicalizeTruthTableTest, BasicBehavior2) {
  std::array<int, 3> key = {2, 0, 1};
  SmallBitset bitmask = 0b10101010;
  CanonicalizeTruthTable<int>(absl::MakeSpan(key), bitmask);
  EXPECT_EQ(std::bitset<8>(bitmask), std::bitset<8>(0b11110000));
}

TEST(CanonicalizeTruthTableTest, BasicBehavior3) {
  std::array<int, 3> key = {1, 0, 2};
  SmallBitset bitmask = 0b10101010;
  CanonicalizeTruthTable<int>(absl::MakeSpan(key), bitmask);
  EXPECT_EQ(std::bitset<8>(bitmask), std::bitset<8>(0b11001100));
}

TEST(FillKeyAndBitmaskTest, BasicBehavior1) {
  std::array<BooleanVariable, 3> key;
  SmallBitset bitmask;
  FillKeyAndBitmask({Literal(+1), Literal(-2), Literal(+3)},
                    absl::MakeSpan(key), bitmask);
  EXPECT_THAT(key,
              ::testing::ElementsAre(BooleanVariable(0), BooleanVariable(1),
                                     BooleanVariable(2)));
  // The bit number 2 = 0b010 should be off.
  EXPECT_EQ(std::bitset<8>(bitmask), std::bitset<8>(0b11111011));
}

TEST(IsFunctionTest, ConstantValue) {
  EXPECT_TRUE(IsFunction(0, 3, 0b10101010));
  EXPECT_FALSE(IsFunction(1, 3, 0b10101010));
  EXPECT_FALSE(IsFunction(2, 3, 0b10101010));
}

TEST(AddHoleAtPositionTest, BasicTest) {
  EXPECT_EQ(AddHoleAtPosition(0, 0xFF), 0b111111110);
  EXPECT_EQ(AddHoleAtPosition(1, 0xFF), 0b111111101);
  EXPECT_EQ(AddHoleAtPosition(8, 0xFF), 0b011111111);
}

TEST(CanonicalizeFunctionTruthTableTest, AndGateWithXAndNotX) {
  Literal output = Literal(+1);
  std::vector<Literal> inputs{Literal(+2), Literal(-2)};
  int table = 0b1000;
  const int new_size =
      CanonicalizeFunctionTruthTable(output, absl::MakeSpan(inputs), table);
  CHECK_EQ(new_size, 0);  // Fixed to zero.
}

TEST(RemoveFixedInputTest, BasicTest1) {
  std::vector<Literal> inputs{Literal(+1), Literal(+2), Literal(+3)};
  int table = 0b01011010;
  const int new_size = RemoveFixedInput(1, true, absl::MakeSpan(inputs), table);
  EXPECT_EQ(new_size, 2);
  EXPECT_EQ(inputs[0], Literal(+1));
  EXPECT_EQ(inputs[1], Literal(+3));
  EXPECT_EQ(table, 0b0110) << std::bitset<4>(table);
}

TEST(RemoveFixedInputTest, BasicTest2) {
  std::vector<Literal> inputs{Literal(+1), Literal(+2), Literal(+3)};
  int table = 0b01011010;
  const int new_size =
      RemoveFixedInput(1, false, absl::MakeSpan(inputs), table);
  EXPECT_EQ(new_size, 2);
  EXPECT_EQ(inputs[0], Literal(+1));
  EXPECT_EQ(inputs[1], Literal(+3));
  EXPECT_EQ(table, 0b0110) << std::bitset<4>(table);
}

TEST(CanonicalizeFunctionTruthTableTest, AndGateWithXAndNotX2) {
  Literal output = Literal(+1);
  std::vector<Literal> inputs{Literal(-2), Literal(+2)};
  int table = 0b1000;
  const int new_size =
      CanonicalizeFunctionTruthTable(output, absl::MakeSpan(inputs), table);
  CHECK_EQ(new_size, 0);  // Fixed to zero.
}

TEST(CanonicalizeFunctionTruthTableTest, RandomTest) {
  absl::BitGen random;
  const int num_vars = 8;

  for (int num_test = 0; num_test < 1000; ++num_test) {
    // Lets generate a random function on k random variables.
    const int k = absl::Uniform(random, 0, 4);
    const int table = absl::Uniform<uint64_t>(random, 0, 1 << (1 << k));
    const Literal output(BooleanVariable(100), absl::Bernoulli(random, 0.5));
    std::vector<Literal> inputs;
    for (int i = 0; i < k; ++i) {
      inputs.push_back(
          Literal(BooleanVariable(absl::Uniform(random, 0, num_vars)),
                  absl::Bernoulli(random, 0.5)));
    }

    Literal new_output = output;
    std::vector<Literal> new_inputs = inputs;
    int new_table = table;
    const int new_size = CanonicalizeFunctionTruthTable(
        new_output, absl::MakeSpan(new_inputs), new_table);
    new_inputs.resize(new_size);

    // Log before potential failure.
    LOG(INFO) << "IN  arity=" << k << " " << output << " = f(" << inputs << ") "
              << std::bitset<16>(table);
    LOG(INFO) << "OUT arity=" << new_size << " " << new_output << " = f("
              << new_inputs << ") " << std::bitset<16>(new_table);

    // Now check that both function always take the same value.
    for (int m = 0; m < (1 << num_vars); ++m) {
      int index = 0;
      for (int i = 0; i < inputs.size(); ++i) {
        const Literal lit = inputs[i];
        int value = (m >> lit.Variable().value()) & 1;
        if (!lit.IsPositive()) value = 1 - value;
        index |= value << i;
      }
      const int target_value = (table >> index) & 1;

      int new_index = 0;
      for (int i = 0; i < new_inputs.size(); ++i) {
        const Literal lit = new_inputs[i];
        int value = (m >> lit.Variable().value()) & 1;
        if (!lit.IsPositive()) value = 1 - value;
        new_index |= value << i;
      }
      const int new_target_value = (new_table >> new_index) & 1;

      if (output == new_output) {
        ASSERT_EQ(target_value, new_target_value) << index << " " << new_index;
      } else {
        ASSERT_EQ(output, new_output.Negated());
        ASSERT_EQ(target_value, 1 - new_target_value)
            << index << " " << new_index;
      }
    }
  }
}

TEST(CombineGate2Test, Exhaustive) {
  for (int type = 0; type < 16; ++type) {
    for (int a = 0; a < 2; ++a) {
      for (int b = 0; b < 2; ++b) {
        ASSERT_EQ(CombineGate2(type, a, b) & 1, (type >> (a + 2 * b)) & 1)
            << std::bitset<4>(type) << " " << a << " " << b;
      }
    }
  }
}

// Test reduction by proving infeasible mitter.
TEST(ReduceTest, Random) {
  absl::BitGen random;

  // Lets create a random circuit.
  BinaryCircuit circuit;
  circuit.num_inputs = 10;
  circuit.num_vars = 30;
  for (int i = circuit.num_inputs; i < circuit.num_vars; ++i) {
    circuit.gates.emplace_back(absl::Uniform(random, 0, 16), i,
                               absl::Uniform(random, 0, i),
                               absl::Uniform(random, 0, i));
  }
  circuit.ResetBooleanMapping();
  LOG(INFO) << "random: " << circuit.DebugString();

  // Lets extract subcicuit to compute the last variable.
  SubcircuitExtractor extractor(circuit);
  const BinaryCircuit base = extractor.Extract({circuit.num_vars - 1});
  LOG(INFO) << "base: " << base.DebugString();

  BinaryCircuit simplified = base;
  RemoveEquivalences({}, &simplified);
  ReduceGates(&simplified);
  LOG(INFO) << "simplified: " << simplified.DebugString();

  // The mitter should be infeasible.
  {
    BinaryCircuit mitter = ConstructMitter(base, simplified);
    const CpModelProto cp_model =
        ConstructCpModelFromBinaryCircuit(mitter, /*enforce_one_output=*/true);
    const CpSolverResponse response = Solve(cp_model);
    EXPECT_EQ(response.status(), INFEASIBLE);

    if (mitter.num_inputs < 20) {
      // Full enumeration should give same result.
      // We need to force output to 1 though.
      mitter.gates.emplace_back(0b1111, mitter.outputs[0], 0, 0);
      ASSERT_FALSE(BinaryCircuitIsFeasible(mitter));
    }
  }

  // We can optimize further.
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

  // Detect and remove equivalent variables.
  {
    absl::BitGen random;
    std::vector<std::vector<BooleanVariable>> solutions;
    for (int i = 0; i < 3; ++i) {
      operations_research::sat::SimplifyCircuit(10, random, cp_sat_solve,
                                                &solutions, &simplified);
      LOG(INFO) << "SIMPLIFIED " << simplified.DebugString();
    }
    operations_research::sat::SampleForEquivalences(simplified, random, {});
  }

  // We should get the same result
  {
    const BinaryCircuit mitter = ConstructMitter(base, simplified);
    const CpModelProto cp_model =
        ConstructCpModelFromBinaryCircuit(mitter, /*enforce_one_output=*/true);
    const CpSolverResponse response = Solve(cp_model);
    EXPECT_EQ(response.status(), INFEASIBLE);
  }
}

}  // namespace
}  // namespace operations_research::sat
