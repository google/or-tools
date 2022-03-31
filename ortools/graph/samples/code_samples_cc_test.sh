#!/bin/bash

declare -r DIR="${TEST_SRCDIR}/com_google_ortools/ortools/graph/samples"

function test::ortools::code_samples_graph_cc() {
    "${DIR}/$1_cc"
}

test::ortools::code_samples_graph_cc $1