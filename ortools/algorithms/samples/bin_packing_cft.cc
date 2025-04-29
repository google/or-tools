#include <absl/log/initialize.h>

#include <cstdlib>

#include "ortools/algorithms/bin_packing.h"
#include "ortools/base/init_google.h"
#include "ortools/set_cover/set_cover_cft.h"

using namespace operations_research;
ABSL_FLAG(std::string, instance, "", "BPP instance int RAIL format.");
ABSL_FLAG(int, bins, 1000, "Number of bins to generate.");

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  BinPackingModel model = ReadBpp(absl::GetFlag(FLAGS_instance));

  BinPackingSetCoverModel scp_model =
      GenerateBins(model, absl::GetFlag(FLAGS_bins));

  scp::PrimalDualState result = scp::RunCftHeuristic(scp_model);
  auto [solution, dual] = result;
  if (solution.subsets().empty()) {
    std::cerr << "Error: failed to find any solution\n";
  } else {
    std::cout << "Solution:         " << solution.cost() << '\n';
  }

  if (dual.multipliers().empty()) {
    std::cerr << "Error: failed to find any dual\n";
  } else {
    std::cout << "Core Lower bound: " << dual.lower_bound() << '\n';
  }

  // The lower bound computed on the full model is not a real lower bound unless
  // the knapsack subproblem failed to fined any negative reduced cost bin to
  // add to the set cover model.
  // TODO(anyone): add a flag to indicate if a valid LB has been found or not.
  if (scp_model.best_dual_state().multipliers().empty()) {
    std::cerr << "Error: no real dual state has been computed\n";
  } else {
    std::cout << "Restricted Lower bound: "
              << scp_model.best_dual_state().lower_bound() << '\n';
  }

  return EXIT_SUCCESS;
}