#include "ortools/base/commandlineflags.h"

// We add this method to tell the linker to include these symbols.
void CommandLineFlagsUnusedMethod() {
  const char kUsage[] = "Unused";
  absl::SetProgramUsageMessage(kUsage);
  int argc = 0;
  char** argv = nullptr;
  absl::ParseCommandLine(argc, argv);
}
