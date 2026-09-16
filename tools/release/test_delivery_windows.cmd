@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\release\test_delivery_win.cmd

REM Check all prerequisite
REM C++
set PATH=%PATH%;tools;tools\win
::echo "path base: %PATH%"
set PRG=%0

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe test.log

:: Display help if no argument
if "%1"=="" (
call :PRINT_HELP
exit /B %ERRORLEVEL%
)

if "%1"=="help" (
call :PRINT_HELP
exit /B %ERRORLEVEL%
)

if "%1"=="cpp" (
call :TEST_CPP
exit /B %ERRORLEVEL%
)

if "%1"=="dotnet" (
call :TEST_DOTNET
exit /B %ERRORLEVEL%
)

if "%1"=="java" (
call :TEST_JAVA
exit /B %ERRORLEVEL%
)

if "%1"=="python" (
call :TEST_PYTHON "%2"
exit /B %ERRORLEVEL%
)

if "%1"=="python_all" (
call :TEST_PYTHON_ALL
exit /B %ERRORLEVEL%
)

echo unknow target %1
exit /B 1


:PRINT_HELP
echo NAME
echo   %PRG% - Test delivery using the local host system.
echo SYNOPSIS
echo   %PRG% [help] cpp^|dotnet^|java^|python
echo DESCRIPTION
echo   Test Google OR-Tools deliveries.
echo   You MUST define the following variables before running this script:
echo.
echo OPTIONS
echo   help: show this help text (default)
echo   cpp: Test C++ packages
echo   dotnet: Test dotnet packages
echo   java: Test java packages
echo   python: Test python packages
echo.
echo EXAMPLES
echo   cmd /c %PRG%
exit /B 0


REM Test C++
:TEST_CPP
title Test Cpp
echo ToDo C++
exit /B 0


REM Test .Net
:TEST_DOTNET
title Test .Net
echo ToDo .Net
exit /B 0


REM Test Java
:TEST_JAVA
title Test Java
echo ToDo Java
exit /B 0


REM Test Python
:TEST_PYTHON
title Test Python 3.%1
echo Creating Python3.%1 venv... | tee.exe -a test.log
python -m pip install virtualenv
set TEMP_DIR=temp_python3%1
python -m virtualenv %TEMP_DIR%\venv
echo Creating Python3.%1 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.%1 venv... | tee.exe -a test.log
FOR %%i IN (export\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.%1 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.%1... | tee.exe -a test.log
%TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.%1...DONE | tee.exe -a test.log
exit /B 0


REM Test Python All
:TEST_PYTHON_ALL
title Test Python3
which.exe cmake || exit 1
which.exe cmake | tee.exe -a test.log
REM Python
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

FOR %%v IN (10 11 12 13 14) DO (
  title Test Python 3.%%v
  echo Creating Python3.%%v venv... | tee.exe -a test.log
  set PATH=c:\python3%%v-64;c:\python3%%V-64\Scripts;%PATH%
  python -m pip install virtualenv
  set TEMP_DIR=temp_python3%%v
  python -m virtualenv %TEMP_DIR%\venv
  set PATH=%LOCAL_PATH%
  echo Creating Python3.%%v venv...DONE | tee.exe -a test.log

  echo Installing ortools Python3.%%v venv... | tee.exe -a test.log
  FOR %%i IN (export\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
  echo Installing ortools Python3.%%v venv...DONE | tee.exe -a test.log

  echo Testing ortools Python3.%%v... | tee.exe -a test.log
  %TEMP_DIR%\venv\Scripts\python cmake\samples\python\sample.py 2>&1 | tee.exe -a test.log
  echo Testing ortools Python3.%%v...DONE | tee.exe -a test.log
)
exit /B 0
