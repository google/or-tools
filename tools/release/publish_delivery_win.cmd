@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM run it as:
REM cmd /c tools\release\publish_delivery_win.cmd all

REM make few tools available
set PATH=%PATH%;tools;tools\win
set PATH=%PATH%;C:\python39-64;C:\python39-64\Scripts
set PRG=%0

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe publish.log

:: Display help if no argument
if "%1"=="" (
call :PRINT_HELP
exit /B %ERRORLEVEL%
)

if "%1"=="help" (
call :PRINT_HELP
exit /B %ERRORLEVEL%
)

if "%ORTOOLS_TOKEN%"=="" (
echo ORTOOLS_TOKEN: NOT FOUND | tee.exe -a publish.log
exit /B 1
)

FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --abbrev-ref HEAD`) DO (SET BRANCH=%%F)
FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --verify HEAD`) DO (SET SHA1=%%F)
echo BRANCH: %BRANCH% | tee.exe -a publish.log
echo SHA1: %SHA1% | tee.exe -a publish.log

if "%1"=="java" (
call :PUBLISH_JAVA
exit /B %ERRORLEVEL%
)

if "%1"=="python" (
call :PUBLISH_PYTHON
exit /B %ERRORLEVEL%
)

if "%1"=="all" (
call :PUBLISH_JAVA
call :PUBLISH_PYTHON
exit /B %ERRORLEVEL%
)

echo unknow target %1
exit /B 1


:PRINT_HELP
echo NAME
echo   %PRG% - Publish delivery packages.
echo SYNOPSIS
echo   %PRG% [help] java^|python^|all
echo DESCRIPTION
echo   Publish Google OR-Tools deliveries.
echo   You MUST define the following variables before running this script:
echo   * ORTOOLS_TOKEN: secret use to decrypt key to sign dotnet and java package.
echo.
echo OPTIONS
echo   help: show this help text (default)
echo   java: publish java packages
echo   python: publish python packages
echo   all: publish everything
echo.
echo EXAMPLES
echo   cmd /c %PRG% all
exit /B 0

:PUBLISH_JAVA
title Publish Java

REM Check Java
echo JAVA_HOME: %JAVA_HOME% | tee.exe -a publish.log
which.exe java || exit 1
which.exe java | tee.exe -a publish.log
which.exe mvn || exit 1
which.exe mvn | tee.exe -a publish.log

which.exe gpg || exit 1
which.exe gpg | tee.exe -a publish.log
which.exe openssl || exit 1
which.exe openssl | tee.exe -a publish.log

echo Publish native Java... | tee.exe -a publish.log
cmake --build temp_java --config Release --target java_native_deploy -v
echo Publish native Java...DONE | tee.exe -a publish.log
exit /B 0

:PUBLISH_PYTHON
title Publish Python
which.exe twine || exit 1
which.exe twine | tee.exe -a publish.log

echo Uploading all Python artifacts... | tee.exe -a publish.log
FOR %%i IN (*.whl) DO twine upload %%i
echo Uploading all Python artifacts...DONE | tee.exe -a publish.log
exit /B 0
