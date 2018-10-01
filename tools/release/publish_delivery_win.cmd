REM /!\ THIS SCRIPT SUPPOSE A FIXED PATH FOR PYTHON /!\
REM run it as: cmd /c tools\release\publish_delivery_win.cmd
set PATH=%PATH%;tools;tools\win
set PATH=%PATH%;C:\python37-64;C:\python37-64\Scripts

REM Print version
make.exe print-OR_TOOLS_VERSION | tee.exe publish.log

which.exe twine || exit 1
which.exe twine | tee.exe -a publish.log

echo Uploading all Python artifacts... | tee.exe -a publish.log
FOR %%i IN (*.whl) DO twine upload %%i
echo Uploading all Python artifacts...DONE | tee.exe -a publish.log
