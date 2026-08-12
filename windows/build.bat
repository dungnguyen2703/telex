@echo off
setlocal enabledelayedexpansion

rem  build.bat            - build build\telex.exe
rem  build.bat test       - build everything, run the engine tests then the e2e tests
rem  build.bat engine     - build and run the engine tests only

if defined DevEnvDir goto :have_vs

set "VSWDIR=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VSWDIR%\vswhere.exe" echo Visual Studio not found ^(vswhere missing^). & exit /b 1
rem  Run vswhere from its own directory: quoting a path that contains "(x86)"
rem  inside a for /f backquote confuses cmd. The .\ prefix is required because
rem  NoDefaultCurrentDirectoryInExePath may be set.
pushd "%VSWDIR%"
for /f "usebackq tokens=*" %%i in (`.\vswhere.exe -latest -products * -property installationPath`) do set "VSPATH=%%i"
popd
set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" echo vcvars64.bat not found under "%VSPATH%". & exit /b 1
rem  stderr is discarded: vcvars itself probes for tools that may be absent and
rem  complains harmlessly. Its exit code is still checked.
call "%VCVARS%" >nul 2>nul
if errorlevel 1 exit /b 1

:have_vs
set "ROOT=%~dp0"
set "OUT=%ROOT%build"
if not exist "%OUT%" mkdir "%OUT%"

set "CFLAGS=/nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /Fo%OUT%\\"
set "ENGINE=%ROOT%src\engine\tables.cpp %ROOT%src\engine\syllable.cpp %ROOT%src\engine\telex.cpp"
set "APP=%ROOT%src\app\main.cpp %ROOT%src\app\hook.cpp %ROOT%src\app\sender.cpp %ROOT%src\app\tray.cpp %ROOT%src\app\icon.cpp %ROOT%src\app\exclusion.cpp"

if /i "%~1"=="engine" goto :engine
if /i "%~1"=="test" goto :test

:app
echo [build] telex.exe
rem  A running instance holds the exe open and the link would fail.
taskkill /IM telex.exe /F >nul 2>nul
cl %CFLAGS% /Fe%OUT%\telex.exe %APP% %ENGINE% user32.lib shell32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
echo Built %OUT%\telex.exe
exit /b 0

:engine
echo [build] engine_tests.exe
cl %CFLAGS% /Fe%OUT%\engine_tests.exe %ROOT%tests\engine_tests.cpp %ENGINE%
if errorlevel 1 exit /b 1
echo [run] engine tests
rem  The corpus is shared with the macOS build; see docs\TESTING.md.
"%OUT%\engine_tests.exe" "%ROOT%..\docs\corpus.txt"
exit /b %errorlevel%

:test
call "%~f0" engine
if errorlevel 1 exit /b 1
echo.
echo [build] telex.exe
rem  A running instance holds the exe open and the link would fail.
taskkill /IM telex.exe /F >nul 2>nul
cl %CFLAGS% /Fe%OUT%\telex.exe %APP% %ENGINE% user32.lib shell32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
echo [build] e2e_test.exe
cl %CFLAGS% /Fe%OUT%\e2e_test.exe %ROOT%tests\e2e_test.cpp user32.lib shell32.lib
if errorlevel 1 exit /b 1
echo.
echo [run] end-to-end tests
"%OUT%\e2e_test.exe" "%OUT%\telex.exe"
exit /b %errorlevel%
