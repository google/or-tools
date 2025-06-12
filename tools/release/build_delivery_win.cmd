@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as:
REM cmd /c tools\release\build_delivery_win.cmd all

REM make few tools available
set PATH=tools;tools\win;%PATH%
::echo "path base: %PATH%"
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

if not exist .\export md .\export

if "%1"=="dotnet" (
call :BUILD_DOTNET
exit /B %ERRORLEVEL%
)

if "%1"=="java" (
call :BUILD_JAVA
exit /B %ERRORLEVEL%
)

if "%1"=="python" (
call :BUILD_PYTHON
exit /B %ERRORLEVEL%
)

if "%1"=="archive" (
call :BUILD_ARCHIVE
exit /B %ERRORLEVEL%
)

if "%1"=="examples" (
call :BUILD_EXAMPLES
exit /B %ERRORLEVEL%
)

if "%1"=="all" (
call :BUILD_DOTNET
call :BUILD_JAVA
call :BUILD_ARCHIVE
:: call :BUILD_EXAMPLES
call :BUILD_PYTHON
exit /B %ERRORLEVEL%
)

if "%1"=="reset" (
call :RESET
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
echo   archive: Build all (C++, .Net, Java) archives
echo   examples: Build examples archives
echo   all: build everything
echo   reset: delete all artifacts and suppress cache file
echo.
echo EXAMPLES
echo   cmd /c %PRG%
exit /B 0


REM Build .Net
:BUILD_DOTNET
title Build .Net
set HASH=
FOR /F "tokens=* delims=" %%x IN (build_dotnet.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo .Net build seems up to date, skipping
exit /B 0
)

REM Check .Net
which.exe dotnet || exit 1
which.exe dotnet | tee.exe -a build.log
which.exe openssl || exit 1
which.exe openssl | tee.exe -a build.log

REM Install .Net snk
echo Install .Net snk | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\or-tools.snk.enc -out or-tools.snk -d
set DOTNET_SNK=or-tools.snk

echo Cleaning .Net... | tee.exe -a build.log
rm.exe -rf temp_dotnet
echo DONE | tee.exe -a build.log

echo Build dotnet: ... | tee.exe -a build.log
set Platform=any
cmake -S. -Btemp_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOTNET=ON -DUSE_DOTNET_462=ON
cmake --build temp_dotnet --config Release -j8 -v
echo DONE | tee.exe -a build.log
REM make.exe test_dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_dotnet: DONE | tee.exe -a build.log

FOR %%i IN (temp_dotnet\dotnet\packages\*.nupkg*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_dotnet.log
exit /B 0


REM Build Java
:BUILD_JAVA
title Build Java
set HASH=
FOR /F "tokens=* delims=" %%x IN (build_java.log) do (set HASH=%%x)
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

echo Cleaning Java... | tee.exe -a build.log
rm.exe -rf temp_java
echo DONE | tee.exe -a build.log

echo Build java: ... | tee.exe -a build.log
cmake -S. -Btemp_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_JAVA=ON -DSKIP_GPG=OFF
cmake --build temp_java --config Release -j8 -v
echo DONE | tee.exe -a build.log
REM cmake --build temp_java --config Release --target RUN_TEST || exit 1
REM echo cmake test_java: DONE | tee.exe -a build.log

FOR %%i IN (temp_java\java\ortools-win32-x86-64\target\*.jar*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
FOR %%i IN (temp_java\java\ortools-java\target\*.jar*) do (
  echo Copy %%i to export... | tee.exe -a build.log
  copy %%i export\.
  echo Copy %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_java.log
exit /B 0


REM Create Archive
:BUILD_ARCHIVE
title Build archives
set HASH=
FOR /F "tokens=* delims=" %%x IN (build_archive.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Archive build seems up to date, skipping
exit /B 0
)

REM Clean archive
make.exe clean_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1

echo Make cpp archive... | tee.exe -a build.log
make.exe archive_cpp WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo DONE | tee.exe -a build.log

echo Make dotnet archive... | tee.exe -a build.log
make.exe archive_dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo DONE | tee.exe -a build.log

echo Make java archive... | tee.exe -a build.log
make.exe archive_java WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo DONE | tee.exe -a build.log

FOR %%i IN (or-tools_*VisualStudio*.zip) do (
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
FOR /F "tokens=* delims=" %%x IN (build_examples.log) do (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Examples build seems up to date, skipping
exit /B 0
)

rm.exe -rf temp *.zip || exit 1
echo Build examples archives... | tee.exe -a build.log
echo   Python examples archive... | tee.exe -a build.log
make.exe python_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo   Java examples archive... | tee.exe -a build.log
make.exe java_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo   .Net examples archive... | tee.exe -a build.log
make.exe dotnet_examples_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo DONE | tee.exe -a build.log

FOR %%i IN (or-tools_*_examples_*.zip) do (
  echo Move %%i to export... | tee.exe -a build.log
  move %%i export\.
  echo Move %%i to export...DONE | tee.exe -a build.log
)
echo %BRANCH% %SHA1%>build_examples.log
exit /B 0

:subroutine
set PATH=C:\python3%1-64\Scripts;%PATH%
set PATH=%userprofile%\AppData\Roaming\Python\Python3%1\Scripts;%PATH%
::echo "python path: %PATH%"
GOTO :eof

REM PYTHON 3.9, 3.10, 3.11, 3.12, 3.13
:BUILD_PYTHON
title Build Python
set HASH=
FOR /F "tokens=* delims=" %%x IN (build_python.log) DO (set HASH=%%x)
if "%HASH%"=="%BRANCH% %SHA1%" (
echo Python build seems up to date, skipping
exit /B 0
)

FOR %%v IN (9 10 11 12 13) DO (
  title Build Python 3.%%v
  echo Check python3.%%v... | tee.exe -a build.log
  which.exe "C:\python3%%v-64\python.exe" || exit 1
  echo "C:\python3%%v-64\python.exe: FOUND" | tee.exe -a build.log
  C:\python3%%v-64\python.exe -m pip install --upgrade --user absl-py mypy mypy-protobuf protobuf numpy pandas

  call :subroutine %%v

  echo Cleaning Python 3.%%v... | tee.exe -a build.log
  rm.exe -rf temp_python3%%v
  echo Cleaning Python 3.%%v...DONE | tee.exe -a build.log

  echo Build Python 3.%%v... | tee.exe -a build.log
  cmake -S. -Btemp_python3%%v -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PYTHON=ON -DPython3_ROOT_DIR=C:\python3%%v-64
  cmake --build temp_python3%%v --config Release -j8 -v
  echo Build Python 3.%%v...DONE | tee.exe -a build.log

  echo Check MYPY files... | tee.exe -a build.log
    FOR %%m IN (
      ortools\algorithms\python\knapsack_solver.pyi
      ortools\constraint_solver\pywrapcp.pyi
      ortools\graph\python\linear_sum_assignment.pyi
      ortools\graph\python\max_flow.pyi
      ortools\graph\python\min_cost_flow.pyi
      ortools\init\python\init.pyi
      ortools\linear_solver\python\model_builder_helper.pyi
      ortools\linear_solver\pywraplp.pyi
      ortools\pdlp\python\pdlp.pyi
      ortools\sat\python\cp_model_helper.pyi
      ortools\scheduling\python\rcpsp.pyi
      ortools\util\python\sorted_interval_list.pyi
    ) DO (
      IF NOT EXIST temp_python3%%v\python\%%m (
        echo File %%m missing in python project | tee.exe -a build.log
        exit /B 1
      )
    )
  echo Check MYPY files...DONE | tee.exe -a build.log


  REM echo Test Python 3.%%v pypi archive... | tee.exe -a build.log
  REM cmake --build temp_python3%%v --config Release --target RUN_TEST || exit 1
  REM echo DONE | tee.exe -a build.log

  FOR %%i IN (temp_python3%%v\python\dist\*.whl) do (
    echo Copy %%i to export... | tee.exe -a build.log
    copy %%i export\.
    echo Copy %%i to export...DONE | tee.exe -a build.log
  )
)
echo %BRANCH% %SHA1%>build_python.log
exit /B 0

REM Reset
:RESET
title Reset
echo clean everything...
make.exe clean || exit 1
del /s /f /q temp_dotnet
rmdir /s /q temp_dotnet
del /s /f /q temp_java
rmdir /s /q temp_java
FOR %%v IN (9 10 11 12 13) do (
  del /s /f /q temp_python3%%v
  rmdir /s /q temp_python3%%v
)
rm.exe -rf export
del or-tools.snk
FOR %%i IN (*.zip) DO del %%i
FOR %%i IN (*.whl) DO del %%i
FOR %%i IN (*.log) DO del %%i
echo DONE
exit /B 0
