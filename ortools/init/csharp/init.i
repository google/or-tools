%include "ortools/base/base.i"

%include "std_string.i"

%{
#include "ortools/init/init.h"
%}

%ignoreall

%unignore operations_research;

// Expose the flags structure.
%unignore operations_research::CppFlags;
%unignore operations_research::CppFlags::logtostderr;
%unignore operations_research::CppFlags::log_prefix;

// Expose the static methods of the bridge class.
%unignore operations_research::CppBridge;
%unignore operations_research::CppBridge::InitLogging;
%unignore operations_research::CppBridge::ShutdownLogging;
%unignore operations_research::CppBridge::SetFlags;
%unignore operations_research::CppBridge::LoadGurobiSharedLibrary;

%include "ortools/init/init.h"

%unignoreall