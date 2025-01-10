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

#include "ortools/util/qap_reader.h"

#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {

// TODO(user): Unit test cases when the function dies, or return
// (and test) a status instead.
QapProblem ReadQapProblemOrDie(absl::string_view filepath) {
  QapProblem qap_problem;

  int n = 0;
  int k = 0;
  for (const std::string& line :
       FileLines(filepath, FileLineIterator::REMOVE_LINEFEED)) {
    const std::vector<std::string> tokens =
        absl::StrSplit(line, ' ', absl::SkipEmpty());
    if (tokens.empty()) continue;
    if (k == 0) {
      CHECK_GE(tokens.size(), 1);
      CHECK_LE(tokens.size(), 2);
      CHECK(absl::SimpleAtoi(tokens[0], &n));
      qap_problem.weights.resize(n);
      qap_problem.distances.resize(n);
      for (int j = 0; j < n; ++j) {
        qap_problem.weights[j].assign(n, std::numeric_limits<int64_t>::max());
        qap_problem.distances[j].assign(n, std::numeric_limits<int64_t>::max());
      }
      if (tokens.size() == 2) {
        CHECK(absl::SimpleAtoi(tokens[1], &qap_problem.best_known_solution));
      }
      ++k;
    } else if (k <= n * n) {
      for (const std::string& token : tokens) {
        const int i = (k - 1) / n;
        const int j = (k - 1) % n;
        int64_t v = 0.0;
        CHECK(absl::SimpleAtoi(token, &v)) << "'" << token << "'";
        qap_problem.weights[i][j] = v;
        ++k;
      }
    } else if (k <= 2 * n * n) {
      for (const std::string& token : tokens) {
        const int i = (k - n * n - 1) / n;
        const int j = (k - n * n - 1) % n;
        int64_t v = 0.0;
        CHECK(absl::SimpleAtoi(token, &v));
        qap_problem.distances[i][j] = v;
        ++k;
      }
    } else {
      CHECK(false) << "File contains more than 1 + 2 * N^2 entries.";
    }
  }
  return qap_problem;
}

}  // namespace operations_research
