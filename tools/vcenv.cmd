@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

rem While the owner uses the PC, builds must stay small and in the background: the file OwnerIsUsingThePc beside this
rem script switches that on (one compiler, below normal priority). Without it the PC is free and builds run at full speed.
if exist "%~dp0OwnerIsUsingThePc" (
    set CMAKE_BUILD_PARALLEL_LEVEL=1
    powershell -NoProfile -Command "$parent = (Get-CimInstance Win32_Process -Filter \"ProcessId=$PID\").ParentProcessId; (Get-Process -Id $parent).PriorityClass = 'BelowNormal'" >nul 2>&1
)
%*
