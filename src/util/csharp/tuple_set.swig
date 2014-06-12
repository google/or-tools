// This is the C# SWIG wrapper for ../tuple_set.h. See that file.
// TODO(user): Refactor this file to comply with the SWIG style guide.

%include "base/base.swig"

// This .swig include provides a lot of typemap conversion tools.
// TODO(user): see if we really need it.
%include util/csharp/data.swig

%{
// TODO(user): See if we really need <vector>.
#include <vector>
#include "base/integral_types.h"
#include "util/tuple_set.h"
%}

%include util/tuple_set.h
