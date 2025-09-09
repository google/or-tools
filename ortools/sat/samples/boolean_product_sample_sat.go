// Copyright 2010-2025 Google LLC
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

// The boolean_product_sample_sat command is a simple example of the product of two literals.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	"google.golang.org/protobuf/proto"

	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

func booleanProductSample() error {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar().WithName("x")
	y := model.NewBoolVar().WithName("y")
	p := model.NewBoolVar().WithName("p")

	// x and y implies p, rewrite as not(x and y) or p.
	model.AddBoolOr(x.Not(), y.Not(), p)

	// p implies x and y, expanded into two implications.
	model.AddImplication(p, x)
	model.AddImplication(p, y)

	// Solve.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	// Set `fill_additional_solutions_in_response` and `enumerate_all_solutions` to true so
	// the solver returns all solutions found.
	params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(4),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		fmt.Printf("x: %v", additionalSolution.GetValues()[x.Index()])
		fmt.Printf(" y: %v", additionalSolution.GetValues()[y.Index()])
		fmt.Printf(" p: %v\n", additionalSolution.GetValues()[p.Index()])
	}

	return nil
}

func main() {
	err := booleanProductSample()
	if err != nil {
		log.Exitf("booleanProductSample returned with error: %v", err)
	}
}
