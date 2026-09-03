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

package statusstreaming

import (
	"errors"
	"fmt"
	"testing"

	spb "ortools/util/status_go_proto"
)

func TestErrorCode(t *testing.T) {
	var tests = []struct {
		name   string
		err    error
		want   int32 // The expected value for the first returned value.
		wantOK bool  // The expected value for the second returned value; NOT related to absl::OkStatus()!
	}{
		{name: "ErrCancelled", err: ErrCancelled, want: cancelledCode.value(), wantOK: true},
		{
			name: "ErrCancelled_wrapped", err: fmt.Errorf("%w: some message", ErrCancelled),
			want: cancelledCode.value(), wantOK: true,
		},
		{
			name: "ErrCancelled_wrapped_twice",
			err:  fmt.Errorf("oops: %w", fmt.Errorf("%w: some message", ErrCancelled)),
			want: cancelledCode.value(), wantOK: true,
		},
		{name: "unexpected_code", err: statusError(-5), want: -5, wantOK: true},
		{
			name: "from_to_error", err: ToError(&spb.StatusProto{
				Code:    cancelledCode.value(),
				Message: "some message",
			}),
			want: cancelledCode.value(), wantOK: true,
		},
		{name: "non_status_error", err: fmt.Errorf("some message"), want: 0, wantOK: false},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, ok := ErrorCode(test.err)
			if got != test.want || ok != test.wantOK {
				t.Errorf("statusstreaming.ErrorCode(%#v) = (%v, %v), want (%v, %v)", test.err, got, ok, test.want, test.wantOK)
			}
		})
	}
}

func TestStatusError(t *testing.T) {
	var tests = []struct {
		name string
		err  error
		want string
	}{
		{name: "cancelled", err: ErrCancelled, want: "CANCELLED"},
		{name: "unknown", err: ErrUnknown, want: "UNKNOWN"},
		{name: "invalid_argument", err: ErrInvalidArgument, want: "INVALID_ARGUMENT"},
		{name: "deadline_exceeded", err: ErrDeadlineExceeded, want: "DEADLINE_EXCEEDED"},
		{name: "not_found", err: ErrNotFound, want: "NOT_FOUND"},
		{name: "already_exists", err: ErrAlreadyExists, want: "ALREADY_EXISTS"},
		{name: "permission_denied", err: ErrPermissionDenied, want: "PERMISSION_DENIED"},
		{name: "unauthenticated", err: ErrUnauthenticated, want: "UNAUTHENTICATED"},
		{name: "resource_exhausted", err: ErrResourceExhausted, want: "RESOURCE_EXHAUSTED"},
		{name: "failed_precondition", err: ErrFailedPrecondition, want: "FAILED_PRECONDITION"},
		{name: "aborted", err: ErrAborted, want: "ABORTED"},
		{name: "out_of_range", err: ErrOutOfRange, want: "OUT_OF_RANGE"},
		{name: "unimplemented", err: ErrUnimplemented, want: "UNIMPLEMENTED"},
		{name: "internal", err: ErrInternal, want: "INTERNAL"},
		{name: "unavailable", err: ErrUnavailable, want: "UNAVAILABLE"},
		{name: "data_loss", err: ErrDataLoss, want: "DATA_LOSS"},
		{name: "unexpected_code", err: statusError(-3), want: "ERROR(-3)"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got := test.err.Error()
			if got != test.want {
				t.Errorf("(%#v).Error() = %q, want %q", test.err, got, test.want)
			}
		})
	}
}

func TestToError(t *testing.T) {
	var tests = []struct {
		name        string
		status      *spb.StatusProto
		wantNil     bool
		wantCode    int32
		wantMessage string
		// If not nil; test that errors.Is() returns true.
		wantIs error
	}{
		{name: "nil", status: nil, wantNil: true},
		{name: "ok", status: &spb.StatusProto{Code: okCode.value(), Message: "ok"}, wantNil: true},
		{
			name:     "not_found",
			status:   &spb.StatusProto{Code: notFoundCode.value(), Message: "oops"},
			wantCode: notFoundCode.value(), wantMessage: "NOT_FOUND: oops",
			wantIs: ErrNotFound,
		},
		{
			name:     "unexpected_code",
			status:   &spb.StatusProto{Code: -5, Message: "oops"},
			wantCode: -5, wantMessage: "ERROR(-5): oops",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got := ToError(test.status)
			if test.wantNil {
				if got != nil {
					t.Errorf("statusstreaming.ToError(%+v) = %#v, want nil", test.status, got)
				}
				return
			}
			if got == nil {
				t.Errorf("statusstreaming.ToError(%+v) = nil, want (%d, %q)", test.status, test.wantCode, test.wantMessage)
			}
			if test.wantIs != nil && !errors.Is(got, test.wantIs) {
				t.Errorf("statusstreaming.ToError(%+v) = %#v, for which errors.Is(_, %#v) is false", test.status, got, test.wantIs)
			}
			code, ok := ErrorCode(got)
			if !ok {
				t.Errorf("ErrorCode(statusstreaming.ToError(%+v)) = (%v, %v), want (_, true)", test.status, code, ok)
			}
			if code != test.wantCode || got.Error() != test.wantMessage {
				t.Errorf("statusstreaming.ToError(%+v) = (%d, %q), want (%d, %q)", test.status, code, got.Error(), test.wantCode, test.wantMessage)
			}
		})
	}
}
