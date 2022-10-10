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

#include <limits>
#include <string>
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
namespace {

// place_vars[f][l] contains the binary variable that decides whether to put
// factory f in location l.
std::vector<std::vector<BoolVar>> CreatePlaceVars(int n,
                                                  CpModelBuilder& cp_model) {
  const BoolVar false_literal = cp_model.FalseVar();
  std::vector<std::vector<BoolVar>> place_vars;
  place_vars.resize(n);
  for (int f = 0; f < n; ++f) {
    place_vars[f].assign(n, false_literal);
  }

  for (int f = 0; f < n; ++f) {
    for (int l = 0; l < n; ++l) {
      place_vars[f][l] =
          cp_model.NewBoolVar().WithName(absl::StrCat("place_", f, "_", l));
    }
  }
  return place_vars;
}

void CreatePlacementConstraints(
    const std::vector<std::vector<BoolVar>>& place_vars,
    CpModelBuilder& cp_model) {
  const int n = place_vars.size();

  // Place each factory exactly once.
  for (int f = 0; f < n; ++f) {
    std::vector<BoolVar> tmp;
    for (int l = 0; l < n; ++l) {
      tmp.push_back(place_vars[f][l]);
    }
    cp_model.AddExactlyOne(tmp);
  }

  // Occupy each location exactly once.
  for (int l = 0; l < n; ++l) {
    std::vector<BoolVar> tmp;
    for (int f = 0; f < n; ++f) {
      tmp.push_back(place_vars[f][l]);
    }
    cp_model.AddExactlyOne(tmp);
  }
}

void CreateObjective(const QapProblem& qap,
                     const std::vector<std::vector<BoolVar>>& place_vars,
                     CpModelBuilder& cp_model) {
  const int n = place_vars.size();

  LinearExpr objective;
  for (int f1 = 0; f1 < n; ++f1) {
    for (int f2 = 0; f2 < n; ++f2) {
      if (f1 == f2) continue;
      if (qap.weights[f1][f2] == 0) continue;
      for (int l1 = 0; l1 < n; ++l1) {
        for (int l2 = 0; l2 < n; ++l2) {
          if (l1 == l2) continue;
          if (qap.distances[l1][l2] == 0) continue;

          BoolVar product = cp_model.NewBoolVar();
          cp_model.AddMultiplicationEquality(product, place_vars[f1][l1],
                                             place_vars[f2][l2]);

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
}

}  // namespace

void SolveQap() {
  LOG(INFO) << "Reading QAP problem from '" << absl::GetFlag(FLAGS_input)
            << "'.";
  QapProblem qap = ReadQapProblemOrDie(absl::GetFlag(FLAGS_input));
  const int n = qap.weights.size();

  CpModelBuilder cp_model;
  std::vector<std::vector<BoolVar>> place_vars = CreatePlaceVars(n, cp_model);
  CreatePlacementConstraints(place_vars, cp_model);
  CreateObjective(qap, place_vars, cp_model);

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
    "Solves quadratic assigment problems with CP-SAT. "
    "The input file should have the format described in the QAPLIB (see "
    "http://anjos.mgi.polymtl.ca/qaplib/).";

int main(int argc, char** argv) {
  InitGoogle(kUsageStr, &argc, &argv, /*remove_flags=*/true);
  CHECK(!absl::GetFlag(FLAGS_input).empty()) << "--input is required";

  operations_research::sat::SolveQap();
  return EXIT_SUCCESS;
}
