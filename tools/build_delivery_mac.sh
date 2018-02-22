#!/bin/bash
set -x
set -e

echo Rebuilding third party...
make clean_third_party
make third_party UNIX_PYTHON_VER=2.7
echo Rebuilding third party...DONE

echo Rebuilding or-tools...
make clean
make all fz -l 1 UNIX_PYTHON_VER=2.7
make test UNIX_PYTHON_VER=2.7
echo Rebuilding or-tools...DONE

echo Creating standard artifacts...
rm -rf temp ./*.tar.gz
make archive fz_archive python_examples_archive UNIX_PYTHON_VER=2.7
echo Creating standard artifacts...DONE

echo Rebuilding for Python 2.7...
make clean_python UNIX_PYTHON_VER=2.7
make python -l 1 UNIX_PYTHON_VER=2.7
make test_python UNIX_PYTHON_VER=2.7
make pypi_archive UNIX_PYTHON_VER=2.7
echo Rebuilding for Python 2.7...DONE
echo Creating Python 2.7 venv...
TEMP_DIR=temp-python2.7
VENV_DIR=${TEMP_DIR}/venv
python2.7 -m pip install --user virtualenv
python2.7 -m virtualenv -p python2.7 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 2.7 venv...DONE

echo Rebuilding for Python 3.5
make clean_python UNIX_PYTHON_VER=3.5
make python -l 1 UNIX_PYTHON_VER=3.5
make test_python UNIX_PYTHON_VER=3.5
make pypi_archive UNIX_PYTHON_VER=3.5
echo Rebuilding for Python 3.5...DONE
echo Creating Python 3.5 venv...
TEMP_DIR=temp-python3.5
VENV_DIR=${TEMP_DIR}/venv
python3.5 -m pip install --user virtualenv
python3.5 -m virtualenv -p python3.5 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 3.5 venv...DONE

echo Rebuilding for Python 3.6
make clean_python UNIX_PYTHON_VER=3.6
make python -l 1 UNIX_PYTHON_VER=3.6
make test_python UNIX_PYTHON_VER=3.6
make pypi_archive UNIX_PYTHON_VER=3.6
echo Rebuilding for Python 3.6...DONE
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
