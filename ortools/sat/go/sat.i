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

%include "stdint.i"

%include "ortools/base/base.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%go_import("github.com/golang/protobuf/proto")

%module(directors="1") sat_wrapper

%define PROTO_INPUT(CppProtoType, GoProtoType, param_name)
%typemap(gotype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "GoProtoType"
%typemap(imtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "[]byte"
%typemap(goin) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT {
	// hello go
	bytes, err := proto.Marshal(&$input)
	if err != nil {
		panic(err)
	}
	$result = bytes
}
%typemap(in) PROTO_TYPE* INPUT (CppProtoType temp), PROTO_TYPE& INPUT (CppProtoType temp) {
  // hello c
	bool parsed_ok = temp.ParseFromArray($input.array, $input.len);
	if (!parsed_ok) {
		_swig_gopanic("Unable to parse CppProtoType protocol message.");
	}
	$1 = &temp;
}

%apply PROTO_TYPE& INPUT { const CppProtoType& param_name }
%apply PROTO_TYPE& INPUT { CppProtoType& param_name }
%apply PROTO_TYPE* INPUT { const CppProtoType* param_name }
%apply PROTO_TYPE* INPUT { CppProtoType* param_name }

%enddef // PROTO_INPUT

PROTO_INPUT(operations_research::sat::CpModelProto, CpModelProto, model_proto);

%ignoreall

%unignore operations_research;
%unignore operations_research::sat;

// Wrap the relevant part of the SatHelper.
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::ValidateModel;

%include "ortools/sat/swig_helper.h"

%unignoreall
