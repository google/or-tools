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

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/opb_reader.h"
#include "ortools/sat/sat_cnf_reader.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/file_util.h"
#include "ortools/util/logging.h"
#include "ortools/util/sigint.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(
    std::string, input, "",
    "Required: input file of the problem to solve. Many format are supported:"
    ".cnf (sat, max-sat, weighted max-sat), .opb (pseudo-boolean sat/optim) "
    "and by default the CpModelProto proto (binary or text).");

ABSL_FLAG(
    std::string, hint_file, "",
    "Protobuf file containing a CpModelResponse. The solution will be used as a"
    " hint to bootstrap the search.");

ABSL_FLAG(std::string, domain_file, "",
          "Protobuf file containing a CpModelResponse. If present, the "
          "tightened models will be used to reduce the domain of variables.");

ABSL_FLAG(std::string, output, "",
          "If non-empty, write the response there. By default it uses the "
          "binary format except if the file extension is '.txt'.");

ABSL_FLAG(std::string, params, "",
          "Parameters for the sat solver in a text format of the "
          "SatParameters proto, example: --params=use_conflicts:true.");

ABSL_FLAG(bool, wcnf_use_strong_slack, true,
          "If true, when we add a slack variable to reify a soft clause, we "
          "enforce the fact that when it is true, the clause must be false.");
ABSL_FLAG(bool, fingerprint_intermediate_solutions, false,
          "Attach the fingerprint of intermediate solutions to the output.");
ABSL_FLAG(bool, competition_mode, false,
          "If true, output the log in a competition format.");
ABSL_FLAG(bool, force_interleave_search, false,
          "If true, enable interleaved workers when num_workers is 1.");

namespace operations_research {
namespace sat {
namespace {

void TryToRemoveSuffix(absl::string_view suffix, std::string* str) {
  if (file::Extension(*str) == suffix) *str = file::Stem(*str);
}

std::string ExtractName(absl::string_view full_filename) {
  std::string filename = std::string(file::Basename(full_filename));
  // The order is important as '.pb.txt.gz' is a common suffix.
  TryToRemoveSuffix("gz", &filename);
  TryToRemoveSuffix("txt", &filename);
  TryToRemoveSuffix("pb", &filename);
  TryToRemoveSuffix("pbtxt", &filename);
  TryToRemoveSuffix("proto", &filename);
  TryToRemoveSuffix("prototxt", &filename);
  TryToRemoveSuffix("textproto", &filename);
  TryToRemoveSuffix("bin", &filename);
  return filename;
}

class LastSolutionPrinter {
 public:
  // Note that is prints the solution in the PB competition format.
  void MaybePrintLastSolution() {
    absl::MutexLock lock(mutex_);
    if (last_solution_printed_) return;
    last_solution_printed_ = true;

    if (last_solution_.empty()) {
      std::cout << "s UNKNOWN" << std::endl;
    } else {
      std::cout << "s SATISFIABLE" << std::endl;
      std::string line;
      for (int i = 0; i < num_variables_; ++i) {
        if (last_solution_[i]) {
          absl::StrAppend(&line, "x", i + 1, " ");
        } else {
          absl::StrAppend(&line, "-x", i + 1, " ");
        }
        if (line.size() >= 75) {
          std::cout << "v " << line << std::endl;
          line.clear();
        }
      }
      if (!line.empty()) {
        std::cout << "v " << line << std::endl;
      }
    }
  }

  void set_num_variables(int num_variables) { num_variables_ = num_variables; }

  void set_last_solution(absl::Span<const int64_t> solution) {
    absl::MutexLock lock(mutex_);
    if (last_solution_printed_) return;
    last_solution_.assign(solution.begin(), solution.end());
  }

  // Returns false if the solution has already been printed, else mark it as
  // printed by caller code.
  bool mark_last_solution_printed() {
    const absl::MutexLock lock(mutex_);
    if (last_solution_printed_) {
      return false;
    }
    last_solution_printed_ = true;
    return true;
  }

