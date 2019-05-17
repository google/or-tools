package sat

import "github.com/google/or-tools/ortools/gen/ortools/sat"

type cpModel struct {
	proto *gen.CpModelProto
}

func NewCpModel() *cpModel {
	return &cpModel{
		proto: &gen.CpModelProto{},
	}
}

func (m *cpModel) SetName(name string) {
	m.proto.Name = name
}

func (m *cpModel) Name() string {
	return m.proto.Name
}

// Integer variables.

/** Creates an integer variable with domain [lb, ub]. */
func (m *cpModel) NewIntVar(lb int64, ub int64, name string) *intVar {
	return newIntVarLowerUpperBounds(m.proto, lb, ub, name)
}

/** Returns a non empty string explaining the issue if the model is invalid. */
func (m *cpModel) Validate() string {
	return gen.SatHelperValidateModel(*m.proto)
}
