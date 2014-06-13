// This is the python SWIG wrapper for ../tuple_set.h. See that file.
// TODO(user): Refactor this file to comply with the SWIG style guide.

%include "base/base.swig"

// This .swig include provides a lot of typemap conversion tools.
// TODO(user): see if we really need it.
%include util/python/data.swig

%{
// TODO(user): see if we can remove <vector>
#include <vector>
#include "base/integral_types.h"
#include "util/tuple_set.h"
%}

// TODO(user): use a whitelist approach to expose the API,
// instead of exposing everything.
%include util/tuple_set.h
