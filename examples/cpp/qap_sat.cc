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

#include <string>
#include <tuple>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/qap_reader.h"

ABSL_FLAG(std::string, input, "", "Input file name containing a QAP instance.");
ABSL_FLAG(std::string, params, "", "Specific params to use with CP-SAT.");

namespace operations_research {
namespace sat {

void SolveQap() {
  LOG(INFO) << "Reading QAP problem from '" << absl::GetFlag(FLAGS_input)
            << "'.";
  QapProblem qap = ReadQapProblemOrDie(absl::GetFlag(FLAGS_input));
  const int n = qap.weights.size();

  CpModelBuilder cp_model;

  // Create placement variables.
  // place_vars[f][l] contains the binary variable that decides whether to put
  // factory f in location l.
  std::vector<std::vector<BoolVar>> place_vars;
  place_vars.resize(n);
  for (int f = 0; f < n; ++f) {
    place_vars[f].resize(n);
    for (int l = 0; l < n; ++l) {
      place_vars[f][l] =
          cp_model.NewBoolVar().WithName(absl::StrCat("place_", f, "_", l));
    }
  }

  // Place each factory exactly once.
  for (int f = 0; f < n; ++f) {
    cp_model.AddExactlyOne(place_vars[f]);
  }

  // Occupy each location exactly once.
  for (int l = 0; l < n; ++l) {
    std::vector<BoolVar> tmp;
    tmp.reserve(n);
    for (int f = 0; f < n; ++f) {
      tmp.push_back(place_vars[f][l]);
    }
    cp_model.AddExactlyOne(tmp);
  }

  // Create objective.
  absl::flat_hash_map<std::tuple<int, int, int, int>, BoolVar> cache;
  LinearExpr objective;
  for (int f1 = 0; f1 < n; ++f1) {
    for (int f2 = 0; f2 < n; ++f2) {
      if (f1 == f2) continue;
      if (qap.weights[f1][f2] == 0) continue;
      for (int l1 = 0; l1 < n; ++l1) {
        for (int l2 = 0; l2 < n; ++l2) {
          if (l1 == l2) continue;
          if (qap.distances[l1][l2] == 0) continue;

          const std::tuple<int, int, int, int> key =
              f1 < f2 ? std::make_tuple(f1, f2, l1, l2)
                      : std::make_tuple(f2, f1, l2, l1);
          BoolVar product;
          const auto it = cache.find(key);
          if (it == cache.end()) {
            product = cp_model.NewBoolVar();
            cp_model.AddMultiplicationEquality(product, place_vars[f1][l1],
                                               place_vars[f2][l2]);
            cache[key] = product;
          } else {
            product = it->second;
          }

          objective += product * qap.weights[f1][f2] * qap.distances[l1][l2];
        }
      }
    }
  }

  for (int f = 0; f < n; ++f) {
    for (int l = 0; l < n; ++l) {
      objective += place_vars[f][l] * qap.weights[f][f] * qap.distances[l][l];
    }
  }

  cp_model.Minimize(objective);

  // Setup parameters.
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  // Parse the --params flag.
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  // Solve.
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);
}

}  // namespace sat
}  // namespace operations_research

static const char kUsageStr[] =
    "Solves quadratic assignment problems with CP-SAT. "
    "The input file should have the format described in the QAPLIB (see "
    "http://anjos.mgi.polymtl.ca/qaplib/).";

int main(int argc, char** argv) {
  InitGoogle(kUsageStr, &argc, &argv, /*remove_flags=*/true);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(INFO) << "--input is required";
    return EXIT_SUCCESS;
  }

  operations_research::sat::SolveQap();
  return EXIT_SUCCESS;
}
