@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\build_delivery_win.cmd

REM Check all prerequisite
REM cc
tools\which cmake || exit 1
tools\which cmake | tools\tee build.log
REM python
tools\which C:\python27-64\python.exe || exit 1
tools\which C:\python27-64\python.exe | tools\tee -a build.log
tools\which C:\python35-64\python.exe || exit 1
tools\which C:\python35-64\python.exe | tools\tee -a build.log
tools\which C:\python36-64\python.exe || exit 1
tools\which C:\python36-64\python.exe | tools\tee -a build.log
REM java
tools\which java || exit 1
tools\which java | tools\tee -a build.log
REM C#
tools\which nuget || exit 1
tools\which nuget | tools\tee -a build.log
tools\which csc || exit 1
tools\which csc | tools\tee -a build.log
tools\which dotnet || exit 1
tools\which dotnet | tools\tee -a build.log
REM F#
tools\which fsc || exit 1
tools\which fsc | tools\tee -a build.log

REM Build Third Party
tools\make clean_third_party || exit 1
tools\make third_party WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make third_party: DONE | tools\tee -a build.log

REM Building OR-Tools
tools\make clean || exit 1
tools\make cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make cc: DONE | tools\tee -a build.log
tools\make test_cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_cc: DONE | tools\tee -a build.log

tools\make python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make python2.7: DONE | tools\tee -a build.log
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_python2.7: DONE | tools\tee -a build.log

tools\make java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make java: DONE | tools\tee -a build.log
tools\make test_java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_java: DONE | tools\tee -a build.log

tools\make csharp WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make csharp: DONE | tools\tee -a build.log
tools\make test_csharp WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_csharp: DONE | tools\tee -a build.log

tools\make fsharp WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fsharp: DONE | tools\tee -a build.log
tools\make test_fsharp WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_fsharp: DONE | tools\tee -a build.log

tools\make fz WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fz: DONE | tools\tee -a build.log

REM Create Archive
tools\rm -rf temp *.zip || exit 1
tools\make archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make archive: DONE | tools\tee -a build.log
tools\make fz_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fz_archive: DONE | tools\tee -a build.log
tools\make python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make python_examples_archive: DONE | tools\tee -a build.log


REM Rebuilding for Python 2.7...
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make python2.7: DONE | tools\tee -a build.log
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_python2.7: DONE | tools\tee -a build.log
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make pypi_archive2.7: DONE | tools\tee -a build.log

REM Rebuilding for Python 3.5...
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo make python3.5: DONE | tools\tee -a build.log
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo make test_python3.5: DONE | tools\tee -a build.log
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo make pypi_archive3.5: DONE | tools\tee -a build.log

REM Rebuilding for Python 3.6...
tools\make clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
tools\make python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo make python3.6: DONE | tools\tee -a build.log
tools\make test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo make test_python3.6: DONE | tools\tee -a build.log
tools\make pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo make pypi_archive3.6: DONE | tools\tee -a build.log


REM Creating .NET artifacts
tools\make nuget_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo make nuget_archive: DONE | tools\tee -a build.log
