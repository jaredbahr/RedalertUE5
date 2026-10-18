@echo off
setlocal
pushd "%~dp0.."
if errorlevel 1 exit /b 1
if /i not "%CD:~0,3%"=="D:\" (popd & exit /b 1)
if not defined VCToolsInstallDir call "D:\DevTools\VisualStudio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (popd & exit /b 1)
if not exist generated mkdir generated
cl /nologo /std:c++17 /EHsc /W4 /WX /I"unreal\Source\RedalertProof" tests\context_order_policy_tests.cpp /Fo"generated\context-order-tests.obj" /Fe"generated\context-order-tests.exe"
if errorlevel 1 (popd & exit /b 1)
generated\context-order-tests.exe
set "testExit=%errorlevel%"
popd
exit /b %testExit%
