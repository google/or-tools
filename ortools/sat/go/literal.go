package sat

/** Interface to describe a boolean variable or its negation. */
type Literal interface {
	Index() int

	/** Returns the Boolean negation of the current literal. */
	Not() Literal

	/** Returns a short string to describe the literal. */
	ShortString() string
}
