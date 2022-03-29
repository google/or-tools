#!/bin/bash

declare -r DIR="${TEST_SRCDIR}/com_google_ortools/ortools/model_builder/samples"

function test::ortools::code_samples_model_builder_py() {
    "${DIR}/$1_py3"
}

test::ortools::code_samples_model_builder_py $1
