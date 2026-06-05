@echo off
setlocal
call "S:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
set "PATH=S:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;S:\Windows Kits\10\bin\10.0.26100.0\x64;S:\Qt 6\6.5.3\msvc2019_64\bin;C:\Users\31752\AppData\Roaming\Python\Python313\Scripts;%PATH%"
cd /d "S:\QT_object\IM6ULL_git"
qmake "S:\QT_object\IM6ULL_git\qt_hmi.pro" -spec win32-msvc CONFIG+=debug CONFIG+=qml_debug
if errorlevel 1 exit /b %errorlevel%
nmake /nologo qmake_all
if errorlevel 1 exit /b %errorlevel%
nmake /nologo /N debug > "S:\QT_object\IM6ULL_git\.vscode\nmake-dryrun.log"
if errorlevel 1 exit /b %errorlevel%
"C:\Users\31752\AppData\Roaming\Python\Python313\Scripts\compiledb.exe" -p "S:\QT_object\IM6ULL_git\.vscode\nmake-dryrun.log" -o "S:\QT_object\IM6ULL_git\compile_commands.json" -f --full-path --command-style
if errorlevel 1 exit /b %errorlevel%
if not "" == "" nmake /nologo 
endlocal
