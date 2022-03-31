#!/bin/bash

declare -r DIR="${TEST_SRCDIR}/com_google_ortools/ortools/sat/samples"

function test::ortools::code_samples_sat_cc() {
    "${DIR}/$1_cc"
}

test::ortools::code_samples_sat_cc $1