@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as:
REM cmd /c tools\release\build_delivery_win.cmd all

REM make few tools available
set PATH=%PATH%;tools;tools\win
set PRG=%0

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe build.log

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
echo ORTOOLS_TOKEN: NOT FOUND | tee.exe -a build.log
exit /B 1
)

FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --abbrev-ref HEAD`) DO (SET BRANCH=%%F)
FOR /F "tokens=* USEBACKQ" %%F IN (`git rev-parse --verify HEAD`) DO (SET SHA1=%%F)
echo BRANCH: %BRANCH% | tee.exe -a build.log
echo SHA1: %SHA1% | tee.exe -a build.log

md export

if "%1"=="java" (
call :BUILD_CXX
call :BUILD_JAVA
exit /B %ERRORLEVEL%
)

if "%1"=="dotnet" (
call :BUILD_CXX
call :BUILD_DOTNET
exit /B %ERRORLEVEL%
)

if "%1"=="python" (
call :BUILD_CXX
call :BUILD_PYTHON
exit /B %ERRORLEVEL%
)

if "%1"=="archive" (
call :BUILD_CXX
call :BUILD_JAVA
call :BUILD_DOTNET
call :BUILD_ARCHIVE
exit /B %ERRORLEVEL%
)

if "%1"=="examples" (
call :BUILD_EXAMPLES
exit /B %ERRORLEVEL%
)

if "%1"=="all" (
call :BUILD_CXX
call :BUILD_JAVA
call :BUILD_DOTNET
call :BUILD_ARCHIVE
call :BUILD_EXAMPLES
call :BUILD_PYTHON
exit /B %ERRORLEVEL%
)

if "%1"=="reset" (
echo clean everything... | tee.exe -a build.log
make.exe clean || exit 1
make.exe clean_third_party || exit 1
rm.exe -rf export
del or-tools.snk
for %%i in (*.zip) DO del %%i
for %%i in (*.whl) DO del %%i
for %%i in (*.log) DO del %%i
exit /B %ERRORLEVEL%
)

echo unknow target %1
exit /B 1


:PRINT_HELP
echo NAME
echo   %PRG% - Build delivery using the local host system.
echo SYNOPSIS
echo   %PRG% [help] dotnet^|java^|python^|archive^|examples^|all^|reset
echo DESCRIPTION
echo   Build Google OR-Tools deliveries.
echo   You MUST define the following variables before running this script:
echo   * ORTOOLS_TOKEN: secret use to decrypt key to sign dotnet and java package.
echo.
echo OPTIONS
echo   help: show this help text (default)
echo   dotnet: Build dotnet packages
echo   java: Build java packages
echo   python: Build python packages
echo   archive: Build archive
echo   examples: Build examples archives
echo   all: build everything
echo   reset: delete all artifacts and suppress cache file
echo.
echo EXAMPLES
echo   cmd /c %PRG%
exit /B 0

:BUILD_CXX
title Build C++
set HASH=
for /F "tokens=* delims=" %%x in (build_cxx.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo C++ build seems up to date, skipping
exit /B 0
)

REM Check C++
which.exe cmake || exit 1
which.exe cmake | tee.exe -a build.log

REM clean everything
echo clean everything... | tee.exe -a build.log
make.exe clean_third_party || exit 1
make.exe clean || exit 1

REM THIRD PARTY
echo make third_party: ... | tee.exe -a build.log
make.exe third_party WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make third_party: DONE | tee.exe -a build.log

echo make cc: ... | tee.exe -a build.log
make.exe cc WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make cc: DONE | tee.exe -a build.log
REM make.exe test_cc WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_cc: DONE | tee.exe -a build.log

echo make fz: ... | tee.exe -a build.log
make.exe fz WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make fz: DONE | tee.exe -a build.log

echo %BRANCH% %SHA1%>build_cxx.log
exit /B 0


:BUILD_JAVA
title Build Java
set HASH=
for /F "tokens=* delims=" %%x in (build_java.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Java build seems up to date, skipping
exit /B 0
)

REM Check Java
echo JAVA_HOME: %JAVA_HOME% | tee.exe -a build.log
which.exe java || exit 1
which.exe java | tee.exe -a build.log
which.exe mvn || exit 1
which.exe mvn | tee.exe -a build.log

which.exe gpg || exit 1
which.exe gpg | tee.exe -a build.log
which.exe openssl || exit 1
which.exe openssl | tee.exe -a build.log

REM Install Java GPG
echo Install Java GPG: ... | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\private-key.gpg.enc -out private-key.gpg -d
gpg --batch --import private-key.gpg
del private-key.gpg
mkdir -p %userprofile%/.m2
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\settings.xml.enc -out %userprofile%/.m2/settings.xml -d
echo Install Java GPG: DONE | tee -a build.log

echo make java: ... | tee.exe -a build.log
make.exe java WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make java: DONE | tee.exe -a build.log
REM make.exe test_java WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_java: DONE | tee.exe -a build.log

for %%i in (temp_java\ortools-win32-x86-64\target\*.jar*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
for %%i in (temp_java\ortools-java\target\*.jar*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_java.log
exit /B 0


:BUILD_DOTNET
title Build .Net
set HASH=
for /F "tokens=* delims=" %%x in (build_dotnet.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo .Net build seems up to date, skipping
exit /B 0
)

REM Check .Net
which.exe dotnet || exit 1
which.exe dotnet | tee.exe -a build.log

REM Install .Net snk
echo Install .Net snk | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\or-tools.snk.enc -out or-tools.snk -d
set DOTNET_SNK=or-tools.snk

echo make dotnet: ... | tee.exe -a build.log
make.exe dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make dotnet: DONE | tee.exe -a build.log
REM make.exe test_dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_dotnet: DONE | tee.exe -a build.log

for %%i in (packages\*nupkg*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_dotnet.log
exit /B 0


REM Create Archive
:BUILD_ARCHIVE
title Build archives
set HASH=
for /F "tokens=* delims=" %%x in (build_archive.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Archive build seems up to date, skipping
exit /B 0
)

make.exe archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make archive: DONE | tee.exe -a build.log
make.exe test_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make test_archive: DONE | tee.exe -a build.log
make.exe fz_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make fz_archive: DONE | tee.exe -a build.log
make.exe test_fz_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make test_fz_archive: DONE | tee.exe -a build.log

for %%i in (or-tools_*VisualStudio2019-64bit*.zip) do (
  echo Move %%i to export... | tee.exe -a build.log
  move %%i export\.
  echo Move %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_archive.log
exit /B 0


REM Build Examples Archives
:BUILD_EXAMPLES
title Build examples
set HASH=
for /F "tokens=* delims=" %%x in (build_examples.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Examples build seems up to date, skipping
exit /B 0
)

rm.exe -rf temp *.zip || exit 1
echo Build examples archives... | tee.exe -a build.log
echo   C++ examples archive... | tee.exe -a build.log
make.exe cc_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo   Python examples archive... | tee.exe -a build.log
make.exe python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo   Java examples archive... | tee.exe -a build.log
make.exe java_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo   .Net examples archive... | tee.exe -a build.log
make.exe dotnet_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo DONE | tee.exe -a build.log

for %%i in (or-tools_*_examples_*.zip) do (
  echo Move %%i to export... | tee.exe -a build.log
  move %%i export\.
  echo Move %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_examples.log
exit /B 0


REM PYTHON 3.6,3.7,3.8,3.9
:BUILD_PYTHON
title Build Python
set HASH=
for /F "tokens=* delims=" %%x in (build_python.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Python build seems up to date, skipping
exit /B 0
)

for %%v in (6 7 8 9) do (
  title Build Python 3.%%v
  REM Check Python
  which.exe C:\python3%%v-64\python.exe || exit 1
  echo C:\python3%%v-64\python.exe: FOUND | tee.exe -a build.log
  C:\python3%%v-64\python.exe -m pip install --user absl-py mypy-protobuf
  set PATH+=;%userprofile%\appdata\roaming\python\python3%%v\Scripts"
  set PATH+=;C:\python3%%v-64\Scripts"

  echo Cleaning Python... | tee.exe -a build.log
 make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python3%%v-64
  echo Cleaning Python...DONE | tee.exe -a build.log

  REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python3%%v-64 || exit 1
  REM echo make python3.%%v: DONE | tee.exe -a build.log
  REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python3%%v-64 || exit 1
  REM echo make test_python3.%%v: DONE | tee.exe -a build.log
  echo Rebuild Python3.%%v pypi archive... | tee.exe -a build.log
  make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python3%%v-64 || exit 1
  echo Rebuild Python3.%%v pypi archive...DONE | tee.exe -a build.log
  echo Test Python3.%%v pypi archive... | tee.exe -a build.log
  make.exe test_package_python WINDOWS_PATH_TO_PYTHON=c:\python3%%v-64 || exit 1
  echo Test Python3.%%v pypi archive...DONE | tee.exe -a build.log

  for %%i in (temp_python3%%v\ortools\dist\*.whl) do (
    echo Copy %%i to export... | tee.exe -a build.log
    copy %%i export\.
    echo Copy %%i to export...DONE | tee.exe -a build.log
  )
)

echo %BRANCH% %SHA1%>build_python.log
exit /B 0

