#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "ortools/base/logging.h"
#include "ortools/gurobi/environment.h"


namespace operations_research {

struct CppFlags {
  bool logtostderr = false;
  bool log_prefix = false;
};

// This class performs various C++ initialization.
// It is meant to be used once at the start of a program.
class CppBridge {
 public:
   // Initialize the C++ logging layer.
  static void InitLogging(const std::string& program_name) {
    google::InitGoogleLogging(program_name.c_str());
  }

  // Shutdown the C++ logging layer.
  static void ShutdownLogging() {
    google::ShutdownGoogleLogging();
  }
  
  // Sets all the C++ flags contained in the CppFlags structure.
  static void SetFlags(const CppFlags& flags)  {
    absl::SetFlag(&FLAGS_logtostderr, flags.logtostderr);
    absl::SetFlag(&FLAGS_log_prefix, flags.log_prefix);
  }

  // Load the gurobi shared library.
  // This is necessary if the library is installed in a non canonical
  // directory, or if for any reason, it is not found.
  // You need to pass the full path, including the shared library file.
  // It returns true if the library was found and correctly loaded.
   static bool LoadGurobiSharedLibrary(const std::string& full_library_path) {
     return LoadGurobiDynamicLibrary({full_library_path}).ok();
   }
};

}  // namespace operations_research