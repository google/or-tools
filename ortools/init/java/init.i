%include "ortools/base/base.i"

%include "std_string.i"

%{
#include "ortools/init/init.h"
%}

%ignoreall

%unignore operations_research;

%unignore operations_research::Init;
%rename (initCppLogging) operations_research::Init::InitCppLogging;
%rename (logGurobiSharedLibrary) operations_research::Init::LoadGurobiSharedLibrary;

%include "ortools/init/init.h"

%unignoreall