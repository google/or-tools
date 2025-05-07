#include <absl/log/initialize.h>
#include <cstdlib>
#include <iostream>

#include "ortools/base/init_google.h"
#include "ortools/set_cover/set_cover_cft.h"
#include "ortools/set_cover/set_cover_reader.h"

using namespace operations_research;
ABSL_FLAG(std::string, instance, "", "SCP instance int RAIL format.");

#define DO_PRICING
int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  scp::Model original_model = ReadOrlibRail(absl::GetFlag(FLAGS_instance));

#ifdef DO_PRICING
  scp::FullToCoreModel model(&original_model);
#else
  scp::SubModel model(&original_model);
#endif

  scp::PrimalDualState result = scp::RunCftHeuristic(model);
  auto [solution, dual] = result;
  if (solution.subsets().empty()) {
    std::cerr << "Error: failed to find any solution\n";
  } else {
    std::cout << "Solution:         " << solution.cost() << '\n';
  }

#ifdef DO_PRICING
  if (dual.multipliers().empty()) {
    std::cerr << "Error: failed to find any dual\n";
  } else {
    std::cout << "Core Lower bound: " << dual.lower_bound() << '\n';
  }
  if (model.best_dual_state().multipliers().empty()) {
    std::cerr << "Error: no real dual state has been computed\n";
  } else {
    std::cout << "Full Lower bound: " << model.best_dual_state().lower_bound()
              << '\n';
  }
#else
  if (dual.multipliers().empty()) {
    std::cerr << "Error: failed to find any dual\n";
  } else {
    std::cout << "Lower bound: " << dual.lower_bound() << '\n';
  }
#endif

  return EXIT_SUCCESS;
}