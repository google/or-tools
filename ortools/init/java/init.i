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
%unignore operations_research::CppFlags::cp_model_dump_prefix;
%unignore operations_research::CppFlags::cp_model_dump_models;
%unignore operations_research::CppFlags::cp_model_dump_lns;
%unignore operations_research::CppFlags::cp_model_dump_response;

// Expose the static methods of the bridge class.
%unignore operations_research::CppBridge;
%rename (initLogging) operations_research::CppBridge::InitLogging;
%rename (shutdownLogging) operations_research::CppBridge::ShutdownLogging;
%rename (setFlags) operations_research::CppBridge::SetFlags;
%rename (logGurobiSharedLibrary) operations_research::CppBridge::LoadGurobiSharedLibrary;

%include "ortools/init/init.h"

%unignoreall