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
	"math"
	"strings"
	"testing"

	"github.com/google/go-cmp/cmp"
)

func TestDomain_NewEmptyDomain(t *testing.T) {
	got := NewEmptyDomain()
	want := Domain{}

	if diff := cmp.Diff(want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
		t.Errorf("NewEmptyDomain() returned with unexpected diff (-want+got);\n%s", diff)
	}
}

func TestDomain_NewSingleDomain(t *testing.T) {
	got := NewSingleDomain(-1)
	want := Domain{[]ClosedInterval{{-1, -1}}}

	if diff := cmp.Diff(want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
		t.Errorf("NewSingleDomain(-1) returned with unexpected diff (-want+got);\n%s", diff)
	}
}

func TestDomain_NewDomain(t *testing.T) {
	testCases := []struct {
		left  int64
		right int64
		want  Domain
	}{
		{
			left:  -5,
			right: 10,
			want:  Domain{[]ClosedInterval{{-5, 10}}},
		},
		{
			left:  10,
			right: -1,
			want:  Domain{},
		},
	}

	for _, test := range testCases {
		got := NewDomain(test.left, test.right)
		if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
			t.Errorf("NewDomain(%v, %v) returned with unexpected diff (-want+got);\n%s", test.left, test.right, diff)
		}
	}
}

func TestDomain_FromValues(t *testing.T) {
	testCases := []struct {
		values []int64
		want   Domain
	}{
		{
			values: []int64{},
			want:   Domain{},
		},
		{
			values: []int64{4},
			want:   Domain{[]ClosedInterval{{4, 4}}},
		},
		{
			values: []int64{1, 1, 3, 1, 2, 3, 2, 3},
			want:   Domain{[]ClosedInterval{{1, 3}}},
		},
		{
			values: []int64{1, 2, 3, 7, 8, -4},
			want:   Domain{[]ClosedInterval{{-4, -4}, {1, 3}, {7, 8}}},
		},
		{
			values: []int64{1, 2, 3, 5, 4, 6, 10, 12, 11, 15, 8},
			want:   Domain{[]ClosedInterval{{1, 6}, {8, 8}, {10, 12}, {15, 15}}},
		},
	}

	for _, test := range testCases {
		got := FromValues(test.values)
		if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
			t.Errorf("FromValues(%v) returned with unexpected diff (-want+got);\n%s", test.values, diff)
		}
	}
}

func TestDomain_FromIntervals(t *testing.T) {
	testCases := []struct {
		intervals []ClosedInterval
		want      Domain
	}{
		{
			intervals: []ClosedInterval{{0, 1}, {0, 10}, {-4, -2}},
			want:      Domain{[]ClosedInterval{{-4, -2}, {0, 10}}},
		},
		{
			intervals: []ClosedInterval{{0, 10}, {1, 6}},
			want:      Domain{[]ClosedInterval{{0, 10}}},
		},
		{
			intervals: []ClosedInterval{{0, 10}, {-1, 5}},
			want:      Domain{[]ClosedInterval{{-1, 10}}},
		},
		{
			intervals: []ClosedInterval{{0, 10}, {0, 10}},
			want:      Domain{[]ClosedInterval{{0, 10}}},
		},
		{
			intervals: []ClosedInterval{{0, 10}, {11, 5}},
			want:      Domain{[]ClosedInterval{{0, 10}}},
		},
	}

	for _, test := range testCases {
		got := FromIntervals(test.intervals)
		if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
			t.Errorf("FromIntervals(%v) returned with unexpected diff (-want+got);\n%s", test.intervals, diff)
		}
	}
}