 private:
  int num_variables_ = 0;
  std::vector<int64_t> last_solution_ ABSL_GUARDED_BY(mutex_);
  bool last_solution_printed_ ABSL_GUARDED_BY(mutex_) = false;
  absl::Mutex mutex_;
};

void LogInPbCompetitionFormat(
    int num_variables, bool has_objective, Model* model,
    SatParameters* parameters,
    std::shared_ptr<LastSolutionPrinter> last_solution_printer) {
  CHECK(last_solution_printer != nullptr);
  last_solution_printer->set_num_variables(num_variables);

  const auto log_callback = [](const std::string& multi_line_input) {
    if (multi_line_input.empty()) {
      std::cout << "c" << std::endl;
      return;
    }
    const std::vector<absl::string_view> lines =
        absl::StrSplit(multi_line_input, '\n');
    for (const absl::string_view& line : lines) {
      std::cout << "c " << line << std::endl;
    }
  };
  model->GetOrCreate<SolverLogger>()->AddInfoLoggingCallback(log_callback);
  parameters->set_log_to_stdout(false);

  const auto response_callback = [last_solution_printer](
                                     const CpSolverResponse& r) {
    std::cout << "o " << static_cast<int64_t>(r.objective_value()) << std::endl;
    last_solution_printer->set_last_solution(r.solution());
  };
  model->Add(NewFeasibleSolutionObserver(response_callback));

  const auto final_response_callback =
      [num_variables, has_objective,
       last_solution_printer](CpSolverResponse* r) {
        if (!last_solution_printer->mark_last_solution_printed()) return;

        switch (r->status()) {
          case CpSolverStatus::OPTIMAL:
            if (has_objective) {
              std::cout << "s OPTIMUM FOUND " << std::endl;
            } else {
              std::cout << "s SATISFIABLE" << std::endl;
            }
            break;
          case CpSolverStatus::FEASIBLE:
            std::cout << "s SATISFIABLE" << std::endl;
            break;
          case CpSolverStatus::INFEASIBLE:
            std::cout << "s UNSATISFIABLE" << std::endl;
            break;
          case CpSolverStatus::MODEL_INVALID:
            std::cout << "s UNSUPPORTED" << std::endl;
            break;
          case CpSolverStatus::UNKNOWN:
            std::cout << "s UNKNOWN" << std::endl;
            break;
          default:
            break;
        }
        if (r->status() == CpSolverStatus::OPTIMAL ||
            r->status() == CpSolverStatus::FEASIBLE) {
          std::string line;
          for (int i = 0; i < num_variables; ++i) {
            if (r->solution(i)) {
              absl::StrAppend(&line, "x", i + 1, " ");
            } else {
              absl::StrAppend(&line, "-x", i + 1, " ");
            }
            if (line.size() >= 75) {
              std::cout << "v " << line << std::endl;
              line.clear();
            }
          }
          if (!line.empty()) {
            std::cout << "v " << line << std::endl;
          }
        }
      };
  model->GetOrCreate<SharedResponseManager>()->AddFinalResponsePostprocessor(
      final_response_callback);
}

void SetInterleavedWorkers(SatParameters* parameters) {
  // Enable interleaved workers when num_workers is 1.
  if (parameters->num_workers() == 1) {
    parameters->set_interleave_search(true);
    parameters->set_use_rins_lns(false);
    parameters->add_subsolvers("default_lp");
    parameters->add_subsolvers("max_lp");
    parameters->add_subsolvers("quick_restart");
    parameters->add_subsolvers("core_or_no_lp");  // no_lp if no objective.
    parameters->set_num_violation_ls(1);          // Off if no objective.
  }
}

bool LoadProblem(const std::string& filename, absl::string_view hint_file,
                 absl::string_view domain_file, CpModelProto* cp_model,
                 Model* model, SatParameters* parameters,
                 std::shared_ptr<LastSolutionPrinter> last_solution_printer) {
  if (absl::EndsWith(filename, ".opb") ||
      absl::EndsWith(filename, ".opb.bz2") ||
      absl::EndsWith(filename, ".opb.gz") || absl::EndsWith(filename, ".wbo") ||
      absl::EndsWith(filename, ".wbo.bz2") ||
      absl::EndsWith(filename, ".wbo.gz")) {
    OpbReader reader;
    if (!reader.LoadAndValidate(filename, cp_model)) {
      if (!reader.model_is_supported()) {  // Some constants are too large.
        if (absl::GetFlag(FLAGS_competition_mode)) {
          // We output the official UNSUPPORTED status.
          std::cout << "s UNSUPPORTED" << std::endl;
          return false;  // Bypass the solve part.
        } else {
          // Create a dummy model with a single variable that overflows.
          // This way, the solver will return MODEL_INVALID instead of
          // crashing.
          IntegerVariableProto* var = cp_model->add_variables();
          var->add_domain(std::numeric_limits<int64_t>::min());
          var->add_domain(std::numeric_limits<int64_t>::max());
          return true;  // Will still call solve() to get the status.
        }
      } else {
        return false;  // Bypass the solve part.
      }
    }

    if (absl::GetFlag(FLAGS_competition_mode)) {
      const int num_variables =
          reader.model_is_supported() ? reader.num_variables() : 1;
      LogInPbCompetitionFormat(num_variables, cp_model->has_objective(), model,
                               parameters, last_solution_printer);
    }
    if (absl::GetFlag(FLAGS_force_interleave_search)) {
      SetInterleavedWorkers(parameters);
    }
    if (parameters->num_workers() >= 2 && parameters->num_workers() <= 15) {
      // Works better without symmetries in search
      // TODO(user): Investigate.
      parameters->add_ignore_subsolvers("max_lp_sym");
      parameters->add_extra_subsolvers("max_lp");
    }
  } else if (absl::EndsWith(filename, ".cnf") ||
             absl::EndsWith(filename, ".cnf.xz") ||
             absl::EndsWith(filename, ".cnf.gz") ||
             absl::EndsWith(filename, ".wcnf") ||
             absl::EndsWith(filename, ".wcnf.xz") ||
             absl::EndsWith(filename, ".wcnf.gz")) {
    SatCnfReader reader(absl::GetFlag(FLAGS_wcnf_use_strong_slack));
    if (!reader.Load(filename, cp_model)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else {
    CHECK_OK(ReadFileToProto(filename, cp_model));
  }

  // Read the hint file.
  if (!hint_file.empty()) {
    CpSolverResponse response;
    CHECK_OK(ReadFileToProto(hint_file, &response));
    if (!response.solution().empty()) {
      CHECK_EQ(response.solution_size(), cp_model->variables_size())
          << "The hint from the response proto is not compatible with the "
             "model proto";

      cp_model->clear_solution_hint();
      for (int i = 0; i < cp_model->variables_size(); ++i) {
        cp_model->mutable_solution_hint()->add_vars(i);
        cp_model->mutable_solution_hint()->add_values(response.solution(i));
      }
    } else {
      LOG(INFO) << "The response proto has no solutions, ignoring.";
    }
  }

  // Read the tightened domain file.
  if (!domain_file.empty()) {
    CpSolverResponse response;
    CHECK_OK(ReadFileToProto(domain_file, &response));
    if (!response.tightened_variables().empty()) {
      CHECK_EQ(response.tightened_variables_size(), cp_model->variables_size())
          << "The tighened variables from the response proto is not "
             "compatible with the model proto";

      for (int i = 0; i < cp_model->variables_size(); ++i) {
        IntegerVariableProto* var_proto = cp_model->mutable_variables(i);
        const Domain tightened_domain =
            ReadDomainFromProto(response.tightened_variables(i));
        const Domain new_domain =
            ReadDomainFromProto(*var_proto).IntersectionWith(tightened_domain);
        FillDomainInProto(new_domain, var_proto);
      }
    } else {
      LOG(INFO) << "The response proto has no tightened variable domains, "
                   "ignoring.";
    }
  }

  // Set the name if not present.
  if (cp_model->name().empty()) {
    cp_model->set_name(ExtractName(filename));
  }
  return true;
}

int Run() {
  Model model;
  SatParameters parameters;
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  // Parse the --params flag.
  parameters.set_log_search_progress(true);
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  // Read the problem.
  google::protobuf::Arena arena;
  CpModelProto* cp_model =
      google::protobuf::Arena::Create<CpModelProto>(&arena);
  std::shared_ptr<LastSolutionPrinter> last_solution_printer;
  if (absl::GetFlag(FLAGS_competition_mode)) {
    last_solution_printer = std::make_shared<LastSolutionPrinter>();
  }
  if (!LoadProblem(absl::GetFlag(FLAGS_input), absl::GetFlag(FLAGS_hint_file),
                   absl::GetFlag(FLAGS_domain_file), cp_model, &model,
                   &parameters, last_solution_printer)) {
    if (!absl::GetFlag(FLAGS_competition_mode)) {
      LOG(FATAL) << "Cannot load file '" << absl::GetFlag(FLAGS_input) << "'.";
    }
    return EXIT_SUCCESS;
  }

  model.Add(NewSatParameters(parameters));
  if (absl::GetFlag(FLAGS_fingerprint_intermediate_solutions)) {
    // Let's add a solution callback that will display the fingerprint of all
    // solutions.
    model.Add(NewFeasibleSolutionLogCallback([](const CpSolverResponse& r) {
      return absl::StrFormat(
          "fingerprint: %#x",
          FingerprintRepeatedField(r.solution(), kDefaultFingerprintSeed));
    }));
  }

  if (absl::GetFlag(FLAGS_competition_mode)) {
    model.GetOrCreate<SigtermHandler>()->Register([last_solution_printer]() {
      last_solution_printer->MaybePrintLastSolution();
      exit(EXIT_SUCCESS);
    });
  }

  const CpSolverResponse response = SolveCpModel(*cp_model, &model);

  if (!absl::GetFlag(FLAGS_output).empty()) {
    if (absl::EndsWith(absl::GetFlag(FLAGS_output), "txt")) {
      CHECK_OK(file::SetTextProto(absl::GetFlag(FLAGS_output), response,
                                  file::Defaults()));
    } else {
      CHECK_OK(file::SetBinaryProto(absl::GetFlag(FLAGS_output), response,
                                    file::Defaults()));
    }
  }

  // The SAT competition requires a particular exit code and since we don't
  // really use it for any other purpose, we comply.
  if (response.status() == CpSolverStatus::OPTIMAL) return 10;
  if (response.status() == CpSolverStatus::FEASIBLE) return 10;
  if (response.status() == CpSolverStatus::INFEASIBLE) return 20;
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program solves a given problem with the CP-SAT solver.";

int main(int argc, char** argv) {
  absl::InitializeLog();
  absl::SetProgramUsageMessage(kUsage);
  absl::ParseCommandLine(argc, argv);
  return operations_research::sat::Run();
}
