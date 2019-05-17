package linearsolver

import (
	"fmt"
	"math"
	"reflect"
	"testing"

	"google3/net/proto2/contrib/go/messagediff/messagediff"
	"google3/net/proto2/go/proto"
	"google3/third_party/golang/godebug/pretty/pretty"

	// Imported for its side-effect of adding a dependency on //u/o/linear_solver/go:glop.
	// Equivalently one can directly add that dependency with "# keep" in BUILD.
	_ "google3/util/operations_research/linear_solver/go/glop"
	pb "google3/util/operations_research/linear_solver/linear_solver_go_proto"
)

func approxEq(x, y float64) bool {
	return math.Abs(x-y) < 1e-9
}

func TestLoadAndSolveEmpty(t *testing.T) {
	name := "lp_solver"
	if solver, err := New(name, pb.MPModelRequest_SCIP_MIXED_INTEGER_PROGRAMMING); err == nil {
		Delete(solver)
		t.Errorf("New(SCIP) err = nil, want not supported type error")
	}
	pType := pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING
	solver, err := New(name, pType)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	if name != solver.Name() {
		t.Errorf("Name() = %q, want %q", solver.Name(), name)
	}
	if pType != solver.ProblemType() {
		t.Errorf("ProblemType() = %v, want %v", solver.ProblemType(), pType)
	}
	model := &pb.MPModelProto{}
	if err := solver.LoadModelFromProto(model, false); err != nil {
		t.Fatalf("LoadModelFromProto(model) err = %v, want nil", err)
	}
	solver.EnableOutput()
	if !solver.OutputIsEnabled() {
		t.Errorf("OutputIsEnabled() = false, want true")
	}
	solver.SuppressOutput()
	if solver.OutputIsEnabled() {
		t.Errorf("OutputIsEnabled() = true, want false")
	}
	status := solver.Solve()
	if status != pb.MPSolverResponseStatus_MPSOLVER_OPTIMAL {
		t.Errorf("Solve(model) = %v, want optimal", status)
	}
	sol := solver.Solution()
	expectSol := &pb.MPSolutionResponse{}
	expectSolStr := "status: MPSOLVER_OPTIMAL  objective_value: 0"
	if err := proto.UnmarshalText(expectSolStr, expectSol); err != nil {
		t.Fatalf("Could not parse proto %v: %v", expectSolStr, err)
	}
	if !proto.Equal(sol, expectSol) {
		t.Errorf("Solution() = %v, want %v", sol, expectSol)
	}
}

func TestObjective(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	o := solver.Objective()
	if !o.Minimization() {
		t.Error("Objective Minimization() = false, want true")
	}
	o.SetMaximization()
	if o.Minimization() {
		t.Error("Objective Minimization() = true, want false")
	}
	if 0 != o.Offset() {
		t.Errorf("Objective Offset() = %f, want 0", o.Offset())
	}
	o.SetOffset(2.5)
	if o.Offset() != 2.5 {
		t.Errorf("Objective Offset() = %f, want 2.5", o.Offset())
	}
	o.Clear()
	if o.Offset() != 0 {
		t.Errorf("Objective Offset() = %f, want 0", o.Offset())
	}
	if o.Maximization() {
		t.Error("Objective Maximization() = true, want false")
	}

	x, err := solver.MakeVar(0, 1, false, "x")
	if err != nil {
		t.Errorf("MakeVar(x) err = %v, want nil", err)
	}
	if o.Coefficient(x) != 0 {
		t.Errorf("Variable coefficient = %f, want 0", o.Coefficient(x))
	}
	o.SetCoefficient(x, 5.5)
	if o.Coefficient(x) != 5.5 {
		t.Errorf("Variable coefficient = %f, want 5.5", o.Coefficient(x))
	}
}

func TestVariables(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	v, err := solver.MakeVar(0, 2.5, false, "x")
	if err != nil {
		t.Errorf("MakeVar(x) err = %v, want nil", err)
	}
	if _, err := solver.MakeVar(0, 2.5, false, "x"); err == nil {
		t.Error("MakeVar(x) err = nil, want duplicate var error")
	}

	if v.Name() != "x" {
		t.Errorf("Variable Name() = %q, want %q", v.Name(), "x")
	}
	x := solver.LookupVar("x")
	if x.LB() != 0 {
		t.Errorf("Variable LB() = %f, want 0", x.LB())
	}
	y := solver.LookupVar("y")
	if y != nil {
		t.Error("Variable y != nil, want nil")
	}
	x.SetUB(3.5)
	if x.UB() != 3.5 {
		t.Errorf("Variable UB() = %f, want 3.5", x.UB())
	}
}

