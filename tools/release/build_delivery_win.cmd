@echo off
REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM run it as: cmd /c tools\release\build_delivery_win.cmd

REM Check all prerequisite
REM C++
set PATH=%PATH%;tools;tools\win
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
which.exe java || exit 1
which.exe java | tee.exe -a build.log
REM .Net
which.exe dotnet || exit 1
which.exe dotnet | tee.exe -a build.log

REM ###################
REM ##  THIRD PARTY  ##
REM ###################
make.exe clean_third_party || exit 1
make.exe third_party WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make third_party: DONE | tee.exe -a build.log

REM ####################
REM ##  CC/JAVA/.Net  ##
REM ####################
make.exe clean || exit 1
make.exe cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make cc: DONE | tee.exe -a build.log
REM make.exe test_cc WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_cc: DONE | tee.exe -a build.log

make.exe fz WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make fz: DONE | tee.exe -a build.log

make.exe java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo make java: DONE | tee.exe -a build.log
REM make.exe test_java WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_java: DONE | tee.exe -a build.log

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
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python27-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make python2.7: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
REM echo make test_python2.7: DONE | tee.exe -a build.log
echo Rebuild Python2.7 pypi archive... | tee.exe -a test.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python27-64 || exit 1
echo Rebuild Python2.7 pypi archive...DONE | tee.exe -a test.log

echo Creating Python2.7 venv... | tee.exe -a test.log
set PATH=c:\python27-64;c:\python27-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp-python27
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python2.7 venv...DONE | tee.exe -a test.log

echo Installing ortools Python2.7 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python2.7 venv...DONE | tee.exe -a test.log

echo Testing ortools Python2.7... | tee.exe -a test.log
copy test.py.in %TEMP_DIR%\venv\test.py
%TEMP_DIR%\venv\Scripts\python %TEMP_DIR%\venv\test.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python2.7...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

REM ##################
REM ##  PYTHON 3.5  ##
REM ##################
echo Cleaning Python... | tee.exe -a test.log
make.exe clean_python WINDOWS_PATH_TO_PYTHON=c:\python35-64
echo Cleaning Python...DONE | tee.exe -a test.log

REM make.exe python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
REM echo make python3.5: DONE | tee.exe -a build.log
REM make.exe test_python WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
REM echo make test_python3.5: DONE | tee.exe -a build.log
echo Rebuild Python3.5 pypi archive... | tee.exe -a test.log
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python35-64 || exit 1
echo Rebuild Python3.5 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.5 venv... | tee.exe -a test.log
set PATH=c:\python35-64;c:\python35-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp-python35
python -m virtualenv %TEMP_DIR%\venv
set PATH=%LOCAL_PATH%
echo Creating Python3.5 venv...DONE | tee.exe -a test.log

echo Installing ortools Python3.5 venv... | tee.exe -a test.log
FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO %TEMP_DIR%\venv\Scripts\python -m pip install %%i
echo Installing ortools Python3.5 venv...DONE | tee.exe -a test.log

echo Testing ortools Python3.5... | tee.exe -a test.log
copy test.py.in %TEMP_DIR%\venv\test.py
%TEMP_DIR%\venv\Scripts\python %TEMP_DIR%\venv\test.py 2>&1 | tee.exe -a test.log
echo Testing ortools Python3.5...DONE | tee.exe -a test.log

FOR %%i IN (%TEMP_DIR%\ortools\dist\*.whl) DO copy %%i .

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
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python36-64 || exit 1
echo Rebuild Python3.6 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.6 venv... | tee.exe -a test.log
set PATH=c:\python36-64;c:\python36-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp-python36
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
make.exe pypi_archive WINDOWS_PATH_TO_PYTHON=c:\python37-64 || exit 1
echo Rebuild Python3.7 pypi archive...DONE | tee.exe -a test.log

echo Creating Python3.7 venv... | tee.exe -a test.log
set PATH=c:\python37-64;c:\python37-64\Scripts;%PATH%
python -m pip install virtualenv
set TEMP_DIR=temp-python37
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
