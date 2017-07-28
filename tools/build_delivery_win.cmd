echo Cleaning or-tools
tools\make.exe clean
echo Builing all libraries
tools\make.exe all fz
echo Running tests
tools\make.exe test
echo Creating standard artifacts.
tools\make.exe python_examples_archive archive fz_archive pypi_upload
tools\make.exe clean_python
echo Rebuilding for python 3.5
tools\make.exe python WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64
echo Rebuilding for python 3.6
tools\make.exe python WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64
