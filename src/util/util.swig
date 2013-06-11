// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

%include util/data.swig

%{
#include <vector>
#include "base/integral_types.h"
#include "util/tuple_set.h"
%}

#if defined(SWIGJAVA)
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
#endif  // SWIGJAVA

%include util/tuple_set.h
