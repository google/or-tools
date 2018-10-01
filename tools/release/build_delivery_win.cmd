@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\release\build_delivery_win.cmd

REM Check all prerequisite
REM C++
set PATH=%PATH%;tools;tools\win

make.exe clean_third_party || exit 1
make.exe clean || exit 1

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe build.log

which.exe cmake || exit 1
which.exe cmake | tee.exe -a build.log
REM Python
which.exe C:\python27-64\python.exe || exit 1
echo C:\python27-64\python.exe: FOUND | tee.exe -a build.log
which.exe C:\python35-64\python.exe || exit 1
echo C:\python35-64\python.exe: FOUND | tee.exe -a build.log
which.exe C:\python36-64\python.exe || exit 1
echo C:\python36-64\python.exe: FOUND | tee.exe -a build.log
which.exe C:\python37-64\python.exe || exit 1
echo C:\python37-64\python.exe: FOUND | tee.exe -a build.log
REM Java
echo JAVA_HOME: %JAVA_HOME% | tee.exe -a build.log
which.exe java || exit 1
which.exe java | tee.exe -a build.log
REM .Net
which.exe dotnet || exit 1
which.exe dotnet | tee.exe -a build.log

REM ###################
REM ##  THIRD PARTY  ##
REM ###################
echo make third_party: ... | tee.exe -a build.log
make.exe third_party WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make third_party: DONE | tee.exe -a build.log

REM ####################
REM ##  CC/JAVA/.Net  ##
REM ####################
echo make cc: ... | tee.exe -a build.log
make.exe cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make cc: DONE | tee.exe -a build.log
REM make.exe test_cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_cc: DONE | tee.exe -a build.log

echo make fz: ... | tee.exe -a build.log
make.exe fz WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fz: DONE | tee.exe -a build.log

echo make java: ... | tee.exe -a build.log
make.exe java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make java: DONE | tee.exe -a build.log
REM make.exe test_java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_java: DONE | tee.exe -a build.log

echo make dotnet: ... | tee.exe -a build.log
make.exe dotnet WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make dotnet: DONE | tee.exe -a build.log
REM make.exe test_dotnet WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_dotnet: DONE | tee.exe -a build.log

REM Create Archive
rm.exe -rf temp *.zip || exit 1
make.exe archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make archive: DONE | tee.exe -a build.log
make.exe test_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_archive: DONE | tee.exe -a build.log
make.exe fz_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fz_archive: DONE | tee.exe -a build.log
make.exe test_fz_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make test_fz_archive: DONE | tee.exe -a build.log
make.exe python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make python_examples_archive: DONE | tee.exe -a build.log

REM ##################
REM ##  PYTHON 2.7  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make python2.7: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_python2.7: DONE | tee.exe -a build.log
echo Rebuild Python2.7 pypi archive... | tee.exe -a build.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Rebuild Python2.7 pypi archive...DONE | tee.exe -a build.log
echo Test Python2.7 pypi archive... | tee.exe -a build.log
make.exe test_pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Test Python2.7 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python27
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.5  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
REM echo make python3.5: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
REM echo make test_python3.5: DONE | tee.exe -a build.log
echo Rebuild Python3.5 pypi archive... | tee.exe -a build.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo Rebuild Python3.5 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.5 pypi archive... | tee.exe -a build.log
make.exe test_pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo Test Python3.5 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python35
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.6  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
REM echo make python3.6: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
REM echo make test_python3.6: DONE | tee.exe -a build.log
echo Rebuild Python3.6 pypi archive... | tee.exe -a build.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Rebuild Python3.6 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.6 pypi archive... | tee.exe -a build.log
make.exe test_pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Test Python3.6 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python36
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.7  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python37-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
REM echo make python3.7: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
REM echo make test_python3.7: DONE | tee.exe -a build.log
echo Rebuild Python3.7 pypi archive... | tee.exe -a build.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Rebuild Python3.7 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.7 pypi archive... | tee.exe -a build.log
make.exe test_pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Test Python3.7 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python37
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .
