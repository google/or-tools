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

// The literal_sample_sat command is a simple example of literals.
package main

import (
	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

func literalSampleSat() {
	model := cpmodel.NewCpModelBuilder()

	x := model.NewBoolVar().WithName("x")
	notX := x.Not()

	log.Infof("x = %d, x.Not() = %d", x.Index(), notX.Index())
}

func main() {
	literalSampleSat()
}
