@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as:
REM set PATH=%PATH%;tools;tools\win
REM cmd /c tools\release\build_delivery_win.cmd

REM Check all prerequisite
set PATH=%PATH%;tools;tools\win

if "%ORTOOLS_TOKEN%" == "" (
  echo ORTOOLS_TOKEN: NOT FOUND | tee.exe build.log
  exit 1
)

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe build.log

REM clean everything
echo clean everything... | tee.exe -a build.log
make.exe clean_third_party || exit 1
make.exe clean || exit 1

which.exe cmake || exit 1
which.exe cmake | tee.exe -a build.log
REM Python
which.exe C:\python36-64\python.exe || exit 1
echo C:\python36-64\python.exe: FOUND | tee.exe -a build.log
C:\python36-64\python.exe -m pip install --user absl-py mypy-protobuf

which.exe C:\python37-64\python.exe || exit 1
echo C:\python37-64\python.exe: FOUND | tee.exe -a build.log
C:\python37-64\python.exe -m pip install --user absl-py mypy-protobuf

which.exe C:\python38-64\python.exe || exit 1
echo C:\python38-64\python.exe: FOUND | tee.exe -a build.log
C:\python38-64\python.exe -m pip install --user absl-py mypy-protobuf

which.exe C:\python39-64\python.exe || exit 1
echo C:\python39-64\python.exe: FOUND | tee.exe -a build.log
C:\python39-64\python.exe -m pip install --user absl-py mypy-protobuf

REM Java
echo JAVA_HOME: %JAVA_HOME% | tee.exe -a build.log
which.exe java || exit 1
which.exe java | tee.exe -a build.log
which.exe mvn || exit 1
which.exe mvn | tee.exe -a build.log

which.exe gpg || exit 1
which.exe gpg | tee.exe -a build.log
which.exe openssl || exit 1
which.exe openssl | tee.exe -a build.log


REM .Net
which.exe dotnet || exit 1
which.exe dotnet | tee.exe -a build.log

REM ###############################
REM ##  Build Examples Archives  ##
REM ###############################
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

REM ###################
REM ##  THIRD PARTY  ##
REM ###################
echo make third_party: ... | tee.exe -a build.log
make.exe third_party WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make third_party: DONE | tee.exe -a build.log

REM ########################
REM ##  Install Java gpg  ##
REM ########################
echo Install Java gpg | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\private-key.gpg.enc -out private-key.gpg -d
gpg --batch --import private-key.gpg
del private-key.gpg
mkdir -p %userprofile%/.m2
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\settings.xml.enc -out %userprofile%/.m2/settings.xml -d
echo "DONE" | tee -a build.log

REM ########################
REM ##  Install .Net snk  ##
REM ########################
echo Install .Net snk | tee.exe -a build.log
openssl aes-256-cbc -iter 42 -pass pass:%ORTOOLS_TOKEN% -in tools\release\or-tools.snk.enc -out or-tools.snk -d
set DOTNET_SNK=or-tools.snk

REM ####################
REM ##  CC/JAVA/.Net  ##
REM ####################
echo make cc: ... | tee.exe -a build.log
make.exe cc WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make cc: DONE | tee.exe -a build.log
REM make.exe test_cc WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_cc: DONE | tee.exe -a build.log

echo make fz: ... | tee.exe -a build.log
make.exe fz WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make fz: DONE | tee.exe -a build.log

echo make java: ... | tee.exe -a build.log
make.exe java WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make java: DONE | tee.exe -a build.log
REM make.exe test_java WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_java: DONE | tee.exe -a build.log

echo make dotnet: ... | tee.exe -a build.log
make.exe dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make dotnet: DONE | tee.exe -a build.log
REM make.exe test_dotnet WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_dotnet: DONE | tee.exe -a build.log

REM Create Archive
make.exe archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make archive: DONE | tee.exe -a build.log
make.exe test_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make test_archive: DONE | tee.exe -a build.log
make.exe fz_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make fz_archive: DONE | tee.exe -a build.log
make.exe test_fz_archive WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo make test_fz_archive: DONE | tee.exe -a build.log

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
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Rebuild Python3.6 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.6 pypi archive... | tee.exe -a build.log
make.exe test_package_python WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
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
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Rebuild Python3.7 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.7 pypi archive... | tee.exe -a build.log
make.exe test_package_python WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Test Python3.7 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python37
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.8  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python38-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
REM echo make python3.8: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
REM echo make test_python3.8: DONE | tee.exe -a build.log
echo Rebuild Python3.8 pypi archive... | tee.exe -a build.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
echo Rebuild Python3.8 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.8 pypi archive... | tee.exe -a build.log
make.exe test_package_python WINDOWS_PATH_TO_PYTHON=c:\python38-64 || exit 1
echo Test Python3.8 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python38
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.9  ##
REM ##################
echo Cleaning Python... | tee.exe -a build.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python39-64
echo Cleaning Python...DONE | tee.exe -a build.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make python3.9: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
REM echo make test_python3.9: DONE | tee.exe -a build.log
echo Rebuild Python3.9 pypi archive... | tee.exe -a build.log
make.exe package_python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo Rebuild Python3.9 pypi archive...DONE | tee.exe -a build.log
echo Test Python3.9 pypi archive... | tee.exe -a build.log
make.exe test_package_python WINDOWS_PATH_TO_PYTHON=c:\python39-64 || exit 1
echo Test Python3.9 pypi archive...DONE | tee.exe -a build.log

set TEMP_DIR=temp_python39
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .
