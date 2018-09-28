REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM (e.g. want to upload only python35 artifacts)
set PATH=%PATH%;tools
set PATH=%PATH%;C:\python37-64;C:\python37-64\Scripts

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe publish.log

which.exe cmake || exit 1
which.exe cmake | tee.exe -a publish.log
REM Python
which.exe C:\python27-64\python.exe || exit 1
echo C:\python27-64\python.exe: FOUND | tee.exe -a publish.log
which.exe C:\python35-64\python.exe || exit 1
echo C:\python35-64\python.exe: FOUND | tee.exe -a publish.log
which.exe C:\python36-64\python.exe || exit 1
echo C:\python36-64\python.exe: FOUND | tee.exe -a publish.log
which.exe C:\python37-64\python.exe || exit 1
echo C:\python37-64\python.exe: FOUND | tee.exe -a publish.log

echo Uploading all Python artifacts...
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python27-64
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python35-64
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python36-64
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python37-64
echo Uploading all Python artifacts...DONE
