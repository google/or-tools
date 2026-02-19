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

// Checks an LRAT proof that a given CNF formula is unsatisfiable.
// See https://arxiv.org/abs/1612.02353 for the LRAT format.
//
// Usage:
//   lrat_checker_main --cnf=... --lrat=...

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/init_google.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/lrat_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_cnf_reader.h"
#include "ortools/sat/sat_parameters.pb.h"

ABSL_FLAG(std::string, cnf, "", "Input CNF file");
ABSL_FLAG(std::string, lrat, "", "Input LRAT proof file to check");

namespace operations_research {
namespace sat {
int Main() {
  Model model;
  LratChecker checker(&model);
  checker.EnableRatProofs();

  absl::flat_hash_map<uint64_t, ClausePtr> all_clauses;

  WallTimer timer;
  timer.Start();
  {
    std::cout << "Loading CNF file: " << absl::GetFlag(FLAGS_cnf) << std::endl;
    SatCnfReader cnf_reader;
    CpModelProto problem;
    if (!cnf_reader.Load(absl::GetFlag(FLAGS_cnf), &problem)) {
      std::cout << "Failed to load CNF file: " << absl::GetFlag(FLAGS_cnf)
                << std::endl;
      return EXIT_FAILURE;
    }
    auto ref_to_literal = [](int ref) {
      return Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref));
    };
    std::vector<Literal> clause;
    for (int i = 0; i < problem.constraints_size(); ++i) {
      const ConstraintProto& ct = problem.constraints(i);
      CHECK_EQ(ct.constraint_case(), ConstraintProto::kBoolOr);
      clause.clear();
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(ref_to_literal(ref).Negated());
      }
      for (const int ref : ct.bool_or().literals()) {
        clause.push_back(ref_to_literal(ref));
      }
      const ClausePtr clause_ptr = ClausePtr(clause);
      CHECK(checker.AddProblemClause(clause_ptr));
      all_clauses[i + 1] = clause_ptr;
    }
  }

  std::ifstream proof(absl::GetFlag(FLAGS_lrat));
  if (!proof.is_open()) {
    std::cout << "Failed to open LRAT proof file: " << absl::GetFlag(FLAGS_lrat)
              << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Checking LRAT proof file: " << absl::GetFlag(FLAGS_lrat)
            << std::endl;
  int line_number = 0;
  auto error = [&](absl::string_view message) {
    std::cout << "At line " << line_number << ": " << message << std::endl;
    return EXIT_FAILURE;
  };
  std::string line;
  std::vector<Literal> clause;
  std::vector<ClausePtr> rup_clauses;
  std::vector<LratChecker::RatClauses> rat_clauses;
  while (std::getline(proof, line)) {
    ++line_number;
    std::vector<std::string_view> terms = absl::StrSplit(line, ' ');
    if (terms.size() < 2 || terms.back() != "0") {
      return error("Invalid line");
    }
    uint64_t id;
    if (!absl::SimpleAtoi(terms[0], &id)) {
      return error("Failed to parse clause ID");
    }
    clause.clear();
    rup_clauses.clear();
    rat_clauses.clear();
    if (terms[1] == "d") {
      for (int i = 2; i < terms.size(); ++i) {
        uint64_t ref;
        if (!absl::SimpleAtoi(terms[i], &ref)) {
          return error("Failed to parse deletion clause ID");
        }
        if (ref == 0) {
          if (i != terms.size() - 1) {
            return error("0 should only appear at the end of the line");
          }
        } else {
          auto it = all_clauses.find(ref);
          if (it == all_clauses.end()) {
            return error(absl::StrCat("Clause ", ref, " not found"));
          }
          rup_clauses.push_back(it->second);
          all_clauses.erase(it);
        }
      }
      checker.DeleteClauses(rup_clauses);
      for (const ClausePtr clause_ptr : rup_clauses) {
        delete clause_ptr.GetSatClause();
      }
    } else {
      bool clause_done = false;
      for (int i = 1; i < terms.size(); ++i) {
        int64_t ref;
        if (!absl::SimpleAtoi(terms[i], &ref)) {
          return error("Failed to parse number");
        }
        if (ref == 0) {
          if (!clause_done) {
            clause_done = true;
          } else {
            if (i != terms.size() - 1) {
              return error(
                  "second 0 should only appear at the end of the line");
            }
            const ClausePtr clause_ptr = ClausePtr(clause);
            all_clauses[id] = clause_ptr;
            if (!checker.AddInferredClause(clause_ptr, rup_clauses,
                                           rat_clauses)) {
              return error(absl::StrCat("Invalid inferred clause: ",
                                        checker.error_message()));
            }
          }
        } else if (!clause_done) {
          clause.push_back(Literal(ref));
        } else if (ref > 0) {
          auto it = all_clauses.find(ref);
          if (it == all_clauses.end()) {
            return error(absl::StrCat("Clause ", ref, " not found"));
          }
          if (rat_clauses.empty()) {
            rup_clauses.push_back(it->second);
          } else {
            rat_clauses.back().rup_clauses.push_back(it->second);
          }
        } else {
          auto it = all_clauses.find(-ref);
          if (it == all_clauses.end()) {
            return error(absl::StrCat("Clause ", -ref, " not found"));
          }
          rat_clauses.push_back({.resolvant = it->second});
        }
      }
    }
  }
  proof.close();
  for (const auto& [ref, clause_ptr] : all_clauses) {
    delete clause_ptr.GetSatClause();
  }
  std::cout << "Check done in " << timer.GetDuration() << std::endl;
  if (checker.Check()) {
    std::cout << "VERIFIED UNSAT" << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cout << "FAILED TO VERIFY UNSAT: " << checker.error_message()
              << std::endl;
    return EXIT_FAILURE;
  }
}
}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);

  return operations_research::sat::Main();
}
