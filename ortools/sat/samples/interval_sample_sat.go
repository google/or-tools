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

// [START program]
// The interval_sample_sat_go command is a simple example of the Interval variable.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

const horizon = 100

func intervalSampleSat() error {
	model := cpmodel.NewCpModelBuilder()
	domain := cpmodel.NewDomain(0, horizon)

	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVar(2, 4).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	// An interval can be created from three affine expressions.
	intervalVar := model.NewIntervalVar(x, y, cpmodel.NewConstant(2).Add(z)).WithName("interval")

	// If the size is fixed, a simpler version uses the start expression and the size.
	fixedSizeIntervalVar := model.NewFixedSizeIntervalVar(x, 10).WithName("fixedSizeInterval")

	// A fixed interval can be created using the same API.
	fixedIntervalVar := model.NewFixedSizeIntervalVar(cpmodel.NewConstant(5), 10).WithName("fixedInterval")

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	fmt.Printf("%v\n", m.GetConstraints()[intervalVar.Index()])
	fmt.Printf("%v\n", m.GetConstraints()[fixedSizeIntervalVar.Index()])
	fmt.Printf("%v\n", m.GetConstraints()[fixedIntervalVar.Index()])

	return nil
}

func main() {
	if err := intervalSampleSat(); err != nil {
		log.Exitf("intervalSampleSat returned with error: %v", err)
	}
}

// [END program]
