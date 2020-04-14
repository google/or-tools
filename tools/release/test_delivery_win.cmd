@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\release\test_delivery_win.cmd

REM Check all prerequisite
REM C++
set PATH=%PATH%;tools;tools\win
make.exe print-OR_TOOLS_VERSION | tee.exe test.log

which.exe cmake || exit 1
which.exe cmake | tee.exe -a test.log
REM Python
which.exe C:\python36-64\python.exe || exit 1
echo C:\python36-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python37-64\python.exe || exit 1
echo C:\python37-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python38-64\python.exe || exit 1
echo C:\python38-64\python.exe: FOUND | tee.exe -a test.log

set LOCAL_PATH=%PATH%

REM ##################
REM ##  PYTHON 3.6  ##
REM ##################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python36-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
REM echo make python3.6: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
REM echo make test_python3.6: DONE | tee.exe -a build.log
echo Rebuild Python3.6 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Rebuild Python3.6 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.6 venv... | tee.exe -a test.log
set PATH=c:\python36-64;c:\python36-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python36
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.6 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.6 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.6 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.6... | tee.exe -a test.log
copy test.py.in %TEMP_DIR%\venv\test.py
%TEMP_DIR%\venv\Scripts\python %TEMP_DIR%\venv\test.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.6...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.7  ##
REM ##################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python37-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
REM echo make python3.7: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
REM echo make test_python3.7: DONE | tee.exe -a build.log
echo Rebuild Python3.7 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Rebuild Python3.7 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.7 venv... | tee.exe -a test.log
set PATH=c:\python37-64;c:\python37-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python37
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.7 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.7 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.7 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.7... | tee.exe -a test.log
copy test.py.in %TEMP_DIR%\venv\test.py
%TEMP_DIR%\venv\Scripts\python %TEMP_DIR%\venv\test.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.7...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.8  ##
REM ##################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python38-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
REM echo make python3.8: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
REM echo make test_python3.8: DONE | tee.exe -a build.log
echo Rebuild Python3.8 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
echo Rebuild Python3.8 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.8 venv... | tee.exe -a test.log
set PATH=c:\python38-64;c:\python38-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python38
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.8 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.8 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.8 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.8... | tee.exe -a test.log
copy test.py.in %TEMP_DIR%\venv\test.py
%TEMP_DIR%\venv\Scripts\python %TEMP_DIR%\venv\test.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.8...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .
