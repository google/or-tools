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
which.exe C:\python39-64\python.exe || exit 1
echo C:\python39-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python310-64\python.exe || exit 1
echo C:\python310-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python311-64\python.exe || exit 1
echo C:\python311-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python312-64\python.exe || exit 1
echo C:\python312-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python313-64\python.exe || exit 1
echo C:\python313-64\python.exe: FOUND | tee.exe -a test.log
which.exe C:\python314-64\python.exe || exit 1
echo C:\python314-64\python.exe: FOUND | tee.exe -a test.log

set LOCAL_PATH=%PATH%

REM ##################
REM ##  PYTHON 3.9  ##
REM ##################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python39-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make python3.9: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_python3.9: DONE | tee.exe -a build.log
echo Rebuild Python3.9 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo Rebuild Python3.9 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.9 venv... | tee.exe -a test.log
set PATH=c:\python39-64;c:\python39-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python39
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.9 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.9 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.9 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.9... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.9...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ###################
REM ##  PYTHON 3.10  ##
REM ###################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python310-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python310-64 || exit 1
REM echo make python3.10: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python310-64 || exit 1
REM echo make test_python3.10: DONE | tee.exe -a build.log
echo Rebuild Python3.10 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python310-64 || exit 1
echo Rebuild Python3.10 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.10 venv... | tee.exe -a test.log
set PATH=c:\python310-64;c:\python310-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python310
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.10 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.10 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.10 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.10... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.10...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ###################
REM ##  PYTHON 3.11  ##
REM ###################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python311-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python311-64 || exit 1
REM echo make python3.11: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python311-64 || exit 1
REM echo make test_python3.11: DONE | tee.exe -a build.log
echo Rebuild Python3.11 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python311-64 || exit 1
echo Rebuild Python3.11 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.11 venv... | tee.exe -a test.log
set PATH=c:\python311-64;c:\python311-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python311
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.11 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.11 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.11 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.11... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.11...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ###################
REM ##  PYTHON 3.12  ##
REM ###################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python312-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python312-64 || exit 1
REM echo make python3.12: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python312-64 || exit 1
REM echo make test_python3.12: DONE | tee.exe -a build.log
echo Rebuild Python3.12 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python312-64 || exit 1
echo Rebuild Python3.12 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.12 venv... | tee.exe -a test.log
set PATH=c:\python312-64;c:\python312-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python312
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.12 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.12 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.12 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.12... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.12...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ###################
REM ##  PYTHON 3.13  ##
REM ###################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python313-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python313-64 || exit 1
REM echo make python3.13: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python313-64 || exit 1
REM echo make test_python3.13: DONE | tee.exe -a build.log
echo Rebuild Python3.13 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python313-64 || exit 1
echo Rebuild Python3.13 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.13 venv... | tee.exe -a test.log
set PATH=c:\python313-64;c:\python313-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python313
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.13 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.13 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.13 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.13... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.13...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ###################
REM ##  PYTHON 3.14  ##
REM ###################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python314-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python314-64 || exit 1
REM echo make python3.14: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python314-64 || exit 1
REM echo make test_python3.14: DONE | tee.exe -a build.log
echo Rebuild Python3.14 pypi archive... | tee.exe -a test.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python314-64 || exit 1
echo Rebuild Python3.14 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.14 venv... | tee.exe -a test.log
set PATH=c:\python314-64;c:\python314-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp_python314
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.14 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.14 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.14 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.14... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.14...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .
