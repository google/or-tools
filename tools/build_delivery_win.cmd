REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\build_delivery_win.cmd
echo Rebuilding third party...
tools\make clean_third_party || exit 1
tools\make third_party WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Rebuilding third party...DONE

echo Rebuilding or-tools...
tools\make clean || exit 1
tools\make all fz WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
tools\make test WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Rebuilding or-tools...DONE

echo Creating standard artifacts...
tools\rm -rf temp *.zip || exit 1
tools\make archive fz_archive python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Creating standard artifacts...DONE

echo Rebuilding for Python 2.7...
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Rebuilding for Python 2.7...DONE

echo Rebuilding for Python 3.5
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo Rebuilding for Python 3.5...DONE

echo Rebuilding for Python 3.6
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Rebuilding for Python 3.6...DONE

echo Creating .NET artifacts...
set VS90COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools || exit 1
tools\make nuget_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Creating .NET artifacts...DONE
