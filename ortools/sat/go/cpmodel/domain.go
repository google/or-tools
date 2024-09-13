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

package cpmodel

import (
	"fmt"
	"math"
	"sort"
)

// ClosedInterval stores the closed interval `[start,end]`. If the `Start` is greater
// than the `End`, the interval is considered empty.
type ClosedInterval struct {
	Start int64
	End   int64
}

// checkOverflowAndAdd first checks if adding `delta` to `i` will cause an integer overflow.
// It will return the value of the summation if there is no overflow. Otherwise, it will
// return MaxInt64 or MinInt64 depending on the direction of the overflow.
func checkOverflowAndAdd(i, delta int64) int64 {
	if i == math.MinInt64 || i == math.MaxInt64 {
		return i
	}

	s := i + delta
	if delta < 0 && s > i {
		return math.MinInt64
	}
	if delta > 0 && s < i {
		return math.MaxInt64
	}

	return s
}

// Offset adds an offset to both the `Start` and `End` of the ClosedInterval `c`. If the `Start`
// is equal to MinInt or if `End` is equal to MaxInt, the offset does not get added since those
// values represent an unbounded domain. Both `Start` and `End` are clamped at math.MinInt64 and
// Math.MaxInt64.
func (c ClosedInterval) Offset(delta int64) ClosedInterval {
	return ClosedInterval{checkOverflowAndAdd(c.Start, delta), checkOverflowAndAdd(c.End, delta)}
}

// Domain stores an ordered list of ClosedIntervals. This represents any subset
// of `[MinInt64,MaxInt64]`. This type can be used to represent such a set efficiently
// as a sorted and non-adjacent list of intervals. This is efficient as long as the
// size of such a list stays reasonable.
type Domain struct {
	intervals []ClosedInterval
}

// joinIntervals sorts the intervals in domain and combines two consecutive intervals
// if they overlap or the start of the second is exactly one more than the end of the first.
// If an interval's `start` is greater than its `end`, then the interval is considered empty.
func (d *Domain) joinIntervals() {
	var itvs []ClosedInterval
	for _, v := range d.intervals {
		if v.Start <= v.End {
			itvs = append(itvs, v)
		}
	}
	d.intervals = itvs
	if len(d.intervals) == 0 {
		return
	}
	sort.Slice(d.intervals, func(i, j int) bool {
		if d.intervals[i].Start != d.intervals[j].Start {
			return d.intervals[i].Start < d.intervals[j].Start
		}
		return d.intervals[i].End < d.intervals[j].End
	})
	newIntervals := []ClosedInterval{d.intervals[0]}
	for i := 1; i < len(d.intervals); i++ {
		lastInt := &newIntervals[len(newIntervals)-1]
		if lastInt.End+1 >= d.intervals[i].Start {
			if lastInt.End < d.intervals[i].End {
				lastInt.End = d.intervals[i].End
			}
		} else {
			newIntervals = append(newIntervals, d.intervals[i])
		}
	}
	d.intervals = newIntervals
}

// NewEmptyDomain creates an empty Domain.
func NewEmptyDomain() Domain {
	return Domain{}
}

// NewSingleDomain creates a new singleton domain `[val]`.
func NewSingleDomain(val int64) Domain {
	return Domain{[]ClosedInterval{{val, val}}}
}

// NewDomain creates a new domain of a single interval `[left,right]`.
// If `left > right`, an empty domain is returned.
func NewDomain(left, right int64) Domain {
	if left > right {
		return NewEmptyDomain()
	}
	return Domain{[]ClosedInterval{{left, right}}}
}

// FromValues creates a new domain from `values`. `values` need not be
// sorted and can repeat.
func FromValues(values []int64) Domain {
	var d Domain
	for _, v := range values {
		d.intervals = append(d.intervals, ClosedInterval{v, v})
	}
	d.joinIntervals()
	return d
}

// FromIntervals creates a domain from the union of the set of unordered `intervals`.
// If an interval's `start` is greater than its `end`, the interval is considered empty.
func FromIntervals(intervals []ClosedInterval) Domain {
	itvs := make([]ClosedInterval, len(intervals))
	copy(itvs, intervals)
	domain := Domain{itvs}
	domain.joinIntervals()
	return domain
}

// FromFlatIntervals creates a new domain from a flattened list of intervals. If there is an
// interval where the start is greater than the end, the interval is considered empty. Returns
// an error if the length of `values` is not even.
func FromFlatIntervals(values []int64) (Domain, error) {
	if len(values) == 0 {
		return NewEmptyDomain(), nil
	}
	if len(values)%2 != 0 {
		return NewEmptyDomain(), fmt.Errorf("len(values)=%v must be a multiple of 2", len(values))
	}
	var intervals []ClosedInterval
	for i := 1; i < len(values); i += 2 {
		intervals = append(intervals, ClosedInterval{values[i-1], values[i]})
	}
	d := Domain{intervals}
	d.joinIntervals()
	return d, nil
}

// FlattenedIntervals returns the flattened list of interval bounds of the domain.
// For example, if Domain d is equal to `[0,2][5,5][9,10]` will return `[0,2,5,5,9,10]`.
func (d Domain) FlattenedIntervals() []int64 {
	var result []int64
	for _, i := range d.intervals {
		result = append(result, i.Start, i.End)
	}
	return result
}

// Min returns the minimum value of the domain, and returns false if no minimum exists,
// i.e. if the domain is empty.
func (d Domain) Min() (int64, bool) {
	if len(d.intervals) == 0 {
		return 0, false
	}
	return d.intervals[0].Start, true
}

// Max returns the maximum value of the domain, and returns false if no maximum exists,
// i.e. if the domain is empty.
func (d Domain) Max() (int64, bool) {
	if len(d.intervals) == 0 {
		return 0, false
	}
	return d.intervals[len(d.intervals)-1].End, true
}