func TestDomain_FromFlatIntervals(t *testing.T) {
	testCases := []struct {
		flatIntervals []int64
		wantDomain    Domain
		wantError     string
	}{
		{
			flatIntervals: []int64{},
			wantDomain:    Domain{},
		},
		{
			flatIntervals: []int64{1},
			wantError:     "must be a multiple of 2",
		},
		{
			flatIntervals: []int64{-1, 1, 3, 3, 5, 10},
			wantDomain:    Domain{[]ClosedInterval{{-1, 1}, {3, 3}, {5, 10}}},
		},
		{
			flatIntervals: []int64{3, 9, 6, 10},
			wantDomain:    Domain{[]ClosedInterval{{3, 10}}},
		},
		{
			flatIntervals: []int64{3, 5, 5, 10},
			wantDomain:    Domain{[]ClosedInterval{{3, 10}}},
		},
		{
			flatIntervals: []int64{5, 3, 4, -1},
			wantDomain:    Domain{},
		},
	}

	for _, test := range testCases {
		got, err := FromFlatIntervals(test.flatIntervals)
		if err != nil && (test.wantError == "" || !strings.Contains(err.Error(), test.wantError)) {
			t.Errorf("FromFlatIntervals(%v) returned with unexpected error %v, want %q substring", test.flatIntervals, err, test.wantError)
		}
		if diff := cmp.Diff(test.wantDomain, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
			t.Errorf("FromFlatIntervals(%v) returned with unexpected diff (-want+got);\n%s", test.flatIntervals, diff)
		}
	}
}

func TestDomain_FlattenedIntervals(t *testing.T) {
	d := Domain{[]ClosedInterval{{-1, 1}, {3, 3}, {5, 10}}}

	got := d.FlattenedIntervals()
	want := []int64{-1, 1, 3, 3, 5, 10}
	if diff := cmp.Diff(want, got); diff != "" {
		t.Errorf("FlattenedIntervals() returned with unexpected diff (-want+got);\n%s", diff)
	}
}

func TestDomain_Min(t *testing.T) {
	d := Domain{[]ClosedInterval{{-1, 1}, {3, 3}, {5, 10}}}

	want := int64(-1)
	if got, ok := d.Min(); got != want || !ok {
		t.Errorf("Min() returned with unexpected value (%v, %v), want (%v, %v)", got, ok, want, true)
	}
}

func TestDomain_MinEmptyDomain(t *testing.T) {
	emptyDomain := NewEmptyDomain()

	if got, ok := emptyDomain.Min(); got != 0 || ok {
		t.Errorf("Min() returned with unexpected value (%v, %v), want (%v, %v)", got, ok, 0, false)
	}
}

func TestDomain_Max(t *testing.T) {
	d := Domain{[]ClosedInterval{{-1, 1}, {3, 3}, {5, 10}}}

	want := int64(10)
	if got, ok := d.Max(); got != want || !ok {
		t.Errorf("Max() returned with unexpected value (%v, %v), want (%v, %v)", got, ok, want, true)
	}
}

func TestDomain_MaxEmptyDomain(t *testing.T) {
	emptyDomain := NewEmptyDomain()

	if got, ok := emptyDomain.Max(); got != 0 || ok {
		t.Errorf("Max() returned with unexpected value (%v, %v), want (%v, %v)", got, ok, 0, false)
	}
}

func TestDomain_Offset(t *testing.T) {
	testCases := []struct {
		interval ClosedInterval
		delta    int64
		want     ClosedInterval
	}{
		{
			interval: ClosedInterval{1, 2},
			delta:    -2,
			want:     ClosedInterval{-1, 0},
		},
		{
			interval: ClosedInterval{math.MinInt64, 2},
			delta:    -2,
			want:     ClosedInterval{math.MinInt64, 0},
		},
		{
			interval: ClosedInterval{1, math.MaxInt64},
			delta:    2,
			want:     ClosedInterval{3, math.MaxInt64},
		},
		{
			interval: ClosedInterval{math.MinInt64, math.MinInt64},
			delta:    -2,
			want:     ClosedInterval{math.MinInt64, math.MinInt64},
		},
		{
			interval: ClosedInterval{math.MaxInt64, math.MaxInt64},
			delta:    2,
			want:     ClosedInterval{math.MaxInt64, math.MaxInt64},
		},
		{
			interval: ClosedInterval{-1, 5},
			delta:    math.MaxInt64,
			want:     ClosedInterval{math.MaxInt64 - 1, math.MaxInt64},
		},
		{
			interval: ClosedInterval{-1, 5},
			delta:    math.MinInt64,
			want:     ClosedInterval{math.MinInt64, math.MinInt64 + 5},
		},
	}

	for _, test := range testCases {
		if got := test.interval.Offset(test.delta); got != test.want {
			t.Errorf("%#v.Offset(%v) return %#v, want %#v", test.interval, test.delta, got, test.want)
		}
	}
}
