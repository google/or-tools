// Copyright 2010-2021 Google LLC
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

// This is the java SWIG wrapper for ../sorted_interval_list.h.  See that file.

%include "ortools/base/base.i"

%include "ortools/util/java/vector.i"

%{
#include <vector>
#include "ortools/base/integral_types.h"
#include "ortools/util/sorted_interval_list.h"
%}

%module operations_research;

%ignoreall

%unignore operations_research;

%unignore operations_research::SortedDisjointIntervalList;
%unignore operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList;
%ignore operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList(const std::vector<ClosedInterval>&);
%unignore operations_research::SortedDisjointIntervalList::~SortedDisjointIntervalList;

%rename (insertInterval) operations_research::SortedDisjointIntervalList::InsertInterval;
%rename (insertIntervals) operations_research::SortedDisjointIntervalList::InsertIntervals;
%rename (numIntervals) operations_research::SortedDisjointIntervalList::NumIntervals;
%rename (buildComplementOnInterval) operations_research::SortedDisjointIntervalList::BuildComplementOnInterval;
%rename (toString) operations_research::SortedDisjointIntervalList::DebugString;

// Make the SWIG-generated constructor public.
// This is necessary as it will be called from the sat package.
SWIG_JAVABODY_PROXY(/*PTRCTOR_VISIBILITY=*/public,
                    /*CPTR_VISIBILITY=*/protected,
                    /*TYPE...=*/SWIGTYPE)

// Wrap the domain class here.
%unignore operations_research::Domain;
%unignore operations_research::Domain::Domain;

%rename (additionWith) operations_research::Domain::AdditionWith;
%rename (allValues) operations_research::Domain::AllValues;
%rename (complement) operations_research::Domain::Complement;
%rename (contains) operations_research::Domain::Contains;
%rename (flattenedIntervals) operations_research::Domain::FlattenedIntervals;
%rename (fromFlatIntervals) operations_research::Domain::FromFlatIntervals;
%rename (fromIntervals) operations_research::Domain::FromVectorIntervals;
%rename (fromValues) operations_research::Domain::FromValues;
%rename (intersectionWith) operations_research::Domain::IntersectionWith;
%rename (isEmpty) operations_research::Domain::IsEmpty;
%rename (max) operations_research::Domain::Max;
%rename (min) operations_research::Domain::Min;
%rename (negation) operations_research::Domain::Negation;
%rename (size) operations_research::Domain::Size;
%rename (toString) operations_research::Domain::ToString;
%rename (unionWith) operations_research::Domain::UnionWith;

%include "ortools/util/sorted_interval_list.h"

%unignoreall
