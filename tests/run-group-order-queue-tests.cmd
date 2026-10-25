@echo off
setlocal
pushd "%~dp0.."
if errorlevel 1 exit /b 1
if /i not "%CD:~0,3%"=="D:\" (popd & exit /b 1)
set "queueFlags="
if /i "%~1"=="optimized" set "queueFlags=/O2 /DNDEBUG"
if not "%~1"=="" if /i not "%~1"=="optimized" (popd & exit /b 2)
if not defined VCToolsInstallDir call "D:\DevTools\VisualStudio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (popd & exit /b 1)
if not exist generated\queue-atomic mkdir generated\queue-atomic
pushd generated\queue-atomic
if errorlevel 1 (popd & exit /b 1)
cl /nologo /std:c++17 /EHsc /W4 /WX /Gy /DWITH_DEV_AUTOMATION_TESTS=1 %queueFlags% /I"..\..\tests\portable_unreal" /I"..\..\unreal\Source\RedalertProof" "..\..\tests\group_order_queue_tests.cpp" "..\..\tests\portable_unreal_md5_windows.cpp" "..\..\unreal\Source\RedalertProof\Commands\ProofCommandModel.cpp" "..\..\unreal\Source\RedalertProof\Skirmish\ProofSkirmishRules.cpp" "..\..\unreal\Source\RedalertProof\Presentation\ProofSelectionState.cpp" "..\..\unreal\Source\RedalertProof\Presentation\ProofReturnCargoSelection.cpp" "..\..\unreal\Source\RedalertProof\Presentation\ProofGroupOrderQueue.cpp" /Fe:queue-tests.exe /link /OPT:REF bcrypt.lib
if errorlevel 1 (popd & popd & exit /b 1)
queue-tests.exe
set "queueExit=%errorlevel%"
popd
popd
exit /b %queueExit%
