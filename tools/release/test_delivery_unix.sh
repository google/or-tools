#!/bin/bash
set -x
set -e

# Check all prerequisite
# cc
which cmake | xargs echo "cmake: " | tee build.log
which make | xargs echo "make: " | tee -a build.log
which swig | xargs echo "swig: " | tee -a build.log
# python
which python2.7 | xargs echo "python2.7: " | tee -a build.log
which python3.5 | xargs echo "python3.5: " | tee -a build.log
which python3.6 | xargs echo "python3.6: " | tee -a build.log

echo Creating Python 2.7 venv...
TEMP_DIR=temp-python2.7
VENV_DIR=${TEMP_DIR}/venv
python2.7 -m pip install --user virtualenv
python2.7 -m virtualenv -p python2.7 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 2.7 venv...DONE

echo Creating Python 3.5 venv...
TEMP_DIR=temp-python3.5
VENV_DIR=${TEMP_DIR}/venv
python3.5 -m pip install --user virtualenv
python3.5 -m virtualenv -p python3.5 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 3.5 venv...DONE

echo Creating Python 3.6 venv...
TEMP_DIR=temp-python3.6
VENV_DIR=${TEMP_DIR}/venv
python3.6 -m pip install --user virtualenv
python3.6 -m virtualenv -p python3.6 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 3.6 venv...DONE

# To be sure to have sandboxed library (i.e. @loader_path)
make clean_cc

echo Testing in virtualenv...
temp-python2.7/venv/bin/python temp-python2.7/venv/test.py
temp-python3.5/venv/bin/python temp-python3.5/venv/test.py
temp-python3.6/venv/bin/python temp-python3.6/venv/test.py
echo Testing in virtualenv...DONE
