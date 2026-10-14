@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as:
REM cmd /c tools\release\publish_delivery_win.cmd all

REM add few tools to path
set PATH=tools;tools\win;%PATH%
::echo "path base: %PATH%"
set PRG=%0

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe publish.log

:: Display help if no argument
if "%1"=="" (
call :PRINT_HELP
exit /B
)

if "%1"=="help" (
call :PRINT_HELP
exit /B
)

if "%ORTOOLS_TOKEN%"=="" (
echo ORTOOLS_TOKEN: NOT FOUND | tee.exe -a publish.log
exit /B 1
)

FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --abbrev-ref HEAD`) DO (SET BRANCH=%%F)
FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --verify HEAD`) DO (SET SHA1=%%F)
echo BRANCH: %BRANCH% | tee.exe -a publish.log
echo SHA1: %SHA1% | tee.exe -a publish.log

if not exist .\export md .\export

if "%1"=="java" (
call :PUBLISH_JAVA
exit /B
)

if "%1"=="python" (
call :PUBLISH_PYTHON
exit /B
)

echo unknow target %1
exit /B 1


:PRINT_HELP
echo NAME
echo   %PRG% - Publish delivery using the local host system.
echo SYNOPSIS
echo   %PRG% [help] java^|python
echo DESCRIPTION
echo   Publish Google OR-Tools deliveries.
echo   You MUST define the following variables before running this script:
echo   * ORTOOLS_TOKEN: secret use to decrypt key to sign dotnet and java package.
echo.
echo OPTIONS
echo   help: show this help text (default)
echo   java: publish java packages
echo   python: publish python packages
echo.
echo EXAMPLES
echo   cmd /c %PRG%
exit /B 0

REM Publish Java
:PUBLISH_JAVA
title Publish Java
set HASH=
FOR /F "tokens=* delims=" %%x IN (publish_java.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Java publish seems up to date, skipping
exit /B 0
)

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

REM Install Java GPG
echo Install Java GPG: ... | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\private-key.gpg.enc -out private-key.gpg -d
gpg --batch --import private-key.gpg
del private-key.gpg
mkdir -p %userprofile%/.m2
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\settings.xml.enc -out %userprofile%/.m2/settings.xml -d
echo Install Java GPG: DONE | tee -a build.log

REM echo Cleaning Java... | tee.exe -a publish.log
REM rm.exe -rf temp_java
REM echo DONE | tee.exe -a publish.log

echo Build native java: ... | tee.exe -a publish.log
cmake -S. -Btemp_java -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_JAVA=ON -DSKIP_GPG=OFF || exit 1
cmake --build temp_java --config Release --target java_native_package || exit 1
echo DONE | tee.exe -a publish.log
REM cmake --build temp_java --config Release --target RUN_TEST || exit 1
REM echo cmake test_java: DONE | tee.exe -a publish.log

echo Publish native Java... | tee.exe -a publish.log
cmake --build temp_java --config Release --target java_native_deploy -v
echo Publish native Java...DONE | tee.exe -a publish.log

echo %BRANCH% %SHA1%>build_java.log
exit /B 0

:PUBLISH_PYTHON
title Publish Python
which.exe twine || exit 1
which.exe twine | tee.exe -a publish.log

echo Uploading all Python artifacts... | tee.exe -a publish.log
FOR %%i IN (*.whl) DO twine upload %%i
echo Uploading all Python artifacts...DONE | tee.exe -a publish.log
exit /B 0
