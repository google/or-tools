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
// The optional_interval_sample_sat command is an example of an Interval variable that is
// marked as optional.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

const horizon = 100

func optionalIntervalSampleSat() error {
	model := cpmodel.NewCpModelBuilder()
	domain := cpmodel.NewDomain(0, horizon)

	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVar(2, 4).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")
	presenceVar := model.NewBoolVar().WithName("presence")

	// An optional interval can be created from three affine expressions and a BoolVar.
	intervalVar := model.NewOptionalIntervalVar(x, y, cpmodel.NewConstant(2).Add(z), presenceVar).WithName("interval")

	// If the size is fixed, a simpler version uses the start expression and the size.
	fixedSizeIntervalVar := model.NewOptionalFixedSizeIntervalVar(x, 10, presenceVar).WithName("fixedSizeInterval")

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	fmt.Printf("%v\n", m.GetConstraints()[intervalVar.Index()])
	fmt.Printf("%v\n", m.GetConstraints()[fixedSizeIntervalVar.Index()])

	return nil
}

func main() {
	if err := optionalIntervalSampleSat(); err != nil {
		log.Exitf("optionalIntervalSampleSat returned with error: %v", err)
	}
}

// [END program]