func TestConstraint(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	x, err := solver.MakeVar(0, 1, false, "x")
	if err != nil {
		t.Errorf("MakeVar(x) err = %v, want nil", err)
	}
	c, err := solver.MakeConstraint(0.1, 0.9, "c")
	if err != nil {
		t.Errorf("MakeConstraint() err = %v, want nil", err)
	}
	if c.Coefficient(x) != 0 {
		t.Errorf("Variable coefficient = %f, want 0", c.Coefficient(x))
	}
	c.SetCoefficient(x, 1.5)
	if c.Coefficient(x) != 1.5 {
		t.Errorf("Variable coefficient = %f, want 1.5", c.Coefficient(x))
	}
	if c.UB() != 0.9 {
		t.Errorf("Constraint UB = %f, want 0.9", c.UB())
	}
	if c.Is_lazy() {
		t.Error("Constraint Is_lazy() = true, want false")
	}
	c.SetUB(3.5)
	if c.UB() != 3.5 {
		t.Errorf("Constraint UB = %f, want 3.5", c.UB())
	}
	c.Set_is_lazy(true)
	if !c.Is_lazy() {
		t.Error("Constraint Is_lazy() = false, want true")
	}
	if c.Name() != "c" {
		t.Errorf("Constraint Name() = %q, want ", c.Name())
	}
	if c.Index() != 0 {
		t.Errorf("Constraint Index() = %d, want 0", c.Index())
	}
	if _, err := solver.MakeConstraint(0, 0, "c"); err == nil {
		t.Error("MakeConstraint() err = nil, want error")
	}
	c2 := solver.LookupConstraint("c")
	if c2.Index() != 0 {
		t.Errorf("Constraint Index() = %d, want 0", c2.Index())
	}
}

func TestBuildAndSolve(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	x, err := solver.MakeVar(0, 2, false, "x")
	if err != nil {
		t.Errorf("MakeVar() err = %v, want nil", err)
	}
	y, err := solver.MakeVar(0, 1, false, "y")
	if err != nil {
		t.Errorf("MakeVar() err = %v, want nil", err)
	}
	ct, err := solver.MakeConstraint(2.2, math.Inf(1), "ct")
	if err != nil {
		t.Errorf("MakeConstraint() err = %v, want nil", err)
	}
	ct.SetCoefficient(x, 1)
	ct.SetCoefficient(y, 1)

	o := solver.Objective()
	o.SetCoefficient(x, 1)
	status := solver.Solve()
	if status != pb.MPSolverResponseStatus_MPSOLVER_OPTIMAL {
		t.Errorf("Solve(model) = %v, want optimal", status)
	}
	if !approxEq(o.Value(), 1.2) {
		t.Errorf("Objective Value() = %f, want 1.2", o.Value())
	}
	if !approxEq(x.Solution_value(), 1.2) {
		t.Errorf("x.Solution_value() = %f, want 1.2", x.Solution_value())
	}
	if !approxEq(y.Solution_value(), 1) {
		t.Errorf("y.Solution_value() = %f, want 1", y.Solution_value())
	}
	if !approxEq(ct.Dual_value(), 1) {
		t.Errorf("ct.Dual_value() = %f, want 1", ct.Dual_value())
	}
	if x.Basis_status() != BASIC {
		t.Errorf("x.Basis_status() = %d, want BASIC", x.Basis_status())
	}
	if y.Basis_status() != AT_UPPER_BOUND {
		t.Errorf("y.Basis_status() = %d, want AT_UPPER_BOUND", y.Basis_status())
	}
	if ct.Basis_status() != AT_LOWER_BOUND {
		t.Errorf("ct.Basis_status() = %d, want AT_LOWER_BOUND", ct.Basis_status())
	}
}

