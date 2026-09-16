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

#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/elemental/testing.h"

namespace operations_research::math_opt {
namespace {

template <int dimension, typename Policy = Elemental::DiePolicy,
          typename Symmetry = NoSymmetry>
void BM_RandomGet(benchmark::State& state) {
  const int n = state.range(0);
  Elemental elemental;
  // Create a model with n variables and n constraints, attributes on all
  // variables and all (variable x constraint).
  std::vector<VariableId> vars;
  std::vector<LinearConstraintId> constraints;
  for (int i = 0; i < n; ++i) {
    vars.push_back(elemental.AddElement<ElementType::kVariable>(""));
    constraints.push_back(
        elemental.AddElement<ElementType::kLinearConstraint>(""));
  }
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);
  for (int i = 0; i < n; ++i) {
    elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(vars[i]), 43.0);
    for (int j = 0; j < n; ++j) {
      elemental.SetAttr(DoubleAttr2::kLinConCoef,
                        AttrKey(vars[i], constraints[j]), 42.0);
    }
  }
  constexpr int kNumKeys = 1000;
  const auto keys = MakeRandomAttrKeys<dimension, NoSymmetry>(kNumKeys, n);
  for (auto s : state) {
    for (const auto& key : keys) {
      if constexpr (dimension == 0) {
        auto v = elemental.GetAttr<Policy>(BoolAttr0::kMaximize, key);
        benchmark::DoNotOptimize(v);
      } else if constexpr (dimension == 1) {
        auto v = elemental.GetAttr<Policy>(DoubleAttr1::kVarLb, key);
        benchmark::DoNotOptimize(v);
      } else if constexpr (dimension == 2) {
        auto v = elemental.GetAttr<Policy>(DoubleAttr2::kLinConCoef, key);
        benchmark::DoNotOptimize(v);
      }
    }
  }
}
BENCHMARK(BM_RandomGet<0>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<0, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<0, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);

void BM_DeleteElement(benchmark::State& state) {
  const int n = state.range(0);
  constexpr int kNumKeys = 100;
  constexpr auto kAttr = DoubleAttr2::kLinConCoef;
  const auto keys = MakeRandomAttrKeys<2, NoSymmetry>(kNumKeys, n);
  for (auto s : state) {
    state.PauseTiming();
    auto elemental = std::make_unique<Elemental>();
    for (int i = 0; i < n; ++i) {
      elemental->AddElement<ElementType::kVariable>("");
      elemental->AddElement<ElementType::kLinearConstraint>("");
    }
    for (int v = 0; v < n; ++v) {
      for (int c = 0; c < n; ++c) {
        elemental->SetAttr(kAttr, AttrKey(c, v), 42.0);
      }
    }
    state.ResumeTiming();
    for (int v = 0; v < n; ++v) {
      elemental->DeleteElement(VariableId(v));
    }
    state.PauseTiming();
    CHECK_EQ(elemental->AttrNonDefaults(kAttr).size(), 0);
    elemental.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeleteElement)->Arg(10)->Arg(100);

}  // namespace
}  // namespace operations_research::math_opt
