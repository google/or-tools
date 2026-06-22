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

#include "benchmark/benchmark.h"
#include "ortools/math_opt/elemental/element_storage.h"

namespace operations_research::math_opt {
namespace {

void BM_AddElements(benchmark::State& state) {
  const int n = state.range(0);
  for (auto s : state) {
    ElementStorage storage;
    for (int i = 0; i < n; ++i) {
      storage.Add("");
    }
    benchmark::DoNotOptimize(storage);
  }
}

BENCHMARK(BM_AddElements)->Arg(100)->Arg(10000);

void BM_Exists(benchmark::State& state) {
  const int n = state.range(0);
  ElementStorage storage;
  for (int i = 0; i < n; ++i) {
    storage.Add("");
  }
  for (auto s : state) {
    for (int i = 0; i < 2 * n; ++i) {
      bool e = storage.Exists(i);
      benchmark::DoNotOptimize(e);
    }
  }
}

BENCHMARK(BM_Exists)->Arg(100)->Arg(10000);

}  // namespace
}  // namespace operations_research::math_opt
