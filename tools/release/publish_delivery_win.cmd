REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM Each blocks could be triggered independently (i.e. commenting others)
REM (e.g. want to upload only python35 artifacts)
set PATH=%PATH%;tools

echo Uploading all Python artifacts...
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python27-64
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python35-64
make.exe pypi_upload WINDOWS_PATH_TO_PYTHON=c:\python36-64
echo Uploading all Python artifacts...DONE

echo Uploading .NET artifacts...
make.exe nuget_upload WINDOWS_PATH_TO_PYTHON=c:\python27-64
echo Uploading .NET artifacts...DONE
