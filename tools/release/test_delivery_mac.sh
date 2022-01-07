#!/usr/bin/env bash
set -euxo pipefail

# Check all prerequisite
# cc
command -v cmake | xargs echo "cmake: " | tee test.log
command -v make | xargs echo "make: " | tee -a test.log
command -v swig | xargs echo "swig: " | tee -a test.log
# python
PY=(3.6 3.7 3.8 3.9 3.10)
for i in "${PY[@]}"; do
  command -v "python$i" | xargs echo "python$i: " | tee -a test.log
done

##################
##  PYTHON 3.X  ##
##################
for i in "${PY[@]}"; do
  echo "Cleaning Python..." | tee -a test.log
  make clean_python
  echo "Cleaning Python...DONE" | tee -a test.log

  echo "Rebuild Python$i pypi archive..." | tee -a test.log
  make package_python UNIX_PYTHON_VER="$i"
  echo "Rebuild Python$i pypi archive...DONE" | tee -a test.log

  echo "Creating Python$i venv..." | tee -a test.log
  TEMP_DIR="temp_python$i"
  VENV_DIR=${TEMP_DIR}/venv
  "python$i" -m pip install --user virtualenv
  "python$i" -m virtualenv "${VENV_DIR}"
  echo "Creating Python$i venv...DONE" | tee -a test.log

  echo "Installing ortools Python$i venv..." | tee -a test.log
  "${VENV_DIR}/bin/python" -m pip install "${TEMP_DIR}/ortools/dist/*.whl"
  echo "Installing ortools Python$i venv...DONE" | tee -a test.log

  set +e
  echo "Testing ortools Python$i..." | tee -a test.log
  (cd "${VENV_DIR}/bin" && ./python -c "from ortools.linear_solver import pywraplp") 2>&1 | tee -a test.log
  (cd "${VENV_DIR}/bin" && ./python -c "from ortools.constraint_solver import pywrapcp") 2>&1 | tee -a test.log
  (cd "${VENV_DIR}/bin" && ./python -c "from ortools.sat import pywrapsat") 2>&1 | tee -a test.log
  (cd "${VENV_DIR}/bin" && ./python -c "from ortools.graph import pywrapgraph") 2>&1 | tee -a test.log
  (cd "${VENV_DIR}/bin" && ./python -c "from ortools.algorithms import pywrapknapsack_solver") 2>&1 | tee -a test.log
  cp test.py.in "${VENV_DIR}/test.py"
  "${VENV_DIR}/bin/python" "${VENV_DIR}/test.py" 2>&1 | tee -a test.log
  echo "Testing ortools Python$i...DONE" | tee -a test.log
  set -e
done
