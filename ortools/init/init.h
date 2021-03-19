#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "ortools/base/logging.h"
#include "ortools/gurobi/environment.h"


namespace operations_research {

// This class performs various C++ initialization.
// It is meant to be used once at the start of a program.
class Init {
 public:
   // Initialize the C++ logging layer.
   // If logtostderr is false, all C++ logging will be ignored.
   // If true, all logging will the displayed on stderr.
   static void InitCppLogging(const std::string& program_name, bool logtostderr) {
     absl::SetFlag(&FLAGS_logtostderr, logtostderr);
     google::InitGoogleLogging(program_name.c_str());
   }

  // Load the gurobi shared library.
  // This is necessary if the library is installed in a non canonical
  // directory, or if for any reason, it is not found.
  // You need to pass the full path, including the shared library file.
   static void LoadGurobiSharedLibrary(const std::string& full_library_path) {
     LoadGurobiDynamicLibrary({full_library_path});
   }
};

}  // namespace operations_research