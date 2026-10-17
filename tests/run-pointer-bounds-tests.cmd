@echo off
setlocal
pushd "%~dp0.."
if errorlevel 1 exit /b 1
if /i not "%CD:~0,3%"=="D:\" (popd & exit /b 1)
if not defined VCToolsInstallDir call "D:\DevTools\VisualStudio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (popd & exit /b 1)
if not exist generated mkdir generated
cl /nologo /std:c++17 /EHsc /W4 /WX /I"unreal\Source\RedalertProof" tests\pointer_bounds_tests.cpp /Fo"generated\pointer-bounds-tests.obj" /Fe"generated\pointer-bounds-tests.exe"
if errorlevel 1 (popd & exit /b 1)
generated\pointer-bounds-tests.exe
set "testExit=%errorlevel%"
popd
exit /b %testExit%