func TestLoadAndSolve(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	solverVersion := solver.SolverVersion()
	solver.Set_time_limit(12345)
	model := &pb.MPModelProto{}
	modelStr := `
      name: "lp"
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
        is_lazy: false
      }
  `
	if err := proto.UnmarshalText(modelStr, model); err != nil {
		t.Fatalf("Could not parse proto %v: %v", modelStr, err)
	}
	if err := solver.LoadModelFromProto(model, true); err != nil {
		t.Fatalf("LoadModelFromProto(model) err = %v, want nil", err)
	}
	if !proto.Equal(model, solver.Model()) {
		t.Errorf("Model() = %v, want %v", solver.Model(), model)
	}
	if solver.NumVariables() != 2 {
		t.Errorf("NumVariables() = %v, want 2", solver.NumVariables())
	}
	if solver.NumConstraints() != 1 {
		t.Errorf("NumConstraints() = %v, want 1", solver.NumConstraints())
	}

	options := NewExportOptions()
	defer DeleteExportOptions(options)
	options.SetObfuscate(true)
	options.SetShowUnusedVariables(true)
	options.SetLogInvalidNames(false)
	options.SetMaxLineLength(80)
	if modelStr, err := ExportModelAsLpFormat(model, options); err != nil {
		t.Errorf("ExportModelAsLpFormat() err = %v, want nil", err)
	} else if len(modelStr) == 0 {
		t.Error("ExportModelAsLpFormat() = empty string, want non-empty")
	}
	if modelStr, err := ExportModelAsMpsFormat(model, options); err != nil {
		t.Errorf("ExportModelAsMpsFormat() err = %v, want nil", err)
	} else if len(modelStr) == 0 {
		t.Error("ExportModelAsMpsFormat() = empty string, want non-empty")
	}

	status := solver.Solve()
	if status != pb.MPSolverResponseStatus_MPSOLVER_OPTIMAL {
		t.Errorf("Solve(model) = %v, want optimal", status)
	}
	sol := solver.Solution()
	expectSol := &pb.MPSolutionResponse{}
	expectSolStr := `
      status: MPSOLVER_OPTIMAL
      objective_value: 1.2
      variable_value:1.2
      variable_value:1
      dual_value:1
			reduced_cost:0.0
			reduced_cost:-1
  `
	if err := proto.UnmarshalText(expectSolStr, expectSol); err != nil {
		t.Fatalf("Could not parse proto %v: %v", expectSolStr, err)
	}
	if !messagediff.Compare(sol, expectSol, messagediff.Approximate()) {
		t.Errorf("Solution() = %v, want %v", sol, expectSol)
	}
	activities := []float64{2.2}
	if !reflect.DeepEqual(solver.ConstraintActivities(), activities) {
		t.Errorf("ConstraintActivities() = %v, want %v", solver.ConstraintActivities(), activities)
	}
	if !solver.VerifySolution(1e-5, false) {
		t.Errorf("VerifySolution() = false, want true")
	}
	solver.Reset()
	solver.LoadSolutionFromProto(sol)
	if !solver.VerifySolution(1e-5, false) {
		t.Errorf("VerifySolution() = false, want true")
	}
	solver.Clear()
	if solverVersion != solver.SolverVersion() {
		t.Errorf("SolverVersion() = %q, want %q", solver.SolverVersion(), solverVersion)
	}
	if solver.Time_limit() != 12345 {
		t.Errorf("Time_limit() = %d, want 12345", solver.Time_limit())
	}
	if solver.NumVariables() != 0 {
		t.Errorf("NumVariables() = %v, want 0", solver.NumVariables())
	}
	if solver.NumConstraints() != 0 {
		t.Errorf("NumConstraints() = %v, want 0", solver.NumConstraints())
	}
	// Test that Wall_time() works. Since this test is very small, it returns 0.
	if solver.Wall_time() > 60000 {
		t.Errorf("Wall_time() = %d which is too long", solver.Wall_time())
	}
}

func TestSolveWithParameters(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)
	model := &pb.MPModelProto{}
	if err := solver.LoadModelFromProto(model, true); err != nil {
		t.Fatalf("LoadModelFromProto(model) err = %v, want nil", err)
	}

	p := NewParameters()
	defer DeleteParameters(p)
	if pt := p.GetDoubleParam(PRIMAL_TOLERANCE); pt != 1e-7 {
		t.Errorf("default primal tolerance %e, want 1e-7", pt)
	}
	p.SetDoubleParam(PRIMAL_TOLERANCE, 1e-4)
	if pt := p.GetDoubleParam(PRIMAL_TOLERANCE); pt != 1e-4 {
		t.Errorf("new primal tolerance %e, want 1e-4", pt)
	}

	if ps := p.GetIntegerParam(PRESOLVE); ps != PRESOLVE_ON {
		t.Errorf("default presolve %d, want PRESOLVE_ON (1)", ps)
	}
	p.SetIntegerParam(PRESOLVE, PRESOLVE_OFF)
	if ps := p.GetIntegerParam(PRESOLVE); ps != PRESOLVE_OFF {
		t.Errorf("new presolve %d, want PRESOLVE_OFF (0)", ps)
	}

	status := solver.SolveWithParameters(p)
	if status != pb.MPSolverResponseStatus_MPSOLVER_OPTIMAL {
		t.Errorf("Solve(model) = %v, want optimal", status)
	}
}

// getModelHint returns the solution_hint of the input model.
func getModelHint(vars []*Variable, mdl *pb.MPModelProto) ([]VariableHint, error) {
	indices := mdl.GetSolutionHint().GetVarIndex()
	values := mdl.GetSolutionHint().GetVarValue()
	if len(indices) != len(values) {
		return nil, fmt.Errorf("model solution_hint.var_index and solution_hint.var_value have different lengths")
	}
	hint := make([]VariableHint, len(indices))
	for i := range indices {
		hint[i].Variable = vars[indices[i]]
		hint[i].Value = values[i]
	}
	return hint, nil
}

func TestSetHint(t *testing.T) {
	solver, err := New("lp", pb.MPModelRequest_GLOP_LINEAR_PROGRAMMING)
	if err != nil {
		t.Fatalf("New(GLOP) err = %v, want nil", err)
	}
	defer Delete(solver)

	x, err := solver.MakeVar(0, 5, false, "x")
	if err != nil {
		t.Fatalf("MakeVar() err = %v, want nil", err)
	}

	y, err := solver.MakeVar(0, 5, false, "y")
	if err != nil {
		t.Fatalf("MakeVar() err = %v, want nil", err)
	}

	hint := []VariableHint{{x, 2}, {y, 4}}
	solver.SetHint(hint)

	mdl := solver.Model()
	got, err := getModelHint([]*Variable{x, y}, mdl)
	if err != nil {
		t.Fatalf("failed to get hint from MPModelProto: %v", err)
	}
	if diff := pretty.Compare(hint, got); diff != "" {
		t.Errorf("model does not contain the expected hints, diff (-want +got):\n%v", diff)
	}
}
