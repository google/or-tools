// Copyright 2010-2018 Google LLC
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

// This .i file exposes the sat cp_model API.

%include "stdint.i"

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

// std::function utilities.
%include "ortools/util/python/functions.i"
%import "ortools/util/python/vector.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
#include "ortools/util/sorted_interval_list.h"
%}

%pythoncode {
import numbers
}

%module(directors="1") operations_research_sat

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpModelProto,
                 operations_research::sat::CpModelProto);

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpSolverResponse,
                 operations_research::sat::CpSolverResponse);

PY_PROTO_TYPEMAP(ortools.sat.sat_parameters_pb2,
                 SatParameters,
                 operations_research::sat::SatParameters);


%ignoreall

%unignore operations_research;
%unignore operations_research::sat;
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithParameters;
%unignore operations_research::sat::SatHelper::SolveWithParametersAndSolutionCallback;
%unignore operations_research::sat::SatHelper::ModelStats;
%unignore operations_research::sat::SatHelper::SolverResponseStats;
%unignore operations_research::sat::SatHelper::ValidateModel;

%feature("director") operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback::~SolutionCallback;
%unignore operations_research::sat::SolutionCallback::BestObjectiveBound;
%feature("nodirector") operations_research::sat::SolutionCallback::BestObjectiveBound;
%unignore operations_research::sat::SolutionCallback::HasResponse;
%feature("nodirector") operations_research::sat::SolutionCallback::HasResponse;
%unignore operations_research::sat::SolutionCallback::NumBinaryPropagations;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBinaryPropagations;
%unignore operations_research::sat::SolutionCallback::NumBooleans;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBooleans;
%unignore operations_research::sat::SolutionCallback::NumBranches;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBooleans;
%unignore operations_research::sat::SolutionCallback::NumConflicts;
%feature("nodirector") operations_research::sat::SolutionCallback::NumConflicts;
%unignore operations_research::sat::SolutionCallback::NumIntegerPropagations;
%feature("nodirector") operations_research::sat::SolutionCallback::NumIntegerPropagations;
%unignore operations_research::sat::SolutionCallback::ObjectiveValue;
%feature("nodirector") operations_research::sat::SolutionCallback::ObjectiveValue;
%unignore operations_research::sat::SolutionCallback::OnSolutionCallback;
%unignore operations_research::sat::SolutionCallback::Response;
%feature("nodirector") operations_research::sat::SolutionCallback::Response;
%unignore operations_research::sat::SolutionCallback::SolutionBooleanValue;
%feature("nodirector") operations_research::sat::SolutionCallback::SolutionBooleanValue;
%unignore operations_research::sat::SolutionCallback::SolutionIntegerValue;
%feature("nodirector") operations_research::sat::SolutionCallback::SolutionIntegerValue;
%unignore operations_research::sat::SolutionCallback::StopSearch;
%feature("nodirector") operations_research::sat::SolutionCallback::StopSearch;
%unignore operations_research::sat::SolutionCallback::UserTime;
%feature("nodirector") operations_research::sat::SolutionCallback::UserTime;
%unignore operations_research::sat::SolutionCallback::WallTime;
%feature("nodirector") operations_research::sat::SolutionCallback::WallTime;

%unignore operations_research::ClosedInterval;
%unignore operations_research::Domain;
%unignore operations_research::Domain::Domain;
%unignore operations_research::Domain::AllValues;
%unignore operations_research::Domain::FromValues;
%unignore operations_research::Domain::IsEmpty;
%unignore operations_research::Domain::Size;
%unignore operations_research::Domain::Min;
%unignore operations_research::Domain::Max;
%unignore operations_research::Domain::FlattenedIntervals;
%unignore operations_research::Domain::FromFlatIntervals;
%extend operations_research::Domain {
  std::vector<int64> FlattenedIntervals() {
    std::vector<int64> result;
    for (const auto& ci : self->intervals()) {
      result.push_back(ci.start);
      result.push_back(ci.end);
    }
    return result;
  }

  static Domain FromFlatIntervals(const std::vector<int64>& flat_intervals) {
    std::vector<operations_research::ClosedInterval> intervals;
    const int length = flat_intervals.size() / 2;
    for (int i = 0; i < length; ++i) {
      operations_research::ClosedInterval ci;
      ci.start = flat_intervals[2 * i];
      ci.end = flat_intervals[2 * i + 1];
      intervals.push_back(ci);
    }
    return operations_research::Domain::FromIntervals(intervals);
  }

  %pythoncode {
    @staticmethod
    def FromIntervals(intervals):
      flat_intervals = []
      for interval in intervals:
        if isinstance(interval, numbers.Integral):
          flat_intervals.append(interval)
          flat_intervals.append(interval)
        elif len(interval) == 1:
          flat_intervals.append(interval[0])
          flat_intervals.append(interval[0])
        else:
          flat_intervals.append(interval[0])
          flat_intervals.append(interval[1])
      return Domain.FromFlatIntervals(flat_intervals)
  } // %pythoncode
}

%include "ortools/sat/swig_helper.h"
%include "ortools/util/sorted_interval_list.h"

%unignoreall
