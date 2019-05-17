package sat

import "fmt"

/**
 * The negation of a boolean variable. This class should not be used directly, Literal must be used
 * instead.
 */
type notBoolVar struct {
	boolVar *intVar
}

func newNotBoolVar(boolVar *intVar) *notBoolVar {
	return &notBoolVar{boolVar: boolVar}
}

/** Internal: returns the index in the literal in the underlying CpModelProto. */
func (n *notBoolVar) Index() int {
	return -n.Index() - 1
}

/** Returns the negation of this literal. */
func (n *notBoolVar) Not() Literal {
	return n.boolVar
}

/** Returns a short string describing this literal. */
func (n *notBoolVar) ShortString() string {
	return fmt.Sprintf("not(%s)", n.boolVar.ShortString())
}
