// This is the java SWIG wrapper for ../tuple_set.h. See that file.
// TODO(user): Refactor this file to comply with the SWIG style guide.

%include "base/base.swig"

// This .swig include provides a lot of typemap conversion tools.
// TODO(user): see if we really need it.
%include util/java/data.swig

%{
// TODO(user): see if we can remove <vector>
#include <vector>
#include "base/integral_types.h"
#include "util/tuple_set.h"
%}

%rename (insert) operations_research::IntTupleSet::Insert;
%rename (insert2) operations_research::IntTupleSet::Insert2;
%rename (insert3) operations_research::IntTupleSet::Insert3;
%rename (insert4) operations_research::IntTupleSet::Insert4;
%rename (insertAll) operations_research::IntTupleSet::InsertAll;
%rename (contains) operations_research::IntTupleSet::Contains;
%rename (numTuples) operations_research::IntTupleSet::NumTuples;
%rename (value) operations_research::IntTupleSet::Value;
%rename (arity) operations_research::IntTupleSet::Arity;
%rename (clear) operations_research::IntTupleSet::Clear;
%rename (rawData) operations_research::IntTupleSet::RawData;

%include util/tuple_set.h
