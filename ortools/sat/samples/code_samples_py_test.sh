#!/bin/bash

declare -r DIR="${TEST_SRCDIR}/com_google_ortools/ortools/sat/samples"

function test::ortools::code_samples_sat_py() {
    "${DIR}/$1_py3"
}

test::ortools::code_samples_sat_py $1
