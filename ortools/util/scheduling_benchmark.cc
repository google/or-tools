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
#include <cstdint>
#include <random>

#include "benchmark/benchmark.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/scheduling.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

DEFINE_STRONG_INT64_TYPE(IntegerValue);

static void BM_update(benchmark::State& state) {
  random_engine_t random_;
  const int size = state.range(0);
  const int num_updates = 4 * size;
  ThetaLambdaTree<IntegerValue> tree;
  std::uniform_int_distribution<int> event_dist(0, size - 1);
  std::uniform_int_distribution<int> enveloppe_dist(-10000, 10000);
  std::uniform_int_distribution<int> energy_dist(0, 10000);
  for (auto _ : state) {
    tree.Reset(size);
    for (int i = 0; i < num_updates; ++i) {
      const int event = event_dist(random_);
      const IntegerValue enveloppe(enveloppe_dist(random_));
      const IntegerValue energy1(energy_dist(random_));
      const IntegerValue energy2(energy_dist(random_));
      tree.AddOrUpdateEvent(event, enveloppe, std::min(energy1, energy2),
                            std::max(energy1, energy2));
    }
  }
  // Number of updates.
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          num_updates);
}

// Note that we didn't pick only powers of two.
BENCHMARK(BM_update)->Arg(10)->Arg(20)->Arg(64)->Arg(100)->Arg(256)->Arg(800);

}  // namespace
}  // namespace sat
}  // namespace operations_research
