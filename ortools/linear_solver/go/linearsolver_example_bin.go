// The linearsolver_example_bin is an example Go binary that tests that the code
// builds fine, and also serves as an example of how linearsolver library is
// used.
package main

import (
	"C"
	"fmt"

	"google3/base/go/google"
	// Imported for its side-effect of adding a dependency on //u/o/linear_solver/go:glop.
	// Equivalently one can directly add that dependency with "# keep" in BUILD.
	_ "google3/util/operations_research/linear_solver/go/glop"
	lp "google3/util/operations_research/linear_solver/go/linearsolver"
	pb "google3/util/operations_research/linear_solver/linear_solver_go_proto"
)
import (
	"google3/base/go/log"
	"google3/net/proto2/go/proto"
)

func main() {
	google.Init()

	model := &pb.MPModelProto{}
	modelStr := `
      name: "test"
      maximize: false
      objective_offset: 0
      variable {
        lower_bound: 0
        upper_bound: 2
        objective_coefficient: 1
        is_integer: false
        name: "x"
      }
      variable {
        lower_bound: 0
        upper_bound: 1
        is_integer: false
        name: "y"
      }
      constraint {
        lower_bound: 2.2
        upper_bound: inf
        name: "ct"
        var_index: 0
        var_index: 1
        coefficient: 1
        coefficient: 1
      }
  `
	if err := proto.UnmarshalText(modelStr, model); err != nil {
		log.Exitf("Failed to parse modelStr: %v", err)
	}

	solver, err := lp.New("my_lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		log.Exitf("Failed New() linear solver: %v", err)
	}
	defer lp.Delete(solver)
	if err := solver.LoadModelFromProto(model, false); err != nil {
		log.Exitf("Failed LoadModelFromProto with error: %v", err)
	}
	status := solver.Solve()
	// This should print out:
	// Solve returns: MPSOLVER_OPTIMAL
	fmt.Println("Solve returns: ", status)

	// This should print out:
	// Solution:  status:MPSOLVER_OPTIMAL objective_value:1.2000000000000002 variable_value:1.2000000000000002 variable_value:1 dual_value:1
	fmt.Println("Solution: ", solver.Solution())
}
