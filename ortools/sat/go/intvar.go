package sat

import (
	"fmt"
	"github.com/google/or-tools/ortools/gen/ortools/sat"
)

/** An integer variable. */
type intVar struct {
	modelProto *gen.CpModelProto
	varIndex   int
	VarProto   *gen.IntegerVariableProto
	negation   *notBoolVar
}

func newIntVarLowerUpperBounds(modelProto *gen.CpModelProto, lb int64, ub int64, name string) *intVar {
	return newIntVarBounds(modelProto, []int64{lb, ub}, name)
}

func newIntVarBounds(modelProto *gen.CpModelProto, bounds []int64, name string) *intVar {
	varProto := gen.IntegerVariableProto{
		Name:   name,
		Domain: bounds,
	}

	intVar := &intVar{
		modelProto: modelProto,
		varIndex:   len(modelProto.GetVariables()),
		VarProto:   &varProto,
	}

	modelProto.Variables = append(modelProto.Variables, intVar.VarProto)

	return intVar
}

/** Returns the name of the variable given upon creation. */
func (i *intVar) Name() string {
	return i.VarProto.GetName()
}

/** Internal, returns the index of the variable in the underlying CpModelProto. */
func (i *intVar) Index() int {
	return i.varIndex
}

/** Returns the negation of a boolean variable. */
func (i *intVar) Not() Literal {
	if i.negation == nil {
		i.negation = newNotBoolVar(i)
	}
	return i.negation
}

/** Returns a short string describing the variable. */
func (i *intVar) ShortString() string {
	varProto := i.VarProto
	if len(varProto.GetName()) == 0 {
		domain := varProto.GetDomain()
		if len(domain) == 2 && domain[0] == domain[1] {
			return fmt.Sprintf("%d", domain[0])
		} else {
			return fmt.Sprintf("var_%d(%s)", i.Index(), i.DisplayBounds())
		}
	} else {
		return fmt.Sprintf("%s(%s)", i.Name(), i.DisplayBounds())
	}
}

/** Returns the domain as a string without the enclosing []. */
func (i *intVar) DisplayBounds() string {
	out := ""
	varProto := i.VarProto
	domain := varProto.GetDomain()
	for n := 0; n < len(domain); n += 2 {
		if n != 0 {
			out += ", "
		}
		if domain[n] == domain[n+1] {
			out += fmt.Sprintf("%d", domain[n])
		} else {
			out += fmt.Sprintf("%d..%d", domain[n], domain[n+1])
		}
	}
	return out
}
