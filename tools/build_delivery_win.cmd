REM Each blocks could be triggered independently (i.e. commenting others)
REM (e.g. want to upload only python35 artifacts)
echo Cleaning or-tools
tools\make clean
echo Builing all libraries
tools\make all fz WINDOWS_PATH_TO_PYTHON=c:\python27-64
echo Running tests
tools\make test WINDOWS_PATH_TO_PYTHON=c:\python27-64

echo Creating standard artifacts.
tools\rm -rf temp *.zip
tools\make archive fz_archive python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64

echo Creating .NET artifacts.
set VS90COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools
tools\make nuget_upload WINDOWS_PATH_TO_PYTHON=c:\python27-64

echo Rebuilding for python 2.7
tools\rm -rf temp-python27
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python27-64
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64
tools\make pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python27-64
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64

echo Rebuilding for python 3.5
tools\rm -rf temp-python35
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python35-64
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64

echo Rebuilding for python 3.6
tools\rm -rf temp-python36
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python36-64
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64

