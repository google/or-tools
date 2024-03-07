// Copyright 2010-2024 Google LLC
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

// The search_for_all_solutions_sample_sat command is an example for how to search for
// all solutions.
package main

import (
	"fmt"

	"github.com/golang/glog"
	"golang/protobuf/v2/proto/proto"
	"ortools/sat/go/cpmodel"
	sppb "ortools/sat/sat_parameters_go_proto"
)

func searchForAllSolutionsSampleSat() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 2)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	model.AddNotEqual(x, y)

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	// Currently, the CpModelBuilder does not allow for callbacks, so each feasible solution cannot
	// be printed while solving. However, the CP Solver can return all of the enumerated solutions
	// in the response by setting the following parameters.
	params := sppb.SatParameters_builder{
		EnumerateAllSolutions:             proto.Bool(true),
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(27),
	}.Build()
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	for i, solution := range response.GetAdditionalSolutions() {
		vs := solution.GetValues()
		fmt.Printf("Solution %v: x = %v, y = %v, z = %v\n", i, vs[x.Index()], vs[y.Index()], vs[z.Index()])
	}

	fmt.Println("Number of solutions found: ", len(response.GetAdditionalSolutions()))

	return nil
}

func main() {
	if err := searchForAllSolutionsSampleSat(); err != nil {
		glog.Exitf("searchForAllSolutionsSampleSat returned with error: %v", err)
	}
}
